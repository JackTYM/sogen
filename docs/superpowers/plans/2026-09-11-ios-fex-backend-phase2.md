# iOS App Shell, Phase 2 (FEX backend) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get FEX's JIT executing `native-gpu-clear-sample.exe` on iOS — first in the Simulator (no JIT26 involved), then via a JIT26-blessed FEXCore allocator on real device — as a narrow proof phase, matching Phase 1's own scope discipline.

**Architecture:** Widen the CMake gate that excludes iOS from `SOGEN_ENABLE_FEX`, forward real cross-compilation toolchain info into FEXCore's `ExternalProject` build (which today silently configures for the host Mac), build FEXCore as a **static** library for iOS (mirroring how MoltenVK is already `-force_load`'d as a static archive rather than embedded as a dylib), get it running the proof sample in the Simulator, then port the same JIT26 breakpoint-protocol/splitwx technique already proven for Unicorn's TCG buffer into FEXCore's own allocator for real device.

**Tech Stack:** CMake `ExternalProject`, FEXCore (C++20, `deps/FEX` — `github.com/JackTYM/FEX` fork), Xcode/`xcodegen` (`tools/sogen-ios/project.yml`), Objective-C++ bridge (`SogenBridge.mm`), the existing JIT26 breakpoint-protocol C code (`tools/sogen-ios/Sources/JIT/JIT26.c`).

**User decisions (already made):**
- FEX backend is the next phase, ahead of LocalDevVPN, input/UX polish, and profiles/root-management UI (in that order).
- Scope is a proof only: get the existing `native-gpu-clear-sample.exe` running via FEX, no backend-picker UI, no LocalDevVPN, no input/UX work.
- Reuse the existing JIT26 debugger-attach machinery (`JITHelper`/`TunnelExtension`) unmodified; only adapt it if real testing shows a conflict.
- The JIT26 client primitives get a **self-contained copy inside the `deps/FEX` fork**, not a shared library extracted from the app target, and not `-undefined dynamic_lookup` symbol resolution.
- All development and iteration happens in the iOS **Simulator first**; the real-device build is handed to the user directly for their own demo, not verified by an agent doing live on-device debugging.

---

## Task 1: Widen the CMake gate so `SOGEN_ENABLE_FEX` recognizes iOS

**Goal:** `cmake --preset=ios-embed-simulator` (and `ios-embed-device`) reports `SOGEN_ENABLE_FEX: enabled`, without affecting any existing Linux/Darwin-desktop configure.

**Files:**
- Modify: `CMakeLists.txt:67-69`

**Acceptance Criteria:**
- [ ] Configuring with `ios-embed-simulator` or `ios-embed-device` prints `FEX backend: enabled (ARM64 iOS + Clang, deps/FEX present)` (or equivalent — see Step 3) instead of silently leaving `SOGEN_ENABLE_FEX` OFF.
- [ ] Configuring with `release` (desktop macOS) still reports the existing `FEX backend: enabled (ARM64 Darwin + Clang, deps/FEX present)` message unchanged.
- [ ] Configuring on Linux (or any non-Clang/non-arm64 host, if available to test) still leaves `SOGEN_ENABLE_FEX` OFF exactly as before.

**Verify:** `cmake --preset=ios-embed-simulator 2>&1 | grep "FEX backend"` → prints an "enabled" line mentioning iOS.

**Steps:**

- [ ] **Step 1: Read the current gate**

`CMakeLists.txt:59-74` currently reads:
```cmake
if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64") AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/FEX/CMakeLists.txt"
   AND ((CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID) OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
  set(SOGEN_ENABLE_FEX ON)
  message(STATUS "FEX backend: enabled (ARM64 ${CMAKE_SYSTEM_NAME} + Clang, deps/FEX present)")
else()
  set(SOGEN_ENABLE_FEX OFF)
endif()
```
Both `cmake/toolchain/ios.cmake` and `cmake/toolchain/ios-simulator.cmake` set `CMAKE_SYSTEM_NAME` to `"iOS"` and `CMAKE_SYSTEM_PROCESSOR` to `"arm64"` explicitly (CMake otherwise leaves the latter empty for an iOS toolchain), so the processor/compiler clauses already match — only the `CMAKE_SYSTEM_NAME` clause excludes iOS.

- [ ] **Step 2: Widen the last clause**

```cmake
if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64") AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/FEX/CMakeLists.txt"
   AND ((CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID) OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"
        OR CMAKE_SYSTEM_NAME STREQUAL "iOS"))
  set(SOGEN_ENABLE_FEX ON)
  message(STATUS "FEX backend: enabled (ARM64 ${CMAKE_SYSTEM_NAME} + Clang, deps/FEX present)")
else()
  set(SOGEN_ENABLE_FEX OFF)
endif()
```

