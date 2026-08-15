#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace sogen
{

    namespace syscalls
    {
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
            if (job_handle.value.type != handle_types::job || !c.proc.jobs.get(job_handle))
            {
                return STATUS_INVALID_HANDLE;
            }

            if (process_handle.value.type != handle_types::process)
            {
                return STATUS_INVALID_HANDLE;
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetInformationJobObject(const syscall_context& c, const handle job_handle,
                                                  const uint32_t /*job_object_information_class*/,
                                                  const uint64_t /*job_object_information*/,
                                                  const uint32_t /*job_object_information_length*/)
        {
            if (job_handle.value.type != handle_types::job || !c.proc.jobs.get(job_handle))
            {
                return STATUS_INVALID_HANDLE;
            }

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
