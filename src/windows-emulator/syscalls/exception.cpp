#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtRaiseHardError(const syscall_context& c, const NTSTATUS error_status, const ULONG number_of_parameters,
                                         const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> unicode_string_parameter_mask,
                                         const uint64_t parameters, const HARDERROR_RESPONSE_OPTION /*valid_response_option*/,
                                         const emulator_object<HARDERROR_RESPONSE> response)
        {
            if (response)
            {
                response.try_write(ResponseAbort);
            }

            if (number_of_parameters > 0)
            {
                std::array<uint64_t, 5> params{};
                const auto mask = unicode_string_parameter_mask.value();

                try
                {
                    if (c.emu.try_read_memory(parameters, params.data(), number_of_parameters * sizeof(uint64_t)))
                    {
                        for (ULONG i = 0; i < number_of_parameters; ++i)
                        {
                            if (mask & (1ull << i))
                            {
                                const auto message =
                                    read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, params[i]});
                                c.win_emu.log.error("Hard error parameter %u: %s\n", static_cast<uint32_t>(i), u16_to_u8(message).c_str());
                            }
                            else
                            {
                                c.win_emu.log.error("Hard error parameter %u: 0x%" PRIx64 "\n", static_cast<uint32_t>(i), params[i]);
                            }
                        }
                    }
                }
                catch (...)
                {
                    // ignore
                }
            }

            c.proc.exit_status = error_status;
            c.win_emu.callbacks.on_exception();
            c.emu.stop();

            return STATUS_SUCCESS;
        }

        NTSTATUS handle_NtRaiseException(const syscall_context& c,
                                         const emulator_object<EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>> /*exception_record*/,
                                         const emulator_object<CONTEXT64> /*thread_context*/, const BOOLEAN handle_exception)
        {
            if (handle_exception)
            {
                c.win_emu.log.error("Unhandled exceptions not supported yet!\n");
                c.emu.stop();
                return STATUS_NOT_SUPPORTED;
            }

            c.win_emu.callbacks.on_exception();
            c.emu.stop();

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
