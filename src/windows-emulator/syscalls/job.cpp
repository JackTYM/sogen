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

            // JOBOBJECT_EXTENDED_LIMIT_INFORMATION wraps JOBOBJECT_BASIC_LIMIT_INFORMATION as its
            // first member, so LimitFlags sits at the same offset for both info classes: past the two
            // LARGE_INTEGER time limits.
            constexpr size_t JOB_OBJECT_LIMIT_FLAGS_OFFSET = 16;
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
                    ids.push_back(target->record_id);
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

                job->limit_flags = limit_flags;

                if ((limit_flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE) != 0)
                {
                    job->kill_on_close = true;
                }
            }

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
