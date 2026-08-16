#include "std_include.hpp"
#include "windows_emulator.hpp"

#include <pthread.h>

#include "cpu_context.hpp"

#include <utils/io.hpp>
#include <utils/string.hpp>
#include <utils/timer.hpp>
#include <utils/finally.hpp>
#include <utils/lazy_object.hpp>

#include "exception_dispatch.hpp"
#include "apiset/apiset.hpp"
#include "syscall_dispatcher.hpp"

#include "network/static_socket_factory.hpp"
#include "memory_permission_ext.hpp"
#include "devices/gpu_bridge.hpp"

namespace sogen
{
    constexpr auto MAX_INSTRUCTIONS_PER_TIME_SLICE = 0x20000;
    constexpr auto MAX_BASIC_BLOCKS_PER_TIME_SLICE = 0x8000;

    namespace
    {
        // TEMPDIAG: remove before finalizing. Dumps every thread's wait-state fields once the
        // idle loop has been spinning for a while with nothing ready, so a captured freeze
        // explains itself in the log instead of requiring live debugger inspection.
        void dump_thread_wait_states_diag(process_context& process, const vcpu_context& vcpu)
        {
            fprintf(stderr, "[SCHED_DIAG] idle spin, this vcpu's active_thread=%p\n", static_cast<void*>(vcpu.active_thread));
            for (auto& [h, thread] : process.threads)
            {
                fprintf(stderr,
                        "[SCHED_DIAG] thread id=%u terminated=%d suspended=%u waiting_for_alert=%d alerted=%d "
                        "await_objects=%zu await_msg_mask=%d await_time=%d await_host_condition=%d await_io_completion=%d\n",
                        thread.id, thread.is_terminated() ? 1 : 0, thread.suspended, thread.waiting_for_alert ? 1 : 0,
                        thread.alerted ? 1 : 0, thread.await_objects.size(), thread.await_msg_mask.has_value() ? 1 : 0,
                        thread.await_time.has_value() ? 1 : 0, thread.await_host_condition ? 1 : 0,
                        thread.await_io_completion.has_value() ? 1 : 0);
            }
        }

        void adjust_working_directory(application_settings& app_settings)
        {
            if (!app_settings.working_directory.empty())
            {
                // Do nothing
            }
#ifdef OS_WINDOWS
            else if (app_settings.application.is_relative())
            {
                app_settings.working_directory = std::filesystem::current_path();
            }
#endif
            else
            {
                app_settings.working_directory = app_settings.application.parent();
            }
        }

        void adjust_application(application_settings& app_settings)
        {
            if (app_settings.application.is_relative())
            {
                app_settings.application = app_settings.working_directory / app_settings.application;
            }
        }

        void fixup_application_settings(application_settings& app_settings)
        {
            adjust_working_directory(app_settings);
            adjust_application(app_settings);
        }

        int16_t point_x(const uint64_t lparam)
        {
            return static_cast<int16_t>(lparam & 0xFFFF);
        }

        int16_t point_y(const uint64_t lparam)
        {
            return static_cast<int16_t>((lparam >> 16) & 0xFFFF);
        }

        uint16_t high_word(const uint64_t value)
        {
            return static_cast<uint16_t>((value >> 16) & 0xFFFFu);
        }

        struct child_hit_test_result
        {
            const window* win{};
            int x{};
            int y{};
        };

        std::optional<child_hit_test_result> find_child_window_at(const process_context& process, const hwnd parent, const int x,
                                                                  const int y)
        {
            std::optional<child_hit_test_result> result{};
            for (const auto& [index, child] : process.windows)
            {
                (void)index;
                if (child.parent_handle != parent || (child.style & WS_VISIBLE) == 0 || (child.style & WS_DISABLED) != 0)
                {
                    continue;
                }

                if (x >= child.x && x < child.x + child.width && y >= child.y && y < child.y + child.height)
                {
                    const auto child_x = x - child.x;
                    const auto child_y = y - child.y;
                    if (auto descendant = find_child_window_at(process, child.handle, child_x, child_y))
                    {
                        result = descendant;
                    }
                    else
                    {
                        result = child_hit_test_result{.win = &child, .x = child_x, .y = child_y};
                    }
                }
            }

            return result;
        }

        std::optional<POINT> get_window_origin_relative_to_ancestor(const process_context& process, const hwnd window, const hwnd ancestor)
        {
            POINT origin{};
            auto current_handle = window;
            while (current_handle != 0 && current_handle != ancestor)
            {
                const auto* current = process.windows.get(current_handle);
                if (!current)
                {
                    return std::nullopt;
                }

                origin.x += current->x;
                origin.y += current->y;
                current_handle = current->parent_handle;
            }

            if (current_handle != ancestor)
            {
                return std::nullopt;
            }

            return origin;
        }

        bool is_mouse_wheel_message(const uint32_t message)
        {
            return message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
        }

        bool is_mouse_button_message(const uint32_t message)
        {
            switch (message)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return true;
            default:
                return false;
            }
        }

        bool is_mouse_button_down_message(const uint32_t message)
        {
            return message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
        }

        bool is_mouse_button_up_message(const uint32_t message)
        {
            return message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP || message == WM_XBUTTONUP;
        }

        bool is_pointer_message(const uint32_t message)
        {
            // All mouse messages go through capture/child hit-testing: while a window holds the mouse
            // capture every mouse message must reach it (so a pressed button still completes its click),
            // and otherwise each is delivered to the child under the cursor (hover, right-click, etc.).
            return message == WM_MOUSEMOVE || is_mouse_button_message(message) || is_mouse_wheel_message(message);
        }

        bool is_key_down_message(const uint32_t message)
        {
            return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        }

        bool is_key_up_message(const uint32_t message)
        {
            return message == WM_KEYUP || message == WM_SYSKEYUP;
        }

        bool is_keyboard_message(const uint32_t message)
        {
            return is_key_down_message(message) || is_key_up_message(message);
        }

        // Window button message -> RAWMOUSE usButtonFlags transition bit (winuser.h RI_MOUSE_* values).
        uint16_t raw_mouse_button_flags(const uint32_t message, const uint64_t wparam)
        {
            switch (message)
            {
            case WM_LBUTTONDOWN:
                return RI_MOUSE_LEFT_BUTTON_DOWN;
            case WM_LBUTTONUP:
                return RI_MOUSE_LEFT_BUTTON_UP;
            case WM_RBUTTONDOWN:
                return RI_MOUSE_RIGHT_BUTTON_DOWN;
            case WM_RBUTTONUP:
                return RI_MOUSE_RIGHT_BUTTON_UP;
            case WM_MBUTTONDOWN:
                return RI_MOUSE_MIDDLE_BUTTON_DOWN;
            case WM_MBUTTONUP:
                return RI_MOUSE_MIDDLE_BUTTON_UP;
            case WM_XBUTTONDOWN:
                return high_word(wparam) == XBUTTON2 ? RI_MOUSE_BUTTON_5_DOWN : RI_MOUSE_BUTTON_4_DOWN;
            case WM_XBUTTONUP:
                return high_word(wparam) == XBUTTON2 ? RI_MOUSE_BUTTON_5_UP : RI_MOUSE_BUTTON_4_UP;
            case WM_MOUSEWHEEL:
                return RI_MOUSE_WHEEL;
            case WM_MOUSEHWHEEL:
                return RI_MOUSE_HWHEEL;
            default:
                return 0;
            }
        }

        uint16_t raw_mouse_button_data(const uint32_t message, const uint64_t wparam)
        {
            if (is_mouse_wheel_message(message))
            {
                return high_word(wparam);
            }
            return 0;
        }

