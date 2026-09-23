# Emulation Screen Chrome + Mouse Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace sogen-ios's single flat view with a real setup-screen/emulation-screen navigation split, and add a second, positioned mouse-input path so ordinary Win32 UI apps (not just raw-input-consuming games) can actually be clicked on.

**Architecture:** A new pair of `windows_emulator` functions synthesize standard positioned mouse messages (`WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/etc.), following the exact same "queue on the UI thread, drain and deliver on the emulator thread inside `pump_events()`" pattern the existing raw-input path already uses (for the same reason: touching process state off the emulator thread is unsafe). `ContentView` splits into `SetupView` (unchanged content) and a new `EmulationView` that owns chrome, gesture recognizers, and a mouse-mode switch between the new positioned path (touchscreen mode) and the existing raw-input path (trackpad mode).

**Tech Stack:** C++20 (`src/windows-emulator/`, `src/emulator-platform/`), Objective-C++ (`tools/sogen-ios/Sources/Bridge/`), Swift/SwiftUI (`tools/sogen-ios/Sources/`).

**User decisions (already made):**
- Two real screens (setup, emulation) with back-button navigation; setup screen's *content* is unchanged in this plan (its redesign is a separate future plan).
- Emulation screen: portrait — back top-left, icon row top-right (mouse-mode/logs/keyboard), keyboard area fixed at bottom. Landscape — guest view fills the screen, same icons overlay, keyboard floats and is collapsible.
- Keyboard icon is present and toggleable in this plan but renders no keyboard content yet (that's a separate future plan) — it's a wired no-op placeholder.
- Two switchable mouse modes: **touchscreen** (tap = click at that position, via new standard message synthesis) and **trackpad** (drag = relative movement, tap = click, via the existing raw-input path). User picks per what's running; switching is instant.
- Gestures, fixed for this plan (remapping is a future settings feature): two-finger tap = right-click; press-and-hold-then-drag = click-and-drag.
- Guest-view container's aspect ratio is computed from the actual incoming frame size, not hardcoded.
- Logs: hidden by default on the emulation screen, shown via a dismissible overlay toggled by the logs icon.

Full design rationale: `docs/superpowers/specs/2026-09-16-ios-input-ux-design.md`.

---

### Task 1: `windows_emulator` — standard positioned mouse messages

**Goal:** Add `deliver_mouse_move`/`deliver_mouse_button`, synthesizing real positioned `WM_MOUSEMOVE`/`WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_RBUTTONDOWN`/`WM_RBUTTONUP` messages, mirroring the existing `deliver_raw_mouse_input`/`deliver_raw_input` pattern exactly but with real coordinates instead of a `WM_INPUT` token.

**Files:**
- Modify: `src/windows-emulator/windows_emulator.hpp:247-248` (declarations)
- Modify: `src/windows-emulator/windows_emulator.cpp:2269-2281` (definitions, right after the existing raw-input delivery functions)

**Acceptance Criteria:**
- [ ] `deliver_mouse_move(int32_t x, int32_t y)` posts a `WM_MOUSEMOVE` to the foreground window with `x`/`y` packed into `lParam` the standard Win32 way (low word = x, high word = y).
- [ ] `deliver_mouse_button(int32_t x, int32_t y, uint32_t message, uint16_t button_data = 0)` posts `message` (one of `WM_LBUTTONDOWN`/`WM_LBUTTONUP`/`WM_RBUTTONDOWN`/`WM_RBUTTONUP`) to the foreground window with the same `lParam` coordinate packing; `wParam` carries `button_data` (mirrors how `deliver_raw_mouse_input`'s `button_data` already carries auxiliary info, e.g. wheel delta — for buttons this is virtual-key-down state, matching real Win32 `WM_LBUTTONDOWN` semantics, but callers in this plan always pass 0 since nothing here needs it yet).
- [ ] Both resolve their target exactly like `deliver_raw_input` does: `this->process.foreground_window` (no explicit-target overload needed — nothing in this plan needs one).
- [ ] `cmake --build --preset=release --target windows-emulator-test` builds clean; existing test suite (`ctest --test-dir build/release`) still passes at the same rate as before this change (no regression — these are new, additive functions with no existing test coverage, matching `deliver_raw_mouse_input`/`deliver_raw_keyboard_input`'s own established precedent of being verified via real-device testing rather than unit tests).

**Verify:** `cmake --build --preset=release --target windows-emulator-test && EMULATOR_ROOT=<path-to-shared-root>/build/release/artifacts/root ctest --test-dir build/release` → `windows-emulator-test` still 100% passing, no new failures.

**Steps:**

- [ ] **Step 1: Read the current code**

Read `src/windows-emulator/windows_emulator.cpp:2232-2281` (the existing `deliver_raw_input`/`deliver_raw_mouse_input`/`deliver_raw_keyboard_input` block) and `src/windows-emulator/windows_emulator.hpp:240-250` in full, to confirm exact current line numbers and the `msg`/`hwnd` types in scope (from `#include <platform/window.hpp>`, already used by this file) before editing — they may have shifted since this plan was written.

- [ ] **Step 2: Add the declarations**

In `src/windows-emulator/windows_emulator.hpp`, immediately after the existing `deliver_raw_keyboard_input` declaration (line 248), add:

```cpp
        // Standard positioned mouse messages (WM_MOUSEMOVE/WM_LBUTTONDOWN/etc, real x/y in
        // lParam) for guests that don't use raw input -- most ordinary Win32 UI apps (dialogs,
        // buttons, text fields) only ever listen for these, not WM_INPUT. Same threading
        // constraint as deliver_raw_mouse_input: touches process state without kernel_lock_, so
        // callers must not invoke this off the emulator thread (see ios_ui_backend's queue for
        // how the iOS frontend handles this).
        void deliver_mouse_move(int32_t x, int32_t y);
        void deliver_mouse_button(int32_t x, int32_t y, uint32_t message, uint16_t button_data = 0);
```

- [ ] **Step 3: Add the definitions**

In `src/windows-emulator/windows_emulator.cpp`, immediately after the existing `deliver_raw_keyboard_input` definition (after line 2281), add:

```cpp
    void windows_emulator::deliver_mouse_move(const int32_t x, const int32_t y)
    {
        const auto target = this->process.foreground_window;
        auto* win = this->process.windows.get(target);
        if (!win)
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, win->thread_id);
        if (!thread)
        {
            return;
        }

        msg m{};
        m.window = target;
        m.message = WM_MOUSEMOVE;
        m.wParam = 0;
        m.lParam = static_cast<lparam>((static_cast<uint32_t>(y) << 16) | (static_cast<uint32_t>(x) & 0xFFFFu));
        thread->post_message(*this, m);
    }

    void windows_emulator::deliver_mouse_button(const int32_t x, const int32_t y, const uint32_t message,
                                                const uint16_t button_data)
    {
        const auto target = this->process.foreground_window;
        auto* win = this->process.windows.get(target);
        if (!win)
        {
            return;
        }

        auto* thread = get_thread_by_id(this->process, win->thread_id);
        if (!thread)
        {
            return;
        }

        msg m{};
        m.window = target;
        m.message = message;
        m.wParam = button_data;
        m.lParam = static_cast<lparam>((static_cast<uint32_t>(y) << 16) | (static_cast<uint32_t>(x) & 0xFFFFu));
        thread->post_message(*this, m);
    }
```

(This duplicates `deliver_raw_input`'s target-resolution logic rather than sharing it, because `deliver_raw_input` also threads a `raw_input_payload`/token through `process.raw_inputs` that these standard messages don't need — matching how the existing `deliver_raw_mouse_input`/`deliver_raw_keyboard_input` are themselves two independent, not-shared-beyond-`deliver_raw_input`, thin wrappers.)

- [ ] **Step 4: Build and run the existing test suite**

```bash
cmake --build --preset=release --target windows-emulator-test
EMULATOR_ROOT=$(cd .. 2>/dev/null; echo "/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root") ctest --test-dir build/release --output-on-failure
```

Expected: build succeeds; `windows-emulator-test`/`linux-emulator-test` pass exactly as they did before this change (no regressions). `analyzer-test`'s pre-existing, unrelated failure (missing `ANALYSIS_SAMPLE` env var) is expected and not a concern.

- [ ] **Step 5: Commit**

```bash
git add src/windows-emulator/windows_emulator.hpp src/windows-emulator/windows_emulator.cpp
git commit -m "feat(input): add standard positioned mouse message delivery"
```

---

### Task 2: `ios_ui_backend` + `SogenBridge` — queue and wire the new mouse events

**Goal:** Extend the existing queue-and-drain infrastructure to carry positioned move/click events (for touchscreen mode) alongside the existing raw button/delta events (for trackpad mode), and wire both through `SogenBridge` to new Objective-C methods the SwiftUI layer can call.

**Files:**
- Modify: `tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp`
- Modify: `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm`
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.h`
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.mm`

**Acceptance Criteria:**
- [ ] `ios_ui_backend` gains a discriminated queued-event representation replacing the current bare `vector<uint16_t> pending_button_flags_`, so it can carry raw button flags, raw movement deltas, AND absolute move/click events in one ordered queue (ordering matters: a click must be delivered after the move that positions the cursor there).
- [ ] New queuing methods: `queue_mouse_delta(int32_t dx, int32_t dy)` (trackpad-mode drag), `queue_mouse_move(int32_t x, int32_t y)` and `queue_mouse_button(int32_t x, int32_t y, uint32_t message)` (touchscreen-mode tap/right-click/drag).
- [ ] New sink types: `mouse_move_sink = std::function<void(int32_t x, int32_t y)>`, `mouse_button_sink = std::function<void(int32_t x, int32_t y, uint32_t message)>`, plus setters, wired in `pump_events()`.
- [ ] `queue_left_click()` (existing) still works unchanged — it becomes a thin wrapper pushing two raw-button events (down, up), same as today's behavior.
- [ ] New `frame_size_sink = std::function<void(int32_t width, int32_t height)>` + setter, invoked once per frame from `present_surface()` with the real `surface.width`/`surface.height` (needed by Task 5's dynamic aspect ratio).
- [ ] `SogenEmulator` (Objective-C) gains: `deliverMouseMove:(CGPoint)point`, `deliverMouseButton:(CGPoint)point message:(uint32_t)message`, `deliverMouseDelta:(CGFloat)dx dy:(CGFloat)dy`, and `onFrameSize` callback property (mirroring the existing `onLogLine` property pattern). `deliverTap` stays as-is (still used as a fallback/simple case, matching the plan's minimal-touch mode).
- [ ] Builds clean for the default (developer-signed) iOS variant.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current files**

Read `IosUiBackend.hpp`, `IosUiBackend.mm`, `SogenBridge.h`, and the relevant section of `SogenBridge.mm` (roughly lines 95-260) in full to confirm exact current content before editing.

- [ ] **Step 2: Rewrite `IosUiBackend.hpp`**

Replace the whole file with:

```cpp
#pragma once

#import <QuartzCore/QuartzCore.h>

#include <platform/ui_backend.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace sogen
{
    // Presents guest frames into a CALayer and feeds queued host input back into the emulator.
    //
    // Threading: the emulator's run thread calls present_surface() and pump_events(); the UI
    // thread calls the queue_*() methods below. Calling deliver_raw_mouse_input/
    // deliver_mouse_move/deliver_mouse_button directly from the UI thread is NOT safe
    // (windows_emulator's delivery functions touch process state without kernel_lock_), so every
    // host input event is queued here and replayed from inside pump_events(), which start()'s
    // run loop invokes on the emulator thread with the kernel lock released.
    class ios_ui_backend final : public ui_backend
    {
      public:
        using raw_mouse_sink = std::function<void(int32_t dx, int32_t dy, uint16_t button_flags, uint16_t button_data)>;
        using mouse_move_sink = std::function<void(int32_t x, int32_t y)>;
        using mouse_button_sink = std::function<void(int32_t x, int32_t y, uint32_t message)>;
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

        // Existing raw-input queuing (trackpad mode / games).
        void queue_left_click();
        void queue_mouse_delta(int32_t dx, int32_t dy);

        // New positioned queuing (touchscreen mode).
        void queue_mouse_move(int32_t x, int32_t y);
        void queue_mouse_button(int32_t x, int32_t y, uint32_t message);

        uint64_t presented_frame_count() const;

      private:
        void emit_log(const char* format, ...) const __attribute__((format(printf, 2, 3)));

        struct queued_input_event
        {
            enum class kind : uint8_t
            {
                raw_button,
                raw_delta,
                absolute_move,
                absolute_button,
            } type;
            int32_t dx{};
            int32_t dy{};
            uint16_t button_flags{};
            uint16_t button_data{};
            int32_t x{};
            int32_t y{};
            uint32_t message{};
        };

        CALayer* layer_{};
        event_sink event_sink_{};
        raw_mouse_sink raw_mouse_sink_{};
        mouse_move_sink mouse_move_sink_{};
        mouse_button_sink mouse_button_sink_{};
        log_sink log_sink_{};
        frame_size_sink frame_size_sink_{};

        mutable std::mutex mutex_{};
        std::vector<queued_input_event> pending_events_{};
        uint64_t presented_frames_{};
        int32_t last_frame_width_{};
        int32_t last_frame_height_{};
    };
}
```

- [ ] **Step 3: Rewrite `IosUiBackend.mm`**

Replace the whole file with:

```cpp
#import "IosUiBackend.hpp"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sogen
{
    namespace
    {
        void release_pixel_copy(void* /*info*/, const void* data, size_t /*size*/)
        {
            std::free(const_cast<void*>(data));
        }
    }

    ios_ui_backend::ios_ui_backend(CALayer* layer)
        : layer_(layer)
    {
    }

    ios_ui_backend::~ios_ui_backend() = default;

    void ios_ui_backend::set_event_sink(event_sink sink)
    {
        this->event_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_raw_mouse_sink(raw_mouse_sink sink)
    {
        this->raw_mouse_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_mouse_move_sink(mouse_move_sink sink)
    {
        this->mouse_move_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_mouse_button_sink(mouse_button_sink sink)
    {
        this->mouse_button_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_log_sink(log_sink sink)
    {
        this->log_sink_ = std::move(sink);
    }

    void ios_ui_backend::set_frame_size_sink(frame_size_sink sink)
    {
        this->frame_size_sink_ = std::move(sink);
    }

    void ios_ui_backend::emit_log(const char* format, ...) const
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (this->log_sink_)
        {
            this->log_sink_(buffer);
        }
        else
        {
            std::fprintf(stderr, "%s\n", buffer);
        }
    }

    uint64_t ios_ui_backend::presented_frame_count() const
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        return this->presented_frames_;
    }

    void ios_ui_backend::queue_left_click()
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_events_.push_back(
            {.type = queued_input_event::kind::raw_button, .button_flags = RI_MOUSE_LEFT_BUTTON_DOWN});
        this->pending_events_.push_back(
            {.type = queued_input_event::kind::raw_button, .button_flags = RI_MOUSE_LEFT_BUTTON_UP});
    }

    void ios_ui_backend::queue_mouse_delta(const int32_t dx, const int32_t dy)
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_events_.push_back({.type = queued_input_event::kind::raw_delta, .dx = dx, .dy = dy});
    }

    void ios_ui_backend::queue_mouse_move(const int32_t x, const int32_t y)
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_events_.push_back({.type = queued_input_event::kind::absolute_move, .x = x, .y = y});
    }

    void ios_ui_backend::queue_mouse_button(const int32_t x, const int32_t y, const uint32_t message)
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_events_.push_back(
            {.type = queued_input_event::kind::absolute_button, .x = x, .y = y, .message = message});
    }

    void ios_ui_backend::pump_events()
    {
        std::vector<queued_input_event> events{};
        {
            const std::lock_guard<std::mutex> lock(this->mutex_);
            events.swap(this->pending_events_);
        }

        for (const auto& event : events)
        {
            switch (event.type)
            {
            case queued_input_event::kind::raw_button:
                this->emit_log("[ios-ui] delivering raw mouse input flags=0x%04X", event.button_flags);
                if (this->raw_mouse_sink_)
                {
                    // dx/dy are 0: this is a button transition, not motion. button_data is the
                    // wheel delta in RAWMOUSE and is 0 for every non-wheel transition.
                    this->raw_mouse_sink_(0, 0, event.button_flags, 0);
                }
                break;
            case queued_input_event::kind::raw_delta:
                if (this->raw_mouse_sink_)
                {
                    this->raw_mouse_sink_(event.dx, event.dy, 0, 0);
                }
                break;
            case queued_input_event::kind::absolute_move:
                this->emit_log("[ios-ui] delivering positioned mouse move x=%d y=%d", event.x, event.y);
                if (this->mouse_move_sink_)
                {
                    this->mouse_move_sink_(event.x, event.y);
                }
                break;
            case queued_input_event::kind::absolute_button:
                this->emit_log("[ios-ui] delivering positioned mouse button message=0x%04X x=%d y=%d", event.message,
                               event.x, event.y);
                if (this->mouse_button_sink_)
                {
                    this->mouse_button_sink_(event.x, event.y, event.message);
                }
                break;
            }
        }
    }

    void ios_ui_backend::present_surface(const hwnd window, const ui_surface_desc& surface)
    {
        if (surface.pixels == nullptr || surface.width <= 0 || surface.height <= 0 ||
            surface.stride < surface.width * 4)
        {
            this->emit_log("[ios-ui] skipping malformed surface %dx%d stride=%d pixels=%p", surface.width,
                           surface.height, surface.stride, surface.pixels);
            return;
        }

        CGBitmapInfo bitmap_info = 0;
        switch (surface.format)
        {
        case ui_surface_format::bgra8:
            // Memory order B,G,R,A == a little-endian 0xAARRGGBB word; the alpha byte is ignored.
            bitmap_info = kCGBitmapByteOrder32Little |
                          static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst);
            break;
        case ui_surface_format::rgba8:
            bitmap_info = kCGBitmapByteOrder32Big |
                          static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipLast);
            break;
        default:
            this->emit_log("[ios-ui] skipping frame with unsupported format %d",
                           static_cast<int>(surface.format));
            return;
        }

        const auto stride = static_cast<size_t>(surface.stride);
        const auto byte_count = stride * static_cast<size_t>(surface.height);

        auto* copy = static_cast<uint8_t*>(std::malloc(byte_count));
        if (copy == nullptr)
        {
            this->emit_log("[ios-ui] out of memory copying %zu-byte frame", byte_count);
            return;
        }
        std::memcpy(copy, surface.pixels, byte_count);

        uint64_t frame_index = 0;
        bool size_changed = false;
        {
            const std::lock_guard<std::mutex> lock(this->mutex_);
            frame_index = this->presented_frames_++;
            size_changed = surface.width != this->last_frame_width_ || surface.height != this->last_frame_height_;
            this->last_frame_width_ = surface.width;
            this->last_frame_height_ = surface.height;
        }

        if (size_changed && this->frame_size_sink_)
        {
            this->frame_size_sink_(surface.width, surface.height);
        }

        this->emit_log("[ios-ui] frame %llu hwnd=0x%llX %dx%d stride=%d fmt=%d first_pixel=%02X%02X%02X%02X",
                       static_cast<unsigned long long>(frame_index),
                       static_cast<unsigned long long>(window), surface.width, surface.height,
                       surface.stride, static_cast<int>(surface.format), copy[0], copy[1], copy[2], copy[3]);

        CGDataProviderRef provider =
            CGDataProviderCreateWithData(nullptr, copy, byte_count, &release_pixel_copy);
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGImageRef image = CGImageCreate(static_cast<size_t>(surface.width), static_cast<size_t>(surface.height),
                                         8, 32, stride, color_space, bitmap_info, provider, nullptr, false,
                                         kCGRenderingIntentDefault);
        CGColorSpaceRelease(color_space);
        CGDataProviderRelease(provider);

        if (image == nullptr)
        {
            this->emit_log("[ios-ui] CGImageCreate failed for frame %llu",
                           static_cast<unsigned long long>(frame_index));
            return;
        }

        CALayer* layer;
        {
            // set_layer() can re-point layer_ from the UI thread while this runs on the emulator
            // thread -- must read it under mutex_.
            const std::lock_guard<std::mutex> lock(this->mutex_);
            layer = this->layer_;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
          [CATransaction begin];
          [CATransaction setDisableActions:YES];
          layer.magnificationFilter = kCAFilterNearest;
          layer.contents = (__bridge id)image;
          [CATransaction commit];
          CGImageRelease(image);
        });
    }
}
```

- [ ] **Step 4: Update `SogenBridge.h`**

Add, right after the existing `deliverTap` declaration:

```objc
/// Delivers a positioned mouse move (touchscreen mode). `point` is in guest-frame pixel
/// coordinates, already transformed from view space by the caller.
- (void)deliverMouseMove:(CGPoint)point;

/// Delivers a positioned mouse button event (touchscreen mode). `message` is one of the
/// WM_LBUTTONDOWN/WM_LBUTTONUP/WM_RBUTTONDOWN/WM_RBUTTONUP constants.
- (void)deliverMouseButton:(CGPoint)point message:(uint32_t)message;

/// Delivers a relative mouse movement delta (trackpad mode) via the existing raw-input path.
- (void)deliverMouseDelta:(CGFloat)dx dy:(CGFloat)dy;

/// Invoked on the main queue whenever the guest's presented frame size changes.
@property (nonatomic, copy, nullable) void (^onFrameSize)(CGSize size);
```

- [ ] **Step 5: Update `SogenBridge.mm`**

In the construction block (where `ui_raw->set_raw_mouse_sink(...)` is currently set, around line 204-207), add the two new sinks right after it:

```objc
            ui_raw->set_raw_mouse_sink(
                [emulator_ptr](const int32_t dx, const int32_t dy, const uint16_t flags, const uint16_t data) {
                    emulator_ptr->deliver_raw_mouse_input(dx, dy, flags, data);
                });
            ui_raw->set_mouse_move_sink([emulator_ptr](const int32_t x, const int32_t y) {
                emulator_ptr->deliver_mouse_move(x, y);
            });
            ui_raw->set_mouse_button_sink([emulator_ptr](const int32_t x, const int32_t y, const uint32_t message) {
                emulator_ptr->deliver_mouse_button(x, y, message);
            });
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

Add the new method implementations right after `deliverTap` (around line 257):

```objc
- (void)deliverMouseMove:(CGPoint)point
{
    if (_ui)
    {
        _ui->queue_mouse_move(static_cast<int32_t>(point.x), static_cast<int32_t>(point.y));
    }
}

- (void)deliverMouseButton:(CGPoint)point message:(uint32_t)message
{
    if (_ui)
    {
        _ui->queue_mouse_button(static_cast<int32_t>(point.x), static_cast<int32_t>(point.y), message);
    }
}

- (void)deliverMouseDelta:(CGFloat)dx dy:(CGFloat)dy
{
    if (_ui)
    {
        _ui->queue_mouse_delta(static_cast<int32_t>(dx), static_cast<int32_t>(dy));
    }
}
```

Add `@synthesize onFrameSize;`-equivalent property backing if needed (Objective-C `@property` with `copy, nullable` already auto-synthesizes a backing ivar the same way `onLogLine` already does — no extra step needed beyond the `@interface` declaration in the header from Step 4, matching how `onLogLine` requires no explicit `@synthesize` either; confirm this by checking whether `onLogLine` has any explicit synthesis in the current `.mm` — if it doesn't, `onFrameSize` doesn't need any either).

- [ ] **Step 6: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 7: Commit**

```bash
git add tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp tools/sogen-ios/Sources/Bridge/IosUiBackend.mm tools/sogen-ios/Sources/Bridge/SogenBridge.h tools/sogen-ios/Sources/Bridge/SogenBridge.mm
git commit -m "feat(ios): queue and wire positioned mouse events + frame-size callback"
```

---

### Task 3: Navigation split — `SetupView` + `EmulationView` shell

**Goal:** Split `ContentView` into a `SetupView` (today's existing content, unchanged) and a new `EmulationView`, connected by real navigation, with a working back button.

**Files:**
- Modify: `tools/sogen-ios/Sources/ContentView.swift` → becomes `SetupView.swift`'s content plus navigation wiring (rename the file too, since `ContentView` the type is being renamed to `SetupView`)
- Modify: `tools/sogen-ios/Sources/SogenApp.swift` (wrap root view in a `NavigationStack`)
- Create: `tools/sogen-ios/Sources/EmulationView.swift` (new, minimal shell for now — chrome/gestures/modes are Tasks 4-5)

**Acceptance Criteria:**
- [ ] `SogenApp.swift`'s `WindowGroup` wraps `SetupView()` in a `NavigationStack`.
- [ ] `SetupView` (renamed from `ContentView`) keeps every existing button/log/boot-flow behavior unchanged, but on successful boot, navigates to `EmulationView` instead of just setting `emulator` and staying on the same screen.
- [ ] `EmulationView` receives the already-constructed `SogenEmulator` and displays the guest's `CALayer` via the existing `EmulatorView` (unchanged internally in this task — gestures come in Task 4).
- [ ] A back button on `EmulationView` calls `emulator.stop()` and navigates back to `SetupView`.
- [ ] Builds clean; on-device, boot still reaches the guest view exactly as before, now on a distinct screen with a working back button.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current `ContentView.swift` in full** to confirm its exact current content (already excerpted earlier in this plan's own design spec — re-read live before editing, since this is the file most likely to have shifted).

- [ ] **Step 2: Rename `ContentView.swift` to `SetupView.swift`, rename the type, and add navigation**

```bash
git mv tools/sogen-ios/Sources/ContentView.swift tools/sogen-ios/Sources/SetupView.swift
```

Change `struct ContentView: View` to `struct SetupView: View`. In `attemptBoot()`, find the two `startEmulator(with: layer)` call sites (in the `JITGateOrchestrator.run`/`runXcodeDebuggerBypass` completion handlers, and the Simulator branch) — each currently calls `startEmulator(with: layer)` directly. Change `startEmulator(with:)` itself so that instead of just setting `emulator = instance` and returning, it also triggers navigation.

Use `.navigationDestination(isPresented:)` with a plain boolean, not `.navigationDestination(item:)` — plain `NSObject` subclasses like `SogenEmulator` don't get automatic `Hashable` conformance in Swift just from being `NSObject`, so `item:`'s requirement isn't guaranteed to be satisfied without extra conformance code this task doesn't otherwise need. A boolean avoids that entirely.

Add the new state alongside the existing ones near the top of the struct:
```swift
    @State private var bootedEmulator: SogenEmulator?
    @State private var didBoot = false
```

At the end of `body`'s `VStack{...}`, add:

```swift
        }
        .navigationDestination(isPresented: $didBoot) {
            if let emulator = bootedEmulator {
                EmulationView(emulator: emulator)
            }
        }
    }
```

In `startEmulator(with:)`, after `emulator = instance` and the existing `appendLog` calls, add:

```swift
        bootedEmulator = instance
        didBoot = true
```

- [ ] **Step 3: Create `EmulationView.swift`**

```swift
import SwiftUI
import UIKit

/// The emulation screen shell. Chrome (back button, icon row, mouse-mode switching, gestures)
/// is added in later tasks of this same plan -- this task only wires navigation and displays
/// the guest view exactly as it already rendered on the setup screen.
struct EmulationView: View {
    let emulator: SogenEmulator
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 0) {
            EmulatorHostViewRepresentable(emulator: emulator)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .navigationBarBackButtonHidden(true)
        .toolbar {
            ToolbarItem(placement: .topBarLeading) {
                Button(action: {
                    emulator.stop()
                    dismiss()
                }) {
                    Image(systemName: "chevron.left")
                }
            }
        }
    }
}

