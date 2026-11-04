#include "std_include.hpp"
#include "windows_emulator.hpp"
#include "keyboard_layout.hpp"

#include "cpu_context.hpp"

#include <utils/io.hpp>
#include <utils/timer.hpp>
#include <utils/finally.hpp>
#include <utils/lazy_object.hpp>
#include <utils/string.hpp>

#include "exception_dispatch.hpp"
#include "apiset/apiset.hpp"
#include "syscall_dispatcher.hpp"
#include "guest_function_call.hpp"

#include "memory_permission_ext.hpp"
#include "devices/named_pipe.hpp"
#include "process_control_server.hpp"
#include "address_utils.hpp"
#include "worker_factory_support.hpp"
#include "window_control.hpp"

namespace sogen
{
    constexpr auto MAX_INSTRUCTIONS_PER_TIME_SLICE = 0x20000;
    constexpr auto MAX_BASIC_BLOCKS_PER_TIME_SLICE = 0x8000;
    constexpr uint32_t MAX_PENDING_GUEST_CALL_DEFERRED_PREEMPTIONS = 100;

    namespace
    {
        // TEMPDIAG: remove before finalizing. Dumps every thread's wait-state fields once the
        // idle loop has been spinning for a while with nothing ready, so a captured freeze
        // explains itself in the log instead of requiring live debugger inspection.
        const char* describe_handle_type_diag(const handle_types::type type)
        {
            switch (type)
            {
            case handle_types::event:
                return "event";
            case handle_types::mutant:
                return "mutant";
            case handle_types::semaphore:
                return "semaphore";
            case handle_types::process:
                return "process";
            case handle_types::thread:
                return "thread";
            case handle_types::timer:
                return "timer";
            case handle_types::io_completion:
                return "io_completion";
            case handle_types::wait_completion_packet:
                return "wait_completion_packet";
            case handle_types::file:
                return "file";
            case handle_types::device:
                return "device";
            default:
                return "other";
            }
        }

        void dump_await_object_diag(process_context& process, const handle obj)
        {
            const auto type = static_cast<handle_types::type>(obj.value.type);
            fprintf(stderr, "[SCHED_DIAG]     await_object handle=0x%llx type=%s", static_cast<unsigned long long>(obj.bits),
                    describe_handle_type_diag(type));

            if (type == handle_types::event)
            {
                if (const auto* e = process.events.get(obj))
                {
                    fprintf(stderr, " signaled=%d event_type=%d name='%s'", e->is_signaled() ? 1 : 0, static_cast<int>(e->type),
                            u16_to_u8(e->name).c_str());
                }
                else
                {
                    fprintf(stderr, " <not found>");
                }
            }
            else if (type == handle_types::mutant)
            {
                if (const auto* m = process.mutants.get(obj))
                {
                    fprintf(stderr, " locked_count=%u owner=%llx abandoned=%d name='%s'", m->locked_count(),
                            static_cast<unsigned long long>(m->owner()), m->abandoned() ? 1 : 0, u16_to_u8(m->name).c_str());
                }
                else
                {
                    fprintf(stderr, " <not found>");
                }
            }
            else if (type == handle_types::semaphore)
            {
                if (const auto* s = process.semaphores.get(obj))
                {
                    fprintf(stderr, " current_count=%u max_count=%u name='%s'", s->current_count(), s->max_count(),
                            u16_to_u8(s->name).c_str());
                }
                else
                {
                    fprintf(stderr, " <not found>");
                }
            }
            else if (type == handle_types::process)
            {
                fprintf(stderr, " <process wait>");
            }
            else if (type == handle_types::thread)
            {
                fprintf(stderr, " <thread wait>");
            }

            fprintf(stderr, "\n");
        }

        void dump_thread_wait_states_diag(process_context& process, const vcpu_context& vcpu)
        {
            fprintf(stderr, "[SCHED_DIAG] pid=%d guest_pid=%u active_thread=%p id=%u\n", ::getpid(), process.process_id,
                    static_cast<void*>(vcpu.active_thread), vcpu.active_thread ? vcpu.active_thread->id : 0);
            if (vcpu.active_thread)
            {
                const auto& pending_call = vcpu.active_thread->pending_guest_call;
                fprintf(stderr, "[SCHED_DIAG] active_thread pending_guest_call=%d deferred_preemptions=%u sentinel=0x%llx\n",
                        pending_call.has_value() ? 1 : 0, pending_call.has_value() ? pending_call->deferred_preemptions : 0,
                        pending_call.has_value() ? static_cast<unsigned long long>(pending_call->sentinel_address) : 0ULL);
            }
            for (auto& [h, thread] : process.threads)
            {
                fprintf(stderr,
                        "[SCHED_DIAG] thread id=%u terminated=%d suspended=%u waiting_for_alert=%d alerted=%d "
                        "await_objects=%zu await_msg_mask=%d await_time=%d await_host_condition=%d await_io_completion=%d\n",
                        thread.id, thread.is_terminated() ? 1 : 0, thread.suspended, thread.waiting_for_alert ? 1 : 0,
                        thread.alerted ? 1 : 0, thread.await_objects.size(), thread.await_msg_mask.has_value() ? 1 : 0,
                        thread.await_time.has_value() ? 1 : 0, thread.await_host_condition ? 1 : 0,
                        thread.await_io_completion.has_value() ? 1 : 0);

                for (const auto& obj : thread.await_objects)
                {
                    dump_await_object_diag(process, obj);
                }

                if (thread.await_io_completion.has_value())
                {
                    const auto& wait = *thread.await_io_completion;
                    const auto* ioc = process.io_completions.get(wait.io_completion_handle);
                    fprintf(stderr,
                            "[SCHED_DIAG]     await_io_completion handle=0x%llx type=%s queue_depth=%zu "
                            "max_entries=%u\n",
                            static_cast<unsigned long long>(wait.io_completion_handle.bits),
                            ioc ? "io_completion" : "io_completion<not found>", ioc ? ioc->queue.size() : 0u, wait.max_entries);
                }
            }
        }

