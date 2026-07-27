// _GNU_SOURCE is required for mremap() (used to alias host memory into the guest address space).
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define FEX_EMULATOR_IMPL
#include "fex_x86_64_emulator.hpp"
#include "fex_x86_64_common.hpp"
#include "fex_x86_64_marshal.hpp"

// ---------------------------------------------------------------------------------------------------
// FEX-Emu backend (basic support).
//
// FEX (https://fex-emu.com) is an in-process x86/x86-64 -> AArch64 binary translator. Unlike the
// Unicorn/Icicle/KVM backends, FEX does NOT manage a sandboxed guest address space: it executes the
// translated guest inside the *host* process and treats guest virtual addresses as host virtual
// addresses (a 1:1 mapping). Consequences that shape this backend:
//
//   * map_memory() is a real mmap(MAP_FIXED) at the guest address; read/write_memory() is a direct
//     host memcpy once the range is known to be mapped.
//   * The guest runs natively (JITed): memory/execution/basic-block hooks are accepted for API
//     compatibility but never fire.
//   * Guest `syscall` instructions are routed back to sogen through a FEXCore::HLE::SyscallHandler,
//     which invokes the registered syscall instruction-hook. That is what lets the Windows emulation
//     layer service NT syscalls.
//
// Real multi-vCPU support: one FEXCore::Context::Context per bitness (context_/context32_, shared
// across every vCPU) drives N InternalThreadStates via FEXCore's own multi-thread-per-context
// embedding model. Each vCPU's own execution state (active thread/context, signal-handling
// bookkeeping, per-vCPU GDT, per-vCPU sigaltstack) lives on a dedicated fex_vcpu object
// (mirroring whp_x86_64_emulator's whp_vcpu); fex_x86_64_emulator itself owns everything
// machine-wide (both contexts, memory-region/MMIO/gate-crossing tables, hook maps) and exposes
// vcpus_ via the standard vcpu_count()/get_cpu() facade, with every inherited cpu-interface
// virtual on the emulator itself forwarding to vcpus_[0] for backward-compatible single-facade
// callers (the loader/setup path).
//
// The functional target of this file is Darwin on Apple Silicon (FEX only JITs to ARM64): the signal
// handlers, MMIO fault emulation, and 16KB/4KB page reconciliation below are all Darwin-only. The
// AArch64 Linux path compiles (CI build coverage) but has none of that runtime support and is not
// expected to run; Android is excluded outright by the CMake gate. The file is written against the
// FEXCore embedding API; spots that depend on FEX-version-specific internals are marked TODO(fex).
// ---------------------------------------------------------------------------------------------------

#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>

// backtrace()/backtrace_symbols() are used unconditionally by the std::terminate handler below
// (initialize_context()). SOGEN_ENABLE_FEX only ever enables this backend on Linux or Darwin (see
// the top-level CMakeLists.txt gate), and both platforms' libc provide <execinfo.h>, so this
// include does not need to be Apple-only.
#include <execinfo.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/arm/thread_status.h>
#include <signal.h>
#include <pthread.h>
#include <libkern/OSCacheControl.h>
#include <libproc.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#endif

#include <algorithm>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <utils/object.hpp>

// FEXCore embedding headers. These are only available when building against a FEX checkout/install;
// the CMake glue gates this whole target behind SOGEN_ENABLE_FEX so non-ARM builds never reach here.
#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/Core/SignalDelegator.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/HLE/SyscallHandler.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/Utils/LongJump.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/Utils/AllocatorHooks.h>

#ifdef __APPLE__
#include "hvf/hvf_vm.hpp"
#include "hvf/hvf_vcpu_executor.hpp"
#endif

namespace sogen::fex
{
    class fex_x86_64_emulator;
    class fex_vcpu;

    namespace
    {
        constexpr size_t page_size = 0x1000;

        // On Apple Silicon macOS, every process has a mandatory, unshrinkable 4GB __PAGEZERO
        // segment - the entire address range a real 32-bit x86 (WoW64) guest process needs (image
        // base, stack, heap, both 32-bit ntdll/kernel32) is permanently unmappable there. FEXCore's
        // JIT rebases every 32-bit-mode guest memory dereference/instruction fetch by a
        // per-Context, runtime-configurable offset (FEXCore::Context::Config.Wow64GuestRebaseValue,
        // set via SetWow64GuestRebaseValue - see its doc comment, public Context.h) so real host
        // memory can back 32-bit guest addresses at guest_addr + that offset instead. Everything
        // above this backend (sogen's PE loader, syscall handlers) continues to deal exclusively in
        // ordinary guest addresses, unaware this translation exists.
        //
        // wow64_guest_rebase_default is only the FIRST candidate this backend's own
        // reserve_wow64_host_window() tries (see its doc comment for the full rationale - in short,
        // no single fixed compile-time offset is safe: what host VA is actually free depends on
        // what else this specific process/machine has already mapped, which a hardcoded guess
        // cannot account for). It's also the value used unconditionally on any platform where that
        // dynamic search doesn't run (e.g. Linux ARM64 - this backend is shared, not Apple-
        // exclusive), preserving this backend's original, pre-dynamic-rebase behavior there exactly.
        constexpr uint64_t wow64_guest_rebase_default = 0x400000000ULL;

        // A 32-bit x86 guest's entire architectural address space is bounded by its 32-bit
        // pointers: [0, 4GB). This is the ONLY range that ever needs the wow64 rebase applied, and
        // it must stay separate from the rebase offset itself: real 64-bit ntdll/win32u modules are
        // placed starting at memory_manager's DEFAULT_ALLOCATION_ADDRESS_64BIT, which is exactly
        // 4GB - i.e. below a 16GB rebase offset - so using the rebase offset as the bound would
        // rebase a wow64 process's real 64-bit ntdll in [4GB, 16GB) right along with genuine 32-bit
        // addresses, corrupting where its real host memory lives relative to where context_'s
        // 64-bit JIT - which applies no internal rebase of its own, see GuestMemoryRebase()'s
        // Config.Is64BitMode() gate in deps/FEX - expects to find it.
        constexpr uint64_t wow64_guest_address_space_size = 0x100000000ULL;

        // The 64-bit user code-segment selector (matches sogen::wow64::heaven_gate::kUserCodeSelector
        // in src/windows-emulator/wow64_heaven_gate.hpp - kept as a local constant to avoid pulling
        // the windows-emulator include tree into this backend). A gate crossing whose target CS is
        // this selector is entering 64-bit mode (context_); anything else (0x23, the 32-bit compat
        // selector) is entering 32-bit mode (context32_). See perform_gate_crossing.
        constexpr uint16_t wow64_user_code_selector_64bit = 0x33;

#ifdef __APPLE__
        // FEXCore's own JITWriteScope (deps/FEX/FEXCore/include/FEXCore/Utils/AllocatorHooks.h) is a
        // plain ctor/dtor boolean toggle around pthread_jit_write_protect_np, not a nesting counter.
        // FEXCore::CPU::Arm64JITCore::ExitFunctionLink and its two delinker helpers (JIT.cpp) patch an
        // already-JIT-compiled call site - self-modifying code needing write-protect disabled - but
        // were written with no JITWriteScope of their own (upstream FEX targets Linux, which has no
        // per-thread MAP_JIT W^X concept). Our own wrapper around Pointers.ExitFunctionLink brackets
        // that call with a toggle to cover it, but ExitFunctionLink's "not yet compiled" path calls
        // CompileBlock, which uses ITS OWN nested JITWriteScope - and that nested scope's destructor
        // re-enables write-protect *before* control returns to our still-writing outer call, faulting
        // on the self-modifying store with a real (not synthetic) protection violation.
        //
        // Interposing pthread_jit_write_protect_np itself (via the __interpose section) to make it
        // reentrant is not viable: both escape hatches for reaching the *real* implementation from
        // inside the interposer - dlsym(RTLD_NEXT, ...) and dlopen(RTLD_NOLOAD)+dlsym-by-handle -
        // resolve back to the interposed replacement itself, because dyld's __interpose rewrite here
        // isn't scoped to "other images calling in" the way DYLD_INTERPOSE is normally described;
        // there's no reliable way to bypass it once installed for this exact symbol name process-wide.
        //
        // Instead: react to the fault itself. The *executing* code at the moment of the crash
        // (ExitFunctionLink, a plain libFEXCore.dylib function) is not MAP_JIT memory - only the
        // CodeBuffer it writes into is - so leaving write-protect disabled for longer than strictly
        // necessary is harmless as long as it's re-enabled before control returns to actually
        // executing JIT-compiled (MAP_JIT) code again. See handle_fault_signal's SEGV_ACCERR branch:
        // on a protection fault whose PC lands outside the dispatcher/guest range entirely (i.e.
        // inside FEXCore's own regular code, not a Break-op trampoline), disable write-protect and
        // retry the same faulting instruction rather than treating it as unhandled. Bounded per fault
        // address so a genuinely different bug can't spin forever.
        //
        // A std::unordered_map<uint64_t, int> is not viable here: operator[] can insert a node or
        // trigger a rehash - both call into the heap allocator - and this code runs inside a real
        // hardware signal handler that can interrupt program execution at an arbitrary point,
        // including mid-malloc()/free() of a totally unrelated allocation. Calling a non-async-
        // signal-safe function from a signal handler in that situation is undefined behavior and can
        // corrupt the allocator's internal free-list/lock if re-entered. A small fixed-size,
        // non-allocating array, linear-scanned, is genuinely async-signal-safe. Per-vCPU (fex_vcpu
        // member, not a file-scope global): under real multi-vCPU concurrency, more than one host
        // thread can be mid-retry at the same time, and a shared array would race across vCPUs.
        struct jit_write_protect_retry_slot
        {
            uint64_t address = 0;
            int count = 0;
            bool used = false;
            uint64_t last_fault_ns = 0;
        };

        constexpr size_t jit_write_protect_retry_slot_count = 8;

        // A burst of retries for the same address within this window counts toward the retry bound;
        // a gap at least this long since the address last faulted means it's being reused healthily
        // (a fresh, unrelated race resolved in the meantime), not spinning - see
        // jit_write_protect_retry_count_for's doc comment.
        constexpr uint64_t jit_write_protect_retry_reset_window_ns = 100'000'000; // 100ms

        uint64_t monotonic_now_ns()
        {
            struct timespec ts{};
            ::clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
        }

        // Apple Silicon's fixed host mmap/mprotect granularity (no way to get 4KB host pages).
        constexpr size_t host_page_size_apple = 0x4000;

        uint64_t host_page_align_down_apple(const uint64_t value)
        {
            return value & ~(host_page_size_apple - 1);
        }

        uint64_t host_page_align_up_apple(const uint64_t value)
        {
            return (value + host_page_size_apple - 1) & ~(host_page_size_apple - 1);
        }

        // Decodes just enough of an AArch64 "Load register" instruction to service an mmio_region
        // fault: destination register, transfer size, and zero/sign extension. Deliberately does not
        // decode the addressing mode (immediate, unscaled, register-offset, ...) - the effective
        // address is already known exactly (it's the fault address FEXCore's guest-VA==host-VA model
        // uses directly), so only the fields the "Load/store register" encoding class keeps at fixed
        // bit positions across every addressing-mode sub-form are needed (size/opc at [31:30]/[23:22],
        // Rt at [4:0] - see the AArch64 ISA's C4.1.3 "Loads and stores" encoding table). Stores are not
        // decoded: this backend's only MMIO consumer is read-only (see mmio_region's doc comment).
        struct decoded_arm64_load
        {
            uint32_t size = 0; // bytes: 1, 2, 4, 8, 16
            bool sign_extend = false;
            bool dest_is_64bit = false;
            bool is_vector = false; // true: rt names a SIMD&FP register (Qt), not a GPR
            uint32_t rt = 0;
        };

        std::optional<decoded_arm64_load> decode_arm64_load(const uint32_t insn)
        {
            struct encoding
            {
                uint32_t value;
                uint32_t size;
                bool sign_extend;
                bool dest_is_64bit;
                bool is_vector = false;
            };

            // "Load register (unsigned immediate)" - top 10 bits (size:2, fixed:6, opc:2).
            static constexpr encoding unsigned_imm_loads[] = {
                {0x39400000U, 1, false, false}, // LDRB
                {0x39800000U, 1, true, true},   // LDRSB, 64-bit dest
                {0x39C00000U, 1, true, false},  // LDRSB, 32-bit dest
                {0x79400000U, 2, false, false}, // LDRH
                {0x79800000U, 2, true, true},   // LDRSH, 64-bit dest
                {0x79C00000U, 2, true, false},  // LDRSH, 32-bit dest
                {0xB9400000U, 4, false, false}, // LDR Wt
                {0xB9800000U, 4, true, true},   // LDRSW
                {0xF9400000U, 8, false, true},  // LDR Xt
            };

            // "Load register (register offset)" - top 22 bits + fixed low bits (opt/S/1/0 = 0x800).
            static constexpr encoding reg_offset_loads[] = {
                {0x38600800U, 1, false, false}, // LDRB (register)
                {0x38A00800U, 1, true, true},   // LDRSB (register), 64-bit dest
                {0x38E00800U, 1, true, false},  // LDRSB (register), 32-bit dest
                {0x78600800U, 2, false, false}, // LDRH (register)
                {0x78A00800U, 2, true, true},   // LDRSH (register), 64-bit dest
                {0x78E00800U, 2, true, false},  // LDRSH (register), 32-bit dest
                {0xB8600800U, 4, false, false}, // LDR Wt (register)
                {0xB8A00800U, 4, true, true},   // LDRSW (register)
                {0xF8600800U, 8, false, true},  // LDR Xt (register)
            };

            // "Load register (unscaled immediate, LDUR)" - same top22+low-bits mask width as the
            // register-offset form above, but bit21=0 (vs. 1) and bits[11:10]="00" (vs. "10"); the
            // 9-bit signed immediate at bits[20:12] is variable/unchecked, same as Rn/Rt.
            static constexpr encoding unscaled_loads[] = {
                {0x38400000U, 1, false, false},        // LDURB
                {0x38800000U, 1, true, true},          // LDURSB, 64-bit dest
                {0x38C00000U, 1, true, false},         // LDURSB, 32-bit dest
                {0x78400000U, 2, false, false},        // LDURH
                {0x78800000U, 2, true, true},          // LDURSH, 64-bit dest
                {0x78C00000U, 2, true, false},         // LDURSH, 32-bit dest
                {0xB8400000U, 4, false, false},        // LDUR Wt
                {0xB8800000U, 4, true, true},          // LDURSW
                {0xF8400000U, 8, false, true},         // LDUR Xt
                {0x3CC00000U, 16, false, false, true}, // LDUR Qt
            };

            // "Load-acquire register (LDAPR/LDAPRB/LDAPRH, and the older base LDAR/LDARB/LDARH from
            // the Load/store-exclusive class)" - the forms FEX emits for guest memory reads to model
            // x86's stronger memory ordering on ARM's weaker one. No addressing mode at all (always
            // [Xn], Rm fixed to the 11111 placeholder), so only Rn/Rt vary - mask out the low 10 bits.
            // No signed variants exist for either family.
            static constexpr encoding acquire_loads[] = {
                {0x38BFC000U, 1, false, false}, // LDAPRB
                {0x78BFC000U, 2, false, false}, // LDAPRH
                {0xB8BFC000U, 4, false, false}, // LDAPR Wt
                {0xF8BFC000U, 8, false, true},  // LDAPR Xt
                {0x08DFFC00U, 1, false, false}, // LDARB
                {0x48DFFC00U, 2, false, false}, // LDARH
                {0x88DFFC00U, 4, false, false}, // LDAR Wt
                {0xC8DFFC00U, 8, false, true},  // LDAR Xt
            };

            // "Load SIMD&FP register (unsigned immediate), 128-bit" - shares unsigned_imm_loads' mask/
            // position (V=1, size=00, opc=11 is the reserved combination meaning a 128-bit Q register
            // rather than a scalar B/H/S/D FP register). FEX uses this for a wide (16-byte) guest read.
            static constexpr encoding vector_loads[] = {
                {0x3DC00000U, 16, false, false, true}, // LDR Qt
            };

            const uint32_t rt = insn & 0x1FU;
            const uint32_t top10 = insn & 0xFFC00000U;
            const uint32_t top22_fixed_low = insn & 0xFFE00C00U;
            const uint32_t acquire_fixed = insn & 0xFFFFFC00U;

            for (const auto& enc : unsigned_imm_loads)
            {
                if (top10 == enc.value)
                {
                    return decoded_arm64_load{enc.size, enc.sign_extend, enc.dest_is_64bit, enc.is_vector, rt};
                }
            }

            for (const auto& enc : reg_offset_loads)
            {
                if (top22_fixed_low == enc.value)
                {
                    return decoded_arm64_load{enc.size, enc.sign_extend, enc.dest_is_64bit, enc.is_vector, rt};
                }
            }

            for (const auto& enc : unscaled_loads)
            {
                if (top22_fixed_low == enc.value)
                {
                    return decoded_arm64_load{enc.size, enc.sign_extend, enc.dest_is_64bit, enc.is_vector, rt};
                }
            }

            for (const auto& enc : acquire_loads)
            {
                if (acquire_fixed == enc.value)
                {
                    return decoded_arm64_load{enc.size, enc.sign_extend, enc.dest_is_64bit, enc.is_vector, rt};
                }
            }

            for (const auto& enc : vector_loads)
            {
                if (top10 == enc.value)
                {
                    return decoded_arm64_load{enc.size, enc.sign_extend, enc.dest_is_64bit, enc.is_vector, rt};
                }
            }

            return std::nullopt;
        }

        // Decodes an AArch64 "Store-release register" (STLR/STLRB/STLRH) instruction - the
        // counterpart to decode_arm64_load's LDAR/LDAPR handling, used only for the misaligned-atomic
        // fallback in handle_fault_signal (see its doc comment), never for mmio_region (this backend's
        // only MMIO consumer is read-only). Same fixed-Rm, no-addressing-mode shape as LDAR/LDAPR.
        //
        // Deliberately does NOT also decode plain STR/STUR (unlike decode_arm64_load, which does
        // cover the equivalent plain-load forms): broadening this table to cover them for
        // handle_general_memory_violation's Category-3 "false fault" case causes a real,
        // reproducible hang isolated to this table specifically (decode_arm64_load's equivalent
        // plain-load coverage is safe - the KUSD/MMIO path already exercises it) with a root cause
        // that is not yet understood, so this table must stay narrow. Category-3 false-fault
        // emulation is therefore currently limited to loads and STLR-family stores; a plain-store
        // false fault falls through to a crash instead of being emulated.
        //
        // This has a real, understood downstream consequence beyond just that crash, tracked
        // separately: handle_general_memory_violation also uses this same decoder to classify a
        // fault's memory_operation (write vs. read) for the *genuine* protection-violation dispatch
        // path, not just the false-fault path. A plain (non-atomic) STR to memory that IS mapped
        // read-only is therefore misclassified as a read: since the page's shadow entry declares
        // read permission, the "read" appears legitimate and is routed into the false-fault
        // recovery paths, which both fail for a genuine protection violation - the fault then
        // surfaces as an UNHANDLED host signal that aborts the whole emulator process, instead of
        // being delivered to the guest as STATUS_ACCESS_VIOLATION. Any guest that deliberately
        // writes to read-only memory and catches the resulting exception (packers/DRM protections
        // do this routinely) hits this. A plain store to UNMAPPED memory does reach the guest as an
        // exception, but with the read/write flag (ExceptionInformation[0]) wrongly reporting a
        // read. Not fixed here since a safe fix needs a classification-only check independent of
        // this decode table (which is unsafe to broaden, per the hang above) and that hasn't been
        // implemented/verified yet.
        struct decoded_arm64_store
        {
            uint32_t size = 0; // bytes: 1, 2, 4, 8
            uint32_t rt = 0;
        };

        std::optional<decoded_arm64_store> decode_arm64_store(const uint32_t insn)
        {
            struct encoding
            {
                uint32_t value;
                uint32_t size;
            };

            static constexpr encoding stores[] = {
                {0x089FFC00U, 1}, // STLRB
                {0x489FFC00U, 2}, // STLRH
                {0x889FFC00U, 4}, // STLR Wt
                {0xC89FFC00U, 8}, // STLR Xt
            };

            const uint32_t rt = insn & 0x1FU;
            const uint32_t fixed = insn & 0xFFFFFC00U;

            for (const auto& enc : stores)
            {
                if (fixed == enc.value)
                {
                    return decoded_arm64_store{enc.size, rt};
                }
            }

            return std::nullopt;
        }
#endif

        bool is_page_aligned(const uint64_t value)
        {
            return (value & (page_size - 1)) == 0;
        }

        int to_prot(const memory_permission permissions)
        {
            int prot = PROT_NONE;
            if ((permissions & memory_permission::read) != memory_permission::none)
            {
                prot |= PROT_READ;
            }
            if ((permissions & memory_permission::write) != memory_permission::none)
            {
                prot |= PROT_WRITE;
            }
            if ((permissions & memory_permission::exec) != memory_permission::none)
            {
                prot |= PROT_EXEC;
            }
            return prot;
        }

#ifdef __APPLE__
        // Apple Silicon's kernel categorically refuses simultaneous write+exec on any mapping that
        // isn't MAP_JIT-backed (mprotect fails outright with EACCES) - unlike Linux, where W^X for
        // guest memory is advisory at best. This is independent of the 16KB/4KB reconciliation
        // above: even a single guest region directly requesting RWX hits it, which real PE loaders
        // do routinely (map .text RWX to patch ASLR relocations, then narrow to RX before the module
        // ever executes). Favoring write over exec here handles that common, well-defined sequence
        // correctly; it would be wrong for a page that is genuinely written and executed in the same
        // window without an intervening apply_memory_protection call, which is not how real PE
        // loading behaves.
        int to_prot_apple(const memory_permission permissions)
        {
            int prot = to_prot(permissions);
            if ((prot & PROT_WRITE) && (prot & PROT_EXEC))
            {
                prot &= ~PROT_EXEC;
            }
            return prot;
        }
#endif

        // Bit-for-bit reimplementation of FEXCore::Context::ContextImpl::ReconstructCompactedEFLAGS /
        // SetFlagsFromCompactedEFLAGS (FEXCore's Core.cpp), operating directly on a CPUState instead
        // of a live InternalThreadState. FEXCore's originals unconditionally dereference the Thread
        // pointer to reach CurrentFrame->State, which doesn't exist yet for the staged CPUState the
        // Windows loader populates before the first start()/create_thread(). The NZCV bit positions
        // below (28-31) mirror FEXCore's fixed IR::OpDispatchBuilder::IndexNZCV mapping, an internal
        // compiler detail with no public header.
        uint32_t index_nzcv(unsigned bit_offset)
        {
            switch (bit_offset)
            {
            case FEXCore::X86State::RFLAG_OF_RAW_LOC:
                return 28;
            case FEXCore::X86State::RFLAG_CF_RAW_LOC:
                return 29;
            case FEXCore::X86State::RFLAG_ZF_RAW_LOC:
                return 30;
            case FEXCore::X86State::RFLAG_SF_RAW_LOC:
                return 31;
            default:
                return 0;
            }
        }

        uint32_t reconstruct_compacted_eflags(const FEXCore::Core::CPUState& state)
        {
            uint32_t eflags = 0;

            for (size_t i = 0; i < FEXCore::Core::CPUState::NUM_EFLAG_BITS; ++i)
            {
                switch (i)
                {
                case FEXCore::X86State::RFLAG_CF_RAW_LOC:
                case FEXCore::X86State::RFLAG_PF_RAW_LOC:
                case FEXCore::X86State::RFLAG_AF_RAW_LOC:
                case FEXCore::X86State::RFLAG_TF_RAW_LOC:
                case FEXCore::X86State::RFLAG_ZF_RAW_LOC:
                case FEXCore::X86State::RFLAG_SF_RAW_LOC:
                case FEXCore::X86State::RFLAG_OF_RAW_LOC:
                case FEXCore::X86State::RFLAG_DF_RAW_LOC:
                    break;
                default:
                    eflags |= uint32_t{state.flags[i]} << i;
                    break;
                }
            }

            uint32_t packed_nzcv = 0;
            std::memcpy(&packed_nzcv, &state.flags[FEXCore::X86State::RFLAG_NZCV_LOC], sizeof(packed_nzcv));

            const uint32_t of = (packed_nzcv >> index_nzcv(FEXCore::X86State::RFLAG_OF_RAW_LOC)) & 1;
            uint32_t cf = (packed_nzcv >> index_nzcv(FEXCore::X86State::RFLAG_CF_RAW_LOC)) & 1;
            const uint32_t zf = (packed_nzcv >> index_nzcv(FEXCore::X86State::RFLAG_ZF_RAW_LOC)) & 1;
            const uint32_t sf = (packed_nzcv >> index_nzcv(FEXCore::X86State::RFLAG_SF_RAW_LOC)) & 1;
            cf ^= 1;

            eflags |= of << FEXCore::X86State::RFLAG_OF_RAW_LOC;
            eflags |= cf << FEXCore::X86State::RFLAG_CF_RAW_LOC;
            eflags |= zf << FEXCore::X86State::RFLAG_ZF_RAW_LOC;
            eflags |= sf << FEXCore::X86State::RFLAG_SF_RAW_LOC;

            const uint32_t pf_byte = state.pf_raw & 0xff;
            const uint32_t pf = static_cast<uint32_t>(std::popcount(pf_byte ^ 1u)) & 1;
            eflags |= pf << FEXCore::X86State::RFLAG_PF_RAW_LOC;

            const uint32_t af = ((state.af_raw ^ pf_byte) & (1 << 4)) ? 1 : 0;
            eflags |= af << FEXCore::X86State::RFLAG_AF_RAW_LOC;

            const uint8_t tf_byte = state.flags[FEXCore::X86State::RFLAG_TF_RAW_LOC];
            eflags |= (tf_byte & 1) << FEXCore::X86State::RFLAG_TF_RAW_LOC;

            const uint8_t df_byte = state.flags[FEXCore::X86State::RFLAG_DF_RAW_LOC];
            if (df_byte & 0x80)
            {
                eflags |= 1 << FEXCore::X86State::RFLAG_DF_RAW_LOC;
            }

            return eflags;
        }

