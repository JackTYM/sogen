#pragma once

// EMULATOR_WAIT_STORM_DIAG=1: temporary diagnostic for the "82% of RIP samples land right after a
// wait/alert syscall" investigation. Measures, live, how often the four dominant wait syscalls
// (NtWaitForAlertByThreadId, NtDelayExecution, NtWaitForSingleObject, NtWaitForMultipleObjects32)
// are called, how quickly each resolves (near-instant vs. genuinely blocking), and how often/how
// expensive the scheduler's switch_to_next_thread scan is. All access happens under kernel_lock_
// (syscall handlers run inside the syscall hook's scoped_lock; the scheduler asserts the lock is
// held), so plain (non-atomic) state is safe here.

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <unordered_map>

namespace sogen::wait_storm_diag
{
    enum class wait_kind : uint8_t
    {
        alert = 0,
        delay,
        single_object,
        multi_object,
        count,
    };

    struct state
    {
        bool enabled = std::getenv("EMULATOR_WAIT_STORM_DIAG") != nullptr;

        std::array<uint64_t, static_cast<size_t>(wait_kind::count)> calls{};
        std::array<uint64_t, static_cast<size_t>(wait_kind::count)> instant{};
        std::array<uint64_t, static_cast<size_t>(wait_kind::count)> short_wait{};
        std::array<uint64_t, static_cast<size_t>(wait_kind::count)> blocking{};

        uint64_t switch_to_next_thread_calls{};
        uint64_t switch_scan_iterations{};
        uint64_t switch_scan_ns{};

        std::chrono::steady_clock::time_point last_report{std::chrono::steady_clock::now()};
        std::unordered_map<uint32_t, std::pair<wait_kind, std::chrono::steady_clock::time_point>> pending{};
    };

    inline state& get_state()
    {
        static state s{};
        return s;
    }

    inline bool enabled()
    {
        return get_state().enabled;
    }

    inline void record_wait_enter(const uint32_t thread_id, const wait_kind kind)
    {
        auto& s = get_state();
        if (!s.enabled)
        {
            return;
        }

        s.calls[static_cast<size_t>(kind)]++;
        s.pending[thread_id] = {kind, std::chrono::steady_clock::now()};
    }

    inline void record_wait_resolved(const uint32_t thread_id)
    {
        auto& s = get_state();
        if (!s.enabled)
        {
            return;
        }

        const auto it = s.pending.find(thread_id);
        if (it == s.pending.end())
        {
            return;
        }

        const auto kind = it->second.first;
        const auto elapsed_us =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - it->second.second).count();
        s.pending.erase(it);

        const auto idx = static_cast<size_t>(kind);
        if (elapsed_us < 200)
        {
            s.instant[idx]++;
        }
        else if (elapsed_us < 5000)
        {
            s.short_wait[idx]++;
        }
        else
        {
            s.blocking[idx]++;
        }
    }

    inline void note_switch_scan_iteration()
    {
        auto& s = get_state();
        if (!s.enabled)
        {
            return;
        }

        ++s.switch_scan_iterations;
    }

    // Accumulates the wall-clock cost of the scan itself, so the reported rate can be turned into a
    // real "how much host CPU is the scheduler burning" number rather than just a call count.
    struct scan_timer
    {
        bool active;
        std::chrono::steady_clock::time_point start;

        scan_timer()
            : active(get_state().enabled),
              start(active ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{})
        {
        }

        ~scan_timer()
        {
            if (active)
            {
                get_state().switch_scan_ns += static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
            }
        }

        scan_timer(const scan_timer&) = delete;
        scan_timer& operator=(const scan_timer&) = delete;
    };

    inline void note_switch_to_next_thread(const size_t thread_count)
    {
        auto& s = get_state();
        if (!s.enabled)
        {
            return;
        }

        ++s.switch_to_next_thread_calls;

        const auto now = std::chrono::steady_clock::now();
        const auto dt = std::chrono::duration<double>(now - s.last_report).count();
        if (dt < 5.0)
        {
            return;
        }

        static constexpr const char* names[] = {"AlertByThreadId", "DelayExecution", "WaitForSingleObject", "WaitForMultipleObjects32"};

        fprintf(stderr,
                "[WAIT_STORM_DIAG] --- %.1fs window, thread_count=%zu, switch_to_next_thread calls=%llu (%.0f/s) "
                "scan_iterations=%llu (%.0f/s, %.1f/call) scan_cost=%.1f ms/s (%.2f us/call) pending_waiters=%zu ---\n",
                dt, thread_count, static_cast<unsigned long long>(s.switch_to_next_thread_calls), s.switch_to_next_thread_calls / dt,
                static_cast<unsigned long long>(s.switch_scan_iterations), s.switch_scan_iterations / dt,
                s.switch_to_next_thread_calls
                    ? static_cast<double>(s.switch_scan_iterations) / static_cast<double>(s.switch_to_next_thread_calls)
                    : 0.0,
                static_cast<double>(s.switch_scan_ns) / 1e6 / dt,
                s.switch_to_next_thread_calls
                    ? static_cast<double>(s.switch_scan_ns) / 1e3 / static_cast<double>(s.switch_to_next_thread_calls)
                    : 0.0,
                s.pending.size());

        for (size_t i = 0; i < static_cast<size_t>(wait_kind::count); ++i)
        {
            if (s.calls[i] == 0)
            {
                continue;
            }

            fprintf(stderr, "[WAIT_STORM_DIAG] %-24s calls=%llu (%.0f/s) instant(<0.2ms)=%llu short(<5ms)=%llu blocking(>=5ms)=%llu\n",
                    names[i], static_cast<unsigned long long>(s.calls[i]), s.calls[i] / dt, static_cast<unsigned long long>(s.instant[i]),
                    static_cast<unsigned long long>(s.short_wait[i]), static_cast<unsigned long long>(s.blocking[i]));
        }

        s.calls.fill(0);
        s.instant.fill(0);
        s.short_wait.fill(0);
        s.blocking.fill(0);
        s.switch_to_next_thread_calls = 0;
        s.switch_scan_iterations = 0;
        s.switch_scan_ns = 0;
        s.last_report = now;
    }
}
