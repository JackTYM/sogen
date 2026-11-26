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
    BOOL handle_NtUserGetWindowPlacement(const syscall_context& c, hwnd window_handle, emulator_pointer placement_address);
    BOOL handle_NtUserSetWindowPlacement(const syscall_context& c, hwnd window, emulator_pointer placement);
    BOOL completion_NtUserSetWindowPos(const syscall_context& c, hwnd hWnd, hwnd hwnd_insert_after, int x, int y, int cx, int cy,
                                       UINT flags);
    BOOL completion_NtUserShowWindow(const syscall_context& c, hwnd hwnd, LONG cmd_show);
}

namespace sogen::test
{
    namespace
    {
        constexpr std::u16string_view class_name = u"SogenPlacementTestClass";
        constexpr uint32_t unicode_class_flags = 0xC0;
        constexpr uint32_t overlapped_window = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
        constexpr DWORD error_marker = 57005;
        constexpr LONG cw_usedefault = std::numeric_limits<int32_t>::min();
        constexpr hwnd garbage_window = 0x7FFF0;

        enum class created_as
        {
            origin_0x0,
            at_50_60_300x200,
            at_100_100_300x200,
            tool_at_50_60_300x200,
        };

        struct set_case
        {
            std::string_view name;
            created_as creation;
            bool initially_visible;
            EMU_WINDOWPLACEMENT placement;
            RECT expected_window;
            bool expected_visible;
            POINT expected_min_position;
            DWORD expected_error;
        };

