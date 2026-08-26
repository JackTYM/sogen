#include "std_include.hpp"
#include "cross_process.hpp"

#include "syscall_utils.hpp"

namespace sogen
{
    std::variant<child_target, NTSTATUS> resolve_child_target(const syscall_context& c, const handle h, const ACCESS_MASK required_access)
    {
        const auto is_process_handle = h.value.is_pseudo && h.value.type == handle_types::process;
        const auto is_thread_handle = h.value.is_pseudo && h.value.type == handle_types::thread;

        if (!is_process_handle && !is_thread_handle)
        {
            return STATUS_NOT_SUPPORTED;
        }

        const auto record_id = static_cast<uint32_t>(h.value.id);

        const auto record_entry = c.proc.child_processes.find(record_id);
        if (record_entry == c.proc.child_processes.end())
        {
            return STATUS_INVALID_HANDLE;
        }

        auto* const channel = c.win_emu.find_child_control_channel(record_id);
        if (!channel)
        {
            return STATUS_NOT_SUPPORTED;
        }

        if ((record_entry->second.granted_access & required_access) != required_access)
        {
            return STATUS_ACCESS_DENIED;
        }

        return child_target{record_id, channel};
    }

} // namespace sogen
