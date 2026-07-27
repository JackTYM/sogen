// Phase-0 feasibility spike (see the FEX-on-HVF hardware-TSO plan, §5 Phase 0): proves that real
// Arm64JITCore-compiled output executes inside a Hypervisor.framework vCPU with ACTLR_EL1.EnTSO=1
// under the "same-VA, compact-IPA" memory model, and that host callbacks reached through
// CpuStateFrame::Pointers slots round-trip via hvc-based hypercall stubs.
//
// Standalone by design: nothing here is wired into fex_x86_64_emulator. Run modes:
//   sogen-hvf-spike               HVF mode (the actual experiment)
//   sogen-hvf-spike --inprocess   reference run of the identical guest program through
//                                 ExecuteThread on the host, for result cross-validation

#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#include <Hypervisor/Hypervisor.h>

#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/Core/SignalDelegator.h>
#include <FEXCore/Core/X86Enums.h>
#include <FEXCore/Debug/InternalThreadState.h>
#include <FEXCore/HLE/SyscallHandler.h>
#include <FEXCore/Utils/AllocatorHooks.h>
#include <FEXCore/Utils/LogManager.h>

#include <sys/sysctl.h>

namespace
{
    constexpr size_t host_page = 16384;
    constexpr size_t guest_page = 4096;

    // HV_SYS_REG_ACTLR_EL1 is declared macOS-15+ in the SDK, but the project's deployment target
    // is 11.0; the spike probes at runtime and fails gracefully on older systems, so use the raw
    // encoding instead of the availability-guarded enumerator.
    constexpr auto sys_reg_actlr_el1 = static_cast<hv_sys_reg_t>(0xc081);

    uint64_t now_ns()
    {
        return clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
    }

