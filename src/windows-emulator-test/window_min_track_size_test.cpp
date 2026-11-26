#include "std_include.hpp"

namespace sogen::syscalls
{
    uint16_t handle_NtUserRegisterClassExWOW(const syscall_context& c, emulator_object<EMU_WNDCLASSEX> wnd_class_ex,
                                             emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_name,
                                             emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> class_version,
                                             emulator_object<CLSMENUNAME<EmulatorTraits<Emu64>>> class_menu_name, DWORD function_id,
                                             DWORD flags, emulator_pointer wow);
    hwnd handle_NtUserCreateWindowEx(const syscall_context& c, DWORD ex_style, emulator_object<LARGE_STRING> class_name,
                                     emulator_object<LARGE_STRING> cls_version, emulator_object<LARGE_STRING> window_name, DWORD style,
                                     int x, int y, int width, int height, hwnd parent, hmenu menu, hinstance instance, pointer l_param,
                                     DWORD flags, pointer acbi_buffer);
    BOOL handle_NtUserMoveWindow(const syscall_context& c, hwnd hwnd, int x, int y, int width, int height, BOOL repaint);
    BOOL handle_NtUserSetWindowPos(const syscall_context& c, hwnd hWnd, hwnd hwnd_insert_after, int x, int y, int cx, int cy, UINT flags);
    BOOL completion_NtUserSetWindowPos(const syscall_context& c, hwnd hWnd, hwnd hwnd_insert_after, int x, int y, int cx, int cy,
                                       UINT flags);
}

namespace sogen::test
{
    namespace
    {
        constexpr std::u16string_view class_name = u"SogenMinTrackTestClass";
        constexpr uint32_t unicode_class_flags = 0xC0;
        constexpr uint32_t overlapped_window = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        constexpr UINT move_flags = SWP_NOZORDER | SWP_NOACTIVATE;

        enum class parent_kind
        {
            none,
            child_of_popup,
            message_only,
        };

        struct create_case
        {
            std::string_view name;
            uint32_t style;
            uint32_t ex_style;
            parent_kind parent;
            int x;
            int y;
            int width;
            int height;
            int expected_width;
            int expected_height;
        };

