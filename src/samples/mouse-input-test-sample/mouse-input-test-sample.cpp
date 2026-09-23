#include <windows.h>
#include <windowsx.h>

#include <array>
#include <cstdio>

#include <dxgk_command_protocol.hpp>

// ---------------------------------------------------------------------------
// D3DKMT kernel-facing struct definitions.
//
// Copied verbatim from native-gpu-clear-sample.cpp -- this sample only runs as a 32-bit
// (WoW64) guest, and these are the packed all-32-bit wire layouts real wow64win.dll thunks
// produce for these specific calls (SUBMITCOMMAND/PRESENT are the two exceptions that are
// already unconditionally 32-bit even on a 64-bit guest -- see the original file's own
// comment for the full explanation). Nothing about the mouse-input additions below changes
// any of this.
// ---------------------------------------------------------------------------

namespace d3dkmt
{

    struct OPENADAPTERFROMLUID
    {
        unsigned int LuidLow{};
        int LuidHigh{};
        unsigned int hAdapter{};
    };
    static_assert(sizeof(OPENADAPTERFROMLUID) == 12);

    struct CREATEDEVICE
    {
        unsigned int hAdapter{};
        unsigned int Flags{};
        unsigned int hDevice{};
        unsigned int pCommandBuffer{};
        unsigned int CommandBufferSize{};
        unsigned int pAllocationList{};
        unsigned int AllocationListSize{};
        unsigned int pPatchLocationList{};
        unsigned int PatchLocationListSize{};
    };
    static_assert(sizeof(CREATEDEVICE) == 0x24);

    struct CREATECONTEXT
    {
        unsigned int hDevice{};
        unsigned int NodeOrdinal{};
        unsigned int EngineAffinity{};
        unsigned int Flags{};
        unsigned int pPrivateDriverData{};
        unsigned int PrivateDriverDataSize{};
        unsigned int ClientHint{};
        unsigned int hContext{};
        unsigned int pCommandBuffer{};
        unsigned int CommandBufferSize{};
        unsigned int pAllocationList{};
        unsigned int AllocationListSize{};
        unsigned int pPatchLocationList{};
        unsigned int PatchLocationListSize{};
        unsigned long long CommandBuffer{}; // real Windows keeps this field 64-bit even on WoW64
    };
    static_assert(sizeof(CREATECONTEXT) == 0x40);

    struct ALLOCATIONINFO
    {
        unsigned int hAllocation{};
        unsigned int pSystemMem{};
        unsigned int pPrivateDriverData{};
        unsigned int PrivateDriverDataSize{};
        unsigned int VidPnSourceId{};
        unsigned int Flags{};
    };
    static_assert(sizeof(ALLOCATIONINFO) == 0x18);

    struct CREATEALLOCATION
    {
        unsigned int hDevice{};
        unsigned int hResource{};
        unsigned int hGlobalShare{};
        unsigned int pPrivateRuntimeData{};
        unsigned int PrivateRuntimeDataSize{};
        unsigned int pPrivateDriverData{};
        unsigned int PrivateDriverDataSize{};
        unsigned int NumAllocations{};
        unsigned int pAllocationInfo{};
        unsigned int Flags{};
        unsigned int hPrivateRuntimeResourceHandle{};
    };
    static_assert(sizeof(CREATEALLOCATION) == 0x2C);

    // Matches EMU_D3DKMT_SUBMITCOMMAND exactly (host declares this struct's
    // pointer fields as 32-bit unconditionally -- see comment above).
    struct SUBMITCOMMAND
    {
        unsigned long long Commands{};
        unsigned int CommandLength{};
        unsigned int Flags{};
        unsigned long long PresentHistoryToken{};
        unsigned int BroadcastContextCount{};
        unsigned int pad0{};
        unsigned int BroadcastContext[64]{};
        unsigned int pPrivateDriverData{};
        unsigned int PrivateDriverDataSize{};
    };

    // Matches EMU_D3DKMT_PRESENT exactly.
    struct PRESENT
    {
        unsigned int hDevice{};
        unsigned int hWindow{};
        unsigned int VidPnSourceId{};
        unsigned int hSource{};
        unsigned int hDestination{};
    };

} // namespace d3dkmt

namespace
{

    constexpr auto* kClassName = "MouseInputTestSampleClass";
    constexpr auto* kWindowTitle = "Mouse Input Test Sample";
    constexpr unsigned int kWidth = 480;
    constexpr unsigned int kHeight = 270;
    constexpr DWORD kLogThrottleMs = 100;

    constexpr unsigned int k_adapter_luid_low = 0x1000;
    constexpr int k_adapter_luid_high = 0;

    using PFN_NtGdi = NTSTATUS(WINAPI*)(void*);

    struct GpuFunctions
    {
        PFN_NtGdi open_adapter_from_luid{};
        PFN_NtGdi create_device{};
        PFN_NtGdi create_context{};
        PFN_NtGdi create_allocation{};
        PFN_NtGdi submit_command{};
        PFN_NtGdi present{};
    };

