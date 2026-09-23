#pragma once

#ifdef __APPLE__

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
// Hypervisor.framework does not exist on iOS at all (device or Simulator - no header, no
// binary, no third-party entitlement path), unlike the header-only gaps elsewhere in this
// backend. hvf_vcpu_executor's real implementation is Darwin-desktop-only and excluded from the
// iOS build entirely (see CMakeLists.txt) - g_hvf (fex_x86_64_emulator.cpp) can never become
// non-null on iOS, so every use of this class there is unreachable dead code. These placeholder
// types exist only so that dead code still parses.
using hv_vcpu_t = uint64_t;
struct hv_vcpu_exit_t;
enum class hv_sys_reg_t : uint32_t
{
};
#else
#include <Hypervisor/Hypervisor.h>
#endif

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

        // kick() plus a latch the run loop consumes before re-entering hv_vcpu_run, closing the
        // window where the loop has already sampled the stage-1 generation but has not yet entered
        // the guest - in that window a bare kick has nothing to cancel and would be lost.
        void kick_for_stage1_invalidation();

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

        // EMULATOR_FEX_HVF_SINGLESTEP_DIAG=1: temporary diagnostic. Lets the backend supply a
        // host-PC -> guest-RIP reconstruction (FEXCore::Context::RestoreRIPFromHostPC) without this
        // class depending on FEXCore directly.
        void set_singlestep_diag_rip_reconstructor(std::function<uint64_t(uint64_t host_pc)> fn);

      private:
        void flush_stage1_tlb_if_stale();
        void dispatch_callback(uint16_t id);
        void maybe_report_stats();
        [[noreturn]] void fail_unhandled_exit(const char* what, uint64_t syndrome);
        uint64_t get_sys(hv_sys_reg_t reg) const;
        void set_sys(hv_sys_reg_t reg, uint64_t value);

        void singlestep_diag_init_from_env();
        void singlestep_diag_maybe_arm();
        void singlestep_diag_on_step_exit();
        void singlestep_diag_finish();
        void singlestep_diag_set_active(bool enable);
        void singlestep_diag_arm_next_step();

        void rip_sample_init_from_env();
        void rip_sample_capture();
        void rip_sample_flush();

        hvf_vm& vm_;
        hv_vcpu_t vcpu_ = 0;
        hv_vcpu_exit_t* exit_ = nullptr;
        uint64_t seen_stage1_generation_ = 0;
        std::atomic<bool> stage1_invalidation_pending_{false};

        // EMULATOR_FEX_HVF_STATS=<seconds>: periodic VM-exit rate report on stderr.
        bool stats_enabled_ = false;
        uint64_t stats_interval_ns_ = 5'000'000'000ull;
        uint64_t stats_interval_start_ns_ = 0;
        uint64_t stats_vector_exits_ = 0;
        uint64_t stats_stage2_aborts_ = 0;
        std::array<uint64_t, hvf_guest_runtime::max_stubs> stats_callback_counts_{};

        struct singlestep_diag_sample
        {
            uint64_t t_ns;
            uint64_t guest_rip;
        };

        enum class singlestep_diag_phase
        {
            idle,
            stepping,
            finished
        };

        bool singlestep_diag_enabled_ = false;
        uint64_t singlestep_diag_delay_ns_ = 0;
        uint64_t singlestep_diag_max_duration_ns_ = 0;
        uint64_t singlestep_diag_max_steps_ = 0;
        std::string singlestep_diag_log_path_;
        bool singlestep_diag_exit_after_ = false;
        singlestep_diag_phase singlestep_diag_phase_ = singlestep_diag_phase::idle;
        uint64_t singlestep_diag_window_start_ns_ = 0;
        std::vector<singlestep_diag_sample> singlestep_diag_samples_;
        std::function<uint64_t(uint64_t)> singlestep_diag_rip_fn_;

        // EMULATOR_FEX_HVF_RIP_SAMPLE=1: records the guest RIP on every exit already caused by the
        // scheduler's periodic kick() (HV_EXIT_REASON_CANCELED), i.e. at the existing ~20ms quantum
        // cadence, instead of forcing single-instruction-granularity exits.
        bool rip_sample_enabled_ = false;
        std::string rip_sample_log_path_;
        std::vector<singlestep_diag_sample> rip_samples_;
    };
}

#endif
