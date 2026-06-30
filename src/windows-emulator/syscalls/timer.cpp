#include "../std_include.hpp"
#include "../syscall_dispatcher.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include <utils/time.hpp>

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtQueryTimerResolution(const syscall_context&, const emulator_object<ULONG> maximum_time,
                                               const emulator_object<ULONG> minimum_time, const emulator_object<ULONG> current_time)
        {
            maximum_time.write_if_valid(0x0002625a);
            minimum_time.write_if_valid(0x00001388);
            current_time.write_if_valid(0x00002710);
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetTimerResolution(const syscall_context&, const ULONG /*desired_resolution*/, const BOOLEAN set_resolution,
                                             const emulator_object<ULONG> current_resolution)
        {
            current_resolution.write_if_valid(0x0002625a);
            if (!set_resolution)
            {
                return STATUS_TIMER_RESOLUTION_NOT_SET;
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateTimer2(const syscall_context& c, const emulator_object<handle> timer_handle, uint64_t /*reserved*/,
                                       const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes,
                                       ULONG /*attributes*/, ACCESS_MASK /*desired_access*/)
        {
            std::u16string name{};
            if (object_attributes)
            {
                const auto attributes = object_attributes.read();
                if (attributes.ObjectName)
                {
                    name = read_unicode_string(c.emu, attributes.ObjectName);
                    c.win_emu.callbacks.on_generic_access("Opening timer", name);
                }
            }

            if (!name.empty())
            {
                for (auto& entry : c.proc.timers)
                {
                    if (entry.second.name == name)
                    {
                        ++entry.second.ref_count;
                        timer_handle.write(c.proc.timers.make_handle(entry.first));
                        return STATUS_OBJECT_NAME_EXISTS;
                    }
                }
            }

            timer t{};
            t.name = std::move(name);

            const auto h = c.proc.timers.store(std::move(t));
            timer_handle.write(h);

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCreateTimer(const syscall_context& c, const emulator_object<handle> timer_handle, ACCESS_MASK desired_access,
                                      const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes, ULONG timer_type)
        {
            // Kernel validates: if ( a4 > 1 ) return 3221225714 (STATUS_INVALID_PARAMETER_4)
            if (timer_type > 1)
            {
                return STATUS_INVALID_PARAMETER;
            }
            return handle_NtCreateTimer2(c, timer_handle, 0, object_attributes, timer_type, desired_access);
        }

        NTSTATUS handle_NtOpenTimer(const syscall_context& c, const emulator_object<handle> timer_handle, ACCESS_MASK /*desired_access*/,
                                    const emulator_object<OBJECT_ATTRIBUTES<EmulatorTraits<Emu64>>> object_attributes)
        {
            if (!timer_handle)
            {
                return STATUS_ACCESS_VIOLATION;
            }

            if (!object_attributes)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attributes = object_attributes.read();

            if (!attributes.ObjectName)
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            auto name = read_unicode_string(c.emu, attributes.ObjectName);
            c.win_emu.callbacks.on_generic_access("Opening timer", name);

            if (name.empty())
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            for (auto& entry : c.proc.timers)
            {
                if (entry.second.name == name)
                {
                    ++entry.second.ref_count;
                    timer_handle.write(c.proc.timers.make_handle(entry.first));
                    return STATUS_SUCCESS;
                }
            }

            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        NTSTATUS handle_NtSetTimer(const syscall_context& c, handle timer_handle, uint64_t due_time, uint64_t /*apc_routine*/,
                                   uint64_t /*apc_context*/, BOOLEAN /*resume_timer*/, LONG period,
                                   const emulator_object<BOOLEAN> previous_state)
        {
            // Kernel: period < 0 → STATUS_INVALID_PARAMETER_6 (0xC00000F4)
            if (period < 0)
            {
                return STATUS_INVALID_PARAMETER_6;
            }
            previous_state.write_if_valid(FALSE);

            auto* t = c.proc.timers.get(timer_handle);
            if (t && due_time)
            {
                const emulator_object<LARGE_INTEGER> li{c.emu, due_time};
                t->signal_time = utils::convert_delay_interval_to_time_point(c.win_emu.clock(), li.read());
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetTimer2(const syscall_context& c, handle timer_handle, uint64_t due_time, uint64_t /*period*/,
                                    uint64_t parameters)
        {
            // Kernel: if (!a2) → STATUS_INVALID_PARAMETER_2 (0xC00000F0 = 3221225712)
            if (!parameters)
            {
                return STATUS_INVALID_PARAMETER_2;
            }

            auto* t = c.proc.timers.get(timer_handle);
            if (t && due_time)
            {
                const emulator_object<LARGE_INTEGER> li{c.emu, due_time};
                const auto li_val = li.read();
                printf("[timer-dbg] NtSetTimer2 due_time.QuadPart=%lld\n", static_cast<long long>(li_val.QuadPart));
                t->signal_time = utils::convert_delay_interval_to_time_point(c.win_emu.clock(), li_val);
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtSetTimerEx(const syscall_context& /*c*/, handle /*timer_handle*/, uint32_t /*timer_set_info_class*/,
                                     uint64_t /*timer_set_information*/, ULONG /*timer_set_information_length*/)
        {
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCancelTimer(const syscall_context& c, handle timer_handle, const emulator_object<BOOLEAN> current_state)
        {
            current_state.write_if_valid(FALSE);
            auto* t = c.proc.timers.get(timer_handle);
            if (t)
            {
                t->signal_time = {};
            }
            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtCancelTimer2(const syscall_context& c, handle timer_handle, uint64_t /*parameters*/)
        {
            auto* t = c.proc.timers.get(timer_handle);
            if (t)
            {
                t->signal_time = {};
            }
            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
