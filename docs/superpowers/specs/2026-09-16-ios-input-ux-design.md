# Emulation Screen Chrome + Mouse Input Design

**Goal:** Replace the current single flat view (setup controls + guest view + logs all in one) with a real two-screen navigation model, and give the guest genuinely usable mouse input — today a tap only produces a positionless raw-input click that most ordinary Windows apps never see at all.

**Context:** This is sub-project A of a four-part input/UX polish effort (the next phase after FEX real-device bring-up and LocalDevVPN support). The other three — **B** (on-screen keyboard), **C** (Arcade Mode virtual gamepad), **D** (setup/profiles screen redesign: root list, per-root management, settings, add/download) — are each their own separate design→plan→build cycle. This spec covers only A.

**Current state, confirmed by reading the code:**
- `ContentView.swift` is one view: setup buttons ("Download Emulation Root", "Check for Pairing File"), the guest view, and the log list, all always visible together. There is no back button, no navigation, no concept of a distinct "emulation screen."
- `EmulatorView.swift`'s `EmulatorHostView` has exactly one gesture recognizer — a plain tap — wired to `deliverTap()` → `SogenBridge.mm`'s `queue_left_click()`, which carries **no position at all**.
- The only input-delivery mechanism that exists in the C++ core is `windows_emulator::deliver_raw_mouse_input(dx, dy, button_flags, button_data)` (`src/windows-emulator/windows_emulator.cpp:2269`) and `deliver_raw_keyboard_input(...)` (`:2276`) — both post a single `WM_INPUT` message via `deliver_raw_input()` (`:2232`). **No standard `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_KEYDOWN`/`WM_KEYUP` synthesis exists anywhere in the codebase.** Raw input is opt-in (`RegisterRawInputDevices`) and used mainly by games wanting unfiltered mouse-look (the existing code comments cite Skyrim as the reference case) — ordinary Win32 UI apps (dialogs, buttons, text fields) do not consume it and would see nothing from the current `queue_left_click()` path.
- The guest-view container hardcodes `.aspectRatio(320.0 / 180.0, contentMode: .fit)` (`ContentView.swift:58`), correct only for the one existing test sample.