        void set_flags_from_compacted_eflags(FEXCore::Core::CPUState& state, uint32_t eflags)
        {
            for (size_t i = 0; i < FEXCore::Core::CPUState::NUM_EFLAG_BITS; ++i)
            {
                switch (i)
                {
                case FEXCore::X86State::RFLAG_OF_RAW_LOC:
                case FEXCore::X86State::RFLAG_CF_RAW_LOC:
                case FEXCore::X86State::RFLAG_ZF_RAW_LOC:
                case FEXCore::X86State::RFLAG_SF_RAW_LOC:
                    break;
                case FEXCore::X86State::RFLAG_AF_RAW_LOC:
                    state.af_raw = (eflags & (1U << i)) ? (1 << 4) : 0;
                    break;
                case FEXCore::X86State::RFLAG_PF_RAW_LOC:
                    state.pf_raw = (eflags & (1U << i)) ? 0 : 1;
                    break;
                case FEXCore::X86State::RFLAG_DF_RAW_LOC:
                    state.flags[i] = (eflags & (1U << i)) ? 0xff : 1;
                    break;
                default:
                    state.flags[i] = (eflags & (1U << i)) ? 1 : 0;
                    break;
                }
            }

            uint32_t packed_nzcv = 0;
            packed_nzcv |=
                (eflags & (1U << FEXCore::X86State::RFLAG_OF_RAW_LOC)) ? 1U << index_nzcv(FEXCore::X86State::RFLAG_OF_RAW_LOC) : 0U;
            packed_nzcv |=
                (eflags & (1U << FEXCore::X86State::RFLAG_CF_RAW_LOC)) ? 0U : 1U << index_nzcv(FEXCore::X86State::RFLAG_CF_RAW_LOC);
            packed_nzcv |=
                (eflags & (1U << FEXCore::X86State::RFLAG_ZF_RAW_LOC)) ? 1U << index_nzcv(FEXCore::X86State::RFLAG_ZF_RAW_LOC) : 0U;
            packed_nzcv |=
                (eflags & (1U << FEXCore::X86State::RFLAG_SF_RAW_LOC)) ? 1U << index_nzcv(FEXCore::X86State::RFLAG_SF_RAW_LOC) : 0U;
            std::memcpy(&state.flags[FEXCore::X86State::RFLAG_NZCV_LOC], &packed_nzcv, sizeof(packed_nzcv));

            state.flags[FEXCore::X86State::RFLAG_RESERVED_LOC] = 1;
            state.flags[FEXCore::X86State::RFLAG_IF_LOC] = 1;
        }

        struct mapped_region
        {
            size_t size = 0;
            memory_permission permissions = memory_permission::none;
            bool owned = true; // false for map_host_memory aliases we must not munmap
        };

        // FEX runs the guest natively with guest VA == host VA, so there is no per-instruction hook to
        // intercept a specific address the way an interpreted backend can - every access to this
        // backend's only current MMIO consumer, KUSER_SHARED_DATA, would otherwise cost a real
        // hardware fault + signal round-trip (measured as the dominant cost of a whole run when guest
        // code polls it at high frequency, e.g. a loading-screen timer). At its raw guest address
        // (0x7ffe0000) the region falls inside __PAGEZERO and truly cannot be backed by real memory
        // (confirmed via both mmap(MAP_FIXED) and mach_vm_allocate) - but for a wow64 process it is
        // rebased above 4GB like any other sub-4GB guest address (see wow64_guest_rebase), where real
        // backing is possible. map_mmio opportunistically mmaps read-only real memory there and keeps
        // it fresh once per quantum (see refresh_mmio_backings); host_backing stays null wherever the
        // real mapping isn't possible (e.g. a native 64-bit process), and the region falls back to the
        // original always-unmapped, fault-and-emulate behavior via handle_mmio_fault. Read-only either
        // way: this backend's only consumer treats guest writes as a no-op, so a write here is left to
        // surface as a normal access violation, matching real Windows (KUSER_SHARED_DATA is read-only
        // user-mapped memory there too) - not a general MMIO implementation.
        struct mmio_region
        {
            uint64_t address = 0;
            size_t size = 0;
            mmio_read_callback read_cb;
            void* host_backing = nullptr;
            size_t host_backing_size = 0;
        };

        struct hook_entry
        {
            x86_hookable_instructions type = x86_hookable_instructions::invalid;
            instruction_hook_callback callback;
        };

#ifdef __APPLE__
        bool sysctl_flag(const char* name)
        {
            int32_t value = 0;
            size_t size = sizeof(value);
            if (::sysctlbyname(name, &value, &size, nullptr, 0) != 0)
            {
                return false;
            }
            return value != 0;
        }

        // Real Apple Silicon feature detection via sysctlbyname's hw.optional.arm.* namespace, in
        // place of FEXCore's own FetchHostFeatures() (Linux-only: reads MIDR_EL1, which isn't
        // EL0-readable/trap-emulated on Darwin the way arm64 Linux's kernel does it). Confirmed
        // present on Apple Silicon (M-series) via `sysctl -a | grep hw.optional`; anything not
        // confirmed present, or without a clear ARM-feature mapping, is conservatively left false -
        // that only costs codegen quality, never correctness.
        FEXCore::HostFeatures fetch_host_features_apple()
        {
            FEXCore::HostFeatures features{};

            uint64_t cache_line_size = 64;
            size_t cache_line_size_len = sizeof(cache_line_size);
            ::sysctlbyname("hw.cachelinesize", &cache_line_size, &cache_line_size_len, nullptr, 0);
            features.DCacheLineSize = static_cast<uint32_t>(cache_line_size);
            features.ICacheLineSize = static_cast<uint32_t>(cache_line_size);
            features.SupportsCacheMaintenanceOps = true;

            features.SupportsAES = sysctl_flag("hw.optional.arm.FEAT_AES");
            features.SupportsCRC = sysctl_flag("hw.optional.arm.FEAT_CRC32");
            features.SupportsAtomics = sysctl_flag("hw.optional.arm.FEAT_LSE");
            features.SupportsRCPC = sysctl_flag("hw.optional.arm.FEAT_LRCPC");
            features.SupportsRAND = sysctl_flag("hw.optional.arm.FEAT_RNG");
            features.SupportsSHA = sysctl_flag("hw.optional.arm.FEAT_SHA1") && sysctl_flag("hw.optional.arm.FEAT_SHA256");
            features.SupportsPMULL_128Bit = sysctl_flag("hw.optional.arm.FEAT_PMULL");
            features.SupportsCSSC = sysctl_flag("hw.optional.arm.FEAT_CSSC");
            features.SupportsFCMA = sysctl_flag("hw.optional.arm.FEAT_FCMA");
            features.SupportsFlagM = sysctl_flag("hw.optional.arm.FEAT_FlagM");
            features.SupportsFlagM2 = sysctl_flag("hw.optional.arm.FEAT_FlagM2");
            features.SupportsRPRES = sysctl_flag("hw.optional.arm.FEAT_RPRES");
            features.SupportsFRINTTS = sysctl_flag("hw.optional.arm.FEAT_FRINTTS");
            features.SupportsECV = sysctl_flag("hw.optional.arm.FEAT_ECV");
            features.SupportsWFXT = sysctl_flag("hw.optional.arm.FEAT_WFxT");
            features.SupportsAFP = sysctl_flag("hw.optional.arm.FEAT_AFP");
            features.SupportsMOPS = sysctl_flag("hw.optional.arm.FEAT_MOPS");

            // No Apple Silicon hardware supports SVE/SVE2 as of this writing.
            features.SupportsSVE128 = false;
            features.SupportsSVE256 = false;
            features.SupportsAVX = false;
            features.SupportsSVEBitPerm = false;

            // TPIDRRO_EL0 is not confirmed to carry a CPU index on Darwin the way arm64 Linux's
            // kernel populates it; the fork's DEF_OP(ProcessorID) treats the non-TPIDRRO fallback as
            // unsupported (matching the Windows/wine precedent), so leaving this false means a guest
            // RDTSCP/RDPID will hard-error rather than silently read garbage. Known gap, not a
            // correctness risk: real-world guest code rarely depends on RDTSCP/RDPID succeeding.
            features.SupportsCPUIndexInTPIDRRO = false;

            // FEXCore's CPUID brand-string leaves (0x80000002-4) index PerCPUData, whose size is
            // CPUMIDRs.size(); an empty CPUMIDRs leaves PerCPUData empty and the leaf null-derefs
            // its ProductName. Linux's FetchHostFeatures() populates this by reading MIDR_EL1 per
            // core, which is unavailable from EL0 on Darwin. Report the host logical-CPU count with
            // a placeholder MIDR of 0: the M-series parts aren't in FEXCore's MIDR table anyway, so
            // it resolves to the "Unknown ARM CPU" brand string (FEXCore's own fallback) rather
            // than crashing, and keeps the guest-visible core count (derived from Cores) realistic.
            uint32_t logical_cpus = 1;
            size_t logical_cpus_len = sizeof(logical_cpus);
            if (::sysctlbyname("hw.logicalcpu", &logical_cpus, &logical_cpus_len, nullptr, 0) != 0 || logical_cpus == 0)
            {
                logical_cpus = 1;
            }
            features.CPUMIDRs.assign(logical_cpus, 0u);

            return features;
        }

        // Guards against a second live FEX emulator instance in this process - fault routing below
        // uses one shared, process-wide sigaction handler, and only one instance may ever install it
        // (reachable e.g. via the Python bindings constructing two emulators). Unrelated to per-vCPU
        // fault routing, which is t_current_vcpu below: FEXCore's own real Linux embedding
        // (SignalDelegator::HandleSignal) validates exactly this split - one shared handler, routed
        // per-thread via a thread-keyed lookup, not a single global "the" active instance.
        fex_x86_64_emulator* g_active_emulator = nullptr;

        // POSIX synchronous signals (SIGSEGV/SIGBUS/SIGILL/SIGTRAP) always deliver to the thread that
        // caused them, so a thread_local pointer to whichever fex_vcpu this host thread is currently
        // driving is a lock-free, correct way to route a fault to the right vCPU's state under
        // multi-vCPU. Set/cleared by fex_vcpu::start() for the duration of guest execution; null on
        // any other thread (UI pump, watchdog), which correctly falls through fault_signal_handler to
        // the existing unhandled-crash report instead of misrouting to some arbitrary vCPU.
        thread_local fex_vcpu* t_current_vcpu = nullptr;

        // RAII guard for t_current_vcpu, scoped to fex_vcpu::start()'s ExecuteThread loop.
        struct current_vcpu_scope
        {
            explicit current_vcpu_scope(fex_vcpu& vcpu)
            {
                t_current_vcpu = &vcpu;
            }

            ~current_vcpu_scope()
            {
                t_current_vcpu = nullptr;
            }

            current_vcpu_scope(const current_vcpu_scope&) = delete;
            current_vcpu_scope& operator=(const current_vcpu_scope&) = delete;
        };

        void fault_signal_handler(int sig, siginfo_t* info, void* raw_ucontext);

        void install_fault_signal_handlers(fex_x86_64_emulator& emulator)
        {
            // Fault routing is process-global (one sigaction handler set, one active-instance
            // pointer). A second live instance would silently receive the first one's faults, so
            // refuse it outright - reachable e.g. via the Python bindings constructing two emulators.
            if (g_active_emulator != nullptr)
            {
                throw std::runtime_error("Only one FEX emulator instance can be active per process");
            }

            g_active_emulator = &emulator;

            struct sigaction action = {};
            action.sa_sigaction = fault_signal_handler;
            action.sa_flags = SA_SIGINFO | SA_ONSTACK;
            sigemptyset(&action.sa_mask);

            ::sigaction(SIGSEGV, &action, nullptr);
            ::sigaction(SIGBUS, &action, nullptr);
            ::sigaction(SIGILL, &action, nullptr);

            // FEXCore's IR "Break" op (see handle_fault_signal's FaultToTopAndGeneratedException
            // comment) models x86 HLT/UD2/INT3/INT1/INTO/unhandled-INT-N uniformly via distinct native
            // trap instructions chosen per Dispatcher.cpp's GuestSignal_SIG* stubs: HLT/UDF raise
            // SIGILL, BRK raises SIGTRAP. INT3 (x86_64_dbgbreak/DebugBreak()) goes through the SIGTRAP
            // stub - without a handler registered here, that BRK was an entirely unhandled hardware
            // trap, terminating the process (exit 128+SIGTRAP) instead of reaching the vector-dispatch
            // logic below, which already handles vector 3 (breakpoint) correctly once it runs.
            ::sigaction(SIGTRAP, &action, nullptr);
        }

        // FEXCore::CPU::Arm64JITCore::ExitFunctionLink (JIT.cpp) patches an already-compiled call
        // site once its target block becomes known - self-modifying code, writing directly into a
        // MAP_JIT code buffer. It doesn't bracket that write with a JIT-write-protect toggle (see
        // FEXCore::Allocator::JITWriteScope) because it's written for Linux, which has no per-thread
        // W^X state to worry about; on Apple Silicon the write faults with a real (not synthetic)
        // protection violation, since MAP_JIT enforces write-XOR-execute per calling thread, not per
        // mapping. FEXCore::HLE::CpuStateFrame::Pointers.ExitFunctionLink is a plain function-pointer
        // slot JIT-compiled code calls through (see JIT.cpp's InitThreadPointers), so it can be
        // intercepted here with a toggling wrapper instead of touching deps/FEX. Write-once (guarded
        // by an atomic default of 0): every vCPU's thread shares the same original function pointer,
        // so only the first thread to install the wrapper needs to capture it.
        std::atomic<uint64_t> g_original_exit_function_link{0};

        uint64_t exit_function_link_jit_write_wrapper(FEXCore::Core::CpuStateFrame* frame, void* record)
        {
            using exit_function_link_fn = uint64_t (*)(FEXCore::Core::CpuStateFrame*, void*);
            const auto real = reinterpret_cast<exit_function_link_fn>(g_original_exit_function_link.load());

            ::pthread_jit_write_protect_np(0);
            const uint64_t result = real(frame, record);
            ::pthread_jit_write_protect_np(1);
            return result;
        }

        // ===========================================================================================
        // HVF hardware-TSO execution path (see docs in the FEX-on-HVF integration plan). Phase 1 is
        // strictly opt-in: EMULATOR_FEX_HVF=1 requires HVF + EnTSO and hard-errors otherwise, so a
        // silent fallback can never mask a regression; unset/0 leaves the in-process software-TSO
        // path bit-for-bit unchanged (g_hvf stays null and every branch below is dead).
        // ===========================================================================================
        hvf::hvf_vm* g_hvf = nullptr;

        bool hvf_requested()
        {
            const char* value = std::getenv("EMULATOR_FEX_HVF");
            return value != nullptr && value[0] != '\0' && !(value[0] == '0' && value[1] == '\0');
        }

        void hvf_pages_replaced_hook(void* ptr, size_t size)
        {
            if (g_hvf != nullptr)
            {
                g_hvf->refresh_backing(reinterpret_cast<uint64_t>(ptr), size);
            }
        }

        // Apple Silicon's SPTM tracks a type for every physical frame and refuses - with a
        // whole-machine panic (VIOLATION_ILLEGAL_MAPPING_TYPE), not a returnable error - to insert
        // a frame typed XNU_USER_EXEC or XNU_USER_DEBUG into a guest stage-2 page table. Those are
        // exactly the types a frame acquires while it is mapped executable in this process, so no
        // host mapping that is PROT_EXEC may be handed to hv_vm_map, whatever permissions the
        // stage-2 entry itself asks for. The insertion is lazy - it happens when
        // the guest first touches the page from inside hv_vcpu_run, not at hv_vm_map time - which
        // is why an offending mapping only panics on the runs that actually reach that page.
        int to_host_prot_hvf(const int prot)
        {
            return g_hvf != nullptr ? (prot & ~PROT_EXEC) : prot;
        }

        // libFEXCore's own image therefore cannot be mapped into the VM: its __TEXT is live,
        // code-signed, executing host code. Guest-executed JIT output does dereference pointers
        // into the image (the NamedVectorConstants and indexed LUT tables in CPUBackend.cpp, which
        // Apple's linker places in __TEXT,__const and __DATA_CONST,__const), so publish an
        // anonymous snapshot of the image at the same relative layout instead and rebase those
        // JITPointers slots onto it (hvf_shim_thread_pointers). Only immutable constant tables are
        // ever read through those slots, so a snapshot is equivalent to the live image; a pointer
        // into the image that this misses lands outside the mirror and faults inside the vCPU
        // rather than reading stale data.
        struct fexcore_image_mirror
        {
            uint64_t image_base = 0;
            uint64_t image_size = 0;
            uint64_t mirror_base = 0;

            bool contains(const uint64_t address) const
            {
                return this->image_size != 0 && address >= this->image_base && address < this->image_base + this->image_size;
            }

            uint64_t rebase(const uint64_t address) const
            {
                return this->mirror_base + (address - this->image_base);
            }
        };

        fexcore_image_mirror g_fexcore_mirror{};

        void hvf_publish_fexcore_constants()
        {
            Dl_info info{};
            if (dladdr(reinterpret_cast<void*>(&FEXCore::Config::Initialize), &info) == 0)
            {
                throw std::runtime_error("HVF: failed to locate the FEXCore image");
            }

            const auto* header = static_cast<const mach_header_64*>(info.dli_fbase);
            intptr_t slide = 0;
            for (uint32_t i = 0; i < _dyld_image_count(); ++i)
            {
                if (_dyld_get_image_header(i) == reinterpret_cast<const mach_header*>(header))
                {
                    slide = _dyld_get_image_vmaddr_slide(i);
                    break;
                }
            }

            const auto for_each_segment = [header, slide](auto&& callback) {
                const auto* cmd = reinterpret_cast<const load_command*>(header + 1);
                for (uint32_t i = 0; i < header->ncmds; ++i)
                {
                    if (cmd->cmd == LC_SEGMENT_64)
                    {
                        const auto* seg = reinterpret_cast<const segment_command_64*>(cmd);
                        if (strcmp(seg->segname, SEG_PAGEZERO) != 0 && seg->vmsize != 0)
                        {
                            callback(*seg, static_cast<uint64_t>(seg->vmaddr) + static_cast<uint64_t>(slide));
                        }
                    }
                    cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(cmd) + cmd->cmdsize);
                }
            };

            uint64_t lowest = std::numeric_limits<uint64_t>::max();
            uint64_t highest = 0;
            for_each_segment([&lowest, &highest](const segment_command_64& seg, const uint64_t va) {
                lowest = std::min(lowest, va);
                highest = std::max(highest, va + seg.vmsize);
            });

            if (highest <= lowest)
            {
                throw std::runtime_error("HVF: the FEXCore image has no mappable segments");
            }

            const size_t size = highest - lowest;
            void* mirror = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (mirror == MAP_FAILED)
            {
                throw std::runtime_error("HVF: failed to allocate the FEXCore constant mirror");
            }

            // filesize, not vmsize: the tail of a segment beyond its file content is bss the loader
            // zero-filled, which a fresh anonymous mapping already matches.
            for_each_segment([mirror, lowest](const segment_command_64& seg, const uint64_t va) {
                std::memcpy(static_cast<uint8_t*>(mirror) + (va - lowest), reinterpret_cast<const void*>(va), seg.filesize);
            });

            if (::mprotect(mirror, size, PROT_READ) != 0)
            {
                throw std::runtime_error("HVF: failed to seal the FEXCore constant mirror");
            }

            g_fexcore_mirror =
                fexcore_image_mirror{.image_base = lowest, .image_size = size, .mirror_base = reinterpret_cast<uint64_t>(mirror)};
            g_hvf->map(g_fexcore_mirror.mirror_base, size, PROT_READ);
        }

        // ===========================================================================================
        // FEXCore-internal host allocation arena (Apple, guest VA == host VA).
        //
        // This backend runs guest VA == host VA, so any host allocation the kernel is free to place
        // wherever it likes can land inside the guest's own address space. FEXCore obtains all of its
        // internal buffers - the BlockLinks red-black-tree storage (fextl monotonic buffer resource),
        // the JIT CodeBuffer, and the dispatcher - through FEXCore::Allocator::mmap(nullptr, ...) (via
        // Allocator::VirtualAlloc), which by default is a raw ::mmap(NULL, ...) with zero coordination
        // with sogen's guest bookkeeping. When one of those buffers happens to be placed at a host
        // address the guest also owns, an ordinary guest SIMD/vector store silently overwrites
        // FEXCore's own bookkeeping - the root cause of the long-standing __tree_balance_after_insert /
        // AddBlockLink corruption (a wild guest store scribbling a live BlockLinks tree node with
        // packed 16-bit-lane vector data).
        //
        // Fix: reserve one large host arena up-front, register its whole range as a reserved_host_range
        // (so sogen's memory manager steers every guest allocation away from it - a genuine two-way
        // exclusion), and satisfy every FEXCore::Allocator::mmap(nullptr, ...) request from inside it.
        // Non-executable requests are placed with MAP_FIXED over the reserved region. Executable
        // (MAP_JIT) requests cannot use MAP_FIXED - Apple rejects MAP_JIT|MAP_FIXED with EINVAL - so
        // the sub-region is unmapped first and MAP_JIT is requested with the hole's address as a
        // (non-fixed) hint, which the kernel reliably honors because nothing else competes for space
        // inside the arena. The result is verified to land inside the arena or the request fails
        // loudly (ENOMEM) rather than silently falling back to an unconstrained mapping that would
        // reintroduce the exact aliasing hazard this exists to prevent.
        // ===========================================================================================
        class fex_internal_arena
        {
          public:
            // Sized to comfortably exceed any realistic FEXCore-internal need (BlockLinks growth, the
            // JIT CodeBuffer and its growth, the dispatcher) for a full game/application workload.
            // Pure VA reservation (PROT_NONE) until sub-regions are actually committed.
            static constexpr size_t arena_size = 0x1'0000'0000ULL; // 4 GiB

            static fex_internal_arena& instance()
            {
                static fex_internal_arena arena;
                return arena;
            }

            // Reserve the arena and install the FEXCore allocator hooks. Must run before the first
            // FEXCore-internal allocation (context/CodeBuffer creation) and before the memory
            // manager first queries reserved_host_ranges(). Idempotent.
            //
            // This runs before notify_process_bitness (bitness isn't known yet at CPU-backend
            // construction time), so if the kernel's ASLR happens to place this arena's own
            // mmap(nullptr, ...) inside [wow64_guest_rebase, wow64_guest_rebase +
            // wow64_guest_address_space_size), that later reservation (see
            // reserve_wow64_host_window's doc comment) finds the window already occupied - by us -
            // and skips itself, silently losing the fix it exists to provide. Bitness isn't known
            // here, so just always avoid that range for the arena's own placement (harmless for a
            // non-wow64 process too - the arena has no reason to prefer any particular address).
            // Bounded retry: an ordinary mmap(nullptr, ...) with no hint gets a fresh ASLR pick each
            // call, so a handful of retries converges quickly; give up and accept the collision
            // rather than spin forever on a determined adversary (in which case
            // reserve_wow64_host_window's own probe-and-skip still keeps this safe, just without
            // the improvement).
            void install()
            {
                if (this->base_ != 0)
                {
                    return;
                }

                void* base = MAP_FAILED;
                constexpr int max_attempts = 8;
                for (int attempt = 0; attempt < max_attempts; ++attempt)
                {
                    void* candidate = ::mmap(nullptr, arena_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    if (candidate == MAP_FAILED)
                    {
                        break;
                    }

                    const auto candidate_addr = reinterpret_cast<uint64_t>(candidate);
                    const auto candidate_end = candidate_addr + arena_size;
                    const bool overlaps_wow64_window = candidate_addr < wow64_guest_rebase_default + wow64_guest_address_space_size &&
                                                       candidate_end > wow64_guest_rebase_default;
                    if (!overlaps_wow64_window)
                    {
                        base = candidate;
                        break;
                    }

                    ::munmap(candidate, arena_size);
                }

                if (base == MAP_FAILED)
                {
                    throw std::runtime_error("Failed to reserve FEXCore-internal host arena");
                }

                this->base_ = reinterpret_cast<uintptr_t>(base);
                this->cursor_ = this->base_;

                FEXCore::Allocator::mmap = &fex_internal_arena::hook_mmap;
                FEXCore::Allocator::munmap = &fex_internal_arena::hook_munmap;
            }

            uintptr_t base() const
            {
                return this->base_;
            }

            size_t size() const
            {
                return arena_size;
            }

            bool active() const
            {
                return this->base_ != 0;
            }

            // Must be enabled before install(): in HVF mode the host never executes JIT output
            // (only the guest does, via stage-2 EXEC), so executable requests are committed plain
            // RW without MAP_JIT, and every committed sub-range is mirrored into the VM.
            void enable_hvf_mode()
            {
                this->hvf_mode_ = true;
            }

          private:
            bool hvf_mode_ = false;
            uintptr_t base_ = 0;
            uintptr_t cursor_ = 0; // bump pointer, monotonically increasing within the arena
            std::mutex lock_;

            struct free_block
            {
                uintptr_t addr;
                size_t size;
            };

            std::vector<free_block> free_list_;

            bool owns(const void* p) const
            {
                const auto a = reinterpret_cast<uintptr_t>(p);
                return this->base_ != 0 && a >= this->base_ && a < this->base_ + arena_size;
            }

            // Reserve `size` bytes of VA inside the arena. Reuses a freed block (first fit, splitting
            // any remainder back onto the free list) before extending the bump cursor. Returns 0 on
            // exhaustion. Caller holds lock_.
            uintptr_t reserve(const size_t size)
            {
                for (auto it = this->free_list_.begin(); it != this->free_list_.end(); ++it)
                {
                    if (it->size >= size)
                    {
                        const auto addr = it->addr;
                        if (it->size > size)
                        {
                            it->addr += size;
                            it->size -= size;
                        }
                        else
                        {
                            this->free_list_.erase(it);
                        }
                        return addr;
                    }
                }

                if (this->cursor_ + size > this->base_ + arena_size)
                {
                    return 0;
                }
                const auto addr = this->cursor_;
                this->cursor_ += size;
                return addr;
            }

            void* allocate(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
            {
                // Only kernel-choose anonymous requests need steering into the arena. Fixed-address or
                // file-backed requests are honored verbatim - FEXCore issues a fixed-address request
                // only where it deliberately targets a specific base it has already reserved itself.
                if (addr != nullptr || (flags & MAP_ANONYMOUS) == 0)
                {
                    return ::mmap(addr, length, prot, flags, fd, offset);
                }

                const size_t rounded = host_page_align_up_apple(length);

                std::lock_guard<std::mutex> guard(this->lock_);
                const uintptr_t slot = this->reserve(rounded);
                if (slot == 0)
                {
                    fprintf(stderr,
                            "[FEX backend] FATAL: FEXCore-internal host arena exhausted (%zu MiB); "
                            "refusing an unconstrained host mmap that could alias guest memory\n",
                            arena_size >> 20);
                    errno = ENOMEM;
                    return MAP_FAILED;
                }

                if ((flags & MAP_JIT) && this->hvf_mode_)
                {
                    // Same trailing-guard trick as the MAP_JIT branch below, but committed plain RW:
                    // the guest is the only executor of this memory, via its stage-2 EXEC mapping.
                    const size_t exec_size = rounded > host_page_size_apple ? rounded - host_page_size_apple : rounded;
                    void* result = ::mmap(reinterpret_cast<void*>(slot), exec_size, PROT_READ | PROT_WRITE, (flags & ~MAP_JIT) | MAP_FIXED,
                                          fd, offset);
                    if (result != reinterpret_cast<void*>(slot))
                    {
                        this->free_list_.push_back({slot, rounded});
                        fprintf(stderr, "[FEX backend] FATAL: could not commit an HVF code buffer inside the FEXCore arena\n");
                        errno = ENOMEM;
                        return MAP_FAILED;
                    }
                    g_hvf->map(slot, exec_size, PROT_READ | PROT_WRITE | PROT_EXEC);
                    return result;
                }

                if (flags & MAP_JIT)
                {
                    // FEXCore's CodeBuffer sizes its request to include a trailing guard page at the
                    // very end (see CPUBackend.h's UsableSize(): AllocatedSize - FEX_HOST_PAGE_SIZE) and
                    // tries to mprotect(PROT_NONE) that last page itself. That mprotect() call always
                    // fails on Apple Silicon (EACCES) - MAP_JIT protection can only be fixed at mmap()
                    // time, not adjusted after the fact by an ordinary mprotect() - which is exactly the
                    // "Failed to mprotect last page of code buffer" diagnostic CPUBackend.cpp logs.
                    // Provide a REAL guard here instead: make only the leading portion (excluding that
                    // final host page) the actual executable MAP_JIT mapping, and simply never touch the
                    // trailing page - it stays part of the arena's permanent PROT_NONE reservation, which
                    // genuinely faults on any access, landing exactly where UsableSize() already expects
                    // the buffer to end. `Ptr`/`AllocatedSize` as seen by the caller are unaffected; only
                    // how much of that byte range is truly writable/executable changes. Skipped for
                    // requests no bigger than one host page (e.g. Dispatcher's fixed-size, guardless
                    // buffer) so a small allocation isn't shrunk into uselessness.
                    const size_t exec_size = rounded > host_page_size_apple ? rounded - host_page_size_apple : rounded;

                    // MAP_JIT | MAP_FIXED is rejected on Apple, so punch a hole in the arena and place
                    // the executable mapping there via a (non-fixed) address hint the kernel honors.
                    ::munmap(reinterpret_cast<void*>(slot), exec_size);
                    void* result = ::mmap(reinterpret_cast<void*>(slot), exec_size, prot, flags, fd, offset);
                    if (result == reinterpret_cast<void*>(slot))
                    {
                        return result;
                    }

                    // The kernel didn't honor the hint. A mapping outside the arena would reintroduce
                    // the aliasing hazard, so fail loudly instead of handing it back.
                    if (result != MAP_FAILED)
                    {
                        ::munmap(result, exec_size);
                    }
                    // Restore the arena's PROT_NONE reservation over the whole slot (including the guard
                    // portion) so it never becomes an unmapped gap the guest could be handed.
                    ::mmap(reinterpret_cast<void*>(slot), rounded, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                    this->free_list_.push_back({slot, rounded});
                    fprintf(stderr, "[FEX backend] FATAL: could not place MAP_JIT code buffer inside the FEXCore arena\n");
                    errno = ENOMEM;
                    return MAP_FAILED;
                }

                // Non-executable: commit directly over the reserved region.
                void* result = ::mmap(reinterpret_cast<void*>(slot), rounded, prot, flags | MAP_FIXED, fd, offset);
                if (this->hvf_mode_ && result == reinterpret_cast<void*>(slot) && prot != PROT_NONE)
                {
                    g_hvf->map(slot, rounded, prot);
                }
                return result;
            }

            int release(void* addr, size_t length)
            {
                if (!this->owns(addr))
                {
                    return ::munmap(addr, length);
                }

                const size_t rounded = host_page_align_up_apple(length);

                std::lock_guard<std::mutex> guard(this->lock_);
                if (this->hvf_mode_)
                {
                    g_hvf->unmap(reinterpret_cast<uint64_t>(addr), rounded);
                }
                // Return the region to the reserved (PROT_NONE) state so it stays part of the arena's
                // contiguous reservation, and record it for reuse. Arena VA is never returned to the OS.
                void* r = ::mmap(addr, rounded, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                if (r == addr)
                {
                    this->free_list_.push_back({reinterpret_cast<uintptr_t>(addr), rounded});
                    return 0;
                }
                return r == MAP_FAILED ? -1 : 0;
            }

            static void* hook_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
            {
                return instance().allocate(addr, length, prot, flags, fd, offset);
            }

            static int hook_munmap(void* addr, size_t length)
            {
                return instance().release(addr, length);
            }
        };
#endif

        // A registered WoW64 bitness mode-switch point (see x86_emulator::register_gate_crossing).
        struct gate_crossing
        {
            uint64_t address = 0;
            size_t size = 0;
            x86_64_cpu::gate_crossing_kind kind = x86_64_cpu::gate_crossing_kind::heaven_gate;
        };

    } // namespace

    // The collision exception thrown by reserve_guest_address_range/sync_host_page_apple's
    // first-claim paths must be sogen::host_memory_collision from memory_interface.hpp - the type
    // the catch sites in memory_manager/module_mapping/section syscalls are compiled against. A
    // backend-local equivalent class is NOT interchangeable: it is a different type, so every catch
    // falls through to a broader catch (const std::exception&) and a recoverable placement
    // collision becomes a fatal syscall failure.
    using sogen::host_memory_collision;

    // -----------------------------------------------------------------------------------------------
    // The syscall handler bridges FEX's guest `syscall` exits to the registered instruction hook.
    // Method bodies are defined out of line (after fex_x86_64_emulator and fex_vcpu are complete)
    // since they touch both classes' internals.
    // -----------------------------------------------------------------------------------------------
    class fex_syscall_handler final : public FEXCore::HLE::SyscallHandler
    {
      public:
        explicit fex_syscall_handler(fex_x86_64_emulator& emulator)
            : emulator_(emulator)
        {
            // OS_GENERIC: FEX does no JIT-side syscall argument handling and spills/fills all registers,
            // which is what we want since the syscall is serviced entirely by sogen's own hook.
            this->OSABI = FEXCore::HLE::SyscallOSABI::OS_GENERIC;
        }

        uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* frame, FEXCore::HLE::SyscallArguments* args) override;
        FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* thread, uint64_t address) override;
        std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState* thread,
                                                                                      uint64_t guest_addr) override;