        // Sizes GetWindowRect reports after CreateWindowEx on Windows 11 build 26300 (x64 and x86 identical).
        constexpr std::array create_cases{
            create_case{"overlapped_0x0", overlapped_window, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"overlapped_1x1", overlapped_window, 0, parent_kind::none, 0, 0, 1, 1, 136, 39},
            create_case{"overlapped_10x10", overlapped_window, 0, parent_kind::none, 0, 0, 10, 10, 136, 39},
            create_case{"overlapped_100x100", overlapped_window, 0, parent_kind::none, 0, 0, 100, 100, 136, 100},
            create_case{"overlapped_neg_w", overlapped_window, 0, parent_kind::none, 0, 0, -5, -5, 136, 39},
            create_case{"overlapped_at_50_60_0x0", overlapped_window, 0, parent_kind::none, 50, 60, 0, 0, 136, 39},
            create_case{"caption_only_0x0", WS_CAPTION, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"caption_sysmenu_0x0", WS_CAPTION | WS_SYSMENU, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"thickframe_only_0x0", WS_THICKFRAME, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"border_only_0x0", WS_BORDER, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"ov_dlgframe_0x0", WS_DLGFRAME, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"ov_clientedge_0x0", WS_OVERLAPPED, WS_EX_CLIENTEDGE, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"ov_plain_0x0", WS_OVERLAPPED, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"ov_minbox_0x0", WS_OVERLAPPED | WS_MINIMIZEBOX, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"popup_0x0", WS_POPUP, 0, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_cap_0x0", WS_POPUP | WS_CAPTION, 0, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_dlgframe_0x0", WS_POPUP | WS_DLGFRAME, 0, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_border_0x0", WS_POPUP | WS_BORDER, 0, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_clientedge_0x0", WS_POPUP, WS_EX_CLIENTEDGE, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_dlgmodalframe_0x0", WS_POPUP, WS_EX_DLGMODALFRAME, parent_kind::none, 0, 0, 0, 0, 0, 0},
            create_case{"popup_thick_border_0x0", WS_POPUP | WS_THICKFRAME | WS_BORDER, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"popup_thick_dlgframe_0x0", WS_POPUP | WS_THICKFRAME | WS_DLGFRAME, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"popup_cap_thick_0x0", WS_POPUP | WS_CAPTION | WS_THICKFRAME, 0, parent_kind::none, 0, 0, 0, 0, 136, 39},
            create_case{"popup_cap_thick_sys_0x0", WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 0, parent_kind::none, 0, 0, 0, 0,
                        136, 39},
            create_case{"child_0x0", WS_CHILD, 0, parent_kind::child_of_popup, 0, 0, 0, 0, 0, 0},
            create_case{"child_cap_0x0", WS_CHILD | WS_CAPTION, 0, parent_kind::child_of_popup, 0, 0, 0, 0, 0, 0},
            create_case{"child_border_0x0", WS_CHILD | WS_BORDER, 0, parent_kind::child_of_popup, 0, 0, 0, 0, 0, 0},
            create_case{"child_thick_border_0x0", WS_CHILD | WS_THICKFRAME | WS_BORDER, 0, parent_kind::child_of_popup, 0, 0, 0, 0, 136,
                        39},
            create_case{"child_cap_thick_0x0", WS_CHILD | WS_CAPTION | WS_THICKFRAME, 0, parent_kind::child_of_popup, 0, 0, 0, 0, 136, 39},
            create_case{"child_cap_thick_sys_0x0", WS_CHILD | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 0, parent_kind::child_of_popup, 0, 0,
                        0, 0, 136, 39},
            create_case{"msgonly_ov_0x0", overlapped_window, 0, parent_kind::message_only, 0, 0, 0, 0, 136, 39},
            create_case{"msgonly_0_0x0", 0, 0, parent_kind::message_only, 0, 0, 0, 0, 136, 39},
            create_case{"msgonly_thick_0x0", WS_THICKFRAME, 0, parent_kind::message_only, 0, 0, 0, 0, 136, 39},
            create_case{"msgonly_popup_0x0", WS_POPUP, 0, parent_kind::message_only, 0, 0, 0, 0, 0, 0},
            create_case{"child_cap_thick_5x5", WS_CHILD | WS_CAPTION | WS_THICKFRAME, 0, parent_kind::child_of_popup, 0, 0, 5, 5, 136, 39},
        };

        struct change_case
        {
            std::string_view name;
            uint32_t style;
            uint32_t ex_style;
            parent_kind parent;
            int width_to_0x0;
            int height_to_0x0;
            int width_to_5x5;
            int height_to_5x5;
        };

        // Sizes on Windows 11 build 26300 after MoveWindow(20, 30, 0, 0), SetWindowPos(40, 50, 0, 0) and SetWindowPos(10, 10, 5, 5).
        constexpr std::array change_cases{
            change_case{"ov_0x0", overlapped_window, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"ov_dlgframe_0x0", WS_DLGFRAME, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"ov_clientedge_0x0", WS_OVERLAPPED, WS_EX_CLIENTEDGE, parent_kind::none, 136, 39, 136, 39},
            change_case{"ov_plain_0x0", WS_OVERLAPPED, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"popup_cap_0x0", WS_POPUP | WS_CAPTION, 0, parent_kind::none, 0, 0, 5, 5},
            change_case{"popup_dlgframe_0x0", WS_POPUP | WS_DLGFRAME, 0, parent_kind::none, 0, 0, 5, 5},
            change_case{"popup_border_0x0", WS_POPUP | WS_BORDER, 0, parent_kind::none, 0, 0, 5, 5},
            change_case{"popup_thick_border_0x0", WS_POPUP | WS_THICKFRAME | WS_BORDER, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"popup_thick_dlgframe_0x0", WS_POPUP | WS_THICKFRAME | WS_DLGFRAME, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"popup_cap_thick_0x0", WS_POPUP | WS_CAPTION | WS_THICKFRAME, 0, parent_kind::none, 136, 39, 136, 39},
            change_case{"popup_cap_thick_sys_0x0", WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 0, parent_kind::none, 136, 39, 136,
                        39},
            change_case{"child_0x0", WS_CHILD, 0, parent_kind::child_of_popup, 0, 0, 5, 5},
            change_case{"child_cap_0x0", WS_CHILD | WS_CAPTION, 0, parent_kind::child_of_popup, 0, 0, 5, 5},
            change_case{"child_border_0x0", WS_CHILD | WS_BORDER, 0, parent_kind::child_of_popup, 0, 0, 5, 5},
            change_case{"child_thick_border_0x0", WS_CHILD | WS_THICKFRAME | WS_BORDER, 0, parent_kind::child_of_popup, 136, 39, 136, 39},
            change_case{"child_cap_thick_0x0", WS_CHILD | WS_CAPTION | WS_THICKFRAME, 0, parent_kind::child_of_popup, 136, 39, 136, 39},
            change_case{"msgonly_ov_0x0", overlapped_window, 0, parent_kind::message_only, 136, 39, 136, 39},
            change_case{"msgonly_0_0x0", 0, 0, parent_kind::message_only, 136, 39, 136, 39},
            change_case{"msgonly_thick_0x0", WS_THICKFRAME, 0, parent_kind::message_only, 136, 39, 136, 39},
            change_case{"msgonly_popup_0x0", WS_POPUP, 0, parent_kind::message_only, 0, 0, 5, 5},
        };

        class WindowMinTrackSizeTest : public testing::Test
        {
          protected:
            void SetUp() override
            {
                this->emu.setup_process_if_necessary();
                this->storage = this->emu.memory.allocate_memory(0x1000, memory_permission::read_write);
                this->atom = this->register_class();
                ASSERT_NE(this->atom, 0);
                this->parent = this->create_window(WS_POPUP, 0, 0, 0, 800, 600, 0);
                ASSERT_NE(this->parent, nullptr);
            }

            uint16_t register_class()
            {
                const emulator_object<EMU_WNDCLASSEX> wnd_class{this->emu.memory, this->storage};
                const emulator_object<UNICODE_STRING<EmulatorTraits<Emu64>>> name{this->emu.memory, this->storage + 0x100};
                const emulator_object<CLSMENUNAME<EmulatorTraits<Emu64>>> class_menu_name{this->emu.memory, this->storage + 0x200};
                const auto name_buffer = this->storage + 0x300;

                wnd_class.write({.cbSize = sizeof(EMU_WNDCLASSEX),
                                 .style = 0,
                                 .lpfnWndProc = 0x140001000,
                                 .cbClsExtra = 0,
                                 .cbWndExtra = 0,
                                 .hInstance = 0x140000000,
                                 .hIcon = 0,
                                 .hCursor = 0,
                                 .hbrBackground = 0,
                                 .lpszMenuName = 0,
                                 .lpszClassName = 0,
                                 .hIconSm = 0});
                this->emu.memory.write_memory(name_buffer, class_name.data(), class_name.size() * sizeof(char16_t));
                name.write({.Length = static_cast<USHORT>(class_name.size() * sizeof(char16_t)),
                            .MaximumLength = static_cast<USHORT>(class_name.size() * sizeof(char16_t)),
                            .Buffer = name_buffer});
                class_menu_name.write({});

                return syscalls::handle_NtUserRegisterClassExWOW(this->context(), wnd_class, name, {this->emu.memory}, class_menu_name, 0,
                                                                 unicode_class_flags, 0);
            }

            syscall_context context()
            {
                return {.win_emu = this->emu, .emu = this->emu.vcpu(0).cpu, .vcpu = this->emu.vcpu(0), .proc = this->emu.process};
            }

            // The creation messages run as guest callbacks; the window already holds its final geometry before the first one.
            window* create_window(const uint32_t style, const uint32_t ex_style, const int x, const int y, const int width,
                                  const int height, const hwnd parent_handle)
            {
                std::set<std::remove_cvref_t<decltype(this->emu.process.windows.begin()->first)>> existing{};
                for (const auto& [index, win] : this->emu.process.windows)
                {
                    existing.insert(index);
                }

                (void)syscalls::handle_NtUserCreateWindowEx(this->context(), ex_style, {this->emu.memory, this->atom}, {this->emu.memory},
                                                            {this->emu.memory}, style, x, y, width, height, parent_handle, 0, 0x140000000,
                                                            0, 0, 0);

                for (auto& [index, win] : this->emu.process.windows)
                {
                    if (!existing.contains(index))
                    {
                        return &win;
                    }
                }

                return nullptr;
            }

            hwnd parent_handle(const parent_kind kind) const
            {
                switch (kind)
                {
                case parent_kind::child_of_popup:
                    return this->parent->handle;
                case parent_kind::message_only:
                    return EMU_HWND_MESSAGE;
                default:
                    return 0;
                }
            }

            window* create_window(const create_case& test)
            {
                return this->create_window(test.style, test.ex_style, test.x, test.y, test.width, test.height,
                                           this->parent_handle(test.parent));
            }

            callback_frame finish_callback()
            {
                auto& callbacks = this->emu.vcpu(0).active_thread->callback_stack;
                auto frame = std::move(callbacks.at(callbacks.size() - 1));
                callbacks.pop_back();
                frame.restore_registers(this->emu.vcpu(0).cpu);
                return frame;
            }

            void set_window_pos(const window& win, const int x, const int y, const int width, const int height, const UINT flags)
            {
                bool callback_pending{};
                {
                    auto c = this->context();
                    (void)syscalls::handle_NtUserSetWindowPos(c, win.handle, 0, x, y, width, height, flags);
                    callback_pending = c.run_callback;
                }

                while (callback_pending)
                {
                    auto frame = this->finish_callback();
                    auto c = this->context();
                    c.is_callback_completion = true;
                    c.current_completion_state = frame.state.get();
                    (void)syscalls::completion_NtUserSetWindowPos(c, win.handle, 0, x, y, width, height, flags);
                    callback_pending = c.run_callback;
                }
            }

            void resize_to_300x200(const window& win)
            {
                ASSERT_TRUE(syscalls::handle_NtUserMoveWindow(this->context(), win.handle, 20, 30, 300, 200, FALSE));
                ASSERT_EQ(300, win.width);
                ASSERT_EQ(200, win.height);
            }

            windows_emulator emu = create_sample_emulator();
            uint64_t storage{};
            uint16_t atom{};
            window* parent{};
        };
    }

