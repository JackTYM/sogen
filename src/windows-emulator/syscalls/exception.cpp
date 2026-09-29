#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "../exception_dispatch.hpp"

namespace sogen
{

    namespace syscalls
    {
        NTSTATUS handle_NtRaiseHardError(const syscall_context& c, const NTSTATUS error_status, const ULONG number_of_parameters,
                                         const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> /*unicode_string_parameter_mask*/,
                                         const uint64_t parameters, const HARDERROR_RESPONSE_OPTION /*valid_response_option*/,
                                         const emulator_object<HARDERROR_RESPONSE> response)
        {
            // Kernel: if ( a2 > 5 ) return 3221225712 = STATUS_INVALID_PARAMETER_2 (0xC00000F0)
            if (number_of_parameters > 5)
            {
                return STATUS_INVALID_PARAMETER_2;
            }

            if (response)
            {
                response.try_write(ResponseAbort);
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
                                         const emulator_object<CONTEXT64> thread_context, const BOOLEAN first_chance)
        {
            // FirstChance == TRUE is the normal case for every software-raised exception
            // (RtlRaiseException/RaiseException always pass TRUE here) and means the guest's own
            // SEH/C++ handler chain must get a chance to catch it via KiUserExceptionDispatcher -
            // it does NOT mean the exception is already known to be unhandled. Only a genuine
            // second-chance raise (FirstChance == FALSE, issued by the guest's own unhandled
            // exception filter once RtlDispatchException found no handler) means the process
            // should actually die.
            if (first_chance && exception_record && thread_context)
            {
                c.write_status = false;
                const auto record = exception_record.read();
                const auto ctx = thread_context.read();
                dispatch_raised_exception(c.win_emu, c.vcpu, record, ctx);
                return STATUS_SUCCESS;
            }

            NTSTATUS exception_code = STATUS_UNSUCCESSFUL;
            if (exception_record)
            {
                const auto record = exception_record.read();
                exception_code = static_cast<NTSTATUS>(record.ExceptionCode);
            }

            c.proc.exit_status = exception_code;
            c.win_emu.callbacks.on_exception();
            c.emu.stop();

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
