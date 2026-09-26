#include "std_include.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "analysis.hpp"
#include "analysis_reporter.hpp"
#include "disassembler.hpp"
#include "windows_emulator.hpp"
#include <utils/lazy_object.hpp>

#if defined(OS_EMSCRIPTEN) && !defined(SOGEN_EMSCRIPTEN_SUPPORT_NODEJS)
#include <event_handler.hpp>
#endif

#define STR_VIEW_VA(str) static_cast<int>((str).size()), (str).data()

namespace sogen
{

    namespace
    {
        constexpr size_t MAX_INSTRUCTION_BYTES = 15;
        constexpr uint64_t SYSCALL_INSTRUCTION_SIZE = 2;

        template <typename Return, typename... Args>
        std::function<Return(Args...)> make_callback(analysis_context& c, Return (*callback)(analysis_context&, Args...))
        {
            return [&c, callback](Args... args) {
                return callback(c, std::forward<Args>(args)...); //
            };
        }

        template <typename Return, typename... Args>
        std::function<Return(Args...)> make_callback(analysis_context& c, Return (*callback)(const analysis_context&, Args...))
        {
            return [&c, callback](Args... args) {
                return callback(c, std::forward<Args>(args)...); //
            };
        }

        std::string get_instruction_string(const disassembler& d, x86_64_cpu& emu, const uint64_t address)
        {
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return {};
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            const auto instructions = d.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return {};
            }

            const auto& inst = instructions[0];
            return std::string(inst.mnemonic) + (strlen(inst.op_str) ? " "s + inst.op_str : "");
        }

        bool is_int_resource(const uint64_t address)
        {
            return (address >> 0x10) == 0;
        }

        template <typename CharType = char>
        std::string read_arg_as_string(windows_emulator& win_emu, const size_t index)
        {
            const auto var_ptr = get_function_argument(win_emu.emu(), index);
            if (!var_ptr || is_int_resource(var_ptr))
            {
                return {};
            }

            try
            {
                auto str = read_string<CharType>(win_emu.memory, var_ptr);
                if constexpr (std::is_same_v<CharType, char16_t>)
                {
                    return u16_to_u8(str);
                }
                else
                {
                    return str;
                }
            }
            catch (...)
            {
                return "[failed to read]";
            }
        }

        std::string read_module_name(windows_emulator& win_emu, const size_t index)
        {
            const auto var_ptr = get_function_argument(win_emu.emu(), index);
            if (!var_ptr)
            {
                return {};
            }

            return win_emu.mod_manager.find_name(var_ptr);
        }

