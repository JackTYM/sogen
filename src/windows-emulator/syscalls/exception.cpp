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
                                         const emulator_object<CONTEXT64> thread_context, const BOOLEAN handle_exception)
        {
            // SEHDIAG: real KiUserExceptionDispatcher reached this syscall deciding no SEH handler
            // caught the exception - dump the guest's exception-registration chain (FS:[0] for a
            // 32-bit wow64 thread) to see whether it looks intact, to investigate why a real __try/
            // __except in the guest isn't being found.
            {
                const auto fs_base = c.emu.get_segment_base(x86_register::fs);
                uint32_t seh_head = 0;
                const bool seh_head_ok = c.emu.try_read_memory(fs_base, &seh_head, sizeof(seh_head));
                fprintf(stderr, "[SEHDIAG] handle_exception=%d fs_base=0x%llx seh_head=0x%x(ok=%d) record=0x%llx ctx=0x%llx\n",
                        handle_exception ? 1 : 0, static_cast<unsigned long long>(fs_base), seh_head, seh_head_ok ? 1 : 0,
                        static_cast<unsigned long long>(exception_record.value()), static_cast<unsigned long long>(thread_context.value()));

                uint32_t chain_addr = seh_head;
                for (int i = 0; i < 8 && seh_head_ok && chain_addr != 0xFFFFFFFF && chain_addr != 0; ++i)
                {
                    struct
                    {
                        uint32_t next;
                        uint32_t handler;
                    } record{};

                    if (!c.emu.try_read_memory(chain_addr, &record, sizeof(record)))
                    {
                        fprintf(stderr, "[SEHDIAG] chain[%d] @0x%x unreadable\n", i, chain_addr);
                        break;
                    }

                    fprintf(stderr, "[SEHDIAG] chain[%d] @0x%x next=0x%x handler=0x%x\n", i, chain_addr, record.next, record.handler);
                    chain_addr = record.next;
                }
                fflush(stderr);
            }

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
