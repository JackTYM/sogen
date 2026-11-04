#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "../cross_process.hpp"

#include <algorithm>

namespace sogen
{

    namespace syscalls
    {
        namespace
        {
            constexpr uint32_t JOB_OBJECT_BASIC_LIMIT_INFORMATION_CLASS = 2;
            constexpr uint32_t JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS = 9;
            constexpr uint32_t JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000;
            constexpr uint32_t JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x00000800;
            constexpr uint32_t JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK = 0x00001000;
            constexpr uint32_t JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION = 0x00000400;
            constexpr uint32_t JOB_OBJECT_LIMIT_ACTIVE_PROCESS = 0x00000008;
            constexpr uint32_t JOB_OBJECT_LIMIT_PROCESS_MEMORY = 0x00000100;
            constexpr uint32_t SUPPORTED_JOB_OBJECT_LIMIT_FLAGS =
                JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK |
                JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION | JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;

            // JOBOBJECT_EXTENDED_LIMIT_INFORMATION wraps JOBOBJECT_BASIC_LIMIT_INFORMATION as its
            // first member, so LimitFlags sits at the same offset for both info classes: past the two
            // LARGE_INTEGER time limits.
            constexpr size_t JOB_OBJECT_LIMIT_FLAGS_OFFSET = 16;
            constexpr size_t JOB_OBJECT_ACTIVE_PROCESS_LIMIT_OFFSET = 40;
            constexpr size_t JOB_OBJECT_PROCESS_MEMORY_LIMIT_OFFSET = 112;
            constexpr size_t JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_SIZE = 144;

            bool is_live_child(const process_context& proc, const uint32_t record_id)
            {
                const auto child = proc.child_processes.find(record_id);
                return child != proc.child_processes.end() && child->second.exit_status == STATUS_PENDING;
            }

            size_t count_active_processes(const process_context& proc, const job_object& job)
            {
                const auto live_children =
                    std::ranges::count_if(job.assigned_child_record_ids, [&](const uint32_t id) { return is_live_child(proc, id); });
                return static_cast<size_t>(live_children) + (job.owner_process_assigned ? 1 : 0);
            }

            bool has_room_for_another_process(const process_context& proc, const job_object& job)
            {
                return !job.limits.active_processes || count_active_processes(proc, job) < *job.limits.active_processes;
            }

            void apply_limits_to_child(const syscall_context& c, const child_target& target, const job_limits& limits)
            {
                utils::buffer_serializer buffer{};
                buffer.write(limits);

                process_control_request request{};
                request.op = process_control_op::apply_job_limits;
                request.payload = buffer.move_buffer();
                send_process_control_request(c, target, request);
            }

            void apply_limits_to_members(const syscall_context& c, const job_object& job)
            {
                if (job.owner_process_assigned)
                {
                    c.proc.assigned_job_limits = job.limits;
                }

                for (const auto record_id : job.assigned_child_record_ids)
                {
                    if (!is_live_child(c.proc, record_id))
                    {
                        continue;
                    }

                    const auto child = resolve_child_target(c, make_pseudo_handle(record_id, handle_types::process), 0);
                    if (const auto* target = std::get_if<child_target>(&child))
                    {
                        apply_limits_to_child(c, *target, job.limits);
                    }
                }
            }

            job_limits read_job_limits(const syscall_context& c, const uint64_t information, const uint32_t limit_flags)
            {
                job_limits limits{};

                if ((limit_flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS) != 0)
                {
                    limits.active_processes = c.emu.read_memory<uint32_t>(information + JOB_OBJECT_ACTIVE_PROCESS_LIMIT_OFFSET);
                }

                if ((limit_flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY) != 0)
                {
                    limits.process_memory = c.emu.read_memory<uint64_t>(information + JOB_OBJECT_PROCESS_MEMORY_LIMIT_OFFSET);
                }

                return limits;
            }
        }