- [ ] **Step 3: Verify on both an iOS preset and the existing desktop preset**

Run:
```sh
cmake --preset=ios-embed-simulator 2>&1 | grep "FEX backend"
cmake --preset=release 2>&1 | grep "FEX backend"
```
Expected: the first line mentions `iOS`; the second is unchanged from before this edit (`Darwin`).

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "build(ios): recognize iOS in the FEX-enable gate"
```

---

## Task 2: Build FEXCore as a static library for iOS, with real cross-compilation forwarding

**Goal:** `deps/CMakeLists.txt`'s `fex_external` `ExternalProject` produces a genuine `arm64-apple-ios`/`arm64-apple-ios-simulator` static `libFEXCore.a` (not a host-Mac dylib), imported as `fexcore` for iOS builds — leaving the existing Darwin-desktop/Linux shared-library path completely untouched.

**Files:**
- Modify: `deps/CMakeLists.txt:142-223`

**Acceptance Criteria:**
- [ ] On `ios-embed-simulator`, the `fex_external` ExternalProject's own build log shows it configuring with `CMAKE_SYSTEM_NAME=iOS` and the iOS simulator sysroot/arch (not the host Mac's).
- [ ] `libFEXCore.a` for the target architecture (confirm via `file`/`lipo -info`) appears at the expected build-tree path after building.
- [ ] The existing desktop (`release`) and Linux configure/build paths are bit-for-bit unaffected (same `FEXCore_shared`/`.dylib`/`.so` target, same `IMPORTED_LOCATION`).

**Verify:** `cmake --build build/ios-embed-simulator --target fex_external && file build/ios-embed-simulator/deps/fex_external-prefix/src/fex_external-build/FEXCore/Source/libFEXCore.a` → reports a Mach-O arm64 object file for the simulator platform.

**Steps:**

- [ ] **Step 1: Confirm FEXCore already has a static-library target**

`deps/FEX/FEXCore/Source/CMakeLists.txt:305-306` already defines both:
```cmake
AddLibrary(${PROJECT_NAME} STATIC)
AddLibrary(${PROJECT_NAME}_shared SHARED)
```
So a `FEXCore` (static) target exists today, alongside `FEXCore_shared`, without any submodule changes needed for this part. Confirm this is still true on the currently-checked-out `deps/FEX` commit before proceeding (`grep -n "AddLibrary(\${PROJECT_NAME}" deps/FEX/FEXCore/Source/CMakeLists.txt`).

- [ ] **Step 2: Branch the Darwin-family/library-kind logic on iOS too, and pick the static target for iOS**

Current code (`deps/CMakeLists.txt:142-186`):
```cmake
if(SOGEN_ENABLE_FEX)
  include(ExternalProject)

  set(_FEX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/FEX")

  if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_FEXCORE_SHARED_LIB "libFEXCore.dylib")
    set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
  else()
    set(_FEXCORE_SHARED_LIB "libFEXCore.so")
    set(_FEXCORE_OSX_ARGS "")
  endif()
  ...
  ExternalProject_Add(fex_external
    SOURCE_DIR "${_FEX_SRC}"
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      ${_FEXCORE_OSX_ARGS}
      ${_FEXCORE_SANITIZER_ARGS}
      -DENABLE_LTO=OFF
      -DENABLE_CCACHE=OFF
      -DBUILD_FEXCONFIG=OFF
      -DBUILD_THUNKS=OFF
      -DBUILD_FEX_LINUX_TESTS=OFF
      -DBUILD_TESTING=OFF
      -DENABLE_OFFLINE_TELEMETRY=OFF
      -DENABLE_FEX_ALLOCATOR=OFF
      -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target FEXCore_shared
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS <BINARY_DIR>/FEXCore/Source/${_FEXCORE_SHARED_LIB}
  )

  ExternalProject_Get_property(fex_external BINARY_DIR)

  add_custom_command(
    TARGET fex_external POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_SHARED_LIB}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    COMMENT "Copying libFEXCore shared library to the artifacts directory"
  )

  add_library(fexcore SHARED IMPORTED GLOBAL)
  set_target_properties(fexcore PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_SHARED_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_FEX_SRC}/FEXCore/include;${BINARY_DIR}/include;${_FEX_SRC}/External/fmt/include;${_FEX_SRC}/External/unordered_dense/include"
    INTERFACE_COMPILE_DEFINITIONS "FMT_HEADER_ONLY=1")
