# FEX backend — development notes

The FEX-Emu emulator backend (`sogen::fex`). A standalone backend that mirrors the structure of the
other backends, selected at runtime via `EMULATOR_FEX=1`. Its functional target is **macOS on Apple
Silicon** — [FEX](https://fex-emu.com) only JITs x86/x86-64 to ARM64.

## Status

**Working on macOS/Apple Silicon — the only functional target.** On Darwin the backend builds,
links, and runs: it passes the full regression suite (`test-sample.exe`, `hello.exe`,
`busybox.exe`) matching the Unicorn backend's behavior byte-for-byte, and has been validated
against ~40 pre-existing sample executables (process/thread introspection, synchronization
primitives, sections, security tokens, the registry, pipes, sockets, timers, file/directory I/O,
GUI dialogs, DXGK harnesses, and 32-bit WoW64 binaries) with every remaining discrepancy traced
to a backend-agnostic gap (an unimplemented syscall, or a missing staged system-DLL export) rather
than a difference between this backend and the existing one.

**ARM64 Linux is build-tested only.** The Linux path compiles in CI but has never been run and is
not expected to work: it installs no fault signal handlers, has no MMIO emulation (the
KUSER_SHARED_DATA decode-and-emulate path is `__APPLE__`-gated), and passes FEXCore an empty
default-constructed `HostFeatures` instead of real hardware detection (a `TODO` in
`fex_x86_64_emulator.cpp`). Android is excluded outright by the CMake gate.

**32-bit WoW64 processes are supported** (see "WoW64 support" below): a real 32-bit guest process
runs against a second, lazily-created 32-bit FEXCore Context, with state marshaled across the
bitness-switch trampoline on every crossing.

### Performance

On a pure compute-bound workload (a tight ALU loop with no syscalls beyond timestamping), this
backend measured roughly **129x less wall-clock time** than the Unicorn backend on the same host
(~3.4s vs. ~439s for 2 billion loop iterations) — JIT-compiled native ARM64 code running directly on
the CPU, versus per-block interpretation. A mixed, syscall-heavy smoke-test workload showed a
smaller but still substantial ~27-30x gap. Expect the larger multiplier to dominate for CPU-bound
guest workloads (game logic, physics, scripting) and the smaller one where I/O/syscall dispatch is a
significant fraction of runtime.

### Known limitations

- **`comctl32.dll`-linked apps** (e.g. a bundled calculator sample) fail identically on both this
  backend and Unicorn due to a staged `comctl32.dll` build that doesn't export the ordinals the
  sample binaries were linked against. This is a staged-asset version mismatch, not a code bug, and
  is not fixable without staging a different `comctl32.dll`.
- **A plain guest store to read-only memory aborts the emulator instead of raising an exception.**
  The fault-classification decoder (`decode_arm64_store`) only recognizes the STLR family, so a
  plain (non-atomic) STR to memory that *is* mapped read-only gets misclassified as a read and
  routed through fault-recovery paths that both fail — the result is an unhandled host signal that
  aborts the whole emulator process, not a `STATUS_ACCESS_VIOLATION` delivered to the guest. This
  affects any guest that deliberately writes to read-only memory and catches the resulting
  exception (packers/DRM protections do this routinely). A plain store to *unmapped* memory does
  reach the guest as an exception, but with the read/write flag (`ExceptionInformation[0]`)
  wrongly reporting a read. See `decode_arm64_store`'s comment for why the decode table cannot
  simply be broadened.
- **`wow64cpu.dll`'s turbo-table fast-path is bypassed.** The reverse (32→64) gate crossing does
  execute real `wow64cpu.dll` 64-bit dispatch code: after marshaling state, it resumes at the real
  `TurboDispatchJumpAddressEnd`, whose genuine code path (through `Wow64SystemServiceEx` in
  `wow64.dll`) services every 32-bit syscall. What is *not* executed is only the turbo-thunk
  fast-path at `TurboDispatchJumpAddressStart` (`jmp [r15+rcx*8]` through the per-service jump
  table) — the crossing always forces the generic dispatch continuation instead of selecting a
  specialized turbo thunk.

## What is in place

- Full `x86_64_emulator` interface implementation (registers, memory, hooks, serialize).
- `x86_register` ↔ `FEXCore::Core::CPUState` mapping (`fex_x86_64_common.hpp`).
- A `FEXCore::HLE::SyscallHandler` that routes guest `syscall` instructions to the registered
  syscall instruction-hook (the path the Windows emulation layer uses to service NT syscalls).
- Real `FEXCore::HostFeatures` detection via `sysctlbyname` on macOS (the Linux path currently
  passes an empty default-constructed `HostFeatures` — a known `TODO`).
- Fault delivery via plain POSIX `sigaction` on macOS (no Mach exception port needed): a per-guest-
  page permission shadow table reconciling Apple Silicon's 16KB host page size against the guest's
  4KB architectural page size, JIT guard-page write-protect race handling, MMIO decode-and-emulate,
  misaligned load-acquire/store-release decode-and-emulate, synthetic-`#GP`-to-real-vector
  remapping, and cooperative thread-stop via FEXCore's `InterruptFaultPage` mechanism.
- Real JIT code-buffer overflow guard pages on macOS. Apple Silicon's `MAP_JIT` memory cannot have
  its protection changed by an ordinary `mprotect()` after the fact (confirmed empirically: it always
  fails `EACCES`, regardless of the per-thread W^X toggle state), so FEXCore's own end-of-buffer guard
  page silently never took effect there. The sogen-side FEXCore-internal allocator arena now places
  the real guard by construction instead: it makes only the buffer's leading portion (excluding one
  trailing host page) the actual executable `MAP_JIT` mapping via the same real-`munmap`-then-hint-
  `mmap` placement it already uses for `MAP_JIT` allocations, and simply never touches that trailing
  page — which stays part of the arena's permanent `PROT_NONE` reservation and genuinely faults on any
  access. This applies to both the main JIT code buffer and the per-compile temporary staging buffer.
