# iOS App Shell, Phase 1 (Unicorn-backed): Design

## Context

Two prior efforts this session established the two hardest unknowns independently:

1. **Cross-compilation** (`docs/superpowers/plans/2026-09-04-ios-cross-compilation.md`, branch
   `plan/ios-cross-compilation`): sogen's `analyzer` CLI target builds cleanly for `arm64-apple-ios`.
   Along the way, research for this spec confirmed the real embedding seam is *not* `analyzer`'s
   `main()` — it's `sogen::windows_emulator` (`src/windows-emulator/windows_emulator.hpp`), the same
   object graph `src/python-bindings/sogen_bindings.hpp` already drives via nanobind. `analyzer`
   depends on `windows-emulator`, which already has iOS-aware CMake branches (Freetype vendored from
   source, `vkd3d-shader` disabled for `IOS`), and it built clean in that plan with a single, unrelated
   FreeType fix — meaning `windows-emulator`'s own iOS portability is *already* substantially proven,
   not a new risk this spec needs to retire.

2. **JIT activation** (an out-of-tree Swift spike, written up at
   `https://claude.ai/code/artifact/0e1a4eee-b721-450d-acbd-18bc25f96963`): a self-contained 3-target
   architecture (host app + an ExtensionKit `JITHelper` extension vendoring `StikJIT` + a
   `TunnelExtension` `NEPacketTunnelProvider`) gets real JIT-style code execution working on iOS 26
   TXM/SPTM hardware, via a breakpoint-driven memory-blessing protocol.

The user now wants these combined into a real running app: sogen's emulator, embedded in an iOS app,
rendering a real guest binary with real input plumbing, on a real iPhone. Three approaches were
discussed for sequencing this (full FEX-JIT integration immediately; Unicorn first with FEX swapped in
later; JIT-mechanism-first with a minimal harness) — **Unicorn-first was chosen**, deliberately
deferring the JIT-specific architecture entirely to a later, separate phase.

**Target workload**: `src/samples/native-gpu-clear-sample` — not MW2 (explicitly ruled out: too heavy
for an A18-class chip for this first integration pass). This is a Windows PE **guest** binary (confirmed:
it calls raw `NtGdiDdDDI*` syscalls directly, is only ever built for `WIN32` hosts via
`src/samples/CMakeLists.txt`, and is meant to run *inside* the emulator, the same way it's invoked today
via `analyzer -s native-gpu-clear-sample.exe`) that does a real Vulkan device/context/present cycle with
a solid-color clear, already validated end-to-end on macOS via MoltenVK
(`docs`/project memory: real `vkCreateInstance`/`vkCreateDevice`, `vkCmdClearColorImage`, a
320×180×4-byte readback matching exactly every frame). It needs no meaningful user input.

**A critical scope consequence of choosing Unicorn**: Unicorn is a pure C++ interpreter. It generates
no code and needs no `CS_DEBUGGED`, no breakpoint protocol, no ExtensionKit helper, no tunnel extension,
no pairing file, no special entitlements beyond ordinary app sandboxing. **Phase 1 is an entirely
ordinary iOS app** — ordinary personal-team signing is sufficient, no NetworkExtension entitlement is
needed. All of the JIT-granting architecture from the out-of-tree spike is Phase 2's concern, not this
one's. This is a substantially smaller Phase 1 than "port the spike's 3-target architecture" would
suggest, and is the whole point of choosing this sequencing.

**This spec covers Phase 1 only.** Phase 2 (swap the backend to FEX, implement the JIT26 breakpoint
protocol in `deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp`/`AllocatorHooks.h`, reintroduce the
3-target JIT-granting architecture) is explicitly out of scope here and will get its own design once
Phase 1 is real and verified.

---

## Architecture Overview

Three pieces, cleanly separated by responsibility:

1. **`sogen-ios-embed`** — a new CMake target configuration (not a new C++ target; the existing
   `windows-emulator` and `unicorn-emulator` libraries, built as static libraries for iOS) producing
   the `.a` files and public headers an iOS app needs to embed the emulator directly, mirroring how
   `SOGEN_BUILD_STATIC=ON` + `SOGEN_BUILD_TOOLS=OFF` already produces exactly this configuration for
   other embedders (the root `CMakeLists.txt`'s own stated purpose: "for embedding (e.g., IDA
   plugins)"). Built via a new CMake preset, separately from the app's own Xcode build — the two build
   systems stay cleanly decoupled; the iOS app project references the resulting static libs and headers
   directly rather than one build system driving the other.

