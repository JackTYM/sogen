# iOS App Shell, Phase 2 (FEX backend): Design

## Context

Phase 1 (`docs/superpowers/plans/2026-09-04-ios-app-shell-phase1.md`) got sogen's Unicorn backend
running end-to-end on real iOS device hardware: `native-gpu-clear-sample.exe` boots, executes real
translated x86-64 code via Unicorn's TCG JIT (after a real, deep bug hunt in `deps/unicorn`'s AArch64
backend — two RX/RW pointer-confusion bugs in `tcg_out_movi`'s `ADR`/`ADRP` fast path and
`code_gen_epilogue`, both requiring the same JIT26 breakpoint-protocol dance already built for the app
shell), and renders all 8 clear-color frames correctly via MoltenVK to the on-screen `CALayer`, ending
on violet, confirmed on the user's own device.

Unicorn is a pure interpreter-style TCG JIT: correct, but roughly two orders of magnitude slower than a
native-code JIT (`docs/fex-backend.md` measures ~129x on a compute-bound workload on desktop). Anything
beyond a trivial proof sample needs FEX's real JIT. `docs/fex-backend.md` already anticipates this
exact follow-on (`## iOS cross-compilation` → `### Follow-on work`), written during the cross-compilation
plan before Phase 1 existed: FEXCore's own allocator needs the same breakpoint-driven memory-blessing
protocol Unicorn just proved out, and the CMake gate that currently excludes iOS entirely
(`CMAKE_SYSTEM_NAME` is `"iOS"` for both device and Simulator toolchains, matching neither of the gate's
`Linux`/`Darwin` branches) needs widening.

**User's stated roadmap for this app** (in order): (1) FEX backend, (2) LocalDevVPN as an alternative to
the current NetworkExtension-requiring `TunnelExtension`, (3) input/UX polish, (4) a real UI (profiles,
root management, hidden-by-default logs). This spec covers **only item (1)**.

## Goal

Get FEX's JIT executing the same Phase 1 proof target (`native-gpu-clear-sample.exe`) on iOS — first in
the Simulator (no JIT26 involvement needed there, since Simulator has no TXM/SPTM hardware enforcement),
then on real device via a JIT26-blessed FEXCore allocator. This is a proof phase, matching Phase 1's own
scope discipline: no backend-picker UI, no LocalDevVPN, no input/UX work — those are later phases.

**Explicitly out of scope**: MW2 or any other substantial guest; a user-facing way to choose Unicorn vs.
FEX (a compile-time/env toggle is sufficient here); anything to do with `TunnelExtension`/NetworkExtension
distribution; UI polish.

## Why Unicorn-first was the right call, and why FEX is next

Phase 1's own design doc chose Unicorn first specifically to defer "the JIT-specific architecture" to a
later phase, while using the exact same target sample so success is directly comparable. That later phase
is now this one. Everything Phase 1 already proved (MoltenVK rendering, the DXGK/`vulkan_host` pipeline,
the JIT26 breakpoint-protocol mechanism and its ExtensionKit/`JITHelper`/`TunnelExtension` architecture)
is reused unchanged — this phase is scoped narrowly to "get FEX's JIT itself blessed and executing,"
not to re-derive anything Phase 1 already solved.

## Architecture

### 1. Build system: three concrete, already-identified gaps

**a. Top-level gate** (`CMakeLists.txt:67-69`). Currently:
```cmake
if((CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64") AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/FEX/CMakeLists.txt"
   AND ((CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID) OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
```
Both `cmake/toolchain/ios.cmake` and `cmake/toolchain/ios-simulator.cmake` set `CMAKE_SYSTEM_NAME` to
`"iOS"` (and `CMAKE_SYSTEM_PROCESSOR` to `"arm64"` explicitly, since CMake leaves it empty for iOS
otherwise) — the condition's last clause needs an `OR CMAKE_SYSTEM_NAME STREQUAL "iOS"` (or equivalent)
addition.

**b. FEXCore `ExternalProject`'s Darwin-family check** (`deps/CMakeLists.txt:147-153`). Currently:
```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(_FEXCORE_SHARED_LIB "libFEXCore.dylib")
  set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
else()
  set(_FEXCORE_SHARED_LIB "libFEXCore.so")
  ...
```
Needs to also treat `"iOS"` as Darwin-family (still produces a `.dylib`).

**c. No cross-compilation forwarding into the `ExternalProject` at all.** `ExternalProject_Add`'s
`CMAKE_ARGS` today only forwards the compiler paths, `CMAKE_OSX_DEPLOYMENT_TARGET`, and a couple of
FEX-specific flags — it spawns a genuinely separate nested `cmake` configure/build, which does **not**
inherit the outer `CMAKE_TOOLCHAIN_FILE`/`CMAKE_SYSTEM_NAME`/`CMAKE_OSX_SYSROOT`/`CMAKE_OSX_ARCHITECTURES`
automatically. For `ios-embed-device`/`ios-embed-simulator`, this needs explicit forwarding of the active
toolchain file (`cmake/toolchain/ios.cmake` or `ios-simulator.cmake`) and its sysroot/arch/deployment
target, or FEXCore would silently configure itself for the host Mac instead of iOS.

