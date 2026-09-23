# Mouse Input Test Guest Sample — Design

## Problem

The iOS app's emulation-chrome-mouse plan (Tasks 1-5, all landed) added two real mouse-input
delivery modes — touchscreen (positioned `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/etc.) and trackpad
(relative raw-input deltas) — plus gesture recognizers, chrome, and a dynamic aspect-ratio
transform. The only guest bundled into the app, `native-gpu-clear-sample.exe`, exercises none of
this: its `window_proc` only handles `WM_CLOSE`/`WM_DESTROY`, and it never calls
`RegisterRawInputDevices`. Task 6's real-device round already hit a real bug (frames presented to
a stale placeholder `CALayer` never reaching the visible layer) purely from the existing sample's
render pipeline — none of the mouse-input criteria have been exercised by anything yet.

## Goal

A small new guest sample, cross-compiled and bundled the same way as `native-gpu-clear-sample`,
whose sole purpose is to make the mouse-input criteria in Task 6 (and future real-device rounds)
actually checkable: tap a spot, read back exactly what coordinate the guest received; drag, watch
the relative deltas scroll by; confirm two-finger-tap/long-press-drag produce the right message
types.

## Guest sample: `mouse-input-test-sample`

**Location:** `src/samples/mouse-input-test-sample/`, structured identically to
`src/samples/native-gpu-clear-sample/` (own `CMakeLists.txt` glob-adding to
`src/samples/CMakeLists.txt`, `WIN`-gated `user32`/`gdi32` linkage, MinGW-w64 cross-compiled).

**Startup sequence** (reuses `native-gpu-clear-sample`'s entire D3DKMT WoW64 wire-protocol
plumbing verbatim — `OpenAdapterFromLuid` → `CreateDevice` → `CreateContext` →
`CreateAllocation`, all copied unchanged since that plumbing has nothing to do with input):

1. `RegisterClassExA`/`CreateWindowExA` — same pattern, but at a **480×270** render target
   (deliberately different from `native-gpu-clear-sample`'s 320×180) so the app is forced to
   letterbox/transform against a genuinely different aspect ratio and frame size than what's
   already been tested.
2. Register for raw mouse input via `RegisterRawInputDevices` immediately after window creation.
3. Submit **one** clear command (a single fixed color) and **one** present — not an 8-frame
   animation loop like `native-gpu-clear-sample`. This guest's job is to give the app a real,
   static, non-default frame size to work against, not to test rendering again.
4. Enter an indefinite `GetMessageA`/`TranslateMessage`/`DispatchMessageA` loop (blocking, not
   `PeekMessageA`-polling — there's no per-frame animation work to interleave with) until
   `WM_QUIT`.

**`window_proc` gains real handling for:**
- `WM_LBUTTONDOWN` / `WM_LBUTTONUP` / `WM_RBUTTONDOWN` / `WM_RBUTTONUP` — log unthrottled,
  `printf("[mits] %s x=%d y=%d\n", <message-name>, GET_X_LPARAM(lp), GET_Y_LPARAM(lp))`.
- `WM_MOUSEMOVE` — log throttled to ~1 line per 100ms (tracked via `GetTickCount()` against a
  static last-logged timestamp), same coordinate format.
- `WM_INPUT` — call `GetRawInputData` to pull the `RAWMOUSE` payload, log the relative
  `lLastX`/`lLastY` deltas, throttled the same way as `WM_MOUSEMOVE`.
- Falls through to `DefWindowProcA` for everything else, same as `native-gpu-clear-sample`.

**Reporting channel:** plain `stdout` (`std::printf`), exactly like `native-gpu-clear-sample`
already does. Confirmed this reaches the iOS app for free: `handle_NtWriteFile` in
`src/windows-emulator/syscalls/file.cpp` forwards `STDOUT_HANDLE` writes to
`windows_emulator`'s `on_stdout` callback, which `SogenBridge.mm` already wires into the same
`onLogLine` sink driving `LogsOverlayView` — no new plumbing needed anywhere in the iOS app or
`ios_ui_backend`. (`OutputDebugString` was checked and confirmed NOT forwarded anywhere — not
used.)

## iOS app change

`SetupView` gains one more button, "Boot Input Test," alongside the existing
"Download Emulation Root"/"Check for Pairing File" row. It boots
`mouse-input-test-sample.exe` (bundled into the app the same way
`native-gpu-clear-sample.exe` already is, via `Bundle.main.path(forResource:ofType:)`) through
the exact same `attemptBoot()`/`startEmulator(with:)` flow already in place — no duplication of
the JIT-grant/root-provisioning sequence, just a different `guestPath` depending on which button
was tapped. Both guests stay boot-able without rebuilding/swapping bundled files between rounds.

## Verification this enables

- **Touchscreen mode positioning**: tap a spot on the 480×270 guest view; the logged
  `WM_LBUTTONDOWN x=/y=` line should match where you tapped, accounting for the visible
  letterbox. This is the first real exercise of `EmulatorHostView.toGuestPoint`'s scale/offset
  math against a non-320×180 frame.
- **Trackpad mode dragging**: pan in trackpad mode; the throttled `WM_INPUT` delta lines confirm
  relative motion is actually being delivered and roughly matches the drag's real speed/direction.
- **Two-finger tap right-click**: confirms `WM_RBUTTONDOWN`/`WM_RBUTTONUP` (touchscreen) or the
  raw right-click path (trackpad) actually arrives.
- **Press-and-hold-drag**: confirms the down/move.../up sequence arrives in the right order.
- **Dynamic aspect ratio**: the guest view's on-screen shape should visibly match 480:270,
  not the 320:180 fallback — directly falsifiable by eye, unlike anything testable with the
  existing sample.

## Explicitly out of scope

- No visual on-screen marker/redraw per click (a fixed single frame is presented once; log lines
  are the sole per-event feedback channel, per explicit choice over the richer alternative).
- No real Win32 child controls (buttons, etc.) — coordinate echo via log is the verification
  mechanism, not hit-testing against a widget.
- No changes to `ios_ui_backend`, `SogenBridge`, or any Swift chrome code beyond the one new
  `SetupView` button and its `guestPath` wiring.
- No changes to the emulation root download/provisioning flow.