2. **A new `ui_backend` implementation for iOS** (new C++/Objective-C++ code) — implements the existing
   `ui_backend` interface (`src/emulator-platform/platform/ui_backend.hpp`)'s `present_surface(hwnd,
   ui_surface_desc)` by taking the raw `width`/`height`/`stride`/`format`/`pixels` buffer it's handed
   (the *exact same* data the existing SDL-based backend already blits into a window on macOS — no new
   GPU/Vulkan-layer work, since rendering has been headless-with-CPU-readback all along) and presenting
   it via a `CALayer`.

3. **The iOS app itself** (Swift/SwiftUI shell, new Xcode project) — an ordinary single-target app (no
   extensions) that: bundles `native-gpu-clear-sample.exe` as a resource, constructs a
   `sogen::windows_emulator` at launch with the Unicorn backend
   (`create_x86_64_emulator(backend_type::unicorn, 1)`) and `application_settings.application` pointing
   at the bundled guest binary, wires the new UIKit `ui_backend` in as the emulator's output surface,
   calls `start()`, and wires a single tap gesture to `deliver_raw_mouse_input(...)` as a proof that the
   input-injection call path genuinely works.

## Rendering: presenting the existing headless pixel buffer

The existing pipeline already ends at `drain_readback()` mapping `readback_memory`, memcpy-ing
`width*height*4` bytes into a `presented_frame`, and calling `ui_backend::present_surface(hwnd,
ui_surface_desc{width, height, stride, format, pixels})` — this is unchanged by this spec.

The new iOS `ui_backend` implementation wraps that raw pixel buffer in a `CGDataProvider`, builds a
`CGImage` from it (matching `ui_surface_desc.format`), and sets it as a `CALayer`'s `contents` — no
Metal pipeline, no vertex/fragment shaders, no render pass. `native-gpu-clear-sample` is a periodic
solid-color clear; a plain bitmap blit is the correct-weight solution, matching how the existing
SDL-based backend already just blits pixels into an SDL window rather than running its own GPU
pipeline. This keeps the new code small and gives a direct, checkable correctness bar: the presented
`CALayer` should show the same solid color the macOS SDL window already shows for this same sample.

## Input: minimal, real, not a control scheme

`deliver_raw_mouse_input(dx, dy, button_flags, button_data)` and `deliver_raw_keyboard_input(vkey,
scan_code, message, extended)` already exist as the host→guest injection API
(`windows_emulator.hpp`) — nothing new needed there. What's missing entirely today is anything that
turns a touch event into a call to either of them (confirmed: zero touch/gesture code anywhere in
`src/`).

Phase 1 adds exactly one `UITapGestureRecognizer` on the rendering view, whose handler calls
`deliver_raw_mouse_input` once with a plausible fixed value. This is deliberately *not* a coordinate-
mapped touch-to-cursor scheme or a virtual keyboard overlay — `native-gpu-clear-sample` doesn't
meaningfully consume input, so building a real control scheme against it would be speculative work
against a target that can't validate it. The goal is proving the Swift→C++ embedding call path for
input genuinely works end to end, which a single deterministic call already does.

## Build Integration

A new CMake preset, `ios-embed` (name to be finalized during planning), inheriting the same toolchain
file as the existing `ios` preset but setting `SOGEN_BUILD_STATIC=ON` and `SOGEN_BUILD_TOOLS=OFF`, with
its build preset targeting `windows-emulator` and `unicorn-emulator` directly (not `analyzer`) — the
same static-library configuration `SOGEN_BUILD_STATIC` already exists for, just invoked for a new
consumer.

The iOS app is a separate Xcode project (via `xcodegen`, matching the pattern already established by
the out-of-tree JIT spike), which references the resulting `.a` files and their public include
directories directly (via `project.yml` library-search-path/header-search-path settings) — built by
running the CMake preset once as a manual pre-step, not by having Xcode invoke CMake as part of its own
build. Keeping the two build systems decoupled avoids a substantial, separate class of integration risk
(getting Xcode to correctly invoke and depend on an external CMake build) that this spec doesn't need to
take on to answer its real question (does the embedding + rendering + input pipeline work at all).

## Testing Strategy: Simulator vs Device

The existing `cmake/toolchain/ios.cmake` is device-only (`CMAKE_OSX_SYSROOT "iphoneos"`). Phase 1 adds a
new, separate file, `cmake/toolchain/ios-simulator.cmake`, mirroring `ios.cmake` but setting
`CMAKE_OSX_SYSROOT "iphonesimulator"` (`CMAKE_OSX_ARCHITECTURES` stays `"arm64"` — an Apple Silicon
Mac's simulator runs arm64 natively, not x86_64) — a separate file rather than a parameterized single
one, matching the existing one-toolchain-file-per-target convention. This is genuine new CMake work, not
just an Xcode scheme setting, since the embeddable static libraries need to actually be built against
the simulator SDK to link into a simulator-run app.

- **Simulator**: the primary iteration loop for this entire spec — rendering (does the `CALayer` show
  the right color), input (does the tap call actually reach `deliver_raw_mouse_input` without crashing),
  and the emulator lifecycle (does `windows_emulator::start()` actually run `native-gpu-clear-sample`
  to completion) are all fully meaningful on a simulator, since none of them depend on real
  code-signing/JIT restrictions — Unicorn is ordinary interpreted C++ code regardless of host.
- **Device**: a final, real-hardware confirmation pass once the simulator path is solid, to rule out any
  simulator-vs-device behavioral difference before calling Phase 1 done. Given Phase 1 needs no special
  entitlements, this is a plain install/launch, not the elaborate ExtensionKit/tunnel-extension dance
  from tonight's spike (that dance is Phase 2's concern).

## Error Handling

Scoped narrowly to what Phase 1 actually needs, not speculative generality:

- If `native-gpu-clear-sample.exe` fails to load or the emulator fails to start, surface this as visible
  on-screen text (not a silent failure) — there's no other UI in this minimal shell to report through.
- If `present_surface` is called with a `ui_surface_desc` the `CGImage` construction can't handle
  (unexpected format/stride), log and skip that frame rather than crashing — a single missed frame is
  harmless for a clear-color loop.
- No handling is added for guest crashes beyond what `windows_emulator` already does internally — this
  spec doesn't change guest-side behavior at all, only how the host embeds and presents it.

---

## Explicitly Out of Scope (Phase 2, not designed here)

- Swapping the CPU backend to FEX.
- The JIT26 breakpoint-protocol allocator work in `deps/FEX`.
- Reintroducing the 3-target JIT-granting architecture (ExtensionKit `JITHelper`, `TunnelExtension`,
  pairing-file import) from the out-of-tree spike.
- Any entitlement/signing work beyond ordinary personal-team app signing.
- A real touch-to-cursor control scheme, virtual keyboard, or any input design beyond the one
  proof-of-path tap.
- Targeting any guest binary other than `native-gpu-clear-sample`.