- GPU-bridge host-memory coherency (`mach_vm_remap` aliasing on macOS, `sys_dcache_flush`/`dc civac`
  cache maintenance) for the paravirtualized-GPU memory-sharing path.
- 32-bit WoW64 process support (a second, lazily-created 32-bit FEXCore Context/Thread per process,
  gate-crossing state marshaling, a WoW64 guest-memory rebase — see below).
- Build, backend-selection, and Python-binding wiring (auto-enabled on ARM64 Linux/macOS + Clang);
  FEXCore built via ExternalProject and linked.

## WoW64 support

FEXCore is fixed-bitness per `Context` — a single `Context` cannot execute both 64-bit and 32-bit
code. A WoW64 process genuinely starts execution in real 64-bit ntdll code (the thread-init thunk),
crosses into 32-bit code at the x86 "heaven's gate" bitness switch, and crosses back on every syscall
return and kernel callback. This backend models that with **two FEXCore Contexts per WoW64 process**
— a 64-bit one (handling the real 64-bit ntdll/wow64*.dll code) and a 32-bit one (handling the
guest's own 32-bit image and its 32-bit ntdll) — switching which one is "active" at each gate
crossing.

### Gate-crossing interception

The bitness-switch trampoline is intercepted via the same generic non-executable-range mechanism
FEXCore already uses for synthetic page faults: the trampoline's guest address range is marked
non-executable, so reaching it raises a controlled synthetic `#PF` before any of its bytes are ever
JIT-compiled, which the backend catches and handles by marshaling state between the two Contexts
directly (rather than letting either Context attempt to decode/execute the other bitness's code).

State marshaling across a crossing needs to be careful about more than a naive whole-`CPUState` copy:
each Context's `CPUState` also carries FEXCore-internal JIT bookkeeping (SRA-mapped GPRs, the call-ret
shadow-stack pointer, the JIT lookup-cache pointer) interleaved with genuinely-architectural x86 state
(GPRs, XMM, x87, EFLAGS, segment selectors). A correct crossing copies the architectural state and
leaves each Context's own JIT bookkeeping alone.

### 32-bit guest memory rebase

Apple Silicon enforces a mandatory, unshrinkable 4GB `__PAGEZERO` for every 64-bit process, making
the entire low 4GB of host address space permanently unmappable. This conflicts directly with this
project's guest-VA-equals-host-VA memory model for a 32-bit guest, whose whole architectural address
space lives in that exact range. The fix is a FEXCore-side, per-Context, runtime-conditional address
rebase (a `CONFIG_WOW64GUESTREBASE` option and `Context::SetNeedsWow64GuestRebase` API in the
`deps/FEX` submodule): when enabled on a Context, every real memory access — data or instruction
fetch — computed at an address below 4GB is transparently rebased to a fixed offset above it, via a
runtime IR `Select` rather than a compile-time-constant add (since a 64-bit-mode Context's addresses
aren't confined to a fixed range the way a 32-bit-mode Context's are). This is a strict no-op for any
Context that doesn't opt in, so it does not affect the existing 64-bit-only Linux/macOS path.

### `wow64cpu.dll`'s real dispatch convention

The real `TurboDispatchJumpAddressStart` mechanism inside `wow64cpu.dll` — the genuine syscall-return
dispatch table a real WoW64 process uses — has a documented calling convention (an index derived
from the high word of `EAX`, dispatched through a jump table `wow64cpu.dll` builds at init, against a
CPU-area `CONTEXT` block holding the 32-bit register file). The reverse (32→64) gate crossing decodes
this convention, marshals state, and resumes real 64-bit execution at `TurboDispatchJumpAddressEnd` —
the generic dispatch continuation, whose genuine `wow64cpu.dll`/`wow64.dll` code
(`Wow64SystemServiceEx`) then services the syscall. Only the turbo-thunk fast-path itself (the
`jmp [r15+rcx*8]` jump-table dispatch into per-service turbo thunks) is never taken; the crossing
always forces the generic path.

## Architecture

- `fex_x86_64_emulator.hpp` — factory `sogen::fex::create_x86_64_emulator()`.
- `fex_x86_64_common.hpp` — header-only `x86_register` → `CPUState` field mapping (GPRs and their
  sub-registers, rip, flags, xmm, mm, fs/gs base, segment selectors, mxcsr/fcw). No FEX includes, so
  it can be reasoned about without the FEX toolchain.
- `fex_x86_64_marshal.hpp` — the architectural-state marshaling helper used at WoW64 gate crossings
  (factored out because it is shared by the two call sites that need it,
  `enter_wow64_32bit_from_run_simulated_code` and `perform_bitness_switch`).
- `fex_x86_64_emulator.cpp` — the backend, the syscall bridge, and the WoW64 gate-crossing/rebase
  integration.

### Guest model — the key difference from the other backends

FEX is an **in-process** binary translator. It does not sandbox a separate guest address space:
translated guest code runs inside the host process and **guest virtual addresses are host virtual
addresses** (a 1:1 mapping, subject to the WoW64 rebase described above for 32-bit processes). This
drives the whole design:

- `map_memory()` is a real `mmap(MAP_FIXED)` at the guest address; `unmap_memory()`/
  `apply_memory_protection()` are `munmap`/`mprotect`. A sorted region map tracks what is mapped, and
  (on macOS) a per-4KB-page permission shadow table reconciles the 16KB host page granularity against
  the guest's 4KB pages.
- `read_memory()`/`write_memory()` are direct host `memcpy`s once the range is confirmed mapped (the
  guest pointer is directly dereferenceable). Writes invalidate FEX's translation cache for the range
  in the **currently-active** Context only. Unmaps additionally invalidate the inactive Context
  (`invalidate_code_range(..., include_inactive_contexts=true)`, called only from `unmap_memory`),
  since the Context active at unmap time does not necessarily match the Context whose cached
  translations cover the unmapped range. Known limitation: 32-bit guest code that self-patches via a
  syscall-serviced `write_memory` (e.g. `NtWriteVirtualMemory`) — which runs while the 64-bit engine
  is active — can leave stale 32-bit JIT translations cached. This gap is deliberate: extending
  inactive-Context invalidation to every write would repeatedly delink the live 32-bit Context's
  blocks and risks livelocking it, given how frequent ordinary syscall-time writes are.
- `map_host_memory()` aliases caller-owned memory into the guest with `mremap(MREMAP_FIXED)` on Linux
  or `mach_vm_remap` on macOS (e.g. for the GPU bridge), and is *not* munmap'd on teardown.

This makes FEX a close cousin of the KVM backend in *capability*: the guest runs natively, so there
is **no per-access or per-instruction instrumentation point**. The fine-grained
`hook_memory_read/write/execution/range_execution` and `hook_basic_block` hooks are registered for API
compatibility but never fire, and `supports_global_memory_execution_hooks()` returns `false`. Only the
`syscall` instruction hook is wired (via the syscall handler). Analyzer features that depend on the
unfired hooks are unsupported under this backend, exactly as with KVM.

### Syscall interception

FEX delivers guest `syscall` instructions to a `FEXCore::HLE::SyscallHandler::HandleSyscall`. The
backend's `fex_syscall_handler` forwards these to the registered syscall instruction-hook. The hook
(the Windows emulation layer) reads and writes guest registers itself through the emulator and places
the NT status in `RAX`; the handler returns that value so FEX preserves it.

## Build

FEX is vendored as a git submodule (`deps/FEX`) and built **in-tree** from source — there is no manual
toggle. The top-level CMake enables the backend automatically when all of these hold:

- target is **ARM64 Linux** (not Android: the NDK's libc++ lacks `std::atomic_ref`, which FEX needs)
  **or ARM64 macOS**,
- the compiler is **Clang** (FEX rejects GCC/MSVC — Apple Clang qualifies), and
- the `deps/FEX` submodule is checked out.

Otherwise the backend is silently skipped, so all other builds are unaffected. FEX is **not** added
via `add_subdirectory` — it assumes it is the top-level project (~60 uses of `CMAKE_SOURCE_DIR`) and
would configure its whole loader/tools tree. Instead `deps/CMakeLists.txt` builds it standalone with
`ExternalProject` (only the `FEXCore_shared` target → a self-contained `libFEXCore.{so,dylib}`) and
exposes it as the imported `fexcore` target that `fex-emulator` links. Embedder-hostile FEX options
(its custom allocator/jemalloc, LTO, telemetry, tools, tests) are disabled.

```sh
# On an arm64 Linux or macOS host with Clang:
git submodule update --init --recursive deps/FEX
cmake --preset=release   # FEX backend auto-enables
cmake --build --preset=release --target fex-emulator
```

**Note for `deps/FEX` changes**: FEXCore is only rebuilt when its own `ExternalProject` target is
invoked directly — a plain `cmake --build --preset=release` does *not* pick up `deps/FEX` source
changes automatically. After editing anything under `deps/FEX`, rebuild it explicitly:

```sh
ninja -C build/release/deps/fex_external-prefix/src/fex_external-build FEXCore/Source/libFEXCore.dylib
```

No copy step is needed afterward — `fex-emulator`'s rpath points directly at that build directory
(see "Build" above), so the freshly-rebuilt library is picked up in place.

`backend-selection` links `fex-emulator`, defines `SOGEN_ENABLE_FEX`, adds `backend_type::fex`, and
honors `EMULATOR_FEX=1`; the Python `Backend` enum gains `fex`.

### Submodule fork

`deps/FEX` is pinned to [github.com/JackTYM/FEX](https://github.com/JackTYM/FEX), a personal fork of
upstream FEX-Emu (`github.com/FEX-Emu/FEX`), not upstream directly. The fork carries the
Darwin/Apple Silicon portability and correctness patches this backend depends on: JIT/temp-buffer
guard pages sized for Apple Silicon's 16KB host page granularity, `MAP_JIT` write-protect and
build/link fixes, a per-`Context`/runtime-configurable WoW64 guest-rebase host offset, and an
opdispatch fix (`InvalidOp` raising `#UD` instead of `#DE`) — see the fork's commit history for the
full list.

Pinning to a personal fork means these patches do not automatically track upstream FEX-Emu,
including upstream security fixes. Picking up an upstream FEX-Emu update requires manually
rebasing/merging this fork onto the corresponding upstream release and re-verifying the Darwin
patches still apply.

## iOS cross-compilation

### Porting notes

`cmake --workflow --preset=ios` (or `cmake --preset=ios && cmake --build --preset=ios`) configures
and builds the `analyzer` target for `arm64-apple-ios`, producing a real Mach-O binary at
`build/ios/artifacts/analyzer` — confirmed via `file` (`Mach-O 64-bit executable arm64`) and
`otool -l` (`LC_BUILD_VERSION` `platform 2` / `minos 14.0`). The preset trio (configure, build,
workflow, all named `ios`) lives in `CMakePresets.json` and points `CMAKE_TOOLCHAIN_FILE` at
`cmake/toolchain/ios.cmake`.

Getting there required no source or `src/CMakeLists.txt` gating at all beyond one narrow fix:

- **Configuring** (`cmake --preset=ios`) needed zero `add_subdirectory` gating anywhere in the
  tree — every dependency's `find_package`/`find_library` resolved cleanly for the iOS toolchain
  once the `ios` preset's own cache settings were in place, in particular
  `SOGEN_USE_SYSTEM_SDL3=OFF`, which forces the vendored `deps/SDL` submodule instead of
  accidentally resolving this host's Homebrew macOS SDL3.
- **Building** `analyzer` hit exactly one real compile error across the whole ~570-step build
  graph: vendored FreeType's zlib shim (`deps/freetype/src/gzip/zutil.h`) defines the `OS_CODE`
  macro twice — once for `TARGET_OS_MAC`, once for `__APPLE__` — and both are true simultaneously
  on any Apple platform. This only trips `-Werror -Wmacro-redefined` on the iOS SDK, not the macOS
  one, apparently because `<TargetConditionals.h>` becomes visible at a different point in this
  translation unit between the two SDKs. It's fixed with a targeted, `IOS`-gated
  `-Wno-macro-redefined` on just `deps/freetype/src/gzip/ftgzip.c` in
  `src/windows-emulator/CMakeLists.txt` (next to the existing freetype `add_subdirectory` there),
  not by patching the `deps/freetype` submodule itself. An earlier attempt patched the submodule
  directly and was discarded before being committed: `deps/freetype` is a shallow submodule
  tracking upstream with no push access, so a local-only submodule commit would silently vanish on
  any future fresh clone/CI checkout. Worth remembering for any other `deps/` submodule patch in
  this repo.

FEX itself stays **off** for iOS, deliberately. The gate above (`CMakeLists.txt:69`) only enables
`SOGEN_ENABLE_FEX` for `(Linux AND NOT ANDROID) OR Darwin`; under the `ios` preset
`CMAKE_SYSTEM_NAME` is `"iOS"`, which matches neither, so the backend is silently skipped. This
was left untouched on purpose — enabling FEX for iOS is real follow-on work, not part of this
plan.

Note also that `analyzer` compiling and linking for iOS is necessary but not sufficient to run
anything there: stock iOS cannot execute a bare Mach-O binary outside a signed `.app` bundle
launched through LaunchServices — there is no interactive shell to `exec` it from the way
`./analyzer` works on macOS. Live on-device execution is out of scope here and belongs to the
app-shell plan below.

### Follow-on work

This unblocks two follow-on plans:

- **FEXCore JIT26 allocator changes** — `deps/FEX/FEXCore/Source/Interface/Core/JIT/JIT.cpp` and
  `deps/FEX/FEXCore/include/FEXCore/Utils/AllocatorHooks.h` need a breakpoint-driven
  memory-blessing protocol (`brk #0xf00d` with `x16=1` to prepare a region, `x16=0` to detach)
  plus a `mach_vm_remap`+`mprotect` writable-alias technique, gated on TXM/SPTM hardware
  presence, before FEX's JIT can actually execute code on iOS 26+ hardware. Plain
  `mmap(MAP_JIT)` + `pthread_jit_write_protect_np` is insufficient there — confirmed blocked with
  `EPERM` even under a real, externally-granted `CS_DEBUGGED`, in a separate out-of-tree
  investigation.
- **The three-target iOS app shell** — host app + an ExtensionKit `JITHelper` extension
  (vendoring the open-source `StikJIT` library, MPL-2.0) + a `TunnelExtension`
  (`NEPacketTunnelProvider`, adapted with attribution from the open-source `StosVPN` project,
  MIT-licensed). This architecture was fully validated and got real JIT execution working
  end-to-end, entirely self-contained (no external app dependencies beyond a one-time
  pairing-file import), in a throwaway Swift spike that lives outside this repository — it's not
  under `tools/`, not part of sogen's CMake build. Full write-up:
  https://claude.ai/code/artifact/0e1a4eee-b721-450d-acbd-18bc25f96963

Shipping either of the above requires signing through an Apple Developer account with the
`NetworkExtension` entitlement approved — confirmed unavailable to free personal-team accounts via
a real Xcode provisioning error in that same out-of-tree investigation.

## Selecting the backend

```sh
EMULATOR_FEX=1 ./analyzer -e root c:/test-sample.exe
```

or, from Python, `sogen.Backend.fex`.
