#pragma once

#include <atomic>
#include <cstdint>

#include <pthread.h>
#include <Hypervisor/Hypervisor.h>

namespace sogen::fex::hvf
{
    class vm;
    class vcpu;

    // Sets ACTLR_EL1.EnTSO, the x86-compatible memory ordering of Apple Silicon, and reports whether it took.
    bool enable_hardware_tso(hv_vcpu_t id);

    class vcpu_exit_handler
    {
      public:
        // Both are called with the PC at the faulting instruction, where the vCPU resumes unless the handler moves
        // it. An exception taken at EL1 is described by ESR_EL1 and FAR_EL1.
        virtual bool on_stage2_fault(vcpu& cpu, uint64_t address, uint64_t syndrome) = 0;
        virtual bool on_exception(vcpu& cpu) = 0;

      protected:
        ~vcpu_exit_handler() = default;
    };

    // Hypervisor.framework binds a vCPU to the thread that creates it: only that thread may run or destroy it.
    class vcpu
    {
      public:
        explicit vcpu(vm& machine);
        ~vcpu();

        vcpu(const vcpu&) = delete;
        vcpu& operator=(const vcpu&) = delete;
        vcpu(vcpu&&) = delete;
        vcpu& operator=(vcpu&&) = delete;

        bool runs_on_current_thread() const;

        // Calls the FEX dispatcher at entry as ExecuteDispatch would, on its own stack, and returns once the
        // dispatcher returns.
        void run(uint64_t entry, uint64_t frame, uint64_t stack_top, vcpu_exit_handler& handler);

        // Makes the running guest leave the VM. Callable from any thread.
        void kick();
        void invalidate_stage1();

        uint64_t gpr(uint32_t index) const;
        void set_gpr(uint32_t index, uint64_t value);
        uint64_t pc() const;
        void set_pc(uint64_t value);
        uint64_t sp() const;
        void set_sp(uint64_t value);
        void vector(uint32_t index, void* value) const;
        void set_vector(uint32_t index, const void* value);
        uint64_t sys_reg(hv_sys_reg_t reg) const;

      private:
        vm& vm_;
        hv_vcpu_t id_{};
        hv_vcpu_exit_t* exit_{};
        pthread_t owner_{};
        uint64_t seen_stage1_generation_{};
        std::atomic<bool> stage1_invalidated_{false};

        void set_sys_reg(hv_sys_reg_t reg, uint64_t value);
        void flush_stale_stage1();
        void call_host(uint16_t index);
        [[noreturn]] void fail(const char* what) const;
    };
}