        // Windows 11 build 26300 captures: the window the call leaves behind, whether it is shown, and what GetWindowPlacement
        // reports afterwards.
        // clang-format off
        constexpr std::array set_cases{
            set_case{"A.ov0.v0.sc00_HIDE", created_as::origin_0x0, false, {44, 0x0, SW_HIDE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, false, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc01_SHOWNORMAL", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc04_SHOWNOACTIVATE", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNOACTIVATE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc05_SHOW", created_as::origin_0x0, false, {44, 0x0, SW_SHOW, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc08_SHOWNA", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNA, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc09_RESTORE", created_as::origin_0x0, false, {44, 0x0, SW_RESTORE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc10_SHOWDEFAULT", created_as::origin_0x0, false, {44, 0x0, SW_SHOWDEFAULT, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc11_FORCEMINIMIZE", created_as::origin_0x0, false, {44, 0x0, SW_FORCEMINIMIZE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, false, {-1, -1}, error_marker},
            set_case{"A.ov0.v0.sc99", created_as::origin_0x0, false, {44, 0x0, 99, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, false, {-1, -1}, 1449},
            set_case{"A.ov0.v1.sc00_HIDE", created_as::origin_0x0, true, {44, 0x0, SW_HIDE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, false, {-1, -1}, error_marker},
            set_case{"A.ov0.v1.sc01_SHOWNORMAL", created_as::origin_0x0, true, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"A.ov0.v1.sc99", created_as::origin_0x0, true, {44, 0x0, 99, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, 1449},
            set_case{"B.ov0.v0.sc01_SHOWNORMAL.rc_below_min_10x10", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {100, 100, 110, 110}}, {100, 100, 236, 139}, true, {-1, -1}, error_marker},
            set_case{"B.ov0.v0.sc01_SHOWNORMAL.rc_zero_size", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {100, 100, 100, 100}}, {100, 100, 236, 139}, true, {-1, -1}, error_marker},
            set_case{"B.ov0.v0.sc01_SHOWNORMAL.rc_negative", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {-50, -60, 150, 140}}, {-50, -60, 150, 140}, true, {-1, -1}, error_marker},
            set_case{"B.ov0.v0.sc01_SHOWNORMAL.rc_inverted", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {900, 700, 100, 100}}, {900, 700, 1036, 739}, true, {-1, -1}, error_marker},
            set_case{"B.ov0.v1.sc01_SHOWNORMAL.rc_all_zero", created_as::origin_0x0, true, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {0, 0, 0, 0}}, {0, 0, 136, 39}, true, {-1, -1}, error_marker},
            set_case{"B.ov300.v0.sc01_SHOWNORMAL.rc_offscreen", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {10000, 10000, 10400, 10300}}, {624, 468, 1024, 768}, true, {-1, -1}, error_marker},
            set_case{"B.tool300.v0.sc01_SHOWNORMAL.rc_offscreen", created_as::tool_at_50_60_300x200, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {10000, 10000, 10400, 10300}}, {10000, 10000, 10400, 10300}, true, {-1, -1}, error_marker},
            set_case{"C.ov300.v0.sc01_SHOWNORMAL.min500_400_noflag", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWNORMAL, {500, 400}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {500, 400}, error_marker},
            set_case{"C.ov300.v0.sc01_SHOWNORMAL.min32767_SETMINPOS", created_as::at_50_60_300x200, false, {44, 0x1, SW_SHOWNORMAL, {32767, 32767}, {-1, -1}, {100, 100, 500, 400}}, {100, 100, 500, 400}, true, {-1, -1}, error_marker},
            set_case{"H1.np.v0.SHOWNORMAL.pt0.app100_100_900_700", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {0, 0}, {0, 0}, {100, 100, 900, 700}}, {100, 100, 900, 700}, true, {0, 0}, error_marker},
            set_case{"H1.np.v0.SHOWNORMAL.pt-1.cwuse4", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {cw_usedefault, cw_usedefault, cw_usedefault, cw_usedefault}}, {0, 0, 136, 39}, true, {-1, -1}, error_marker},
            set_case{"H1.np.v0.SHOWNORMAL.pt-1.cwuse_xy_wh800x600", created_as::origin_0x0, false, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {cw_usedefault, cw_usedefault, 800, 600}}, {cw_usedefault, cw_usedefault, -2147483512, -2147483609}, true, {-1, -1}, error_marker},
            set_case{"H1.np.v1.SHOWNORMAL.pt-1.app100_100_900_700", created_as::origin_0x0, true, {44, 0x0, SW_SHOWNORMAL, {-1, -1}, {-1, -1}, {100, 100, 900, 700}}, {100, 100, 900, 700}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=0,0", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {0, 0}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {0, 0}, error_marker},
            set_case{"PT which=ptMax flags=0 val=100,100", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {-1, -1}, {100, 100}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=-1,5", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {-1, 5}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=5,-1", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {5, -1}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=-120,5", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {-120, 5}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-120, 5}, error_marker},
            set_case{"PT which=ptMin flags=0 val=-2,5", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {-2, 5}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-2, 5}, error_marker},
            set_case{"PT which=ptMin flags=0 val=5,-29", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {5, -29}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=5,-28", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {5, -28}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {5, -28}, error_marker},
            set_case{"PT which=ptMin flags=0 val=1024,5", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {1024, 5}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {1024, 5}, error_marker},
            set_case{"PT which=ptMin flags=0 val=1025,5", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {1025, 5}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
            set_case{"PT which=ptMin flags=0 val=5,768", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {5, 768}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {5, 768}, error_marker},
            set_case{"PT which=ptMin flags=0 val=5,769", created_as::at_100_100_300x200, false, {44, 0, SW_SHOWNORMAL, {5, 769}, {-1, -1}, {10, 20, 410, 320}}, {10, 20, 410, 320}, true, {-1, -1}, error_marker},
        };
        // clang-format on

        enum class pointer_mode
        {
            normal,
            null,
            one,
            kernel,
            read_only,
        };

        enum class window_choice
        {
            live,
            garbage,
            null,
        };

        struct invalid_set_case
        {
            std::string_view name;
            window_choice window;
            pointer_mode pointer;
            DWORD length;
            DWORD expected_error;
        };

        constexpr std::array invalid_set_cases{
            invalid_set_case{"D.set.len0", window_choice::live, pointer_mode::normal, 0, 87},
            invalid_set_case{"D.set.len4294967295", window_choice::live, pointer_mode::normal, 0xFFFFFFFF, 87},
            invalid_set_case{"E.set.ptr_null", window_choice::live, pointer_mode::null, 44, 998},
            invalid_set_case{"E.set.ptr_one", window_choice::live, pointer_mode::one, 44, 998},
            invalid_set_case{"E.set.ptr_kernel", window_choice::live, pointer_mode::kernel, 44, 998},
            invalid_set_case{"E.set.hwnd_garbage", window_choice::garbage, pointer_mode::normal, 44, 1400},
            invalid_set_case{"ORDER.badhwnd_goodptr_len43", window_choice::garbage, pointer_mode::normal, 43, 1400},
            invalid_set_case{"ORDER.badhwnd_goodptr_len0", window_choice::garbage, pointer_mode::normal, 0, 1400},
            invalid_set_case{"ORDER.badhwnd_nullptr", window_choice::garbage, pointer_mode::null, 44, 1400},
            invalid_set_case{"ORDER.nullhwnd_goodptr_goodlen", window_choice::null, pointer_mode::normal, 44, 1400},
            invalid_set_case{"ORDER.nullhwnd_nullptr", window_choice::null, pointer_mode::null, 44, 1400},
        };

        struct get_case
        {
            std::string_view name;
            window_choice window;
            pointer_mode pointer;
            DWORD length;
            BOOL expected_result;
            DWORD expected_error;
        };

        constexpr std::array get_cases{
            get_case{"F.get.len.shown_normal.len0", window_choice::live, pointer_mode::normal, 0, TRUE, error_marker},
            get_case{"F.get.len.shown_normal.len4294967295", window_choice::live, pointer_mode::normal, 0xFFFFFFFF, TRUE, error_marker},
            get_case{"F.get.ptr_null", window_choice::live, pointer_mode::null, 44, FALSE, 998},
            get_case{"F.get.ptr_one", window_choice::live, pointer_mode::one, 44, FALSE, 998},
            get_case{"F.get.ptr_kernel", window_choice::live, pointer_mode::kernel, 44, FALSE, 998},
            get_case{"F.get.ptr_readonly", window_choice::live, pointer_mode::read_only, 44, FALSE, 998},
            get_case{"F.get.hwnd_garbage", window_choice::garbage, pointer_mode::normal, 44, FALSE, 1400},
        };

        struct minmax_case
        {
            std::string_view name;
            created_as creation;
            bool initially_visible;
            EMU_WINDOWPLACEMENT placement;
            DWORD expected_error;
            std::optional<POINT> expected_min_position;
        };

        // clang-format off
        constexpr std::array minmax_cases{
            minmax_case{"A.ov300.v0.sc02_SHOWMINIMIZED", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWMINIMIZED, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, 87, std::nullopt},
            minmax_case{"A.ov300.v0.sc03_SHOWMAXIMIZED", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWMAXIMIZED, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, error_marker, POINT{-1, -1}},
            minmax_case{"A.ov300.v0.sc06_MINIMIZE", created_as::at_50_60_300x200, false, {44, 0x0, SW_MINIMIZE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, 87, std::nullopt},
            minmax_case{"A.ov300.v0.sc07_SHOWMINNOACTIVE", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWMINNOACTIVE, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, 87, std::nullopt},
            minmax_case{"A.ov300.v1.sc02_SHOWMINIMIZED", created_as::at_50_60_300x200, true, {44, 0x0, SW_SHOWMINIMIZED, {-1, -1}, {-1, -1}, {100, 100, 500, 400}}, error_marker, std::nullopt},
            minmax_case{"C.ov300.v0.sc03_SHOWMAXIMIZED.min500_400_noflag", created_as::at_50_60_300x200, false, {44, 0x0, SW_SHOWMAXIMIZED, {500, 400}, {-1, -1}, {100, 100, 500, 400}}, error_marker, POINT{500, 400}},
        };
        // clang-format on

        class WindowPlacementTest : public testing::Test
        {
          protected:
            void SetUp() override
            {
                this->emu.setup_process_if_necessary();
                this->emu.process.set_display_mode(this->emu, 1024, 768);
                this->storage = this->emu.memory.allocate_memory(0x1000, memory_permission::read_write);
                this->read_only_storage = this->emu.memory.allocate_memory(0x1000, memory_permission::read);
                this->atom = this->register_class();
                ASSERT_NE(this->atom, 0);
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

            window* create_window(const int x, const int y, const int width, const int height, const bool visible, const DWORD ex_style = 0)
            {
                std::set<std::remove_cvref_t<decltype(this->emu.process.windows.begin()->first)>> existing{};
                for (const auto& [index, win] : this->emu.process.windows)
                {
                    existing.insert(index);
                }

                (void)syscalls::handle_NtUserCreateWindowEx(this->context(), ex_style, {this->emu.memory, this->atom}, {this->emu.memory},
                                                            {this->emu.memory}, overlapped_window, x, y, width, height, 0, 0, 0x140000000,
                                                            0, 0, 0);

                for (auto& [index, win] : this->emu.process.windows)
                {
                    if (!existing.contains(index))
                    {
                        if (visible)
                        {
                            win.style |= WS_VISIBLE;
                        }
                        return &win;
                    }
                }

                return nullptr;
            }

            window* create_window_as(const created_as creation, const bool visible)
            {
                switch (creation)
                {
                case created_as::at_50_60_300x200:
                    return this->create_window(50, 60, 300, 200, visible);
                case created_as::at_100_100_300x200:
                    return this->create_window(100, 100, 300, 200, visible);
                case created_as::tool_at_50_60_300x200:
                    return this->create_window(50, 60, 300, 200, visible, WS_EX_TOOLWINDOW);
                default:
                    return this->create_window(0, 0, 0, 0, visible);
                }
            }

            static hwnd pick_window(const window_choice choice, const window& live)
            {
                switch (choice)
                {
                case window_choice::garbage:
                    return garbage_window;
                case window_choice::null:
                    return 0;
                default:
                    return live.handle;
                }
            }

            callback_frame finish_callback()
            {
                auto& callbacks = this->emu.vcpu(0).active_thread->callback_stack;
                auto frame = std::move(callbacks.at(callbacks.size() - 1));
                callbacks.pop_back();
                frame.restore_registers(this->emu.vcpu(0).cpu);
                return frame;
            }

            DWORD last_error()
            {
                return this->emu.vcpu(0).active_thread->teb64->read().LastErrorValue;
            }

            void reset_last_error()
            {
                this->emu.vcpu(0).active_thread->teb64->access([](TEB64& teb) { teb.LastErrorValue = error_marker; });
            }

            uint64_t placement_address(const pointer_mode mode) const
            {
                switch (mode)
                {
                case pointer_mode::null:
                    return 0;
                case pointer_mode::one:
                    return 1;
                case pointer_mode::kernel:
                    return 0xFFFF800000000000;
                case pointer_mode::read_only:
                    return this->read_only_storage;
                default:
                    return this->storage + 0x400;
                }
            }

            EMU_WINDOWPLACEMENT get_placement(const hwnd handle, BOOL* result = nullptr, const pointer_mode mode = pointer_mode::normal,
                                              const DWORD length = sizeof(EMU_WINDOWPLACEMENT))
            {
                const auto address = this->placement_address(mode);
                if (mode == pointer_mode::normal)
                {
                    EMU_WINDOWPLACEMENT input{};
                    input.length = length;
                    this->emu.memory.write_memory(address, &input, sizeof(input));
                }

                const auto got = syscalls::handle_NtUserGetWindowPlacement(this->context(), handle, address);
                if (result)
                {
                    *result = got;
                }

                EMU_WINDOWPLACEMENT placement{};
                if (got)
                {
                    this->emu.memory.read_memory(address, &placement, sizeof(placement));
                }
                return placement;
            }

            BOOL set_placement(const hwnd handle, const pointer_mode mode, const EMU_WINDOWPLACEMENT& placement)
            {
                const auto address = this->placement_address(mode);
                if (mode == pointer_mode::normal)
                {
                    this->emu.memory.write_memory(address, &placement, sizeof(placement));
                }

                BOOL result{};
                bool callback_pending{};
                {
                    auto c = this->context();
                    result = syscalls::handle_NtUserSetWindowPlacement(c, handle, address);
                    callback_pending = c.run_callback;
                }

                while (callback_pending)
                {
                    auto frame = this->finish_callback();
                    auto c = this->context();
                    c.is_callback_completion = true;
                    c.current_completion_state = frame.state.get();
                    result = frame.handler_id == callback_id::NtUserShowWindow
                                 ? syscalls::completion_NtUserShowWindow(c, handle, 0)
                                 : syscalls::completion_NtUserSetWindowPos(c, handle, 0, 0, 0, 0, 0, 0);
                    callback_pending = c.run_callback;
                }

                return result;
            }

            windows_emulator emu = create_sample_emulator();
            uint64_t storage{};
            uint64_t read_only_storage{};
            uint16_t atom{};
        };

        RECT window_rect(const window& win)
        {
            return {.left = win.x, .top = win.y, .right = win.x + win.width, .bottom = win.y + win.height};
        }

        void expect_rect_eq(const RECT& expected, const RECT& actual)
        {
            EXPECT_EQ(expected.left, actual.left);
            EXPECT_EQ(expected.top, actual.top);
            EXPECT_EQ(expected.right, actual.right);
            EXPECT_EQ(expected.bottom, actual.bottom);
        }
    }

    TEST_F(WindowPlacementTest, SetPlacesAndShowsTheWindowAndGetReportsIt)
    {
        for (const auto& test : set_cases)
        {
            SCOPED_TRACE(test.name);
            auto* win = this->create_window_as(test.creation, test.initially_visible);
            ASSERT_NE(win, nullptr);

            this->reset_last_error();
            EXPECT_TRUE(this->set_placement(win->handle, pointer_mode::normal, test.placement));
            EXPECT_EQ(test.expected_error, this->last_error());
            expect_rect_eq(test.expected_window, window_rect(*win));
            EXPECT_EQ(test.expected_visible, (win->style & WS_VISIBLE) != 0);

            const auto reported = this->get_placement(win->handle);
            EXPECT_EQ(0u, reported.flags);
            EXPECT_EQ(static_cast<DWORD>(SW_SHOWNORMAL), reported.showCmd);
            EXPECT_EQ(test.expected_min_position.x, reported.ptMinPosition.x);
            EXPECT_EQ(test.expected_min_position.y, reported.ptMinPosition.y);
            EXPECT_EQ(-1, reported.ptMaxPosition.x);
            EXPECT_EQ(-1, reported.ptMaxPosition.y);
            expect_rect_eq(test.expected_window, reported.rcNormalPosition);
        }
    }

    TEST_F(WindowPlacementTest, SetWithAMinimizeOrMaximizeCommandMovesAndShowsTheWindow)
    {
        for (const auto& test : minmax_cases)
        {
            SCOPED_TRACE(test.name);
            auto* win = this->create_window_as(test.creation, test.initially_visible);
            ASSERT_NE(win, nullptr);

            this->reset_last_error();
            EXPECT_TRUE(this->set_placement(win->handle, pointer_mode::normal, test.placement));
            EXPECT_EQ(test.expected_error, this->last_error());
            EXPECT_NE(0u, win->style & WS_VISIBLE);

            const auto reported = this->get_placement(win->handle);
            expect_rect_eq(test.placement.rcNormalPosition, reported.rcNormalPosition);
            if (test.expected_min_position)
            {
                EXPECT_EQ(test.expected_min_position->x, reported.ptMinPosition.x);
                EXPECT_EQ(test.expected_min_position->y, reported.ptMinPosition.y);
            }
        }
    }

    TEST_F(WindowPlacementTest, SetRejectsAnInvalidLengthPointerOrWindowWithoutChangingTheWindow)
    {
        for (const auto& test : invalid_set_cases)
        {
            SCOPED_TRACE(test.name);
            auto* win = this->create_window(50, 60, 300, 200, false);
            ASSERT_NE(win, nullptr);

            this->reset_last_error();
            const EMU_WINDOWPLACEMENT placement{.length = test.length,
                                                .flags = 0,
                                                .showCmd = SW_SHOWNORMAL,
                                                .ptMinPosition = {-1, -1},
                                                .ptMaxPosition = {-1, -1},
                                                .rcNormalPosition = {100, 100, 500, 400}};
            EXPECT_FALSE(this->set_placement(pick_window(test.window, *win), test.pointer, placement));
            EXPECT_EQ(test.expected_error, this->last_error());
            expect_rect_eq({50, 60, 350, 260}, window_rect(*win));
            EXPECT_EQ(0u, win->style & WS_VISIBLE);
        }
    }

    TEST_F(WindowPlacementTest, GetIgnoresTheLengthFieldAndFailsOnlyForAnUnwritableBufferOrWindow)
    {
        for (const auto& test : get_cases)
        {
            SCOPED_TRACE(test.name);
            auto* win = this->create_window(50, 60, 300, 200, true);
            ASSERT_NE(win, nullptr);
            ASSERT_TRUE(syscalls::handle_NtUserMoveWindow(this->context(), win->handle, 120, 130, 400, 300, FALSE));

            this->reset_last_error();
            BOOL result{};
            const auto placement = this->get_placement(pick_window(test.window, *win), &result, test.pointer, test.length);
            EXPECT_EQ(test.expected_result, result);
            EXPECT_EQ(test.expected_error, this->last_error());
            if (test.expected_result)
            {
                EXPECT_EQ(sizeof(EMU_WINDOWPLACEMENT), placement.length);
                expect_rect_eq({120, 130, 520, 430}, placement.rcNormalPosition);
            }
        }
    }
}