### 2. Simulator proof (no JIT26 involved)

The Simulator has no TXM/SPTM hardware enforcement, so FEX's existing `MAP_JIT` allocator path (already
proven working on desktop macOS/Apple Silicon per `docs/fex-backend.md`) should work there unmodified —
this stage is really "does FEXCore cross-compile and link for iOS at all, and does its existing macOS
logic carry over," not a new JIT-blessing problem.

`tools/sogen-ios/Sources/Bridge/SogenBridge.mm:168` currently hardcodes:
```objc
auto emu = sogen::create_x86_64_emulator(sogen::backend_type::unicorn, 1);
```
This phase adds a way to select `sogen::backend_type::fex` instead (a compile-time flag or environment
variable read at the same call site is sufficient — no UI picker, per scope).

**Success criteria** (mirrors Phase 1's Task 7 exactly, on Simulator): all 8 `native-gpu-clear-sample`
frames render in the correct color sequence (blue → green → red → yellow → cyan → magenta → orange →
violet), the view holds on violet, `[ngcs] done` appears in the log, and a log line confirms the FEX
backend was actually the one running (not a silent fallback to Unicorn).

### 3. Real-device JIT26 integration (FEXCore allocator)

`docs/fex-backend.md`'s existing "Follow-on work" section already named the exact files:
`deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp` and
`deps/FEX/FEXCore/include/FEXCore/Utils/AllocatorHooks.h` need the same breakpoint-driven
memory-blessing protocol already proven for Unicorn's TCG buffer this session (`brk #0xf00d` with
`x16=1` to prepare a region / `x16=0` to detach, plus a `mach_vm_remap`+`mprotect` writable-alias
technique) in place of a plain `mmap(MAP_JIT)`, which is confirmed insufficient on real iOS 26+ hardware
even under an externally-granted `CS_DEBUGGED`.

Per the approach decision made during brainstorming: rather than link `libFEXCore.dylib` against the
app target's existing `JIT26.c` (a real cross-library-linking problem, since FEXCore builds as an
independent `ExternalProject`-driven shared library, not part of the app's own link step) or rely on
`-undefined dynamic_lookup`-style flat-namespace symbol resolution, this phase adds a **self-contained
copy** of the `jit26_prepare_region`/`jit26_writable_alias` client primitives directly into the
`deps/FEX` fork (`github.com/JackTYM/FEX`, already a heavily Darwin-patched personal fork — this is one
more patch in the same vein as the existing `MAP_JIT` guard-page fix). The allocator/`JIT.cpp` changes
follow the same real pattern already proven for Unicorn: request via `jit26_prepare_region`, get a
writable alias via `jit26_writable_alias`, write generated code through the alias, execute via the
original RX pointer.

This section's correctness can only be verified on real device — it cannot be exercised in the
Simulator at all, since the Simulator never reaches this code path (plain `mmap(MAP_JIT)` already
works there).

### 4. Testing & handoff

Per explicit user instruction: **all development and iteration happens in the Simulator first.** The
implementer gets FEX building, linking, and rendering all 8 frames correctly in the Simulator before
touching the JIT26/FEXCore-allocator work at all. Once the JIT26 allocator changes are written and the
device archive builds and packages cleanly (standard verification: desktop `release` + full test suite,
`ios-embed-simulator` compile, `ios-embed-device` archive + `NOUNDEFS`), the build is handed directly to
the user for the real-device demo — unlike Phase 1's Task 7/8, this phase does not include an agent
performing live on-device debugging rounds. If the real-device demo surfaces a bug, that becomes its own
follow-up round (mirroring exactly how Phase 1's Task 7 crash led into Task 8), not a re-opening of this
spec's scope.

## Testing

- **Simulator**: `native-gpu-clear-sample.exe` run via the FEX backend selector renders all 8 colors
  correctly ending on violet, `[ngcs] done`, with a log line confirming FEX (not Unicorn) executed.
- **Desktop**: `windows-emulator-test` continues to pass in full (the FEX backend is exercised there
  today on macOS/Apple Silicon; this phase's CMake/allocator changes must not regress it).
- **Real device**: performed by the user directly, once the Simulator proof and the JIT26 allocator
  work are both in place.

## Explicitly deferred (later phases, per the user's roadmap)

- Any backend-picker UI (Unicorn vs. FEX) beyond a compile-time/env toggle.
- LocalDevVPN support / any change to how `TunnelExtension`'s NetworkExtension requirement is handled.
- Input/UX polish, profiles, root management, or hiding logs by default.
- Running anything beyond `native-gpu-clear-sample.exe` (in particular, MW2 or any other substantial
  guest) via the FEX backend on iOS.
