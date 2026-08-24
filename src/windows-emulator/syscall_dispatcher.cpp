#include "std_include.hpp"
#include "syscall_dispatcher.hpp"
#include "syscall_utils.hpp"

#include <utils/string.hpp>

// #define ENABLE_NTSTATUS_PROBE

namespace sogen
{

    namespace
    {
        // Real ntdll's RtlpInitCodePageTables always leaves this internal lead-byte-info-table pointer
        // (at a fixed offset from ntdll's image base) populated - either a real DBCS table, or ntdll's
        // own empty/all-zero NlsEmptyLeadByteInfoTable for single-byte codepages. In a wow64 process
        // under this emulator the field stays null even though the sibling codepage-table fields are
        // populated, and any guest read of a null table computes a near-null address and crashes.
        // An empty/all-zero lead-byte table matches ntdll's own single-byte-codepage behavior.
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

        // Temporary diagnostic (EMULATOR_SYSCALL_FREQ_DIAG=1): syscall-name frequency counts, printed
        // every 200,000 dispatches. With every D3D9-host-side and UI-present-side FPS lever this
        // session investigated either fixed or measured and ruled out, and the RIP-sampler (see
        // fex_x86_64_emulator.cpp's EMULATOR_FEX_RIP_SAMPLE) consistently finding ~98% of guest-
        // execution samples in FEXCore's own syscall-dispatch/WoW64-gate-crossing trampoline, this
        // checks what the guest is actually spending syscall volume on beyond D3D9's own Escape calls.
        if (getenv("EMULATOR_SYSCALL_FREQ_DIAG"))
        {
            static std::unordered_map<std::string, uint64_t> counts;
            static uint64_t total = 0;
            ++counts[syscall_name];
            if (++total % 200000 == 0)
            {
                std::vector<std::pair<std::string, uint64_t>> sorted(counts.begin(), counts.end());
                std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
                fprintf(stderr, "[syscall-freq-diag] total=%llu top:\n", static_cast<unsigned long long>(total));
                for (size_t i = 0; i < sorted.size() && i < 15; ++i)
                {
                    fprintf(stderr, "[syscall-freq-diag]   %s = %llu (%.1f%%)\n", sorted[i].first.c_str(),
                            static_cast<unsigned long long>(sorted[i].second), 100.0 * static_cast<double>(sorted[i].second) / static_cast<double>(total));
                }
            }
        }

        if (getenv("EMULATOR_BINK_CONTROL_DIAG"))
        {
            constexpr uint64_t bink_ptr_addr = 0x1C87BD4;
            constexpr uint64_t caller_pause_state_addr = 0x1C87CC0; // iw4sp.exe's own sub_508FB0 pause flag
            constexpr uint64_t caller_video_flags_addr = 0x1C879B0; // iw4sp.exe's own render-mode flags (bits 2/4 tested)

            uint32_t bink_ptr = 0;
            uint32_t caller_pause_state = 0;
            uint32_t caller_video_flags = 0;
            const bool have_bink_ptr = emu.try_read_memory(bink_ptr_addr, &bink_ptr, sizeof(bink_ptr)) && bink_ptr != 0;
            emu.try_read_memory(caller_pause_state_addr, &caller_pause_state, sizeof(caller_pause_state));
            emu.try_read_memory(caller_video_flags_addr, &caller_video_flags, sizeof(caller_video_flags));

            if (have_bink_ptr)
            {
                uint32_t stopped = 0;       // +28
                uint32_t timer_gate = 0;    // +20  (0 = timer disabled, gate C auto-passes)
                uint32_t mode_flag = 0;     // +220 (must be 0 for gate A to pass)
                uint32_t track_count = 0;   // +744 (0 = no audio track, gate B auto-passes)
                uint32_t sound_on = 0;      // +648 (per-track "sound on" latch, gate B)
                uint32_t last_time = 0;     // +616 (v2, compared against target_time)
                uint32_t total_frames = 0;  // +8   (constant; caller's sub_508FB0 compares this against +12)
                uint32_t current_frame = 0; // +12  (advances as Bink decodes; caller only swaps the displayed
                                            // frame and calls BinkGetRects/blits when this differs from +8)
                int32_t rect_sentinel = 0;  // +180 (BinkGetRects' cached-rect-count/-1-means-recompute state;
                                            // BinkGetRects only rebuilds rects and returns >0 when this is -1,
                                            // and only BinkDoFrame's per-frame tail resets it back to -1)
                uint32_t stop_trigger = 0;  // +272 (nonzero here latches +28/"stopped", which makes BinkDoFrame
                                            // take its early-return path and skip the +180 reset entirely)
                emu.try_read_memory(bink_ptr + 28, &stopped, sizeof(stopped));
                emu.try_read_memory(bink_ptr + 20, &timer_gate, sizeof(timer_gate));
                emu.try_read_memory(bink_ptr + 220, &mode_flag, sizeof(mode_flag));
                emu.try_read_memory(bink_ptr + 744, &track_count, sizeof(track_count));
                emu.try_read_memory(bink_ptr + 648, &sound_on, sizeof(sound_on));
                emu.try_read_memory(bink_ptr + 616, &last_time, sizeof(last_time));
                emu.try_read_memory(bink_ptr + 8, &total_frames, sizeof(total_frames));
                emu.try_read_memory(bink_ptr + 12, &current_frame, sizeof(current_frame));
                emu.try_read_memory(bink_ptr + 180, &rect_sentinel, sizeof(rect_sentinel));
                emu.try_read_memory(bink_ptr + 272, &stop_trigger, sizeof(stop_trigger));
                win_emu.log.warn("[bink-control-diag] bink=0x%X caller_pause(+1C87CC0)=%u caller_flags(+1C879B0)=0x%X stopped(+28)=%u "
                                 "timer(+20)=%u mode(+220)=%u tracks(+744)=%u sound_on(+648)=%u last_time(+616)=%u total_frames(+8)=%u "
                                 "current_frame(+12)=%u rects(+180)=%d stop_trigger(+272)=%u\n",
                                 bink_ptr, caller_pause_state, caller_video_flags, stopped, timer_gate, mode_flag, track_count, sound_on,
                                 last_time, total_frames, current_frame, rect_sentinel, stop_trigger);
            }
        }

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
                win_emu.stop();
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
                win_emu.stop();
                return;
            }