      private:
        fex_x86_64_emulator& emulator_;
    };

    // -----------------------------------------------------------------------------------------------
    // fex_vcpu: owns everything describing one running guest vCPU's execution state, mirroring
    // whp_x86_64_emulator's whp_vcpu. Method bodies that touch fex_x86_64_emulator's internals are
    // defined out of line, after that class is complete.
    // -----------------------------------------------------------------------------------------------
    class fex_vcpu final : public x86_64_cpu
    {
      public:
        explicit fex_vcpu(fex_x86_64_emulator& emulator, size_t index)
            : emulator_(emulator),
              index_(index)
        {
        }

        ~fex_vcpu() override;

        size_t index() const override
        {
            return this->index_;
        }

        memory_interface& memory() override;
        const memory_interface& memory() const override;

        void start(size_t count) override;
        void stop() override;

        size_t read_raw_register(int reg, void* value, size_t size) override;
        size_t write_raw_register(int reg, const void* value, size_t size) override;

        bool read_descriptor_table(int reg, descriptor_table_register& table) override;

        std::vector<std::byte> save_registers() const override;
        void restore_registers(const std::vector<std::byte>& register_data) override;

        bool has_violation() const override
        {
            return false;
        }

        bool supports_instruction_counting() const override
        {
            return false;
        }

        // FEXCore's INT3 handling (OpcodeDispatcher.cpp) sets SetRIPToNext, so the RIP observed once
        // the breakpoint fault surfaces here is already one past the 0xCC byte - unlike KVM/WHP, which
        // both catch INT3 at the instruction's own (pre-advance) address.
        bool reports_breakpoint_rip_past_instruction() const override
        {
            return true;
        }

        // FEXCore maintains separate context_/thread_ (64-bit) and context32_/thread32_ (32-bit)
        // engines for a WoW64 process, only one of which is active at a time.
        bool has_separate_bitness_engines() const override
        {
            return true;
        }

        bool is_stop_thread_safe() const override
        {
            return true;
        }

        void set_segment_base(x86_register base, pointer_type value) override;
        pointer_type get_segment_base(x86_register base) override;
        void load_gdt(pointer_type address, uint32_t limit) override;

        void notify_process_bitness(bool is_wow64_process) override;
        void register_gate_crossing(pointer_type address, size_t size, gate_crossing_kind kind) override;
        void set_wow64_turbo_dispatch_end(pointer_type address) override;

        // --[ fex_vcpu-internal, called from fex_x86_64_emulator/fex_syscall_handler ]---------------

        FEXCore::Core::CPUState& cpu_state();
        const FEXCore::Core::CPUState& cpu_state() const;
        uint64_t read_rflags() const;
        void write_rflags(uint64_t rflags);
        void request_thread_stop();
        void create_thread();

#ifdef __APPLE__
        bool handle_fault_signal(int sig, siginfo_t* info, void* raw_ucontext);
#endif

        std::atomic<bool> stop_requested_{false};

      private:
        friend class fex_x86_64_emulator;
        friend class fex_syscall_handler;

        void create_thread32();
        void ensure_callret_buffer(FEXCore::Core::CPUState& state);
        void ensure_callret_stack(FEXCore::Core::CPUState& state);
        void restore_state_into(FEXCore::Core::InternalThreadState* thread, const std::byte* src);
        void mark_executable_range(uint64_t address, size_t size, memory_permission permissions);
        void invalidate_code_range_in(FEXCore::Context::Context* context, FEXCore::Core::InternalThreadState* thread, uint64_t address,
                                      size_t size) const;
        void invalidate_code_range(uint64_t address, size_t size, bool include_inactive_contexts = false) const;
        uint16_t segment_selector(int index) const;
        void set_segment_selector(int index, const void* value, size_t size);

#ifdef __APPLE__
        int& jit_write_protect_retry_count_for(uint64_t fault_addr);
        bool perform_gate_crossing(const gate_crossing& gate);
        bool enter_wow64_32bit_from_run_simulated_code(const gate_crossing& gate);
        bool enter_wow64_64bit_from_wow64svc_thunk(const gate_crossing& gate);
        bool enter_bitness_switch_from_far_jmp(const gate_crossing& gate);
        bool perform_bitness_switch(uint64_t target_rip, uint64_t target_rsp, uint16_t target_cs);
        void complete_decoded_load(ucontext_t* uctx, const decoded_arm64_load& decoded, const void* data, uint64_t pc);
        bool handle_mmio_fault(ucontext_t* uctx, const mmio_region& region, uint64_t fault_addr);
        bool handle_misaligned_atomic_fault(ucontext_t* uctx, uint64_t fault_addr);
        bool handle_callret_stack_fault(ucontext_t* uctx, uint64_t fault_addr) const;
        bool handle_general_memory_violation(ucontext_t* uctx, uint64_t fault_addr);
        bool host_pc_in_any_dispatcher(uint64_t pc) const;

        // See pending_fault_kind's doc comment (declared here, used by dispatch_pending_hook_if_any/
        // defer_hook_dispatch below): memory_violation_hooks_/interrupt_hooks_ callbacks are shared,
        // backend-agnostic windows-emulator code that allocates, logs, and mutates STL containers
        // freely - safe when invoked from normal call context, but NOT safe to call directly from
        // inside handle_fault_signal, a real kernel-delivered SIGSEGV/SIGBUS/SIGILL handler that can
        // interrupt an unrelated malloc()/free() or STL mutation already in progress on this thread.
        // Instead of calling hooks in-handler, stash what's needed here (plain data, no allocation)
        // and force ExecuteThread to unwind back to start() (via ThreadStopHandlerAddress), which
        // dispatches the hook in normal context and resumes guest execution by simply re-entering
        // ExecuteThread.
        enum class pending_fault_kind
        {
            none,
            memory_violation,
            interrupt,
            // A WoW64 gate crossing already performed the state marshal + active_context_/
            // active_thread_ flip inside handle_fault_signal; this only tells start()'s loop to
            // resume (re-enter ExecuteThread on the now-active engine) rather than break. No hook
            // runs.
            gate_crossing,
        };

        struct pending_fault_dispatch
        {
            pending_fault_kind kind = pending_fault_kind::none;
            uint64_t address = 0;
            size_t size = 0;
            memory_operation operation{};
            memory_violation_type type{};
            int vector = 0;
        };

        bool dispatch_pending_hook_if_any();
        void defer_hook_dispatch(ucontext_t* uctx, const pending_fault_dispatch& dispatch, bool sra_already_spilled);

        // --[ HVF execution path (active only when g_hvf != nullptr) ]-------------------------------

        struct hvf_exit_adapter final : hvf::hvf_exit_handler
        {
            explicit hvf_exit_adapter(fex_vcpu& vcpu)
                : vcpu_(vcpu)
            {
            }

            bool on_stage2_abort(hvf::hvf_vcpu_executor& vcpu, uint64_t va, uint64_t ipa, uint64_t syndrome) override
            {
                return this->vcpu_.hvf_on_stage2_abort(vcpu, va, ipa, syndrome);
            }

            bool on_guest_exception(hvf::hvf_vcpu_executor& vcpu, uint32_t vector_entry) override
            {
                return this->vcpu_.hvf_on_guest_exception(vcpu, vector_entry);
            }

            fex_vcpu& vcpu_;
        };

        void start_hvf(size_t count);
        void hvf_shim_thread_pointers(FEXCore::Core::CpuStateFrame& frame) const;
        bool hvf_on_stage2_abort(hvf::hvf_vcpu_executor& vcpu, uint64_t va, uint64_t ipa, uint64_t syndrome);
        bool hvf_on_guest_exception(hvf::hvf_vcpu_executor& vcpu, uint32_t vector_entry);
        void hvf_complete_decoded_load(hvf::hvf_vcpu_executor& vcpu, const decoded_arm64_load& decoded, const void* data,
                                       uint64_t pc) const;
        bool hvf_handle_mmio_fault(hvf::hvf_vcpu_executor& vcpu, const mmio_region& region, uint64_t guest_fault_addr, uint64_t pc) const;
        bool hvf_handle_misaligned_atomic_fault(hvf::hvf_vcpu_executor& vcpu, uint64_t fault_addr, uint64_t pc);
        bool hvf_handle_callret_stack_fault(hvf::hvf_vcpu_executor& vcpu, uint64_t fault_addr) const;
        bool hvf_handle_general_memory_violation(hvf::hvf_vcpu_executor& vcpu, uint64_t fault_addr, uint64_t pc,
                                                 memory_operation operation);
        void hvf_defer_hook_dispatch(hvf::hvf_vcpu_executor& vcpu, const pending_fault_dispatch& dispatch, bool sra_already_spilled);

        std::unique_ptr<hvf::hvf_vcpu_executor> hvf_executor_;
        // request_thread_stop() kicks the vCPU from arbitrary threads (quantum timer); the unique_ptr
        // above is only ever touched by the owning worker thread, this mirror is the cross-thread view.
        std::atomic<hvf::hvf_vcpu_executor*> hvf_executor_for_kick_{nullptr};
        uint64_t hvf_emulator_stack_top_ = 0;

        pending_fault_dispatch pending_fault_dispatch_{};
        // Set by handle_fault_signal when it unwinds ExecuteThread through an InterruptFaultPage hit;
        // consumed by start()'s loop to tell that unwind apart from any other clean return. No atomics:
        // the signal handler runs on the same host thread whose start() consumes the flag.
        bool interrupt_page_unwind_ = false;

        // Per-vCPU alternate signal stack, registered on first entry into start() on this vCPU's own
        // host thread (a single shared static buffer, as a single-host-thread cooperative model used,
        // does not cover every vCPU worker's own host thread under real multi-vCPU concurrency).
        std::array<std::byte, 64 * 1024> alt_stack_{};

        // Per-vCPU (not a file-scope global): under real multi-vCPU concurrency more than one host
        // thread can be mid-retry at once, and a shared array would race across vCPUs.
        std::array<jit_write_protect_retry_slot, jit_write_protect_retry_slot_count> jit_write_protect_retry_slots_{};
        size_t jit_write_protect_retry_next_evict_ = 0;
#endif

        fex_x86_64_emulator& emulator_;
        size_t index_ = 0;

        // The always-64-bit FEXCore::Context - see notify_process_bitness's doc comment. Per-vCPU
        // pointer to the shared, machine-wide context this vCPU's thread_ belongs to.
        FEXCore::Core::InternalThreadState* thread_ = nullptr;
        FEXCore::Core::InternalThreadState* thread32_ = nullptr;

        // Whichever context/thread is *currently executing* on this vCPU - starts out equal to
        // context_/thread_ (execution always begins on the 64-bit engine) and is flipped by the gate
        // crossings; it is what every JIT-operation call site below actually uses. Atomic: the
        // quantum-timer watchdog reads/writes active_thread_ cross-thread via stop()/
        // request_thread_stop() (a missed stop is benign - the watchdog refires next quantum, the same
        // window that existed under single-vCPU).
        FEXCore::Context::Context* active_context_ = nullptr;
        std::atomic<FEXCore::Core::InternalThreadState*> active_thread_{nullptr};

        FEXCore::Core::CPUState staged_state_{};

        // Per-vCPU GDT (gdt_base_for_vcpu() in process_context.hpp gives each vCPU its own GDT page
        // specifically so one WoW64 thread's FS descriptor (TEB32 base) can never be read from
        // another vCPU) - must live here, not on the shared emulator, or whichever vCPU calls
        // load_gdt() last determines every vCPU's segment table.
        uint64_t gdt_base_ = 0;
        uint32_t gdt_limit_ = 0;
    };

    // -----------------------------------------------------------------------------------------------
    // The emulator itself: machine-wide state (both FEXCore contexts, memory-region/MMIO/gate-
    // crossing tables, hook maps) shared by every vCPU, plus the vcpus_ facade.
    // -----------------------------------------------------------------------------------------------
    class fex_x86_64_emulator final : public x86_64_emulator
    {
      public:
        explicit fex_x86_64_emulator(const size_t vcpu_count)
        {
            if (vcpu_count < 1)
            {
                throw std::runtime_error("FEX backend requires at least one vCPU");
            }

            this->initialize_context();

#ifdef __APPLE__
            if (g_hvf != nullptr)
            {
                const auto max_vcpus = g_hvf->max_vcpu_count();
                if (vcpu_count > max_vcpus)
                {
                    char buf[160];
                    snprintf(buf, sizeof(buf), "EMULATOR_FEX_HVF=1 supports at most %u vCPUs on this machine (%zu requested)", max_vcpus,
                             vcpu_count);
                    throw std::runtime_error(buf);
                }
            }
#endif

            this->vcpus_.reserve(vcpu_count);
            for (size_t i = 0; i < vcpu_count; ++i)
            {
                this->vcpus_.push_back(std::make_unique<fex_vcpu>(*this, i));
            }
        }

        ~fex_x86_64_emulator() override
        {
            utils::reset_object_with_delayed_destruction(this->memory_read_hooks_);
            utils::reset_object_with_delayed_destruction(this->memory_write_hooks_);
            utils::reset_object_with_delayed_destruction(this->memory_execution_hooks_);
            utils::reset_object_with_delayed_destruction(this->memory_violation_hooks_);
            utils::reset_object_with_delayed_destruction(this->interrupt_hooks_);
            utils::reset_object_with_delayed_destruction(this->basic_block_hooks_);
            utils::reset_object_with_delayed_destruction(this->instruction_hooks_);

            // Destroy every vCPU's threads before the owning contexts are torn down.
            for (auto& vcpu : this->vcpus_)
            {
                if (vcpu->thread_ != nullptr && this->context_)
                {
#ifdef __APPLE__
                    if (g_hvf != nullptr)
                    {
                        g_hvf->unmap(reinterpret_cast<uint64_t>(vcpu->thread_), sizeof(FEXCore::Core::InternalThreadState));
                    }
#endif
                    this->context_->DestroyThread(vcpu->thread_);
                    vcpu->thread_ = nullptr;
                }
                if (vcpu->thread32_ != nullptr && this->context32_)
                {
#ifdef __APPLE__
                    if (g_hvf != nullptr)
                    {
                        g_hvf->unmap(reinterpret_cast<uint64_t>(vcpu->thread32_), sizeof(FEXCore::Core::InternalThreadState));
                    }
#endif
                    this->context32_->DestroyThread(vcpu->thread32_);
                    vcpu->thread32_ = nullptr;
                }
            }
            this->vcpus_.clear();

            // Release everything we mmap'd into the (host == guest, modulo wow64_guest_rebase in
            // 32-bit mode) address space.
#ifdef __APPLE__
            // All host mappings this backend owns are tracked as 16KB host pages (see
            // mapped_host_pages_apple_), including PROT_NONE reservation claims that no regions_
            // entry covers; map_host_memory aliases (owned=false) are never in this set. Darwin's
            // munmap also requires host-page alignment, which regions_ entries don't guarantee.
            for (const auto host_page : this->mapped_host_pages_apple_)
            {
                const auto rebase = rebase_for(this->is_wow64_process_, host_page);
                if (g_hvf != nullptr)
                {
                    g_hvf->sync_page(host_page + rebase, PROT_NONE);
                }
                if (rebase != 0 && this->wow64_host_window_reserved_)
                {
                    // Covered by the whole-window munmap below.
                    continue;
                }
                ::munmap(reinterpret_cast<void*>(host_page + rebase), host_page_size_apple);
            }

            if (this->wow64_host_window_reserved_)
            {
                ::munmap(reinterpret_cast<void*>(this->wow64_guest_rebase_), wow64_guest_address_space_size);
            }

            if (g_active_emulator == this)
            {
                g_active_emulator = nullptr;
            }
#else
            for (const auto& [address, region] : this->regions_)
            {
                if (region.owned)
                {
                    const auto rebase = rebase_for(this->is_wow64_process_, address);
                    ::munmap(reinterpret_cast<void*>(address + rebase), region.size);
                }
            }
#endif

            // Per-logical-thread call-ret buffers (see ensure_callret_buffer) live in host allocator
            // space, not regions_, and are never freed as individual guest threads exit - release them
            // all here so they don't outlive this emulator instance.
            for (const auto& [alloc_base, alloc_size] : this->callret_buffers_)
            {
                FEXCore::Allocator::munmap(alloc_base, alloc_size);
            }
        }

        // --[ vcpu facade ]--------------------------------------------------------------------------

        size_t vcpu_count() const override
        {
            return this->vcpus_.size();
        }

        x86_64_cpu& get_cpu(const size_t index) override
        {
            if (index >= this->vcpus_.size())
            {
                throw std::out_of_range("Invalid vCPU index");
            }
            return *this->vcpus_[index];
        }

        // --[ cpu_interface / x86_cpu, forwarded to vcpus_[0] for single-facade callers ]------------

        size_t index() const override
        {
            return 0;
        }

        memory_interface& memory() override
        {
            return *this;
        }

        const memory_interface& memory() const override
        {
            return *this;
        }

        void start(const size_t count) override
        {
            this->vcpus_[0]->start(count);
        }

        void stop() override
        {
            this->vcpus_[0]->stop();
        }

        size_t read_raw_register(const int reg, void* value, const size_t size) override
        {
            return this->vcpus_[0]->read_raw_register(reg, value, size);
        }

        size_t write_raw_register(const int reg, const void* value, const size_t size) override
        {
            return this->vcpus_[0]->write_raw_register(reg, value, size);
        }

        bool read_descriptor_table(const int reg, descriptor_table_register& table) override
        {
            return this->vcpus_[0]->read_descriptor_table(reg, table);
        }

        std::vector<std::byte> save_registers() const override
        {
            return this->vcpus_[0]->save_registers();
        }

        void restore_registers(const std::vector<std::byte>& register_data) override
        {
            this->vcpus_[0]->restore_registers(register_data);
        }

        bool has_violation() const override
        {
            return this->vcpus_[0]->has_violation();
        }

        bool supports_instruction_counting() const override
        {
            return false;
        }

        bool reports_breakpoint_rip_past_instruction() const override
        {
            return true;
        }

        bool has_separate_bitness_engines() const override
        {
            return true;
        }

        bool is_stop_thread_safe() const override
        {
            return true;
        }

        void set_segment_base(const x86_register base, const pointer_type value) override
        {
            this->vcpus_[0]->set_segment_base(base, value);
        }

        pointer_type get_segment_base(const x86_register base) override
        {
            return this->vcpus_[0]->get_segment_base(base);
        }

        void load_gdt(const pointer_type address, const uint32_t limit) override
        {
            this->vcpus_[0]->load_gdt(address, limit);
        }

        void notify_process_bitness(bool is_wow64_process) override
        {
            this->vcpus_[0]->notify_process_bitness(is_wow64_process);
        }

        void register_gate_crossing(const pointer_type address, const size_t size, const gate_crossing_kind kind) override
        {
            this->vcpus_[0]->register_gate_crossing(address, size, kind);
        }

        void set_wow64_turbo_dispatch_end(const pointer_type address) override
        {
            this->vcpus_[0]->set_wow64_turbo_dispatch_end(address);
        }

        // --[ emulator ]-----------------------------------------------------------------------------

        std::string get_name() const override
        {
            return "FEX";
        }

        bool supports_multiple_vcpus() const override
        {
            // Real multi-vCPU support: FEXCore drives N InternalThreadStates per bitness context,
            // one per fex_vcpu, all genuinely concurrent host threads.
            return true;
        }

        // A vCPU worker thread's own OS-chosen default stack is ordinary host memory, placed wherever
        // the OS likes - under this backend's guest-VA==host-VA model, a new worker thread's stack can
        // coincidentally land on an address the guest program is about to use (structurally impossible
        // in single-vCPU mode, where the one thread either reuses the pre-existing main thread's stack
        // or is a single dynamically-placed allocation that never races guest memory). Hand back a
        // pre-reserved, host-only stack region from a dedicated arena instead.
        bool reserve_worker_thread_stack(size_t vcpu_index, void*& stack_base, size_t& stack_size) override;

        void serialize_state(utils::buffer_serializer& buffer, bool /*is_snapshot*/) const override
        {
            buffer.write_vector(this->save_registers());
            // TODO(fex): a full snapshot should also persist the mapped-memory layout and contents so a
            // restore can re-mmap and refill the (host == guest) address space. Registers-only for now.
        }

        void deserialize_state(utils::buffer_deserializer& buffer, bool /*is_snapshot*/) override
        {
            this->restore_registers(buffer.read_vector<std::byte>());
        }

        // --[ memory_interface (public) ]------------------------------------------------------------

        void read_memory(uint64_t address, void* data, size_t size) const override
        {
            if (!this->try_read_memory(address, data, size))
            {
                throw std::runtime_error("Failed to read FEX guest memory");
            }
        }

        bool try_read_memory(uint64_t address, void* data, size_t size) const override
        {
            if (!this->is_range_mapped(address, size))
            {
                return false;
            }
            // Guest VA == host VA under FEX for 64-bit guests. For 32-bit (WoW64) guests, real host
            // memory instead lives at address + wow64_guest_rebase (see that constant's doc comment) -
            // regions_/is_range_mapped stay keyed by the plain guest address throughout.
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            // memmove, not memcpy: callers (e.g. the PE loader's section copy) can hand this a source
            // buffer that overlaps the guest destination range - overlapping memcpy is undefined
            // behaviour. This is a generic guest memory-copy primitive with no non-overlap contract.
            std::memmove(data, reinterpret_cast<const void*>(address + rebase), size);
            return true;
        }

