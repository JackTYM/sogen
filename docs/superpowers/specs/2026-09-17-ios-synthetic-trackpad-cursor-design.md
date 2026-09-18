# Synthetic Trackpad Cursor — Design

## Problem

Trackpad mode delivers relative mouse deltas to the guest via raw input (`RegisterRawInputDevices`/`WM_INPUT`), exactly matching how a real game with a hidden OS cursor and custom in-game cursor rendering works. But sogen's guest environment has no desktop compositor with its own cursor sprite — nothing visible tracks the pointer's position unless the specific guest app renders its own cursor in response. For any guest that doesn't do that (which is every guest currently bundled in this app), trackpad-mode dragging is blind: there's no way to see where you are while dragging.

## Goal

The iOS app itself draws a small synthetic cursor overlay in trackpad mode, purely as a host-side visual affordance — independent of whatever the guest renders. It respects the guest's own request to hide the cursor (`ShowCursor(FALSE)`/`SetCursor(NULL)`, the standard pattern a game uses when it wants to render its own cursor or run as a fullscreen game with no OS cursor), so the overlay disappears exactly when a real Windows cursor would.

## What already exists (confirmed via codebase research, not assumed)

The syscall-to-backend plumbing for cursor visibility is **already fully implemented** on the emulator side, with one missing link:

- `src/windows-emulator/syscalls/user.cpp`: `handle_NtUserShowCursor` tracks `process_context::cursor_show_count` (a counter, matching real `ShowCursor`'s increment/decrement semantics); `handle_NtUserSetCursor` tracks `process_context::cursor_shape_visible` (`SetCursor(NULL)` clears it). Both call `apply_cursor_visibility(c)`, which calls `c.win_emu.ui().set_cursor_visibility(cursor_show_count >= 0 && cursor_shape_visible)`.
- `src/emulator-platform/platform/ui_backend.hpp` declares `virtual void set_cursor_visibility(bool visible)` on the `ui_backend` interface (default no-op).
- `src/windows-emulator/ui_backends/sdl_ui_backend.cpp` already implements it for desktop via `SDL_ShowCursor()`/`SDL_HideCursor()`.
- **`ios_ui_backend` (`tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp`/`.mm`) does not override `set_cursor_visibility` at all** — it silently falls through to the base no-op. This is the only gap; the syscall handlers, state tracking, and the call site are all already correct and already firing today, just against nothing.

The `frame_size_sink`/`onFrameSize` pattern (present_surface detects a change → invokes a `std::function` sink → `SogenBridge.mm` forwards to an ObjC block property → SwiftUI consumes it as `@State`) is the exact shape to replicate for cursor visibility.

## Design

**`ios_ui_backend`** gains:
- `using cursor_visibility_sink = std::function<void(bool visible)>;` + `set_cursor_visibility_sink(cursor_visibility_sink)` setter + member, mirroring `frame_size_sink` exactly.
- An override of `void set_cursor_visibility(bool visible) override;` that invokes the sink if set.

**`SogenBridge`** gains an `onCursorVisibilityChange: ((Bool) -> Void)?`-equivalent block property (ObjC: `@property (nonatomic, copy, nullable) void (^onCursorVisibilityChange)(BOOL visible);`), wired in the same construction block as `set_frame_size_sink`, dispatching to the main queue the same way `onFrameSize` does.

**`EmulationView`** gains:
- `@State private var cursorVisible = true` — starts visible (a fresh Windows process's cursor is visible until an app explicitly hides it), updated by `onCursorVisibilityChange`.
- `@State private var cursorPosition: CGPoint` — view-space coordinates of the synthetic cursor, reset to the center of the guest's displayed content rect whenever `mode` transitions to `.trackpad`.
- A small filled-circle overlay (simple dot, per the approved choice) rendered at `cursorPosition`, shown only when `mode == .trackpad && cursorVisible`.

**`EmulatorHostView.handlePan`** (already computing a pan gesture's translation delta for delivery to the guest via `onDeliverDelta`) additionally accumulates that same delta into the cursor's local position and reports it upward (a new callback, e.g. `onCursorPositionChange: ((CGPoint) -> Void)?`, alongside the existing `onDeliverDelta`), clamped to the guest content's displayed bounds — reusing the same letterbox scale/offset math `toGuestPoint` already computes, so the synthetic cursor never visually leaves the guest's rendered area.

## Data flow

1. Guest calls `ShowCursor(FALSE)` (or `SetCursor(NULL)`) → existing syscall handler → existing `process_context` state → existing `apply_cursor_visibility` → `ui().set_cursor_visibility(false)` → **new** `ios_ui_backend::set_cursor_visibility` override → **new** `cursor_visibility_sink` → **new** `SogenBridge.onCursorVisibilityChange` → **new** `EmulationView`'s `cursorVisible = false` → overlay disappears.
2. User pans in trackpad mode → `EmulatorHostView.handlePan` → (existing) `onDeliverDelta` sends the raw delta to the guest, AND (new) accumulates/clamps the same delta into `cursorPosition`, reported upward → overlay moves, if currently visible.
3. Mode switches to `.trackpad` → `cursorPosition` resets to the guest content rect's center.

## Explicitly out of scope

- **`SetCursorPos`-driven absolute repositioning.** `ui_backend.hpp` also declares `set_cursor_position` (for when a guest programmatically snaps the cursor, e.g. FPS-style re-centering every frame) — not implemented in this round. The synthetic cursor only tracks accumulated relative motion, so a guest that both hides the cursor and repeatedly calls `SetCursorPos` (a common combo) could theoretically let the local `cursorPosition` drift from where a real cursor logically would be. Since the overlay is hidden in exactly that scenario, this shouldn't be visually noticeable; flagged for a future round if it turns out to matter in practice.
- Touchscreen mode is entirely unaffected — this feature only applies to trackpad mode.
- No changes to the guest samples (`native-gpu-clear-sample`, `mouse-input-test-sample`) — this is purely a host-side (iOS app) rendering affordance.
- No changes to Arcade Mode or the on-screen keyboard — those remain their own separate, not-yet-started sub-projects.
