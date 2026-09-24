// _GNU_SOURCE exposes the Linux host VM and signal definitions used by this backend.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define FEX_EMULATOR_IMPL
#include "fex_x86_64_emulator.hpp"
#include "fex_x86_64_common.hpp"
#include "fex_x86_64_marshal.hpp"

#include "address_utils.hpp"

// FEX (https://fex-emu.com) is an in-process x86-64 -> AArch64 binary translator. Unlike the
// Unicorn/Icicle/KVM backends it does not manage a sandboxed guest address space: the translated
// guest executes inside the host process with guest VA == host VA. So map_memory() is a real
// mmap(MAP_FIXED) at the guest address and read/write_memory() a direct host memcpy, and - as with
// KVM - there is no per-access or per-instruction instrumentation point, so memory/execution/block
// hooks are accepted for API compatibility but never fire. Guest `syscall` instructions come back
// through a FEXCore::HLE::SyscallHandler that invokes the registered syscall instruction hook.
//
// The functional targets are Darwin on Apple Silicon and Android on AArch64; the signal handlers and
// MMIO fault emulation below support both. Android currently requires a 4KB host page; the 16KB/4KB
// page reconciliation and MAP_JIT handling are Darwin-only. Other AArch64 Linux hosts have build
// coverage only.

#include <cstdlib>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>

#ifdef __ANDROID__
#if __ANDROID_API__ >= 33
#include <execinfo.h>
#endif
#include <asm/hwcap.h>
#include <linux/prctl.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#else
#include <execinfo.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/arm/thread_status.h>
#include <libkern/OSCacheControl.h>
#include <libproc.h>
#endif

#include <atomic>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <charconv>
#include <ranges>
#include <string_view>

#include <utils/object.hpp>
#include <utils/io.hpp>

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

namespace sogen::fex
{
    class fex_x86_64_emulator;

    namespace
    {
        constexpr size_t page_size = 0x1000;

#ifdef __ANDROID__
        constexpr uint64_t guest_address_space_end = 0x00007ffffffeffffULL + 1;

#ifndef MAP_FIXED_NOREPLACE
        constexpr int MAP_FIXED_NOREPLACE = 0x100000;
#endif

        std::vector<host_reserved_range> read_host_mappings_android(const uint64_t window_start, const uint64_t window_end)
        {
            std::vector<std::byte> data;
            if (!utils::io::read_file("/proc/self/maps", &data))
            {
                throw std::runtime_error("FEX backend failed to enumerate Android host mappings");
            }

            const std::string_view maps{reinterpret_cast<const char*>(data.data()), data.size()};

            std::vector<host_reserved_range> ranges;

            for (auto line : maps | std::views::split('\n'))
            {
                const auto dash = std::ranges::find(line, '-');
                if (dash == line.end())
                {
                    continue;
                }

                uint64_t mapping_start, mapping_end;

                const char* begin = std::to_address(line.begin());
                const char* separator = std::to_address(dash);
                const char* end = std::to_address(line.end());

                const auto [p1, ec1] = std::from_chars(begin, separator, mapping_start, 16);
                const auto [p2, ec2] = std::from_chars(separator + 1, end, mapping_end, 16);

                if (ec1 != std::errc{} || ec2 != std::errc{})
                {
                    continue;
                }

                const auto hit_start = std::max(mapping_start, window_start);
                const auto hit_end = std::min(mapping_end, window_end);

                if (hit_start < hit_end)
                {
                    ranges.push_back({
                        .address = hit_start,
                        .size = static_cast<size_t>(hit_end - hit_start),
                    });
                }
            }

            return ranges;
        }

        FEXCore::HostFeatures fetch_host_features_android()
        {
            FEXCore::HostFeatures features{};

            const unsigned long hwcap = ::getauxval(AT_HWCAP);
            const unsigned long hwcap2 = ::getauxval(AT_HWCAP2);

            const auto has = [hwcap](unsigned long flag) { return (hwcap & flag) != 0; };
            const auto has2 = [hwcap2](unsigned long flag) { return (hwcap2 & flag) != 0; };

            features.SupportsAES = has(HWCAP_AES);
            features.SupportsCRC = has(HWCAP_CRC32);
            features.SupportsAtomics = has(HWCAP_ATOMICS);
            features.SupportsRCPC = has(HWCAP_LRCPC);
            features.SupportsTSOImm9 = has(HWCAP_ILRCPC);
            features.SupportsSHA = has(HWCAP_SHA1) && has(HWCAP_SHA2);
            features.SupportsPMULL_128Bit = has(HWCAP_PMULL);
            features.SupportsFCMA = has(HWCAP_FCMA);
            features.SupportsFlagM = has(HWCAP_FLAGM);

            features.SupportsRAND = has2(HWCAP2_RNG);
            features.SupportsFlagM2 = has2(HWCAP2_FLAGM2);
            features.SupportsFRINTTS = has2(HWCAP2_FRINT);
            features.SupportsECV = has2(HWCAP2_ECV);
            features.SupportsAFP = has2(HWCAP2_AFP);
            features.SupportsRPRES = has2(HWCAP2_RPRES);
            features.SupportsWFXT = has2(HWCAP2_WFXT);
            features.SupportsSVEBitPerm = has2(HWCAP2_SVEBITPERM);

#ifdef HWCAP2_CSSC
            features.SupportsCSSC = has2(HWCAP2_CSSC);
#endif

#ifdef HWCAP2_MOPS
            features.SupportsMOPS = has2(HWCAP2_MOPS);
#endif

            features.SupportsSVE128 = has2(HWCAP2_SVE2);
            if (features.SupportsSVE128)
            {
                const int sve_vl = ::prctl(PR_SVE_GET_VL);
                if (sve_vl >= 0)
                {
                    features.SupportsSVE256 = (sve_vl & PR_SVE_VL_LEN_MASK) >= 32;
                }
            }

            // AVX can make FEX emit paired SIMD loads, which MMIO emulation doesn't support yet.
            features.SupportsAVX = false;
            features.SupportsAES256 = features.SupportsAES && features.SupportsAVX;

            const long dcache_line = ::sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
            const long icache_line = ::sysconf(_SC_LEVEL1_ICACHE_LINESIZE);
            features.DCacheLineSize = dcache_line > 0 ? static_cast<uint32_t>(dcache_line) : 64;
            features.ICacheLineSize = icache_line > 0 ? static_cast<uint32_t>(icache_line) : 64;
            features.SupportsCacheMaintenanceOps = true;

            uint64_t dczid = 0;
            __asm__ volatile("mrs %0, dczid_el0" : "=r"(dczid));
            features.SupportsCLZERO = (dczid & (1ULL << 4)) == 0 && (4ULL << (dczid & 0xF)) == 64;

            const long logical_cpus = ::sysconf(_SC_NPROCESSORS_ONLN);
            features.CPUMIDRs.assign(logical_cpus > 0 ? static_cast<size_t>(logical_cpus) : 1, 0u);

            features.SupportsCPUIndexInTPIDRRO = false;

            return features;
        }
#endif
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

        // A real wow64 process maps BOTH a 32-bit executable/ntdll32 (living in [0, 4GB), needing
        // the rebase) AND the real 64-bit ntdll/win32u/wow64*.dll support modules (living anywhere
        // from 4GB up, needing NO rebase at all) - module_manager::load_wow64_modules maps both
        // kinds while this backend's is_wow64_process_ is already true. Blanket-applying the rebase
        // whenever is_wow64_process_ is set - rather than per-address - would incorrectly shift the
        // 64-bit modules' own addresses too. Gate on the address itself: anything at or past the
        // true 32-bit address-space boundary (wow64_guest_address_space_size) is left alone,
        // regardless of how far below the rebase offset (the unrelated value actually added) it
        // happens to sit.

        // The 64-bit user code-segment selector (matches sogen::wow64::heaven_gate::kUserCodeSelector
        // in src/windows-emulator/wow64_heaven_gate.hpp - kept as a local constant to avoid pulling
        // the windows-emulator include tree into this backend). A gate crossing whose target CS is
        // this selector is entering 64-bit mode (context_); anything else (0x23, the 32-bit compat
        // selector) is entering 32-bit mode (context32_). See perform_gate_crossing.
        constexpr uint16_t wow64_user_code_selector_64bit = 0x33;
#ifdef __APPLE__
        // FEXCore's JITWriteScope (FEXCore/Utils/AllocatorHooks.h) toggles pthread_jit_write_protect_np
        // per call, not via a nesting counter, so bracketing our own call to Pointers.ExitFunctionLink
        // is not enough: its "not yet compiled" path calls CompileBlock, whose own nested JITWriteScope
        // re-enables write protection before control returns to our still-writing outer call, faulting
        // on the self-modifying store. Interposing pthread_jit_write_protect_np to make it reentrant
        // does not work either - every route to the real implementation from inside the interposer
        // resolves back to the replacement.
        //
        // So the fault is reacted to instead: handle_fault_signal's SEGV_ACCERR branch disables write
        // protection and retries when the faulting PC is FEXCore's own code, not MAP_JIT memory,
        // bounded per fault address so a different bug cannot spin. The bound lives in a fixed array
        // rather than an unordered_map since operator[] can rehash and is unsafe to call from a signal
        // handler that may interrupt an unrelated malloc()/free(). Eight slots suffice: guest execution
        // is cooperative, so at most one address is ever mid-retry.
        struct jit_write_protect_retry_slot
        {
            uint64_t address = 0;
            int count = 0;
            bool used = false;
            uint64_t last_fault_ns = 0;
        };

        constexpr size_t jit_write_protect_retry_slot_count = 8;
        jit_write_protect_retry_slot g_jit_write_protect_retry_slots[jit_write_protect_retry_slot_count];
        size_t g_jit_write_protect_retry_next_evict = 0;

        // A gap at least this long since an address last faulted means the site is being reused
        // healthily rather than spinning, so its retry budget is reset.
        constexpr uint64_t jit_write_protect_retry_reset_window_ns = 100'000'000; // 100ms

        uint64_t monotonic_now_ns()
        {
            struct timespec ts{};
            ::clock_gettime(CLOCK_MONOTONIC, &ts);
            return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
        }

        // Resets the counter when the address has not faulted within the window: otherwise a site that
        // legitimately resolves this race many times over a long run would exhaust its budget and turn a
        // benign race into a hard crash. Async-signal-safe: fixed-array scan, no allocation, and
        // clock_gettime(CLOCK_MONOTONIC) is vDSO-backed.
        int& jit_write_protect_retry_count_for(const uint64_t fault_addr)
        {
            const uint64_t now_ns = monotonic_now_ns();
            jit_write_protect_retry_slot* free_slot = nullptr;
            for (auto& slot : g_jit_write_protect_retry_slots)
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

            auto& slot = (free_slot != nullptr)
                             ? *free_slot
                             : g_jit_write_protect_retry_slots[g_jit_write_protect_retry_next_evict++ % jit_write_protect_retry_slot_count];
            slot.address = fault_addr;
            slot.count = 0;
            slot.used = true;
            slot.last_fault_ns = now_ns;
            return slot.count;
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
#endif

#if defined(__APPLE__) || defined(__ANDROID__)
        // Decodes only what an mmio_region fault needs: destination register, transfer size and
        // extension. The addressing mode is deliberately not decoded - the effective address is already
        // known, being the fault address itself under guest VA == host VA - so only the fields the
        // "Load/store register" class keeps at fixed bit positions across every sub-form are read
        // (size/opc at [31:30]/[23:22], Rt at [4:0]; AArch64 ISA C4.1.3). Stores are not decoded: this
        // backend's only MMIO consumer is read-only.
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

            // "Load register (unscaled immediate)" - LDUR plus FEAT_LRCPC2's LDAPUR family. Both
            // use a signed imm9 in bits[20:12], so the same top22+low-bits mask handles them while
            // ignoring the immediate, Rn and Rt.
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

                {0x19400000U, 1, false, false}, // LDAPURB
                {0x19800000U, 1, true, true},   // LDAPURSB, 64-bit dest
                {0x19C00000U, 1, true, false},  // LDAPURSB, 32-bit dest
                {0x59400000U, 2, false, false}, // LDAPURH
                {0x59800000U, 2, true, true},   // LDAPURSH, 64-bit dest
                {0x59C00000U, 2, true, false},  // LDAPURSH, 32-bit dest
                {0x99400000U, 4, false, false}, // LDAPUR Wt
                {0x99800000U, 4, true, true},   // LDAPURSW
                {0xD9400000U, 8, false, true},  // LDAPUR Xt
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

        // Store-release forms (STLR/STLRB/STLRH and STLUR/STLURB/STLURH), the counterpart to
        // decode_arm64_load's LDAR/LDAPR/LDAPUR handling. Used only by handle_fault_signal's
        // misaligned-atomic fallback; mmio_region's only
        // consumer here is read-only.
        //
        // Deliberately narrow: adding plain STR/STUR causes a reproducible hang in false-fault
        // emulation, isolated to this table (root cause not yet understood; decode_arm64_load's
        // equivalent plain-load coverage is safe).
        //
        // Known consequence: handle_general_memory_violation also uses this decoder to classify
        // protection faults as reads vs. writes. Android gets this from ESR, but Darwin therefore
        // reports plain STR faults as reads. A plain store to read-only memory can thus surface as
        // an unhandled host signal instead of a guest STATUS_ACCESS_VIOLATION (e.g. packers/DRM);
        // a plain store to unmapped memory reaches the guest, but ExceptionInformation[0] wrongly
        // identifies it as a read.
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

            // FEAT_LRCPC2 store-release with signed imm9. FEX emits these when SupportsTSOImm9 is
            // enabled; unlike ordinary STUR, they retain release ordering and require natural alignment.
            static constexpr encoding unscaled_stores[] = {
                {0x19000000U, 1}, // STLURB
                {0x59000000U, 2}, // STLURH
                {0x99000000U, 4}, // STLUR Wt
                {0xD9000000U, 8}, // STLUR Xt
            };

            const uint32_t rt = insn & 0x1FU;
            const uint32_t fixed = insn & 0xFFFFFC00U;
            const uint32_t unscaled_fixed = insn & 0xFFE00C00U;

            for (const auto& enc : stores)
            {
                if (fixed == enc.value)
                {
                    return decoded_arm64_store{enc.size, rt};
                }
            }

            for (const auto& enc : unscaled_stores)
            {
                if (unscaled_fixed == enc.value)
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
        // Apple Silicon's kernel refuses simultaneous write+exec on any non-MAP_JIT mapping (mprotect
        // fails with EACCES), unlike Linux where guest W^X is advisory. Real PE loaders hit this
        // routinely: map .text RWX to apply relocations, then narrow to RX before executing. Favoring
        // write handles that sequence; it would be wrong for a page genuinely written and executed in
        // the same window without an intervening apply_memory_protection call, which PE loading is not.
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

        // Replaces FEXCore's own FetchHostFeatures(), which is Linux-only: it reads MIDR_EL1, neither
        // EL0-readable nor trap-emulated on Darwin. Anything not confirmed present in
        // hw.optional.arm.*, or without a clear ARM-feature mapping, is left false - that costs codegen
        // quality, never correctness.
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
            // AVX can make FEX emit paired SIMD loads, which MMIO emulation doesn't support yet.
            features.SupportsAVX = false;
            features.SupportsAES256 = features.SupportsAES && features.SupportsAVX;
            features.SupportsSVEBitPerm = false;

            // TPIDRRO_EL0 is not confirmed to carry a CPU index on Darwin, and DEF_OP(ProcessorID)
            // treats the non-TPIDRRO fallback as unsupported (matching the Windows/wine precedent), so
            // leaving this false makes a guest RDTSCP/RDPID hard-error rather than read garbage.
            features.SupportsCPUIndexInTPIDRRO = false;

            // FEXCore's CPUID brand-string leaves (0x80000002-4) index PerCPUData, sized from CPUMIDRs;
            // an empty CPUMIDRs null-derefs the leaf's ProductName. Linux populates it from per-core
            // MIDR_EL1, unavailable from EL0 on Darwin, so the host logical-CPU count is reported with a
            // placeholder MIDR of 0: M-series parts are absent from FEXCore's MIDR table anyway, so it
            // resolves to FEXCore's own "Unknown ARM CPU" fallback while keeping the guest-visible core
            // count realistic.
            uint32_t logical_cpus = 1;
            size_t logical_cpus_len = sizeof(logical_cpus);
            if (::sysctlbyname("hw.logicalcpu", &logical_cpus, &logical_cpus_len, nullptr, 0) != 0 || logical_cpus == 0)
            {
                logical_cpus = 1;
            }
            features.CPUMIDRs.assign(logical_cpus, 0u);

            return features;
        }
#endif

#if defined(__APPLE__) || defined(__ANDROID__)
        // sogen runs one FEX-backed guest thread per process, so a single active-instance pointer is
        // enough for the signal handler below to reach the emulator's hook tables and thread state.
        // Signal handlers cannot be non-static member functions, hence the indirection.
        fex_x86_64_emulator* g_active_emulator = nullptr;

        void fault_signal_handler(int sig, siginfo_t* info, void* raw_ucontext);

        // FEXCore's Break stubs use SIGILL for HLT/UDF and SIGTRAP for BRK, including x86 INT3, so both
        // must reach the same host-fault dispatcher as SIGSEGV/SIGBUS.
        constexpr std::array<int, 4> fault_signals = {SIGSEGV, SIGBUS, SIGILL, SIGTRAP};

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

            // A dedicated alternate stack, so a second signal arriving while this handler is already
            // running does not nest on the faulting thread's potentially near-exhausted stack.
            static std::array<std::byte, 64 * 1024> alt_stack{};
            stack_t stack{};
            stack.ss_sp = alt_stack.data();
            stack.ss_size = alt_stack.size();
            ::sigaltstack(&stack, nullptr);

            struct sigaction action{};
            action.sa_sigaction = fault_signal_handler;
            action.sa_flags = SA_SIGINFO | SA_ONSTACK;
            sigemptyset(&action.sa_mask);

            // FEXCore's IR "Break" op models x86 HLT/UD2/INT3/INT1/INTO uniformly via distinct native
            // traps chosen per Dispatcher.cpp's GuestSignal_SIG* stubs: HLT/UDF raise SIGILL, BRK raises
            // SIGTRAP. INT3 (DebugBreak()) goes through the SIGTRAP stub, so without a handler here that
            // BRK is an unhandled hardware trap terminating the process, instead of reaching the vector
            // dispatch below, which handles vector 3 correctly.
            for (const int signal : fault_signals)
            {
                ::sigaction(signal, &action, nullptr);
            }
        }
#endif

#ifdef __APPLE__
        // Arm64JITCore::ExitFunctionLink (JIT.cpp) patches an already-compiled call site once its target
        // block is known, writing straight into a MAP_JIT code buffer without a JITWriteScope, because
        // it is written for Linux, which has no per-thread W^X state. Pointers.ExitFunctionLink is a
        // plain function-pointer slot JIT code calls through (JIT.cpp's InitThreadPointers), so it can
        // be wrapped here instead of patching deps/FEX.
        uint64_t g_original_exit_function_link = 0;

        uint64_t exit_function_link_jit_write_wrapper(FEXCore::Core::CpuStateFrame* frame, void* record)
        {
            using exit_function_link_fn = uint64_t (*)(FEXCore::Core::CpuStateFrame*, void*);
            const auto real = reinterpret_cast<exit_function_link_fn>(g_original_exit_function_link);

            // pthread_jit_write_protect_np is per-thread and exclusive with execute permission on this
            // thread's MAP_JIT pages: leaving it disabled past this call would fault the next guest
            // instruction fetch on this thread, not just widen an otherwise-harmless window.
            ::pthread_jit_write_protect_np(0);
            const uint64_t result = real(frame, record);
            ::pthread_jit_write_protect_np(1);
            return result;
        }

        // Under guest VA == host VA, FEXCore's own internal buffers (obtained via a raw ::mmap(NULL,
        // ...), uncoordinated with sogen's bookkeeping) can land inside the guest's own address space,
        // so an ordinary guest write into a buffer placed there corrupts FEXCore's state (the
        // long-standing __tree_balance_after_insert / AddBlockLink corruption).
        //
        // So one large host arena is reserved up front and registered as a reserved_host_range for a
        // two-way exclusion, and every FEXCore::Allocator::mmap(nullptr, ...) is satisfied from inside
        // it. Non-executable requests use MAP_FIXED; executable (MAP_JIT) ones cannot - Apple rejects
        // MAP_JIT|MAP_FIXED with EINVAL - so the sub-region is unmapped first and MAP_JIT gets the
        // hole's address as a non-fixed hint, reliable since nothing else competes for space inside the
        // arena. The result is verified to land inside the arena and fails loudly rather than silently
        // falling back to an unconstrained mapping that would reintroduce the hazard.
        class fex_internal_arena
        {
          public:
            // Pure VA reservation (PROT_NONE) until sub-regions are committed, sized to exceed any
            // realistic FEXCore-internal need for a full application workload.
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

          private:
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

            // Returns 0 on exhaustion. Caller holds lock_.
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

                if (flags & MAP_JIT)
                {
                    // FEXCore's CodeBuffer sizes its request to include a trailing guard page
                    // (CPUBackend.h's UsableSize(): AllocatedSize - FEX_HOST_PAGE_SIZE) and
                    // mprotect(PROT_NONE)s that page itself. That always fails on Apple Silicon with
                    // EACCES - MAP_JIT protection is fixed at mmap() time and cannot be adjusted
                    // afterwards - which is the "Failed to mprotect last page of code buffer" diagnostic
                    // CPUBackend.cpp logs. A real guard is provided instead: only the leading portion
                    // becomes the executable MAP_JIT mapping and the trailing page stays part of the
                    // arena's permanent PROT_NONE reservation, faulting on any access exactly where
                    // UsableSize() expects the buffer to end. Skipped at or below one host page so a
                    // small allocation (the Dispatcher's guardless buffer) is not shrunk into uselessness.
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
                return ::mmap(reinterpret_cast<void*>(slot), rounded, prot, flags | MAP_FIXED, fd, offset);
            }

            int release(void* addr, size_t length)
            {
                if (!this->owns(addr))
                {
                    return ::munmap(addr, length);
                }

                const size_t rounded = host_page_align_up_apple(length);

                std::lock_guard<std::mutex> guard(this->lock_);
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

#ifdef __ANDROID__
        template <typename Context>
        Context* find_aarch64_context(ucontext_t* context, uint32_t magic)
        {
            auto* current = reinterpret_cast<_aarch64_ctx*>(context->uc_mcontext.__reserved);
            const auto* end = context->uc_mcontext.__reserved + sizeof(context->uc_mcontext.__reserved);
            while (reinterpret_cast<const uint8_t*>(current) + sizeof(*current) <= end && current->size >= sizeof(*current))
            {
                if (current->magic == magic && current->size >= sizeof(Context))
                {
                    return reinterpret_cast<Context*>(current);
                }
                current = reinterpret_cast<_aarch64_ctx*>(reinterpret_cast<uint8_t*>(current) + current->size);
            }
            return nullptr;
        }
#endif

    }

    class fex_x86_64_emulator;

    // Bridges FEX's guest `syscall` exits to the registered instruction hook. Method bodies are out of
    // line, after fex_x86_64_emulator is complete, since they touch its internals.
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

    class fex_x86_64_emulator final : public x86_64_emulator
    {
        struct callret_buffer_record
        {
            void* allocation_base = nullptr;
            size_t allocation_size = 0;
            uint64_t code_buffer_generation = 0;
        };