        void publish_kernel_event(process_context& process, const std::u16string_view name)
        {
            event object{};
            object.type = NotificationEvent;
            object.name = std::u16string(name);
            object.state.word(1).store(static_cast<uint64_t>(NotificationEvent));

            const auto [handle, stored] = process.events.store_and_get(std::move(object));
            (void)named_objects::publish(process, named_objects::kind::event, stored->name, stored->state);
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

        std::optional<POINT> get_client_origin_relative_to_ancestor(const process_context& process, const hwnd window, const hwnd ancestor)
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

                origin.x += current->client_x();
                origin.y += current->client_y();
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

        constexpr int k_wh_keyboard_ll = 13;
        constexpr int k_wh_mouse_ll = 14;

        bool is_character_message(const uint32_t message)
        {
            return message == WM_CHAR || message == WM_SYSCHAR || message == WM_DEADCHAR || message == WM_SYSDEADCHAR;
        }

        int low_level_hook_id(const uint32_t message)
        {
            switch (message)
            {
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return k_wh_keyboard_ll;
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return k_wh_mouse_ll;
            default:
                return 0;
            }
        }

        bool is_input_message(const uint32_t message)
        {
            return low_level_hook_id(message) != 0 || is_character_message(message) || is_pointer_message(message);
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
            case VK_MENU:
                return 0x38;
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
                    const auto captured_origin = get_client_origin_relative_to_ancestor(process, captured->handle, 0);
                    const auto top_level_origin = get_client_origin_relative_to_ancestor(process, top_level, 0);
                    if (captured_origin && top_level_origin)
                    {
                        const auto screen_x = top_level_origin->x + x;
                        const auto screen_y = top_level_origin->y + y;
                        return {.window = captured->handle, .x = screen_x - captured_origin->x, .y = screen_y - captured_origin->y};
                    }
                }
                else
                {
                    process.set_mouse_capture(0);
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

        void dump_threads_periodically(windows_emulator& win_emu)
        {
            static const auto interval_seconds = [] {
                const auto* value = std::getenv("SOGEN_DUMP_THREADS_EVERY_SEC");
                return value ? std::strtoul(value, nullptr, 10) : 0UL;
            }();

            if (interval_seconds == 0)
            {
                return;
            }

            static auto next_dump = std::chrono::steady_clock::now() + std::chrono::seconds(interval_seconds);
            const auto now = std::chrono::steady_clock::now();
            if (now < next_dump)
            {
                return;
            }

            next_dump = now + std::chrono::seconds(interval_seconds);

            for (const auto& thread : win_emu.process.threads | std::views::values)
            {
                const auto* mod = win_emu.mod_manager.find_by_address(thread.current_ip);
                win_emu.log.error(
                    "[thread-dump] pid=%u tid=%u ip=%s+0x%llx waiting_for_alert=%d await_objects=%zu await_time=%d await_msg=%d "
                    "await_io_completion=%d suspended=%u terminated=%d\n",
                    win_emu.process.process_id, thread.id, mod ? mod->name.c_str() : "?",
                    mod ? static_cast<unsigned long long>(thread.current_ip - mod->image_base)
                        : static_cast<unsigned long long>(thread.current_ip),
                    thread.waiting_for_alert, thread.await_objects.size(), thread.await_time.has_value(), thread.await_msg.has_value(),
                    thread.await_io_completion.has_value(), thread.suspended, thread.is_terminated());

                for (const auto& object : thread.await_objects)
                {
                    win_emu.log.error("[thread-dump]   tid=%u awaits object=0x%llx\n", thread.id,
                                      static_cast<unsigned long long>(object.bits));
                }

                if (thread.await_io_completion)
                {
                    win_emu.log.error("[thread-dump]   tid=%u awaits io_completion=0x%llx\n", thread.id,
                                      static_cast<unsigned long long>(thread.await_io_completion->io_completion_handle.bits));
                }
            }

            for (const auto& [id, packet] : win_emu.process.wait_completion_packets)
            {
                if (!packet.associated && !packet.queued_completion)
                {
                    continue;
                }

                const auto* target_event = packet.target_object_handle.value.type == handle_types::event
                                               ? win_emu.process.events.get(packet.target_object_handle)
                                               : nullptr;
                win_emu.log.error("[thread-dump] pid=%u wait_packet=0x%llx io_completion=0x%llx target=0x%llx associated=%d queued=%d "
                                  "target_event_signaled=%d\n",
                                  win_emu.process.process_id, static_cast<unsigned long long>(id),
                                  static_cast<unsigned long long>(packet.io_completion_handle.bits),
                                  static_cast<unsigned long long>(packet.target_object_handle.bits), packet.associated,
                                  packet.queued_completion, target_event ? static_cast<int>(target_event->is_signaled()) : -1);
            }

            for (const auto& [id, completion] : win_emu.process.io_completions)
            {
                win_emu.log.error("[thread-dump] pid=%u io_completion_id=0x%llx queued=%zu\n", win_emu.process.process_id,
                                  static_cast<unsigned long long>(id), completion.queue.size());
            }
        }

        void perform_context_switch_work(windows_emulator& win_emu, vcpu_context& vcpu)
        {
            win_emu.apply_pending_guest_event_signals();

            auto& threads = win_emu.process.threads;

            for (auto it = threads.begin(); it != threads.end();)
            {
                auto& thread = it->second;
                if (!thread.can_be_deleted(&vcpu))
                {
                    ++it;
                    continue;
                }

                if (vcpu.active_thread == &thread)
                {
                    vcpu.active_thread = nullptr;
                }

                it = threads.erase(it).first;
            }

            win_emu.pump_pipe_ipc();
            win_emu.pump_process_control_server();
            win_emu.pump_child_routing_channels();
            win_emu.pump_child_exit_notifications();

            worker_factory_support::create_workers_for_pending_work(win_emu);

            dump_threads_periodically(win_emu);

            auto& devices = win_emu.process.devices;

            // Crappy mechanism to prevent mutation while iterating.
            const auto was_blocked = devices.block_mutation(true);
            const auto _ = utils::finally([&] { devices.block_mutation(was_blocked); });

            for (auto& dev : devices | std::views::values)
            {
                dev.work(win_emu);
            }

            win_emu.inject_scheduled_input();
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

            auto& apcs = thread.pending_apcs;
            if (apcs.empty())
            {
                return;
            }

            thread.setup_if_necessary(vcpu.cpu, win_emu.process);
            auto& cpu = vcpu.cpu;

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
                cpu.write_memory(next_apx.apc_argument2, &status32, sizeof(status32));
                cpu.write_memory(next_apx.apc_argument2 + sizeof(status32), &next_apx.io_information, sizeof(next_apx.io_information));
            }

            if (auto* arm64 = dynamic_cast<arm64_cpu*>(&cpu))
            {
                // KiUserApcDispatcher frame: routine, three arguments, flags (bit 0: test alert), then CONTEXT at +0x30.
                struct
                {
                    uint64_t routine{};
                    std::array<uint64_t, 3> arguments{};
                    uint32_t flags{};
                    uint32_t reserved{};
                    uint64_t padding{};
                    arm64_context context{};
                } arm64_layout;

                static_assert(offsetof(decltype(arm64_layout), context) == 0x30);

                arm64_layout.routine = next_apx.apc_routine;
                arm64_layout.arguments = {next_apx.apc_argument1, next_apx.apc_argument2, next_apx.apc_argument3};
                arm64_layout.flags = 1;
                arm64_layout.context.ContextFlags = arm64_context_all;
                cpu_context::save(*arm64, arm64_layout.context);

                const auto new_sp = align_down(arm64->read_stack_pointer() - sizeof(arm64_layout), 0x10);
                arm64->write_memory(new_sp, arm64_layout);
                arm64->reg(arm64_register::sp, new_sp);
                arm64->reg(arm64_register::pc, win_emu.process.ki_user_apc_dispatcher);
                return;
            }

            auto& emu = vcpu.x64_cpu();

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

            if (thread.registers_in_transit || thread.pending_register_accesses != 0)
            {
                return false;
            }

            auto& emu = vcpu.cpu;
            auto& context = win_emu.process;

            const auto is_ready = thread.is_thread_ready(win_emu);
            const auto has_pending_status = thread.pending_status.has_value();
            const auto can_dispatch_apcs = thread.apc_alertable && !thread.pending_apcs.empty();
            const auto can_run_message_hook = !is_ready && thread.message_wait_return_register.has_value() &&
                                              (win_emu.has_low_level_hook_work(thread.id) || thread.has_hooked_message(win_emu));

            if (!is_ready && !force && !can_dispatch_apcs && !can_run_message_hook)
            {
                return false;
            }

            auto* active_thread = vcpu.active_thread;

            if (active_thread != &thread)
            {
                if (active_thread)
                {
                    win_emu.callbacks.on_thread_switch(*active_thread, thread);
                }

                vcpu.set_active_thread(&thread);

                win_emu.transfer_thread_registers(vcpu, active_thread, thread);
            }

            const auto is_first_setup = !thread.setup_done;
            thread.setup_if_necessary(emu, context);

            if (can_dispatch_apcs && !has_pending_status)
            {
                thread.mark_as_ready(STATUS_USER_APC);
                dispatch_next_apc(win_emu, vcpu, thread);
            }
            else if (can_run_message_hook)
            {
                // The message syscall runs the hook first when it executes again.
                thread.restart_message_wait(emu, win_emu.abi());
            }
            else if (is_first_setup)
            {
                // Only right here, immediately after setup_registers() has just placed this thread at
                // its own real, freshly-assigned entry state (never yet handed to a live cpu.start()
                // call) is it safe to redirect through a real guest call the same way the process-wide
                // warm-up does from a genuine int3 trap - see try_warm_kernelbase_nls_cache_for_thread's
                // own doc comment for why every thread needs this done for itself. Doing the same thing
                // later, against an already-running thread's mid-execution resumption point, corrupted
                // its restored state instead (live-observed: a real thread crashing on a stale ntdll
                // .data address shortly after resuming) - FEX's own per-thread JIT state is only known
                // safe to redirect via a real trap taken from inside an active cpu.start() call, which a
                // thread's first-ever activation trivially satisfies since nothing has run for it yet.
                win_emu.try_warm_kernelbase_nls_cache_for_thread(vcpu);
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

        template <typename Predicate>
        bool switch_to_next_thread_matching(windows_emulator& win_emu, vcpu_context& vcpu, const Predicate& predicate)
        {
            auto& threads = win_emu.process.threads;

            auto resume_at = vcpu.active_thread ? threads.find(*vcpu.active_thread) : threads.end();
            if (resume_at != threads.end())
            {
                ++resume_at;
            }

            for (auto it = resume_at; it != threads.end(); ++it)
            {
                if (predicate(it->second) && switch_to_thread(win_emu, vcpu, it->second))
                {
                    return true;
                }
            }

            for (auto it = threads.begin(); it != resume_at; ++it)
            {
                if (predicate(it->second) && switch_to_thread(win_emu, vcpu, it->second))
                {
                    return true;
                }
            }

            return false;
        }

        bool switch_to_next_thread(windows_emulator& win_emu, vcpu_context& vcpu)
        {
            const scheduler_clock::scope time_scope{win_emu.scheduling_clock()};
            perform_context_switch_work(win_emu, vcpu);

            // Windows boosts a thread whose wait is satisfied, so on a uniprocessor it runs ahead of
            // equal-priority threads that were merely preempted. Without that, every hand-off between
            // waiting threads (posted messages, I/O completions) sits behind a CPU-bound thread's whole
            // time slice.
            const auto prefer_waiting = vcpu.active_thread && vcpu.active_thread->is_waiting();
            if (prefer_waiting && switch_to_next_thread_matching(win_emu, vcpu, [](const emulator_thread& t) { return t.is_waiting(); }))
            {
                return true;
            }

            return switch_to_next_thread_matching(
                win_emu, vcpu, [prefer_waiting](const emulator_thread& t) { return !prefer_waiting || !t.is_waiting(); });
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

        std::unique_ptr<network::dns_lookup> get_dns_lookup(emulator_interfaces& interfaces, const network::network_mode mode)
        {
            if (interfaces.dns_lookup)
            {
                return std::move(interfaces.dns_lookup);
            }

            return network::create_dns_lookup(mode);
        }

        std::unique_ptr<network::socket_factory> get_socket_factory(emulator_interfaces& interfaces, const network::network_mode mode)
        {
            if (interfaces.socket_factory)
            {
                return std::move(interfaces.socket_factory);
            }

            return network::create_socket_factory(mode);
        }

        pipe_ipc_message make_window_created_message(const hwnd window, const uint32_t process_id, const uint32_t thread_id)
        {
            pipe_ipc_message message{};
            message.type = pipe_ipc_message_type::window_created;
            message.window = window;
            message.client_process_id = process_id;
            message.window_thread_id = thread_id;
            return message;
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

        std::unique_ptr<crypt_protect_backend> get_crypt_protect_backend(emulator_interfaces& interfaces,
                                                                         const std::filesystem::path& emulation_root)
        {
            if (interfaces.crypt_protect)
            {
                return std::move(interfaces.crypt_protect);
            }

            return create_default_crypt_protect_backend(emulation_root);
        }

        std::unique_ptr<console_backend> get_console_backend(emulator_interfaces& interfaces)
        {
            if (interfaces.console)
            {
                return std::move(interfaces.console);
            }

            return create_default_console_backend();
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

    PEMachineType get_application_machine(const emulator_settings& settings, application_settings app_settings)
    {
        fixup_application_settings(app_settings);
        const auto& application = app_settings.application;

        const auto root = settings.emulation_root.empty() ? settings.emulation_root : absolute(settings.emulation_root);
        file_system file_sys{root.empty() ? root : root / "filesys", settings.root_mode, settings.root_layer};
        for (const auto& mount : settings.mounts)
        {
            file_sys.map(mount.guest_path, mount.host_path, mount.mode);
        }

        const auto detection = pe_architecture_detector::detect_from_file(file_sys, application);
        if (!detection.is_valid())
        {
            throw std::runtime_error("Cannot determine the machine type of " + application.string() + ": " + detection.error_message);
        }

        return detection.machine;
    }

    machine_type get_machine_type(const PEMachineType machine)
    {
        switch (machine)
        {
        case PEMachineType::AMD64:
        case PEMachineType::ARM64EC:
        case PEMachineType::I386:
            return machine_type::x86_64;
        case PEMachineType::ARM64:
            return machine_type::arm64;
        default:
            throw std::runtime_error("Unsupported executable machine type: 0x" +
                                     utils::string::to_hex_number(static_cast<uint16_t>(machine)));
        }
    }

    windows_emulator::windows_emulator(std::unique_ptr<guest_machine> emu, application_settings app_settings,
                                       const emulator_settings& settings, emulator_callbacks callbacks, emulator_interfaces interfaces)
        : windows_emulator(std::move(emu), settings, std::move(callbacks), std::move(interfaces))
    {
        fixup_application_settings(app_settings);
        this->application_settings_ = std::move(app_settings);
    }

    windows_emulator::windows_emulator(std::unique_ptr<guest_machine> emu, const emulator_settings& settings, emulator_callbacks callbacks,
                                       emulator_interfaces interfaces)
        : emu_(std::move(emu)),
          use_native_timestamp_counter_(!interfaces.clock && !settings.use_relative_time &&
                                        this->emu_->supports_native_timestamp_counter()),
          clock_(std::make_unique<scheduler_clock>(get_clock(interfaces, this->executed_instructions_, settings.use_relative_time))),
          network_mode_(settings.network),
          dns_lookup_(get_dns_lookup(interfaces, settings.network)),
          socket_factory_(get_socket_factory(interfaces, settings.network)),
          ui_backend_(get_ui_backend(interfaces)),
          audio_backend_(get_audio_backend(interfaces)),
          crypt_protect_backend_(get_crypt_protect_backend(
              interfaces, settings.emulation_root.empty() ? settings.emulation_root : absolute(settings.emulation_root))),
          process_manager_(interfaces.processes),
          console_backend_(get_console_backend(interfaces)),
          emulation_root{settings.emulation_root.empty() ? settings.emulation_root : absolute(settings.emulation_root)},
          fake_env(effective_fake_env(settings, static_cast<uint32_t>(this->emu_->vcpu_count()))),
          callbacks(std::move(callbacks)),
          file_sys(emulation_root.empty() ? emulation_root : emulation_root / "filesys", settings.root_mode, settings.root_layer),
          memory(*this->emu_),
          registry(settings.load_registry
                       ? registry_manager{emulation_root.empty() ? settings.registry_directory : emulation_root / "registry"}
                       : registry_manager{}),
          mod_manager(memory, file_sys, this->callbacks, guest_architecture_of(*this->emu_)),
          process(*this->emu_, memory, *this->clock_, this->callbacks, settings.intercept_kusd, guest_architecture_of(*this->emu_)),
          use_relative_time_(settings.use_relative_time),
          // Instruction precision runs through a global execution hook; backends that only count instructions
          // (single-stepping) cannot provide it.
          instruction_precision_(settings.use_instruction_precision && this->emu_->get_guest_cpu(0).supports_instruction_counting() &&
                                 this->emu_->supports_global_memory_execution_hooks()),
          intercept_rdtsc_(settings.intercept_rdtsc),
          intercept_kusd_(settings.intercept_kusd),
          spoof_gpu_features_(settings.spoof_gpu_features),
          elevated_(settings.elevated),
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
            this->vcpus_.push_back(std::make_unique<vcpu_context>(this->emu_->get_guest_cpu(i)));
        }

        this->ui_backend_->set_event_sink([this](const ui_event& event) { this->handle_ui_event(event); });

        if (const auto* inject_input = std::getenv("SOGEN_INJECT_INPUT"))
        {
            this->parse_scheduled_input(inject_input);
        }

#ifndef OS_WINDOWS
        if (this->emulation_root.empty())
        {
            throw std::runtime_error("Emulation root directory can not be empty!");
        }
#endif

        for (const auto& mount : settings.mounts)
        {
            this->file_sys.map(mount.guest_path, mount.host_path, mount.mode);
        }

        for (auto& mount : interfaces.mounts)
        {
            this->file_sys.mount(mount.guest_path, std::move(mount.provider), mount.mode);
        }

        for (const auto& mapping : settings.port_mappings)
        {
            this->map_port(mapping.first, mapping.second);
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

        this->mod_manager.map_main_modules(this->vcpu(0).cpu, this->application_settings_.application, this->version, context, this->log);
        this->install_section_first_execution_hooks();
        publish_kernel_event(context, u"\\SECURITY\\LSA_AUTHENTICATION_INITIALIZED");
        publish_kernel_event(context, u"\\KernelObjects\\SystemErrorPortReady");

        const auto* executable = this->mod_manager.executable;
        const auto* ntdll = this->mod_manager.ntdll;
        const auto* win32u = this->mod_manager.win32u;

        const auto apiset_data =
            apiset::obtain(this->file_sys.read_file(this->version.get_system_root() / "System32" / "apisetschema.dll"));

        this->process.setup(*this, this->application_settings_, *executable, *ntdll, apiset_data, this->mod_manager.wow64_modules_.ntdll32);

        const auto ntdll_data = emu.read_memory(ntdll->image_base, static_cast<size_t>(ntdll->size_of_image));
        const auto win32u_data = emu.read_memory(win32u->image_base, static_cast<size_t>(win32u->size_of_image));

        this->dispatcher.setup(ntdll->exports, ntdll_data, win32u->exports, win32u_data, this->abi().syscall_abi());

        const auto initial_argument = this->process.is_wow64_process ? this->process.peb32->value() : this->process.peb64.value();
        const auto main_thread_id = context.create_thread(this->memory, this->mod_manager.executable->entry_point, initial_argument,
                                                          this->mod_manager.executable->size_of_stack_reserve, 0, true);

        const std::scoped_lock lock(this->kernel_lock_);
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

    void windows_emulator::register_pipe_ipc_peer(std::unique_ptr<pipe_ipc_channel> peer)
    {
        for (const auto& short_name : this->known_named_pipe_servers_)
        {
            pipe_ipc_message message{};
            message.type = pipe_ipc_message_type::server_created;
            message.pipe_name = short_name;
            peer->send(message);
        }

        for (const auto& win : this->process.windows | std::views::values)
        {
            peer->send(make_window_created_message(win.handle, this->process.process_id, win.thread_id));
        }

        for (const auto& [window, owner] : this->process.foreign_windows)
        {
            peer->send(make_window_created_message(window, owner.process_id, owner.thread_id));
        }

        for (const auto& [pid, native_64bit] : this->known_sibling_pids_)
        {
            pipe_ipc_message message{};
            message.type = pipe_ipc_message_type::process_alive;
            message.client_process_id = pid;
            message.native_64bit = native_64bit;
            peer->send(message);
        }

        this->pipe_ipc_peers_.push_back(std::move(peer));
    }

    void windows_emulator::register_child_control_channel(const uint32_t record_id, std::unique_ptr<process_control_channel> channel)
    {
        this->child_control_channels_[record_id] = std::move(channel);
    }

    process_control_channel* windows_emulator::find_child_control_channel(const uint32_t record_id)
    {
        const auto entry = this->child_control_channels_.find(record_id);
        if (entry == this->child_control_channels_.end())
        {
            return nullptr;
        }

        return entry->second.get();
    }

    void windows_emulator::drop_child_control_channel(const uint32_t record_id)
    {
        this->child_control_channels_.erase(record_id);
    }

    void windows_emulator::broadcast_named_pipe_write(const std::u16string_view name, const uint64_t connection_id, const bool to_server,
                                                      const std::string_view data)
    {
        if (std::getenv("SOGEN_TRACE_PIPE_RELAY_HEALTH"))
        {
            this->log.info("[pipe-relay-health-trace] broadcast_named_pipe_write name='%s' bytes=%zu peers=%zu\n", u16_to_u8(name).c_str(),
                           data.size(), this->pipe_ipc_peers_.size());
        }

        if (this->pipe_ipc_peers_.empty())
        {
            return;
        }

        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::write;
        message.pipe_name = name;
        message.data = data;
        message.connection_id = connection_id;
        message.to_server = to_server;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_alpc(const std::u16string_view port_name, const uint64_t connection_id, const bool to_server,
                                          std::string data)
    {
        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::alpc;
        message.pipe_name = port_name;
        message.data = std::move(data);
        message.connection_id = connection_id;
        message.to_server = to_server;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_named_pipe_connect(const std::u16string_view name, const uint64_t connection_id,
                                                        const std::optional<uint32_t> client_process_id)
    {
        if (std::getenv("SOGEN_TRACE_PIPE_IO"))
        {
            this->log.info("[pipe-io-trace] broadcast_named_pipe_connect name='%s' peers=%zu\n", u16_to_u8(name).c_str(),
                           this->pipe_ipc_peers_.size());
        }

        if (this->pipe_ipc_peers_.empty())
        {
            return;
        }

        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::connect;
        message.pipe_name = name;
        message.client_process_id = client_process_id;
        message.connection_id = connection_id;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_named_pipe_server_created(const std::u16string_view name)
    {
        if (this->pipe_ipc_peers_.empty())
        {
            return;
        }

        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::server_created;
        message.pipe_name = name;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_process_alive(const uint32_t pid, const std::optional<bool> native_64bit)
    {
        this->known_sibling_pids_.insert_or_assign(pid, native_64bit);

        if (this->pipe_ipc_peers_.empty())
        {
            return;
        }

        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::process_alive;
        message.client_process_id = pid;
        message.native_64bit = native_64bit;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_focus_change(const hwnd window, const hwnd parent_window)
    {
        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::focus_changed;
        message.window = window;
        message.parent_window = parent_window;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::present_window_surface(const hwnd window, const ui_surface_desc& surface)
    {
        const auto* win = this->process.windows.get(window);
        if (!win)
        {
            this->ui().present_surface(window, surface);
            return;
        }

        const auto width = win->client_width() > 0 ? win->client_width() : surface.width;
        const auto height = win->client_height() > 0 ? win->client_height() : surface.height;
        const RECT client{.left = 0, .top = 0, .right = width, .bottom = height};
        const auto route = syscalls::route_window_surface(this->process, window, client, client);
        if (!route || !route->foreign)
        {
            this->ui().present_surface(window, surface);
            return;
        }

        window_surface_update update{.layer = window, .destination = route->destination, .clip = route->clip, .visible = route->visible};
        const auto inline_pixels = this->surface_exporter_.publish(update, surface);
        this->send_window_surface(route->target, update, inline_pixels.value_or(std::string{}));
    }

    void windows_emulator::send_window_surface(const hwnd window, const window_surface_update& update, const std::string& inline_pixels)
    {
        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::window_surface;
        message.window = window;
        message.data = inline_pixels;
        message.surface = update;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::relay_window_surface(const pipe_ipc_message& message)
    {
        const auto& update = *message.surface;
        const auto route = syscalls::route_window_surface(this->process, message.window, update.destination, update.clip);
        if (!route)
        {
            return;
        }

        if (route->foreign)
        {
            auto next = update;
            next.destination = route->destination;
            next.clip = route->clip;
            next.visible = update.visible && route->visible;
            this->send_window_surface(route->target, next, message.data);
            return;
        }

        if (!update.visible || !route->visible)
        {
            this->ui().remove_layer(update.layer);
            return;
        }

        if (const auto surface = this->surface_importer_.resolve(update, message.data))
        {
            this->ui().present_layer(route->target, update.layer, route->destination, route->clip, *surface);
        }
    }

    void windows_emulator::broadcast_window_created(const hwnd window, const uint32_t thread_id)
    {
        const auto message = make_window_created_message(window, this->process.process_id, thread_id);
        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    void windows_emulator::broadcast_window_destroyed(const hwnd window)
    {
        pipe_ipc_message message{};
        message.type = pipe_ipc_message_type::window_destroyed;
        message.window = window;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            peer->send(message);
        }
    }

    bool windows_emulator::is_known_sibling_pid(const uint32_t pid) const
    {
        return this->known_sibling_pids_.contains(pid);
    }

    std::optional<bool> windows_emulator::known_sibling_native_64bit(const uint32_t pid) const
    {
        const auto sibling = this->known_sibling_pids_.find(pid);
        return sibling == this->known_sibling_pids_.end() ? std::nullopt : sibling->second;
    }

    void windows_emulator::pump_pipe_ipc()
    {
        static const bool trace_pipe_io = std::getenv("SOGEN_TRACE_PIPE_IO") != nullptr;

        for (auto& peer : this->pipe_ipc_peers_)
        {
            while (const auto message = peer->try_receive())
            {
                if (trace_pipe_io)
                {
                    this->log.info("[pipe-io-trace] pump_pipe_ipc received type=%d name='%s'\n", static_cast<int>(message->type),
                                   u16_to_u8(message->pipe_name).c_str());
                }

                if (message->type == pipe_ipc_message_type::write)
                {
                    deliver_bytes_to_named_pipe(this->process, message->pipe_name, message->connection_id, message->to_server,
                                                message->data);
                }
                else if (message->type == pipe_ipc_message_type::connect)
                {
                    connect_named_pipe_client(*this, this->process, message->pipe_name, message->connection_id, message->client_process_id);
                }
                else if (message->type == pipe_ipc_message_type::alpc)
                {
                    alpc::deliver(*this, *message);
                }
                else if (message->type == pipe_ipc_message_type::server_created)
                {
                    this->register_named_pipe_server(pipe_short_name(message->pipe_name));
                }
                else if (message->type == pipe_ipc_message_type::process_alive && message->client_process_id)
                {
                    this->known_sibling_pids_.insert_or_assign(*message->client_process_id, message->native_64bit);
                }
                else if (message->type == pipe_ipc_message_type::focus_changed && this->process.windows.get(message->parent_window))
                {
                    const auto previous = this->process.focus_window;
                    this->process.focus_window = message->window;

                    const auto* previous_window = this->process.windows.get(previous);
                    if (auto* thread = previous_window ? get_thread_by_id(this->process, previous_window->thread_id) : nullptr)
                    {
                        thread->post_message(*this,
                                             msg{.window = previous, .message = WM_KILLFOCUS, .wParam = message->window, .lParam = 0});
                    }
                }
                else if (message->type == pipe_ipc_message_type::window_created && message->client_process_id &&
                         syscalls::is_foreign_window_handle(this->process, message->window))
                {
                    this->process.foreign_windows[message->window] = {.process_id = *message->client_process_id,
                                                                      .thread_id = message->window_thread_id};
                    this->process.user_handles.mirror_foreign_window(message->window, message->window_thread_id);
                }
                else if (message->type == pipe_ipc_message_type::window_destroyed &&
                         syscalls::is_foreign_window_handle(this->process, message->window))
                {
                    this->process.foreign_windows.erase(message->window);
                    this->process.user_handles.unmirror_foreign_window(message->window);
                }
                else if (message->type == pipe_ipc_message_type::window_surface && message->surface &&
                         this->process.windows.get(message->window))
                {
                    this->relay_window_surface(*message);
                }

                // pipe_ipc_peers_ mirrors the process spawn tree (one entry for this process's own
                // parent, plus one per child it spawned - see the member's doc comment), so relaying
                // to every OTHER entry floods the message across the whole tree without looping back:
                // each edge is walked at most once per hop, and a tree has no cycles to re-visit. Two
                // siblings (e.g. a crash handler and a network-service child of the same browser
                // process) only share a common parent, not a direct link, so without this they could
                // never reach each other at all.
                for (auto& other_peer : this->pipe_ipc_peers_)
                {
                    if (other_peer != peer)
                    {
                        other_peer->send(*message);
                    }
                }
            }
        }
    }

    void windows_emulator::set_process_control_server(std::unique_ptr<process_control_channel> channel)
    {
        this->process_control_server_ = std::move(channel);
    }

    void windows_emulator::pump_process_control_server()
    {
        if (this->process_control_server_)
        {
            pump_process_control(*this, *this->process_control_server_);
        }
    }

    void windows_emulator::set_process_control_router(std::unique_ptr<process_control_channel> channel)
    {
        this->process_control_router_ = std::move(channel);
    }

    process_control_channel* windows_emulator::process_control_router() const
    {
        return this->process_control_router_.get();
    }

    void windows_emulator::register_child_routing_channel(std::unique_ptr<process_control_channel> channel)
    {
        this->child_routing_channels_.push_back(std::move(channel));
    }

    void windows_emulator::pump_child_routing_channels()
    {
        for (const auto& channel : this->child_routing_channels_)
        {
            while (const auto request = channel->try_receive())
            {
                channel->respond(route_process_control_request(*this, *request));
            }
        }
    }

    process_control_channel* windows_emulator::find_child_control_channel_by_pid(const uint32_t pid)
    {
        for (const auto& [record_id, record] : this->process.child_processes)
        {
            if (record.pid != pid || record.exit_status != STATUS_PENDING)
            {
                continue;
            }

            if (auto* channel = this->find_child_control_channel(record_id))
            {
                return channel;
            }
        }

        return nullptr;
    }

    void windows_emulator::notify_own_exit(const std::optional<NTSTATUS> exit_status)
    {
        if (!this->process_control_server_)
        {
            return;
        }

        this->process_control_server_->notify_exit(static_cast<int32_t>(exit_status.value_or(STATUS_UNSUCCESSFUL)));
    }

    void windows_emulator::pump_child_exit_notifications()
    {
        for (auto it = this->child_control_channels_.begin(); it != this->child_control_channels_.end();)
        {
            const auto exit_status = it->second ? it->second->try_receive_exit_notification() : std::nullopt;

            if (!exit_status.has_value())
            {
                ++it;
                continue;
            }

            const auto record = this->process.child_processes.find(it->first);
            if (record != this->process.child_processes.end() && record->second.exit_status == STATUS_PENDING)
            {
                record->second.exit_status = static_cast<NTSTATUS>(*exit_status);
            }

            it = this->child_control_channels_.erase(it);
        }
    }

    void windows_emulator::register_named_pipe_server(const std::u16string_view short_name)
    {
        this->known_named_pipe_servers_.emplace(short_name);

        auto [begin, end] = this->pending_pipe_waits_.equal_range(short_name);
        for (auto it = begin; it != end; ++it)
        {
            if (auto* e = this->process.events.get(it->second))
            {
                e->set_signaled(true);
            }
        }

        this->pending_pipe_waits_.erase(begin, end);
    }

    void windows_emulator::unregister_named_pipe_server(const std::u16string_view short_name)
    {
        const auto known = this->known_named_pipe_servers_.find(short_name);
        if (known != this->known_named_pipe_servers_.end())
        {
            this->known_named_pipe_servers_.erase(known);
        }
    }

    bool windows_emulator::is_named_pipe_server_known(const std::u16string_view short_name)
    {
        return this->known_named_pipe_servers_.contains(short_name) || named_objects::is_pipe_server_published(this->process, short_name);
    }

    void windows_emulator::register_pipe_wait(const std::u16string_view short_name, const handle event)
    {
        this->pending_pipe_waits_.emplace(short_name, event);
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

        static const bool sched_diag = std::getenv("EMULATOR_SCHED_DIAG") != nullptr;

        if (sched_diag && needed_switch)
        {
            dump_thread_wait_states_diag(this->process, vcpu);
        }

        // guest_function_call.hpp's invoke_guest_function redirects a thread's own instruction
        // stream into a real guest function and expects to observe the result once it returns to
        // its sentinel address, without another thread running in between and reading state the
        // call is still populating (e.g. try_warm_kernelbase_nls_cache_breakpoint's NLS-cache
        // warm-up). A fairness/timer preemption (this vcpu's own time slice, or the periodic
        // interrupt_thread used on backends without instruction precision) knows nothing about
        // that invariant and would otherwise switch to another ready thread mid-call. Defer such a
        // preemption while the call is in flight and the thread stays genuinely ready - a real
        // block (a wait the call itself enters) still switches away normally. Bounded so a call
        // that unexpectedly never reaches its sentinel cannot hang the vCPU forever; falling back
        // to the pre-existing racy scheduling behavior here is never worse than never having
        // deferred at all.
        if (needed_switch && vcpu.active_thread && vcpu.active_thread->pending_guest_call.has_value() &&
            vcpu.active_thread->is_thread_ready(*this))
        {
            auto& pending_call = *vcpu.active_thread->pending_guest_call;
            if (++pending_call.deferred_preemptions <= MAX_PENDING_GUEST_CALL_DEFERRED_PREEMPTIONS)
            {
                return true;
            }
        }

        static thread_local int idle_spin_count = 0;
        bool woken_by_signal = false;

        while (!switch_to_next_thread(*this, vcpu))
        {
            woken_by_signal = false;

            if (sched_diag)
            {
                ++idle_spin_count;
                if (idle_spin_count == 500 || idle_spin_count == 2000 || idle_spin_count % 1000 == 0)
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
                this->thread_registers_saved_.notify_all();
            }

            const auto host_wait_pending = has_pending_host_wait(this->process);

            ++this->idle_vcpus_;

            // Idle: nothing is ready. Release the kernel lock while pumping UI events
            // and sleeping so other threads (UI delivery, vCPU workers) can run.
            lock.unlock();
            this->debugger_.run_pending_tasks(vcpu.cpu.index());

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
                woken_by_signal = this->wait_for_idle_wakeup(std::chrono::milliseconds(1));
            }

            --this->idle_vcpus_;
            lock.lock();

            this->drain_low_level_input();

            if (this->should_stop)
            {
                vcpu.switch_thread = needed_switch;
                return false;
            }
        }

        if (woken_by_signal)
        {
            this->wake_idle_vcpu();
        }

        if (sched_diag && needed_switch)
        {
            fprintf(stderr, "[SCHED_DIAG] quantum tick switched to thread id=%u\n", vcpu.active_thread ? vcpu.active_thread->id : 0);
        }

        idle_spin_count = 0;
        return true;
    }

    void windows_emulator::vcpu_worker(vcpu_context& vcpu)
    {
        vcpu.runner = std::this_thread::get_id();
        const auto clear_runner = utils::finally([&vcpu] { vcpu.runner = std::thread::id{}; });

        std::unique_lock lock(this->kernel_lock_);

        while (!this->should_stop)
        {
            // Debugger requests run before the scheduling decision, as the single-vCPU loop does: one of them can
            // suspend a thread, and running them afterwards would let a suspend land on the thread just chosen, which
            // would then run a quantum even though the debugger was told the suspend succeeded.
            lock.unlock();
            this->debugger_.run_pending_tasks(vcpu.cpu.index());
            lock.lock();

            // A loop rather than a single check: the register transfer of a switch releases the kernel lock, so the
            // chosen thread can be terminated or suspended before it runs.
            bool switched = true;
            while (vcpu.switch_thread || !vcpu.active_thread || !vcpu.thread().is_thread_ready(*this))
            {
                switched = this->perform_thread_switch(vcpu, lock);
                if (!switched)
                {
                    break;
                }
            }

            if (!switched || this->should_stop)
            {
                break;
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

    void windows_emulator::transfer_thread_registers(vcpu_context& vcpu, emulator_thread* outgoing, emulator_thread& incoming)
    {
        this->kernel_lock_.assert_held();

        if (this->vcpu_count_ == 1)
        {
            if (outgoing)
            {
                outgoing->save(vcpu.cpu);
            }

            incoming.restore(vcpu.cpu, this->process.gdt_base);
            return;
        }

        if (outgoing)
        {
            outgoing->registers_in_transit = true;
        }
        incoming.registers_in_transit = true;

        {
            this->kernel_lock_.unlock();
            const auto _ = utils::finally([&] {
                this->kernel_lock_.lock();
                if (outgoing)
                {
                    outgoing->registers_in_transit = false;
                }
                incoming.registers_in_transit = false;
                this->thread_registers_saved_.notify_all();
            });

            if (outgoing)
            {
                outgoing->save(vcpu.cpu);
            }

            incoming.load_registers(vcpu.cpu);
        }

        incoming.refresh_execution_context(vcpu.cpu, this->process.gdt_base);
    }

    bool windows_emulator::wait_for_thread_registers(vcpu_context& vcpu, emulator_thread& thread)
    {
        this->kernel_lock_.assert_held();

        const auto loaded_elsewhere = [&]() -> vcpu_context* {
            auto* owner = find_vcpu_running_thread(*this, thread);
            return owner && owner != &vcpu && !owner->active_thread_saved ? owner : nullptr;
        };

        if (!thread.registers_in_transit && !loaded_elsewhere())
        {
            return true;
        }

        const auto was_saved = std::exchange(vcpu.active_thread_saved, true);
        ++thread.pending_register_accesses;
        this->thread_registers_saved_.notify_all();
        const auto _ = utils::finally([&] {
            vcpu.active_thread_saved = was_saved;
            --thread.pending_register_accesses;
            this->wake_idle_vcpu();
        });

        while (thread.registers_in_transit || loaded_elsewhere())
        {
            if (this->should_stop)
            {
                return false;
            }

            if (auto* owner = thread.registers_in_transit ? nullptr : loaded_elsewhere())
            {
                owner->switch_thread = true;
                owner->cpu.stop();
            }

            // Bounded, so that the kick is repeated: a stop that reaches a vCPU just before it enters guest code is
            // lost on WHP.
            auto* dispatch_vcpu = std::exchange(this->dispatch_vcpu_, nullptr);
            this->thread_registers_saved_.wait_for(this->kernel_lock_, std::chrono::milliseconds(1));
            this->dispatch_vcpu_ = dispatch_vcpu;
        }

        return true;
    }

    bool windows_emulator::save_dispatching_thread(vcpu_context* vcpu)
    {
        if (this->vcpu_count_ == 1 || !vcpu || !vcpu->active_thread || vcpu->active_thread_saved)
        {
            return false;
        }

        vcpu->active_thread->save(vcpu->cpu);
        vcpu->active_thread_saved = true;
        this->thread_registers_saved_.notify_all();
        return true;
    }

    void windows_emulator::reload_dispatching_thread(vcpu_context& vcpu) const
    {
        vcpu.thread().restore(vcpu.cpu, this->process.gdt_base);
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

        if (try_complete_guest_function(vcpu, address))
        {
            if (this->process.kernelbase_nls_cache_warming)
            {
                this->process.kernelbase_nls_cache_warming = false;

                if (thread.teb64.has_value())
                {
                    thread.teb64->access([&](TEB64& teb) {
                        if (teb.NlsCache == 0)
                        {
                            teb.NlsCache = this->process.kernelbase_nls_process_local_cache;
                        }
                    });
                }
            }

            return;
        }

        if (!this->process.kernelbase_nls_cache_warmed)
        {
            this->try_warm_kernelbase_nls_cache(vcpu, address);
        }

        if (!thread.callback_stack.empty() && address == this->process.zw_callback_return)
        {
            thread.callback_return_rax = this->abi().return_value(vcpu.cpu);
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

        static const bool trace_mojo_write_path = std::getenv("SOGEN_TRACE_MOJO_WRITE_PATH") != nullptr;
        if (trace_mojo_write_path)
        {
            struct mojo_write_path_site
            {
                uint64_t rva;
                uint64_t resolved_addr;
                const char* name;
                bool dump_channel_win_state;
            };

            static mojo_write_path_site sites[] = {
                {0x1bf3d20ULL, 0, "Connector::Accept", false},
                {0x10c0ddeULL, 0, "Connector::AcceptAndGetResult", false},
                {0x51b3fbULL, 0, "MojoWriteMessage", false},
                {0x1469eb0ULL, 0, "SyncContext::OnChannelOpened", false},
                {0x1469fa0ULL, 0, "Context::OnChannelOpened", false},
                {0x2902f5cULL, 0, "Channel::Connect", false},
                {0x290323aULL, 0, "MessagePipeReader::FinishInitializationOnIOThread", false},
                {0x2903400ULL, 0, "mojom::ChannelProxy::SetPeerPid", false},
                {0x860658ULL, 0, "Router::SetOutwardLink", false},
                {0x1097694ULL, 0, "Router::Flush", false},
                {0x109c9d0ULL, 0, "Router::AcceptRouteClosureFrom", false},
                {0x81c660ULL, 0, "RemoteRouterLink::AcceptParcel", false},
                {0x10a16d0ULL, 0, "LocalRouterLink::AcceptParcel", false},
                {0x81f168ULL, 0, "NodeLink::Transmit", false},
                {0xab58e0ULL, 0, "ChannelWin::Write", true},
                {0x1c07620ULL, 0, "MessagePumpForIO::RegisterIOHandler", false},
                {0x1c075e0ULL, 0, "CurrentIOThread::RegisterIOHandler", false},
            };

            static bool resolved_mojo_write_path_addrs = false;
            if (!resolved_mojo_write_path_addrs)
            {
                const auto* mod = this->mod_manager.find_by_name("msedge.dll");
                if (mod)
                {
                    for (auto& site : sites)
                    {
                        site.resolved_addr = mod->image_base + site.rva;
                    }
                    resolved_mojo_write_path_addrs = true;
                }
            }

            if (resolved_mojo_write_path_addrs)
            {
                for (const auto& site : sites)
                {
                    if (address == site.resolved_addr)
                    {
                        const auto rcx = vcpu.x64_cpu().reg<uint64_t>(x86_register::rcx);
                        const auto rdx = vcpu.x64_cpu().reg<uint64_t>(x86_register::rdx);
                        fprintf(stderr, "[MOJO_WRITE_PATH] pid=%d guest_pid=%u tid=%u site=%s rip=0x%llx this=0x%llx arg1=0x%llx\n",
                                ::getpid(), this->process.process_id, thread.id, site.name, static_cast<unsigned long long>(address),
                                static_cast<unsigned long long>(rcx), static_cast<unsigned long long>(rdx));

                        if (site.dump_channel_win_state)
                        {
                            uint64_t outgoing_begin{};
                            uint64_t outgoing_end{};
                            uint8_t delay_writes{};
                            uint8_t reject_writes{};
                            uint8_t is_write_pending{};

                            const auto ok = vcpu.cpu.try_read_memory(rcx + 0x110, &outgoing_begin, sizeof(outgoing_begin)) &&
                                            vcpu.cpu.try_read_memory(rcx + 0x118, &outgoing_end, sizeof(outgoing_end)) &&
                                            vcpu.cpu.try_read_memory(rcx + 0x120, &delay_writes, sizeof(delay_writes)) &&
                                            vcpu.cpu.try_read_memory(rcx + 0x121, &reject_writes, sizeof(reject_writes)) &&
                                            vcpu.cpu.try_read_memory(rcx + 0x122, &is_write_pending, sizeof(is_write_pending));

                            if (ok)
                            {
                                fprintf(stderr,
                                        "[MOJO_WRITE_PATH]   ChannelWin state: outgoing_begin=0x%llx outgoing_end=0x%llx "
                                        "queue_empty=%d delay_writes=%d reject_writes=%d is_write_pending=%d\n",
                                        static_cast<unsigned long long>(outgoing_begin), static_cast<unsigned long long>(outgoing_end),
                                        outgoing_begin == outgoing_end, delay_writes != 0, reject_writes != 0, is_write_pending != 0);
                            }
                            else
                            {
                                fprintf(stderr, "[MOJO_WRITE_PATH]   ChannelWin state: memory read failed\n");
                            }
                        }

                        break;
                    }
                }
            }
        }

        static const bool trace_gpu_device_init = std::getenv("SOGEN_TRACE_GPU_DEVICE_INIT") != nullptr;
        if (trace_gpu_device_init)
        {
            struct gpu_device_init_site
            {
                const char* module_name;
                uint64_t rva;
                uint64_t resolved_addr;
                const char* name;
            };

            static gpu_device_init_site sites[] = {
                {"libGLESv2.dll", 0x382260ULL, 0, "Renderer9::initialize"},    {"libGLESv2.dll", 0x2fb830ULL, 0, "Renderer11::initialize"},
                {"libGLESv2.dll", 0x336050ULL, 0, "DisplayD3D::initialize"},   {"d3d11.dll", 0x4a660ULL, 0, "D3D11CreateDevice"},
                {"d3d11.dll", 0x4a7e0ULL, 0, "D3D11CreateDeviceAndSwapChain"},
            };

            for (auto& site : sites)
            {
                if (site.resolved_addr == 0)
                {
                    const auto* mod = this->mod_manager.find_by_name(site.module_name);
                    if (mod)
                    {
                        site.resolved_addr = mod->image_base + site.rva;
                        fprintf(stderr, "[GPU_DEVICE_INIT] pid=%d guest_pid=%u resolved site=%s at 0x%llx (module %s loaded)\n", ::getpid(),
                                this->process.process_id, site.name, static_cast<unsigned long long>(site.resolved_addr), site.module_name);
                    }
                }

                if (site.resolved_addr != 0 && address == site.resolved_addr)
                {
                    fprintf(stderr, "[GPU_DEVICE_INIT] pid=%d guest_pid=%u tid=%u site=%s rip=0x%llx\n", ::getpid(),
                            this->process.process_id, thread.id, site.name, static_cast<unsigned long long>(address));
                }
            }
        }

        this->callbacks.on_instruction(address);
    }

    void windows_emulator::try_warm_kernelbase_nls_cache(vcpu_context& vcpu, const uint64_t address)
    {
        if (this->process.kernelbase_dllmain_return_address != 0)
        {
            if (address != this->process.kernelbase_dllmain_return_address)
            {
                return;
            }

            this->process.kernelbase_nls_cache_warmed = true;
            this->process.kernelbase_dllmain_return_address = 0;

            if (this->process.kernelbase_get_user_default_lcid != 0)
            {
                this->begin_kernelbase_nls_cache_warmup(vcpu);
            }

            return;
        }

        if (this->process.kernelbase_entry_point == 0 || address != this->process.kernelbase_entry_point)
        {
            return;
        }

        uint64_t real_return_address = 0;
        if (vcpu.cpu.try_read_memory(vcpu.cpu.read_stack_pointer(), &real_return_address, sizeof(real_return_address)) &&
            real_return_address != 0)
        {
            this->process.kernelbase_dllmain_return_address = real_return_address;
        }

        this->process.kernelbase_entry_point = 0;
    }

    void windows_emulator::arm_kernelbase_nls_cache_breakpoint(const uint64_t address)
    {
        std::byte original{};
        if (!this->emu().try_read_memory(address, &original, sizeof(original)))
        {
            return;
        }

        constexpr std::byte int3{0xCC};
        if (!this->emu().try_write_memory(address, &int3, sizeof(int3)))
        {
            return;
        }

        for (const auto& vcpu : this->vcpus_)
        {
            vcpu->cpu.invalidate_code_cache(address, sizeof(int3));
        }

        this->process.kernelbase_nls_cache_breakpoint_address = address;
        this->process.kernelbase_nls_cache_breakpoint_original_byte = original;
    }

    void windows_emulator::disarm_kernelbase_nls_cache_breakpoint()
    {
        if (this->process.kernelbase_nls_cache_breakpoint_address == 0)
        {
            return;
        }

        this->emu().try_write_memory(this->process.kernelbase_nls_cache_breakpoint_address,
                                     &this->process.kernelbase_nls_cache_breakpoint_original_byte,
                                     sizeof(this->process.kernelbase_nls_cache_breakpoint_original_byte));
        for (const auto& vcpu : this->vcpus_)
        {
            vcpu->cpu.invalidate_code_cache(this->process.kernelbase_nls_cache_breakpoint_address,
                                            sizeof(this->process.kernelbase_nls_cache_breakpoint_original_byte));
        }
        this->process.kernelbase_nls_cache_breakpoint_address = 0;
    }

    void windows_emulator::arm_rtl_query_performance_counter_trap(const uint64_t address)
    {
        constexpr std::byte int3{0xCC};
        if (this->emu().try_write_memory(address, &int3, sizeof(int3)))
        {
            this->process.rtl_query_performance_counter_trap = address;
        }
    }

    bool windows_emulator::try_service_rtl_query_performance_counter(vcpu_context& vcpu, const uint64_t address) const
    {
        if (address == 0 || address != this->process.rtl_query_performance_counter_trap)
        {
            return false;
        }

        auto& acting = vcpu.x64_cpu();
        const auto counter_ptr = acting.reg<uint64_t>(x86_register::rcx);
        const auto rsp = acting.read_stack_pointer();

        uint64_t return_address = 0;
        if (!acting.try_read_memory(rsp, &return_address, sizeof(return_address)))
        {
            return false;
        }

        const int64_t counter_value = this->process.kusd.performance_counter_value();
        if (counter_ptr != 0)
        {
            acting.try_write_memory(counter_ptr, &counter_value, sizeof(counter_value));
        }

        acting.reg(x86_register::rax, uint64_t{1});
        acting.reg(x86_register::rsp, rsp + sizeof(return_address));
        acting.reg(x86_register::rip, return_address);
        return true;
    }

    bool windows_emulator::try_warm_kernelbase_nls_cache_breakpoint(vcpu_context& vcpu, const uint64_t address)
    {
        auto& thread = vcpu.thread();
        static const bool trace_nls_warmup = std::getenv("SOGEN_TRACE_NLS_WARMUP") != nullptr;

        if (address == 0 || address != this->process.kernelbase_nls_cache_breakpoint_address)
        {
            return false;
        }

        if (try_complete_guest_function(vcpu, address))
        {
            this->process.kernelbase_nls_cache_warming = false;

            if (thread.teb64.has_value())
            {
                thread.teb64->access([&](TEB64& teb) {
                    if (teb.NlsCache == 0)
                    {
                        teb.NlsCache = this->process.kernelbase_nls_process_local_cache;
                    }
                });
            }

            if (trace_nls_warmup)
            {
                fprintf(stderr, "[NLS_WARMUP] tid=%u pid=%u warm-up call completed, NlsCache placeholder=0x%llx\n", thread.id,
                        this->process.process_id, static_cast<unsigned long long>(this->process.kernelbase_nls_process_local_cache));
                fflush(stderr);
            }

            this->disarm_kernelbase_nls_cache_breakpoint();
            return true;
        }

        if (address != this->process.kernelbase_entry_point && address != this->process.kernelbase_dllmain_return_address)
        {
            // Some other guest thread (e.g. a real KiUserCallbackDispatcher round trip through
            // zw_callback_return) reached the address this function is temporarily watching for the
            // warm-up's own sentinel. Resume it as if the patch was never there - the warm-up's own
            // completion is still pending and stays armed for its actual owner.
            if (trace_nls_warmup)
            {
                fprintf(stderr, "[NLS_WARMUP] tid=%u pid=%u BYSTANDER hit watched addr 0x%llx while warm-up in flight\n", thread.id,
                        this->process.process_id, static_cast<unsigned long long>(address));
                fflush(stderr);
            }
            vcpu.x64_cpu().reg(x86_register::rip, address);
            return true;
        }

        this->disarm_kernelbase_nls_cache_breakpoint();
        vcpu.x64_cpu().reg(x86_register::rip, address);

        if (address == this->process.kernelbase_entry_point)
        {
            uint64_t real_return_address = 0;
            if (vcpu.cpu.try_read_memory(vcpu.cpu.read_stack_pointer(), &real_return_address, sizeof(real_return_address)) &&
                real_return_address != 0)
            {
                this->process.kernelbase_dllmain_return_address = real_return_address;
                this->arm_kernelbase_nls_cache_breakpoint(real_return_address);
            }

            if (trace_nls_warmup)
            {
                fprintf(stderr, "[NLS_WARMUP] tid=%u pid=%u hit kernelbase.dll entry point, real_return_address=0x%llx\n", thread.id,
                        this->process.process_id, static_cast<unsigned long long>(real_return_address));
                fflush(stderr);
            }

            this->process.kernelbase_entry_point = 0;
            return true;
        }

        this->process.kernelbase_nls_cache_warmed = true;
        this->process.kernelbase_dllmain_return_address = 0;

        if (this->process.kernelbase_get_user_default_lcid != 0)
        {
            this->begin_kernelbase_nls_cache_warmup(vcpu);

            if (trace_nls_warmup)
            {
                fprintf(stderr, "[NLS_WARMUP] tid=%u pid=%u kernelbase.dll DllMain returned, invoking GetUserDefaultLCID warm-up\n",
                        thread.id, this->process.process_id);
                fflush(stderr);
            }
        }

        return true;
    }

    void windows_emulator::begin_kernelbase_nls_cache_warmup(vcpu_context& vcpu)
    {
        auto& thread = vcpu.thread();

        thread.kernelbase_nls_cache_warmed_individually = true;

        if (thread.teb64.has_value())
        {
            thread.teb64->access([](TEB64& teb) { teb.NlsCache = 0; });
        }

        this->process.kernelbase_nls_cache_warming = true;
        invoke_guest_function(thread, vcpu.x64_cpu(), this->process.zw_callback_return, this->process.kernelbase_get_user_default_lcid);

        if (!this->uses_instruction_precision())
        {
            this->arm_kernelbase_nls_cache_breakpoint(this->process.zw_callback_return);
        }
    }

    void windows_emulator::try_warm_kernelbase_nls_cache_for_thread(vcpu_context& vcpu)
    {
        auto& thread = vcpu.thread();

        if (thread.kernelbase_nls_cache_warmed_individually || !this->process.kernelbase_nls_cache_warmed ||
            this->process.kernelbase_nls_cache_warming || this->process.kernelbase_nls_cache_breakpoint_address != 0 ||
            this->process.kernelbase_get_user_default_lcid == 0)
        {
            return;
        }

        this->begin_kernelbase_nls_cache_warmup(vcpu);
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

    void windows_emulator::setup_hooks()
    {
        this->callbacks.on_module_load.add([this](mapped_module& mod) {
            for (size_t i = 0; i < mod.sections.size(); ++i)
            {
                this->install_section_first_execution_hook(mod, i);
            }

            if (this->debugger_saw_process_ && this->reports_module_to_debugger(mod))
            {
                this->report_debug_event({
                    .kind = process_debug_event_kind::load_dll,
                    .thread_id = this->current_thread().id,
                    .base_address = mod.image_base,
                    .path = u16_to_u8(mod.module_path.u16string()),
                });
            }
        });

        // try_warm_kernelbase_nls_cache relies on uses_instruction_precision()'s host hook to reach
        // kernelbase.dll's DllMain entry/return - unavailable on FEX/KVM/WHP (see
        // kernelbase_nls_cache_breakpoint_address's doc comment). Arm the breakpoint-based
        // equivalent's first stage here instead, the moment kernelbase.dll's real entry point is
        // known, well before the loader ever calls it. IMAGE_FILE_MACHINE_AMD64-gated: a WoW64
        // process maps both the native 64-bit kernelbase.dll (system32) and a 32-bit one
        // (syswow64), both named "kernelbase.dll" - this mechanism tracks only one patched address
        // at a time (kernelbase_nls_cache_breakpoint_address), and the crash it targets is in the
        // native 64-bit copy (TEB64.NlsCache), so the 32-bit one must never arm it: doing so would
        // silently overwrite the tracked address without restoring the first patch, leaving a
        // permanently un-restored 0xCC in the 64-bit copy's own entry point.
        this->callbacks.on_module_load.add([this](mapped_module& mod) {
            if (!this->uses_instruction_precision() && mod.name == "kernelbase.dll" && mod.machine == IMAGE_FILE_MACHINE_AMD64 &&
                mod.entry_point != 0)
            {
                this->arm_kernelbase_nls_cache_breakpoint(mod.entry_point);

                if (std::getenv("SOGEN_TRACE_NLS_WARMUP"))
                {
                    fprintf(stderr, "[NLS_WARMUP] pid=%u armed entry-point breakpoint at 0x%llx (process.kernelbase_entry_point=0x%llx)\n",
                            this->process.process_id, static_cast<unsigned long long>(mod.entry_point),
                            static_cast<unsigned long long>(this->process.kernelbase_entry_point));
                    fflush(stderr);
                }
            }
        });

        // See rtl_query_performance_counter_trap's doc comment: ntdll64 is mapped once, synchronously,
        // before this process ever runs guest code, so its own real export address is known the
        // moment it loads - no need for the multi-stage DllMain-return tracking kernelbase.dll's
        // hook above needs. IMAGE_FILE_MACHINE_AMD64-gated for the same reason as that hook: a WoW64
        // process maps both a native 64-bit ntdll.dll (system32) and a 32-bit one (syswow64), both
        // named "ntdll.dll", and only the native 64-bit copy's RtlQueryPerformanceCounter is ever
        // reachable from unrebased, always-unbacked-on-this-host KUSER_SHARED_DATA accesses.
        this->callbacks.on_module_load.add([this](mapped_module& mod) {
            if (!this->uses_instruction_precision() && mod.name == "ntdll.dll" && mod.machine == IMAGE_FILE_MACHINE_AMD64)
            {
                const auto address = mod.find_export("RtlQueryPerformanceCounter");
                if (address != 0)
                {
                    this->arm_rtl_query_performance_counter_trap(address);
                }
            }
        });

        this->callbacks.on_module_unload.add([this](mapped_module& mod) {
            if (this->debugger_saw_process_ && this->reports_module_to_debugger(mod))
            {
                this->report_debug_event({
                    .kind = process_debug_event_kind::unload_dll,
                    .thread_id = this->current_thread().id,
                    .base_address = mod.image_base,
                });
            }

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
        });

        if (auto* x64 = dynamic_cast<x86_64_emulator*>(this->emu_.get()))
        {
            this->setup_x64_hooks(*x64);
        }
        else if (auto* arm64 = dynamic_cast<arm64_emulator*>(this->emu_.get()))
        {
            this->setup_arm64_hooks(*arm64);
        }
        else
        {
            throw std::runtime_error("Unsupported Windows guest machine: " + this->emu().get_name());
        }

        this->emu().hook_memory_violation([&](cpu_interface& cpu, const uint64_t address, const size_t size,
                                              const memory_operation operation, const memory_violation_type type) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto* x64 = dynamic_cast<x86_64_cpu*>(&vcpu.cpu);
            if (x64 && x64->reg<uint16_t>(x86_register::cs) == 0x33)
            {
                // loading gs selector only works in 64-bit mode
                const auto required_gs_base = vcpu.thread().gs_segment->get_base();
                const auto actual_gs_base = x64->get_segment_base(x86_register::gs);
                if (actual_gs_base != required_gs_base)
                {
                    x64->set_segment_base(x86_register::gs, required_gs_base);
                    return memory_violation_continuation::restart;
                }
            }

            if (const auto guarded_address = this->memory.find_guard_page_violation_address(address, size, operation))
            {
                // The stack_guard tag is set once at thread creation on the specific region sogen
                // allocated as the thread's terminal guard buffer, but a guest legitimately re-arming
                // PAGE_GUARD there (e.g. crypt32.dll's InternalVerifyStackAvailable calling the
                // standard _resetstkoflw()-style recovery idiom after catching STATUS_STACK_OVERFLOW)
                // goes through the ordinary NtProtectVirtualMemory path, which has no way to restore an
                // sogen-internal tag. Real Windows re-evaluates "is this within one page of
                // DeallocationStack" on every fault rather than consuming a one-shot flag, so check the
                // thread's actual guard-buffer bounds directly as well.
                auto& thread = vcpu.thread();
                constexpr uint64_t page_size = 0x1000;
                const auto deallocation_stack = thread.stack_base - thread.stack_guard_size;
                const auto within_stack_guard_buffer =
                    thread.stack_guard_size != 0 && *guarded_address >= deallocation_stack && *guarded_address < thread.stack_base;

                // stack_guard_size is a whole ALLOCATION_GRANULARITY (64 KiB) buffer, not a single
                // page: real Windows only treats a fault as terminal (STATUS_STACK_OVERFLOW) when it
                // lands within one page of DeallocationStack (MiCheckForUserStackOverflow); every other
                // guard-page hit above that is grown silently, with no exception delivered to the guest.
                const auto at_true_bottom = within_stack_guard_buffer && *guarded_address < deallocation_stack + page_size;
                const auto guarded_page = page_align_down(*guarded_address, page_size);
                const auto page_permissions = this->memory.get_region_info(guarded_page).permissions;
                const auto is_stack_guard = page_permissions.is_stack_guard() || at_true_bottom;

                this->memory.protect_memory(guarded_page, page_size,
                                            page_permissions & ~(memory_permission_ext::guard | memory_permission_ext::stack_guard));

                if (within_stack_guard_buffer && !at_true_bottom)
                {
                    return memory_violation_continuation::restart;
                }

                if (is_stack_guard)
                {
                    dispatch_stack_overflow(*this, vcpu, *guarded_address, operation);
                }
                else
                {
                    dispatch_guard_page_violation(*this, vcpu, *guarded_address, operation);
                }
            }
            else
            {
                // A fault on a null/near-null address is almost always a call through a null function
                // pointer (e.g. a Vulkan entry point the shim doesn't implement). Log the caller's
                // return address so the missing function's call site can be identified.
                if (x64 && address < 0x1000)
                {
                    const auto is_32bit = is_32bit_code_segment(*x64);
                    const uint64_t sp = is_32bit ? x64->reg<uint32_t>(x86_register::esp) : x64->read_stack_pointer();
                    uint64_t return_address = 0;
                    if (x64->try_read_memory(sp, &return_address, is_32bit ? sizeof(uint32_t) : sizeof(uint64_t)))
                    {
                        const auto* mod = this->mod_manager.find_by_address(return_address);
                        this->log.error("Null-pointer call to 0x%llx; caller return address 0x%llx (%s+0x%llx)\n",
                                        static_cast<unsigned long long>(address), static_cast<unsigned long long>(return_address),
                                        mod ? mod->name.c_str() : "?",
                                        static_cast<unsigned long long>(mod ? return_address - mod->image_base : return_address));
                    }

                    if (std::getenv("EMULATOR_NPC_DIAG"))
                    {
                        fprintf(stderr,
                                "[NPC_DIAG] thread_id=%u vcpu_index=%zu sp=0x%llx eax=0x%llx ecx=0x%llx edx=0x%llx ebx=0x%llx "
                                "ebp=0x%llx esi=0x%llx edi=0x%llx\n",
                                vcpu.thread().id, cpu.index(), static_cast<unsigned long long>(sp),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::eax)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::ecx)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::edx)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::ebx)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::ebp)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::esi)),
                                static_cast<unsigned long long>(x64->reg<uint32_t>(x86_register::edi)));
                        for (int off = -16; off <= 32; off += 4)
                        {
                            uint32_t slot = 0;
                            if (x64->try_read_memory(sp + static_cast<int64_t>(off), &slot, sizeof(slot)))
                            {
                                fprintf(stderr, "[NPC_DIAG]   [sp%+d] = 0x%08x\n", off, slot);
                            }
                        }
                    }
                }

                if (x64 && this->callbacks.on_generic_activity)
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

                    const auto ebp = x64->reg<uint64_t>(x86_register::rbp);
                    uint32_t stack_args[3]{};
                    x64->try_read_memory(ebp + 8, stack_args, sizeof(stack_args));

                    // For a wild ret/call the frame is already popped, so ebp is useless - the smashed
                    // frame's remnants still sit around esp ([esp-4] = the popped return address,
                    // [esp..] = the previous frame's arguments), which is what identifies the caller.
                    const auto esp = x64->reg<uint64_t>(x86_register::rsp);
                    std::string stack_window{};
                    for (int64_t offset = -16; offset <= 40; offset += 4)
                    {
                        uint32_t slot = 0;
                        if (x64->try_read_memory(esp + offset, &slot, sizeof(slot)))
                        {
                            stack_window += utils::string::va(" %08x", slot);
                        }
                        else
                        {
                            stack_window += " ????????";
                        }
                    }

                    this->callbacks.on_generic_activity(utils::string::va(
                        "Memory violation context: addr=0x%" PRIx64 " %s ebp=0x%" PRIx64 " [ebp+8]=0x%x [ebp+c]=0x%x [ebp+10]=0x%x"
                        " esp=0x%" PRIx64 " stack[esp-16..esp+40]=%s",
                        address, neighborhood.c_str(), ebp, stack_args[0], stack_args[1], stack_args[2], esp, stack_window.c_str()));
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
        else if (!this->emu().get_guest_cpu(0).is_stop_thread_safe())
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

    void windows_emulator::setup_arm64_hooks(arm64_emulator& emu)
    {
        // Handlers that move PC rewind it by the SVC size, so skipping the SVC lands on their target.
        emu.hook_instruction(arm64_hookable_instructions::svc, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            this->dispatcher.dispatch(*this, vcpu);
            return instruction_hook_continuation::skip_instruction;
        });

        emu.hook_instruction(arm64_hookable_instructions::read_counter, [&](cpu_interface& cpu, const uint64_t destination) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            this->callbacks.on_rdtsc();

            const auto ticks = this->clock_->timestamp_counter();
            dynamic_cast<arm64_cpu&>(vcpu.cpu).reg(static_cast<arm64_register>(destination), ticks);
            return instruction_hook_continuation::skip_instruction;
        });

        emu.hook_interrupt([&](cpu_interface& cpu, const int exception_class) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            this->callbacks.on_exception();

            switch (static_cast<arm64_exception_class>(exception_class))
            {
            case arm64_exception_class::undefined_instruction:
                this->callbacks.on_suspicious_activity("Illegal instruction");
                dispatch_illegal_instruction_violation(*this, vcpu);
                return;
            case arm64_exception_class::breakpoint_instruction:
                this->handle_arm64_breakpoint(vcpu);
                return;
            case arm64_exception_class::software_step:
                // The step completed, so it is reported at the next instruction.
                vcpu.thread().current_ip = vcpu.cpu.read_instruction_pointer();
                dispatch_single_step(*this, vcpu);
                return;
            default:
                throw std::runtime_error("Unsupported ARM64 guest exception class " + std::to_string(exception_class));
            }
        });
    }

    // Windows on ARM selects the kernel action by the BRK immediate; unassigned immediates are illegal instructions.
    void windows_emulator::handle_arm64_breakpoint(vcpu_context& vcpu)
    {
        auto& cpu = dynamic_cast<arm64_cpu&>(vcpu.cpu);
        const auto pc = cpu.read_instruction_pointer();

        switch ((cpu.read_memory<uint32_t>(pc) >> 5) & 0xFFFF)
        {
        case 0xF000:
            this->callbacks.on_suspicious_activity("Breakpoint");
            dispatch_breakpoint(*this, vcpu);
            return;
        case 0xF001:
            dispatch_exception(*this, vcpu, STATUS_ASSERTION_FAILURE, {});
            return;
        case 0xF002: {
            this->callbacks.on_suspicious_activity("DbgPrint");
            const auto service = cpu.reg(arm64_register::x16);
            if (service == BREAKPOINT_PRINT || service == BREAKPOINT_LOAD_SYMBOLS || service == BREAKPOINT_UNLOAD_SYMBOLS ||
                service == BREAKPOINT_COMMAND_STRING)
            {
                // ntdll follows the debug service BRK with BRK #0xF000 for an attached debugger to stop on.
                cpu.reg(arm64_register::pc, pc + 8);
            }
            else
            {
                dispatch_breakpoint(*this, vcpu);
            }
            return;
        }
        case 0xF003:
            this->callbacks.on_fast_fail(cpu.reg<uint32_t>(arm64_register::w0));
            this->process.exit_status = STATUS_FAIL_FAST_EXCEPTION;
            this->stop();
            return;
        case 0xF004:
            dispatch_integer_division_by_zero(*this, vcpu);
            return;
        default:
            this->callbacks.on_suspicious_activity("Illegal instruction");
            dispatch_illegal_instruction_violation(*this, vcpu);
            return;
        }
    }

    void windows_emulator::setup_x64_hooks(x86_64_emulator& emu)
    {
        emu.hook_instruction(x86_hookable_instructions::syscall, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            this->dispatcher.dispatch(*this, vcpu);
            return instruction_hook_continuation::skip_instruction;
        });

        // rdtsc/rdtscp are intercepted only when timing must stay exact (the analyzer). Otherwise the
        // guest reads the host TSC natively - the WHP backend serves the exit with the real TSC and no
        // kernel lock - removing a hot VM-exit path for games.
        if (this->intercept_rdtsc_ || !this->use_native_timestamp_counter_)
        {
            emu.hook_instruction(x86_hookable_instructions::rdtscp, [&](cpu_interface& cpu, uint64_t) {
                const std::scoped_lock lock(this->kernel_lock_);
                auto& vcpu = this->vcpu(cpu.index());
                const scoped_dispatch dispatch(*this, vcpu);
                auto& acting = vcpu.x64_cpu();
                this->callbacks.on_rdtscp();

                if (this->use_native_timestamp_counter_)
                {
                    return instruction_hook_continuation::run_instruction;
                }

                const auto ticks = this->clock_->timestamp_counter();
                acting.reg(x86_register::rax, static_cast<uint32_t>(ticks));
                acting.reg(x86_register::rdx, static_cast<uint32_t>(ticks >> 32));

                // Return the IA32_TSC_AUX value in RCX (low 32 bits)
                auto tsc_aux = 0; // Need to replace this with proper CPUID later
                acting.reg(x86_register::rcx, tsc_aux);

                return instruction_hook_continuation::skip_instruction;
            });

            emu.hook_instruction(x86_hookable_instructions::rdtsc, [&](cpu_interface& cpu, uint64_t) {
                const std::scoped_lock lock(this->kernel_lock_);
                auto& vcpu = this->vcpu(cpu.index());
                const scoped_dispatch dispatch(*this, vcpu);
                auto& acting = vcpu.x64_cpu();
                this->callbacks.on_rdtsc();

                if (this->use_native_timestamp_counter_)
                {
                    return instruction_hook_continuation::run_instruction;
                }

                const auto ticks = this->clock_->timestamp_counter();
                acting.reg(x86_register::rax, static_cast<uint32_t>(ticks));
                acting.reg(x86_register::rdx, static_cast<uint32_t>(ticks >> 32));

                return instruction_hook_continuation::skip_instruction;
            });
        }

        // TODO: Unicorn needs this - This should be handled in the backend
        emu.hook_instruction(x86_hookable_instructions::invalid, [&](cpu_interface& cpu, uint64_t) {
            const std::scoped_lock lock(this->kernel_lock_);
            // TODO: Unify icicle & unicorn handling
            dispatch_illegal_instruction_violation(*this, this->vcpu(cpu.index()));
            return instruction_hook_continuation::skip_instruction; //
        });

        emu.hook_interrupt([&](cpu_interface& cpu, const int interrupt) {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(cpu.index());
            const scoped_dispatch dispatch(*this, vcpu);
            auto& acting = vcpu.x64_cpu();
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
            case 3: {
                const auto real_bp_address =
                    acting.read_instruction_pointer() - (acting.reports_hook_observed_rip_past_instruction() ? 1 : 0);
                if (!this->uses_instruction_precision() && this->try_warm_kernelbase_nls_cache_breakpoint(vcpu, real_bp_address))
                {
                    return;
                }

                if (!this->uses_instruction_precision() && this->try_service_rtl_query_performance_counter(vcpu, real_bp_address))
                {
                    return;
                }

                this->callbacks.on_suspicious_activity("Breakpoint");
                dispatch_breakpoint(*this, vcpu);
                return;
            }
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

    void windows_emulator::attach_debugger()
    {
        this->debugger_.attach();
        this->process.peb64.access([](PEB64& peb) { peb.BeingDebugged = TRUE; });
        if (this->process.peb32)
        {
            this->process.peb32->access([](PEB32& peb) { peb.BeingDebugged = TRUE; });
        }
    }

    void windows_emulator::detach_debugger()
    {
        this->debugger_.run_on_emulation_thread(*this, [this] {
            this->process.peb64.access([](PEB64& peb) { peb.BeingDebugged = FALSE; });
            if (this->process.peb32)
            {
                this->process.peb32->access([](PEB32& peb) { peb.BeingDebugged = FALSE; });
            }
        });
        this->debugger_.detach();
    }

    uint32_t windows_emulator::report_debug_event(process_debug_event event)
    {
        return this->debugger_.report(*this, this->active_vcpu().cpu.index(), std::move(event));
    }

    void windows_emulator::interrupt_for_debugger()
    {
        {
            const std::scoped_lock lock(this->kernel_lock_);
            auto& vcpu = this->vcpu(0);
            if (vcpu.cpu.is_stop_thread_safe())
            {
                vcpu.switch_thread = true;
                vcpu.cpu.stop();
            }
        }

        this->wake_idle_vcpu();
    }

    // Only images backed by a file are reported, which leaves out the emulator's own code (the WOW64 heaven gate).
    // A WOW64 process's 64-bit modules (native ntdll, wow64*.dll) lie above 4GB, out of a 32-bit debugger's reach.
    bool windows_emulator::reports_module_to_debugger(const mapped_module& mod) const
    {
        if (mod.module_path.empty())
        {
            return false;
        }

        return !this->process.is_wow64_process || mod.image_base + mod.size_of_image <= 0x100000000ULL;
    }

    // Like the kernel at process creation: the image first, then every module mapped before the first instruction.
    void windows_emulator::report_debugger_process_start()
    {
        if (this->debugger_saw_process_ || !this->debugger_.is_attached())
        {
            return;
        }

        const std::scoped_lock lock(this->kernel_lock_);
        this->debugger_saw_process_ = true;

        const auto thread_id = this->process.initial_thread_id;

        // The debugger reads and writes the initial thread's registers at these events, so it has to be on the vCPU
        // already, as for its first time slice.
        auto& vcpu = this->vcpu(0);
        if (!vcpu.active_thread)
        {
            for (auto& thread : this->process.threads | std::views::values)
            {
                if (thread.id == thread_id)
                {
                    switch_to_thread(*this, vcpu, thread, true);
                    break;
                }
            }
        }
        const auto* executable = this->mod_manager.executable;
        this->report_debug_event({
            .kind = process_debug_event_kind::create_process,
            .thread_id = thread_id,
            .base_address = executable ? executable->image_base : 0,
            .start_address = executable ? executable->entry_point : 0,
            .path = executable ? u16_to_u8(executable->module_path.u16string()) : std::string{},
        });

        for (const auto& mod : this->mod_manager.modules() | std::views::values)
        {
            if (&mod != executable && this->reports_module_to_debugger(mod))
            {
                this->report_debug_event({
                    .kind = process_debug_event_kind::load_dll,
                    .thread_id = thread_id,
                    .base_address = mod.image_base,
                    .path = u16_to_u8(mod.module_path.u16string()),
                });
            }
        }
    }

    void windows_emulator::start(size_t count)
    {
        this->should_stop = false;
        this->last_stop_reason_ = stop_reason::none;
        this->last_stop_detail_.clear();
        this->setup_process_if_necessary();

        this->debugger_.begin_emulation();
        const auto end_debugger_emulation = utils::finally([this] { this->debugger_.end_emulation(); });
        this->report_debugger_process_start();

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
        std::vector<std::thread> workers{};
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
                if (worker.joinable())
                {
                    worker.join();
                }
            }

            if (interrupt_thread.joinable())
            {
                interrupt_thread.join();
            }
        });

        const bool preempt_vcpus = !this->uses_instruction_precision() && this->emu().get_guest_cpu(0).is_stop_thread_safe();
        const bool refresh_kusd = !this->intercept_kusd_;
        if (preempt_vcpus || refresh_kusd)
        {
            interrupt_thread = std::thread([&, preempt_vcpus, refresh_kusd] {
                while (!this->should_stop)
                {
                    std::unique_lock lock{interrupt_mutex};
                    interrupt_cond.wait_for(lock, std::chrono::milliseconds(20), [&] {
                        return this->should_stop.load(); //
                    });

                    if (this->should_stop)
                    {
                        break;
                    }

                    if (refresh_kusd)
                    {
                        const std::scoped_lock kernel_lock(this->kernel_lock_);
                        this->process.kusd.refresh();
                    }

                    if (preempt_vcpus)
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
            this->ui_backend_->pump_events();
            // One worker thread per vCPU; this thread pumps UI events until the run ends.
            active_workers = this->vcpu_count_;
            workers.reserve(this->vcpu_count_);

            for (uint32_t i = 0; i < this->vcpu_count_; ++i)
            {
                workers.emplace_back([this, i, &active_workers] {
                    try
                    {
                        this->vcpu_worker(this->vcpu(i));
                    }
                    catch (const std::exception& e)
                    {
                        this->log.error("vCPU %u worker terminated: %s\n", i, e.what());
                        this->stop();
                    }

                    --active_workers;
                });
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
        vcpu.runner = std::this_thread::get_id();
        const auto clear_runner = utils::finally([&vcpu] { vcpu.runner = std::thread::id{}; });

        std::unique_lock lock(this->kernel_lock_);

        while (!this->should_stop)
        {
            lock.unlock();
            this->ui_backend_->pump_events();
            this->callbacks.on_event_pump();
            this->debugger_.run_pending_tasks(vcpu.cpu.index());
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

    void windows_emulator::set_foreground_window(const hwnd window)
    {
        const auto previous = this->process.foreground_window;
        if (previous == window)
        {
            return;
        }

        this->process.foreground_window = window;

        if (const auto* previous_window = this->process.windows.get(previous))
        {
            if (auto* owner = get_thread_by_id(this->process, previous_window->thread_id); owner && owner->active_window == previous)
            {
                owner->set_active_window(0);
            }
        }

        if (const auto* new_window = this->process.windows.get(window))
        {
            if (auto* owner = get_thread_by_id(this->process, new_window->thread_id))
            {
                owner->set_active_window(window);
            }
        }
    }

    void windows_emulator::handle_ui_event(const ui_event& event)
    {
        const std::scoped_lock lock(this->kernel_lock_);
        this->handle_ui_event_locked(event);
    }

    void windows_emulator::handle_ui_event_locked(const ui_event& event)
    {
        this->kernel_lock_.assert_held();

        const auto hook_id = low_level_hook_id(event.message);
        auto& pending = this->process.pending_low_level_input;

        if (is_character_message(event.message) && this->process.drop_low_level_characters)
        {
            return;
        }

        if (hook_id == k_wh_keyboard_ll)
        {
            this->process.drop_low_level_characters = false;
        }

        std::vector<uint64_t> hooks{};
        if (hook_id != 0)
        {
            for (auto& thread : this->process.threads | std::views::values)
            {
                if (thread.is_terminated())
                {
                    continue;
                }

                for (const auto& hook : thread.low_level_hooks)
                {
                    if (hook.id == hook_id)
                    {
                        hooks.push_back(hook.handle);
                    }
                }
            }

            // The most recently installed hook runs first.
            std::ranges::sort(hooks, std::greater{});
        }

        // Input already waiting for hooks keeps everything after it waiting too, so the guest sees it in order.
        if (hooks.empty() && (pending.empty() || !is_input_message(event.message)))
        {
            this->deliver_ui_event(event);
            return;
        }

        auto [screen_x, screen_y] = std::pair{this->process.cursor_x, this->process.cursor_y};
        if (is_pointer_message(event.message) && !is_mouse_wheel_message(event.message))
        {
            std::tie(screen_x, screen_y) = this->cursor_after_pointer_event(event);
        }

        const auto now = this->clock().steady_now();
        pending.push_back(process_context::low_level_input{
            .id = this->process.next_low_level_input_id++,
            .event = event,
            .screen_x = screen_x,
            .screen_y = screen_y,
            .time = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()),
            .hooks = std::move(hooks),
            .waiting_since = now,
        });

        if (auto* owner = this->drain_low_level_input())
        {
            this->wake_thread(*owner);
        }
    }

    std::pair<emulator_thread*, const user_low_level_hook*> windows_emulator::find_low_level_hook(const uint64_t handle)
    {
        for (auto& thread : this->process.threads | std::views::values)
        {
            if (thread.is_terminated())
            {
                continue;
            }

            for (const auto& hook : thread.low_level_hooks)
            {
                if (hook.handle == handle)
                {
                    return {&thread, &hook};
                }
            }
        }

        return {};
    }

    // Windows passes input on to the next hook when a low-level hook does not return within LowLevelHooksTimeout, and
    // ignores the result of a hook that returns later. Since Windows 7 it also silently removes that hook, so a thread
    // that installs one and never pumps messages delays only the first input, not every one.
    emulator_thread* windows_emulator::drain_low_level_input()
    {
        constexpr auto low_level_hooks_timeout = std::chrono::seconds(1);

        auto& pending = this->process.pending_low_level_input;
        const auto now = this->clock().steady_now();
        while (!pending.empty())
        {
            auto& front = pending.front();
            const auto timed_out = now - front.waiting_since > low_level_hooks_timeout;
            if (front.in_progress && !timed_out)
            {
                return nullptr;
            }

            while (!front.in_progress && !front.hooks.empty() && !this->find_low_level_hook(front.hooks.front()).second)
            {
                front.hooks.erase(front.hooks.begin());
            }

            if (timed_out && !front.hooks.empty())
            {
                const auto stalled = front.hooks.front();
                for (auto& thread : this->process.threads | std::views::values)
                {
                    if (std::erase_if(thread.low_level_hooks,
                                      [stalled](const user_low_level_hook& hook) { return hook.handle == stalled; }) != 0)
                    {
                        std::erase(thread.running_low_level_inputs, front.id);
                    }
                }

                front.hooks.erase(front.hooks.begin());
                front.in_progress = false;
                front.waiting_since = now;
                continue;
            }

            if (front.hooks.empty())
            {
                const auto event = front.event;
                pending.erase(pending.begin());
                if (!pending.empty())
                {
                    pending.front().waiting_since = now;
                }

                this->deliver_ui_event(event);
                continue;
            }

            return this->find_low_level_hook(front.hooks.front()).first;
        }

        return nullptr;
    }

    std::optional<std::pair<process_context::low_level_input, user_low_level_hook>> windows_emulator::begin_low_level_hook(
        emulator_thread& thread)
    {
        if (!this->has_low_level_hook_work(thread.id))
        {
            return std::nullopt;
        }

        auto& front = this->process.pending_low_level_input.front();
        front.in_progress = true;
        front.waiting_since = this->clock().steady_now();
        thread.running_low_level_inputs.push_back(front.id);
        return std::pair{front, *this->find_low_level_hook(front.hooks.front()).second};
    }

    void windows_emulator::finish_low_level_hook(emulator_thread& thread, const bool swallow)
    {
        if (thread.running_low_level_inputs.empty())
        {
            return;
        }

        const auto id = thread.running_low_level_inputs.back();
        thread.running_low_level_inputs.pop_back();

        auto& pending = this->process.pending_low_level_input;
        if (pending.empty() || !pending.front().in_progress || pending.front().id != id)
        {
            return;
        }

        const auto now = this->clock().steady_now();
        auto& front = pending.front();
        front.in_progress = false;
        front.waiting_since = now;
        if (swallow)
        {
            this->process.drop_low_level_characters = is_key_down_message(front.event.message);
            pending.erase(pending.begin());

            while (this->process.drop_low_level_characters && !pending.empty() && is_character_message(pending.front().event.message))
            {
                pending.erase(pending.begin());
            }

            if (!pending.empty())
            {
                pending.front().waiting_since = now;
            }
        }
        else if (!front.hooks.empty())
        {
            front.hooks.erase(front.hooks.begin());
        }

        if (auto* owner = this->drain_low_level_input())
        {
            this->wake_thread(*owner);
        }
    }

    bool windows_emulator::has_low_level_hook_work(const uint32_t thread_id)
    {
        this->drain_low_level_input();

        const auto& pending = this->process.pending_low_level_input;
        if (pending.empty() || pending.front().in_progress || pending.front().hooks.empty())
        {
            return false;
        }

        const auto* owner = this->find_low_level_hook(pending.front().hooks.front()).first;
        return owner && owner->id == thread_id;
    }

    // Kick the vCPU running the thread so it promptly re-checks its message queue; if the thread is not running on
    // any vCPU right now, fall back to vCPU 0 to force a reschedule that can pick up the now-ready thread.
    void windows_emulator::wake_thread(emulator_thread& thread)
    {
        auto* running_on = find_vcpu_running_thread(*this, thread);
        auto& vcpu = running_on ? *running_on : this->vcpu(0);
        vcpu.switch_thread = true;
        vcpu.cpu.stop();
    }

    std::pair<int32_t, int32_t> windows_emulator::cursor_after_pointer_event(const ui_event& event) const
    {
        int32_t cursor_x = this->process.cursor_x;
        int32_t cursor_y = this->process.cursor_y;
        if (event.relative_motion || event.cursor_target)
        {
            if (event.cursor_target)
            {
                std::tie(cursor_x, cursor_y) = *event.cursor_target;
            }
            else
            {
                cursor_x += event.relative_motion->first;
                cursor_y += event.relative_motion->second;
            }
            if (const auto& clip = this->process.cursor_clip)
            {
                const auto left = static_cast<int32_t>(clip->left);
                const auto top = static_cast<int32_t>(clip->top);
                cursor_x = std::clamp(cursor_x, left, std::max(left, static_cast<int32_t>(clip->right) - 1));
                cursor_y = std::clamp(cursor_y, top, std::max(top, static_cast<int32_t>(clip->bottom) - 1));
            }
        }
        else if (const auto origin = get_client_origin_relative_to_ancestor(this->process, event.window, 0))
        {
            cursor_x = origin->x + point_x(event.lParam);
            cursor_y = origin->y + point_y(event.lParam);
        }

        return {cursor_x, cursor_y};
    }

    void windows_emulator::update_key_state(const ui_event& event)
    {
        const auto virtual_key = static_cast<uint8_t>(event.wParam & 0xFF);
        if (virtual_key == VK_PACKET)
        {
            return;
        }

        const bool down = event.message == WM_KEYDOWN || event.message == WM_SYSKEYDOWN;
        const auto set = [&](const uint8_t key) {
            auto& state = this->process.key_state[key];
            if (down && (state & 0x80) == 0)
            {
                this->process.async_key_state[key] = 1;
                if (key == VK_CAPITAL || key == VK_NUMLOCK || key == VK_SCROLL)
                {
                    state ^= 0x01;
                }
            }
            state = static_cast<uint8_t>((down ? 0x80 : 0) | (state & 0x01));
        };

        const auto side = side_virtual_key(static_cast<uint32_t>(event.wParam & 0xFF), event.lParam);
        set(side);
        for (const auto& [generic, left, right] : modifier_sides)
        {
            if (side == left || side == right)
            {
                const bool any_down = ((this->process.key_state[left] | this->process.key_state[right]) & 0x80) != 0;
                if (any_down && (this->process.key_state[generic] & 0x80) == 0)
                {
                    this->process.async_key_state[generic] = 1;
                }
                this->process.key_state[generic] = any_down ? 0x80 : 0;
            }
        }
    }

    ui_event windows_emulator::resolve_injected_event(const ui_event& event) const
    {
        if (!event.injected)
        {
            return event;
        }

        auto resolved = event;
        const auto down = [&](const uint32_t key) { return (this->process.key_state[key] & 0x80) != 0; };

        if (is_keyboard_message(event.message))
        {
            const auto virtual_key = static_cast<uint32_t>(event.wParam & 0xFF);
            const bool up = event.message == WM_KEYUP || event.message == WM_SYSKEYUP;
            const bool alt = virtual_key == VK_MENU || down(VK_MENU);
            const bool system_key = (alt || virtual_key == VK_F10) && !down(VK_CONTROL);
            if (system_key)
            {
                resolved.message = up ? WM_SYSKEYUP : WM_SYSKEYDOWN;
            }
            else
            {
                resolved.message = up ? WM_KEYUP : WM_KEYDOWN;
            }

            constexpr uint64_t context_bit = 1ull << 29;
            constexpr uint64_t previous_state_bit = 1ull << 30;
            resolved.lParam &= ~(context_bit | previous_state_bit);
            resolved.lParam |= (alt && system_key) ? context_bit : 0;
            resolved.lParam |=
                (up || (virtual_key != VK_PACKET && down(side_virtual_key(virtual_key, event.lParam)))) ? previous_state_bit : 0;
            return resolved;
        }

        if (is_pointer_message(event.message))
        {
            uint64_t flags = 0;
            for (const auto& [key, flag] :
                 {std::pair{VK_LBUTTON, MK_LBUTTON}, std::pair{VK_RBUTTON, MK_RBUTTON}, std::pair{VK_SHIFT, MK_SHIFT},
                  std::pair{VK_CONTROL, MK_CONTROL}, std::pair{VK_MBUTTON, MK_MBUTTON}, std::pair{VK_XBUTTON1, MK_XBUTTON1},
                  std::pair{VK_XBUTTON2, MK_XBUTTON2}})
            {
                const bool pressed =
                    mouse_button_virtual_key(event.message, event.wParam) == key ? is_mouse_button_down_message(event.message) : down(key);
                flags |= pressed ? static_cast<uint64_t>(flag) : 0;
            }
            resolved.wParam = (event.wParam & 0xFFFF0000) | flags;
        }

        return resolved;
    }

    bool windows_emulator::forward_pointer_input_to_foreign_children(const hwnd parent, const uint32_t message, const uint64_t w_param,
                                                                     const int32_t x, const int32_t y)
    {
        const pointer_input_request input{.message = message, .w_param = w_param, .x = x, .y = y};
        for (auto it = this->process.foreign_child_windows.begin(); it != this->process.foreign_child_windows.end();)
        {
            if (it->second != parent)
            {
                ++it;
                continue;
            }

            const auto response = syscalls::forward_window_control(*this, window_control_op::pointer_input, it->first, 0,
                                                                   std::as_bytes(std::span{&input, 1}));
            if (!response)
            {
                it = this->process.foreign_child_windows.erase(it);
                continue;
            }

            if (static_cast<NTSTATUS>(response->status) == STATUS_SUCCESS)
            {
                return true;
            }

            ++it;
        }

        return false;
    }

    bool windows_emulator::deliver_pointer_input_from_parent(const hwnd window, const pointer_input_request& input)
    {
        const auto* win = this->process.windows.get(window);
        if (!win || (win->style & WS_VISIBLE) == 0 || (win->style & WS_DISABLED) != 0)
        {
            return false;
        }

        if (input.x < win->x || input.x >= win->x + win->width || input.y < win->y || input.y >= win->y + win->height)
        {
            return false;
        }

        this->deliver_ui_event(ui_event{
            .window = window,
            .message = input.message,
            .wParam = input.w_param,
            .lParam = pack_point(input.x - win->x, input.y - win->y),
        });
        return true;
    }

    // SOGEN_INJECT_INPUT=<window class>@<seconds>:<action>[;...] where <action> is <x>,<y> (left click),
    // key=<name> (Tab, Enter, Escape, Backspace, Space or a single character) or text=<characters>.
    // text-file types the content of the file named by SOGEN_INJECT_TEXT_FILE, keeping it off command lines.
    void windows_emulator::parse_scheduled_input(const std::string_view specification)
    {
        static const std::map<std::string, uint16_t, std::less<>> named_keys{
            {"Backspace", keyboard_layout::vk_back}, {"Enter", keyboard_layout::vk_return}, {"Escape", keyboard_layout::vk_escape},
            {"Space", keyboard_layout::vk_space},    {"Tab", keyboard_layout::vk_tab},
        };

        for (size_t start = 0; start <= specification.size();)
        {
            const auto end = std::min(specification.find(';', start), specification.size());
            const auto entry = specification.substr(start, end - start);
            start = end + 1;
            if (entry.empty())
            {
                continue;
            }

            const auto fail = [&] { throw std::invalid_argument("Invalid SOGEN_INJECT_INPUT entry: " + std::string(entry)); };

            const auto at = entry.find('@');
            const auto colon = entry.find(':', at);
            if (at == std::string_view::npos || colon == std::string_view::npos)
            {
                fail();
            }

            scheduled_input input{};
            input.window_class = u8_to_u16(std::string(entry.substr(0, at)));
            input.delay =
                std::chrono::milliseconds(static_cast<int64_t>(std::stod(std::string(entry.substr(at + 1, colon - at - 1))) * 1000.0));

            const auto action = entry.substr(colon + 1);
            if (action.starts_with("key="))
            {
                const auto name = action.substr(4);
                input.type = scheduled_input::kind::key;
                if (const auto named = named_keys.find(name); named != named_keys.end())
                {
                    input.virtual_key = named->second;
                }
                else if (const auto stroke =
                             name.size() == 1 ? keyboard_layout::key_stroke_for_character(static_cast<char16_t>(name[0])) : std::nullopt)
                {
                    input.virtual_key = stroke->vk;
                }
                else
                {
                    fail();
                }
            }
            else if (action.starts_with("text="))
            {
                input.type = scheduled_input::kind::text;
                input.text = u8_to_u16(std::string(action.substr(5)));
            }
            else if (action == "text-file")
            {
                const auto* path = std::getenv("SOGEN_INJECT_TEXT_FILE");
                if (!path)
                {
                    throw std::invalid_argument("SOGEN_INJECT_INPUT text-file needs SOGEN_INJECT_TEXT_FILE");
                }

                std::vector<std::byte> content{};
                if (!utils::io::read_file(path, &content))
                {
                    throw std::invalid_argument("SOGEN_INJECT_TEXT_FILE can not be read");
                }

                input.type = scheduled_input::kind::text;
                input.text = u8_to_u16(std::string(reinterpret_cast<const char*>(content.data()), content.size()));
                std::erase_if(input.text, [](const char16_t c) { return c == u'\r' || c == u'\n'; });
            }
            else if (std::sscanf(std::string(action).c_str(), "%d,%d", &input.x, &input.y) == 2)
            {
                input.type = scheduled_input::kind::click;
            }
            else
            {
                fail();
            }

            this->scheduled_inputs_.push_back(std::move(input));
        }
    }

    void windows_emulator::append_key_stroke(std::vector<keyboard_input_request>& events, const uint16_t virtual_key, const bool shift)
    {
        const auto key_event = [&](const uint32_t message, const uint16_t vk) {
            uint64_t l_param = 1 | (static_cast<uint64_t>(keyboard_layout::scan_code_for_virtual_key(vk)) << 16);
            if (message == WM_KEYUP)
            {
                l_param |= (1ull << 30) | (1ull << 31);
            }

            events.push_back({.message = message, .w_param = vk, .l_param = l_param});
        };

        if (shift)
        {
            key_event(WM_KEYDOWN, keyboard_layout::vk_shift);
        }

        key_event(WM_KEYDOWN, virtual_key);
        key_event(WM_KEYUP, virtual_key);

        if (shift)
        {
            key_event(WM_KEYUP, keyboard_layout::vk_shift);
        }
    }

    void windows_emulator::deliver_keyboard_input(const hwnd window, const std::span<const keyboard_input_request> events)
    {
        if (this->process.focus_window != 0 && syscalls::is_foreign_window_handle(this->process, this->process.focus_window))
        {
            this->forward_keyboard_input(events);
            return;
        }

        for (const auto& input : events)
        {
            this->deliver_ui_event(ui_event{.window = window, .message = input.message, .wParam = input.w_param, .lParam = input.l_param});
        }
    }

    void windows_emulator::forward_keyboard_input(const std::span<const keyboard_input_request> events)
    {
        const auto focus = this->process.focus_window;
        const auto response = syscalls::forward_window_control(*this, window_control_op::keyboard_input, focus, 0, std::as_bytes(events));
        if (response && static_cast<NTSTATUS>(response->status) == STATUS_SUCCESS)
        {
            return;
        }

        this->log.warn("Keyboard input for foreign focus window 0x%llx was not delivered (%s)\n", static_cast<unsigned long long>(focus),
                       response ? "rejected" : "no owner answered");
        this->process.focus_window = 0;
    }

    void windows_emulator::inject_scheduled_input()
    {
        for (auto& input : this->scheduled_inputs_)
        {
            if (input.delivered)
            {
                continue;
            }

            const window* target = nullptr;
            for (const auto& win : this->process.windows | std::views::values)
            {
                if ((win.style & WS_VISIBLE) != 0 &&
                    (win.class_name == input.window_class || normalize_builtin_window_class_name(win.class_name) == input.window_class))
                {
                    target = &win;
                }
            }

            if (!target)
            {
                return;
            }

            const auto now = std::chrono::steady_clock::now();
            if (!input.window_seen || input.seen_window != target->handle)
            {
                input.window_seen = now;
                input.seen_window = target->handle;
            }

            if (now - *input.window_seen < input.delay)
            {
                return;
            }

            input.delivered = true;
            const auto window = target->handle;
            const auto class_name = u16_to_u8(input.window_class);

            switch (input.type)
            {
            case scheduled_input::kind::click: {
                const auto point = pack_point(input.x, input.y);
                this->log.info("[inject-input] click at %d,%d in window 0x%llx (%s)\n", input.x, input.y,
                               static_cast<unsigned long long>(window), class_name.c_str());

                this->deliver_ui_event(ui_event{.window = window, .message = WM_MOUSEMOVE, .wParam = 0, .lParam = point});
                this->deliver_ui_event(ui_event{.window = window, .message = WM_LBUTTONDOWN, .wParam = MK_LBUTTON, .lParam = point});
                this->deliver_ui_event(ui_event{.window = window, .message = WM_LBUTTONUP, .wParam = 0, .lParam = point});
                break;
            }

            case scheduled_input::kind::key: {
                this->log.info("[inject-input] key 0x%x in window 0x%llx (%s), focus 0x%llx\n", input.virtual_key,
                               static_cast<unsigned long long>(window), class_name.c_str(),
                               static_cast<unsigned long long>(this->process.focus_window));
                std::vector<keyboard_input_request> events{};
                append_key_stroke(events, input.virtual_key, false);
                this->deliver_keyboard_input(window, events);
                break;
            }

            case scheduled_input::kind::text: {
                size_t typed = 0;
                std::vector<keyboard_input_request> events{};
                for (const auto character : input.text)
                {
                    if (const auto stroke = keyboard_layout::key_stroke_for_character(character))
                    {
                        append_key_stroke(events, stroke->vk, stroke->shift);
                        ++typed;
                    }
                }

                this->deliver_keyboard_input(window, events);

                this->log.info("[inject-input] typed %zu of %zu characters in window 0x%llx (%s), focus 0x%llx\n", typed, input.text.size(),
                               static_cast<unsigned long long>(window), class_name.c_str(),
                               static_cast<unsigned long long>(this->process.focus_window));
                break;
            }
            }

            return;
        }
    }

    void windows_emulator::deliver_keyboard_input_from_parent(const hwnd window, const std::span<const keyboard_input_request> events)
    {
        this->deliver_keyboard_input(window, events);
    }

    void windows_emulator::take_focus_from_parent(const hwnd window, const hwnd previous_focus)
    {
        const auto previous = this->process.focus_window;
        this->process.focus_window = window;

        const auto post_focus_message = [&](const hwnd target, const uint32_t message, const hwnd other) {
            const auto* win = this->process.windows.get(target);
            auto* thread = win ? get_thread_by_id(this->process, win->thread_id) : nullptr;
            if (thread)
            {
                thread->post_message(*this, msg{.window = target, .message = message, .wParam = other, .lParam = 0});
            }
        };

        if (previous != 0 && previous != window)
        {
            post_focus_message(previous, WM_KILLFOCUS, window);
        }

        if (previous != window)
        {
            post_focus_message(window, WM_SETFOCUS, previous != 0 ? previous : previous_focus);
        }
    }

    void windows_emulator::release_focus_to_parent(const hwnd window, const hwnd new_focus)
    {
        if (this->process.focus_window != window)
        {
            return;
        }

        this->process.focus_window = 0;

        const auto* win = this->process.windows.get(window);
        if (auto* thread = win ? get_thread_by_id(this->process, win->thread_id) : nullptr)
        {
            thread->post_message(*this, msg{.window = window, .message = WM_KILLFOCUS, .wParam = new_focus, .lParam = 0});
        }
    }

    void windows_emulator::deliver_ui_event(const ui_event& queued_event)
    {
        auto event = this->resolve_injected_event(queued_event);
        if (is_keyboard_message(event.message) && this->process.focus_window != 0 && this->process.focus_window != event.window)
        {
            const auto focus = this->process.focus_window;
            if (syscalls::is_foreign_window_handle(this->process, focus))
            {
                const keyboard_input_request input{.message = event.message, .w_param = event.wParam, .l_param = event.lParam};
                this->forward_keyboard_input(std::span{&input, 1});
                return;
            }

            if (this->process.windows.get(focus))
            {
                event.window = focus;
            }
        }

        if (event.message == WM_SIZE)
        {
            this->resize_window_from_host(event.window, event.wParam == SIZE_MAXIMIZED, point_x(event.lParam), point_y(event.lParam));
            return;
        }

        if (event.message == WM_MOVE)
        {
            const auto [x, y] = event.host_position.value_or(std::pair{point_x(event.lParam), point_y(event.lParam)});
            this->move_window_from_host(event.window, x, y);
            return;
        }

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

        const auto previous_foreground = this->process.foreground_window;

        msg m{};
        m.window = event.window;
        m.message = event.message;
        m.wParam = event.wParam;
        m.lParam = event.lParam;

        if (event.injected && is_keyboard_message(event.message) && thread->focus_window != 0)
        {
            if (const auto* focus = this->process.windows.get(thread->focus_window); focus && focus->thread_id == win->thread_id)
            {
                m.window = thread->focus_window;
            }
        }

        if (is_pointer_message(event.message))
        {
            // Track the cursor in screen coordinates (client origin + client-local position) so GetCursorPos
            // reflects it, and treat the window the user is pointing at as the foreground one.
            const auto origin = get_client_origin_relative_to_ancestor(this->process, event.window, 0);
            auto l_param = event.lParam;
            const auto [new_cursor_x, new_cursor_y] = this->cursor_after_pointer_event(event);
            int32_t raw_dx = new_cursor_x - this->process.cursor_x;
            int32_t raw_dy = new_cursor_y - this->process.cursor_y;
            if (event.relative_motion || event.cursor_target)
            {
                // Like a ClipCursor-confined cursor on Windows: the position stops at the clip rectangle while the
                // raw motion keeps flowing.
                if (event.relative_motion)
                {
                    raw_dx = event.relative_motion->first;
                    raw_dy = event.relative_motion->second;
                }
                if (origin)
                {
                    l_param = pack_point(new_cursor_x - origin->x, new_cursor_y - origin->y);
                }
            }

            // A game using raw input for mouse-look reads relative deltas from WM_INPUT, not WM_MOUSEMOVE.
            // Synthesize that delta from the change in tracked cursor position before updating it. A
            // SetCursorPos recenter pre-updates cursor_x/y, so the warp-echo motion event yields a zero
            // delta -- no spurious look -- while genuine motion between recenters produces the real delta.
            if (this->process.raw_mouse_registered)
            {
                if (event.message == WM_MOUSEMOVE)
                {
                    if (raw_dx != 0 || raw_dy != 0)
                    {
                        this->deliver_raw_mouse_input(raw_dx, raw_dy, 0);
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
            this->set_foreground_window(event.window);

            const auto target = route_pointer(this->process, event.window, point_x(l_param), point_y(l_param));
            if (!is_mouse_wheel_message(event.message) &&
                this->forward_pointer_input_to_foreign_children(target.window, event.message, event.wParam, target.x, target.y))
            {
                return;
            }

            m.window = target.window;
            // WM_MOUSEWHEEL/WM_MOUSEHWHEEL carry screen coordinates in lParam, unlike button/move
            // messages which carry client coordinates. Keep the routed target but preserve screen coords.
            m.lParam = is_mouse_wheel_message(event.message) ? pack_point(new_cursor_x, new_cursor_y) : pack_point(target.x, target.y);
        }
        else if ((event.message == WM_ACTIVATE && event.wParam != 0) || event.message == WM_SETFOCUS)
        {
            // The window just became active/focused: make it the foreground window so polling APIs
            // (GetForegroundWindow/GetActiveWindow) agree with the activation the game just received.
            this->set_foreground_window(event.window);
        }
        else if ((event.message == WM_ACTIVATE && event.wParam == 0) && this->process.foreground_window == event.window)
        {
            this->set_foreground_window(0);
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
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            this->update_key_state(event);
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

        if (event.injected && is_key_down_message(event.message))
        {
            const auto virtual_key = static_cast<uint32_t>(event.wParam & 0xFF);
            const auto character =
                virtual_key == VK_PACKET ? event.injected_character : key_character(virtual_key, this->process.key_state);
            if (character != 0)
            {
                msg character_message = m;
                character_message.message = event.message == WM_SYSKEYDOWN ? WM_SYSCHAR : WM_CHAR;
                character_message.wParam = character;
                thread->post_message(*this, character_message, true);
            }
        }

        if (event.message == WM_CLOSE || event.message == WM_COMMAND || is_key_down_message(event.message) ||
            is_mouse_button_message(event.message) || is_mouse_wheel_message(event.message))
        {
            this->wake_thread(*thread);
        }

        if (this->process.foreground_window != previous_foreground)
        {
            this->apply_cursor_capture();
        }
    }

    void windows_emulator::move_window_from_host(const hwnd window_handle, const int32_t x, const int32_t y)
    {
        auto* win = this->process.windows.get(window_handle);
        if (!win || (win->x == x && win->y == y))
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, win->thread_id);
        if (!thread)
        {
            return;
        }

        win->x = x;
        win->y = y;
        win->sync_guest_rects();

        msg m{};
        m.window = window_handle;
        m.message = WM_MOVE;
        m.lParam = pack_point(win->client_x(), win->client_y());
        thread->post_message(*this, m, true);
        this->wake_thread(*thread);
    }

    void windows_emulator::resize_window_from_host(const hwnd window_handle, const bool maximized, const int32_t client_width,
                                                   const int32_t client_height)
    {
        auto* win = this->process.windows.get(window_handle);
        if (!win)
        {
            return;
        }

        const bool was_maximized = (win->style & WS_MAXIMIZE) != 0;
        if (win->client_width() == client_width && win->client_height() == client_height && was_maximized == maximized)
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, win->thread_id);
        if (!thread)
        {
            return;
        }

        const auto insets = win->nonclient_insets();
        win->width = client_width + insets.left + insets.right;
        win->height = client_height + insets.top + insets.bottom;
        win->style = maximized ? (win->style | WS_MAXIMIZE) : (win->style & ~WS_MAXIMIZE);
        win->guest.access([&](USER_WINDOW& guest_win) { guest_win.dwStyle = win->style; });
        win->sync_guest_rects();

        win->update_rect = RECT{.left = 0, .top = 0, .right = win->client_width(), .bottom = win->client_height()};
        win->update_pending = true;
        win->erase_pending = true;
        thread->mark_queue_status_changed(QS_PAINT);

        msg m{};
        m.window = window_handle;
        m.message = WM_SIZE;
        m.wParam = maximized ? SIZE_MAXIMIZED : SIZE_RESTORED;
        m.lParam = pack_point(win->client_width(), win->client_height());
        thread->post_message(*this, m, true);
        this->wake_thread(*thread);
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

    void windows_emulator::request_guest_event_signal(const handle event_handle, const uint32_t maximum_pending)
    {
        {
            const std::lock_guard lock{this->pending_event_signals_mutex_};
            auto& pending = this->pending_event_signals_[event_handle.bits];
            pending = std::min(pending + 1, maximum_pending);
        }

        this->credit_idle_wakeup();
    }

    void windows_emulator::apply_pending_guest_event_signals()
    {
        this->kernel_lock_.assert_held();

        const std::lock_guard lock{this->pending_event_signals_mutex_};

        for (auto it = this->pending_event_signals_.begin(); it != this->pending_event_signals_.end();)
        {
            auto* entry = this->process.events.get(make_handle(it->first));
            if (!entry)
            {
                it = this->pending_event_signals_.erase(it);
                continue;
            }

            if (!entry->is_signaled())
            {
                entry->set_signaled(true);
                if (--it->second == 0)
                {
                    it = this->pending_event_signals_.erase(it);
                    continue;
                }
            }

            ++it;
        }
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
        {
            const std::lock_guard lock{this->idle_wakeup_mutex_};
            this->should_stop = true;
        }

        this->idle_wakeup_cv_.notify_all();
        this->thread_registers_saved_.notify_all();

        for (uint32_t i = 0; i < this->vcpu_count_; ++i)
        {
            // A backend whose stop is not thread-safe races with its own execution when stopped from another thread,
            // such as a Python thread timing out a run. Its vCPU picks up should_stop at the next time-slice yield.
            auto& vcpu = this->vcpu(i);
            if (vcpu.cpu.is_stop_thread_safe() || vcpu.runner.load() == std::this_thread::get_id())
            {
                vcpu.cpu.stop();
            }
        }
    }

    void windows_emulator::wake_idle_vcpu()
    {
        if (this->idle_vcpus_.load() != 0)
        {
            this->credit_idle_wakeup();
        }
    }

    void windows_emulator::credit_idle_wakeup()
    {
        {
            const std::lock_guard lock{this->idle_wakeup_mutex_};
            this->pending_idle_wakeups_ = std::min(this->pending_idle_wakeups_ + 1, this->vcpu_count_);
        }

        this->idle_wakeup_cv_.notify_one();
    }

    bool windows_emulator::wait_for_idle_wakeup(const std::chrono::milliseconds timeout)
    {
        std::unique_lock lock{this->idle_wakeup_mutex_};
        this->idle_wakeup_cv_.wait_for(lock, timeout, [this] { return this->pending_idle_wakeups_ > 0 || this->should_stop; });

        if (this->pending_idle_wakeups_ == 0)
        {
            return false;
        }

        --this->pending_idle_wakeups_;
        return true;
    }

    void windows_emulator::register_factories(utils::buffer_deserializer& buffer)
    {
        buffer.register_factory<memory_manager_wrapper>([this] {
            return memory_manager_wrapper{this->memory}; //
        });

        buffer.register_factory<module_manager_wrapper>([this] {
            return module_manager_wrapper{this->mod_manager}; //
        });

        buffer.register_factory<guest_machine_wrapper>([this] {
            return guest_machine_wrapper{this->emu()}; //
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

        buffer.register_factory<file_system_wrapper>([this] {
            return file_system_wrapper{this->file_sys}; //
        });

        buffer.register_factory<window>([this] {
            return window{this->emu()}; //
        });

        buffer.register_factory<menu>([this] { return menu{this->emu()}; });
        buffer.register_factory<accelerator_table>([this] { return accelerator_table{this->emu()}; });
    }

    void windows_emulator::request_suspend()
    {
        const std::scoped_lock lock(this->kernel_lock_);
        if (std::ranges::any_of(this->process.devices | std::views::values,
                                [](const io_device_container& device) { return device.suspends_at_frame_end(); }))
        {
            this->suspend_requested_ = true;
            return;
        }

        this->stop();
    }

    bool windows_emulator::take_suspend_request()
    {
        return this->suspend_requested_.exchange(false);
    }

    void windows_emulator::cancel_suspend_request()
    {
        this->suspend_requested_ = false;
    }

    void windows_emulator::ensure_serialization_support() const
    {
        for (const auto& device : this->process.devices | std::views::values)
        {
            if (!device.supports_serialization())
            {
                throw std::runtime_error("The device " + u16_to_u8(device.get_device_path()) + " does not support snapshots");
            }
        }
    }

    void windows_emulator::prepare_serialization()
    {
        this->ensure_serialization_support();
        for (auto& device : this->process.devices | std::views::values)
        {
            device.prepare_serialization(*this);
        }
    }

    void windows_emulator::serialize(utils::buffer_serializer& buffer) const
    {
        this->ensure_serialization_support();

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
        this->process.section_views.serialize(buffer, this->memory);
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

        this->process.prepare_for_state_restore(*this);
        this->memory.unmap_all_memory();
        this->clear_section_first_execution_hooks();
        this->ui().reset();
        this->audio().stop();
        this->console().reset();

        // Match raw serialize() above; do not use backend snapshot mode here.
        this->emu().deserialize_state(buffer, false);
        this->memory.deserialize_memory_state(buffer, false);
        this->mod_manager.deserialize(buffer);
        this->install_section_first_execution_hooks();
        this->dispatcher.deserialize(buffer);
        emulator_thread* restored_thread{};
        this->process.deserialize(buffer, restored_thread);
        this->vcpus_[0]->set_active_thread(restored_thread);
        this->process.section_views.deserialize(buffer);
        this->process.restore_after_state_restore(*this);

        // A windows_emulator constructed as a pure deserialize() target (create_empty_emulator) never ran
        // map_main_modules, so the kernelbase.dll NLS-cache resolver was never registered.
        this->mod_manager.ensure_kernelbase_nls_cache_hook(this->process);
        this->ui().set_cursor_visibility(this->process.cursor_show_count >= 0 && this->process.cursor_shape_visible);
        this->apply_cursor_capture();
        this->release_held_input();
    }

    // Nothing is held when a saved process continues, so what was held when it was saved is released, the way the guest
    // would have seen it happen.
    void windows_emulator::release_held_input()
    {
        for (uint32_t key = 0; key < this->process.key_state.size(); ++key)
        {
            if ((this->process.key_state[key] & 0x80) == 0)
            {
                continue;
            }

            if (key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON || key == VK_XBUTTON1 || key == VK_XBUTTON2)
            {
                this->process.key_state[key] = 0;
                continue;
            }

            const bool alt_held = (this->process.key_state[VK_MENU] & 0x80) != 0;
            constexpr uint64_t release_flags = 0xC0000001;
            this->handle_ui_event(ui_event{
                .window = this->process.foreground_window,
                .message = static_cast<uint32_t>(alt_held ? WM_SYSKEYUP : WM_KEYUP),
                .wParam = key,
                .lParam = release_flags | (static_cast<uint64_t>(vk_to_scan_code(static_cast<uint16_t>(key))) << 16),
            });

            // Delivering the release does not clear the key when there is no window to deliver it to.
            this->process.key_state[key] &= 0x01;
        }
    }

    uint64_t windows_emulator::highest_user_address() const
    {
        if (!this->process.is_wow64_process)
        {
            return MAX_ALLOCATION_END_EXCL - 1;
        }
        const auto* executable = this->mod_manager.executable;
        return (executable && (executable->characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE)) ? 0xFFFEFFFF : 0x7FFEFFFF;
    }

    // The guest's ClipCursor is process-wide; the host confines whichever window is in the foreground, in relative mode while
    // the guest cursor is hidden.
    void windows_emulator::apply_cursor_capture()
    {
        std::optional<ui_rect> clip{};
        if (const auto& rect = this->process.cursor_clip)
        {
            clip = ui_rect{.left = rect->left, .top = rect->top, .right = rect->right, .bottom = rect->bottom};
        }

        // SDL's relative mode pins the cursor to a pixel in the window's center but may leave it visible. Only raw motion
        // reaches the guest then, just as with a hidden cursor.
        const bool hidden = this->process.cursor_show_count < 0 || !this->process.cursor_shape_visible;
        const bool pinned = clip && clip->right - clip->left <= 2 && clip->bottom - clip->top <= 2;
        this->ui().set_cursor_clip(this->process.foreground_window, clip, clip.has_value() && (hidden || pinned));
    }

    void windows_emulator::save_snapshot()
    {
        this->prepare_serialization();

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
        this->process.section_views.serialize(buffer, this->memory);

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

        this->process.prepare_for_state_restore(*this);
        this->memory.unmap_all_memory();
        this->clear_section_first_execution_hooks();
        this->ui().reset();
        this->audio().stop();
        this->console().reset();

        this->emu().deserialize_state(buffer, false);
        this->memory.deserialize_memory_state(buffer, false);
        this->mod_manager.deserialize(buffer);
        this->install_section_first_execution_hooks();
        this->dispatcher.deserialize(buffer);
        emulator_thread* restored_thread{};
        this->process.deserialize(buffer, restored_thread);
        this->vcpus_[0]->set_active_thread(restored_thread);
        this->process.section_views.deserialize(buffer);
        this->process.restore_after_state_restore(*this);

        // A windows_emulator constructed as a pure deserialize() target (create_empty_emulator) never ran
        // map_main_modules, so the kernelbase.dll NLS-cache resolver was never registered.
        this->mod_manager.ensure_kernelbase_nls_cache_hook(this->process);
        this->ui().set_cursor_visibility(this->process.cursor_show_count >= 0 && this->process.cursor_shape_visible);
        this->apply_cursor_capture();
    }

} // namespace sogen
