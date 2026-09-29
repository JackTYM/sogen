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
                c.win_emu.log.error("Bad handle type for NtReleaseMutant\n");
                c.emu.stop();
                return STATUS_NOT_SUPPORTED;
            }

            auto* mutant = c.proc.mutants.get(mutant_handle);
            if (!mutant)
            {
                return STATUS_INVALID_HANDLE;
            }

            const auto [old_count, succeeded] = mutant->release(mutant::make_owner_key(c.proc.process_id, c.thread().id));

            if (previous_count)
            {
                previous_count.write(static_cast<LONG>(old_count));
            }

            if (succeeded)
            {
                record_object_signal(mutant_handle, c.thread().id, "NtReleaseMutant");

                for (auto& thread : c.proc.threads | std::views::values)
                {
                    if (std::ranges::find(thread.await_objects, mutant_handle) != thread.await_objects.end())
                    {
                        (void)thread.is_thread_ready(c.win_emu);
                    }
                }
            }

            return succeeded ? STATUS_SUCCESS : STATUS_MUTANT_NOT_OWNED;
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
    }

} // namespace sogen
