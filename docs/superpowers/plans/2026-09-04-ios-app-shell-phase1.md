# iOS App Shell, Phase 1 (Unicorn-backed) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an ordinary (non-JIT, personal-team-signed) iOS app that embeds `sogen::windows_emulator` with the Unicorn backend, runs the Windows PE guest `native-gpu-clear-sample.exe` inside it, presents each presented frame through a new `CALayer`-based `ui_backend`, and proves the host→guest input path with a single tap gesture.

**Architecture:** CMake builds `windows-emulator` + `backend-selection` + `unicorn-emulator` as static `.a` libraries for the iOS device and iOS Simulator SDKs via two new presets (`ios-embed-device`, `ios-embed-simulator`) and one new toolchain file (`cmake/toolchain/ios-simulator.cmake`). A separate, decoupled xcodegen-generated Xcode project at `tools/sogen-ios/` links those archives directly and supplies its own `sogen::ios_ui_backend` (Objective-C++, lives in the app's own sources) injected through `emulator_interfaces::ui` — the app never goes through `create_default_ui_backend()`. The ~1.8 GB emulation root is *provisioned into the app's Documents container* with `simctl`/`devicectl` rather than bundled, keeping the `.app` small; only the 32-bit guest `.exe` is a bundled resource, reached via `emulator_settings::path_mappings`.

**Tech Stack:** C++20, Objective-C++ (ARC), Swift/SwiftUI, CMake presets + Ninja, xcodegen 2.46, Unicorn CPU backend, MoltenVK (static, iOS device + simulator slices), `i686-w64-mingw32-g++` (mingw-w64 14.0.0 from Homebrew) for the guest PE.

**User decisions (already made):**
- "**Unicorn-first was chosen**, deliberately deferring the JIT-specific architecture entirely to a later, separate phase." — no FEX, no JIT26, no ExtensionKit/TunnelExtension, no entitlements beyond ordinary personal-team signing.
- "**Target workload**: `src/samples/native-gpu-clear-sample` — not MW2 (explicitly ruled out: too heavy for an A18-class chip for this first integration pass)." No other guest binary is in scope.
- "Phase 1 adds exactly one `UITapGestureRecognizer` on the rendering view, whose handler calls `deliver_raw_mouse_input` once with a plausible fixed value. This is deliberately *not* a coordinate-mapped touch-to-cursor scheme or a virtual keyboard overlay."
- "**Simulator**: the primary iteration loop for this entire spec … **Device**: a final, real-hardware confirmation pass once the simulator path is solid."
- "Phase 1 adds a new, separate file, `cmake/toolchain/ios-simulator.cmake`, mirroring `ios.cmake` but setting `CMAKE_OSX_SYSROOT "iphonesimulator"` … a separate file rather than a parameterized single one."
- "The iOS app is a separate Xcode project (via `xcodegen`) … built by running the CMake preset once as a manual pre-step, not by having Xcode invoke CMake as part of its own build."

---

## Facts established by research (do not re-derive)

These were verified against the real repo before this plan was written. They are the load-bearing assumptions of every task below.

1. **`emulation_root` is mandatory on non-Windows hosts.** `src/windows-emulator/windows_emulator.cpp` (constructor):
   ```cpp
   #ifndef OS_WINDOWS
           if (this->emulation_root.empty())
           {
               throw std::runtime_error("Emulation root directory can not be empty!");
           }
   #endif
   ```
   and `file_sys(emulation_root.empty() ? emulation_root : emulation_root / "filesys")`, `registry(... registry_manager{emulation_root / "registry"})`. `file_system`'s constructor calls `canonical(root)`, which throws if `<root>/filesys` does not exist. **The app must ship/provision a real emulation root.** This is the single biggest addition relative to the design spec, which did not mention it.

2. **The full emulation root on this machine is 44 GB** (`/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root`), almost all of it sample games/apps under `filesys/c/`. The parts the emulator actually needs are `filesys/c/windows` (1.6 GB: `system32` 257 MB / 177 files, `syswow64` 1.35 GB / 2954 flat files, `globalization` 3 MB), `registry/` (208 MB), and `api-set.bin` (24 KB) — **≈ 1.8 GB total**. Hive loading is lazy (`hive_parser` seeks through an `std::ifstream`; it does not slurp the 191 MB `SOFTWARE` hive into RAM), so the 1.8 GB is disk-only, not memory pressure.

3. **`native-gpu-clear-sample` is a 32-bit (WoW64) guest.** Its own source says so verbatim: *"This sample only runs as a 32-bit (WoW64) guest."* Its `d3dkmt::*` structs are the packed all-32-bit wire layout, and `gdi.cpp`'s `handle_NtGdiDdDDISubmitCommand`/`handle_NtGdiDdDDIPresent` only apply their offset fixups under `if (c.proc.is_wow64_process)`. It must be built with `i686-w64-mingw32-g++`, not `x86_64-w64-mingw32-g++`. `src/CMakeLists.txt:53` only adds `samples/` under `if(WIN32)`, so CMake never builds it on macOS.

4. **The sample renders 8 known colors, 400 ms apart, then exits.** From `native-gpu-clear-sample.cpp`: `kWidth=320`, `kHeight=180`, `format = 0 /* VK_FORMAT_B8G8R8A8_UNORM */`, and `frame_colors` = blue, green, red, yellow, cyan, magenta, orange(1.0,0.5,0.0), violet(0.5,0.0,1.0). Stdout is exactly `[ngcs] hAdapter=…`, `[ngcs] hDevice=…`, `[ngcs] hContext=…`, `[ngcs] hRenderTarget=…`, eight `[ngcs] frame N rgba=(…)` lines, then `[ngcs] done`.

5. **Nothing reaches `present_surface` without a working host Vulkan.** `gdi.cpp:handle_NtGdiDdDDICreateDevice` constructs a `vulkan_host`, and if `available()` is false it does `c.proc.dxgk.vk_host.reset()`. Every later handler starts with `if (!c.proc.dxgk.vk_host …) return STATUS_SUCCESS;`. `handle_NtGdiDdDDIPresent` is the only caller of `ui().present_surface(...)` on this path. **So MoltenVK is required for anything to appear on screen** (see Task 5). Without it the guest still runs to `[ngcs] done` — which is why Task 4 is verifiable before Task 5 exists.

6. **`vulkan_host` finds its loader by `dlopen` of a fixed name list** (`src/windows-emulator/devices/vulkan_host.cpp:114`, `impl()` ctor at :1142). The `__APPLE__` list is macOS-oriented (`libvulkan.1.dylib`, `/opt/homebrew/lib/libMoltenVK.dylib`, …). iOS needs a new branch.

7. **`deliver_raw_mouse_input`'s `button_flags` are Win32 `RI_MOUSE_*` (RAWMOUSE `usButtonFlags`) values**, defined for non-Windows builds at `src/emulator-platform/platform/window.hpp:326` (`RI_MOUSE_LEFT_BUTTON_DOWN 0x0001`, `RI_MOUSE_LEFT_BUTTON_UP 0x0002`, … `RI_MOUSE_WHEEL 0x0400`). `windows_emulator.cpp:232 raw_mouse_button_flags()` maps `WM_LBUTTONDOWN → RI_MOUSE_LEFT_BUTTON_DOWN`. `button_data` is the wheel delta and is `0` for button events.

8. **`deliver_raw_input` does NOT take `kernel_lock_`.** It mutates `process.raw_inputs` / `process.next_raw_input_token` directly. The only safe host thread to call it from is the emulator's own run thread. `windows_emulator::start()`'s single-vCPU loop does `lock.unlock(); this->ui_backend_->pump_events(); this->callbacks.on_event_pump(); lock.lock();` (`windows_emulator.cpp:2189-2194`) — so `pump_events()` runs on the emulator thread with the kernel lock released. **Taps must be queued on the main thread and drained inside `pump_events()`.**

9. **`emulator_interfaces` members left null get internal defaults** (`windows_emulator.cpp:739-796`): `clock` → `instruction_tick_clock` if `use_relative_time` else `utils::clock`; `dns_lookup` → `network::dns_lookup`; `socket_factory` → `network::socket_factory`; `ui` → `create_default_ui_backend()`; `audio` → `create_default_audio_backend()` (SDL on iOS). This plan injects `ui` (the new iOS backend) and `audio` (`sogen::null_audio_backend`, declared at `platform/audio_backend.hpp:47`) and leaves the other three null.

10. **The exact header search path set that compiles `windows_emulator.hpp` + `backend_selection.hpp` against both iOS SDKs, including alongside `<UIKit/UIKit.h>` in ARC Objective-C++, was verified with `clang -fsyntax-only`:** `src/windows-emulator`, `src/emulator`, `src/common`, `src/emulator-platform`, `src`, `src/backend-selection`. Nothing from `deps/` is needed for the public headers.

11. **`unicorn-emulator` is the real CMake target name** (`src/backends/unicorn-emulator/CMakeLists.txt:10`, `STATIC` when `SOGEN_BUILD_STATIC`). `create_x86_64_emulator` lives in the separate `backend-selection` library (`src/backend-selection/`), which links `unicorn-emulator` PRIVATE. `backend_type` is `{unicorn, icicle, whp, kvm, fex}`.

12. **SDL3 stays in the build and is harmless.** `src/windows-emulator/CMakeLists.txt:13-19` hard-errors without `SDL3::SDL3` on non-Emscripten platforms, and the existing `ios` preset already builds `libSDL3.a` (14.7 MB) from the vendored `deps/SDL`. Because `windows_emulator`'s `get_ui_backend()` fallback references `create_default_ui_backend()`, the linker *will* pull `sdl_ui_backend.o` and therefore `libSDL3.a` into the app even though the app injects its own backend. That is fine — SDL is simply never *initialized*. **Do not strip SDL out**; just link it and its iOS frameworks.

13. **LTO is on by default** (`CMakeLists.txt:9 SOGEN_ENABLE_LTO=ON` → `CMAKE_INTERPROCEDURAL_OPTIMIZATION`), which is why `otool -l build/ios/artifacts/libemulator.a` today reports *"is an LLVM bit-code file"*. The new embed presets turn LTO **off** so the archives are plain Mach-O (faster Xcode links, inspectable with `otool`, no bitcode/dead-strip interaction with `-force_load`/`-export_dynamic`).

14. **Tooling present on this machine:** `xcodegen` 2.46.0, `gh`, `xcrun`, `/opt/homebrew/bin/i686-w64-mingw32-g++` and `x86_64-w64-mingw32-g++` (mingw-w64 14.0.0_1). A built macOS `analyzer` exists at `build/release/artifacts/analyzer` in this worktree. The out-of-tree JIT spike's working xcodegen template is at `/Users/jack/Documents/Coding/C++/sogen/.worktrees/ios-jit-test/tools/ios-jit-test/project.yml` (bundle prefix `com.jacksonyarger`).

---

## File Structure

**Created**
| Path | Responsibility |
|---|---|
| `cmake/toolchain/ios-simulator.cmake` | iOS Simulator (arm64) cross-compile toolchain. Mirrors `ios.cmake`, sysroot `iphonesimulator`. |
| `tools/stage-ios-emulation-root.sh` | Stages the ~1.8 GB minimal emulation root + builds/installs the 32-bit guest PE into it. |
| `tools/sogen-ios/project.yml` | xcodegen project definition: one app target, link flags, search paths, frameworks. |
| `tools/sogen-ios/.gitignore` | Keeps `SogenIOS.xcodeproj/`, `build/`, `Resources/*.exe`, `Vendor/` out of git. |
| `tools/sogen-ios/README.md` | Build/provision/run recipe for both simulator and device. |
| `tools/sogen-ios/Sources/SogenApp.swift` | SwiftUI `@main` entry point. |
| `tools/sogen-ios/Sources/ContentView.swift` | The single screen: emulator view + scrolling log text. |
| `tools/sogen-ios/Sources/EmulatorView.swift` | `UIViewRepresentable` host view; owns the `CALayer` and the tap recognizer. |
| `tools/sogen-ios/Sources/Bridging-Header.h` | Swift ↔ Obj-C bridge (imports `SogenBridge.h` only — no C++). |
| `tools/sogen-ios/Sources/Bridge/SogenBridge.h` | Pure Objective-C façade (`SogenEmulator`) — the only thing Swift sees. |
| `tools/sogen-ios/Sources/Bridge/SogenBridge.mm` | Owns the `windows_emulator`, the run thread, settings wiring, log/stdout sinks. |
| `tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp` | `sogen::ios_ui_backend` declaration. |
| `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm` | `present_surface` → `CGImage` → `CALayer.contents`; tap queue drained in `pump_events`. |

**Modified**
| Path | Change |
|---|---|
| `CMakePresets.json` | Add `ios-embed-device` / `ios-embed-simulator` configure, build and workflow presets. |
| `src/windows-emulator/devices/vulkan_host.cpp` | Add an iOS branch to `vulkan_loader_names` (statically linked MoltenVK ⇒ `dlopen(nullptr)`). |
| `docs/fex-backend.md` | Extend the existing `## iOS cross-compilation` section with the embed presets + app pointer. |

**Not created, deliberately:** no `create_ios_ui_backend()` factory, no file under `src/windows-emulator/ui_backends/`. The backend is injected through `emulator_interfaces::ui`, so nothing inside `windows-emulator` needs to name it; keeping it in the app's own sources means UIKit/QuartzCore never enter the `windows-emulator` CMake target, and Xcode (not CMake) compiles it once per SDK, which is exactly the build-system decoupling the spec asked for.

---

### Task 1: iOS-Simulator toolchain + `ios-embed` preset pair

**Goal:** `cmake --workflow --preset=ios-embed-device` and `cmake --workflow --preset=ios-embed-simulator` each produce a full set of plain (non-bitcode) static `.a` archives for `windows-emulator`, `backend-selection` and `unicorn-emulator`, built against the `iphoneos` and `iphonesimulator` SDKs respectively.

**Files:**
- Create: `cmake/toolchain/ios-simulator.cmake`
- Modify: `CMakePresets.json` (configurePresets after the existing `"ios"` entry at lines 58-70; buildPresets after the existing `"ios"` entry at lines 147-153; workflowPresets after the existing `"ios"` entry at lines 221-233)

**Acceptance Criteria:**
- [ ] `cmake --list-presets=all` shows `ios-embed-device` and `ios-embed-simulator` under configure, build and workflow presets (6 matches total).
- [ ] `build/ios-embed-device/artifacts/` and `build/ios-embed-simulator/artifacts/` both contain `libwindows-emulator.a`, `libbackend-selection.a`, `libunicorn-emulator.a` as **static archives** (not `.dylib`).
- [ ] `otool -l build/ios-embed-device/artifacts/libemulator.a` reports `platform 2` (iOS); the simulator one reports `platform 7` (iOS Simulator). Neither says "is an LLVM bit-code file".
- [ ] `lipo -info` on each reports `architecture: arm64`.
- [ ] No `analyzer` executable is produced in either artifacts directory (tools are off).
- [ ] `cmake --preset=release` still configures successfully afterward.

**Verify:**
```bash
cmake --list-presets=all 2>&1 | grep -c 'ios-embed'
```
→ `6`

**Steps:**

- [ ] **Step 1: Write `cmake/toolchain/ios-simulator.cmake`**

```cmake
set(CMAKE_SYSTEM_NAME "iOS")
# CMake leaves CMAKE_SYSTEM_PROCESSOR empty for an iOS CMAKE_SYSTEM_NAME; several
# CMakeLists here and in deps/ branch on it and would take the wrong arch path.
set(CMAKE_SYSTEM_PROCESSOR "arm64")
# An Apple Silicon Mac runs the iOS Simulator natively as arm64; an x86_64 slice
# would only ever be needed on an Intel host, which this project does not target.
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
set(CMAKE_OSX_SYSROOT "iphonesimulator")
# CMake defaults executables to .app bundles on iOS; the embeddable static-library
# configuration produces no executables, and app bundling belongs to the Xcode project.
set(CMAKE_MACOSX_BUNDLE OFF)
```

- [ ] **Step 2: Add the two configure presets to `CMakePresets.json`**

Insert immediately after the existing `"ios"` configure preset object (which currently ends at line 70, before the `"emscripten-base"` entry):

```json
        {
            "name": "ios-embed-device",
            "inherits": [
                "ios"
            ],
            "cacheVariables": {
                "SOGEN_BUILD_STATIC": "ON",
                "SOGEN_BUILD_TOOLS": "OFF",
                "SOGEN_ENABLE_LTO": "OFF"
            }
        },
        {
            "name": "ios-embed-simulator",
            "inherits": [
                "ios-embed-device"
            ],
            "cacheVariables": {
                "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/toolchain/ios-simulator.cmake"
            }
        },
```

`binaryDir` comes from the hidden `"build"` preset as `${sourceDir}/build/${presetName}`, and `${presetName}` expands to the *inheriting* preset's name, so these land in `build/ios-embed-device` and `build/ios-embed-simulator`. `SOGEN_ENABLE_LTO=OFF` is deliberate — see "Facts established by research" #13.

- [ ] **Step 3: Add the two build presets**

Insert after the existing `"ios"` build preset object (currently lines 147-153):

```json
        {
            "name": "ios-embed-device",
            "configurePreset": "ios-embed-device",
            "targets": [
                "windows-emulator",
                "backend-selection",
                "unicorn-emulator"
            ]
        },
        {
            "name": "ios-embed-simulator",
            "configurePreset": "ios-embed-simulator",
            "targets": [
                "windows-emulator",
                "backend-selection",
                "unicorn-emulator"
            ]
        },
```

Restricting `targets` mirrors the existing `emscripten32`/`emscripten64` build presets. `backend-selection` is listed because `create_x86_64_emulator()` lives there, not in `unicorn-emulator`.

- [ ] **Step 4: Add the two workflow presets**

Insert after the existing `"ios"` workflow preset object (currently lines 221-233):

```json
        {
            "name": "ios-embed-device",
            "steps": [
                {
                    "type": "configure",
                    "name": "ios-embed-device"
                },
                {
                    "type": "build",
                    "name": "ios-embed-device"
                }
            ]
        },
        {
            "name": "ios-embed-simulator",
            "steps": [
                {
                    "type": "configure",
                    "name": "ios-embed-simulator"
                },
                {
                    "type": "build",
                    "name": "ios-embed-simulator"
                }
            ]
        },
```

- [ ] **Step 5: Build both**

```bash
cmake --workflow --preset=ios-embed-device
cmake --workflow --preset=ios-embed-simulator
```
Expected: both end with `ninja: build stopped` never appearing; the last lines are Ninja's `[N/N] Linking …` / completion with exit code 0.

If a configure or compile error appears, fix it the same way the completed cross-compilation plan did: real errors get real fixes; a subdirectory is only gated behind `CMAKE_SYSTEM_NAME STREQUAL "iOS"` when the dependency is fundamentally host-only, with a one-line comment saying why. Do **not** flip `SOGEN_BUILD_STATIC`/`SOGEN_BUILD_TOOLS` back.

- [ ] **Step 6: Inspect the artifacts**

```bash
ls build/ios-embed-device/artifacts/
ls build/ios-embed-simulator/artifacts/
```
Expected in each (this is the transitive closure of the three targets; record the real list, Task 4's link flags are derived from it):
```
libSDL3.a  libbackend-selection.a  libemulator-common.a  libemulator.a
libfreetype.a  libminidump.a  libunicorn-common.a  libunicorn-emulator.a
libunicorn.a  libwindows-emulator.a  libx86_64-softmmu.a  libzstd.a
```
Then:
```bash
otool -l build/ios-embed-device/artifacts/libemulator.a    | grep -m1 -A4 LC_BUILD_VERSION
otool -l build/ios-embed-simulator/artifacts/libemulator.a | grep -m1 -A4 LC_BUILD_VERSION
lipo -info build/ios-embed-device/artifacts/libwindows-emulator.a
```
Expected: `platform 2` for device, `platform 7` for simulator, `architecture: arm64` for both.

- [ ] **Step 7: Confirm the desktop build is unaffected**

```bash
cmake --preset=release 2>&1 | tail -3
```
Expected: `-- Build files have been written to: …/build/release`

- [ ] **Step 8: Commit**

```bash
git add cmake/toolchain/ios-simulator.cmake CMakePresets.json
git commit -m "build(ios): add ios-embed device/simulator presets for static embedding"
```

---

### Task 2: Build the 32-bit guest PE and stage a runnable emulation root

**Goal:** A reproducible script produces `build/ios-root/` (≈1.8 GB: `filesys/c/windows`, `registry`, `api-set.bin`) plus `build/ios-root/native-gpu-clear-sample.exe` (a 32-bit Windows PE), and the macOS `analyzer` runs that exe against that root all the way to `[ngcs] done`.

**Files:**
- Create: `tools/stage-ios-emulation-root.sh`
- Uses (read-only): `/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root` (the full 44 GB root; the source of the staged subset)

**Acceptance Criteria:**
- [ ] `file build/ios-root/native-gpu-clear-sample.exe` reports `PE32 executable (console) Intel 80386, for MS Windows` (i.e. 32-bit, **not** `PE32+ … x86-64`).
- [ ] `build/ios-root/` contains exactly `api-set.bin`, `filesys/c/windows/`, `registry/`, and `native-gpu-clear-sample.exe`; `du -sh build/ios-root` is under 2.0 GB.
- [ ] `analyzer -e build/ios-root c:/native-gpu-clear-sample.exe` prints `[ngcs] done` and exits 0.
- [ ] The script is idempotent: running it twice produces the same tree and does not re-copy unchanged files (`rsync`).

**Verify:**
```bash
cd /Users/jack/Documents/Coding/C++/sogen/.claude/worktrees/plan-ios-cross-compilation && \
  ./build/release/artifacts/analyzer -b -e "$PWD/build/ios-root" c:/native-gpu-clear-sample.exe 2>&1 | tr -d '\r' | grep -c '^\[ngcs\] done$'
```
→ `1`

(`-b`/`--buffer` is required: the guest's ucrtbase stdio HLE writes one byte per `NtWriteFile`
syscall, and `console_reporter.cpp`'s `stdout_chunk_event` handler appends a synthetic `\n` after
every unbuffered chunk, fragmenting `[ngcs] done` across many single-character lines without it.
Guest output is also real Windows-CRT-translated `\r\n`, hence `tr -d '\r'` before the exact-line
grep. Both are pre-existing `analyzer`/HLE behavior, unrelated to this task's script.)

**Steps:**

- [ ] **Step 1: Write `tools/stage-ios-emulation-root.sh`**

```bash
#!/usr/bin/env bash
# Stages the minimal emulation root the iOS app needs, plus the 32-bit guest PE it runs.
#
# The full captured root is ~44 GB, almost all of it sample games/apps under filesys/c that
# native-gpu-clear-sample never touches. windows_emulator only needs the Windows system tree,
# the registry hives and api-set.bin; everything else under filesys/c is dropped.
#
# Usage: tools/stage-ios-emulation-root.sh [/path/to/full/root] [/path/to/output/root]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_ROOT="${1:-/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root}"
OUT_ROOT="${2:-$REPO_ROOT/build/ios-root}"

if [[ ! -d "$SRC_ROOT/filesys/c/windows" ]]; then
  echo "error: '$SRC_ROOT' does not look like a sogen emulation root (no filesys/c/windows)" >&2
  exit 1
fi

echo "==> staging emulation root: $SRC_ROOT -> $OUT_ROOT"
mkdir -p "$OUT_ROOT/filesys/c"
rsync -a --delete "$SRC_ROOT/filesys/c/windows/" "$OUT_ROOT/filesys/c/windows/"
rsync -a --delete "$SRC_ROOT/registry/"          "$OUT_ROOT/registry/"
cp -f "$SRC_ROOT/api-set.bin" "$OUT_ROOT/api-set.bin"

echo "==> building the 32-bit guest PE"
# The sample is a WoW64-only guest: its d3dkmt structs are the packed all-32-bit wire layout
# that sogen's gdi.cpp handlers apply their `is_wow64_process` fixups to. A 64-bit build would
# hand the host handlers the wrong struct shape. src/CMakeLists.txt only adds samples/ under
# WIN32, so this is built directly with mingw-w64 instead of through a CMake preset.
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

Two copies of the exe are produced on purpose: the one inside `filesys/c/` makes the plain
`analyzer -e … c:/native-gpu-clear-sample.exe` smoke test work with no extra flags, and the one
at the root of `build/ios-root/` is the file Task 4 copies into the app bundle's `Resources/`.

- [ ] **Step 2: Make it executable and run it**

```bash
chmod +x tools/stage-ios-emulation-root.sh
./tools/stage-ios-emulation-root.sh
```
Expected tail:
```
==> done
1.8G	.../build/ios-root
.../build/ios-root/native-gpu-clear-sample.exe: PE32 executable (console) Intel 80386, for MS Windows, 5 sections
```
The `rsync` of ~3100 files takes several minutes on first run and is near-instant afterwards.

If `i686-w64-mingw32-g++` is missing: `brew install mingw-w64` (it ships both the i686 and x86_64 cross toolchains; this machine already has `mingw-w64 14.0.0_1`).

- [ ] **Step 3: Smoke-test the staged root with the macOS analyzer**

```bash
./build/release/artifacts/analyzer -e "$PWD/build/ios-root" c:/native-gpu-clear-sample.exe 2>&1 | tail -20
```
Expected (order fixed, addresses will differ):
```
[ngcs] hAdapter=0x...
[ngcs] hDevice=0x...
[ngcs] hContext=0x...
[ngcs] hRenderTarget=0x...
[ngcs] frame 0 rgba=(0.00,0.00,1.00,1.00)
...
[ngcs] frame 7 rgba=(0.50,0.00,1.00,1.00)
[ngcs] done
```
If a `STATUS_OBJECT_NAME_NOT_FOUND` / "Failed to map module" error names a DLL, copy that DLL from
`$SRC_ROOT` into the matching directory under `$OUT_ROOT` and add the copy to the script — the
staged set is intentionally the whole `windows` tree, so this should not happen, but a missing file
outside `windows/` (for example under `filesys/c/programdata/`) is the one realistic gap.

If `NtGdiDdDDICreateDevice: host Vulkan not available` appears, that is expected on a machine
without MoltenVK on the dyld path and does **not** fail this task — the criterion is `[ngcs] done`.

- [ ] **Step 4: Commit**

```bash
git add tools/stage-ios-emulation-root.sh
git commit -m "tools(ios): stage a minimal emulation root and the 32-bit guest sample"
```
(`build/` is already gitignored, so the 1.8 GB tree is not committed.)

---

### Task 3: The `ios_ui_backend` (CALayer presentation + tap queue)

**Goal:** A self-contained Objective-C++ `sogen::ios_ui_backend` that implements `ui_backend`, turns each `present_surface` into a `CGImage` assigned to a `CALayer`, and hands queued taps to a caller-supplied raw-mouse sink from inside `pump_events()` — compiling cleanly against both iOS SDKs before any Xcode project exists.

**Files:**
- Create: `tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp`
- Create: `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm`

**Acceptance Criteria:**
- [ ] The file compiles with `-fsyntax-only` for `arm64-apple-ios14.0` and `arm64-apple-ios14.0-simulator` with ARC on, zero warnings under `-Wall -Wextra`.
- [ ] Only `set_event_sink`, `pump_events` and `present_surface` are overridden; every other `ui_backend` member inherits its default no-op body.
- [ ] `present_surface` rejects (logs and returns, does not crash or assign) a surface with `pixels == nullptr`, `width <= 0`, `height <= 0`, `stride < width * 4`, or a format outside `{bgra8, rgba8}`.
- [ ] `queue_left_click()` enqueues a DOWN then an UP with `RI_MOUSE_LEFT_BUTTON_DOWN` / `RI_MOUSE_LEFT_BUTTON_UP`; `pump_events()` drains them under a mutex and calls the sink outside the lock.

**Verify:**
```bash
cd tools/sogen-ios && \
xcrun --sdk iphonesimulator clang++ -x objective-c++ -fsyntax-only -fobjc-arc -std=c++20 \
  -Wall -Wextra \
  -I ../../src/windows-emulator -I ../../src/emulator -I ../../src/common \
  -I ../../src/emulator-platform -I ../../src -I ../../src/backend-selection \
  -I Sources/Bridge \
  -target arm64-apple-ios14.0-simulator Sources/Bridge/IosUiBackend.mm && echo OK
```
→ `OK`

**Steps:**

- [ ] **Step 1: Write `tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp`**

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
    // Presents guest frames into a CALayer and feeds queued host taps back into the emulator.
    //
    // Threading: the emulator's run thread calls present_surface() and pump_events(); the UI
    // thread calls queue_left_click(). deliver_raw_mouse_input() is NOT safe to call from the UI
    // thread (windows_emulator::deliver_raw_input touches process state without kernel_lock_),
    // so taps are queued here and replayed from inside pump_events(), which start()'s run loop
    // invokes on the emulator thread with the kernel lock released.
    class ios_ui_backend final : public ui_backend
    {
      public:
        using raw_mouse_sink = std::function<void(int32_t dx, int32_t dy, uint16_t button_flags, uint16_t button_data)>;
        using log_sink = std::function<void(const char* line)>;

        explicit ios_ui_backend(CALayer* layer);
        ~ios_ui_backend() override;

        void set_event_sink(event_sink sink) override;
        void pump_events() override;
        void present_surface(hwnd window, const ui_surface_desc& surface) override;

        void set_raw_mouse_sink(raw_mouse_sink sink);
        void set_log_sink(log_sink sink);
        void queue_left_click();

        uint64_t presented_frame_count() const;

      private:
        void emit_log(const char* format, ...) const __attribute__((format(printf, 2, 3)));

        CALayer* layer_{};
        event_sink event_sink_{};
        raw_mouse_sink raw_mouse_sink_{};
        log_sink log_sink_{};

        mutable std::mutex mutex_{};
        std::vector<uint16_t> pending_button_flags_{};
        uint64_t presented_frames_{};
    };
}
```

- [ ] **Step 2: Write `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm`**

```objective-c++
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

    void ios_ui_backend::set_log_sink(log_sink sink)
    {
        this->log_sink_ = std::move(sink);
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
        this->pending_button_flags_.push_back(RI_MOUSE_LEFT_BUTTON_DOWN);
        this->pending_button_flags_.push_back(RI_MOUSE_LEFT_BUTTON_UP);
    }

    void ios_ui_backend::pump_events()
    {
        std::vector<uint16_t> flags{};
        {
            const std::lock_guard<std::mutex> lock(this->mutex_);
            flags.swap(this->pending_button_flags_);
        }

        for (const auto button_flags : flags)
        {
            this->emit_log("[ios-ui] delivering raw mouse input flags=0x%04X", button_flags);
            if (this->raw_mouse_sink_)
            {
                // dx/dy are 0: this is a button transition, not motion. button_data is the wheel
                // delta in RAWMOUSE and is 0 for every non-wheel transition.
                this->raw_mouse_sink_(0, 0, button_flags, 0);
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
            bitmap_info = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
            break;
        case ui_surface_format::rgba8:
            bitmap_info = kCGBitmapByteOrder32Big | kCGImageAlphaNoneSkipLast;
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
        {
            const std::lock_guard<std::mutex> lock(this->mutex_);
            frame_index = this->presented_frames_++;
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

        CALayer* layer = this->layer_;
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

`RI_MOUSE_LEFT_BUTTON_DOWN` / `_UP` come from `platform/window.hpp`, which `platform/ui_backend.hpp`
already includes — no extra include is needed.

- [ ] **Step 3: Compile-check for both SDKs**

```bash
cd tools/sogen-ios
for T in "iphonesimulator arm64-apple-ios14.0-simulator" "iphoneos arm64-apple-ios14.0"; do
  set -- $T
  xcrun --sdk "$1" clang++ -x objective-c++ -fsyntax-only -fobjc-arc -std=c++20 -Wall -Wextra \
    -I ../../src/windows-emulator -I ../../src/emulator -I ../../src/common \
    -I ../../src/emulator-platform -I ../../src -I ../../src/backend-selection -I Sources/Bridge \
    -target "$2" Sources/Bridge/IosUiBackend.mm && echo "OK $1"
done
```
Expected:
```
OK iphonesimulator
OK iphoneos
```

- [ ] **Step 4: Commit**

```bash
git add tools/sogen-ios/Sources/Bridge/IosUiBackend.hpp tools/sogen-ios/Sources/Bridge/IosUiBackend.mm
git commit -m "feat(ios): add a CALayer-backed ui_backend for the iOS app shell"
```

---

### Task 4: xcodegen project + SwiftUI shell that boots the emulator to `[ngcs] done`

**Goal:** An installable simulator app that links the Task 1 archives, bundles the Task 2 guest PE, reads the provisioned emulation root out of its Documents container, constructs `windows_emulator` with the Unicorn backend and the Task 3 `ios_ui_backend`, runs `start()` on a background thread, and shows the guest's `[ngcs]` stdout on screen ending in `[ngcs] done`.

**Files:**
- Create: `tools/sogen-ios/project.yml`
- Create: `tools/sogen-ios/.gitignore`
- Create: `tools/sogen-ios/README.md`
- Create: `tools/sogen-ios/Sources/SogenApp.swift`
- Create: `tools/sogen-ios/Sources/ContentView.swift`
- Create: `tools/sogen-ios/Sources/EmulatorView.swift`
- Create: `tools/sogen-ios/Sources/Bridging-Header.h`
- Create: `tools/sogen-ios/Sources/Bridge/SogenBridge.h`
- Create: `tools/sogen-ios/Sources/Bridge/SogenBridge.mm`
- Copies in: `tools/sogen-ios/Resources/native-gpu-clear-sample.exe` (gitignored, produced by Task 2)

**Acceptance Criteria:**
- [ ] `xcodegen generate` in `tools/sogen-ios` produces `SogenIOS.xcodeproj` with no warnings.
- [ ] `xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphonesimulator -configuration Debug build` exits 0 with `** BUILD SUCCEEDED **`.
- [ ] The app installs and launches on a booted arm64 simulator without crashing.
- [ ] The on-screen log ends with `[ngcs] done`, preceded by the eight `[ngcs] frame N rgba=(…)` lines in order.
- [ ] With the root deliberately absent, the app shows a visible on-screen error naming the missing path instead of crashing (spec's Error Handling requirement).

**Verify:**
```bash
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | grep -c '\[ngcs\] done'
```
→ `1`

**Steps:**

- [ ] **Step 1: Write `tools/sogen-ios/.gitignore`**

```gitignore
SogenIOS.xcodeproj/
Generated-Info.plist
build/
DerivedData/
Resources/*.exe
Vendor/
```

- [ ] **Step 2: Write `tools/sogen-ios/project.yml`**

```yaml
name: SogenIOS
options:
  bundleIdPrefix: com.jacksonyarger
  deploymentTarget:
    iOS: "14.0"
  createIntermediateGroups: true

targets:
  SogenIOS:
    type: application
    platform: iOS
    sources:
      - path: Sources
      - path: Resources
        buildPhase: resources
    dependencies:
      - sdk: UIKit.framework
      - sdk: Foundation.framework
      - sdk: QuartzCore.framework
      - sdk: CoreGraphics.framework
      - sdk: CoreVideo.framework
      - sdk: CoreMedia.framework
      - sdk: CoreMotion.framework
      - sdk: CoreHaptics.framework
      - sdk: CoreBluetooth.framework
      - sdk: AVFoundation.framework
      - sdk: AudioToolbox.framework
      - sdk: GameController.framework
      - sdk: Metal.framework
      - sdk: IOSurface.framework
      - sdk: OpenGLES.framework
      - sdk: Security.framework
      - sdk: UserNotifications.framework
    settings:
      base:
        PRODUCT_BUNDLE_IDENTIFIER: com.jacksonyarger.sogenios
        PRODUCT_NAME: SogenIOS
        MARKETING_VERSION: "1.0"
        CURRENT_PROJECT_VERSION: "1"
        SWIFT_VERSION: "5.0"
        CODE_SIGN_STYLE: Automatic
        CLANG_CXX_LANGUAGE_STANDARD: "c++20"
        CLANG_CXX_LIBRARY: "libc++"
        CLANG_ENABLE_OBJC_ARC: YES
        SWIFT_OBJC_BRIDGING_HEADER: Sources/Bridging-Header.h
        # The static archives are LTO-free plain Mach-O (see the ios-embed presets), but the
        # app still dlsym()s vkGetInstanceProcAddr out of its own image, so nothing MoltenVK
        # brings in may be dead-stripped. Task 5 adds -force_load; this keeps it reachable.
        DEAD_CODE_STRIPPING: NO
        # Repo root, two levels up from tools/sogen-ios.
        SOGEN_ROOT: "$(SRCROOT)/../.."
        HEADER_SEARCH_PATHS:
          - "$(SRCROOT)/Sources"
          - "$(SRCROOT)/Sources/Bridge"
          - "$(SOGEN_ROOT)/src"
          - "$(SOGEN_ROOT)/src/windows-emulator"
          - "$(SOGEN_ROOT)/src/emulator"
          - "$(SOGEN_ROOT)/src/emulator-platform"
          - "$(SOGEN_ROOT)/src/common"
          - "$(SOGEN_ROOT)/src/backend-selection"
        # The simulator archives are arm64-only (see cmake/toolchain/ios-simulator.cmake).
        EXCLUDED_ARCHS[sdk=iphonesimulator*]: "x86_64"
        LIBRARY_SEARCH_PATHS[sdk=iphoneos*]: "$(SOGEN_ROOT)/build/ios-embed-device/artifacts"
        LIBRARY_SEARCH_PATHS[sdk=iphonesimulator*]: "$(SOGEN_ROOT)/build/ios-embed-simulator/artifacts"
        OTHER_LDFLAGS: >-
          -lwindows-emulator -lbackend-selection -lunicorn-emulator -lunicorn
          -lunicorn-common -lx86_64-softmmu -lemulator -lemulator-common
          -lminidump -lzstd -lfreetype -lSDL3 -liconv
          -Wl,-export_dynamic
    info:
      path: Generated-Info.plist
      properties:
        UILaunchScreen: {}
        UIFileSharingEnabled: true
        LSSupportsOpeningDocumentsInPlace: true
        UISupportedInterfaceOrientations:
          - UIInterfaceOrientationPortrait
```

`UIFileSharingEnabled` + `LSSupportsOpeningDocumentsInPlace` mirror the JIT spike's project and make
the Documents container (where the emulation root lands) reachable from Finder as a fallback to
`devicectl`. If Step 8's link fails with an undefined ObjC class or symbol from a framework not in
the list above, add that framework as another `- sdk: X.framework` entry — SDL3's iOS framework set
is what drives this list and CMake never emits it for a static archive.

- [ ] **Step 3: Write `tools/sogen-ios/Sources/Bridge/SogenBridge.h`**

This header is imported into Swift, so it must stay pure Objective-C — no C++ types leak through it.

```objective-c
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

NS_ASSUME_NONNULL_BEGIN

/// Objective-C facade over sogen::windows_emulator. Every method is main-thread safe;
/// the emulator itself runs on a private background thread owned by this object.
@interface SogenEmulator : NSObject

/// `layer` receives every frame the guest presents. `emulationRoot` is the directory holding
/// filesys/ and registry/. `guestExecutablePath` is the host path of the bundled .exe, which is
/// mapped into the guest as c:\native-gpu-clear-sample.exe.
- (instancetype)initWithLayer:(CALayer *)layer
                emulationRoot:(NSString *)emulationRoot
          guestExecutablePath:(NSString *)guestExecutablePath NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Invoked on the main queue for every emulator log line and every byte of guest stdout.
@property (nonatomic, copy, nullable) void (^onLogLine)(NSString *line);

/// Starts the guest on a background thread and returns immediately.
- (void)start;

/// Requests that the run loop stop. Safe to call more than once.
- (void)stop;

/// Queues one left-button click. Delivered on the emulator thread at the next event pump.
- (void)deliverTap;

@end

NS_ASSUME_NONNULL_END
```

- [ ] **Step 4: Write `tools/sogen-ios/Sources/Bridge/SogenBridge.mm`**

```objective-c++
#import "SogenBridge.h"
#import "IosUiBackend.hpp"

#include <windows_emulator.hpp>
#include <backend_selection.hpp>

#include <exception>
#include <memory>
#include <string>
#include <thread>

@implementation SogenEmulator {
    std::unique_ptr<sogen::windows_emulator> _emulator;
    sogen::ios_ui_backend* _ui;  // owned by _emulator via emulator_interfaces::ui
    std::thread _runThread;
    NSString* _emulationRoot;
    NSString* _guestExecutablePath;
    CALayer* _layer;
}

- (instancetype)initWithLayer:(CALayer *)layer
                emulationRoot:(NSString *)emulationRoot
          guestExecutablePath:(NSString *)guestExecutablePath
{
    self = [super init];
    if (self)
    {
        _layer = layer;
        _emulationRoot = [emulationRoot copy];
        _guestExecutablePath = [guestExecutablePath copy];
        _ui = nullptr;
    }
    return self;
}

- (void)dealloc
{
    [self stop];
}

- (void)appendLog:(NSString *)line
{
    void (^sink)(NSString *) = self.onLogLine;
    if (!sink)
    {
        NSLog(@"%@", line);
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
      sink(line);
    });
}

- (BOOL)validatePaths
{
    NSFileManager* fm = [NSFileManager defaultManager];
    BOOL isDirectory = NO;

    NSString* filesys = [_emulationRoot stringByAppendingPathComponent:@"filesys"];
    NSString* registry = [_emulationRoot stringByAppendingPathComponent:@"registry"];

    if (![fm fileExistsAtPath:filesys isDirectory:&isDirectory] || !isDirectory)
    {
        [self appendLog:[NSString stringWithFormat:@"ERROR: emulation root incomplete, missing %@", filesys]];
        return NO;
    }
    if (![fm fileExistsAtPath:registry isDirectory:&isDirectory] || !isDirectory)
    {
        [self appendLog:[NSString stringWithFormat:@"ERROR: emulation root incomplete, missing %@", registry]];
        return NO;
    }
    if (![fm fileExistsAtPath:_guestExecutablePath])
    {
        [self appendLog:[NSString stringWithFormat:@"ERROR: guest executable missing at %@", _guestExecutablePath]];
        return NO;
    }
    return YES;
}

- (void)start
{
    if (_runThread.joinable())
    {
        return;
    }

    if (![self validatePaths])
    {
        return;
    }

    NSString* root = _emulationRoot;
    NSString* guest = _guestExecutablePath;
    CALayer* layer = _layer;
    __weak SogenEmulator* weakSelf = self;

    _runThread = std::thread([weakSelf, root, guest, layer]() {
        SogenEmulator* strongSelf = weakSelf;
        if (!strongSelf)
        {
            return;
        }

        try
        {
            auto ui = std::make_unique<sogen::ios_ui_backend>(layer);
            auto* ui_raw = ui.get();
            ui_raw->set_log_sink([weakSelf](const char* line) {
                [weakSelf appendLog:[NSString stringWithUTF8String:line]];
            });

            sogen::emulator_interfaces interfaces{};
            interfaces.ui = std::move(ui);
            // Nothing in this app produces audio; the default would be the SDL audio backend,
            // which would spin up an AVAudioSession for no reason.
            interfaces.audio = std::make_unique<sogen::null_audio_backend>();

            sogen::emulator_settings settings{};
            settings.emulation_root = std::filesystem::path(root.UTF8String);
            // The guest .exe ships in the app bundle (read-only), not inside the provisioned
            // emulation root, so map its guest path straight at the bundle resource.
            settings.path_mappings[sogen::windows_path("c:/native-gpu-clear-sample.exe")] =
                std::filesystem::path(guest.UTF8String);

            sogen::application_settings app_settings{};
            app_settings.application = sogen::windows_path("c:/native-gpu-clear-sample.exe");

            sogen::emulator_callbacks callbacks{};
            callbacks.on_stdout = [weakSelf](const std::string_view data) {
                NSString* text = [[NSString alloc] initWithBytes:data.data()
                                                          length:data.size()
                                                        encoding:NSUTF8StringEncoding];
                if (text)
                {
                    [weakSelf appendLog:text];
                }
            };

            auto emu = sogen::create_x86_64_emulator(sogen::backend_type::unicorn, 1);
            auto win_emu = std::make_unique<sogen::windows_emulator>(
                std::move(emu), std::move(app_settings), settings, std::move(callbacks), std::move(interfaces));

            win_emu->log.set_sink([weakSelf](sogen::color, const std::string_view message) {
                NSString* text = [[NSString alloc] initWithBytes:message.data()
                                                          length:message.size()
                                                        encoding:NSUTF8StringEncoding];
                if (text)
                {
                    [weakSelf appendLog:text];
                }
            });

            auto* emulator_ptr = win_emu.get();
            ui_raw->set_raw_mouse_sink(
                [emulator_ptr](const int32_t dx, const int32_t dy, const uint16_t flags, const uint16_t data) {
                    emulator_ptr->deliver_raw_mouse_input(dx, dy, flags, data);
                });

            strongSelf->_emulator = std::move(win_emu);
            strongSelf->_ui = ui_raw;

            [weakSelf appendLog:@"[sogen] starting guest"];
            strongSelf->_emulator->start();
            [weakSelf appendLog:@"[sogen] guest run finished"];
        }
        catch (const std::exception& e)
        {
            [weakSelf appendLog:[NSString stringWithFormat:@"ERROR: %s", e.what()]];
        }
        catch (...)
        {
            [weakSelf appendLog:@"ERROR: unknown exception on the emulator thread"];
        }
    });
}

- (void)stop
{
    // windows_emulator::should_stop is a std::atomic_bool, so requesting the stop from another
    // thread is safe; the run loop notices it at the top of its next iteration. Everything else
    // (destroying the emulator, clearing the backend pointer) happens only after the join, on
    // whichever thread called stop.
    if (_emulator)
    {
        _emulator->stop();
    }

    if (_runThread.joinable())
    {
        _runThread.join();
    }

    _ui = nullptr;
    _emulator.reset();
}

- (void)deliverTap
{
    // Runs on the main thread. queue_left_click() only touches a mutex-guarded vector; the actual
    // deliver_raw_mouse_input call happens on the emulator thread inside pump_events().
    if (_ui)
    {
        _ui->queue_left_click();
    }
}

@end
```

Note the ordering in `stop`: `_ui` is cleared **before** `_emulator.reset()`, because `_ui` is a
non-owning pointer to the backend that `emulator_interfaces::ui` handed to the emulator — destroying
the emulator destroys the backend, and a stale `_ui` would be a dangling read from `deliverTap`.
`dealloc` (written above) already calls `[self stop]`, so the thread is always joined before the
object goes away.

- [ ] **Step 5: Write `tools/sogen-ios/Sources/Bridging-Header.h`**

```objective-c
#import "Bridge/SogenBridge.h"
```

This is the only Objective-C surface Swift sees. It must never grow a C++ include — `HEADER_SEARCH_PATHS`
puts `$(SRCROOT)/Sources` first so the `Bridge/` prefix resolves.

- [ ] **Step 6: Write the three Swift files**

`tools/sogen-ios/Sources/SogenApp.swift`:

```swift
import SwiftUI

@main
struct SogenApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
```

`tools/sogen-ios/Sources/EmulatorView.swift`:

```swift
import UIKit
import SwiftUI

/// A plain UIView whose backing layer receives guest frames, plus the single tap recognizer
/// that proves the host -> guest input path.
final class EmulatorHostView: UIView {
    var onTap: (() -> Void)?

    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .black
        layer.magnificationFilter = .nearest
        let recognizer = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        addGestureRecognizer(recognizer)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) is not used")
    }

    @objc private func handleTap() {
        onTap?()
    }
}

struct EmulatorView: UIViewRepresentable {
    let onViewReady: (CALayer) -> Void
    let onTap: () -> Void

    func makeUIView(context: Context) -> EmulatorHostView {
        let view = EmulatorHostView(frame: .zero)
        view.onTap = onTap
        onViewReady(view.layer)
        return view
    }

    func updateUIView(_ uiView: EmulatorHostView, context: Context) {
        uiView.onTap = onTap
    }
}
```

`tools/sogen-ios/Sources/ContentView.swift`:

```swift
import SwiftUI

struct ContentView: View {
    @State private var logLines: [String] = []
    @State private var emulator: SogenEmulator?

    var body: some View {
        VStack(spacing: 0) {
            EmulatorView(
                onViewReady: { layer in startEmulator(with: layer) },
                onTap: { emulator?.deliverTap() }
            )
            .frame(maxWidth: .infinity)
            .aspectRatio(320.0 / 180.0, contentMode: .fit)

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
    }

    private func startEmulator(with layer: CALayer) {
        guard emulator == nil else { return }

        let documents = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let root = documents.appendingPathComponent("root").path

        guard let guestPath = Bundle.main.path(forResource: "native-gpu-clear-sample", ofType: "exe") else {
            logLines.append("ERROR: native-gpu-clear-sample.exe is not in the app bundle")
            return
        }

        let instance = SogenEmulator(layer: layer, emulationRoot: root, guestExecutablePath: guestPath)
        instance.onLogLine = { line in
            logLines.append(line.trimmingCharacters(in: .newlines))
        }
        emulator = instance
        logLines.append("[sogen] emulation root: \(root)")
        logLines.append("[sogen] guest executable: \(guestPath)")
        instance.start()
    }
}
```

`makeUIView` is called once per view lifetime, and `startEmulator` is `guard`ed on `emulator == nil`,
so a SwiftUI re-render cannot start a second emulator on the same layer.

- [ ] **Step 7: Stage the guest resource and generate the project**

```bash
mkdir -p tools/sogen-ios/Resources
cp build/ios-root/native-gpu-clear-sample.exe tools/sogen-ios/Resources/
cd tools/sogen-ios && xcodegen generate
```

Expected:
```
⚙️  Generating plists...
⚙️  Generating project...
⚙️  Writing project...
Created project at .../tools/sogen-ios/SogenIOS.xcodeproj
```

- [ ] **Step 8: Build for the simulator**

```bash
cd tools/sogen-ios
xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS \
  -sdk iphonesimulator -configuration Debug \
  -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath build build 2>&1 | tail -20
```

Expected: `** BUILD SUCCEEDED **`

Undefined-symbol failures on the first attempt are expected and are the intended discovery loop:

- A symbol like `_OBJC_CLASS_$_AVAudioSession`, `_OBJC_CLASS_$_GCController` or `_CMMotionManager`
  names a **missing framework** → add another `- sdk: X.framework` entry under `dependencies` in
  `project.yml`. These come from `libSDL3.a`, whose iOS framework set CMake never emits for a static
  archive; the list already in `project.yml` is the expected complete set, derived from
  `deps/SDL/CMakeLists.txt`'s `LINK_LIBRARY:FRAMEWORK` generator expressions minus the macOS-only
  ones (Cocoa, Carbon, IOKit, ForceFeedback).
- A `sogen::…`, `uc_…`, `FT_…`, `ZSTD_…` or `udmp_…` symbol names a **missing archive** → add its
  `-l` name to `OTHER_LDFLAGS`, taken from the real `ls build/ios-embed-simulator/artifacts/` output
  recorded in Task 1 Step 6.

Re-run `xcodegen generate` after every `project.yml` edit — the `.xcodeproj` is generated, never
hand-edited.

- [ ] **Step 9: Boot a simulator, install, and confirm the missing-root error path**

```bash
xcrun simctl boot "iPhone 16" 2>/dev/null || true
xcrun simctl bootstatus booted -b
xcrun simctl install booted build/Build/Products/Debug-iphonesimulator/SogenIOS.app
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | head -10
```

Expected on this **first** launch (the Documents container exists but has no `root/` yet):
```
[sogen] emulation root: /Users/.../data/Containers/Data/Application/<UUID>/Documents/root
[sogen] guest executable: /Users/.../SogenIOS.app/native-gpu-clear-sample.exe
ERROR: emulation root incomplete, missing /Users/.../Documents/root/filesys
```

Confirm on the simulator screen that this error text is **visible in the log pane** and the app is
still running (not crashed, no exception dialog). This is the spec's Error Handling requirement
("surface this as visible on-screen text (not a silent failure)") and is an acceptance criterion of
this task — do not skip past it to Step 10.

```bash
xcrun simctl terminate booted com.jacksonyarger.sogenios
```

- [ ] **Step 10: Provision the emulation root into the app's Documents container**

```bash
DATA=$(xcrun simctl get_app_container booted com.jacksonyarger.sogenios data)
echo "container: $DATA"
mkdir -p "$DATA/Documents"
rsync -a --delete ../../build/ios-root/ "$DATA/Documents/root/"
du -sh "$DATA/Documents/root"
ls "$DATA/Documents/root"
```

Expected:
```
1.8G	/Users/.../Documents/root
api-set.bin	filesys		native-gpu-clear-sample.exe	registry
```

This is a local disk-to-disk copy and takes well under a minute. The `native-gpu-clear-sample.exe`
sitting at the root of the copy is harmless here — the app reaches the guest through
`path_mappings` at the *bundle* copy, not this one.

- [ ] **Step 11: Run and read the log**

```bash
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | tail -40
```

Expected (interleaved with emulator log lines):
```
[sogen] emulation root: /Users/.../Documents/root
[sogen] starting guest
...
NtGdiDdDDICreateDevice: host Vulkan not available
[ngcs] hAdapter=0x...
[ngcs] hDevice=0x...
[ngcs] hContext=0x...
[ngcs] hRenderTarget=0x...
[ngcs] frame 0 rgba=(0.00,0.00,1.00,1.00)
[ngcs] frame 1 rgba=(0.00,1.00,0.00,1.00)
[ngcs] frame 2 rgba=(1.00,0.00,0.00,1.00)
[ngcs] frame 3 rgba=(1.00,1.00,0.00,1.00)
[ngcs] frame 4 rgba=(0.00,1.00,1.00,1.00)
[ngcs] frame 5 rgba=(1.00,0.00,1.00,1.00)
[ngcs] frame 6 rgba=(1.00,0.50,0.00,1.00)
[ngcs] frame 7 rgba=(0.50,0.00,1.00,1.00)
[ngcs] done
[sogen] guest run finished
```

The `host Vulkan not available` warning and the black emulator view are **correct** for this task —
`present_surface` is unreachable without a host Vulkan device (see "Facts established by research"
#5). Task 5 resolves both. What this task proves is that the emulator constructs, loads the guest,
runs it to completion under Unicorn on iOS, and never crashes.

If instead the log stops at a `Failed to map module` / `STATUS_OBJECT_NAME_NOT_FOUND` naming a DLL,
the staged root from Task 2 is incomplete for this host — add the named file to
`tools/stage-ios-emulation-root.sh`, re-run it, re-`rsync`, and repeat.

- [ ] **Step 12: Write `tools/sogen-ios/README.md`**

````markdown
# sogen iOS app shell (Phase 1)

An ordinary iOS app that embeds `sogen::windows_emulator` with the Unicorn backend and runs the
`native-gpu-clear-sample` Windows guest inside it, presenting frames through a `CALayer`.

No JIT, no entitlements beyond ordinary personal-team signing, no app extensions. See
`docs/superpowers/specs/2026-09-04-ios-app-shell-phase1-design.md` for the design and
`docs/superpowers/plans/2026-09-04-ios-app-shell-phase1.md` for how it was built.

## One-time host setup

```sh
brew install xcodegen mingw-w64
# Static sogen libraries for both SDKs:
cmake --workflow --preset=ios-embed-device
cmake --workflow --preset=ios-embed-simulator
# Minimal emulation root (~1.8 GB) + the 32-bit guest PE:
tools/stage-ios-emulation-root.sh
```

## Build

```sh
cd tools/sogen-ios
mkdir -p Resources && cp ../../build/ios-root/native-gpu-clear-sample.exe Resources/
xcodegen generate
xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphonesimulator \
  -configuration Debug -destination 'generic/platform=iOS Simulator' -derivedDataPath build build
```

The `.xcodeproj` is generated by xcodegen and is gitignored — edit `project.yml`, never the project
file. CMake and Xcode stay decoupled: Xcode never invokes CMake, it just links the archives the
`ios-embed-*` presets already produced.

## Provision the emulation root

The root is ~1.8 GB and is deliberately **not** bundled in the `.app`; it is copied into the app's
Documents container instead. The app looks for `<Documents>/root` and reports a visible on-screen
error if `filesys/` or `registry/` is missing there.

Simulator:
```sh
xcrun simctl install booted build/Build/Products/Debug-iphonesimulator/SogenIOS.app
xcrun simctl launch booted com.jacksonyarger.sogenios && xcrun simctl terminate booted com.jacksonyarger.sogenios
DATA=$(xcrun simctl get_app_container booted com.jacksonyarger.sogenios data)
rsync -a --delete ../../build/ios-root/ "$DATA/Documents/root/"
```

Device:
```sh
xcrun devicectl device install app --device <udid> \
  build/Build/Products/Debug-iphoneos/SogenIOS.app
xcrun devicectl device copy to --device <udid> \
  --domain-type appDataContainer --domain-identifier com.jacksonyarger.sogenios \
  --source ../../build/ios-root --destination Documents/root
```

`UIFileSharingEnabled` is set, so dragging `build/ios-root` in as `root` via Finder's device file
sharing is an equivalent fallback if `devicectl copy to` misbehaves.

## Run

`xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios`

Expected: the view cycles blue → green → red → yellow → cyan → magenta → orange → violet at 400 ms
per frame and holds violet; the log ends with `[ngcs] done`. Tapping the view logs
`[ios-ui] delivering raw mouse input flags=0x0001` / `0x0002`.
````

- [ ] **Step 13: Commit**

```bash
git add tools/sogen-ios/project.yml tools/sogen-ios/.gitignore tools/sogen-ios/README.md tools/sogen-ios/Sources
git commit -m "feat(ios): SwiftUI app shell embedding windows_emulator on Unicorn"
```

`Resources/native-gpu-clear-sample.exe` and `SogenIOS.xcodeproj/` are excluded by
`tools/sogen-ios/.gitignore`, so this commit is source-only.

---

### Task 5: Statically link MoltenVK so the guest's frames actually reach the CALayer

**Goal:** `vulkan_host` finds a Vulkan implementation on iOS (MoltenVK linked into the app binary), so `NtGdiDdDDIPresent` reaches `ui().present_surface()`, and the simulator screen shows the sample's eight clear colors, ending held on violet.

**Files:**
- Modify: `src/windows-emulator/devices/vulkan_host.cpp` (the `#else`/`#include <dlfcn.h>` block at lines 41-44, and the `vulkan_loader_names` platform block at lines 109-118)
- Modify: `tools/sogen-ios/project.yml` (add per-SDK `OTHER_LDFLAGS`)
- Adds (gitignored by `tools/sogen-ios/.gitignore`'s `Vendor/` rule): `tools/sogen-ios/Vendor/MoltenVK/ios-device/libMoltenVK.a`, `tools/sogen-ios/Vendor/MoltenVK/ios-simulator/libMoltenVK.a`

**Acceptance Criteria:**
- [ ] The app log contains `NtGdiDdDDICreateDevice: host Vulkan device id=0x…` and **not** `host Vulkan not available`.
- [ ] The log contains eight `[ios-ui] frame N …` lines with `first_pixel` exactly `FF0000FF` (frame 0, blue), `00FF00FF` (1, green), `0000FFFF` (2, red), `00FFFFFF` (3, yellow), `FFFF00FF` (4, cyan), `FF00FFFF` (5, magenta) — BGRA byte order, `VK_FORMAT_B8G8R8A8_UNORM`. Frames 6 and 7 contain a 0.5 component and are not asserted byte-exactly.
- [ ] A screenshot taken after the run completes shows the emulator view filled with solid violet ≈ RGB(128, 0, 255) — the last presented frame, which `CALayer.contents` holds indefinitely.
- [ ] `cmake --build --preset=release` still succeeds after the `vulkan_host.cpp` change (macOS behaviour is unchanged).

**Verify:**
```bash
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | grep -c 'first_pixel=FF0000FF'
```
→ `1`

**Steps:**

- [ ] **Step 1: Obtain MoltenVK static libraries for both iOS SDKs**

```bash
mkdir -p /tmp/mvk
gh release download --repo KhronosGroup/MoltenVK --pattern 'MoltenVK-all.tar' --dir /tmp/mvk --clobber
cd /tmp/mvk && tar xf MoltenVK-all.tar
find /tmp/mvk -maxdepth 6 -name 'MoltenVK.xcframework' -type d
```

Expected: exactly one path printed, e.g. `/tmp/mvk/MoltenVK/MoltenVK/static/MoltenVK.xcframework`.
The nesting differs between MoltenVK releases, which is why this is a `find` rather than a hardcoded
path — use whatever it reports.

```bash
XCF=$(find /tmp/mvk -maxdepth 6 -name 'MoltenVK.xcframework' -type d | head -1)
ls "$XCF"
```

Expected: `Info.plist`, one device slice directory whose name contains `ios-arm64` with **no**
`simulator` suffix, and one simulator slice directory whose name contains `simulator` (typically
`ios-arm64_x86_64-simulator`). If `ls` shows a different shape, stop and read `Info.plist` —
`SupportedPlatform`/`SupportedPlatformVariant` name the slices authoritatively.

```bash
cd /Users/jack/Documents/Coding/C++/sogen/.claude/worktrees/plan-ios-cross-compilation/tools/sogen-ios
mkdir -p Vendor/MoltenVK/ios-device Vendor/MoltenVK/ios-simulator
cp "$XCF"/ios-arm64/libMoltenVK.a   Vendor/MoltenVK/ios-device/libMoltenVK.a
cp "$XCF"/*simulator*/libMoltenVK.a Vendor/MoltenVK/ios-simulator/libMoltenVK.a
lipo -info Vendor/MoltenVK/ios-device/libMoltenVK.a
lipo -info Vendor/MoltenVK/ios-simulator/libMoltenVK.a
```

Expected: the device archive reports `Non-fat file: … is architecture: arm64`; the simulator archive
reports either `arm64` or `Architectures in the fat file: … are: x86_64 arm64` — both link fine, and
`EXCLUDED_ARCHS[sdk=iphonesimulator*]: "x86_64"` from Task 4 already drops the unused slice.

If `gh release download` reports no matching asset (release asset names have changed across MoltenVK
versions), build from source instead — this is MoltenVK's own documented path and yields the same two
archives:

```bash
git clone --depth 1 https://github.com/KhronosGroup/MoltenVK.git /tmp/MoltenVK-src
cd /tmp/MoltenVK-src
./fetchDependencies --ios --iossim
make ios iossim
find Package/Latest -name libMoltenVK.a
```

Then copy the two reported archives into the same `Vendor/MoltenVK/{ios-device,ios-simulator}/`
layout. Expect 30-60 minutes for the source build.

- [ ] **Step 2: Add the iOS loader branch to `vulkan_host.cpp`**

First, the include. The file's platform block currently reads (lines 34-44):

```cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
```

Change only the `#else` arm so `TARGET_OS_IPHONE` is defined before it is tested:

```cpp
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif
#endif
```

Second, the loader-name list. The non-Windows half of the platform block currently ends like this
(lines 105-119, immediately after `free_library`):

```cpp
        void free_library(library_handle handle)
        {
            ::dlclose(handle);
        }

#if defined(__APPLE__)
        // Bare names rely on the dynamic linker's default search path, which covers Intel
        // Homebrew's /usr/local/lib but not Apple Silicon Homebrew's /opt/homebrew/lib unless
        // DYLD_LIBRARY_PATH is set; the absolute paths below are a fallback for that case.
        constexpr std::array<const char*, 5> vulkan_loader_names{"libvulkan.1.dylib", "libvulkan.dylib", "libMoltenVK.dylib",
                                                                 "/opt/homebrew/lib/libvulkan.1.dylib",
                                                                 "/opt/homebrew/lib/libMoltenVK.dylib"};
#else
        constexpr std::array<const char*, 2> vulkan_loader_names{"libvulkan.so.1", "libvulkan.so"};
#endif
#endif
```

Replace the `#if defined(__APPLE__)` … `#endif` chain (leaving `free_library` and the trailing
`#endif` that closes the outer `#ifdef _WIN32` untouched) with:

```cpp
        void free_library(library_handle handle)
        {
            ::dlclose(handle);
        }

#if defined(__APPLE__) && TARGET_OS_IPHONE
        // iOS ships no Vulkan loader, and dlopen() by leaf name never searches an app bundle, so
        // MoltenVK is linked statically into the embedding app instead (see the -force_load in
        // tools/sogen-ios/project.yml). A null path makes dlopen() return the main executable's
        // own handle, which is where vkGetInstanceProcAddr then resolves from.
        constexpr std::array<const char*, 1> vulkan_loader_names{nullptr};
#elif defined(__APPLE__)
        // Bare names rely on the dynamic linker's default search path, which covers Intel
        // Homebrew's /usr/local/lib but not Apple Silicon Homebrew's /opt/homebrew/lib unless
        // DYLD_LIBRARY_PATH is set; the absolute paths below are a fallback for that case.
        constexpr std::array<const char*, 5> vulkan_loader_names{"libvulkan.1.dylib", "libvulkan.dylib", "libMoltenVK.dylib",
                                                                 "/opt/homebrew/lib/libvulkan.1.dylib",
                                                                 "/opt/homebrew/lib/libMoltenVK.dylib"};
#else
        constexpr std::array<const char*, 2> vulkan_loader_names{"libvulkan.so.1", "libvulkan.so"};
#endif
#endif
```

No change is needed in the `impl()` constructor's loop (`vulkan_host.cpp:1149`) — it already does
`this->loader = load_library(name);` for each entry, and `load_library(nullptr)` forwards straight to
`::dlopen(nullptr, RTLD_NOW | RTLD_LOCAL)`, which on Darwin returns a handle for the main program;
`dlsym` on it searches the main executable and its dependents. The existing
`if (!this->loader) return;` guard still degrades gracefully if that ever fails.

- [ ] **Step 3: Rebuild both static-library sets and the desktop build**

```bash
cmake --build --preset=ios-embed-device
cmake --build --preset=ios-embed-simulator
cmake --build --preset=release 2>&1 | tail -3
```

Expected: all three exit 0. The `release` build is what proves the macOS loader list is untouched —
`TARGET_OS_IPHONE` is 0 on macOS, so the `#elif` arm is what compiles there.

- [ ] **Step 4: Add the per-SDK force-load flags to `project.yml`**

Under `targets.SogenIOS.settings.base`, immediately after the existing `OTHER_LDFLAGS` key, add:

```yaml
        OTHER_LDFLAGS[sdk=iphoneos*]: >-
          $(inherited)
          -force_load $(SRCROOT)/Vendor/MoltenVK/ios-device/libMoltenVK.a
        OTHER_LDFLAGS[sdk=iphonesimulator*]: >-
          $(inherited)
          -force_load $(SRCROOT)/Vendor/MoltenVK/ios-simulator/libMoltenVK.a
```

`-force_load` is required because nothing in the app *references* MoltenVK's symbols at link time —
without it the archive members are never pulled in and `dlsym` finds nothing. `-Wl,-export_dynamic`
(already in the base `OTHER_LDFLAGS` from Task 4) keeps every global symbol in the executable's
export trie so `dlsym` on the `dlopen(nullptr)` handle can see `vkGetInstanceProcAddr`, and
`DEAD_CODE_STRIPPING: NO` (also already set in Task 4) makes sure nothing force-loaded is stripped
back out. `$(inherited)` preserves the base `OTHER_LDFLAGS` value; without it the SDK-conditional key
replaces it entirely and every sogen archive drops off the link line.

MoltenVK's own framework dependencies — Metal, Foundation, QuartzCore, CoreGraphics and IOSurface —
are already in Task 4's `dependencies` list. Confirm all five are present before moving on; add any
that are missing rather than assuming.

- [ ] **Step 5: Rebuild, reinstall, run**

```bash
cd tools/sogen-ios
xcodegen generate
xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphonesimulator \
  -configuration Debug -destination 'generic/platform=iOS Simulator' \
  -derivedDataPath build build 2>&1 | tail -5
xcrun simctl install booted build/Build/Products/Debug-iphonesimulator/SogenIOS.app
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | \
  grep -E 'host Vulkan|first_pixel|\[ngcs\] done'
```

Expected:
```
NtGdiDdDDICreateDevice: host Vulkan device id=0x...
[ios-ui] frame 0 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=FF0000FF
[ios-ui] frame 1 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=00FF00FF
[ios-ui] frame 2 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=0000FFFF
[ios-ui] frame 3 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=00FFFFFF
[ios-ui] frame 4 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=FFFF00FF
[ios-ui] frame 5 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=FF00FFFF
[ios-ui] frame 6 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=...
[ios-ui] frame 7 hwnd=0x... 320x180 stride=1280 fmt=0 first_pixel=...
[ngcs] done
```

The `first_pixel` bytes are BGRA in memory order, so pure blue `(r=0, g=0, b=1, a=1)` is
`B=FF, G=00, R=00, A=FF` → `FF0000FF`. Frames 6 (orange, `1.0, 0.5, 0.0`) and 7 (violet,
`0.5, 0.0, 1.0`) round a 0.5 float component to 127 or 128 depending on the driver, so they are not
asserted byte-exactly.

- [ ] **Step 6: Capture the on-screen proof**

```bash
sleep 6
xcrun simctl io booted screenshot /tmp/sogen-ios-phase1.png
```

Open `/tmp/sogen-ios-phase1.png`. The 320×180-aspect region at the top must be a solid violet block
(≈ RGB 128, 0, 255) — frame 7, retained by `CALayer.contents` after the guest exits — with the log
text below it. The `sleep 6` is comfortably past the guest's ~3.2 s run (8 frames × 400 ms) plus
emulator startup, so this is not a timing race.

If MoltenVK reports no physical device **in the Simulator** — `enumerate_physical_devices` returns 0
and the log shows `NtGdiDdDDICreateDevice: host Vulkan not available` even with the archive linked —
that is a known iOS-Simulator Metal limitation, not a defect in this plan's code. In that case:
satisfy this task on the link-level and log-level criteria (the archive links, `dlopen(nullptr)`
resolves, no crash), record the observation explicitly in the task's completion notes, and treat
Task 7's device run as the authoritative render confirmation. Do **not** silently skip the criteria —
record which of the two cases occurred.

- [ ] **Step 7: Commit**

```bash
git add src/windows-emulator/devices/vulkan_host.cpp tools/sogen-ios/project.yml
git commit -m "feat(ios): resolve Vulkan from the app image so guest frames reach the CALayer

MoltenVK is linked statically into the embedding iOS app (-force_load), so there is no
loader dylib to dlopen by name. A null path makes dlopen return the main executable's
handle, which is where vkGetInstanceProcAddr resolves from on iOS."
```

---

### Task 6: Tap gesture → `deliver_raw_mouse_input` (input proof-of-path)

**Goal:** Tapping the emulator view produces exactly one `deliver_raw_mouse_input(0, 0, RI_MOUSE_LEFT_BUTTON_DOWN, 0)` followed by one `deliver_raw_mouse_input(0, 0, RI_MOUSE_LEFT_BUTTON_UP, 0)`, executed on the emulator's own run thread from inside `pump_events()`, with the app still running afterwards.

> **USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in `acceptanceCriteria` has been re-validated independently, with output captured.

**Files:**
- Modify: `tools/sogen-ios/Sources/Bridge/IosUiBackend.mm` (add the delivery-confirmation log line)
- Uses as-is: `tools/sogen-ios/Sources/EmulatorView.swift` (tap recognizer, Task 4), `tools/sogen-ios/Sources/Bridge/SogenBridge.mm` (`deliverTap`, Task 4)

**Acceptance Criteria:**
- [ ] Tapping the emulator view once while the guest is running appends exactly two lines to the on-screen log: `[ios-ui] delivering raw mouse input flags=0x0001` then `[ios-ui] delivering raw mouse input flags=0x0002`.
- [ ] The values are the real Win32 `RI_MOUSE_LEFT_BUTTON_DOWN` (0x0001) / `RI_MOUSE_LEFT_BUTTON_UP` (0x0002) constants from `src/emulator-platform/platform/window.hpp:326-327`, taken from the header — not literals typed into the app.
- [ ] `[ngcs] done` still appears afterwards: the tap did not crash, hang or abort the guest.
- [ ] Ten rapid taps produce twenty log lines and the app remains responsive (no deadlock between the UI thread's `queue_left_click()` and the emulator thread's `pump_events()` drain).

**Verify:**
```bash
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios 2>&1 | \
  grep -E 'delivering raw mouse input|\[ngcs\] done'
```
(tap the simulator window ~once per second during the run)
→ at least one `flags=0x0001` line, one `flags=0x0002` line, and one `[ngcs] done`

**Steps:**

- [ ] **Step 1: Confirm the wiring already written in Tasks 3 and 4 is intact**

Re-read these three places and confirm they still say exactly this (nothing new is written unless one
of them drifted):

`IosUiBackend.mm`, `queue_left_click`:
```cpp
    void ios_ui_backend::queue_left_click()
    {
        const std::lock_guard<std::mutex> lock(this->mutex_);
        this->pending_button_flags_.push_back(RI_MOUSE_LEFT_BUTTON_DOWN);
        this->pending_button_flags_.push_back(RI_MOUSE_LEFT_BUTTON_UP);
    }
```

`IosUiBackend.mm`, `pump_events` (the drain must copy under the lock and call the sink outside it —
`deliver_raw_mouse_input` must never run while `mutex_` is held):
```cpp
        std::vector<uint16_t> flags{};
        {
            const std::lock_guard<std::mutex> lock(this->mutex_);
            flags.swap(this->pending_button_flags_);
        }

        for (const auto button_flags : flags)
        {
            this->emit_log("[ios-ui] delivering raw mouse input flags=0x%04X", button_flags);
            if (this->raw_mouse_sink_)
            {
                this->raw_mouse_sink_(0, 0, button_flags, 0);
            }
        }
```

`SogenBridge.mm`, the sink installed on the run thread:
```cpp
            ui_raw->set_raw_mouse_sink(
                [emulator_ptr](const int32_t dx, const int32_t dy, const uint16_t flags, const uint16_t data) {
                    emulator_ptr->deliver_raw_mouse_input(dx, dy, flags, data);
                });
```

`EmulatorView.swift`, the recognizer and its plumb-through:
```swift
        let recognizer = UITapGestureRecognizer(target: self, action: #selector(handleTap))
        addGestureRecognizer(recognizer)
```
```swift
                onTap: { emulator?.deliverTap() }
```

- [ ] **Step 2: Rebuild and reinstall**

```bash
cd tools/sogen-ios
xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphonesimulator \
  -configuration Debug -destination 'generic/platform=iOS Simulator' -derivedDataPath build build 2>&1 | tail -3
xcrun simctl install booted build/Build/Products/Debug-iphonesimulator/SogenIOS.app
```
Expected: `** BUILD SUCCEEDED **`

- [ ] **Step 3: Run and tap**

```bash
xcrun simctl launch --console-pty booted com.jacksonyarger.sogenios
```
While the eight frames play (~3.2 s), tap the emulator view in the Simulator window. Then tap ten
times in quick succession. Capture the console output.

Expected, interleaved with the frame lines:
```
[ios-ui] delivering raw mouse input flags=0x0001
[ios-ui] delivering raw mouse input flags=0x0002
...
[ngcs] done
[sogen] guest run finished
```

`native-gpu-clear-sample` never registers for raw input, so `windows_emulator::deliver_raw_input`
will find no `raw_mouse_target`/foreground window and return early — that is expected and is not a
failure. The proof this task is after is that the Swift → Obj-C → C++ → `deliver_raw_mouse_input`
call path executes, on the right thread, without crashing.

- [ ] **Step 4: Commit**

```bash
git add tools/sogen-ios/Sources
git commit -m "feat(ios): wire the tap gesture through to deliver_raw_mouse_input"
```

---

### Task 7: Real-device confirmation pass

**Goal:** The same app, installed on a physical iPhone with ordinary personal-team signing, reaches `[ngcs] done` in its on-screen log, presents the eight guest frames (screenshot shows solid violet after the run), and logs both tap flag lines — matching the Simulator result recorded in Tasks 5 and 6, with the device console output captured to a file as evidence.

> **USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation. It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in `acceptanceCriteria` has been re-validated independently, with output captured.

**Files:**
- Modify: `docs/fex-backend.md` (extend the existing `## iOS cross-compilation` section)
- Modify: `tools/sogen-ios/README.md` (record the confirmed device model / iOS version and any device-only differences)

**Acceptance Criteria:**
- [ ] `xcodebuild … -sdk iphoneos` produces a signed `SogenIOS.app` under an ordinary personal team (no NetworkExtension or other special entitlement).
- [ ] `xcrun devicectl device install app` succeeds and the app launches from the Home Screen.
- [ ] Device log shows `NtGdiDdDDICreateDevice: host Vulkan device id=0x…`, the eight `[ios-ui] frame N …` lines with `first_pixel` values `FF0000FF, 00FF00FF, 0000FFFF, 00FFFFFF, FFFF00FF, FF00FFFF` for frames 0-5, and `[ngcs] done`.
- [ ] A device screenshot taken after the run shows the emulator view filled with solid violet ≈ RGB(128, 0, 255).
- [ ] Tapping the view logs `flags=0x0001` and `flags=0x0002`.
- [ ] Every difference from the Simulator result (including "none") is written into `tools/sogen-ios/README.md` under a `## Confirmed on` heading naming the device model and iOS version.

**Verify:**
```bash
xcrun devicectl device info details --device "$UDID" >/dev/null && \
  grep -c '\[ngcs\] done' /tmp/sogen-ios-device.log
```
→ `1`

**Steps:**

- [ ] **Step 1: Identify the device and set the signing team**

```bash
xcrun devicectl list devices
```
Record the UDID as `$UDID`. Open `SogenIOS.xcodeproj` once in Xcode and pick your personal team
under Signing & Capabilities (`CODE_SIGN_STYLE: Automatic` is already set in `project.yml`), or pass
`DEVELOPMENT_TEAM=<TEAMID>` on the `xcodebuild` command line.

- [ ] **Step 2: Build for the device**

```bash
cd tools/sogen-ios
xcodebuild -project SogenIOS.xcodeproj -scheme SogenIOS -sdk iphoneos \
  -configuration Debug -destination "platform=iOS,id=$UDID" \
  -derivedDataPath build build 2>&1 | tail -5
```
Expected: `** BUILD SUCCEEDED **`

This is the first time the `ios-embed-device` archives from Task 1 and the device MoltenVK slice from
Task 5 are actually linked; undefined symbols here are handled the same way as Task 4 Step 8.

- [ ] **Step 3: Install and provision the root on the device**

```bash
xcrun devicectl device install app --device "$UDID" \
  build/Build/Products/Debug-iphoneos/SogenIOS.app
# Launch once so the data container exists.
xcrun devicectl device process launch --device "$UDID" com.jacksonyarger.sogenios
xcrun devicectl device copy to --device "$UDID" \
  --domain-type appDataContainer --domain-identifier com.jacksonyarger.sogenios \
  --source ../../build/ios-root --destination Documents/root
```
Expected: the copy reports success. It moves ~1.8 GB / ~3100 files over USB and takes several
minutes; do not interrupt it. If `devicectl device copy to` is unavailable or errors, use the Finder
route instead — `UIFileSharingEnabled` is already set, so the app appears under the device's Files
sharing pane and `build/ios-root` can be dragged in as `root`.

- [ ] **Step 4: Run on device and capture the log**

Launch the app from the Home Screen (not from Xcode's debugger). Then:
```bash
xcrun devicectl device process launch --device "$UDID" --console com.jacksonyarger.sogenios \
  2>&1 | tee /tmp/sogen-ios-device.log
```
Tap the emulator view a few times during the ~3.2 s run.

Expected in `/tmp/sogen-ios-device.log`: the same `host Vulkan device id=`, eight `[ios-ui] frame N`
lines, `delivering raw mouse input flags=0x0001` / `0x0002`, and `[ngcs] done`.

- [ ] **Step 5: Capture the device screenshot**

Take a screenshot on the phone after the run finishes (Volume-Up + Side button), then pull it, or use
the connected-device screenshot in Xcode's Devices window. Confirm the emulator view is solid violet
≈ RGB(128, 0, 255).

- [ ] **Step 6: Record the result**

Append to `tools/sogen-ios/README.md`:

```markdown
## Confirmed on

- Simulator: <simulator device name>, iOS <version> — <result>
- Device: <iPhone model>, iOS <version> — <result>

Differences between simulator and device: <"none", or the concrete list>
```

Extend the existing `## iOS cross-compilation` section of `docs/fex-backend.md` with:

```markdown
### Embeddable static libraries and the Phase 1 app shell

`cmake --workflow --preset=ios-embed-device` / `--preset=ios-embed-simulator` build
`windows-emulator`, `backend-selection` and `unicorn-emulator` as plain (LTO-free) static archives
against the `iphoneos` and `iphonesimulator` SDKs. They set `SOGEN_BUILD_STATIC=ON`,
`SOGEN_BUILD_TOOLS=OFF` and `SOGEN_ENABLE_LTO=OFF`; the simulator preset uses the separate
`cmake/toolchain/ios-simulator.cmake`.

`tools/sogen-ios/` is an xcodegen-generated iOS app that links those archives, injects its own
`CALayer`-backed `ui_backend` through `emulator_interfaces::ui`, and runs
`native-gpu-clear-sample.exe` on the Unicorn backend. It is an ordinary app: no JIT, no
entitlements beyond personal-team signing. MoltenVK is linked statically and reached via the
`TARGET_OS_IPHONE` branch of `vulkan_host.cpp`'s loader list (`dlopen(nullptr)` = the app image).

The emulation root (~1.8 GB: `filesys/c/windows`, `registry`, `api-set.bin`, staged by
`tools/stage-ios-emulation-root.sh`) is provisioned into the app's Documents container with
`simctl`/`devicectl` rather than bundled. Phase 2 (FEX backend + the JIT26 breakpoint protocol +
the three-target JIT-granting architecture) remains out of scope.
```

- [ ] **Step 7: Commit**

```bash
git add docs/fex-backend.md tools/sogen-ios/README.md
git commit -m "docs(ios): record the Phase 1 device confirmation result"
```

---

### Task 8: Pull forward the JIT-granting machinery so Unicorn's own TCG JIT can run on real hardware

**Added after Task 7's real-device run.** Task 7 found that `sogen::windows_emulator` running on the Unicorn backend crashes on real iOS hardware (not the Simulator) the moment the guest exercises a code path that requires a *new* TCG translation: `EXC_BAD_ACCESS`/`SIGKILL`, `KERN_PROTECTION_FAILURE` at a 128MB `VM_ALLOCATE` JIT-buffer region, `CODESIGNING: Invalid Page`. The faulting stack is entirely inside Unicorn's own vCPU-resume path (`resume_all_vcpus_x86_64` → `uc_emu_start` → `unicorn_x86_64_emulator::start`) — nothing to do with Tasks 3/4/6's tap-delivery code. This falsifies the design spec's premise that "Unicorn is a pure C++ interpreter [that] generates no code and needs no `CS_DEBUGGED`": Unicorn's TCG (Tiny Code Generator) translates guest x86 into host ARM64 machine code and executes it directly — a JIT, same fundamental mechanism as FEX. Neither macOS nor the iOS Simulator enforce the TXM/SPTM-level JIT restriction real hardware does, which is why Tasks 2 and 4-6 never saw this.

**User decision:** rather than search for a JIT-free Unicorn build mode or defer this to Phase 2, the user directed pulling forward the already-proven JIT-granting architecture from this session's out-of-tree spike (`.worktrees/ios-jit-test/tools/ios-jit-test/`, branch `spike/ios-jit-self-attach` — a fully self-contained host app + ExtensionKit `JITHelper` extension (vendoring StikJIT) + `TunnelExtension` `NEPacketTunnelProvider`, proven end-to-end on real iOS 26 hardware earlier this session) into `tools/sogen-ios/`, and drive the JIT26 breakpoint-protocol dance before `windows_emulator::start()` so Unicorn's TCG buffer gets its W→X transition allowed.

**Goal:** the same device build from Task 7, now additionally performing the JIT-grant sequence at launch (self-attach/extension-driven `CS_DEBUGGED` + JIT26 `brk` protocol) before starting the emulator, so the guest's tap-triggered new-code-path execution — the exact scenario that crashed in Task 7 — completes without a `CODESIGNING`/`KERN_PROTECTION_FAILURE` crash on the real iPhone.

**Files:**
- Port (adapt paths/bundle-ids, do not blindly copy build artifacts): `Sources/JIT26.{c,h}`, `Sources/JITExec.{c,h}`, `Sources/JITSelfAttach.{c,h}`, `Sources/JITCoordinator.swift`, `Sources/JITExtensionPoint.swift`, `Sources/JITLog.swift`, `Sources/StikDebugBridge.swift`, `Sources/TunnelManager.swift`, `Sources/JITBridge/JITMessage.swift` from `.worktrees/ios-jit-test/tools/ios-jit-test/` into new locations under `tools/sogen-ios/`
- Port: `HelperExtension/JITHelperExtension.swift` + `HelperExtension/Info.plist` → a new `tools/sogen-ios/HelperExtension/` target
- Port: `TunnelExtension/PacketTunnelProvider.swift` + `Info.plist` + `TunnelExtension.entitlements` → a new `tools/sogen-ios/TunnelExtension/` target
- Port: the vendored StikJIT source/framework the spike used (check `.gitignore`/`Vendor/` in the spike for exactly what was vendored vs. fetched)
- Modify: `tools/sogen-ios/project.yml` — add the two new targets (ExtensionKit appex + NetworkExtension appex) with the exact gotchas already discovered in the spike: `EX_ENABLE_EXTENSION_POINT_GENERATION` set on BOTH the host app AND the extension target (not just the extension), `VALIDATE_PRODUCT: NO` for unsigned intermediate builds, no `-framework XPC` (Swift overlay only, `import XPC` suffices)
- Modify: `tools/sogen-ios/JITTest.entitlements`-equivalent (a new `tools/sogen-ios/SogenIOS.entitlements`) and the two extension entitlements files
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.mm` or `SogenApp.swift` — drive the JIT-grant sequence (attach → breakpoint protocol → confirm granted) on a background queue BEFORE constructing `windows_emulator`/calling `start()`, using the same concurrency fix Test 5 in the spike required (run the `brk`-protocol calls concurrently with, not sequentially after, the XPC call to the helper — sequencing them after deadlocks, per the spike's own hard-won fix)

**Acceptance Criteria:**
- Device build (personal-team signing, same as Task 7 — no new entitlement beyond what the spike already proved works with a personal JIT-enabled dev certificate) installs and launches with the JITHelper/TunnelExtension targets embedded
- App log (on-screen, per the established constraint) shows the JIT-grant sequence completing (`CS_DEBUGGED` true, JIT26 region prepared) before `[sogen] starting guest`
- The exact Task 7 repro (tap the emulator view once while the guest runs) no longer crashes — the guest continues running past the point that previously faulted
- `cmake --build --preset=release` and the Task 1/4/5 build presets remain unaffected (this task only touches `tools/sogen-ios/` and its own entitlements/targets, not `src/` or CMake)

**Verify:** repeat Task 7's device run + tap; confirm no `CODESIGNING`/`KERN_PROTECTION_FAILURE` crash log appears in `xcrun devicectl device info files --domain-type systemCrashLogs` for the several minutes after the tap.

**Steps:** read the spike's source in full first (`.worktrees/ios-jit-test/tools/ios-jit-test/`), including its `README.md`/any docs describing Test 5's deadlock-then-crash-then-fix history — this session already paid for those lessons once, port the fixed version, not a naive first attempt. Then integrate exactly as described above, iterating on real device builds the same way Tasks 4/5/7 did.

---

## Self-Review

**1. Spec coverage** — every section of `docs/superpowers/specs/2026-09-04-ios-app-shell-phase1-design.md` maps to a task:

| Spec section | Task |
|---|---|
| `sogen-ios-embed` static-library configuration (`SOGEN_BUILD_STATIC=ON`, `SOGEN_BUILD_TOOLS=OFF`, new preset) | 1 |
| `cmake/toolchain/ios-simulator.cmake` (Testing Strategy) | 1 |
| New `ui_backend` implementation, `present_surface` → `CALayer` | 3 |
| The iOS app itself: bundles the guest exe, `create_x86_64_emulator(unicorn, 1)`, `application_settings.application`, wires the ui_backend, calls `start()` | 4 |
| Rendering: present the existing headless pixel buffer; "the same solid color the macOS SDL window already shows" | 5 |
| Input: one `UITapGestureRecognizer` → `deliver_raw_mouse_input` | 6 |
| Build Integration: xcodegen `project.yml`, CMake as a manual pre-step, decoupled build systems | 1 + 4 |
| Testing Strategy: simulator first, device last | 4/5/6 (simulator), 7 (device) |
| Error Handling: visible on-screen text if load/start fails | 4 (Step 9 criterion, `validatePaths` + on-screen log) |
| Error Handling: log-and-skip a bad `ui_surface_desc` | 3 (acceptance criterion 3) |
| Out of scope (FEX, JIT26, 3-target architecture, entitlements, real control scheme, other guests) | not planned anywhere |

**Gap found and closed:** the spec is silent on the emulation root, but the emulator throws
`"Emulation root directory can not be empty!"` on every non-Windows host and the full root is 44 GB.
Task 2 was added to stage a 1.8 GB subset and prove it runs, and Task 4/7 provision it into the app
container rather than the bundle. **Second gap found and closed:** the spec says "no new GPU/Vulkan-layer
work", but `present_surface` is unreachable without a host Vulkan device, so Task 5 was added.

**2. Placeholder scan** — no "TBD", "implement later", "add appropriate …", "similar to Task N", or
code-free code steps. The two places with a discovery loop (Task 4 Step 8's framework/archive link
errors, Task 5 Step 1's MoltenVK slice directory names) both give the exact command, the exact
expected output shape, and the exact rule for what to do with the result — they are not
"figure it out" instructions. Task 1's expected artifact list and Task 5's expected pixel values are
concrete.

**3. Type consistency** — checked across tasks: `sogen::ios_ui_backend` (Task 3) is the exact type
`SogenBridge.mm` constructs (Task 4); `set_raw_mouse_sink` / `set_log_sink` / `queue_left_click` /
`presented_frame_count` are spelled identically in the header, the `.mm`, and Task 6's re-read step;
`SogenEmulator`'s `initWithLayer:emulationRoot:guestExecutablePath:`, `onLogLine`, `start`, `stop`,
`deliverTap` match one-for-one between `SogenBridge.h`, `SogenBridge.mm` and `ContentView.swift`;
the bundle id `com.jacksonyarger.sogenios` is identical in `project.yml`, the README, and every
`simctl`/`devicectl` command; the preset names `ios-embed-device` / `ios-embed-simulator` are
identical in `CMakePresets.json`, the `LIBRARY_SEARCH_PATHS` in `project.yml`, and the README.
