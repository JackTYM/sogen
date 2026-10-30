#include "hvf_vcpu.hpp"
#include "hvf_vm.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace sogen::fex::hvf
{
    namespace
    {
        constexpr uint64_t actlr_entso = 1ULL << 1;

        // T0SZ=16 (48-bit addresses), write-back cacheable inner-shareable walks, 4KB granule, TTBR1 walks
        // disabled (EPD1), 40-bit output addresses (IPS).
        constexpr uint64_t tcr_el1 = 0x200803510ULL;
        constexpr uint64_t mair_el1 = 0xFF;
        constexpr uint64_t cpacr_el1_fp_enabled = 3ULL << 20;
        constexpr uint64_t sctlr_el1_mmu_and_caches = (1ULL << 0) | (1ULL << 2) | (1ULL << 12);
        constexpr uint64_t cpsr_el1h_interrupts_masked = 0x3C5;

        constexpr uint32_t ec_hvc64 = 0x16;
        constexpr uint32_t ec_instruction_abort_lower_el = 0x20;
        constexpr uint32_t ec_data_abort_lower_el = 0x24;

        void check_hv(const char* what, const hv_return_t result)
        {
            if (result != HV_SUCCESS)
            {
                std::array<char, 128> message{};
                snprintf(message.data(), message.size(), "HVF vCPU: %s failed: 0x%x", what, static_cast<uint32_t>(result));
                throw std::runtime_error(message.data());
            }
        }

        uint32_t exception_class(const uint64_t syndrome)
        {
            return static_cast<uint32_t>((syndrome >> 26) & 0x3F);
        }

        uint16_t hvc_immediate(const uint64_t syndrome)
        {
            return static_cast<uint16_t>(syndrome & 0xFFFF);
        }
    }

    bool enable_hardware_tso(const hv_vcpu_t id)
    {
        if (__builtin_available(macOS 15.0, *))
        {
            uint64_t actlr = 0;
            uint64_t readback = 0;
            return hv_vcpu_get_sys_reg(id, HV_SYS_REG_ACTLR_EL1, &actlr) == HV_SUCCESS &&
                   hv_vcpu_set_sys_reg(id, HV_SYS_REG_ACTLR_EL1, actlr | actlr_entso) == HV_SUCCESS &&
                   hv_vcpu_get_sys_reg(id, HV_SYS_REG_ACTLR_EL1, &readback) == HV_SUCCESS && (readback & actlr_entso) != 0;
        }

        return false;
    }

    vcpu::vcpu(vm& machine)
        : vm_(machine),
          owner_(pthread_self())
    {
        check_hv("hv_vcpu_create", hv_vcpu_create(&this->id_, &this->exit_, nullptr));

        try
        {
            if (!enable_hardware_tso(this->id_))
            {
                throw std::runtime_error("HVF vCPU: hardware TSO did not take");
            }

            this->set_sys_reg(HV_SYS_REG_MAIR_EL1, mair_el1);
            this->set_sys_reg(HV_SYS_REG_TCR_EL1, tcr_el1);
            this->set_sys_reg(HV_SYS_REG_TTBR0_EL1, this->vm_.stage1_root());
            this->set_sys_reg(HV_SYS_REG_CPACR_EL1, cpacr_el1_fp_enabled);
            this->set_sys_reg(HV_SYS_REG_VBAR_EL1, this->vm_.runtime().vector_table());
            this->set_sys_reg(HV_SYS_REG_SCTLR_EL1, this->sys_reg(HV_SYS_REG_SCTLR_EL1) | sctlr_el1_mmu_and_caches);
            check_hv("set CPSR", hv_vcpu_set_reg(this->id_, HV_REG_CPSR, cpsr_el1h_interrupts_masked));
            check_hv("set FPCR", hv_vcpu_set_reg(this->id_, HV_REG_FPCR, 0));
            check_hv("set FPSR", hv_vcpu_set_reg(this->id_, HV_REG_FPSR, 0));
        }
        catch (...)
        {
            hv_vcpu_destroy(this->id_);
            throw;
        }

        this->seen_stage1_generation_ = this->vm_.stage1_generation();
        this->vm_.attach(*this);
    }

    vcpu::~vcpu()
    {
        this->vm_.detach(*this);
        hv_vcpu_destroy(this->id_);
    }

    bool vcpu::runs_on_current_thread() const
    {
        return pthread_equal(this->owner_, pthread_self()) != 0;
    }

    void vcpu::kick()
    {
        hv_vcpus_exit(&this->id_, 1);
    }

    // The run loop only flushes between runs, and a kick that lands before the guest is entered has nothing to
    // interrupt, so the request is latched as well.
    void vcpu::invalidate_stage1()
    {
        this->stage1_invalidated_.store(true, std::memory_order_release);
        this->kick();
    }

    uint64_t vcpu::gpr(const uint32_t index) const
    {
        if (index >= 31)
        {
            return 0;
        }

        uint64_t value = 0;
        check_hv("get register", hv_vcpu_get_reg(this->id_, static_cast<hv_reg_t>(HV_REG_X0 + index), &value));
        return value;
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void vcpu::set_gpr(const uint32_t index, const uint64_t value)
    {
        if (index < 31)
        {
            check_hv("set register", hv_vcpu_set_reg(this->id_, static_cast<hv_reg_t>(HV_REG_X0 + index), value));
        }
    }

    uint64_t vcpu::pc() const
    {
        uint64_t value = 0;
        check_hv("get PC", hv_vcpu_get_reg(this->id_, HV_REG_PC, &value));
        return value;
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void vcpu::set_pc(const uint64_t value)
    {
        check_hv("set PC", hv_vcpu_set_reg(this->id_, HV_REG_PC, value));
    }

    uint64_t vcpu::sp() const
    {
        return this->sys_reg(HV_SYS_REG_SP_EL1);
    }

    void vcpu::set_sp(const uint64_t value)
    {
        this->set_sys_reg(HV_SYS_REG_SP_EL1, value);
    }

    void vcpu::vector(const uint32_t index, void* value) const
    {
        hv_simd_fp_uchar16_t raw{};
        check_hv("get vector register", hv_vcpu_get_simd_fp_reg(this->id_, static_cast<hv_simd_fp_reg_t>(HV_SIMD_FP_REG_Q0 + index), &raw));
        std::memcpy(value, &raw, sizeof(raw));
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void vcpu::set_vector(const uint32_t index, const void* value)
    {
        hv_simd_fp_uchar16_t raw{};
        std::memcpy(&raw, value, sizeof(raw));
        check_hv("set vector register", hv_vcpu_set_simd_fp_reg(this->id_, static_cast<hv_simd_fp_reg_t>(HV_SIMD_FP_REG_Q0 + index), raw));
    }

    uint64_t vcpu::sys_reg(const hv_sys_reg_t reg) const
    {
        uint64_t value = 0;
        check_hv("get system register", hv_vcpu_get_sys_reg(this->id_, reg, &value));
        return value;
    }

    // NOLINTNEXTLINE(readability-make-member-function-const)
    void vcpu::set_sys_reg(const hv_sys_reg_t reg, const uint64_t value)
    {
        check_hv("set system register", hv_vcpu_set_sys_reg(this->id_, reg, value));
    }

    void vcpu::fail(const char* what) const
    {
        std::array<char, 256> message{};
        snprintf(message.data(), message.size(), "HVF vCPU: unhandled exit (%s): reason=%u syndrome=0x%llx pc=0x%llx far=0x%llx", what,
                 this->exit_->reason, static_cast<unsigned long long>(this->exit_->exception.syndrome),
                 static_cast<unsigned long long>(this->pc()), static_cast<unsigned long long>(this->exit_->exception.virtual_address));
        throw std::runtime_error(message.data());
    }

    void vcpu::flush_stale_stage1()
    {
        const uint64_t generation = this->vm_.stage1_generation();
        if (generation == this->seen_stage1_generation_)
        {
            return;
        }

        const uint64_t resume_pc = this->pc();
        this->set_pc(this->vm_.runtime().tlb_flush_stub());
        for (;;)
        {
            check_hv("hv_vcpu_run", hv_vcpu_run(this->id_));
            if (this->exit_->reason == HV_EXIT_REASON_CANCELED)
            {
                continue;
            }

            const auto syndrome = this->exit_->exception.syndrome;
            if (this->exit_->reason == HV_EXIT_REASON_EXCEPTION && exception_class(syndrome) == ec_hvc64 &&
                hvc_immediate(syndrome) == tlb_flush_done_call)
            {
                break;
            }

            this->fail("stage-1 TLB flush");
        }

        this->set_pc(resume_pc);
        this->seen_stage1_generation_ = generation;
    }

    void vcpu::call_host(const uint16_t index)
    {
        const auto call = this->vm_.get_host_call(index);
        if (!call)
        {
            this->fail("unknown host call");
        }

        host_call_registers registers{};
        for (uint32_t i = 0; i < registers.x.size(); ++i)
        {
            registers.x[i] = this->gpr(i);
        }

        if (call->uses_vector_registers)
        {
            for (uint32_t i = 0; i < registers.q.size(); ++i)
            {
                this->vector(i, registers.q[i].data());
            }
        }

        sogen_fex_hvf_call_host(&registers, call->function);

        this->set_gpr(0, registers.x[0]);
        this->set_gpr(1, registers.x[1]);
        if (call->uses_vector_registers)
        {
            this->set_vector(0, registers.q[0].data());
            this->set_vector(1, registers.q[1].data());
        }
    }

    void vcpu::run(const uint64_t entry, const uint64_t frame, const uint64_t stack_top, vcpu_exit_handler& handler)
    {
        this->set_gpr(0, frame);
        this->set_gpr(1, 0);
        this->set_gpr(30, this->vm_.runtime().exit_stub());
        this->set_sp(stack_top);
        this->set_pc(entry);

        for (;;)
        {
            this->flush_stale_stage1();
            if (this->stage1_invalidated_.exchange(false, std::memory_order_acquire))
            {
                continue;
            }

            check_hv("hv_vcpu_run", hv_vcpu_run(this->id_));

            if (this->exit_->reason == HV_EXIT_REASON_CANCELED)
            {
                continue;
            }

            if (this->exit_->reason == HV_EXIT_REASON_VTIMER_ACTIVATED)
            {
                hv_vcpu_set_vtimer_mask(this->id_, false);
                continue;
            }

            if (this->exit_->reason != HV_EXIT_REASON_EXCEPTION)
            {
                this->fail("exit reason");
            }

            const auto syndrome = this->exit_->exception.syndrome;
            const auto ec = exception_class(syndrome);

            if (ec == ec_hvc64)
            {
                const auto call = hvc_immediate(syndrome);
                if (call == exit_call)
                {
                    return;
                }

                if (call >= first_vector_call && call < first_vector_call + vector_count)
                {
                    this->set_pc(this->sys_reg(HV_SYS_REG_ELR_EL1));
                    if (!handler.on_exception(*this))
                    {
                        this->fail("guest exception");
                    }

                    check_hv("set CPSR", hv_vcpu_set_reg(this->id_, HV_REG_CPSR, this->sys_reg(HV_SYS_REG_SPSR_EL1)));
                    continue;
                }

                this->call_host(call);
                continue;
            }

            if (ec == ec_data_abort_lower_el || ec == ec_instruction_abort_lower_el)
            {
                if (!handler.on_stage2_fault(*this, this->exit_->exception.virtual_address, syndrome))
                {
                    this->fail("stage-2 fault");
                }

                continue;
            }

            this->fail("exception class");
        }
    }
}
