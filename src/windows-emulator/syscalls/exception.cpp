#include "../std_include.hpp"
#include "../emulator_utils.hpp"
#include "../syscall_utils.hpp"
#include "../cpu_context.hpp"
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
            if (number_of_parameters > MAXIMUM_HARDERROR_PARAMETERS)
            {
                return STATUS_INVALID_PARAMETER_2;
            }

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

            if (std::getenv("SOGEN_TRACE_HARDERROR_STACK") != nullptr)
            {
                const auto rip = c.emu.read_instruction_pointer();
                const auto rsp = c.emu.read_stack_pointer();
                const auto* rip_mod = c.win_emu.mod_manager.find_by_address(rip);
                fprintf(stderr, "[HARDERROR_STACK] pid=%u tid=%u status=0x%X rip=0x%llx (%s+0x%llx) rsp=0x%llx\n", c.proc.process_id,
                        c.thread().id, error_status, static_cast<unsigned long long>(rip), rip_mod ? rip_mod->name.c_str() : "?",
                        rip_mod ? static_cast<unsigned long long>(rip - rip_mod->image_base) : 0ULL, static_cast<unsigned long long>(rsp));
                for (const auto& mod : c.win_emu.mod_manager.modules() | std::views::values)
                {
                    fprintf(stderr, "  [MODULE] %s base=0x%llx size=0x%llx\n", mod.name.c_str(),
                            static_cast<unsigned long long>(mod.image_base), static_cast<unsigned long long>(mod.size_of_image));
                }
                for (uint64_t i = 0; i < 128; ++i)
                {
                    uint64_t value{};
                    if (!c.win_emu.memory.try_read_memory(rsp + (i * 8), &value, sizeof(value)))
                    {
                        break;
                    }
                    const auto* mod = c.win_emu.mod_manager.find_by_address(value);
                    char mod_suffix[128] = {};
                    if (mod)
                    {
                        snprintf(mod_suffix, sizeof(mod_suffix), "%s+0x%llx", mod->name.c_str(),
                                 static_cast<unsigned long long>(value - mod->image_base));
                    }
                    fprintf(stderr, "  [rsp+0x%llx] = 0x%llx %s\n", static_cast<unsigned long long>(i * 8),
                            static_cast<unsigned long long>(value), mod_suffix);
                }
                fflush(stderr);
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

        // A debugger sees the exception first; when it handles it, the thread continues from the raising context, as
        // after NtContinue. A first chance it does not handle goes to the guest's handlers.
        NTSTATUS handle_NtRaiseException(const syscall_context& c,
                                         const emulator_object<EMU_EXCEPTION_RECORD<EmulatorTraits<Emu64>>> exception_record,
                                         const emulator_object<CONTEXT64> thread_context, const BOOLEAN first_chance)
        {
            const auto record = exception_record.read();

            // The guest passes the context of its own architecture, so the read has to match it: an ARM64 context is
            // smaller than a CONTEXT64 and reading the larger one can run off the end of its mapping.
            auto* arm64 = dynamic_cast<arm64_cpu*>(&c.emu);
            const auto arm64_raising_context = arm64 ? c.emu.read_memory<arm64_context>(thread_context.value()) : arm64_context{};
            const auto context = arm64 ? CONTEXT64{} : thread_context.read();

            // The raising context becomes the thread's state before the debugger is told, as it already is on the
            // hardware exception path. Otherwise the debugger's GetThreadContext shows this syscall's stub registers,
            // and a SetThreadContext it makes before DBG_CONTINUE is overwritten afterwards.
            if (c.win_emu.debugger().is_attached())
            {
                if (arm64)
                {
                    cpu_context::restore(*arm64, arm64_raising_context);
                }
                else
                {
                    cpu_context::restore(c.x64_cpu(), context);
                }
            }

            if (report_exception_to_debugger(c.win_emu, c.vcpu, record, first_chance != FALSE))
            {
                c.write_status = false;
                return STATUS_SUCCESS;
            }

            if (first_chance)
            {
                c.write_status = false;
                if (arm64)
                {
                    dispatch_exception_record(c.win_emu, c.vcpu, record, arm64_raising_context);
                }
                else
                {
                    dispatch_exception_record(c.win_emu, c.vcpu, record, context);
                }
                return STATUS_SUCCESS;
            }

            c.proc.exit_status = record.ExceptionCode;
            c.win_emu.callbacks.on_exception();
            c.emu.stop();

            return STATUS_SUCCESS;
        }
    }

} // namespace sogen