        void write_memory(uint64_t address, const void* data, size_t size) override
        {
            if (!this->try_write_memory(address, data, size))
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "Failed to write FEX guest memory at 0x%llx size=0x%zx wow64=%d",
                         static_cast<unsigned long long>(address), size, this->is_wow64_process_ ? 1 : 0);
                throw std::runtime_error(buf);
            }
        }

        bool try_write_memory(uint64_t address, const void* data, size_t size) override
        {
            return this->try_write_memory_impl(address, data, size, /*invalidate_translations=*/true);
        }

        // Writes WoW64 gate-crossing CPU-state marshaling data (the CpuArea i386-CONTEXT block at
        // TEB64+0x1488+0x80, and the 64-bit RunSimulatedCode stack frame) into guest memory. These
        // targets are pure data structures that never hold JIT-compiled guest code, so the code-cache
        // invalidation try_write_memory normally performs is a guaranteed no-op in every FEXCore
        // context - skip it to avoid a GetCodeInvalidationMutex lock + Apple pthread_jit_write_protect
        // toggle pair per marshaled register (~25 per WoW64 syscall round-trip). If such an address
        // were ever repurposed as code, map_memory/apply_memory_protection would invalidate independently.
        bool write_marshal_state(uint64_t address, const void* data, size_t size)
        {
            return this->try_write_memory_impl(address, data, size, /*invalidate_translations=*/false);
        }

        // Bug 3 fix: this used to take only a shared_lock, reasoning it was "just reading the
        // bookkeeping tables" - actually wrong: it can call set_temporary_write_access, which does
        // real ::mprotect() toggles on the actual host page, and Apple's memory management works at
        // 16KB host-page granularity, so two unrelated guest addresses written by two different vCPUs
        // can share the same underlying host page. A shared lock let two such calls interleave: one
        // vCPU's "revert to declared permissions" could land mid-memmove of another vCPU's still-in-
        // flight write. A unique lock excludes concurrent readers too, which is exactly what's needed.
        bool try_write_memory_impl(uint64_t address, const void* data, size_t size, bool invalidate_translations)
        {
            // tables_mutex_ is deliberately released (see the closing brace below) BEFORE
            // invalidate_code_range_locked runs, never held across it: FEXCore's own compile path
            // holds GetCodeInvalidationMutex (shared) and calls back into QueryGuestExecutableRange,
            // which needs tables_mutex_ (shared) - the reverse acquisition order from holding
            // tables_mutex_ into GetCodeInvalidationMutex here, a real ABBA deadlock hit on the very
            // first genuine --vcpus 2 run.
            {
                const std::unique_lock lock(this->tables_mutex_);

                if (!this->is_range_mapped(address, size))
                {
                    return false;
                }

                // sogen's own loader writes guest memory it has already declared read-only (e.g. a PE
                // section's raw file bytes, before/regardless of the section's final protection).
                // Unlike Unicorn's uc_mem_write, which operates on emulated memory with no real host
                // enforcement, FEX's guest-VA==host-VA model is backed by actual host mprotect state,
                // so such a write needs a temporary permission bump around the memcpy. Every page of
                // the range must be checked, not just the first: a write straddling into a read-only
                // region would otherwise fault mid-memmove on the host instead of taking the bump.
                const bool needs_temporary_write = !this->range_is_writable(address, size);

                if (needs_temporary_write)
                {
                    this->set_temporary_write_access(address, size, true);
                }

                // See try_read_memory's doc comment on wow64_guest_rebase.
                const auto rebase = rebase_for(this->is_wow64_process_, address);
                // memmove, not memcpy: see try_read_memory - the source may overlap the guest destination.
                std::memmove(reinterpret_cast<void*>(address + rebase), data, size);

                if (needs_temporary_write)
                {
                    this->set_temporary_write_access(address, size, false);
                }
            }

            if (invalidate_translations)
            {
                // Writing to a mapped region may overwrite already-translated code; drop FEX's cache for it.
                this->invalidate_code_range_locked(address, size);
            }
            return true;
        }

        // --[ hook_interface ]-----------------------------------------------------------------------
        //
        // Like the KVM backend, FEX runs the guest natively, so fine-grained memory/execution/basic-
        // block hooks cannot fire. They are accepted (and tracked, so delete_hook works) for API
        // compatibility. Only instruction hooks for `syscall` are actually wired (see the syscall
        // bridge). Registered once globally (not per-vCPU): every fex_vcpu's hook_*() forwards here,
        // since a hook must fire for whichever vCPU's guest thread triggers it, not just the vCPU it
        // happened to be registered through.

        emulator_hook* hook_memory_execution(memory_execution_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_execution(uint64_t /*address*/, memory_execution_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_range_execution(uint64_t /*address*/, uint64_t /*size*/,
                                                   memory_execution_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_read(uint64_t /*address*/, uint64_t /*size*/, memory_access_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_read_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_write(uint64_t /*address*/, uint64_t /*size*/, memory_access_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_write_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_instruction(int instruction_type, instruction_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            auto& entry = this->instruction_hooks_[hook];
            entry.type = static_cast<x86_hookable_instructions>(instruction_type);
            entry.callback = std::move(callback);
            if (entry.type == x86_hookable_instructions::syscall)
            {
                this->syscall_hook_ = &entry;
            }
            return hook;
        }

        emulator_hook* hook_interrupt(interrupt_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->interrupt_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_violation(memory_violation_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->memory_violation_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_basic_block(basic_block_hook_callback callback) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            auto* hook = this->make_hook();
            this->basic_block_hooks_[hook] = std::move(callback);
            return hook;
        }

        bool supports_global_memory_execution_hooks() const override
        {
            return false;
        }

        void delete_hook(emulator_hook* hook) override
        {
            const std::unique_lock lock(this->tables_mutex_);
            if (this->syscall_hook_ != nullptr)
            {
                const auto it = this->instruction_hooks_.find(hook);
                if (it != this->instruction_hooks_.end() && &it->second == this->syscall_hook_)
                {
                    this->syscall_hook_ = nullptr;
                }
            }

            this->instruction_hooks_.erase(hook);
            this->interrupt_hooks_.erase(hook);
            this->memory_read_hooks_.erase(hook);
            this->memory_write_hooks_.erase(hook);
            this->memory_execution_hooks_.erase(hook);
            this->memory_violation_hooks_.erase(hook);
            this->basic_block_hooks_.erase(hook);
        }

        emulator_hook* make_hook()
        {
            return reinterpret_cast<emulator_hook*>(this->next_hook_id_++);
        }

#ifdef __APPLE__
        std::vector<host_reserved_range> reserved_host_ranges() const override
        {
            // Guest VA == host VA, so this process's own memory (its loaded image, dyld, shared
            // libraries, thread stacks, heap) shares the same address space the guest uses - unlike
            // Unicorn/Icicle/KVM, which sandbox or translate the guest address space independently.
            // Enumerate everything currently mapped in this process via the Mach VM region API (the
            // Darwin equivalent of walking /proc/self/maps) so the memory manager can steer guest
            // allocations away from it. One-shot snapshot: see the interface doc comment for the
            // residual risk of host allocations made after this call.
            std::vector<host_reserved_range> ranges;

            const auto& arena = fex_internal_arena::instance();
            const uint64_t arena_base = arena.base();
            const uint64_t arena_end = arena.active() ? arena_base + arena.size() : 0;
            if (arena.active())
            {
                ranges.push_back({.address = arena_base, .size = arena.size()});
            }

            mach_vm_address_t address = 0;
            bool first_region = true;
            while (true)
            {
                mach_vm_size_t size = 0;
                vm_region_basic_info_data_64_t info{};
                mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object_name = MACH_PORT_NULL;
                const kern_return_t result = mach_vm_region(mach_task_self(), &address, &size, VM_REGION_BASIC_INFO_64,
                                                            reinterpret_cast<vm_region_info_t>(&info), &info_count, &object_name);
                if (result != KERN_SUCCESS)
                {
                    break;
                }

                if (arena.active() && address >= arena_base && address < arena_end)
                {
                    address += size;
                    continue;
                }

                if (this->wow64_host_window_reserved_ && address >= this->wow64_guest_rebase_ &&
                    address < this->wow64_guest_rebase_ + wow64_guest_address_space_size)
                {
                    address += size;
                    continue;
                }

                if (first_region)
                {
                    const uint64_t gap_start = this->is_wow64_process_ ? wow64_guest_address_space_size : 0;
                    if (address > gap_start)
                    {
                        ranges.push_back({.address = gap_start, .size = static_cast<size_t>(address - gap_start)});
                    }
                    first_region = false;
                }

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
                constexpr mach_vm_size_t asan_shadow_region_threshold = 0x100000000ULL; // 4 GiB
                if (size >= asan_shadow_region_threshold)
                {
                    address += size;
                    continue;
                }
#endif
#endif

                ranges.push_back({.address = address, .size = static_cast<size_t>(size)});
                address += size;
            }
            return ranges;
        }

        std::vector<host_reserved_range> reserved_host_ranges_in(uint64_t address, size_t size) const override
        {
            std::vector<host_reserved_range> ranges;

            const auto rebase = rebase_for(this->is_wow64_process_, address);

            if (this->wow64_host_window_reserved_ && rebase != 0)
            {
                return ranges;
            }

            const mach_vm_address_t window_start = address + rebase;
            const mach_vm_address_t window_end = window_start + size;

            mach_vm_address_t probe = window_start;
            while (probe < window_end)
            {
                mach_vm_address_t region_addr = probe;
                mach_vm_size_t region_size = 0;
                vm_region_basic_info_data_64_t info{};
                mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object_name = MACH_PORT_NULL;
                if (mach_vm_region(mach_task_self(), &region_addr, &region_size, VM_REGION_BASIC_INFO_64,
                                   reinterpret_cast<vm_region_info_t>(&info), &info_count, &object_name) != KERN_SUCCESS)
                {
                    break;
                }
                if (region_addr >= window_end)
                {
                    break;
                }

                const uint64_t hit_start = std::max<uint64_t>(region_addr, window_start);
                const uint64_t hit_end = std::min<uint64_t>(region_addr + region_size, window_end);
                ranges.push_back({.address = hit_start - rebase, .size = static_cast<size_t>(hit_end - hit_start)});

                probe = region_addr + region_size;
            }

            return ranges;
        }
#endif

        // --[ machine-wide bookkeeping, called by fex_vcpu ]------------------------------------------

        uint64_t rebase_for(bool is_32bit_mode, uint64_t address) const
        {
            return (is_32bit_mode && address < wow64_guest_address_space_size) ? this->wow64_guest_rebase_ : 0ULL;
        }

        uint64_t unrebase_fault_addr(uint64_t fault_addr) const
        {
            if (this->is_wow64_process_ && fault_addr >= this->wow64_guest_rebase_ &&
                fault_addr < this->wow64_guest_rebase_ + wow64_guest_address_space_size)
            {
                return fault_addr - this->wow64_guest_rebase_;
            }
            return fault_addr;
        }

        // Called only under tables_mutex_ (a real, asynchronously-delivered signal can interrupt a
        // shared_lock holder on the same thread while it's re-acquiring for write elsewhere - the
        // fault-handling call sites all take their own lock, never nesting here).
        std::optional<gate_crossing> find_gate_crossing(uint64_t rip) const
        {
            std::shared_lock lock(this->tables_mutex_);
            for (const auto& gate : this->gate_crossings_)
            {
                if (rip >= gate.address && rip < gate.address + gate.size)
                {
                    return gate;
                }
            }
            return std::nullopt;
        }

        static uint32_t gdt_segment_base(FEXCore::Core::CPUState& state, uint16_t selector)
        {
            const auto* segment = FEXCore::Core::CPUState::GetSegmentFromIndex(state, selector);
            return FEXCore::Core::CPUState::CalculateGDTBase(*segment);
        }

        void ensure_context32()
        {
            const std::unique_lock lock(this->tables_mutex_);
            if (this->context32_)
            {
                return;
            }

            FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "0");

#ifdef __APPLE__
            const FEXCore::HostFeatures features = fetch_host_features_apple();
#else
            const FEXCore::HostFeatures features{};
#endif
            this->context32_ = FEXCore::Context::Context::CreateNewContext(features);
            this->context32_->SetWow64GuestRebaseValue(this->wow64_guest_rebase_);
#ifdef __APPLE__
            if (g_hvf != nullptr)
            {
                this->context32_->SetHardwareTSOSupport(true);
            }
#endif

            this->syscall_handler32_ = std::make_unique<fex_syscall_handler>(*this);
            this->context32_->SetSyscallHandler(this->syscall_handler32_.get());

            this->signal_delegator32_ = std::make_unique<FEXCore::SignalDelegator>();
            this->context32_->SetSignalDelegator(this->signal_delegator32_.get());

            this->context32_->InitCore();

            FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
        }

      private:
        friend class fex_vcpu;
        friend class fex_syscall_handler;

        void map_mmio(uint64_t address, size_t size, mmio_read_callback read_cb, mmio_write_callback /*write_cb*/) override
        {
            const std::unique_lock lock(this->tables_mutex_);

            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX MMIO mappings must be page aligned");
            }

            void* host_backing = nullptr;
            size_t host_backing_size = 0;

#ifdef __APPLE__
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            host_backing_size = host_page_align_up_apple(size);
            void* result = ::mmap(reinterpret_cast<void*>(address + rebase), host_backing_size, PROT_READ | PROT_WRITE,
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
            if (result != MAP_FAILED && result == reinterpret_cast<void*>(address + rebase))
            {
                host_backing = result;
            }
#endif

            if (host_backing != nullptr)
            {
                read_cb(0, host_backing, size);
                ::mprotect(host_backing, host_backing_size, PROT_READ);
#ifdef __APPLE__
                if (g_hvf != nullptr)
                {
                    // Read-only inside the VM for the whole region lifetime: the once-per-quantum
                    // refresh writes through the host mapping (same physical pages), so the guest
                    // sees fresh content without any stage-2 churn, and guest writes abort exactly
                    // like the host-mprotect path faults today.
                    g_hvf->map(reinterpret_cast<uint64_t>(host_backing), host_backing_size, PROT_READ);
                }
#endif

                // KUSD-collision fix: register the covering host page(s) in mapped_host_pages_apple_
                // (without a page_shadow_apple_ entry) right after establishing the real backing.
                // memory_manager's own MMIO reservation is 4KB-page-granular with no notion of
                // Apple's 16KB host-page granularity, so a different, unrelated guest allocation can
                // legitimately land in the unused remainder of this region's 16KB host page while
                // still looking "free" - and its own first-claim path (sync_host_page_apple) would
                // otherwise see !currently_mapped and claim the whole page fresh, silently destroying
                // this real backing content. Registering it here routes a later co-resident
                // allocation through the ordinary shared-page mprotect reconciliation path instead.
#ifdef __APPLE__
                for (uint64_t host_page = host_page_align_down_apple(address); host_page < address + host_backing_size;
                     host_page += host_page_size_apple)
                {
                    this->mapped_host_pages_apple_.insert(host_page);
                }
#endif
            }

            this->mmio_regions_.emplace_back(mmio_region{.address = address,
                                                         .size = size,
                                                         .read_cb = std::move(read_cb),
                                                         .host_backing = host_backing,
                                                         .host_backing_size = host_backing_size});
        }

        // Rewrites every MMIO region's real backing (see mmio_region's doc comment) with fresh
        // content. Called once per quantum from every vCPU's start() loop independently under real
        // multi-vCPU concurrency - a shared_lock here would let one vCPU's mprotect(read-only) race
        // another vCPU's in-flight read_cb memcpy into the same shared host_backing memory, which
        // Darwin can report as BUS_ADRALN rather than the "expected" SEGV_ACCERR (the same
        // misclassification documented elsewhere in this file). A unique lock excludes both.
        void refresh_mmio_backings()
        {
            const std::unique_lock lock(this->tables_mutex_);
            for (const auto& region : this->mmio_regions_)
            {
                if (region.host_backing == nullptr)
                {
                    continue;
                }

                ::mprotect(region.host_backing, region.host_backing_size, PROT_READ | PROT_WRITE);
                region.read_cb(0, region.host_backing, region.size);
                ::mprotect(region.host_backing, region.host_backing_size, PROT_READ);
            }
        }

        void map_memory(uint64_t address, size_t size, memory_permission permissions) override
        {
            const std::unique_lock lock(this->tables_mutex_);

            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX memory mappings must be page aligned");
            }

#ifdef __APPLE__
            this->set_shadow_range_apple(address, size, permissions);
            this->sync_host_pages_covering_apple(address, size);
#else
            void* result = ::mmap(reinterpret_cast<void*>(address), size, to_prot(permissions),
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
            if (result == MAP_FAILED || reinterpret_cast<uint64_t>(result) != address)
            {
                throw std::runtime_error("FEX backend failed to map guest memory at requested address");
            }
#endif

            this->erase_region_range(address, size);
            this->regions_[address] = mapped_region{.size = size, .permissions = permissions, .owned = true};
            this->mark_executable_range_locked(address, size, permissions);
        }

#ifdef __APPLE__
        bool reserve_guest_address_range(uint64_t address, size_t size) override
        {
            const std::unique_lock lock(this->tables_mutex_);

            const uint64_t start = host_page_align_down_apple(address);
            const uint64_t end = host_page_align_up_apple(address + size);
            std::vector<uint64_t> claimed_this_call;
            for (uint64_t host_page = start; host_page < end; host_page += host_page_size_apple)
            {
                if (this->mapped_host_pages_apple_.contains(host_page))
                {
                    continue;
                }

                const auto rebase = rebase_for(this->is_wow64_process_, host_page);

                // Bug 4 fix: claim via mach_vm_allocate(VM_FLAGS_FIXED) WITHOUT VM_FLAGS_OVERWRITE -
                // the kernel refuses (KERN_NO_SPACE) instead of silently overwriting an intervening
                // foreign mapping another vCPU's concurrent syscall placed here between an earlier
                // free-space probe and this claim.
                mach_vm_address_t target = host_page + rebase;
                const kern_return_t result = ::mach_vm_allocate(mach_task_self(), &target, host_page_size_apple, VM_FLAGS_FIXED);
                if (result != KERN_SUCCESS)
                {
                    // Roll back every page claimed earlier in this same multi-page call so a partial
                    // claim never leaks as a permanently-orphaned host page. Collision means return
                    // false, not throw: the interface contract (memory_interface.hpp) has the caller
                    // re-pick a different address on false - memory_manager's auto-placement retry
                    // loop tests the return value and has no try/catch, so a throw here escapes as a
                    // fatal syscall failure instead of triggering the retry.
                    for (const auto rollback_page : claimed_this_call)
                    {
                        const auto rollback_rebase = rebase_for(this->is_wow64_process_, rollback_page);
                        if (g_hvf != nullptr)
                        {
                            g_hvf->sync_page(rollback_page + rollback_rebase, PROT_NONE);
                        }
                        ::munmap(reinterpret_cast<void*>(rollback_page + rollback_rebase), host_page_size_apple);
                        this->mapped_host_pages_apple_.erase(rollback_page);
                    }
                    return false;
                }
                this->mapped_host_pages_apple_.insert(host_page);
                claimed_this_call.push_back(host_page);
                if (g_hvf != nullptr)
                {
                    // mach_vm_allocate hands back VM_PROT_DEFAULT (rw) memory; mirror that so the
                    // page behaves identically inside the vCPU until a shadow sync assigns real
                    // guest permissions.
                    g_hvf->sync_page(host_page + rebase, PROT_READ | PROT_WRITE);
                }
            }
            return true;
        }

        void release_guest_address_range(uint64_t address, size_t size) override
        {
            const std::unique_lock lock(this->tables_mutex_);

            const uint64_t start = host_page_align_up_apple(address);
            const uint64_t end = host_page_align_down_apple(address + size);

            auto it = this->mapped_host_pages_apple_.lower_bound(start);
            while (it != this->mapped_host_pages_apple_.end() && *it + host_page_size_apple <= end)
            {
                const auto rebase = rebase_for(this->is_wow64_process_, *it);
                void* const host_ptr = reinterpret_cast<void*>(*it + rebase);
                if (rebase != 0 && this->wow64_host_window_reserved_)
                {
                    // A page inside the up-front-reserved wow64 window is never actually released at
                    // the host level - it's re-armed as our own PROT_NONE placeholder, exactly as
                    // reserve_wow64_host_window left it initially. It must stay registered in
                    // mapped_host_pages_apple_ (advance past it, don't erase): erasing it here would
                    // desync the bookkeeping from reality - the page genuinely IS still mapped (as
                    // this placeholder), so a later claim attempt for it would incorrectly believe it
                    // needs a fresh mach_vm_allocate, which then correctly (but uselessly) fails since
                    // the page really is still ours, throwing a false-positive host_memory_collision.
                    if (g_hvf != nullptr)
                    {
                        g_hvf->sync_page(reinterpret_cast<uint64_t>(host_ptr), PROT_NONE);
                    }
                    ::mmap(host_ptr, host_page_size_apple, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
                    ++it;
                }
                else
                {
                    if (g_hvf != nullptr)
                    {
                        g_hvf->sync_page(reinterpret_cast<uint64_t>(host_ptr), PROT_NONE);
                    }
                    ::munmap(host_ptr, host_page_size_apple);
                    it = this->mapped_host_pages_apple_.erase(it);
                }
            }
        }
#endif

        void map_host_memory(uint64_t address, size_t size, void* host_pointer, memory_permission permissions) override
        {
            const std::unique_lock lock(this->tables_mutex_);

            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX host memory mappings must be page aligned");
            }

            const auto rebase = rebase_for(this->is_wow64_process_, address);
            const uint64_t host_address = address + rebase;

#ifdef __APPLE__
            mach_vm_address_t target_address = host_address;
            vm_prot_t cur_protection = VM_PROT_NONE;
            vm_prot_t max_protection = VM_PROT_NONE;
            const kern_return_t result = ::mach_vm_remap(mach_task_self(), &target_address, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                                                         mach_task_self(), reinterpret_cast<mach_vm_address_t>(host_pointer), FALSE,
                                                         &cur_protection, &max_protection, VM_INHERIT_NONE);
            if (result != KERN_SUCCESS || target_address != host_address)
            {
                throw std::runtime_error("FEX backend failed to alias host memory into the guest");
            }

            // Without shadow entries, handle_general_memory_violation classifies any hardware fault in
            // this range as an unmapped-memory violation - including the misaligned STLR-family faults
            // (SIGBUS/BUS_ADRALN) FEX's TSO modeling routinely produces for x86-legal unaligned guest
            // accesses, which must instead reach handle_misaligned_atomic_fault's emulation like they
            // do for ordinary mappings. Registering the covering host pages additionally keeps the
            // first-claim path from treating this live aliased range as free (same reasoning as
            // map_mmio's KUSD-collision fix above).
            this->set_shadow_range_apple(address, size, permissions);
            for (uint64_t host_page = host_page_align_down_apple(address); host_page < address + size; host_page += host_page_size_apple)
            {
                this->mapped_host_pages_apple_.insert(host_page);
            }
#else
            void* result = ::mremap(host_pointer, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, reinterpret_cast<void*>(host_address));
            if (result == MAP_FAILED || reinterpret_cast<uint64_t>(result) != host_address)
            {
                throw std::runtime_error("FEX backend failed to alias host memory into the guest");
            }
#endif

#ifdef __APPLE__
            ::mprotect(reinterpret_cast<void*>(host_address), size, to_host_prot_hvf(to_prot(permissions)));
            if (g_hvf != nullptr)
            {
                g_hvf->map(host_address, size, to_prot_apple(permissions));
            }
#else
            ::mprotect(reinterpret_cast<void*>(host_address), size, to_prot(permissions));
#endif
            this->erase_region_range(address, size);
            this->regions_[address] = mapped_region{.size = size, .permissions = permissions, .owned = false};
            this->mark_executable_range_locked(address, size, permissions);
        }

        bool host_memory_aliasing_is_coherent() const override
        {
            return false;
        }

        void flush_host_memory_cache(const void* host_pointer, size_t size) override
        {
            if (host_pointer == nullptr || size == 0)
            {
                return;
            }

#ifdef __APPLE__
            ::sys_dcache_flush(const_cast<void*>(host_pointer), size);
#else
            constexpr size_t cache_line_size = 64; // Conservative for all known ARM64 implementations.
            const auto first = reinterpret_cast<uintptr_t>(host_pointer) & ~(cache_line_size - 1);
            const auto last = reinterpret_cast<uintptr_t>(host_pointer) + size;
            for (auto line = first; line < last; line += cache_line_size)
            {
                __asm__ volatile("dc civac, %0" : : "r"(line) : "memory");
            }
            __asm__ volatile("dsb sy" ::: "memory");
#endif
        }

        void unmap_memory(uint64_t address, size_t size) override
        {
            // tables_mutex_ is released before invalidate_code_range_locked runs - see
            // try_write_memory_impl's doc comment for why holding it across that call is a real ABBA
            // deadlock against FEXCore's own compile path.
            {
                const std::unique_lock lock(this->tables_mutex_);

                if (std::erase_if(this->mmio_regions_, [address](const mmio_region& region) {
                        if (region.address != address)
                        {
                            return false;
                        }
                        if (region.host_backing != nullptr)
                        {
#ifdef __APPLE__
                            if (g_hvf != nullptr)
                            {
                                g_hvf->unmap(reinterpret_cast<uint64_t>(region.host_backing), region.host_backing_size);
                            }
#endif
                            ::munmap(region.host_backing, region.host_backing_size);
                        }
                        return true;
                    }))
                {
                    return;
                }

#ifdef __APPLE__
                this->set_shadow_range_apple(address, size, std::nullopt);
                this->sync_host_pages_covering_apple(address, size);
#else
                ::munmap(reinterpret_cast<void*>(address), size);
#endif
                this->erase_region_range(address, size);
            }
            this->invalidate_code_range_locked(address, size, /*include_inactive_contexts=*/true);
        }

        void apply_memory_protection(uint64_t address, size_t size, memory_permission permissions) override
        {
            // See unmap_memory's doc comment: tables_mutex_ must be released before
            // invalidate_code_range_locked/mark_executable_range_locked run.
            {
                const std::unique_lock lock(this->tables_mutex_);

#ifdef __APPLE__
                this->set_shadow_range_apple(address, size, permissions);
                this->sync_host_pages_covering_apple(address, size);
#else
                if (::mprotect(reinterpret_cast<void*>(address), size, to_prot(permissions)) != 0)
                {
                    throw std::runtime_error("FEX backend failed to change memory protection");
                }
#endif

                this->set_region_range_permissions(address, size, permissions);
            }
            this->invalidate_code_range_locked(address, size);
            this->mark_executable_range_locked(address, size, permissions);
        }

        // --[ region bookkeeping - callers already hold tables_mutex_ ]------------------------------

        bool range_is_writable(uint64_t address, size_t size) const
        {
            uint64_t cursor = address;
            const uint64_t end = address + size;

            while (cursor < end)
            {
                auto it = this->regions_.upper_bound(cursor);
                if (it == this->regions_.begin())
                {
                    return false;
                }
                --it;

                const uint64_t region_end = it->first + it->second.size;
                if (cursor < it->first || cursor >= region_end ||
                    (it->second.permissions & memory_permission::write) == memory_permission::none)
                {
                    return false;
                }
                cursor = region_end;
            }

            return true;
        }

        void set_temporary_write_access(uint64_t address, size_t size, bool enable)
        {
#ifdef __APPLE__
            const uint64_t start = host_page_align_down_apple(address);
            const uint64_t end = host_page_align_up_apple(address + size);
            for (uint64_t host_page = start; host_page < end; host_page += host_page_size_apple)
            {
                if (!enable)
                {
                    this->sync_host_page_apple(host_page);
                    continue;
                }

                memory_permission effective = memory_permission::none;
                for (uint64_t page = host_page; page < host_page + host_page_size_apple; page += page_size)
                {
                    const auto it = this->page_shadow_apple_.find(page);
                    if (it != this->page_shadow_apple_.end())
                    {
                        effective = effective | it->second;
                    }
                }
                const auto rebase = rebase_for(this->is_wow64_process_, host_page);
                ::mprotect(reinterpret_cast<void*>(host_page + rebase), host_page_size_apple,
                           to_prot_apple(effective | memory_permission::write));
            }
#else
            const uint64_t end = address + size;

            auto it = this->regions_.upper_bound(address);
            if (it != this->regions_.begin())
            {
                --it;
            }

            for (; it != this->regions_.end() && it->first < end; ++it)
            {
                const uint64_t region_end = it->first + it->second.size;
                if (region_end <= address)
                {
                    continue;
                }

                const uint64_t sub_start = std::max(it->first, address & ~(page_size - 1));
                const uint64_t sub_end = std::min(region_end, (end + page_size - 1) & ~(page_size - 1));
                const memory_permission perm = enable ? (it->second.permissions | memory_permission::write) : it->second.permissions;
                ::mprotect(reinterpret_cast<void*>(sub_start), sub_end - sub_start, to_prot(perm));
            }
#endif
        }

        void erase_region_range(uint64_t address, size_t size)
        {
            const uint64_t end = address + size;

            auto it = this->regions_.lower_bound(address);
            if (it != this->regions_.begin())
            {
                auto prev = std::prev(it);
                if (prev->first + prev->second.size > address)
                {
                    it = prev;
                }
            }

            while (it != this->regions_.end() && it->first < end)
            {
                const uint64_t region_start = it->first;
                const uint64_t region_end = region_start + it->second.size;
                if (region_end <= address)
                {
                    ++it;
                    continue;
                }

                const auto region = it->second;
                it = this->regions_.erase(it);

                if (region_start < address)
                {
                    this->regions_[region_start] = mapped_region{
                        .size = static_cast<size_t>(address - region_start), .permissions = region.permissions, .owned = region.owned};
                }
                if (region_end > end)
                {
                    it = this->regions_
                             .emplace(end, mapped_region{.size = static_cast<size_t>(region_end - end),
                                                         .permissions = region.permissions,
                                                         .owned = region.owned})
                             .first;
                    ++it;
                }
            }
        }

        void set_region_range_permissions(uint64_t address, size_t size, memory_permission permissions)
        {
            const uint64_t end = address + size;

            std::vector<std::pair<uint64_t, mapped_region>> touched;
            auto it = this->regions_.lower_bound(address);
            if (it != this->regions_.begin())
            {
                auto prev = std::prev(it);
                if (prev->first + prev->second.size > address)
                {
                    it = prev;
                }
            }
            for (; it != this->regions_.end() && it->first < end; ++it)
            {
                if (it->first + it->second.size > address)
                {
                    touched.emplace_back(it->first, it->second);
                }
            }

            for (const auto& [region_start, region] : touched)
            {
                const uint64_t region_end = region_start + region.size;
                this->regions_.erase(region_start);

                if (region_start < address)
                {
                    this->regions_[region_start] = mapped_region{
                        .size = static_cast<size_t>(address - region_start), .permissions = region.permissions, .owned = region.owned};
                }
                const uint64_t inner_start = std::max(region_start, address);
                const uint64_t inner_end = std::min(region_end, end);
                this->regions_[inner_start] =
                    mapped_region{.size = static_cast<size_t>(inner_end - inner_start), .permissions = permissions, .owned = region.owned};
                if (region_end > end)
                {
                    this->regions_[end] = mapped_region{
                        .size = static_cast<size_t>(region_end - end), .permissions = region.permissions, .owned = region.owned};
                }
            }
        }

        bool is_range_mapped(uint64_t address, size_t size) const
        {
            if (size == 0)
            {
                return true;
            }

            uint64_t cursor = address;
            const uint64_t end = address + size;

            while (cursor < end)
            {
                auto it = this->regions_.upper_bound(cursor);
                if (it == this->regions_.begin())
                {
                    return false;
                }
                --it;

                const uint64_t region_end = it->first + it->second.size;
                if (cursor < it->first || cursor >= region_end)
                {
                    return false;
                }
                cursor = region_end;
            }

            return true;
        }

#ifdef __APPLE__
        void set_shadow_range_apple(uint64_t address, size_t size, std::optional<memory_permission> permissions)
        {
            for (uint64_t page = address; page < address + size; page += page_size)
            {
                if (permissions.has_value())
                {
                    this->page_shadow_apple_[page] = *permissions;
                }
                else
                {
                    this->page_shadow_apple_.erase(page);
                }
            }
        }

        void sync_host_page_apple(uint64_t host_page_addr)
        {
            memory_permission effective = memory_permission::none;
            bool any_slot_present = false;

            for (uint64_t page = host_page_addr; page < host_page_addr + host_page_size_apple; page += page_size)
            {
                const auto it = this->page_shadow_apple_.find(page);
                if (it == this->page_shadow_apple_.end())
                {
                    continue;
                }
                any_slot_present = true;
                effective = effective | it->second;
            }

            const auto rebase = rebase_for(this->is_wow64_process_, host_page_addr);
            void* host_ptr = reinterpret_cast<void*>(host_page_addr + rebase);
            const bool currently_mapped = this->mapped_host_pages_apple_.contains(host_page_addr);

            if (!any_slot_present)
            {
                if (currently_mapped)
                {
                    if (g_hvf != nullptr)
                    {
                        g_hvf->sync_page(host_page_addr + rebase, PROT_NONE);
                    }
                    void* result =
                        ::mmap(host_ptr, host_page_size_apple, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
                    if (result != host_ptr)
                    {
                        throw std::runtime_error("FEX backend failed to re-reserve decommitted guest memory");
                    }
                }
                return;
            }

            if (!currently_mapped)
            {
                // Bug 4 fix: claim via mach_vm_allocate(VM_FLAGS_FIXED) without VM_FLAGS_OVERWRITE
                // first, so a foreign mapping placed here by another vCPU's concurrent syscall
                // between an earlier probe and this claim is detected instead of silently destroyed
                // - then map the real content over the now-guaranteed-free page. Every page inside
                // the up-front-reserved wow64 window is pre-registered in mapped_host_pages_apple_
                // (see reserve_wow64_host_window), so currently_mapped is already true there and this
                // branch is only ever reached for a genuinely fresh page outside that window.
                mach_vm_address_t target = host_page_addr + rebase;
                const kern_return_t probe_result = ::mach_vm_allocate(mach_task_self(), &target, host_page_size_apple, VM_FLAGS_FIXED);
                if (probe_result != KERN_SUCCESS)
                {
                    throw host_memory_collision{};
                }
                ::munmap(host_ptr, host_page_size_apple);
                void* result = ::mmap(host_ptr, host_page_size_apple, to_host_prot_hvf(to_prot_apple(effective)),
                                      MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
                if (result == MAP_FAILED || result != host_ptr)
                {
                    throw std::runtime_error("FEX backend failed to map guest memory at requested address");
                }
                this->mapped_host_pages_apple_.insert(host_page_addr);
                if (g_hvf != nullptr)
                {
                    g_hvf->sync_page(host_page_addr + rebase, to_prot_apple(effective));
                }
                return;
            }

            if (::mprotect(host_ptr, host_page_size_apple, to_host_prot_hvf(to_prot_apple(effective))) != 0)
            {
                throw std::runtime_error("FEX backend failed to change memory protection");
            }
            if (g_hvf != nullptr)
            {
                g_hvf->sync_page(host_page_addr + rebase, to_prot_apple(effective));
            }
        }

        void sync_host_pages_covering_apple(uint64_t address, size_t size)
        {
            const uint64_t start = host_page_align_down_apple(address);
            const uint64_t end = host_page_align_up_apple(address + size);
            for (uint64_t host_page = start; host_page < end; host_page += host_page_size_apple)
            {
                this->sync_host_page_apple(host_page);
            }
        }
#endif

        void initialize_context()
        {
            LogMan::Msg::InstallHandler([](LogMan::DebugLevels level, const char* message) {
                fprintf(stderr, "[FEXCore LogMan] level=%s: %s\n", LogMan::DebugLevelStr(level), message);
            });
            LogMan::Throw::InstallHandler([](const char* message) { fprintf(stderr, "[FEXCore LogMan THROW] %s\n", message); });

#ifdef __APPLE__
            if (hvf_requested())
            {
                auto& vm = hvf::hvf_vm::instance();
                if (vm.create() != hvf::vm_create_result::hardware_tso)
                {
                    throw std::runtime_error("EMULATOR_FEX_HVF=1 requires Hypervisor.framework with hardware TSO "
                                             "(missing entitlement, unsupported chip/OS, or nested virtualization)");
                }
                g_hvf = &vm;
                fex_internal_arena::instance().enable_hvf_mode();
                FEXCore::Allocator::PagesReplaced = &hvf_pages_replaced_hook;
                hvf_publish_fexcore_constants();
            }

            fex_internal_arena::instance().install();
            this->reserve_wow64_host_window();
#endif

            std::set_terminate([]() {
                fprintf(stderr, "[FEX backend] std::terminate invoked\n");
                if (auto exc = std::current_exception())
                {
                    try
                    {
                        std::rethrow_exception(exc);
                    }
                    catch (const std::exception& e)
                    {
                        fprintf(stderr, "[FEX backend] uncaught exception: %s\n", e.what());
                    }
                    catch (...)
                    {
                        fprintf(stderr, "[FEX backend] uncaught exception of unknown type\n");
                    }
                }

                void* frames[64]{};
                const int frame_count = ::backtrace(frames, 64);
                char** symbols = ::backtrace_symbols(frames, frame_count);
                fprintf(stderr, "[FEX backend] backtrace (%d frames):\n", frame_count);
                for (int i = 0; i < frame_count; ++i)
                {
                    fprintf(stderr, "  %s\n", symbols ? symbols[i] : "?");
                }
                free(symbols);

                std::abort();
            });

            FEXCore::Config::Initialize();
            FEXCore::Config::Load();

            if (const char* dumpir_dir = std::getenv("EMULATOR_FEX_DUMPIR"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_DUMPIR, dumpir_dir);
                FEXCore::Config::Set(FEXCore::Config::CONFIG_PASSMANAGERDUMPIR, "3");
            }

            FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
            FEXCore::Config::Set(FEXCore::Config::CONFIG_GDBSERVER, "1");

            // Performance experiments (see docs/fex-backend.md perf section): each of FEXCore's
            // software TSO-emulation-cost levers, gated behind its own env var so it can be A/B
            // tested independently against the EMULATOR_FPS_COUNTER instrumentation. All default to
            // FEXCore's own conservative defaults (unset = untouched) until validated.
            if (std::getenv("EMULATOR_FEX_VECTOR_TSO"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_VECTORTSOENABLED, "1");
            }
            if (std::getenv("EMULATOR_FEX_MEMCPY_TSO"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_MEMCPYSETTSOENABLED, "1");
            }
            if (std::getenv("EMULATOR_FEX_X87_REDUCED_PRECISION"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_X87REDUCEDPRECISION, "1");
            }
            if (std::getenv("EMULATOR_FEX_NO_TSO"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_TSOENABLED, "0");
            }
            if (std::getenv("EMULATOR_FEX_SMC_NONE"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_SMCCHECKS, "0");
            }
            if (std::getenv("EMULATOR_FEX_STRICT_SPLIT_LOCKS"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_STRICTINPROCESSSPLITLOCKS, "1");
            }
            if (std::getenv("EMULATOR_FEX_LRCPC2"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_HOSTFEATURES, "enablelrcpc2");
            }

#ifdef __APPLE__
            const FEXCore::HostFeatures features = fetch_host_features_apple();
#else
            const FEXCore::HostFeatures features{}; // TODO(fex): FEXCore::FetchHostFeatures() on real HW.
#endif
            this->context_ = FEXCore::Context::Context::CreateNewContext(features);
            this->context_->SetWow64GuestRebaseValue(this->wow64_guest_rebase_);
#ifdef __APPLE__
            if (g_hvf != nullptr)
            {
                this->context_->SetHardwareTSOSupport(true);
            }
#endif

            this->syscall_handler_ = std::make_unique<fex_syscall_handler>(*this);
            this->context_->SetSyscallHandler(this->syscall_handler_.get());

            this->signal_delegator_ = std::make_unique<FEXCore::SignalDelegator>();
            this->context_->SetSignalDelegator(this->signal_delegator_.get());

            this->context_->InitCore();

#ifdef __APPLE__
            install_fault_signal_handlers(*this);
#endif
        }

#ifdef __APPLE__
        void reserve_wow64_host_window()
        {
            if (this->wow64_host_window_reserved_)
            {
                return;
            }

            constexpr uint64_t search_ceiling = 0x8000000000ULL; // 512 GiB
            constexpr int max_candidates = 32;

            uint64_t candidate = wow64_guest_rebase_default;
            for (int attempt = 0; attempt < max_candidates && candidate + wow64_guest_address_space_size <= search_ceiling; ++attempt)
            {
                mach_vm_address_t probe_addr = candidate;
                mach_vm_size_t probe_size = 0;
                vm_region_basic_info_data_64_t info{};
                mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
                mach_port_t object_name = MACH_PORT_NULL;
                const kern_return_t probe_result = mach_vm_region(mach_task_self(), &probe_addr, &probe_size, VM_REGION_BASIC_INFO_64,
                                                                  reinterpret_cast<vm_region_info_t>(&info), &info_count, &object_name);

                const bool candidate_is_free = probe_result != KERN_SUCCESS || probe_addr >= candidate + wow64_guest_address_space_size;
                if (!candidate_is_free)
                {
                    char path_buf[PROC_PIDPATHINFO_MAXSIZE] = {};
                    const int path_len = proc_regionfilename(getpid(), probe_addr, path_buf, sizeof(path_buf));
                    fprintf(stderr,
                            "[FEX backend] wow64 host window candidate [0x%llx, 0x%llx) occupied (mapping at 0x%llx "
                            "size=0x%llx prot=%d file=%s) - trying the next candidate\n",
                            static_cast<unsigned long long>(candidate),
                            static_cast<unsigned long long>(candidate + wow64_guest_address_space_size),
                            static_cast<unsigned long long>(probe_addr), static_cast<unsigned long long>(probe_size), info.protection,
                            path_len > 0 ? path_buf : "<none>");

                    const uint64_t occupant_end = probe_addr + probe_size;
                    candidate = (occupant_end + wow64_guest_address_space_size - 1) & ~(wow64_guest_address_space_size - 1);
                    continue;
                }

                void* const target = reinterpret_cast<void*>(candidate);
                void* const result =
                    ::mmap(target, wow64_guest_address_space_size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (result != target)
                {
                    fprintf(stderr, "[FEX backend] failed to reserve wow64 host window at 0x%llx - trying the next candidate\n",
                            static_cast<unsigned long long>(candidate));
                    if (result != MAP_FAILED)
                    {
                        ::munmap(result, wow64_guest_address_space_size);
                    }
                    candidate += wow64_guest_address_space_size;
                    continue;
                }

                this->wow64_guest_rebase_ = candidate;
                this->wow64_host_window_reserved_ = true;

                // Register every host page of the freshly-reserved window in
                // mapped_host_pages_apple_ up front (keyed by the corresponding *guest* address,
                // matching this map's existing convention - see rebase_for). Without this, a later
                // individual-page claim inside the window (reserve_guest_address_range/
                // sync_host_page_apple) sees "not yet mapped" and takes the TOCTOU-safe
                // mach_vm_allocate(VM_FLAGS_FIXED) path - which then genuinely fails, since the page
                // really is already mapped, by this very reservation, not a foreign occupant.
                // Registering them now means every later claim inside the window correctly takes
                // the "already ours" -> mprotect-only path instead, which is always safe here: this
                // whole window was reserved before any guest or FEXCore-internal code ever ran, so
                // nothing could have raced to place a genuine foreign mapping inside it.
                for (uint64_t guest_page = 0; guest_page < wow64_guest_address_space_size; guest_page += host_page_size_apple)
                {
                    this->mapped_host_pages_apple_.insert(guest_page);
                }
                return;
            }

            fprintf(stderr,
                    "[FEX backend] exhausted %d candidates below 0x%llx searching for a free wow64 host window - "
                    "falling back to the default at 0x%llx with detect-and-retry\n",
                    max_candidates, static_cast<unsigned long long>(search_ceiling),
                    static_cast<unsigned long long>(wow64_guest_rebase_default));
        }
#endif

        // mark_executable_range/invalidate_code_range need to run per-acting-vcpu (they touch a
        // specific InternalThreadState), but are called from machine-wide, already-locked contexts
        // (map_memory/apply_memory_protection). Fan out to every vCPU's thread of the relevant
        // context (Task 5): with N threads sharing a context, invalidate the shared code buffer once,
        // then invalidate every vCPU's own cached-code view - otherwise other vCPUs keep executing
        // stale translations after a code-modifying event on one vCPU.
        void mark_executable_range_locked(uint64_t address, size_t size, memory_permission permissions)
        {
            if ((permissions & memory_permission::exec) == memory_permission::none)
            {
                return;
            }
            for (auto& vcpu : this->vcpus_)
            {
                vcpu->mark_executable_range(address, size, permissions);
            }
        }

        void invalidate_code_range_locked(uint64_t address, size_t size, bool include_inactive_contexts = false)
        {
            for (auto& vcpu : this->vcpus_)
            {
                vcpu->invalidate_code_range(address, size, include_inactive_contexts);
            }
        }

        // --[ state ]--------------------------------------------------------------------------------

        // Protects every machine-wide table below (regions_, mmio_regions_, gate_crossings_, the
        // Apple host-page shadow tables, callret_buffers_, hook maps) against concurrent access from
        // multiple vCPUs' host threads. Never held across a call into guest-execution code (JIT
        // dispatch/ExecuteThread) or across a hook callback that re-enters the kernel lock - only
        // ever taken to protect a bounded, non-reentrant table mutation/read. A synchronous fault
        // interrupts JIT/dispatcher code only, which never holds this mutex, so signal-context
        // acquisition here can't self-deadlock.
        mutable std::shared_mutex tables_mutex_;

        std::vector<std::unique_ptr<fex_vcpu>> vcpus_;

        fextl::unique_ptr<FEXCore::Context::Context> context_{};
        std::unique_ptr<fex_syscall_handler> syscall_handler_{};
        std::unique_ptr<FEXCore::SignalDelegator> signal_delegator_{};

        fextl::unique_ptr<FEXCore::Context::Context> context32_{};
        std::unique_ptr<fex_syscall_handler> syscall_handler32_{};
        std::unique_ptr<FEXCore::SignalDelegator> signal_delegator32_{};

        bool is_wow64_process_ = false;
        uint64_t wow64_guest_rebase_ = wow64_guest_rebase_default;
#ifdef __APPLE__
        bool wow64_host_window_reserved_ = false;
#endif
        uint64_t wow64_turbo_dispatch_end_ = 0;

        std::map<uint64_t, mapped_region> regions_;
        std::vector<mmio_region> mmio_regions_;
        std::vector<std::pair<void*, size_t>> callret_buffers_;
        std::vector<gate_crossing> gate_crossings_;

#ifdef __APPLE__
        std::map<uint64_t, memory_permission> page_shadow_apple_;
        std::set<uint64_t> mapped_host_pages_apple_;
#endif

        hook_entry* syscall_hook_ = nullptr;
        std::unordered_map<emulator_hook*, hook_entry> instruction_hooks_;
        std::unordered_map<emulator_hook*, interrupt_hook_callback> interrupt_hooks_;
        std::unordered_map<emulator_hook*, memory_access_hook_callback> memory_read_hooks_;
        std::unordered_map<emulator_hook*, memory_access_hook_callback> memory_write_hooks_;
        std::unordered_map<emulator_hook*, memory_execution_hook_callback> memory_execution_hooks_;
        std::unordered_map<emulator_hook*, memory_violation_hook_callback> memory_violation_hooks_;
        std::unordered_map<emulator_hook*, basic_block_hook_callback> basic_block_hooks_;
        uintptr_t next_hook_id_ = 1;
    };

#ifdef __APPLE__
    namespace
    {
        // Bug 2 fix: a vCPU worker thread's own OS-chosen default stack is ordinary host memory,
        // placed by the OS wherever it likes - under this backend's guest-VA==host-VA model, a new
        // thread's stack can coincidentally land on an address the guest program is about to use,
        // something structurally impossible in single-vCPU mode. Reserve a dedicated arena of fixed-
        // size, fixed-offset worker stacks up front (mirroring fex_internal_arena's reservation
        // pattern) and hand one back per vCPU index via reserve_worker_thread_stack, avoiding the
        // wow64 host window the same way the internal arena does.
        class fex_worker_stack_arena
        {
          public:
            static constexpr size_t max_workers = 64;
            static constexpr size_t stack_size = 8 * 1024 * 1024;

            static fex_worker_stack_arena& instance()
            {
                static fex_worker_stack_arena arena;
                return arena;
            }

            bool get(size_t vcpu_index, void*& stack_base, size_t& out_stack_size)
            {
                if (vcpu_index >= max_workers)
                {
                    return false;
                }

                this->ensure_installed();
                if (this->base_ == 0)
                {
                    return false;
                }

                stack_base = reinterpret_cast<void*>(this->base_ + vcpu_index * stack_size);
                out_stack_size = stack_size;
                return true;
            }

          private:
            uintptr_t base_ = 0;

            void ensure_installed()
            {
                if (this->base_ != 0)
                {
                    return;
                }

                const size_t total_size = max_workers * stack_size;
                void* base = MAP_FAILED;
                constexpr int max_attempts = 8;
                for (int attempt = 0; attempt < max_attempts; ++attempt)
                {
                    void* candidate = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                    if (candidate == MAP_FAILED)
                    {
                        break;
                    }

                    const auto candidate_addr = reinterpret_cast<uint64_t>(candidate);
                    const auto candidate_end = candidate_addr + total_size;
                    const bool overlaps_wow64_window = candidate_addr < wow64_guest_rebase_default + wow64_guest_address_space_size &&
                                                       candidate_end > wow64_guest_rebase_default;
                    if (!overlaps_wow64_window)
                    {
                        base = candidate;
                        break;
                    }

                    ::munmap(candidate, total_size);
                }

                if (base == MAP_FAILED)
                {
                    return;
                }

                this->base_ = reinterpret_cast<uintptr_t>(base);
            }
        };
    } // namespace
#endif

    bool fex_x86_64_emulator::reserve_worker_thread_stack([[maybe_unused]] size_t vcpu_index, [[maybe_unused]] void*& stack_base,
                                                          [[maybe_unused]] size_t& stack_size)
    {
#ifdef __APPLE__
        return fex_worker_stack_arena::instance().get(vcpu_index, stack_base, stack_size);
#else
        return false;
#endif
    }

    // -----------------------------------------------------------------------------------------------
    // fex_vcpu method bodies (fex_x86_64_emulator is now complete).
    // -----------------------------------------------------------------------------------------------

    fex_vcpu::~fex_vcpu() = default;

    memory_interface& fex_vcpu::memory()
    {
        return this->emulator_;
    }

    const memory_interface& fex_vcpu::memory() const
    {
        return this->emulator_;
    }

    bool fex_vcpu::read_descriptor_table(int reg, descriptor_table_register& table)
    {
        // FEX is a user-mode emulator: there is no real IDT, and the GDT is synthesized internally.
        // Only report the GDT base we were handed via load_gdt(); everything else is unsupported.
        if (reg == static_cast<int>(x86_register::gdtr))
        {
            table.base = this->gdt_base_;
            table.limit = this->gdt_limit_;
            return true;
        }
        return false;
    }

    void fex_vcpu::load_gdt(pointer_type address, uint32_t limit)
    {
        // Only remember the base/limit for callers querying gdtr (see read_descriptor_table). Kept
        // per-vCPU (not on the shared emulator) - each vCPU has its own GDT page specifically so one
        // WoW64 thread's FS descriptor (TEB32 base) can never be read from another vCPU.
        this->gdt_base_ = address;
        this->gdt_limit_ = limit;

        const auto rebase = this->emulator_.rebase_for(this->emulator_.is_wow64_process_, address);
        this->cpu_state().segment_arrays[0] = reinterpret_cast<FEXCore::Core::CPUState::gdt_segment*>(address + rebase);
    }

#ifdef __APPLE__
    void fex_vcpu::start(size_t count)
    {
        if (g_hvf != nullptr)
        {
            this->start_hvf(count);
            return;
        }

        this->emulator_.refresh_mmio_backings();

        if (count != 0)
        {
            // FEX has CompileRIPCount() for bounded execution, but wiring exact instruction counts
            // through the JIT exit path is non-trivial; match the KVM backend and refuse for now.
            throw std::runtime_error("FEX backend does not support exact instruction counts yet");
        }

        // sigaltstack is per-host-thread; each vCPU's worker thread registers its own alt_stack_ once,
        // the first time it ever enters here (a vCPU's start() always runs on the same host thread
        // thereafter - windows_emulator's vcpu_worker owns exactly one host thread per vCPU).
        thread_local bool sigaltstack_registered = false;
        if (!sigaltstack_registered)
        {
            stack_t ss{};
            ss.ss_sp = this->alt_stack_.data();
            ss.ss_size = this->alt_stack_.size();
            ss.ss_flags = 0;
            ::sigaltstack(&ss, nullptr);
            sigaltstack_registered = true;
        }

        // Routes this host thread's faults to this vCPU's state for the duration of guest execution.
        const current_vcpu_scope current_vcpu_guard(*this);

        if (this->active_thread_.load() == nullptr)
        {
            this->create_thread();
        }

        this->stop_requested_ = false;
        // Re-arm InterruptFaultPage for this quantum - a prior stop() may have left it protected to
        // force the last quantum's ExecuteThread to return, and it must be writable again before the
        // JIT's per-block-entry store runs.
        {
            auto* const active = this->active_thread_.load();
            ::mprotect(active->InterruptFaultPage, sizeof(active->InterruptFaultPage), PROT_READ | PROT_WRITE);
        }

        // ExecuteThread runs the translated guest until the thread is asked to stop (which the
        // syscall bridge does when a hook calls stop()), or the guest faults/exits. It can also
        // return early because handle_fault_signal deferred a hook dispatch rather than a genuine
        // stop - dispatch it here, in normal call context where it's actually safe to do so, then
        // resume by calling ExecuteThread again.
        for (;;)
        {
            this->active_context_->ExecuteThread(this->active_thread_.load());

            const bool hook_dispatched = this->dispatch_pending_hook_if_any();
            const bool interrupt_page_unwind = std::exchange(this->interrupt_page_unwind_, false);

            // An InterruptFaultPage unwind with no stop pending is the quantum timer racing this
            // quantum's own entry: stop() sets stop_requested_ then protects the page, but a
            // concurrently-entered start() has already cleared the flag and only then does the
            // timer's mprotect land - past this quantum's re-arm above. Re-arm and resume instead of
            // treating this as a real stop.
            if (this->stop_requested_ || (!hook_dispatched && !interrupt_page_unwind))
            {
                break;
            }

            // A deferred hook or a raced InterruptFaultPage unwind is resuming (no stop pending). If
            // it was a WoW64 gate crossing, active_thread_ was just swapped to the OTHER FEXCore
            // engine mid-quantum - re-arm the now-active engine's page here (a no-op if already
            // writable), so it resumes cleanly instead of immediately re-faulting as a spurious stop.
            auto* const active = this->active_thread_.load();
            ::mprotect(active->InterruptFaultPage, sizeof(active->InterruptFaultPage), PROT_READ | PROT_WRITE);
        }
    }
#else
    void fex_vcpu::start(size_t count)
    {
        this->emulator_.refresh_mmio_backings();

        if (count != 0)
        {
            throw std::runtime_error("FEX backend does not support exact instruction counts yet");
        }

        if (this->active_thread_.load() == nullptr)
        {
            this->create_thread();
        }

        this->stop_requested_ = false;
        this->active_context_->ExecuteThread(this->active_thread_.load());
    }
#endif

    void fex_vcpu::stop()
    {
        this->stop_requested_ = true;
        this->request_thread_stop();
    }

    size_t fex_vcpu::read_raw_register(int reg, void* value, size_t size)
    {
        const auto xreg = static_cast<x86_register>(reg);
        const auto mapping = detail::map_register(xreg);
        auto& state = this->cpu_state();

        switch (mapping.kind)
        {
        case detail::register_kind::gpr: {
            // In a WoW64 process the 32-bit engine (context32_) has no architectural r8-r15: 32-bit
            // x86 cannot address them, and when the 32-bit engine takes a real fault its SRA spill
            // leaves those greg slots holding host register values (observed as host stack pointers).
            // The meaningful high-register state - the wow64cpu-reserved r12-r15 (r14 = the 64-bit
            // exception stack, r13 = CpuArea CONTEXT block, ...) that a 64-bit CONTEXT capture needs -
            // lives in the frozen 64-bit engine (thread_), maintained by the forward gate. Source
            // r8-r15 from there so dispatch_exception's CONTEXT64 (consumed by ntdll!
            // KiUserExceptionDispatcher -> wow64!Wow64PrepareForException) carries the real values.
            auto* const active = this->active_thread_.load();
            const FEXCore::Core::CPUState& gpr_state =
                (this->emulator_.is_wow64_process_ && active == this->thread32_ && this->thread_ != nullptr &&
                 mapping.gpr.index >= detail::greg_r8 && mapping.gpr.index <= detail::greg_r8 + 7)
                    ? this->thread_->CurrentFrame->State
                    : state;
            uint64_t raw = gpr_state.gregs[mapping.gpr.index] >> (mapping.gpr.byte_offset * 8);
            std::memcpy(value, &raw, (std::min)(size, mapping.gpr.width));
            return size;
        }
        case detail::register_kind::rip:
            std::memcpy(value, &state.rip, (std::min)(size, sizeof(state.rip)));
            return size;
        case detail::register_kind::flags: {
            const uint64_t rflags = this->read_rflags();
            std::memcpy(value, &rflags, (std::min)(size, sizeof(rflags)));
            return size;
        }
        case detail::register_kind::xmm:
            std::memcpy(value, &state.xmm.avx.data[mapping.index][0], (std::min)(size, size_t{16}));
            return size;
        case detail::register_kind::mm:
            std::memcpy(value, &state.mm[mapping.index][0], (std::min)(size, size_t{16}));
            return size;
        case detail::register_kind::mxcsr:
            std::memcpy(value, &state.mxcsr, (std::min)(size, sizeof(state.mxcsr)));
            return size;
        case detail::register_kind::fcw:
            std::memcpy(value, &state.FCW, (std::min)(size, sizeof(state.FCW)));
            return size;
        case detail::register_kind::fs_base:
            std::memcpy(value, &state.fs_cached, (std::min)(size, sizeof(state.fs_cached)));
            return size;
        case detail::register_kind::gs_base:
            std::memcpy(value, &state.gs_cached, (std::min)(size, sizeof(state.gs_cached)));
            return size;
        case detail::register_kind::segment: {
            const uint16_t selector = this->segment_selector(mapping.index);
            std::memcpy(value, &selector, (std::min)(size, sizeof(selector)));
            return size;
        }
        case detail::register_kind::fsw:
        case detail::register_kind::unsupported:
        default:
            std::memset(value, 0, size);
            return size;
        }
    }

    size_t fex_vcpu::write_raw_register(int reg, const void* value, size_t size)
    {
        const auto xreg = static_cast<x86_register>(reg);
        const auto mapping = detail::map_register(xreg);
        auto& state = this->cpu_state();

        switch (mapping.kind)
        {
        case detail::register_kind::gpr: {
            auto& slot = state.gregs[mapping.gpr.index];
            if (mapping.gpr.width == 8)
            {
                std::memcpy(&slot, value, sizeof(slot));
            }
            else if (mapping.gpr.zero_extend_32)
            {
                uint32_t v = 0;
                std::memcpy(&v, value, sizeof(v));
                slot = v;
            }
            else
            {
                uint64_t incoming = 0;
                std::memcpy(&incoming, value, mapping.gpr.width);
                const auto shift = mapping.gpr.byte_offset * 8;
                const uint64_t mask = ((1ULL << (mapping.gpr.width * 8)) - 1) << shift;
                slot = (slot & ~mask) | ((incoming << shift) & mask);
            }
            return size;
        }
        case detail::register_kind::rip:
            std::memcpy(&state.rip, value, (std::min)(size, sizeof(state.rip)));
            return size;
        case detail::register_kind::flags: {
            uint64_t rflags = 0;
            std::memcpy(&rflags, value, (std::min)(size, sizeof(rflags)));
            this->write_rflags(rflags);
            return size;
        }
        case detail::register_kind::xmm:
            std::memcpy(&state.xmm.avx.data[mapping.index][0], value, (std::min)(size, size_t{16}));
            return size;
        case detail::register_kind::mm:
            std::memcpy(&state.mm[mapping.index][0], value, (std::min)(size, size_t{16}));
            return size;
        case detail::register_kind::mxcsr:
            std::memcpy(&state.mxcsr, value, (std::min)(size, sizeof(state.mxcsr)));
            return size;
        case detail::register_kind::fcw:
            std::memcpy(&state.FCW, value, (std::min)(size, sizeof(state.FCW)));
            return size;
        case detail::register_kind::fs_base:
            std::memcpy(&state.fs_cached, value, (std::min)(size, sizeof(state.fs_cached)));
            return size;
        case detail::register_kind::gs_base:
            std::memcpy(&state.gs_cached, value, (std::min)(size, sizeof(state.gs_cached)));
            return size;
        case detail::register_kind::segment:
            this->set_segment_selector(mapping.index, value, size);
            return size;
        case detail::register_kind::fsw:
        case detail::register_kind::unsupported:
        default:
            return size;
        }
    }

    static constexpr size_t kWow64SnapshotHeader = 8; // uint64 active-is-32 flag, kept 8 for alignment

    static constexpr size_t wow64_snapshot_size()
    {
        return kWow64SnapshotHeader + 2 * sizeof(FEXCore::Core::CPUState);
    }

    std::vector<std::byte> fex_vcpu::save_registers() const
    {
        // For a wow64 process, once the 32-bit engine exists a logical thread's full state spans
        // BOTH engines (active + parked). Snapshot both, tagged with which one is active, so a
        // thread switch preserves the parked excursion frame instead of leaking it to whichever
        // logical thread next runs the shared engine.
        if (this->emulator_.is_wow64_process_ && this->thread32_ != nullptr && this->thread_ != nullptr)
        {
            std::vector<std::byte> data(wow64_snapshot_size());
            const uint64_t active_is_32 = (this->active_context_ == this->emulator_.context32_.get()) ? 1 : 0;
            std::memcpy(data.data(), &active_is_32, sizeof(active_is_32));
            std::memcpy(data.data() + kWow64SnapshotHeader, &this->thread_->CurrentFrame->State, sizeof(FEXCore::Core::CPUState));
            std::memcpy(data.data() + kWow64SnapshotHeader + sizeof(FEXCore::Core::CPUState), &this->thread32_->CurrentFrame->State,
                        sizeof(FEXCore::Core::CPUState));
            return data;
        }

        const auto& state = this->cpu_state();
        std::vector<std::byte> data(sizeof(FEXCore::Core::CPUState));
        std::memcpy(data.data(), &state, sizeof(state));
        return data;
    }

    void fex_vcpu::restore_state_into(FEXCore::Core::InternalThreadState* thread, const std::byte* src)
    {
        auto& state = thread->CurrentFrame->State;
        const auto l1_pointer = state.L1Pointer;
        const auto l1_mask = state.L1Mask;
        // segment_arrays[0] is this vCPU's own GDT pointer (see load_gdt's doc comment: each vCPU
        // has its own GDT page specifically so one thread's FS descriptor can never be read from
        // another vCPU). A migrating thread's saved snapshot carries whichever vCPU it last ran on's
        // GDT pointer - blindly memcpy-ing the whole CPUState here would silently overwrite this
        // vCPU's correct GDT pointer with a stale, foreign one, corrupting every FS/SS-relative
        // access (TEB base, stack segment base) for the rest of this thread's life on this vCPU.
        // Preserve it exactly like L1Pointer/L1Mask below.
        const auto segment_array_0 = state.segment_arrays[0];
        std::memcpy(&state, src, sizeof(FEXCore::Core::CPUState));
        state.L1Pointer = l1_pointer;
        state.L1Mask = l1_mask;
        state.segment_arrays[0] = segment_array_0;
        this->ensure_callret_buffer(state);
        thread->CallRetStackBase = reinterpret_cast<void*>(state._pad1);

        // The snapshot's callret_sp would resurrect call-ret entries pushed during an earlier
        // scheduling quantum - host JIT code pointers that are only valid for the engine thread and
        // code-buffer generation that pushed them. FEXCore wipes only the callret buffer attached to
        // an engine thread when that thread rotates its code buffer or invalidates code (see
        // CheckCodeBufferUpdate/InvalidateThreadCachedCodeRange); a descheduled thread's buffer is
        // never wiped, so a resumed thread's RET can pop a matching guest address paired with a host
        // pointer into freed or foreign-generation JIT memory (observed as ExitFunctionLink "Record
        // outside code buffer" bails and wild host jumps under --vcpus > 1). The entries are purely a
        // RET fast path, so dropping them on every restore is always safe.
        state.callret_sp = state._pad1 + FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE / 4;
    }

    void fex_vcpu::restore_registers(const std::vector<std::byte>& register_data)
    {
        if (register_data.size() == wow64_snapshot_size())
        {
            if (this->thread_ == nullptr)
            {
                // A wow64 thread that already ran elsewhere can migrate onto a vCPU that never
                // executed anything: create the engines lazily here, exactly like start() does.
                // The staged_state_ seed already carries this vCPU's own GDT pointer, because
                // emulator_thread::restore() calls refresh_execution_context (load_gdt) first.
                this->create_thread();
            }
            if (this->thread32_ == nullptr)
            {
                this->create_thread32();
            }
            uint64_t active_is_32 = 0;
            std::memcpy(&active_is_32, register_data.data(), sizeof(active_is_32));
            this->restore_state_into(this->thread_, register_data.data() + kWow64SnapshotHeader);
            this->restore_state_into(this->thread32_, register_data.data() + kWow64SnapshotHeader + sizeof(FEXCore::Core::CPUState));
            if (active_is_32)
            {
                this->active_context_ = this->emulator_.context32_.get();
                this->active_thread_ = this->thread32_;
            }
            else
            {
                this->active_context_ = this->emulator_.context_.get();
                this->active_thread_ = this->thread_;
            }
            return;
        }

        if (register_data.size() != sizeof(FEXCore::Core::CPUState))
        {
            throw std::runtime_error("FEX register snapshot has unexpected size");
        }

        if (this->active_thread_.load() == nullptr)
        {
            // No thread yet: writing into staged_state_, which create_thread() will seed the
            // real thread from.
            std::memcpy(&this->staged_state_, register_data.data(), sizeof(FEXCore::Core::CPUState));
            return;
        }

        const auto incoming_cs = reinterpret_cast<const FEXCore::Core::CPUState*>(register_data.data())->cs_idx;
        const bool incoming_is_32bit = this->emulator_.is_wow64_process_ && incoming_cs == 0x23;
        if (incoming_is_32bit)
        {
            if (this->thread32_ == nullptr)
            {
                this->create_thread32();
            }
            this->active_context_ = this->emulator_.context32_.get();
            this->active_thread_ = this->thread32_;
        }
        else
        {
            this->active_context_ = this->emulator_.context_.get();
            this->active_thread_ = this->thread_;
        }

        this->restore_state_into(this->active_thread_.load(), register_data.data());
    }

    void fex_vcpu::set_segment_base(x86_register base, pointer_type value)
    {
        auto& state = this->cpu_state();
        if (base == x86_register::fs || base == x86_register::fs_base)
        {
            state.fs_cached = value;
        }
        else if (base == x86_register::gs || base == x86_register::gs_base)
        {
            state.gs_cached = value;
        }
    }

    fex_vcpu::pointer_type fex_vcpu::get_segment_base(x86_register base)
    {
        const auto& state = this->cpu_state();
        if (base == x86_register::fs || base == x86_register::fs_base)
        {
            return state.fs_cached;
        }
        if (base == x86_register::gs || base == x86_register::gs_base)
        {
            return state.gs_cached;
        }
        return 0;
    }

    void fex_vcpu::notify_process_bitness(bool is_wow64_process)
    {
        this->emulator_.is_wow64_process_ = is_wow64_process;
        this->emulator_.context_->SetNeedsWow64GuestRebase(is_wow64_process);
        if (is_wow64_process)
        {
            this->emulator_.ensure_context32();
        }
    }

    void fex_vcpu::register_gate_crossing(pointer_type address, size_t size, gate_crossing_kind kind)
    {
        const std::unique_lock lock(this->emulator_.tables_mutex_);
        this->emulator_.gate_crossings_.push_back(gate_crossing{address, size, kind});
    }

    void fex_vcpu::set_wow64_turbo_dispatch_end(pointer_type address)
    {
        this->emulator_.wow64_turbo_dispatch_end_ = address;
    }

    FEXCore::Core::CPUState& fex_vcpu::cpu_state()
    {
        auto* const active = this->active_thread_.load();
        if (active != nullptr)
        {
            return active->CurrentFrame->State;
        }
        return this->staged_state_;
    }

    const FEXCore::Core::CPUState& fex_vcpu::cpu_state() const
    {
        auto* const active = this->active_thread_.load();
        if (active != nullptr)
        {
            return active->CurrentFrame->State;
        }
        return this->staged_state_;
    }

    uint64_t fex_vcpu::read_rflags() const
    {
        auto* const active = this->active_thread_.load();
        if (active != nullptr)
        {
            return this->active_context_->ReconstructCompactedEFLAGS(active, /*WasInJIT=*/false, nullptr, 0);
        }
        return reconstruct_compacted_eflags(this->staged_state_);
    }

    void fex_vcpu::write_rflags(uint64_t rflags)
    {
        auto* const active = this->active_thread_.load();
        if (active != nullptr)
        {
            this->active_context_->SetFlagsFromCompactedEFLAGS(active, static_cast<uint32_t>(rflags));
            return;
        }
        set_flags_from_compacted_eflags(this->staged_state_, static_cast<uint32_t>(rflags));
    }

    uint16_t fex_vcpu::segment_selector(int index) const
    {
        const auto& state = this->cpu_state();
        switch (index)
        {
        case 0:
            return state.es_idx;
        case 1:
            return state.cs_idx;
        case 2:
            return state.ss_idx;
        case 3:
            return state.ds_idx;
        case 4:
            return state.fs_idx;
        case 5:
            return state.gs_idx;
        default:
            return 0;
        }
    }

    void fex_vcpu::set_segment_selector(int index, const void* value, size_t size)
    {
        uint16_t selector = 0;
        std::memcpy(&selector, value, (std::min)(size, sizeof(selector)));
        auto& state = this->cpu_state();
        switch (index)
        {
        case 0:
            state.es_idx = selector;
            break;
        case 1:
            state.cs_idx = selector;
            break;
        case 2:
            state.ss_idx = selector;
            break;
        case 3:
            state.ds_idx = selector;
            break;
        case 4:
            state.fs_idx = selector;
            break;
        case 5:
            state.gs_idx = selector;
            break;
        default:
            break;
        }
    }

    void fex_vcpu::request_thread_stop()
    {
        // Forces the in-flight ExecuteThread to return, whether called from the same thread
        // (synchronously, e.g. from within a syscall hook) or a different one (e.g. a quantum
        // timer thread). FEXCore's JIT emits a `str zr, [InterruptFaultPage]` at every translated
        // block's entry when Config.NeedsPendingInterruptFaultCheck is set - protecting that page
        // makes the next block entry fault, landing in handle_fault_signal, which redirects any
        // fault on InterruptFaultPage into FEXCore's own ThreadStopHandlerAddress.
        auto* const active = this->active_thread_.load();
        if (active == nullptr)
        {
            return;
        }

#ifdef __APPLE__
        if (g_hvf != nullptr)
        {
            // Host mprotect has no effect on stage-2 translation - the same protocol needs the HVF
            // lever instead: revoke stage-2 write so the JIT's per-block-entry store aborts, plus an
            // immediate kick so a mid-block vCPU exits without waiting for the next block entry.
            g_hvf->protect(reinterpret_cast<uint64_t>(active->InterruptFaultPage), sizeof(active->InterruptFaultPage), PROT_READ);
            if (auto* const executor = this->hvf_executor_for_kick_.load())
            {
                executor->kick();
            }
            return;
        }
#endif

        ::mprotect(active->InterruptFaultPage, sizeof(active->InterruptFaultPage), PROT_NONE);
    }

    void fex_vcpu::create_thread()
    {
        // Seed the FEX thread from the staged CPUState the loader populated before the first start().
        this->thread_ = this->emulator_.context_->CreateThread(this->staged_state_.rip, this->staged_state_.gregs[detail::greg_rsp],
                                                               &this->staged_state_);
        this->active_context_ = this->emulator_.context_.get();
        this->active_thread_ = this->thread_;

        // FEXCore's core does not set up the "call-ret stack" (its own dedicated shadow stack for
        // x86 CALL/RET emulation, SRA-mapped to callret_sp) - replicate the embedder glue here.
        this->ensure_callret_stack(this->thread_->CurrentFrame->State);

#ifdef __APPLE__
        if (g_hvf != nullptr)
        {
            g_hvf->map(reinterpret_cast<uint64_t>(this->thread_), sizeof(FEXCore::Core::InternalThreadState), PROT_READ | PROT_WRITE);
            this->hvf_shim_thread_pointers(*this->thread_->CurrentFrame);
        }
        else
        {
            // See exit_function_link_jit_write_wrapper's doc comment: intercept the plain function-
            // pointer slot JIT-compiled code calls through to patch call sites, so the write into the
            // (MAP_JIT) code buffer happens with this thread's JIT write-protection disabled. Every
            // vCPU's thread shares the same original pointer - write-once. Not installed on the HVF
            // path: code buffers are plain RW there, so there is no W^X state to toggle.
            uint64_t expected_zero = 0;
            g_original_exit_function_link.compare_exchange_strong(expected_zero, this->thread_->CurrentFrame->Pointers.ExitFunctionLink);
            this->thread_->CurrentFrame->Pointers.ExitFunctionLink = reinterpret_cast<uint64_t>(&exit_function_link_jit_write_wrapper);
        }
#endif

        // Build thread32_ here too, in this ordinary call context, rather than leaving it to be
        // lazily created on the process's first gate crossing (unsafe from a signal handler).
        if (this->emulator_.is_wow64_process_ && this->thread32_ == nullptr)
        {
            this->create_thread32();
        }
    }

    void fex_vcpu::create_thread32()
    {
        this->thread32_ = this->emulator_.context32_->CreateThread(0, 0, nullptr);

#ifdef __APPLE__
        if (g_hvf != nullptr)
        {
            g_hvf->map(reinterpret_cast<uint64_t>(this->thread32_), sizeof(FEXCore::Core::InternalThreadState), PROT_READ | PROT_WRITE);
            this->hvf_shim_thread_pointers(*this->thread32_->CurrentFrame);
        }
#endif

        // Real Windows shares one GDT across both bitnesses of a wow64 process - point context32_'s
        // segment table at the exact same physical GDT memory sogen's loader wrote for this vCPU's
        // context_ engine.
        const auto rebase = this->emulator_.rebase_for(this->emulator_.is_wow64_process_, this->gdt_base_);
        this->thread32_->CurrentFrame->State.segment_arrays[0] =
            reinterpret_cast<FEXCore::Core::CPUState::gdt_segment*>(this->gdt_base_ + rebase);

        // ensure_callret_stack writes into whatever this->active_thread_ currently is - temporarily
        // point it at the new thread32_ engine so it gets its own private call-ret stack set up
        // correctly, then restore whatever was active before.
        auto* const previously_active_thread = this->active_thread_.load();
        this->active_thread_ = this->thread32_;
        this->ensure_callret_stack(this->thread32_->CurrentFrame->State);
        this->active_thread_ = previously_active_thread;
    }

    void fex_vcpu::ensure_callret_buffer(FEXCore::Core::CPUState& state)
    {
        if (state._pad1 == 0)
        {
            const size_t host_page = static_cast<size_t>(::getpagesize());
            constexpr size_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
            const size_t callret_alloc_size = callret_stack_size + 2 * host_page;

            void* alloc_base = FEXCore::Allocator::mmap(nullptr, callret_alloc_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (alloc_base == MAP_FAILED)
            {
                throw std::runtime_error("FEX backend failed to allocate the call-ret stack");
            }

            auto* callret_stack_base = static_cast<uint8_t*>(alloc_base) + host_page;
            if (::mprotect(callret_stack_base, callret_stack_size, PROT_READ | PROT_WRITE) != 0)
            {
                throw std::runtime_error("FEX backend failed to make the call-ret stack writable");
            }

#ifdef __APPLE__
            // The PROT_NONE allocation above is invisible to the VM (the arena mirror skips
            // PROT_NONE commits); only the writable interior gets mapped, so the surrounding guard
            // pages fault inside the vCPU exactly like they do on the host.
            if (g_hvf != nullptr)
            {
                g_hvf->map(reinterpret_cast<uint64_t>(callret_stack_base), callret_stack_size, PROT_READ | PROT_WRITE);
            }
#endif

            state._pad1 = reinterpret_cast<uint64_t>(callret_stack_base);
            state.callret_sp = reinterpret_cast<uint64_t>(callret_stack_base) + callret_stack_size / 4;

            const std::unique_lock lock(this->emulator_.tables_mutex_);
            this->emulator_.callret_buffers_.emplace_back(alloc_base, callret_alloc_size);
        }
    }

    void fex_vcpu::ensure_callret_stack(FEXCore::Core::CPUState& state)
    {
        this->ensure_callret_buffer(state);
        this->active_thread_.load()->CallRetStackBase = reinterpret_cast<void*>(state._pad1);
    }

    void fex_vcpu::mark_executable_range(uint64_t address, size_t size, memory_permission permissions)
    {
        auto* const active = this->active_thread_.load();
        if (active != nullptr && (permissions & memory_permission::exec) != memory_permission::none)
        {
            this->emulator_.syscall_handler_->MarkGuestExecutableRange(active, address, size);
        }
    }

    void fex_vcpu::invalidate_code_range_in(FEXCore::Context::Context* context, FEXCore::Core::InternalThreadState* thread,
                                            uint64_t address, size_t size) const
    {
        if (context == nullptr)
        {
            return;
        }

        // InvalidateCodeBuffersCodeRange/InvalidateThreadCachedCodeRange both require the caller to
        // already hold GetCodeInvalidationMutex() exclusively.
        std::unique_lock lock(context->GetCodeInvalidationMutex());

#ifdef __APPLE__
        ::pthread_jit_write_protect_np(0);
#endif
        context->InvalidateCodeBuffersCodeRange(address, size);
        if (thread != nullptr)
        {
            context->InvalidateThreadCachedCodeRange(thread, address, size);
        }
#ifdef __APPLE__
        ::pthread_jit_write_protect_np(1);
#endif
    }

    void fex_vcpu::invalidate_code_range(uint64_t address, size_t size, bool include_inactive_contexts) const
    {
        if (!this->active_context_)
        {
            return;
        }

        this->invalidate_code_range_in(this->active_context_, this->active_thread_.load(), address, size);

        // A WoW64 process runs two independent FEXCore contexts - invalidating only active_context_
        // leaves stale translations in the inactive one behind on an unmap.
        if (include_inactive_contexts && this->emulator_.context32_.get() != nullptr &&
            this->emulator_.context32_.get() != this->active_context_ && this->thread32_ != nullptr)
        {
            this->invalidate_code_range_in(this->emulator_.context32_.get(), this->thread32_, address, size);
        }
    }

#ifdef __APPLE__
    int& fex_vcpu::jit_write_protect_retry_count_for(const uint64_t fault_addr)
    {
        const uint64_t now_ns = monotonic_now_ns();
        jit_write_protect_retry_slot* free_slot = nullptr;
        for (auto& slot : this->jit_write_protect_retry_slots_)
        {
            if (slot.used && slot.address == fault_addr)
            {
                if (now_ns - slot.last_fault_ns > jit_write_protect_retry_reset_window_ns)
                {
                    slot.count = 0;
                }
                slot.last_fault_ns = now_ns;
                return slot.count;
            }
            if (free_slot == nullptr && !slot.used)
            {
                free_slot = &slot;
            }
        }

        auto& slot =
            (free_slot != nullptr)
                ? *free_slot
                : this->jit_write_protect_retry_slots_[this->jit_write_protect_retry_next_evict_++ % jit_write_protect_retry_slot_count];
        slot.address = fault_addr;
        slot.count = 0;
        slot.used = true;
        slot.last_fault_ns = now_ns;
        return slot.count;
    }

    // Applies a decode_arm64_load result once its data has been fetched - writes the (possibly
    // extended) value into the destination register and advances PC past the single decoded
    // instruction.
    void fex_vcpu::complete_decoded_load(ucontext_t* uctx, const decoded_arm64_load& decoded, const void* data, uint64_t pc)
    {
        if (decoded.is_vector)
        {
            __uint128_t value{};
            std::memcpy(&value, data, sizeof(value));
            auto* fprs = reinterpret_cast<__uint128_t*>(&uctx->uc_mcontext->__ns.__v[0]);
            fprs[decoded.rt] = value;
            arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(pc + 4));
            return;
        }

        uint64_t raw_value = 0;
        std::memcpy(&raw_value, data, decoded.size);

        uint64_t result = 0;
        switch (decoded.size)
        {
        case 1:
            result =
                decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(raw_value))) : (raw_value & 0xFFULL);
            break;
        case 2:
            result = decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(raw_value)))
                                         : (raw_value & 0xFFFFULL);
            break;
        case 4:
            result = decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(raw_value)))
                                         : (raw_value & 0xFFFFFFFFULL);
            break;
        default:
            result = raw_value;
            break;
        }

        if (!decoded.dest_is_64bit)
        {
            result &= 0xFFFFFFFFULL;
        }

        if (decoded.rt <= 28)
        {
            uctx->uc_mcontext->__ss.__x[decoded.rt] = result;
        }
        else if (decoded.rt == 29)
        {
            uctx->uc_mcontext->__ss.__fp = result;
        }
        else if (decoded.rt == 30)
        {
            uctx->uc_mcontext->__ss.__lr = result;
        }

        arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(pc + 4));
    }

    bool fex_vcpu::handle_mmio_fault(ucontext_t* uctx, const mmio_region& region, uint64_t fault_addr)
    {
        const uint64_t pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
        const auto insn = *reinterpret_cast<const uint32_t*>(pc);
        const auto decoded = decode_arm64_load(insn);
        if (!decoded)
        {
            char buf[128];
            const int len = snprintf(buf, sizeof(buf), "[MMIO] unrecognized instruction 0x%08x at pc=%p for fault_addr=0x%llx\n", insn,
                                     reinterpret_cast<void*>(pc), static_cast<unsigned long long>(fault_addr));
            if (len > 0)
            {
                const auto write_len = static_cast<size_t>(len) < sizeof(buf) ? static_cast<size_t>(len) : sizeof(buf);
                ::write(STDERR_FILENO, buf, write_len);
            }
            return false;
        }

        alignas(16) std::byte buffer[16]{};
        region.read_cb(fault_addr - region.address, buffer, decoded->size);
        this->complete_decoded_load(uctx, *decoded, buffer, pc);
        return true;
    }

    namespace
    {
        // x86 keeps plain unaligned loads/stores single-copy atomic as long as they stay inside one
        // cache line (Intel SDM vol. 3A, 9.1.1), and the faulted LDAR/LDAPR/STLR additionally
        // carried acquire/release ordering - a plain memcpy emulation provides neither, so another
        // vCPU doing a concurrent non-faulting access to overlapping bytes could observe a torn
        // value. Accesses contained in one aligned 16-byte window go through 128-bit atomics
        // (single-copy atomic per LSE2, which every Apple Silicon core has); accesses spanning two
        // windows keep the memcpy but regain the ordering via fences - hardware x86 still
        // guarantees atomicity for those when they stay inside one cache line, an ARM64 host simply
        // has no primitive wide enough to reproduce it.
        bool contained_in_atomic_window(const uint64_t addr, const uint32_t size)
        {
            constexpr uint64_t window_mask = ~uint64_t{15};
            return (addr & window_mask) == ((addr + size - 1) & window_mask);
        }

        void read_memory_single_copy_atomic(const uint64_t addr, void* out, const uint32_t size)
        {
            if (contained_in_atomic_window(addr, size))
            {
                const uint64_t window = addr & ~uint64_t{15};
                const auto value = __atomic_load_n(reinterpret_cast<const unsigned __int128*>(window), __ATOMIC_SEQ_CST);
                std::memcpy(out, reinterpret_cast<const std::byte*>(&value) + (addr - window), size);
                return;
            }

            std::atomic_thread_fence(std::memory_order_seq_cst);
            std::memcpy(out, reinterpret_cast<const void*>(addr), size);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }

        void write_memory_single_copy_atomic(const uint64_t addr, const void* data, const uint32_t size)
        {
            if (contained_in_atomic_window(addr, size))
            {
                const uint64_t window = addr & ~uint64_t{15};
                auto* const target = reinterpret_cast<unsigned __int128*>(window);
                auto expected = __atomic_load_n(target, __ATOMIC_RELAXED);
                while (true)
                {
                    auto desired = expected;
                    std::memcpy(reinterpret_cast<std::byte*>(&desired) + (addr - window), data, size);
                    if (__atomic_compare_exchange_n(target, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
                    {
                        return;
                    }
                }
            }

            std::atomic_thread_fence(std::memory_order_seq_cst);
            std::memcpy(reinterpret_cast<void*>(addr), data, size);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
    }

    // Real hardware LDAR/LDAPR/STLR (load-acquire/store-release) instructions require natural
    // alignment, unlike plain LDR/STR - but x86 permits unaligned accesses freely, and FEX uses this
    // family to model x86's stronger memory ordering on ARM's weaker one, so an ordinary unaligned
    // guest access to otherwise legitimately mapped memory can fault here. Under real multi-vCPU
    // concurrency, two vCPUs can hit this same handler for the same address at the same time -
    // tables_mutex_ makes the emulated access mutually exclusive against every other vCPU that also
    // faults, while the single-copy-atomic helpers above protect against concurrent accesses that
    // never fault and thus never take the mutex.
    bool fex_vcpu::handle_misaligned_atomic_fault(ucontext_t* uctx, uint64_t fault_addr)
    {
        const uint64_t pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
        const auto insn = *reinterpret_cast<const uint32_t*>(pc);

        const std::unique_lock lock(this->emulator_.tables_mutex_);

        if (const auto load = decode_arm64_load(insn))
        {
            alignas(16) std::byte buffer[16]{};
            read_memory_single_copy_atomic(fault_addr, buffer, load->size);
            this->complete_decoded_load(uctx, *load, buffer, pc);
            return true;
        }

        if (const auto store = decode_arm64_store(insn))
        {
            uint64_t value = 0;
            if (store->rt <= 28)
            {
                value = uctx->uc_mcontext->__ss.__x[store->rt];
            }
            else if (store->rt == 29)
            {
                value = uctx->uc_mcontext->__ss.__fp;
            }
            else if (store->rt == 30)
            {
                value = uctx->uc_mcontext->__ss.__lr;
            }

            write_memory_single_copy_atomic(fault_addr, &value, store->size);
            arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(pc + 4));
            return true;
        }

        return false;
    }

    bool fex_vcpu::handle_callret_stack_fault(ucontext_t* uctx, uint64_t fault_addr) const
    {
        auto* const active = this->active_thread_.load();
        if (active == nullptr || active->CallRetStackBase == nullptr)
        {
            return false;
        }
        const auto base = reinterpret_cast<uint64_t>(active->CallRetStackBase);
        const auto host_page = static_cast<uint64_t>(::getpagesize());
        constexpr uint64_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
        if (fault_addr < base - host_page || fault_addr >= base + callret_stack_size + host_page)
        {
            return false;
        }
        uctx->uc_mcontext->__ss.__x[25] = base + callret_stack_size / 4;
        return true;
    }

    bool fex_vcpu::handle_general_memory_violation(ucontext_t* uctx, uint64_t fault_addr)
    {
        const uint64_t pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
        const auto guest_fault_addr = this->emulator_.unrebase_fault_addr(fault_addr);
        const auto guest_page = guest_fault_addr & ~(page_size - 1);
        memory_permission declared;
        {
            const std::shared_lock lock(this->emulator_.tables_mutex_);
            const auto shadow_it = this->emulator_.page_shadow_apple_.find(guest_page);
            declared = (shadow_it != this->emulator_.page_shadow_apple_.end()) ? shadow_it->second : memory_permission::none;
        }

        memory_operation operation = memory_operation::exec;
        if (fault_addr != pc)
        {
            const auto insn = *reinterpret_cast<const uint32_t*>(pc);
            operation = decode_arm64_store(insn) ? memory_operation::write : memory_operation::read;
        }

        if ((declared & operation) == operation)
        {
            return this->handle_misaligned_atomic_fault(uctx, fault_addr);
        }

        const auto type = (declared == memory_permission::none) ? memory_violation_type::unmapped : memory_violation_type::protection;

        // This fault interrupted live guest-translated JIT code at an arbitrary point. Reconstruct the
        // real guest rip from the live host PC (FEX's block-chaining advances execution without
        // rewriting CurrentFrame->State.rip, which is frequently stale here).
        auto* const active = this->active_thread_.load();
        if (const uint64_t recon_rip = this->active_context_->RestoreRIPFromHostPC(active, pc))
        {
            active->CurrentFrame->State.rip = recon_rip;
        }

        pending_fault_dispatch dispatch{};
        dispatch.kind = pending_fault_kind::memory_violation;
        dispatch.address = guest_fault_addr;
        dispatch.size = 1;
        dispatch.operation = operation;
        dispatch.type = type;

        this->defer_hook_dispatch(uctx, dispatch, /*sra_already_spilled=*/false);
        return true;
    }

    bool fex_vcpu::host_pc_in_any_dispatcher(uint64_t pc) const
    {
        for (const auto* delegator : {this->emulator_.signal_delegator_.get(), this->emulator_.signal_delegator32_.get()})
        {
            if (delegator == nullptr)
            {
                continue;
            }
            const auto& cfg = delegator->GetConfig();
            if (pc >= cfg.DispatcherBegin && pc < cfg.DispatcherEnd)
            {
                return true;
            }
        }
        return false;
    }

    // -----------------------------------------------------------------------------------------------
    // HVF execution path: the same quantum/stop/hook state machine as the signal-based start()
    // above, but guest execution happens inside a Hypervisor.framework vCPU with hardware TSO, and
    // guest faults arrive as VM exits in ordinary thread context instead of POSIX signals.
    // -----------------------------------------------------------------------------------------------

    void fex_vcpu::start_hvf(const size_t count)
    {
        this->emulator_.refresh_mmio_backings();

        if (count != 0)
        {
            throw std::runtime_error("FEX backend does not support exact instruction counts yet");
        }

        const current_vcpu_scope current_vcpu_guard(*this);

        if (this->active_thread_.load() == nullptr)
        {
            this->create_thread();
        }

        if (!this->hvf_executor_)
        {
            this->hvf_executor_ = std::make_unique<hvf::hvf_vcpu_executor>(*g_hvf);
            this->hvf_executor_for_kick_.store(this->hvf_executor_.get());
        }

        if (this->hvf_emulator_stack_top_ == 0)
        {
            // Stands in for the host thread stack ExecuteDispatch would use: the dispatcher's
            // PushCalleeSavedRegisters frame and the Syscall op's argument staging live here.
            // Allocated through the FEX allocator hooks so it lands in the already-mirrored arena.
            constexpr size_t emulator_stack_size = 8ull << 20;
            void* stack =
                FEXCore::Allocator::mmap(nullptr, emulator_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (stack == MAP_FAILED)
            {
                throw std::runtime_error("FEX backend failed to allocate the HVF emulator stack");
            }
            this->hvf_emulator_stack_top_ = (reinterpret_cast<uint64_t>(stack) + emulator_stack_size - 64) & ~static_cast<uint64_t>(15);
        }

        this->stop_requested_ = false;
        {
            auto* const active = this->active_thread_.load();
            g_hvf->protect(reinterpret_cast<uint64_t>(active->InterruptFaultPage), sizeof(active->InterruptFaultPage),
                           PROT_READ | PROT_WRITE);
        }

        hvf_exit_adapter adapter{*this};
        for (;;)
        {
            auto* const active = this->active_thread_.load();
            auto* const delegator = (this->active_context_ == this->emulator_.context32_.get()) ? this->emulator_.signal_delegator32_.get()
                                                                                                : this->emulator_.signal_delegator_.get();
            const auto& cfg = delegator->GetConfig();

            this->hvf_executor_->run_dispatch(cfg.DispatcherBegin, reinterpret_cast<uint64_t>(active->CurrentFrame),
                                              this->hvf_emulator_stack_top_, adapter);

            const bool hook_dispatched = this->dispatch_pending_hook_if_any();
            const bool interrupt_page_unwind = std::exchange(this->interrupt_page_unwind_, false);

            // Same quantum-timer-race handling as the signal-based loop above.
            if (this->stop_requested_ || (!hook_dispatched && !interrupt_page_unwind))
            {
                break;
            }

            auto* const now_active = this->active_thread_.load();
            g_hvf->protect(reinterpret_cast<uint64_t>(now_active->InterruptFaultPage), sizeof(now_active->InterruptFaultPage),
                           PROT_READ | PROT_WRITE);
        }
    }

    void fex_vcpu::hvf_shim_thread_pointers(FEXCore::Core::CpuStateFrame& frame) const
    {
        auto& pointers = frame.Pointers;
        const auto shim = [](uint64_t& slot, const bool needs_fp) {
            if (slot == 0 || g_hvf->runtime().contains_stub(slot) || g_hvf->x87_fastpath().contains_entry(slot))
            {
                return;
            }
            slot = g_hvf->register_callback(slot, needs_fp);
        };

        shim(pointers.PrintValue, false);
        shim(pointers.PrintVectorValue, false);
        shim(pointers.PrintMsgValue, false);
        shim(pointers.ThreadRemoveCodeEntryFromJIT, false);
        shim(pointers.CPUIDFunction, false);
        shim(pointers.XCRFunction, false);
        shim(pointers.SyscallHandlerFunc, false);
        shim(pointers.ExitFunctionLink, false);
        shim(pointers.MonoBackpatcherWrite, false);
        shim(pointers.LUDIV, false);
        shim(pointers.LDIV, false);
        shim(pointers.CompileBlockFunc, false);
        shim(pointers.CompileSingleStepFunc, false);
        shim(pointers.SleepFunc, false);

        for (size_t i = 0; i < FEXCore::Core::OPINDEX_MAX; ++i)
        {
            shim(pointers.FallbackHandlerPointers[i].Func, true);
        }

        // Ops with a guest-resident implementation call it instead of exiting; it falls back to the
        // hypercall stub installed above for the operand shapes it does not handle.
        static constexpr std::pair<FEXCore::Core::FallbackHandlerIndex, hvf::hvf_x87_fastpath::op> fastpath_ops[] = {
            {FEXCore::Core::OPINDEX_F80CVT_4, hvf::hvf_x87_fastpath::op::f80_cvt_f32},
            {FEXCore::Core::OPINDEX_F80CVT_8, hvf::hvf_x87_fastpath::op::f80_cvt_f64},
            {FEXCore::Core::OPINDEX_F80MUL, hvf::hvf_x87_fastpath::op::f80_mul},
            {FEXCore::Core::OPINDEX_F80ADD, hvf::hvf_x87_fastpath::op::f80_add},
            {FEXCore::Core::OPINDEX_F80SUB, hvf::hvf_x87_fastpath::op::f80_sub},
        };

        for (const auto& [index, which] : fastpath_ops)
        {
            auto& slot = pointers.FallbackHandlerPointers[index].Func;
            if (slot != 0 && g_hvf->runtime().contains_stub(slot))
            {
                slot = g_hvf->x87_fastpath().bind(which, slot);
            }
        }

        // These two are the only slots the guest dereferences rather than calls; every host
        // function above stays at its real address because the host, not the guest, runs it.
        const auto rebase_constant = [](uint64_t& slot) {
            if (g_fexcore_mirror.contains(slot))
            {
                slot = g_fexcore_mirror.rebase(slot);
            }
        };

        for (auto& slot : pointers.NamedVectorConstantPointers)
        {
            rebase_constant(slot);
        }
        for (auto& slot : pointers.IndexedNamedVectorConstantPointers)
        {
            rebase_constant(slot);
        }
    }

    bool fex_vcpu::hvf_on_stage2_abort(hvf::hvf_vcpu_executor& vcpu, const uint64_t va, uint64_t /*ipa*/, const uint64_t syndrome)
    {
        auto* const active_thread = this->active_thread_.load();
        if (active_thread == nullptr)
        {
            return false;
        }

        const bool is_data = ((syndrome >> 26) & 0x3F) == 0x24;
        const auto interrupt_page_addr = reinterpret_cast<uint64_t>(active_thread->InterruptFaultPage);
        if (is_data && va >= interrupt_page_addr && va < interrupt_page_addr + sizeof(active_thread->InterruptFaultPage))
        {
            const uint64_t fault_pc = vcpu.get_pc();
            const bool is_dispatch_code = this->active_context_ && this->active_context_->IsAddressInCodeBuffer(active_thread, fault_pc);
            const bool is_strb_epilogue_write = (*reinterpret_cast<const uint32_t*>(fault_pc) & 0xFFC00000u) == 0x39000000u;

            if (is_dispatch_code && !is_strb_epilogue_write)
            {
                active_thread->CurrentFrame->State.rip = this->active_context_->RestoreRIPFromHostPC(active_thread, fault_pc);
                this->interrupt_page_unwind_ = true;
                const auto& stop_cfg = this->emulator_.signal_delegator_->GetConfig();
                vcpu.set_pc(stop_cfg.ThreadStopHandlerAddressSpillSRA);
                return true;
            }

            vcpu.set_pc(fault_pc + 4);
            return true;
        }

        // Anything else reaching stage-2 has a valid stage-1 entry but revoked backing permissions
        // (e.g. a guest write to the read-only-mapped real MMIO backing) - same dispatch as a
        // permission fault. The syndrome's WnR bit (ISS[6]) classifies the access exactly, unlike
        // the deliberately-narrow store decode table (which cannot see plain stores - and with
        // hardware TSO every guest store is a plain store).
        const memory_operation operation = !is_data                     ? memory_operation::exec
                                           : ((syndrome >> 6) & 1) != 0 ? memory_operation::write
                                                                        : memory_operation::read;
        return this->hvf_handle_general_memory_violation(vcpu, va, vcpu.get_pc(), operation);
    }

    bool fex_vcpu::hvf_on_guest_exception(hvf::hvf_vcpu_executor& vcpu, uint32_t /*vector_entry*/)
    {
        auto* const active_thread = this->active_thread_.load();
        if (active_thread == nullptr)
        {
            return false;
        }

        const uint64_t esr = vcpu.esr_el1();
        const uint64_t elr = vcpu.elr_el1();
        const auto ec = static_cast<uint32_t>((esr >> 26) & 0x3F);

        if (ec == 0x25) // data abort taken at EL1: stage-1 unmapped / permission / alignment
        {
            const uint64_t fault_addr = vcpu.far_el1();
            const uint32_t dfsc = esr & 0x3F;

            if (this->hvf_handle_callret_stack_fault(vcpu, fault_addr))
            {
                vcpu.set_pc(elr);
                return true;
            }

            {
                const auto guest_fault_addr = this->emulator_.unrebase_fault_addr(fault_addr);
                const std::shared_lock lock(this->emulator_.tables_mutex_);
                for (const auto& region : this->emulator_.mmio_regions_)
                {
                    if (guest_fault_addr >= region.address && guest_fault_addr < region.address + region.size)
                    {
                        return this->hvf_handle_mmio_fault(vcpu, region, guest_fault_addr, elr);
                    }
                }
            }

            constexpr uint32_t dfsc_alignment_fault = 0x21;
            if (dfsc == dfsc_alignment_fault && this->hvf_handle_misaligned_atomic_fault(vcpu, fault_addr, elr))
            {
                return true;
            }

            // FEXCore's GuestSignal_SIGSEGV dispatcher stub does not trap - unlike its SIGILL/SIGTRAP
            // siblings (hlt(0)/brk(0)) it raises a real SIGSEGV by deliberately dereferencing null:
            // `LoadConstant(r1, 0); ldr x1, [x1]` (Dispatcher.cpp, the !ExitOnHLTEnabled branch). Inside
            // the VM that is an ordinary stage-1 data abort at EL1 with FAR=0, syndrome-identical to a
            // genuine guest null dereference, so it must be told apart by the faulting pc instead - the
            // same discrimination handle_fault_signal already performs by only reaching
            // handle_general_memory_violation when the faulting pc is NOT in a dispatcher. Every Break
            // op with Signal=FAULT_SIGSEGV arrives this way, which includes every WoW64 gate crossing
            // (NoExecOp on a registered gate range emits exactly that Break), so misreading it as a
            // guest memory violation at address 0 loses the crossing and re-faults forever.
            const bool is_dispatcher_generated_break =
                this->host_pc_in_any_dispatcher(elr) && active_thread->CurrentFrame->SynchronousFaultData.FaultToTopAndGeneratedException;
            if (!is_dispatcher_generated_break)
            {
                const memory_operation operation = ((esr >> 6) & 1) != 0 ? memory_operation::write : memory_operation::read;
                return this->hvf_handle_general_memory_violation(vcpu, fault_addr, elr, operation);
            }
        }

        if (ec == 0x21) // instruction abort taken at EL1
        {
            return this->hvf_handle_general_memory_violation(vcpu, elr, elr, memory_operation::exec);
        }

        // Break-op traps: the dispatcher's GuestSignal_* stubs execute hlt(0) (UNDEF at EL1,
        // EC 0x00) or brk(0) (EC 0x3C) to surface a guest-generated exception - the same events
        // that arrive as SIGILL/SIGTRAP on the in-process path.
        if (!this->host_pc_in_any_dispatcher(elr))
        {
            return false;
        }

        auto* frame = active_thread->CurrentFrame;
        if (!frame->SynchronousFaultData.FaultToTopAndGeneratedException)
        {
            return false;
        }

        auto vector = static_cast<int>(frame->SynchronousFaultData.TrapNo);

        constexpr int gp_fault_vector = 13;
        constexpr uint32_t idt_reference_bit = 0x2;
        if (vector == gp_fault_vector && (frame->SynchronousFaultData.err_code & idt_reference_bit) != 0)
        {
            vector = static_cast<int>(frame->SynchronousFaultData.err_code >> 3);
        }

        frame->SynchronousFaultData.FaultToTopAndGeneratedException = false;

        pending_fault_dispatch dispatch{};
        if (vector == 14)
        {
            if (const auto gate = this->emulator_.find_gate_crossing(frame->State.rip))
            {
                auto* const source_signal_delegator = (this->active_context_ == this->emulator_.context32_.get())
                                                          ? this->emulator_.signal_delegator32_.get()
                                                          : this->emulator_.signal_delegator_.get();

                if (this->perform_gate_crossing(*gate))
                {
                    this->pending_fault_dispatch_.kind = pending_fault_kind::gate_crossing;
                    const auto& stop_cfg = source_signal_delegator->GetConfig();
                    vcpu.set_pc(stop_cfg.ThreadStopHandlerAddress);
                    return true;
                }
            }

            const auto err_code = frame->SynchronousFaultData.err_code;
            const bool is_write = (err_code & 0x2) != 0;
            const bool is_instr_fetch = (err_code & 0x10) != 0;
            dispatch.kind = pending_fault_kind::memory_violation;
            dispatch.address = frame->State.rip;
            dispatch.size = 1;
            dispatch.operation = is_instr_fetch ? memory_operation::exec : is_write ? memory_operation::write : memory_operation::read;
            dispatch.type = (err_code & 0x1) ? memory_violation_type::protection : memory_violation_type::unmapped;
        }
        else
        {
            dispatch.kind = pending_fault_kind::interrupt;
            dispatch.vector = vector;
        }

        this->hvf_defer_hook_dispatch(vcpu, dispatch, /*sra_already_spilled=*/true);
        return true;
    }

    void fex_vcpu::hvf_complete_decoded_load(hvf::hvf_vcpu_executor& vcpu, const decoded_arm64_load& decoded, const void* data,
                                             const uint64_t pc) const
    {
        if (decoded.is_vector)
        {
            vcpu.set_simd(decoded.rt, data);
            vcpu.set_pc(pc + 4);
            return;
        }

        uint64_t raw_value = 0;
        std::memcpy(&raw_value, data, decoded.size);

        uint64_t result = 0;
        switch (decoded.size)
        {
        case 1:
            result =
                decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(raw_value))) : (raw_value & 0xFFULL);
            break;
        case 2:
            result = decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(raw_value)))
                                         : (raw_value & 0xFFFFULL);
            break;
        case 4:
            result = decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(raw_value)))
                                         : (raw_value & 0xFFFFFFFFULL);
            break;
        default:
            result = raw_value;
            break;
        }

        if (!decoded.dest_is_64bit)
        {
            result &= 0xFFFFFFFFULL;
        }

        if (decoded.rt <= 30)
        {
            vcpu.set_gpr(decoded.rt, result);
        }

        vcpu.set_pc(pc + 4);
    }

    bool fex_vcpu::hvf_handle_mmio_fault(hvf::hvf_vcpu_executor& vcpu, const mmio_region& region, const uint64_t guest_fault_addr,
                                         const uint64_t pc) const
    {
        const auto insn = *reinterpret_cast<const uint32_t*>(pc);
        const auto decoded = decode_arm64_load(insn);
        if (!decoded)
        {
            fprintf(stderr, "[MMIO] unrecognized instruction 0x%08x at pc=0x%llx for fault_addr=0x%llx\n", insn,
                    static_cast<unsigned long long>(pc), static_cast<unsigned long long>(guest_fault_addr));
            return false;
        }

        alignas(16) std::byte buffer[16]{};
        region.read_cb(guest_fault_addr - region.address, buffer, decoded->size);
        this->hvf_complete_decoded_load(vcpu, *decoded, buffer, pc);
        return true;
    }

    bool fex_vcpu::hvf_handle_misaligned_atomic_fault(hvf::hvf_vcpu_executor& vcpu, const uint64_t fault_addr, const uint64_t pc)
    {
        const auto insn = *reinterpret_cast<const uint32_t*>(pc);

        const std::unique_lock lock(this->emulator_.tables_mutex_);

        if (const auto load = decode_arm64_load(insn))
        {
            alignas(16) std::byte buffer[16]{};
            read_memory_single_copy_atomic(fault_addr, buffer, load->size);
            this->hvf_complete_decoded_load(vcpu, *load, buffer, pc);
            return true;
        }

        if (const auto store = decode_arm64_store(insn))
        {
            const uint64_t value = store->rt <= 30 ? vcpu.get_gpr(store->rt) : 0;
            write_memory_single_copy_atomic(fault_addr, &value, store->size);
            vcpu.set_pc(pc + 4);
            return true;
        }

        return false;
    }

    bool fex_vcpu::hvf_handle_callret_stack_fault(hvf::hvf_vcpu_executor& vcpu, const uint64_t fault_addr) const
    {
        auto* const active = this->active_thread_.load();
        if (active == nullptr || active->CallRetStackBase == nullptr)
        {
            return false;
        }
        const auto base = reinterpret_cast<uint64_t>(active->CallRetStackBase);
        const auto host_page = static_cast<uint64_t>(::getpagesize());
        constexpr uint64_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
        if (fault_addr < base - host_page || fault_addr >= base + callret_stack_size + host_page)
        {
            return false;
        }
        constexpr unsigned reg_callret_sp = 25;
        vcpu.set_gpr(reg_callret_sp, base + callret_stack_size / 4);
        return true;
    }

    bool fex_vcpu::hvf_handle_general_memory_violation(hvf::hvf_vcpu_executor& vcpu, const uint64_t fault_addr, const uint64_t pc,
                                                       const memory_operation operation)
    {
        const auto guest_fault_addr = this->emulator_.unrebase_fault_addr(fault_addr);
        const auto guest_page = guest_fault_addr & ~(page_size - 1);
        memory_permission declared;
        {
            const std::shared_lock lock(this->emulator_.tables_mutex_);
            const auto shadow_it = this->emulator_.page_shadow_apple_.find(guest_page);
            declared = (shadow_it != this->emulator_.page_shadow_apple_.end()) ? shadow_it->second : memory_permission::none;
        }

        if ((declared & operation) == operation)
        {
            return this->hvf_handle_misaligned_atomic_fault(vcpu, fault_addr, pc);
        }

        const auto type = (declared == memory_permission::none) ? memory_violation_type::unmapped : memory_violation_type::protection;

        if (std::getenv("EMULATOR_FEX_HVF_DIAG") != nullptr)
        {
            int host_probe = -1;
            char probe_byte = 0;
            mach_vm_size_t read_count = 0;
            const kern_return_t kr = ::mach_vm_read_overwrite(mach_task_self(), fault_addr & ~static_cast<uint64_t>(0xFFF), 1,
                                                              reinterpret_cast<mach_vm_address_t>(&probe_byte), &read_count);
            host_probe = (kr == KERN_SUCCESS) ? 1 : 0;
            fprintf(stderr,
                    "[HVF diag] general violation: fault=0x%llx guest=0x%llx pc=0x%llx declared=%d op=%d vm_mapped=%d "
                    "host_readable=%d esr=0x%llx insn=0x%08x\n",
                    static_cast<unsigned long long>(fault_addr), static_cast<unsigned long long>(guest_fault_addr),
                    static_cast<unsigned long long>(pc), static_cast<int>(declared), static_cast<int>(operation),
                    g_hvf->is_mapped_page(fault_addr) ? 1 : 0, host_probe, static_cast<unsigned long long>(vcpu.esr_el1()),
                    *reinterpret_cast<const uint32_t*>(pc));
        }

        auto* const active = this->active_thread_.load();
        if (const uint64_t recon_rip = this->active_context_->RestoreRIPFromHostPC(active, pc))
        {
            active->CurrentFrame->State.rip = recon_rip;
        }

        pending_fault_dispatch dispatch{};
        dispatch.kind = pending_fault_kind::memory_violation;
        dispatch.address = guest_fault_addr;
        dispatch.size = 1;
        dispatch.operation = operation;
        dispatch.type = type;

        this->hvf_defer_hook_dispatch(vcpu, dispatch, /*sra_already_spilled=*/false);
        return true;
    }

    void fex_vcpu::hvf_defer_hook_dispatch(hvf::hvf_vcpu_executor& vcpu, const pending_fault_dispatch& dispatch,
                                           const bool sra_already_spilled)
    {
        this->pending_fault_dispatch_ = dispatch;
        const auto& cfg = this->emulator_.signal_delegator_->GetConfig();
        vcpu.set_pc(sra_already_spilled ? cfg.ThreadStopHandlerAddress : cfg.ThreadStopHandlerAddressSpillSRA);
    }

    bool fex_vcpu::dispatch_pending_hook_if_any()
    {
        const pending_fault_dispatch dispatch = this->pending_fault_dispatch_;
        this->pending_fault_dispatch_.kind = pending_fault_kind::none;

        switch (dispatch.kind)
        {
        case pending_fault_kind::memory_violation:
            for (auto& [_, hook] : this->emulator_.memory_violation_hooks_)
            {
                hook(*this, dispatch.address, dispatch.size, dispatch.operation, dispatch.type);
            }
            return true;
        case pending_fault_kind::interrupt:
            for (auto& [_, hook] : this->emulator_.interrupt_hooks_)
            {
                hook(*this, dispatch.vector);
            }
            return true;
        case pending_fault_kind::gate_crossing:
            return true;
        case pending_fault_kind::none:
        default:
            return false;
        }
    }

    void fex_vcpu::defer_hook_dispatch(ucontext_t* uctx, const pending_fault_dispatch& dispatch, bool sra_already_spilled)
    {
        this->pending_fault_dispatch_ = dispatch;
        const auto& cfg = this->emulator_.signal_delegator_->GetConfig();
        const auto target = sra_already_spilled ? cfg.ThreadStopHandlerAddress : cfg.ThreadStopHandlerAddressSpillSRA;
        arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(target));
    }

    bool fex_vcpu::enter_wow64_32bit_from_run_simulated_code(const gate_crossing& gate)
    {
        const auto& src = this->thread_->CurrentFrame->State;

        uint64_t teb64 = 0;
        if (!this->emulator_.try_read_memory(src.gs_cached + 0x30, &teb64, sizeof(teb64)) || teb64 == 0)
        {
            return false;
        }
        uint64_t cpu_area = 0;
        if (!this->emulator_.try_read_memory(teb64 + 0x1488, &cpu_area, sizeof(cpu_area)) || cpu_area == 0)
        {
            return false;
        }
        const uint64_t block = cpu_area + 0x80;

        bool reads_ok = true;
        const auto read32 = [&](uint64_t offset) -> uint32_t {
            uint32_t value = 0;
            if (!this->emulator_.try_read_memory(block + offset, &value, sizeof(value)))
            {
                reads_ok = false;
            }
            return value;
        };

        const uint32_t edi = read32(0x20);
        const uint32_t esi = read32(0x24);
        const uint32_t ebx = read32(0x28);
        const uint32_t edx = read32(0x2c);
        const uint32_t ecx = read32(0x30);
        const uint32_t eax = read32(0x34);
        const uint32_t ebp = read32(0x38);
        const uint32_t eip = read32(0x3c);
        const uint32_t eflags = read32(0x44);
        const uint32_t esp = read32(0x48);

        if (!reads_ok)
        {
            return false;
        }

        if (this->thread32_ == nullptr)
        {
            return false;
        }
        auto& dst = this->thread32_->CurrentFrame->State;

        marshal_architectural_state(src, dst);

        dst.gregs[detail::greg_rax] = eax;
        dst.gregs[detail::greg_rcx] = ecx;
        dst.gregs[detail::greg_rdx] = edx;
        dst.gregs[detail::greg_rbx] = ebx;
        dst.gregs[detail::greg_rsp] = esp;
        dst.gregs[detail::greg_rbp] = ebp;
        dst.gregs[detail::greg_rsi] = esi;
        dst.gregs[detail::greg_rdi] = edi;
        dst.rip = eip;
        set_flags_from_compacted_eflags(dst, eflags);

        for (int i = 0; i < 6; ++i)
        {
            this->emulator_.try_read_memory(block + 0xf0 + static_cast<uint64_t>(i) * 0x10, &dst.xmm.avx.data[i][0], 16);
        }

        dst.cs_idx = 0x23;
        dst.ss_idx = 0x2b;
        dst.ds_idx = 0x2b;
        dst.es_idx = 0x2b;
        dst.fs_idx = 0x53;
        dst.gs_idx = 0;
        dst.cs_cached = fex_x86_64_emulator::gdt_segment_base(dst, 0x23);
        dst.ss_cached = fex_x86_64_emulator::gdt_segment_base(dst, 0x2b);
        dst.ds_cached = fex_x86_64_emulator::gdt_segment_base(dst, 0x2b);
        dst.es_cached = fex_x86_64_emulator::gdt_segment_base(dst, 0x2b);
        dst.fs_cached = fex_x86_64_emulator::gdt_segment_base(dst, 0x53);
        dst.gs_cached = 0;

        auto& state64 = this->thread_->CurrentFrame->State;
        if (src.rip == gate.address)
        {
            const uint64_t entry_rsp = state64.gregs[detail::greg_rsp];
            const auto spill = [&](uint64_t below_entry, int greg) {
                const uint64_t value = state64.gregs[greg];
                this->emulator_.write_marshal_state(entry_rsp - below_entry, &value, sizeof(value));
            };
            spill(0x08, 15);
            spill(0x10, 14);
            spill(0x18, 13);
            spill(0x20, 12);
            spill(0x28, detail::greg_rbx);
            spill(0x30, detail::greg_rsi);
            spill(0x38, detail::greg_rdi);
            spill(0x40, detail::greg_rbp);
            state64.gregs[detail::greg_rsp] = entry_rsp - 0xA8;
        }

        state64.gregs[12] = teb64;
        state64.gregs[13] = block;
        state64.gregs[14] = state64.gregs[detail::greg_rsp];
        state64.gregs[15] = (gate.address & ~static_cast<uint64_t>(0xFFFF)) + 0x36d0;

        this->active_context_ = this->emulator_.context32_.get();
        this->active_thread_ = this->thread32_;
        return true;
    }

    bool fex_vcpu::enter_wow64_64bit_from_wow64svc_thunk(const gate_crossing& gate)
    {
        const uint64_t image_base = gate.address & ~static_cast<uint64_t>(0xFFFF);
        const uint64_t generic_dispatch =
            this->emulator_.wow64_turbo_dispatch_end_ != 0 ? this->emulator_.wow64_turbo_dispatch_end_ : image_base + 0x17af;
        const uint64_t jump_table = image_base + 0x36d0;

        auto* const active = this->active_thread_.load();
        const auto& src32 = active->CurrentFrame->State;
        const uint32_t eax = static_cast<uint32_t>(src32.gregs[detail::greg_rax]);
        const uint32_t ecx = static_cast<uint32_t>(src32.gregs[detail::greg_rcx]);
        const uint32_t edx = static_cast<uint32_t>(src32.gregs[detail::greg_rdx]);
        const uint32_t ebx = static_cast<uint32_t>(src32.gregs[detail::greg_rbx]);
        const uint32_t ebp = static_cast<uint32_t>(src32.gregs[detail::greg_rbp]);
        const uint32_t esi = static_cast<uint32_t>(src32.gregs[detail::greg_rsi]);
        const uint32_t edi = static_cast<uint32_t>(src32.gregs[detail::greg_rdi]);
        const uint32_t esp = static_cast<uint32_t>(src32.gregs[detail::greg_rsp]);
        const uint32_t eflags = reconstruct_compacted_eflags(src32);

        uint32_t return_eip = 0;
        if (!this->emulator_.try_read_memory(esp, &return_eip, sizeof(return_eip)))
        {
            return false;
        }

        const auto& state64 = this->thread_->CurrentFrame->State;
        uint64_t teb64 = 0;
        if (!this->emulator_.try_read_memory(state64.gs_cached + 0x30, &teb64, sizeof(teb64)) || teb64 == 0)
        {
            return false;
        }
        uint64_t cpu_area = 0;
        if (!this->emulator_.try_read_memory(teb64 + 0x1488, &cpu_area, sizeof(cpu_area)) || cpu_area == 0)
        {
            return false;
        }
        const uint64_t block = cpu_area + 0x80;

        const auto write32 = [&](uint64_t offset, uint32_t value) {
            this->emulator_.write_marshal_state(block + offset, &value, sizeof(value));
        };
        write32(0x20, edi);
        write32(0x24, esi);
        write32(0x28, ebx);
        write32(0x2c, edx);
        write32(0x30, ecx);
        write32(0x34, eax);
        write32(0x38, ebp);
        write32(0x3c, return_eip);
        write32(0x44, eflags);
        write32(0x48, esp + 4);
        for (int i = 0; i < 6; ++i)
        {
            this->emulator_.write_marshal_state(block + 0xf0 + static_cast<uint64_t>(i) * 0x10, &src32.xmm.avx.data[i][0], 16);
        }

        auto& dst64 = this->thread_->CurrentFrame->State;
        dst64.rip = generic_dispatch;
        dst64.gregs[detail::greg_rax] = eax;
        dst64.gregs[detail::greg_rcx] = ecx;
        dst64.gregs[detail::greg_rdx] = edx;
        dst64.gregs[detail::greg_rbx] = ebx;
        dst64.gregs[detail::greg_rbp] = ebp;
        dst64.gregs[detail::greg_rsi] = esi;
        dst64.gregs[detail::greg_rdi] = edi;
        dst64.gregs[11] = static_cast<uint64_t>(esp) + 8;
        dst64.gregs[13] = block;
        dst64.gregs[15] = jump_table;

        this->active_context_ = this->emulator_.context_.get();
        this->active_thread_ = this->thread_;

        return true;
    }

    bool fex_vcpu::enter_bitness_switch_from_far_jmp(const gate_crossing& gate)
    {
        return this->enter_wow64_64bit_from_wow64svc_thunk(gate);
    }

    bool fex_vcpu::perform_bitness_switch(const uint64_t target_rip, const uint64_t target_rsp, const uint16_t target_cs)
    {
        auto* const acting_thread = this->active_thread_.load();
        const auto& src = acting_thread->CurrentFrame->State;
        const bool target_is_64bit = (target_cs == wow64_user_code_selector_64bit);

        FEXCore::Context::Context* dst_context = nullptr;
        FEXCore::Core::InternalThreadState* dst_thread = nullptr;
        if (target_is_64bit)
        {
            dst_context = this->emulator_.context_.get();
            dst_thread = this->thread_;
        }
        else
        {
            if (this->thread32_ == nullptr)
            {
                return false;
            }
            dst_context = this->emulator_.context32_.get();
            dst_thread = this->thread32_;
        }

        auto& dst = dst_thread->CurrentFrame->State;

        const auto saved_es_idx = dst.es_idx;
        const auto saved_cs_idx = dst.cs_idx;
        const auto saved_ss_idx = dst.ss_idx;
        const auto saved_ds_idx = dst.ds_idx;
        const auto saved_fs_idx = dst.fs_idx;
        const auto saved_gs_idx = dst.gs_idx;
        const auto saved_es_cached = dst.es_cached;
        const auto saved_cs_cached = dst.cs_cached;
        const auto saved_ss_cached = dst.ss_cached;
        const auto saved_ds_cached = dst.ds_cached;
        const auto saved_fs_cached = dst.fs_cached;
        const auto saved_gs_cached = dst.gs_cached;

        const auto saved_r12 = dst.gregs[12];
        const auto saved_r13 = dst.gregs[13];
        const auto saved_r14 = dst.gregs[14];
        const auto saved_r15 = dst.gregs[15];

        const auto saved_rax = dst.gregs[detail::greg_rax];
        const auto saved_rbx = dst.gregs[detail::greg_rbx];
        const auto saved_rcx = dst.gregs[detail::greg_rcx];
        const auto saved_rdx = dst.gregs[detail::greg_rdx];

        marshal_architectural_state(src, dst);

        dst.es_idx = saved_es_idx;
        dst.cs_idx = saved_cs_idx;
        dst.ss_idx = saved_ss_idx;
        dst.ds_idx = saved_ds_idx;
        dst.fs_idx = saved_fs_idx;
        dst.gs_idx = saved_gs_idx;
        dst.es_cached = saved_es_cached;
        dst.cs_cached = saved_cs_cached;
        dst.ss_cached = saved_ss_cached;
        dst.ds_cached = saved_ds_cached;
        dst.fs_cached = saved_fs_cached;
        dst.gs_cached = saved_gs_cached;
        dst.gregs[12] = saved_r12;
        dst.gregs[13] = saved_r13;
        dst.gregs[14] = saved_r14;
        dst.gregs[15] = saved_r15;
        dst.gregs[detail::greg_rax] = saved_rax;
        dst.gregs[detail::greg_rbx] = saved_rbx;
        dst.gregs[detail::greg_rcx] = saved_rcx;
        dst.gregs[detail::greg_rdx] = saved_rdx;

        dst.rip = target_rip;
        dst.gregs[detail::greg_rsp] = target_rsp;

        this->active_context_ = dst_context;
        this->active_thread_ = dst_thread;
        return true;
    }

    bool fex_vcpu::perform_gate_crossing(const gate_crossing& gate)
    {
        if (gate.kind == gate_crossing_kind::wow64_run_simulated_code)
        {
            return this->enter_wow64_32bit_from_run_simulated_code(gate);
        }

        if (gate.kind == gate_crossing_kind::wow64cpu_dispatch)
        {
            return this->enter_wow64_64bit_from_wow64svc_thunk(gate);
        }

        if (gate.kind == gate_crossing_kind::far_jmp_bitness_switch)
        {
            return this->enter_bitness_switch_from_far_jmp(gate);
        }

        auto* const active = this->active_thread_.load();
        const auto& src = active->CurrentFrame->State;
        return this->perform_bitness_switch(src.gregs[detail::greg_rax], src.gregs[detail::greg_rbx],
                                            static_cast<uint16_t>(src.gregs[detail::greg_rcx]));
    }

    bool fex_vcpu::handle_fault_signal(int sig, siginfo_t* info, void* raw_ucontext)
    {
        auto* const active_thread = this->active_thread_.load();
        if (active_thread == nullptr)
        {
            return false;
        }

        auto* uctx = static_cast<ucontext_t*>(raw_ucontext);

        if (sig == SIGSEGV || sig == SIGBUS)
        {
            const auto fault_addr = reinterpret_cast<uint64_t>(info->si_addr);

            const auto interrupt_page_addr = reinterpret_cast<uint64_t>(active_thread->InterruptFaultPage);
            if (fault_addr >= interrupt_page_addr && fault_addr < interrupt_page_addr + sizeof(active_thread->InterruptFaultPage))
            {
                const auto fault_pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
                const bool is_dispatch_code =
                    this->active_context_ && this->active_context_->IsAddressInCodeBuffer(active_thread, fault_pc);

                const bool is_strb_epilogue_write = (*reinterpret_cast<const uint32_t*>(fault_pc) & 0xFFC00000u) == 0x39000000u;

                if (is_dispatch_code && !is_strb_epilogue_write)
                {
                    active_thread->CurrentFrame->State.rip = this->active_context_->RestoreRIPFromHostPC(active_thread, fault_pc);
                    this->interrupt_page_unwind_ = true;
                    const auto& stop_cfg = this->emulator_.signal_delegator_->GetConfig();
                    arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss,
                                                   reinterpret_cast<void*>(stop_cfg.ThreadStopHandlerAddressSpillSRA));
                    return true;
                }

                arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(fault_pc + 4));
                return true;
            }

            {
                const auto host_pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
                if (fault_addr == host_pc && this->host_pc_in_any_dispatcher(host_pc))
                {
                    ::pthread_jit_write_protect_np(1);
                    return true;
                }
            }

            if (this->handle_callret_stack_fault(uctx, fault_addr))
            {
                return true;
            }

            const auto pc_for_mmio_check = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
            if (fault_addr != pc_for_mmio_check)
            {
                const auto guest_fault_addr = this->emulator_.unrebase_fault_addr(fault_addr);
                const std::shared_lock lock(this->emulator_.tables_mutex_);
                for (const auto& region : this->emulator_.mmio_regions_)
                {
                    if (guest_fault_addr >= region.address && guest_fault_addr < region.address + region.size)
                    {
                        return this->handle_mmio_fault(uctx, region, guest_fault_addr);
                    }
                }
            }

            if (sig == SIGBUS && info->si_code == BUS_ADRALN && this->active_context_ &&
                this->active_context_->IsAddressInCodeBuffer(active_thread, fault_addr))
            {
                auto& retry_count = this->jit_write_protect_retry_count_for(fault_addr);
                constexpr int max_write_protect_retries = 4;
                if (retry_count < max_write_protect_retries)
                {
                    ++retry_count;
                    ::pthread_jit_write_protect_np(0);
                    return true;
                }
            }

            if (sig == SIGBUS && info->si_code == BUS_ADRALN && this->handle_general_memory_violation(uctx, fault_addr))
            {
                return true;
            }
        }

        if ((sig == SIGSEGV || sig == SIGBUS) && (info->si_code == SEGV_ACCERR || info->si_code == SEGV_MAPERR))
        {
            const auto guard_page = active_thread->JITGuardPage;
            const auto fault_addr = reinterpret_cast<uintptr_t>(info->si_addr);
            if (guard_page != 0 && fault_addr >= guard_page && fault_addr < guard_page + FEXCore::Utils::FEX_HOST_PAGE_SIZE)
            {
                auto* gprs = reinterpret_cast<uint64_t*>(&uctx->uc_mcontext->__ss);
                auto* fprs = reinterpret_cast<__uint128_t*>(&uctx->uc_mcontext->__ns.__v[0]);
                auto* pc_ptr = reinterpret_cast<uint64_t*>(&uctx->uc_mcontext->__ss.__pc);
                FEXCore::UncheckedLongJump::ManuallyLoadJumpBuf(active_thread->RestartJump, active_thread->JITGuardOverflowArgument, gprs,
                                                                fprs, pc_ptr);
                return true;
            }

            if (this->active_context_ && this->active_context_->IsAddressInCodeBuffer(active_thread, fault_addr))
            {
                const auto fault_addr_u64 = reinterpret_cast<uint64_t>(info->si_addr);
                auto& retry_count = this->jit_write_protect_retry_count_for(fault_addr_u64);
                constexpr int max_write_protect_retries = 4;
                if (retry_count < max_write_protect_retries)
                {
                    ++retry_count;
                    const uint64_t faulting_pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
                    const bool is_instruction_fetch = (faulting_pc == fault_addr_u64);
                    ::pthread_jit_write_protect_np(is_instruction_fetch ? 1 : 0);
                    return true;
                }
            }
        }

        const uint64_t pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);

        if (!this->host_pc_in_any_dispatcher(pc))
        {
            if ((sig == SIGSEGV || sig == SIGBUS) && this->active_context_ &&
                this->active_context_->IsAddressInCodeBuffer(active_thread, pc) &&
                this->handle_general_memory_violation(uctx, reinterpret_cast<uint64_t>(info->si_addr)))
            {
                return true;
            }

            return false;
        }

        auto* frame = active_thread->CurrentFrame;
        if (!frame->SynchronousFaultData.FaultToTopAndGeneratedException)
        {
            return false;
        }

        auto vector = static_cast<int>(frame->SynchronousFaultData.TrapNo);

        constexpr int gp_fault_vector = 13;
        constexpr uint32_t idt_reference_bit = 0x2;
        if (vector == gp_fault_vector && (frame->SynchronousFaultData.err_code & idt_reference_bit) != 0)
        {
            vector = static_cast<int>(frame->SynchronousFaultData.err_code >> 3);
        }

        frame->SynchronousFaultData.FaultToTopAndGeneratedException = false;

        pending_fault_dispatch dispatch{};
        if (vector == 14)
        {
            if (const auto gate = this->emulator_.find_gate_crossing(frame->State.rip))
            {
                auto* const source_signal_delegator = (this->active_context_ == this->emulator_.context32_.get())
                                                          ? this->emulator_.signal_delegator32_.get()
                                                          : this->emulator_.signal_delegator_.get();

                if (this->perform_gate_crossing(*gate))
                {
                    this->pending_fault_dispatch_.kind = pending_fault_kind::gate_crossing;
                    const auto& stop_cfg = source_signal_delegator->GetConfig();
                    arm_thread_state64_set_pc_fptr(uctx->uc_mcontext->__ss, reinterpret_cast<void*>(stop_cfg.ThreadStopHandlerAddress));
                    return true;
                }
            }

            const auto err_code = frame->SynchronousFaultData.err_code;
            const bool is_write = (err_code & 0x2) != 0;
            const bool is_instr_fetch = (err_code & 0x10) != 0;
            dispatch.kind = pending_fault_kind::memory_violation;
            dispatch.address = frame->State.rip;
            dispatch.size = 1;
            dispatch.operation = is_instr_fetch ? memory_operation::exec : is_write ? memory_operation::write : memory_operation::read;
            dispatch.type = (err_code & 0x1) ? memory_violation_type::protection : memory_violation_type::unmapped;
        }
        else
        {
            dispatch.kind = pending_fault_kind::interrupt;
            dispatch.vector = vector;
        }

        this->defer_hook_dispatch(uctx, dispatch, /*sra_already_spilled=*/true);
        return true;
    }
