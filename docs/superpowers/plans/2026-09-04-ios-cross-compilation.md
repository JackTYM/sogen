# iOS Cross-Compilation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Get sogen's `analyzer` target (the smallest meaningful CLI entry point, matching the existing `emscripten32`/`emscripten64` preset precedent) to configure and compile cleanly against the iOS SDK via CMake, producing a real arm64-ios Mach-O binary — proving the foundational, currently-unproven risk before any FEXCore JIT26 allocator work or iOS app-shell work is attempted.

**Architecture:** Complete and wire up the existing (but stub, 3-line, never-finished) `cmake/toolchain/ios.cmake`, add an `ios` configure/build preset pair to `CMakePresets.json` mirroring the `emscripten32`/`emscripten64` pattern already in the file, then iteratively resolve whatever real configure- and build-time errors CMake and Clang surface — there is no way to pre-enumerate these; the two discovery tasks below give the concrete methodology for finding and fixing them rather than guessing. FEXCore's own iOS-specific JIT allocator work and the three-target iOS app shell (host + `JITHelper` ExtensionKit extension + `TunnelExtension`) are explicitly out of scope for this plan and become their own follow-on plans once this one is done. That architecture was validated by a throwaway Swift JIT spike that lives **outside this repository** (it is not under `tools/` and is not part of sogen's CMake build) and is written up in a published findings artifact — do not go looking for it in-tree.

**Tech Stack:** CMake 3.26+ / Ninja generator, Apple Clang via Xcode's iOS SDK, the existing `deps/FEX` submodule (JackTYM/FEX fork, currently gated OFF for iOS by a `CMAKE_SYSTEM_NAME STREQUAL "Darwin"` check that does not match `"iOS"` — deliberately left OFF in this plan, see Task 1).

**User decisions (already made):**
- Scope this plan to cross-compilation only; the FEXCore JIT26 allocator change and the iOS app shell are separate, later plans (user chose "prove iOS cross-compilation first" explicitly over a combined plan).
- Target the `analyzer` CLI binary specifically, not the whole project — matches the existing emscripten preset's own restriction to `analyzer`/`linux-analyzer` and avoids fighting GUI/fuzzer/test targets that were never meant to cross-compile.
- FEX stays OFF for this plan (the existing Darwin-only gate is not touched here) — enabling it for iOS is real, separate work belonging to the JIT26 allocator plan, not this one.

---

## Task 1: Complete the iOS toolchain file and wire an `ios` CMake preset

**Goal:** `cmake/toolchain/ios.cmake` sets everything a Ninja-generator iOS device cross-compile needs (the current 3-line stub only sets system name, architecture, and deployment target — no SDK sysroot, which Ninja does not auto-derive the way the Xcode generator does), and `CMakePresets.json` gains an `ios` configure preset that uses it, following the exact structure of the existing `emscripten-base`/`emscripten32` presets.

**Context:** `cmake/toolchain/ios.cmake` was added in a single commit (`db0d3428`, upstream author momo5502, March 2025) alongside a CI matrix entry that was later removed from `.github/workflows/build.yml` entirely — there is no evidence it ever got past a bare `cmake` configure, and no preset has ever referenced it. `CMakeLists.txt:34` already special-cases `CMAKE_SYSTEM_NAME STREQUAL "iOS"` (skipping the `CMAKE_OSX_DEPLOYMENT_TARGET` line since the toolchain file sets its own), and `cmake/utils.cmake:353` already excludes iOS from a compiler-flag block used for host tooling — both signs that iOS was anticipated but never finished.

**Files:**
- Modify: `cmake/toolchain/ios.cmake`
- Modify: `CMakePresets.json`