endif()
```

Replace it with a variant that branches once more on `CMAKE_SYSTEM_NAME STREQUAL "iOS"` for the library kind (static, matching how MoltenVK is already statically `-force_load`'d into the iOS app rather than embedded as a dylib — see `tools/sogen-ios/project.yml`'s existing `-force_load .../libMoltenVK.a` lines), while forwarding the real active iOS toolchain file so the nested `ExternalProject` configure actually cross-compiles instead of silently targeting the host Mac:

```cmake
if(SOGEN_ENABLE_FEX)
  include(ExternalProject)

  set(_FEX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/FEX")
  set(_FEXCORE_IOS_ARGS "")

  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_FEXCORE_BUILD_TARGET "FEXCore")
    set(_FEXCORE_LIB "libFEXCore.a")
    set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    # ExternalProject spawns a genuinely separate nested `cmake` configure - it does NOT inherit
    # CMAKE_TOOLCHAIN_FILE/CMAKE_OSX_SYSROOT/CMAKE_OSX_ARCHITECTURES from the outer configure
    # automatically. Forward the same toolchain file and OSX settings the outer ios-embed-device/
    # ios-embed-simulator preset is already using, or FEXCore would silently configure for the host
    # Mac instead of iOS.
    set(_FEXCORE_IOS_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_FEXCORE_BUILD_TARGET "FEXCore_shared")
    set(_FEXCORE_LIB "libFEXCore.dylib")
    set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
  else()
    set(_FEXCORE_BUILD_TARGET "FEXCore_shared")
    set(_FEXCORE_LIB "libFEXCore.so")
    set(_FEXCORE_OSX_ARGS "")
  endif()

  if(SOGEN_ENABLE_SANITIZER)
    set(_FEXCORE_SANITIZER_ARGS -DENABLE_ASAN=ON)
  else()
    set(_FEXCORE_SANITIZER_ARGS "")
  endif()

  ExternalProject_Add(fex_external
    SOURCE_DIR "${_FEX_SRC}"
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=Release
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      ${_FEXCORE_OSX_ARGS}
      ${_FEXCORE_IOS_ARGS}
      ${_FEXCORE_SANITIZER_ARGS}
      -DENABLE_LTO=OFF
      -DENABLE_CCACHE=OFF
      -DBUILD_FEXCONFIG=OFF
      -DBUILD_THUNKS=OFF
      -DBUILD_FEX_LINUX_TESTS=OFF
      -DBUILD_TESTING=OFF
      -DENABLE_OFFLINE_TELEMETRY=OFF
      -DENABLE_FEX_ALLOCATOR=OFF
      -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target ${_FEXCORE_BUILD_TARGET}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS <BINARY_DIR>/FEXCore/Source/${_FEXCORE_LIB}
  )

  ExternalProject_Get_property(fex_external BINARY_DIR)

  file(MAKE_DIRECTORY "${BINARY_DIR}/include")

  add_custom_command(
    TARGET fex_external POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_LIB}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    COMMENT "Copying libFEXCore to the artifacts directory"
  )

  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_library(fexcore STATIC IMPORTED GLOBAL)
  else()
    add_library(fexcore SHARED IMPORTED GLOBAL)
  endif()
  set_target_properties(fexcore PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_FEX_SRC}/FEXCore/include;${BINARY_DIR}/include;${_FEX_SRC}/External/fmt/include;${_FEX_SRC}/External/unordered_dense/include"
    INTERFACE_COMPILE_DEFINITIONS "FMT_HEADER_ONLY=1")