        std::vector<function_execution_detail> collect_function_details(const analysis_context& c, const std::string_view function)
        {
            std::vector<function_execution_detail> details{};

            const auto push_detail = [&](std::string value, std::string label = {}) {
                if (!value.empty())
                {
                    details.emplace_back(function_execution_detail{.label = std::move(label), .value = std::move(value)});
                }
            };

            if (function == "GetEnvironmentVariableA"      //
                || function == "ExpandEnvironmentStringsA" //
                || function == "LoadLibraryA")
            {
                push_detail(read_arg_as_string(*c.win_emu, 0));
            }
            else if (function == "LoadLibraryW")
            {
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 0));
            }
            else if (function == "LoadImageW")
            {
                auto& emu = c.win_emu->emu();
                char buf[256]{};
                snprintf(buf, sizeof(buf), "hInst=0x%llx name=0x%llx type=%llu cx=%llu cy=%llu flags=0x%llx",
                         static_cast<unsigned long long>(get_function_argument(emu, 0)),
                         static_cast<unsigned long long>(get_function_argument(emu, 1)),
                         static_cast<unsigned long long>(get_function_argument(emu, 2)),
                         static_cast<unsigned long long>(get_function_argument(emu, 3)),
                         static_cast<unsigned long long>(get_function_argument(emu, 4)),
                         static_cast<unsigned long long>(get_function_argument(emu, 5)));
                push_detail(buf);
            }
            else if (function == "MessageBoxA")
            {
                push_detail(read_arg_as_string(*c.win_emu, 2));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }
            else if (function == "MessageBoxW")
            {
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 2));
                push_detail(read_arg_as_string<char16_t>(*c.win_emu, 1));
            }
            else if (function == "GetProcAddress")
            {
                push_detail(read_module_name(*c.win_emu, 0));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }
            else if (function == "WinVerifyTrust")
            {
                auto& emu = c.win_emu->emu();
                emu.reg(x86_register::rip, emu.read_stack(0));
                emu.reg(x86_register::rsp, emu.reg(x86_register::rsp) + 8);
                emu.reg(x86_register::rax, 0);
            }
            else if (function == "lstrcmp" || function == "lstrcmpi")
            {
                push_detail(read_arg_as_string(*c.win_emu, 0));
                push_detail(read_arg_as_string(*c.win_emu, 1));
            }

            return details;
        }

        void handle_suspicious_activity(const analysis_context& c, const std::string_view details)
        {
            std::string decoded_instruction{};
            const auto rip = c.win_emu->emu().read_instruction_pointer();

            if (details == "Illegal instruction")
            {
                decoded_instruction = get_instruction_string(c.d, c.win_emu->emu(), rip);
            }

            c.emit_observation<suspicious_activity_event>([&](auto& event) {
                event.details = std::string(details);
                event.decoded_instruction = std::move(decoded_instruction);
            });
        }

        void handle_debug_string(const analysis_context& c, const std::string_view details)
        {
            c.emit_observation<debug_string_event>([&](auto& event) { event.details = std::string(details); });
        }

        void handle_generic_activity(const analysis_context& c, const std::string_view details)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<generic_activity_event>([&](auto& event) { event.details = std::string(details); });
            }
        }

        void handle_generic_access(const analysis_context& c, const std::string_view type, const std::u16string_view name)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<generic_access_event>([&](auto& event) {
                    event.type = std::string(type);
                    event.name = u16_to_u8(name);
                });
            }
        }

        void handle_memory_allocate(const analysis_context& c, const uint64_t address, const uint64_t length,
                                    const memory_permission permission, const bool commit)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<memory_allocate_event>([&](auto& event) {
                    event.address = address;
                    event.length = length;
                    event.permissions = get_permission_string(permission);
                    event.commit = commit;
                });
            }
        }

        void handle_memory_protect(const analysis_context& c, const uint64_t address, const uint64_t length,
                                   const memory_permission permission)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<memory_protect_event>([&](auto& event) {
                    event.address = address;
                    event.length = length;
                    event.permissions = get_permission_string(permission);
                });
            }
        }

        void handle_memory_violate(const analysis_context& c, const uint64_t address, const uint64_t size, const memory_operation operation,
                                   const memory_violation_type type)
        {
            c.emit_observation<memory_violation_event>([&](auto& event) {
                event.address = address;
                event.size = size;
                event.operation = get_permission_string(operation);
                event.violation_type = type == memory_violation_type::protection ? "protection"s : "unmapped"s;
            });

            if (type == memory_violation_type::unmapped)
            {
                if (c.mapping_violation.first == address)
                {
                    if (++c.mapping_violation.second > 5)
                    {
                        throw std::runtime_error("Too many identical violations. Aborting...");
                    }
                }
                else
                {
                    c.mapping_violation.first = address;
                    c.mapping_violation.second = 1;
                }
            }
        }

        void handle_ioctrl(const analysis_context& c, const io_device&, const std::u16string_view device_name, const ULONG code)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<io_control_event>([&](auto& event) {
                    event.device_name = u16_to_u8(device_name);
                    event.code = static_cast<uint32_t>(code);
                });
            }
        }

        void handle_thread_create(const analysis_context& c, handle, emulator_thread& t)
        {
            if (c.settings->skip_generic_activity)
            {
                return;
            }

            std::vector<std::string> flags{};

            if (t.create_flags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED)
            {
                flags.emplace_back("suspended");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH)
            {
                flags.emplace_back("skip thread attach");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER)
            {
                flags.emplace_back("hide from debugger");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_LOADER_WORKER)
            {
                flags.emplace_back("loader worker");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_SKIP_LOADER_INIT)
            {
                flags.emplace_back("skip loader init");
            }
            if (t.create_flags & THREAD_CREATE_FLAGS_BYPASS_PROCESS_FREEZE)
            {
                flags.emplace_back("bypass process freeze");
            }

            c.emit_observation<thread_create_event>([&](auto& event) {
                event.created_thread_id = t.id;
                event.start_address = t.start_address;
                event.argument = t.argument;
                event.flags = std::move(flags);
            });
        }

        void handle_thread_terminated(const analysis_context& c, handle, emulator_thread& t)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<thread_terminated_event>([&](auto& event) { event.terminated_thread_id = t.id; });
            }
        }

        void handle_thread_set_name(const analysis_context& c, const emulator_thread& t)
        {
            c.emit_observation<thread_set_name_event>([&](auto& event) {
                event.renamed_thread_id = t.id;
                event.name = u16_to_u8(t.name);
            });
        }

        void handle_thread_switch(const analysis_context& c, const emulator_thread& current_thread, const emulator_thread& new_thread)
        {
            if (!c.settings->skip_generic_activity)
            {
                c.emit_observation<thread_switch_event>([&](auto& event) {
                    event.previous_thread_id = current_thread.id;
                    event.next_thread_id = new_thread.id;
                });
            }
        }

        void handle_module_load(const analysis_context& c, const mapped_module& mod)
        {
            c.emit_observation<module_load_event>([&](auto& event) {
                event.path = mod.module_path.string();
                event.image_base = mod.image_base;
            });
        }

        void handle_module_unload(const analysis_context& c, const mapped_module& mod)
        {
            c.emit_observation<module_unload_event>([&](auto& event) {
                event.path = mod.module_path.string();
                event.image_base = mod.image_base;
            });
        }

        void handle_fast_fail(const analysis_context& c, const uint32_t fail_code)
        {
            c.emit_observation<fast_fail_event>([&](auto& event) { event.fail_code = fail_code; });
        }

        bool is_thread_alive(const analysis_context& c, const uint32_t thread_id)
        {
            for (const auto& t : c.win_emu->process.threads | std::views::values)
            {
                if (t.id == thread_id)
                {
                    return true;
                }
            }

            return false;
        }

        void update_import_access(analysis_context& c, const uint64_t address)
        {
            if (c.accessed_imports.empty())
            {
                return;
            }

            const auto& t = c.win_emu->current_thread();
            for (auto entry = c.accessed_imports.begin(); entry != c.accessed_imports.end();)
            {
                auto& a = *entry;
                const auto is_same_thread = t.id == a.access_context.thread_id;

                if (is_same_thread && address == a.address)
                {
                    entry = c.accessed_imports.erase(entry);
                    continue;
                }

                constexpr auto inst_delay = 100u;
                const auto execution_delay_reached = is_same_thread && a.access_inst_count + inst_delay <= t.executed_instructions;

                if (!execution_delay_reached && is_thread_alive(c, a.access_context.thread_id))
                {
                    ++entry;
                    continue;
                }

                c.emit_observation<import_read_event>(a.access_context, [&](auto& event) {
                    event.resolved_address = a.address;
                    event.import_name = a.import_name;
                    event.import_module = a.import_module;
                });

                entry = c.accessed_imports.erase(entry);
            }
        }

        bool is_return(const disassembler& d, x86_64_cpu& emu, const uint64_t address)
        {
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return false;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            const auto instructions = d.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return false;
            }

            const auto handle = d.resolve_handle(emu, reg_cs);
            return cs_insn_group(handle, instructions.data(), CS_GRP_RET);
        }

        void record_instruction(analysis_context& c, const uint64_t address)
        {
            auto& emu = c.win_emu->emu();
            std::array<uint8_t, MAX_INSTRUCTION_BYTES> instruction_bytes{};
            const auto result = emu.try_read_memory(address, instruction_bytes.data(), instruction_bytes.size());
            if (!result)
            {
                return;
            }

            const auto reg_cs = emu.reg<uint16_t>(x86_register::cs);
            disassembler disasm{};
            const auto instructions = disasm.disassemble(emu, reg_cs, instruction_bytes, 1, address);
            if (instructions.empty())
            {
                return;
            }

            ++c.instructions[instructions[0].id];
        }

        uint64_t next_traced_call_count(analysis_context& c)
        {
            return ++c.traced_call_count;
        }

        bool break_before_traced_call(analysis_context& c, const uint64_t call_count)
        {
            if (!c.auto_break_before_call || *c.auto_break_before_call != call_count)
            {
                return false;
            }

            c.auto_break_before_call.reset();
            c.win_emu->stop();
            return true;
        }

        bool break_before_traced_syscall(analysis_context& c, const uint64_t call_count, const uint64_t address)
        {
            if (!break_before_traced_call(c, call_count))
            {
                return false;
            }

            c.syscall_to_resume_after_break = address;
            c.win_emu->emu().reg<uint64_t>(x86_register::rip, address - SYSCALL_INSTRUCTION_SIZE);
            return true;
        }

        void handle_section_first_execution(analysis_context& c, const mapped_module& binary, const mapped_section& section,
                                            const uint64_t address)
        {
            const auto is_main_exe = &binary == c.win_emu->mod_manager.executable;
            if (!c.has_reached_main && c.settings->concise_logging && !c.settings->silent && is_main_exe)
            {
                c.has_reached_main = true;
                c.win_emu->log.disable_output(false);
            }

            if (!c.settings->log_first_section_execution)
            {
                return;
            }

            c.emit_observation<section_first_execute_event>([&](auto& event) {
                event.module_name = binary.name;
                event.section_name = section.name;
                event.file_address = address - binary.image_base + binary.image_base_file;
            });
        }

        void handle_instruction(analysis_context& c, const uint64_t address)
        {
            auto& win_emu = *c.win_emu;
            update_import_access(c, address);

#if defined(OS_EMSCRIPTEN) && !defined(SOGEN_EMSCRIPTEN_SUPPORT_NODEJS)
            if ((win_emu.get_executed_instructions() % 0x20000) == 0)
            {
                debugger::event_context ec{.win_emu = win_emu};
                debugger::handle_events(ec);
            }
#endif

            const auto& current_thread = c.win_emu->current_thread();
            const auto previous_ip = current_thread.previous_ip;
            [[maybe_unused]] const auto current_ip = current_thread.current_ip;
            const auto is_main_exe = win_emu.mod_manager.executable->contains(address);
            const auto is_previous_main_exe = win_emu.mod_manager.executable->contains(previous_ip);

            const auto binary = utils::make_lazy([&] {
                if (is_main_exe)
                {
                    return win_emu.mod_manager.executable;
                }

                return win_emu.mod_manager.find_by_address(address); //
            });

            const auto previous_binary = utils::make_lazy([&] {
                if (is_previous_main_exe)
                {
                    return win_emu.mod_manager.executable;
                }

                return win_emu.mod_manager.find_by_address(previous_ip); //
            });

            const auto is_current_binary_interesting = utils::make_lazy([&] {
                return is_main_exe || (binary && c.settings->modules.contains(binary->name)); //
            });

            const auto is_in_interesting_module = [&] {
                if (c.settings->modules.empty())
                {
                    return false;
                }

                return is_current_binary_interesting || (previous_binary && c.settings->modules.contains(previous_binary->name));
            };

            if (c.settings->instruction_summary && (is_current_binary_interesting || !binary))
            {
                record_instruction(c, address);
            }

            const auto is_interesting_call = is_previous_main_exe                                              //
                                             || (!previous_binary && current_thread.executed_instructions > 1) //
                                             || is_in_interesting_module();

            if ((!c.settings->verbose_logging && !is_interesting_call) || !binary)
            {
                return;
            }

            const auto export_entry = binary->address_names.find(address);
            if (export_entry != binary->address_names.end())
            {
                if (!c.settings->ignored_functions.contains(export_entry->second))
                {
                    auto details = collect_function_details(c, export_entry->second);
                    const auto call_count = next_traced_call_count(c);
                    c.emit_observation<function_execution_event>([&](auto& event) {
                        event.call_count = call_count;
                        event.function_name = export_entry->second;
                        event.interesting = is_interesting_call;
                        event.details = std::move(details);
                    });
                    (void)break_before_traced_call(c, call_count);
                }
            }
            else if (address == binary->entry_point)
            {
                c.emit_observation<entry_point_execution_event>([&](auto& event) { event.interesting = is_interesting_call; });
            }
            else if (is_previous_main_exe && binary != previous_binary && !is_return(c.d, c.win_emu->emu(), previous_ip))
            {
                auto nearest_entry = binary->address_names.upper_bound(address);
                if (nearest_entry == binary->address_names.begin())
                {
                    return;
                }

                --nearest_entry;
                c.emit_observation<foreign_code_transition_event>([&](auto& event) {
                    event.function_name = nearest_entry->second;
                    event.function_offset = address - nearest_entry->first;
                    event.interesting = is_interesting_call;
                });
            }
        }

        void handle_rdtsc(analysis_context& c)
        {
            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto rip = emu.read_instruction_pointer();
            const auto mod = get_module_if_interesting(win_emu.mod_manager, c.settings->modules, rip);

            if (!mod.has_value() || (c.settings->concise_logging && !c.rdtsc_cache.insert(rip).second))
            {
                return;
            }

            c.emit_observation<rdtsc_event>();
        }

        void handle_rdtscp(analysis_context& c)
        {
            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto rip = emu.read_instruction_pointer();
            const auto mod = get_module_if_interesting(win_emu.mod_manager, c.settings->modules, rip);

            if (!mod.has_value() || (c.settings->concise_logging && !c.rdtscp_cache.insert(rip).second))
            {
                return;
            }

            c.emit_observation<rdtscp_event>();
        }

        emulator_callbacks::continuation handle_syscall(analysis_context& c, const uint32_t syscall_id, const std::string_view syscall_name)
        {
            if (c.settings->ignored_functions.contains(syscall_name))
            {
                return instruction_hook_continuation::run_instruction;
            }

            auto& win_emu = *c.win_emu;
            auto& emu = win_emu.active_cpu();

            const auto address = emu.read_instruction_pointer();
            if (c.syscall_to_resume_after_break)
            {
                const auto syscall_to_resume = std::exchange(c.syscall_to_resume_after_break, std::nullopt);
                if (*syscall_to_resume == address)
                {
                    return instruction_hook_continuation::run_instruction;
                }
            }

            const auto* mod = win_emu.mod_manager.find_by_address(address);
            const auto is_sus_module = mod != win_emu.mod_manager.ntdll && mod != win_emu.mod_manager.win32u;
            const auto previous_ip = win_emu.current_thread().previous_ip;
            const auto is_valid_32_bit_module = utils::make_lazy([&] {
                return mod                                                              //
                       && win_emu.process.is_wow64_process                              //
                       && (mod->name == "wow64cpu.dll" || mod->name == "wow64win.dll"); //
            });

            if (is_sus_module && !is_valid_32_bit_module)
            {
                const auto call_count = next_traced_call_count(c);
                c.emit_observation<syscall_event>([&](auto& event) {
                    event.call_count = call_count;
                    event.classification = syscall_classification::inline_syscall;
                    event.syscall_id = syscall_id;
                    event.syscall_name = std::string(syscall_name);
                });

                if (break_before_traced_syscall(c, call_count, address))
                {
                    return instruction_hook_continuation::skip_instruction;
                }
            }
            else if (!previous_ip || mod->contains(previous_ip))
            {
                if (!c.settings->skip_syscalls)
                {
                    const auto rsp = emu.read_stack_pointer();

                    uint64_t return_address{};
                    emu.try_read_memory(rsp, &return_address, sizeof(return_address));

                    const auto* caller_mod_name = win_emu.mod_manager.find_name(return_address);
                    const auto call_count = next_traced_call_count(c);

                    c.emit_observation<syscall_event>([&](auto& event) {
                        event.call_count = call_count;
                        event.classification = syscall_classification::regular;
                        event.syscall_id = syscall_id;
                        event.syscall_name = std::string(syscall_name);
                        event.caller_rip = return_address;
                        event.caller_module = caller_mod_name ? std::optional<std::string>{caller_mod_name} : std::nullopt;
                    });

                    if (break_before_traced_syscall(c, call_count, address))
                    {
                        return instruction_hook_continuation::skip_instruction;
                    }
                }
            }
            else
            {
                const auto* previous_mod = win_emu.mod_manager.find_by_address(previous_ip);
                const auto call_count = next_traced_call_count(c);

                c.emit_observation<syscall_event>([&](auto& event) {
                    event.call_count = call_count;
                    event.classification = syscall_classification::crafted_out_of_line;
                    event.syscall_id = syscall_id;
                    event.syscall_name = std::string(syscall_name);
                    event.caller_rip = previous_ip;
                    event.caller_module = previous_mod ? std::optional<std::string>{previous_mod->name} : std::nullopt;
                });

                if (break_before_traced_syscall(c, call_count, address))
                {
                    return instruction_hook_continuation::skip_instruction;
                }
            }

            return instruction_hook_continuation::run_instruction;
        }

        void handle_dialog_auto_click(analysis_context& c)
        {
            if (c.click_dialog_rules.empty())
            {
                return;
            }

            auto& proc = c.win_emu->process;

            // Prune entries whose dialog window no longer exists so a later, unrelated dialog
            // can't silently reuse the destroyed dialog's recycled HWND and get treated as
            // already clicked.
            std::erase_if(c.clicked_dialogs, [&](const uint64_t handle) { return proc.windows.get(static_cast<hwnd>(handle)) == nullptr; });

            for (auto& win : proc.windows | std::views::values)
            {
                if (!win.is_dialog() || c.clicked_dialogs.contains(win.handle))
                {
                    continue;
                }

                // A dialog whose title matches none of the rules is left alone: it might be a
                // real, unexpected error rather than one of the known dialogs to auto-dismiss.
                const auto title = u16_to_u8(win.name);
                const auto rule = std::ranges::find_if(c.click_dialog_rules,
                                                       [&](const auto& entry) { return title.find(entry.first) != std::string::npos; });
                if (rule == c.click_dialog_rules.end())
                {
                    continue;
                }

                const auto wanted = rule->second;

                // The WM_COMMAND below is posted (queued), so the owning thread need not already
                // be blocked in a message wait - a plain PeekMessage pump will still pick it up;
                // do not gate this on await_msg/await_msg_mask.
                const emulator_thread* owner = proc.find_thread_by_id(win.thread_id);
                if (!owner)
                {
                    continue;
                }

                hwnd child_handle = 0;
                for (auto& child : proc.windows | std::views::values)
                {
                    if (child.parent_handle != win.handle)
                    {
                        continue;
                    }

                    uint32_t control_id = 0;
                    child.guest.access([&](const USER_WINDOW& gw) { control_id = static_cast<uint32_t>(gw.wID); });
                    if (control_id == wanted)
                    {
                        child_handle = child.handle;
                        break;
                    }
                }

                if (child_handle == 0)
                {
                    continue;
                }

                ui_event event{};
                event.window = win.handle;
                event.message = WM_COMMAND;
                event.wParam = wanted & 0xFFFF;
                event.lParam = child_handle;

                c.win_emu->handle_ui_event(event);
                c.clicked_dialogs.insert(win.handle);
                return;
            }
        }

        window* find_input_target_window(process_context& proc)
        {
            const auto desktop = proc.default_desktop_window_handle.bits;
            window* best = nullptr;
            int64_t best_area = -1;

            for (auto& win : proc.windows | std::views::values)
            {
                if (win.handle == desktop || win.is_dialog() || (win.style & WS_VISIBLE) == 0 || win.thread_id == 0 ||
                    (win.parent_handle != 0 && win.parent_handle != desktop))
                {
                    continue;
                }

                const auto area = static_cast<int64_t>(win.client_width()) * win.client_height();
                if (area > best_area)
                {
                    best_area = area;
                    best = &win;
                }
            }

            return best;
        }

        uint64_t make_key_lparam(const input_action& action, const bool key_up)
        {
            uint64_t lparam = 1;
            lparam |= static_cast<uint64_t>(action.scan) << 16;

            if (action.extended)
            {
                lparam |= 1ull << 24;
            }

            if (key_up)
            {
                lparam |= (1ull << 30) | (1ull << 31);
            }

            return lparam;
        }

        void send_synthetic_ui_event(analysis_context& c, const hwnd window, const uint32_t message, const uint64_t wparam,
                                     const uint64_t lparam)
        {
            c.win_emu->handle_ui_event(ui_event{.window = window, .message = message, .wParam = wparam, .lParam = lparam});
        }

        void handle_input_script(analysis_context& c)
        {
            if (c.input_script_pos >= c.input_script.size())
            {
                return;
            }

            auto& proc = c.win_emu->process;
            const auto now = std::chrono::steady_clock::now();

            if (!c.input_script_deadline)
            {
                const auto* target = find_input_target_window(proc);
                if (!target)
                {
                    return;
                }

                c.input_script_deadline = now;
                c.win_emu->log.info("Input script armed on window %llx ('%s')\n", static_cast<unsigned long long>(target->handle),
                                    u16_to_u8(target->name).c_str());
            }

            while (c.input_script_pos < c.input_script.size() && now >= *c.input_script_deadline)
            {
                const auto& action = c.input_script[c.input_script_pos];

                if (action.type == input_action::kind::wait)
                {
                    *c.input_script_deadline += std::chrono::milliseconds(action.delay_ms);
                    ++c.input_script_pos;
                    continue;
                }

                if (action.type == input_action::kind::wait_window)
                {
                    const auto* wait_target = find_input_target_window(proc);
                    const bool matched =
                        wait_target && u16_to_u8(wait_target->class_name).find(action.text) != std::string::npos;

                    if (matched)
                    {
                        c.win_emu->log.info("Input script: window class now contains '%s', continuing\n", action.text.c_str());
                        c.input_wait_window_started.reset();
                        ++c.input_script_pos;
                        continue;
                    }

                    if (!c.input_wait_window_started)
                    {
                        c.input_wait_window_started = now;
                    }

                    if (now - *c.input_wait_window_started < std::chrono::milliseconds(action.delay_ms))
                    {
                        return;
                    }

                    c.win_emu->log.warn("Input script: waiting for window class containing '%s' timed out after %u ms, "
                                        "continuing anyway\n",
                                        action.text.c_str(), action.delay_ms);
                    c.input_wait_window_started.reset();
                    ++c.input_script_pos;
                    continue;
                }

                auto* target = find_input_target_window(proc);
                if (!target)
                {
                    return;
                }

                if (c.input_target_window != target->handle)
                {
                    c.win_emu->log.info("Input script: target window changed to %llx ('%s', class '%s')\n",
                                        static_cast<unsigned long long>(target->handle), u16_to_u8(target->name).c_str(),
                                        u16_to_u8(target->class_name).c_str());
                    send_synthetic_ui_event(c, target->handle, WM_SETFOCUS, 0, 0);
                    send_synthetic_ui_event(c, target->handle, WM_ACTIVATE, WA_ACTIVE, 0);
                    c.input_target_window = target->handle;
                }

                const auto to_client = [](const float value, const bool normalized, const int32_t extent) {
                    const auto max_coord = std::max(0, extent - 1);
                    const auto raw =
                        normalized ? static_cast<int>(std::lround(value * static_cast<float>(extent))) : static_cast<int>(value);
                    return std::clamp(raw, 0, max_coord);
                };

                const auto client_x = to_client(action.x, action.normalized, target->client_width());
                const auto client_y = to_client(action.y, action.normalized, target->client_height());
                const auto point = (static_cast<uint64_t>(static_cast<uint16_t>(client_y)) << 16) | static_cast<uint16_t>(client_x);

                switch (action.type)
                {
                case input_action::kind::mouse_move:
                    send_synthetic_ui_event(c, target->handle, WM_MOUSEMOVE, 0, point);
                    c.win_emu->log.info("Input script: mouse move to (%d, %d)\n", client_x, client_y);
                    break;
                case input_action::kind::button_down:
                    send_synthetic_ui_event(c, target->handle, WM_LBUTTONDOWN, MK_LBUTTON, point);
                    c.win_emu->log.info("Input script: left button down at (%d, %d)\n", client_x, client_y);
                    break;
                case input_action::kind::button_up:
                    send_synthetic_ui_event(c, target->handle, WM_LBUTTONUP, 0, point);
                    c.win_emu->log.info("Input script: left button up at (%d, %d)\n", client_x, client_y);
                    break;
                case input_action::kind::key_down:
                    send_synthetic_ui_event(c, target->handle, WM_KEYDOWN, action.vk, make_key_lparam(action, false));
                    c.win_emu->log.info("Input script: key down 0x%x\n", action.vk);
                    break;
                case input_action::kind::key_up:
                    send_synthetic_ui_event(c, target->handle, WM_KEYUP, action.vk, make_key_lparam(action, true));
                    c.win_emu->log.info("Input script: key up 0x%x\n", action.vk);
                    break;
                case input_action::kind::send_text:
                    for (const char ch : action.text)
                    {
                        send_synthetic_ui_event(c, target->handle, WM_CHAR, static_cast<uint8_t>(ch), 0);
                    }
                    c.win_emu->log.info("Input script: text '%s'\n", action.text.c_str());
                    break;
                case input_action::kind::wait:
                case input_action::kind::wait_window:
                    break;
                }

                ++c.input_script_pos;
            }
        }

        void handle_event_pump(analysis_context& c)
        {
            handle_dialog_auto_click(c);
            handle_input_script(c);
        }

        void handle_stdout(analysis_context& c, const std::string_view data)
        {
            c.emit_observation<stdout_chunk_event>([&](auto& event) { event.data = std::string(data); });

            if (c.settings->buffer_stdout && !c.settings->silent)
            {
                c.output.append(data);
            }
        }

        void watch_import_table(analysis_context& c)
        {
            c.win_emu->setup_process_if_necessary();

            const auto& import_list = c.win_emu->mod_manager.executable->imports;
            if (import_list.empty())
            {
                return;
            }

            auto min = std::numeric_limits<uint64_t>::max();
            auto max = std::numeric_limits<uint64_t>::min();

            for (const auto& import_thunk : import_list | std::views::keys)
            {
                min = std::min(import_thunk, min);
                max = std::max(import_thunk, max);
            }

            c.win_emu->emu().hook_memory_write(min, max - min,
                                               [&c](cpu_interface&, const uint64_t address, const void* value, size_t size) {
                                                   const auto& watched_module = *c.win_emu->mod_manager.executable;

                                                   const auto sym = watched_module.imports.find(address);
                                                   if (sym == watched_module.imports.end())
                                                   {
                                                       // TODO: Print unaligned write accesses?
                                                       return;
                                                   }

                                                   uint64_t int_value{};
                                                   memcpy(&int_value, value, std::min(size, sizeof(int_value)));

                                                   const auto import_module = watched_module.imported_modules.at(sym->second.module_index);

                                                   c.emit_observation<import_write_event>([&](auto& event) {
                                                       event.size = size;
                                                       event.value = int_value;
                                                       event.import_name = sym->second.name;
                                                       event.import_module = import_module;
                                                   });
                                               });

            c.win_emu->emu().hook_memory_read(min, max - min, [&c](cpu_interface&, const uint64_t address, const void*, size_t) {
                const auto rip = c.win_emu->emu().read_instruction_pointer();
                const auto& watched_module = *c.win_emu->mod_manager.executable;
                const auto accessor_module = get_module_if_interesting(c.win_emu->mod_manager, c.settings->modules, rip);

                if (!accessor_module.has_value())
                {
                    return;
                }

                const auto sym = watched_module.imports.find(address);
                if (sym == watched_module.imports.end())
                {
                    return;
                }

                accessed_import access{};
                access.address = c.win_emu->emu().read_memory<uint64_t>(address);
                access.access_context = c.make_execution_context();
                access.import_name = sym->second.name;
                access.import_module = watched_module.imported_modules.at(sym->second.module_index);

                const auto& t = c.win_emu->current_thread();
                access.access_inst_count = t.executed_instructions;

                c.accessed_imports.push_back(std::move(access));
            });
        }

        std::string_view trim(std::string_view text)
        {
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
            {
                text.remove_prefix(1);
            }
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
            {
                text.remove_suffix(1);
            }
            return text;
        }

        std::vector<std::string> split_string(const std::string_view text, const char separator)
        {
            std::vector<std::string> parts{};
            size_t start = 0;
            while (start <= text.size())
            {
                const auto end = text.find(separator, start);
                if (end == std::string_view::npos)
                {
                    parts.emplace_back(text.substr(start));
                    break;
                }

                parts.emplace_back(text.substr(start, end - start));
                start = end + 1;
            }
            return parts;
        }

        struct key_spec
        {
            uint16_t vk{};
            uint8_t scan{};
            bool extended{};
        };

        std::optional<key_spec> lookup_key(std::string name)
        {
            std::ranges::transform(name, name.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

            static const std::unordered_map<std::string, key_spec> named_keys{
                {"enter", {VK_RETURN, 0x1C, false}},   {"return", {VK_RETURN, 0x1C, false}}, {"esc", {VK_ESCAPE, 0x01, false}},
                {"escape", {VK_ESCAPE, 0x01, false}},  {"space", {VK_SPACE, 0x39, false}},   {"tab", {VK_TAB, 0x0F, false}},
                {"backspace", {VK_BACK, 0x0E, false}}, {"up", {VK_UP, 0x48, true}},          {"down", {VK_DOWN, 0x50, true}},
                {"left", {VK_LEFT, 0x4B, true}},       {"right", {VK_RIGHT, 0x4D, true}},    {"pgup", {VK_PRIOR, 0x49, true}},
                {"pgdn", {VK_NEXT, 0x51, true}},       {"home", {VK_HOME, 0x47, true}},      {"end", {VK_END, 0x4F, true}},
                {"grave", {VK_OEM_3, 0x29, false}},    {"tilde", {VK_OEM_3, 0x29, false}},
            };

            if (const auto entry = named_keys.find(name); entry != named_keys.end())
            {
                return entry->second;
            }

            if (name.size() >= 2 && name.size() <= 3 && name[0] == 'f' && std::isdigit(static_cast<unsigned char>(name[1])))
            {
                const auto number = std::stoul(name.substr(1));
                if (number >= 1 && number <= 12)
                {
                    static constexpr std::array<uint8_t, 12> function_scans{0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
                                                                            0x41, 0x42, 0x43, 0x44, 0x57, 0x58};
                    return key_spec{static_cast<uint16_t>(VK_F1 + number - 1), function_scans[number - 1], false};
                }
                return std::nullopt;
            }

            if (name.size() == 1)
            {
                const char c = name[0];
                if (c >= 'a' && c <= 'z')
                {
                    static constexpr std::array<uint8_t, 26> letter_scans{0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                                                          0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                                                          0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
                    return key_spec{static_cast<uint16_t>(std::toupper(static_cast<unsigned char>(c))),
                                    letter_scans[static_cast<size_t>(c - 'a')], false};
                }
                if (c >= '0' && c <= '9')
                {
                    const auto scan = c == '0' ? uint8_t{0x0B} : static_cast<uint8_t>(0x02 + (c - '1'));
                    return key_spec{static_cast<uint16_t>(c), scan, false};
                }
                return std::nullopt;
            }

            if (name.starts_with("0x"))
            {
                const auto vk = std::stoul(name, nullptr, 16);
                if (vk > 0 && vk <= 0xFF)
                {
                    return key_spec{static_cast<uint16_t>(vk), 0, false};
                }
            }

            return std::nullopt;
        }
    }

    std::vector<input_action> parse_input_script(const std::string_view script)
    {
        constexpr uint32_t press_duration_ms = 80;
        std::vector<input_action> actions{};

        for (const auto& part : split_string(script, ';'))
        {
            const auto token = trim(part);
            if (token.empty())
            {
                continue;
            }

            const auto fail = [&]() -> void { throw std::runtime_error("Invalid input script action: " + std::string(token)); };

            const auto fields = split_string(token, ':');
            const auto& op = fields[0];

            const auto parse_delay = [&](const std::string& text) {
                try
                {
                    return static_cast<uint32_t>(std::stoul(text));
                }
                catch (const std::exception&)
                {
                    fail();
                    return uint32_t{};
                }
            };

            const auto parse_point = [&](input_action& action) {
                try
                {
                    action.normalized = fields[1].find('.') != std::string::npos || fields[2].find('.') != std::string::npos;
                    action.x = std::stof(fields[1]);
                    action.y = std::stof(fields[2]);
                }
                catch (const std::exception&)
                {
                    fail();
                }
            };

            const auto parse_key = [&](input_action& action) {
                const auto key = fields.size() == 2 ? lookup_key(fields[1]) : std::nullopt;
                if (!key)
                {
                    fail();
                    return;
                }
                action.vk = key->vk;
                action.scan = key->scan;
                action.extended = key->extended;
            };

            input_action action{};

            if (op == "wait" && fields.size() == 2)
            {
                action.type = input_action::kind::wait;
                action.delay_ms = parse_delay(fields[1]);
                actions.push_back(action);
            }
            else if (op == "waitclass" && fields.size() == 3)
            {
                action.type = input_action::kind::wait_window;
                action.text = fields[1];
                action.delay_ms = parse_delay(fields[2]);
                actions.push_back(action);
            }
            else if (op == "move" && fields.size() == 3)
            {
                action.type = input_action::kind::mouse_move;
                parse_point(action);
                actions.push_back(action);
            }
            else if (op == "click" && fields.size() == 3)
            {
                parse_point(action);

                action.type = input_action::kind::mouse_move;
                actions.push_back(action);

                action.type = input_action::kind::button_down;
                actions.push_back(action);

                actions.push_back({.type = input_action::kind::wait, .delay_ms = press_duration_ms});

                action.type = input_action::kind::button_up;
                actions.push_back(action);
            }
            else if (op == "key")
            {
                parse_key(action);

                action.type = input_action::kind::key_down;
                actions.push_back(action);

                actions.push_back({.type = input_action::kind::wait, .delay_ms = press_duration_ms});

                action.type = input_action::kind::key_up;
                actions.push_back(action);
            }
            else if (op == "keydown")
            {
                parse_key(action);
                action.type = input_action::kind::key_down;
                actions.push_back(action);
            }
            else if (op == "keyup")
            {
                parse_key(action);
                action.type = input_action::kind::key_up;
                actions.push_back(action);
            }
            else if (op == "text" && fields.size() >= 2)
            {
                action.type = input_action::kind::send_text;
                action.text = std::string(token.substr(op.size() + 1));
                actions.push_back(action);
            }
            else
            {
                fail();
            }
        }

        return actions;
    }

    event_header analysis_context::make_event_header() const
    {
        return {
            .sequence = this->next_event_sequence++,
            .instruction_count = this->win_emu ? this->win_emu->get_executed_instructions() : 0,
        };
    }

    execution_context analysis_context::make_execution_context() const
    {
        auto& emu = this->win_emu->active_cpu();
        const auto rip = emu.read_instruction_pointer();
        const auto* rip_module = this->win_emu->mod_manager.find_name(rip);

        execution_context context{
            .thread_id = 0,
            .rip = rip,
            .rip_module = rip_module ? rip_module : "<N/A>",
        };

        try
        {
            const auto& thread = this->win_emu->current_thread();
            const auto previous_ip = thread.previous_ip;
            const auto* previous_module = previous_ip ? this->win_emu->mod_manager.find_name(previous_ip) : nullptr;
            context.thread_id = thread.id;
            context.previous_ip = previous_ip ? std::optional<uint64_t>{previous_ip} : std::nullopt;
            context.previous_ip_module = previous_module ? std::optional<std::string>{previous_module} : std::nullopt;
        }
        catch (...)
        {
            // Some early lifecycle events fire before a thread is active.
        }

        return context;
    }

    void analysis_context::emit_event(const analysis_event& event) const
    {
        for (auto* reporter : this->reporters)
        {
            reporter->report(event);
        }
    }

    void register_analysis_callbacks(analysis_context& c)
    {
        auto& cb = c.win_emu->callbacks;

        cb.on_stdout = make_callback(c, handle_stdout);
        cb.on_syscall = make_callback(c, handle_syscall);
        cb.on_rdtsc = make_callback(c, handle_rdtsc);
        cb.on_rdtscp = make_callback(c, handle_rdtscp);
        cb.on_ioctrl = make_callback(c, handle_ioctrl);

        cb.on_memory_protect = make_callback(c, handle_memory_protect);
        cb.on_memory_violate = make_callback(c, handle_memory_violate);
        cb.on_memory_allocate = make_callback(c, handle_memory_allocate);

        (void)cb.on_module_load.add(make_callback(c, handle_module_load));
        (void)cb.on_module_unload.add(make_callback(c, handle_module_unload));
        (void)cb.on_section_first_execution.add(make_callback(c, handle_section_first_execution));

        cb.on_thread_create = make_callback(c, handle_thread_create);
        cb.on_thread_terminated = make_callback(c, handle_thread_terminated);
        cb.on_thread_switch = make_callback(c, handle_thread_switch);
        cb.on_thread_set_name = make_callback(c, handle_thread_set_name);

        cb.on_instruction = make_callback(c, handle_instruction);
        cb.on_event_pump = make_callback(c, handle_event_pump);
        cb.on_debug_string.add(make_callback(c, handle_debug_string));
        cb.on_generic_access = make_callback(c, handle_generic_access);
        cb.on_generic_activity = make_callback(c, handle_generic_activity);
        cb.on_suspicious_activity = make_callback(c, handle_suspicious_activity);
        cb.on_fast_fail = make_callback(c, handle_fast_fail);

        watch_import_table(c);
    }

    std::optional<mapped_module*> get_module_if_interesting(module_manager& manager, const string_set& modules, const uint64_t address)
    {
        if (manager.executable->contains(address))
        {
            return manager.executable;
        }

        auto* mod = manager.find_by_address(address);
        if (!mod)
        {
            // Not being part of any module is interesting
            return nullptr;
        }

        if (modules.contains(mod->name))
        {
            return mod;
        }

        return std::nullopt;
    }

} // namespace sogen
