# Mouse Input Test Guest Sample Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A small guest Windows PE, bundled alongside `native-gpu-clear-sample.exe` in the iOS app, that echoes every mouse message/raw-input delta it receives via stdout (visible in the app's Logs overlay), so the emulation-chrome-mouse plan's Task 6 mouse-input criteria have something real to verify against.

**Architecture:** A new guest sample under `src/samples/mouse-input-test-sample/` reuses `native-gpu-clear-sample`'s entire D3DKMT WoW64 wire-protocol plumbing verbatim (window creation, adapter/device/context/allocation, one present) but at a distinct 480×270 resolution, adds `RegisterRawInputDevices` + real `window_proc` handling for positioned mouse messages and `WM_INPUT`, and reports everything via throttled `printf` (confirmed to already reach the iOS Logs overlay through the existing `on_stdout`/`onLogLine` path — no new plumbing). `SetupView` gains one more button to boot it instead of the default guest.

**Tech Stack:** C++20, MinGW-w64 (`i686-w64-mingw32-g++`, 32-bit WoW64-only, matching `native-gpu-clear-sample`), Swift/SwiftUI.

**User decisions (already made):**
- Reporting is log-only via stdout printf, no visual per-click marker redraw (chose "Log-only" over "Log + visual marker").
- Boot path is a second, permanent "Boot Input Test" button in `SetupView`, not a one-off swap of the bundled guest (chose "Add a second boot button").
- The guest still presents one static frame at a new resolution (480×270) so the app's dynamic-aspect-ratio/coordinate-transform logic is actually exercised (chose "Yes, one static frame at a new size" over "no frames at all").
- Continuous events (`WM_MOUSEMOVE`, `WM_INPUT` deltas) throttle to ~10/sec; discrete events (button down/up) log unthrottled (chose "Throttle to ~10/sec" over unthrottled).

---

## Task 1: Guest sample source

**Goal:** A new `mouse-input-test-sample.exe` guest that creates a window, registers for raw mouse input, presents one static 480×270 frame, and logs every positioned mouse message and raw delta it receives via `printf`.

**Files:**
- Create: `src/samples/mouse-input-test-sample/CMakeLists.txt`
- Create: `src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp`
- Modify: `src/samples/CMakeLists.txt`

**Acceptance Criteria:**
- [ ] `mouse-input-test-sample.cpp` compiles cleanly with the project's 32-bit MinGW toolchain (same one `tools/stage-ios-emulation-root.sh` already uses for `native-gpu-clear-sample`).
- [ ] `window_proc` handles `WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_RBUTTONDOWN`/`WM_RBUTTONUP` (logged unthrottled) and `WM_MOUSEMOVE`/`WM_INPUT` (logged throttled to ~1 line per 100ms).
- [ ] `main()` registers for raw mouse input via `RegisterRawInputDevices` right after window creation, presents exactly one static frame at 480×270, then blocks in a `GetMessageA` loop until `WM_QUIT`.
- [ ] `src/samples/CMakeLists.txt` adds the new subdirectory, matching every other sample's registration.
- [ ] This CMakeLists.txt has zero effect on the macOS desktop build (samples/ is only ever added under `if(WIN32)` in `src/CMakeLists.txt` — confirmed, not something this task changes).

**Verify:** `i686-w64-mingw32-g++ -O2 -std=c++20 -static -static-libgcc -static-libstdc++ -I src/dxgk-command-protocol src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp -o /tmp/mouse-input-test-sample.exe -luser32 -lgdi32` → exits 0, `file /tmp/mouse-input-test-sample.exe` reports a 32-bit PE.

**Steps:**

- [ ] **Step 1: Read `src/samples/native-gpu-clear-sample/native-gpu-clear-sample.cpp` in full** to confirm the exact D3DKMT struct layouts and GPU-function-loading code being reused below haven't changed since this plan was written.

- [ ] **Step 2: Create `src/samples/mouse-input-test-sample/CMakeLists.txt`**

```cmake
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

list(SORT SRC_FILES)

add_executable(mouse-input-test-sample ${SRC_FILES})

target_link_libraries(mouse-input-test-sample PRIVATE dxgk-command-protocol)

if(WIN)
  target_link_libraries(mouse-input-test-sample PRIVATE gdi32 user32)
endif()

sogen_assign_source_group(${SRC_FILES})
```

- [ ] **Step 3: Add the subdirectory to `src/samples/CMakeLists.txt`**

Find:
```cmake
add_subdirectory(native-gpu-clear-sample)
```