    const char* hv_err_str(hv_return_t r)
    {
        switch (static_cast<uint32_t>(r))
        {
        case 0:
            return "HV_SUCCESS";
        case 0xfae94001:
            return "HV_ERROR";
        case 0xfae94002:
            return "HV_BUSY";
        case 0xfae94003:
            return "HV_BAD_ARGUMENT";
        case 0xfae94004:
            return "HV_ILLEGAL_GUEST_STATE";
        case 0xfae94005:
            return "HV_NO_RESOURCES";
        case 0xfae94006:
            return "HV_NO_DEVICE";
        case 0xfae94007:
            return "HV_DENIED";
        case 0xfae9400f:
            return "HV_UNSUPPORTED";
        default:
            return "HV_<unknown>";
        }
    }

#define CHECK_HV(expr)                                                                                            \
    do                                                                                                            \
    {                                                                                                             \
        const hv_return_t check_hv_r = (expr);                                                                    \
        if (check_hv_r != HV_SUCCESS)                                                                             \
        {                                                                                                         \
            fprintf(stderr, "FATAL: %s -> 0x%x (%s) at %s:%d\n", #expr, static_cast<uint32_t>(check_hv_r),        \
                    hv_err_str(check_hv_r), __FILE__, __LINE__);                                                  \
            exit(1);                                                                                              \
        }                                                                                                         \
    } while (0)

#define CHECK_TRUE(expr)                                                                  \
    do                                                                                    \
    {                                                                                     \
        if (!(expr))                                                                      \
        {                                                                                 \
            fprintf(stderr, "FATAL: %s failed at %s:%d\n", #expr, __FILE__, __LINE__);    \
            exit(1);                                                                      \
        }                                                                                 \
    } while (0)

    // =============================================================================================
    // FEXCore-internal allocation arena. Mirrors the backend's fex_internal_arena idea, simplified
    // for the spike: one large, fully-committed RW reservation satisfied by a bump allocator, so
    // the entire FEXCore-internal world (code buffer, dispatcher, lookup caches, callret stack,
    // emulator stack) lives in ONE contiguous host range that gets a single hv_vm_map + stage-1
    // mapping. In HVF mode, MAP_JIT/PROT_EXEC are stripped from executable requests: the host
    // never executes JIT output on this path (only the guest does, via stage-2 EXEC), which is
    // exactly the memory model Phase 1 targets (plan §2.5).
    // =============================================================================================
    class spike_arena
    {
      public:
        static constexpr size_t arena_size = 0x8000'0000ULL; // 2 GiB

        static spike_arena& instance()
        {
            static spike_arena arena;
            return arena;
        }

        void install(bool strip_exec)
        {
            this->strip_exec_ = strip_exec;
            void* base = ::mmap(nullptr, arena_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            CHECK_TRUE(base != MAP_FAILED);
            this->base_ = reinterpret_cast<uintptr_t>(base);
            this->cursor_ = this->base_;

            FEXCore::Allocator::mmap = &spike_arena::hook_mmap;
            FEXCore::Allocator::munmap = &spike_arena::hook_munmap;
        }

        uintptr_t base() const
        {
            return this->base_;
        }

        size_t used() const
        {
            return this->cursor_ - this->base_;
        }

        struct exec_range
        {
            uintptr_t addr;
            size_t size;
        };

        const std::vector<exec_range>& exec_ranges() const
        {
            return this->exec_ranges_;
        }

      private:
        uintptr_t base_ = 0;
        uintptr_t cursor_ = 0;
        bool strip_exec_ = false;
        std::vector<exec_range> exec_ranges_;

        void* allocate(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
        {
            if (addr != nullptr || (flags & MAP_ANONYMOUS) == 0)
            {
                return ::mmap(addr, length, prot, flags, fd, offset);
            }

            const size_t rounded = (length + host_page - 1) & ~(host_page - 1);
            if (this->cursor_ + rounded > this->base_ + arena_size)
            {
                fprintf(stderr, "[hvf-spike] FATAL: arena exhausted (%zu MiB used)\n", this->used() >> 20);
                errno = ENOMEM;
                return MAP_FAILED;
            }

            const uintptr_t slot = this->cursor_;
            this->cursor_ += rounded;

            const bool wants_exec = (prot & PROT_EXEC) != 0;
            if (wants_exec)
            {
                this->exec_ranges_.push_back({slot, rounded});
            }

            if (wants_exec && !this->strip_exec_)
            {
                // In-process mode keeps real MAP_JIT semantics; MAP_JIT|MAP_FIXED is rejected on
                // Apple, so punch a hole and re-request with an address hint like the backend does.
                ::munmap(reinterpret_cast<void*>(slot), rounded);
                void* result = ::mmap(reinterpret_cast<void*>(slot), rounded, prot, flags, fd, offset);
                if (result != reinterpret_cast<void*>(slot))
                {
                    fprintf(stderr, "[hvf-spike] FATAL: could not place MAP_JIT buffer in arena\n");
                    errno = ENOMEM;
                    return MAP_FAILED;
                }
                return result;
            }

            if (this->strip_exec_)
            {
                // Memory is already RW; requests are handed out as-is (bump allocations are fresh
                // zero pages, matching mmap semantics).
                return reinterpret_cast<void*>(slot);
            }

            return ::mmap(reinterpret_cast<void*>(slot), rounded, prot, flags | MAP_FIXED, fd, offset);
        }

        static void* hook_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
        {
            return instance().allocate(addr, length, prot, flags, fd, offset);
        }

        static int hook_munmap(void* addr, size_t length)
        {
            // Bump allocator: freed ranges are never reused within one spike run.
            (void)addr;
            (void)length;
            return 0;
        }
    };

    // =============================================================================================
    // Stage-1 page tables: 4KB granule, 4 levels, T0SZ=16 (48-bit VA), so every guest-visible
    // object keeps its exact host virtual address. Table pool is a single host allocation mapped
    // into the IPA space; descriptors hold IPAs (stage-1 output addresses are IPAs, translated by
    // stage-2).
    // =============================================================================================
    class stage1_tables
    {
      public:
        static constexpr uint64_t attr_af = 1ull << 10;
        static constexpr uint64_t attr_sh_inner = 3ull << 8;
        static constexpr uint64_t attr_ap_ro = 1ull << 7;
        static constexpr uint64_t attr_pxn = 1ull << 53;
        static constexpr uint64_t attr_uxn = 1ull << 54;

        void init(uint64_t pool_ipa, size_t pool_size)
        {
            this->pool_size_ = pool_size;
            this->pool_ipa_ = pool_ipa;
            void* pool = ::mmap(nullptr, pool_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            CHECK_TRUE(pool != MAP_FAILED);
            this->pool_ = reinterpret_cast<uint8_t*>(pool);
            CHECK_HV(hv_vm_map(pool, pool_ipa, pool_size, HV_MEMORY_READ | HV_MEMORY_WRITE));
            this->root_ = this->alloc_table();
        }

        uint64_t root_ipa() const
        {
            return this->table_ipa(this->root_);
        }

        void map_range(uint64_t va, uint64_t ipa, size_t size, bool writable, bool executable)
        {
            CHECK_TRUE((va % guest_page) == 0 && (ipa % guest_page) == 0 && (size % guest_page) == 0);
            uint64_t attrs = 0b11ull | attr_af | attr_sh_inner | attr_uxn;
            if (!writable)
            {
                attrs |= attr_ap_ro;
            }
            if (!executable)
            {
                attrs |= attr_pxn;
            }
            for (size_t off = 0; off < size; off += guest_page)
            {
                uint64_t* entry = this->walk(va + off);
                CHECK_TRUE(*entry == 0);
                *entry = (ipa + off) | attrs;
            }
        }

      private:
        uint8_t* pool_ = nullptr;
        size_t pool_size_ = 0;
        size_t pool_used_ = 0;
        uint64_t pool_ipa_ = 0;
        uint64_t* root_ = nullptr;

        uint64_t* alloc_table()
        {
            CHECK_TRUE(this->pool_used_ + guest_page <= this->pool_size_);
            auto* table = reinterpret_cast<uint64_t*>(this->pool_ + this->pool_used_);
            this->pool_used_ += guest_page;
            return table;
        }

        uint64_t table_ipa(const uint64_t* table) const
        {
            return this->pool_ipa_ + (reinterpret_cast<uintptr_t>(table) - reinterpret_cast<uintptr_t>(this->pool_));
        }

        uint64_t* walk(uint64_t va)
        {
            CHECK_TRUE((va >> 48) == 0);
            uint64_t* table = this->root_;
            for (int shift = 39; shift > 12; shift -= 9)
            {
                const size_t index = (va >> shift) & 0x1FF;
                if (table[index] == 0)
                {
                    uint64_t* next = this->alloc_table();
                    table[index] = this->table_ipa(next) | 0b11ull;
                }
                table = reinterpret_cast<uint64_t*>(this->pool_ + ((table[index] & ~0xFFFull) - this->pool_ipa_));
            }
            return &table[(va >> 12) & 0x1FF];
        }
    };

    // =============================================================================================
    // VM mapper: allocates compact IPA space, installs stage-2 (hv_vm_map) and stage-1 mappings
    // for host VA ranges so they stay addressable at their host addresses inside the vCPU.
    // =============================================================================================
    class vm_mapper
    {
      public:
        void init()
        {
            this->tables_.init(this->alloc_ipa(64ull << 20), 64ull << 20);
        }

        void map_same_va(const void* host, size_t size, bool writable, bool executable)
        {
            const auto va = reinterpret_cast<uintptr_t>(host);
            CHECK_TRUE((va % host_page) == 0);
            const size_t rounded = (size + host_page - 1) & ~(host_page - 1);
            const uint64_t ipa = this->alloc_ipa(rounded);

            hv_memory_flags_t flags = HV_MEMORY_READ;
            if (writable)
            {
                flags |= HV_MEMORY_WRITE;
            }
            if (executable)
            {
                flags |= HV_MEMORY_EXEC;
            }
            const hv_return_t r = hv_vm_map(const_cast<void*>(host), ipa, rounded, flags);
            if (r != HV_SUCCESS)
            {
                fprintf(stderr, "[hvf-spike] hv_vm_map(%p, ipa=0x%llx, size=0x%zx, flags=%u) -> %s\n", host,
                        static_cast<unsigned long long>(ipa), rounded, static_cast<unsigned>(flags), hv_err_str(r));
                exit(1);
            }
            this->tables_.map_range(va, ipa, rounded, writable, executable);
            this->mappings_.push_back({va, ipa, rounded});
        }

        uint64_t ipa_of(uint64_t va) const
        {
            for (const auto& m : this->mappings_)
            {
                if (va >= m.va && va < m.va + m.size)
                {
                    return m.ipa + (va - m.va);
                }
            }
            return UINT64_MAX;
        }

        uint64_t va_of_ipa(uint64_t ipa) const
        {
            for (const auto& m : this->mappings_)
            {
                if (ipa >= m.ipa && ipa < m.ipa + m.size)
                {
                    return m.va + (ipa - m.ipa);
                }
            }
            return UINT64_MAX;
        }

        uint64_t root_ipa() const
        {
            return this->tables_.root_ipa();
        }

      private:
        struct mapping
        {
            uint64_t va;
            uint64_t ipa;
            uint64_t size;
        };

        stage1_tables tables_;
        uint64_t next_ipa_ = 0x10000;
        std::vector<mapping> mappings_;

        uint64_t alloc_ipa(size_t size)
        {
            const uint64_t ipa = this->next_ipa_;
            this->next_ipa_ += (size + host_page - 1) & ~(host_page - 1);
            return ipa;
        }
    };

    // =============================================================================================
    // Hypercall ID space and the guest runtime page (EL1 vector table + hypercall stubs).
    // =============================================================================================
    constexpr uint16_t hc_callback_base = 0x000; // + slot index
    constexpr uint16_t hc_vector_base = 0x100;   // + vector entry index (offset/0x80)
    constexpr uint16_t hc_sanity = 0x1F0;
    constexpr uint16_t hc_done = 0x1FF;

    constexpr uint32_t insn_ret = 0xD65F03C0;
    constexpr uint32_t insn_b_self = 0x14000000;

    constexpr uint32_t insn_hvc(uint16_t imm)
    {
        return 0xD4000002u | (static_cast<uint32_t>(imm) << 5);
    }

    struct guest_runtime_page
    {
        uint8_t* page = nullptr;
        uint64_t vector_table_va = 0;
        uint64_t done_stub_va = 0;
        uint64_t stub_base_va = 0;
        static constexpr size_t max_stubs = 128;

        void build()
        {
            this->page =
                static_cast<uint8_t*>(::mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
            CHECK_TRUE(this->page != MAP_FAILED);

            auto* insns = reinterpret_cast<uint32_t*>(this->page);
            for (int entry = 0; entry < 16; ++entry)
            {
                insns[entry * 0x80 / 4] = insn_hvc(hc_vector_base + entry);
                insns[entry * 0x80 / 4 + 1] = insn_b_self;
            }
            this->vector_table_va = reinterpret_cast<uint64_t>(this->page);

            auto* done = reinterpret_cast<uint32_t*>(this->page + 0x800);
            done[0] = insn_hvc(hc_done);
            done[1] = insn_b_self;
            this->done_stub_va = reinterpret_cast<uint64_t>(done);

            auto* stubs = reinterpret_cast<uint32_t*>(this->page + 0x900);
            for (size_t i = 0; i < max_stubs; ++i)
            {
                stubs[i * 2] = insn_hvc(static_cast<uint16_t>(hc_callback_base + i));
                stubs[i * 2 + 1] = insn_ret;
            }
            this->stub_base_va = reinterpret_cast<uint64_t>(stubs);

            sys_icache_invalidate(this->page, host_page);
        }

        uint64_t stub_va(size_t index) const
        {
            CHECK_TRUE(index < max_stubs);
            return this->stub_base_va + index * 8;
        }
    };

    // =============================================================================================
    // Generic host-call trampoline: replays the guest's argument registers (x0-x8, q0-q7) into
    // host registers, calls the original host function a Pointers slot referred to, and captures
    // the C-ABI result registers (x0, x1, q0, q1). The VM exit/enter round trip preserves every
    // other guest register, so this reproduces the exact ABI the JIT-emitted call site expects.
    // =============================================================================================
    extern "C" void hvf_spike_call_trampoline(uint64_t* gprs, uint8_t* vecs, uint64_t target);
    __asm__(
        ".text\n"
        ".p2align 2\n"
        ".globl _hvf_spike_call_trampoline\n"
        "_hvf_spike_call_trampoline:\n"
        "  sub sp, sp, #0x20\n"
        "  stp x19, x20, [sp]\n"
        "  stp x21, lr, [sp, #16]\n"
        "  mov x19, x0\n"
        "  mov x20, x1\n"
        "  mov x21, x2\n"
        "  ldp q0, q1, [x20]\n"
        "  ldp q2, q3, [x20, #32]\n"
        "  ldp q4, q5, [x20, #64]\n"
        "  ldp q6, q7, [x20, #96]\n"
        "  ldp x0, x1, [x19]\n"
        "  ldp x2, x3, [x19, #16]\n"
        "  ldp x4, x5, [x19, #32]\n"
        "  ldp x6, x7, [x19, #48]\n"
        "  ldr x8, [x19, #64]\n"
        "  blr x21\n"
        "  stp x0, x1, [x19]\n"
        "  stp q0, q1, [x20]\n"
        "  ldp x21, lr, [sp, #16]\n"
        "  ldp x19, x20, [sp]\n"
        "  add sp, sp, #0x20\n"
        "  ret\n");

    struct callback_slot
    {
        std::string name;
        uint64_t original = 0;
        uint64_t count = 0;
        bool needs_fp = false;
    };

    // =============================================================================================
    // Guest x86-64 test program.
    // =============================================================================================
    struct x86_program
    {
        std::vector<uint8_t> bytes;
        size_t val_offset = 0;
        size_t res_offset = 0;

        static constexpr uint32_t ping_iters = 1000;
        static constexpr uint32_t tight_iters = 10'000'000;
        static constexpr uint32_t rtt_iters = 100'000;

        void emit(std::initializer_list<uint8_t> code)
        {
            this->bytes.insert(this->bytes.end(), code);
        }

        void emit_u32(uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
            {
                this->bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
            }
        }

        void emit_mov_rax(uint32_t value)
        {
            this->emit({0x48, 0xC7, 0xC0});
            this->emit_u32(value);
        }

        void emit_mov_ecx(uint32_t value)
        {
            this->emit({0xB9});
            this->emit_u32(value);
        }

        void emit_syscall()
        {
            this->emit({0x0F, 0x05});
        }

        size_t here() const
        {
            return this->bytes.size();
        }

        static int8_t rel8(size_t target, size_t insn_end)
        {
            const auto delta = static_cast<int64_t>(target) - static_cast<int64_t>(insn_end);
            CHECK_TRUE(delta >= -128 && delta <= 127);
            return static_cast<int8_t>(delta);
        }

        void build()
        {
            this->emit_mov_rax(1); // marker 1: begin
            this->emit_syscall();

            // Two-block ping-pong: exercises ExitFunctionLink and block linking.
            this->emit_mov_ecx(ping_iters);
            const size_t ping_a = this->here();
            this->emit({0xFF, 0xC9}); // dec ecx
            this->emit({0x74, 0x04}); // jz ping_done
            this->emit({0xEB, 0x00}); // jmp ping_b (block terminator; target is the next instruction)
            const size_t ping_b = this->here();
            this->emit({0xEB, static_cast<uint8_t>(rel8(ping_a, this->here() + 2))}); // jmp ping_a
            // ping_done:
            CHECK_TRUE(this->here() == ping_b + 2);
            this->emit_mov_rax(2); // marker 2: linking phase done
            this->emit_syscall();

            // Tight single-block loop: steady-state exit-rate measurement.
            this->emit_mov_ecx(tight_iters);
            this->emit({0x31, 0xDB}); // xor ebx, ebx
            const size_t tight = this->here();
            this->emit({0x48, 0x83, 0xC3, 0x02});                                  // add rbx, 2
            this->emit({0xFF, 0xC9});                                              // dec ecx
            this->emit({0x75, static_cast<uint8_t>(rel8(tight, this->here() + 2))}); // jnz tight
            this->emit_mov_rax(3); // marker 3: tight loop done
            this->emit_syscall();

            // Syscall round-trip measurement loop. Counter lives in esi: the x86-64 syscall
            // instruction clobbers rcx (next-RIP) and r11 (RFLAGS) architecturally.
            this->emit_mov_rax(4);
            this->emit({0xBE}); // mov esi, imm32
            this->emit_u32(rtt_iters);
            const size_t rtt = this->here();
            this->emit_syscall();
            this->emit({0xFF, 0xCE});                                              // dec esi
            this->emit({0x75, static_cast<uint8_t>(rel8(rtt, this->here() + 2))}); // jnz rtt
            this->emit_mov_rax(5); // marker 5: rtt loop done
            this->emit_syscall();

            // x87: fld/fsin/fstp all reach FallbackHandlerPointers hypercalls.
            this->emit({0xDD, 0x05});
            const size_t fld_disp_at = this->here();
            this->emit_u32(0); // patched below
            this->emit({0xD9, 0xFE}); // fsin
            this->emit({0xDD, 0x1D});
            const size_t fstp_disp_at = this->here();
            this->emit_u32(0); // patched below
            this->emit_mov_rax(6); // marker 6: fsin done, request stop
            this->emit_syscall();
            const size_t spin = this->here();
            this->emit({0xEB, static_cast<uint8_t>(rel8(spin, this->here() + 2))}); // jmp spin

            while (this->bytes.size() % 8 != 0)
            {
                this->bytes.push_back(0x90);
            }
            this->val_offset = this->here();
            const double val = 0.5;
            const uint8_t* val_bytes = reinterpret_cast<const uint8_t*>(&val);
            this->bytes.insert(this->bytes.end(), val_bytes, val_bytes + 8);
            this->res_offset = this->here();
            for (int i = 0; i < 8; ++i)
            {
                this->bytes.push_back(0);
            }

            const auto patch32 = [this](size_t at, uint32_t value) {
                memcpy(this->bytes.data() + at, &value, sizeof(value));
            };
            patch32(fld_disp_at, static_cast<uint32_t>(this->val_offset - (fld_disp_at + 4)));
            patch32(fstp_disp_at, static_cast<uint32_t>(this->res_offset - (fstp_disp_at + 4)));
        }
    };

    // =============================================================================================
    // Spike-global run state shared between the exit loop and the syscall handler.
    // =============================================================================================
    struct run_state
    {
        bool inprocess = false;
        uint64_t marker_time[8] = {};
        uint64_t marker_exits[8] = {};
        uint64_t marker_hits[8] = {};
        uint64_t total_exits = 0;
        bool stop_requested = false;
        volatile double* fsin_result = nullptr;
        uint64_t tight_rbx = 0;
    };

    run_state g_run;

    void print_result_validation(const char* tag)
    {
        const double expected = std::sin(0.5);
        const double got = *g_run.fsin_result;
        printf("[%s] tight loop rbx = %" PRIu64 " (expected %u) -> %s\n", tag, g_run.tight_rbx,
               2 * x86_program::tight_iters,
               g_run.tight_rbx == 2ull * x86_program::tight_iters ? "OK" : "MISMATCH");
        printf("[%s] fsin(0.5) = %.17g (libm %.17g, delta %.3g) -> %s\n", tag, got, expected, std::fabs(got - expected),
               std::fabs(got - expected) < 1e-12 ? "OK" : "MISMATCH");
        printf("[%s] marker timings: linking=%.3f ms, tight loop=%.3f ms, rtt loop=%.3f ms\n", tag,
               (g_run.marker_time[2] - g_run.marker_time[1]) / 1e6, (g_run.marker_time[3] - g_run.marker_time[2]) / 1e6,
               (g_run.marker_time[5] - g_run.marker_time[4]) / 1e6);
    }

    class spike_syscall_handler final : public FEXCore::HLE::SyscallHandler
    {
      public:
        spike_syscall_handler()
        {
            this->OSABI = FEXCore::HLE::SyscallOSABI::OS_GENERIC;
        }

        uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* frame, FEXCore::HLE::SyscallArguments* /*args*/) override
        {
            const uint64_t marker = frame->State.gregs[FEXCore::X86State::REG_RAX];
            if (marker < 8)
            {
                if (g_run.marker_hits[marker] == 0)
                {
                    g_run.marker_time[marker] = now_ns();
                    g_run.marker_exits[marker] = g_run.total_exits;
                    printf("[spike] marker %" PRIu64 " first hit: rip=0x%llx rcx=%llu\n", marker,
                           static_cast<unsigned long long>(frame->State.rip),
                           static_cast<unsigned long long>(frame->State.gregs[FEXCore::X86State::REG_RCX]));
                }
                g_run.marker_hits[marker]++;
                if (g_run.marker_hits[marker] == 200000)
                {
                    fprintf(stderr, "[spike] marker %" PRIu64 " hit 200000 times - runaway loop, rip=0x%llx rcx=%llu\n",
                            marker, static_cast<unsigned long long>(frame->State.rip),
                            static_cast<unsigned long long>(frame->State.gregs[FEXCore::X86State::REG_RCX]));
                    std::exit(2);
                }
            }

            if (marker == 3)
            {
                g_run.tight_rbx = frame->State.gregs[FEXCore::X86State::REG_RBX];
            }

            if (marker == 6)
            {
                g_run.stop_requested = true;
                if (g_run.inprocess)
                {
                    // The spike has no in-process stop plumbing (that is the backend's signal
                    // machinery); everything worth validating is available here, so leave directly.
                    print_result_validation("inprocess");
                    std::exit(0);
                }
            }

            frame->State.rip += 2;
            return frame->State.gregs[FEXCore::X86State::REG_RAX];
        }

        FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* /*thread*/,
                                                                    uint64_t address) override
        {
            if (address >= this->program_base && address < this->program_base + this->program_size)
            {
                return {this->program_base, this->program_size, false};
            }
            return {};
        }

        std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState* /*thread*/,
                                                                                     uint64_t /*guest_addr*/) override
        {
            return std::nullopt;
        }

        uint64_t program_base = 0;
        uint64_t program_size = 0;
    };

    FEXCore::HostFeatures fetch_host_features_apple()
    {
        const auto flag = [](const char* name) {
            int32_t value = 0;
            size_t size = sizeof(value);
            if (::sysctlbyname(name, &value, &size, nullptr, 0) != 0)
            {
                return false;
            }
            return value != 0;
        };

        FEXCore::HostFeatures features{};

        uint64_t cache_line_size = 64;
        size_t cache_line_size_len = sizeof(cache_line_size);
        ::sysctlbyname("hw.cachelinesize", &cache_line_size, &cache_line_size_len, nullptr, 0);
        features.DCacheLineSize = static_cast<uint32_t>(cache_line_size);
        features.ICacheLineSize = static_cast<uint32_t>(cache_line_size);
        features.SupportsCacheMaintenanceOps = true;

        features.SupportsAES = flag("hw.optional.arm.FEAT_AES");
        features.SupportsCRC = flag("hw.optional.arm.FEAT_CRC32");
        features.SupportsAtomics = flag("hw.optional.arm.FEAT_LSE");
        features.SupportsRCPC = flag("hw.optional.arm.FEAT_LRCPC");
        features.SupportsRAND = flag("hw.optional.arm.FEAT_RNG");
        features.SupportsSHA = flag("hw.optional.arm.FEAT_SHA1") && flag("hw.optional.arm.FEAT_SHA256");
        features.SupportsPMULL_128Bit = flag("hw.optional.arm.FEAT_PMULL");
        features.SupportsCSSC = flag("hw.optional.arm.FEAT_CSSC");
        features.SupportsFCMA = flag("hw.optional.arm.FEAT_FCMA");
        features.SupportsFlagM = flag("hw.optional.arm.FEAT_FlagM");
        features.SupportsFlagM2 = flag("hw.optional.arm.FEAT_FlagM2");
        features.SupportsRPRES = flag("hw.optional.arm.FEAT_RPRES");
        features.SupportsFRINTTS = flag("hw.optional.arm.FEAT_FRINTTS");
        features.SupportsECV = flag("hw.optional.arm.FEAT_ECV");
        features.SupportsWFXT = flag("hw.optional.arm.FEAT_WFxT");
        features.SupportsAFP = flag("hw.optional.arm.FEAT_AFP");
        features.SupportsMOPS = flag("hw.optional.arm.FEAT_MOPS");

        features.SupportsSVE128 = false;
        features.SupportsSVE256 = false;
        features.SupportsAVX = false;
        features.SupportsSVEBitPerm = false;
        features.SupportsCPUIndexInTPIDRRO = false;

        uint32_t logical_cpus = 1;
        size_t logical_cpus_len = sizeof(logical_cpus);
        if (::sysctlbyname("hw.logicalcpu", &logical_cpus, &logical_cpus_len, nullptr, 0) != 0 || logical_cpus == 0)
        {
            logical_cpus = 1;
        }
        features.CPUMIDRs.assign(logical_cpus, 0u);

        return features;
    }

    // =============================================================================================
    // Emitted-code audit (plan §5 Phase 0 audit items): scan the ranges FEXCore requested as
    // executable for `mrs`/`msr` of unexpected system registers and for stray `hlt` (which would
    // indicate vixl indirect runtime calls being active).
    // =============================================================================================
    void audit_emitted_code()
    {
        struct sysreg_hit
        {
            uint32_t encoding;
            uint64_t count;
        };
        std::vector<sysreg_hit> mrs_hits;
        uint64_t hlt_count = 0;
        uint64_t hlt_nonzero_imm = 0;

        const auto known_sysreg = [](uint32_t sysreg) {
            switch (sysreg)
            {
            case 0xDA10: // NZCV
            case 0xDA20: // FPCR
            case 0xDA21: // FPSR
            case 0xDF02: // CNTVCT_EL0
            case 0xD807: // DCZID_EL0
            case 0xD801: // CTR_EL0
            case 0xDF01: // CNTFRQ_EL0... (0xDF00)
            case 0xDF00:
                return true;
            default:
                return false;
            }
        };

        bool unexpected = false;
        for (const auto& range : spike_arena::instance().exec_ranges())
        {
            const auto* insns = reinterpret_cast<const uint32_t*>(range.addr);
            // FEXCore's CodeBuffer mprotects its trailing page PROT_NONE as a guard
            // (CPUBackend.h UsableSize()); never scan into it.
            const size_t scannable = range.size > host_page ? range.size - host_page : range.size;
            const size_t count = scannable / 4;
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t insn = insns[i];
                if ((insn & 0xFFF00000u) == 0xD5300000u || (insn & 0xFFF00000u) == 0xD5100000u)
                {
                    const uint32_t sysreg = (insn >> 5) & 0xFFFF;
                    bool found = false;
                    for (auto& hit : mrs_hits)
                    {
                        if (hit.encoding == sysreg)
                        {
                            hit.count++;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        mrs_hits.push_back({sysreg, 1});
                    }
                    if (!known_sysreg(sysreg))
                    {
                        unexpected = true;
                    }
                }
                else if ((insn & 0xFFE0001Fu) == 0xD4400000u)
                {
                    hlt_count++;
                    if (((insn >> 5) & 0xFFFF) != 0)
                    {
                        hlt_nonzero_imm++;
                    }
                }
            }
        }

        printf("[audit] mrs/msr sysreg encodings in emitted code:\n");
        for (const auto& hit : mrs_hits)
        {
            const uint32_t op0 = 2 + ((hit.encoding >> 14) & 1);
            const uint32_t op1 = (hit.encoding >> 11) & 7;
            const uint32_t crn = (hit.encoding >> 7) & 15;
            const uint32_t crm = (hit.encoding >> 3) & 15;
            const uint32_t op2 = hit.encoding & 7;
            printf("  s%u_%u_c%u_c%u_%u (raw 0x%x): %" PRIu64 " occurrence(s)%s\n", op0, op1, crn, crm, op2, hit.encoding,
                   hit.count, known_sysreg(hit.encoding) ? "" : "  <-- UNEXPECTED");
        }
        printf("[audit] hlt instructions: %" PRIu64 " (nonzero-imm: %" PRIu64 ", vixl indirect calls would use these)\n",
               hlt_count, hlt_nonzero_imm);
        printf("[audit] verdict: %s\n", unexpected ? "UNEXPECTED host-state reads found" : "clean");
    }

    // =============================================================================================
    // libFEXCore image mapping: guest-executed code dereferences pointers into the dylib's data
    // segments (NamedVectorConstants, indexed LUTs - CPUBackend.cpp), so map every non-pagezero
    // segment of the image at its host VA.
    // =============================================================================================
    void map_fexcore_image(vm_mapper& mapper)
    {
        Dl_info info{};
        CHECK_TRUE(dladdr(reinterpret_cast<void*>(&FEXCore::Config::Initialize), &info) != 0);

        const auto* header = static_cast<const mach_header_64*>(info.dli_fbase);
        CHECK_TRUE(header->magic == MH_MAGIC_64);

        intptr_t slide = 0;
        for (uint32_t i = 0; i < _dyld_image_count(); ++i)
        {
            if (_dyld_get_image_header(i) == reinterpret_cast<const mach_header*>(header))
            {
                slide = _dyld_get_image_vmaddr_slide(i);
                break;
            }
        }

        const auto* cmd = reinterpret_cast<const load_command*>(header + 1);
        for (uint32_t i = 0; i < header->ncmds; ++i)
        {
            if (cmd->cmd == LC_SEGMENT_64)
            {
                const auto* seg = reinterpret_cast<const segment_command_64*>(cmd);
                if (strcmp(seg->segname, SEG_PAGEZERO) != 0 && seg->vmsize != 0)
                {
                    const auto va = static_cast<uintptr_t>(seg->vmaddr) + slide;
                    const bool writable = (seg->initprot & VM_PROT_WRITE) != 0;
                    mapper.map_same_va(reinterpret_cast<void*>(va), seg->vmsize, writable, false);
                }
            }
            cmd = reinterpret_cast<const load_command*>(reinterpret_cast<const uint8_t*>(cmd) + cmd->cmdsize);
        }
        printf("[hvf-spike] mapped libFEXCore image at %p (slide 0x%lx)\n", static_cast<const void*>(header), slide);
    }

    // =============================================================================================
    // vCPU setup and run loop.
    // =============================================================================================
    struct vcpu_context
    {
        hv_vcpu_t vcpu = 0;
        hv_vcpu_exit_t* exit = nullptr;
    };

    void setup_vcpu_system_state(vcpu_context& vc, const vm_mapper& mapper, uint64_t vbar_va)
    {
        uint64_t mmfr0 = 0;
        CHECK_HV(hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_ID_AA64MMFR0_EL1, &mmfr0));
        const uint32_t tgran4 = (mmfr0 >> 28) & 0xF;
        printf("[hvf-spike] ID_AA64MMFR0_EL1=0x%llx TGran4=%u (%s)\n", static_cast<unsigned long long>(mmfr0), tgran4,
               (tgran4 == 0 || tgran4 == 1) ? "4KB stage-1 granule supported" : "4KB stage-1 granule NOT supported");
        CHECK_TRUE(tgran4 == 0 || tgran4 == 1);

        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_MAIR_EL1, 0xFF)); // attr0 = Normal WB RW-alloc
        // T0SZ=16 (48-bit VA), IRGN0/ORGN0=WB-WA, SH0=inner, TG0=4KB, EPD1=1, IPS=40-bit
        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_TCR_EL1, 0x200803510ull));
        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_TTBR0_EL1, mapper.root_ipa()));
        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_CPACR_EL1, 3ull << 20)); // FPEN: no FP/SIMD trap
        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_VBAR_EL1, vbar_va));

        uint64_t sctlr = 0;
        CHECK_HV(hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_SCTLR_EL1, &sctlr));
        sctlr |= (1ull << 0) | (1ull << 2) | (1ull << 12); // M | C | I
        CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_SCTLR_EL1, sctlr));

        CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_CPSR, 0x3c5)); // EL1h, DAIF masked
        CHECK_HV(hv_vcpu_set_vtimer_offset(vc.vcpu, 0));
    }

    bool enable_entso(vcpu_context& vc)
    {
        uint64_t actlr = 0;
        if (hv_vcpu_get_sys_reg(vc.vcpu, sys_reg_actlr_el1, &actlr) != HV_SUCCESS)
        {
            return false;
        }
        actlr |= 1ull << 1;
        if (hv_vcpu_set_sys_reg(vc.vcpu, sys_reg_actlr_el1, actlr) != HV_SUCCESS)
        {
            return false;
        }
        uint64_t readback = 0;
        if (hv_vcpu_get_sys_reg(vc.vcpu, sys_reg_actlr_el1, &readback) != HV_SUCCESS)
        {
            return false;
        }
        return ((readback >> 1) & 1) == 1;
    }

    uint32_t esr_ec(uint64_t syndrome)
    {
        return static_cast<uint32_t>((syndrome >> 26) & 0x3F);
    }

    void dump_guest_state(vcpu_context& vc)
    {
        for (int i = 0; i < 31; ++i)
        {
            uint64_t value = 0;
            hv_vcpu_get_reg(vc.vcpu, static_cast<hv_reg_t>(HV_REG_X0 + i), &value);
            fprintf(stderr, "  x%-2d = 0x%016llx%s", i, static_cast<unsigned long long>(value), (i % 3 == 2) ? "\n" : " ");
        }
        uint64_t pc = 0;
        uint64_t sp = 0;
        hv_vcpu_get_reg(vc.vcpu, HV_REG_PC, &pc);
        hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_SP_EL1, &sp);
        fprintf(stderr, "\n  pc = 0x%016llx sp_el1 = 0x%016llx\n", static_cast<unsigned long long>(pc),
                static_cast<unsigned long long>(sp));
    }

    void report_vector_exit(vcpu_context& vc, uint16_t entry)
    {
        uint64_t esr = 0;
        uint64_t elr = 0;
        uint64_t far = 0;
        uint64_t spsr = 0;
        hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_ESR_EL1, &esr);
        hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_ELR_EL1, &elr);
        hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_FAR_EL1, &far);
        hv_vcpu_get_sys_reg(vc.vcpu, HV_SYS_REG_SPSR_EL1, &spsr);
        fprintf(stderr,
                "[hvf-spike] guest EL1 exception via vector entry %u: ESR_EL1=0x%llx (EC=0x%x) ELR_EL1=0x%llx "
                "FAR_EL1=0x%llx SPSR_EL1=0x%llx\n",
                entry, static_cast<unsigned long long>(esr), esr_ec(esr), static_cast<unsigned long long>(elr),
                static_cast<unsigned long long>(far), static_cast<unsigned long long>(spsr));
        dump_guest_state(vc);
    }

    // Same-VA sanity gate: hand-assembled ARM64 at its host VA, before any FEXCore involvement.
    void run_sanity_blob(vcpu_context& vc, vm_mapper& mapper)
    {
        auto* code =
            static_cast<uint32_t*>(::mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        CHECK_TRUE(code != MAP_FAILED);
        auto* data =
            static_cast<uint64_t*>(::mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        CHECK_TRUE(data != MAP_FAILED);

        code[0] = 0xF9000020; // str x0, [x1]
        code[1] = 0x91000400; // add x0, x0, #1
        code[2] = insn_hvc(hc_sanity);
        code[3] = 0x17FFFFFD; // b start
        sys_icache_invalidate(code, 16);

        mapper.map_same_va(code, host_page, false, true);
        mapper.map_same_va(data, host_page, true, false);

        CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_PC, reinterpret_cast<uint64_t>(code)));
        CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X0, 41));
        CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X1, reinterpret_cast<uint64_t>(data)));

        for (int round = 0; round < 3; ++round)
        {
            for (;;)
            {
                CHECK_HV(hv_vcpu_run(vc.vcpu));
                if (vc.exit->reason == HV_EXIT_REASON_VTIMER_ACTIVATED)
                {
                    hv_vcpu_set_vtimer_mask(vc.vcpu, false);
                    continue;
                }
                break;
            }
            CHECK_TRUE(vc.exit->reason == HV_EXIT_REASON_EXCEPTION);
            CHECK_TRUE(esr_ec(vc.exit->exception.syndrome) == 0x16);
            CHECK_TRUE((vc.exit->exception.syndrome & 0xFFFF) == hc_sanity);
            CHECK_TRUE(*data == 41ull + static_cast<uint64_t>(round));
        }
        printf("[hvf-spike] sanity gate passed: hand-assembled code ran at host VA %p under stage-1 MMU, "
               "stores visible at host VA %p\n",
               static_cast<void*>(code), static_cast<void*>(data));
    }
} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    alarm(600);

    g_run.inprocess = argc > 1 && strcmp(argv[1], "--inprocess") == 0;
    printf("[hvf-spike] mode: %s\n", g_run.inprocess ? "in-process reference" : "HVF");

    LogMan::Msg::InstallHandler([](LogMan::DebugLevels level, const char* message) {
        fprintf(stderr, "[FEXCore LogMan] level=%s: %s\n", LogMan::DebugLevelStr(level), message);
    });
    LogMan::Throw::InstallHandler([](const char* message) { fprintf(stderr, "[FEXCore LogMan THROW] %s\n", message); });

    spike_arena::instance().install(!g_run.inprocess);

    FEXCore::Config::Initialize();
    FEXCore::Config::Load();
    FEXCore::Config::Set(FEXCore::Config::CONFIG_IS64BIT_MODE, "1");
    // Matches the sogen backend's configuration: forces NeedsPendingInterruptFaultCheck, so block
    // entries store to the InterruptFaultPage - the stop protocol this spike validates.
    FEXCore::Config::Set(FEXCore::Config::CONFIG_GDBSERVER, "1");
    FEXCore::Config::Set(FEXCore::Config::CONFIG_SMCCHECKS, "0");

    const FEXCore::HostFeatures features = fetch_host_features_apple();
    auto context = FEXCore::Context::Context::CreateNewContext(features);

    auto syscall_handler = std::make_unique<spike_syscall_handler>();
    context->SetSyscallHandler(syscall_handler.get());

    auto signal_delegator = std::make_unique<FEXCore::SignalDelegator>();
    context->SetSignalDelegator(signal_delegator.get());

    context->SetHardwareTSOSupport(!g_run.inprocess);

    CHECK_TRUE(context->InitCore());

    const auto& dispatcher_config = signal_delegator->GetConfig();
    printf("[hvf-spike] dispatcher: begin=0x%llx end=0x%llx\n",
           static_cast<unsigned long long>(dispatcher_config.DispatcherBegin),
           static_cast<unsigned long long>(dispatcher_config.DispatcherEnd));

    // Guest program + stack.
    x86_program program;
    program.build();
    auto* program_page =
        static_cast<uint8_t*>(::mmap(nullptr, host_page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    CHECK_TRUE(program_page != MAP_FAILED);
    memcpy(program_page, program.bytes.data(), program.bytes.size());
    const auto program_base = reinterpret_cast<uint64_t>(program_page);
    g_run.fsin_result = reinterpret_cast<volatile double*>(program_page + program.res_offset);
    syscall_handler->program_base = program_base;
    syscall_handler->program_size = host_page;

    constexpr size_t guest_stack_size = 256 * 1024;
    auto* guest_stack =
        static_cast<uint8_t*>(::mmap(nullptr, guest_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    CHECK_TRUE(guest_stack != MAP_FAILED);
    const uint64_t guest_rsp = reinterpret_cast<uint64_t>(guest_stack) + guest_stack_size - 64;

    FEXCore::Core::InternalThreadState* thread = context->CreateThread(program_base, guest_rsp, nullptr);
    CHECK_TRUE(thread != nullptr);
    context->SetFlagsFromCompactedEFLAGS(thread, 0x202);
    thread->CurrentFrame->State.FCW = 0x37F;

    // Minimal flat GDT: the decoder reads CS.L to determine the operating mode
    // (Frontend.cpp DecodeInstructionsAtEntry). Lives in the guest-mapped program page.
    {
        auto* gdt = reinterpret_cast<FEXCore::Core::CPUState::gdt_segment*>(program_page + 0x1000);
        memset(gdt, 0, sizeof(FEXCore::Core::CPUState::gdt_segment) * 16);
        gdt[1].L = 1;
        auto& state = thread->CurrentFrame->State;
        state.segment_arrays[0] = gdt;
        state.segment_arrays[1] = gdt;
        state.cs_idx = 0x08;
        state.cs_cached = 0;
    }

    // Call-ret stack: embedder glue FEXCore expects (mirrors fex_vcpu::ensure_callret_stack).
    {
        constexpr size_t callret_size = FEXCore::Core::InternalThreadState::CALLRET_STACK_SIZE;
        void* callret = FEXCore::Allocator::mmap(nullptr, callret_size + 2 * host_page, PROT_NONE,
                                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK_TRUE(callret != MAP_FAILED);
        auto* callret_base = static_cast<uint8_t*>(callret) + host_page;
        // The spike arena hands out already-RW memory in HVF mode; mprotect still validates the range.
        CHECK_TRUE(::mprotect(callret_base, callret_size, PROT_READ | PROT_WRITE) == 0);
        thread->CurrentFrame->State._pad1 = reinterpret_cast<uint64_t>(callret_base);
        thread->CurrentFrame->State.callret_sp = reinterpret_cast<uint64_t>(callret_base) + callret_size / 4;
        thread->CallRetStackBase = callret_base;
    }

    if (g_run.inprocess)
    {
        // Runs until marker 6, where HandleSyscall validates results and exits the process.
        context->ExecuteThread(thread);
        fprintf(stderr, "[inprocess] ExecuteThread returned unexpectedly\n");
        return 1;
    }

    // ------------------------------------------------------------------------------------------
    // HVF path.
    // ------------------------------------------------------------------------------------------
    CHECK_HV(hv_vm_create(nullptr));

    vm_mapper mapper;
    mapper.init();

    guest_runtime_page runtime;
    runtime.build();

    vcpu_context vc;
    CHECK_HV(hv_vcpu_create(&vc.vcpu, &vc.exit, nullptr));

    const bool entso_ok = enable_entso(vc);
    printf("[hvf-spike] EnTSO on executing vCPU: %s\n", entso_ok ? "ENABLED (readback=1)" : "FAILED");
    CHECK_TRUE(entso_ok);

    setup_vcpu_system_state(vc, mapper, runtime.vector_table_va);

    // Mappings: FEX arena (code buffer, dispatcher, lookup caches, callret + emulator stacks),
    // thread state (host heap), program + stack pages, runtime page, libFEXCore image.
    mapper.map_same_va(reinterpret_cast<void*>(spike_arena::instance().base()), spike_arena::arena_size, true, true);
    const auto thread_state_size = (sizeof(FEXCore::Core::InternalThreadState) + host_page - 1) & ~(host_page - 1);
    mapper.map_same_va(thread, thread_state_size, true, false);
    mapper.map_same_va(program_page, host_page, true, false);
    mapper.map_same_va(guest_stack, guest_stack_size, true, false);
    mapper.map_same_va(runtime.page, host_page, false, true);
    map_fexcore_image(mapper);

    run_sanity_blob(vc, mapper);

    // Emulator stack for the dispatcher's PushCalleeSavedRegisters / Syscall-op staging: allocate
    // through the FEX allocator hooks so it lives inside the already-mapped arena.
    constexpr size_t emulator_stack_size = 8ull << 20;
    void* emulator_stack =
        FEXCore::Allocator::mmap(nullptr, emulator_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK_TRUE(emulator_stack != MAP_FAILED);
    const uint64_t emulator_sp = (reinterpret_cast<uint64_t>(emulator_stack) + emulator_stack_size - 64) & ~15ull;

    // Rewrite every host-function Pointers slot to a hypercall stub, remembering the originals.
    std::vector<callback_slot> slots;
    const auto shim = [&](uint64_t& slot_ref, const char* name, bool needs_fp) {
        CHECK_TRUE(slots.size() < guest_runtime_page::max_stubs);
        callback_slot slot;
        slot.name = name;
        slot.original = slot_ref;
        slot.needs_fp = needs_fp;
        slot_ref = runtime.stub_va(slots.size());
        slots.push_back(std::move(slot));
    };

    auto& pointers = thread->CurrentFrame->Pointers;
    size_t syscall_slot_index = 0;
    {
        shim(pointers.PrintValue, "PrintValue", false);
        shim(pointers.PrintVectorValue, "PrintVectorValue", false);
        shim(pointers.PrintMsgValue, "PrintMsgValue", false);
        shim(pointers.ThreadRemoveCodeEntryFromJIT, "ThreadRemoveCodeEntryFromJIT", false);
        shim(pointers.CPUIDFunction, "CPUIDFunction", false);
        shim(pointers.XCRFunction, "XCRFunction", false);
        syscall_slot_index = slots.size();
        shim(pointers.SyscallHandlerFunc, "SyscallHandlerFunc", false);
        shim(pointers.ExitFunctionLink, "ExitFunctionLink", false);
        shim(pointers.MonoBackpatcherWrite, "MonoBackpatcherWrite", false);
        shim(pointers.LUDIV, "LUDIV", false);
        shim(pointers.LDIV, "LDIV", false);
        shim(pointers.CompileBlockFunc, "CompileBlock", false);
        shim(pointers.CompileSingleStepFunc, "CompileSingleStep", false);
        shim(pointers.SleepFunc, "SleepThread", false);

        for (size_t i = 0; i < FEXCore::Core::OPINDEX_MAX; ++i)
        {
            char name[64];
            snprintf(name, sizeof(name), "Fallback[%zu]", i);
            shim(pointers.FallbackHandlerPointers[i].Func, name, true);
        }
    }
    printf("[hvf-spike] shimmed %zu Pointers slots to hypercall stubs\n", slots.size());

    // Pre-compile the entry block host-side so the dispatcher's L2 probe hits in-guest; all later
    // blocks are compiled host-side from within the ExitFunctionLink hypercall.
    context->CompileRIP(thread, program_base);

    audit_emitted_code();

    // Enter the unmodified FEX dispatcher inside the vCPU, exactly as ExecuteDispatch would:
    // x0 = frame, x1 = SingleInst(false), LR = exit stub, SP = emulator stack.
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_PC, dispatcher_config.DispatcherBegin));
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X0, reinterpret_cast<uint64_t>(thread->CurrentFrame)));
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X1, 0));
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_LR, runtime.done_stub_va));
    CHECK_HV(hv_vcpu_set_sys_reg(vc.vcpu, HV_SYS_REG_SP_EL1, emulator_sp));
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_FPCR, 0));
    CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_FPSR, 0));

    const uint64_t fault_page_va = reinterpret_cast<uint64_t>(&thread->InterruptFaultPage);
    const uint64_t fault_page_ipa = mapper.ipa_of(fault_page_va);
    CHECK_TRUE(fault_page_ipa != UINT64_MAX);
    bool stop_armed = false;
    bool done = false;

    uint64_t gprs[9];
    alignas(16) uint8_t vecs[8 * 16];

    const uint64_t run_start = now_ns();
    while (!done)
    {
        CHECK_HV(hv_vcpu_run(vc.vcpu));
        g_run.total_exits++;

        if (vc.exit->reason == HV_EXIT_REASON_VTIMER_ACTIVATED)
        {
            hv_vcpu_set_vtimer_mask(vc.vcpu, false);
            continue;
        }

        if (vc.exit->reason != HV_EXIT_REASON_EXCEPTION)
        {
            fprintf(stderr, "[hvf-spike] unexpected exit reason %u\n", vc.exit->reason);
            dump_guest_state(vc);
            return 1;
        }

        const uint64_t syndrome = vc.exit->exception.syndrome;
        const uint32_t ec = esr_ec(syndrome);

        if (ec == 0x16) // HVC64
        {
            const uint16_t id = syndrome & 0xFFFF;
            if (id == hc_done)
            {
                done = true;
                continue;
            }
            if (id >= hc_vector_base && id < hc_vector_base + 16)
            {
                report_vector_exit(vc, id - hc_vector_base);
                return 1;
            }
            if (id >= slots.size())
            {
                fprintf(stderr, "[hvf-spike] hvc with unknown id 0x%x\n", id);
                return 1;
            }

            auto& slot = slots[id];
            slot.count++;

            for (int i = 0; i < 9; ++i)
            {
                CHECK_HV(hv_vcpu_get_reg(vc.vcpu, static_cast<hv_reg_t>(HV_REG_X0 + i), &gprs[i]));
            }
            if (slot.needs_fp)
            {
                for (int i = 0; i < 8; ++i)
                {
                    CHECK_HV(hv_vcpu_get_simd_fp_reg(vc.vcpu, static_cast<hv_simd_fp_reg_t>(HV_SIMD_FP_REG_Q0 + i),
                                                     reinterpret_cast<hv_simd_fp_uchar16_t*>(&vecs[i * 16])));
                }
            }
            else
            {
                memset(vecs, 0, sizeof(vecs));
            }

            hvf_spike_call_trampoline(gprs, vecs, slot.original);

            CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X0, gprs[0]));
            CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_X1, gprs[1]));
            if (slot.needs_fp)
            {
                CHECK_HV(hv_vcpu_set_simd_fp_reg(vc.vcpu, HV_SIMD_FP_REG_Q0,
                                                 *reinterpret_cast<hv_simd_fp_uchar16_t*>(&vecs[0])));
                CHECK_HV(hv_vcpu_set_simd_fp_reg(vc.vcpu, HV_SIMD_FP_REG_Q1,
                                                 *reinterpret_cast<hv_simd_fp_uchar16_t*>(&vecs[16])));
            }

            if (id == syscall_slot_index && g_run.stop_requested && !stop_armed)
            {
                CHECK_HV(hv_vm_protect(fault_page_ipa, host_page, HV_MEMORY_READ));
                stop_armed = true;
            }
            continue;
        }

        if (ec == 0x24 || ec == 0x20) // data / instruction abort from a lower EL (stage-2)
        {
            const uint64_t fault_va = vc.exit->exception.virtual_address;
            const uint64_t fault_ipa = vc.exit->exception.physical_address;
            if (ec == 0x24 && stop_armed && fault_ipa >= fault_page_ipa && fault_ipa < fault_page_ipa + host_page)
            {
                CHECK_HV(hv_vm_protect(fault_page_ipa, host_page, HV_MEMORY_READ | HV_MEMORY_WRITE));
                CHECK_HV(hv_vcpu_set_reg(vc.vcpu, HV_REG_PC, pointers.ThreadStopHandlerSpillSRA));
                continue;
            }
            fprintf(stderr, "[hvf-spike] unexpected stage-2 %s abort: VA=0x%llx IPA=0x%llx (mapped VA for that IPA: 0x%llx)\n",
                    ec == 0x24 ? "data" : "instruction", static_cast<unsigned long long>(fault_va),
                    static_cast<unsigned long long>(fault_ipa),
                    static_cast<unsigned long long>(mapper.va_of_ipa(fault_ipa)));
            dump_guest_state(vc);
            return 1;
        }

        fprintf(stderr, "[hvf-spike] unexpected exception exit: syndrome=0x%llx EC=0x%x VA=0x%llx IPA=0x%llx\n",
                static_cast<unsigned long long>(syndrome), ec, static_cast<unsigned long long>(vc.exit->exception.virtual_address),
                static_cast<unsigned long long>(vc.exit->exception.physical_address));
        dump_guest_state(vc);
        return 1;
    }
    const uint64_t run_end = now_ns();

    // EnTSO must still be set after all execution (criterion 4).
    uint64_t actlr_final = 0;
    CHECK_HV(hv_vcpu_get_sys_reg(vc.vcpu, sys_reg_actlr_el1, &actlr_final));
    const bool entso_final = ((actlr_final >> 1) & 1) == 1;

    // ------------------------------------------------------------------------------------------
    // Results.
    // ------------------------------------------------------------------------------------------
    printf("\n================= hvf-spike results =================\n");
    printf("EnTSO readback after run: %llu (%s)\n", static_cast<unsigned long long>((actlr_final >> 1) & 1),
           entso_final ? "OK" : "LOST");

    const double expected_sin = std::sin(0.5);
    const double got_sin = *g_run.fsin_result;
    printf("tight loop rbx = %" PRIu64 " (expected %u) -> %s\n", g_run.tight_rbx, 2 * x86_program::tight_iters,
           g_run.tight_rbx == 2ull * x86_program::tight_iters ? "OK" : "MISMATCH");
    printf("fsin(0.5) = %.17g (libm %.17g, delta %.3g) -> %s\n", got_sin, expected_sin, std::fabs(got_sin - expected_sin),
           std::fabs(got_sin - expected_sin) < 1e-12 ? "OK" : "MISMATCH");

    const uint64_t ping_exits = g_run.marker_exits[2] - g_run.marker_exits[1];
    const uint64_t tight_exits = g_run.marker_exits[3] - g_run.marker_exits[2];
    const uint64_t rtt_exits = g_run.marker_exits[5] - g_run.marker_exits[3];
    const uint64_t tight_ns = g_run.marker_time[3] - g_run.marker_time[2];
    const uint64_t rtt_ns = g_run.marker_time[5] - g_run.marker_time[4];
    printf("block-linking phase (%u two-block iterations): %" PRIu64 " exits total -> %s\n", x86_program::ping_iters,
           ping_exits, ping_exits < 16 ? "linking effective (near-zero steady-state exits)" : "LINKING NOT EFFECTIVE");
    printf("tight loop (%u iterations): %" PRIu64 " exits, %.1f ms (%.2f ns/iter) -> %s\n", x86_program::tight_iters,
           tight_exits, tight_ns / 1e6, static_cast<double>(tight_ns) / x86_program::tight_iters,
           tight_exits < 8 ? "~zero steady-state exit rate" : "UNEXPECTED EXITS");
    printf("syscall hypercall round trip: %" PRIu64 " syscalls (%" PRIu64 " exits) in %.1f ms -> %.0f ns each "
           "(standalone hvc baseline ~720)\n",
           static_cast<uint64_t>(x86_program::rtt_iters), rtt_exits, rtt_ns / 1e6,
           static_cast<double>(rtt_ns) / x86_program::rtt_iters);
    printf("total exits: %" PRIu64 " in %.1f ms\n", g_run.total_exits, (run_end - run_start) / 1e6);

    printf("\nper-callback exit counts (fallback counter harness):\n");
    for (size_t i = 0; i < slots.size(); ++i)
    {
        if (slots[i].count != 0)
        {
            printf("  [%3zu] %-28s %" PRIu64 "\n", i, slots[i].name.c_str(), slots[i].count);
        }
    }

    printf("\nfinal guest state: rip=0x%llx (stop protocol: %s)\n",
           static_cast<unsigned long long>(thread->CurrentFrame->State.rip),
           done ? "clean ThreadStopHandler unwind through exit stub" : "did not complete");

    hv_vcpu_destroy(vc.vcpu);
    hv_vm_destroy();
    return 0;
}