#endif

#ifdef __APPLE__
    namespace
    {
        void fault_signal_handler(int sig, siginfo_t* info, void* raw_ucontext)
        {
            auto* uctx = static_cast<ucontext_t*>(raw_ucontext);

            const bool handled = t_current_vcpu != nullptr && t_current_vcpu->handle_fault_signal(sig, info, raw_ucontext);
            if (handled)
            {
                return;
            }

            char buf[1024];
            const uint64_t pc = arm_thread_state64_get_pc(uctx->uc_mcontext->__ss);
            int len = snprintf(buf, sizeof(buf), "[FEX backend] unhandled signal %d si_code=%d at pc=0x%llx fault_addr=%p\n", sig,
                               info->si_code, static_cast<unsigned long long>(pc), info->si_addr);
            // Full host GPR dump: a wild host-level branch (e.g. a corrupt JIT link target) leaves its
            // source only in registers - lr identifies a blr's call site, x25 (REG_CALLRET_SP) the
            // call-ret shadow stack, x28 (STATE) the engine thread - none of which the pc/fault_addr
            // line alone can recover post-mortem.
            if (len > 0 && static_cast<size_t>(len) < sizeof(buf))
            {
                const auto& ss = uctx->uc_mcontext->__ss;
                for (int i = 0; i < 29 && static_cast<size_t>(len) < sizeof(buf); ++i)
                {
                    len += snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len), "x%d=0x%llx%s", i,
                                    static_cast<unsigned long long>(ss.__x[i]), (i % 6 == 5) ? "\n" : " ");
                }
                if (static_cast<size_t>(len) < sizeof(buf))
                {
                    len += snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len), "fp=0x%llx lr=0x%llx sp=0x%llx\n",
                                    static_cast<unsigned long long>(arm_thread_state64_get_fp(ss)),
                                    static_cast<unsigned long long>(arm_thread_state64_get_lr(ss)),
                                    static_cast<unsigned long long>(arm_thread_state64_get_sp(ss)));
                }
            }
            if (len > 0)
            {
                const auto write_len = static_cast<size_t>(len) < sizeof(buf) ? static_cast<size_t>(len) : sizeof(buf);
                ::write(STDERR_FILENO, buf, write_len);
            }

            struct sigaction default_action = {};
            default_action.sa_handler = SIG_DFL;
            ::sigaction(sig, &default_action, nullptr);
            ::raise(sig);
        }
    } // namespace
