#include "std_include.hpp"
#include "syscall_dispatcher.hpp"
#include "syscall_utils.hpp"

#include <utils/string.hpp>

namespace sogen
{

    namespace
    {
        // Real ntdll's RtlpInitCodePageTables always leaves this internal lead-byte-info-table pointer
        // (at a fixed offset from ntdll's image base) populated - either a real DBCS table, or ntdll's
        // own empty/all-zero NlsEmptyLeadByteInfoTable for single-byte codepages. For a wow64 process
        // specifically, this field is observed to stay null even though the sibling codepage-table
        // fields are populated correctly, and any guest read of a null table computes a near-null
        // address and crashes. Populating it with an empty/all-zero lead-byte table is semantically
        // identical to ntdll's own single-byte-codepage behavior, so it's a safe fix regardless of the
        // exact root cause.
        constexpr uint64_t nls_lead_byte_info_table_offset = 0x172760;
        constexpr uint64_t nls_global_rtl_state_offset = 0x1726d0;

        bool address_is_in_writable_section(const mapped_module& mod, const uint64_t address)
        {
            for (const auto& section : mod.sections)
            {
                const auto& region = section.region;
                if (address >= region.start && address - region.start < region.length)
                {
                    return (region.permissions & memory_permission::write) != memory_permission::none;
                }
            }

            return false;
        }

        void ensure_nls_lead_byte_info_table(windows_emulator& win_emu)
        {
            auto& resolved = win_emu.process.nls_lead_byte_info_table_resolved;
            if (resolved.has_value())
            {
                return;
            }

            if (!win_emu.process.is_wow64_process)
            {
                resolved = false;
                return;
            }

            const auto* ntdll_mod = win_emu.mod_manager.ntdll;
            if (ntdll_mod == nullptr)
            {
                return;
            }

            const auto field_address = ntdll_mod->image_base + nls_lead_byte_info_table_offset;
            const auto global_rtl_state_address = ntdll_mod->image_base + nls_global_rtl_state_offset;

            if (!address_is_in_writable_section(*ntdll_mod, field_address) ||
                !address_is_in_writable_section(*ntdll_mod, global_rtl_state_address))
            {
                resolved = false;
                return;
            }

            uint64_t current_value = 0;
            if (!win_emu.emu().try_read_memory(field_address, &current_value, sizeof(current_value)))
            {
                resolved = false;
                return;
            }
            if (current_value != 0)
            {
                resolved = true;
                return;
            }

            uint16_t global_rtl_nls_state = 0;
            // 0xFDE9 is ntdll's own not-yet-initialized marker for this state; skip until ntdll's own
            // codepage init has actually run.
            if (!win_emu.emu().try_read_memory(global_rtl_state_address, &global_rtl_nls_state, sizeof(global_rtl_nls_state)) ||
                global_rtl_nls_state == 0xFDE9)
            {
                return;
            }

            constexpr size_t lead_byte_table_size = 0x200; // 256 WORD entries, one per possible byte value
            const auto aligned_size = static_cast<size_t>(page_align_up(lead_byte_table_size));
            const auto table_address = win_emu.memory.allocate_memory(aligned_size, memory_permission::read);
            const std::vector<std::byte> zeroed_table(lead_byte_table_size, std::byte{0});
            win_emu.emu().write_memory(table_address, zeroed_table.data(), zeroed_table.size());
            win_emu.emu().write_memory(field_address, &table_address, sizeof(table_address));
            resolved = true;
        }

    } // namespace

    static void serialize(utils::buffer_serializer& buffer, const syscall_handler_entry& obj)
    {
        buffer.write(obj.name);
    }

    static void deserialize(utils::buffer_deserializer& buffer, syscall_handler_entry& obj)
    {
        buffer.read(obj.name);
        obj.handler = nullptr;
    }

    void syscall_dispatcher::serialize(utils::buffer_serializer& buffer) const
    {
        buffer.write_map(this->handlers_);
    }

    void syscall_dispatcher::deserialize(utils::buffer_deserializer& buffer)
    {
        buffer.read_map(this->handlers_);
        this->add_handlers();
        this->add_callbacks();
    }

    void syscall_dispatcher::setup(const exported_symbols& ntdll_exports, const std::span<const std::byte> ntdll_data,
                                   const exported_symbols& win32u_exports, const std::span<const std::byte> win32u_data)
    {
        this->handlers_ = {};

        const auto ntdll_syscalls = find_syscalls(ntdll_exports, ntdll_data);
        const auto win32u_syscalls = find_syscalls(win32u_exports, win32u_data);

        map_syscalls(this->handlers_, ntdll_syscalls);
        map_syscalls(this->handlers_, win32u_syscalls);

        this->add_handlers();
        this->add_callbacks();
    }

    void syscall_dispatcher::add_handlers()
    {
        std::map<std::string, syscall_handler> handler_mapping{};
        syscall_dispatcher::add_handlers(handler_mapping);

        for (auto& entry : this->handlers_ | std::views::values)
        {
            const auto handler = handler_mapping.find(entry.name);
            if (handler == handler_mapping.end())
            {
                continue;
            }

            entry.handler = handler->second;

#ifndef NDEBUG
            handler_mapping.erase(handler);
#endif
        }
    }

