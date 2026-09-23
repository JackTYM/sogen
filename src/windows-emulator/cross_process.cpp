#include "std_include.hpp"
#include "cross_process.hpp"

#include "syscall_utils.hpp"

#include <cstdlib>

namespace sogen
{
    namespace
    {
        constexpr ACCESS_MASK MAXIMUM_ALLOWED = 0x02000000;
        constexpr ACCESS_MASK PROCESS_ALL_ACCESS = 0x001FFFFF;

        const char* process_control_op_name(const process_control_op op)
        {
            switch (op)
            {
            case process_control_op::read_memory:
                return "read_memory";
            case process_control_op::write_memory:
                return "write_memory";
            case process_control_op::allocate_memory:
                return "allocate_memory";
            case process_control_op::protect_memory:
                return "protect_memory";
            case process_control_op::free_memory:
                return "free_memory";
            case process_control_op::query_memory:
                return "query_memory";
            case process_control_op::terminate:
                return "terminate";
            case process_control_op::resume_thread:
                return "resume_thread";
            case process_control_op::adopt_section:
                return "adopt_section";
            case process_control_op::query_wow64_info:
                return "query_wow64_info";
            case process_control_op::query_cycle_time:
                return "query_cycle_time";
            case process_control_op::adopt_event:
                return "adopt_event";
            case process_control_op::adopt_mutant:
                return "adopt_mutant";
            case process_control_op::export_handle:
                return "export_handle";
            }

            return "unknown";
        }

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

    std::optional<process_control_response> send_process_control_request(const syscall_context& c, const child_target& target,
                                                                         const process_control_request& request, const int timeout_ms)
    {
        const auto trace = std::getenv("SOGEN_TRACE_XPROC_CTRL") != nullptr;

        if (trace)
        {
            c.win_emu.log.log("[XPROC_CTRL] -> child=%u op=%s address=0x%llx size=0x%llx allocation_type=0x%x "
                              "protection=0x%x free_type=0x%x info_class=0x%x exit_status=%d maximum_size=0x%llx "
                              "page_protection=0x%x allocation_attributes=0x%x granted_access=0x%x payload_size=%zu\n",
                              target.record_id, process_control_op_name(request.op), static_cast<unsigned long long>(request.address),
                              static_cast<unsigned long long>(request.size), request.allocation_type, request.protection, request.free_type,
                              request.info_class, request.exit_status, static_cast<unsigned long long>(request.maximum_size),
                              request.page_protection, request.allocation_attributes, request.granted_access, request.payload.size());
        }

        const auto response = target.channel->request(request, timeout_ms);

        if (trace)
        {
            if (!response)
            {
                c.win_emu.log.log("[XPROC_CTRL] <- child=%u op=%s TIMEOUT_OR_DEAD\n", target.record_id,
                                  process_control_op_name(request.op));
            }
            else
            {
                c.win_emu.log.log(
                    "[XPROC_CTRL] <- child=%u op=%s status=0x%x bytes_written=0x%llx base_address=0x%llx "
                    "region_size=0x%llx old_protection=0x%x previous_suspend_count=%u minted_handle_bits=0x%llx "
                    "exported_object_type=0x%x allocation_type=0x%x size=0x%llx maximum_size=0x%llx "
                    "page_protection=0x%x allocation_attributes=0x%x granted_access=0x%x payload_size=%zu\n",
                    target.record_id, process_control_op_name(request.op), static_cast<unsigned int>(response->status),
                    static_cast<unsigned long long>(response->bytes_written), static_cast<unsigned long long>(response->base_address),
                    static_cast<unsigned long long>(response->region_size), response->old_protection, response->previous_suspend_count,
                    static_cast<unsigned long long>(response->minted_handle_bits), response->exported_object_type,
                    response->allocation_type, static_cast<unsigned long long>(response->size),
                    static_cast<unsigned long long>(response->maximum_size), response->page_protection, response->allocation_attributes,
                    response->granted_access, response->payload.size());
            }
        }

        return response;
    }

} // namespace sogen