/// Thin wrapper so EmulationView doesn't need to know EmulatorView's own onViewReady/onTap
/// plumbing -- the CALayer is already attached to `emulator` by the time this view appears
/// (SetupView's startEmulator(with:) already called emulator.start() before navigating here).
private struct EmulatorHostViewRepresentable: UIViewRepresentable {
    let emulator: SogenEmulator

    func makeUIView(context: Context) -> UIView {
        let view = UIView()
        view.backgroundColor = .black
        view.layer.magnificationFilter = .nearest
        return view
    }

    func updateUIView(_ uiView: UIView, context: Context) {
    }
}
```

Note for the implementer: this task's `EmulatorHostViewRepresentable` is intentionally minimal (a plain `UIView`, no gesture recognizers, no re-use of the existing `EmulatorHostView`/`EmulatorView` machinery) because Task 4 replaces it entirely with the real gesture-aware view. Wiring the *actual* `CALayer` the emulator already presents into (rather than this placeholder's own fresh, unused layer) is also Task 4's job — this task's `EmulationView` will show a black screen with a working back button, not live guest frames yet. If that gap feels wrong to leave even temporarily, merge Task 3 and Task 4 into one combined task instead — but keeping them separate makes each easier to review; flag this either way in your task report so the coordinator knows which you chose.

- [ ] **Step 4: Update `SogenApp.swift`**

```swift
import SwiftUI

