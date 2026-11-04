#include "../std_include.hpp"
#include "../syscall_dispatcher.hpp"
#include "../cpu_context.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "wait_trace.hpp"
#include "../named_objects.hpp"

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtReleaseMutant(const syscall_context& c, const handle mutant_handle, const emulator_object<LONG> previous_count)
        {
            if (mutant_handle.value.type != handle_types::mutant)
            {
                return mutant_handle.value.type == handle_types::reserved ? STATUS_INVALID_HANDLE : STATUS_OBJECT_TYPE_MISMATCH;
            }

            auto* mutant = c.proc.mutants.get(mutant_handle);
            if (!mutant)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto [old_count, succeeded] = mutant->release(mutant::make_owner_key(c.proc.process_id, c.thread().id));

            if (!succeeded)
            {
                return mutant->abandoned() ? STATUS_ABANDONED : STATUS_MUTANT_NOT_OWNED;
            }

            if (previous_count)
            {
                previous_count.write(1 - static_cast<LONG>(old_count));
            }

            record_object_signal(mutant_handle, c, "NtReleaseMutant");

            for (auto& thread : c.proc.threads | std::views::values)
            {
                if (std::ranges::find(thread.await_objects, mutant_handle) != thread.await_objects.end())
                {
                    (void)thread.is_thread_ready(c.win_emu);
                }
            }

            if (old_count == 1)
            {
                c.win_emu.wake_idle_vcpu();
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtOpenMutant(const syscall_context& c, const emulator_object<handle> mutant_handle,
                                     const ACCESS_MASK /*desired_access*/,
                                     const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            std::u16string name{};
            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                if (attributes.ObjectName)
                {
                    name = read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, attributes.ObjectName});
                    c.win_emu.callbacks.on_generic_access("Opening mutant", name);
                }
            }

            if (name.empty())
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            for (auto& entry : c.proc.mutants)
            {
                if (object_names_equal(entry.second.name, name))
                {
                    ++entry.second.ref_count;
                    mutant_handle.write(c.proc.mutants.make_handle(entry.first));
                    return STATUS_SUCCESS;
                }
            }

            mutant shared{};
            const auto found = named_objects::open(c.proc, named_objects::kind::mutant, name, shared.state);
            if (NT_SUCCESS(found))
            {
                shared.name = std::move(name);
                mutant_handle.write(c.proc.mutants.store(std::move(shared)));
                return STATUS_SUCCESS;
            }

            if (found == STATUS_OBJECT_TYPE_MISMATCH)
            {
                return found;
            }

            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        NTSTATUS handle_NtCreateMutant(const syscall_context& c, const emulator_object<handle> mutant_handle,
                                       const ACCESS_MASK /*desired_access*/,
                                       const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                       const BOOLEAN initial_owner)
        {
            std::u16string name{};
            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                if (attributes.ObjectName)
                {
                    name = read_unicode_string(c.emu, attributes.ObjectName);
                    c.win_emu.callbacks.on_generic_access("Opening mutant", name);
                }
            }

            if (!name.empty())
            {
                for (auto& entry : c.proc.mutants)
                {
                    if (object_names_equal(entry.second.name, name))
                    {
                        ++entry.second.ref_count;
                        mutant_handle.write(c.proc.mutants.make_handle(entry.first));
                        return STATUS_OBJECT_NAME_EXISTS;
                    }
                }

                mutant shared{};
                const auto found = named_objects::open(c.proc, named_objects::kind::mutant, name, shared.state);
                if (found == STATUS_OBJECT_TYPE_MISMATCH)
                {
                    return found;
                }

                if (NT_SUCCESS(found))
                {
                    shared.name = std::move(name);
                    mutant_handle.write(c.proc.mutants.store(std::move(shared)));
                    return STATUS_OBJECT_NAME_EXISTS;
                }
            }

            mutant e{};
            e.name = std::move(name);
            e.inherit = object_attributes && (object_attributes.read().Attributes & OBJ_INHERIT) != 0;

            if (initial_owner)
            {
                e.try_lock(mutant::make_owner_key(c.proc.process_id, c.thread().id));
            }

            const auto [handle, stored] = c.proc.mutants.store_and_get(std::move(e));
            mutant_handle.write(handle);

            if (stored->name.empty())
            {
                return STATUS_SUCCESS;
            }

            return named_objects::publish(c.proc, named_objects::kind::mutant, stored->name, stored->state);
        }

        NTSTATUS handle_NtQueryMutant(const syscall_context& c, const handle mutant_handle, const ULONG mutant_information_class,
                                      const uint64_t mutant_information, const ULONG mutant_information_length,
                                      const emulator_object<ULONG> return_length)
        {
            ULONG required_length{};
            switch (mutant_information_class)
            {
            case MutantBasicInformation:
                required_length = sizeof(MUTANT_BASIC_INFORMATION);
                break;
            case MutantOwnerInformation:
                required_length = sizeof(MUTANT_OWNER_INFORMATION);
                break;
            default:
                return STATUS_INVALID_INFO_CLASS;
            }

            if (mutant_information_length != required_length)
            {
                return STATUS_INFO_LENGTH_MISMATCH;
            }

            if (const auto status = check_handle_type(mutant_handle, handle_types::mutant); status != STATUS_SUCCESS)
            {
                return status;
            }

            const auto* mutant = c.proc.mutants.get(mutant_handle);
            if (!mutant)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto locked_count = mutant->locked_count();
            const auto is_owned = locked_count > 0;
            const auto owner_key = mutant->owner();

            if (mutant_information_class == MutantBasicInformation)
            {
                emulator_object<MUTANT_BASIC_INFORMATION>{c.emu, mutant_information}.access([&](MUTANT_BASIC_INFORMATION& info) {
                    info.CurrentCount = 1 - static_cast<LONG>(locked_count);
                    info.OwnedByCaller = is_owned && owner_key == mutant::make_owner_key(c.proc.process_id, c.thread().id);
                    info.AbandonedState = mutant->abandoned();
                });
            }
            else
            {
                MUTANT_OWNER_INFORMATION info{};
                if (is_owned)
                {
                    info.ClientId.UniqueProcess = owner_key >> 24;
                    info.ClientId.UniqueThread = owner_key & 0xFFFFFF;
                }

                emulator_object<MUTANT_OWNER_INFORMATION>{c.emu, mutant_information}.write(info);
            }

            return_length.write_if_valid(required_length);
            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