Change to:
```cmake
add_subdirectory(native-gpu-clear-sample)
add_subdirectory(mouse-input-test-sample)
```

- [ ] **Step 4: Create `src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp`**

```cpp
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
            std::printf("[mits] WM_RBUTTONDOWN x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
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
```

- [ ] **Step 5: Build and verify**

```bash
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I src/dxgk-command-protocol \
  src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp \
  -o /tmp/mouse-input-test-sample.exe \
  -luser32 -lgdi32
file /tmp/mouse-input-test-sample.exe
```
Expected: exit 0, `file` reports `PE32 executable (console) Intel 80386` (or similar 32-bit PE description).

- [ ] **Step 6: Commit**

```bash
git add src/samples/mouse-input-test-sample/CMakeLists.txt src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp src/samples/CMakeLists.txt
git commit -m "feat(samples): add mouse-input-test-sample guest for iOS mouse-mode verification"
```

---

## Task 2: Wire the new sample into iOS root staging

**Goal:** `tools/stage-ios-emulation-root.sh` builds and stages `mouse-input-test-sample.exe` the same way it already does for `native-gpu-clear-sample.exe`, and the README documents copying both into `Resources/`.

**Files:**
- Modify: `tools/stage-ios-emulation-root.sh`
- Modify: `tools/sogen-ios/README.md`

**Acceptance Criteria:**
- [ ] Running the staging script produces `build/ios-root/mouse-input-test-sample.exe` alongside the existing `build/ios-root/native-gpu-clear-sample.exe`.
- [ ] `tools/sogen-ios/README.md`'s Build section documents copying both `.exe`s into `Resources/`.

**Verify:** `tools/stage-ios-emulation-root.sh` → both `.exe`s present under `build/ios-root/`, `file build/ios-root/mouse-input-test-sample.exe` reports a 32-bit PE.

**Steps:**

- [ ] **Step 1: Read `tools/stage-ios-emulation-root.sh` in full** to confirm the exact insertion point (right after the existing `native-gpu-clear-sample.exe` build block, before the final `echo "==> done"`).

- [ ] **Step 2: Edit `tools/stage-ios-emulation-root.sh`**

Find:
```bash
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I "$REPO_ROOT/src/dxgk-command-protocol" \
  "$REPO_ROOT/src/samples/native-gpu-clear-sample/native-gpu-clear-sample.cpp" \
  -o "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" \
  -luser32 -lgdi32

cp -f "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" "$OUT_ROOT/native-gpu-clear-sample.exe"

echo "==> done"
du -sh "$OUT_ROOT"
file "$OUT_ROOT/native-gpu-clear-sample.exe"
```

Replace with:
```bash
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I "$REPO_ROOT/src/dxgk-command-protocol" \
  "$REPO_ROOT/src/samples/native-gpu-clear-sample/native-gpu-clear-sample.cpp" \
  -o "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" \
  -luser32 -lgdi32

cp -f "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" "$OUT_ROOT/native-gpu-clear-sample.exe"

echo "==> building the mouse-input-test-sample guest PE"
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I "$REPO_ROOT/src/dxgk-command-protocol" \
  "$REPO_ROOT/src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp" \
  -o "$OUT_ROOT/filesys/c/mouse-input-test-sample.exe" \
  -luser32 -lgdi32

cp -f "$OUT_ROOT/filesys/c/mouse-input-test-sample.exe" "$OUT_ROOT/mouse-input-test-sample.exe"

echo "==> done"
du -sh "$OUT_ROOT"
file "$OUT_ROOT/native-gpu-clear-sample.exe"
file "$OUT_ROOT/mouse-input-test-sample.exe"
```

- [ ] **Step 3: Edit `tools/sogen-ios/README.md`**

Find (in the Build section):
```
mkdir -p Resources && cp ../../build/ios-root/native-gpu-clear-sample.exe Resources/
```

Replace with:
```
mkdir -p Resources
cp ../../build/ios-root/native-gpu-clear-sample.exe Resources/
cp ../../build/ios-root/mouse-input-test-sample.exe Resources/
```

- [ ] **Step 4: Run the staging script and verify**

```bash
tools/stage-ios-emulation-root.sh
file build/ios-root/mouse-input-test-sample.exe
```
Expected: both `.exe`s present, `file` reports a 32-bit PE for the new one.

- [ ] **Step 5: Commit**

```bash
git add tools/stage-ios-emulation-root.sh tools/sogen-ios/README.md
git commit -m "chore(ios): stage mouse-input-test-sample alongside native-gpu-clear-sample"
```