@main
struct SogenApp: App {
    var body: some Scene {
        WindowGroup {
            NavigationStack {
                SetupView()
            }
            .onOpenURL { url in
                if url.scheme == "sogenios" {
                    LocalDevVPNManager.handleCallback()
                }
            }
        }
    }
}
```

- [ ] **Step 5: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 6: Commit**

```bash
git add tools/sogen-ios/Sources/SetupView.swift tools/sogen-ios/Sources/EmulationView.swift tools/sogen-ios/Sources/SogenApp.swift
git rm tools/sogen-ios/Sources/ContentView.swift 2>/dev/null || true
git commit -m "feat(ios): split setup/emulation screens with real back-button navigation"
```

---

### Task 4: Gesture recognizers + mouse-mode dispatch

**Goal:** Give `EmulationView`'s guest-view host real gesture recognizers driving both mouse modes, replacing Task 3's placeholder view with the actual live guest surface.

**Files:**
- Modify: `tools/sogen-ios/Sources/EmulatorView.swift`
- Modify: `tools/sogen-ios/Sources/EmulationView.swift`

**Acceptance Criteria:**
- [ ] `EmulationView` now displays the guest's real, live `CALayer` (obtained the same way `SetupView` used to, via an `onViewReady` callback), not Task 3's placeholder black view.
- [ ] A `MouseMode` enum (`.touchscreen`, `.trackpad`) drives which gesture behavior is active; starts in `.touchscreen`.
- [ ] Touchscreen mode: single tap delivers `deliverMouseMove` immediately followed by `deliverMouseButton` (down, then up) with `WM_LBUTTONDOWN`/`WM_LBUTTONUP`, at the tap's location transformed into guest-frame coordinates (accounting for the view's current letterbox scale/offset vs. the last known frame size from `onFrameSize`, wired in Task 2).
- [ ] Trackpad mode: a pan gesture reports incremental translation deltas via `deliverMouseDelta:dy:`; a separate tap (not the pan) delivers a plain click via the existing `deliverTap` path.
- [ ] Two-finger tap (either mode) delivers a right-click: `deliverMouseButton:message:` with `WM_RBUTTONDOWN`/`WM_RBUTTONUP` in touchscreen mode; in trackpad mode, a raw-input right-click (extend `deliverTap`'s underlying `queue_left_click`-style call with a `deliverRightClick` equivalent — add this to `SogenBridge` if it doesn't already cleanly fall out of the existing raw-input button-flag constants, mirroring `RI_MOUSE_LEFT_BUTTON_DOWN`/`UP` with `RI_MOUSE_RIGHT_BUTTON_DOWN`/`UP`).
- [ ] Press-and-hold-then-drag delivers a click-and-drag: button down at the press location, repeated moves as the finger moves, button up on release (touchscreen mode via `deliverMouseButton`/`deliverMouseMove`; trackpad mode via a held raw button plus `deliverMouseDelta:dy:` calls).
- [ ] Builds clean; on real device, both modes are switchable (even without the actual mode-switch icon yet, which is Task 5 — for this task, a temporary/debug way to flip `MouseMode` is acceptable, e.g. a hardcoded default plus a comment noting Task 5 wires the real toggle) and produce the right message types (verified in Task 7's real-device pass).

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current `EmulatorView.swift`, `EmulationView.swift` (from Task 3), and `SogenBridge.h`/`.mm`** in full to confirm exact current content before editing.

- [ ] **Step 2: Rewrite `EmulatorView.swift`**

```swift
import UIKit
import SwiftUI