#endif

    // -----------------------------------------------------------------------------------------------
    // fex_syscall_handler method bodies (fex_x86_64_emulator and fex_vcpu are now complete).
    // -----------------------------------------------------------------------------------------------

    uint64_t fex_syscall_handler::HandleSyscall(FEXCore::Core::CpuStateFrame* /*frame*/, FEXCore::HLE::SyscallArguments* /*args*/)
    {
        // Called from the guest-execution thread that issued this syscall, so t_current_vcpu (set for
        // the duration of fex_vcpu::start()'s ExecuteThread loop) is the correct acting vCPU - not
        // always vcpu 0, under real multi-vCPU concurrency.
        auto* const vcpu = t_current_vcpu;

        auto* hook = this->emulator_.syscall_hook_;
        if (hook != nullptr && hook->callback)
        {
            hook->callback(*vcpu, 0);
        }

        vcpu->cpu_state().rip += 2;

        if (vcpu->stop_requested_)
        {
            vcpu->request_thread_stop();
        }

        return vcpu->cpu_state().gregs[detail::greg_rax];
    }

    FEXCore::HLE::ExecutableRangeInfo fex_syscall_handler::QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* /*thread*/,
                                                                                     uint64_t address)
    {
        const std::shared_lock lock(this->emulator_.tables_mutex_);

        for (const auto& gate : this->emulator_.gate_crossings_)
        {
            if (address >= gate.address && address < gate.address + gate.size)
            {
                return {};
            }
        }

        auto& regions = this->emulator_.regions_;
        auto it = regions.upper_bound(address);
        if (it == regions.begin())
        {
            return {};
        }
        --it;
        const uint64_t region_end = it->first + it->second.size;
        if (address < it->first || address >= region_end)
        {
            return {};
        }

        const auto perms = it->second.permissions;
        if ((perms & memory_permission::exec) == memory_permission::none)
        {
            return {};
        }

        uint64_t base = it->first;
        uint64_t end = region_end;
        for (const auto& gate : this->emulator_.gate_crossings_)
        {
            const uint64_t gate_end = gate.address + gate.size;
            if (gate_end <= address)
            {
                base = (std::max)(base, gate_end);
            }
            else if (gate.address > address)
            {
                end = (std::min)(end, gate.address);
            }
        }

        return {
            .Base = base,
            .Size = end - base,
            .Writable = (perms & memory_permission::write) != memory_permission::none,
        };
    }

    std::optional<FEXCore::ExecutableFileSectionInfo> fex_syscall_handler::LookupExecutableFileSection(
        FEXCore::Core::InternalThreadState* /*thread*/, uint64_t /*guest_addr*/)
    {
        return std::nullopt;
    }

    std::unique_ptr<x86_64_emulator> create_x86_64_emulator(size_t vcpu_count)
    {
        return std::make_unique<fex_x86_64_emulator>(vcpu_count);
    }
} // namespace sogen::fex