endif()
```

Note: `CMAKE_OSX_SYSROOT`/`CMAKE_OSX_ARCHITECTURES` are already set by the outer configure (via `cmake/toolchain/ios.cmake`/`ios-simulator.cmake`) by the time `deps/CMakeLists.txt` runs, so forwarding them here is just passing through the outer values — no new values need inventing.

- [ ] **Step 3: Build and inspect**

```sh
cmake --preset=ios-embed-simulator
cmake --build build/ios-embed-simulator --target fex_external
file build/ios-embed-simulator/deps/fex_external-prefix/src/fex_external-build/FEXCore/Source/libFEXCore.a
```
Expected: a real Mach-O arm64 (simulator) static archive, not a host x86_64/arm64-macOS one, and not a dylib.

- [ ] **Step 4: Confirm the desktop path is unaffected**

```sh
cmake --preset=release
cmake --build build/release --target fex_external
file build/release/deps/fex_external-prefix/src/fex_external-build/FEXCore/Source/libFEXCore.dylib
```
Expected: unchanged from before this task (still a host-Mac dylib), and `cmake --build --preset=release --target windows-emulator-test` still passes in full (see Task 3's own verification for the full run — this step is just confirming nothing regressed yet).

- [ ] **Step 5: Commit**

```bash
git add deps/CMakeLists.txt
git commit -m "build(ios): produce a static FEXCore for iOS instead of a dylib, with real cross-compilation forwarding"
```

---

## Task 3: Get `fex-emulator`/`backend-selection` building as part of the iOS static-archive presets

**Goal:** The `ios-embed-simulator`/`ios-embed-device` presets (which already build `windows-emulator`, `backend-selection`, and `unicorn-emulator` as plain static archives per Phase 1) also build `fex-emulator`, with `SOGEN_ENABLE_FEX`/`backend_type::fex` genuinely wired in, fixing whatever real iOS-specific compile errors this surfaces.

**Files:**
- Modify: `CMakePresets.json` (the `ios-embed-device`/`ios-embed-simulator` preset definitions, wherever they enumerate which targets to build)
- Modify: whatever real compile errors surface (exact files unknown until built — see Step 2)

**Acceptance Criteria:**
- [ ] `cmake --build build/ios-embed-simulator --target fex-emulator` succeeds and produces a plain (non-LTO) static archive for `fex-emulator`, matching how `unicorn-emulator` already builds for this preset.
- [ ] Same for `ios-embed-device`.
- [ ] `windows-emulator-test` (desktop) still passes in full — this task must not regress the existing, working desktop FEX backend.

**Verify:** `cmake --build build/ios-embed-simulator --target fex-emulator && cmake --build --preset=release --target windows-emulator-test && ctest --test-dir build/release` → both succeed.

**Steps:**

- [ ] **Step 1: Add `fex-emulator` to the ios-embed presets' target list**

Find where `CMakePresets.json`'s `ios-embed-device`/`ios-embed-simulator` build presets currently enumerate `windows-emulator`/`backend-selection`/`unicorn-emulator` as explicit build targets, and add `fex-emulator` alongside them.

- [ ] **Step 2: Build and fix whatever real errors surface**

```sh
cmake --preset=ios-embed-simulator
cmake --build build/ios-embed-simulator --target fex-emulator
```
This is genuinely unpredictable ahead of time (the original iOS cross-compilation plan hit exactly one real compile error — a FreeType macro-redefinition clash — that could not have been listed in advance either; see `docs/fex-backend.md`'s own porting notes for that precedent). Read each error, fix it at its real source, and re-build, the same way that plan's Step 2 worked. Do not add workarounds that paper over a real incompatibility (e.g. do not `#ifdef` out a whole subsystem just to make it compile) — fix the actual portability gap, matching this codebase's established standard from every prior round this session.

- [ ] **Step 3: Confirm desktop is unaffected**

```sh
cmake --build --preset=release --target windows-emulator-test
ctest --test-dir build/release
```
Expected: full pass, identical to before this task.

- [ ] **Step 4: Commit**

```bash
git add CMakePresets.json <any files touched in Step 2>
git commit -m "build(ios): build fex-emulator for the iOS static-archive presets"
```

---

## Task 4: Link FEXCore into the iOS app and add a way to select the FEX backend

**Goal:** `SogenIOS.app` links `libFEXCore.a` and `fex-emulator`'s static archive, and can construct a FEX-backed emulator instead of Unicorn via a compile-time/env toggle (no UI picker).

**Files:**
- Modify: `tools/sogen-ios/project.yml` (link flags)
- Modify: `tools/sogen-ios/Sources/Bridge/SogenBridge.mm:168`