**User decisions (already made):**
- Real navigation: a **setup screen** (today's existing controls, left as-is — its real redesign is sub-project D) and a new **emulation screen**, entered on successful boot, with a back button that returns to setup and stops the emulator.
- Emulation screen, portrait: back button top-left; icon row top-right (mouse-mode toggle, logs toggle, keyboard toggle); guest view in the middle; on-screen keyboard area fixed at the bottom (icon wired now, actual keyboard content is sub-project B, so this cycle's keyboard icon is a placeholder no-op).
- Emulation screen, landscape: guest view fills the entire screen; back button + icon row overlay on top; keyboard floats above the guest view when opened (deferred content, same as above), collapsible.
- Guest-view container dynamically fits whatever aspect ratio the incoming frame actually has, instead of the hardcoded 320:180. Letting the user *configure* the guest's boot resolution is a separate, deeper per-environment setting deferred to sub-project D.
- Two user-switchable mouse modes, chosen per what's running, switched live via the icon — no restart needed:
  - **Touchscreen mode**: tap = click at that exact position. Requires a *new* capability: synthesizing standard positioned `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP` messages with real coordinates — this is what makes ordinary UI apps respond, since they don't consume raw input.
  - **Trackpad mode**: drag = relative cursor movement, tap = click — drives the *existing* `deliver_raw_mouse_input` relative-delta path, for raw-input-consuming games (Steam Link's own "Trackpad" vs "Direct Cursor" modes are the direct inspiration).
- Gestures (fixed defaults this cycle; user remapping is a sub-project D settings feature): two-finger tap = right-click; press-and-hold-then-drag = click-and-drag.
- Logs: hidden by default on the emulation screen; the logs icon toggles a dismissible overlay panel showing the same log lines the setup screen already renders.

---

## Architecture

`ContentView.swift` splits into two views under a `NavigationStack` (or equivalent SwiftUI navigation container): `SetupView` (today's existing body, extracted verbatim — no redesign in this cycle) and a new `EmulationView`. `SetupView` currently owns `pendingLayer`/`bootAttempted`/`emulator` state and the `attemptBoot()`/`startEmulator()` logic; that state and logic stay where they are, but once `startEmulator(with:)` succeeds, instead of just setting `emulator`, `SetupView` navigates to `EmulationView`, passing the `SogenEmulator` instance and log-line array (or a shared observable object) forward. Backing out of `EmulationView` calls `emulator.stop()` (mirroring `SogenBridge.mm`'s existing teardown, called via whatever the emulator wrapper already exposes) and pops back to `SetupView`.

`EmulationView` owns: the guest `EmulatorView` (unchanged internally except for gesture/coordinate handling below), the icon row, the mouse-mode state, the logs-overlay state, and a keyboard-icon state (wired to a `@State` toggle, but rendering nothing yet — sub-project B fills this in). Layout is driven by `UIDevice`/`GeometryReader` orientation detection, switching between the portrait and landscape arrangements from the approved mockup.

## Mouse input

### New C++ core capability: standard positioned mouse messages

A new pair of functions alongside the existing raw-input ones in `windows_emulator` (`src/windows-emulator/windows_emulator.hpp`/`.cpp`):

```cpp
void deliver_mouse_move(int32_t x, int32_t y);
void deliver_mouse_button(int32_t x, int32_t y, uint32_t message, uint16_t button_data = 0);
```

These synthesize a real `WM_MOUSEMOVE` (for the move) and `WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_RBUTTONDOWN`/`WM_RBUTTONUP` (for `deliver_mouse_button`, `message` being whichever of those applies) posted to the foreground window, with `x`/`y` packed into `lParam` the standard way (`MAKELPARAM(x, y)`, client coordinates) — mirroring exactly how `deliver_raw_input` already resolves its target window (explicit target if given, else `this->process.foreground_window`) and posts via `thread->post_message`, just with a different message/`lParam` shape instead of the `WM_INPUT`/token indirection raw input uses. No new message-queue plumbing is needed beyond what `deliver_raw_input` already proves works.

### Touchscreen mode

`EmulatorHostView` (in `EmulatorView.swift`) gains a tap handler that reports the tap's location in the view's own coordinate space. That coordinate must be transformed from view-space into guest-frame-space before reaching the C++ layer: the guest view is letterboxed (aspect-fit) inside its container, so the transform accounts for the current scale factor and offset (derived from the container's size vs. the actual incoming frame's reported width/height — the same information driving the dynamic-aspect-ratio fix below). `SogenBridge.mm` gains `deliverMouseClick:(CGPoint)guestPoint` (or similar), calling `deliver_mouse_move` then immediately `deliver_mouse_button` (down, then up) with the transformed coordinates.

### Trackpad mode

A pan/drag gesture recognizer on `EmulatorHostView` reports incremental translation deltas (UIKit's `UIPanGestureRecognizer` already reports these directly — no coordinate transform needed, since these are relative, not absolute). Each delta calls the *existing* `deliverMouseDelta:(CGFloat)dx dy:(CGFloat)dy` path (a thin new `SogenBridge.mm` wrapper around the already-existing `deliver_raw_mouse_input(dx, dy, 0)`), and a separate tap gesture (not the pan) triggers a click via the same `deliver_raw_mouse_input` call with button flags set, carrying no position — exactly matching today's `queue_left_click()` semantics, just reachable alongside the pan recognizer instead of being the only recognizer.

### Mode switching

A `@State`/shared enum (`.touchscreen` / `.trackpad`) in `EmulationView`, toggled by the mouse-mode icon, determines which gesture recognizers are active on `EmulatorHostView` at any time (both sets can be always-attached with the inactive one's handler short-circuiting, or attached/detached on mode switch — an implementation-level choice for the plan).

### Gestures

- **Two-finger tap → right-click**: a second `UITapGestureRecognizer` with `numberOfTouchesRequired = 2`, delivering a right-button click at the tapped location in touchscreen mode (via `deliver_mouse_button` with `WM_RBUTTONDOWN`/`WM_RBUTTONUP`), or a raw-input right-click (button flags) with no position in trackpad mode.
- **Press-and-hold-then-drag → click-and-drag**: a `UILongPressGestureRecognizer` that, once triggered, tracks its own subsequent movement (a long-press recognizer reports `.changed` states with updated `location(in:)` as the finger continues moving) as a held-button drag — `deliver_mouse_button` (down) at the press location, then repeated `deliver_mouse_move` calls as the finger moves, then `deliver_mouse_button` (up) on release. This applies to touchscreen mode; trackpad mode's equivalent is a long-press-then-pan producing a button-held sequence of `deliver_raw_mouse_input` deltas.

## Dynamic aspect ratio

`IosUiBackend.mm`'s frame-delivery path (already logging `frame %llu ... %dx%d ...` per the existing `[ios-ui]` log line) already knows each frame's real width/height. That size needs to reach `EmulationView` (e.g., via the same `onLogLine`-style callback mechanism, or a new small `onFrameSize: (CGSize) -> Void` callback) so the guest-view container's `.aspectRatio(...)` modifier can be computed from the actual incoming frame instead of the hardcoded `320.0/180.0` constant. This same real size is also what touchscreen mode's coordinate transform (above) needs to convert a tap in view-space into a position in guest-frame-space.

## Logs overlay

A dismissible panel (matching the existing log-line rendering already in `ContentView.swift`'s `ScrollView`/`LazyVStack`, moved into a reusable view) toggled by the logs icon, overlaying the emulation screen without navigating away from it.

## Testing

No C++ unit-test coverage plan needed for the new `deliver_mouse_move`/`deliver_mouse_button` functions beyond mirroring the existing `deliver_raw_mouse_input`/`deliver_raw_keyboard_input`'s own (lack of dedicated unit tests — they're exercised via real device testing only, matching this project's established pattern for all iOS-side input work so far). Verification is manual, on real device: confirm touchscreen mode correctly clicks buttons/dialogs in a real Win32 UI app (not just the raw-input-only `native-gpu-clear-sample.exe`), confirm trackpad mode still drives the existing sample's raw-input path unchanged, confirm two-finger-tap and long-press-drag gestures, confirm portrait/landscape layout switching, confirm the logs overlay, confirm the guest view now scales correctly for the actual frame aspect ratio.
