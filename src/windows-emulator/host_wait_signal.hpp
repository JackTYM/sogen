#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace sogen
{

    // Wake channel for a guest thread parked on a host condition (emulator_thread::await_host_condition).
    // The scheduler's idle loop re-evaluates those predicates on a poll, so whatever satisfies one
    // host-side signals here to make an idle vCPU re-scan now instead of at its next poll interval.
    //
    // Deliberately independent of the kernel lock. Signalers are host threads that must never block on
    // the emulator (docs/multi-vcpu-design.md, section 7.2), and waiters have released the kernel lock
    // before they reach here -- so this mutex is never held while acquiring the kernel lock, nor the
    // other way round, and the two can never form a cycle.
    class host_wait_signal
    {
      public:
        uint64_t generation()
        {
            const std::scoped_lock lock(this->mutex_);
            return this->generation_;
        }

        void signal()
        {
            {
                const std::scoped_lock lock(this->mutex_);
                ++this->generation_;
            }

            this->condition_.notify_all();
        }

        // Returns once signal() has run since `generation` was observed, or after `timeout` elapses.
        // Reading the generation before the readiness scan and passing it here is what closes the
        // lost-wakeup window: a signal racing the scan bumps it past `generation` and this returns at once.
        void wait_for(const uint64_t generation, const std::chrono::microseconds timeout)
        {
            std::unique_lock lock(this->mutex_);
            this->condition_.wait_for(lock, timeout, [&] { return this->generation_ != generation; });
        }

      private:
        std::mutex mutex_{};
        std::condition_variable condition_{};
        uint64_t generation_{};
    };

} // namespace sogen