      public:
        fex_x86_64_emulator()
        {
#ifdef __ANDROID__
            if (static_cast<size_t>(::getpagesize()) != page_size)
            {
                // The FEX backend is currently configured for 4KB host pages on Android.
                // This is not an inherent requirement and may be relaxed with appropriate support.
                throw std::runtime_error("FEX backend requires 4KB host pages on Android");
            }
#endif
            this->initialize_context();
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

            if (this->thread_ != nullptr && this->context_)
            {
                this->context_->DestroyThread(this->thread_);
                this->thread_ = nullptr;
            }

            if (this->thread32_ != nullptr && this->context32_)
            {
                this->context32_->DestroyThread(this->thread32_);
                this->thread32_ = nullptr;
            }

#if defined(__APPLE__) || defined(__ANDROID__)
            // Release everything we claimed in the shared host == guest address space
            // (claimed_host_ranges_ stays keyed by the guest address; modulo wow64_guest_rebase in
            // 32-bit mode - see rebase_for's doc comment).
            for (const auto& [address, size] : this->claimed_host_ranges_)
            {
                const auto rebase = rebase_for(this->is_wow64_process_, address);
#ifdef __APPLE__
                if (rebase != 0 && this->wow64_host_window_reserved_)
                {
                    // Never released - see the wow64 window comment below, right after this loop.
                    continue;
                }
                ::mach_vm_deallocate(mach_task_self(), address + rebase, size);
#else
                ::munmap(reinterpret_cast<void*>(address + rebase), size);
#endif
            }

#ifdef __APPLE__
            // Deliberately never released, unlike every other claim above - confirmed via direct
            // testing that releasing it CAN itself trigger the same kernel guard reserve_wow64_host_
            // window()'s doc comment describes (EXC_GUARD/DEALLOC_GAP - the "DEALLOC_GAP" half is
            // exactly this: the guard's own name is about deallocation, not just reservation). It
            // reproduces only after this window has actually been used for a while: a fresh reserve-
            // then-immediately-release of the same range is fine, but by process exit this window has
            // typically been carved into many separate mappings by claim_host_range()'s VM_FLAGS_
            // OVERWRITE calls (every module section, every guest VirtualAlloc/VirtualFree in 32-bit
            // mode) - releasing the original whole-window span in one call, across all of that internal
            // fragmentation, is what the kernel's deallocation guard is confirmed to catch. There is no
            // partial-release scheme that avoids this without tracking every fragment individually
            // (which claimed_host_ranges_ already does above for everything outside this window, at
            // the cost of one guard-worthy operation per fragment instead of one for the whole window -
            // not obviously safer, and not worth the complexity here). This is exactly what
            // fex_internal_arena::instance() already does for its own 4GB reservation (see its "Arena
            // VA is never returned to the OS" comment) - both are one-time, address-space-only
            // (PROT_NONE, no physical memory) reservations that live for exactly this process's
            // lifetime anyway, so leaking the VA costs nothing a process exit doesn't already reclaim.
#endif

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
            for (const auto& [_, buffer] : this->callret_buffers_)
            {
                FEXCore::Allocator::munmap(buffer.allocation_base, buffer.allocation_size);
            }
        }

        // cpu_interface

        bool read_descriptor_table(int reg, descriptor_table_register& table) override
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

        void start(size_t count) override
        {
            this->refresh_mmio_backings();

            if (count != 0)
            {
                // FEX has CompileRIPCount() for bounded execution, but wiring exact instruction counts
                // through the JIT exit path is non-trivial; match the KVM backend and refuse for now.
                throw std::runtime_error("FEX backend does not support exact instruction counts yet");
            }

            if (this->active_thread() == nullptr)
            {
                this->create_thread();
            }

            this->stop_requested_ = false;
            // Re-arm InterruptFaultPage for this quantum - see request_thread_stop's doc comment; a
            // prior stop() may have left it protected to force the last quantum's ExecuteThread to
            // return, and it must be writable again before the JIT's per-block-entry store runs.
            ::mprotect(this->active_thread()->InterruptFaultPage, sizeof(this->active_thread()->InterruptFaultPage),
                       PROT_READ | PROT_WRITE);

            // ExecuteThread runs the translated guest until the thread is asked to stop (which the
            // syscall bridge does when a hook calls stop()), or the guest faults/exits.
#if defined(__APPLE__) || defined(__ANDROID__)
            // Here it can also return early because handle_fault_signal deferred a hook dispatch (see
            // pending_fault_dispatch_) rather than genuinely stopping - dispatch it in normal call
            // context, where that is safe, then resume by calling ExecuteThread again; it always
            // restarts from CurrentFrame->State.rip, which the hook is free to have redirected.
            // Bounds the raced-unwind retry below: a real gate-crossing round-trip only ever needs a
            // handful of re-arms to settle, so this is generous headroom, not a tight budget - see the
            // retry's own comment for why an unbounded loop is unsafe here.
            constexpr uint32_t max_consecutive_raced_unwinds = 64;
            uint32_t consecutive_raced_unwinds = 0;
            for (;;)
            {
                this->active_context()->ExecuteThread(this->active_thread());

                const bool hook_dispatched = this->dispatch_pending_hook_if_any();
                const bool interrupt_page_unwind = this->interrupt_page_unwind_.exchange(false);

                // An InterruptFaultPage unwind with no stop pending is the quantum timer racing this
                // quantum's own entry: stop() sets stop_requested_ then protects the page, but a
                // concurrently-entered start() has already cleared the flag and the timer's mprotect
                // only lands after the re-arm above, so the first block-entry check faults with nothing
                // requested. Treating that as a real stop makes windows_emulator::vcpu_worker read it as
                // a fatal wind-down, so re-arm and resume instead; the timer's pending switch_thread
                // request is honored at the next genuine stop.
                if (this->stop_requested_ || (!hook_dispatched && !interrupt_page_unwind))
                {
                    break;
                }

                // A deferred hook or a raced InterruptFaultPage unwind is resuming (no stop pending). If
                // it was a WoW64 gate crossing, active_thread_ was just swapped to the OTHER FEXCore
                // engine mid-quantum. Each engine owns a distinct InterruptFaultPage (the cooperative-stop
                // mechanism - see request_thread_stop), but this quantum's re-arm above only touched
                // the engine active at entry. The newly-active engine's page may still be PROT_NONE
                // from a PRIOR quantum's stop (e.g. another logical thread yielded while running this
                // same shared 32-bit engine), which would make its very first block-entry interrupt
                // check fault and unwind ExecuteThread as a spurious "stop" - the second-32-bit-thread
                // startup failure at LdrInitializeThunk. Re-arm the now-active engine's page here (in
                // normal call context, and only on the continue path where no stop is pending) so it
                // resumes cleanly. This also covers a same-engine InterruptFaultPage-unwind resume,
                // since active_thread_ is unchanged there and re-arming an already-writable page is a
                // no-op.
                ::mprotect(this->active_thread()->InterruptFaultPage, sizeof(this->active_thread()->InterruptFaultPage),
                           PROT_READ | PROT_WRITE);

                // A rapid gate-crossing round-trip (e.g. a WoW64 syscall thunk bouncing straight back)
                // can flip active_thread_ again before this quantum ever gets a real instruction
                // executed on the engine just re-armed above: each side's own first block-entry check
                // then faults on the *other* side's still-PROT_NONE page in turn, live-verified as an
                // unbounded alternation between both engines' InterruptFaultPage that never resolves on
                // its own. Re-arming only the currently-active engine can never break that cycle, since
                // by the time this line runs, active_thread() may already have flipped past the engine
                // that actually faulted. Re-arm the inactive engine's page too, matching the "already
                // writable is a no-op" tolerance the comment above already relies on for the active
                // side, and cap the number of consecutive unresolved unwinds so a case this doesn't
                // fully cover fails as a real stop instead of hanging forever.
                if (this->is_wow64_process_ && this->thread32_ != nullptr)
                {
                    auto* const inactive = this->active_thread() == this->thread32_ ? this->thread_ : this->thread32_;
                    ::mprotect(inactive->InterruptFaultPage, sizeof(inactive->InterruptFaultPage), PROT_READ | PROT_WRITE);
                }

                // Only the interrupt-page-unwind case is the unresolved-livelock risk this guards
                // against; a real deferred-hook dispatch is unrelated forward progress and resets it.
                consecutive_raced_unwinds = interrupt_page_unwind ? consecutive_raced_unwinds + 1 : 0;
                if (consecutive_raced_unwinds > max_consecutive_raced_unwinds)
                {
                    break;
                }
            }
#else
            this->active_context()->ExecuteThread(this->active_thread());
#endif
        }

        void stop() override
        {
            this->stop_requested_ = true;
            this->request_thread_stop();
        }

        size_t read_raw_register(int reg, void* value, size_t size) override
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
                const FEXCore::Core::CPUState& gpr_state =
                    (this->is_wow64_process_ && this->active_thread() == this->thread32_ && this->thread_ != nullptr &&
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
                // Low 128 bits of the (possibly AVX) vector register.
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
                // Unknown/unsupported register: report zeroed value rather than throwing, matching the
                // lenient behavior of the other backends for rarely-used registers.
                std::memset(value, 0, size);
                return size;
            }
        }

        size_t write_raw_register(int reg, const void* value, size_t size) override
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
                    slot = v; // 32-bit writes clear the high 32 bits
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

        // Extended WoW64 register-snapshot layout (see save_registers/restore_registers). A wow64
        // logical thread carries state in BOTH FEXCore engines simultaneously - the active one plus a
        // "parked" excursion frame in the other (a 32-bit thread mid-32-bit-code leaves its last 64-bit
        // RunSimulatedCode dispatch frame frozen in thread_; a thread mid-64-bit-syscall leaves its
        // last 32-bit state frozen in thread32_). Both engines are single, shared instances multiplexed
        // across every logical thread, so a snapshot of only the active engine loses the parked frame,
        // which the next logical thread to run that engine then overwrites.
        static constexpr size_t kWow64SnapshotHeader = 8; // uint64 active-is-32 flag, kept 8 for alignment

        static constexpr size_t wow64_snapshot_size()
        {
            return kWow64SnapshotHeader + 2 * sizeof(FEXCore::Core::CPUState);
        }

        std::vector<std::byte> save_registers() const override
        {
            // For a wow64 process, once the 32-bit engine exists a logical thread's full state spans
            // BOTH engines (active + parked). Snapshot both, tagged with which one is active, so a
            // thread switch preserves the parked excursion frame instead of leaking it to whichever
            // logical thread next runs the shared engine.
            if (this->is_wow64_process_ && this->thread32_ != nullptr && this->thread_ != nullptr)
            {
                std::vector<std::byte> data(wow64_snapshot_size());
                const uint64_t active_is_32 = (this->active_context() == this->context32_.get()) ? 1 : 0;
                std::memcpy(data.data(), &active_is_32, sizeof(active_is_32));
                std::memcpy(data.data() + kWow64SnapshotHeader, &this->thread_->CurrentFrame->State, sizeof(FEXCore::Core::CPUState));
                std::memcpy(data.data() + kWow64SnapshotHeader + sizeof(FEXCore::Core::CPUState), &this->thread32_->CurrentFrame->State,
                            sizeof(FEXCore::Core::CPUState));

                // Both engines are single, shared instances multiplexed across every logical thread (see
                // this function's own doc comment above): this snapshot's RIP in each engine is about to
                // become a parked frame that outlives the buffer swap FEXCore's own CurrentCodeBuffer
                // bookkeeping does for whichever *other* logical thread runs next on that engine.
                // RetainCodeBufferAt takes a *host* CodeBuffer address, not a guest RIP - resolve each
                // engine's current guest RIP through FindHostAddressForGuestRIP first. It keeps the
                // buffer each RIP currently lives in alive until the matching restore_state_into (see
                // its own comment) releases it on resume. Refcounted and a no-op if the address isn't
                // in any known buffer, so this is safe even before the first real JIT compile.
                this->context_->RetainCodeBufferAt(
                    this->context_->FindHostAddressForGuestRIP(this->thread_, this->thread_->CurrentFrame->State.rip));
                this->context32_->RetainCodeBufferAt(
                    this->context32_->FindHostAddressForGuestRIP(this->thread32_, this->thread32_->CurrentFrame->State.rip));
                return data;
            }

            // The whole architectural state lives in a single CPUState struct; snapshot it verbatim.
            const auto& state = this->cpu_state();
            std::vector<std::byte> data(sizeof(FEXCore::Core::CPUState));
            std::memcpy(data.data(), &state, sizeof(state));

            // See the wow64 branch above for why this retain exists. No real thread yet means state is
            // staged_state_, not a live engine's frame - nothing to retain.
            if (this->active_thread() != nullptr)
            {
                this->active_context()->RetainCodeBufferAt(
                    this->active_context()->FindHostAddressForGuestRIP(this->active_thread(), state.rip));
            }
            return data;
        }

        // Preserves the fields that are per-FEXCore-thread-global rather than per-logical-guest-thread:
        // FEXCore rewrites L1Pointer/L1Mask itself whenever the JIT lookup cache reallocates, so the
        // live values are always the correct ones and a stale snapshot must not clobber them.
        // callret_sp/_pad1 are handled by ensure_callret_buffer.
        void restore_state_into(FEXCore::Core::InternalThreadState* thread, const std::byte* src)
        {
            auto& state = thread->CurrentFrame->State;
            const auto l1_pointer = state.L1Pointer;
            const auto l1_mask = state.L1Mask;
            std::memcpy(&state, src, sizeof(FEXCore::Core::CPUState));
            state.L1Pointer = l1_pointer;
            state.L1Mask = l1_mask;

            // Mirrors save_registers' RetainCodeBufferAt on this same RIP when this logical thread was
            // last parked (see that function's comment) - this engine is live again now, so the buffer
            // is naturally protected by FEXCore's own CurrentCodeBuffer bookkeeping going forward, until
            // it's parked again (which re-retains whatever RIP it has at that later point). Refcounted
            // and a no-op if there's no outstanding retain on this address, so this is also safe the
            // first time a snapshot that was never actually retained (e.g. one seeded from
            // staged_state_/deserialize rather than a live save_registers) gets restored here.
            // ReleaseCodeBufferAt takes a *host* address too - resolve state.rip the same way
            // save_registers' matching retain did.
            auto* const release_context = (thread == this->thread32_ ? this->context32_.get() : this->context_.get());
            release_context->ReleaseCodeBufferAt(release_context->FindHostAddressForGuestRIP(thread, state.rip));

            this->ensure_callret_buffer(thread, state);
            thread->CallRetStackBase = reinterpret_cast<void*>(state._pad1);

            // FEXCore bumps CodeBufferGeneration whenever it discards a Context's code buffers; every
            // return address this snapshot's shadow stack still holds points into the discarded buffer,
            // so reset it rather than resume on stale entries. Tracked per buffer (one per logical guest
            // thread per engine) against that engine's own counter - a WoW64 process's two Contexts
            // discard their buffers independently.
            auto& buffer = this->callret_buffers_.at(state._pad1);
            if (buffer.code_buffer_generation != thread->CodeBufferGeneration)
            {
                constexpr size_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
                FEXCore::Allocator::VirtualDontNeed(thread->CallRetStackBase, callret_stack_size);
                state.callret_sp = state._pad1 + callret_stack_size / 4;
                buffer.code_buffer_generation = thread->CodeBufferGeneration;
            }
        }

        void restore_registers(const std::vector<std::byte>& register_data) override
        {
            // Extended wow64 snapshot: restore BOTH engines (active + parked) and select the active one
            // from the saved flag. This is what keeps each logical thread's parked excursion frame
            // (the frozen state in whichever engine it is NOT currently running) intact across a thread
            // switch - without it, the reverse/forward gate later reads the OTHER logical thread's
            // residual engine state (stale TEB64/rsp) and mis-marshals, corrupting the 64-bit dispatch
            // stack (a wild 64-bit ret into a 32-bit-range address).
            if (register_data.size() == wow64_snapshot_size())
            {
                if (this->thread_ == nullptr)
                {
                    throw std::runtime_error("Extended wow64 snapshot restored before the 64-bit engine exists");
                }
                if (this->thread32_ == nullptr)
                {
                    this->create_thread32();
                }
                uint64_t active_is_32 = 0;
                std::memcpy(&active_is_32, register_data.data(), sizeof(active_is_32));

                // Order matters here. active_context_/active_thread_ are read from another host
                // thread at essentially any time (see their own doc comment on the request_thread_stop
                // cross-thread path) - if they were updated only after both restore_state_into calls
                // below, there would be a window where they still name the *previous* occupant's
                // engine while that engine's CurrentFrame->State has already been overwritten with
                // this restore's data, so a reader in that window would pair a stale identity with
                // fresh state. Updating them first means a reader during the restore sees the engine
                // that's about to become active, paired with that engine's own state - briefly stale
                // (not yet overwritten) rather than mismatched, which restore_state_into's caller
                // already tolerates elsewhere (this whole function only ever runs with kernel_lock_
                // held, so no logic here depends on active_thread()/active_context() mid-restore).
                if (active_is_32)
                {
                    this->active_context_.store(this->context32_.get(), std::memory_order_release);
                    this->active_thread_.store(this->thread32_, std::memory_order_release);
                }
                else
                {
                    this->active_context_.store(this->context_.get(), std::memory_order_release);
                    this->active_thread_.store(this->thread_, std::memory_order_release);
                }

                this->restore_state_into(this->thread_, register_data.data() + kWow64SnapshotHeader);
                this->restore_state_into(this->thread32_, register_data.data() + kWow64SnapshotHeader + sizeof(FEXCore::Core::CPUState));
                return;
            }

            if (register_data.size() != sizeof(FEXCore::Core::CPUState))
            {
                throw std::runtime_error("FEX register snapshot has unexpected size");
            }

            if (this->active_thread() == nullptr)
            {
                // No thread yet: writing into staged_state_, which create_thread() will seed the
                // real thread from (including installing L1Pointer/L1Mask/callret_sp correctly
                // itself afterward) - a verbatim copy here is fine.
                std::memcpy(&this->staged_state_, register_data.data(), sizeof(FEXCore::Core::CPUState));
                return;
            }

            // Single-CPUState snapshot: the process is either pure-64-bit (native - always thread_) or a
            // wow64 thread that has not yet run 32-bit code (default_register_set / a freshly-seeded
            // thread, cs=0x33). Route by the saved cs selector (0x23 = 32-bit compat) so the state lands
            // in the matching engine. Strict no-op for a pure 64-bit process (is_wow64_process_ false ->
            // always thread_, already active).
            const auto incoming_cs = reinterpret_cast<const FEXCore::Core::CPUState*>(register_data.data())->cs_idx;
            const bool incoming_is_32bit = this->is_wow64_process_ && incoming_cs == 0x23;
            if (incoming_is_32bit)
            {
                if (this->thread32_ == nullptr)
                {
                    this->create_thread32();
                }
                this->active_context_.store(this->context32_.get(), std::memory_order_release);
                this->active_thread_.store(this->thread32_, std::memory_order_release);
            }
            else
            {
                this->active_context_.store(this->context_.get(), std::memory_order_release);
                this->active_thread_.store(this->thread_, std::memory_order_release);
            }

            this->restore_state_into(this->active_thread(), register_data.data());
        }

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
        // both catch INT3 at the instruction's own (pre-advance) address. See
        // reports_breakpoint_rip_past_instruction's doc comment.
        bool reports_breakpoint_rip_past_instruction() const override
        {
            return true;
        }

        // FEXCore maintains separate context_/thread_ (64-bit) and context32_/thread32_ (32-bit)
        // engines for a WoW64 process, only one of which is active at a time (see
        // perform_bitness_switch) - see has_separate_bitness_engines' doc comment for why this
        // matters for the WoW64 NtContinue reverse-gate.
        bool has_separate_bitness_engines() const override
        {
            return true;
        }

        // request_thread_stop() mprotects InterruptFaultPage to PROT_NONE, which is safe to call from
        // any host thread - the software-quantum watchdog thread in windows_emulator::start() relies on
        // exactly this (supports_instruction_counting() is false, so that path is the only time-slicing
        // mechanism available). Matches KVM's reasoning for the same accessor.
        bool is_stop_thread_safe() const override
        {
            return true;
        }

        // emulator

        std::string get_name() const override
        {
            return "FEX";
        }

        bool supports_multiple_vcpus() const override
        {
            // sogen multiplexes logical guest threads onto a single FEXCore engine per bitness
            // (context_/context32_), cooperatively scheduled on one host thread - no multi-vCPU support.
            return false;
        }

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

        // x86_emulator

        void set_segment_base(x86_register base, pointer_type value) override
        {
            auto& state = this->cpu_state();
            if (base == x86_register::fs || base == x86_register::fs_base)
            {
                state.fs_cached = value;
            }
            else if (base == x86_register::gs || base == x86_register::gs_base)
            {
                // gs_cached stays a plain, logical (unrebased) guest address - deps/FEX's own JIT
                // now applies the wow64 rebase itself, conditionally, for GS-relative (and any
                // other) memory accesses under context_ when CONFIG_WOW64GUESTREBASE is set (see
                // ensure_context32's sibling call, notify_process_bitness, and
                // OpDispatchBuilder::GuestMemoryRebase()/Addressing.cpp in deps/FEX) - no embedder-
                // side adjustment needed here.
                state.gs_cached = value;
            }
        }

        pointer_type get_segment_base(x86_register base) override
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

        // Called once, before load_gdt() or create_thread(), right after the windows-emulator layer
        // determines the process's execution mode (see arch_emulator.hpp's doc comment on this
        // virtual). context_ (see its doc comment) is ALWAYS the 64-bit FEXCore::Context, wow64 or
        // not: a real wow64 process's thread genuinely starts executing real 64-bit ntdll code
        // before any 32-bit code ever runs, so building context_ itself as 32-bit would make the
        // JIT mis-decode that unavoidable 64-bit startup code as 32-bit garbage. is_wow64_process_
        // only gates the memory-interface-level wow64_guest_rebase (needed regardless of which
        // FEXCore::Context is executing, since the 32-bit executable/ntdll32 modules live in the
        // low address range either way) and whether context32_ gets stood up at all; it does not
        // select context_'s own bitness.
        void notify_process_bitness(bool is_wow64_process) override
        {
            this->is_wow64_process_ = is_wow64_process;
            // Tell context_'s JIT to conditionally rebase low (<4GB) addresses too - see
            // SetNeedsWow64GuestRebase's doc comment (public Context.h) for why this can't just be
            // CONFIG_IS64BIT_MODE's existing unconditional-rebase behavior: context_ stays 64-bit,
            // whose addresses aren't restricted to any range, so ordinary heap/stack/module
            // accesses must NOT be rebased - only content sogen deliberately placed below 4GB
            // (the wow64 TEB pair, wow64cpu.dll, the heaven's-gate trampoline) needs it.
            this->context_->SetNeedsWow64GuestRebase(is_wow64_process);
            if (is_wow64_process)
            {
                this->ensure_context32();
            }
        }

#ifdef __APPLE__
        // Picks a genuinely free 4GB host window for sub-4GB guest addresses to rebase into, and
        // reserves it up front as PROT_NONE before FEXCore or anything else in this process can
        // lazily claim any part of it - storing the choice in wow64_guest_rebase_. Deliberately does
        // NOT tell context_ about it here: context_ doesn't exist yet at this call site (this must
        // run before CreateNewContext() to have any chance of winning the race against FEXCore's own
        // internal allocations - see below), so initialize_context() calls
        // context_->SetWow64GuestRebaseValue(wow64_guest_rebase_) itself once context_ exists,
        // right after constructing it.
        //
        // No single fixed candidate is safe here: Cocoa/Metal's dyld-load-time host VA reservations
        // (multi-GB, ASLR'd, placed before main() runs) and a RAM-proportional system reservation
        // starting at the machine's physical RAM size both land at host-dependent addresses no
        // compile-time constant can avoid.
        // So this tries a sequence of candidates, live-probed via mach_vm_region, jumping past
        // whatever occupies each rejected one (using the occupant's own reported extent, so a huge
        // reservation is skipped in one step rather than walked past 4GB at a time) until one is
        // found genuinely empty - bounded by both a candidate-count cap and an address ceiling chosen
        // to stay comfortably below AddressSanitizer's shadow-memory floor (~0x7e00000000 on macOS/
        // arm64 - see reserved_host_ranges()'s ASan-skip comment) for instrumented builds.
        //
        // Called once, unconditionally, from initialize_context() - this backend's own construction,
        // before FEXCore's context/CodeBuffer exist and before any guest or guest-triggered code has
        // run - regardless of whether this process turns out to be wow64 at all (bitness isn't known
        // this early; harmless for a plain 64-bit guest, which never rebases anything - see
        // rebase_for). This is also as early as this backend's own code can possibly run: moving it
        // even earlier isn't possible from here, since Cocoa/Metal's own reservation happens via dyld
        // loading the frameworks before main() - before ANY of this process's own C++ constructors,
        // sogen's included - even runs at all.
        //
        // Falls back to leaving wow64_guest_rebase_/wow64_host_window_reserved_ at their defaults
        // (unchanged, existing detect-and-retry behavior) if every candidate is exhausted - this can
        // only ever improve on that baseline, never regress it.
        //
        // Reserves via mach_vm_map(), not the BSD mmap() syscall used everywhere else in this file for
        // ordinary (non-wow64-window) host allocations. A macOS 26.6.2 security update (build 25G83,
        // Aug 2026) added EXC_GUARD/DEALLOC_GAP enforcement specifically to the BSD mmap() entry point
        // for MAP_FIXED requests landing in certain host address windows - delivering an uncatchable
        // SIGKILL no in-process handler can intercept, confirmed directly on this machine at exactly
        // the address this function's default candidate uses. mach_vm_map() targeting the identical
        // address/size was confirmed, side by side in the same process, to succeed cleanly every time -
        // the guard is specific to the BSD syscall path, not the underlying Mach VM subsystem mmap() is
        // itself implemented on top of. Do not "simplify" this back to mmap(): it looks equivalent and
        // reintroduces the crash on any host running this or a later macOS security update.
        //
        // Deliberately NOT surfaced as a "reserved" range via reserved_host_ranges()/
        // reserved_host_ranges_in() (contrast with fex_internal_arena, which IS surfaced there) -
        // unlike the arena, this window IS guest address space; guest memory is meant to live here.
        // A guest's own fixed-address mmap(MAP_FIXED) call silently overwrites this PROT_NONE
        // placeholder at the kernel level with no help needed. The only failure mode to avoid is
        // sogen's OWN collision detection mistaking this placeholder for a foreign occupant and
        // refusing the legitimate guest allocation that's supposed to land there - exactly the
        // regression reserved_host_ranges()'s doc comment documents for a different range (an
        // earlier fix reserved [0, first_hit) unconditionally and broke install_wow64_heaven_gate's
        // fixed allocate_memory(kCodeBase, ...) call outright, which has no fallback search to skip
        // past a conflicting reservation). Reuse/re-allocation correctness for addresses actually
        // inside this window is already handled independently by memory_manager's own
        // reserved_regions_/overlaps_reserved_region bookkeeping - these two functions only need to
        // stop reporting the window as "foreign" at all, which the skip checks below do.
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

