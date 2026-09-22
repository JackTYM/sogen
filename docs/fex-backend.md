# FEX backend — development notes

The FEX-Emu emulator backend (`sogen::fex`). A standalone backend that mirrors the structure of the
other backends, selected at runtime via `EMULATOR_FEX=1`. Its functional targets are **macOS on Apple
Silicon** and **Android on AArch64** — [FEX](https://fex-emu.com) only JITs x86/x86-64 to ARM64.
32-bit WoW64 guests are additionally supported on the macOS/Apple Silicon target (see "WoW64 support"
below).

## Status

Working on Darwin/Apple Silicon: `test-sample.exe` runs to completion, matching the Unicorn
backend's behavior, and has been validated against ~40 pre-existing sample executables
(process/thread introspection, synchronization primitives, sections, security tokens, the registry,
pipes, sockets, timers, file/directory I/O, GUI dialogs, DXGK harnesses, and 32-bit WoW64 binaries)
with every remaining discrepancy traced to a backend-agnostic gap (an unimplemented syscall, or a
missing staged system-DLL export) rather than a difference between this backend and the existing
one. That full walk was last done in July 2026, on a macOS build predating the Apple security update
described under "32-bit guest memory rebase" below; this round re-confirmed `test-sample.exe` and
the full `windows-emulator-test` suite (which exercises most of the same surface: process/thread
introspection, sections, tokens, the registry, and more) still pass on the current macOS build with
that fix in place, but did not re-walk the complete ~40-sample list. Android/AArch64 is now a
functional target for native 64-bit guests and currently requires a 4KB host page; WoW64 has not
been validated there. Other AArch64 Linux hosts have build coverage only and are not expected to
run.

**32-bit WoW64 processes are supported on Darwin, live-verified end to end** (see "WoW64 support"
below): a real 32-bit guest process runs against a second, lazily-created 32-bit FEXCore Context,
with state marshaled across the bitness-switch trampoline on every crossing.
`wow64-test-sample.exe` completes under `--backend fex` on Apple Silicon, printing
`wow64-test-sample: ok` and exiting 0 — confirmed repeatedly this session after the Mach VM fix
described under "32-bit guest memory rebase" below. This is one sample, not the broader ~40-sample
walk above; no other pre-existing sample has been re-validated specifically under WoW64.

### Performance

On a pure compute-bound workload (a tight ALU loop with no syscalls beyond timestamping), this
backend measured roughly **129x less wall-clock time** than the Unicorn backend on the same host
(~3.4s vs. ~439s for 2 billion loop iterations) — JIT-compiled native ARM64 code running directly on
the CPU, versus per-block interpretation. A mixed, syscall-heavy smoke-test workload showed a
smaller but still substantial ~27-30x gap. Expect the larger multiplier to dominate for CPU-bound
guest workloads (game logic, physics, scripting) and the smaller one where I/O/syscall dispatch is a
significant fraction of runtime. Measured in July 2026, on the same pre-update macOS build as the
sample walk above; not re-measured this round. Unaffected by the WoW64 host-window fix below either
way, since that workload is a native 64-bit process and never reaches WoW64-only code.

## Security / address-space model

**FEX shares the guest and host address space — there is no isolation between them.** Unlike
Unicorn/Icicle (a fully software-simulated guest address space, sandboxed from the host process by
construction) or KVM/WHP (real hardware virtualization, a genuinely separate guest physical address
space), FEX is an in-process JIT: it translates x86/x86-64 to ARM64 and executes the result directly
inside this process, treating guest virtual addresses as host virtual addresses (a 1:1 mapping,
`guest VA == host VA`, subject to the WoW64 guest-memory rebase described below for 32-bit
processes). Concretely:

- `map_memory()` is a real `mmap(MAP_FIXED)` at the guest address; `read_memory()`/`write_memory()`
  are a direct host `memcpy` once the range is known to be mapped — there is no page-table or bounds
  check standing between a guest access and this process's own memory.
- A guest out-of-bounds read or write can therefore land directly in this process's own memory —
  including sogen's own runtime state — in a way that is structurally impossible under
  Unicorn/Icicle. This is an inherent property of how FEX-Emu is designed to work (it is built as a
  transparent Linux binfmt-style translation layer, the same category as `qemu-user`/`box64`, where
  "guest pointer is a host pointer" is exactly what makes syscall passthrough and native-speed
  execution possible), not a gap specific to this port. See
  [`FEX-Emu/FEX`'s own `docs/ProgrammingConcerns.md`](https://github.com/FEX-Emu/FEX/blob/main/docs/ProgrammingConcerns.md)
  and `docs/allocator_usage.md`, which state this plainly from FEX's own side.
- This backend's own address-space protections (`host_reserved` region tracking in
  `windows-emulator/memory_manager.cpp`, the Apple-only FEXCore internal arena, and `GDT_ADDR`'s
  placement in `process_context.hpp`) are mitigations, not a sandbox: they keep ordinary guest
  allocations from *landing on* addresses this backend or the host process needs, reducing collision
  risk. They do not and cannot stop a guest from computing an arbitrary pointer into any other valid
  host-mapped memory — there is no hard fault boundary for that, by design.
- Practically, this means FEX is not an appropriate backend for running guest code you do not trust
  at all to stay within its own memory — the same class of workload Unicorn/Icicle's sandboxed model
  is suited for.
- `deps/CMakeLists.txt` explicitly disables FEX's own internal allocator
  (`-DENABLE_FEX_ALLOCATOR=OFF`, `-DENABLE_JEMALLOC_GLIBC_ALLOC=OFF`) — guest allocation is owned by
  sogen's memory manager. On Apple, the backend separately installs FEXCore allocator hooks that
  steer FEXCore's own anonymous internal mappings into a dedicated 4GiB host-reserved arena, keeping
  them out of guest allocation ranges. This is still only a collision mitigation, not isolation.
- A 32-bit WoW64 guest's whole architectural address space would otherwise sit inside Apple Silicon's
  mandatory, unmappable 4GB `__PAGEZERO` (see "32-bit guest memory rebase" below) — the rebase is what
  makes `guest VA == host VA` viable at all for that case.

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

#### Which engine is live: `active_context_` / `active_thread_`

A crossing flips `active_context_`/`active_thread_` — the pointers naming which of the two fixed
Context/thread pairs is executing right now — from inside `handle_fault_signal`, a real
kernel-delivered signal handler. Both are `std::atomic`, for two independent reasons:

- The C++ abstract machine has no control-flow edge for signal delivery, so a plain member could be
  cached across the opaque `ExecuteThread()` call and `start()`'s resume loop would re-enter the
  *pre*-crossing engine. This is the same hazard `interrupt_page_unwind_` is atomic for.
- `request_thread_stop()` reads `active_thread_` from the quantum-timer thread (see
  `is_stop_thread_safe()`) while the vCPU thread runs guest code with the kernel lock released — a
  genuine cross-thread access, not just a signal-handler one.

Every write happens on the vCPU thread, so that thread's own reads need atomicity but not ordering
and go through `active_context()`/`active_thread()`, which load `memory_order_relaxed`. The stores
are `memory_order_release` and `request_thread_stop()`'s single load is `memory_order_acquire`, so
the one cross-thread reader also observes the `InternalThreadState` the pointer names fully
constructed. `request_thread_stop()` loads once into a local rather than re-reading between its null
check and its `mprotect`: a crossing on the vCPU thread in that window would otherwise protect one
engine's `InterruptFaultPage` after having tested the other's.

### Translation-cache invalidation across contexts

`write_memory()`/`unmap_memory()` invalidate FEX's translation cache for the affected range in the
**currently-active** Context only. `unmap_memory()` additionally invalidates the inactive Context
(`invalidate_code_range(..., include_inactive_contexts=true)`), since the Context active at unmap
time does not necessarily match the Context whose cached translations cover the unmapped range.
Known limitation: 32-bit guest code that self-patches via a syscall-serviced `write_memory` (e.g.
`NtWriteVirtualMemory`) — which runs while the 64-bit engine is active — can leave stale 32-bit JIT
translations cached. This gap is deliberate: extending inactive-Context invalidation to every write
would repeatedly delink the live 32-bit Context's blocks and risks livelocking it, given how frequent
ordinary syscall-time writes are.

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

The rebased-to host window itself is claimed up front, on Apple, by `reserve_wow64_host_window()`,
carved into per-allocation ranges by `claim_host_range()`, backed by `map_mmio()` for an MMIO region
landing inside it, and re-protected across a decommit/recommit by `remap_host_claim()`. All four go
through the Mach VM API (`mach_vm_map`/`mach_vm_allocate`/`mach_vm_deallocate`), not the BSD
`mmap`/`munmap` used for every other host allocation in this file: a macOS 26.6.2 security update
added `EXC_GUARD`/`DEALLOC_GAP` enforcement to the BSD `mmap`/`munmap` syscall path for `MAP_FIXED`
requests landing in this window, delivering an uncatchable `SIGKILL`, while the equivalent Mach VM
calls targeting the same addresses/sizes succeed cleanly (see "Fixes found during bring-up and
review" below). `remap_host_claim()` is hardened unconditionally on Apple, not only inside this
window: `reserve_wow64_host_window()` runs unconditionally for every guest (wow64 or not, see its
own call site's doc comment) and steers this backend's own allocations away from this specific
window regardless of bitness, so this window itself is not reachable by a plain 64-bit guest's own
claims - but the security update's wording ("certain host address windows", plural) does not say
this is the only guarded range on the system, and `remap_host_claim()` is shared, unconditional code
for every claimed range on either bitness. The window's whole-span release at emulator teardown is
skipped entirely rather than moved to `mach_vm_deallocate`, since releasing it in one call after
per-range `claim_host_range()` calls have fragmented it is confirmed to trip the same guard on the
deallocation side; the address space is leaked instead, for the lifetime of the process, matching
`fex_internal_arena`'s own existing precedent for its own reservation.

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

- `fex_x86_64_emulator.hpp`/`.cpp` — factory `sogen::fex::create_x86_64_emulator()` and the backend
  itself; see the file's own top-of-file comment for the implementation-level version of the model
  described above.
- `fex_x86_64_common.hpp` — register classification (`classify_gpr` and friends) mapping
  `x86_register` to FEXCore's `CPUState`.
- `fex_x86_64_marshal.hpp` — the architectural-state marshaling helper used at WoW64 gate crossings
  (factored out because it is shared by the two call sites that need it,
  `enter_wow64_32bit_from_run_simulated_code` and `perform_bitness_switch`).
- Guest `syscall` instructions route back to sogen through a `FEXCore::HLE::SyscallHandler`, which
  invokes the registered syscall instruction-hook — the same mechanism the Windows emulation layer
  uses for every backend.
- The fine-grained `hook_memory_read/write/execution/range_execution` and `hook_basic_block` hooks
  are accepted for API compatibility but never fire, exactly like the KVM backend: guest code runs
  natively, so there is no per-access/per-instruction instrumentation point short of single-stepping.
- `deps/FEX` is **unchanged by this branch**: it stays on `origin/main`'s own pin,
  `github.com/momo5502/FEX` (branch `macos-arm64`) at `ff4f18b2`. (`origin/main`'s own text here said
  `github.com/JackTYM/FEX`, which was already stale before this branch — `.gitmodules` has pointed at
  `momo5502/FEX` since that fork was created.) An earlier revision of this work repointed the
  submodule at `github.com/JackTYM/FEX` @ `efa89c99`, on the assumption that
  `momo5502/FEX` lacked the `CONFIG_WOW64GUESTREBASE` option and the
  `Context::SetWow64GuestRebaseValue`/`SetNeedsWow64GuestRebase` API this WoW64 support depends on.
  That assumption was wrong and the submodule change has been dropped. `momo5502/FEX` is a fork
  *of* `JackTYM/FEX` (GitHub reports `parent = JackTYM/FEX`, `source = FEX-Emu/FEX`), not a
  competing one: `efa89c99` is an ancestor of `momo5502/FEX`'s `macos-arm64` tip, and was in fact
  `origin/main`'s own `deps/FEX` pin before two dependabot bumps moved it to `e6975e6f` and then
  `ff4f18b2` (which only adds Android host support on top). The full WoW64 rebase API is present in
  the pinned commit (`FEXCore/include/FEXCore/Core/Context.h`,
  `FEXCore/Source/Interface/Core/{Addressing.h,Frontend.cpp,OpcodeDispatcher.h}`), and this branch
  builds, passes `windows-emulator-test` and passes the WoW64 live smoke test against it unmodified.
  Pinning to a fork at all still means its patches don't automatically track upstream FEX-Emu
  security fixes; periodically rebasing onto a newer upstream release remains a maintenance cost
  worth weighing, but that applies equally to `origin/main` today and is not something this branch
  changes.

## Build & test

No special setup needed, unlike the KVM backend (which requires a Docker-on-Windows workaround to
build or run at all): from an ARM64 Clang host (Apple Silicon macOS, ARM64 Linux, or Android), the
ordinary `cmake --preset=release` picks this backend up automatically once `deps/FEX` is checked out
(see the root `CMakeLists.txt` gate). Run with `EMULATOR_FEX=1`. Android currently requires a 4KB
host page.

## Known limitations

- **On Darwin, a protection fault from a plain `STR`/`STUR` store can be misclassified as a read.**
  `decode_arm64_store` deliberately recognizes only the `STLR` family; a plain store to read-only
  memory can therefore surface as an unhandled host signal instead of a guest
  `STATUS_ACCESS_VIOLATION`, while an unmapped plain store reaches the guest with the operation marked
  as a read. Android avoids this specific misclassification by using the kernel-provided ESR WnR bit.
  This affects any guest that deliberately writes to read-only memory and catches the resulting
  exception (packers/DRM protections do this routinely).
- **`sync_host_page_apple`'s permission union can strip `PROT_EXEC` or over-grant write access near
  PE section boundaries.** Guest permissions are tracked per-4KB shadow slot but applied per-16KB
  Apple host page (four slots share one host page); when slots sharing a host page disagree, the
  code unions them (documented in-code as a deliberate, temporary simplification pending a later
  Mach-exception-handler phase that can resolve per-slot faults properly). A PE's `.text` (RX)
  immediately followed by `.data` (RW) landing in the same host page can lose exec permission on the
  tail of the code page — a real execution fault, not just an over-permissive read-only page.
- **The MMIO fault path isn't fully async-signal-safe.** `handle_mmio_fault` calls the registered
  MMIO callback directly from inside the real signal handler, rather than through this file's own
  `pending_fault_dispatch_` mechanism (built for exactly this class of hazard, and already used for
  `memory_violation_hooks_`/`interrupt_hooks_`). The one production registrant, `kusd_mmio::read`,
  takes a mutex also used by `kusd_mmio::access()` from normal call context. No live self-deadlock
  path exists today under the current single-cooperative-thread-per-vCPU model, but this becomes a
  real hazard once multi-vCPU FEX support matures.
- **A smaller, same-bug-class gap remains:** `commit_memory` guards against committing into
  `section_kind`/`host_reserved` regions but has no equivalent guard for a guest probing directly into
  other backends' reserved ranges.
- **`comctl32.dll`-linked apps** (e.g. a bundled calculator sample) fail identically on both this
  backend and Unicorn due to a staged `comctl32.dll` build that doesn't export the ordinals the
  sample binaries were linked against. This is a staged-asset version mismatch, not a code bug, and
  is not fixable without staging a different `comctl32.dll`.
- **`wow64cpu.dll`'s turbo-table fast-path is bypassed.** The reverse (32→64) gate crossing does
  execute real `wow64cpu.dll` 64-bit dispatch code: after marshaling state, it resumes at the real
  `TurboDispatchJumpAddressEnd`, whose genuine code path (through `Wow64SystemServiceEx` in
  `wow64.dll`) services every 32-bit syscall. What is *not* executed is only the turbo-thunk
  fast-path at `TurboDispatchJumpAddressStart` (`jmp [r15+rcx*8]` through the per-service jump
  table) — the crossing always forces the generic dispatch continuation instead of selecting a
  specialized turbo thunk.

## Fixes found during bring-up and review

- **A macOS 26.6.2 security update made the wow64 host window's BSD `mmap`/`munmap` reservation
  fatal.** The update added `EXC_GUARD`/`DEALLOC_GAP` enforcement to the BSD `mmap`/`munmap` syscall
  path for `MAP_FIXED` requests landing in the address window `reserve_wow64_host_window()` uses,
  delivering an uncatchable `SIGKILL` no in-process handler can intercept — confirmed directly, at
  exactly the address this backend's default candidate uses. The equivalent Mach VM calls
  (`mach_vm_map`/`mach_vm_allocate`) targeting the identical address/size were confirmed to succeed
  cleanly side by side in the same process, so every call site that can target this window now goes
  through those instead: `reserve_wow64_host_window()` itself, `claim_host_range()`, `map_mmio()`
  (an MMIO region's real backing, when it lands inside the window), and `remap_host_claim()`
  (re-protecting an already-claimed range across a decommit/recommit, e.g. from
  `sync_host_page_apple`) — the last of these hardened unconditionally on Apple, not only inside
  this window, since it is shared, bitness-independent code and the security update's own wording
  ("certain host address windows", plural) does not guarantee this is the only guarded range on the
  system (see "32-bit guest memory rebase" above for why a plain 64-bit guest's own claims cannot
  reach this specific window today, and why that does not make hardening the other three sites
  optional). The window's whole-span release at teardown is skipped entirely rather than moved to
  `mach_vm_deallocate`, since releasing it in one call after per-range `claim_host_range()` claims
  have fragmented it is confirmed to trip the same guard on the deallocation side; leaked instead,
  matching `fex_internal_arena`'s own existing precedent. Live-verified repeatedly across all of the
  above: `wow64-test-sample.exe` now runs to completion under `--backend fex` on Apple Silicon,
  printing `wow64-test-sample: ok` and exiting 0, and the full `windows-emulator-test` suite remains
  at 69/69.
- **Host-reserved-address-space tracking under Apple's `mmap(MAP_FIXED)` semantics.** Two real
  memory-safety bugs: an MMIO-region-insertion path could violate the host-reserved overlap-tracking
  invariant (undetected until a `mmap(MAP_FIXED)` failure crashed the analyzer), and a
  decommit→recommit window could let a foreign host allocation land in a guest's still-reserved
  range. Fixed by properly carving host-reserved holes and adding
  `memory_interface::release_guest_address_range()` to distinguish decommit (keep the address
  claimed) from release (actually free).
- **Scheduler-quantum-race** (backend-agnostic, extracted to its own fix — see `windows_emulator.cpp`
  `start()`'s quantum-timer watchdog): a non-atomic `switch_thread = true` / `cpu.stop()` pair could
  be split across the scheduler's own consume step, misread as a fatal crash. This backend's
  cooperative page-protect stop makes the race hot enough to hit reliably, which is how it was found.
- **`InterruptFaultPage` unwind atomicity.** A raced interrupt-fault-page unwind (the mechanism this
  backend uses to interrupt JIT-compiled code) could be misread as a fatal stop by `start()`'s run
  loop under concurrent access; tagged and re-armed instead of terminating.
- **`active_thread_`/`active_context_` data race.** Both were plain pointers written from inside the
  real signal handler and read from the quantum-timer thread by `request_thread_stop()`. Now
  `std::atomic`, with the ordering and single-load rules described under "Which engine is live"
  above. `create_thread32()` also no longer briefly publishes `thread32_` as the active engine just
  to set up its call-ret stack — `ensure_callret_stack()` takes the engine explicitly instead, which
  removes the window in which a cross-thread `stop()` could have protected the wrong engine's
  `InterruptFaultPage`.
- **`int 2Dh` (the Windows debug-service trap) resumed at the wrong address.** FEXCore only reports
  RIP already past the trapping instruction for real `INT3`/`INT1` (`SetRIPToNext` in FEXCore's
  `OpcodeDispatcher.cpp`); a generic `INT n` this backend can't dispatch directly (remapped from a
  synthetic `#GP` back to its own vector) already reports the instruction's own start address. A
  shared-code fixup that unconditionally corrected for the INT3 case broke this one. Fixed by
  normalizing in this backend, conditioned on FEXCore's `TrapNo == X86_TRAPNO_BP` (verified to be set
  in exactly one place in all of FEXCore — the real `0xCC` case) — which let the shared-code
  workaround be deleted outright rather than patched further.
- **Missing GPR sub-register widths.** `classify_gpr` had no cases for `r8b`-`r15b`/`r8w`-`r15w`/
  `r8d`-`r15d` (only the full 64-bit and the original 8 registers at every width) — silently
  zero-filled on read, silently dropped on write. Common in real x86-64 code (e.g. `mov r9d, eax`).
- **`setup_gdt` discarded an allocation failure.** `GDT_ADDR` is deliberately placed at a high, fixed
  address on Apple specifically because low addresses and a range of typical host allocations are
  unusable/collision-prone here (see the address-space model above) — a collision there is real, not
  theoretical, and previously failed silently instead of raising a diagnosable error.
