#pragma once

#include "../windows_emulator.hpp"

#include <chrono>
#include <cstdlib>
#include <unordered_map>

namespace sogen::syscalls
{
    inline bool wait_trace_enabled()
    {
        static const bool enabled = std::getenv("SOGEN_TRACE_WAIT_TARGETS") != nullptr;
        return enabled;
    }

    struct wait_signal_record
    {
        uint32_t signaling_tid{};
        std::chrono::steady_clock::time_point time{};
        const char* syscall_name{};
    };

    inline std::unordered_map<uint64_t, wait_signal_record>& wait_signal_registry()
    {
        static std::unordered_map<uint64_t, wait_signal_record> registry{};
        return registry;
    }

    inline void record_object_signal(const handle h, const uint32_t signaling_tid, const char* syscall_name)
    {
        if (!wait_trace_enabled())
        {
            return;
        }

        wait_signal_registry()[h.bits & 0xFFFFFFFFULL] = {
            .signaling_tid = signaling_tid, .time = std::chrono::steady_clock::now(), .syscall_name = syscall_name};
    }

    struct wait_start_record
    {
        uint64_t object_key{};
        std::chrono::steady_clock::time_point time{};
    };

    inline std::unordered_map<uint32_t, wait_start_record>& wait_start_registry()
    {
        static std::unordered_map<uint32_t, wait_start_record> registry{};
        return registry;
    }

    inline void record_wait_start(const handle h, const uint32_t waiting_tid)
    {
        if (!wait_trace_enabled())
        {
            return;
        }

        wait_start_registry()[waiting_tid] = {.object_key = h.bits & 0xFFFFFFFFULL, .time = std::chrono::steady_clock::now()};
    }

    inline void report_wait_resolution(windows_emulator& win_emu, const uint32_t waiting_tid, const bool timed_out)
    {
        if (!wait_trace_enabled())
        {
            return;
        }

        auto& starts = wait_start_registry();
        const auto start_it = starts.find(waiting_tid);
        if (start_it == starts.end())
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration<double, std::milli>(now - start_it->second.time).count();

        if (timed_out)
        {
            win_emu.log.error("[wait-resolution-trace] tid=%u resolved=timeout elapsed_ms=%.1f\n", waiting_tid, elapsed_ms);
            starts.erase(start_it);
            return;
        }

        auto& signals = wait_signal_registry();
        const auto signal_it = signals.find(start_it->second.object_key);
        if (signal_it == signals.end() || signal_it->second.time < start_it->second.time)
        {
            win_emu.log.error("[wait-resolution-trace] tid=%u resolved=signal elapsed_ms=%.1f signaled_by=unknown-or-pre-signaled\n",
                              waiting_tid, elapsed_ms);
        }
        else
        {
            const auto signal_delay_ms = std::chrono::duration<double, std::milli>(signal_it->second.time - start_it->second.time).count();
            const auto delivery_delay_ms = elapsed_ms - signal_delay_ms;
            win_emu.log.error(
                "[wait-resolution-trace] tid=%u resolved=signal elapsed_ms=%.1f signaled_by_tid=%u via=%s signal_delay_ms=%.1f "
                "delivery_delay_ms=%.1f\n",
                waiting_tid, elapsed_ms, signal_it->second.signaling_tid, signal_it->second.syscall_name, signal_delay_ms,
                delivery_delay_ms);
        }

        starts.erase(start_it);
    }
}
