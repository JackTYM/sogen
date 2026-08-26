#include "std_include.hpp"
#include "cross_process.hpp"

#include "syscall_utils.hpp"

namespace sogen
{
    namespace
    {
        constexpr ACCESS_MASK MAXIMUM_ALLOWED = 0x02000000;
        constexpr ACCESS_MASK PROCESS_ALL_ACCESS = 0x001FFFFF;

        // Steps 1-2 shared by resolve_child_target and resolve_child_record: recognizing h as one of
        // the process/thread pseudo handles NtCreateUserProcess mints, then looking up the record it
        // names. Neither the channel lookup nor the access-mask check belongs here - resolve_child_target
        // requires a live channel, resolve_child_record deliberately doesn't. Returns nullopt and sets
        // failure_status on any resolution failure - a plain uint32_t/NTSTATUS variant isn't usable
        // here since NTSTATUS is itself just a uint32_t typedef, which would make the two alternatives
        // indistinguishable to std::get/std::holds_alternative.
        std::optional<uint32_t> resolve_child_record_id(const syscall_context& c, const handle h, NTSTATUS& failure_status)
        {
            const auto is_process_handle = h.value.is_pseudo && h.value.type == handle_types::process;
            const auto is_thread_handle = h.value.is_pseudo && h.value.type == handle_types::thread;

            if (!is_process_handle && !is_thread_handle)
            {
                failure_status = STATUS_NOT_SUPPORTED;
                return std::nullopt;
            }

            const auto record_id = static_cast<uint32_t>(h.value.id);
            if (!c.proc.child_processes.contains(record_id))
            {
                failure_status = STATUS_INVALID_HANDLE;
                return std::nullopt;
            }

            return record_id;
        }
    }

    ACCESS_MASK resolve_granted_process_access(const ACCESS_MASK requested_access)
    {
        if (requested_access == MAXIMUM_ALLOWED || (requested_access & GENERIC_ALL) != 0)
        {
            return PROCESS_ALL_ACCESS;
        }

        return requested_access;
    }

    std::variant<child_target, NTSTATUS> resolve_child_target(const syscall_context& c, const handle h, const ACCESS_MASK required_access)
    {
        NTSTATUS failure_status{};
        const auto record_id = resolve_child_record_id(c, h, failure_status);
        if (!record_id)
        {
            return failure_status;
        }

        const auto& record = c.proc.child_processes.at(*record_id);

        auto* const channel = c.win_emu.find_child_control_channel(*record_id);
        if (!channel)
        {
            return STATUS_NOT_SUPPORTED;
        }

        if ((record.granted_access & required_access) != required_access)
        {
            return STATUS_ACCESS_DENIED;
        }

        return child_target{*record_id, channel};
    }

    std::variant<const process_context::child_process_record*, NTSTATUS> resolve_child_record(const syscall_context& c, const handle h,
                                                                                              const ACCESS_MASK required_access)
    {
        NTSTATUS failure_status{};
        const auto record_id = resolve_child_record_id(c, h, failure_status);
        if (!record_id)
        {
            return failure_status;
        }

        const auto& record = c.proc.child_processes.at(*record_id);

        if ((record.granted_access & required_access) != required_access)
        {
            return STATUS_ACCESS_DENIED;
        }

        return &record;
    }

} // namespace sogen