    void syscall_dispatcher::dispatch(windows_emulator& win_emu, vcpu_context& vcpu)
    {
        auto& emu = vcpu.cpu;
        auto& context = win_emu.process;

        ensure_nls_lead_byte_info_table(win_emu);

        const auto address = emu.read_instruction_pointer();
        const auto raw_syscall_id = emu.reg<uint32_t>(x86_register::eax);
        const auto syscall_id = raw_syscall_id & 0x3FFF; // Only take low bits for WOW64 compatibility, match windoows wraparound

        const auto entry = this->handlers_.find(syscall_id);
        const auto* syscall_name = (entry != this->handlers_.end()) ? entry->second.name.c_str() : "<unknown>";

        const syscall_context c{
            .win_emu = win_emu,
            .emu = emu,
            .vcpu = vcpu,
            .proc = context,
            .write_status = true,
        };

        try
        {
            if (entry == this->handlers_.end())
            {
                win_emu.log.error("Unknown syscall: 0x%X (raw: 0x%X)\n", syscall_id, raw_syscall_id);
                win_emu.record_stop(stop_reason::unknown_syscall, "0x" + utils::string::to_hex_number(syscall_id));
                c.emu.reg<uint64_t>(x86_register::rax, STATUS_NOT_SUPPORTED);
                c.emu.stop();
                return;
            }

            const auto res = win_emu.callbacks.on_syscall(syscall_id, entry->second.name);
            if (res == instruction_hook_continuation::skip_instruction)
            {
                return;
            }

            if (!entry->second.handler)
            {
                win_emu.log.error("Unimplemented syscall: %s - 0x%X (raw: 0x%X)\n", entry->second.name.c_str(), syscall_id, raw_syscall_id);
                win_emu.record_stop(stop_reason::unimplemented_syscall, entry->second.name);
                c.emu.reg<uint64_t>(x86_register::rax, STATUS_NOT_SUPPORTED);
                c.emu.stop();
                return;
            }

            entry->second.handler(c);

            dispatch_callback(win_emu, entry->second.name);
        }
        catch (std::exception& e)
        {
            win_emu.log.error("Syscall %s threw an exception: 0x%X (raw: 0x%X) (0x%" PRIx64 ") - %s\n", syscall_name, syscall_id,
                              raw_syscall_id, address, e.what());
            win_emu.record_stop(stop_reason::syscall_exception, std::string(syscall_name) + ": " + e.what());
            emu.reg<uint64_t>(x86_register::rax, STATUS_UNSUCCESSFUL);
            win_emu.stop();
        }
        catch (...)
        {
            win_emu.log.error("Syscall %s threw an unknown exception: 0x%X (raw: 0x%X) (0x%" PRIx64 ")\n", syscall_name, syscall_id,
                              raw_syscall_id, address);
            win_emu.record_stop(stop_reason::syscall_exception, std::string(syscall_name) + ": <unknown exception>");
            emu.reg<uint64_t>(x86_register::rax, STATUS_UNSUCCESSFUL);
            win_emu.stop();
        }
    }

    void syscall_dispatcher::dispatch_callback(windows_emulator& win_emu, std::string& syscall_name)
    {
        // active_cpu(), not emu(): this runs under the syscall's scoped_dispatch, and with more than one
        // vCPU the instrumentation-callback redirect must rewrite the acting vCPU's RIP/r10, not vCPU 0's.
        auto& emu = win_emu.active_cpu();
        auto& context = win_emu.process;

        if (context.instrumentation_callback != 0 && syscall_name != "NtContinue")
        {
            auto rip_old = emu.reg<uint64_t>(x86_register::rip);

            // The increase in RIP caused by executing the syscall here has not yet occurred.
            // If RIP is set directly, it will lead to an incorrect address, so the length of
            // the syscall instruction needs to be subtracted.
            emu.reg<uint64_t>(x86_register::rip, context.instrumentation_callback - 2);

            emu.reg<uint64_t>(x86_register::r10, rip_old);
        }
    }

    dispatch_result syscall_dispatcher::dispatch_completion(windows_emulator& win_emu, vcpu_context& vcpu, callback_id callback_id,
                                                            completion_state* completion_state, uint64_t callback_result)
    {
        auto& emu = vcpu.cpu;

        const syscall_context c{.win_emu = win_emu,
                                .emu = emu,
                                .vcpu = vcpu,
                                .proc = win_emu.process,
                                .write_status = true,
                                .is_callback_completion = true,
                                .current_completion_state = completion_state,
                                .previous_callback_result = callback_result};

        const auto entry = this->completion_handlers_.find(callback_id);

        if (entry == this->completion_handlers_.end())
        {
            win_emu.log.error("Unknown callback: 0x%X\n", static_cast<uint32_t>(callback_id));
            c.emu.stop();
            return dispatch_result::error;
        }

        try
        {
            entry->second(c);
            return c.run_callback ? dispatch_result::new_callback : dispatch_result::completed;
        }
        catch (std::exception& e)
        {
            win_emu.log.error("Completion for callback 0x%X threw an exception - %s\n", static_cast<int>(callback_id), e.what());
            emu.stop();
            return dispatch_result::error;
        }
        catch (...)
        {
            win_emu.log.error("Completion for callback 0x%X threw an unknown exception\n", static_cast<int>(callback_id));
            emu.stop();
            return dispatch_result::error;
        }
    }

    syscall_dispatcher::syscall_dispatcher(const exported_symbols& ntdll_exports, const std::span<const std::byte> ntdll_data,
                                           const exported_symbols& win32u_exports, const std::span<const std::byte> win32u_data)
    {
        this->setup(ntdll_exports, ntdll_data, win32u_exports, win32u_data);
    }

    std::map<callback_id, std::function<std::unique_ptr<completion_state>()>> syscall_dispatcher::completion_state_factories_{};

} // namespace sogen