**Acceptance Criteria:**
- [ ] `SogenIOS`'s `OTHER_LDFLAGS` `-force_load`s the built `libFEXCore.a` for both `iphoneos*`/`iphonesimulator*` SDKs, mirroring the existing MoltenVK lines exactly.
- [ ] `fex-emulator`'s own static archive is on the app's link line (via `LIBRARY_SEARCH_PATHS`, already pointing at `build/ios-embed-device/artifacts` / `build/ios-embed-simulator/artifacts` per Phase 1 — confirm `fex-emulator`'s output lands there too, from Task 3).
- [ ] Setting the toggle (see Step 2) causes `SogenBridge.mm` to construct `sogen::backend_type::fex` instead of `sogen::backend_type::unicorn`; leaving it unset preserves today's Unicorn-only behavior exactly.
- [ ] The app still builds and launches in the Simulator with the toggle unset (Unicorn path unchanged).

**Verify:** Build+run in the Simulator with the toggle unset → behaves exactly as Phase 1 left it (Unicorn, working). Build+run with the toggle set → app launches (rendering correctness is Task 5's job, not this one).

**Steps:**

- [ ] **Step 1: Add the force_load lines**

In `tools/sogen-ios/project.yml`, alongside the existing MoltenVK lines (`-force_load $(SRCROOT)/../../build/ios-embed-device/artifacts/libMoltenVK.a`-style entries — confirm exact existing paths before editing), add matching `-force_load` entries pointing at wherever Task 2/3's build actually places `libFEXCore.a` and `fex-emulator`'s static archive for each SDK (`build/ios-embed-device/artifacts/...` / `build/ios-embed-simulator/artifacts/...`, matching the existing `LIBRARY_SEARCH_PATHS` entries at `project.yml:97-98`).

- [ ] **Step 2: Add the backend-selection toggle**

`SogenBridge.mm:168` currently reads:
```objc
auto emu = sogen::create_x86_64_emulator(sogen::backend_type::unicorn, 1);
```
Change to:
```objc
#if defined(SOGEN_IOS_USE_FEX)
        const auto backend = sogen::backend_type::fex;
#else
        const auto backend = sogen::backend_type::unicorn;
#endif
        auto emu = sogen::create_x86_64_emulator(backend, 1);
```
Add a corresponding `SOGEN_IOS_USE_FEX` compile definition switch to `project.yml` (e.g. a scheme/configuration-level `GCC_PREPROCESSOR_DEFINITIONS` entry that's easy to flip for this proof phase — no UI, per scope).

- [ ] **Step 3: Build and smoke-test both configurations in the Simulator**

Build+run with the toggle unset: confirm the app behaves exactly as Phase 1 left it (Unicorn, `[ngcs] done`, all 8 colors). Build+run with the toggle set: confirm the app at least launches without a link/load error (correctness is Task 5).

- [ ] **Step 4: Commit**

```bash
git add tools/sogen-ios/project.yml tools/sogen-ios/Sources/Bridge/SogenBridge.mm
git commit -m "feat(ios): link FEXCore and add a compile-time FEX backend toggle"
```

---

## Task 5: Simulator proof — FEX renders all 8 frames correctly

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation ("all work should be done starting in Simulator and then once its assumed to have worked I will demo on a real iphone for you"). It MUST NOT be closed by walking around it, by declaring it "verified inline", or by substituting a cheaper check. Close only after every item in `acceptanceCriteria` has been re-validated independently, with output captured.

**Goal:** With the Task 4 toggle set to FEX, `native-gpu-clear-sample.exe` runs to completion in the iOS Simulator via the FEX backend, rendering all 8 clear-color frames correctly and ending on violet — matching Phase 1's own Task 7 success criteria, on Simulator, via FEX instead of Unicorn.

**Files:** None expected beyond whatever real bugs Step 2 surfaces (unpredictable ahead of time, same caveat as Task 3).

**Acceptance Criteria:**
- [ ] The Simulator's on-screen view cycles blue → green → red → yellow → cyan → magenta → orange → violet and holds on violet.
- [ ] The captured log shows `[ngcs] done`.
- [ ] The captured log shows `[ios-ui] frame 7 ... first_pixel=...` decoding to violet (`FF0080FF` in the BGRA byte order this app already uses, per the Phase 1 rendering fix), or equivalent direct evidence the correct final frame reached the screen.
- [ ] A log line or other direct evidence confirms the **FEX** backend actually executed (not a silent fallback to Unicorn) — e.g. log the selected `backend_type` at startup if nothing already does.

**Verify:** Run the app in the Simulator with the FEX toggle set, capture the log (same method used throughout Phase 1 — `sogen_log.txt` via the app's Documents folder, or `xcrun simctl launch --console-pty`), confirm all criteria above.

**Steps:**

- [ ] **Step 1: Add a startup log line confirming the active backend**

In `SogenBridge.mm`, right after constructing `emu` (Task 4, Step 2), log which backend was actually selected (e.g. `appendLog("[sogen] backend: fex")` / `"[sogen] backend: unicorn"` via the existing log-callback mechanism), so this gate's last acceptance criterion has direct evidence rather than an inference.

- [ ] **Step 2: Run in the Simulator, fix whatever real bugs surface**

Boot a Simulator, build+install+launch with the FEX toggle set (mirroring the exact process already used successfully during Phase 1's rendering-bug hunt: `xcrun simctl boot`/`install`/`launch`, read `sogen_log.txt` from the Simulator's container filesystem directly). This is genuinely unpredictable — FEX has never run on this platform before. Trace and fix real bugs with real evidence (added logging, not guessing), the same standard used throughout Phase 1's entire JIT/rendering investigation. Do not proceed to Task 6 until this is genuinely green.

- [ ] **Step 3: Capture and verify evidence for every acceptance criterion**

Re-read the final log and confirm each criterion above explicitly before considering this task done.

- [ ] **Step 4: Commit**

```bash
git add <any files touched in Step 1/2>
git commit -m "fix(ios): get FEX backend rendering correctly in the Simulator"
```

---

## Task 6: Port the JIT26 client primitives into the deps/FEX fork

**Goal:** `deps/FEX` gains its own self-contained copy of the JIT26 breakpoint-protocol client primitives, independent of the app target's `JIT26.c`, so `libFEXCore.a` never needs to resolve those symbols across a library boundary.

**Files:**
- Create: `deps/FEX/FEXCore/Source/Utils/JIT26.c` (or `.cpp` — match the existing file's language; check `tools/sogen-ios/Sources/JIT/JIT26.c`'s actual content before choosing)
- Create: `deps/FEX/FEXCore/include/FEXCore/Utils/JIT26.h`
- Reference (read-only, do not modify): `tools/sogen-ios/Sources/JIT/JIT26.c`, `tools/sogen-ios/Sources/JIT/JIT26.h`

**Acceptance Criteria:**
- [ ] The new files compile as part of FEXCore's build for iOS device (`ios-embed-device`), gated so they compile to inert/unused code on Simulator and desktop (mirroring `src/common/utils/ios_device_jit_mmap_shim.cpp`'s own `#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR` gate).
- [ ] `jit26_prepare_region`, `jit26_writable_alias`, `jit26_detach`, `jit26_is_debugged` are declared with identical signatures to the app target's existing `JIT26.h`.
- [ ] Building `ios-embed-simulator` and desktop `release` are both unaffected (the new files are inert there).

**Verify:** `cmake --build build/ios-embed-device --target fex_external` succeeds with the new files compiled in; `cmake --build --preset=release --target windows-emulator-test && ctest --test-dir build/release` still passes in full.

**Steps:**

- [ ] **Step 1: Copy the real implementation, not a stub**

Read `tools/sogen-ios/Sources/JIT/JIT26.c` and `.h` in full. Copy their real content into the two new `deps/FEX` files verbatim (adjusting only the include guard name and any app-target-specific includes that don't apply inside `deps/FEX`, e.g. anything pulling in Swift-bridging headers). This is the exact breakpoint-driven protocol client (`brk #0xf00d` with `x16=1`/`x16=0`) already proven working for Unicorn's TCG buffer — do not reimplement it from a written description, copy the real, tested bytes.

- [ ] **Step 2: Gate the whole file for real device only**

Match `src/common/utils/ios_device_jit_mmap_shim.cpp`'s own top-of-file pattern:
```c
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
/* ... real implementation ... */
#endif
```

- [ ] **Step 3: Wire the new files into FEXCore's Source/CMakeLists.txt**

Add `Utils/JIT26.c` (or `.cpp`) to whatever source list feeds the `FEXCore`/`FEXCore_shared` targets in `deps/FEX/FEXCore/Source/CMakeLists.txt` (the same list containing `Utils/AllocatorHooks.cpp`, per the earlier `grep` in this plan's research — add it alongside that entry).

- [ ] **Step 4: Build and confirm inertness elsewhere**

```sh
cmake --build build/ios-embed-device --target fex_external
cmake --build build/ios-embed-simulator --target fex_external
cmake --build --preset=release --target windows-emulator-test
ctest --test-dir build/release
```
All four must succeed, with the new file compiling to nothing observable outside real iOS device.

- [ ] **Step 5: Commit (inside the deps/FEX submodule, then bump the pin)**

```bash
cd deps/FEX
git add FEXCore/Source/Utils/JIT26.c FEXCore/include/FEXCore/Utils/JIT26.h FEXCore/Source/CMakeLists.txt
git commit -m "feat(ios): add a self-contained JIT26 breakpoint-protocol client"
git push origin HEAD:macos-arm64   # deps/FEX's actual fork branch (confirmed via deps/FEX/CLAUDE.md) -- NOT "dev" (that's deps/unicorn's branch, a different fork)
cd ../..
git add deps/FEX
git commit -m "chore(deps): bump FEX pin — self-contained JIT26 client primitives"
```

---

## Task 7: Wire FEXCore's JIT allocator through JIT26 on real device

**Goal:** On real iOS device, FEXCore's JIT code-buffer allocation goes through `jit26_prepare_region`/`jit26_writable_alias` (the exact RX/RW splitwx pattern already proven for Unicorn's TCG buffer this session) instead of plain `mmap(..., MAP_JIT)`, which is confirmed insufficient there even under a real, externally-granted `CS_DEBUGGED`.

**Files:**
- Modify: `deps/FEX/FEXCore/include/FEXCore/Utils/AllocatorHooks.h:137-153` (the two `VirtualAlloc()` overloads)
- Modify: `deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp` (the guard-page/`TempCodeBuffer` logic already gated `#ifdef __APPLE__` around lines 614-955 — check whether it needs an iOS-device-specific branch once `VirtualAlloc` no longer returns a plain `MAP_JIT` pointer there)

**Acceptance Criteria:**
- [ ] `VirtualAlloc(size_t, bool, bool)` and `VirtualAlloc(void*, size_t, bool, bool)` route through `jit26_prepare_region`/`jit26_writable_alias` when `Execute` is true and the target is real iOS device (`TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR`), returning the writable-alias pointer as the function's result (mirroring exactly what `src/common/utils/ios_device_jit_mmap_shim.cpp` already does for Unicorn: "this file only needs to perform the allocation and hand back the RW (alias) pointer as mmap's result").
- [ ] Every address that becomes a jump target or gets embedded/executed goes through the corresponding RX conversion, not the RW alias — reusing whatever `splitwx`-style RX/RW bookkeeping approach this requires (the exact mechanism Unicorn needed was a `tcg_splitwx_to_rx`/`_to_rw` pair of conversion functions plus a per-buffer diff constant; FEXCore's own internal buffer-tracking structures need the equivalent, wherever it tracks "the pointer generated code lives at" separately from "the pointer used to write it").
- [ ] Existing macOS/`MAP_JIT` desktop behavior is completely unchanged (this is additive, gated to real iOS device only).
- [ ] Desktop `windows-emulator-test` (which exercises this exact `AllocatorHooks.h` on macOS/Apple Silicon) still passes in full.

**Verify:** `cmake --build --preset=release --target windows-emulator-test && ctest --test-dir build/release` still passes in full (confirms the macOS path is a genuine no-op change); real-device correctness is confirmed only via Task 8's handoff, per this phase's explicit testing approach.

**Steps:**

- [ ] **Step 1: Read both files in full before changing anything**

Read `deps/FEX/FEXCore/include/FEXCore/Utils/AllocatorHooks.h` in full (already partially quoted in this plan's research — re-read the live file, since Task 6 may have shifted line numbers) and `deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp`'s `#ifdef __APPLE__` sections around lines 614, 739, 890, 927-952 (the guard-page/`TempCodeBuffer` logic, whose comment already explains real device nuances: "Apple Silicon's `MAP_JIT` memory cannot have its protection changed by an ordinary `mprotect()` after the fact"). Confirm which of these call sites actually need to change versus which only touch buffers that don't need to be executable (and so can keep using plain `mmap` unmodified).

- [ ] **Step 2: Add the RX/RW conversion mechanism**

Following the exact precedent in `deps/unicorn`'s own fork (a `sogen_tcg_splitwx_diff`-style global plus `_to_rx()`/`_to_rw()` conversion functions, added to `qemu/include/tcg/tcg.h` and used everywhere a pointer crosses between "the address written through" and "the address executed from" or embedded as a jump target), add the equivalent to FEXCore: a diff constant and conversion helpers, scoped to the real-iOS-device build only, set at allocation time from `jit26_prepare_region`'s return value (the RX address) and `jit26_writable_alias`'s return value (the RW address) exactly as `ios_device_jit_mmap_shim.cpp` already computes it for Unicorn.

- [ ] **Step 3: Route the two `VirtualAlloc()` overloads through JIT26 on real device**

```c
inline void* VirtualAlloc(size_t Size, bool Execute = false, bool Commit = true) {
#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
  if (Execute) {
    void* const rx = jit26_prepare_region(nullptr, Size);
    if (rx == nullptr) {
      return nullptr;
    }
    int kern_return = 0;
    unsigned int cur_prot = 0, max_prot = 0;
    void* const rw = jit26_writable_alias(rx, Size, &kern_return, &cur_prot, &max_prot);
    sogen_fexcore_splitwx_record(rx, rw); // the Step 2 bookkeeping helper
    return rw ? rw : rx;
  }
#elif defined(__APPLE__)
  return FEXCore::Allocator::mmap(nullptr, Size, PROT_READ | PROT_WRITE | (Execute ? PROT_EXEC : 0),
                                  MAP_PRIVATE | MAP_ANONYMOUS | MapJitFlagIfExecutable(Execute), -1, 0);
#else
  return FEXCore::Allocator::mmap(nullptr, Size, PROT_READ | PROT_WRITE | (Execute ? PROT_EXEC : 0), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
}
```
Apply the equivalent change to the `VirtualAlloc(void* Base, ...)` overload — note `jit26_prepare_region`'s `address` hint parameter mirrors `Base` directly (same signature shape Unicorn's shim already relies on: `jit26_prepare_region(nullptr, length)`).

- [ ] **Step 4: Fix up JIT.cpp's consumers of the allocated pointer**

Wherever `JIT.cpp` stores "the code buffer pointer" for later use as a jump target, embedded call target, or `flush_icache_range`/cache-maintenance argument, convert it to the RX address at that point (via the Step 2 helper) — mirroring exactly how `deps/unicorn`'s `tcg_gen_code()`/`tb_target_set_jmp_target()` fixes worked this session (write through RW, execute/jump/patch through RX). Wherever it only *writes* generated code, keep using the RW pointer this `VirtualAlloc` now returns.

- [ ] **Step 5: Verify macOS is unaffected, real device awaits Task 8's handoff**

```sh
cmake --build --preset=release --target windows-emulator-test
ctest --test-dir build/release
```
Expected: full pass, unchanged from before this task. This task's real-device correctness cannot be verified here — that's Task 8.

- [ ] **Step 6: Commit (inside deps/FEX, then bump the pin)**

```bash
cd deps/FEX
git add FEXCore/include/FEXCore/Utils/AllocatorHooks.h FEXCore/Source/Interface/Core/JIT/JIT.cpp
git commit -m "feat(ios): route FEXCore's JIT allocator through JIT26 on real device"
git push origin HEAD:macos-arm64   # deps/FEX's actual fork branch, NOT "dev"
cd ../..
git add deps/FEX
git commit -m "chore(deps): bump FEX pin — JIT26-blessed allocator for real iOS device"
```

---

## Task 8: Build the real-device archive and hand off

**USER-ORDERED GATE — NON-SKIPPABLE.** This task was requested by the user in the current conversation ("I will demo on a real iphone for you"). Its acceptance criteria are about producing a correct, verifiable build for the user's own demo — it MUST NOT be closed by an agent declaring real-device success itself; that confirmation belongs to the user.

**Goal:** A `SogenIOS-unsigned.ipa` containing the FEX-enabled, JIT26-wired build is packaged and delivered to the user for their own real-device demo, following the exact standard verification/delivery process established throughout Phase 1.

**Files:** None (packaging/verification only).

**Acceptance Criteria:**
- [ ] Desktop `release` build clean, `windows-emulator-test` passes in full.
- [ ] `ios-embed-simulator` preset compiles clean.
- [ ] `ios-embed-device` preset compiles clean, and the Xcode archive (`xcodebuild archive`) succeeds.
- [ ] `otool -hv` on the archived `SogenIOS` binary confirms `NOUNDEFS`.
- [ ] The FEX backend toggle (Task 4) is set to FEX for this build (not left on Unicorn by accident).
- [ ] The packaged `.ipa` is copied to the user's iCloud Drive path (`/Users/jack/Library/Mobile Documents/com~apple~CloudDocs/SogenIOS-unsigned.ipa`), matching every prior Phase 1 handoff.

**Verify:** Standard Phase 1 verification chain: `cmake --build --preset=release --target windows-emulator-test && ctest --test-dir build/release`, `cmake --build --preset=ios-embed-simulator`, `cmake --build --preset=ios-embed-device`, `xcodebuild archive ... && otool -hv .../SogenIOS`.

**Steps:**

- [ ] **Step 1: Run the full standard verification chain**

```sh
cmake --build --preset=release --target windows-emulator-test
ctest --test-dir build/release
cmake --build --preset=ios-embed-simulator
cmake --build --preset=ios-embed-device
```
All must succeed.

- [ ] **Step 2: Archive and verify NOUNDEFS**

Build the Xcode archive exactly as every prior Phase 1 round did (via `xcodebuild archive`, `CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO` per this session's standing authorization for unsigned builds), then:
```sh
otool -hv <path-to-archived-SogenIOS-binary> | grep NOUNDEFS
```
Expected: `NOUNDEFS` present.

- [ ] **Step 3: Package and deliver**

```sh
# Build the unsigned .ipa the same way every prior Phase 1 round did, then:
cp SogenIOS-unsigned.ipa "/Users/jack/Library/Mobile Documents/com~apple~CloudDocs/SogenIOS-unsigned.ipa"
```

- [ ] **Step 4: Report to the user**

State plainly that this build has the FEX backend toggle enabled, has passed every automated check available (desktop tests, Simulator/device compile, archive, `NOUNDEFS`), and that the guest sample's actual behavior on real hardware needs the user's own demo — do not claim real-device success has been verified by this task.

- [ ] **Step 5: Commit**

```bash
git add <any files touched, if applicable>
git commit -m "chore(ios): package FEX-enabled build for real-device demo"
```
