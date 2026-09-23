# Synthetic Trackpad Cursor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In trackpad mode, the iOS app draws a small synthetic on-screen cursor that tracks accumulated drag deltas locally, and hides itself when the guest calls `ShowCursor(FALSE)`/`SetCursor(NULL)` (the standard pattern a game uses when it wants to render its own cursor or run fullscreen).

**Architecture:** The syscall-to-`ui_backend` plumbing for cursor visibility (`NtUserShowCursor`/`NtUserSetCursor` → `process_context` state → `ui().set_cursor_visibility(bool)`) already exists end-to-end in `windows_emulator`; the only gap is that `ios_ui_backend` never overrides that virtual. Add a `cursor_visibility_sink` mirroring the existing `frame_size_sink` pattern exactly, forward it through `SogenBridge` as an ObjC block property, and consume it as `@State` in `EmulationView`. Separately, `EmulatorHostView` (which already computes trackpad-mode pan deltas for delivery to the guest) also accumulates those same deltas into a local cursor position, clamped to the guest content's displayed rect, and reports it upward for the overlay to render.

**Tech Stack:** C++20 (`ios_ui_backend`), Objective-C++ (`SogenBridge`), Swift/SwiftUI, MinGW-cross-compiled C++ guest sample.

**User decisions (already made):**
- Cursor style: simple filled dot, not an arrow glyph.
- Reset position: center of the guest content rect every time trackpad mode is entered, not remembered across mode switches.
- `SetCursorPos`-driven absolute repositioning is explicitly out of scope (spec-level decision) — only relative motion and visibility are handled.
- Since neither guest sample currently calls `ShowCursor`/`SetCursor`, `mouse-input-test-sample.exe`'s existing right-click (`WM_RBUTTONDOWN`) handler is extended to toggle cursor visibility, specifically so the hide/show path has something real to verify on-device (chosen over leaving it code-review-only).

---

## Task 1: `ios_ui_backend` + `SogenBridge` — cursor-visibility sink

**Goal:** `ios_ui_backend` overrides `ui_backend::set_cursor_visibility(bool)` and forwards it through a new sink; `SogenBridge` exposes that as an ObjC block property, wired the same way `onFrameSize` already is.

**Files:**
- Modify: `tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp`
- Modify: `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm`
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.h`
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.mm`