enum MouseMode {
    case touchscreen
    case trackpad
}

/// Hosts the guest's CALayer and translates touch gestures into guest mouse input, per the
/// active MouseMode. See docs/superpowers/specs/2026-09-16-ios-input-ux-design.md for the
/// touchscreen-vs-trackpad rationale (Steam Link's Direct-Cursor/Trackpad modes are the direct
/// inspiration).
final class EmulatorHostView: UIView {
    var mode: MouseMode = .touchscreen
    var frameSize: CGSize = .zero
    var onDeliverMove: ((CGPoint) -> Void)?
    var onDeliverButton: ((CGPoint, UInt32) -> Void)?
    var onDeliverDelta: ((CGFloat, CGFloat) -> Void)?
    var onDeliverClick: (() -> Void)?
    var onDeliverRightClick: (() -> Void)?

    private let wmLButtonDown: UInt32 = 0x0201
    private let wmLButtonUp: UInt32 = 0x0202
    private let wmRButtonDown: UInt32 = 0x0204
    private let wmRButtonUp: UInt32 = 0x0205

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .black
        layer.magnificationFilter = .nearest

        let tap = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        addGestureRecognizer(tap)

        let twoFingerTap = UITapGestureRecognizer(target: self, action: #selector(handleTwoFingerTap))
        twoFingerTap.numberOfTouchesRequired = 2
        addGestureRecognizer(twoFingerTap)

        let pan = UIPanGestureRecognizer(target: self, action: #selector(handlePan))
        addGestureRecognizer(pan)

        let longPress = UILongPressGestureRecognizer(target: self, action: #selector(handleLongPress))
        longPress.minimumPressDuration = 0.35
        addGestureRecognizer(longPress)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not used")
    }

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

    @objc private func handleTap(_ recognizer: UITapGestureRecognizer) {
        switch mode {
        case .touchscreen:
            let guestPoint = toGuestPoint(recognizer.location(in: self))
            onDeliverMove?(guestPoint)
            onDeliverButton?(guestPoint, wmLButtonDown)
            onDeliverButton?(guestPoint, wmLButtonUp)
        case .trackpad:
            onDeliverClick?()
        }
    }

    @objc private func handleTwoFingerTap(_ recognizer: UITapGestureRecognizer) {
        switch mode {
        case .touchscreen:
            let guestPoint = toGuestPoint(recognizer.location(in: self))
            onDeliverButton?(guestPoint, wmRButtonDown)
            onDeliverButton?(guestPoint, wmRButtonUp)
        case .trackpad:
            onDeliverRightClick?()
        }
    }

    @objc private func handlePan(_ recognizer: UIPanGestureRecognizer) {
        guard mode == .trackpad else { return }
        let translation = recognizer.translation(in: self)
        onDeliverDelta?(translation.x, translation.y)
        recognizer.setTranslation(.zero, in: self)
    }

    @objc private func handleLongPress(_ recognizer: UILongPressGestureRecognizer) {
        let point = recognizer.location(in: self)
        switch recognizer.state {
        case .began:
            switch mode {
            case .touchscreen:
                let guestPoint = toGuestPoint(point)
                onDeliverMove?(guestPoint)
                onDeliverButton?(guestPoint, wmLButtonDown)
            case .trackpad:
                onDeliverClick?()
            }
        case .changed:
            if mode == .touchscreen {
                onDeliverMove?(toGuestPoint(point))
            }
        case .ended, .cancelled:
            if mode == .touchscreen {
                onDeliverButton?(toGuestPoint(point), wmLButtonUp)
            }
        default:
            break
        }
    }
}

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

- [ ] **Step 3: Add `deliverRightClick` to `SogenBridge`**

In `SogenBridge.h`, add after `deliverTap`:

```objc
/// Queues one right-button click via the existing raw-input path (trackpad mode).
- (void)deliverRightClick;
```

First add `queue_right_click()` to `ios_ui_backend`, mirroring `queue_left_click()` exactly. In `IosUiBackend.hpp`, add to the public section, right after `queue_left_click()`:

```cpp
        void queue_right_click();
```

In `IosUiBackend.mm`, add right after `queue_left_click()`'s definition:

```cpp
    void ios_ui_backend::queue_right_click()
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_events_.push_back(
            {.type = queued_input_event::kind::raw_button, .button_flags = RI_MOUSE_RIGHT_BUTTON_DOWN});
        this->pending_events_.push_back(
            {.type = queued_input_event::kind::raw_button, .button_flags = RI_MOUSE_RIGHT_BUTTON_UP});
    }
```

Then in `SogenBridge.mm`, add after `deliverTap`'s implementation:

```objc
- (void)deliverRightClick
{
    if (_ui)
    {
        _ui->queue_right_click();
    }
}
```

- [ ] **Step 4: Rewrite `EmulationView.swift`** to use the real `EmulatorView` and wire the guest layer:

```swift
import SwiftUI
import UIKit

struct EmulationView: View {
    let emulator: SogenEmulator
    @Environment(\.dismiss) private var dismiss

    @State private var mode: MouseMode = .touchscreen
    @State private var frameSize: CGSize = .zero

    var body: some View {
        VStack(spacing: 0) {
            EmulatorView(
                onViewReady: { layer in
                    emulator.attachLayer(layer)
                },
                mode: mode,
                frameSize: frameSize,
                onDeliverMove: { point in emulator.deliverMouseMove(point) },
                onDeliverButton: { point, message in emulator.deliverMouseButton(point, message: message) },
                onDeliverDelta: { dx, dy in emulator.deliverMouseDelta(dx, dy: dy) },
                onDeliverClick: { emulator.deliverTap() },
                onDeliverRightClick: { emulator.deliverRightClick() }
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .navigationBarBackButtonHidden(true)
        .toolbar {
            ToolbarItem(placement: .topBarLeading) {
                Button(action: {
                    emulator.stop()
                    dismiss()
                }) {
                    Image(systemName: "chevron.left")
                }
            }
        }
        .onAppear {
            emulator.onFrameSize = { size in
                frameSize = size
            }
        }
    }
}
```

This references `emulator.attachLayer(layer)` — a new method needed because `SogenEmulator` is constructed with its `CALayer` up front today (`initWithLayer:emulationRoot:guestExecutablePath:`), but `EmulationView`'s `EmulatorView.onViewReady` only produces a layer once `EmulationView` itself appears, which is AFTER `SetupView` already fully constructed and started the emulator with its OWN (now-orphaned) layer from Task 3's flow. Deferring `SogenEmulator` construction until `EmulationView` appears would touch far more of `SetupView`'s existing, carefully-ordered boot sequence (pairing file, JIT grant, root provisioning all happen before `startEmulator` today) than is justified here — instead, add a `-(void)attachLayer:(CALayer *)layer` to `SogenEmulator` (`SogenBridge.h`/`.mm`) that re-points `_ui`'s internal `layer_` to the new layer:

In `IosUiBackend.hpp`, add to the public section:
```cpp
        void set_layer(CALayer* layer);
```

In `IosUiBackend.mm`, add:
```cpp
    void ios_ui_backend::set_layer(CALayer* layer)
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->layer_ = layer;
    }
```

In `SogenBridge.h`, add after `deliverRightClick`:
```objc
/// Re-points frame presentation at a new layer -- used when EmulationView produces its own
/// CALayer after the emulator was already constructed/started against SetupView's placeholder.
- (void)attachLayer:(CALayer *)layer;
```

In `SogenBridge.mm`, add:
```objc
- (void)attachLayer:(CALayer *)layer
{
    if (_ui)
    {
        _ui->set_layer(layer);
    }
}
```

- [ ] **Step 5: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 6: Commit**

```bash
git add tools/sogen-ios/Sources/EmulatorView.swift tools/sogen-ios/Sources/EmulationView.swift tools/sogen-ios/Sources/Bridge/SogenBridge.h tools/sogen-ios/Sources/Bridge/SogenBridge.mm tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp tools/sogen-ios/Sources/Bridge/IosUiBackend.mm
git commit -m "feat(ios): real gesture recognizers driving touchscreen/trackpad mouse modes"
```

---

### Task 5: Chrome — icon row, portrait/landscape layout, logs overlay, mode switch

**Goal:** Add the actual visible chrome: icon row with a working mouse-mode toggle and logs overlay toggle (keyboard icon present as a wired no-op), portrait/landscape-adaptive layout matching the approved mockup.

**Files:**
- Modify: `tools/sogen-ios/Sources/EmulationView.swift`
- Create: `tools/sogen-ios/Sources/LogsOverlayView.swift`

**Acceptance Criteria:**
- [ ] Icon row (top-right, both orientations): mouse-mode toggle icon (switches `mode` between `.touchscreen`/`.trackpad`, updates its own glyph to reflect current mode), logs toggle icon (shows/hides the logs overlay), keyboard toggle icon (toggles a `@State` boolean with no visible effect yet — sub-project B fills this in).
- [ ] Back button stays top-left in both orientations (already added in Task 3/4).
- [ ] Portrait: guest view in the middle, with an empty placeholder area reserved at the bottom for the future on-screen keyboard (a fixed-height `Color.clear` or similar, sized plausibly for a keyboard, not yet populated).
- [ ] Landscape (detected via `UIDevice.current.orientation` or a `GeometryReader` width/height comparison): guest view fills the entire screen; back button and icon row overlay on top with a semi-transparent background so they stay legible over guest content.
- [ ] Logs overlay: a dismissible panel (tap the logs icon again, or a close button on the panel) showing the same log lines already available (need a way to receive them in `EmulationView` — pass the log-line array/binding forward from `SetupView` at navigation time, same as `emulator` is passed today).
- [ ] Dynamic aspect ratio: the guest-view container's `.aspectRatio(...)` is computed from `frameSize` (already wired in Task 4) instead of a hardcoded constant.

**Verify:** `cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build` → `** BUILD SUCCEEDED **`.

**Steps:**

- [ ] **Step 1: Read the current `EmulationView.swift`** (from Task 4) in full before editing.

- [ ] **Step 2: Create `LogsOverlayView.swift`**

```swift
import SwiftUI

struct LogsOverlayView: View {
    let logLines: [String]
    let onClose: () -> Void

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("Logs").font(.headline)
                Spacer()
                Button(action: onClose) {
                    Image(systemName: "xmark.circle.fill")
                }
            }
            .padding(8)

            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1) {
                        ForEach(Array(logLines.enumerated()), id: \.offset) { index, line in
                            Text(line)
                                .font(.system(size: 10, design: .monospaced))
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .id(index)
                        }
                    }
                    .padding(6)
                }
                .onChange(of: logLines.count) { count in
                    proxy.scrollTo(count - 1, anchor: .bottom)
                }
            }
        }
        .background(Color.black.opacity(0.92))
    }
}
```

(This mirrors `SetupView`'s existing log-rendering `ScrollViewReader`/`LazyVStack` block exactly, extracted into a reusable view — check `SetupView.swift`'s current log-list code to confirm the exact structure matches before finalizing.)

- [ ] **Step 3: Rewrite `EmulationView.swift`**

```swift
import SwiftUI
import UIKit

struct EmulationView: View {
    let emulator: SogenEmulator
    let logLines: [String]
    @Environment(\.dismiss) private var dismiss

    @State private var mode: MouseMode = .touchscreen
    @State private var frameSize: CGSize = .zero
    @State private var showLogs = false
    @State private var showKeyboard = false

    private var guestAspectRatio: CGFloat {
        guard frameSize.width > 0, frameSize.height > 0 else { return 320.0 / 180.0 }
        return frameSize.width / frameSize.height
    }

    var body: some View {
        GeometryReader { geometry in
            let isLandscape = geometry.size.width > geometry.size.height

            ZStack(alignment: .top) {
                if isLandscape {
                    guestView
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else {
                    VStack(spacing: 0) {
                        guestView
                            .aspectRatio(guestAspectRatio, contentMode: .fit)
                            .frame(maxWidth: .infinity)
                        Color.clear
                            .frame(maxWidth: .infinity, minHeight: 160, maxHeight: 220)
                    }
                }

                topBar
                    .padding(8)
                    .background(isLandscape ? Color.black.opacity(0.4) : Color.clear)

                if showLogs {
                    LogsOverlayView(logLines: logLines, onClose: { showLogs = false })
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
        }
        .navigationBarBackButtonHidden(true)
        .background(Color.black)
        .onAppear {
            emulator.onFrameSize = { size in
                frameSize = size
            }
        }
    }

    private var guestView: some View {
        EmulatorView(
            onViewReady: { layer in
                emulator.attachLayer(layer)
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

    private var topBar: some View {
        HStack {
            Button(action: {
                emulator.stop()
                dismiss()
            }) {
                Image(systemName: "chevron.left")
                    .padding(6)
                    .background(Color.black.opacity(0.6))
                    .clipShape(RoundedRectangle(cornerRadius: 6))
            }

            Spacer()

            HStack(spacing: 6) {
                Button(action: {
                    mode = (mode == .touchscreen) ? .trackpad : .touchscreen
                }) {
                    Image(systemName: mode == .touchscreen ? "hand.tap" : "cursorarrow.motionlines")
                        .padding(6)
                        .background(Color.black.opacity(0.6))
                        .clipShape(RoundedRectangle(cornerRadius: 6))
                }
                Button(action: { showLogs.toggle() }) {
                    Image(systemName: "doc.text")
                        .padding(6)
                        .background(Color.black.opacity(0.6))
                        .clipShape(RoundedRectangle(cornerRadius: 6))
                }
                Button(action: { showKeyboard.toggle() }) {
                    // No-op for now -- the on-screen keyboard itself is a separate future plan.
                    Image(systemName: "keyboard")
                        .padding(6)
                        .background(Color.black.opacity(0.6))
                        .clipShape(RoundedRectangle(cornerRadius: 6))
                }
            }
        }
    }
}
```

- [ ] **Step 4: Pass `logLines` forward from `SetupView`**

In `SetupView.swift`, find the `.navigationDestination(isPresented:)` added in Task 3 and update it to also pass `logLines`:

```swift
        .navigationDestination(isPresented: $didBoot) {
            if let emulator = bootedEmulator {
                EmulationView(emulator: emulator, logLines: logLines)
            }
        }
```

- [ ] **Step 5: Build**

```bash
cd tools/sogen-ios && xcodegen generate && xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos -configuration Release -destination 'generic/platform=iOS' CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO build
```

Expected: `** BUILD SUCCEEDED **`.

- [ ] **Step 6: Commit**

```bash
git add tools/sogen-ios/Sources/EmulationView.swift tools/sogen-ios/Sources/LogsOverlayView.swift tools/sogen-ios/Sources/SetupView.swift
git commit -m "feat(ios): emulation-screen chrome, portrait/landscape layout, logs overlay"
```

---

### Task 6: Real-device verification

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in the acceptance criteria has been re-validated independently, with output captured.

**Goal:** Confirm the new chrome and both mouse modes actually work on real hardware — this is real-device-only functionality (gestures, orientation, live guest input) that cannot be meaningfully verified any other way.

**Files:** None (verification only).

**Acceptance Criteria:**
- [ ] Boot reaches the new `EmulationView` (not the old flat layout); back button returns to `SetupView` and stops the emulator.
- [ ] Portrait layout matches the approved mockup (back top-left, icon row top-right, guest view + reserved keyboard space at bottom).
- [ ] Landscape layout matches the approved mockup (guest view fills the screen, chrome overlays on top).
- [ ] Touchscreen mode: tapping a specific point on the guest view produces a click at that exact position — verify against a real Win32 UI app with clickable controls (not just `native-gpu-clear-sample.exe`, which never listens for these messages at all — use any ordinary Windows binary with a visible button/dialog already available in the project's existing test assets, or `calc.exe`/`notepad.exe` if bundled in the shared emulation root).
- [ ] Trackpad mode: dragging produces smooth relative cursor movement in a raw-input-consuming guest (`native-gpu-clear-sample.exe`'s own behavior, or any existing raw-input test guest already used elsewhere in this project).
- [ ] Two-finger tap produces a right-click in both modes.
- [ ] Press-and-hold-then-drag produces a click-and-drag in both modes.
- [ ] Logs icon shows/hides the log overlay with real, current log lines.
- [ ] Mouse-mode icon switches modes instantly, glyph updates to reflect current mode.
- [ ] Guest-view aspect ratio matches the actual guest frame (not stretched/letterboxed incorrectly), confirmed against at least one guest whose resolution differs from `native-gpu-clear-sample.exe`'s if one is available; if not, confirm it still renders `native-gpu-clear-sample.exe` correctly (regression check) and note that a differently-sized guest hasn't been tried.

**Verify:** Manual, on-device, for each bullet above. Capture the on-screen log / `sogen_log.txt` pull as evidence for each.

**Steps:**

- [ ] **Step 1: Build and deliver**

Build and package the developer-signed variant (default `project.yml` state) following this project's established recipe (`xcodebuild archive` with the unsigned-signing flags used throughout this session, `otool -hv` `NOUNDEFS` check, package into `Payload/SogenIOS.app` → `.ipa`), and deliver it to the user for installation exactly as every prior real-device round in this project has.

- [ ] **Step 2: User tests each acceptance-criteria bullet**

User confirms each bullet above and shares the resulting log/description of behavior for any that don't match expectations.

- [ ] **Step 3: Fix and re-verify**

Any bullet that fails gets a real fix (not a workaround) dispatched as its own follow-up round, re-verified the same way, before this task is considered closed.

```json:metadata
{"userGate": true, "tags": ["user-gate"], "requiresUserSpecification": false, "files": [], "verifyCommand": "manual on-device", "acceptanceCriteria": ["boot reaches EmulationView with working back button", "portrait layout matches mockup", "landscape layout matches mockup", "touchscreen mode clicks at exact tap position on a real UI app", "trackpad mode drags smoothly on a raw-input guest", "two-finger tap right-clicks in both modes", "press-and-hold-drag click-drags in both modes", "logs overlay shows real log lines", "mouse-mode icon switches modes with correct glyph", "guest aspect ratio matches actual frame size"], "modelTier": "standard"}
```

---

## Execution Handoff