            entry->second.handler(c);

#ifdef ENABLE_NTSTATUS_PROBE
            {
                const auto status = static_cast<uint32_t>(emu.reg<uint64_t>(x86_register::rax));
                if (c.write_status && !c.retrigger_syscall && !c.run_callback && (status & 0xC0000000) == 0xC0000000)
                {
                    win_emu.log.error("[NTSTATUS_PROBE] %s -> 0x%08X (ip=0x%" PRIx64 ")\n", entry->second.name.c_str(), status, address);
                }
            }
#endif

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
        const auto* active_thread = win_emu.vcpu(emu.index()).active_thread;

        if (context.instrumentation_callback != 0 && syscall_name != "NtContinue")
        {
            // In WoW64 processes, the instrumentation callback is the wow64.dll dispatcher
            // that transitions from 64-bit syscall return back to 32-bit guest code. It only
            // applies to WoW64 threads (those with a WOW64_CPURESERVED). Native 64-bit threads
            // in a WoW64 process (e.g. worker factory threads) have no 32-bit context to return
            // to, so skip the callback for them — they continue in 64-bit mode normally.
            if (context.is_wow64_process && (!active_thread || !active_thread->wow64_cpu_reserved.has_value()))
            {
                return;
            }

            auto rip_old = emu.reg<uint64_t>(x86_register::rip);

            const auto target = context.instrumentation_callback;
            emu.reg<uint64_t>(x86_register::rip, emu.syscall_hook_requires_rip_compensation() ? target - 2 : target);

            emu.reg<uint64_t>(x86_register::r10, rip_old);

            // On x64 hardware, SYSCALL clobbers R11 with RFLAGS. wow64cpu.dll places
            // pWow64PerThreadData (TEB64.TlsSlots[1]) in R11 before SYSCALL so the
            // instrumentation callback can find the per-thread 32-bit context via [R11+0x68].
            // Unicorn fires its hook before SYSCALL executes so R11 still holds this pointer;
            // KVM executes SYSCALL natively so R11 is already RFLAGS by the time we intercept.
            // Restore R11 to the expected pointer so both backends behave identically.
            if (active_thread && active_thread->wow64_cpu_reserved.has_value())
            {
                emu.reg<uint64_t>(x86_register::r11, active_thread->wow64_cpu_reserved->value());
            }
        }
    }

    dispatch_result syscall_dispatcher::dispatch_completion(windows_emulator& win_emu, vcpu_context& vcpu, callback_id callback_id,
                                                            completion_state* completion_state, const user_callback_result& callback_result)
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
            win_emu.stop();
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
            win_emu.stop();
            return dispatch_result::error;
        }
        catch (...)
        {
            win_emu.log.error("Completion for callback 0x%X threw an unknown exception\n", static_cast<int>(callback_id));
            win_emu.stop();
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
