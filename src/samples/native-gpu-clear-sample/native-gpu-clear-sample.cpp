#include <windows.h>

#include <array>
#include <cstdio>

#include <dxgk_command_protocol.hpp>

// ---------------------------------------------------------------------------
// D3DKMT kernel-facing struct definitions.
//
// This sample only runs as a 32-bit (WoW64) guest. A 32-bit guest's actual
// NtGdiDdDDI* call arguments are the tightly-packed, all-32-bit-field "wire"
// layout below — real Microsoft wow64win.dll code (genuinely emulated, not
// sogen's own) widens every pointer-ish field to a naturally-aligned 64-bit
// slot before the syscall reaches the host handler, and narrows the result
// back on return. That widened shape is what gdi.hpp's EMU_D3DKMT_* structs
// describe; it is NOT the shape a 32-bit guest should construct itself.
// Verified against wow64win.dll's whNtGdiDdDDICreateDevice/CreateContext/
// CreateAllocation thunks via IDA.
//
// SUBMITCOMMAND/PRESENT are the exception: gdi.hpp declares their pointer
// fields as UINT32 unconditionally (no widening), so their wire shape here
// already matches what the host handler reads directly.
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
    // pointer fields as 32-bit unconditionally — see comment above).
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

    constexpr auto* kClassName = "NativeGpuClearSampleClass";
    constexpr auto* kWindowTitle = "Native GPU Clear Sample";
    constexpr unsigned int kWidth = 320;
    constexpr unsigned int kHeight = 180;

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
            std::printf("[ngcs] win32u.dll not found\n");
            return false;
        }

        fns.open_adapter_from_luid = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDIOpenAdapterFromLuid"));
        fns.create_device = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateDevice"));
        fns.create_context = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateContext"));
        fns.create_allocation = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDICreateAllocation"));
        fns.submit_command = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDISubmitCommand"));
        fns.present = reinterpret_cast<PFN_NtGdi>(GetProcAddress(win32u, "NtGdiDdDDIPresent"));

        if (!fns.open_adapter_from_luid || !fns.create_device || !fns.create_context || !fns.create_allocation || !fns.submit_command ||
            !fns.present)
        {
            std::printf("[ngcs] one or more NtGdiDdDDI* entrypoints not found\n");
            return false;
        }
        return true;
    }

    LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg)
        {
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
        std::printf("[ngcs] RegisterClassExA failed\n");
        return 1;
    }

    HWND hwnd = CreateWindowExA(0, kClassName, kWindowTitle, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, static_cast<int>(kWidth),
                                static_cast<int>(kHeight), nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!hwnd)
    {
        std::printf("[ngcs] CreateWindowExA failed\n");
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // -----------------------------------------------------------------------
    // 2. Load NtGdiDdDDI* entry points from win32u.dll.
    // -----------------------------------------------------------------------
    GpuFunctions fns{};
    if (!load_gpu_functions(fns))
    {
        return 1;
    }

    // -----------------------------------------------------------------------
    // 3. Open adapter from the emulator's fixed LUID.
    // -----------------------------------------------------------------------
    d3dkmt::OPENADAPTERFROMLUID open_adapter{};
    open_adapter.LuidLow = k_adapter_luid_low;
    open_adapter.LuidHigh = k_adapter_luid_high;
    if (NTSTATUS st = fns.open_adapter_from_luid(&open_adapter); st != 0)
    {
        std::printf("[ngcs] OpenAdapterFromLuid failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_adapter = open_adapter.hAdapter;
    std::printf("[ngcs] hAdapter=0x%X\n", h_adapter);

    // -----------------------------------------------------------------------
    // 4. Create device.
    // -----------------------------------------------------------------------
    d3dkmt::CREATEDEVICE create_device{};
    create_device.hAdapter = h_adapter;
    if (NTSTATUS st = fns.create_device(&create_device); st != 0)
    {
        std::printf("[ngcs] CreateDevice failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_device = create_device.hDevice;
    std::printf("[ngcs] hDevice=0x%X\n", h_device);

    // -----------------------------------------------------------------------
    // 5. Create context.
    // -----------------------------------------------------------------------
    d3dkmt::CREATECONTEXT create_context{};
    create_context.hDevice = h_device;
    if (NTSTATUS st = fns.create_context(&create_context); st != 0)
    {
        std::printf("[ngcs] CreateContext failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_context = create_context.hContext;
    std::printf("[ngcs] hContext=0x%X\n", h_context);

    // -----------------------------------------------------------------------
    // 6. Create render-target allocation.
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
        std::printf("[ngcs] CreateAllocation failed 0x%lX\n", static_cast<unsigned long>(st));
        return 1;
    }
    const unsigned int h_rt = alloc_info.hAllocation;
    std::printf("[ngcs] hRenderTarget=0x%X\n", h_rt);

    // -----------------------------------------------------------------------
    // 7. Frame loop: animate color, clear, present.
    // -----------------------------------------------------------------------
    constexpr int kFrameCount = 8;
    const std::array<std::array<float, 4>, kFrameCount> frame_colors = {{
        {0.0f, 0.0f, 1.0f, 1.0f}, // blue
        {0.0f, 1.0f, 0.0f, 1.0f}, // green
        {1.0f, 0.0f, 0.0f, 1.0f}, // red
        {1.0f, 1.0f, 0.0f, 1.0f}, // yellow
        {0.0f, 1.0f, 1.0f, 1.0f}, // cyan
        {1.0f, 0.0f, 1.0f, 1.0f}, // magenta
        {1.0f, 0.5f, 0.0f, 1.0f}, // orange
        {0.5f, 0.0f, 1.0f, 1.0f}, // violet
    }};

    for (int frame = 0; frame < kFrameCount; ++frame)
    {
        MSG msg{};
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT)
            {
                std::printf("[ngcs] window closed\n");
                return 0;
            }
        }

        const auto& color = frame_colors[static_cast<size_t>(frame)];

        // Build the clear command in guest-accessible memory.
        sogen::dxgk_cmd::clear_command cmd{};
        cmd.magic = sogen::dxgk_cmd::protocol_magic;
        cmd.type = static_cast<unsigned int>(sogen::dxgk_cmd::command_type::clear);
        cmd.target_allocation = h_rt;
        cmd.color = color;

        // SubmitCommand: clear the render target on the host GPU.
        d3dkmt::SUBMITCOMMAND submit{};
        submit.BroadcastContextCount = 1;
        submit.BroadcastContext[0] = h_context;
        submit.pPrivateDriverData = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(&cmd));
        submit.PrivateDriverDataSize = static_cast<unsigned int>(sizeof(cmd));

        if (NTSTATUS st = fns.submit_command(&submit); st != 0)
        {
            std::printf("[ngcs] SubmitCommand frame %d failed 0x%lX\n", frame, static_cast<unsigned long>(st));
        }

        // Present: readback the GPU image and push to the SDL window.
        //
        // The real D3DKMT_PRESENT structure is far larger than the 5 fields sogen's host
        // handler reads (wow64win.dll's thunk touches offsets past 1400 bytes, for present
        // region arrays / private data the host doesn't use). A struct sized to only what the
        // host reads leaves wow64win.dll reading uninitialized stack memory beyond it as if it
        // were real fields, which crashes when it finds garbage "pointers"/"counts" there. Back
        // it with a large zeroed buffer so those unused fields read as harmless zero/null.
        alignas(d3dkmt::PRESENT) unsigned char present_storage[4096]{};
        auto& present = *reinterpret_cast<d3dkmt::PRESENT*>(present_storage);
        present.hDevice = h_device;
        present.hWindow = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(hwnd));
        present.hSource = h_rt;

        if (NTSTATUS st = fns.present(&present); st != 0)
        {
            std::printf("[ngcs] Present frame %d failed 0x%lX\n", frame, static_cast<unsigned long>(st));
        }

        std::printf("[ngcs] frame %d rgba=(%.2f,%.2f,%.2f,%.2f)\n", frame, color[0], color[1], color[2], color[3]);

        Sleep(400);
    }

    std::printf("[ngcs] done\n");
    return 0;
}