    bool load_gpu_functions(GpuFunctions& fns)
    {
        HMODULE win32u = GetModuleHandleA("win32u.dll");
        if (!win32u)
        {
            win32u = LoadLibraryA("win32u.dll");
        }
        if (!win32u)
        {
            std::printf("[mits] win32u.dll not found\n");
            return false;
        }

        fns.open_adapter_from_luid = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDIOpenAdapterFromLuid"));
        fns.create_device = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateDevice"));
        fns.create_context = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateContext"));
        fns.create_allocation = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateAllocation"));
        fns.submit_command = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDISubmitCommand"));
        fns.present = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDIPresent"));

        if (!fns.open_adapter_from_luid || !fns.create_device || !fns.create_context || !fns.create_allocation ||
            !fns.submit_command || !fns.present)
        {
            std::printf("[mits] one or more NtGdiDdDDI* entrypoints not found\n");
            return false;
        }
        return true;
    }

    LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        static DWORD last_move_log_tick = 0;
        static DWORD last_input_log_tick = 0;

        switch (msg)
        {
        case WM_LBUTTONDOWN:
            std::printf("[mits] WM_LBUTTONDOWN x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_LBUTTONUP:
            std::printf("[mits] WM_LBUTTONUP x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_RBUTTONDOWN:
        {
            static bool cursor_visible = true;
            cursor_visible = !cursor_visible;
            ShowCursor(cursor_visible ? TRUE : FALSE);
            std::printf("[mits] WM_RBUTTONDOWN x=%d y=%d cursor_visible=%s\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                        cursor_visible ? "true" : "false");
            return 0;
        }
        case WM_RBUTTONUP:
            std::printf("[mits] WM_RBUTTONUP x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_MOUSEMOVE:
        {
            const DWORD now = GetTickCount();
            if (now - last_move_log_tick >= kLogThrottleMs)
            {
                last_move_log_tick = now;
                std::printf("[mits] WM_MOUSEMOVE x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            }
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            std::printf("[mits] %s vk=0x%02X scan=0x%02X extended=%d was_down=%d\n",
                        msg == WM_SYSKEYDOWN ? "WM_SYSKEYDOWN" : "WM_KEYDOWN", static_cast<unsigned>(wp),
                        static_cast<unsigned>((lp >> 16) & 0xFF), static_cast<int>((lp >> 24) & 1),
                        static_cast<int>((lp >> 30) & 1));
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            std::printf("[mits] %s vk=0x%02X scan=0x%02X extended=%d\n",
                        msg == WM_SYSKEYUP ? "WM_SYSKEYUP" : "WM_KEYUP", static_cast<unsigned>(wp),
                        static_cast<unsigned>((lp >> 16) & 0xFF), static_cast<int>((lp >> 24) & 1));
            return 0;
        case WM_CHAR:
            std::printf("[mits] WM_CHAR ch=0x%04X\n", static_cast<unsigned>(wp));
            return 0;
        case WM_INPUT:
        {
            UINT size = sizeof(RAWINPUT);
            BYTE buffer[sizeof(RAWINPUT)];
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) ==
                static_cast<UINT>(-1))
            {
                break;
            }
            const auto* raw = reinterpret_cast<RAWINPUT*>(buffer);
            if (raw->header.dwType == RIM_TYPEMOUSE)
            {
                const DWORD now = GetTickCount();
                if (now - last_input_log_tick >= kLogThrottleMs)
                {
                    last_input_log_tick = now;
                    std::printf("[mits] WM_INPUT dx=%ld dy=%ld\n", raw->data.mouse.lLastX, raw->data.mouse.lLastY);
                }
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcA(hwnd, msg, wp, lp);
    }

} // namespace

int main()
{
    // -----------------------------------------------------------------------
    // 1. Create a top-level window so the UI backend registers the HWND.
    // -----------------------------------------------------------------------
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    if (!RegisterClassExA(&wc))
    {
        std::printf("[mits] RegisterClassExA failed\n");
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, kClassName, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200,
                                static_cast<int>(kWidth), static_cast<int>(kHeight), nullptr, nullptr,
                                GetModuleHandleA(nullptr), nullptr);
    if (!hwnd)
    {
        std::printf("[mits] CreateWindowExA failed\n");
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // -----------------------------------------------------------------------
    // 2. Register for raw mouse input (trackpad-mode relative deltas).
    // -----------------------------------------------------------------------
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    rid.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
    rid.dwFlags = 0;
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        std::printf("[mits] RegisterRawInputDevices failed\n");
    }

    // -----------------------------------------------------------------------
    // 3. Load NtGdiDdDDI* entry points from win32u.dll.
    // -----------------------------------------------------------------------
    GpuFunctions fns{};
    if (!load_gpu_functions(fns))
    {
        return 1;
    }

    // -----------------------------------------------------------------------
    // 4. Open adapter from the emulator's fixed LUID.
    // -----------------------------------------------------------------------
    d3dkmt::OPENADAPTERFROMLUID open_adapter{};
    open_adapter.LuidLow = k_adapter_luid_low;
    open_adapter.LuidHigh = k_adapter_luid_high;
    if (NTSTATUS st = fns.open_adapter_from_luid(&open_adapter); st != 0)
    {
        std::printf("[mits] OpenAdapterFromLuid failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_adapter = open_adapter.hAdapter;
    std::printf("[mits] hAdapter=0x%X\n", h_adapter);

    // -----------------------------------------------------------------------
    // 5. Create device.
    // -----------------------------------------------------------------------
    d3dkmt::CREATEDEVICE create_device{};
    create_device.hAdapter = h_adapter;
    if (NTSTATUS st = fns.create_device(&create_device); st != 0)
    {
        std::printf("[mits] CreateDevice failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_device = create_device.hDevice;
    std::printf("[mits] hDevice=0x%X\n", h_device);

    // -----------------------------------------------------------------------
    // 6. Create context.
    // -----------------------------------------------------------------------
    d3dkmt::CREATECONTEXT create_context{};
    create_context.hDevice = h_device;
    if (NTSTATUS st = fns.create_context(&create_context); st != 0)
    {
        std::printf("[mits] CreateContext failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_context = create_context.hContext;
    std::printf("[mits] hContext=0x%X\n", h_context);

    // -----------------------------------------------------------------------
    // 7. Create render-target allocation.
    // -----------------------------------------------------------------------
    sogen::dxgk_cmd::render_target_desc rt_desc{};
    rt_desc.magic = sogen::dxgk_cmd::protocol_magic;
    rt_desc.width = kWidth;
    rt_desc.height = kHeight;
    rt_desc.format = 0; // VK_FORMAT_B8G8R8A8_UNORM

    d3dkmt::ALLOCATIONINFO alloc_info{};
    alloc_info.pPrivateDriverData = reinterpret_cast<unsigned long long>(&rt_desc);
    alloc_info.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(rt_desc));

    d3dkmt::CREATEALLOCATION create_alloc{};
    create_alloc.hDevice = h_device;
    create_alloc.NumAllocations = 1;
    create_alloc.pAllocationInfo = reinterpret_cast<unsigned long long>(&alloc_info);

    if (NTSTATUS st = fns.create_allocation(&create_alloc); st != 0)
    {
        std::printf("[mits] CreateAllocation failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_rt = alloc_info.hAllocation;
    std::printf("[mits] hRenderTarget=0x%X\n", h_rt);

    // -----------------------------------------------------------------------
    // 8. Clear and present exactly one static frame -- gives the app a real,
    //    non-default frame size to letterbox/transform against.
    // -----------------------------------------------------------------------
    constexpr std::array<float, 4> kFrameColor = {0.2f, 0.4f, 0.8f, 1.0f}; // steady blue

    sogen::dxgk_cmd::clear_command cmd{};
    cmd.magic = sogen::dxgk_cmd::protocol_magic;
    cmd.type = static_cast<unsigned int>(sogen::dxgk_cmd::command_type::clear);
    cmd.target_allocation = h_rt;
    cmd.color = kFrameColor;

    d3dkmt::SUBMITCOMMAND submit{};
    submit.BroadcastContextCount = 1;
    submit.BroadcastContext[0] = h_context;
    submit.pPrivateDriverData = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(&cmd));
    submit.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(cmd));

    if (NTSTATUS st = fns.submit_command(&submit); st != 0)
    {
        std::printf("[mits] SubmitCommand failed 0x%lX\n", static_cast<unsigned long>(st));
    }

    // See native-gpu-clear-sample.cpp's own comment: the real D3DKMT_PRESENT structure is far
    // larger than the 5 fields sogen's host handler reads, so this must be backed by a large
    // zeroed buffer rather than a struct sized to only the fields actually used.
    alignas(d3dkmt::PRESENT) unsigned char present_storage[4096]{};
    auto& present = *reinterpret_cast<d3dkmt::PRESENT*>(present_storage);
    present.hDevice = h_device;
    present.hWindow = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(hwnd));
    present.hSource = h_rt;

    if (NTSTATUS st = fns.present(&present); st != 0)
    {
        std::printf("[mits] Present failed 0x%lX\n", static_cast<unsigned long>(st));
    }

    std::printf("[mits] frame presented %ux%u rgba=(%.2f,%.2f,%.2f,%.2f)\n", kWidth, kHeight, kFrameColor[0],
                kFrameColor[1], kFrameColor[2], kFrameColor[3]);
    std::printf("[mits] ready -- waiting for input\n");

    // -----------------------------------------------------------------------
    // 9. Block on the message loop -- no per-frame animation work, unlike
    //    native-gpu-clear-sample's PeekMessageA polling loop.
    // -----------------------------------------------------------------------
    MSG msg{};
    while (GetMessageA(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    std::printf("[mits] window closed\n");
    return 0;
}
