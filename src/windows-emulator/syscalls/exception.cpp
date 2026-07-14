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

                // The SEH chain itself looks intact - dump the native (64-bit) stack too, so real
                // ntdll's own internal call chain (RtlDispatchException, RtlpExecuteHandlerForException,
                // etc.) leading to this "no handler found" decision can be identified without symbols.
                const auto native_rsp = c.emu.reg<uint64_t>(x86_register::rsp);
                fprintf(stderr, "[SEHDIAG] native rsp=0x%llx rip=0x%llx\n", static_cast<unsigned long long>(native_rsp),
                        static_cast<unsigned long long>(c.emu.reg<uint64_t>(x86_register::rip)));
                for (uint64_t off = 0; off < 0x200; off += 8)
                {
                    uint64_t candidate = 0;
                    if (!c.emu.try_read_memory(native_rsp + off, &candidate, sizeof(candidate)))
                    {
                        continue;
                    }
                    const auto* mod = c.win_emu.mod_manager.find_by_address(candidate);
                    if (mod)
                    {
                        fprintf(stderr, "[SEHDIAG] stack[rsp+0x%llx]=0x%llx -> %s+0x%llx\n", static_cast<unsigned long long>(off),
                                static_cast<unsigned long long>(candidate), mod->name.c_str(),
                                static_cast<unsigned long long>(candidate - mod->image_base));

                        // wow64cpu.dll mediates 32/64-bit mode switches - a return address landing
                        // there is the strongest lead for "real ntdll tried to call back into 32-bit
                        // guest code (the SEH handler) and something about that transition failed."
                        // Dump a code window there (and any other candidate) so it can be disassembled
                        // directly, same approach that found the CONTEXT.Rsp bug.
                        if (mod->name == "wow64cpu.dll" || mod->name == "wow64.dll")
                        {
                            constexpr uint64_t k_lead_in = 0x30;
                            constexpr uint64_t k_trail = 0x30;
                            std::array<uint8_t, k_lead_in + k_trail> window{};
                            const auto window_start = candidate - k_lead_in;
                            if (c.emu.try_read_memory(window_start, window.data(), window.size()))
                            {
                                fprintf(stderr, "[SEHDIAG] code window at 0x%llx (%s+0x%llx-0x%llx..+0x%llx): ",
                                        static_cast<unsigned long long>(window_start), mod->name.c_str(),
                                        static_cast<unsigned long long>(candidate - mod->image_base),
                                        static_cast<unsigned long long>(k_lead_in), static_cast<unsigned long long>(k_trail));
                                for (const auto b : window)
                                {
                                    fprintf(stderr, "%02x ", b);
                                }
                                fprintf(stderr, "\n");
                            }
                        }
                    }
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