        uint8_t mouse_button_virtual_key(const uint32_t message, const uint64_t wparam)
        {
            switch (message)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                return VK_LBUTTON;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                return VK_RBUTTON;
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return VK_MBUTTON;
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return high_word(wparam) == XBUTTON2 ? VK_XBUTTON2 : VK_XBUTTON1;
            default:
                return 0;
            }
        }

        // Best-effort US-layout virtual-key -> PS/2 set-1 scan code for the RAWKEYBOARD MakeCode field
        // (games that bind by scan code need it; the VKey is delivered too for those that use it).
        uint16_t vk_to_scan_code(const uint16_t vk)
        {
            switch (vk)
            {
            case VK_ESCAPE:
                return 0x01;
            case VK_RETURN:
                return 0x1C;
            case VK_SPACE:
                return 0x39;
            case VK_TAB:
                return 0x0F;
            case VK_BACK:
                return 0x0E;
            case VK_SHIFT:
                return 0x2A;
            case VK_CONTROL:
                return 0x1D;
            case VK_UP:
                return 0x48;
            case VK_DOWN:
                return 0x50;
            case VK_LEFT:
                return 0x4B;
            case VK_RIGHT:
                return 0x4D;
            default:
                if (vk >= 'A' && vk <= 'Z')
                {
                    static constexpr std::array<uint8_t, 26> letter_scan = {0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                                                            0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                                                            0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
                    return letter_scan[static_cast<size_t>(vk - 'A')];
                }
                if (vk >= '1' && vk <= '9')
                {
                    return static_cast<uint16_t>(0x02 + (vk - '1'));
                }
                if (vk == '0')
                {
                    return 0x0B;
                }
                return 0;
            }
        }

        uint64_t pack_point(const int x, const int y)
        {
            return static_cast<uint16_t>(x) | (static_cast<uint64_t>(static_cast<uint16_t>(y)) << 16);
        }

        struct pointer_target
        {
            hwnd window{};
            int x{};
            int y{};
        };

        // Single authority for routing a top-level-local pointer event to its destination window.
        // Backends only forward (top-level window, top-level-local x/y); capture and child hit-testing
        // are decided here, never in the host backends.
        pointer_target route_pointer(process_context& process, const hwnd top_level, const int x, const int y)
        {
            if (process.mouse_capture_window != 0)
            {
                if (const auto* captured = process.windows.get(process.mouse_capture_window);
                    captured && (captured->style & WS_VISIBLE) != 0)
                {
                    // Capture sends every pointer event to the capturing window, even one reported for
                    // another top-level. Translate via screen coordinates (origin relative to the root)
                    // so it works across top-levels; for a child of top_level this is the same offset.
                    const auto captured_origin = get_window_origin_relative_to_ancestor(process, captured->handle, 0);
                    const auto top_level_origin = get_window_origin_relative_to_ancestor(process, top_level, 0);
                    if (captured_origin && top_level_origin)
                    {
                        const auto screen_x = top_level_origin->x + x;
                        const auto screen_y = top_level_origin->y + y;
                        return {.window = captured->handle, .x = screen_x - captured_origin->x, .y = screen_y - captured_origin->y};
                    }
                }
                else
                {
                    process.mouse_capture_window = 0;
                }

                return {.window = top_level, .x = x, .y = y};
            }

            // Otherwise deliver to the deepest visible/enabled child under the cursor.
            if (const auto child = find_child_window_at(process, top_level, x, y))
            {
                return {.window = child->win->handle, .x = child->x, .y = child->y};
            }

            return {.window = top_level, .x = x, .y = y};
        }

        bool has_pending_host_wait(const process_context& process)
        {
            for (const auto& thread_entry : process.threads)
            {
                if (thread_entry.second.await_host_condition)
                {
                    return true;
                }
            }

            return false;
        }

        vcpu_context* find_vcpu_running_thread(windows_emulator& win_emu, const emulator_thread& thread);

        void perform_context_switch_work(windows_emulator& win_emu, vcpu_context& vcpu)
        {
            auto& threads = win_emu.process.threads;

            for (auto it = threads.begin(); it != threads.end();)
            {
                if (!it->second.is_terminated() || it->second.ref_count > 0)
                {
                    ++it;
                    continue;
                }

                if (auto* running_on = find_vcpu_running_thread(win_emu, it->second))
                {
                    if (running_on != &vcpu)
                    {
                        // Another vCPU still has this thread loaded; it will detach it soon.
                        ++it;
                        continue;
                    }

                    running_on->active_thread = nullptr;
                }

                const auto [new_it, deleted] = threads.erase(it);
                if (!deleted)
                {
                    ++it;
                }
                else
                {
                    it = new_it;
                }
            }

            auto& devices = win_emu.process.devices;

            // Crappy mechanism to prevent mutation while iterating.
            const auto was_blocked = devices.block_mutation(true);
            const auto _ = utils::finally([&] { devices.block_mutation(was_blocked); });

            for (auto& dev : devices | std::views::values)
            {
                dev.work(win_emu);
            }

            // Drive the D3DKMTEscape GPU processor's presents on the same cadence the SogenGpu io_device uses.
            if (const auto& gpu_processor = win_emu.process.dxgk.gpu_processor)
            {
                pump_gpu_presents(gpu_processor.get(), win_emu);
            }
        }

        emulator_thread* get_thread_by_id(process_context& process, const uint32_t id)
        {
            for (auto& t : process.threads | std::views::values)
            {
                if (t.id == id)
                {
                    return &t;
                }
            }

            return nullptr;
        }

        void dispatch_next_apc(windows_emulator& win_emu, vcpu_context& vcpu, emulator_thread& thread)
        {
            assert(vcpu.active_thread == &thread);

            auto& emu = vcpu.cpu;
            auto& apcs = thread.pending_apcs;
            if (apcs.empty())
            {
                return;
            }

            thread.setup_if_necessary(vcpu.cpu, win_emu.process);

            win_emu.callbacks.on_generic_activity("APC Dispatch");

            const auto next_apx = apcs.front();
            apcs.erase(apcs.begin());

            if (next_apx.restamp_io_status_block && next_apx.apc_argument2)
            {
                // Stamp the WoW64 32-bit I/O status block (Status @0, Information @4) here, immediately
                // before the completion routine runs. This mirrors the real kernel, which writes the
                // IO_STATUS_BLOCK as it dispatches the completion APC rather than when the I/O is first
                // queued -- so any value the guest left in that buffer while the async request was pending
                // (e.g. reusing it for an intervening synchronous call) is correctly superseded.
                const auto status32 = static_cast<uint32_t>(next_apx.io_status);
                emu.write_memory(next_apx.apc_argument2, &status32, sizeof(status32));
                emu.write_memory(next_apx.apc_argument2 + sizeof(status32), &next_apx.io_information, sizeof(next_apx.io_information));
            }

            struct
            {
                CONTEXT64 context{};
                CONTEXT_EX context_ex{};
                KCONTINUE_ARGUMENT continue_argument{};
            } stack_layout;

            static_assert(offsetof(decltype(stack_layout), continue_argument) == 0x4F0);

            stack_layout.context.P1Home = next_apx.apc_argument1;
            stack_layout.context.P2Home = next_apx.apc_argument2;
            stack_layout.context.P3Home = next_apx.apc_argument3;
            stack_layout.context.P4Home = next_apx.apc_routine;

            stack_layout.continue_argument.ContinueFlags |= KCONTINUE_FLAG_TEST_ALERT;

            auto& ctx = stack_layout.context;
            ctx.ContextFlags = CONTEXT64_ALL;
            cpu_context::save(emu, ctx);

            const auto initial_sp = emu.reg(x86_register::rsp);
            const auto new_sp = align_down(initial_sp - sizeof(stack_layout), 0x100);

            emu.write_memory(new_sp, stack_layout);

            emu.reg(x86_register::rsp, new_sp);
            emu.reg(x86_register::rip, win_emu.process.ki_user_apc_dispatcher);
        }

        vcpu_context* find_vcpu_running_thread(windows_emulator& win_emu, const emulator_thread& thread)
        {
            for (uint32_t i = 0; i < win_emu.vcpu_count(); ++i)
            {
                auto& vcpu = win_emu.vcpu(i);
                if (vcpu.active_thread == &thread)
                {
                    return &vcpu;
                }
            }

            return nullptr;
        }

        // EMULATOR_STICKY_VCPU: pins each guest thread to whichever vCPU first ran it, for the
        // lifetime of the process. Originally built as a bisection tool for a live FEX-backend crash
        // investigation (a WoW64 guest reliably corrupting memory under --vcpus > 1 - see the bug-hunt
        // workflow report referenced in the plan): landing every migration-family fix the research
        // turned up (cross-engine call/ret shadow-stack ownership, the non-atomic active-context/
        // active-thread pair read cross-thread) did NOT resolve the crash, but disabling migration
        // entirely via this flag does, reliably (6/6 vs 0/8 in matched A/B batches on an idle
        // machine). The true remaining root cause is still open, so until it's found this is the
        // practical way to get a stable FEX+WoW64+multi-vCPU run - recommended for MW2-like workloads.
        // Off by default: it defeats real load-balancing (a thread never migrates to a less-busy
        // vCPU even if one is idle), and WHP's own multi-vCPU implementation has no evidence of
        // needing it. Scoped to the anonymous namespace (not a windows_emulator member) since running
        // under kernel_lock_ (see vcpu_worker) makes a plain map safe here without one.
        bool sticky_vcpu_enabled()
        {
            static const bool enabled = std::getenv("EMULATOR_STICKY_VCPU") != nullptr;
            return enabled;
        }

        std::unordered_map<uint32_t, uint32_t>& sticky_vcpu_assignments()
        {
            static std::unordered_map<uint32_t, uint32_t> assignments{};
            return assignments;
        }

        bool switch_to_thread(windows_emulator& win_emu, vcpu_context& vcpu, emulator_thread& thread, const bool force = false)
        {
            if (thread.is_terminated())
            {
                return false;
            }

            // A thread that is loaded on another vCPU can only run there.
            if (auto* running_on = find_vcpu_running_thread(win_emu, thread); running_on && running_on != &vcpu)
            {
                return false;
            }

            auto& emu = vcpu.cpu;
            auto& context = win_emu.process;

            const auto is_ready = thread.is_thread_ready(win_emu);
            const auto has_pending_status = thread.pending_status.has_value();
            const auto can_dispatch_apcs = thread.apc_alertable && !thread.pending_apcs.empty();

            if (!is_ready && !force && !can_dispatch_apcs)
            {
                return false;
            }

            // Checked only once every other gate has passed, so a thread we're merely scanning past
            // (not actually about to switch to) never claims a vCPU it will never run on.
            if (sticky_vcpu_enabled())
            {
                auto& assignments = sticky_vcpu_assignments();
                const auto [it, inserted] = assignments.try_emplace(thread.id, vcpu.cpu.index());
                if (!inserted && it->second != vcpu.cpu.index())
                {
                    return false;
                }
            }

            auto* active_thread = vcpu.active_thread;

            if (active_thread != &thread)
            {
                if (active_thread)
                {
                    win_emu.callbacks.on_thread_switch(*active_thread, thread);
                    active_thread->save(emu);
                }

                vcpu.active_thread = &thread;

                thread.restore(emu);
            }

            thread.setup_if_necessary(emu, context);

            if (can_dispatch_apcs && !has_pending_status)
            {
                thread.mark_as_ready(STATUS_USER_APC);
                dispatch_next_apc(win_emu, vcpu, thread);
            }

            thread.apc_alertable = false;
            return true;
        }

        bool switch_to_thread(windows_emulator& win_emu, vcpu_context& vcpu, const handle thread_handle)
        {
            auto* thread = win_emu.process.threads.get(thread_handle);
            if (!thread)
            {
                throw std::runtime_error("Bad thread handle");
            }

            return switch_to_thread(win_emu, vcpu, *thread);
        }

        bool switch_to_next_thread(windows_emulator& win_emu, vcpu_context& vcpu)
        {
            perform_context_switch_work(win_emu, vcpu);

            auto& context = win_emu.process;

            bool next_thread = false;

            for (auto& t : context.threads | std::views::values)
            {
                if (next_thread)
                {
                    if (switch_to_thread(win_emu, vcpu, t))
                    {
                        return true;
                    }

                    continue;
                }

                if (&t == vcpu.active_thread)
                {
                    next_thread = true;
                }
            }

            for (auto& t : context.threads | std::views::values)
            {
                if (switch_to_thread(win_emu, vcpu, t))
                {
                    return true;
                }
            }

            return false;
        }

        struct instruction_tick_clock : utils::tick_clock
        {
            const uint64_t* instructions_{};

            instruction_tick_clock(const uint64_t& instructions, const system_time_point system_start = {},
                                   const steady_time_point steady_start = {})
                : tick_clock(1000, system_start, steady_start),
                  instructions_(&instructions)
            {
            }

            uint64_t ticks() override
            {
                return *this->instructions_;
            }
        };

        std::unique_ptr<utils::clock> get_clock(emulator_interfaces& interfaces, const uint64_t& instructions, const bool use_relative_time)
        {
            if (interfaces.clock)
            {
                return std::move(interfaces.clock);
            }

            if (use_relative_time)
            {
                return std::make_unique<instruction_tick_clock>(instructions);
            }

            return std::make_unique<utils::clock>();
        }

        std::unique_ptr<network::dns_lookup> get_dns_lookup(emulator_interfaces& interfaces)
        {
            if (interfaces.dns_lookup)
            {
                return std::move(interfaces.dns_lookup);
            }

            return std::make_unique<network::dns_lookup>();
        }

        std::unique_ptr<network::socket_factory> get_socket_factory(emulator_interfaces& interfaces)
        {
            if (interfaces.socket_factory)
            {
                return std::move(interfaces.socket_factory);
            }

#ifdef OS_EMSCRIPTEN
            return network::create_static_socket_factory();
#else
            return std::make_unique<network::socket_factory>();
#endif
        }

        std::unique_ptr<ui_backend> get_ui_backend(emulator_interfaces& interfaces)
        {
            if (interfaces.ui)
            {
                return std::move(interfaces.ui);
            }

            return create_default_ui_backend();
        }

        std::unique_ptr<audio_backend> get_audio_backend(emulator_interfaces& interfaces)
        {
            if (interfaces.audio)
            {
                return std::move(interfaces.audio);
            }

            return create_default_audio_backend();
        }

        // The guest must see at least as many logical processors as there are vCPUs, otherwise a
        // thread running on a higher-indexed vCPU would report a processor number the guest
        // considers out of range. The configured fake value still wins when it is larger (e.g. the
        // anti-analysis default of 4 with a single vCPU).
        fake_environment_config effective_fake_env(const emulator_settings& settings, const uint32_t vcpu_count)
        {
            auto fake_env = settings.fake_env;
            fake_env.number_of_processors = std::max(fake_env.number_of_processors, vcpu_count);
            return fake_env;
        }
    }

    windows_emulator::windows_emulator(std::unique_ptr<x86_64_emulator> emu, application_settings app_settings,
                                       const emulator_settings& settings, emulator_callbacks callbacks, emulator_interfaces interfaces)
        : windows_emulator(std::move(emu), settings, std::move(callbacks), std::move(interfaces))
    {
        fixup_application_settings(app_settings);
        this->application_settings_ = std::move(app_settings);
    }

    windows_emulator::windows_emulator(std::unique_ptr<x86_64_emulator> emu, const emulator_settings& settings,
                                       emulator_callbacks callbacks, emulator_interfaces interfaces)
        : emu_(std::move(emu)),
          clock_(get_clock(interfaces, this->executed_instructions_, settings.use_relative_time)),
          dns_lookup_(get_dns_lookup(interfaces)),
          socket_factory_(get_socket_factory(interfaces)),
          ui_backend_(get_ui_backend(interfaces)),
          audio_backend_(get_audio_backend(interfaces)),
          emulation_root{settings.emulation_root.empty() ? settings.emulation_root : absolute(settings.emulation_root)},
          fake_env(effective_fake_env(settings, static_cast<uint32_t>(this->emu_->vcpu_count()))),
          callbacks(std::move(callbacks)),
          file_sys(emulation_root.empty() ? emulation_root : emulation_root / "filesys"),
          memory(*this->emu_),
          registry(settings.load_registry
                       ? registry_manager{emulation_root.empty() ? settings.registry_directory : emulation_root / "registry"}
                       : registry_manager{}),
          mod_manager(memory, file_sys, this->callbacks),
          process(*this->emu_, memory, *this->clock_, this->callbacks),
          use_relative_time_(settings.use_relative_time),
          instruction_precision_(settings.use_instruction_precision && this->emu_->supports_instruction_counting()),
          vcpu_count_(static_cast<uint32_t>(this->emu_->vcpu_count()))
    {
        if (this->vcpu_count_ == 0)
        {
            throw std::invalid_argument("At least one vCPU is required");
        }

        if (this->vcpu_count_ > 1 && !this->emu_->supports_multiple_vcpus())
        {
            throw std::invalid_argument("The " + this->emu_->get_name() + " backend does not support multiple vCPUs");
        }

        if (this->vcpu_count_ > 1)
        {
            // Deliberate hard errors instead of silent clamping (docs/multi-vcpu-design.md).
            if (this->instruction_precision_)
            {
                throw std::invalid_argument("Instruction precision requires a single vCPU");
            }

            if (this->use_relative_time_)
            {
                throw std::invalid_argument("Relative time requires a single vCPU");
            }
        }

        this->vcpus_.reserve(this->vcpu_count_);
        for (uint32_t i = 0; i < this->vcpu_count_; ++i)
        {
            this->vcpus_.push_back(std::make_unique<vcpu_context>(this->emu_->get_cpu(i)));
        }

        this->ui_backend_->set_event_sink([this](const ui_event& event) { this->handle_ui_event(event); });
#ifndef OS_WINDOWS
        if (this->emulation_root.empty())
        {
            throw std::runtime_error("Emulation root directory can not be empty!");
        }
#endif

        for (const auto& mapping : settings.path_mappings)
        {
            this->file_sys.map(mapping.first, mapping.second);
        }

        for (const auto& mapping : settings.port_mappings)
        {
            this->map_port(mapping.first, mapping.second);
        }

        // Register the sogen Vulkan ICD so a real Khronos loader in the guest discovers it via the standard
        // HKLM\SOFTWARE\Khronos\Vulkan\Drivers key (native + WOW6432Node), no VK_DRIVER_FILES needed. Harmless
        // if the guest runs the shim as vulkan-1.dll instead of a real loader, or if no registry hive exists.
        try
        {
            const auto register_icd = [this](const char* drivers_key, const char* manifest_path) {
                const auto key = this->registry.create_key({drivers_key});
                constexpr uint32_t enabled = 0;
                const auto* bytes = reinterpret_cast<const std::byte*>(&enabled);
                this->registry.set_value(key, manifest_path, 4 /* REG_DWORD */, std::span<const std::byte>(bytes, sizeof(enabled)));
            };
            register_icd(R"(\Registry\Machine\Software\Khronos\Vulkan\Drivers)", R"(C:\Windows\System32\sogen_vk_icd.json)");
            register_icd(R"(\Registry\Machine\Software\WOW6432Node\Khronos\Vulkan\Drivers)", R"(C:\Windows\SysWOW64\sogen_vk_icd.json)");
        }
        catch (const std::exception&)
        {
        }

        this->setup_hooks();
    }

    windows_emulator::~windows_emulator() = default;

    void windows_emulator::setup_process_if_necessary()
    {
        if (this->setup_completed_)
        {
            return;
        }

        this->setup_completed_ = true;

        this->setup_process();
    }

    void windows_emulator::setup_process()
    {
        const auto& emu = this->emu();
        auto& context = this->process;

        this->version.load_from_registry(this->registry, this->log);

        this->mod_manager.map_main_modules(this->emu(), this->application_settings_.application, this->version, context, this->log);
        this->install_section_first_execution_hooks();

        const auto* executable = this->mod_manager.executable;
        const auto* ntdll = this->mod_manager.ntdll;
        const auto* win32u = this->mod_manager.win32u;

        const auto apiset_data = apiset::obtain(this->emulation_root);

        this->process.setup(*this, this->application_settings_, *executable, *ntdll, apiset_data, this->mod_manager.wow64_modules_.ntdll32);

        const auto ntdll_data = emu.read_memory(ntdll->image_base, static_cast<size_t>(ntdll->size_of_image));
        const auto win32u_data = emu.read_memory(win32u->image_base, static_cast<size_t>(win32u->size_of_image));

        this->dispatcher.setup(ntdll->exports, ntdll_data, win32u->exports, win32u_data);

        const auto main_thread_id = context.create_thread(this->memory, this->mod_manager.executable->entry_point, 0,
                                                          this->mod_manager.executable->size_of_stack_reserve, 0, true);

        switch_to_thread(*this, this->vcpu(0), main_thread_id);

        // The desktop window is created in setup() before any thread exists, so its owning thread id
        // stays 0 there. Real Windows reports a valid thread for GetWindowThreadProcessId(GetDesktopWindow()),
        // and DirectSound depends on it: SetCooperativeLevel stores
        // GetWindowThreadProcessId(GetRootParentWindow(hwnd)), and IDirectSoundBuffer::Play rejects every
        // call with DSERR_PRIOLEVELNEEDED while that id is 0. user32 resolves the id via the
        // NtUserQueryWindow syscall (which reads window::thread_id) whenever the shared aheList entry's
        // pOwner is null, so setting thread_id is sufficient here.
        //
        // Deliberately does NOT publish anything into the aheList entry's pOwner: every other window
        // stores a win32-thread-info guest POINTER there (set_user_handle_owner), and no such structure
        // exists yet for the main thread at this point. Publishing the raw thread id in its place sends
        // user32's pOwner-consuming client paths dereferencing a non-pointer, which derails dsound's
        // device bring-up and stalls a WoW64 game boot (MW2) before its audio init completes.
        if (auto* desktop = context.windows.get(context.default_desktop_window_handle))
        {
            const auto desktop_thread_id = this->current_thread().id;
            desktop->thread_id = desktop_thread_id;
            desktop->guest.access([&](USER_WINDOW& window) { window.threadId = desktop_thread_id; });
        }
    }

    void windows_emulator::yield_thread(vcpu_context& vcpu, const bool alertable)
    {
        this->kernel_lock_.assert_held();

        vcpu.switch_thread = true;
        vcpu.thread().apc_alertable = alertable;
        vcpu.cpu.stop();
    }

    bool windows_emulator::perform_thread_switch(vcpu_context& vcpu)
    {
        std::unique_lock lock(this->kernel_lock_);
        return this->perform_thread_switch(vcpu, lock);
    }

    bool windows_emulator::perform_thread_switch(vcpu_context& vcpu, std::unique_lock<kernel_lock>& lock)
    {
        this->kernel_lock_.assert_held();

        const auto needed_switch = vcpu.switch_thread.exchange(false);

        static thread_local int idle_spin_count = 0;
        static const bool sched_diag = std::getenv("EMULATOR_SCHED_DIAG") != nullptr;

        while (!switch_to_next_thread(*this, vcpu))
        {
            if (sched_diag)
            {
                ++idle_spin_count;
                if (idle_spin_count == 500 || idle_spin_count == 2000 || idle_spin_count == 5000)
                {
                    dump_thread_wait_states_diag(this->process, vcpu);
                }
            }

            if (this->vcpu_count_ > 1 && vcpu.active_thread)
            {
                // Nothing runnable for this vCPU: detach the stale thread so another
                // vCPU can pick it up once it becomes ready.
                vcpu.active_thread->save(vcpu.cpu);
                vcpu.active_thread = nullptr;
            }

            const auto host_wait_pending = has_pending_host_wait(this->process);

            // Idle: nothing is ready. Release the kernel lock while pumping UI events
            // and sleeping so other threads (UI delivery, vCPU workers) can run.
            lock.unlock();

            if (this->vcpu_count_ == 1)
            {
                // With workers, the thread calling start() pumps UI events instead.
                this->ui_backend_->pump_events();
                this->callbacks.on_event_pump();
            }

            if (this->use_relative_time_)
            {
                this->executed_instructions_ += MAX_INSTRUCTIONS_PER_TIME_SLICE;
            }
            else if (host_wait_pending)
            {
                // A host wait (e.g. a GPU semaphore) is parked - re-poll immediately to wake it promptly.
                std::this_thread::yield();
            }
            else
            {
                // Only timed waits remain; nothing host-side wakes sooner than wall-clock, so don't busy-spin.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            lock.lock();

            if (this->should_stop)
            {
                vcpu.switch_thread = needed_switch;
                return false;
            }
        }

        idle_spin_count = 0;
        return true;
    }

    // pthread_create's start routine can't be a capturing lambda; args carries what the old
    // std::thread-based spawn loop used to capture directly, and this owns/frees it (matching
    // pthread_create's contract that the start routine receives sole ownership of its argument).
    void* windows_emulator::vcpu_worker_thread_trampoline(void* raw_args)
    {
        const std::unique_ptr<vcpu_worker_thread_args> args(static_cast<vcpu_worker_thread_args*>(raw_args));

        try
        {
            args->self->vcpu_worker(args->self->vcpu(args->index));
        }
        catch (const std::exception& e)
        {
            args->self->log.error("vCPU %u worker terminated: %s\n", args->index, e.what());
            args->self->stop();
        }

        --(*args->active_workers);
        return nullptr;
    }

    void windows_emulator::vcpu_worker(vcpu_context& vcpu)
    {
        std::unique_lock lock(this->kernel_lock_);

        while (!this->should_stop)
        {
            if (vcpu.switch_thread || !vcpu.active_thread || !vcpu.thread().is_thread_ready(*this))
            {
                if (!this->perform_thread_switch(vcpu, lock))
                {
                    break;
                }
            }

            // Guest code executes with the kernel lock released; hook callbacks
            // (syscalls, exceptions, exec hooks) re-acquire it on VM exit.
            lock.unlock();
            vcpu.cpu.start();
            lock.lock();

            if (!vcpu.switch_thread && !vcpu.cpu.has_violation())
            {
                break;
            }
        }

        lock.unlock();

        // One vCPU winding down (process exit, fatal error) ends the whole run.
        this->stop();
    }

    bool windows_emulator::activate_thread(vcpu_context& vcpu, const uint32_t id)
    {
        const std::scoped_lock lock(this->kernel_lock_);

        auto* thread = get_thread_by_id(this->process, id);
        if (!thread)
        {
            return false;
        }

        return switch_to_thread(*this, vcpu, *thread, true);
    }

    void windows_emulator::on_instruction_execution(vcpu_context& vcpu, const uint64_t address)
    {
        auto& thread = vcpu.thread();

        if (!thread.callback_stack.empty() && address == this->process.zw_callback_return)
        {
            thread.callback_return_rax = vcpu.cpu.reg<uint64_t>(x86_register::rax);
        }

        ++this->executed_instructions_;
        const auto thread_insts = ++thread.executed_instructions;
        if (thread_insts % MAX_INSTRUCTIONS_PER_TIME_SLICE == 0)
        {
            this->yield_thread(vcpu);
        }

        thread.previous_ip = thread.current_ip;
        thread.current_ip = vcpu.cpu.read_instruction_pointer();

        if (!this->uses_section_first_execution_hooks())
        {
            this->track_section_first_execution(address);
        }

        this->callbacks.on_instruction(address);
    }

    bool windows_emulator::uses_section_first_execution_hooks() const
    {
        return !this->emu().supports_global_memory_execution_hooks();
    }

    void windows_emulator::track_section_first_execution(const uint64_t address)
    {
        auto* mod = this->mod_manager.find_by_address(address);
        if (!mod)
        {
            return;
        }

        auto* hook_states = [&]() -> std::vector<emulator_hook*>* {
            const auto entry = this->section_first_execution_hooks_.find(mod->image_base);
            return entry == this->section_first_execution_hooks_.end() ? nullptr : &entry->second;
        }();

        for (size_t i = 0; i < mod->sections.size(); ++i)
        {
            auto& section = mod->sections[i];
            if (!is_within_start_and_length(address, section.region.start, section.region.length))
            {
                continue;
            }

            if (section.first_execute.has_value())
            {
                return;
            }

            section.first_execute = address;

            if (hook_states && i < hook_states->size() && (*hook_states)[i])
            {
                auto* hook = (*hook_states)[i];
                (*hook_states)[i] = nullptr;
                this->emu().delete_hook(hook);
            }

            this->callbacks.on_section_first_execution(*mod, section, address);

            return;
        }
    }

    void windows_emulator::clear_section_first_execution_hooks()
    {
        for (const auto& hooks : this->section_first_execution_hooks_ | std::views::values)
        {
            for (auto* hook : hooks)
            {
                if (hook)
                {
                    this->emu().delete_hook(hook);
                }
            }
        }

        this->section_first_execution_hooks_.clear();
    }

    void windows_emulator::install_section_first_execution_hook(const mapped_module& mod, const size_t section_index)
    {
        if (!this->uses_section_first_execution_hooks() || section_index >= mod.sections.size())
        {
            return;
        }

        const auto& section = mod.sections[section_index];
        if (section.first_execute.has_value() || section.region.length == 0)
        {
            return;
        }

        auto& hooks = this->section_first_execution_hooks_[mod.image_base];
        if (hooks.size() < mod.sections.size())
        {
            hooks.resize(mod.sections.size());
        }

        if (hooks[section_index])
        {
            return;
        }

        hooks[section_index] = this->emu().hook_memory_range_execution(section.region.start, section.region.length,
                                                                       [this](cpu_interface&, const uint64_t address) {
                                                                           const std::scoped_lock lock(this->kernel_lock_);
                                                                           this->track_section_first_execution(address); //
                                                                       });
    }

    void windows_emulator::install_section_first_execution_hooks()
    {
        if (!this->uses_section_first_execution_hooks())
        {
            return;
        }

        for (const auto& mod : this->mod_manager.modules() | std::views::values)
        {
            for (size_t i = 0; i < mod.sections.size(); ++i)
            {
                this->install_section_first_execution_hook(mod, i);
            }
        }
    }

    void windows_emulator::install_d3d9_caps_patch_hook(const mapped_module& mod)
    {
        // Real Microsoft d3d9.dll unconditionally strips the D3DCAPS2_CANMANAGERESOURCE caps bit
        // (bit 28) inside its own QueryLHDDICaps, then stores the stripped value back into the Caps2
        // field (offset +0xc of the struct the routine holds a pointer to). With the bit stripped,
        // d3d9.dll keeps every D3DPOOL_MANAGED texture's pixels in its OWN private CMipMap sysmem copy
        // and never routes the app's LockRect through the driver -- so a UMD-based D3D9 implementation
        // sees pfnLock/pfnUnlock traffic whose buffers the app never writes to, and every texture it
        // ever samples is a zero-filled allocation. Keeping the bit set is what opens the
        // driver-managed path (docs/d3d9-roadmap.md's D3DPOOL_MANAGED entries have the full history).
        //
        // This is done by neutering the strip INSTRUCTION ITSELF -- an in-place, same-length patch of
        // d3d9.dll's mapped image -- rather than by hooking execution just past the store and repairing
        // the field. The behavioral effect is identical, but a plain memory write works on every
        // backend, whereas a fine-grained execution hook does not: the FEX and KVM backends run the
        // guest natively and accept hook_memory_execution purely for API compatibility, never firing it
        // (see fex_x86_64_emulator.cpp's own comment). Under those backends the old hook silently did
        // nothing -- it still logged "installed", because the RVA pattern check passed -- which is
        // precisely how Modern Warfare 2 (a 32-bit WoW64 title, run on FEX) reached a steady, crash-free
        // render loop that presented a solid black frame: real draws, real submits, every sampled
        // texture all zeros.
        //
        // The patch turns each architecture's strip into the corresponding SET of the same bit, rather
        // than merely deleting the strip. That distinction is load-bearing: this UMD does not report
        // D3DCAPS2_CANMANAGERESOURCE in D3DCAPS9::Caps2 at all (see sogen_d3d9_umd.cpp's fill_d3d9caps
        // -- the bit is not forceable through the reported-caps surface, which is the whole reason this
        // patch exists), so the value reaching the strip never has bit 28 set and simply removing the
        // strip would change nothing. Forcing the bit ON here reproduces exactly what the previous
        // post-store execution hook did (`value | can_manage_resource_bit`).
        //
        // Each architecture's real d3d9.dll compiles the strip differently, so each has its own
        // separately-RE'd RVA and byte pattern. In both cases the replacement is exactly as long as the
        // instruction it replaces (no relocation), and the instruction that follows is the `mov` store,
        // which reads no flags -- so the differing flag side effects are harmless.
        constexpr uint16_t machine_amd64 = 0x8664;
        constexpr uint16_t machine_i386 = 0x014c;

        // Verifies the exact expected bytes at `rva` before overwriting the first `patch.size()` of them,
        // so a d3d9.dll build this was not RE'd against is left completely untouched.
        const auto patch_strip = [&](const uint64_t rva, const std::span<const uint8_t> expected, const std::span<const uint8_t> patch,
                                     const char* sha256, const char* arch_note) {
            std::array<uint8_t, 8> actual{};
            const auto actual_view = std::span(actual).first(expected.size());
            if (!this->emu().try_read_memory(mod.image_base + rva, actual.data(), expected.size()) ||
                !std::equal(actual_view.begin(), actual_view.end(), expected.begin()))
            {
                this->log.warn("d3d9.dll caps-patch RVA pattern mismatch at image_base+0x%llx (sha256 %s expected) -- "
                               "MANAGED-pool caps-forcing disabled for this build\n",
                               static_cast<unsigned long long>(rva), sha256);
                return;
            }

            this->emu().write_memory(mod.image_base + rva, patch.data(), patch.size());
            this->log.info("d3d9.dll D3DPOOL_MANAGED caps-forcing patch applied at 0x%llx%s\n",
                           static_cast<unsigned long long>(mod.image_base + rva), arch_note);
        };

        if (mod.machine == machine_amd64)
        {
            // x64 system32/d3d9.dll (sha256 bb65372a53445b5607cbd705a29b4671ab1fb250bef32b3fd0377704088c366c):
            // `btr eax, 0x1c` (0F BA F0 1C) then `mov [rsi+0xc], eax`. BTR and BTS share an encoding
            // that differs only in the ModRM reg field (/6 vs /5), so flipping F0 -> E8 turns the
            // "clear bit 28" into "set bit 28" in place, same four bytes.
            static constexpr std::array<uint8_t, 7> expected = {0x0F, 0xBA, 0xF0, 0x1C, 0x89, 0x46, 0x0C};
            static constexpr std::array<uint8_t, 4> patch = {0x0F, 0xBA, 0xE8, 0x1C};
            patch_strip(0x158af, expected, patch, "bb65372a53445b5607cbd705a29b4671ab1fb250bef32b3fd0377704088c366c", "");
            return;
        }

        if (mod.machine == machine_i386)
        {
            // 32-bit syswow64/d3d9.dll (sha256 99840c2a6b9b75011dfbb3456644e90fa7c2728b10480db1b87f7fd2e8897302):
            // `and eax, 0xEFFFFFFF` (25 FF FF FF EF) then `mov [ebx+0xc], eax` -- a literal AND rather
            // than x64's BTR, and the struct pointer is in EBX. `and eax, imm32` (opcode 25) and
            // `or eax, imm32` (opcode 0D) are both 5-byte EAX-accumulator forms, so the whole
            // instruction is rewritten to `or eax, 0x10000000` in place, same 5 bytes.
            static constexpr std::array<uint8_t, 8> expected = {0x25, 0xFF, 0xFF, 0xFF, 0xEF, 0x89, 0x43, 0x0C};
            static constexpr std::array<uint8_t, 5> patch = {0x0D, 0x00, 0x00, 0x00, 0x10};
            patch_strip(0x51c91, expected, patch, "99840c2a6b9b75011dfbb3456644e90fa7c2728b10480db1b87f7fd2e8897302", " (x86/WoW64)");

            this->install_d3d9_flip_target_hook(mod);
            return;
        }
    }

    void windows_emulator::install_d3d9_flip_target_hook(const mapped_module& mod)
    {
        // MW2 (and any fullscreen-exclusive D3DSWAPEFFECT app) presents through d3d9's own DDraw
        // flip path: IDirect3DDevice9::Present -> CSwapChain::PresentMain -> FlipToSurface -> the
        // DDraw HAL Flip thunk DdFlipLH (32-bit d3d9.dll RVA 0xbebe0). DdFlipLH's first act is
        //   ebx = flipData->lpSurfTarg;  device = *(ebx + 0x44);
        // i.e. it fetches the flip's device from the *target* surface's kernel handle.
        //
        // In a fullscreen flip chain d3d9 gives every buffer a "kernel handle" (a driver-side
        // DDraw surface local). The back buffers get one via the in-process create-surface DDI,
        // but the fullscreen primary/scanout surface never does in sogen's headless GPU model:
        // there is no real scanout allocation to back it, so CreateSurfaceLH leaves its handle
        // null (the path that would fill it, D3DKMTGetSharedPrimaryHandle, is never reached for
        // this surface). The first Present still succeeds because the rendered back buffer's
        // handle is valid, but FlipToSurface's tail rotation then cascades the primary's null
        // handle into the back buffer, so the second Present hands DdFlipLH a null lpSurfTarg and
        // it faults reading offset +0x44 of a null object.
        //
        // lpSurfCurr (the current/front surface, flip-data offset +4) is always valid and carries
        // the same device pointer at +0x44. When lpSurfTarg is null we substitute lpSurfCurr as
        // the flip target: a faithful no-op "flip to self" for a headless swap chain (there is no
        // scanout to page-flip), which lets DdFlipLH obtain the device and Flush normally instead
        // of dereferencing null. Mirrors install_d3d9_caps_patch_hook's "intercept one specific
        // broken call and make it succeed" pattern.
        constexpr uint16_t machine_i386 = 0x014c;
        if (mod.machine != machine_i386)
        {
            return;
        }

        // Guard on DdFlipLH's prologue (`mov edi,edi; push ebp; mov ebp,esp; and esp,...`) so a
        // differently-compiled d3d9 is not silently patched at the wrong address.
        constexpr uint64_t flip_rva = 0xbebe0;
        constexpr std::array<uint8_t, 6> expected_prologue = {0x8b, 0xff, 0x55, 0x8b, 0xec, 0x83};
        std::array<uint8_t, 6> actual_prologue{};
        if (!this->emu().try_read_memory(mod.image_base + flip_rva, actual_prologue.data(), actual_prologue.size()) ||
            actual_prologue != expected_prologue)
        {
            this->log.warn("d3d9.dll DdFlipLH prologue mismatch at image_base+0x%llx -- fullscreen-flip null-target guard "
                           "disabled for this build\n",
                           static_cast<unsigned long long>(flip_rva));
            return;
        }

        auto* hook = this->emu().hook_memory_execution(mod.image_base + flip_rva, [this](cpu_interface& cpu, const uint64_t) {
            auto& c = this->vcpu(cpu.index()).cpu;
            uint32_t flip_data = 0;
            const auto esp = c.reg<uint32_t>(x86_register::esp);
            if (!c.try_read_memory(esp + 4, &flip_data, sizeof(flip_data)) || flip_data == 0)
            {
                return;
            }

            uint32_t surf_targ = 0;
            uint32_t surf_curr = 0;
            c.try_read_memory(flip_data + 0x8, &surf_targ, sizeof(surf_targ));
            c.try_read_memory(flip_data + 0x4, &surf_curr, sizeof(surf_curr));
            if (surf_targ == 0 && surf_curr != 0)
            {
                c.write_memory<uint32_t>(flip_data + 0x8, surf_curr);
            }
        });

        this->d3d9_caps_hooks_[mod.image_base + flip_rva] = hook;
        this->log.info("d3d9.dll fullscreen-flip null-target guard installed at 0x%llx (x86/WoW64)\n",
                       static_cast<unsigned long long>(mod.image_base + flip_rva));
    }

    void windows_emulator::install_ddraw_vidmem_hook(const mapped_module& mod)
    {
        // MW2's legacy DirectDraw-compatibility probe calls IDirectDraw7::GetAvailableVidMem (vtbl
        // index 23, offset +0x5c) right after a successful DirectDrawCreateEx. On a modern WDDM
        // config that method routes through dxgi.dll -> directxdatabasehelper.dll -> dxcore.dll's
        // private adapter-enumeration factory, which -- in this GPU-less emulation environment --
        // returns S_OK with a NULL IDXCoreAdapterList and then hard null-derefs inside
        // directxdatabasehelper.dll (directxdatabasehelper.dll+0x14930, `mov eax,[eax]`). That is a
        // robustness bug in closed-source Microsoft code that cannot be fixed at the source level.
        // We intercept the ddraw.dll method entry and synthesize a *successful* GetAvailableVidMem --
        // reporting a plausible amount of video memory -- so the crashing dxcore-backed body never
        // runs. Success is the most faithful emulation: on real hardware this legacy query succeeds
        // and returns the adapter's video memory, and the guest uses the value only to pick the
        // adapter with the most memory (an unsigned max), so a plausible amount keeps behaviour
        // identical to a real single-GPU machine. This mirrors install_d3d9_caps_patch_hook: a
        // runtime, in-memory behavior patch keyed to a specific, SHA256-pinned Microsoft DLL build,
        // never an on-disk modification. See HANDOFF_MACBOOK.md's §70-75 DirectDraw-probe arc.
        constexpr uint16_t machine_i386 = 0x014c;

        // Only the 32-bit syswow64/ddraw.dll matters (real MW2 is a 32-bit/WoW64 guest) and only that
        // build has been RE'd; a 64-bit ddraw.dll would need its own separately-verified RVA.
        if (mod.machine != machine_i386)
        {
            return;
        }

        // 32-bit syswow64/ddraw.dll (sha256 37113406...0566b0): GetAvailableVidMem entry at RVA
        // 0x10dc0. Two relocation-invariant guard bands re-verify the build/RVA before hooking: the
        // hotpatch+SEH prologue at the entry, and the distinctive 4-argument spill sequence at +0x38.
        constexpr uint64_t entry_rva = 0x10dc0;
        constexpr uint64_t argspill_rva = 0x10df8;
        constexpr std::array<uint8_t, 7> entry_pattern = {0x8B, 0xFF, 0x55, 0x8B, 0xEC, 0x6A, 0xFE};
        constexpr std::array<uint8_t, 18> argspill_pattern = {0x8B, 0x45, 0x08, 0x89, 0x45, 0xB8, 0x8B, 0x75, 0x0C,
                                                              0x8B, 0x7D, 0x10, 0x8B, 0x45, 0x14, 0x89, 0x45, 0xBC};

        std::array<uint8_t, 7> actual_entry{};
        std::array<uint8_t, 18> actual_argspill{};
        if (!this->emu().try_read_memory(mod.image_base + entry_rva, actual_entry.data(), actual_entry.size()) ||
            actual_entry != entry_pattern ||
            !this->emu().try_read_memory(mod.image_base + argspill_rva, actual_argspill.data(), actual_argspill.size()) ||
            actual_argspill != argspill_pattern)
        {
            this->log.warn("ddraw.dll GetAvailableVidMem RVA pattern mismatch at image_base+0x%llx (sha256 "
                           "37113406967162585c9a67c252005757459d8efe87684bbc0ce184907d0566b0 expected) -- "
                           "DirectDraw vidmem-probe crash-guard disabled for this build\n",
                           static_cast<unsigned long long>(entry_rva));
            return;
        }

        // Overwrite the method prologue in the guest's mapped image (an in-memory detour; the on-disk
        // DLL is never touched) with a tiny stub that reports a plausible amount of video memory and
        // returns S_OK, so the crashing dxcore-backed body never runs. On entry the __stdcall frame is
        //   [esp]      return address        [esp+4]    this (LPDIRECTDRAW7)
        //   [esp+8]    lpDDSCaps2            [esp+0xc]  lpdwTotal    [esp+0x10] lpdwFree
        // and the stub is:
        //   mov eax,[esp+0xC]; test eax,eax; jz +6; mov dword[eax],0x20000000   ; *lpdwTotal
        //   mov eax,[esp+0x10];test eax,eax; jz +6; mov dword[eax],0x20000000   ; *lpdwFree
        //   xor eax,eax                                                          ; S_OK
        //   ret 0x10                                                             ; __stdcall cleanup
        // A native `ret 0x10` is used rather than an execution hook that rewrites RIP/RSP, because a
        // single-instruction UC_HOOK_CODE on this backend does not redirect RIP -- only the RSP write
        // would take effect, walking the stack pointer off the top of the thread's stack.
        constexpr std::array<uint8_t, 33> stub = {
            0x8B, 0x44, 0x24, 0x0C,             // mov eax, [esp+0xC]
            0x85, 0xC0,                         // test eax, eax
            0x74, 0x06,                         // jz +6
            0xC7, 0x00, 0x00, 0x00, 0x00, 0x20, // mov dword ptr [eax], 0x20000000
            0x8B, 0x44, 0x24, 0x10,             // mov eax, [esp+0x10]
            0x85, 0xC0,                         // test eax, eax
            0x74, 0x06,                         // jz +6
            0xC7, 0x00, 0x00, 0x00, 0x00, 0x20, // mov dword ptr [eax], 0x20000000
            0x33, 0xC0,                         // xor eax, eax  (S_OK)
            0xC2, 0x10, 0x00,                   // ret 0x10
        };

        if (!this->emu().try_write_memory(mod.image_base + entry_rva, stub.data(), stub.size()))
        {
            this->log.warn("ddraw.dll GetAvailableVidMem crash-guard: failed to patch prologue at 0x%llx\n",
                           static_cast<unsigned long long>(mod.image_base + entry_rva));
            return;
        }

        this->log.info("ddraw.dll GetAvailableVidMem crash-guard installed at 0x%llx (x86/WoW64)\n",
                       static_cast<unsigned long long>(mod.image_base + entry_rva));
    }

    void windows_emulator::setup_hooks()
    {
        this->callbacks.on_module_load.add([this](mapped_module& mod) {
            for (size_t i = 0; i < mod.sections.size(); ++i)
            {
                this->install_section_first_execution_hook(mod, i);
            }

            if (mod.name == "wow64.dll")
            {
                this->mod_manager.wow64_modules_.wow64_dll = &mod;
            }
            else if (mod.name == "wow64win.dll")
            {
                this->mod_manager.wow64_modules_.wow64win_dll = &mod;
            }
            else if (mod.name == "d3d9.dll")
            {
                this->install_d3d9_caps_patch_hook(mod);
            }
            else if (mod.name == "ddraw.dll")
            {
                this->install_ddraw_vidmem_hook(mod);
            }
        });

        this->callbacks.on_module_unload.add([this](mapped_module& mod) {
            const auto hooks = this->section_first_execution_hooks_.extract(mod.image_base);
            if (hooks)
            {
                for (auto* hook : hooks.mapped())
                {
                    if (hook)
                    {
                        this->emu().delete_hook(hook);
                    }
                }
            }

            const auto d3d9_hook = this->d3d9_caps_hooks_.extract(mod.image_base);
            if (d3d9_hook && d3d9_hook.mapped())
            {
                this->emu().delete_hook(d3d9_hook.mapped());
            }
        });

        this->emu().hook_instruction(x86_hookable_instructions::syscall, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            this->dispatcher.dispatch(*this, vcpu);
            return instruction_hook_continuation::skip_instruction;
        });

        this->emu().hook_instruction(x86_hookable_instructions::rdtscp, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto& acting = vcpu.cpu;
            this->callbacks.on_rdtscp();

            const auto ticks = this->clock_->timestamp_counter();
            acting.reg(x86_register::rax, static_cast<uint32_t>(ticks));
            acting.reg(x86_register::rdx, static_cast<uint32_t>(ticks >> 32));

            // Return the IA32_TSC_AUX value in RCX (low 32 bits)
            auto tsc_aux = 0; // Need to replace this with proper CPUID later
            acting.reg(x86_register::rcx, tsc_aux);

            return instruction_hook_continuation::skip_instruction;
        });

        this->emu().hook_instruction(x86_hookable_instructions::rdtsc, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto& acting = vcpu.cpu;
            this->callbacks.on_rdtsc();

            const auto ticks = this->clock_->timestamp_counter();
            acting.reg(x86_register::rax, static_cast<uint32_t>(ticks));
            acting.reg(x86_register::rdx, static_cast<uint32_t>(ticks >> 32));

            return instruction_hook_continuation::skip_instruction;
        });

        // TODO: Unicorn needs this - This should be handled in the backend
        this->emu().hook_instruction(x86_hookable_instructions::invalid, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            // TODO: Unify icicle & unicorn handling
            dispatch_illegal_instruction_violation(*this, this->vcpu(cpu.index()));
            return instruction_hook_continuation::skip_instruction; //
        });

        this->emu().hook_interrupt([&](cpu_interface& cpu, const int interrupt) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto& acting = vcpu.cpu;
            this->callbacks.on_exception();
            const auto eflags = acting.reg<uint32_t>(x86_register::eflags);

            switch (interrupt)
            {
            case 0:
                dispatch_integer_division_by_zero(*this, vcpu);
                return;
            case 1:
                if ((eflags & 0x100) != 0)
                {
                    acting.reg(x86_register::eflags, eflags & ~0x100);
                }

                this->callbacks.on_suspicious_activity("Singlestep");
                dispatch_single_step(*this, vcpu);
                return;
            case 3:
                this->callbacks.on_suspicious_activity("Breakpoint");
                dispatch_breakpoint(*this, vcpu);
                return;
            case 6:
                this->callbacks.on_suspicious_activity("Illegal instruction");
                dispatch_illegal_instruction_violation(*this, vcpu);
                return;
            case 41:
                this->callbacks.on_fast_fail(acting.reg<uint32_t>(x86_register::ecx));
                this->process.exit_status = STATUS_FAIL_FAST_EXCEPTION;
                this->stop();
                return;
            case 45:
                this->callbacks.on_suspicious_activity("DbgPrint");
                {
                    const auto cs_selector = acting.reg<uint16_t>(x86_register::cs);
                    const auto bitness = segment_utils::get_segment_bitness(acting, cs_selector);
                    const auto service = acting.reg<uint32_t>(x86_register::eax);

                    if (bitness && *bitness == segment_utils::segment_bitness::bit64 &&
                        (service == BREAKPOINT_PRINT || service == BREAKPOINT_LOAD_SYMBOLS || service == BREAKPOINT_UNLOAD_SYMBOLS ||
                         service == BREAKPOINT_COMMAND_STRING))
                    {
                        const auto ip = this->uses_instruction_precision() //
                                            ? vcpu.thread().current_ip
                                            : acting.read_instruction_pointer();
                        acting.reg(x86_register::rip, ip + 3);
                    }
                    else
                    {
                        dispatch_breakpoint(*this, vcpu);
                    }
                }
                return;
            default:
                if (this->callbacks.on_generic_activity)
                {
                    this->callbacks.on_generic_activity("Interrupt " + std::to_string(interrupt));
                }

                break;
            }
        });

        this->emu().hook_memory_violation([&](cpu_interface& cpu, const uint64_t address, const size_t size,
                                              const memory_operation operation, const memory_violation_type type) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto& acting = vcpu.cpu;
            if (acting.reg<uint16_t>(x86_register::cs) == 0x33)
            {
                // loading gs selector only works in 64-bit mode
                const auto required_gs_base = vcpu.thread().gs_segment->get_base();
                const auto actual_gs_base = acting.get_segment_base(x86_register::gs);
                if (actual_gs_base != required_gs_base)
                {
                    acting.set_segment_base(x86_register::gs, required_gs_base);
                    return memory_violation_continuation::restart;
                }
            }

            auto region = this->memory.get_region_info(address);
            if (region.permissions.is_guarded())
            {
                // Unset the GUARD_PAGE flag and dispatch a STATUS_GUARD_PAGE_VIOLATION
                this->memory.protect_memory(region.allocation_base, region.length, region.permissions & ~memory_permission_ext::guard);
                dispatch_guard_page_violation(*this, vcpu, address, operation);
            }
            else
            {
                // A fault on a null/near-null address is almost always a call through a null function
                // pointer (e.g. a Vulkan entry point the shim doesn't implement). Log the caller's
                // return address so the missing function's call site can be identified.
                if (address < 0x1000)
                {
                    const auto sp = static_cast<uint32_t>(acting.reg<uint64_t>(x86_register::rsp));
                    uint32_t return_address = 0;
                    if (acting.try_read_memory(sp, &return_address, sizeof(return_address)))
                    {
                        const auto* mod = this->mod_manager.find_by_address(return_address);
                        this->log.error("Null-pointer call to 0x%llx; caller return address 0x%x (%s+0x%llx)\n",
                                        static_cast<unsigned long long>(address), return_address, mod ? mod->name.c_str() : "?",
                                        mod ? static_cast<unsigned long long>(return_address - mod->image_base) : return_address);
                    }

                    if (std::getenv("EMULATOR_NPC_DIAG"))
                    {
                        auto& t = vcpu.thread();
                        fprintf(stderr,
                                "[NPC_DIAG] thread_id=%u vcpu_index=%zu sp=0x%x eax=0x%llx ecx=0x%llx edx=0x%llx ebx=0x%llx "
                                "ebp=0x%llx esi=0x%llx edi=0x%llx\n",
                                t.id, cpu.index(), sp, static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::eax)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::ecx)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::edx)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::ebx)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::ebp)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::esi)),
                                static_cast<unsigned long long>(acting.reg<uint32_t>(x86_register::edi)));
                        for (int off = -16; off <= 32; off += 4)
                        {
                            uint32_t slot = 0;
                            if (acting.try_read_memory(static_cast<uint64_t>(sp + off), &slot, sizeof(slot)))
                            {
                                fprintf(stderr, "[NPC_DIAG]   [sp%+d] = 0x%08x\n", off, slot);
                            }
                        }
                    }
                }

                if (this->callbacks.on_generic_activity)
                {
                    const auto& regions = this->memory.get_reserved_regions();
                    auto next = regions.upper_bound(address);
                    std::string neighborhood{};
                    if (next != regions.begin())
                    {
                        const auto prev = std::prev(next);
                        neighborhood += utils::string::va("prev_region=0x%" PRIx64 "+0x%zx kind=%u", prev->first, prev->second.length,
                                                          static_cast<uint32_t>(prev->second.kind));
                    }
                    if (next != regions.end())
                    {
                        neighborhood += utils::string::va("%snext_region=0x%" PRIx64 "+0x%zx kind=%u", neighborhood.empty() ? "" : " ",
                                                          next->first, next->second.length, static_cast<uint32_t>(next->second.kind));
                    }

                    const auto ebp = acting.reg<uint64_t>(x86_register::rbp);
                    uint32_t stack_args[3]{};
                    acting.try_read_memory(ebp + 8, stack_args, sizeof(stack_args));

                    // For a wild ret/call the frame is already popped, so ebp is useless - the smashed
                    // frame's remnants still sit around esp ([esp-4] = the popped return address,
                    // [esp..] = the previous frame's arguments), which is what identifies the caller.
                    const auto esp = acting.reg<uint64_t>(x86_register::rsp);
                    std::string stack_window{};
                    for (int64_t offset = -16; offset <= 40; offset += 4)
                    {
                        uint32_t slot = 0;
                        if (acting.try_read_memory(esp + offset, &slot, sizeof(slot)))
                        {
                            stack_window += utils::string::va(" %08x", slot);
                        }
                        else
                        {
                            stack_window += " ????????";
                        }
                    }

                    this->callbacks.on_generic_activity(
                        utils::string::va("Memory violation context: addr=0x%" PRIx64 " %s tid=%u vcpu=%zu"
                                          " eax=0x%x ecx=0x%x edx=0x%x ebx=0x%x esi=0x%x edi=0x%x"
                                          " ebp=0x%" PRIx64 " [ebp+8]=0x%x [ebp+c]=0x%x [ebp+10]=0x%x"
                                          " esp=0x%" PRIx64 " stack[esp-16..esp+40]=%s",
                                          address, neighborhood.c_str(), vcpu.thread().id, cpu.index(),
                                          acting.reg<uint32_t>(x86_register::eax), acting.reg<uint32_t>(x86_register::ecx),
                                          acting.reg<uint32_t>(x86_register::edx), acting.reg<uint32_t>(x86_register::ebx),
                                          acting.reg<uint32_t>(x86_register::esi), acting.reg<uint32_t>(x86_register::edi), ebp,
                                          stack_args[0], stack_args[1], stack_args[2], esp, stack_window.c_str()));
                }

                this->callbacks.on_memory_violate(address, size, operation, type);
                dispatch_access_violation(*this, vcpu, address, operation);
            }

            return memory_violation_continuation::resume;
        });

        if (this->uses_instruction_precision())
        {
            this->emu().hook_memory_execution([&](cpu_interface& cpu, const uint64_t address) {
                const std::scoped_lock lock(this->kernel_lock_);
                auto& vcpu = this->vcpu(cpu.index());
                const scoped_dispatch dispatch(*this, vcpu);
                this->on_instruction_execution(vcpu, address); //
            });
        }
        else if (!this->emu().is_stop_thread_safe())
        {
            // The backend cannot be stopped safely from another thread, so the interrupt thread in start()
            // is not available for time-slicing. Preempt cooperatively from the CPU thread via a basic-block
            // hook instead.
            this->emu().hook_basic_block([&](cpu_interface& cpu, const basic_block& block) {
                const std::scoped_lock lock(this->kernel_lock_);
                this->on_basic_block_execution(this->vcpu(cpu.index()), block); //
            });
        }
    }

    void windows_emulator::on_basic_block_execution(vcpu_context& vcpu, const basic_block&)
    {
        auto& thread = vcpu.thread();

        // This path deliberately trades instruction precision for speed (one callback per block instead of
        // one per instruction), so we cannot account for individual instructions. Time-slice on a fixed number
        // of executed blocks instead.
        ++this->executed_instructions_;

        if (++thread.executed_blocks % MAX_BASIC_BLOCKS_PER_TIME_SLICE == 0)
        {
            this->yield_thread(vcpu);
        }
    }

    void windows_emulator::start(size_t count)
    {
        this->should_stop = false;
        this->last_stop_reason_ = stop_reason::none;
        this->last_stop_detail_.clear();
        this->setup_process_if_necessary();

        if (count > 0 && this->vcpu_count_ > 1)
        {
            throw std::invalid_argument("Instruction-count budgets require a single vCPU");
        }

        const auto use_count = count > 0;
        const auto start_instructions = this->executed_instructions_;
        const auto target_instructions = start_instructions + count;

        std::mutex interrupt_mutex{};
        std::condition_variable interrupt_cond{};
        std::thread interrupt_thread{};
        std::vector<pthread_t> workers{};
        std::atomic<uint32_t> active_workers{0};

        const auto _ = utils::finally([&] {
            {
                std::unique_lock lock{interrupt_mutex};
                this->should_stop = true;
            }

            interrupt_cond.notify_all();

            for (uint32_t i = 0; i < this->vcpu_count_; ++i)
            {
                this->vcpu(i).cpu.stop();
            }

            for (auto& worker : workers)
            {
                pthread_join(worker, nullptr);
            }

            if (interrupt_thread.joinable())
            {
                interrupt_thread.join();
            }
        });

        if (!this->uses_instruction_precision() && this->emu().is_stop_thread_safe())
        {
            interrupt_thread = std::thread([&] {
                while (!this->should_stop)
                {
                    std::unique_lock lock{interrupt_mutex};
                    interrupt_cond.wait_for(lock, std::chrono::milliseconds(20), [&] {
                        return this->should_stop.load(); //
                    });

                    if (!this->should_stop)
                    {
                        // Under the kernel lock so this switch_thread/stop() pair can't straddle a vCPU's
                        // own scheduling step: perform_thread_switch consumes switch_thread (exchange to
                        // false) under the lock, and a preemption whose switch request lands before that
                        // consume while its stop() only lands inside the next quantum surfaces there as a
                        // stop with no pending switch - the exact shape of a fatal wind-down, tearing the
                        // whole run off at a random parked rip. Serialized against the scheduler, the pair
                        // lands either fully before the consume (plain early switch) or fully inside the
                        // running quantum (ordinary preemption), never split across it.
                        const std::scoped_lock kernel_lock(this->kernel_lock_);
                        for (uint32_t i = 0; i < this->vcpu_count_; ++i)
                        {
                            auto& v = this->vcpu(i);
                            v.switch_thread = true;
                            v.cpu.stop();
                        }
                    }
                }
            });
        }

        if (this->vcpu_count_ > 1)
        {
            // Establishes this thread as the UI backend's owning thread (e.g. runs SDL/Cocoa's one-time,
            // main-thread-only init) before any vCPU worker starts. Without this, a worker whose guest
            // syscall creates a window first would race the main thread for that one-time init and could
            // run it on a non-main thread, which is undefined behavior for Cocoa/AppKit.
            this->ui_backend_->pump_events();

            // One worker thread per vCPU; this thread pumps UI events until the run ends.
            active_workers = this->vcpu_count_;
            workers.reserve(this->vcpu_count_);

            for (uint32_t i = 0; i < this->vcpu_count_; ++i)
            {
                auto* args = new vcpu_worker_thread_args{this, i, &active_workers};

                pthread_attr_t attr{};
                pthread_attr_init(&attr);

                // See reserve_worker_thread_stack's doc comment (arch_emulator.hpp): backends whose
                // guest memory shares the host's own address space (FEX) need their worker threads'
                // stacks to come from a pre-reserved arena instead of the OS's default placement, or
                // a new thread's stack can land on an address the guest program is about to use.
                void* stack_base = nullptr;
                size_t stack_size = 0;
                if (this->emu().reserve_worker_thread_stack(i, stack_base, stack_size))
                {
                    pthread_attr_setstack(&attr, stack_base, stack_size);
                }

                pthread_t handle{};
                const auto rc = pthread_create(&handle, &attr, &windows_emulator::vcpu_worker_thread_trampoline, args);
                pthread_attr_destroy(&attr);

                if (rc != 0)
                {
                    delete args;
                    throw std::runtime_error("Failed to create vCPU worker thread");
                }

                workers.push_back(handle);
            }

            while (active_workers.load() > 0)
            {
                this->ui_backend_->pump_events();
                this->callbacks.on_event_pump();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            this->dump_exception_trace();
            this->dump_lock_profile();
            return;
        }

        auto& vcpu = this->vcpu(0);

        std::unique_lock lock(this->kernel_lock_);

        while (!this->should_stop)
        {
            lock.unlock();
            this->ui_backend_->pump_events();
            this->callbacks.on_event_pump();
            lock.lock();

            if (vcpu.switch_thread || !vcpu.thread().is_thread_ready(*this))
            {
                if (!this->perform_thread_switch(vcpu, lock))
                {
                    break;
                }
            }

            // Guest code executes with the kernel lock released; hook callbacks
            // (syscalls, exceptions, exec hooks) re-acquire it on VM exit.
            lock.unlock();
            vcpu.cpu.start(count);
            lock.lock();

            if (!vcpu.switch_thread && !vcpu.cpu.has_violation())
            {
                break;
            }

            if (use_count)
            {
                const auto current_instructions = this->executed_instructions_;

                if (current_instructions >= target_instructions)
                {
                    break;
                }

                count = static_cast<size_t>(target_instructions - current_instructions);
            }
        }

        this->dump_exception_trace();
        this->dump_lock_profile();
    }

    void windows_emulator::deliver_raw_input(const process_context::raw_input_payload& payload, const hwnd explicit_target)
    {
        // Resolve the destination at delivery time: an explicit hwndTarget, else the foreground window
        // (raw input registered with a NULL target follows keyboard focus). If neither resolves to a live
        // window right now there is nowhere to deliver to, so skip without dropping the registration.
        const auto target = explicit_target != 0 ? explicit_target : this->process.foreground_window;
        auto* raw_win = this->process.windows.get(target);
        if (!raw_win)
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, raw_win->thread_id);
        if (!thread)
        {
            return;
        }

        const auto token = this->process.next_raw_input_token++;
        this->process.raw_inputs[token] = payload;

        // Bound the pending set: WM_INPUT is normally consumed at once by GetRawInputData, but if the guest
        // stops pumping, drop the oldest tokens so the map can't grow without limit.
        constexpr size_t max_pending_raw_inputs = 256;
        while (this->process.raw_inputs.size() > max_pending_raw_inputs)
        {
            this->process.raw_inputs.erase(this->process.raw_inputs.begin());
        }

        msg m{};
        m.window = target;
        m.message = WM_INPUT;
        m.wParam = RIM_INPUT;
        m.lParam = token;
        thread->post_message(*this, m);
    }

    void windows_emulator::deliver_raw_mouse_input(const int32_t dx, const int32_t dy, const uint16_t button_flags,
                                                   const uint16_t button_data)
    {
        this->deliver_raw_input({.keyboard = false, .dx = dx, .dy = dy, .mouse_buttons = button_flags, .mouse_button_data = button_data},
                                this->process.raw_mouse_target);
    }

    void windows_emulator::deliver_raw_keyboard_input(const uint16_t vkey, const uint16_t scan_code, const uint32_t message,
                                                      const bool extended)
    {
        this->deliver_raw_input({.keyboard = true, .vkey = vkey, .scan_code = scan_code, .key_message = message, .key_extended = extended},
                                this->process.raw_keyboard_target);
    }

    void windows_emulator::handle_ui_event(const ui_event& event)
    {
        const std::scoped_lock lock(this->kernel_lock_);

        const auto* win = this->process.windows.get(event.window);
        if (!win)
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, win->thread_id);
        if (!thread)
        {
            return;
        }

        msg m{};
        m.window = event.window;
        m.message = event.message;
        m.wParam = event.wParam;
        m.lParam = event.lParam;

        if (is_pointer_message(event.message))
        {
            // Track the cursor in screen coordinates (top-level window origin + window-local position) so
            // GetCursorPos reflects it, and treat the window the user is pointing at as the foreground one.
            int32_t new_cursor_x = this->process.cursor_x;
            int32_t new_cursor_y = this->process.cursor_y;
            if (const auto origin = get_window_origin_relative_to_ancestor(this->process, event.window, 0))
            {
                new_cursor_x = origin->x + point_x(event.lParam);
                new_cursor_y = origin->y + point_y(event.lParam);
            }

            // A game using raw input for mouse-look reads relative deltas from WM_INPUT, not WM_MOUSEMOVE.
            // Synthesize that delta from the change in tracked cursor position before updating it. A
            // SetCursorPos recenter pre-updates cursor_x/y, so the warp-echo motion event yields a zero
            // delta -- no spurious look -- while genuine motion between recenters produces the real delta.
            if (this->process.raw_mouse_registered)
            {
                if (event.message == WM_MOUSEMOVE)
                {
                    const int32_t dx = new_cursor_x - this->process.cursor_x;
                    const int32_t dy = new_cursor_y - this->process.cursor_y;
                    if (dx != 0 || dy != 0)
                    {
                        this->deliver_raw_mouse_input(dx, dy, 0);
                    }
                }
                else if (const uint16_t buttons = raw_mouse_button_flags(event.message, event.wParam); buttons != 0)
                {
                    // Raw-input games read button transitions and wheel deltas from WM_INPUT, not only
                    // the corresponding window messages. For wheel messages the delta lives in usButtonData.
                    this->deliver_raw_mouse_input(0, 0, buttons, raw_mouse_button_data(event.message, event.wParam));
                }
            }

            this->process.cursor_x = new_cursor_x;
            this->process.cursor_y = new_cursor_y;
            this->process.foreground_window = event.window;

            const auto target = route_pointer(this->process, event.window, point_x(event.lParam), point_y(event.lParam));
            m.window = target.window;
            // WM_MOUSEWHEEL/WM_MOUSEHWHEEL carry screen coordinates in lParam, unlike button/move
            // messages which carry client coordinates. Keep the routed target but preserve screen coords.
            m.lParam = is_mouse_wheel_message(event.message) ? pack_point(new_cursor_x, new_cursor_y) : pack_point(target.x, target.y);
        }
        else if ((event.message == WM_ACTIVATE && event.wParam != 0) || event.message == WM_SETFOCUS)
        {
            // The window just became active/focused: make it the foreground window so polling APIs
            // (GetForegroundWindow/GetActiveWindow) agree with the activation the game just received.
            this->process.foreground_window = event.window;
        }
        else if ((event.message == WM_ACTIVATE && event.wParam == 0) && this->process.foreground_window == event.window)
        {
            this->process.foreground_window = 0;
        }

        // Mirror the foreground window into the shared SERVERINFO so the guest's client-side
        // GetForegroundWindow (which reads gpsi directly, never syscalling) returns the active window.
        // Fall back to the desktop window when no app window is active: real Windows always has a
        // foreground window, and code that needs a valid HWND (e.g. DirectSound's SetCooperativeLevel,
        // which Miles feeds from GetForegroundWindow) breaks on a null one.
        const auto foreground =
            this->process.foreground_window != 0 ? this->process.foreground_window : this->process.default_desktop_window_handle.bits;
        this->process.user_handles.get_server_info().access([&](USER_SERVERINFO& server_info) {
            server_info.foregroundWindow = foreground; //
        });

        // Maintain the polled key state from key and mouse-button transitions. GetKeyState reports the high
        // down bit; GetAsyncKeyState also reports a low edge bit that is set once when a key transitions from
        // up to down and cleared by the next GetAsyncKeyState query for that virtual key.
        switch (event.message)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const auto virtual_key = static_cast<uint8_t>(event.wParam & 0xFF);
            if ((this->process.key_state[virtual_key] & 0x80) == 0)
            {
                this->process.async_key_state[virtual_key] = 1;
            }
            this->process.key_state[virtual_key] = 0x80;
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            this->process.key_state[static_cast<uint8_t>(event.wParam & 0xFF)] = 0;
            break;
        default:
            if (const auto virtual_key = mouse_button_virtual_key(event.message, event.wParam); virtual_key != 0)
            {
                if (is_mouse_button_down_message(event.message))
                {
                    if ((this->process.key_state[virtual_key] & 0x80) == 0)
                    {
                        this->process.async_key_state[virtual_key] = 1;
                    }
                    this->process.key_state[virtual_key] = 0x80;
                }
                else if (is_mouse_button_up_message(event.message))
                {
                    this->process.key_state[virtual_key] = 0;
                }
            }
            break;
        }

        // Raw-input games (e.g. Skyrim) read the keyboard via WM_INPUT/GetRawInputData, not WM_KEYDOWN.
        if (this->process.raw_keyboard_registered && is_keyboard_message(event.message))
        {
            const auto vk = static_cast<uint16_t>(event.wParam & 0xFFFF);
            auto scan_code = static_cast<uint16_t>((event.lParam >> 16) & 0xFF);
            if (scan_code == 0)
            {
                scan_code = vk_to_scan_code(vk);
            }

            const bool extended = (event.lParam & (1ull << 24)) != 0;
            this->deliver_raw_keyboard_input(vk, scan_code, event.message, extended);
        }

        thread->post_message(*this, m, true);

        // The 32-bit ButtonWndProc tracks BST_PUSHED via direct memory access into tagWND at
        // 32-bit offsets that don't match our 64-bit USER_WINDOW layout, so it never reads the
        // pushed state back and therefore never calls ReleaseCapture or posts WM_COMMAND(BN_CLICKED).
        // Synthesize both here when WM_LBUTTONUP arrives for a captured Button-class window.
        if (m.message == WM_LBUTTONUP && this->process.mouse_capture_window == m.window)
        {
            const auto* btn_win = this->process.windows.get(m.window);
            const auto& cn = btn_win ? btn_win->class_name : std::u16string{};
            if (cn == u"Button" || cn == u"BUTTON" || cn == u"#1")
            {
                uint64_t button_id = 0;
                btn_win->guest.access([&](const USER_WINDOW& gw) { button_id = gw.wID; });
                const auto parent_hwnd = btn_win->parent_handle;

                if (const auto* parent_win = this->process.windows.get(parent_hwnd))
                {
                    if (auto* parent_thread = get_thread_by_id(this->process, parent_win->thread_id))
                    {
                        msg cmd{};
                        cmd.window = parent_hwnd;
                        cmd.message = WM_COMMAND;
                        cmd.wParam = button_id & 0xFFFF; // BN_CLICKED=0 in high word
                        cmd.lParam = static_cast<uint32_t>(m.window);
                        parent_thread->post_message(*this, cmd, true);
                    }
                }

                this->process.mouse_capture_window = 0;
            }
        }

        if (event.message == WM_CLOSE || event.message == WM_COMMAND || is_key_down_message(event.message) ||
            is_mouse_button_message(event.message) || is_mouse_wheel_message(event.message))
        {
            // Kick the vCPU currently running the target thread so it promptly re-checks its message
            // queue; if the thread is not running on any vCPU right now, fall back to vCPU 0 to force a
            // reschedule that can pick up the now-ready thread.
            auto* running_on = find_vcpu_running_thread(*this, *thread);
            auto& vcpu = running_on ? *running_on : this->vcpu(0);
            vcpu.switch_thread = true;
            vcpu.cpu.stop();
        }
    }

    void windows_emulator::dump_exception_trace()
    {
        // Opt-in post-mortem aid for multi-vCPU debugging (see docs/multi-vcpu-design.md).
        const auto* enabled = std::getenv("SOGEN_TRACE_EXCEPTIONS");
        if (!enabled || enabled[0] != '1')
        {
            return;
        }

        const auto count = std::min(this->exception_trace_index_, this->exception_trace_.size());
        if (count == 0)
        {
            return;
        }

        const auto total = this->exception_trace_index_;
        const auto start = total - count;
        this->log.error("--- exception trace (last %zu of %zu) ---\n", count, total);

        const auto describe = [this](const uint64_t address) -> std::string {
            const auto* mod = this->mod_manager.find_by_address(address);
            const auto region = this->memory.get_region_info(address);
            std::array<char, 256> buffer{};
            std::snprintf(buffer.data(), buffer.size(), "%s+0x%llx [%s]", mod ? mod->name.c_str() : "?",
                          mod ? static_cast<unsigned long long>(address - mod->image_base) : address,
                          region.is_committed ? "committed" : "FREE/reserved");
            return buffer.data();
        };

        for (size_t i = 0; i < count; ++i)
        {
            const auto& e = this->exception_trace_[(start + i) % this->exception_trace_.size()];
            this->log.error("  [%zu] status 0x%08x vcpu %u tid %u\n      rip  0x%llx %s\n      addr 0x%llx %s\n", start + i, e.status,
                            e.vcpu, e.tid, static_cast<unsigned long long>(e.rip), describe(e.rip).c_str(),
                            static_cast<unsigned long long>(e.info), describe(e.info).c_str());
        }
    }

    bool windows_emulator::try_signal_guest_event(const handle event_handle)
    {
        if (!this->kernel_lock_.try_lock())
        {
            return false;
        }

        const std::lock_guard<kernel_lock> lock{this->kernel_lock_, std::adopt_lock};

        auto* entry = this->process.events.get(event_handle);
        if (!entry)
        {
            return false;
        }

        entry->signaled = true;
        return true;
    }

    void windows_emulator::dump_lock_profile()
    {
        if (!kernel_lock::profiling_enabled())
        {
            return;
        }

        const auto stats = this->kernel_lock_.profile();
        const auto held_ms = static_cast<double>(stats.held_nanos) / 1e6;
        const auto wait_ms = static_cast<double>(stats.wait_nanos) / 1e6;
        const auto contended_pct =
            stats.acquisitions ? (100.0 * static_cast<double>(stats.contended) / static_cast<double>(stats.acquisitions)) : 0.0;

        this->log.print(color::cyan,
                        "--- kernel lock (BEL) profile ---\n"
                        "  acquisitions:   %llu\n"
                        "  contended:      %llu (%.1f%%)\n"
                        "  wait time:      %.1f ms (blocked on a busy BEL)\n"
                        "  held time:      %.1f ms (BEL busy across all threads)\n",
                        static_cast<unsigned long long>(stats.acquisitions), static_cast<unsigned long long>(stats.contended),
                        contended_pct, wait_ms, held_ms);
    }

    void windows_emulator::stop()
    {
        this->should_stop = true;

        for (uint32_t i = 0; i < this->vcpu_count_; ++i)
        {
            this->vcpu(i).cpu.stop();
        }
    }

    void windows_emulator::register_factories(utils::buffer_deserializer& buffer)
    {
        buffer.register_factory<memory_manager_wrapper>([this] {
            return memory_manager_wrapper{this->memory}; //
        });

        buffer.register_factory<module_manager_wrapper>([this] {
            return module_manager_wrapper{this->mod_manager}; //
        });

        buffer.register_factory<x64_emulator_wrapper>([this] {
            return x64_emulator_wrapper{this->emu()}; //
        });

        buffer.register_factory<windows_emulator_wrapper>([this] {
            return windows_emulator_wrapper{*this}; //
        });

        buffer.register_factory<clock_wrapper>([this] {
            return clock_wrapper{this->clock()}; //
        });

        buffer.register_factory<socket_factory_wrapper>([this] {
            return socket_factory_wrapper{this->socket_factory()}; //
        });

        buffer.register_factory<window>([this] {
            return window{this->emu()}; //
        });
    }

    void windows_emulator::serialize(utils::buffer_serializer& buffer) const
    {
        buffer.write(this->application_settings_);
        buffer.write(this->setup_completed_);
        buffer.write(this->executed_instructions_);
        buffer.write_atomic(this->vcpus_[0]->switch_thread);
        buffer.write(this->use_relative_time_);

        this->version.serialize(buffer);
        this->registry.serialize_runtime_state(buffer);

        // Backend snapshot mode is not used here; Unicorn's in-place snapshot path is broken.
        this->emu().serialize_state(buffer, false);
        this->memory.serialize_memory_state(buffer, false);
        this->mod_manager.serialize(buffer);
        this->dispatcher.serialize(buffer);
        this->process.serialize(buffer, this->vcpus_[0]->active_thread);
    }

    void windows_emulator::deserialize(utils::buffer_deserializer& buffer)
    {
        this->register_factories(buffer);

        buffer.read(this->application_settings_);
        buffer.read(this->setup_completed_);
        buffer.read(this->executed_instructions_);
        buffer.read_atomic(this->vcpus_[0]->switch_thread);

        const auto old_relative_time = this->use_relative_time_;
        buffer.read(this->use_relative_time_);

        if (old_relative_time != this->use_relative_time_)
        {
            throw std::runtime_error("Can not deserialize emulator with different time dimensions");
        }

        this->version.deserialize(buffer);
        this->registry.deserialize_runtime_state(buffer);

        this->memory.unmap_all_memory();
        this->clear_section_first_execution_hooks();

        // Match raw serialize() above; do not use backend snapshot mode here.
        this->emu().deserialize_state(buffer, false);
        this->memory.deserialize_memory_state(buffer, false);
        this->mod_manager.deserialize(buffer);
        this->install_section_first_execution_hooks();
        this->dispatcher.deserialize(buffer);
        this->process.deserialize(buffer, this->vcpus_[0]->active_thread);
        this->restore_ui_backend();
    }

    void windows_emulator::restore_ui_backend()
    {
        this->ui().reset();
        this->audio().stop();

        std::vector<const window*> pending{};
        pending.reserve(this->process.windows.size());
        for (const auto& [index, win] : this->process.windows)
        {
            (void)index;
            if (win.host_surface_window)
            {
                pending.push_back(&win);
            }
        }

        std::unordered_set<hwnd> created{};
        const auto dependency_ready = [&](const hwnd handle) {
            if (handle == 0)
            {
                return true;
            }

            const auto* dependency = this->process.windows.get(handle);
            return !dependency || !dependency->host_surface_window || created.contains(handle);
        };

        const auto create_window = [&](const window& win) {
            const auto child = (win.style & WS_CHILD) != 0;
            uint32_t control_id = 0;
            if (child)
            {
                if (const auto guest_window = win.guest.try_read())
                {
                    control_id = static_cast<uint32_t>(guest_window->wID);
                }
            }

            this->ui().create_window(ui_window_desc{
                .handle = win.handle,
                .parent = child ? win.parent_handle : 0,
                .owner = child ? 0 : win.owner_handle,
                .rect = {.left = win.x, .top = win.y, .right = win.x + win.width, .bottom = win.y + win.height},
                .client_insets = {},
                .class_name = std::u16string{normalize_builtin_window_class_name(win.class_name)},
                .title = win.name,
                .style = win.style,
                .ex_style = win.ex_style,
                .control_id = control_id,
                .visible = (win.style & WS_VISIBLE) != 0,
                .enabled = (win.style & WS_DISABLED) == 0,
                .top_level = !child,
            });
            created.insert(win.handle);
        };

        while (!pending.empty())
        {
            bool made_progress = false;
            for (auto it = pending.begin(); it != pending.end();)
            {
                const auto& win = **it;
                if (dependency_ready(win.parent_handle) && dependency_ready(win.owner_handle))
                {
                    create_window(win);
                    it = pending.erase(it);
                    made_progress = true;
                }
                else
                {
                    ++it;
                }
            }

            if (!made_progress)
            {
                create_window(*pending.front());
                pending.erase(pending.begin());
            }
        }

        for (const auto& [index, win] : this->process.windows)
        {
            (void)index;
            if (!win.host_surface_window || (win.style & WS_CHILD) != 0)
            {
                continue;
            }

            const auto surface = this->process.gdi_window_surfaces.find(static_cast<uint32_t>(win.handle));
            if (surface == this->process.gdi_window_surfaces.end() || surface->second.width == 0 || surface->second.height == 0 ||
                surface->second.pixels.empty())
            {
                continue;
            }

            const auto& restored = surface->second;
            this->ui().present_surface(win.handle, ui_surface_desc{
                                                       .width = static_cast<int>(restored.width),
                                                       .height = static_cast<int>(restored.height),
                                                       .stride = static_cast<int>(restored.width * sizeof(uint32_t)),
                                                       .format = ui_surface_format::bgra8,
                                                       .pixels = restored.pixels.data(),
                                                   });
        }
    }

    void windows_emulator::save_snapshot()
    {
        utils::buffer_serializer buffer{};

        buffer.write(this->setup_completed_);
        buffer.write(this->executed_instructions_);
        buffer.write_atomic(this->vcpus_[0]->switch_thread);

        this->version.serialize(buffer);
        this->registry.serialize_runtime_state(buffer);

        // Snapshot path still uses regular backend state serialization.
        // Backend snapshot mode (is_snapshot=true) is not reliable yet.
        this->emu().serialize_state(buffer, false);
        this->memory.serialize_memory_state(buffer, false);
        this->mod_manager.serialize(buffer);
        this->dispatcher.serialize(buffer);
        this->process.serialize(buffer, this->vcpus_[0]->active_thread);

        this->process_snapshot_ = buffer.move_buffer();
    }

    void windows_emulator::restore_snapshot()
    {
        if (this->process_snapshot_.empty())
        {
            throw std::runtime_error("No snapshot saved");
        }

        utils::buffer_deserializer buffer{this->process_snapshot_};

        this->register_factories(buffer);

        buffer.read(this->setup_completed_);
        buffer.read(this->executed_instructions_);
        buffer.read_atomic(this->vcpus_[0]->switch_thread);

        this->version.deserialize(buffer);
        this->registry.deserialize_runtime_state(buffer);

        this->memory.unmap_all_memory();
        this->clear_section_first_execution_hooks();

        this->emu().deserialize_state(buffer, false);
        this->memory.deserialize_memory_state(buffer, false);
        this->mod_manager.deserialize(buffer);
        this->install_section_first_execution_hooks();
        this->dispatcher.deserialize(buffer);
        this->process.deserialize(buffer, this->vcpus_[0]->active_thread);
        this->restore_ui_backend();
    }

} // namespace sogen