    TEST_F(WindowMinTrackSizeTest, CreationClampsTheRequestedSize)
    {
        for (const auto& test : create_cases)
        {
            SCOPED_TRACE(test.name);
            const auto* win = this->create_window(test);
            ASSERT_NE(win, nullptr);
            EXPECT_EQ(test.x, win->x);
            EXPECT_EQ(test.y, win->y);
            EXPECT_EQ(test.expected_width, win->width);
            EXPECT_EQ(test.expected_height, win->height);
        }
    }

    TEST_F(WindowMinTrackSizeTest, SizeChangesClampOnlyWhenWindowPosChangingIsSent)
    {
        for (const auto& test : change_cases)
        {
            SCOPED_TRACE(test.name);
            const auto* win = this->create_window(test.style, test.ex_style, 0, 0, 300, 200, this->parent_handle(test.parent));
            ASSERT_NE(win, nullptr);
            ASSERT_NO_FATAL_FAILURE(this->resize_to_300x200(*win));

            ASSERT_TRUE(syscalls::handle_NtUserMoveWindow(this->context(), win->handle, 20, 30, 0, 0, FALSE));
            EXPECT_EQ(20, win->x);
            EXPECT_EQ(30, win->y);
            EXPECT_EQ(test.width_to_0x0, win->width);
            EXPECT_EQ(test.height_to_0x0, win->height);

            ASSERT_NO_FATAL_FAILURE(this->resize_to_300x200(*win));
            this->set_window_pos(*win, 40, 50, 0, 0, move_flags);
            EXPECT_EQ(40, win->x);
            EXPECT_EQ(50, win->y);
            EXPECT_EQ(test.width_to_0x0, win->width);
            EXPECT_EQ(test.height_to_0x0, win->height);

            ASSERT_NO_FATAL_FAILURE(this->resize_to_300x200(*win));
            this->set_window_pos(*win, 40, 50, 0, 0, move_flags | SWP_NOSENDCHANGING);
            EXPECT_EQ(40, win->x);
            EXPECT_EQ(50, win->y);
            EXPECT_EQ(0, win->width);
            EXPECT_EQ(0, win->height);

            ASSERT_NO_FATAL_FAILURE(this->resize_to_300x200(*win));
            this->set_window_pos(*win, 10, 10, 5, 5, move_flags);
            EXPECT_EQ(10, win->x);
            EXPECT_EQ(10, win->y);
            EXPECT_EQ(test.width_to_5x5, win->width);
            EXPECT_EQ(test.height_to_5x5, win->height);
        }

        auto* popup = this->create_window(WS_POPUP, 0, 0, 0, 0, 0, 0);
        ASSERT_NE(popup, nullptr);
        popup->style = WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU;

        this->set_window_pos(*popup, 0, 0, 0, 0, move_flags | SWP_NOSIZE | SWP_NOMOVE | SWP_FRAMECHANGED);
        EXPECT_EQ(0, popup->width);
        EXPECT_EQ(0, popup->height);

        this->set_window_pos(*popup, 7, 8, 0, 0, move_flags | SWP_NOSIZE);
        EXPECT_EQ(7, popup->x);
        EXPECT_EQ(8, popup->y);
        EXPECT_EQ(0, popup->width);
        EXPECT_EQ(0, popup->height);
    }
}
