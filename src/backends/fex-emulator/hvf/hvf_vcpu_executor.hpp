#pragma once

#ifdef __APPLE__

#include <cstdint>

#include <Hypervisor/Hypervisor.h>

#include "hvf_vm.hpp"

namespace sogen::fex::hvf
{
    class hvf_vcpu_executor;

    // Backend-provided policy for the two exit classes that need guest-fault semantics. Handlers
    // run in ordinary host thread context; returning false marks the exit unhandled and the
    // executor fails loudly with a full register dump.
    struct hvf_exit_handler
    {
        virtual bool on_stage2_abort(hvf_vcpu_executor& vcpu, uint64_t va, uint64_t ipa, uint64_t syndrome) = 0;
        virtual bool on_guest_exception(hvf_vcpu_executor& vcpu, uint32_t vector_entry) = 0;

      protected:
        ~hvf_exit_handler() = default;
    };

    // One hv_vcpu, bound to the host thread that constructs it (Hypervisor.framework requires
    // create/run thread affinity). Owns EnTSO + stage-1 MMU system-register setup and the
    // dispatch/exit loop that stands in for FEXCore's ExecuteDispatch.
    class hvf_vcpu_executor
    {
      public:
        explicit hvf_vcpu_executor(hvf_vm& vm);
        ~hvf_vcpu_executor();

        hvf_vcpu_executor(const hvf_vcpu_executor&) = delete;
        hvf_vcpu_executor& operator=(const hvf_vcpu_executor&) = delete;

        // Enters the emitted FEX dispatcher exactly as ExecuteDispatch would (x0 = frame,
        // x1 = SingleInst(false), LR = exit-done stub, SP_EL1 = dedicated emulator stack) and
        // services exits until the dispatcher unwinds through the done stub.
        void run_dispatch(uint64_t entry_pc, uint64_t frame_ptr, uint64_t stack_top, hvf_exit_handler& handler);

        // Forces a running hv_vcpu_run to return; callable from any thread.
        void kick();

        uint64_t get_gpr(unsigned index) const;
        void set_gpr(unsigned index, uint64_t value);
        uint64_t get_pc() const;
        void set_pc(uint64_t value);
        void get_simd(unsigned index, void* out16) const;
        void set_simd(unsigned index, const void* in16);

        uint64_t esr_el1() const;
        uint64_t elr_el1() const;
        uint64_t far_el1() const;
        uint64_t spsr_el1() const;

      private:
        void flush_stage1_tlb_if_stale();
        void dispatch_callback(uint16_t id);
        [[noreturn]] void fail_unhandled_exit(const char* what, uint64_t syndrome);
        uint64_t get_sys(hv_sys_reg_t reg) const;
        void set_sys(hv_sys_reg_t reg, uint64_t value);

        hvf_vm& vm_;
        hv_vcpu_t vcpu_ = 0;
        hv_vcpu_exit_t* exit_ = nullptr;
        uint64_t seen_stage1_generation_ = 0;
    };
}

#endif
