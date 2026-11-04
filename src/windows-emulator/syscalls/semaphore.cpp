#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "wait_trace.hpp"
#include "../named_objects.hpp"

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtOpenSemaphore(const syscall_context& c, const emulator_object<handle> semaphore_handle,
                                        const ACCESS_MASK /*desired_access*/,
                                        const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            if (!object_attributes)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = object_attributes.read();
            if (!attributes.ObjectName)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto name =
                read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, attributes.ObjectName});
            if (name.empty())
            {
                return STATUS_INVALID_PARAMETER;
            }

            for (auto& semaphore : c.proc.semaphores)
            {
                if (object_names_equal(semaphore.second.name, name))
                {
                    ++semaphore.second.ref_count;
                    semaphore_handle.write(c.proc.semaphores.make_handle(semaphore.first));
                    return STATUS_SUCCESS;
                }
            }

            semaphore shared{};
            const auto found = named_objects::open(c.proc, named_objects::kind::semaphore, name, shared.state);
            if (NT_SUCCESS(found))
            {
                shared.name = name;
                semaphore_handle.write(c.proc.semaphores.store(std::move(shared)));
                return STATUS_SUCCESS;
            }

            return found;
        }

        NTSTATUS handle_NtReleaseSemaphore(const syscall_context& c, const handle semaphore_handle, const LONG release_count,
                                           const emulator_object<LONG> previous_count)
        {
            if (release_count <= 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            if (semaphore_handle.value.type != handle_types::semaphore)
            {
                return STATUS_OBJECT_TYPE_MISMATCH;
            }

            auto* mutant = c.proc.semaphores.get(semaphore_handle);
            if (!mutant)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto [old_count, succeeded] = mutant->release(static_cast<ULONG>(release_count));
            if (!succeeded)
            {
                return STATUS_SEMAPHORE_LIMIT_EXCEEDED;
            }

            if (previous_count)
            {
                previous_count.write(static_cast<LONG>(old_count));
            }

            record_object_signal(semaphore_handle, c, "NtReleaseSemaphore");

            for (auto& thread : c.proc.threads | std::views::values)
            {
                if (std::ranges::find(thread.await_objects, semaphore_handle) != thread.await_objects.end())
                {
                    (void)thread.is_thread_ready(c.win_emu);
                }
            }

            c.win_emu.wake_idle_vcpu();
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateSemaphore(const syscall_context& c, const emulator_object<handle> semaphore_handle,
                                          const ACCESS_MASK /*desired_access*/,
                                          const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                          const LONG initial_count, const LONG maximum_count)
        {
            if (maximum_count <= 0 || initial_count < 0 || initial_count > maximum_count)
            {
                return STATUS_INVALID_PARAMETER;
            }

            semaphore s{};
            s.initialize(initial_count, maximum_count);

            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                s.inherit = (attributes.Attributes & OBJ_INHERIT) != 0;
                if (attributes.ObjectName)
                {
                    s.name = read_unicode_string(c.emu, attributes.ObjectName);
                }
            }

            if (!s.name.empty())
            {
                for (auto& entry : c.proc.semaphores)
                {
                    if (object_names_equal(entry.second.name, s.name))
                    {
                        ++entry.second.ref_count;
                        semaphore_handle.write(c.proc.semaphores.make_handle(entry.first));
                        return STATUS_OBJECT_NAME_EXISTS;
                    }
                }

                semaphore shared{};
                const auto found = named_objects::open(c.proc, named_objects::kind::semaphore, s.name, shared.state);
                if (found == STATUS_OBJECT_TYPE_MISMATCH)
                {
                    return found;
                }

                if (NT_SUCCESS(found))
                {
                    shared.name = std::move(s.name);
                    semaphore_handle.write(c.proc.semaphores.store(std::move(shared)));
                    return STATUS_OBJECT_NAME_EXISTS;
                }
            }

            const auto [handle, stored] = c.proc.semaphores.store_and_get(std::move(s));
            semaphore_handle.write(handle);

            if (stored->name.empty())
            {
                return STATUS_SUCCESS;
            }

            return named_objects::publish(c.proc, named_objects::kind::semaphore, stored->name, stored->state);
        }

        NTSTATUS handle_NtQuerySemaphore(const syscall_context& c, const handle semaphore_handle, const ULONG semaphore_information_class,
                                         const emulator_object<SEMAPHORE_BASIC_INFORMATION> semaphore_information,
                                         const ULONG semaphore_information_length, const emulator_object<ULONG> return_length)
        {
            if (semaphore_information_class != SemaphoreBasicInformation)
            {
                return STATUS_INVALID_INFO_CLASS;
            }

            if (semaphore_information_length != sizeof(SEMAPHORE_BASIC_INFORMATION))
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            if (const auto status = check_handle_type(semaphore_handle, handle_types::semaphore); status != STATUS_SUCCESS)
            {
                return status;
            }

            const auto* semaphore = c.proc.semaphores.get(semaphore_handle);
            if (!semaphore)
            {
                return STATUS_INVALID_HANDLE;
            }

            semaphore_information.write({
                .CurrentCount = static_cast<LONG>(semaphore->current_count()),
                .MaximumCount = static_cast<LONG>(semaphore->max_count()),
            });

            return_length.write_if_valid(sizeof(SEMAPHORE_BASIC_INFORMATION));
            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