        NTSTATUS handle_NtCreateJobObject(const syscall_context& c, const emulator_object<handle> job_handle,
                                          const ACCESS_MASK /*desired_access*/,
                                          const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            job_object job{};

            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                if (attributes.ObjectName)
                {
                    job.name = read_unicode_string(c.emu, attributes.ObjectName);
                }
            }

            if (!job.name.empty())
            {
                for (auto& entry : c.proc.jobs)
                {
                    if (entry.second.name == job.name)
                    {
                        ++entry.second.ref_count;
                        job_handle.write(c.proc.jobs.make_handle(entry.first));
                        return STATUS_OBJECT_NAME_EXISTS;
                    }
                }
            }

            const auto handle = c.proc.jobs.store(std::move(job));
            job_handle.write(handle);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtAssignProcessToJobObject(const syscall_context& c, const handle job_handle, const handle process_handle)
        {
            auto* job = c.proc.jobs.get(job_handle);
            if (job_handle.value.type != handle_types::job || !job)
            {
                return STATUS_INVALID_HANDLE;
            }

            if (c.proc.is_current_process_handle(process_handle))
            {
                if (!job->owner_process_assigned)
                {
                    if (!has_room_for_another_process(c.proc, *job))
                    {
                        return STATUS_QUOTA_EXCEEDED;
                    }

                    job->owner_process_assigned = true;
                    c.proc.assigned_job_limits = job->limits;
                }

                return STATUS_SUCCESS;
            }

            if (process_handle.value.type != handle_types::process)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto child = resolve_child_target(c, process_handle, 0);
            if (const auto* target = std::get_if<child_target>(&child))
            {
                auto& ids = job->assigned_child_record_ids;
                if (std::ranges::find(ids, target->record_id) == ids.end())
                {
                    if (!has_room_for_another_process(c.proc, *job))
                    {
                        return STATUS_QUOTA_EXCEEDED;
                    }

                    ids.push_back(target->record_id);
                    apply_limits_to_child(c, *target, job->limits);
                }
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationJobObject(const syscall_context& c, const handle job_handle,
                                                  const uint32_t job_object_information_class, const uint64_t job_object_information,
                                                  const uint32_t job_object_information_length)
        {
            auto* job = c.proc.jobs.get(job_handle);
            if (job_handle.value.type != handle_types::job || !job)
            {
                return STATUS_INVALID_HANDLE;
            }

            const bool carries_limit_flags = job_object_information_class == JOB_OBJECT_BASIC_LIMIT_INFORMATION_CLASS ||
                                             job_object_information_class == JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS;

            if (carries_limit_flags && job_object_information_length >= JOB_OBJECT_LIMIT_FLAGS_OFFSET + sizeof(uint32_t))
            {
                uint32_t limit_flags{};
                c.emu.read_memory(job_object_information + JOB_OBJECT_LIMIT_FLAGS_OFFSET, &limit_flags, sizeof(limit_flags));

                if (std::getenv("SOGEN_DEBUG_JOB_OBJECT_INFO") != nullptr)
                {
                    fprintf(stderr, "[JOB_OBJECT_SET] class=%u length=%u limit_flags=0x%x\n", job_object_information_class,
                            job_object_information_length, limit_flags);
                    fflush(stderr);
                }

                if ((limit_flags & ~SUPPORTED_JOB_OBJECT_LIMIT_FLAGS) != 0)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                const bool limits_active_processes = (limit_flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS) != 0;
                if (limits_active_processes && job_object_information_length < JOB_OBJECT_ACTIVE_PROCESS_LIMIT_OFFSET + sizeof(uint32_t))
                {
                    return STATUS_INFO_LENGTH_MISMATCH;
                }

                const bool limits_process_memory = (limit_flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY) != 0;
                if (limits_process_memory && (job_object_information_class != JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS ||
                                              job_object_information_length < JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_SIZE))
                {
                    return STATUS_NOT_SUPPORTED;
                }

                job->limit_flags = limit_flags;
                job->limits = read_job_limits(c, job_object_information, limit_flags);
                apply_limits_to_members(c, *job);

                if ((limit_flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) != 0)
                {
                    job->kill_on_close = true;
                }
            }

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
