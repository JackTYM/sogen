#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace sogen
{

    // Wake channel for the scheduler's idle loop, which only re-evaluates thread readiness on a poll.
    // Whatever makes a parked thread runnable -- a host condition being satisfied
    // (emulator_thread::await_host_condition), or a guest thread signalling a kernel object another
    // vCPU's thread waits on -- signals here to make an idle vCPU re-scan now instead of at its next
    // poll interval.
    //
    // Deliberately independent of the kernel lock. Host-thread signalers must never block on the
    // emulator (docs/multi-vcpu-design.md, section 7.2); guest-side signalers hold the kernel lock,
    // and waiters have released it before they reach here -- so the only possible order is kernel lock
    // then this mutex, never the reverse, and the two can never form a cycle.
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
            this->bump();
            this->condition_.notify_all();
        }

        // Wakes a single waiter, for a transition that makes one thread runnable and therefore occupies
        // exactly one vCPU. Waking the rest only has them contend for the kernel lock to re-run a scan
        // that cannot find them work, which costs more than the poll of latency they are left with.
        void signal_one()
        {
            this->bump();
            this->condition_.notify_one();
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
        void bump()
        {
            const std::scoped_lock lock(this->mutex_);
            ++this->generation_;
        }

        std::mutex mutex_{};
        std::condition_variable condition_{};
        uint64_t generation_{};
    };

} // namespace sogen