                mach_vm_address_t reserved_addr = candidate;
                const kern_return_t reserve_result =
                    ::mach_vm_map(mach_task_self(), &reserved_addr, wow64_guest_address_space_size, 0, VM_FLAGS_FIXED, MEMORY_OBJECT_NULL,
                                  0, FALSE, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_DEFAULT);
                if (reserve_result != KERN_SUCCESS || reserved_addr != candidate)
                {
                    // A racer claimed this exact candidate between our probe and our reservation - move on.
                    fprintf(stderr, "[FEX backend] failed to reserve wow64 host window at 0x%llx - trying the next candidate\n",
                            static_cast<unsigned long long>(candidate));
                    if (reserve_result == KERN_SUCCESS)
                    {
                        ::mach_vm_deallocate(mach_task_self(), reserved_addr, wow64_guest_address_space_size);
                    }
                    candidate += wow64_guest_address_space_size;
                    continue;
                }

                this->wow64_guest_rebase_ = candidate;
                this->wow64_host_window_reserved_ = true;
                return;
            }

            fprintf(stderr,
                    "[FEX backend] exhausted %d candidates below 0x%llx searching for a free wow64 host window - "
                    "falling back to the default at 0x%llx with detect-and-retry\n",
                    max_candidates, static_cast<unsigned long long>(search_ceiling),
                    static_cast<unsigned long long>(wow64_guest_rebase_default));
        }
#endif

        void register_gate_crossing(pointer_type address, size_t size, gate_crossing_kind kind) override
        {
            // A gate is inherently non-executable to the JIT (QueryGuestExecutableRange consults
            // gate_crossings_ directly, so reaching it raises FEXCore's synthetic #PF before any
            // byte there is compiled). handle_fault_signal's vector-14 path then recognizes the
            // faulting RIP as a gate and performs the actual crossing instead of dispatching a
            // memory violation.
            this->gate_crossings_.push_back(gate_crossing{address, size, kind});
        }

        void load_gdt(pointer_type address, uint32_t limit) override
        {
            // Only remember the base/limit for callers querying gdtr (see read_descriptor_table).
            this->gdt_base_ = address;
            this->gdt_limit_ = limit;

            // sogen writes real GDT descriptors (matching FEXCore::Core::CPUState::gdt_segment's
            // bitfield layout byte-for-byte) directly into guest memory at `address`. Since guest VA
            // == host VA under this backend's model, point FEX's own segment table at that same
            // memory instead of duplicating it - CS/segment lookups (GetSegmentFromIndex) then see
            // whatever sogen's loader wrote, including the long-mode (L) bit, with no extra sync step.
            // In 32-bit mode this is a DIRECT pointer assignment FEXCore dereferences as a host
            // address outside of any guest instruction - it bypasses the JIT-side
            // WOW64_GUEST_REBASE logic entirely, so the rebase must be applied here explicitly (see
            // wow64_guest_rebase's doc comment) - though in practice GDT_ADDR (process_context.hpp)
            // already lives well above the rebase threshold on Apple Silicon, so this evaluates to 0.
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            this->cpu_state().segment_arrays[0] = reinterpret_cast<FEXCore::Core::CPUState::gdt_segment*>(address + rebase);
        }

        // memory_interface (public)

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

        bool try_write_memory_impl(uint64_t address, const void* data, size_t size, bool invalidate_translations)
        {
            if (!this->is_range_mapped(address, size))
            {
                return false;
            }

            // sogen's loader writes guest memory it has already declared read-only (a PE section's raw
            // file bytes, regardless of the section's final protection). Unlike Unicorn's uc_mem_write,
            // this backend's guest VA == host VA is backed by real host mprotect state, so the write
            // needs a temporary permission bump. Every page must be checked, not just the first: a write
            // straddling into a read-only region would otherwise fault mid-memmove.
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

            if (invalidate_translations)
            {
                // Writing to a mapped region may overwrite already-translated code; drop FEX's cache for it.
                this->invalidate_code_range(address, size);
            }
            return true;
        }

        // hook_interface
        //
        // As with KVM, the guest runs natively, so fine-grained memory/execution/basic-block hooks
        // cannot fire; they are accepted and tracked (so delete_hook works) purely for API
        // compatibility. Only `syscall` instruction hooks are actually wired.

