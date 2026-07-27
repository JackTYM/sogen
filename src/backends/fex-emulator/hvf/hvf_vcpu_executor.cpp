#ifdef __APPLE__

#include "hvf_vcpu_executor.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace sogen::fex::hvf
{
    namespace
    {
        constexpr auto sys_reg_actlr_el1 = static_cast<hv_sys_reg_t>(0xc081);

        // T0SZ=16 (48-bit VA), IRGN0/ORGN0=WB-WA, SH0=inner, TG0=4KB, EPD1=1, IPS=40-bit.
        constexpr uint64_t tcr_el1_value = 0x200803510ull;
        constexpr uint64_t mair_el1_value = 0xFF; // attr0 = Normal WB RW-alloc
        constexpr uint64_t cpsr_el1h_daif_masked = 0x3c5;

        void check_hv(const char* what, const hv_return_t r)
        {
            if (r != HV_SUCCESS)
            {
                char buf[192];
                snprintf(buf, sizeof(buf), "HVF vcpu: %s failed: 0x%x", what, static_cast<uint32_t>(r));
                throw std::runtime_error(buf);
            }
        }

        uint32_t esr_ec(const uint64_t syndrome)
        {
            return static_cast<uint32_t>((syndrome >> 26) & 0x3F);
        }
    }

    // Replays the guest's C-ABI argument registers (x0-x8, q0-q7) into host registers, calls the
    // host function a rewritten JITPointers slot referred to, and captures the C-ABI result
    // registers (x0, x1, q0, q1). The VM exit/enter round trip preserves every other guest
    // register, so this reproduces the exact ABI the JIT-emitted call site expects.
    extern "C" void sogen_hvf_host_call_trampoline(uint64_t* gprs, uint8_t* vecs, uint64_t target);
    __asm__(".text\n"
            ".p2align 2\n"
            ".globl _sogen_hvf_host_call_trampoline\n"
            "_sogen_hvf_host_call_trampoline:\n"
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

    hvf_vcpu_executor::hvf_vcpu_executor(hvf_vm& vm)
        : vm_(vm)
    {
        check_hv("hv_vcpu_create", hv_vcpu_create(&this->vcpu_, &this->exit_, nullptr));

        uint64_t actlr = 0;
        check_hv("get ACTLR_EL1", hv_vcpu_get_sys_reg(this->vcpu_, sys_reg_actlr_el1, &actlr));
        check_hv("set ACTLR_EL1.EnTSO", hv_vcpu_set_sys_reg(this->vcpu_, sys_reg_actlr_el1, actlr | (1ull << 1)));
        uint64_t readback = 0;
        check_hv("readback ACTLR_EL1", hv_vcpu_get_sys_reg(this->vcpu_, sys_reg_actlr_el1, &readback));
        if (((readback >> 1) & 1) != 1)
        {
            throw std::runtime_error("HVF vcpu: EnTSO did not stick on this vCPU");
        }

        uint64_t mmfr0 = 0;
        check_hv("get ID_AA64MMFR0_EL1", hv_vcpu_get_sys_reg(this->vcpu_, HV_SYS_REG_ID_AA64MMFR0_EL1, &mmfr0));
        const uint32_t tgran4 = (mmfr0 >> 28) & 0xF;
        if (tgran4 != 0 && tgran4 != 1)
        {
            throw std::runtime_error("HVF vcpu: 4KB stage-1 granule unsupported");
        }

        check_hv("set MAIR_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_MAIR_EL1, mair_el1_value));
        check_hv("set TCR_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_TCR_EL1, tcr_el1_value));
        check_hv("set TTBR0_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_TTBR0_EL1, this->vm_.stage1_root_ipa()));
        check_hv("set CPACR_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_CPACR_EL1, 3ull << 20));
        check_hv("set VBAR_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_VBAR_EL1, this->vm_.runtime().vector_table_va));

        uint64_t sctlr = 0;
        check_hv("get SCTLR_EL1", hv_vcpu_get_sys_reg(this->vcpu_, HV_SYS_REG_SCTLR_EL1, &sctlr));
        sctlr |= (1ull << 0) | (1ull << 2) | (1ull << 12); // M | C | I
        check_hv("set SCTLR_EL1", hv_vcpu_set_sys_reg(this->vcpu_, HV_SYS_REG_SCTLR_EL1, sctlr));

        check_hv("set CPSR", hv_vcpu_set_reg(this->vcpu_, HV_REG_CPSR, cpsr_el1h_daif_masked));
        check_hv("set vtimer offset", hv_vcpu_set_vtimer_offset(this->vcpu_, 0));
        check_hv("set FPCR", hv_vcpu_set_reg(this->vcpu_, HV_REG_FPCR, 0));
        check_hv("set FPSR", hv_vcpu_set_reg(this->vcpu_, HV_REG_FPSR, 0));
    }

    hvf_vcpu_executor::~hvf_vcpu_executor()
    {
        hv_vcpu_destroy(this->vcpu_);
    }

    void hvf_vcpu_executor::kick()
    {
        hv_vcpus_exit(&this->vcpu_, 1);
    }

    uint64_t hvf_vcpu_executor::get_gpr(const unsigned index) const
    {
        uint64_t value = 0;
        check_hv("hv_vcpu_get_reg", hv_vcpu_get_reg(this->vcpu_, static_cast<hv_reg_t>(HV_REG_X0 + index), &value));
        return value;
    }

    void hvf_vcpu_executor::set_gpr(const unsigned index, const uint64_t value)
    {
        check_hv("hv_vcpu_set_reg", hv_vcpu_set_reg(this->vcpu_, static_cast<hv_reg_t>(HV_REG_X0 + index), value));
    }

    uint64_t hvf_vcpu_executor::get_pc() const
    {
        uint64_t value = 0;
        check_hv("get PC", hv_vcpu_get_reg(this->vcpu_, HV_REG_PC, &value));
        return value;
    }

    void hvf_vcpu_executor::set_pc(const uint64_t value)
    {
        check_hv("set PC", hv_vcpu_set_reg(this->vcpu_, HV_REG_PC, value));
    }

    void hvf_vcpu_executor::get_simd(const unsigned index, void* out16) const
    {
        hv_simd_fp_uchar16_t value{};
        check_hv("hv_vcpu_get_simd_fp_reg",
                 hv_vcpu_get_simd_fp_reg(this->vcpu_, static_cast<hv_simd_fp_reg_t>(HV_SIMD_FP_REG_Q0 + index), &value));
        std::memcpy(out16, &value, sizeof(value));
    }

    void hvf_vcpu_executor::set_simd(const unsigned index, const void* in16)
    {
        hv_simd_fp_uchar16_t value{};
        std::memcpy(&value, in16, sizeof(value));
        check_hv("hv_vcpu_set_simd_fp_reg",
                 hv_vcpu_set_simd_fp_reg(this->vcpu_, static_cast<hv_simd_fp_reg_t>(HV_SIMD_FP_REG_Q0 + index), value));
    }

    uint64_t hvf_vcpu_executor::get_sys(const hv_sys_reg_t reg) const
    {
        uint64_t value = 0;
        check_hv("hv_vcpu_get_sys_reg", hv_vcpu_get_sys_reg(this->vcpu_, reg, &value));
        return value;
    }

    void hvf_vcpu_executor::set_sys(const hv_sys_reg_t reg, const uint64_t value)
    {
        check_hv("hv_vcpu_set_sys_reg", hv_vcpu_set_sys_reg(this->vcpu_, reg, value));
    }

    uint64_t hvf_vcpu_executor::esr_el1() const
    {
        return this->get_sys(HV_SYS_REG_ESR_EL1);
    }

    uint64_t hvf_vcpu_executor::elr_el1() const
    {
        return this->get_sys(HV_SYS_REG_ELR_EL1);
    }

    uint64_t hvf_vcpu_executor::far_el1() const
    {
        return this->get_sys(HV_SYS_REG_FAR_EL1);
    }

    uint64_t hvf_vcpu_executor::spsr_el1() const
    {
        return this->get_sys(HV_SYS_REG_SPSR_EL1);
    }

    void hvf_vcpu_executor::flush_stage1_tlb_if_stale()
    {
        const uint64_t generation = this->vm_.stage1_generation();
        if (generation == this->seen_stage1_generation_)
        {
            return;
        }

        const uint64_t saved_pc = this->get_pc();
        this->set_pc(this->vm_.runtime().tlbi_stub_va);
        for (;;)
        {
            check_hv("hv_vcpu_run(tlbi)", hv_vcpu_run(this->vcpu_));
            if (this->exit_->reason == HV_EXIT_REASON_VTIMER_ACTIVATED)
            {
                hv_vcpu_set_vtimer_mask(this->vcpu_, false);
                continue;
            }
            if (this->exit_->reason == HV_EXIT_REASON_CANCELED)
            {
                continue;
            }
            if (this->exit_->reason == HV_EXIT_REASON_EXCEPTION && esr_ec(this->exit_->exception.syndrome) == 0x16 &&
                (this->exit_->exception.syndrome & 0xFFFF) == hc_tlbi_done)
            {
                break;
            }
            this->fail_unhandled_exit("stage-1 TLB flush stub", this->exit_->exception.syndrome);
        }
        this->set_pc(saved_pc);
        this->seen_stage1_generation_ = generation;
    }

    void hvf_vcpu_executor::dispatch_callback(const uint16_t id)
    {
        hvf_callback_slot slot{};
        if (!this->vm_.lookup_callback(id, slot))
        {
            this->fail_unhandled_exit("unknown hypercall id", id);
        }

        uint64_t gprs[9];
        alignas(16) uint8_t vecs[8 * 16];

        for (unsigned i = 0; i < 9; ++i)
        {
            gprs[i] = this->get_gpr(i);
        }
        if (slot.needs_fp)
        {
            for (unsigned i = 0; i < 8; ++i)
            {
                this->get_simd(i, &vecs[i * 16]);
            }
        }
        else
        {
            std::memset(vecs, 0, sizeof(vecs));
        }

        sogen_hvf_host_call_trampoline(gprs, vecs, slot.original);

        this->set_gpr(0, gprs[0]);
        this->set_gpr(1, gprs[1]);
        if (slot.needs_fp)
        {
            this->set_simd(0, &vecs[0]);
            this->set_simd(1, &vecs[16]);
        }
    }

    void hvf_vcpu_executor::fail_unhandled_exit(const char* what, const uint64_t syndrome)
    {
        fprintf(stderr, "[FEX backend] HVF vcpu: unhandled exit (%s): reason=%u syndrome=0x%llx EC=0x%x VA=0x%llx IPA=0x%llx\n",
                what, this->exit_->reason, static_cast<unsigned long long>(syndrome), esr_ec(syndrome),
                static_cast<unsigned long long>(this->exit_->exception.virtual_address),
                static_cast<unsigned long long>(this->exit_->exception.physical_address));
        for (unsigned i = 0; i < 31; ++i)
        {
            uint64_t value = 0;
            hv_vcpu_get_reg(this->vcpu_, static_cast<hv_reg_t>(HV_REG_X0 + i), &value);
            fprintf(stderr, "  x%-2u = 0x%016llx%s", i, static_cast<unsigned long long>(value), (i % 3 == 2) ? "\n" : " ");
        }
        uint64_t pc = 0;
        uint64_t sp = 0;
        hv_vcpu_get_reg(this->vcpu_, HV_REG_PC, &pc);
        hv_vcpu_get_sys_reg(this->vcpu_, HV_SYS_REG_SP_EL1, &sp);
        fprintf(stderr, "\n  pc = 0x%016llx sp_el1 = 0x%016llx esr_el1 = 0x%016llx elr_el1 = 0x%016llx far_el1 = 0x%016llx\n",
                static_cast<unsigned long long>(pc), static_cast<unsigned long long>(sp),
                static_cast<unsigned long long>(this->esr_el1()), static_cast<unsigned long long>(this->elr_el1()),
                static_cast<unsigned long long>(this->far_el1()));
        throw std::runtime_error("HVF vcpu: unhandled VM exit");
    }

    void hvf_vcpu_executor::run_dispatch(const uint64_t entry_pc, const uint64_t frame_ptr, const uint64_t stack_top,
                                         hvf_exit_handler& handler)
    {
        this->set_gpr(0, frame_ptr);
        this->set_gpr(1, 0);
        this->set_gpr(30, this->vm_.runtime().done_stub_va);
        this->set_sys(HV_SYS_REG_SP_EL1, stack_top);
        this->set_pc(entry_pc);

        for (;;)
        {
            this->flush_stage1_tlb_if_stale();

            check_hv("hv_vcpu_run", hv_vcpu_run(this->vcpu_));

            if (this->exit_->reason == HV_EXIT_REASON_VTIMER_ACTIVATED)
            {
                hv_vcpu_set_vtimer_mask(this->vcpu_, false);
                continue;
            }
            if (this->exit_->reason == HV_EXIT_REASON_CANCELED)
            {
                continue;
            }
            if (this->exit_->reason != HV_EXIT_REASON_EXCEPTION)
            {
                this->fail_unhandled_exit("non-exception exit", 0);
            }

            const uint64_t syndrome = this->exit_->exception.syndrome;
            const uint32_t ec = esr_ec(syndrome);

            if (ec == 0x16) // HVC64
            {
                const auto id = static_cast<uint16_t>(syndrome & 0xFFFF);
                if (id == hc_done)
                {
                    return;
                }
                if (id >= hc_vector_base && id < hc_vector_base + 16)
                {
                    if (!handler.on_guest_exception(*this, id - hc_vector_base))
                    {
                        this->fail_unhandled_exit("guest EL1 exception", syndrome);
                    }
                    continue;
                }
                this->dispatch_callback(id);
                continue;
            }

            if (ec == 0x24 || ec == 0x20) // stage-2 data / instruction abort from a lower EL
            {
                if (!handler.on_stage2_abort(*this, this->exit_->exception.virtual_address,
                                             this->exit_->exception.physical_address, syndrome))
                {
                    this->fail_unhandled_exit("stage-2 abort", syndrome);
                }
                continue;
            }

            this->fail_unhandled_exit("unexpected exception class", syndrome);
        }
    }
}

#endif