**Acceptance Criteria:**
- [ ] `ios_ui_backend` declares `cursor_visibility_sink` (`std::function<void(bool)>`), a setter, a member, and overrides `set_cursor_visibility(bool) override` from the base `ui_backend` class (confirmed exact base signature: `virtual void set_cursor_visibility(const bool visible)` in `src/emulator-platform/platform/ui_backend.hpp`).
- [ ] The override logs the change (matching the file's existing `emit_log` convention) and invokes the sink if set.
- [ ] `SogenEmulator` gains `onCursorVisibilityChange: ((Bool) -> Void)?`, wired in the same construction block as `onFrameSize`, dispatching to the main queue the same way.
- [ ] Builds clean.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read `IosUiBackend.hpp`, `IosUiBackend.mm`, `SogenBridge.h`, `SogenBridge.mm` in full** to confirm exact current content before editing.

- [ ] **Step 2: Edit `IosUiBackend.hpp`**

Find:
```cpp
        using log_sink = std::function<void(const char* line)>;
        using frame_size_sink = std::function<void(int32_t width, int32_t height)>;

        explicit ios_ui_backend(CALayer* layer);
        ~ios_ui_backend() override;

        void set_event_sink(event_sink sink) override;
        void pump_events() override;
        void present_surface(hwnd window, const ui_surface_desc& surface) override;

        void set_raw_mouse_sink(raw_mouse_sink sink);
        void set_mouse_move_sink(mouse_move_sink sink);
        void set_mouse_button_sink(mouse_button_sink sink);
        void set_log_sink(log_sink sink);
        void set_frame_size_sink(frame_size_sink sink);
        void set_layer(CALayer* layer);
```

Replace with:
```cpp
        using log_sink = std::function<void(const char* line)>;
        using frame_size_sink = std::function<void(int32_t width, int32_t height)>;
        using cursor_visibility_sink = std::function<void(bool visible)>;

        explicit ios_ui_backend(CALayer* layer);
        ~ios_ui_backend() override;

        void set_event_sink(event_sink sink) override;
        void pump_events() override;
        void present_surface(hwnd window, const ui_surface_desc& surface) override;
        void set_cursor_visibility(bool visible) override;

        void set_raw_mouse_sink(raw_mouse_sink sink);
        void set_mouse_move_sink(mouse_move_sink sink);
        void set_mouse_button_sink(mouse_button_sink sink);
        void set_log_sink(log_sink sink);
        void set_frame_size_sink(frame_size_sink sink);
        void set_cursor_visibility_sink(cursor_visibility_sink sink);
        void set_layer(CALayer* layer);
```

Find:
```cpp
        log_sink log_sink_{};
        frame_size_sink frame_size_sink_{};
```

Replace with:
```cpp
        log_sink log_sink_{};
        frame_size_sink frame_size_sink_{};
        cursor_visibility_sink cursor_visibility_sink_{};
```

- [ ] **Step 3: Edit `IosUiBackend.mm`**

Find:
```cpp
    void ios_ui_backend::set_frame_size_sink(frame_size_sink sink)
    {
        this->frame_size_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_layer(CALayer* layer)
```

Replace with:
```cpp
    void ios_ui_backend::set_frame_size_sink(frame_size_sink sink)
    {
        this->frame_size_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_cursor_visibility_sink(cursor_visibility_sink sink)
    {
        this->cursor_visibility_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_cursor_visibility(const bool visible)
    {
        this->emit_log("[ios-ui] cursor visibility=%s", visible ? "visible" : "hidden");
        if (this->cursor_visibility_sink_)
        {
            this->cursor_visibility_sink_(visible);
        }
    }

    void ios_ui_backend::set_layer(CALayer* layer)
```

- [ ] **Step 4: Edit `SogenBridge.h`**

Find:
```objc
/// Invoked on the main queue whenever the guest's presented frame size changes.
@property (nonatomic, copy, nullable) void (^onFrameSize)(CGSize size);

@end
```

Replace with:
```objc
/// Invoked on the main queue whenever the guest's presented frame size changes.
@property (nonatomic, copy, nullable) void (^onFrameSize)(CGSize size);

/// Invoked on the main queue whenever the guest calls ShowCursor/SetCursor to show or hide the
/// cursor (e.g. a fullscreen game hiding it to render its own).
@property (nonatomic, copy, nullable) void (^onCursorVisibilityChange)(BOOL visible);

@end
```

- [ ] **Step 5: Edit `SogenBridge.mm`**

Find:
```objc
            ui_raw->set_frame_size_sink([weakSelf](const int32_t width, const int32_t height) {
                SogenEmulator* strongSelf = weakSelf;
                if (!strongSelf || !strongSelf.onFrameSize)
                {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                  strongSelf.onFrameSize(CGSizeMake(width, height));
                });
            });
```

Replace with:
```objc
            ui_raw->set_frame_size_sink([weakSelf](const int32_t width, const int32_t height) {
                SogenEmulator* strongSelf = weakSelf;
                if (!strongSelf || !strongSelf.onFrameSize)
                {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                  strongSelf.onFrameSize(CGSizeMake(width, height));
                });
            });
            ui_raw->set_cursor_visibility_sink([weakSelf](const bool visible) {
                SogenEmulator* strongSelf = weakSelf;
                if (!strongSelf || !strongSelf.onCursorVisibilityChange)
                {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                  strongSelf.onCursorVisibilityChange(visible ? YES : NO);
                });
            });
```

- [ ] **Step 6: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`. If the linker fails with undefined symbols referencing something in `windows_emulator`, the prebuilt static library at `build/ios-embed-device/artifacts/libwindows-emulator.a` may be stale — run `cmake --build --preset=ios-embed-device` to relink it, then retry. This task doesn't touch `windows_emulator` itself, so this is unlikely to be needed, but it's a known, previously-seen build-environment staleness issue in this project.

- [ ] **Step 7: Commit**

```bash
git add tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp tools/sogen-ios/Sources/Bridge/IosUiBackend.mm tools/sogen-ios/Sources/Bridge/SogenBridge.h tools/sogen-ios/Sources/Bridge/SogenBridge.mm
git commit -m "feat(ios): wire cursor-visibility sink through ios_ui_backend and SogenBridge"
```

If `git status`/`git diff`/`git add`/`git commit` misbehave via whatever shell wrapper is active (a known `rtk` wrapper glitch in this project that spuriously refuses plain git commands), use `/usr/bin/git` directly instead — this reliably works around it.

---

## Task 2: `EmulatorView` + `EmulationView` — synthetic trackpad cursor

**Goal:** `EmulatorHostView` accumulates trackpad-mode pan deltas into a local cursor position (clamped to the guest content's displayed rect, reset to center on entering trackpad mode) and reports it upward; `EmulationView` renders a dot overlay at that position, visible only in trackpad mode and only while the guest hasn't hidden the cursor.

**Files:**
- Modify: `tools/sogen-ios/Sources/EmulatorView.swift`
- Modify: `tools/sogen-ios/Sources/EmulationView.swift`

**Acceptance Criteria:**
- [ ] `EmulatorHostView` gains `onCursorPositionChange: ((CGPoint) -> Void)?` and a private `cursorPosition` that's reset to the view's bounds center exactly when `mode` transitions INTO `.trackpad` (not on every redundant same-value assignment — `mode`'s `didSet` guards on `oldValue`).
- [ ] `handlePan` (trackpad mode) accumulates its translation into `cursorPosition`, clamps it to the guest content's displayed rect (same letterbox math `toGuestPoint` already uses), and reports it via `onCursorPositionChange`.
- [ ] `EmulationView` gains `@State private var cursorVisible = true` (starts visible) and `@State private var cursorPosition: CGPoint`, wired to the emulator's `onCursorVisibilityChange` and the new `EmulatorView.onCursorPositionChange` respectively.
- [ ] A small white dot overlay renders at `cursorPosition`, shown only when `mode == .trackpad && cursorVisible`, `.allowsHitTesting(false)` so it never intercepts gestures.
- [ ] Builds clean.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current `EmulatorView.swift` and `EmulationView.swift` in full** to confirm exact current content before editing (this task depends on Task 1's `onCursorVisibilityChange` property existing on `SogenEmulator` — confirm Task 1 is committed first).

- [ ] **Step 2: Edit `EmulatorView.swift` — extend `EmulatorHostView`'s stored properties**

Find:
```swift
    var mode: MouseMode = .touchscreen {
        didSet {
            panRecognizer.isEnabled = mode == .trackpad
        }
    }
    var frameSize: CGSize = .zero
    var onDeliverMove: ((CGPoint) -> Void)?
    var onDeliverButton: ((CGPoint, UInt32) -> Void)?
    var onDeliverDelta: ((CGFloat, CGFloat) -> Void)?
    var onDeliverClick: (() -> Void)?
    var onDeliverRightClick: (() -> Void)?
```

Replace with:
```swift
    var mode: MouseMode = .touchscreen {
        didSet {
            panRecognizer.isEnabled = mode == .trackpad
            // The synthetic trackpad cursor has no meaningful position to remember from
            // touchscreen mode (which never moves it), so start it fresh each time trackpad
            // mode is entered, rather than carrying over a stale/default value.
            if mode == .trackpad && oldValue != .trackpad {
                resetCursorPosition()
            }
        }
    }
    var frameSize: CGSize = .zero
    var onDeliverMove: ((CGPoint) -> Void)?
    var onDeliverButton: ((CGPoint, UInt32) -> Void)?
    var onDeliverDelta: ((CGFloat, CGFloat) -> Void)?
    var onDeliverClick: (() -> Void)?
    var onDeliverRightClick: (() -> Void)?
    var onCursorPositionChange: ((CGPoint) -> Void)?

    private var cursorPosition: CGPoint = .zero
```

- [ ] **Step 3: Edit `EmulatorView.swift` — add the clamp/reset helpers right after `toGuestPoint`**

Find:
```swift
    /// Transforms a point in this view's own coordinate space into guest-frame pixel space,
    /// accounting for the aspect-fit letterbox scale/offset between the two.
    private func toGuestPoint(_ viewPoint: CGPoint) -> CGPoint {
        guard frameSize.width > 0, frameSize.height > 0, bounds.width > 0, bounds.height > 0 else {
            return viewPoint
        }
        let scale = min(bounds.width / frameSize.width, bounds.height / frameSize.height)
        let displayedWidth = frameSize.width * scale
        let displayedHeight = frameSize.height * scale
        let offsetX = (bounds.width - displayedWidth) / 2
        let offsetY = (bounds.height - displayedHeight) / 2
        let guestX = (viewPoint.x - offsetX) / scale
        let guestY = (viewPoint.y - offsetY) / scale
        return CGPoint(x: guestX, y: guestY)
    }
```

Replace with:
```swift
    /// Transforms a point in this view's own coordinate space into guest-frame pixel space,
    /// accounting for the aspect-fit letterbox scale/offset between the two.
    private func toGuestPoint(_ viewPoint: CGPoint) -> CGPoint {
        guard frameSize.width > 0, frameSize.height > 0, bounds.width > 0, bounds.height > 0 else {
            return viewPoint
        }
        let scale = min(bounds.width / frameSize.width, bounds.height / frameSize.height)
        let displayedWidth = frameSize.width * scale
        let displayedHeight = frameSize.height * scale
        let offsetX = (bounds.width - displayedWidth) / 2
        let offsetY = (bounds.height - displayedHeight) / 2
        let guestX = (viewPoint.x - offsetX) / scale
        let guestY = (viewPoint.y - offsetY) / scale
        return CGPoint(x: guestX, y: guestY)
    }

    /// Clamps a point (in this view's own coordinate space) to the guest content's displayed
    /// rect, using the same aspect-fit letterbox scale/offset toGuestPoint uses, so the
    /// synthetic trackpad cursor never visually leaves the guest's rendered area.
    private func clampToGuestRect(_ point: CGPoint) -> CGPoint {
        guard frameSize.width > 0, frameSize.height > 0, bounds.width > 0, bounds.height > 0 else {
            return point
        }
        let scale = min(bounds.width / frameSize.width, bounds.height / frameSize.height)
        let displayedWidth = frameSize.width * scale
        let displayedHeight = frameSize.height * scale
        let offsetX = (bounds.width - displayedWidth) / 2
        let offsetY = (bounds.height - displayedHeight) / 2
        let clampedX = min(max(point.x, offsetX), offsetX + displayedWidth)
        let clampedY = min(max(point.y, offsetY), offsetY + displayedHeight)
        return CGPoint(x: clampedX, y: clampedY)
    }

    private func resetCursorPosition() {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let center = CGPoint(x: bounds.midX, y: bounds.midY)
        cursorPosition = center
        onCursorPositionChange?(center)
    }
```

- [ ] **Step 4: Edit `EmulatorView.swift` — update `handlePan`**

Find:
```swift
    @objc private func handlePan(_ recognizer: UIPanGestureRecognizer) {
        guard mode == .trackpad else { return }
        let translation = recognizer.translation(in: self)
        onDeliverDelta?(translation.x, translation.y)
        recognizer.setTranslation(.zero, in: self)
    }
```

Replace with:
```swift
    @objc private func handlePan(_ recognizer: UIPanGestureRecognizer) {
        guard mode == .trackpad else { return }
        let translation = recognizer.translation(in: self)
        onDeliverDelta?(translation.x, translation.y)
        cursorPosition = clampToGuestRect(
            CGPoint(x: cursorPosition.x + translation.x, y: cursorPosition.y + translation.y))
        onCursorPositionChange?(cursorPosition)
        recognizer.setTranslation(.zero, in: self)
    }
```

- [ ] **Step 5: Edit `EmulatorView.swift` — add the new callback to the `EmulatorView` struct**

Find:
```swift
struct EmulatorView: UIViewRepresentable {
    let onViewReady: (CALayer) -> Void
    let mode: MouseMode
    let frameSize: CGSize
    let onDeliverMove: (CGPoint) -> Void
    let onDeliverButton: (CGPoint, UInt32) -> Void
    let onDeliverDelta: (CGFloat, CGFloat) -> Void
    let onDeliverClick: () -> Void
    let onDeliverRightClick: () -> Void

    func makeUIView(context: Context) -> EmulatorHostView {
        let view = EmulatorHostView(frame: .zero)
        configure(view)
        onViewReady(view.layer)
        return view
    }

    func updateUIView(_ uiView: EmulatorHostView, context: Context) {
        configure(uiView)
    }

    private func configure(_ view: EmulatorHostView) {
        view.mode = mode
        view.frameSize = frameSize
        view.onDeliverMove = onDeliverMove
        view.onDeliverButton = onDeliverButton
        view.onDeliverDelta = onDeliverDelta
        view.onDeliverClick = onDeliverClick
        view.onDeliverRightClick = onDeliverRightClick
    }
}
```

Replace with:
```swift
struct EmulatorView: UIViewRepresentable {
    let onViewReady: (CALayer) -> Void
    let mode: MouseMode
    let frameSize: CGSize
    let onDeliverMove: (CGPoint) -> Void
    let onDeliverButton: (CGPoint, UInt32) -> Void
    let onDeliverDelta: (CGFloat, CGFloat) -> Void
    let onDeliverClick: () -> Void
    let onDeliverRightClick: () -> Void
    let onCursorPositionChange: (CGPoint) -> Void

    func makeUIView(context: Context) -> EmulatorHostView {
        let view = EmulatorHostView(frame: .zero)
        configure(view)
        onViewReady(view.layer)
        return view
    }

    func updateUIView(_ uiView: EmulatorHostView, context: Context) {
        configure(uiView)
    }

    private func configure(_ view: EmulatorHostView) {
        view.mode = mode
        view.frameSize = frameSize
        view.onDeliverMove = onDeliverMove
        view.onDeliverButton = onDeliverButton
        view.onDeliverDelta = onDeliverDelta
        view.onDeliverClick = onDeliverClick
        view.onDeliverRightClick = onDeliverRightClick
        view.onCursorPositionChange = onCursorPositionChange
    }
}
```

- [ ] **Step 6: Edit `EmulationView.swift` — add state**

Find:
```swift
    @State private var mode: MouseMode = .touchscreen
    @State private var frameSize: CGSize = .zero
    @State private var showLogs = false
    @State private var showKeyboard = false
```

Replace with:
```swift
    @State private var mode: MouseMode = .touchscreen
    @State private var frameSize: CGSize = .zero
    @State private var showLogs = false
    @State private var showKeyboard = false
    @State private var cursorVisible = true
    @State private var cursorPosition: CGPoint = .zero
```

- [ ] **Step 7: Edit `EmulationView.swift` — add the overlay**

Find:
```swift
                topBar
                    .padding(8)
                    .background(isLandscape ? Color.black.opacity(0.4) : Color.clear)

                if showLogs {
```

Replace with:
```swift
                if mode == .trackpad && cursorVisible {
                    Circle()
                        .fill(Color.white)
                        .frame(width: 14, height: 14)
                        .shadow(radius: 2)
                        .position(cursorPosition)
                        .allowsHitTesting(false)
                }

                topBar
                    .padding(8)
                    .background(isLandscape ? Color.black.opacity(0.4) : Color.clear)

                if showLogs {
```

- [ ] **Step 8: Edit `EmulationView.swift` — wire `onCursorVisibilityChange`**

Find:
```swift
        .onAppear {
            emulator.onFrameSize = { size in
                frameSize = size
            }
        }
```

Replace with:
```swift
        .onAppear {
            emulator.onFrameSize = { size in
                frameSize = size
            }
            emulator.onCursorVisibilityChange = { visible in
                cursorVisible = visible
            }
        }
```

- [ ] **Step 9: Edit `EmulationView.swift` — wire `onCursorPositionChange`**

Find:
```swift
    private var guestView: some View {
        EmulatorView(
            onViewReady: { layer in
                emulator.attach(layer)
            },
            mode: mode,
            frameSize: frameSize,
            onDeliverMove: { point in emulator.deliverMouseMove(point) },
            onDeliverButton: { point, message in emulator.deliverMouseButton(point, message: message) },
            onDeliverDelta: { dx, dy in emulator.deliverMouseDelta(dx, dy: dy) },
            onDeliverClick: { emulator.deliverTap() },
            onDeliverRightClick: { emulator.deliverRightClick() }
        )
    }
```

Replace with:
```swift
    private var guestView: some View {
        EmulatorView(
            onViewReady: { layer in
                emulator.attach(layer)
            },
            mode: mode,
            frameSize: frameSize,
            onDeliverMove: { point in emulator.deliverMouseMove(point) },
            onDeliverButton: { point, message in emulator.deliverMouseButton(point, message: message) },
            onDeliverDelta: { dx, dy in emulator.deliverMouseDelta(dx, dy: dy) },
            onDeliverClick: { emulator.deliverTap() },
            onDeliverRightClick: { emulator.deliverRightClick() },
            onCursorPositionChange: { point in cursorPosition = point }
        )
    }
```

- [ ] **Step 10: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 11: Commit**

```bash
git add tools/sogen-ios/Sources/EmulatorView.swift tools/sogen-ios/Sources/EmulationView.swift
git commit -m "feat(ios): render synthetic trackpad cursor, hidden when the guest hides it"
```

---

## Task 3: `mouse-input-test-sample` — toggle cursor visibility on right-click

**Goal:** So the hide/show path has something real to verify on-device, extend the guest's existing `WM_RBUTTONDOWN` handler to also call `ShowCursor`, toggling visibility each time and logging the new state.

**Files:**
- Modify: `src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp`

**Acceptance Criteria:**
- [ ] `WM_RBUTTONDOWN`'s existing handler additionally toggles a static `cursor_visible` flag (starting `true`, matching real Windows' default), calls `ShowCursor(cursor_visible ? TRUE : FALSE)`, and logs the resulting state alongside the existing coordinate log line.
- [ ] `WM_RBUTTONUP`'s handler is unchanged (the toggle happens on down, not up, so it fires exactly once per right-click/two-finger-tap).
- [ ] Compiles cleanly with the project's 32-bit MinGW toolchain.

**Verify:** `i686-w64-mingw32-g++ -O2 -std=c++20 -static -static-libgcc -static-libstdc++ -I src/dxgk-command-protocol src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp -o /tmp/mouse-input-test-sample.exe -luser32 -lgdi32` → exits 0.

**Steps:**

- [ ] **Step 1: Read `src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp` in full** to confirm the exact current `WM_RBUTTONDOWN` handler text before editing.

- [ ] **Step 2: Edit the `WM_RBUTTONDOWN` case in `window_proc`**

Find:
```cpp
        case WM_RBUTTONDOWN:
            std::printf("[mits] WM_RBUTTONDOWN x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_RBUTTONUP:
            std::printf("[mits] WM_RBUTTONUP x=%d y=%d\n", GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
```

Replace with:
```cpp
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
```

- [ ] **Step 3: Build and verify**

```bash
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I src/dxgk-command-protocol \
  src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp \
  -o /tmp/mouse-input-test-sample.exe \
  -luser32 -lgdi32
```

Expected: exit 0.

- [ ] **Step 4: Commit**

```bash
git add src/samples/mouse-input-test-sample/mouse-input-test-sample.cpp
git commit -m "feat(samples): toggle cursor visibility on right-click in mouse-input-test-sample"
```

---

## Task 4: Real-device verification

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in the acceptance criteria has been re-validated independently, with output captured.

**Goal:** Confirm the synthetic trackpad cursor actually tracks drags and actually hides/shows in response to the guest's own `ShowCursor` calls, on real hardware.

**Files:** None (packaging/verification only — uses Tasks 1-3's outputs).

**Acceptance Criteria:**
- [ ] Re-stage the emulation root and re-bundle `mouse-input-test-sample.exe` (it changed in Task 3) into `Resources/`; `native-gpu-clear-sample.exe` is unchanged and doesn't need rebuilding, but re-copying it is harmless.
- [ ] Build, archive, and package a fresh `.ipa`; `otool -hv` confirms `NOUNDEFS`; deliver to the user's iCloud Drive, replacing the previous copy.
- [ ] On real device, boot `mouse-input-test-sample` (via "Boot Input Test"), switch to trackpad mode: dragging moves a visible white dot that stays within the guest's rendered content area.
- [ ] Switching into trackpad mode resets the dot to the center of the guest content.
- [ ] Switch to touchscreen mode, two-finger-tap (right-click) once: the log shows `cursor_visible=false`; switch back to trackpad mode — the dot is now hidden.
- [ ] Two-finger-tap again (touchscreen mode): the log shows `cursor_visible=true`; switch back to trackpad mode — the dot is visible again.
- [ ] Regression check: the default boot (`native-gpu-clear-sample`) and touchscreen-mode tapping still work exactly as before (no cursor overlay ever appears outside trackpad mode).

**Verify:** Manual, on-device, for each bullet above. Capture the on-screen log for each.

**Steps:**

- [ ] **Step 1: Re-stage and re-bundle**

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

- [ ] **Step 4: User tests each acceptance-criteria bullet**

User confirms each bullet above and shares the resulting log/description of behavior for any that don't match expectations.

- [ ] **Step 5: Fix and re-verify**

Any bullet that fails gets a real fix (not a workaround) dispatched as its own follow-up round, re-verified the same way, before this task is considered closed.

- [ ] **Step 6: No commit expected**

This task produces build/packaging artifacts only (all gitignored). If `git status` shows anything unexpected and trackable, investigate before assuming it's safe to ignore.

```json:metadata
{"userGate": true, "tags": ["user-gate"], "requiresUserSpecification": false, "files": [], "verifyCommand": "manual on-device", "acceptanceCriteria": ["both exes staged and bundled", "archive succeeds with NOUNDEFS", "ipa delivered to iCloud Drive", "trackpad drag moves dot within guest content bounds", "entering trackpad mode resets dot to center", "guest ShowCursor(FALSE) via right-click hides the dot", "guest ShowCursor(TRUE) via right-click shows the dot again", "default boot and touchscreen mode unaffected (regression)"], "modelTier": "standard"}
```