        emulator_hook* hook_memory_execution(memory_execution_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_execution(uint64_t /*address*/, memory_execution_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_range_execution(uint64_t /*address*/, uint64_t /*size*/,
                                                   memory_execution_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_execution_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_read(uint64_t /*address*/, uint64_t /*size*/, memory_access_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_read_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_write(uint64_t /*address*/, uint64_t /*size*/, memory_access_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_write_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_instruction(int instruction_type, instruction_hook_callback callback) override
        {
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
            auto* hook = this->make_hook();
            this->interrupt_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_memory_violation(memory_violation_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->memory_violation_hooks_[hook] = std::move(callback);
            return hook;
        }

        emulator_hook* hook_basic_block(basic_block_hook_callback callback) override
        {
            auto* hook = this->make_hook();
            this->basic_block_hooks_[hook] = std::move(callback);
            return hook;
        }

        void delete_hook(emulator_hook* hook) override
        {
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

        bool supports_global_memory_execution_hooks() const override
        {
            // Native execution: global execution hooks would require single-stepping the JIT.
            return false;
        }

#ifdef __APPLE__
        std::vector<host_reserved_range> reserved_host_ranges() const override
        {
            // Enumerates everything currently mapped in this process via the Mach VM region API, the
            // Darwin equivalent of walking /proc/self/maps.
            std::vector<host_reserved_range> ranges;

            // Every 64-bit Mach-O executable reserves a __PAGEZERO segment spanning at least [0, 4GB),
            // an OS/linker convention enforced at the mmap syscall level rather than a listed VM region
            // mach_vm_region reports: its first real hit starts well above 4GB (ASLR-dependent), yet
            // mapping guest memory anywhere in the gap below still fails. So the whole gap up to the
            // scan's first real region is reserved rather than guessing a fixed size.
            //
            // Always reserved from wow64_guest_address_space_size (4GB) up, wow64 process or not:
            // even for a wow64 process, real 64-bit modules (ntdll/win32u/wow64*.dll, see
            // module_manager::load_wow64_modules) execute under context_ (always the 64-bit
            // FEXCore::Context - see notify_process_bitness's doc comment), which applies NO
            // internal rebase of its own - their host backing must be genuinely mappable at their
            // raw guest address, so find_free_allocation_base must steer clear of this gap for them
            // exactly like it does for an ordinary 64-bit-only process. (Confirmed by a real
            // regression: with this gap left unreserved for wow64 processes, real ntdll's own
            // preferred-base placement fell back to find_free_allocation_base(...,
            // DEFAULT_ALLOCATION_ADDRESS_64BIT) - exactly 4GB, i.e. still inside this gap - and the
            // resulting raw, unrebased host mmap failed outright.)
            //
            // NOT reserved below 4GB, even for a wow64 process: unlike the plain 64-bit-only case,
            // sub-4GB guest addresses in a wow64 process are exactly this backend's
            // wow64_guest_rebase-shifted territory (the 32-bit executable/ntdll32, AND, just as
            // importantly, low-but-still-64-bit-content deliberately placed under 4GB for 32-bit-
            // pointer reachability - wow64cpu.dll via must_map_module_below_4gb, and this backend's
            // own fixed-address heaven's-gate trampoline at kCodeBase/0xFF300000,
            // wow64_heaven_gate.hpp) - all of it gets a real, valid, rebased host address up at
            // [wow64_guest_rebase, wow64_guest_rebase + 4GB) regardless, so none of it ever actually
            // touches this gap at the host level. (Reserving all of [0, first_hit) unconditionally
            // would block install_wow64_heaven_gate's fixed allocate_memory(kCodeBase, ...) call
            // outright, since that call has no fallback search to skip past a conflicting
            // reservation.)
            // The FEXCore-internal arena (see fex_internal_arena) is a live mapping the Mach scan
            // below would otherwise report as many separate sub-regions (PROT_NONE reservation, the
            // committed BlockLinks buffers, the MAP_JIT CodeBuffer, freed holes...). Skip all of them
            // and register the whole arena as a single reserved range instead, so the guest steers
            // clear of every part of it - including sub-regions allocated lazily after this one-shot
            // snapshot and any transient unmapped holes - with no dependence on the scan's timing.
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

                // Sub-regions of the FEXCore-internal arena are covered by the single explicit range
                // pushed above; don't double-report them (harmless but avoids overlap churn).
                if (arena.active() && address >= arena_base && address < arena_end)
                {
                    address += size;
                    continue;
                }

                // See reserve_wow64_host_window's doc comment: unlike the arena above, this window
                // is deliberately NOT added as a reserved range at all - it's guest address space,
                // and memory_manager's own reserved_regions_ already tracks whatever sogen has
                // legitimately placed inside it. Just don't let this one-shot scan report it as a
                // foreign occupant (whether it's still our own PROT_NONE placeholder or already-
                // committed guest content, neither is foreign).
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

                // AddressSanitizer's sparse shadow map spans tens of GB up to multiple TB per region,
                // placed far above where the guest ever allocates. Feeding those to the memory manager
                // bloats reserved_regions_ into the thousands and stalls process setup, so they are
                // skipped - in instrumented builds only.
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
            // Targeted equivalent of reserved_host_ranges() for a single query window. The fixed-
            // address allocate_memory overload only needs to know whether THIS window has been
            // claimed by a foreign host mapping since sogen last released it - not to re-enumerate
            // every region in the process, whose count (and thus that walk's cost) grows unbounded
            // over a long session. mach_vm_region's start-address parameter lets the kernel skip
            // straight to the first region at or above the (rebased) window, so this visits only
            // regions actually inside the window (usually none).
            //
            // The arena and the __PAGEZERO gap are captured into reserved_regions_ by the first full
            // scan at startup and never released, so overlaps_reserved_region already rejects a
            // target landing in them without help here; the only thing a rescan of an otherwise-free
            // window can add is a foreign mapping in a gap an earlier guest unmap munmap'd back to
            // the OS - which is exactly what a bare "is anything mapped in this host window" probe finds.
            std::vector<host_reserved_range> ranges;

            const auto rebase = rebase_for(this->is_wow64_process_, address);

            // See reserve_wow64_host_window's doc comment. A non-zero rebase means this query
            // targets the wow64 rebase window, which - if the up-front reservation succeeded - is
            // never a foreign occupant: it's either still our own PROT_NONE placeholder, or guest
            // content memory_manager's own reserved_regions_ already tracks. Reporting nothing here
            // is exactly the fix; without it, this live probe would find our own placeholder mapped
            // at the target address and register it as host_reserved before the actual fixed
            // allocation gets a chance to proceed, incorrectly rejecting it.
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

                // Report in guest (unrebased) coordinates, matching reserved_host_ranges() and what
                // reserved_regions_ is keyed by.
                const uint64_t hit_start = std::max<uint64_t>(region_addr, window_start);
                const uint64_t hit_end = std::min<uint64_t>(region_addr + region_size, window_end);
                ranges.push_back({.address = hit_start - rebase, .size = static_cast<size_t>(hit_end - hit_start)});

                probe = region_addr + region_size;
            }

            return ranges;
        }
#elif defined(__ANDROID__)
        std::vector<host_reserved_range> reserved_host_ranges() const override
        {
            return read_host_mappings_android(0, guest_address_space_end);
        }

        std::vector<host_reserved_range> reserved_host_ranges_in(const uint64_t address, const size_t size) const override
        {
            return read_host_mappings_android(address, address + size);
        }
#endif

      private:
        friend class fex_syscall_handler;

        // memory_interface (private)

        void map_mmio(uint64_t address, size_t size, mmio_read_callback read_cb, mmio_write_callback /*write_cb*/) override
        {
            // See mmio_region's doc comment for the real-backing/fault-and-emulate split.
            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX MMIO mappings must be page aligned");
            }

            void* host_backing = nullptr;
            size_t host_backing_size = 0;

#ifdef __APPLE__
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            host_backing_size = host_page_align_up_apple(size);
            const uint64_t host_address = address + rebase;

            // Same EXC_GUARD/DEALLOC_GAP hazard reserve_wow64_host_window()'s doc comment describes for
            // BSD mmap(MAP_FIXED) in this window - go through the Mach VM API instead, with the same
            // VM_FLAGS_OVERWRITE-inside-our-own-window exception claim_host_range() uses, since an MMIO
            // region backing a wow64 guest's rebased address can land inside that window's placeholder.
            mach_vm_address_t target = host_address;
            const int allocate_flags =
                rebase != 0 && this->wow64_host_window_reserved_ ? VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE : VM_FLAGS_FIXED;
            const kern_return_t result = ::mach_vm_allocate(mach_task_self(), &target, host_backing_size, allocate_flags);
            if (result == KERN_SUCCESS && target == host_address)
            {
                if (::mprotect(reinterpret_cast<void*>(host_address), host_backing_size, PROT_READ | PROT_WRITE) == 0)
                {
                    host_backing = reinterpret_cast<void*>(host_address);
                }
                else
                {
                    ::mach_vm_deallocate(mach_task_self(), target, host_backing_size);
                }
            }
#endif

            if (host_backing != nullptr)
            {
                read_cb(0, host_backing, size);
                ::mprotect(host_backing, host_backing_size, PROT_READ);
            }

            this->mmio_regions_.emplace_back(mmio_region{.address = address,
                                                         .size = size,
                                                         .read_cb = std::move(read_cb),
                                                         .host_backing = host_backing,
                                                         .host_backing_size = host_backing_size});
        }

        // Rewrites every MMIO region's real backing (see mmio_region's doc comment) with fresh
        // content, run from the guest-execution thread itself at each quantum boundary (see start())
        // so it can never race a guest read of the same page.
        void refresh_mmio_backings()
        {
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
            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX memory mappings must be page aligned");
            }

#ifdef __APPLE__
            // The host mmap/mprotect calls happen at 16KB granularity via the shadow table (see
            // sync_host_page_apple); guest VA == host VA is unaffected, this only changes which host
            // syscalls actually get issued and at what alignment.
            this->set_shadow_range_apple(address, size, permissions);
            this->sync_host_pages_covering_apple(address, size);
#elif defined(__ANDROID__)
            if (!this->host_range_is_claimed(address, size))
            {
                this->claim_host_range(address, size);
            }
            this->remap_host_claim(address, size, to_prot(permissions));
#else
            // Place the guest pages at their guest address in the host address space (guest VA == host VA).
            void* result = ::mmap(reinterpret_cast<void*>(address), size, to_prot(permissions),
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
            if (result == MAP_FAILED || reinterpret_cast<uint64_t>(result) != address)
            {
                throw std::runtime_error("FEX backend failed to map guest memory at requested address");
            }
#endif

            this->erase_region_range(address, size);
            this->regions_[address] = mapped_region{.size = size, .permissions = permissions, .owned = true};
            this->mark_executable_range(address, size, permissions);
        }

#if defined(__APPLE__) || defined(__ANDROID__)
        void reserve_guest_address_range(uint64_t address, size_t size) override
        {
#ifdef __APPLE__
            const uint64_t start = host_page_align_down_apple(address);
            const uint64_t end = host_page_align_up_apple(address + size);
#else
            const uint64_t start = address;
            const uint64_t end = address + size;
#endif

            uint64_t cursor = start;
            while (cursor < end)
            {
                auto next = this->claimed_host_ranges_.upper_bound(cursor);
                if (next != this->claimed_host_ranges_.begin())
                {
                    const auto& previous = *std::prev(next);
                    const uint64_t previous_end = previous.first + previous.second;
                    if (previous_end > cursor)
                    {
                        cursor = std::min(previous_end, end);
                        continue;
                    }
                }

                const uint64_t run_end = next == this->claimed_host_ranges_.end() ? end : std::min(next->first, end);
                this->claim_host_range(cursor, static_cast<size_t>(run_end - cursor));
                cursor = run_end;
            }
        }

        void release_guest_address_range(uint64_t address, size_t size) override
        {
            // The caller guarantees the range holds no reserved guest ranges (see memory_interface), so
            // every host page wholly inside it is a stale claim and can go back to the OS. Boundary
            // pages are kept: their outside part may belong to a neighboring, still-live reservation.
#ifdef __APPLE__
            const uint64_t start = host_page_align_up_apple(address);
            const uint64_t end = host_page_align_down_apple(address + size);
#else
            const uint64_t start = address;
            const uint64_t end = address + size;
#endif
            std::vector<host_reserved_range> released;

            auto it = this->claimed_host_ranges_.upper_bound(start);
            if (it != this->claimed_host_ranges_.begin())
            {
                --it;
            }

            for (; it != this->claimed_host_ranges_.end() && it->first < end; ++it)
            {
                const uint64_t hit_start = std::max(it->first, start);
                const uint64_t hit_end = std::min<uint64_t>(it->first + it->second, end);
                if (hit_start < hit_end)
                {
                    released.push_back({.address = hit_start, .size = static_cast<size_t>(hit_end - hit_start)});
                }
            }

            for (const auto& range : released)
            {
                const auto rebase = rebase_for(this->is_wow64_process_, range.address);
                void* const host_ptr = reinterpret_cast<void*>(range.address + rebase);

                bool restore_placeholder = false;
#ifdef __APPLE__
                // Pages inside the up-front-reserved wow64 host window must never be munmap'd:
                // reserved_host_ranges_in() reports nothing for the whole window on the strength of
                // "everything in it is ours", so a hole punched here could be claimed by a foreign
                // host allocation that a later fixed guest allocation's MAP_FIXED would silently
                // clobber. Restore the window's PROT_NONE placeholder in place instead.
                restore_placeholder = rebase != 0 && this->wow64_host_window_reserved_;
#endif
                if (restore_placeholder)
                {
                    ::mmap(host_ptr, range.size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
                }
                else
                {
                    ::munmap(host_ptr, range.size);
                }
                this->remove_host_claim(range.address, range.size);
            }
        }
#endif

        void map_host_memory(uint64_t address, size_t size, void* host_pointer, memory_permission permissions) override
        {
            if (!is_page_aligned(address) || !is_page_aligned(size))
            {
                throw std::runtime_error("FEX host memory mappings must be page aligned");
            }

            // See wow64_guest_rebase's doc comment - regions_ stays keyed by the guest (unrebased)
            // address; the real host aliasing target is rebased in 32-bit mode.
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            const uint64_t host_address = address + rebase;

#ifdef __APPLE__
            // VM_FLAGS_OVERWRITE replaces the reservation the memory manager put at the target;
            // copy=FALSE creates a second mapping of the caller-owned pages rather than copying them.
            const uint64_t claim_start = host_page_align_down_apple(host_address);
            const size_t claim_size = static_cast<size_t>(host_page_align_up_apple(host_address + size) - claim_start);
            if (!this->host_range_is_claimed(claim_start, claim_size))
            {
                this->claim_host_range(claim_start, claim_size);
            }

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
#else
            if (host_address != get_untagged_pointer_address(host_pointer))
            {
                throw std::runtime_error("FEX host memory mappings must retain their host address on Linux");
            }
#endif

            ::mprotect(reinterpret_cast<void*>(host_address), size, to_prot(permissions));
            // owned=false: the memory belongs to the caller; we must not munmap it on teardown.
            this->erase_region_range(address, size);
            this->regions_[address] = mapped_region{.size = size, .permissions = permissions, .owned = false};
            this->mark_executable_range(address, size, permissions);
        }

        bool host_memory_aliasing_is_coherent() const override
        {
#ifdef __ANDROID__
            return true;
#else
            // Apple Silicon's unified memory makes CPU/GPU coherency for the Metal buffers behind
            // MoltenVK's Vulkan buffers likely, but it is not guaranteed across every Metal storage
            // mode this bridge might use. An unnecessary flush is a harmless no-op, whereas wrongly
            // claiming coherence surfaces as rendering corruption.
            return false;
#endif
        }

#ifndef __APPLE__
        bool host_memory_mapping_requires_identity() const override
        {
            return true;
        }
#endif

        void flush_host_memory_cache(const void* host_pointer, size_t size) override
        {
            if (host_pointer == nullptr || size == 0)
            {
                return;
            }

            // The ARM64 equivalent of the KVM backend's clflushopt+sfence pair: evict the CPU data cache
            // out to memory so a GPU reading the same physical pages non-coherently sees guest writes.
#ifdef __APPLE__
            // Darwin's public API for exactly this ("useful when dealing with cache incoherent devices
            // or DMA" - OSCacheControl.h), preferred over hand-rolled `dc civac`, since EL0 access to
            // cache-maintenance instructions is not something an embedder should assume is permitted.
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
            // MMIO regions (see mmio_region's doc comment) were never really mapped at the host level
            // beyond their own optional host_backing, which is torn down here if present.
            if (std::erase_if(this->mmio_regions_, [address](const mmio_region& region) {
                    if (region.address != address)
                    {
                        return false;
                    }
                    if (region.host_backing != nullptr)
                    {
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
#elif defined(__ANDROID__)
            const auto region = this->find_region_containing(address);
            const bool unmap = region == this->regions_.end() || region->second.owned;
            if (unmap)
            {
                this->remap_host_claim(address, size, PROT_NONE);
            }
#else
            const auto region = this->find_region_containing(address);
            const bool unmap = region == this->regions_.end() || region->second.owned;
            if (unmap)
            {
                ::munmap(reinterpret_cast<void*>(address), size);
            }
#endif
            this->invalidate_code_range(address, size, /*include_inactive_contexts=*/true);
            this->erase_region_range(address, size);
        }

        void apply_memory_protection(uint64_t address, size_t size, memory_permission permissions) override
        {
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

            // Permission changes can expose/retract executable code; keep FEX's translation cache honest.
            this->invalidate_code_range(address, size);
            this->mark_executable_range(address, size, permissions);
        }

        // region bookkeeping

#if defined(__APPLE__) || defined(__ANDROID__)
        bool host_range_is_claimed(const uint64_t address, const size_t size) const
        {
            auto it = this->claimed_host_ranges_.upper_bound(address);
            if (it == this->claimed_host_ranges_.begin())
            {
                return false;
            }

            --it;
            return address >= it->first && address + size <= it->first + it->second;
        }

        void add_host_claim(const uint64_t address, const size_t size)
        {
            uint64_t start = address;
            uint64_t end = address + size;
            auto it = this->claimed_host_ranges_.lower_bound(start);

            if (it != this->claimed_host_ranges_.begin())
            {
                const auto previous = std::prev(it);
                if (previous->first + previous->second >= start)
                {
                    start = previous->first;
                    end = std::max(end, previous->first + previous->second);
                    it = this->claimed_host_ranges_.erase(previous);
                }
            }

            while (it != this->claimed_host_ranges_.end() && it->first <= end)
            {
                end = std::max(end, it->first + it->second);
                it = this->claimed_host_ranges_.erase(it);
            }

            this->claimed_host_ranges_.emplace(start, static_cast<size_t>(end - start));
        }

        void remove_host_claim(const uint64_t address, const size_t size)
        {
            auto it = this->claimed_host_ranges_.upper_bound(address);
            if (it == this->claimed_host_ranges_.begin())
            {
                return;
            }

            --it;
            const uint64_t claim_start = it->first;
            const uint64_t claim_end = claim_start + it->second;
            const uint64_t end = address + size;
            if (address < claim_start || end > claim_end)
            {
                return;
            }

            this->claimed_host_ranges_.erase(it);
            if (claim_start < address)
            {
                this->claimed_host_ranges_.emplace(claim_start, static_cast<size_t>(address - claim_start));
            }
            if (end < claim_end)
            {
                this->claimed_host_ranges_.emplace(end, static_cast<size_t>(claim_end - end));
            }
        }

        // address/size are always guest (unrebased) coordinates - add_host_claim/host_range_is_claimed
        // and claimed_host_ranges_ stay keyed by them; only the real host syscall target below is
        // shifted by rebase_for (a no-op outside a wow64 32-bit process).
        void claim_host_range(const uint64_t address, const size_t size)
        {
            const auto rebase = rebase_for(this->is_wow64_process_, address);
            const uint64_t host_address = address + rebase;
#ifdef __APPLE__
            mach_vm_address_t target = host_address;
            // Plain VM_FLAGS_FIXED requires the target to be completely unmapped, unlike BSD
            // mmap(MAP_FIXED) - it fails with KERN_NO_SPACE rather than silently replacing an existing
            // mapping. reserve_wow64_host_window() already placed a real PROT_NONE Mach mapping across
            // the whole window an in-range address lives in (see its doc comment), so claiming a
            // sub-range here needs VM_FLAGS_OVERWRITE to take it from underneath that placeholder.
            // Scoped to addresses actually inside our own already-verified-safe window: everywhere else,
            // a genuine foreign occupant still fails loudly via plain VM_FLAGS_FIXED instead of silently
            // overwriting memory this backend does not own.
            const int allocate_flags =
                rebase != 0 && this->wow64_host_window_reserved_ ? VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE : VM_FLAGS_FIXED;
            const kern_return_t result = ::mach_vm_allocate(mach_task_self(), &target, size, allocate_flags);
            if (result != KERN_SUCCESS || target != host_address)
            {
                throw std::runtime_error("FEX backend failed to reserve guest address range at the host level");
            }
            if (::mprotect(reinterpret_cast<void*>(host_address), size, PROT_NONE) != 0)
            {
                ::mach_vm_deallocate(mach_task_self(), target, size);
                throw std::runtime_error("FEX backend failed to protect a guest address reservation");
            }
#else
            void* result = ::mmap(reinterpret_cast<void*>(host_address), size, PROT_NONE,
                                  MAP_FIXED_NOREPLACE | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
            if (result == MAP_FAILED || reinterpret_cast<uint64_t>(result) != host_address)
            {
                if (result != MAP_FAILED)
                {
                    ::munmap(result, size);
                }
                throw std::runtime_error("FEX backend failed to reserve guest address range at the host level");
            }
#endif
            this->add_host_claim(address, size);
        }

        void remap_host_claim(const uint64_t address, const size_t size, const int protection)
        {
            if (!this->host_range_is_claimed(address, size))
            {
                throw std::logic_error("FEX backend cannot replace an unclaimed host range");
            }

            const auto rebase = rebase_for(this->is_wow64_process_, address);
            const uint64_t host_address = address + rebase;
#ifdef __APPLE__
            // Same EXC_GUARD/DEALLOC_GAP hazard as claim_host_range() for BSD mmap(MAP_FIXED) - this
            // always replaces an already-claimed range, so VM_FLAGS_OVERWRITE is unconditional here
            // (unlike claim_host_range()'s first-claim case, which only needs it inside the wow64
            // window's own placeholder).
            mach_vm_address_t target = host_address;
            const kern_return_t result = ::mach_vm_allocate(mach_task_self(), &target, size, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
            if (result != KERN_SUCCESS || target != host_address ||
                ::mprotect(reinterpret_cast<void*>(host_address), size, protection) != 0)
            {
                throw std::runtime_error("FEX backend failed to replace a claimed host range");
            }
#else
            void* result = ::mmap(reinterpret_cast<void*>(host_address), size, protection,
                                  MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
            if (result == MAP_FAILED || reinterpret_cast<uint64_t>(result) != host_address)
            {
                throw std::runtime_error("FEX backend failed to replace a claimed host range");
            }
#endif
        }
#endif

        // Returns the region that contains address, not merely its predecessor in the ordered map.
        std::map<uint64_t, mapped_region>::const_iterator find_region_containing(uint64_t address) const
        {
            auto it = this->regions_.upper_bound(address);
            if (it == this->regions_.begin())
            {
                return this->regions_.end();
            }

            --it;
            return address < it->first + it->second.size ? it : this->regions_.end();
        }

        // Callers are expected to have checked is_range_mapped already.
        bool range_is_writable(uint64_t address, size_t size) const
        {
            uint64_t cursor = address;
            const uint64_t end = address + size;

            while (cursor < end)
            {
                const auto it = this->find_region_containing(cursor);
                if (it == this->regions_.end() || (it->second.permissions & memory_permission::write) == memory_permission::none)
                {
                    return false;
                }

                cursor = it->first + it->second.size;
            }

            return true;
        }

        // For loader-privileged writes: sogen itself writing guest memory it declared read-only, such as
        // a PE section's initial file content before its final permission is locked in. Needed because
        // this backend enforces the declared permission through real host protection, unlike Unicorn's
        // uc_mem_write, which operates on emulated memory independent of any host mprotect state.
        void set_temporary_write_access(uint64_t address, size_t size, bool enable)
        {
#ifdef __APPLE__
            const uint64_t start = host_page_align_down_apple(address);
            const uint64_t end = host_page_align_up_apple(address + size);
            for (uint64_t host_page = start; host_page < end; host_page += host_page_size_apple)
            {
                if (!enable)
                {
                    // Restores via sync_host_page_apple, which re-derives the declared permissions
                    // from the shadow table.
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
                // See wow64_guest_rebase's doc comment - host_page is a guest address here, rebase
                // needed for the real host mprotect target.
                const auto rebase = rebase_for(this->is_wow64_process_, host_page);
                ::mprotect(reinterpret_cast<void*>(host_page + rebase), host_page_size_apple,
                           to_prot_apple(effective | memory_permission::write));
            }
#else
            // The range can span several regions_ entries with different declared permissions, so
            // both the bump and the restore work per intersecting entry.
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

        // Keeps regions_ non-overlapping, the invariant is_range_mapped/range_is_writable rely on. The
        // guest memory manager commits a reserved region in gap-filling sub-ranges (several map_memory
        // calls, so several entries tile one committed region) but decommits it in one call, so an
        // unmap range spans several entries and does not start at each entry's key. A plain
        // regions_.erase(address) would orphan the rest, and is_range_mapped's --upper_bound walk would
        // later land on a stale inner entry and wrongly report "not mapped". Entries straddling an edge
        // are trimmed or split, since the host-level unmap only touches [address, address+size).
        void erase_region_range(uint64_t address, size_t size)
        {
            const uint64_t end = address + size;

            auto it = this->regions_.lower_bound(address);
            if (it != this->regions_.begin())
            {
                auto prev = std::prev(it);
                if (prev->first + prev->second.size > address)
                {
                    it = prev; // a region starting before `address` extends into the range
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

        // Splits entries straddling an edge so only the in-range part changes. A guest protection change
        // can target a sub-range of a larger committed region, start mid-entry, or span several entries,
        // all of which a plain regions_.find(address) either misses or over-applies. Since
        // QueryGuestExecutableRange decides executability straight from these recorded permissions, a
        // stale entry makes the JIT reject a legitimately-executable page or execute one it should not.
        // Gaps in the tiling are preserved: unmapped holes are never fabricated as mapped.
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
                    it = prev; // a region starting before `address` extends into the range
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

            // Walk the (sorted) region map covering [address, end). Regions are page-granular and
            // non-overlapping, so a simple forward walk suffices.
            while (cursor < end)
            {
                const auto it = this->find_region_containing(cursor);
                if (it == this->regions_.end())
                {
                    return false;
                }
                cursor = it->first + it->second.size;
            }

            return true;
        }

#ifdef __APPLE__
        // 16KB-host vs 4KB-guest permission reconciliation. nullopt permissions means unmap: the pages
        // become "never requested" again, which must still fault like reserved-but-uncommitted guest
        // memory rather than being silently allowed.
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

        // Derives one 16KB host page's permission from its up to four 4KB shadow slots, taking their
        // union when they disagree. The union also swallows the "some slot absent" case: a
        // guard/reserved slot sharing a host page with mapped memory is folded in rather than made to
        // fault, a deliberate relaxation until a Mach exception handler can resolve faults on a
        // PROT_NONE page without breaking a legitimate access from a stricter neighbor.
        // host_page_addr is the guest page address (page_shadow_apple_/claimed_host_ranges_ stay keyed
        // by it, unrebased); the real host mmap/mprotect/munmap target is rebase_for'd in 32-bit mode -
        // despite this function's name, it's not the actual host pointer until that rebase is added.
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
            const bool currently_mapped = this->host_range_is_claimed(host_page_addr, host_page_size_apple);

            if (!any_slot_present)
            {
                if (currently_mapped)
                {
                    // The guest range may merely be decommitted while still MEM_RESERVE'd, so the page
                    // must stay claimed: munmapping it would let a foreign host allocation land here
                    // and be clobbered by a later recommit's MAP_FIXED. Replacing the mapping rather
                    // than mprotect'ing it discards the old contents, so a recommit sees the zeroed
                    // pages MEM_COMMIT requires. release_guest_address_range drops the claim for good.
                    this->remap_host_claim(host_page_addr, host_page_size_apple, PROT_NONE);
                }
                return;
            }

            if (!currently_mapped)
            {
                this->claim_host_range(host_page_addr, host_page_size_apple);
            }

            if (::mprotect(host_ptr, host_page_size_apple, to_prot_apple(effective)) != 0)
            {
                throw std::runtime_error("FEX backend failed to change memory protection");
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

        // FEX context plumbing

        void initialize_context()
        {
            // Without an installed handler, LogMan silently discards the formatted message (LogManager.
            // cpp's `if (Handler)`) while still executing FEX_TRAP_EXECUTION for ASSERT-level messages,
            // so every internal FEXCore assertion failure crashes with no indication of what failed.
            LogMan::Msg::InstallHandler([](LogMan::DebugLevels level, const char* message) {
                fprintf(stderr, "[FEXCore LogMan] level=%s: %s\n", LogMan::DebugLevelStr(level), message);
            });
            LogMan::Throw::InstallHandler([](const char* message) { fprintf(stderr, "[FEXCore LogMan THROW] %s\n", message); });

#ifdef __APPLE__
            // Must happen before the first FEXCore-internal allocation (CreateNewContext allocates the
            // CodeBuffer and dispatcher) and before reserved_host_ranges() is first queried, so the
            // whole arena is off-limits to guest allocations.
            fex_internal_arena::instance().install();

            // Claim the wow64 rebase window here too, unconditionally - not only once bitness is known
            // to be wow64 (notify_process_bitness, which used to be the sole call site). A real
            // regression showed this window can already be occupied by the time notify_process_bitness
            // runs: this process links Cocoa/Metal (sogen's own GPU/window subsystem - see the
            // vulkan-shim work), and Metal's device/heap setup reserves a large (multi-GB) host VA
            // range whose ASLR placement can land inside this window before any target executable
            // (and thus its bitness) is even known - measured directly landing on the heaven's-gate
            // trampoline's rebased target and reproducibly breaking install_wow64_heaven_gate. This is
            // the earliest point in the process (this backend's own construction, before FEXCore's
            // context/CodeBuffer, before any GUI/graphics initialization sogen itself triggers) this
            // code can act, so it gives the reservation the best chance of winning that race. Harmless
            // for a plain 64-bit-only guest: it only ever steers that guest's own allocations away from
            // this one 4GB range, exactly like any other foreign occupant already does.
            this->reserve_wow64_host_window();
#endif

            // libc++abi's default terminate handler prints nothing useful for an uncaught exception.
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

#if !defined(__ANDROID__) || __ANDROID_API__ >= 33
                void* frames[64]{};
                const int frame_count = ::backtrace(frames, 64);
                char** symbols = ::backtrace_symbols(frames, frame_count);
                fprintf(stderr, "[FEX backend] backtrace (%d frames):\n", frame_count);
                for (int i = 0; i < frame_count; ++i)
                {
                    fprintf(stderr, "  %s\n", symbols ? symbols[i] : "?");
                }
                free(symbols);
#endif

                std::abort();
            });

            FEXCore::Config::Initialize();
            FEXCore::Config::Load();

            // With EMULATOR_FEX_DUMPIR naming an existing directory, FEXCore writes one file per
            // translated guest basic block, keyed by guest RIP: "<rip:x>-pre.ir" straight out of the
            // decoder and "<rip:x>-post.ir" after optimization and register allocation. Unlike inline
            // hot-path diagnostics, this is confirmed not to perturb timing-sensitive JIT bugs.
            // PassManagerDumpIR value 3 == BEFOREOPT(1)|AFTEROPT(2).
            if (const char* dumpir_dir = std::getenv("EMULATOR_FEX_DUMPIR"))
            {
                FEXCore::Config::Set(FEXCore::Config::CONFIG_DUMPIR, dumpir_dir);
                FEXCore::Config::Set(FEXCore::Config::CONFIG_PASSMANAGERDUMPIR, "3");
            }

            // context_ is always the 64-bit FEXCore::Context, wow64 process or not - see
            // notify_process_bitness's doc comment. context32_ (ensure_context32(), built lazily
            // once a wow64 process is known) is the one built with CONFIG_IS64BIT_MODE=0.
            FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");

            // Piggybacks on FEXCore's GdbServer flag, whose only effect inside FEXCore (ContextImpl::
            // InitCore) is setting Config.NeedsPendingInterruptFaultCheck, making the JIT emit a
            // `str zr, [InterruptFaultPage]` at every block entry - the mechanism request_thread_stop()
            // needs to force a stuck-in-JIT thread to fault. FEXCore's own gdbserver is unused (sogen
            // has its own stub), so there is no other observable effect.
            FEXCore::Config::Set(FEXCore::Config::CONFIG_GDBSERVER, "1");

#ifdef __APPLE__
            const FEXCore::HostFeatures features = fetch_host_features_apple();
#elif defined(__ANDROID__)
            const FEXCore::HostFeatures features = fetch_host_features_android();
#else
            const FEXCore::HostFeatures features{}; // TODO(fex): FEXCore::FetchHostFeatures() on real HW.
#endif
            this->context_ = FEXCore::Context::Context::CreateNewContext(features);

            // Tell context_'s JIT the actual host address offset to use for the wow64 rebase -
            // whatever reserve_wow64_host_window() (called above, before context_ existed) already
            // chose, or the unchanged default if that never ran (e.g. non-Apple platforms). Must
            // happen before the first block compiles (InitCore(), below) - see
            // SetWow64GuestRebaseValue's doc comment (public Context.h).
            this->context_->SetWow64GuestRebaseValue(this->wow64_guest_rebase_);

            // active_context_/active_thread_ track whichever context/thread is currently executing -
            // see their doc comment. Execution always begins on the 64-bit engine (even a wow64
            // process starts in real 64-bit ntdll code), so initialize it to context_ here, once,
            // right after construction.
            this->active_context_.store(this->context_.get(), std::memory_order_release);

            this->syscall_handler_ = std::make_unique<fex_syscall_handler>(*this);
            this->context_->SetSyscallHandler(this->syscall_handler_.get());

            // InitCore() requires a non-null SignalDelegator, and the dispatcher entry-point addresses
            // handle_fault_signal needs come from non-virtual base-class methods, so the plain base
            // suffices - fault delivery is handled by the host signal handler installed below.
            this->signal_delegator_ = std::make_unique<FEXCore::SignalDelegator>();
            this->context_->SetSignalDelegator(this->signal_delegator_.get());

            this->context_->InitCore();

#if defined(__APPLE__) || defined(__ANDROID__)
            install_fault_signal_handlers(*this);
#endif
        }

        void ensure_context32()
        {
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

            // context32_ is a genuinely 32-bit-mode Context (GuestMemoryRebase() applies the rebase
            // unconditionally there, not just when NeedsWow64GuestRebase is set - see its doc
            // comment), so it must agree with context_/wow64_guest_rebase_ on the actual host address
            // offset - whatever reserve_wow64_host_window() already chose for this process.
            this->context32_->SetWow64GuestRebaseValue(this->wow64_guest_rebase_);

            this->syscall_handler32_ = std::make_unique<fex_syscall_handler>(*this);
            this->context32_->SetSyscallHandler(this->syscall_handler32_.get());

            this->signal_delegator32_ = std::make_unique<FEXCore::SignalDelegator>();
            this->context32_->SetSignalDelegator(this->signal_delegator32_.get());

            this->context32_->InitCore();

            // Restore the global back to what context_ (the currently-executing context) actually
            // is, so any later, unrelated CreateNewContext-driving code path (there is none today,
            // but the global is otherwise easy to leave in a surprising state) doesn't silently pick
            // up "0" from this call.
            FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
        }

        // Creates thread32_, the InternalThreadState that actually executes 32-bit guest code on
        // context32_. Called once, from create_thread() (ordinary call context) for a wow64 process
        // - deliberately not left lazy for the first 64->32 gate crossing to create, since that
        // crossing is only ever reached from inside handle_fault_signal (a signal handler), where
        // this function's real heap allocation would be unsafe. The initial CPUState this seeds is
        // irrelevant: the first gate-crossing handler to actually use thread32_ immediately
        // overwrites every GPR/XMM/x87/EFLAGS/RIP/RSP with the marshaled state from whichever
        // context is crossing down, so an all-zero NewThreadState (CreateThread's own default) is
        // fine here.
        void create_thread32()
        {
            this->thread32_ = this->context32_->CreateThread(0, 0, nullptr);

            // Real Windows shares one GDT across both bitnesses of a wow64 process (see load_gdt's
            // doc comment) - point context32_'s segment table at the exact same physical GDT memory
            // sogen's loader wrote for context_. GDT_ADDR (process_context.hpp) sits well above the
            // rebase threshold on Apple Silicon, so rebase_for evaluates to 0 regardless of bitness,
            // but apply it anyway to stay correct if that constant ever changes.
            const auto rebase = rebase_for(this->is_wow64_process_, this->gdt_base_);
            this->thread32_->CurrentFrame->State.segment_arrays[0] =
                reinterpret_cast<FEXCore::Core::CPUState::gdt_segment*>(this->gdt_base_ + rebase);

            // thread32_ does not become the active engine here - the gate-crossing handler flips
            // active_thread_/active_context_ itself, right after marshaling state into it.
            this->ensure_callret_stack(this->thread32_, this->thread32_->CurrentFrame->State);
        }

        // A registered WoW64 bitness mode-switch point (see x86_emulator::register_gate_crossing).
        struct gate_crossing
        {
            uint64_t address = 0;
            size_t size = 0;
            gate_crossing_kind kind = gate_crossing_kind::heaven_gate;
        };

        // Returns the registered gate crossing whose range contains `rip`, or nullptr. Called from
        // handle_fault_signal's synthetic-#PF path with the faulting guest RIP (the address FEXCore
        // refused to compile because QueryGuestExecutableRange reported the gate range non-executable).
        const gate_crossing* find_gate_crossing(uint64_t rip) const
        {
            for (const auto& gate : this->gate_crossings_)
            {
                if (rip >= gate.address && rip < gate.address + gate.size)
                {
                    return &gate;
                }
            }
            return nullptr;
        }

        // Extracts the 32-bit linear base of a GDT selector from the shared GDT context32_ points at
        // (segment_arrays[0]), matching FEXCore's own UpdatePrefixFromSegment/CalculateGDTBase. Used
        // to resolve the 32-bit segment bases (notably fs -> TEB32) when entering 32-bit mode, since
        // the crossing sets the selectors directly rather than executing the `mov Sreg` that would
        // otherwise populate the cached base.
        static uint32_t gdt_segment_base(FEXCore::Core::CPUState& state, uint16_t selector)
        {
            const auto* segment = FEXCore::Core::CPUState::GetSegmentFromIndex(state, selector);
            return FEXCore::Core::CPUState::CalculateGDTBase(*segment);
        }

        // Performs the real WoW64 forward (64->32) transition by emulating RunSimulatedCode's observable
        // effect: it reads the WoW64 CPU-area register block (an i386 CONTEXT the 64-bit side prepared)
        // and marshals it into the 32-bit context32_, exactly as RunSimulatedCode's own segment-setup +
        // `iretq`/`ljmp 0x23:EIP` tail would, then flips execution to the 32-bit engine. Skipping
        // RunSimulatedCode's body entirely is what avoids ever handing its `mov gs, cx` (unimplemented
        // in FEX's 64-bit JIT) to the compiler. All field offsets are relative to the r13 pointer
        // RunSimulatedCode computes as *(TEB64+0x1488)+0x80; they were decoded from the shipped
        // wow64cpu.dll (an i386 CONTEXT sits at r13-0x7C, i.e. cpu_area+4; r13-0x60 is where its
        // FloatSave member begins). Returns true on success.
        bool enter_wow64_32bit_from_run_simulated_code(const gate_crossing& gate)
        {
            // Source is always the 64-bit engine: RunSimulatedCode only ever runs under context_.
            const auto& src = this->thread_->CurrentFrame->State;

            // r12 = gs:[0x30] (TEB64 self-pointer); r13 = *(TEB64 + 0x1488) + 0x80. gs_cached is the
            // logical (unrebased) 64-bit GS base; read_memory applies the wow64 rebase as needed.
            uint64_t teb64 = 0;
            if (!this->try_read_memory(src.gs_cached + 0x30, &teb64, sizeof(teb64)) || teb64 == 0)
            {
                return false;
            }
            uint64_t cpu_area = 0;
            if (!this->try_read_memory(teb64 + 0x1488, &cpu_area, sizeof(cpu_area)) || cpu_area == 0)
            {
                return false;
            }
            const uint64_t block = cpu_area + 0x80; // == r13

            bool reads_ok = true;
            const auto read32 = [&](uint64_t offset) -> uint32_t {
                uint32_t value = 0;
                if (!this->try_read_memory(block + offset, &value, sizeof(value)))
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

            // thread32_ is built eagerly in create_thread() (ordinary call context), never lazily
            // from here - this runs inside handle_fault_signal's synthetic-#PF path, where
            // create_thread32()'s real heap allocation (CreateThread, SignalDelegator
            // construction, InitCore()) would be unsafe. Null here means create_thread() genuinely
            // never ran for this (wow64) process, which should be unreachable - a process can't
            // execute far enough to reach the heaven's gate before start()/create_thread() has run.
            // Fail the crossing rather than allocate from the signal handler if that invariant is
            // ever wrong; the caller already has a safe fallback for a failed crossing (dispatches
            // an ordinary memory violation instead of resuming into half-marshaled state).
            if (this->thread32_ == nullptr)
            {
                return false;
            }
            auto& dst = this->thread32_->CurrentFrame->State;

            // Carry the genuinely-architectural register file across first, then overwrite the pieces
            // the WoW64 CPU-area block authoritatively defines for the 32-bit entry.
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

            // RunSimulatedCode's FULL path restores xmm0..5 from the CPU-area block (0xf0..0x140); the
            // rest of the XMM file is carried from the 64-bit engine by marshal_architectural_state.
            for (int i = 0; i < 6; ++i)
            {
                this->try_read_memory(block + 0xf0 + static_cast<uint64_t>(i) * 0x10, &dst.xmm.avx.data[i][0], 16);
            }

            // The 32-bit compat-mode selector set RunSimulatedCode installs: CS=0x23, SS/DS/ES=0x2b,
            // FS=0x53 (TEB32), GS flat. Resolve the cached bases from the shared GDT (only FS is
            // non-zero - it points at TEB32); leaving them right is what makes 32-bit fs:[...] TEB
            // accesses land correctly.
            dst.cs_idx = 0x23;
            dst.ss_idx = 0x2b;
            dst.ds_idx = 0x2b;
            dst.es_idx = 0x2b;
            dst.fs_idx = 0x53;
            dst.gs_idx = 0;
            dst.cs_cached = gdt_segment_base(dst, 0x23);
            dst.ss_cached = gdt_segment_base(dst, 0x2b);
            dst.ds_cached = gdt_segment_base(dst, 0x2b);
            dst.es_cached = gdt_segment_base(dst, 0x2b);
            dst.fs_cached = gdt_segment_base(dst, 0x53);
            dst.gs_cached = 0;

            // The forward gate intercepts RunSimulatedCode at its true entry (RVA 0x1650 == gate.address),
            // so its prologue never executes. That prologue is (from wow64cpu.dll's on-image UNWIND_INFO):
            //   push r15; push r14; push r13; push r12; push rbx; push rsi; push rdi; push rbp; sub rsp,0x68
            // leaving 8 saved nonvolatiles + a 0x68 local frame, with RunSimulatedCode's own return address
            // (into its caller: BTCpuSimulate, or Wow64KiUserCallbackDispatcher during a kernel callback) at
            // rsp+0xA8. The 64-bit engine is frozen here and later resumed INSIDE RunSimulatedCode's body
            // (the reverse gate resumes it at 0x17af to run Wow64SystemServiceEx), where the on-image
            // UNWIND_INFO assumes the prologue ran. If we leave the frozen rsp at the raw entry level, a
            // later callback-return longjmp (RtlUnwindEx) virtual-unwinds this frame by rsp+0xA8 and reads
            // uninitialized stack instead of the real caller return address ->
            // RtlpxVirtualUnwind's no-progress leaf guard returns 0xC00000FF ->
            // noncontinuable exception -> STATUS_FATAL_USER_CALLBACK_EXCEPTION. Native (Unicorn) executes
            // the real prologue and unwinds correctly. So emulate the prologue's stack effect now: spill the
            // 8 nonvolatiles into their canonical slots and drop rsp by 0xA8, making the frozen 64-bit frame
            // unwindable exactly as the on-image UNWIND_INFO describes. Only on the true entry - the 0x167f
            // syscall re-entry already runs with rsp prologue-adjusted and must not be double-counted.
            auto& state64 = this->thread_->CurrentFrame->State;
            if (src.rip == gate.address)
            {
                const uint64_t entry_rsp = state64.gregs[detail::greg_rsp];
                const auto spill = [&](uint64_t below_entry, int greg) {
                    const uint64_t value = state64.gregs[greg];
                    this->write_marshal_state(entry_rsp - below_entry, &value, sizeof(value));
                };
                spill(0x08, 15); // r15
                spill(0x10, 14); // r14
                spill(0x18, 13); // r13
                spill(0x20, 12); // r12
                spill(0x28, detail::greg_rbx);
                spill(0x30, detail::greg_rsi);
                spill(0x38, detail::greg_rdi);
                spill(0x40, detail::greg_rbp);
                state64.gregs[detail::greg_rsp] = entry_rsp - 0xA8;
            }

            // RunSimulatedCode's body (which this forward gate skips) loads the WoW64-reserved 64-bit
            // registers before the `jmp` into 32-bit mode (wow64cpu.dll, from RVA 0x1660 onward):
            //   r12 = gs:[0x30] (TEB64 self-pointer)
            //   r13 = *(TEB64+0x1488)+0x80 (the CpuArea i386-CONTEXT block == `block` above)
            //   r14 = the 64-bit rsp captured right before the mode switch (`mov r14, rsp`, i.e. the
            //         frozen RunSimulatedCode frame the thread returns to when re-entering 64-bit)
            //   r15 = wow64cpu!TurboThunkDispatch jump table (image_base + 0x36d0)
            // The 64-bit engine (thread_) stays frozen here while 32-bit code runs, but on a 32-bit
            // fault dispatch_exception must build a 64-bit exception CONTEXT whose R12..R15 hold these
            // reserved values: ntdll!KiUserExceptionDispatcher -> wow64!Wow64PrepareForException
            // derives the 64-bit exception stack directly from CONTEXT.R14 (`mov rsp, <R14-derived>`),
            // and read_raw_register sources r8..r15 for a wow64 32-bit-active capture from thread_ (the
            // 32-bit engine's own r8..r15 are meaningless and get clobbered by the fault's SRA spill).
            // Leaving these stale sent the 64-bit exception dispatcher to a wild host-range stack and
            // crashed exception delivery (rip=4); native's RunSimulatedCode body runs for real and sets
            // them, so it never diverged. Match RunSimulatedCode's `mov r14, rsp` timing: by this point
            // state64.gregs[rsp] holds the final frozen 64-bit frame on both the 0x1650 entry and the
            // 0x167f syscall re-entry (both skip the body).
            state64.gregs[12] = teb64;                                                    // r12 = TEB64
            state64.gregs[13] = block;                                                    // r13 = CpuArea block
            state64.gregs[14] = state64.gregs[detail::greg_rsp];                          // r14 = 64-bit frame
            state64.gregs[15] = (gate.address & ~static_cast<uint64_t>(0xFFFF)) + 0x36d0; // r15 = turbo table

            this->active_context_.store(this->context32_.get(), std::memory_order_release);
            this->active_thread_.store(this->thread32_, std::memory_order_release);
            return true;
        }

        // Performs the real WoW64 reverse (32->64) transition. The 32-bit ntdll syscall stub reaches
        // wow64cpu.dll's WOW64SVC thunk (RVA 0x2010) via `call fs:[0xC0]` (Wow64Transition); the thunk
        // does a far `ljmp 0x33:0x2024` into 64-bit mode, which the fixed-bitness 32-bit Context cannot
        // execute. Instead of the thunk's bitness switch + wow64cpu.dll's own reverse-marshal (0x1779),
        // we marshal the current 32-bit register file into the WoW64 CPU-area CONTEXT block ourselves
        // and resume the 64-bit engine (context_, frozen at RunSimulatedCode's entry by the forward
        // crossing) at TurboDispatchJumpAddressEnd (0x17af). The real 64-bit TurboDispatch +
        // wow64.dll!Wow64SystemServiceEx then run, translating the 32-bit service number to its 64-bit
        // equivalent and issuing a genuine 64-bit `syscall` that sogen's own syscall hook catches (the
        // same path the native backends use, so sogen dispatches by the translated 64-bit number).
        // wow64.dll writes the result into CONTEXT.Eax and `jmp 0x167f`s back into RunSimulatedCode's
        // body - which is inside the forward gate's range, so the forward crossing re-fires and
        // re-enters 32-bit code at the syscall's return point carrying the result. Returns true on
        // success; false only on a genuine memory-read failure (caller then falls through to the
        // ordinary memory-violation path).
        bool enter_wow64_64bit_from_wow64svc_thunk(const gate_crossing& gate)
        {
            // wow64cpu.dll layout: TurboDispatchJumpAddressStart @0x17a6; the r15 turbo-thunk jump
            // table that BTCpuProcessInit builds @0x36d0. This handler serves BOTH reverse-gate bop
            // codes - the WOW64SVC thunk (RVA 0x2010) and the W64SVC turbo bop (RVA 0x6000) - so the
            // image base is recovered by rounding the gate address down to the 64K PE allocation
            // granularity rather than subtracting a single fixed RVA.
            const uint64_t image_base = gate.address & ~static_cast<uint64_t>(0xFFFF);
            // Resume at the GENERIC dispatcher (TurboDispatchJumpAddressEnd @0x17af), NOT the turbo
            // table dispatch @0x17a6. 0x17a6 does `mov ecx,eax; shr ecx,0x10; jmp [r15+8*rcx]`, which
            // for a turbo-encoded service number (eax>>16 != 0, e.g. NtQueryPerformanceCounter) jumps
            // into an inline turbo thunk in wow64cpu.dll whose RETURN to 32-bit is its own bitness-
            // switch `ljmp` (RVA 0x1cd4 et al.) - sogen registers no gate there, so FEX cannot execute
            // that far jump and the turbo thunk corrupts the 32-bit state (the *next* syscall then reads
            // garbage args on the *next* syscall). 0x17af instead does
            // `mov ecx,eax; mov rdx,r11; call Wow64SystemServiceEx; mov [r13+0x34],eax; jmp 0x167f`,
            // returning via 0x167f (inside RunSimulatedCode's forward gate) - the only 64->32 return
            // sogen intercepts. Forcing generic for ALL syscalls is correct: Wow64SystemServiceEx
            // derives the service table/index from only the low 14 bits of eax ((eax>>12)&3, eax&0xFFF)
            // and ignores the high-word turbo index, so no masking of eax is needed. table[0] is 0x17af
            // anyway, so non-turbo syscalls are unaffected; turbo syscalls just take the slower (but
            // correct) generic thunk.
            //
            // The +0x17af offset is only a fallback: the RVA drifts across real wow64cpu.dll builds
            // (on some builds the bytes there decode as nonsense, not the documented
            // `mov ecx,eax; ...` sequence, and executing them corrupts guest memory beyond repair).
            // module_manager resolves the real TurboDispatchJumpAddressEnd export address and hands
            // it over via set_wow64_turbo_dispatch_end - always prefer that when it's been set.
            const uint64_t generic_dispatch = this->wow64_turbo_dispatch_end_ != 0 ? this->wow64_turbo_dispatch_end_ : image_base + 0x17af;
            const uint64_t jump_table = image_base + 0x36d0;

            // Source: the 32-bit engine that reached the thunk (SRA already spilled - this is a
            // controlled synthetic #PF). Its live register file is the syscall's argument context.
            const auto& src32 = this->active_thread()->CurrentFrame->State;
            const uint32_t eax = static_cast<uint32_t>(src32.gregs[detail::greg_rax]);
            const uint32_t ecx = static_cast<uint32_t>(src32.gregs[detail::greg_rcx]);
            const uint32_t edx = static_cast<uint32_t>(src32.gregs[detail::greg_rdx]);
            const uint32_t ebx = static_cast<uint32_t>(src32.gregs[detail::greg_rbx]);
            const uint32_t ebp = static_cast<uint32_t>(src32.gregs[detail::greg_rbp]);
            const uint32_t esi = static_cast<uint32_t>(src32.gregs[detail::greg_rsi]);
            const uint32_t edi = static_cast<uint32_t>(src32.gregs[detail::greg_rdi]);
            const uint32_t esp = static_cast<uint32_t>(src32.gregs[detail::greg_rsp]);
            const uint32_t eflags = reconstruct_compacted_eflags(src32);

            // The stub reached the thunk via `call fs:[0xC0]`, so [esp] is the 32-bit return address
            // (the `ret` after that call) and the syscall args follow the caller's own return slot:
            // [esp]=stub-ret, [esp+4]=caller-ret, [esp+8]=arg1. The transition must resume the stub at
            // its return address with esp advanced past it (as if `call fs:[0xC0]` returned normally).
            uint32_t return_eip = 0;
            if (!this->try_read_memory(esp, &return_eip, sizeof(return_eip)))
            {
                return false;
            }

            // Recompute the CpuArea CONTEXT block from the 64-bit engine's TEB64, exactly as the
            // forward crossing does (TEB64/CpuArea are high addresses, so the wow64 rebase is a no-op).
            const auto& state64 = this->thread_->CurrentFrame->State;
            uint64_t teb64 = 0;
            if (!this->try_read_memory(state64.gs_cached + 0x30, &teb64, sizeof(teb64)) || teb64 == 0)
            {
                return false;
            }
            uint64_t cpu_area = 0;
            if (!this->try_read_memory(teb64 + 0x1488, &cpu_area, sizeof(cpu_area)) || cpu_area == 0)
            {
                return false;
            }
            const uint64_t block = cpu_area + 0x80;

            // Reverse-marshal the full 32-bit register file into the CONTEXT block (what wow64cpu.dll's
            // 0x1779 does for the GPRs, plus xmm0..5 so the block is the authoritative 32-bit state
            // across the dispatch - the forward re-entry reads xmm back unconditionally). wow64.dll
            // overwrites CONTEXT.Eax@0x34 with the syscall result before that re-entry.
            const auto write32 = [&](uint64_t offset, uint32_t value) { this->write_marshal_state(block + offset, &value, sizeof(value)); };
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
                this->write_marshal_state(block + 0xf0 + static_cast<uint64_t>(i) * 0x10, &src32.xmm.avx.data[i][0], 16);
            }

            // Set up the 64-bit engine to resume at the generic dispatcher (0x17af), which does
            // `mov ecx,eax; mov rdx,r11; call Wow64SystemServiceEx; mov [r13+0x34],eax; jmp 0x167f`.
            // Required: eax=service#, r13=CONTEXT block, r11=args pointer. The remaining GPRs are
            // carried live (harmless; the generic thunk reads its args from [r11]). r15 is still set to
            // the turbo jump table for parity even though 0x17af does not consult it. rsp is left frozen
            // at the 64-bit RunSimulatedCode stack (a valid writable stack for the dispatch call frame);
            // the re-entry rebuilds r12/r14 itself.
            auto& dst64 = this->thread_->CurrentFrame->State;
            dst64.rip = generic_dispatch;
            dst64.gregs[detail::greg_rax] = eax;
            dst64.gregs[detail::greg_rcx] = ecx;
            dst64.gregs[detail::greg_rdx] = edx;
            dst64.gregs[detail::greg_rbx] = ebx;
            dst64.gregs[detail::greg_rbp] = ebp;
            dst64.gregs[detail::greg_rsi] = esi;
            dst64.gregs[detail::greg_rdi] = edi;
            dst64.gregs[11] = static_cast<uint64_t>(esp) + 8; // R11 = args pointer (rebased on deref)
            dst64.gregs[13] = block;                          // R13 = CONTEXT block
            dst64.gregs[15] = jump_table;                     // R15 = turbo-thunk jump table

            this->active_context_.store(this->context_.get(), std::memory_order_release);
            this->active_thread_.store(this->thread_, std::memory_order_release);

            return true;
        }

        // Performs a generic bitness-switch crossing given an already-decoded target RIP/RSP/CS:
        // marshals the architectural register file from the currently-active engine into whichever
        // engine target_cs selects, preserving the destination's own segment state, r12-r15, and
        // rax-rbx-rcx-rdx (all of which marshal_architectural_state would otherwise clobber with the
        // source's - see the comments below), then flips active_context_/active_thread_ so the next
        // ExecuteThread runs the destination engine from target_rip. Shared by the heaven's-gate
        // exception-delivery crossing (whose target_rip/rsp/cs come from registers dispatch_exception_
        // pointers set up) and the far-jmp CPU-mode-probe crossing (whose target_rip/cs come from
        // decoding the `jmp far` instruction's own immediate operand instead).
        bool perform_bitness_switch(const uint64_t target_rip, const uint64_t target_rsp, const uint16_t target_cs)
        {
            const auto& src = this->active_thread()->CurrentFrame->State;
            const bool target_is_64bit = (target_cs == wow64_user_code_selector_64bit);

            FEXCore::Context::Context* dst_context = nullptr;
            FEXCore::Core::InternalThreadState* dst_thread = nullptr;
            if (target_is_64bit)
            {
                // thread_ always exists by the time any crossing can fire - a wow64 process starts
                // executing 64-bit code (which created thread_) long before it can reach a gate.
                dst_context = this->context_.get();
                dst_thread = this->thread_;
            }
            else
            {
                // thread32_ is built eagerly in create_thread() (ordinary call context) - see its
                // doc comment for why lazily creating it from here (inside handle_fault_signal's
                // call chain) would be unsafe. Null here should be unreachable; fail the crossing
                // rather than allocate from the signal handler if that invariant is ever wrong.
                if (this->thread32_ == nullptr)
                {
                    return false;
                }
                dst_context = this->context32_.get();
                dst_thread = this->thread32_;
            }

            auto& dst = dst_thread->CurrentFrame->State;

            // A WoW64 bitness crossing must NOT carry the source engine's segment state into the
            // destination: each engine owns mode-appropriate FS/GS bases (the 64-bit engine's GS ->
            // TEB64, the 32-bit engine's FS -> TEB32). marshal_architectural_state copies the whole
            // segment block, which clobbered the 64-bit engine's GS base (TEB64) with the 32-bit
            // engine's (0) on the heaven's-gate exception path, so the 64-bit KiUserExceptionDispatcher
            // read gs:[0x30] against a null base and corrupted itself into a wild jump. Preserve the
            // destination engine's own segment selectors + cached bases across the marshal, mirroring
            // how the reverse gate (enter_wow64_64bit_from_wow64svc_thunk) leaves the 64-bit engine's
            // segments untouched. The destination is a continuously-live engine, so its selectors and
            // bases are already correct for its own mode.
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

            // Same reasoning as the segment preservation above, applied to r12-r15: these are the
            // wow64cpu-reserved registers the forward crossing populates on the 64-bit engine (thread_)
            // - r12/r13 = TEB64/CpuArea block, r14 = the frozen 64-bit exception stack real Windows'
            // Wow64PrepareForException reads via CONTEXT.R14 (see enter_wow64_32bit_from_run_simulated_code's
            // doc comment), r15 = the turbo jump table. marshal_architectural_state copies the FULL
            // register file including r8-r15, so crossing back into the 64-bit engine here (the
            // heaven's-gate exception-delivery path) overwrote thread_'s carefully-set r12-r15 with
            // whatever was in the 32-bit engine's (thread32_) same slots - meaningless SRA-spill garbage,
            // since 32-bit code cannot address r8-r15 at all. That garbage r14 then fed
            // Wow64PrepareForException's real stack-derivation logic, producing a wild address and a
            // second fault inside KiUserExceptionDispatcher itself. Preserve the destination engine's
            // own r12-r15 across the marshal exactly like the segment state above.
            const auto saved_r12 = dst.gregs[12];
            const auto saved_r13 = dst.gregs[13];
            const auto saved_r14 = dst.gregs[14];
            const auto saved_r15 = dst.gregs[15];

            // rax/rbx/rcx/rdx are the trampoline's OWN scratch registers here (see the convention
            // comment above: dispatch_exception_pointers stuffs rax=target RIP, rbx=target RSP,
            // rcx=target CS selector, rdx=target SS selector into the SOURCE (pre-crossing) engine
            // purely so the trampoline's iretq can consume them). They were never meant to be live
            // architectural state - target_rip/target_rsp are already captured above from src, and
            // target_cs was only needed to pick the destination engine. marshal_architectural_state
            // copies the whole GPR file though, so without this the destination engine's genuine
            // rax/rbx/rcx/rdx (whatever the guest was last doing with them) get clobbered by these
            // selector/address scratch values instead - the same class of leak the segment and
            // r12-r15 preservation above already guards against. Preserve them the same way.
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

            this->active_context_.store(dst_context, std::memory_order_release);
            this->active_thread_.store(dst_thread, std::memory_order_release);
            return true;
        }

        // gate.address here is a `jmp far 0x33:<target>` (opcode 0xEA - see gate_crossing_kind::
        // far_jmp_bitness_switch's doc comment). It was originally taken for a standalone, one-time
        // "can the CPU switch to 64-bit mode" hardware check, unrelated to the real syscall
        // dispatch path - but live traces prove otherwise: it's reached via the EXACT SAME route as
        // wow64cpu_dispatch's WOW64SVC thunk (the 32-bit syscall stub's `call fs:[0xC0]` /
        // Wow64Transition indirection lands here directly, with eax/edx already holding the
        // syscall number and the stub's own return address still on the stack, un-pushed-to by
        // anything in between). This IS wow64cpu.dll's real Wow64Transition entry point for this
        // build - a `jmp far` into 64-bit mode is simply how it happens to be implemented here,
        // rather than the turbo-bop mechanism wow64cpu_dispatch's gates model. Treat it exactly
        // like reaching the WOW64SVC thunk: reverse-marshal the 32-bit register file and resume at
        // the generic dispatcher, using the already-proven-correct logic verbatim.
        bool enter_bitness_switch_from_far_jmp(const gate_crossing& gate)
        {
            return this->enter_wow64_64bit_from_wow64svc_thunk(gate);
        }

        // Performs a WoW64 bitness gate crossing: marshals the architectural register file out of the
        // currently-active engine into the other-bitness engine, sets the target's entry point/stack/
        // segment selectors per the crossing's calling convention, and flips active_context_/
        // active_thread_ so the next ExecuteThread runs the other engine. Called from
        // handle_fault_signal once the faulting RIP is recognized as a registered gate.
        //
        // For the heaven's-gate kind, direction is data-driven by the target CS selector, not by
        // which engine is currently active: the same trampoline mechanism is bidirectional (whatever
        // CS you load selects the mode), so reading the target CS is the robust way to decide the
        // destination engine. This makes both the 64->32 entry into 32-bit code and the 32->64
        // return (e.g. exception delivery via exception_dispatch.cpp) go through one handler.
        //
        // Returns true if the crossing was performed; false on a genuine marshaling failure, in
        // which case the caller falls back to the ordinary memory-violation path rather than
        // silently applying the wrong convention.
        bool perform_gate_crossing(const gate_crossing& gate)
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

            // gate_crossing_kind::heaven_gate: confirmed trampoline convention (wow64_heaven_gate.hpp,
            // cross-checked against exception_dispatch.cpp which drives it programmatically) - the
            // trampoline's final iretq consumes RIP<-RAX, CS<-RCX, RFLAGS<-(pushfq), RSP<-RBX, SS<-RDX,
            // leaving the GPRs otherwise intact.
            const auto& src = this->active_thread()->CurrentFrame->State;
            return this->perform_bitness_switch(src.gregs[detail::greg_rax], src.gregs[detail::greg_rbx],
                                                static_cast<uint16_t>(src.gregs[detail::greg_rcx]));
        }

        // Returns the host address offset to add to `address` if it needs the wow64 rebase applied -
        // see wow64_guest_rebase_default's doc comment for why this is a per-instance member
        // (wow64_guest_rebase_) rather than a fixed constant. See wow64_guest_address_space_size's
        // doc comment for why is_32bit_mode alone isn't the gate - the address itself must also be
        // below that boundary. Unconditional (not Apple-only): this backend is shared with Linux
        // ARM64, which also needs it (wow64_guest_rebase_ just stays at its default there, since
        // reserve_wow64_host_window - the only thing that ever changes it - is Apple-only).
        uint64_t rebase_for(bool is_32bit_mode, uint64_t address) const
        {
            return (is_32bit_mode && address < wow64_guest_address_space_size) ? this->wow64_guest_rebase_ : 0ULL;
        }

        // Un-rebases a real hardware fault address (info->si_addr) back to the guest address space
        // when it falls in the wow64-rebased range a 32-bit context's guest memory actually lives in
        // (see rebase_for's doc comment) - a no-op in 64-bit mode. Needed anywhere a fault address is
        // compared against or dispatched to guest-address-keyed structures (mmio_regions_,
        // page_shadow_apple_, memory_violation_hooks_), as opposed to used directly as a real host
        // pointer (e.g. handle_misaligned_atomic_fault's memcpy), which must keep the original,
        // rebased address.
        uint64_t unrebase_fault_addr(uint64_t fault_addr) const
        {
            if (this->is_wow64_process_ && fault_addr >= this->wow64_guest_rebase_ &&
                fault_addr < this->wow64_guest_rebase_ + wow64_guest_address_space_size)
            {
                return fault_addr - this->wow64_guest_rebase_;
            }
            return fault_addr;
        }

#if defined(__APPLE__) || defined(__ANDROID__)
      public:
        // Applies a decode_arm64_load result once its data has been fetched (from an mmio_region's
        // read_cb, or a plain memcpy off real guest memory - see handle_mmio_fault and
        // handle_misaligned_atomic_fault) - writes the (possibly extended) value into the destination
        // register and advances PC past the single decoded instruction.
        bool complete_decoded_load(ucontext_t* uctx, const decoded_arm64_load& decoded, const void* data, uint64_t pc)
        {
            if (decoded.is_vector)
            {
                auto* fprs = get_host_vector_registers(uctx);
                if (fprs == nullptr)
                {
                    return false;
                }

                __uint128_t value{};
                std::memcpy(&value, data, sizeof(value));
                fprs[decoded.rt] = value;
                set_host_pc(uctx, pc + 4);
                return true;
            }

            uint64_t raw_value = 0;
            std::memcpy(&raw_value, data, decoded.size);

            uint64_t result = 0;
            switch (decoded.size)
            {
            case 1:
                result = decoded.sign_extend ? static_cast<uint64_t>(static_cast<int64_t>(static_cast<int8_t>(raw_value)))
                                             : (raw_value & 0xFFULL);
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
                // Writing Wt always zeroes bits 63:32 of the aliased Xt (AArch64 register semantics).
                result &= 0xFFFFFFFFULL;
            }

            // set_host_gpr no-ops for index 31 (XZR/WZR): the load's result is discarded, nothing to
            // write back.
            set_host_gpr(uctx, decoded.rt, result);
            set_host_pc(uctx, pc + 4);
            return true;
        }

        bool handle_mmio_fault(ucontext_t* uctx, const mmio_region& region, uint64_t fault_addr)
        {
            const uint64_t pc = get_host_pc(uctx);
            const auto insn = *reinterpret_cast<const uint32_t*>(pc);
            const auto decoded = decode_arm64_load(insn);
            if (!decoded)
            {
                // fprintf/stdio is not async-signal-safe (internal buffering/locking) - this runs
                // inside a real signal handler, so use snprintf into a fixed stack buffer followed by
                // a single write(2) instead, the standard pragmatic idiom for signal-handler-safe
                // formatted output (see jit_write_protect_retry_count_for's doc comment for the fuller
                // async-signal-safety rationale that motivated this).
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
            return this->complete_decoded_load(uctx, *decoded, buffer, pc);
        }

        // Real hardware LDAR/LDAPR/STLR (load-acquire/store-release) instructions require natural
        // alignment, unlike plain LDR/STR - but x86 permits unaligned accesses freely, and FEX uses
        // this family to model x86's stronger memory ordering on ARM's weaker one, so an ordinary
        // unaligned guest access to otherwise legitimately mapped memory can fault here (Darwin
        // reports it as SIGBUS/BUS_ADRALN). sogen runs every guest thread of a process cooperatively
        // on a single host thread (see windows_emulator.cpp's central loop), so there is no real
        // concurrent host-thread race for these instructions to order against here - downgrading to a
        // plain, non-atomic access is therefore correctness-preserving, not just a workaround.
        bool handle_misaligned_atomic_fault(ucontext_t* uctx, uint64_t fault_addr)
        {
            const uint64_t pc = get_host_pc(uctx);
            const auto insn = *reinterpret_cast<const uint32_t*>(pc);

            if (const auto load = decode_arm64_load(insn))
            {
                return this->complete_decoded_load(uctx, *load, reinterpret_cast<const void*>(fault_addr), pc);
            }

            if (const auto store = decode_arm64_store(insn))
            {
                // get_host_gpr returns 0 for index 31 (XZR): stores zero, matching real hardware.
                const uint64_t value = get_host_gpr(uctx, store->rt);
                std::memcpy(reinterpret_cast<void*>(fault_addr), &value, store->size);
                set_host_pc(uctx, pc + 4);
                return true;
            }

            return false;
        }

        // memory_violation_hooks_/interrupt_hooks_ callbacks are shared, backend-agnostic
        // windows-emulator code (dispatch_exception and friends) that allocates, logs, and mutates
        // STL containers freely - safe when invoked from normal call context (as KVM/Unicorn do, after
        // a blocking syscall or interpreter callback returns), but NOT safe to call directly from
        // inside handle_fault_signal, a real kernel-delivered SIGSEGV/SIGBUS/SIGILL handler that can
        // interrupt an unrelated malloc()/free() or STL mutation already in progress on this thread -
        // confirmed to be a real, ASLR-timing-dependent heap-corruption hazard (see
        // jit_write_protect_retry_count_for's doc comment for the same class of bug at smaller scale).
        // Instead of calling hooks in-handler, stash what's needed here (plain data, no allocation) and
        // force ExecuteThread to unwind back to start() (via ThreadStopHandlerAddress, exactly like a
        // real stop - but without touching stop_requested_), which then dispatches the hook in normal
        // context and resumes guest execution by simply re-entering ExecuteThread: it always starts
        // fresh from CurrentFrame->State.rip, which is exactly what AbsoluteLoopTopAddressFillSRA
        // already re-derived SRA from, so this is behaviorally identical to resuming in-handler.
        enum class pending_fault_kind
        {
            none,
            memory_violation,
            interrupt,
            // A WoW64 gate crossing already performed the state marshal + active_context_/
            // active_thread_ flip inside handle_fault_signal; this only tells start()'s loop to
            // resume (re-enter ExecuteThread on the now-active engine) rather than break. No hook runs.
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

        // Called only from start(), in normal call context, right after ExecuteThread returns - see
        // pending_fault_dispatch_'s doc comment. Returns true if a hook was actually dispatched (i.e.
        // ExecuteThread returned because handle_fault_signal deferred a hook, not because of a genuine
        // stop_requested_ - start()'s loop uses the return value to decide whether to resume).
        bool dispatch_pending_hook_if_any()
        {
            const pending_fault_dispatch dispatch = this->pending_fault_dispatch_;
            this->pending_fault_dispatch_.kind = pending_fault_kind::none;

            switch (dispatch.kind)
            {
            case pending_fault_kind::memory_violation:
                for (auto& [_, hook] : this->memory_violation_hooks_)
                {
                    hook(*this, dispatch.address, dispatch.size, dispatch.operation, dispatch.type);
                }
                return true;
            case pending_fault_kind::interrupt:
                for (auto& [_, hook] : this->interrupt_hooks_)
                {
                    hook(*this, dispatch.vector);
                }
                return true;
            case pending_fault_kind::gate_crossing:
                // The crossing itself already happened in-handler; nothing to dispatch. Return true
                // so start()'s loop re-enters ExecuteThread on the freshly-flipped active engine
                // (which resumes from its CurrentFrame->State.rip, set by perform_gate_crossing).
                return true;
            case pending_fault_kind::none:
            default:
                return false;
            }
        }

        // Called only from within handle_fault_signal (real signal-handler context) whenever a hook
        // needs to run. See pending_fault_dispatch_'s doc comment for why hooks can't be called
        // directly from here: stash the (plain-data, non-allocating) dispatch request and force
        // ExecuteThread to unwind back to start(), which dispatches it safely in normal call context
        // and then simply resumes by re-entering ExecuteThread - it always starts fresh from
        // CurrentFrame->State.rip, which the hook is free to redirect (e.g. into the guest's own
        // exception dispatcher), exactly as it could before when resumed via
        // AbsoluteLoopTopAddressFillSRA directly from here.
        //
        // sra_already_spilled distinguishes the two unwind entry points the dispatcher provides
        // (Dispatcher.cpp: ThreadStopHandlerAddressSpillSRA falls through SpillStaticRegs into
        // ThreadStopHandlerAddress's plain PopCalleeSavedRegisters+ret) - callers whose fault happened
        // via FEXCore's own controlled synthetic-exception path (vector==14/interrupt dispatch, where
        // SRA is already spilled to CpuStateFrame by the time this C++ code runs) must pass true;
        // callers interrupting arbitrary, uncontrolled points in live guest-translated JIT code (a
        // real hardware fault directly on translated code, see handle_general_memory_violation) must
        // pass false, since SRA is still live only in host registers there and skipping the spill
        // leaves stale/inconsistent state for the next ExecuteThread entry to read.
        void defer_hook_dispatch(ucontext_t* uctx, const pending_fault_dispatch& dispatch, bool sra_already_spilled)
        {
            this->pending_fault_dispatch_ = dispatch;
            const auto& cfg = this->signal_delegator_->GetConfig();
            const auto target = sra_already_spilled ? cfg.ThreadStopHandlerAddress : cfg.ThreadStopHandlerAddressSpillSRA;
            set_host_pc(uctx, target);
        }

        // True if a host PC lies inside either FEXCore Context's dispatcher trampoline. The dispatcher
        // is host MAP_JIT code (like a CodeBuffer) but IsAddressInCodeBuffer does not recognize it, so
        // the CodeBuffer-gated W^X retries in handle_fault_signal never fire for a dispatcher fault.
        // Both Contexts' dispatchers live in the same MAP_JIT arena and are covered by this thread's
        // single per-thread write-protect bit, so a dispatcher fault from either must be checked.
        bool host_pc_in_any_dispatcher(uint64_t pc) const
        {
            for (const auto* delegator : {this->signal_delegator_.get(), this->signal_delegator32_.get()})
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

        static uint64_t get_host_pc(ucontext_t* context)
        {
#ifdef __APPLE__
            return arm_thread_state64_get_pc(context->uc_mcontext->__ss);
#else
            return context->uc_mcontext.pc;
#endif
        }

        static void set_host_pc(ucontext_t* context, uint64_t pc)
        {
#ifdef __APPLE__
            arm_thread_state64_set_pc_fptr(context->uc_mcontext->__ss, reinterpret_cast<void*>(pc));
#else
            context->uc_mcontext.pc = pc;
#endif
        }

        static uint64_t get_host_gpr(ucontext_t* context, uint32_t index)
        {
            if (index == 31)
            {
                return 0;
            }

#ifdef __APPLE__
            if (index <= 28)
            {
                return context->uc_mcontext->__ss.__x[index];
            }
            return index == 29 ? context->uc_mcontext->__ss.__fp : context->uc_mcontext->__ss.__lr;
#else
            return context->uc_mcontext.regs[index];
#endif
        }

        static void set_host_gpr(ucontext_t* context, uint32_t index, uint64_t value)
        {
            if (index == 31)
            {
                return;
            }

#ifdef __APPLE__
            if (index <= 28)
            {
                context->uc_mcontext->__ss.__x[index] = value;
            }
            else if (index == 29)
            {
                context->uc_mcontext->__ss.__fp = value;
            }
            else
            {
                context->uc_mcontext->__ss.__lr = value;
            }
#else
            context->uc_mcontext.regs[index] = value;
#endif
        }

        static __uint128_t* get_host_vector_registers(ucontext_t* context)
        {
#ifdef __APPLE__
            return reinterpret_cast<__uint128_t*>(&context->uc_mcontext->__ns.__v[0]);
#else
            auto* fpsimd = find_aarch64_context<fpsimd_context>(context, FPSIMD_MAGIC);
            return fpsimd != nullptr ? fpsimd->vregs : nullptr;
#endif
        }

        // FEXCore's call-ret shadow stack (REG_CALLRET_SP == x25, ensure_callret_buffer) is a return-
        // address predictor bracketed by a guard page on each side. A deep guest call chain - or a
        // guest stack pivot / longjmp that abandons already-pushed frames, as steam_api.dll's RLD DRM
        // does - legitimately underflows (or overflows) it past the committed region into a guard page.
        // Upstream FEX treats this as expected and recovers by resetting REG_CALLRET_SP to the buffer's
        // default location: Linux SyscallHandler::HandleSegfault (LinuxSyscalls/SyscallsSMCTracking.cpp)
        // and Windows FEX::Windows::CallRetStack::HandleAccessViolation (Source/Windows/Common/
        // CallRetStack.h, called from WOW64/Module.cpp) do exactly this. sogen's macOS backend set up
        // the buffer and its default location (matching GetCallRetStackInfo) but never ported the guard-
        // page fault recovery, so a guard-page hit fell through to handle_general_memory_violation,
        // which mis-read the host callret-stack address as a bogus guest access violation and crashed
        // marshaling a synthetic exception with it. Classify by shape (fault address inside the active
        // engine's callret allocation, guard pages included) rather than si_code - Darwin reports a
        // PROT_NONE guard-page hit as SEGV_ACCERR/SEGV_MAPERR/BUS_ADRALN interchangeably (see the
        // CodeBuffer-race comments) - and reset x25 to the default location, mirroring GetCallRetStackInfo
        // exactly (Base +- host_page guard, DefaultLocation = Base + CALLRET_STACK_SIZE/4). Strict no-op
        // for any fault outside the callret allocation.
        bool handle_callret_stack_fault(ucontext_t* uctx, uint64_t fault_addr) const
        {
            auto* const thread = this->active_thread();
            if (thread == nullptr || thread->CallRetStackBase == nullptr)
            {
                return false;
            }
            const auto base = reinterpret_cast<uint64_t>(thread->CallRetStackBase);
            const auto host_page = static_cast<uint64_t>(::getpagesize());
            constexpr uint64_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
            if (fault_addr < base - host_page || fault_addr >= base + callret_stack_size + host_page)
            {
                return false;
            }
            set_host_gpr(uctx, 25, base + callret_stack_size / 4);
            return true;
        }

        // Real (non-synthetic) guest memory violations: FEXCore's own vector-14 synthetic #PF
        // (NoExecOp, see handle_fault_signal) is handled separately, but an ordinary guest
        // load/store/instruction-fetch that directly faults - a real Windows PAGE_GUARD page,
        // genuinely unmapped memory, or a Category-3 shadow-table page (page_shadow_apple_'s doc
        // comment: mprotect'd to PROT_NONE because some 4KB guest slot within its host page is
        // guard/unmapped while another slot is legitimately mapped) - has no path to
        // memory_violation_hooks_ otherwise. Consult the shadow table for the *specific* 4KB guest
        // page the fault address falls in: if the requested operation exceeds what's declared there,
        // this is a real violation - classify it and defer_hook_dispatch (mirroring the existing
        // vector-14 branch). Otherwise the access is genuinely legitimate per the shadow (a false
        // fault from a stricter neighbor sharing the host page) - decode-and-emulate it exactly like
        // handle_misaligned_atomic_fault already does for a different fault kind (same technique,
        // reused directly).
        bool handle_general_memory_violation(ucontext_t* uctx, uint64_t fault_addr)
        {
            const uint64_t pc = get_host_pc(uctx);
            const auto guest_fault_addr = this->unrebase_fault_addr(fault_addr);
#ifdef __APPLE__
            // Apple Silicon host pages are 16KB, so the 4KB guest-page shadow is authoritative when
            // neighboring guest pages require different permissions within one host page.
            const auto guest_page = guest_fault_addr & ~(page_size - 1);
            const auto shadow_it = this->page_shadow_apple_.find(guest_page);
            const auto declared = (shadow_it != this->page_shadow_apple_.end()) ? shadow_it->second : memory_permission::none;
#else
            const auto region_it = this->find_region_containing(guest_fault_addr);
            const auto declared = region_it != this->regions_.end() ? region_it->second.permissions : memory_permission::none;
#endif

            memory_operation operation = memory_operation::exec;
            if (fault_addr != pc)
            {
#ifdef __ANDROID__
                // The emulation decoder below is intentionally limited to the ordered STLR-family
                // instructions that can fault for alignment. Ordinary translated STR/STUR stores must
                // still be reported as writes on real protection faults, so use the kernel-provided
                // AArch64 ESR WnR bit for access classification instead of broadening that decoder.
                constexpr uint64_t esr_write_not_read = 1ULL << 6;
                if (const auto* esr = find_aarch64_context<esr_context>(uctx, ESR_MAGIC))
                {
                    operation = (esr->esr & esr_write_not_read) != 0 ? memory_operation::write : memory_operation::read;
                }
                else
#endif
                {
                    const auto insn = *reinterpret_cast<const uint32_t*>(pc);
                    operation = decode_arm64_store(insn) ? memory_operation::write : memory_operation::read;
                }
            }

            if ((declared & operation) == operation)
            {
                return this->handle_misaligned_atomic_fault(uctx, fault_addr);
            }

            const auto type = (declared == memory_permission::none) ? memory_violation_type::unmapped : memory_violation_type::protection;

            // This fault interrupted live guest-translated JIT code at an arbitrary point. FEX's call-ret
            // block-chaining (directly-linked blocks and callret RET fast-paths) advances execution
            // WITHOUT rewriting CurrentFrame->State.rip - it holds whatever was last written to it (e.g. a
            // prior syscall's fallthrough or a gate resume PC), so it is frequently STALE here. The
            // memory-violation hook (and the synthetic exception record it dispatches to the guest) reads
            // State.rip as the faulting instruction pointer, so without this it reports a misleading PC -
            // unlike Unicorn/native, which are instruction-precise and report the true faulting insn.
            // Reconstruct the real guest rip from the live host PC (the same mechanism FEX's own
            // suspend-time ReconstructThreadState and the InterruptFaultPage cooperative-stop path use);
            // the host PC is squarely inside a compiled block here, so this resolves accurately. Guard on
            // a non-zero result so a failed reconstruction never zeroes a usable stale rip.
            //
            // active_thread()/active_context() are not necessarily who actually faulted: a gate crossing
            // can flip which engine is "active" while this thread is still finishing host-side work for
            // the engine it just left (see handle_fault_signal's own InterruptFaultPage check, which hits
            // the identical mismatch). Reconstructing against the wrong context here doesn't fail safely
            // like a bad address lookup would - RestoreRIPFromHostPC silently returns a garbage or zero
            // rip for a pc that isn't in that context's own code buffers, and that garbage rip then gets
            // written into CurrentFrame->State and later resumed into. Pick whichever engine's
            // IsAddressInCodeBuffer actually recognizes pc, not just whichever the rest of the emulator
            // currently considers active.
            auto* thread = this->active_thread();
            auto* context = this->active_context();
            if (this->is_wow64_process_ && this->thread32_ != nullptr && context &&
                !context->IsAddressInCodeBuffer(thread, pc))
            {
                auto* const other_thread = thread == this->thread32_ ? this->thread_ : this->thread32_;
                auto* const other_context = other_thread == this->thread32_ ? this->context32_.get() : this->context_.get();
                if (other_context && other_context->IsAddressInCodeBuffer(other_thread, pc))
                {
                    thread = other_thread;
                    context = other_context;
                }
            }
            if (context != nullptr)
            {
                if (const uint64_t recon_rip = context->RestoreRIPFromHostPC(thread, pc))
                {
                    thread->CurrentFrame->State.rip = recon_rip;
                }
            }

            pending_fault_dispatch dispatch{};
            dispatch.kind = pending_fault_kind::memory_violation;
            dispatch.address = guest_fault_addr;
            dispatch.size = 1;
            dispatch.operation = operation;
            dispatch.type = type;

            // SRA is still live only in host registers here - this fault interrupted guest-translated
            // JIT code at an arbitrary point, not FEXCore's own controlled synthetic-exception path.
            this->defer_hook_dispatch(uctx, dispatch, /*sra_already_spilled=*/false);
            return true;
        }

        bool handle_fault_signal(int sig, siginfo_t* info, void* raw_ucontext)
        {
            if (this->active_thread() == nullptr)
            {
                return false;
            }

            auto* uctx = static_cast<ucontext_t*>(raw_ucontext);

            if (sig == SIGSEGV || sig == SIGBUS)
            {
                const auto fault_addr = reinterpret_cast<uint64_t>(info->si_addr);

                // This check must run first, before any signal/si_code-specific branch below: just
                // like the CodeBuffer race (see the BUS_ADRALN branch's own comment), Darwin can
                // report this exact same PROT_NONE violation as BUS_ADRALN instead of the expected
                // SEGV_ACCERR/SEGV_MAPERR. Since InterruptFaultPage's address is never inside the
                // CodeBuffer, a misclassified BUS_ADRALN fault here would fall past that check
                // straight into handle_general_memory_violation, which unconditionally treats
                // fault_addr as a *guest* address (unrebase_fault_addr/page_shadow_apple_ lookup) and
                // dispatches a synthetic guest memory-violation exception with that bogus "guest
                // address" (really just this backend's own internal heap pointer) - corrupting
                // whatever the resulting nonsense exception dispatch touches downstream. Checking this
                // first, before any signal/si_code-specific branch, means every InterruptFaultPage
                // fault is caught here regardless of how Darwin classifies it.
                // Which thread actually faulted is not necessarily active_thread(): host-side work still
                // in flight for the engine a gate crossing just deactivated (e.g. CompileBlock finishing
                // a compile it started before the crossing) can still touch that engine's own
                // InterruptFaultPage after active_thread_ has already moved on to the other one. Check
                // whichever engine's range the fault address actually falls in, not just whichever the
                // rest of the emulator currently considers "active".
                auto* faulting_thread = this->active_thread();
                auto* context = this->active_context();
#ifdef __ANDROID__
                auto interrupt_page_addr = get_untagged_pointer_address(faulting_thread->InterruptFaultPage);
#else
                auto interrupt_page_addr = reinterpret_cast<uint64_t>(faulting_thread->InterruptFaultPage);
#endif
                if (this->is_wow64_process_ && this->thread32_ != nullptr &&
                    !(fault_addr >= interrupt_page_addr && fault_addr < interrupt_page_addr + sizeof(faulting_thread->InterruptFaultPage)))
                {
                    auto* const other_thread = faulting_thread == this->thread32_ ? this->thread_ : this->thread32_;
#ifdef __ANDROID__
                    const auto other_page_addr = get_untagged_pointer_address(other_thread->InterruptFaultPage);
#else
                    const auto other_page_addr = reinterpret_cast<uint64_t>(other_thread->InterruptFaultPage);
#endif
                    if (fault_addr >= other_page_addr && fault_addr < other_page_addr + sizeof(other_thread->InterruptFaultPage))
                    {
                        faulting_thread = other_thread;
                        context = (other_thread == this->thread32_) ? this->context32_.get() : this->context_.get();
                        interrupt_page_addr = other_page_addr;
                    }
                }

                if (fault_addr >= interrupt_page_addr && fault_addr < interrupt_page_addr + sizeof(faulting_thread->InterruptFaultPage))
                {
                    const auto fault_pc = get_host_pc(uctx);
                    const bool is_dispatch_code = context && context->IsAddressInCodeBuffer(faulting_thread, fault_pc);

                    // ExitFunctionLinkerAddress's OWN epilogue (EmitSignalGuardedRegion's closing
                    // sequence, Dispatcher.cpp) also writes to InterruptFaultPage from inside the
                    // CodeBuffer - via a `strb`, functionally identical to the DeferredSignalRefCount
                    // Guard host-C++ destructor below, just JIT-emitted. IsAddressInCodeBuffer alone
                    // can't tell this apart from a genuine per-block-entry interrupt check
                    // (EmitSuspendInterruptCheck's 64-bit `str`/128-bit vector `str`, JIT.cpp) - both
                    // are "inside the CodeBuffer". Redirecting to ThreadStopHandlerAddress while
                    // actually mid-trampoline-epilogue pops the dispatcher's own frame at the wrong
                    // stack depth. Distinguish via the raw instruction word: STRB (unsigned-offset
                    // immediate) always encodes with size=00,V=0,opc=00 - genuinely distinct from both
                    // of EmitSuspendInterruptCheck's forms (64-bit `str` has size=11; 128-bit vector
                    // `str` has V=1) - so this mask catches only the epilogue's strb, never either
                    // genuine block-entry form.
                    const bool is_strb_epilogue_write = (*reinterpret_cast<const uint32_t*>(fault_pc) & 0xFFC00000u) == 0x39000000u;

                    if (is_dispatch_code && !is_strb_epilogue_write)
                    {
                        // This cooperative stop is taken at a genuine JIT block-entry / loop back-edge
                        // interrupt check (EmitSuspendInterruptCheck, JIT.cpp), triggered by the
                        // quantum-timer thread's async mprotect of InterruptFaultPage. At such a point
                        // the live guest state is in host registers (SRA) and CPUState.rip holds
                        // whatever was last written to it, which is frequently stale: a completed
                        // syscall leaves rip at its fallthrough (HandleSyscall sets rip = syscall+2),
                        // and execution then runs on through directly-linked blocks / callret RET
                        // fast-paths that never rewrite CPUState.rip. Redirecting to the non-spill
                        // ThreadStopHandlerAddress would resume ExecuteThread from that stale rip with
                        // stale registers, re-executing an already-retired instruction - e.g. a `retn`
                        // whose return slot has since been reused by a later call, popping garbage and
                        // producing a wild branch ("NoExec instruction" in the entry block). Reconstructing
                        // the real guest rip from the faulting host PC and redirecting through the
                        // SpillSRA stop handler writes the live SRA GPRs/FPRs/flags back to CPUState
                        // before ExecuteThread returns, mirroring FEX's own suspend-time reconstruction
                        // (Source/Windows/WOW64/Module.cpp ReconstructThreadState, which does exactly
                        // RestoreRIPFromHostPC + SRA spill). Both halves are required: without the rip
                        // reconstruction resume lands on the stale instruction; without the SRA spill it
                        // resumes with stale registers.
                        faulting_thread->CurrentFrame->State.rip = context->RestoreRIPFromHostPC(faulting_thread, fault_pc);
                        this->interrupt_page_unwind_ = true;
                        const auto& stop_cfg = this->signal_delegator_->GetConfig();
                        set_host_pc(uctx, stop_cfg.ThreadStopHandlerAddressSpillSRA);
                        return true;
                    }

                    // Not a genuine block-entry check - either FEXCore's own
                    // DeferredSignalRefCountGuard destructor (SignalScopeGuards.h, host C++ code) or
                    // ExitFunctionLinkerAddress's own JIT-emitted epilogue strb (both write to this
                    // same page as ordinary bookkeeping, coincidentally racing with a stop request
                    // from another thread, e.g. the quantum timer, that just mprotect'd the page).
                    // Redirecting to ThreadStopHandlerAddress here would be wrong in either case:
                    // that entry point expects to unwind a live JIT dispatcher stack frame, not
                    // whatever is actually executing at the moment of the race, and doing so from the
                    // host-C++-side DeferredSignalRefCountGuard destructor corrupts the stack,
                    // producing a pc==lr==0 crash.
                    // The store's actual value is inconsequential - only the page's protection state
                    // drives the cooperative-stop mechanism - so just skip the single faulting store
                    // instruction; the next real JIT block entry will still see the page protected
                    // and stop correctly.
                    set_host_pc(uctx, fault_pc + 4);
                    return true;
                }

                // A W^X (write-XOR-execute) instruction-fetch fault on FEXCore's own dispatcher
                // trampoline. The dispatcher is host MAP_JIT code, just like a CodeBuffer, but
                // IsAddressInCodeBuffer does not recognize it, so the CodeBuffer-gated W^X retries
                // below never fire for it. This only surfaces once a second FEXCore Context exists (the
                // 32-bit wow64 context32_): its dispatcher/blocks are lazily compiled from inside the
                // gate-crossing signal handler, which can leave this thread's per-thread JIT write-
                // protect in write mode, so re-entering either Context's dispatcher then faults on the
                // instruction fetch (fault address == pc). Darwin reports this as SIGSEGV or SIGBUS with
                // any of SEGV_ACCERR/SEGV_MAPERR/BUS_ADRALN (see the CodeBuffer-race comments below for
                // the same si_code ambiguity), so classify by shape - an instruction fetch (fault_addr
                // == pc) inside a known dispatcher range - rather than by si_code, and toggle execute
                // mode and retry the identical instruction.
                //
                // This is deliberately NOT bounded by the per-address retry budget the CodeBuffer cases
                // use. A host pc inside the dispatcher is unambiguously FEXCore's own code executing
                // (never a wild guest branch - guest addresses rebase elsewhere), and the dispatcher is
                // genuine RWX-capable MAP_JIT, so toggling execute mode ALWAYS lets the fetch succeed and
                // execution proceeds - it can never spin. A WoW64 syscall round-trip compiles many blocks
                // back-to-back, each leaving the thread in write mode, so the very same dispatcher entry
                // legitimately faults far more than a handful of times in rapid succession (never letting
                // the 100ms reset window fire); a small budget spuriously exhausted here, dropping the
                // fault through to handle_general_memory_violation which mis-read the host dispatcher
                // address as a bogus guest access violation and crashed marshaling it to the guest stack.
#ifdef __APPLE__
                {
                    const auto host_pc = get_host_pc(uctx);
                    if (fault_addr == host_pc && this->host_pc_in_any_dispatcher(host_pc))
                    {
                        ::pthread_jit_write_protect_np(1);
                        return true;
                    }
                }
#endif

                // A call-ret shadow-stack guard-page hit (underflow/overflow) - reset REG_CALLRET_SP to
                // the buffer's default location and resume, exactly as upstream FEX does. Checked early,
                // by shape, before the general-violation routing that would otherwise mis-dispatch this
                // host arena address as a bogus guest access violation (see the helper's doc comment).
                if (this->handle_callret_stack_fault(uctx, fault_addr))
                {
                    return true;
                }

                // An instruction-fetch fault reports si_addr == the faulting pc itself (a real MMIO
                // data access from a mapped mmio_region's guest address never coincides with a live
                // code address, so this is never a false negative for a genuine MMIO hit). Excluding
                // it here matters: if the underlying root cause is a bad branch to a garbage/null pc
                // (root-caused elsewhere, not by this backend's fault handling), that garbage address
                // can coincidentally fall inside some registered mmio_region's range purely by chance -
                // routing it into handle_mmio_fault would then try to decode "the instruction at pc"
                // from that same garbage/unmapped address and crash again there instead, which is a
                // confusing secondary symptom of the real bug, not a new one. Let it fall through to
                // the ordinary unhandled-signal report untouched.
                const auto pc_for_mmio_check = get_host_pc(uctx);
                if (fault_addr != pc_for_mmio_check)
                {
                    // mmio_regions_ is keyed by guest address (see unrebase_fault_addr's doc
                    // comment) - a 32-bit guest's real host access lands at guest_addr +
                    // wow64_guest_rebase, so it must be un-rebased before matching here.
                    const auto guest_fault_addr = this->unrebase_fault_addr(fault_addr);
                    for (const auto& region : this->mmio_regions_)
                    {
                        if (guest_fault_addr >= region.address && guest_fault_addr < region.address + region.size)
                        {
                            return this->handle_mmio_fault(uctx, region, guest_fault_addr);
                        }
                    }
                }

                // A BUS_ADRALN fault whose *address* falls inside the live JIT CodeBuffer, but whose
                // *PC* is FEXCore's own host C++ code (e.g. ExitFunctionLink's self-modifying-write
                // path), decodes to a perfectly ordinary, 4-byte-aligned 32-bit `str` (e.g. 0xb900032a
                // = `str w10, [x25]`, no offset, size=32-bit, not an exclusive/ordered form at all) -
                // i.e. this is NOT a real alignment fault (plain STR never requires alignment on
                // ARM64, and this address is aligned anyway). This is the JIT write-XOR-execute race -
                // the CodeBuffer is currently execute-only - which Darwin sometimes reports via
                // BUS_ADRALN instead of the expected SEGV_ACCERR/SEGV_MAPERR, mirroring the
                // already-documented SEGV_MAPERR-instead-of-SEGV_ACCERR quirk for the exact same
                // underlying mechanism (see the JITGuardPage comment below). Falling through to the
                // BUS_ADRALN branch further down would route this to handle_general_memory_violation -
                // designed for genuine guest memory accesses, it unwinds via
                // defer_hook_dispatch/ThreadStopHandlerAddress as if interrupting the JIT dispatcher's
                // own call frame. That's wrong here: execution is several real C++ call frames deep
                // inside FEXCore's own code (dispatcher trampoline -> embedder wrapper ->
                // ExitFunctionLink), so popping "the dispatcher's" callee-saved registers off the stack
                // pops whatever's actually there instead - corrupting STATE (x28) and other SRA
                // registers with stack garbage, which then crashes on the *next* unlinked call with an
                // unrelated-looking null-Frame dereference.
                //
                // Treat it exactly like the SEGV_ACCERR/SEGV_MAPERR write-protect race just below
                // - toggle write access on and retry the identical instruction, bounded by the same
                // per-address retry counter (a genuinely different bug at this exact address, rather
                // than an unresolvable race, would still eventually surface as unhandled after
                // max_write_protect_retries, not spin forever).
                //
                // BUS_ADRALN is Darwin's own alignment-fault si_code; Android/Linux never reports it
                // this way, so both this and the general BUS_ADRALN dispatch below are Apple-only.
#ifdef __APPLE__
                auto* const codebuffer_context = this->active_context();
                if (sig == SIGBUS && info->si_code == BUS_ADRALN && codebuffer_context &&
                    codebuffer_context->IsAddressInCodeBuffer(faulting_thread, fault_addr))
                {
                    auto& retry_count = jit_write_protect_retry_count_for(fault_addr);
                    constexpr int max_write_protect_retries = 4;
                    if (retry_count < max_write_protect_retries)
                    {
                        ++retry_count;
                        ::pthread_jit_write_protect_np(0);
                        return true;
                    }
                }

                // See handle_misaligned_atomic_fault's doc comment. Routed through
                // handle_general_memory_violation rather than calling handle_misaligned_atomic_fault
                // directly: that doc comment's "otherwise legitimately mapped memory" assumption isn't
                // actually guaranteed - a guest instruction can compute a genuinely garbage/unmapped
                // address (a real access violation that merely happens to also be unaligned), and
                // blindly memcpy-ing to/from it would fault a second time inside the signal handler
                // itself, surfacing as an unhandled crash instead of a normal guest exception. Going
                // through the shadow-table-validated path first means a genuinely bad address gets
                // correctly classified and raised via memory_violation_hooks_ instead. The CodeBuffer
                // case above is handled first and returns early, so by this point fault_addr is known
                // not to be a CodeBuffer address - this is a genuine guest-memory BUS_ADRALN.
                if (sig == SIGBUS && info->si_code == BUS_ADRALN && this->handle_general_memory_violation(uctx, fault_addr))
                {
                    return true;
                }
#endif
            }

            // JIT code-buffer overflow guard: FEXCore protects the last host page of each CodeBuffer
            // (CPUBackend.cpp's CodeBuffer constructor) and deliberately writes into it mid-compile to
            // detect running out of space. Not a real bug - resume via the jump-buffer FEXCore already
            // set up before compiling started (mirrors SignalDelegator.cpp's
            // HandleFrontendSIGSEGV/ManuallyLoadJumpBuf). Darwin can report this access violation as
            // either SIGSEGV or SIGBUS depending on the exact protection-fault kind, unlike Linux's
            // single SIGSEGV, so both are checked here. Empirically, Darwin also sometimes reports
            // this exact MAP_JIT write-XOR-execute violation as SEGV_MAPERR (si_code=1, normally
            // "not mapped at all") rather than SEGV_ACCERR (si_code=2, normally "mapped, wrong
            // permission") - confirmed by querying mach_vm_region for the fault address from within
            // this handler and finding it fully mapped RWX (region_prot=7) despite the si_code=1
            // report, so both si_codes are treated the same way below.
#ifdef __APPLE__
            if ((sig == SIGSEGV || sig == SIGBUS) && (info->si_code == SEGV_ACCERR || info->si_code == SEGV_MAPERR))
#else
            if (sig == SIGSEGV || sig == SIGBUS)
#endif
            {
                auto* const faulting_thread = this->active_thread();
                const auto guard_page = faulting_thread->JITGuardPage;
                const auto fault_addr = reinterpret_cast<uintptr_t>(info->si_addr);
                if (guard_page != 0 && fault_addr >= guard_page && fault_addr < guard_page + FEXCore::Utils::FEX_HOST_PAGE_SIZE)
                {
#ifdef __APPLE__
                    auto* gprs = reinterpret_cast<uint64_t*>(&uctx->uc_mcontext->__ss);
                    auto* pc_ptr = reinterpret_cast<uint64_t*>(&uctx->uc_mcontext->__ss.__pc);
#else
                    auto* gprs = reinterpret_cast<uint64_t*>(uctx->uc_mcontext.regs);
                    auto* pc_ptr = reinterpret_cast<uint64_t*>(&uctx->uc_mcontext.pc);
#endif
                    auto* fprs = get_host_vector_registers(uctx);
                    if (fprs == nullptr)
                    {
                        return false;
                    }
                    FEXCore::UncheckedLongJump::ManuallyLoadJumpBuf(faulting_thread->RestartJump, faulting_thread->JITGuardOverflowArgument,
                                                                    gprs, fprs, pc_ptr);
                    return true;
                }

#ifdef __APPLE__
                // See jit_write_protect_retry_count_for's doc comment: FEXCore's own code-patching paths
                // (ExitFunctionLink, block delinkers) don't reliably leave this thread's JIT write-
                // protect state correct for the duration of their self-modifying writes into a
                // CodeBuffer. Set it to whatever the faulting access actually needs and retry the
                // exact same faulting instruction (PC/registers otherwise untouched) rather than
                // treating this as fatal - bounded per fault address so a genuinely different bug
                // can't spin forever. An instruction-fetch fault (PC == fault address) needs execute
                // mode (1); a data write needs write mode (0) - guessing the wrong direction here
                // would just re-fault immediately and consume a retry harmlessly.
                //
                // Gated on IsAddressInCodeBuffer(fault_addr): without this gate, the branch would fire
                // for *any* SEGV_ACCERR/SEGV_MAPERR regardless of the fault address, wasting up to
                // max_write_protect_retries toggling W^X for faults that are never a CodeBuffer
                // write-protect race at all - e.g. a genuine branch-to-null (pc==fault_addr==0) would
                // be retried this way before falling through as unhandled, even though toggling JIT
                // write-protection has nothing to do with a null pointer.
                auto* const codebuffer_context = this->active_context();
                if (codebuffer_context && codebuffer_context->IsAddressInCodeBuffer(faulting_thread, fault_addr))
                {
                    const auto fault_addr_u64 = reinterpret_cast<uint64_t>(info->si_addr);
                    auto& retry_count = jit_write_protect_retry_count_for(fault_addr_u64);
                    constexpr int max_write_protect_retries = 4;
                    if (retry_count < max_write_protect_retries)
                    {
                        ++retry_count;
                        const uint64_t faulting_pc = get_host_pc(uctx);
                        const bool is_instruction_fetch = (faulting_pc == fault_addr_u64);
                        ::pthread_jit_write_protect_np(is_instruction_fetch ? 1 : 0);
                        return true;
                    }
                }
#endif
            }

            const uint64_t pc = get_host_pc(uctx);

            // FEXCore's guest-exception trampoline always re-enters within a dispatcher range. In a
            // WoW64 process there are TWO dispatchers (the 64-bit context_ and the 32-bit context32_),
            // and a synthetic #PF raised by 32-bit guest code (e.g. the reverse WoW64SVC gate's NoExecOp
            // BreakOp) re-enters the 32-bit dispatcher - which lies OUTSIDE the 64-bit delegator's
            // [DispatcherBegin,DispatcherEnd). Checking only the 64-bit range here rejected every
            // 32-bit-originated synthetic fault as "not the trampoline", so the reverse gate crossing
            // was never performed. Accept a re-entry into EITHER dispatcher.
            if (!this->host_pc_in_any_dispatcher(pc))
            {
                // Not FEXCore's own guest-exception trampoline (that always re-enters within the
                // dispatcher range). The common case: real, translated guest code faulted directly -
                // see handle_general_memory_violation. Only attempt that once we've confirmed pc is
                // genuinely inside a live JIT code buffer (the public IsAddressInCodeBuffer API) -
                // otherwise this is a real host bug elsewhere that we have no business trying to
                // interpret as guest state; the signal handler wrapper below logs and re-raises it.
                auto* const context = this->active_context();
                if ((sig == SIGSEGV || sig == SIGBUS) && context && context->IsAddressInCodeBuffer(this->active_thread(), pc) &&
                    this->handle_general_memory_violation(uctx, reinterpret_cast<uint64_t>(info->si_addr)))
                {
                    return true;
                }

                return false;
            }

            auto* frame = this->active_thread()->CurrentFrame;
            if (!frame->SynchronousFaultData.FaultToTopAndGeneratedException)
            {
                return false;
            }

            // FEXCore's IR "Break" op raises this both for x86 conditions with a compile-time-known
            // trap vector (HLT/UD2/INT3/INT1/INTO/unhandled INT N) and for its own synthetic #PF
            // (X86_TRAPNO_PF, e.g. NoExecOp when QueryGuestExecutableRange reports an address isn't
            // executable). Vector 14 is therefore a real memory-access-violation-shaped event and
            // needs the fault address; everything else is a plain CPU exception vector. Mirrors the
            // KVM backend's #PF vs. other-vector split in handle_exception() (kvm_x86_64_emulator.cpp).
            auto vector = static_cast<int>(frame->SynchronousFaultData.TrapNo);

            // sogen has no guest IDT (this is a user-mode-only emulator - there's no kernel to
            // populate one), so a guest `INT N` FEXCore can't dispatch directly synthesizes a real
            // #GP(13) whose error code names the referenced IDT selector (bit1 set, selector index in
            // bits[15:3]) - the same effect real hardware produces for an unprivileged/absent IDT gate.
            // Unicorn/KVM don't model IDT lookups at all and report `INT N` as vector N directly, so
            // remap FEX's more architecturally faithful #GP back to the plain vector windows_emulator.cpp's
            // shared interrupt dispatch already expects - otherwise e.g. a CFG/__fastfail `int 0x29`
            // re-faults on the same instruction forever instead of reaching fast-fail dispatch.
            constexpr int gp_fault_vector = 13;
            constexpr uint32_t idt_reference_bit = 0x2;
            if (vector == gp_fault_vector && (frame->SynchronousFaultData.err_code & idt_reference_bit) != 0)
            {
                vector = static_cast<int>(frame->SynchronousFaultData.err_code >> 3);
            }

            // Must be reset before deferring the hook dispatch (not after) - it gates re-entry into
            // this branch (see the check above), and the hook may not actually run until start() gets
            // around to it in normal context; the next real fault of this shape must not be swallowed
            // in the meantime.
            frame->SynchronousFaultData.FaultToTopAndGeneratedException = false;

            pending_fault_dispatch dispatch{};
            if (vector == 14)
            {
                // A registered WoW64 gate crossing surfaces here as a synthetic #PF (its range is
                // reported non-executable by QueryGuestExecutableRange, so FEXCore refuses to compile
                // the mode-switch bytes and Break-ops instead). Perform the actual bitness switch
                // rather than dispatching a memory violation.
                if (const auto* gate = this->find_gate_crossing(frame->State.rip))
                {
                    // Capture the source (currently-active, pre-crossing) engine's dispatcher stop
                    // handler *before* perform_gate_crossing flips active_context_: the in-flight
                    // ExecuteThread that must unwind belongs to the source Context, so it has to
                    // return through that Context's own ThreadStopHandlerAddress. Using the
                    // destination's would re-enter the wrong dispatcher.
                    auto* const source_signal_delegator = (this->active_context() == this->context32_.get())
                                                              ? this->signal_delegator32_.get()
                                                              : this->signal_delegator_.get();

                    if (this->perform_gate_crossing(*gate))
                    {
                        this->pending_fault_dispatch_.kind = pending_fault_kind::gate_crossing;
                        // SRA is already spilled here (FEXCore's controlled synthetic-#PF/Break-op
                        // path), so use the plain ThreadStopHandlerAddress - same rationale as the
                        // sra_already_spilled=true memory-violation dispatch below.
                        const auto& stop_cfg = source_signal_delegator->GetConfig();
                        set_host_pc(uctx, stop_cfg.ThreadStopHandlerAddress);
                        return true;
                    }
                    // A gate handler that returns false (a genuine memory-read failure while
                    // marshaling) falls through to the ordinary memory-violation dispatch rather than
                    // resuming into half-marshaled register state.
                }

                const auto err_code = frame->SynchronousFaultData.err_code;
                // NoExecOp (the only current producer of a synthetic #PF) always faults on the
                // instruction fetch at the current guest RIP - there is no separate stored fault
                // address (real x86 would use CR2), so RIP is the only correct source for now.
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

            // See defer_hook_dispatch's doc comment: the hook runs later, in normal call context, once
            // start() dispatches it - it may call this->stop() synchronously (e.g. the fast-fail path),
            // which start()'s loop checks for after dispatching, matching this function's old behavior
            // of redirecting into ThreadStopHandlerAddress instead of resuming when that happens. SRA
            // is already spilled here (this is FEXCore's own controlled synthetic-exception/Break-op
            // path, not an arbitrary interruption of live JIT code), matching this function's own old
            // (pre-hook-deferral) comment justifying the non-spilling ThreadStopHandlerAddress variant.
            this->defer_hook_dispatch(uctx, dispatch, /*sra_already_spilled=*/true);
            return true;
        }
#endif
      private:
        void create_thread()
        {
            // Seed the FEX thread from the staged CPUState the loader populated before the first start().
            this->thread_ =
                this->context_->CreateThread(this->staged_state_.rip, this->staged_state_.gregs[detail::greg_rsp], &this->staged_state_);
            // active_context_/active_thread_ start out equal to context_/thread_ - see their doc
            // comment - execution always begins on the 64-bit engine, so reflect the newly-created
            // thread as the active one right away.
            this->active_thread_.store(this->thread_, std::memory_order_release);

            // FEXCore's core does not set up the call-ret shadow stack; on Linux that is embedder glue
            // in ThreadManager::CreateThread, replicated here. Without it the first x86 CALL in compiled
            // code dereferences a null callret_sp and crashes.
            this->ensure_callret_stack(this->thread_, this->thread_->CurrentFrame->State);

#ifdef __APPLE__
            // See exit_function_link_jit_write_wrapper: the call-site patch must happen with this
            // thread's JIT write-protection disabled.
            g_original_exit_function_link = this->thread_->CurrentFrame->Pointers.ExitFunctionLink;
            this->thread_->CurrentFrame->Pointers.ExitFunctionLink = reinterpret_cast<uint64_t>(&exit_function_link_jit_write_wrapper);
#endif

            // Build thread32_ here too, in this ordinary call context, rather than leaving it to be
            // lazily created on the process's first gate crossing - that crossing is only ever
            // reached from inside handle_fault_signal (a signal handler), and create_thread32()
            // does real heap allocation (FEXCore::Context::CreateThread, SignalDelegator
            // construction, InitCore()). By the time create_thread() runs (called from start(),
            // never from a signal handler), is_wow64_process_ and gdt_base_ are both already set
            // (notify_process_bitness() and load_gdt() both run before start()), so there's nothing
            // create_thread32() needs that isn't ready yet.
            if (this->is_wow64_process_ && this->thread32_ == nullptr)
            {
                this->create_thread32();
            }
        }

        // FEXCore's call-ret shadow stack (callret_sp, see CoreState.h) has no notion of "logical
        // guest thread" - it's just a raw pointer into whatever host buffer this sets up. sogen models
        // multiple logical guest threads as CPUState-sized snapshots swapped in and out of this one
        // FEXCore thread (see save_registers/restore_registers); if every logical thread's callret_sp
        // pointed at the same buffer, a thread suspended mid-call-chain (e.g. blocked in a syscall,
        // with pending pushed return addresses) would have those frames corrupted the moment a
        // different logical thread starts pushing its own calls from the same default position. Give
        // each logical thread its own private buffer instead, identified by state._pad1 (otherwise-
        // unused CPUState padding immediately after callret_sp) doubling as a marker: 0 means this
        // exact snapshot has never been assigned one (true for the very first snapshot any logical
        // thread starts from - captured before any thread/buffer existed - and for a thread that
        // hasn't made its first CALL yet), non-zero is that buffer's base pointer, safe to trust and
        // reuse verbatim since it round-trips with the rest of this logical thread's own snapshot
        // (save_registers/restore_registers memcpy the whole CPUState, _pad1 included). Also keeps
        // Thread->CallRetStackBase in sync - FEXCore's own code-invalidation path
        // (Core.cpp/JIT.cpp's `Allocator::VirtualDontNeed(Thread->CallRetStackBase, ...)`) resets
        // whatever buffer that field currently names, so it must always point at the logical thread
        // that's actually active right now.
        // Allocates this logical thread's private call-ret shadow-stack buffer on first use (state._pad1
        // == 0), recording it in state._pad1 (round-tripped by save/restore). Does NOT touch any
        // InternalThreadState::CallRetStackBase - callers point the right engine's field at the buffer
        // themselves (ensure_callret_stack for a named engine; restore_state_into per restored engine).
        void ensure_callret_buffer(FEXCore::Core::InternalThreadState* thread, FEXCore::Core::CPUState& state)
        {
            if (state._pad1 == 0)
            {
                // Guard pages on both sides, sized to the real host page (getpagesize(), not the
                // guest's fixed 4KB) so mprotect can't spill onto the guard.
                const size_t host_page = static_cast<size_t>(::getpagesize());
                constexpr size_t callret_stack_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
                const size_t callret_alloc_size = callret_stack_size + 2 * host_page;

                // Routed through FEXCore::Allocator::mmap rather than raw ::mmap so that on Apple it
                // hits the fex_internal_arena hook and lands inside the guest-excluded arena. The JIT
                // consumes callret_sp as a plain host pointer, so a buffer aliasing the live guest stack
                // would let a host-side push/pop scribble guest memory with no guest instruction
                // involved. On Linux the hook defaults to raw ::mmap, so behavior there is unchanged.
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

                state._pad1 = reinterpret_cast<uint64_t>(callret_stack_base);
                // Leave headroom for underflows without hitting the guard page immediately, matching
                // ThreadManager::GetCallRetStackInfo's DefaultLocation (Base + size/4).
                state.callret_sp = reinterpret_cast<uint64_t>(callret_stack_base) + callret_stack_size / 4;

                this->callret_buffers_.emplace(state._pad1, callret_buffer_record{
                                                                .allocation_base = alloc_base,
                                                                .allocation_size = callret_alloc_size,
                                                                .code_buffer_generation = thread->CodeBufferGeneration,
                                                            });
            }
        }

        // Ensures the given engine's call-ret buffer exists and points its CallRetStackBase at it.
        void ensure_callret_stack(FEXCore::Core::InternalThreadState* thread, FEXCore::Core::CPUState& state)
        {
            this->ensure_callret_buffer(thread, state);
            thread->CallRetStackBase = reinterpret_cast<void*>(state._pad1);
        }

        // CPUState is owned by the thread frame once a thread exists. Before the thread is created we
        // stage register accesses in a local CPUState so the Windows loader can set up the initial
        // context; create_thread() seeds the real thread from it.
        FEXCore::Core::CPUState& cpu_state()
        {
            auto* const thread = this->active_thread();
            if (thread != nullptr)
            {
                return thread->CurrentFrame->State; // TODO(fex): confirm field path for the FEX version.
            }
            return this->staged_state_;
        }

        const FEXCore::Core::CPUState& cpu_state() const
        {
            auto* const thread = this->active_thread();
            if (thread != nullptr)
            {
                return thread->CurrentFrame->State;
            }
            return this->staged_state_;
        }

        uint64_t read_rflags() const
        {
            // FEXCore's ReconstructCompactedEFLAGS requires a live thread (it dereferences Thread to
            // reach CurrentFrame->State); before create_thread(), fall back to the local
            // reimplementation operating on the staged CPUState directly (see reconstruct_compacted_eflags).
            auto* const thread = this->active_thread();
            if (thread != nullptr)
            {
                // At rest (not in JIT) WasInJIT=false and the host GPR/PSTATE inputs are unused.
                return this->active_context()->ReconstructCompactedEFLAGS(thread, /*WasInJIT=*/false, nullptr, 0);
            }
            return reconstruct_compacted_eflags(this->staged_state_);
        }

        void write_rflags(uint64_t rflags)
        {
            auto* const thread = this->active_thread();
            if (thread != nullptr)
            {
                this->active_context()->SetFlagsFromCompactedEFLAGS(thread, static_cast<uint32_t>(rflags));
                return;
            }
            set_flags_from_compacted_eflags(this->staged_state_, static_cast<uint32_t>(rflags));
        }

        uint16_t segment_selector(int index) const
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

        void set_segment_selector(int index, const void* value, size_t size)
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

        void mark_executable_range(uint64_t address, size_t size, memory_permission permissions)
        {
            auto* const thread = this->active_thread();
            if (thread != nullptr && (permissions & memory_permission::exec) != memory_permission::none)
            {
                this->syscall_handler_->MarkGuestExecutableRange(thread, address, size);
            }
        }

        void invalidate_code_range_in(FEXCore::Context::Context* context, FEXCore::Core::InternalThreadState* thread, uint64_t address,
                                      size_t size) const
        {
            if (context == nullptr)
            {
                return;
            }

            // InvalidateCodeBuffersCodeRange/InvalidateThreadCachedCodeRange both require the caller
            // to already hold GetCodeInvalidationMutex() exclusively (see FEXCore's own
            // ThreadManager::InvalidateGuestCodeRange, the canonical caller on Linux) - without it,
            // CompileBlock's shared lock deadlocks permanently the first time this runs.
            std::unique_lock lock(context->GetCodeInvalidationMutex());

#ifdef __APPLE__
            // Invalidating a range can synchronously delink already-linked call sites (AddBlockLink's
            // delinker callbacks), writing into a MAP_JIT buffer - the same per-thread write-protect
            // requirement as exit_function_link_jit_write_wrapper.
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

        // include_inactive_contexts additionally drops the range from the *other* (currently-inactive)
        // FEXCore context's translation cache. This is needed only when guest code is actually removed
        // from an address (an unmap), not on ordinary protection changes: see the WoW64 note below.
        void invalidate_code_range(uint64_t address, size_t size, bool include_inactive_contexts = false) const
        {
            auto* const context = this->active_context();
            if (!context)
            {
                return;
            }

            // Invalidate the currently-active context exactly as before.
            this->invalidate_code_range_in(context, this->active_thread(), address, size);

            // A WoW64 process runs two independent FEXCore contexts - context_ (64-bit) and context32_
            // (32-bit) - each with its own translation cache and code buffers. An unmap of 32-bit guest
            // code is serviced while active_context_ is the 64-bit context (a 32-bit guest syscall
            // crosses through the heaven's gate to 64-bit mode before reaching sogen's syscall handler),
            // so invalidating only active_context_ leaves stale *32-bit* translations behind. If a new
            // module is later mapped at the same base, FEX runs the phantom translation of the old
            // module's bytes instead of recompiling the new ones - so an unmap must invalidate the
            // inactive context's cache for the range too.
            // Only unmaps request this - doing it on every protection change would repeatedly
            // delink the live 32-bit context's blocks from the inactive side and livelock it.
            if (include_inactive_contexts && this->context32_.get() != nullptr && this->context32_.get() != context &&
                this->thread32_ != nullptr)
            {
                this->invalidate_code_range_in(this->context32_.get(), this->thread32_, address, size);
            }
        }

        void request_thread_stop()
        {
            // Forces the in-flight ExecuteThread to return, whether called from the same thread
            // (synchronously, e.g. from within a syscall hook) or a different one (e.g. a quantum
            // timer thread). FEXCore's JIT emits a `str zr, [InterruptFaultPage]` at every translated
            // block's entry when Config.NeedsPendingInterruptFaultCheck is set (see
            // initialize_context's CONFIG_GDBSERVER comment) - protecting that page makes the next
            // block entry fault, landing in handle_fault_signal, which redirects any fault on
            // InterruptFaultPage into FEXCore's own ThreadStopHandlerAddress instead of resuming
            // (it does not consult stop_requested_ for that).
            //
            // This is the only reader of active_thread_ that can run on a thread other than the one
            // that writes it, so it is also the only one that must load it exactly once: the emulation
            // thread can perform a gate crossing between a null check and a dereference, which would
            // protect the page of an engine that is no longer the one about to re-enter the JIT.
            // Protecting the engine that was active when the stop was requested is the intended
            // behaviour; protecting a torn mix of the two is not. Acquire pairs with the release
            // stores so the InternalThreadState this names is fully constructed as seen from here.
            auto* const thread = this->active_thread_.load(std::memory_order_acquire);
            if (thread == nullptr)
            {
                return;
            }

            ::mprotect(thread->InterruptFaultPage, sizeof(thread->InterruptFaultPage), PROT_NONE);
        }

        emulator_hook* make_hook()
        {
            return reinterpret_cast<emulator_hook*>(this->next_hook_id_++);
        }

        // state

        // The always-64-bit FEXCore::Context - see notify_process_bitness's doc comment.
        fextl::unique_ptr<FEXCore::Context::Context> context_{};
        FEXCore::Core::InternalThreadState* thread_ = nullptr;

        // Whichever context/thread is *currently executing* - starts out equal to context_/thread_
        // (execution always begins on the 64-bit engine) and is flipped by the gate crossings; it is
        // what every JIT-operation call site below actually uses. context_/thread_ and context32_/
        // thread32_ (declared further below) are the two fixed, named instances;
        // active_context_/active_thread_ is which *one* of them is live right now.
        //
        // Atomic for two independent reasons. The gate crossings write them from inside
        // handle_fault_signal, a real kernel-delivered signal handler, and the C++ abstract machine has
        // no control-flow edge for that: a plain member could be cached across the opaque
        // ExecuteThread() call, so start()'s resume loop would re-enter the pre-crossing engine (the
        // same hazard interrupt_page_unwind_ documents). Separately, request_thread_stop() reads
        // active_thread_ from the quantum-timer thread while this thread runs guest code with the
        // kernel lock released (see is_stop_thread_safe), which is a genuine cross-thread access.
        //
        // Every write happens on the emulation thread, so the emulation thread's own reads need
        // atomicity but not ordering and use memory_order_relaxed (the accessors below). The stores are
        // memory_order_release and request_thread_stop's single load is memory_order_acquire, so that
        // one cross-thread reader also sees the InternalThreadState the pointer names fully constructed.
        std::atomic<FEXCore::Context::Context*> active_context_{nullptr};
        std::atomic<FEXCore::Core::InternalThreadState*> active_thread_{nullptr};

        FEXCore::Context::Context* active_context() const
        {
            return this->active_context_.load(std::memory_order_relaxed);
        }

        FEXCore::Core::InternalThreadState* active_thread() const
        {
            return this->active_thread_.load(std::memory_order_relaxed);
        }

        // Set once via notify_process_bitness(), before any thread is created (see that override's
        // doc comment) - gates every guest-memory-touching method's wow64_guest_rebase application
        // below (needed for the 32-bit executable/ntdll32 modules regardless of which FEXCore::Context
        // is currently executing) and whether ensure_context32() builds context32_ at all. Does not
        // select context_'s own bitness - context_ is always the 64-bit Context.
        bool is_wow64_process_ = false;
        // The actual host address offset added to sub-4GB guest addresses (see rebase_for). Starts
        // at wow64_guest_rebase_default and, on Apple, is overwritten by reserve_wow64_host_window()
        // once it finds a genuinely free candidate window - see that method's doc comment. Stays at
        // the default on every other platform (this backend is shared, not Apple-exclusive), which
        // is also FEXCore::Context::Config.Wow64GuestRebaseValue's own default, so both sides agree
        // without sogen ever needing to call SetWow64GuestRebaseValue there at all.
        uint64_t wow64_guest_rebase_ = wow64_guest_rebase_default;
#ifdef __APPLE__
        // Set by reserve_wow64_host_window() iff it actually claimed [wow64_guest_rebase_,
        // wow64_guest_rebase_ + wow64_guest_address_space_size) up front - see its doc comment. Gates
        // the skip checks in reserved_host_ranges()/reserved_host_ranges_in() below; if the
        // reservation attempt was skipped or failed (logged loudly either way), this stays false and
        // both functions behave exactly as before - detect-and-report, not silently assume-safe.
        // Apple-only: reserve_wow64_host_window() and its two call sites below are both
        // __APPLE__-gated (the whole mechanism is a macOS/mach_vm_region-specific fix), so this
        // member is unused - and -Werror,-Wunused-private-field - on other platforms without this.
        bool wow64_host_window_reserved_ = false;
#endif
        // wow64cpu.dll's real TurboDispatchJumpAddressEnd export address, set via
        // set_wow64_turbo_dispatch_end once module_manager resolves it - see that method's doc
        // comment for why this can't just be a fixed offset from the image base. Stays 0 until
        // then; enter_wow64_64bit_from_wow64svc_thunk falls back to the old (best-effort) fixed-
        // offset computation if it's never been set, rather than crashing outright.
        uint64_t wow64_turbo_dispatch_end_ = 0;

        void set_wow64_turbo_dispatch_end(pointer_type address) override
        {
            this->wow64_turbo_dispatch_end_ = address;
        }

        std::unique_ptr<fex_syscall_handler> syscall_handler_{};
        // Does no fault handling; the plain base only carries the dispatcher config
        // (ThreadStopHandlerAddress, DispatcherBegin/End) that handle_fault_signal reads.
        std::unique_ptr<FEXCore::SignalDelegator> signal_delegator_{};
        FEXCore::Core::CPUState staged_state_{};

        // The second, 32-bit FEXCore::Context a wow64 process needs (see ensure_context32's doc
        // comment); thread32_ is built eagerly in create_thread() - see create_thread32's doc comment.
        fextl::unique_ptr<FEXCore::Context::Context> context32_{};
        FEXCore::Core::InternalThreadState* thread32_ = nullptr;
        std::unique_ptr<fex_syscall_handler> syscall_handler32_{};
        std::unique_ptr<FEXCore::SignalDelegator> signal_delegator32_{};

        uint64_t gdt_base_ = 0;
        uint32_t gdt_limit_ = 0;

        std::atomic<bool> stop_requested_{false};
        uintptr_t next_hook_id_ = 1;

#if defined(__APPLE__) || defined(__ANDROID__)
        pending_fault_dispatch pending_fault_dispatch_{};
        // Set by handle_fault_signal on an InterruptFaultPage unwind, consumed by start()'s loop to tell
        // it apart from any other clean return. Atomic even though both ends are the same thread: the
        // C++ abstract machine has no signal-delivery control-flow edge, so an optimizer may treat this
        // as unmodified across the opaque ExecuteThread() call and cache a stale read.
        std::atomic<bool> interrupt_page_unwind_{false};
#endif

        std::map<uint64_t, mapped_region> regions_;
        std::vector<mmio_region> mmio_regions_;

        // Every per-logical-thread call-ret buffer ever allocated by ensure_callret_buffer, so the
        // destructor can release them - these live outside regions_ (host allocator space, not the
        // guest address space) and outlive any individual CPUState snapshot they were allocated for.
        std::unordered_map<uint64_t, callret_buffer_record> callret_buffers_;

#if defined(__APPLE__) || defined(__ANDROID__)
        std::map<uint64_t, size_t> claimed_host_ranges_;
#endif

        // See x86_emulator::register_gate_crossing / the gate_crossing struct (declared above, near
        // find_gate_crossing). Each entry is a WoW64 mode-switch point; reaching one (recognized in
        // handle_fault_signal by matching the faulting RIP) marshals CPU state between context_/
        // context32_ and flips active_context_/active_thread_ instead of raising a memory violation.
        // Consulted by QueryGuestExecutableRange (so the range is non-executable to the JIT, forcing
        // the synthetic #PF) and by perform_gate_crossing.
        std::vector<gate_crossing> gate_crossings_;

#ifdef __APPLE__
        // Apple Silicon's 16KB host page is coarser than the guest's 4KB architectural page, so one host
        // mprotect cannot always express what the guest requested per 4KB page - a PE image's .text (RX)
        // directly followed by .data (RW) share a host page. page_shadow_apple_ is the per-4KB source of
        // truth (absent = never requested, which must still fault).
        std::map<uint64_t, memory_permission> page_shadow_apple_;
#endif

        hook_entry* syscall_hook_ = nullptr;
        std::unordered_map<emulator_hook*, hook_entry> instruction_hooks_;
        std::unordered_map<emulator_hook*, interrupt_hook_callback> interrupt_hooks_;
        std::unordered_map<emulator_hook*, memory_access_hook_callback> memory_read_hooks_;
        std::unordered_map<emulator_hook*, memory_access_hook_callback> memory_write_hooks_;
        std::unordered_map<emulator_hook*, memory_execution_hook_callback> memory_execution_hooks_;
        std::unordered_map<emulator_hook*, memory_violation_hook_callback> memory_violation_hooks_;
        std::unordered_map<emulator_hook*, basic_block_hook_callback> basic_block_hooks_;
    };

#if defined(__APPLE__) || defined(__ANDROID__)
    namespace
    {
        // Keep this function async-signal-safe: it may run from a signal handler while
        // libc or other runtime code is in an inconsistent/locked state. Avoid stdio,
        // snprintf, allocation, locks, and other non-async-signal-safe operations.
        // Formatting is therefore done manually into a fixed stack buffer, followed by
        // a single write(2), which is async-signal-safe.
        static void log_unhandled_signal(int sig, const siginfo_t* info, ucontext_t* uctx)
        {
            const int saved_errno = errno;

            char buf[160];
            char* out = buf;
            const char* const end = buf + sizeof(buf);

            const auto append_string = [&](const char* str) {
                while (*str != '\0' && out < end)
                {
                    *out++ = *str++;
                }
            };

            const auto append_decimal = [&](int64_t value) {
                char digits[20];
                size_t count = 0;

                uint64_t magnitude;
                if (value < 0)
                {
                    if (out < end)
                    {
                        *out++ = '-';
                    }

                    magnitude = static_cast<uint64_t>(-(value + 1)) + 1;
                }
                else
                {
                    magnitude = static_cast<uint64_t>(value);
                }

                do
                {
                    digits[count++] = static_cast<char>('0' + magnitude % 10);
                    magnitude /= 10;
                } while (magnitude != 0);

                while (count != 0 && out < end)
                {
                    *out++ = digits[--count];
                }
            };

            const auto append_hex = [&](uint64_t value) {
                constexpr char hex_digits[] = "0123456789abcdef";

                char digits[16];
                size_t count = 0;

                do
                {
                    digits[count++] = hex_digits[value & 0xf];
                    value >>= 4;
                } while (value != 0);

                while (count != 0 && out < end)
                {
                    *out++ = digits[--count];
                }
            };

            append_string("[FEX backend] unhandled signal ");
            append_decimal(sig);

            append_string(" si_code=");
            append_decimal(info->si_code);

            append_string(" at pc=0x");
#ifdef __APPLE__
            append_hex(arm_thread_state64_get_pc(uctx->uc_mcontext->__ss));
#else
            append_hex(uctx->uc_mcontext.pc);
#endif

            append_string(" fault_addr=0x");
            append_hex(reinterpret_cast<uintptr_t>(info->si_addr));

            if (out < end)
            {
                *out++ = '\n';
            }

            if (out != buf)
            {
                (void)::write(STDERR_FILENO, buf, static_cast<size_t>(out - buf));
            }

            errno = saved_errno;
        }

        void fault_signal_handler(int sig, siginfo_t* info, void* raw_ucontext)
        {
            auto* uctx = static_cast<ucontext_t*>(raw_ucontext);

            const bool handled = g_active_emulator != nullptr && g_active_emulator->handle_fault_signal(sig, info, raw_ucontext);
            if (handled)
            {
                return;
            }

            log_unhandled_signal(sig, info, uctx);

            struct sigaction default_action{};
            default_action.sa_handler = SIG_DFL;
            ::sigaction(sig, &default_action, nullptr);
            ::raise(sig);
        }
    } // namespace
#endif

    uint64_t fex_syscall_handler::HandleSyscall(FEXCore::Core::CpuStateFrame* /*frame*/, FEXCore::HLE::SyscallArguments* /*args*/)
    {
        // SyscallOp sets CPUState.rip to the address of the `syscall` instruction itself before invoking
        // us, which is the convention sogen's shared syscall layer expects: it leaves rip so that
        // advancing by the 2-byte syscall length always yields the intended next rip - the following
        // instruction for a plain syscall, and the real target for a redirecting one, which sets
        // rip = target - 2. On non-_WIN32 hosts `syscall` is not FLAGS_BLOCK_END, so without the
        // unconditional advance below the JIT falls through and either re-executes the syscall on block
        // re-entry or, for a redirect, branches into the middle of an instruction at target - 2.
        auto* hook = this->emulator_.syscall_hook_;
        if (hook != nullptr && hook->callback)
        {
            // The Windows syscall layer reads/writes guest registers itself through the emulator, so the
            // hook needs no data argument here. It places the NT status in RAX before returning.
            hook->callback(this->emulator_, 0);
        }

        this->emulator_.cpu_state().rip += 2;

        if (this->emulator_.stop_requested_)
        {
            this->emulator_.request_thread_stop();
        }

        // FEX writes our return value into guest RAX; hand back whatever the hook already set so the
        // value is preserved.
        return this->emulator_.cpu_state().gregs[detail::greg_rax];
    }

    FEXCore::HLE::ExecutableRangeInfo fex_syscall_handler::QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* /*thread*/,
                                                                                     uint64_t address)
    {
        // FEXCore checks this before compiling/executing a guest address (see the decoder's use of
        // QueryGuestExecutableRange) and synthesizes a #PF (IR "Break" op, TrapNo=X86_TRAPNO_PF) if the
        // address isn't reported as executable here - so returning the default-constructed {} for
        // every address would make every guest instruction fetch look like a DEP violation to
        // FEXCore.
        //
        // A registered WoW64 gate crossing is non-executable to the JIT - reaching it must raise a
        // synthetic #PF (which perform_gate_crossing then services) rather than compile the real
        // mode-switch bytes there, which the fixed-bitness JIT can't execute anyway. Checked first,
        // ahead of the region's own real permissions, so this always wins regardless of how the
        // range is actually mapped (sogen's own WoW64 heaven's-gate trampoline is deliberately
        // mapped read|exec for KVM/Unicorn's benefit, but must never look executable here).
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

        // FEXCore caches the returned [Base, Base+Size) span and skips re-querying any address inside
        // it (Frontend.cpp CheckRangeExecutable), so a gate crossing that lives *inside* an otherwise-
        // executable region (e.g. RunSimulatedCode inside wow64cpu.dll's .text, unlike the heaven's-
        // gate trampoline which is its own standalone mapping) would never be consulted: the region
        // query at some earlier address caches the whole span as executable, spanning right across the
        // gate. Clamp the reported span so it stops at the nearest gate boundary on either side of the
        // queried address - the gate itself then falls outside the cache, forcing a fresh query (which
        // returns {} above) the moment execution reaches it.
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
        // We do not back guest code by host file sections, so there is nothing to look up.
        return std::nullopt;
    }

    std::unique_ptr<x86_64_emulator> create_x86_64_emulator()
    {
        return std::make_unique<fex_x86_64_emulator>();
    }
} // namespace sogen::fex