---

## Task 3: SetupView "Boot Input Test" button

**Goal:** `SetupView` can boot either bundled guest: the existing default flow is unchanged, and a new "Boot Input Test" button (visible only after the first successful boot, since JIT is granted once per process and doesn't need repeating) starts a fresh `SogenEmulator` against `mouse-input-test-sample.exe` instead.

**Files:**
- Modify: `tools/sogen-ios/Sources/SetupView.swift`

**Acceptance Criteria:**
- [ ] `startEmulator(with:guestResourceName:)` takes a guest resource name defaulting to `"native-gpu-clear-sample"` — every existing call site (Simulator branch, Xcode-debugger-bypass completion, `JITGateOrchestrator.run` completion) is unchanged and keeps booting the default guest exactly as before.
- [ ] A new `@State private var everBooted = false` is set `true` right after the first successful `instance.start()`.
- [ ] A "Boot Input Test" button appears in the existing button row once `everBooted` is `true`; tapping it stops the current emulator, resets `emulator`/`bootedEmulator`/`didBoot`, and calls `startEmulator(with:guestResourceName:)` with `"mouse-input-test-sample"`.
- [ ] Builds clean.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current `SetupView.swift` in full** to confirm exact current content before editing (it has been touched by three prior tasks in the emulation-chrome-mouse plan — the `logLines`/`didBoot`/`EmulatorView` call-site details must match exactly).

- [ ] **Step 2: Add the `everBooted` state property**

Find:
```swift
    @State private var bootedEmulator: SogenEmulator?
    @State private var didBoot = false
```

Replace with:
```swift
    @State private var bootedEmulator: SogenEmulator?
    @State private var didBoot = false
    @State private var everBooted = false
```

- [ ] **Step 3: Add the "Boot Input Test" button**

Find:
```swift
                if needsLocalDevVPNInstall {
```

Replace with:
```swift
                if everBooted {
                    Button("Boot Input Test") {
                        guard let layer = pendingLayer else { return }
                        emulator?.stop()
                        emulator = nil
                        bootedEmulator = nil
                        didBoot = false
                        startEmulator(with: layer, guestResourceName: "mouse-input-test-sample")
                    }
                    .padding(6)
                }
                if needsLocalDevVPNInstall {
```

- [ ] **Step 4: Parameterize `startEmulator` and set `everBooted`**

Find:
```swift
    private func startEmulator(with layer: CALayer) {
        guard emulator == nil else { return }

        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let root = documents.appendingPathComponent("root").path

        guard let guestPath = Bundle.main.path(forResource: "native-gpu-clear-sample", ofType: "exe") else {
            appendLog("ERROR: native-gpu-clear-sample.exe is not in the app bundle")
            return
        }

        let instance = SogenEmulator(layer: layer, emulationRoot: root, guestExecutablePath: guestPath)
        instance.onLogLine = { line in
            appendLog(line.trimmingCharacters(in: .newlines))
        }
        emulator = instance
        appendLog("[sogen] emulation root: \(root)")
        appendLog("[sogen] guest executable: \(guestPath)")
        instance.start()
        bootedEmulator = instance
        didBoot = true
    }
```

Replace with:
```swift
    private func startEmulator(with layer: CALayer, guestResourceName: String = "native-gpu-clear-sample") {
        guard emulator == nil else { return }

        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let root = documents.appendingPathComponent("root").path

        guard let guestPath = Bundle.main.path(forResource: guestResourceName, ofType: "exe") else {
            appendLog("ERROR: \(guestResourceName).exe is not in the app bundle")
            return
        }

        let instance = SogenEmulator(layer: layer, emulationRoot: root, guestExecutablePath: guestPath)
        instance.onLogLine = { line in
            appendLog(line.trimmingCharacters(in: .newlines))
        }
        emulator = instance
        appendLog("[sogen] emulation root: \(root)")
        appendLog("[sogen] guest executable: \(guestPath)")
        instance.start()
        bootedEmulator = instance
        didBoot = true
        everBooted = true
    }
```

Every existing call site (`startEmulator(with: layer)`, three occurrences) is untouched — the new `guestResourceName` parameter's default value (`"native-gpu-clear-sample"`) preserves their behavior exactly.

- [ ] **Step 5: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```
Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 6: Commit**

```bash
git add tools/sogen-ios/Sources/SetupView.swift
git commit -m "feat(ios): add Boot Input Test button for mouse-input-test-sample"
```

---

## Task 4: Build, bundle, and deliver

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in the acceptance criteria has been re-validated independently, with output captured.

**Goal:** An updated `.ipa` bundling both guest samples is delivered, and tapping "Boot Input Test" on a real device produces real `[mits]` log lines in response to actual touches/drags — confirming the new tool actually works before it's used to re-run the original plan's Task 6 checklist.

**Files:** None (packaging/verification only — uses Tasks 1-3's outputs).

**Acceptance Criteria:**
- [ ] `tools/stage-ios-emulation-root.sh` re-run produces both `.exe`s; both copied into `tools/sogen-ios/Resources/`.
- [ ] `xcodebuild archive` succeeds; `otool -hv` on the archived binary confirms `NOUNDEFS`.
- [ ] Packaged `.ipa` delivered to `/Users/jack/Library/Mobile Documents/com~apple~CloudDocs/SogenIOS-unsigned.ipa`, replacing the previous copy.
- [ ] On real device: default boot still reaches `native-gpu-clear-sample` unchanged (regression check).
- [ ] On real device: tapping "Boot Input Test" boots `mouse-input-test-sample.exe`; the Logs overlay shows `[mits] frame presented 480x270 ...` and `[mits] ready -- waiting for input`.
- [ ] On real device: tapping the guest view in touchscreen mode produces a `[mits] WM_LBUTTONDOWN x=.. y=..`/`WM_LBUTTONUP x=.. y=..` pair in the log.
- [ ] On real device: dragging in trackpad mode produces `[mits] WM_INPUT dx=.. dy=..` lines in the log.

**Verify:** Manual, on-device, for each of the last four bullets above. Capture the on-screen log for each.

**Steps:**

- [ ] **Step 1: Re-stage the root and bundle both guests**

```bash
tools/stage-ios-emulation-root.sh
cd tools/sogen-ios
cp ../../build/ios-root/native-gpu-clear-sample.exe Resources/
cp ../../build/ios-root/mouse-input-test-sample.exe Resources/
```

- [ ] **Step 2: Archive and verify NOUNDEFS**

```bash
xcodegen generate
xcodebuild archive \
  -project SogenIOS.xcodeproj \
  -scheme SogenIOS \
  -sdk iphoneos \
  -configuration Release \
  -archivePath build-unsigned/SogenIOS.xcarchive \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
  CODE_SIGN_STYLE=Manual DEVELOPMENT_TEAM="" PROVISIONING_PROFILE_SPECIFIER="" \
  AD_HOC_CODE_SIGNING_ALLOWED=YES
otool -hv build-unsigned/SogenIOS.xcarchive/Products/Applications/SogenIOS.app/SogenIOS | grep NOUNDEFS
```
Expected: `** ARCHIVE SUCCEEDED **`, `NOUNDEFS` present.

- [ ] **Step 3: Package and deliver**

```bash
rm -rf ipa-stage && mkdir -p ipa-stage/Payload
cp -R build-unsigned/SogenIOS.xcarchive/Products/Applications/SogenIOS.app ipa-stage/Payload/
rm -f SogenIOS-unsigned.ipa
cd ipa-stage && zip -qr ../SogenIOS-unsigned.ipa Payload && cd ..
cp SogenIOS-unsigned.ipa "/Users/jack/Library/Mobile Documents/com~apple~CloudDocs/SogenIOS-unsigned.ipa"
```

- [ ] **Step 4: User installs and tests**

User re-installs the `.ipa`, confirms the default boot still works unchanged, taps "Boot Input Test," and confirms the `[mits]` log lines described in the acceptance criteria for both touchscreen taps and trackpad drags.

- [ ] **Step 5: Fix and re-verify**

Any bullet that fails gets a real fix (not a workaround) dispatched as its own follow-up round, re-verified the same way, before this task is considered closed.

- [ ] **Step 6: No commit expected**

This task produces build/packaging artifacts only (all gitignored). If `git status` shows anything unexpected and trackable, investigate before assuming it's safe to ignore.

```json:metadata
{"userGate": true, "tags": ["user-gate"], "requiresUserSpecification": false, "files": [], "verifyCommand": "manual on-device", "acceptanceCriteria": ["both exes staged and bundled", "archive succeeds with NOUNDEFS", "ipa delivered to iCloud Drive", "default boot unchanged (regression)", "Boot Input Test boots mouse-input-test-sample and logs frame-presented/ready lines", "touchscreen tap logs WM_LBUTTONDOWN/UP pair", "trackpad drag logs WM_INPUT dx/dy lines"], "modelTier": "standard"}
```

---

## Execution Handoff
