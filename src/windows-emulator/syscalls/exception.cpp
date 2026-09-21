#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtRaiseHardError(const syscall_context& c, const NTSTATUS error_status, const ULONG number_of_parameters,
                                         const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*unicode_string_parameter_mask*/,
                                         const uint64_t parameters, const HARDERROR_RESPONSE_OPTION /*valid_response_option*/,
                                         const emulator_object<HARDERROR_RESPONSE> response)
        {
            if (response)
            {
                response.try_write(ResponseAbort);
            }

            if (getenv("SOGEN_DIAG382"))
            {
                c.win_emu.log.error("NtRaiseHardError: status=0x%X num_params=%u params=0x%llX\n", error_status, number_of_parameters,
                                    static_cast<unsigned long long>(parameters));
                for (ULONG i = 0; i < number_of_parameters && i < 4; ++i)
                {
                    uint64_t param_value = 0;
                    if (!c.emu.try_read_memory(parameters + i * sizeof(uint64_t), &param_value, sizeof(param_value)))
                    {
                        continue;
                    }

                    try
                    {
                        const auto message =
                            read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, param_value});
                        c.win_emu.log.error("  param[%u] as UNICODE_STRING: %s\n", i, u16_to_u8(message).c_str());
                    }
                    catch (...)
                    {
                        c.win_emu.log.error("  param[%u] raw=0x%llX (not a readable UNICODE_STRING)\n", i,
                                            static_cast<unsigned long long>(param_value));
                    }
                }
            }

            if (error_status & STATUS_SERVICE_NOTIFICATION && number_of_parameters >= 3)
            {
                std::array<uint64_t, 3> params = {0, 0, 0};

                try
                {
                    if (c.emu.try_read_memory(parameters, &params, sizeof(params)))
                    {
                        const auto message =
                            read_unicode_string(c.emu, emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>>{c.emu, params[0]});
                        c.win_emu.log.error("Error Message: %s\n", u16_to_u8(message).c_str());
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
                                         const emulator_object<EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>> exception_record,
                                         const emulator_object<CONTEXT64> /*thread_context*/, const BOOLEAN handle_exception)
        {
            if (handle_exception)
            {
                c.win_emu.log.error("Unhandled exceptions not supported yet!\n");
                c.emu.stop();
                return STATUS_NOT_SUPPORTED;
            }

            c.proc.exit_status = exception_record.read().ExceptionCode;
            c.win_emu.callbacks.on_exception();
            c.emu.stop();

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
