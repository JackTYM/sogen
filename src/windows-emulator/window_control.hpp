#pragma once

#include "process_control_channel.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace sogen
{
    class windows_emulator;
    struct syscall_context;

    enum class window_control_op : uint32_t
    {
        set_parent = 1,
        set_window_pos = 2,
    };

    struct window_pos_request
    {
        int32_t x{};
        int32_t y{};
        int32_t cx{};
        int32_t cy{};
        uint32_t flags{};
    };

    namespace syscalls
    {
        void execute_window_control(windows_emulator& target, const process_control_request& request, process_control_response& response);

        // Windows are owned by the host process that created them, and window handles carry a per-process
        // namespace (see user_handle_table), so a handle from another namespace names a window in a child.
        // Offers the operation to every child and returns the answer of the one that owns the window.
        std::optional<process_control_response> forward_window_control(const syscall_context& c, window_control_op op, uint64_t hwnd,
                                                                       uint64_t argument, std::span<const std::byte> payload = {});

        bool is_foreign_window_handle(const syscall_context& c, uint64_t hwnd);
    }
}