**Acceptance Criteria:**
- [ ] `cmake/toolchain/ios.cmake` sets `CMAKE_OSX_SYSROOT` to the iPhoneOS SDK, a non-empty `CMAKE_SYSTEM_PROCESSOR`, and `CMAKE_MACOSX_BUNDLE OFF`.
- [ ] `CMakePresets.json` has an `ios` configure preset that inherits `ninja`/`build`, sets `CMAKE_TOOLCHAIN_FILE` to the iOS toolchain file, and sets `SOGEN_ENABLE_RUST_CODE=OFF` (matching the `emscripten-base` preset's own reasoning — Rust's iOS cross-compilation is untested and is separate scope from proving the C++ core builds).
- [ ] The `ios` configure preset sets `SOGEN_USE_SYSTEM_SDL3=OFF`. `deps/CMakeLists.txt:59` calls `find_package(SDL3 CONFIG QUIET)` when that option is on (its default), and this machine has a Homebrew SDL3 at `/opt/homebrew/lib/cmake/SDL3` — a **macOS** arm64 build. CMake's default find behaviour would happily resolve it in an iOS configure and only fail much later at link time with a confusing "building for iOS but linking macOS dylib" error. Forcing the vendored `deps/SDL` submodule instead avoids a wholly avoidable false trail.
- [ ] `CMakePresets.json` has an `ios` build preset targeting only `analyzer` (matching `emscripten32`/`emscripten64`'s `targets: ["analyzer", "linux-analyzer"]` pattern — start with just `analyzer` since `linux-analyzer` pulls in Linux-guest-specific code that has never been checked for iOS portability either).
- [ ] `cmake --workflow --preset=ios --fresh` is a valid CMake invocation (does not error on preset structure itself — actual configure success is Task 2).

**Verify:** `cmake --list-presets=all` includes `ios` under all three of "Available configure presets", "Available build presets", and "Available workflow presets". (Note: bare `cmake --list-presets` lists *only* configure presets — the `=all` form is required here.)

**Steps:**

- [ ] **Step 1: Find the iPhoneOS SDK path and confirm it resolves**

```bash
xcrun --sdk iphoneos --show-sdk-path
```

Expected: a path like `/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS<version>.sdk` (verified on this machine as of plan-writing: `.../iPhoneOS26.5.sdk`, with CMake 4.4.0 and AppleClang 21.0.0). If this errors, Xcode's command-line tools are not pointed at a full Xcode install (`xcode-select -p` should show `/Applications/Xcode.app/Contents/Developer`, not `CommandLineTools` — the iPhoneOS SDK only ships with the full Xcode app) — fix that first, it blocks everything else in this plan.

- [ ] **Step 2: Complete the toolchain file**

Replace the full contents of `cmake/toolchain/ios.cmake` with:

```cmake
set(CMAKE_SYSTEM_NAME "iOS")
# CMake leaves CMAKE_SYSTEM_PROCESSOR empty for an iOS CMAKE_SYSTEM_NAME; several
# CMakeLists here and in deps/ branch on it and would take the wrong arch path.
set(CMAKE_SYSTEM_PROCESSOR "arm64")
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
set(CMAKE_OSX_SYSROOT "iphoneos")
# CMake defaults executables to .app bundles on iOS; this plan wants a plain
# Mach-O to inspect, and bundling belongs to the later app-shell plan.
set(CMAKE_MACOSX_BUNDLE OFF)
```

Notes on what is deliberately *not* in this file, both empirically checked against this exact toolchain content with CMake 4.4.0 + Ninja before this plan was written:

- **No `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY`.** A common iOS-toolchain reflex, but unnecessary here — the default `EXECUTABLE` try-compile links fine against the iPhoneOS sysroot (CMake never *runs* try_compile output, so nothing needs to execute on-device), and forcing `STATIC_LIBRARY` would turn every link-based dependency probe (`check_symbol_exists`, `check_function_exists`, `check_library_exists`) into a compile-only check, silently producing false positives across `deps/`. If a specific dependency's try_compile genuinely fails, that is a Task 2 finding to fix at that dependency — not something to pre-empt globally here.
- **No `CMAKE_SYSTEM_VERSION`.** `CMAKE_OSX_DEPLOYMENT_TARGET` already drives the `-target arm64-apple-ios14.0` flag and the resulting `LC_BUILD_VERSION minos`.

One consequence of setting `CMAKE_SYSTEM_PROCESSOR` worth knowing: it satisfies the *first* clause of the FEX gate at `CMakeLists.txt:67-69`. The gate's `(Linux AND NOT ANDROID) OR Darwin` clause at line 69 is then the sole thing keeping `SOGEN_ENABLE_FEX` OFF for iOS — which is the intent (see "User decisions"), but it means an implementer who later relaxes line 69 gets FEX turned on immediately, with no second safety net. Do not touch line 69 in this plan.

Sanity check that this toolchain alone produces the right thing (a scratch project, not sogen — ~2 seconds, and it isolates a toolchain bug from a sogen bug before Task 2 muddies the water):

```bash
D=$(mktemp -d) && printf 'cmake_minimum_required(VERSION 3.26)\nproject(t C)\nadd_executable(x main.c)\n' > "$D/CMakeLists.txt" && printf 'int main(void){return 0;}\n' > "$D/main.c" && cmake -G Ninja -S "$D" -B "$D/b" -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchain/ios.cmake" >/dev/null && cmake --build "$D/b" >/dev/null && file "$D/b/x"
```

Expected: `Mach-O 64-bit executable arm64` — at path `b/x`, **not** `b/x.app/x` (that would mean `CMAKE_MACOSX_BUNDLE OFF` did not take).

- [ ] **Step 3: Add the `ios` configure preset**

In `CMakePresets.json`, inside `configurePresets`, add this entry after the `asan` preset (before `emscripten-base`):

```json
        {
            "name": "ios",
            "inherits": [
                "ninja",
                "build"
            ],
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "RelWithDebInfo",
                "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/toolchain/ios.cmake",
                "SOGEN_ENABLE_RUST_CODE": "OFF",
                "SOGEN_USE_SYSTEM_SDL3": "OFF"
            }
        },
```

(Placement note: `configurePresets` is a flat ordered array — `build`, `ninja`, `release`, `debug`, `tidy`, `asan`, `emscripten-base`, `emscripten32`, `emscripten64`, `vs2022`, `vs2026`. Insert between the `asan` object's closing `},` and the `emscripten-base` object's opening `{`. Order carries no semantics to CMake; this is purely for readability.)

- [ ] **Step 4: Add the `ios` build preset**

In `CMakePresets.json`, inside `buildPresets`, add this entry after `emscripten64` — which is the **last** element of the `buildPresets` array, so the existing `emscripten64` object needs a `,` appended after its closing brace and this new entry must *not* have a trailing comma:

```json
        {
            "name": "ios",
            "configurePreset": "ios",
            "targets": [
                "analyzer"
            ]
        }
```

- [ ] **Step 5: Add the `ios` workflow preset**

In `CMakePresets.json`, inside `workflowPresets`, add this entry after `emscripten64` — again the last array element, so append a `,` after its closing brace and leave this new entry without one:

```json
        {
            "name": "ios",
            "steps": [
                {
                    "type": "configure",
                    "name": "ios"
                },
                {
                    "type": "build",
                    "name": "ios"
                }
            ]
        }
```

- [ ] **Step 6: Validate the JSON and preset structure**

```bash
python3 -c "import json; json.load(open('CMakePresets.json'))" && echo "valid JSON"
cmake --list-presets=all 2>&1 | grep -c '"ios"'
```

Expected: `valid JSON`, and a count of `3` — `ios` listed once each under configure, build, and workflow presets, with no CMake parse error above it. A count of `0` with a CMake error means the JSON is valid but the preset graph is not (e.g. a bad `inherits` name); a count of `1` or `2` means one of Steps 3-5 landed in the wrong array.

- [ ] **Step 7: Commit**

```bash
git add cmake/toolchain/ios.cmake CMakePresets.json
git commit -m "build(ios): complete the iOS toolchain file and add an ios CMake preset"
```

---

## Task 2: Get `cmake --preset=ios` to configure cleanly

**Goal:** `cmake --preset=ios` exits 0 with no errors. This is a real discovery task, not a scripted one — every subdirectory listed in `src/CMakeLists.txt` gets processed at configure time regardless of which target the build preset later restricts to (CMake's `add_subdirectory` always parses the child `CMakeLists.txt`), so the first real blockers will most likely come from `find_package`/`find_library` calls for host-only tooling (SDL3's iOS backend support, capstone, zstd, or similar) that assume a desktop target. Do not guess fixes in advance — run it, read the exact CMake error, and fix that one thing.

**Context:** This project already has a working precedent for handling a dependency that legitimately cannot cross-compile: `deps/vkd3d`'s `vkd3d-shader-bridge` target is documented in `CLAUDE.md` as "host-only... excluded from the default build until [something] links against [it]" — i.e., gated off via a CMake option or `if()` rather than deleted. Follow that same pattern here: when a subdirectory's configure step fails because it fundamentally can't target iOS (not because of a fixable typo or missing flag), gate its `add_subdirectory` call behind `AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS"` in `src/CMakeLists.txt` rather than deleting or hand-patching the dependency itself, and note which target got gated in this task's own notes so the later iOS-app-shell plan knows what's missing.

**Files:**
- Modify: `src/CMakeLists.txt` (only if a subdirectory needs iOS-gating — see Context)
- Modify: whichever specific `CMakeLists.txt` files the actual errors point to (unknown until Step 1 runs; do not pre-guess which)

**Acceptance Criteria:**
- [ ] `cmake --preset=ios 2>&1 | tail -5` ends with CMake's normal "Build files have been written to..." success message, not an error.
- [ ] Every `add_subdirectory` call skipped for iOS (if any) is documented with a one-line comment in `src/CMakeLists.txt` explaining why (e.g. "SDL3's iOS backend needs its own UIKit shell — belongs to the app-shell plan, not here").
- [ ] The non-iOS presets (`release`, `debug`) still configure successfully afterward — nothing in this task's changes is iOS-only in a way that silently breaks other platforms. Run `cmake --preset=release 2>&1 | tail -5` and confirm the same success message.

**Verify:** `rm -rf build/ios && cmake --preset=ios` → exits 0, ends with "Build files have been written to: <path>/build/ios".

**Steps:**

- [ ] **Step 1: Run it and capture the first real error**

```bash
rm -rf build/ios
cmake --preset=ios 2>&1 | tee /tmp/ios-configure.log | tail -40
```

Read the actual error CMake prints — it names the exact `CMakeLists.txt` line and the exact `find_package`/`find_library`/compiler check that failed. This is the concrete input to Step 2; there is nothing to fix until this has actually been run once.

- [ ] **Step 2: Triage the error**

For whatever the real error is, decide between two fixes, in this order of preference:
1. **A real, targeted fix** if the failure is something like a missing `-D` flag, a wrong assumption about a host tool being available, or a compiler check that needs an iOS-specific branch. Make the smallest change that addresses the actual error.
2. **An iOS-gate** in `src/CMakeLists.txt` (per Context above) only if the dependency is fundamentally host/desktop-only (e.g. requires linking against AppKit, or a build tool that has no iOS equivalent at all).

Do not reach for the blunt instruments — do not force `SOGEN_BUILD_TOOLS=OFF`/`SOGEN_BUILD_STATIC=ON` (the pair that turns off `_SOGEN_BUILD_STANDALONE_TARGETS` at `src/CMakeLists.txt:28-34`, which would also drop `analyzer` itself, this plan's entire deliverable), and do not delete subdirectories. Gate individual `add_subdirectory` calls only when Step 1's actual error demonstrates that specific one cannot configure for iOS.

- [ ] **Step 3: Re-run and repeat Steps 1-2 until clean**

```bash
cmake --preset=ios 2>&1 | tail -40
```

Repeat until this ends in the "Build files have been written to" success line. Each iteration is one real, observed error — not a pre-written list, since the exact sequence is unknown until run.

- [ ] **Step 4: Confirm the other presets still work**

```bash
cmake --preset=release 2>&1 | tail -5
```

Expected: same "Build files have been written to" success message as before this task's changes. If any iOS-gating in `src/CMakeLists.txt` used a bare condition instead of one scoped to `CMAKE_SYSTEM_NAME STREQUAL "iOS"`, this is where that mistake would surface.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "build(ios): resolve cmake configure errors for the ios preset"
```

(If Step 2 never required any file changes beyond Task 1's preset — i.e. configure was already clean — state that explicitly in the commit message body and skip this commit; do not create an empty commit.)

---

## Task 3: Get `analyzer` to build for iOS

**Goal:** `cmake --build --preset=ios` exits 0 and produces an `analyzer` Mach-O binary. Same discovery methodology as Task 2, one layer deeper — configure succeeding means every `CMakeLists.txt` parsed, not that every source file actually compiles for `arm64-apple-ios`. Expect real errors from code that assumes macOS-only APIs (anything gated on `__APPLE__` without also checking `TARGET_OS_IPHONE`/`TARGET_OS_IOS` from `<TargetConditionals.h>` is suspect) or Unix APIs iOS's sandboxed libc restricts.

**Files:**
- Modify: whichever `.cpp`/`.hpp` files the actual compiler/linker errors point to (unknown until Step 1 runs)

**Acceptance Criteria:**
- [ ] `cmake --build --preset=ios 2>&1 | tail -20` ends with a successful link of `analyzer`, no errors.
- [ ] `file build/ios/artifacts/analyzer` reports `Mach-O 64-bit executable arm64`. That path is deterministic, not a guess: `sogen_set_new_artifact_directory()` (`cmake/utils.cmake:311-321`) puts single-config-generator output in `${CMAKE_BINARY_DIR}/artifacts`, the `ios` configure preset inherits `build`'s `binaryDir` of `${sourceDir}/build/${presetName}`, and Task 1's `CMAKE_MACOSX_BUNDLE OFF` keeps it a bare file rather than `analyzer.app/analyzer`.
- [ ] Any `#if`/`#ifdef` added to route around an iOS-incompatible code path checks `TARGET_OS_IOS` (from `<TargetConditionals.h>`, already the correct convention for this exact situation), not a bare `__APPLE__` — a bare `__APPLE__` check would also match the existing macOS build and silently change its behavior, which is explicitly out of scope for this plan.
- [ ] `cmake --build --preset=release 2>&1 | tail -10` (or your existing local release build) still succeeds afterward — confirms nothing broke macOS.

**Verify:** `cmake --build --preset=ios 2>&1 | tail -5 && file build/ios/artifacts/analyzer` → build succeeds, `file` reports `Mach-O 64-bit executable arm64`.

**Steps:**

- [ ] **Step 1: Build and capture the first real error**

```bash
cmake --build --preset=ios 2>&1 | tee /tmp/ios-build.log | tail -60
```

- [ ] **Step 2: Fix the actual error shown**

Same rule as Task 2 Step 2: fix the specific thing the compiler or linker actually reported. Common categories to expect (do not pre-fix these speculatively — only act once Step 1 actually shows one):
- A source file using a macOS-only header or API with no `TARGET_OS_IOS` guard.
- A linker error for a library that has no iOS build (would mean Task 2's iOS-gating missed a dependency that only fails at link time, not configure time — go back and gate it in `src/CMakeLists.txt` the same way).

- [ ] **Step 3: Re-run and repeat until clean**

```bash
cmake --build --preset=ios 2>&1 | tail -60
```

- [ ] **Step 4: Confirm the binary is real**

```bash
file build/ios/artifacts/analyzer
otool -l build/ios/artifacts/analyzer | grep -A5 LC_BUILD_VERSION
```

Expected: `Mach-O 64-bit executable arm64`, and an `LC_BUILD_VERSION` load command reading `platform 2` with `minos 14.0`. Note `otool` prints the platform **numerically** — `2` is `PLATFORM_IOS`; a `1` there would mean `PLATFORM_MACOS`, i.e. the toolchain file never took effect and this is a host binary wearing the right filename.

- [ ] **Step 5: Confirm macOS still builds**

```bash
cmake --build --preset=release 2>&1 | tail -10
```

Expected: unchanged success — this is the same check as Task 2 Step 4, run again because Task 3 touched actual source files, which carries more risk of an accidental cross-platform regression than Task 2's CMake-only changes did.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "build(ios): get the analyzer target compiling for arm64-apple-ios"
```

---

## Task 4: Document the result and hand off to the next plan

**Goal:** Anyone picking this up next — including a future session with no memory of this one — can see exactly what was proven, what was gated off and why, and what the two follow-on plans (FEXCore JIT26 allocator, iOS app shell) need to pick up.

**Context:** `analyzer` compiling for iOS is necessary but not sufficient to run anything: stock iOS cannot execute a bare Mach-O binary outside a signed `.app` bundle launched through LaunchServices — there is no interactive shell to `exec` it from the way `./analyzer` works on macOS. Live on-device execution genuinely belongs to the iOS-app-shell plan, not this one; do not attempt to package or run the binary here.

**Files:**
- Modify: `docs/fex-backend.md` (add an iOS section — confirm the exact insertion point by reading the file's current structure first, don't guess a location)

**Acceptance Criteria:**
- [ ] `docs/fex-backend.md` documents: the `ios` preset now exists and builds `analyzer`; FEX itself is still OFF for iOS (the `Darwin`-only gate in `CMakeLists.txt:69` was deliberately left untouched); the exact list of anything gated off in Task 2/3 (or "nothing needed gating" if that turned out to be true).
- [ ] The doc explicitly states the two next plans this unblocks (FEXCore JIT26 allocator changes to `deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp` and `AllocatorHooks.h`; the three-target iOS app shell per the out-of-tree spike's validated architecture) so a future session finds them without re-deriving this session's reasoning.

**Verify:** `grep -A8 "^## iOS" docs/fex-backend.md` shows the new section with real content, not a stub.

**Steps:**

- [ ] **Step 1: Read the current doc structure**

```bash
grep -n "^#" docs/fex-backend.md
```

Pick the insertion point based on the real heading structure this returns — do not assume a location without checking. As of plan-writing the file's top-level sections are `##` (`## Status`, `## What is in place`, `## WoW64 support`, `## Architecture`, `## Build`, `## Selecting the backend`), with `###` subsections under several of them; the natural home is a new `##` section immediately after `## Build`'s subsections and before `## Selecting the backend`. Confirm that still holds before writing.

- [ ] **Step 2: Write the section**

Add it as a `##`-level section headed `## iOS cross-compilation` (matching the file's other top-level sections, and matching this task's Verify grep), covering, as real prose, not a template:
- What `cmake --workflow --preset=ios` now does and where its output lands.
- The exact list of anything gated off `src/CMakeLists.txt` in Task 2/3, each with the one-line reason already written there.
- That FEX is still OFF for iOS by the existing `CMAKE_SYSTEM_NAME STREQUAL "Darwin"` gate at `CMakeLists.txt:69`, deliberately, and that turning it on (plus the JIT26 breakpoint-protocol allocator change) is the next plan's job, not done here.
- A link to the published findings artifact from the out-of-tree Swift JIT spike, for the app-shell architecture context. Paste the real artifact URL; do not reference an in-repo path, since that spike does not live in this repository.

- [ ] **Step 3: Commit**

```bash
git add docs/fex-backend.md
git commit -m "docs(ios): document the ios CMake preset and hand off to the FEX/app-shell plans"
```
