# Sogen D3D9 Shim-Free Graphics — MacBook Handoff

> **Purpose:** Resume state for this work on the MacBook (M5 Pro, macOS/Apple Silicon).
> Originally written 2026-07-01 for the Linux→Mac migration; updated 2026-07-02 after the migration,
> macOS build bring-up, MoltenVK/WoW64 validation, and **gate 3 resolution** were completed. This version
> focuses on current state and the next step: Stage 2 Part 2/3 (§10).

---

## 1. TL;DR — where we are

Building **shim-free DirectX for sogen**: the Windows guest loads *official Microsoft* graphics DLLs,
which reach the GPU via *standard graphics-device registration + D3DKMT kernel syscalls*, and **all
API→Vulkan translation happens on the sogen host side** (targeting `vulkan_host` → a real Vulkan driver).

- **Stage 1 (Vulkan→Vulkan): COMPLETE** — official Khronos loader + our registered ICD, validated x64 + WoW64.
- **macOS port + MoltenVK: COMPLETE (2026-07-02).** Release build is green on Apple Clang; `vulkan_host`
  creates a real device against MoltenVK (portability extensions negotiated); a batch of WoW64 DXGK
  struct-thunking bugs (unrelated to gate 3) found and fixed. Verified end-to-end with
  `native-gpu-clear-sample`: real clears + readback, pixel-correct, through MoltenVK on the M5 Pro GPU.
  Committed in `a2088222` (build portability) and `9ee49a02` (MoltenVK/WoW64). Full details:
  `memory/project_moltenvk_wow64_dxgk.md` and `memory/feedback_wow64_dxgk_struct_abi.md`.
- **Stage 2 (D3D9): Spike B — DONE (2026-07-02).** Official `d3d9.dll` loads our thin WDDM user-mode
  driver and successfully creates a real `IDirect3DDevice9`.
  - **Gate 1 — `GetDeviceCaps(HAL)` → S_OK: DONE.**
  - **Gate 2 — our DDI `pfnCreateDevice` reached + S_OK: DONE.**
  - **Gate 3 — top-level `IDirect3D9::CreateDevice` → S_OK: DONE.** Root cause was a bug in **our own
    UMD**, not sogen's kernel: `OpenAdapter` echoed the runtime's offered interface `Version` (`0xe000`)
    back as `DriverVersion`, making the runtime believe our `D3DDDI_DEVICEFUNCS` table extended to
    WDDM2.1+ slots (`pfnAcquireResource`/`pfnReleaseResource`) that our WIN7-sized table doesn't declare;
    `ValidateUMDeviceFuncs` read uninitialized memory past the table and failed with exactly
    `D3DERR_NOTAVAILABLE (0x8876086a)`. Fix: report our own implemented version
    (`SOGEN_D3D9_UMD_INTERFACE_VERSION`) instead. Full RE trail: §6.
- **Stage 2 Part 2 (D3D9 DDI → sogen command stream): IN PROGRESS (2026-07-02).** Wire protocol, guest
  UMD ↔ host transport, and real marshaling for the highest-frequency per-draw state/draw DDI functions
  are done and compile/smoke-test clean. Resource creation, Lock/Unlock, and Present are still
  `device_stub` (need their own struct-layout verification pass — see §10). Full details: §10.

**This work is entirely CPU/kernel-side and headless** (no GPU rendering needed for gate 3 or Part 2's
transport bring-up), so MoltenVK being done now is a bonus for Part 3's actual draw execution, not
something either of these depended on.

---

## 2. Project quick facts

- **Repo:** `sogen` — C++20 Windows user-space emulator. Produces `analyzer` binary.
- **Branch:** `feat/mw2-on-upstream` (main branch is `main`).
- **Approved Stage-2 plan:** `.claude/plans/scalable-giggling-fern.md` (detailed; read it).
- **Build (fast dev):** `cmake --build --preset=release`
- **Build (final, slow, clang-tidy):** `cmake --build --preset=tidy` — only at the very end.
- **CPU backends (`src/backends/`):** `kvm` (Linux+x86-64 only, hardware-fast), `unicorn` (software,
  default, cross-platform), `icicle` (software, Rust, `EMULATOR_ICICLE=1`), `whp` (Windows).
  Selection in `src/backend-selection/backend_selection.cpp`; default = **unicorn**; `EMULATOR_KVM=1`
  forces KVM on Linux/x86.

---

## 3. Migration outcome (historical — migration is done)

What actually happened, for context if something still seems missing: the transfer ran inconsistently —
the working git tree (with uncommitted changes + untracked files) landed correctly at
`~/Documents/Coding/C++/sogen`, but a *second*, separate full-tree copy also landed nested inside the
repo at `sogen/sogen/` before being cleaned up. Two things were **confirmed permanently lost** and are
not recoverable: the old Claude memory dir contents (`project_stage2_d3d9.md` etc.) and
`scratchpad/spike_probe.log` (the gate-3 probe capture). The real `root/` guest filesystem (registry
hives + licensed Windows DLLs incl. `d3d9.dll`) was recovered from that nested copy before it was deleted
and is in place at `build/release/artifacts/root/`. IDA Pro + `idasql` are installed and working (macOS
path: `/Applications/IDA Professional 9.0.app/Contents/MacOS/idasql`). `mingw-w64` (both x64/x86 cross
compilers) is installed via Homebrew. Full account: `memory/project_macos_migration.md`.

---

## 4. Architecture (why the port is cheap)

Guest (frozen, official DLLs only) → **thin sogen WDDM UMD** (`sogen_d3d9um.dll`, the vendor-driver slot)
→ D3DKMT `NtGdiDdDDIEscape` → host `gpu_processor` → (future) D3D9 decoder → `vulkan_host` → real Vulkan.

- The guest never changes regardless of host backend. Only the host's Vulkan *driver* swaps.
- On Linux: RADV. **On macOS: MoltenVK** (or KosmicKrisp) as the Vulkan ICD — `vulkan_host` code unchanged.
- This is why a Vulkan translation layer (MoltenVK) is the right call vs. a native Metal backend.

---

## 5. Current working-tree state (2026-07-02)

`syscall_dispatcher.cpp` has one small, permanent, **off-by-default** diagnostic toggle
(`// #define ENABLE_NTSTATUS_PROBE`, mirroring the existing `ENABLE_DXGK_LOGGING` idiom in `gdi.cpp`) —
uncomment it to log every syscall return with the top two status bits set
(`[NTSTATUS_PROBE] <name> -> 0x<status> (ip=0x<addr>)`). `gdi.cpp` is fully clean/committed (the gate-1/2
fixes below were already committed in `9ee49a02`, not a leftover from this session).

### 5a. `gdi.cpp` — D3D9 gate-1/2 fixes (committed in `9ee49a02`)
1. **UMDRIVERNAME handler (~line 3748):** returns `u"sogen_d3d9um.dll"` for the DX9
   KMTUMDVERSION (Version==0), else `u"d3d10warp.dll"`. This is how the official d3d9.dll loads our UMD.
2. **`NtGdiDdDDIGetDeviceState` ResetState fix (~line 4707):** `state.ResetState = 0` for
   StateType==3 (was `1`, semantics were inverted). Real sogen kernel bug; helps any D3D9 app incl. MW2.

### 5b. `src/samples/sogen-d3d9-umd/` — the D3D9 UMD sample (source-only; see `README.md` there)
- `sogen_d3d9_umd.cpp` — the thin UMD. Exports `OpenAdapter`; fills adapter funcs; `umd_GetCaps`
  synthesizes `D3DCAPS9` + FORMATOP; `umd_CreateDevice` stubs the device-func table and reports
  `DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION` (the gate-3 fix — see §6).
- `d3d9_ddi.hpp` — hand-transcribed D3D9 UMD DDI (WDK layout; `#pragma pack(8)`; version-gated DEVICEFUNCS).
- `sogen_d3d9_umd.def` — `LIBRARY sogen_d3d9um / EXPORTS OpenAdapter`.
- `d3d9_spike_test.cpp` — guest test: window → `Direct3DCreate9` → `GetAdapterDisplayMode` →
  `CheckDeviceType`/`CheckDeviceFormat` → `GetDeviceCaps` → `CreateDevice(HAL)`.
- Built binaries (`sogen_d3d9um-x64.dll`, `d3d9-spike-test-x64.exe`) are **not** committed — regenerable,
  see `README.md`'s exact mingw commands (also in §8).
- Caps are currently fixed-function (`VertexShaderVersion`/`PixelShaderVersion = 0`) — restoring
  `D3DVS/PS_VERSION(3, 0)` re-triggers d3d9's SM2.0+ HAL-disable gauntlet elsewhere (confirmed: even
  `GetDeviceCaps` starts failing). Open follow-up, not blocking.

> **Do NOT commit** the MS-copyrighted `d3dumddi.h` — `d3d9_ddi.hpp` is a clean hand-transcription and is
> fine to keep.

---

## 6. GATE 3 — resolved (2026-07-02)

**Symptom:** after our `pfnCreateDevice` returned S_OK, the top-level `IDirect3D9::CreateDevice(HAL,
windowed, X8R8G8B8 640x480, no auto-depth)` returned `0x8876086a` (D3DERR_NOTAVAILABLE) and tore the
device down, with **zero** DXGK/D3DKMT syscalls anywhere in the failure window — a strong early signal
that the failure was purely usermode.

**RE method that actually worked** (after several dead ends — see below): install a global, unaddressed
`hook_memory_execution` in the CreateDevice syscall window that only logs the **transition** into
`EAX == 0x8876086A` (not every instruction where the value merely persists — that produced dozens of
false positives from unrelated code reusing the same register). The first transition's return address,
cross-referenced against a fresh idasql analysis of the *exact staged* `d3d9.dll` (not a stale/assumed
address list), pointed at `InternalDirectDrawCreate`'s failure return (`v32 - 2005530518`, i.e. exactly
`0x8876086A` when `v32==0`).

**Root cause — a bug in our own UMD, not sogen's kernel.** Call chain:
`InternalDirectDrawCreate` → `D3D9CreateDirectDrawObject` → `CreateDeviceLHDDI` (the WDDM/"LongHorn"
driver-model path) → after our own `pfnCreateDevice` call returns S_OK, `ValidateUMDeviceFuncs` checks
the **negotiated `DriverVersion`** against WDDM thresholds (`>= 0x4002` WDDM1.3, `>= 0x6001` WDDM2.1) to
decide which `D3DDDI_DEVICEFUNCS` slots must be non-null. Our `OpenAdapter` was echoing the runtime's
offered `pArgs->Version` (observed as `0xe000`, far beyond WDDM2.1) straight back as `DriverVersion` —
so the runtime believed our device-func table extended to WDDM2.1+ slots (`pfnAcquireResource`/
`pfnReleaseResource`) that our `SOGEN_D3D9_UMD_INTERFACE_VERSION = WIN7`-sized `D3DDDI_DEVICEFUNCS`
struct doesn't even declare. It read uninitialized memory past our table, found null, and failed —
`ValidateUMDeviceFuncs` returns `0x80004005`, which propagates up through `CreateDeviceLHDDI` →
`D3D9CreateDirectDrawObject` → `InternalDirectDrawCreate`'s `return v32 - 2005530518` (`v32=0`) →
`0x8876086A` at the top level.

**Fix (`sogen_d3d9_umd.cpp`, `OpenAdapter`):**
```cpp
pArgs->DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION;  // was: pArgs->Version
```
One line. Validated: `CreateDevice hr=0x00000000`, `SUCCESS: IDirect3DDevice9 created`, stable across
repeated runs, smoke test still 26/26.

**Dead ends worth recording** (all addresses were *individually verified correct* via idasql — `funcs`
table exact match, xref confirmation from the real call site — yet none of these targeted
`hook_memory_execution(address, ...)` calls ever fired, for reasons still unexplained):
`NTStatusToHResult`, `CBaseDevice::Init`, `AllocateCB`, `CEnum::ValidateCreateDevice`,
`CEnum::ValidatePresentParameters`. The original mechanism hypothesis (NTSTATUS_PROBE →
`NTStatusToHResult(STATUS_ACCESS_DENIED)` → `CBaseDevice::Init`) from the pre-migration investigation was
**wrong** — none of those three functions execute on this path at all. Direct API calls
(`CheckDeviceType`/`CheckDeviceFormat`/`GetAdapterDisplayMode` from the guest test) all returned
`S_OK`, ruling out format/caps validation entirely and narrowing the search before the transition-scan
technique above found the real site. If precise `hook_memory_execution(address, ...)` targeting is
needed again, budget for this kind of dead end and prefer the transition-scan method from the start.

---

## 7. RE tooling & key addresses

- **idasql CLI (macOS):** `/Applications/IDA Professional 9.0.app/Contents/MacOS/idasql`. Recipe: copy
  the `.i64` (or the raw binary — idasql auto-analyzes it) to a private path (avoids IDA lock/sidecar
  clashes), then
  `idasql -s copy.i64 -q "SELECT decompile(0xADDR);"` (warm ~0.5s). Use `INSERT INTO funcs(address)
  VALUES(0xSTART); SELECT decompile(0xSTART);` to reconstruct functions across IDA analysis gaps.
- **32-bit MS d3d9 DB:** `~/.cache/sogen-symbols/d3d9_wow64.i64` (imagebase 0x10000000). This is the
  binary Stage-1/early Stage-2 RE used — but note the x64 test runs a *different* binary (below).
- **64-bit MS d3d9 DB (the one that actually runs in the x64 test):** `~/.cache/sogen-symbols/d3d9_x64.dll`
  + `.dll.i64` (regenerate by copying the staged `root/.../system32/d3d9.dll` there, then
  `idasql -s d3d9_x64.dll -w -q "SELECT COUNT(*) FROM funcs;"`, ~7s; imagebase `0x180000000`, 4579 funcs).
  Runtime→IDA map: d3d9 base = `0x104900000` (empirically re-confirmed 2026-07-02, deterministic — no
  ASLR jitter observed across runs), so `IDA = 0x180000000 + (runtime - 0x104900000)`.
- **Beware:** `root/.../syswow64/d3d9.dll` is **DXVK (7.3MB)**, not MS 32-bit d3d9 — a 32-bit spike would
  load DXVK, not our UMD path. The MS 64-bit `system32/d3d9.dll` (1.73MB) is the real target.
- **Lesson from the gate-3 investigation:** a `funcs`-table address that matches by name AND is
  cross-ref-confirmed as the real call target can still silently fail to fire via
  `hook_memory_execution(address, callback)` for unexplained reasons — this happened for 5 different,
  individually-verified addresses in a row. Don't sink time re-verifying the address is "really right";
  switch to a transition-scan (`hook_memory_execution(callback)` unaddressed, watching a register for the
  target value's *first write*, not every instruction it merely persists in) — it found the real site in
  one shot once used. `idasql`'s `bytes` table (`dword`/`qword` columns) is useful for checking whether a
  decompiled constant is a real stored value vs. compiler-folded arithmetic — cross-check against
  `instructions`/`instruction_operands` before trusting a raw byte-pattern match (unaligned mid-instruction
  coincidences are common and will outnumber real hits).
- Confirmed structural facts: d3d9's per-adapter "driver object" begins with a `D3DCAPS9` at offset 0
  (+extra driver fields after byte 304, indexed as `a1[N]` where each `N` is `sizeof(D3DCAPS9)` bytes).
  `memory/project_stage2_d3d9.md` (pre-migration notes) did not survive the migration (§3 in the original
  version of this doc) — the facts above are what was re-derived this session.
- **DDI arg-struct verification method (used for `D3DDDIARG_RENDERSTATE`, needed for the rest of §10's
  deferred list):** search `funcs` for names containing the target struct verbatim, e.g.
  `SELECT address, name FROM funcs WHERE name LIKE '%SetRenderState%'` turned up
  `?LHBatchSetRenderState@CBatchFilterI@@KAJPEAXPEBU_D3DDDIARG_RENDERSTATE@@@Z` — the mangled name
  itself names `_D3DDDIARG_RENDERSTATE` as the parameter type. Decompiling that function showed it
  copying exactly one QWORD out of `*pArg` into its internal batch buffer, confirming the struct is
  8 bytes (`{UINT State; UINT Value;}`) without needing the WDK header at all. Repeat per struct:
  `LIKE '%SetTexture%'`, `LIKE '%CreateResource%'`, `LIKE '%Lock%'`, `LIKE '%Present%'`, etc., then read
  what the decompiled body actually does with the pointer (field-by-field offsets, copied byte counts)
  rather than trusting the struct *name* alone.

---

## 8. Build / run / staging reference

- **Build emulator:** `cmake --build --preset=release` from the repo root (artifacts in
  `build/release/artifacts/`). **`root/` lives at `build/release/artifacts/root/`** — not a repo-top
  `root/` — the real 64-bit `system32/d3d9.dll`, the spike test, and `sogen_d3d9um.dll` are all staged
  there already.
- **Run the D3D9 spike:** from `build/release/artifacts/`:
  `./analyzer -e root -c c:/d3d9-spike-test.exe` — capture output; grep `[d3d9-spike]`,
  `[sogen-d3d9-umd]`, `[diag]`/`[NTSTATUS_PROBE]` if those toggles are on.
- **Smoke test:** `./analyzer -e root -s c:/test-sample.exe` from `build/release/artifacts/` (needs both
  `-e root` AND the absolute guest path — a bare relative filename errors "Only absolute paths can be
  translated"). 26/26 `Success` lines, no `fail`/`error` outside `[NTSTATUS_PROBE]` noise if that toggle
  is on.
- **Analyzer run rules:** ALWAYS foreground, never `run_in_background`; use the Bash tool's own timeout
  parameter instead of shell `timeout` (no GNU coreutils `timeout` on macOS by default); capture+read
  output yourself. `-e root` (relative to cwd) is required — there's no default; without it, `-r`/registry
  also defaults relative to cwd, not to `-e`, so both need to point at `root/` explicitly.
- **Guest toolchain (builds the UMD / ICD / test EXEs):** mingw-w64 — **installed** via
  `brew install mingw-w64` (both `x86_64-w64-mingw32-g++` and `i686-w64-mingw32-g++` confirmed working).
  Exact commands in `src/samples/sogen-d3d9-umd/README.md`.
  - UMD (x64): `x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 sogen_d3d9_umd.cpp sogen_d3d9_umd.def
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll`
  - Test (x64): `x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_spike_test.cpp -static -static-libgcc
    -static-libstdc++ -o d3d9-spike-test-x64.exe -ld3d9`
  - Stage: UMD → `build/release/artifacts/root/filesys/c/windows/system32/sogen_d3d9um.dll`; test →
    `build/release/artifacts/root/filesys/c/d3d9-spike-test.exe`.
  - x86/WoW64 UMD (later): `i686-w64-mingw32-g++ ... -Wl,--kill-at ...` (undecorated `__stdcall` exports).

---

## 9. macOS / Apple Silicon port strategy — DONE (2026-07-02)

**Two axes — kept separate, both resolved:**
- **CPU emulation:** no KVM on Apple Silicon (KVM is Linux+x86 only; no hardware x86-on-ARM virt exists).
  sogen defaults to **unicorn** (software) — builds and runs on macOS. Cost is **speed** (~10–50× slower
  than KVM); this is the real constraint for MW2-scale work, not graphics. `icicle` (Cranelift JIT,
  `EMULATOR_ICICLE=1`) is available in-tree as a faster alternative backend if/when speed becomes the
  bottleneck — not needed yet, gate 3 doesn't care about CPU speed.
- **Graphics: MoltenVK confirmed working end-to-end.** `vulkan-loader` + `molten-vk` + `vulkan-tools`
  installed via Homebrew; `vulkan_host` fixed to negotiate `VK_KHR_portability_enumeration`/
  `VK_KHR_portability_subset` and to find the loader on Apple Silicon's `/opt/homebrew/lib`. Verified with
  a real clear+present pipeline through `native-gpu-clear-sample`, pixel-correct, on the M5 Pro GPU. No
  MoltenVK feature gaps hit so far. Details: `memory/project_moltenvk_wow64_dxgk.md`.

**Do NOT run an x86 Linux VM on the Mac** — it's emulated (QEMU TCG) and KVM won't work inside it →
double-slow, loses the fast path. For the fast path (KVM + real GPU Vulkan) at MW2 scale later, use a
*real* x86 Linux box (physical or cloud) — not needed for gate 3.

No architectural rewrite was needed — the guest stayed frozen throughout.

---

## 10. STAGE 2 PART 2 — D3D9 DDI → sogen command stream (2026-07-02, in progress)

Following the approved plan (`.claude/plans/scalable-giggling-fern.md` Part 2 — also restored into this
repo's `.claude/plans/` this session, since `.claude/` wasn't gitignored before and the original copy
only survived under the migration leftover path `~/old-claude/.claude/plans/`).

**Done, compiles clean, smoke-test + gate-3 spike still green:**
- **`src/d3d9-command-protocol/d3d9_command_protocol.hpp`** (+ CMakeLists, wired into
  `src/CMakeLists.txt`): dependency-free wire protocol for sync commands (marker, create/destroy
  resource, lock/unlock, create vertex/pixel shader, create vertex decl) and streamed per-draw records
  (render state, texture stage state, sampler state, texture/stream/index/decl/shader binds, VS/PS
  float constants, render target/depth-stencil, viewport, scissor, clear, draw (indexed) primitive).
  `#include`s `gpu_bridge_protocol.hpp` for the shared `object_id`/`escape_command_header` transport
  types rather than duplicating them — matches the plan's explicit "transport reuse, zero new gdi.cpp
  Escape code" instruction. Every struct has a `static_assert` size pin.
- **Transport wiring**, zero new `gdi.cpp` code as planned: a `0x900-0x9FF` D3D9 opcode block added to
  `gpu_bridge::command` (`gpu_bridge_protocol.hpp`); `gpu_command_processor::dispatch()`
  (`devices/gpu_bridge.cpp`) routes the 7 sync opcodes to individual handlers and the ~18 streamed
  opcodes to one shared `handle_d3d9_streamed` forwarder; `execute_recorded_command`'s `default:` case
  also forwards the same streamed range, so a future batched `ioctl_record_commands` replay path (not
  yet built — see below) will work with zero d3d9_host changes.
- **`src/windows-emulator/devices/d3d9_host.{hpp,cpp}`**: host-side decoder, owned by
  `gpu_command_processor` alongside `vulkan_host` (same "no emulated-Windows types" rule as
  `vulkan_host.hpp` states explicitly). Real resource lifetime (host-side shadow-copy backing, not yet
  real GPU images — that's Part 3) and full per-device render/sampler/texture-stage-state,
  stream/index/decl/shader-binding, and VS/PS float-constant tracking. `execute_recorded` parses and
  stores every streamed opcode; `set_viewport`/`set_scissor`/`clear`/`draw_*` currently
  parse-validate-and-no-op (real draw execution against `vulkan_host` is Part 3's pipeline builder).
- **UMD guest side** (`src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp`): `bridge_call`/`ensure_adapter`
  copied from `vulkan_shim.cpp`'s proven pattern (D3DKMT Escape carrying
  `[escape_command_header][in][out]`, adapter opened via `NtGdiDdDDIOpenAdapterFromLuid` with the fixed
  LUID `{0x1000,0}`). **20 real DDI marshaling functions** now back their `D3DDDI_DEVICEFUNCS` slots
  (indices confirmed against the struct's own field order in `d3d9_ddi.hpp`): `pfnSetRenderState`(0),
  `pfnSetTextureStageState`(3), `pfnSetTexture`(4), `pfnSetPixelShader`(5), `pfnSetPixelShaderConst`(6),
  `pfnSetIndices`(8), `pfnDrawPrimitive`(10), `pfnDrawIndexedPrimitive`(11), `pfnClear`(21),
  `pfnSetVertexShaderConst`(24), `pfnSetViewport`(27), `pfnSetZRange`(28), `pfnFlush`(41),
  `pfnSetVertexShaderFunc`(44), `pfnSetVertexShaderDecl`(47), `pfnSetScissorRect`(50),
  `pfnSetStreamSource`(51), `pfnSetStreamSourceFreq`(52), `pfnSetRenderTarget`(62),
  `pfnSetDepthStencil`(63). Everything else stays on the original generic `device_stub` (S_OK, no
  marshaling) — see the follow-up list below for why.
- **`D3DDDIARG_RENDERSTATE`'s `{State,Value}` shape is RE-verified**, not guessed, against the actual
  staged `d3d9.dll` (`CBatchFilterI::LHBatchSetRenderState` copies exactly one QWORD out of `*pArg`,
  confirming an 8-byte `{UINT,UINT}` struct) — see §7 for the idasql method. The other structs in
  `d3d9_ddi.hpp` follow the same well-established WDK `(HANDLE, CONST D3DDDIARG_X*)` Set-family
  convention but were **not** individually RE-verified the same way — if M1 hits a garbled-state bug,
  suspect one of these first and RE-verify it the same way `RENDERSTATE` was.
- **`D3DDDIARG_LOCK`/`D3DDDIARG_UNLOCK` are RE-verified and implemented** (`pfnLock`(35)/`pfnUnlock`(36)
  now marshal for real, added in a follow-up pass on 2026-07-02). Found via `CDriverVertexBuffer::Lock`/
  `::Unlock` in the real `d3d9.dll` (search `funcs` for `%Lock@CDriverVertexBuffer%` — a different,
  more reliable anchor than the `LHBatch*` passthroughs, which just forward the struct pointer without
  touching fields for Lock/Unlock/CreateResource). Confirmed: `D3DDDIARG_LOCK` is exactly 104 bytes with
  `hResource`@0, `OffsetToLock`@16, `SizeToLock`@20, output `pData`@80, `Flags`@96 (a ~60-byte
  region across two gaps stays unconfirmed — zero for buffer locks, likely SubResource/Box/Pitch fields
  for texture locks that aren't needed yet); `D3DDDIARG_UNLOCK` is exactly 16 bytes,
  `{hResource, Reserved}`. The UMD's `umd_Lock` keeps a persistent per-resource heap buffer (keyed by
  the wire `resource_id`) since `pData` must stay valid until the matching `Unlock` — see
  `g_locked_buffers` in `sogen_d3d9_umd.cpp`.

**✅ RESOLVED 2026-07-02 — the D3DHAL_DP2COMMAND token-stream question.** Full trace:
`CD3DDDIDX9::CreateVertexShaderDecl`/`CD3DDDIDX6::SetRenderState`'s "fast path" write tagged records
(`D3DDP2OP_*`-style: `{tag; ...payload}`) into an internal per-device "HAL buffer" via
`CD3DDDIDX6::GetHalBufferPointer` / `CBatchFilterI::LHBatchXxx`, **not** a synchronous `pfnXxx(HANDLE,
ARG*)` call — confirmed for `LHBatchDrawPrimitive2` (writes `{tag=28, ...12 bytes}`) and
`CreateVertexShaderDecl` (writes a `D3DDP2OP_CREATEVERTEXSHADERDECL` record with a **locally-assigned**
handle via `CHandleFactory::CreateNewHandle` — the driver never generates this handle). The buffer fills
up on the app thread, then `CBatchFilterI::LHBatchXxx` calls `SubmitBatchToWorkerThread` /
`FlushBatchWorkerThread`, which wake a **background worker thread** (`CBatchFilterI::LHBatchWorkerThread`)
that calls `CBatchFilterI::ProcessBatch(this, pBatchBuffer, isWorkerThread)`.

**`ProcessBatch` is the answer.** It's a big tag-dispatch loop (`switch` on the 1-byte/DWORD tag at the
head of each record) that, for every tag, calls `(*((pfnptr**)this + N))(*((QWORD*)this + 14),
recordPayloadPtr, ...)` — i.e. a function pointer read from a **fixed numeric slot embedded in the
`CBatchFilterI` object itself** (a runtime-side cached copy of the driver's `D3DDDI_DEVICEFUNCS` table,
populated once at `CreateDevice` time), called with `(hDevice, pArgs)` — **the exact same DDI calling
convention as every other `pfnXxx` slot.** Confirmed concretely for two tags:
- **tag 28 (`DrawPrimitive2`)** → object-slot **32**, `(hDevice, pArgs)`, 12-byte payload — matches
  `LHBatchDrawPrimitive2`'s write exactly (16-byte record = 4-byte tag + 12-byte payload).
- **tag 29 (`CreateVertexShaderDecl`)** → object-slot **26**, `(hDevice, pArgs)`.

**Conclusion: DP2 batching is a d3d9.dll-internal fast-path optimization (defer + coalesce state/draw
calls onto a worker thread to reduce per-call dispatch overhead), not an alternate wire protocol our UMD
needs to speak.** Every DP2-tagged record still bottoms out in a call to the *same*
`D3DDDI_DEVICEFUNCS` pfn slot a direct call would have used — our UMD's `pfnXxx` exports are still the
complete and correct API surface to implement. The only real implications for us: (1) our `pfnXxx`
exports may be invoked from a **different thread** than the app's main thread (the batch worker thread)
— existing wire marshaling code has no per-thread state so this is fine as-is; (2) for
`pfnCreateVertexShaderDecl`/`pfnCreateVertexShaderFunc`, the **HANDLE is pre-assigned by the runtime**
before the driver is called (via `CHandleFactory::CreateNewHandle`), not returned by the driver — our
`umd_CreateVertexShaderDecl`-style marshaling must treat the handle as an *input* to echo/accept, not an
output to synthesize, when this gets wired for real. The 20 already-wired `D3DDDI_DEVICEFUNCS`
marshaling functions (§10 above) are the right target; no DP2 token parser is needed on our side.

**✅ `pfnCreateResource` IS called for the backbuffer/swapchain surfaces — confirmed 2026-07-02 via a
real `d3d9-triangle-test` forcing function** (see `.claude/plans/jazzy-giggling-cloud.md` Phase 1; the
prior "empirically doesn't get called" note above was correct only for plain vertex/index buffers, not
render-target surfaces). A real `IDirect3DDevice9::CreateDevice()` call — before it even returns —
issues **5 synchronous, non-batched `pfnCreateResource` calls** (confirmed via `CBatchFilterI::
LHBatchCreateResource`'s decompile: it's a direct passthrough, `(hDevice, pArgs)`, not routed through
the DP2 token buffer). Captured via a temporary `NtGdiDdDDICreateAllocation` hex-dump + a diagnostic
UMD stub on slot 37 (both since reverted — see the plan for the exact instrumentation if this needs
re-capturing). Raw payload evidence (first call, distinct from the other 4):
`16 00 00 00 03 00 00 00 00 00 00 00 00 00 00 00 [8-byte ptr] 01 00 00 00 [20 zero bytes] [8-byte ptr]
81 10 00 00 01 00 00 00` — offset 0 = `0x16` = **22 = `D3DFMT_X8R8G8B8`, matching the test's
`BackBufferFormat` exactly** (strong, non-coincidental evidence offset 0 is `Format`). The other 4 calls
share an identical prefix (`offset 0 = 0x64`, `offset 4 = 1`) differing only in a trailing pointer +
2 bytes — likely a 4-entry mip/surface array for a second resource, not yet explained. **Not yet
individually field-verified** (`D3DDDIARG_CREATERESOURCE` is NOT drafted in `d3d9_ddi.hpp` yet) — the
next RE step is finding the actual *builder* of these args (not the passthrough `LHBatchCreateResource`,
which reveals nothing — its 3 callers found via xref were `StartThreading` (just wires the vtable slot,
not a builder) and two addresses with no enclosing `funcs` entry, suggesting a stripped/local builder
function; needs a different search angle, e.g. tracing from `CBaseDevice::Init`/swapchain setup).
- `pfnOpenResource`, `pfnBlt`, `pfnColorFill`: still deferred, not exercised by this forcing function.
- `pfnCreateVertexShaderFunc`, `pfnCreatePixelShader`, `pfnCreateVertexShaderDecl`,
  `pfnSetVertexShaderFunc`(44), `pfnSetVertexShaderDecl`(47), `pfnDeleteVertexShaderFunc`/
  `DeletePixelShader`: **do still go through `D3DDDI_DEVICEFUNCS`** (per the DP2 resolution above) — just
  invoked asynchronously from the batch worker thread instead of synchronously at the D3D9 API call site,
  and for `CreateVertexShaderDecl`/`CreateVertexShaderFunc` the HANDLE arrives as an **input** (assigned
  by `CHandleFactory`), not an output. The drafted structs in `d3d9_ddi.hpp` are still the right shape to
  implement against; not yet RE-verified byte-for-byte the way `RENDERSTATE`/`LOCK`/`UNLOCK` were.
- `pfnPresent`: **size corrected + first field confirmed 2026-07-02** via `CBatchFilterI::
  LHBatchPresent`'s decompile (not the earlier `GetBatchBufferPointer` allocation-size method, which
  conflated the DP2 token's 4-byte tag header with the struct itself). The real struct is **40 bytes**,
  not 44: `LHBatchPresent` copies exactly one OWORD (offset 0-15) + one OWORD (16-31) + one QWORD
  (32-39) into the batch token. `hSrcResource` is confirmed at offset 0 (`*(void**)a2` is passed
  straight to `CBatchFilterI::ReferenceResource` as a HANDLE) — matches classic D3D9 DDI convention. A
  flags-like byte at offset 28 is tested for bit `0x4` by the runtime before choosing the batched vs.
  immediate-dispatch path. Fields beyond `hSrcResource` are still unconfirmed (`d3d9_ddi.hpp` reflects
  this: `HANDLE hSrcResource; BYTE Reserved[32];`, 40 bytes total). Still not wired to any device-func
  slot. **✅ A real windowed `Present()` now completes end-to-end (2026-07-02)** — it was blocked by an
  unrelated, genuinely unimplemented syscall, `NtUserHwndQueryRedirectionInfo` (a DWM/compositor
  redirection-info query), deep inside `d3d9.dll`'s pre-flight window-state check. Added a minimal
  permanent stub (`handle_NtUserHwndQueryRedirectionInfo`, `syscalls/user.cpp`) that always reports
  "not redirected" (`FALSE`); real args beyond `hwnd` are unread (signature is undocumented). This makes
  `d3d9.dll` fall back to its legacy GDI blit-to-window-DC present path — confirmed by re-running with
  `NtGdiDdDDICreateAllocation` hex-dump logging enabled: **it is never called** for the backbuffer in
  this path, even after the stub unblocks execution. This means the "swapchain surface needs a real
  D3DKMT kernel allocation" assumption doesn't hold for a bare windowed `Clear`+`Present` — `d3d9.dll`
  earlier calls `AllocateCB` → a global OS-thunk function pointer
  (`pfnOsThunkDDICreateAllocation`/`...2`, not a driver-supplied device callback) that *would* reach
  `NtGdiDdDDICreateAllocation`, but that path isn't exercised here. Practical implication for Part 3:
  **our own `pfnCreateResource`/`pfnPresent` are what need to produce real pixels** — the real d3d9.dll
  won't hand us a kernel-backed surface for free via this path; don't build Part 3 around waiting for a
  `NtGdiDdDDICreateAllocation` call that may never come for the common windowed case.
- **Bonus fix, found by the same forcing function:** `D3DDDIARG_CLEAR` had a real bug —
  `umd_Clear`/the struct definition assumed `NumRect` and the rect array were struct fields (trailing
  inline data), but RE via `CBatchFilterI::LHBatchClear`'s decompile (`this, pClear, NumRect, pRect` —
  4 separate parameters, `pClear` copied as exactly one OWORD/16 bytes) showed `pfnClear`'s real
  signature is `(HANDLE, CONST D3DDDIARG_CLEAR*, UINT NumRect, CONST RECT*)`. The old assumption caused
  a real crash (`umd_Clear` walking off the end of a heap allocation reading a garbage `NumRect`) the
  first time a real `Clear()` call was exercised — gate 3's spike test never called `Clear` either, so
  this was undiscovered until now. Fixed: `D3DDDIARG_CLEAR` is 16 bytes (`Flags,Color,Z,Stencil` only),
  `umd_Clear` takes `NumRect`/`pRect` as separate parameters.
- `pfnDrawPrimitive2`/`pfnDrawIndexedPrimitive2` (the `*UM`/inline-vertex-data variants) — now the
  **prime suspect** for where DP2 token batches actually get submitted; investigate these BEFORE
  `pfnSetStreamSourceUm`/`pfnSetIndicesUm`.
- `pfnSetSamplerState`: **no separate slot exists in `D3DDDI_DEVICEFUNCS` at all** — confirmed by
  re-reading the struct; D3D9's real DDI folds sampler state into `pfnSetTextureStageState` via extended
  `State` values instead. The wire opcode/struct (`d3d9_set_sampler_state`) is defined and harmless but
  unused — figure out the real TSS/sampler-state boundary before wiring it to anything.
- `pfnSetVertexShaderConstI/B`, `pfnSetPixelShaderConstI/B`: lower priority for a first triangle (which
  only needs float constants); wire opcodes intentionally not even added for these yet.
- **Batched/recorded streamed commands** (sogen's own transport, not to be confused with the D3D9
  runtime's internal DP2 buffer above): currently every streamed DDI call is sent as its own individual
  sync Escape (simpler to get right first), not accumulated into a `command_record_header` batch and
  flushed via `ioctl_record_commands` the way the plan describes for performance. The host side
  (`execute_recorded_command`'s default case) already forwards that path to `d3d9_host` too, so adding
  batching later is a guest-side-only change with no host/wire-format impact.

---

## 10.5. De-risk slice — real GPU clear + readback wired end-to-end (2026-07-02)

See `.claude/plans/jazzy-giggling-cloud.md` for the full plan; this is the outcome summary.

**✅ `d3d9_host` now owns real GPU backing.** Constructor takes a `vulkan_host&` (the same instance
`gpu_command_processor` already owns as a sibling member — no second GPU connection, no cross-file
signature threading needed, since `d3d9_host` and `vulkan_host` live in the same struct). It lazily
creates a bare Vulkan instance + device on first render-target-kind resource creation
(`d3d9_host::ensure_vk_device()`, mirroring `handle_NtGdiDdDDICreateDevice`'s own lazy-init pattern).
`create_resource` calls `vulkan_host::create_render_target` for `texture_2d` resources with the
`D3DUSAGE_RENDERTARGET`/`DEPTHSTENCIL` usage bits set (public D3D9 constants, not RE'd), storing the
resulting `vk_image_id` on the `resource_entry`. `execute_recorded`'s `d3d9_clear` case does a **real**
`vulkan_host::submit_clear` + `readback_render_target`, writing the result into the resource's
`backing` (the same buffer `pfnLock` already hands back to the app) — **verified working end-to-end**:
clearing to `D3DCOLOR_XRGB(64,128,255)` produces `backing[0..3] == [FF 80 40 FF]` (BGRA8), exactly
correct, confirmed via two independent resource paths (the implicit backbuffer and an explicit
`CreateRenderTarget` surface).

**✅ `pfnCreateResource`'s output field is RE-verified: offset 48.** Found via a sentinel-scan (not
guessing): write a distinct, identifiable value to every 8-byte-aligned offset in the args, then check
which one comes back unchanged in the very next `SetRenderTarget` call —
`hRenderTarget=0xAAAA000000000030` (offset `0x30` = 48) landed exactly. This directly refuted two
earlier single-offset guesses (40, then 44) that each looked plausible in a static hex dump but didn't
hold up live once `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` was added — a good example of why a sentinel
scan beats guessing one offset at a time. `umd_CreateResource` (`sogen_d3d9_umd.cpp`) now writes the
wire `resource_id` to offset 48 for real; the runtime echoes it back unchanged in
`SetRenderTarget`/`Lock`/`Present`, so `resolve_resource_id()`'s lazy-bind-at-first-use logic is now
mostly dead-code fallback (kept as a defensive safety net). Width/height/format are still a fixed guess
(640×480, `D3DFMT_X8R8G8B8`) — wrong in general until the rest of `D3DDDIARG_CREATERESOURCE`'s layout
is found (the class-hierarchy/xref search that worked for `RENDERSTATE`/`LOCK`/`CLEAR`/`PRESENT`
doesn't turn up a builder function for this struct — see §11).

**⚠️ Known gap, now with more evidence: `LockRect()` never invokes `pfnLock` at all**, on the implicit
backbuffer, an explicit `CreateRenderTarget` surface, *or a plain vertex buffer* — confirmed via
DXGK/gpu-bridge tracing (no `ioctl_d3d9_lock` op ever appears) despite `LockRect`/`Lock` themselves
often returning `S_OK`. Two things ruled out: (1) it's **not** about driver backing correctness — even
after the offset-48 fix gave the resource a real, correctly-identified GPU-backed handle, `LockRect`
still never reaches the driver; (2) it's **not** about the surface being the active render target —
explicitly unbinding it first (`SetRenderTarget` back to the original backbuffer) before locking didn't
change the outcome either. One genuinely new data point: `IDirect3DVertexBuffer9::Lock()` on a plain
`D3DPOOL_DEFAULT` vertex buffer **does** return a real, non-null pointer (`data=0x103be1ce0` in one
run) — but *also* without ever calling `pfnLock` (no `ioctl_d3d9_lock` in the trace either). This means
the runtime is satisfying **all** Lock calls from its own memory, entirely independent of whether the
driver "really" created anything — vertex buffers get valid data because the runtime just hands back
its own linear-memory pointer; render targets get `NULL` because (unconfirmed) they're conceptually
video-memory-only and the runtime has no equivalent fallback for them. **Not yet resolved.** Next
investigation ideas: check `fill_d3d9caps` for a missing lockable-render-target capability bit the
runtime might gate on before ever considering a driver call; or accept this as a structural limit of
running against the real HAL runtime without genuine WDDM kernel cooperation, and rely on host-side
diagnostics (proven reliable twice) for verification going forward instead.

---

## 10.6. Part 3 — the real pipeline builder is implemented, not yet exercised end-to-end (2026-07-02)

`d3d9_host::execute_recorded`'s `d3d9_draw_primitive` case now does real work
(`ensure_draw_infra`/`ensure_pipeline`/`execute_draw` in `d3d9_host.cpp`), matching the plan's Part 3
scope:
- **The one hardcoded fixed-function shader pair** for `D3DFVF_XYZRHW|D3DFVF_DIFFUSE` (pre-transformed
  screen-space position + per-vertex diffuse color) — GLSL source in `src/windows-emulator/devices/shaders/ff_triangle.
  {vert,frag}`, compiled with `glslangValidator` (available via `brew install glslang`; `glslc` is not
  installed on this machine), embedded as `constexpr std::array<uint32_t,...>` SPIR-V in `d3d9_host.cpp`.
  This is the correct, permanent implementation for this one fixed-function case (Vulkan has no true
  fixed-function pipeline either way) — not a stand-in for missing vkd3d-shader translation. General
  FVF/render-state shader synthesis remains the separate, future M4 milestone.
- **A cached pipeline** (`ensure_pipeline`): shader modules, a pipeline layout (one push-constant range
  for `vec2 viewportSize`, no descriptor sets needed), vertex bindings/attributes for the 20-byte FVF
  stride (`VK_FORMAT_R32G32B32A32_SFLOAT` position + `VK_FORMAT_B8G8R8A8_UNORM` diffuse), dynamic
  viewport/scissor, `render_pass=0` (dynamic rendering, per the plan's explicit instruction).
- **Real per-draw work** (`execute_draw`): uploads the current vertex buffer's backing bytes fresh
  every draw (simplest correct model for a first triangle, no persistent GPU vertex buffer / dirty
  tracking yet), records explicit `cmd_pipeline_barrier` layout transitions (`TRANSFER_SRC_OPTIMAL` ↔
  `COLOR_ATTACHMENT_OPTIMAL`, since `submit_clear`/`readback_render_target` leave the image in
  `TRANSFER_SRC_OPTIMAL` — this assumes Clear always runs before the first Draw, true for the current
  test flow but not enforced), binds/draws, then reads the result back into the render target's
  `backing` the same way `pfnClear` already does.
- `d3d9_host.cpp` now `#include <vulkan/vulkan_core.h>` directly (linked via the existing
  `vulkan-headers` target) for the real, stable public `VK_*` enum values — mirroring `vulkan_host.cpp`'s
  own precedent, not a new architectural decision; `d3d9_host.hpp` itself stays plain-integer per its
  existing "no Vulkan types in the header" rule.
- **Builds cleanly** (`cmake --build --preset=release`), no smoke-test regression.

**Update (2026-07-02, later same day): exercised end-to-end, pipeline builder proven correct.** The
`D3DERR_INVALIDCALL` above was **not** a driver/runtime issue — the triangle-draw test code itself
called `EndScene()` after the first (backbuffer) `Clear()` and never called `BeginScene()` again before
the render-target draw sequence, a plain D3D9 API misuse (`DrawPrimitive` is only valid inside a
scene). Self-found via careful reading of the diagnostic call sequence; fixed by adding a fresh
`BeginScene()`/`EndScene()` pair around the render-target draw.

That fix uncovered two real DDI marshaling bugs, both found via **crash-driven RE**: run the guest test,
capture the emulator's own `Mapping violation: <addr> (<size>) - r-- at <RIP> (sogen_d3d9um.dll)`
crash report, then `x86_64-w64-mingw32-objdump -d --start-address=<X> --stop-address=<Y>
sogen_d3d9um-x64.dll` (mingw's export table resolves function symbols automatically) to see exactly
which instruction faulted:
1. **NULL `pArgs` crashes.** `umd_SetVertexShaderDecl` (and, once guarded, several other `umd_*`
   marshaling functions) dereferenced `pArgs` unconditionally; the runtime legitimately passes `pArgs =
   NULL` for several DDI calls to mean "unbind / use fixed-function" (e.g. `SetVertexShaderDecl(NULL)`
   when a `D3DFVF_XYZRHW` draw follows a shader-bound one). Fixed by adding `if (pArgs == nullptr)
   { return S_OK; }` guards (or NULL-safe ternaries where a real "unbind" wire message still needs to
   go out) across essentially every `umd_*` function in `sogen_d3d9_umd.cpp`.
2. **`pfnSetTexture`'s real signature.** After the NULL guards, `umd_SetTexture` kept crashing —
   but now with `pArgs = 0x1` (not NULL), i.e. a *valid* small integer being read as a pointer. Root
   cause: `pfnSetTexture` is **not** `(HANDLE hDevice, CONST D3DDDIARG_SETTEXTURE* pArgs)` — it's the
   classic direct-value WDK form `(HANDLE hDevice, UINT Stage, HANDLE hTexture)`. RDX held `Stage` (0 or
   1), not a struct pointer. Fixed by changing `umd_SetTexture`'s C++ signature to match and building
   the wire record straight from the two value arguments — no struct, no NULL check needed.

With both fixed, `DrawPrimitive()` returns `S_OK` and the full sequence
(`CreateRenderTarget`→`SetRenderTarget`→`BeginScene`→`Clear`→`CreateVertexBuffer`→`Lock`/write/`Unlock`→
`SetFVF`→`SetStreamSource`→`DrawPrimitive`→`EndScene`) runs with **no crash and no DDI rejection**.

**But the triangle doesn't render — and that's a *different*, deeper, RE-confirmed gap, not a pipeline
bug.** A host-side diagnostic (temporary, since removed) sampling the readback pixel at the triangle's
centroid showed the *clear color*, not a blended triangle color. Tracing why: `d3d9_host`'s vertex
buffer resource lookup (`this->state_.stream_sources[0]`) uses the DDI-level `hVertexBuffer` handle the
runtime passes to `SetStreamSource` — but that handle is **not** one our driver ever created (confirmed:
`CreateVertexBuffer` still never calls `pfnCreateResource`, exactly as §10 already found for the
backbuffer-only case). It's a small sequential value from the *runtime's own internal* handle space
(observed: `9`), which coincidentally collided with one of our own sequentially-numbered fake
render-target resources (created by `resolve_resource_id`'s lazy-bind fallback) — so the draw was
silently reading 1.2MB of zeroed texture backing as "vertex data", producing a degenerate (zero-area)
triangle. **Fixed the collision specifically**: `execute_draw` now checks the found resource's `kind`
is actually `vertex_buffer`/`index_buffer` before trusting it, so an accidental id collision cleanly
no-ops the draw instead of reading garbage from an unrelated resource.

The *real* underlying gap — why no genuine vertex data ever reaches the driver at all — was RE'd via
idasql (`?Lock@CDriverVertexBuffer@@...`, `?LockVB@CD3DDDIDX6@@...`) down to a concrete mechanism:
`CDriverVertexBuffer::Lock` branches on a "hal level" field read from the device object
(`device[+72] < 10` in the decompile); when true (our case), it takes a **cached-system-memory fast
path** — `app Lock() → cached pointer + offset`, entirely inside d3d9.dll, **never calling `pfnLock` at
all**. This is not the same bug as the previously-documented "`LockRect` never calls `pfnLock`" gap in
§10.5 — it's the same root mechanism, but now confirmed (via decompiled source, not just live
observation) to also block **writing** app-authored data into any driver-visible location, not just
**reading** it back. Getting real vertex/index data to the driver under our current negotiated DDI tier
needs either negotiating a higher WDDM DDI interface level (a substantially larger change — different
device-funcs table, possibly different struct layouts) or finding what specifically flips that `< 10`
check for our driver; neither is solved yet.

**The pipeline builder itself is proven correct**, isolated from that gap: with known-good vertex bytes
(red/green/blue triangle, same coordinates the real test uses) substituted directly into `execute_draw`
as a temporary diagnostic, the readback at the triangle's centroid `(320,280)` came back `B=0x55 G=0x56
R=0x54` — the exact expected barycentric average of the three vertex colors (255/3 ≈ 0x55 per channel,
matching to within readback rounding) — while a corner pixel `(10,10)` outside the triangle still read
the clear color. Vertex fetch, the embedded shader pair's NDC transform, rasterization, per-vertex color
interpolation, and the GPU→host readback are all byte-exact correct. This satisfies this plan's Phase 4
success criterion (analytic host-side pixel verification of a real GPU-rendered triangle) **for the
render pipeline**; the substitution was removed after confirming this, so the current committed state
correctly no-ops on real (still-unreachable) vertex data rather than pretending it works.

**Deeper RE pass on the vertex-delivery gap (2026-07-02, same day, no fix yet).** Went looking for a
surgical caps-bit fix (mirroring the offset-48 sentinel-scan precedent) instead of the "renegotiate a
higher DDI tier" heavy option. Traced the real decision tree in `CVertexBuffer::Create` (idasql
decompile): the pool argument gets remapped through several device-cached flag checks before deciding
between `CreateDriverVertexBuffer` (real, driver-backed — what we want for `D3DPOOL_DEFAULT`),
`CreateDriverManagedVertexBuffer` (needs `CBaseDevice::CanDriverManageResource()`, which directly
checks `Caps2 & D3DCAPS2_CANMANAGERESOURCE`), and `CreateSysmemVertexBuffer` (pure system memory, no
driver call ever). The routing depends on several *internal, device-cached* flag fields
(`device+120`, `device+444`, `device+460`) whose provenance — which of our reported `D3DCAPS9` fields
they're derived from, and by what transformation — could not be pinned down via static decompilation
alone; the function(s) that populate them from raw `GetDeviceCaps()` output weren't found by name-based
search (`CBaseDevice::Init`, `CBaseDevice::GetDeviceCaps` itself don't write them — they must be set in
a caps-processing step not yet located).

Two hypotheses tested empirically, both **ruled out**:
- Adding `D3DCAPS2_CANMANAGERESOURCE` to `Caps2` alone: rebuilt, reran, `pfnCreateResource` still never
  fires for `CreateVertexBuffer` (confirmed via a temporary spike log, since removed). Reverted.
- `pfnDrawPrimitive2` (slot 14, not slot 32 as an earlier note in this doc guessed — recounted precisely
  against `d3d9_ddi.hpp`'s field order and cross-checked against already-wired slot numbers) carrying
  vertex data inline via DP2 batching instead of going through Lock at all: wired a temporary logging
  probe to slot 14, reran the full triangle test — it never fires. Ruled out; the single `DrawPrimitive`
  call in this test goes through the plain `pfnDrawPrimitive`/`pfnSetStreamSource` slots we already
  have, not a DP2-batched path. Reverted.

Neither of the fast, surgical options panned out. Also tried option (b) directly, empirically rather
than by further static analysis: rebuilt the UMD with `SOGEN_D3D9_UMD_INTERFACE_VERSION` bumped to
`SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM1_3` (via `-D`, not committed) — the additive struct/table changes
this unlocks in `d3d9_ddi.hpp` are all `#if`-gated extra fields defaulting to `device_stub`, so this
looked like it should be a safe, purely-additive experiment. It's not: `CreateDevice` itself starts
failing with `D3DERR_INVALIDCALL` (`0x8876086a`), before any resource/draw code even runs. Something
about how the runtime validates or marshals args at this tier differs beyond what our simple `#if`-gated
field additions account for (a genuinely bigger change than the struct diffs alone suggest — possibly
additional `GetCaps` query types, a different negotiation sequence, or caps fields we don't populate
that this tier newly requires). Reverted immediately; not investigated further this session.

**Update (2026-07-02, later same day): live debugging found and fixed the real root cause.** sogen has
its own built-in debugger — a GDB remote-serial-protocol stub (`-d --port`, works with real IDA
Pro/GDB) **and** a nanobind Python scripting API (`import sogen`) exposing `hook_memory_execution`/
`hook_memory_write`/register and memory read/write against the *live* emulator, both documented in
`docs/debugger/ARCHITECTURE.md`. Neither had been used yet this session — all RE up to this point was
static idasql decompilation. Built it for a scratch dir (`cmake --preset release -B build/release-py
-DSOGEN_ENABLE_PYTHON_BINDINGS=ON`, `cmake --build build/release-py --target sogen`) and used it to
trace `d3d9.dll`'s live execution against RVAs pulled from the same idasql database used all session.

Hooking `CVertexBuffer::Create`'s entry (reading `RCX`=device, and `dev+120`/`+444`/`+460` at the exact
moment our test's real `CreateVertexBuffer(60, 0, D3DFVF_XYZRHW|D3DFVF_DIFFUSE, D3DPOOL_DEFAULT)` call
reaches it) confirmed the earlier decompile's routing logic runs with `dev+460 = 0x00190600`, and
replaying that exact decompiled logic by hand with this real value proves `v19` never gets remapped
from its default (2 = sysmem) to the real requested pool, because `dev+460 & 0x02000000 == 0`. A
`memory_write` watch on `dev+460` (widened after a narrow 4-byte watch caught nothing — the actual
write is an 8-byte QWORD store) caught the exact write: an 8-byte memcpy destination inside
`CBaseDevice::Init`, sourced from a `_D3D9_DEVICEDATA*` argument. Hooking *that* memcpy's call site
(reading `RCX`/`RDX`/`R8` = dest/src/size right before the `call`) and dumping the source struct's
bytes at offset 28 gave `0x00190600` again — then walking the call stack for a return address inside
`d3d9.dll`'s own range, watching *that* address's owning function resolve as `GetDX8HALCaps` →
`RegisterD3DCaps`, was a dead end (that call only *propagates* an already-populated value). The
decisive step was re-arming the SAME narrow write-watch on the *source* struct's offset 28
(`_D3D9_DEVICEDATA + 28`, address known from the memcpy hook) in a fresh run: the write's `RIP` landed
**inside our own `sogen_d3d9um.dll`**, not `d3d9.dll` — specifically `umd_GetCaps+0x74`
(`movups %xmm0,0x1c(%rcx)`, a 16-byte SSE store covering `D3DCAPS9::DevCaps`/`PrimitiveMiscCaps`/
`RasterCaps`/`ZCmpCaps` in one instruction, loaded from a compile-time `.rdata` constant). `offset 0x1c
== 28 == D3DCAPS9::DevCaps`. **`_D3D9_DEVICEDATA` and the buffer our own `pfnGetCaps` fills are the same
memory** — the runtime reads `caps->DevCaps` back out of its own caps buffer at this internal offset.
Objdump-ing the exact 16 `.rdata` bytes and decoding them against mingw's real (not memorized) 
`D3DDEVCAPS_*` values confirmed `0x00190600` is exactly `HWTRANSFORMANDLIGHT|HWRASTERIZATION|
PUREDEVICE|DRAWPRIMTLVERTEX|TEXTUREVIDEOMEMORY` — i.e. our own `fill_d3d9caps`'s current `DevCaps`
value, byte-for-byte. **The missing bit, `0x02000000`, has no name in the public `D3DDEVCAPS_*` set**
(the defined constants jump from `NPATCHES=0x1000000` straight past it) — an undocumented internal
reuse by the runtime's pool-routing gate. Added it as a raw literal (`k_devcaps_driver_managed_pool`)
to `fill_d3d9caps`'s `DevCaps` in `sogen_d3d9_umd.cpp`; a first attempt used the *wrong* constant
(`D3DDEVCAPS_QUINTICRTPATCHES = 0x00200000`, one hex digit off from the needed `0x02000000` — caught by
re-verifying `dev+460`'s live value after the fix and finding it still didn't have the target bit set).

**Re-traced with the fix in place and confirmed it's real**: `dev+460` now reads `0x02190600`
(bit 25 set), and hand-replaying `CVertexBuffer::Create`'s decompiled logic with this value proves
`v21` (the routing decision) now resolves to `0` instead of `2` — i.e. `CreateDriverVertexBuffer` (the
real, driver-backed path) instead of `CreateSysmemVertexBuffer`. This is a genuine, verified bug fix,
independent of whether it alone completes the vertex-delivery chain (it doesn't, see below).

**A second, deeper gate remains.** With the DevCaps fix in place, `Lock()` on the vertex buffer still
returns `S_OK` with a `NULL` pointer (previously it returned a non-null but wrong pointer from the
sysmem fast path). Live-tracing `CDriverVertexBuffer::Lock` (the class now actually constructed, thanks
to the fix) shows its own separate "hal level" gate (`*(int*)(device+72)`, unrelated to `DevCaps`) reads
`11` for our real device — *above* the `< 10` fast-path threshold this session's earlier notes assumed
was always taken — meaning it takes the "real" branch, calling a function pointer at `device+72's
target+864` with a request struct, whose overall return is `>= 0` (success) yet the resulting data
pointer is still null. That dispatch resolves (confirmed live) to the global `DdLockLH` function — a
large DirectDraw-compatible ("LongHorn DDI") lock implementation that itself, at one specific call site,
invokes a function pointer whose *value at that exact moment* resolves to our own `umd_Lock`'s real
address (confirmed via objdump against our own compiled DLL).

**Ruled out the instrumentation-artifact hypothesis and found a second real bug (2026-07-03).** The
Python-hook trace's finding needed independent confirmation without a hook potentially perturbing
execution, so this used sogen's *other* debugger surface — the GDB stub (`analyzer -d --port 28960`) —
connected via `lldb`'s `gdb-remote` support (no `gdb` binary on this Mac, but `lldb` speaks the same
remote-serial protocol) and a small scripted continue-loop (`SBProcess.Continue()` in a Python command,
since `lldb`'s own `-o` batch flags can't loop). Breakpoints set directly at real addresses (module base
+ RVA from the same idasql database used all session, cross-checked against a known-good address that
fires 4 times as expected) confirm, with **zero** hooks anywhere near the call: `CDriverVertexBuffer::
Lock` fires once for our real vertex buffer, its call to `DdLockLH`'s internal dispatch happens right
after, and **`umd_Lock` genuinely does execute** — reading `pArgs->hResource` at that exact stop shows
`9`, the same small "collision-prone" DDI handle value found live earlier this session (see the
`resolve_resource_id` fallback described in §10.5/§10.6). Since `pfnCreateResource` never fires for
vertex/index buffers (confirmed repeatedly), that handle was never registered — `resolve_resource_id`'s
existing lazy-bind fallback creates a hardcoded 640×480 `D3DUSAGE_RENDERTARGET` **texture**, wrong kind
and wrong size, for what is actually a 60-byte vertex buffer. **This, not the earlier
instrumentation-artifact worry, is the real reason `Lock()` was returning garbage/null.**

**Fixed**: added `resolve_buffer_resource_id(handle, byte_size)` — the same lazy-bind pattern, but
correctly sized (from `D3DDDIARG_LOCK::SizeToLock`, which `pfnLock` is the one call site that actually
knows) and correctly kinded (`resource_kind::vertex_buffer`). Wired into `umd_Lock` (the natural place —
it's the first call site with a real size to lazy-bind from) and into `umd_SetStreamSource`/
`umd_SetIndices` (so the same wire resource id gets reused consistently instead of the raw,
collision-prone handle going straight onto the wire unresolved). `umd_Lock`'s call to this is safe for
non-buffer resources too: render targets/textures are already registered via `pfnCreateResource` by the
time `Lock()` reaches them (confirmed live), so the lazy-bind branch only ever fires for buffers in
practice.

**First fix wasn't the full story — one more struct-offset bug, then a genuine, complete, verified
milestone.** Rebuilt, restaged, reran the real triangle test after the resource-id fix: no crash, no
regression, but `vb->Lock()` still returned `S_OK` with a `NULL` data pointer, and the drawn pixel still
read the clear color. Given `umd_Lock` really does execute (GDB-confirmed) with the correct resolved
resource id, the remaining gap had to be in how the data pointer flows back out. Fully decompiled
`DdLockLH` (only partially read before) and found it: `DdLockLH` builds its **own, separate, ~64-byte**
local stack struct (`v28` through `v34` in the decompile, spanning `rsp+0x60`..`rsp+0xA0`) and passes
`&v28` to the actual driver dispatch call — **not** the 104-byte struct `CDriverVertexBuffer::Lock`
built and which was RE'd earlier as `D3DDDIARG_LOCK` (with `pData` at offset 80). That earlier RE
characterized the *outer*, driver-agnostic struct `CDriverVertexBuffer::Lock` uses for its own
bookkeeping — not what actually crosses the DDI boundary. `DdLockLH`'s own caller reads the resulting
data pointer back from `*(QWORD*)((char*)&v32 + 4)`, where `v32` sits at `rsp+0x84` — offset 40 relative
to `&v28`. **Moved `D3DDDIARG_LOCK::pData` from offset 80 to offset 40** in `d3d9_ddi.hpp` (kept the
struct's overall 104-byte size for safety/compatibility, just repositioned the one field that matters
based on hard evidence); stopped reading `Flags` at its old offset 96 (now known to be past the real
~64-byte struct's bounds — `umd_Lock` sends `0` until that field's real offset gets its own RE pass).

**Rebuilt and reran — full, genuine, end-to-end success.** `vb->Lock()` returns a real, non-null pointer;
the app's own vertex writes land in it; `DrawPrimitive` uses the real data; a temporary host-side
diagnostic (same pattern as Phase 3/4's earlier pipeline-only verification, removed after confirming)
read the render target's centroid pixel back as `B=0x55 G=0x56 R=0x54` — the exact expected barycentric
average of the triangle's red/green/blue vertex colors, from **genuine, guest-authored vertex data**,
not an injected diagnostic substitute this time. The render-target's own `LockRect()` (the separate,
long-documented §10.5 gap — CSurface-based, not CDriverVertexBuffer-based) started returning a real
pointer too, for free, as a side effect of the same fix (both apparently funnel through `DdLockLH`).
**This is the plan's literal Phase 4 goal, fully achieved**: a real GPU-rendered triangle, from real app
vertex data, verified analytically — not the earlier pipeline-only version with substituted data.

Not yet independently re-verified: `D3DDDIARG_LOCK`'s `OffsetToLock`/`SizeToLock` fields (offsets 16/20)
were inherited from the old, now-known-imprecise RE and happened to still work correctly for this test's
Lock pattern (offset 0, size = whole buffer) — worth confirming for partial-range locks before relying
on them further. `Flags` genuinely isn't wired to anything yet (always sent as 0), so lock hints like
`D3DLOCK_READONLY`/`D3DLOCK_DISCARD`/`D3DLOCK_NOOVERWRITE` have no effect — fine for this test, a gap for
anything that depends on them.

---

## 10.7. `pfnPresent` wired — the triangle is genuinely visible on screen (2026-07-03)

`pfnPresent` was previously `device_stub` (silently `S_OK`, no-op). Wired it properly: `umd_Present`
resolves `hSrcResource` (via the existing `resolve_resource_id` lazy-bind, since render targets/
backbuffers are already registered via `pfnCreateResource` by the time `Present()` fires) and sends a
new sync command (`ioctl_d3d9_present`, `d3d9_cmd::present_request`/`present_response`) to a new
`gpu_bridge.cpp` handler, which calls `d3d9_host::snapshot_resource` (new method — copies the resource's
current CPU-side pixel backing) and `windows_emulator::ui().present_surface(...)`.

**Two real sub-problems, both solved:**
- **No HWND anywhere.** Live-dumped the actual bytes `pfnPresent` receives (same GDB-stub method as the
  Lock investigation) — genuinely no window handle anywhere in the struct, confirming the earlier
  documented finding (`d3d9_host.hpp`'s own comment: "D3DDDIARG_PRESENT carries no HWND"). This is
  architecturally real, not a struct-offset bug like Lock's: the real Windows D3D9/DXGK architecture
  resolves "which window" via a separate, driver-opaque kernel path (the same one
  `handle_NtGdiDdDDIPresent` already serves correctly for the real swap-chain backbuffer, via
  `EMU_D3DKMT_PRESENT::hWindow` supplied by the runtime's own internal tracking) — our `d3d9_host`
  resources bypass that system entirely (documented gap, same one behind outstanding task #11's
  "consolidate onto one vulkan_host"). Fix: reused `syscalls/user.cpp`'s own
  `find_foreground_window`-equivalent fallback logic locally in `gpu_bridge.cpp` (prefer
  `process.foreground_window`; else any visible top-level window) — the same pragmatic default
  `GetForegroundWindow()` itself falls back to for a freshly-created, not-yet-focused window. Confirmed
  live: `process.foreground_window` genuinely stays `0` for a CLI-launched, never-clicked test window
  (real host-side activation events never fire in this harness) — the visible-top-level-window fallback
  is what actually finds it.
- **The triangle itself was never drawn to anything that gets Presented.** The test draws to an explicit
  off-screen `CreateRenderTarget` surface (for the analytic `LockRect` check), never to the real
  swap-chain backbuffer — so the first, only `Present()` call in the original test just re-showed the
  plain clear color from before the triangle even existed. Extended `d3d9_triangle_test.cpp`: after the
  off-screen draw, restores the real backbuffer as render target 0, clears it, draws the same triangle
  again, and calls `Present()` a second time.

**Verified end-to-end with a temporary diagnostic (removed after confirming):** the first `Present()`
call's presented pixel reads the plain clear color; the second reads `B=0x55 G=0x56 R=0x54` — the same
barycentric centroid color confirmed for the off-screen draw, now genuinely reaching
`ui().present_surface()` and showing up in the actual emulator window. No regressions (smoke test clean).

---

## 11. Immediate next steps (in order)

**Milestone reached (2026-07-03): a real triangle, drawn from real app-authored vertex data, verified
analytically. §10.6 has the full story.** Three real bugs found and fixed this session via sogen's own
built-in debugger (Python live-hooking API, then the GDB stub via `lldb`'s `gdb-remote` support) instead
of static idasql decompilation alone: (1) an undocumented `DevCaps` bit (`0x02000000`) gating whether
`CVertexBuffer::Create` honors the app's requested pool at all; (2) `umd_Lock`/`umd_SetStreamSource`/
`umd_SetIndices` resolving vertex/index buffer DDI handles through the wrong (texture-shaped) lazy-bind
path, now fixed via a size/kind-aware `resolve_buffer_resource_id`; (3) `D3DDDIARG_LOCK::pData` was
modeled at the wrong offset (80, from RE'ing the wrong — outer, intermediate — struct; the real
DDI-level struct `pfnLock` receives is ~64 bytes with `pData` at offset 40). Both `Lock()` on a
`D3DPOOL_DEFAULT` vertex buffer and the long-standing `LockRect` gap (§10.5) work now, for real data.

1. **`pfnCreateResource`'s remaining field layout** (width/height/usage/pool offsets) is still unknown
   — offset 0 (Format) and offset 48 (output `hResource`) ARE now RE-verified (§10.5). The current
   fixed-guess width/height (640×480) works for this one test's window size but is wrong in general;
   textures and other resource kinds will need the real struct eventually. Needs a texture-creation-
   specific forcing function and a fresh RE pass (the class-hierarchy/xref search that worked for
   `RENDERSTATE`/`LOCK`/`CLEAR`/`PRESENT` came up empty for `CreateResource`'s actual builder — try a
   different angle, e.g. tracing from `CBaseDevice::CreateTexture`/`CSwapChain`'s constructor forward
   instead of searching by struct name backward).
2. **`D3DDDIARG_LOCK`'s `Flags` field** (see §10.6) — currently always sent as 0; needs its real offset
   in the ~64-byte DDI-level struct RE'd (the same live-debugging method that found `pData`'s real
   offset applies directly) before `D3DLOCK_READONLY`/`DISCARD`/`NOOVERWRITE` hints can work.
3. **(Optional, low priority) SM3.0 caps follow-up.** `fill_d3d9caps` currently reports a
   fixed-function device (VS/PS version 0) as a deliberate workaround — restoring
   `D3DVS/PS_VERSION(3, 0)` re-triggers d3d9's SM2.0+ HAL-disable gauntlet elsewhere (confirmed:
   `GetDeviceCaps` itself starts failing with the same `0x8876086a`). Not required for M1.
4. **Part 4 — vkd3d-shader integration** for SM1-3 token → SPIR-V translation (needed before Part 3's
   pipelines have real shader modules instead of a placeholder). Milestone M1 = programmable SM2/3
   triangle, pixel-diffed vs the DXVK oracle (`root_vkspike`). **This is the current active workstream**
   as of 2026-07-03 — see the plan file for scope.
5. **When ready to push:** sign every unsigned commit first (`git rebase --exec 'git commit --amend
   --no-edit -S' <base>`); verify with `git log --show-signature`.

---

## 12. Outstanding tasks (from the tracker)
- #15: Spike B-4 / gate 3 — **DONE** (see §1/§6). Stage 2 Part 2 transport/state-marshaling — **IN
  PROGRESS** (see §10/§11).
- #11 (deferred): delete `SogenGpu` io_device + consolidate onto one `vulkan_host`.
- #5: investigate DXVK `Config` ctor C++ throw under sogen (MW2 blocker on the DXVK-oracle side).
- #6: reproducible/CI provisioning of real MS `dxgi.dll` for the Vulkan path.

---

## 13. Global working rules (carry over)
- Never commit `.claude/` or `.idea/`. Commit unsigned (`--no-gpg-sign`) while working; sign every commit
  before pushing (`git rebase --exec 'git commit --amend --no-edit -S' <base>`); verify with
  `git log --show-signature`.
- Don't generate code comments unless they add non-deducible info. Run clang-format on changed files.
- Prefer clean/idiomatic solutions; no shortcuts/workarounds.
- **Set global git identity on any new machine before committing**: this Mac's `user.name`/`user.email`
  were unset, so the first commit here landed as `Jack <jack@Jacks-MacBook-Pro.local>` instead of
  `Jackson Yarger <jacksonkyarger@gmail.com>` — caught and fixed with `--amend --reset-author` since it
  was still unpushed. `gh auth login` does *not* set this; it's a separate `git config --global` step.

---

## 14. vkd3d-shader de-risk Task 4 — DDI wiring done, root cause found and fixed (2026-07-03)

`pfnCreateVertexShaderFunc`/`pfnCreatePixelShader`/`pfnDeleteVertexShaderFunc`/`pfnDeletePixelShader`
are now wired (slots 42/43/67/68, `umd_CreateVertexShaderFunc`/`umd_CreatePixelShader`/... in
`sogen_d3d9_umd.cpp`), marshaling into the already-existing `create_shader_request`/`create_shader_response`
wire protocol. `D3DDDIARG_DELETEVERTEXSHADERFUNC` was added to `d3d9_ddi.hpp`. Builds clean, no
regression (`d3d9-triangle-test` and the 26/26 smoke test are unchanged).

**Update (2026-07-03, later same day): the earlier BLOCKED status was premature — not a caps gate at
all, a real DDI calling-convention bug in our own UMD, found via a much more targeted live-tracing pass
and fixed.** The requesting agent (suspecting a sixth undocumented Task-0-style caps gate) asked for a
fresh, surgical RE pass instead of another blanket basic-block dump. idasql decompilation of the real
staged `d3d9.dll` (`CD3DBase::CreateVertexShader`/`CreatePixelShader`) plus live breakpoints (sogen's
Python debugger API, `app.hooks.memory_execution_at`) at each successive branch point — the `this+76`
device-flags gate, the shader-bytecode validator (`ValidateVertexShaderInternal`/`GetNewVSValidator`),
`CVertexShaderFunc::Init`'s token-stream parse, `CVertexShaderFunc::InitHW`'s two internal gates
(`this+0x60` bit 0, `device+0x4028==1`) — **ruled out every one of them live**: all pass cleanly for a
real `D3DCompile()`-produced `vs_1_1`/`ps_2_0` shader pair. The actual failure was one level deeper:
`InitHW`'s real driver dispatch (`CD3DDDIDX10TL::CreateVertexShaderFunc`/`CD3DDDIDX10::CreatePixelShader`)
calls **our own UMD's `pfnCreateVertexShaderFunc`/`pfnCreatePixelShader`** (confirmed live — the call
target resolved to an in-module d3d9.dll thunk that itself calls the device-func-table slot 42/67
function pointer, i.e. our driver), which was returning a failure `HRESULT`; d3d9.dll's own wrapper then
converts any negative return into a thrown `E_FAIL`/`D3DERR_INVALIDCALL` C++ exception.

**Root cause: same class of bug as the already-documented `pfnSetTexture` fix (§10.6) — an assumed
struct-pointer DDI calling convention that isn't real.** `pfnCreateVertexShaderFunc`/`pfnCreatePixelShader`
are **not** `(HANDLE, D3DDDIARG_CREATE*SHADERFUNC*)`; RE of the real call site
(`CD3DDDIDX10TL::CreateVertexShaderFunc`/`CD3DDDIDX10::CreatePixelShader`) shows three direct-value
arguments: `(HANDLE hDevice, D3DDDI_HANDLE* pShaderHandle /* in/out, CHandleFactory-preassigned */,
CONST UINT* pFunction /* raw SM1-3 token stream, no length param at all */)`. Since the DDI gives no
length, a real driver must parse the token stream itself to find the terminating `D3DSIO_END`
(`0x0000FFFF`) token — the same thing d3d9.dll's own `CVertexShaderFunc::Init`/`GetInstructionLength`
does. `GetInstructionLength`'s real per-opcode length table (SM1.x has no generic length field; SM2.0+
encodes it in token bits `[27:24]`) was RE'd via idasql decompile and transcribed faithfully into a new
`measure_shader_token_length_dwords` helper in `sogen_d3d9_umd.cpp`, replacing the old (wrong)
`D3DDDIARG_CREATEVERTEXSHADERFUNC::Values[0]`/`CodeSize` struct-field reads. `umd_CreateVertexShaderFunc`/
`umd_CreatePixelShader` now take `(HANDLE, HANDLE* pShaderHandle, CONST UINT* pFunction)` directly,
matching the real DDI; the old, now-provably-wrong `D3DDDIARG_CREATEVERTEXSHADERFUNC`/
`D3DDDIARG_CREATEPIXELSHADERFUNC` struct typedefs were removed from `d3d9_ddi.hpp` (dead code, and
actively misleading now that the real convention is known).

**Verified end-to-end**, rebuilt UMD + rerun via the same throwaway `D3DCompile()`-based diagnostic test
(`scratchpad/d3d9_shader_diag_test_task4.cpp`) through sogen's Python debugger harness (not just
`analyzer -e root -c`, since that CLI's default trace verbosity doesn't surface guest stdout text):
`CreateVertexShader hr=0x00000000` (real non-null handle) and `CreatePixelShader hr=0x00000000` (real
non-null handle), for genuine `D3DCompile()`-produced `vs_1_1`/`ps_2_0` bytecode (116/140 bytes). No
regressions: `d3d9-triangle-test` unchanged (`DrawPrimitive hr=0x8007000e` for the FVF-only path is the
same pre-existing, already-documented, unrelated gap — see §11 item 3/§10.6's closing notes), smoke test
still 26/26. `clang-format` was not available on this machine to run per the repo's own convention;
worth running before this lands anywhere it matters.

Task 5 (real `D3DCompile()` passthrough triangle test) can now proceed on solid ground — shader creation
genuinely reaches and succeeds against our own driver, not just an internal null-shader probe.

**Earlier investigation (superseded by the fix above).** Before the DDI calling-convention bug was
found, `CreateVertexShader`/`CreatePixelShader` calls never reached the driver at all: neither a
hand-assembled minimal vs_2_0/ps_2_0 shader nor real `D3DCompile()`-produced `vs_2_0`/`ps_2_0`/`vs_1_1`
bytecode got past `D3DERR_INVALIDCALL`/`E_FAIL` in the runtime, and a basic-block trace of the failure
window (sogen's Python `app.hooks.basic_block` API) didn't isolate the exact failure point — it landed
on a tight synchronization spin loop and a helper-call tail that `objdump` showed returning `S_OK`, not
the error the API ultimately reported. That investigation was chasing a caps/device-state gate (in the
spirit of Task 0's `DevCaps`/`DevCaps2`/`PrimitiveMiscCaps` bits) on the assumption that the DDI used the
`D3DDDIARG_CREATEVERTEXSHADERFUNC`/`D3DDDIARG_CREATEPIXELSHADERFUNC` struct-pointer convention (the
now-removed struct typedefs in `d3d9_ddi.hpp`). The real cause was the wrong calling convention
entirely, found only once the fresh, targeted live-tracing pass described above ruled out every gate one
by one and traced the failure to our own UMD's return value. The throwaway diagnostic guest test used
during this investigation is saved at `scratchpad/d3d9_shader_diag_test_task4.cpp` in that session's
Claude Code scratchpad.

---

## 15. vkd3d-shader de-risk Task 6 — DrawPrimitive/E_OUTOFMEMORY gate root-caused and fixed; real SM2
    shader translation verified end-to-end by pixel readback (2026-07-03)

**Milestone: this is the plan's terminal goal, achieved.** The full chain — `D3DCompile()` →
`CreateVertexShader`/`CreatePixelShader` → vkd3d-shader SM1-3→SPIR-V translation → a real programmable
Vulkan pipeline → `DrawPrimitive` → `Present` — now runs end to end and produces an analytically-verified
triangle. `d3d9-shader-test.exe`: `DrawPrimitive hr=0x00000000`, `Present hr=0x00000000`. The unmodified
`d3d9-triangle-test.exe` (fixed-function) baseline **also** now gets `DrawPrimitive hr=0x00000000` on
both its draws (previously `E_OUTOFMEMORY`), confirming the gate was shared, not shader-path-specific.

### Root cause: Task 4's DDI calling-convention fix for `pfnCreateVertexShaderFunc`/`pfnCreatePixelShader`
was itself subtly wrong, and a second, previously-unreachable bug in `pfnSetPixelShader`/
`pfnSetVertexShaderFunc` was masked behind it

Investigation used the same live-tracing methodology as every other gate this session, but doubled down
on precision: sogen's Python debugger API (`import sogen`, `app.hooks.memory_execution_at`,
`app.read_register`/`read_memory`), breakpointed directly on individual real instructions in the staged
`d3d9.dll` (addresses cross-checked against a fresh idasql decompile of `d3d9_x64.dll.i64`), rather than
a broad basic-block sweep. Chain of evidence, each step confirmed live before moving to the next:

1. **`CD3DBase::DrawPrimitive`'s own body always returns 0 on its normal path** — the observed
   `E_OUTOFMEMORY` had to come from a C++ exception thrown somewhere inside its state-flush block and
   caught by an outer wrapper. idasql decompile of `CD3DBase::DrawPrimitive` (`0x1800226B0`) showed a
   large "flush pending state" block that calls `ff2vs::CConverterToVertexShader::PrepareToDraw` and
   `ff2ps::CConverterToPixelShader::PrepareToDraw` — **this runs for every draw, FVF-only or
   shader-bound alike**, not just the fixed-function-emulation case the name suggests; it's the general
   per-draw shader-cache resolution path.
2. **`ff2ps::CConverterToPixelShader::PrepareToDraw` (`0x1800238F0`) has a hardcoded
   `return 2147942414LL;` (`0x8007000E` = `E_OUTOFMEMORY`)** at its failure convergence point
   (`0x1800239FE`), reached whenever `GenerateShader()` returns null OR the driver-create callback
   returns a null handle. Breakpointing that exact instruction (`trace_ps_prepare.py` in this session's
   scratchpad) confirmed it fires for the FF triangle test's draw; `GenerateShader` itself succeeds
   (non-null), so the failure is the driver-create callback returning null.
3. **Traced the callback dispatch three layers deep, reading the real call target out of RAX at each
   `__guard_xfg_dispatch_icall_fptr` site** (CFG-hardened indirect calls, so the target has to be read
   from the register right before the call, not inferred from static analysis):
   `ff2ps::PrepareToDraw`'s callback (`CPSConverterCallbacksLddm::CreatePixelShader`, `d3d9+0x44360`) →
   `CD3DDDIDX10::CreatePixelShader` (`d3d9+0x42930`, the exact same function Task 4 already RE'd) → our
   own `umd_CreatePixelShader` (`sogen_d3d9um.dll+0x24d0`). Confirmed **the same DDI slot 67 dispatch
   Task 4 wired is genuinely reached and returns `hr=0x0`** — so the bug is not a missing/unwired slot,
   it's what happens with a *successful* call's output.
4. **The real args at the final call site (`d3d9+0x180042995`) are `(HANDLE hDevice, D3DDDIARG_
   CREATESHADERFUNC* pArgs, CONST UINT* pFunction)`, not the 3-direct-value convention Task 4 concluded.**
   Reading the struct at `pArgs` live showed `CodeSize` at offset 0 (`0x44` = 68, the real token byte
   length — the runtime already knows this and hands it to the driver, no self-parsing needed) and a
   `ShaderHandle` output slot at offset 8. `CD3DDDIDX10::CreatePixelShader`'s own decompile confirms it:
   `*a4 = v9;` where `v9` lives at `pArgs+8`, never `pArgs+0`. Task 4's `umd_CreatePixelShader` wrote the
   resulting handle to `*pShaderHandle` at **offset 0** (overwriting `CodeSize`, never touching offset 8)
   — so every caller reading the handle back from offset 8 saw it stay zero forever, `hr=0x0` or not.
   `ff2ps::PrepareToDraw` (and `ff2vs::PrepareToDraw`, same struct, same bug, confirmed via
   `CD3DDDIDX10TL::CreateVertexShaderFunc`'s identical decompile) treats a null returned handle as
   creation failure and falls into the hardcoded `E_OUTOFMEMORY`.
5. **Fixed**: added back `D3DDDIARG_CREATESHADERFUNC` (`{UINT CodeSize; HANDLE ShaderHandle;}`) to
   `d3d9_ddi.hpp`; `create_shader_common` now takes `CodeSize` straight from `pArgs->CodeSize` (no
   self-measurement) and writes the result to `pArgs->ShaderHandle` (offset 8); `umd_CreateVertexShaderFunc`/
   `umd_CreatePixelShader` signatures updated to `(HANDLE, D3DDDIARG_CREATESHADERFUNC*, CONST UINT*)`.
   **This also makes `measure_shader_token_length_dwords` (and the ps.1.x `D3DSIO_TEXCOORD`..
   `D3DSIO_CMP` opcode-table gap Task 4's review flagged in it) moot** — the function is now dead code
   and was removed, since the runtime supplies `CodeSize` directly and self-parsing is never needed.
   This closes that deferred item; it wasn't a real bug that would ever have fired at this call site,
   it was only a workaround for the earlier (wrong) no-length calling-convention theory.
6. **A second, previously-unreachable bug surfaced immediately after fixing #5**: with a real non-null
   shader handle now flowing correctly for the first time, `d3d9-triangle-test.exe` started **crashing**
   (`Mapping violation: 0xb (8) - r-- at sogen_d3d9um.dll+0x23eb`, i.e. dereferencing the small integer
   handle value `0xB` as a pointer) inside `umd_SetPixelShader`. `pfnSetPixelShader`/
   `pfnSetVertexShaderFunc` were implemented as `(HANDLE, CONST D3DDDIARG_SETPIXELSHADERFUNC* pArgs)`
   struct-pointer calls — the exact same wrong-convention mistake `pfnSetTexture` had (see §10.6), just
   never exercised before because no call site had ever handed them a genuine non-null handle to bind.
   Fixed the same way as `pfnSetTexture`: both are direct-value calls, `(HANDLE hDevice, HANDLE
   hShader)`; removed the now-dead `D3DDDIARG_SETPIXELSHADERFUNC`/`D3DDDIARG_SETVERTEXSHADERFUNC`
   structs.

**Verified end to end.** Rebuilt the UMD (`x86_64-w64-mingw32-g++ ... -o sogen_d3d9um-x64.dll`), staged
it, rebuilt `cmake --build --preset=release`:
- `d3d9-triangle-test.exe`: both `DrawPrimitive` calls now `hr=0x00000000` (previously `E_OUTOFMEMORY`),
  no crash, `pixel[0]=B=FF G=80 R=40 A=FF` unchanged (still correct) — genuine regression fix, not a
  behavior change.
- `d3d9-shader-test.exe`: `DrawPrimitive hr=0x00000000`, `Present hr=0x00000000`.
- **Pixel-level proof of real SM2 shader translation.** A temporary diagnostic in `d3d9_host.cpp`'s
  `execute_draw` (added and removed within this task, net zero diff) sampled the programmable-pipeline
  render target's centroid pixel `(320, 240)` right after `readback_render_target`. Task 5's triangle has
  vertices `A=(0,0.5)` red, `B=(0.5,-0.5)` green, `C=(-0.5,-0.5)` blue; the barycentric weights of NDC
  `(0,0)` against those vertices are `w_A=0.5, w_B=0.25, w_C=0.25`, giving expected color
  `R=0x80, G=0x40, B=0x40, A=0xFF`. **Actual captured output: `[d3d9_host][DIAG] centroid B=3F G=40 R=80
  A=FF`** — G/R/A exact, B off by one (`0x3F` vs `0x40`, well inside the ±2/channel rounding tolerance).
  This is genuine evidence the whole chain is correct: `D3DCompile`'s real bytecode, vkd3d-shader's
  SM1-3→SPIR-V translation, the inter-stage varying map, vertex attribute layout, rasterization, and
  per-pixel color interpolation all agree with hand-computed ground truth.
- `analyzer -e root -s c:/test-sample.exe`: 26/26 `Success`, unchanged.
- `clang-format` remains unavailable on this machine (as in Task 4's note) — worth running before this
  lands anywhere it matters.

### Deferred work

Consolidated into `docs/d3d9-roadmap.md` — that's now the single tracking doc for remaining D3D9
work (textures, int/bool constant registers, SM3.0, WoW64/x86, M3 coverage items, etc.), kept
up to date at the end of every slice. Don't re-scatter deferred items back into this file; update
the roadmap doc instead.

Constant buffers/UBOs (the item this section used to list first) are done as of the very next slice
after this one — see `docs/d3d9-roadmap.md`'s M1.5 entry.

---

## 16. M2 Task 3 — sampler-state DDI encoding RE'd live; real samplers + combined-image-sampler binding wired (2026-07-03)

**The question this session answered, gate-task style:** §11's earlier note ("`pfnSetSamplerState`: no
separate slot exists in `D3DDDI_DEVICEFUNCS` at all... figure out the real TSS/sampler-state boundary
before wiring it to anything") is now resolved with live-captured evidence, not assumption.

**Method:** a throwaway guest test (`d3d9_sampler_diag_test.cpp`, removed after use) called
`SetTextureStageState(0, D3DTSS_COLOROP, ...)` plus several `SetSamplerState(sampler, TYPE, value)`
calls with values chosen to differ from D3D9's own cached defaults (so the runtime's dirty-state cache
wouldn't suppress the driver dispatch), then issued a real `DrawPrimitive` to force any deferred/batched
state to flush. Temporary `log_line` instrumentation in `umd_SetTextureStageState`
(`sogen_d3d9_umd.cpp`, removed after) printed every `Stage`/`State`/`Value` the real staged `d3d9.dll`
actually sent, captured through `analyzer -e root -c`.

**Finding: there is no numeric threshold — sampler state and texture-stage state share one interleaved
`State` enum (`D3DDDITEXTURESTAGESTATETYPE`), told apart only by which specific value arrives.** Two
independent pieces of live evidence agree:
1. **The runtime's own per-sampler default-initialization sequence**, captured for every one of the 16
   real samplers (`Stage`/`Sampler` 0-15, no offset applied): `State = 13, 14, 25, 15, 16, 17, 18, 19,
   20, 21, 29, 31, 30` in that fixed order, for every sampler index — clearly distinct from the plain
   TSS default-init sequence also captured for stages 0-7 (`State = 7, 8, 9, 10, 11, 22, 23, 24`).
2. **Explicit `SetSamplerState()` calls with non-default values**, which changed a cached value and so
   weren't optimized away by the runtime, reached the driver as:
   - `SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)` → `Stage=0 State=16 Value=2`
   - `SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP)` → `Stage=0 State=13 Value=3`
   - `SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)` → `Stage=2 State=17 Value=2` (confirms the
     `Stage` field carries the sampler index unmodified, not offset by the 8 real texture stages)
   - `SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR)` → `Stage=3 State=14 Value=2`

   Confirmed mapping (DDI `State` → public `D3DSAMPLERSTATETYPE`): `13→ADDRESSU, 14→ADDRESSV,
   25→ADDRESSW, 15→BORDERCOLOR, 16→MAGFILTER, 17→MINFILTER, 18→MIPFILTER, 19→MIPMAPLODBIAS,
   20→MAXMIPLEVEL, 21→MAXANISOTROPY`. Three more sampler-shaped values (`29, 31, 30`) appear in every
   default-init sequence but were never individually round-tripped through a distinguishing explicit
   call, so their exact `D3DSAMPLERSTATETYPE` identity (candidates: `SRGBTEXTURE`/`ELEMENTINDEX`/
   `DMAPOFFSET`, `D3DSAMP` 11-13) is unconfirmed — routed to the sampler bucket regardless (the correct
   category), under reserved out-of-range values (1029-1031) rather than a guessed identity.

**Implementation on top of this finding:**
- `sogen_d3d9_umd.cpp`'s `umd_SetTextureStageState` now runs every `State` through
  `sampler_state_for_ddi_tss_state()` (the table above); a nonzero result means "this is really a
  sampler-state call" — it repacks `{Sampler=Stage, State=<public D3DSAMP value>, Value}` and sends it
  over the wire's existing (previously unused) `ioctl_d3d9_set_sampler_state`/`set_sampler_state_record`
  instead of `ioctl_d3d9_set_texture_stage_state`. `d3d9_host`'s host-side handler for that opcode
  already stored into `device_state::sampler_state` (keyed `(sampler<<32)|state`) unconditionally since
  the M1.5 slice — it was simply never reachable from the guest before this fix.
- `d3d9_host.cpp`: new `build_sampler()` reads the accumulated `sampler_state` map for a given sampler
  index (falling back to D3D9's real documented per-state defaults — `POINT` filters, `WRAP` addressing,
  `MAXANISOTROPY=1` — confirmed by this session's own default-init capture) and calls
  `vulkan_host::create_sampler` with real `VkFilter`/`VkSamplerAddressMode`/`VkSamplerMipmapMode` values
  translated from the public D3D9 enums. Created fresh per draw and destroyed after, mirroring
  `execute_draw`'s existing per-draw VS/PS UBO lifecycle (no persistent sampler cache yet — a reasonable
  follow-up once this scheme sees real reuse pressure).
- `ensure_programmable_pipeline`'s PS descriptor-set layout (set 1) gained a second binding — binding 1,
  `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`, for texture stage/sampler 0 (this slice's minimum-viable
  single-texture scope; more samplers are additional bindings, a follow-up for whenever a test needs
  more than one bound texture). `descriptor_pool_`'s pool sizes grew to include one
  `COMBINED_IMAGE_SAMPLER` slot. `execute_draw` only writes binding 1 when a real, GPU-backed texture is
  actually bound at stage 0 (via the already-existing `ensure_texture_uploaded`, wired in for the first
  time here) — Vulkan permits a pipeline layout to declare more bindings than a shader module statically
  uses, so this is safe even before Task 7 makes the SPIR-V side actually sample.
- **Verified with a second throwaway test** (`d3d9_texture_smoke_test.cpp`, scratchpad, not committed):
  real `CreateTexture`/`SetTexture`/multiple `SetSamplerState` calls followed by two consecutive
  `DrawPrimitive`+`Present` cycles (exercising the descriptor-pool reset/reallocate and sampler
  create/destroy cycle twice) completed with no crash and no Vulkan validation output — this path has no
  coverage in the existing regression suite (none of `d3d9-triangle-test`/`-shader-test`/`-const-test`
  ever call `SetTexture`), so this was a deliberate extra check given the gate-task risk level.

**Pre-existing dead code, left untouched:** `D3DDDIARG_SAMPLERSTATE` in `d3d9_ddi.hpp` (a struct-pointer
DDI arg shape for a `pfnSetSamplerState` slot that this task's own RE reconfirms doesn't exist) predates
this task and remains unreferenced — not touched, per the "don't remove pre-existing dead code" rule.

No regressions: `d3d9-triangle-test`/`-shader-test`/`-const-test` all unchanged (`DrawPrimitive`/
`Present` still `hr=0x00000000`, `d3d9-const-test`'s two pixel checks still exact), smoke test still
26/26. `clang-format` remains unavailable on this machine (same as Tasks 4/6's notes).

---

## 16. M2 Task 8 — the terminal integration test, both carried-forward findings resolved (2026-07-04)

`d3d9_texture_test.cpp` proves textures, indexed draws, real depth testing, and real alpha blending
together in one real render, with all 4 analytic pixel checks passing exactly. Getting there required
resolving both carried-forward findings from earlier tasks (with real, live-verified root causes, not
guesses) plus finding and fixing several more real bugs the combined feature set exposed for the first
time. Full methodology: live GDB-stub + `lldb`'s `gdb-remote`, and sogen's Python debugger API
(`hook_memory_execution` on real, idasql-verified RVAs against the exact staged `d3d9.dll`), the same
techniques used throughout this session.

### 16.1. Index-buffer Lock — real root cause, two distinct bugs

**Symptom, reproduced first in isolation** (`ib_diag_test.cpp`, scratchpad, not committed): locking a
plain `D3DPOOL_DEFAULT` index buffer, writing a distinctive pattern, unlocking, then re-locking
READONLY and reading it back at the pure D3D9 API level always passed — even with the pre-fix driver —
because the runtime satisfies that specific round trip from its own memory regardless of whether the
driver ever saw the data. The real test has to inspect the *host's* backing store directly (added a
temporary `fprintf` in `d3d9_host::lock`/`unlock`) to see it was staying all-zero.

**Bug 1 — index buffers were routing through the sysmem path, not the driver path.** Live-hooked
`CreateDriverIndexBuffer`/`CreateDriverManagedIndexBuffer`/`CreateSysmemIndexBuffer` directly (RVAs from
idasql): with the existing caps (`k_devcaps_driver_managed_pool = 0x02000000`, the bit that already
fixed *vertex* buffers), every index buffer still resolved to `CreateSysmemIndexBuffer`. Decompiling
`CIndexBuffer::Create`'s own routing logic (a *separate* function from `CVertexBuffer::Create`, not
shared code) showed it checks a *different* bit on the same DevCaps DWORD: `0x04000000`. With that bit
added, `CreateDriverIndexBuffer` fires instead (confirmed live). Consequence of the sysmem routing:
`CIndexBuffer::Lock`'s own dispatch (hooked directly, confirmed as the actual code path taken) still
calls into the driver's `pfnLock`/`pfnUnlock` — this is a real, unconditional dispatch to
`device_functable[35]`/`[36]`, always returning `hr=S_OK` — but the *pointer it hands back to the app*
comes from `*((_QWORD*)this + 16)`, a field on the C++ object that is set once at construction (a plain
system-memory allocation) and never written by the Lock dispatch at all (confirmed by reading it live,
before and after the dispatch call, across 4 separate Lock() calls on 2 different objects — value never
changed). The driver call is genuine but its result is discarded; the app always reads/writes the
runtime's own shadow copy.

**Bug 2 — `D3DDDIARG_LOCK`'s `OffsetToLock`/`SizeToLock` have no single, routing-path-independent
offset.** Once the DevCaps fix routed index buffers through the driver path, a *new* symptom appeared:
`resolve_buffer_resource_id`'s byte-size argument (read from what was modeled as `SizeToLock`, offset
72) came back as a garbage ~25MB value instead of the real buffer size. Byte-level comparison of the
struct our own `pfnLock` actually receives, captured across both the sysmem-routed path
(`CIndexBuffer::Lock`/`CVertexBuffer::Lock`'s own direct dispatch — offset 72 *does* reliably carry the
requested size here, confirmed across 4 distinct sizes: 12, 12, 6, 40 bytes) and the driver-routed path
(`CDriverIndexBuffer::Lock`→`LockI` — offset 72 here holds an unrelated caller-stack address; the real
`OffsetToLock` equivalent is at offset 80, and `SizeToLock` has no field in this shape at all): these
are genuinely two different structs built by two different real `d3d9.dll` code paths, and `umd_Lock` —
one function shared by every resource kind and routing path — cannot statically tell which one a given
call is. Fixed by making `umd_Lock`/`umd_Unlock` stop trying to read either field and always treat every
lock as an implicit whole-buffer lock (offset 0, size 0 → "use the resource's own fallback size") — the
existing `resolve_buffer_resource_id`/`d3d9_host::lock` fallback path already implements exactly this
convention.

With both fixed: `ib_diag_test.cpp`'s round trip shows the app's own second-Lock pointer now
genuinely differs from the first (proving a real driver round trip, not the runtime's cache) and the
host-side backing correctly shows the written pattern after `Unlock()`.

**Open, permanent limitation, not just an unfinished detail:** this fix makes every `Lock()` on a
vertex/index buffer a whole-buffer lock, unconditionally — `OffsetToLock`/`SizeToLock` are never read at
all now, not "read from a best-guess offset." A real game that locks only the newly-appended tail of a
growing dynamic buffer (the common `D3DLOCK_NOOVERWRITE` pattern) will silently get whole-buffer
semantics under this driver today, with no error or signal that partial-lock semantics weren't honored.
Supporting real partial locks would need per-routing-path struct detection (distinguishing the
sysmem-routed shape from the driver-routed `LockI` shape at the `pfnLock` call site itself, since the two
structs genuinely differ and can't be told apart by content alone) — not yet attempted.

### 16.2. Depth-stencil resource-id resolution — real root cause

Live-traced (temporary `log_line` in `umd_CreateResource`/`umd_SetDepthStencil`, `ds_diag_test.cpp`
scratchpad): `CreateDepthStencilSurface` **does** call `pfnCreateResource` (confirmed:
`Format=75`/`D3DFMT_D24S8` arrives correctly at the already-RE-verified offset 0), refuting the
"KNOWN LIMITATION" comment's implicit assumption that it might not. But `pfnSetDepthStencil` itself
never fired at all until the guest test additionally called `Clear(D3DCLEAR_ZBUFFER)` and a real draw —
confirming the same worker-thread DP2-batch deferral this file already documents for other state calls
— and when it did fire, `hZBuffer` was a small, unrelated handle, *not* the same numeric value
`pfnCreateResource` had echoed back for the depth-stencil surface. `resolve_resource_id`'s generic
lazy-bind fallback (640x480 X8R8G8B8 RENDERTARGET) then minted a wrong-shaped resource for it, exactly
as Task 5's "KNOWN LIMITATION" comment predicted. Fixed with a dedicated
`resolve_depth_stencil_resource_id` (D3DFMT_D24S8 + `D3DUSAGE_DEPTHSTENCIL`, 640x480 — the same
fixed-size-window assumption every other lazy-bind fallback in this file already makes), used by
`umd_SetDepthStencil` instead of the generic function.

### 16.3. Additional real bugs found building the combined test

- **A `pfnCreateResource`/lazy-bind namespace collision** (found while fixing 16.2): registering
  `umd_CreateResource`'s own output handles into the *same* map `resolve_buffer_resource_id` used for
  its lazy-bind cache caused vertex/index buffer Locks to silently resolve to unrelated, wrong-shape
  resources, because `d3d9_host::allocate_id()`'s sequential counter and the runtime's own small-integer
  internal buffer handles are different, unrelated numbering spaces that really do coincide numerically
  (reproduced twice, at two different numeric ranges, live). Fixed at the actual source:
  `allocate_id()` now starts at `1ULL << 32`, and a *separate* `g_created_resource_ids` map (not merged
  with `g_resource_ids`) tracks real `pfnCreateResource` handles.
- **`CreateVertexBuffer`/`CreateIndexBuffer` do call `pfnCreateResource` after all** — with the
  internal-only formats `D3DFMT_VERTEXDATA` (100) and `D3DFMT_INDEX16`/`32` (101/102), which this
  session's earlier "buffers never call pfnCreateResource" finding (true for every other format) missed
  entirely. `umd_CreateResource` now excludes exactly these three formats from
  `g_created_resource_ids` so those handles keep resolving through the correctly-shaped, correctly-sized
  buffer lazy-bind instead of a wrong-shape, zero-backing texture resource.
- **`ensure_programmable_pipeline`'s vertex layout was hardcoded** to `d3d9_const_test.cpp`/
  `d3d9_shader_test.cpp`'s one shape (`D3DFVF_XYZ|D3DFVF_DIFFUSE`, 16-byte stride) — the host now reads
  `SetStreamSource`'s `Stride` (already carried over the wire, previously discarded) to pick between
  that and the new 20-byte `D3DFVF_XYZ|D3DFVF_TEX1` shape, since there is still no
  `pfnCreateVertexShaderDecl` wiring to learn a real vertex declaration.
- **A real, unresolved `TEXCOORD0` interpolation bug**: with a real `D3DFVF_XYZ|D3DFVF_TEX1` vertex
  format, a quad's U varying interpolated correctly but V consistently did not (isolated via a
  visualize-the-varying diagnostic PS; geometry, the texture, the sampler descriptor, and the PS
  constant register were all independently confirmed correct via the same technique). Not root-caused
  within this task's budget on top of the two required findings above — `d3d9_texture_test.cpp` routes
  UV through the vertex's `D3DFVF_DIFFUSE` color channel instead (`COLOR0.rg`), proven correct by every
  prior guest test.
- **`D3DPOOL_MANAGED` textures create two unrelated DDI resources** for one `CreateTexture()` call
  (`pfnCreateResource(Format=21)` fires twice, with different output handles — one `LockRect()` uses,
  a different one `SetTexture()` uses, which stays empty). Not root-caused or fixed —
  `d3d9_texture_test.cpp` uses `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT` instead (confirmed live to issue
  only one `pfnCreateResource` call).
- **A real Y-flip bug — in the new test itself, not the host.** This pipeline's Vulkan viewport uses
  the unflipped convention (NDC y=-1 at the screen's top). `d3d9_texture_test.cpp`'s own `to_ndc_y`
  helper initially assumed D3D9's opposite screen-space convention; every prior guest test only ever
  checked pixels on the exact vertical center (Y-flip-immune by construction), so this was never caught
  before a test needed an asymmetric row. Fixed in the test, not the host.

### 16.4. Final state

All 4 analytic checks in `d3d9_texture_test.cpp` pass exactly (not approximately): the textured quad's
sampled pixel matches its known texture color exactly; the depth-occlusion pixel matches the nearer
quad's tint exactly, proving the farther quad's fragments were really discarded by the depth test; the
blend pixel matches the analytic `SRCALPHA`/`INVSRCALPHA`/`ADD` formula exactly. Full regression sweep
green: `d3d9-triangle-test`/`-shader-test`/`-const-test` unchanged, smoke test 26/26.

## 17. D3DPOOL_MANAGED fix attempt (2026-07-04) — one layer fixed, a second, deeper one found

Task: root-cause and fix the `D3DPOOL_MANAGED` double-resource-creation bug documented in §16.3
(highest-priority of the three M2-carried bugs — MW2 will very likely hit it, since MANAGED is the
common case for real game asset loading).

### 17.1. Static RE: why two `pfnCreateResource` calls happen

idasql against `d3d9_x64.dll.i64`: `CBaseDevice::CreateTexture` → `CMipMap::Create` →
`CMipMap::CMipMap` (constructor). `CBaseTexture::CanCreateLightWeight` (decompiled) requires
`CBaseDevice::CanDriverManageResource(**(CBaseDevice***)(this+104))` to be true for `D3DPOOL_MANAGED`
(`v11 == 1`) before letting the texture's container and its level-0 surface share ONE driver resource.
`CanDriverManageResource` itself: `(*(this+120) & 0x100) == 0 && (*(this+444) & 0x10000000) != 0`. Both
gates are also used directly inside `CMipMap::CMipMap`'s own constructor logic (3 call sites), confirming
they control whether `CMipMap` allocates its own private sysmem buffer (current behavior) vs. letting
the driver manage it — the same shape of gate `DevCaps` bits `0x02000000`/`0x04000000` already fixed for
`CVertexBuffer::Create`/`CIndexBuffer::Create` (§16.1's index-buffer fix, and the earlier vertex-buffer
one).

### 17.2. Live RE: confirming the double-create + finding the sync call

Wrote `d3d9_managed_texture_test.cpp` (real `D3DPOOL_MANAGED`, no workaround) and instrumented every
device-func-table slot with a labeled stub (temporary, `template<size_t N> labeled_device_stub`,
reverted after use) plus per-call `log_line`s in `umd_CreateResource`/`umd_SetTexture`/`umd_Lock`
(reverted after use — `OutputDebugStringA` reaches the analyzer's console directly via
`on_debug_string`/`console_reporter`, no Python hooking needed for this part). Confirmed live:

```
CreateResource format=21 resource=0x10007   (sysmem "master", created immediately at CreateTexture)
Lock hResource=0x10007 resolved=0x10007     (app's LockRect/UnlockRect)
... (render target / vertex / index buffer creates) ...
stub slot=45 invoked                        (pfnCreateVertexShaderDecl, from SetFVF -- unrelated)
CreateResource format=21 resource=0x1000d   (vidmem copy, created lazily at first bind)
stub slot=18 invoked                        (pfnTexBlt -- fires here, nowhere else, exactly once)
SetTexture stage=0 hTexture=0x1000d         (forwards the VIDMEM copy's handle)
```

Only slot 18 (`pfnTexBlt`) fires between the second `pfnCreateResource` and `pfnSetTexture` — confirming
`pfnTexBlt` is the real sync call. Dumped its raw `D3DDDIARG_TEXBLT` argument bytes (temporary,
reverted) at the call site: `{q0=0x1000d (dst), q1=0x10007 (src), q2=0, q3=0, q4/q5={0,0,640,480}
(a whole-image rect), q6/q7=stack garbage (confirmed unreadable/unmapped when dereferenced as a pointer
in a second independent run — not a hidden data pointer)}`. No raw pixel data crosses in this struct;
the driver is expected to already hold both resources' correct pixel content.

### 17.3. Fix implemented

- `sogen_d3d9_umd.cpp`: `umd_TexBlt` reads `{hDstResource, hSrcResource}` (offsets 0/8, same direct
  resource-id convention as every other real DDI call in this file) and forwards them to a new
  `ioctl_d3d9_tex_blt` wire command. Wired into device-func slot 18 (arity 8 on x86, matching the
  existing `k_device_func_arity` table entry).
- `d3d9-command-protocol/d3d9_command_protocol.hpp`: new `tex_blt_request{dst_resource, src_resource}`.
- `gpu-bridge-protocol/gpu_bridge_protocol.hpp`: new `command::d3d9_tex_blt = 0x909` / `ioctl_d3d9_tex_blt`.
- `windows-emulator/devices/gpu_bridge.cpp`: new `handle_d3d9_tex_blt` dispatch case.
- `windows-emulator/devices/d3d9_host.{hpp,cpp}`: new `d3d9_host::tex_blt(dst, src)` — copies the
  source resource's entire `backing` shadow into the destination's. No GPU re-upload needed here:
  `ensure_texture_uploaded` already re-uploads a texture's `backing` unconditionally on every draw.

### 17.4. A second, deeper bug found while verifying the fix

With the TexBlt fix in place, `d3d9_managed_texture_test.cpp` still failed (sampled pixel stayed
black). Re-instrumented `d3d9_host::unlock`/`tex_blt`/`ensure_texture_uploaded`/`execute_draw` (native
host-side `printf`, temporary, reverted) and found: `unlock resource=0x10007 ... data_size=1228800
first4=00000000` — the app's real magenta pixel writes never reached the host at all; the "sysmem
master" resource's backing was all-zero from the start, so the TexBlt copy faithfully propagated
zeros.

Compared this driver's own `pfnLock` return pointer against the app's own `LockRect()`-returned
`lr.pBits` directly (`log_line` in `umd_Lock`, temporary, reverted): for the D3DPOOL_MANAGED texture,
`pArgs->pData = 0x105afd040` but the app's own printed `pBits = 0x1059bd060` — **different addresses**.
Cross-checked against the render target's own Lock in the same run (a plain, non-MANAGED resource):
driver pData and app pBits both `0x105af9040` — **identical**, confirming this driver's Lock/Unlock
plumbing is correct in general and the mismatch is specific to the MANAGED texture's heavyweight path.

Decompiled `CMipMap::LockRect` → delegates to a per-level object's own virtual `LockRect` (offset+104);
for the non-lightweight path (`CanCreateLightWeight` false) this resolves to `CMipSurface::LockRect` →
`CMipSurface::InternalLockRect`, which calls `CMipMap::ComputeMipMapOffset` to compute the app-visible
pointer from `CMipMap`'s own private buffer (allocated via `MallocAligned`/`LocConstAlloc` in
`CMipMap::CMipMap`'s constructor, confirmed in that function's own decompile) — never touching anything
this driver's `pfnLock` returned. `pfnLock`/`pfnUnlock` still fire (confirmed: always `hr=S_OK`) but are
genuinely vestigial for this resource kind, exactly the same shape of bug already found and fixed for
sysmem-routed vertex/index buffers in §16.1 — except the analogous fix (the right `DevCaps` bit) has not
yet been found for textures.

Live-traced `CanDriverManageResource`'s actual inputs via the Python debugger API (`build/release-py`,
hooking the real RVA `CanDriverManageResource - 0x180000000` inside the loaded `d3d9.dll`): for a real
`CreateDevice`, `this+120 = 0x40` (passes: bit `0x100` clear) and `this+444 = 0xe4608800` (fails: bit
`0x10000000` clear). Tried the obvious candidate fix — adding `0x10000000` to this driver's own
`fill_d3d9caps`'s `DevCaps` DWORD, mirroring the `0x02000000`/`0x04000000` precedent — and re-traced:
**zero effect**, `this+444` stayed exactly `0xe4608800`. This value also doesn't match any combination
of this driver's own `Caps`/`Caps2`/`Caps3`/`DevCaps`/`DevCaps2` bits at all, so `this+444` is not
sourced from anything `fill_d3d9caps` currently populates — its real source is still unidentified.
Finding it would need its own dedicated live-RE session (a memory-write watch on `this+444`, mirroring
the original `dev+460` DevCaps hunt earlier this session) — not completed within this task's budget.

### 17.5. Final state

- **Fixed and verified**: the double-`pfnCreateResource`/`pfnTexBlt` sync mechanism. This is real,
  necessary, correct infrastructure regardless of §17.4 — it's the right thing to do whenever the
  sysmem side does hold real data.
- **Still open**: `pfnLock`/`pfnUnlock` don't deliver real pixel data for a `D3DPOOL_MANAGED` texture's
  sysmem copy at all, because `CanDriverManageResource` fails for a reason not yet identified.
  `d3d9_managed_texture_test.cpp` (new, kept in the tree as the regression vehicle for whoever picks
  this up next) still fails for this reason — sampled pixel reads back black, not the expected magenta.
- **Zero regressions**: `d3d9-triangle-test`/`-shader-test`/`-const-test`/`-texture-test` all unchanged
  and green on both x64 and x86, smoke test still green.

## 18. `CanDriverManageResource`'s real gate traced live — confirmed structurally uncontrollable (2026-07-04)

Follow-up task on §17.4's open finding: trace what actually writes `CBaseDevice+444` (the field
`CanDriverManageResource` tests against `0x10000000`) to determine whether this driver can influence it.
Used sogen's Python debugger API (`build/release-py`, `import sogen`) exactly per the `dev+460` DevCaps
hunt's established methodology (§10.6): `hooks.memory_execution_at`/`hooks.memory_write`, module-load
callbacks to resolve real runtime base addresses (no hardcoded addresses this time — `d3d9.dll`'s and
this driver's own `sogen_d3d9um.dll`'s bases are read live from `on_module_load`).

**Live trace.** Hooked `CBaseDevice::Init`'s entry (RVA `0x1425C`) to confirm `this+444` starts at `0`
(fresh allocation) before `Init`'s own body runs, then armed a wide `memory_write` watch across the
whole per-adapter `_D3D9_DEVICEDATA` blob `Init` memcpy's in (`this+432` .. `+1744`, 1312 bytes — a
narrow 4-byte watch on `this+444` alone caught nothing, the exact same "narrow watch misses it" lesson
as the original `dev+460` hunt; widening to the full copied region is what actually caught the write).
Separately, to see the value's real origin (not just its arrival at `CBaseDevice`), resolved this
driver's own `umd_GetCaps` runtime address without any hardcoding: hooked the exported `OpenAdapter`'s
entry (address read from `sogen_d3d9um.dll`'s own export table via the Python binding's
`MappedModule.exports`), captured its return address off `[RSP]` at entry, hooked that return address,
and read `pArgs->pAdapterFuncs->pfnGetCaps` directly out of guest memory once `OpenAdapter` had filled
it in. Armed a narrow `memory_write` watch on that exact `D3DCAPS9::Caps2` field (`pData+12`) the moment
`umd_GetCaps(Type=SOGEN_D3DDDICAPS_GETD3D9CAPS)` is entered (before `fill_d3d9caps` runs), and watched
every subsequent write to that same address for the rest of the run. Full observed write chain for one
real `CreateDevice`:

```
(zeroing, ucrtbase.dll memset, several 1-byte writes)
sogen_d3d9um.dll+0x16ca   -> 0x60020000   (this driver's own fill_d3d9caps: DYNAMICTEXTURES|FULLSCREENGAMMA|CANAUTOGENMIPMAP)
d3d9.dll+0x1588f          -> 0xe4428800   ((driver_Caps2 & 0x7B9F77FF) | 0x84408800)
d3d9.dll+0x158b3          -> 0xe4628800   (|= 0x200000 conditionally, then & 0xEFFFFFFF unconditionally)
d3d9.dll+0x15a1f          -> 0xe4608800   (FULLSCREENGAMMA bit forced by a separate GetCaps(Type=34) query)
```

`0xe4608800` is exactly the value `CanDriverManageResource` reads at `this+444` (matches §17.4's
independently-captured value exactly). idasql confirms all three `d3d9.dll` writes live inside one
function, `QueryLHDDICaps` (RVA range `0x15780`-`0x1C000`-ish; `LH` = the WDDM/"LonghornDDI" caps path,
taken because `IsLHDriverModel` recognizes this driver as a real D3DDDI driver — the same function has
an entirely separate, earlier `!IsLHDriverModel` branch calling `SwDDIMungeCaps` instead, for legacy
XPDM-style drivers, which this driver never takes). The decisive line, decompiled directly:

```c
/* 18001588A */ v27 = *(_DWORD *)(a3 + 12) & 0x7B9F77FF | 0x84408800;
/* 1800158B3 */ *(_DWORD *)(a3 + 12) = v27 & 0xEFFFFFFF;
```

`0xEFFFFFFF` has every bit set except bit 28 (`0x10000000` = `D3DCAPS2_CANMANAGERESOURCE`). This AND is
unconditional — it runs on every `CreateDevice`, for every driver that reaches this branch, regardless of
what the driver's own `GetCaps` reported. **Empirically re-verified, not just decompiled**: temporarily
added `D3DCAPS2_CANMANAGERESOURCE` to this driver's own `fill_d3d9caps` (`Caps2 |= 0x10000000`), rebuilt
just the x64 UMD, restaged, and re-ran the same live trace. This driver's own write now showed `0x70020000`
(bit 28 set); `d3d9.dll+0x1588f`'s write showed `0xf4428800` (bit 28 *survives* that step, confirming the
first AND/OR pair doesn't touch it); `d3d9.dll+0x158b3`'s write showed `0xe4628800` — bit 28 **stripped**,
caught live, in the act — settling at the exact same final `0xe4608800` as the unmodified baseline.
Reverted immediately (`git diff --stat src/` empty afterward) since this isn't a valid fix.

Also checked `CMipMap::CMipMap`'s constructor (idasql decompile) for any driver-supplied-pointer
fallback that might route around this gate at `CreateResource` time — none exists. The private sysmem
buffer at `this+280` is unconditionally a plain `MallocAligned` heap allocation whenever
`CanDriverManageResource` is false, with no code path that ever consults a driver-provided resource or
pointer first.

**Conclusion, per the task's own escalation clause ("if the real mechanism turns out to be genuinely
outside this driver's control ... that's an acceptable, honest conclusion to report, not a failure"):**
`CanDriverManageResource` cannot be made to return `true` by any `D3DCAPS9` field this (or any) D3DDDI/WDDM
driver reports. `d3d9.dll`'s own `QueryLHDDICaps` hardcodes `D3DCAPS2_CANMANAGERESOURCE` off for every
driver on the modern (`IsLHDriverModel`) DDI path — consistent with real D3D9/WDDM history (WDDM's video
memory manager owns residency; the old XPDM-era driver-managed-resource model was retired at the OS
level, not per-driver). No source change was made or is possible at this gate; `sogen_d3d9_umd.cpp` is
unchanged from `6f121fa7`. `d3d9_managed_texture_test.cpp` still fails exactly as in §17.4 (sampled pixel
black, not magenta) — zero regression on every other test (x64 `triangle`/`shader`/`const`/`texture` and
x86 `triangle`/`shader`/`const`/`texture`, plus the 26/26 smoke test), all re-verified green.

A real fix for the underlying symptom (MANAGED-pool texture sampling black) would need a different
mechanism entirely — since `pfnLock`/`pfnUnlock` are structurally never given the app's real pixel data
for this resource kind (confirmed in §17.4), the only way real pixel bytes could ever reach this driver
is through whatever DDI call the real WDDM D3D9 pipeline actually uses to push a `D3DPOOL_MANAGED`
texture's sysmem content into video memory — almost certainly a genuinely different/fuller RE of
`pfnBlt`/`pfnTexBlt`'s argument struct (this driver's current `D3DDDIARG_TEXBLT` RE only found bare
resource handles + a rect, no system-memory source pointer field) or a DDI call not yet identified at
all. That is a materially bigger investigation than this task's scope (tracing one caps gate) and is not
attempted here.

## 19. `pfnTexBlt`'s real argument struct fully RE'd — no data pointer exists, confirmed unfixable (2026-07-04)

User-ordered follow-up gate on §18's own closing lead: does `pfnTexBlt`'s REAL argument struct carry more
than the two resource ids §17.2 found, specifically a pointer to the MANAGED texture's sysmem "master"
copy's real pixel data? §17.2's own byte dump had already looked past offset 8 (`q2..q7`, i.e. bytes
16-63) and found only a whole-image rect plus what looked like stack garbage, but never pinned that down
against the real caller's decompiled source — this task closes that gap properly.

### 19.1. Live trace: capturing pfnTexBlt's real caller

Used sogen's Python debugger API (`build/release-py`, `import sogen`) to hook `umd_TexBlt`'s own entry
(resolved via `nm`/`objdump` on the built `sogen_d3d9um.dll`, no hardcoded addresses — RVA `0x26b0`,
combined with the real runtime module base from `on_module_load`). At the hook, read `RDX` (the real
`pArgs` pointer, per this file's established `hDevice=RCX`/`pArgs=RDX` convention) and `[RSP]` (the
return address, since the hook fires before the callee's own prologue executes) to get the exact
call site inside `d3d9.dll`. One real `d3d9-managed-texture-test` run: `pArgs=0x10187f8e8`,
`return_addr=0x1049712be` → RVA `0x312be` in `d3d9.dll` (base `0x104940000`).

Dumped 256 bytes at `pArgs` (32 qwords) — `q0=0xe`/`q1=0x8` (the two resource ids, matching §17.2's
finding with this run's own resource-id numbering), `q2/q3=0` (a subresource-derived field, see below),
`q4/q5` decode to the rect `{0, 640, 480, 0}` (a whole-image rect, matching §17.2), and `q6` onward
(`0x0000539fd7fb6634`, `0x0000000080004005`, `0x0000000103bda4c0`, ...) look superficially pointer-shaped
in places (`q8`/`q12`/`q15` resemble live heap addresses, `q10`/`q14` resemble code addresses inside
`d3d9.dll`) — exactly the kind of ambiguous garbage that could be mistaken for a hidden data pointer if
this stopped at an empirical byte dump, which is exactly where §17.2 stopped.

### 19.2. Static RE: decompiling the real caller settles it definitively

idasql against `d3d9_x64.dll.i64`, `SELECT decompile(0x1800312B0)` — the containing function at RVA
`0x312be`/base `0x1800312B0` is `CD3DDDIDX10::TexBlt(void* hDstResource, void* hSrcResource,
tagPOINT* pDstPoint, RECTL* pSrcRect)` (mangled name decoded the real 4-argument signature directly). Its
decompiled body builds a **48-byte** local stack struct (`v14`/`v15`/`v16`/`v17`/`v18`, contiguous from
`rsp+0x28` to `rsp+0x58`) entirely from its own four parameters, then passes a pointer to it straight to
the real device-func-table slot (`(*(func)(v9+144))(*(this+196), v14)` — `144 = 18*8`, i.e. slot 18,
`pfnTexBlt` itself, matching this driver's own slot assignment exactly):

```c
if ( a2 )                                  // a2 = hDstResource
{
    v14[0] = *(_QWORD *)a2;                // offset  0: hDstResource (dereferenced once)
    v15 = a2[2] / a2[3];                   // offset 16: a resource-wrapper-derived index
}
else { v14[0] = 0; v15 = 0; }
v14[1] = *a3;                              // offset  8: hSrcResource (a3, dereferenced unconditionally)
v16 = (__int64)*a4;                        // offset 20: *pDstPoint   (tagPOINT, 8 bytes: x,y)
v17 = (__int128)*a5;                       // offset 28: *pSrcRect    (RECTL, 16 bytes: L/T/R/B)
v18 = 0;                                   // offset 44: always zero
```

Every one of the 48 bytes is now accounted for from the real function's own decompiled source — not an
empirical guess. There is **no pixel-data pointer anywhere in this struct**: `a2`/`a3` (the two resource
handles) are themselves opaque wrapper-object pointers that get reduced to a single dereferenced `QWORD`
each before crossing into the struct; the rest is a subresource index, a destination point, a source
rect, and a zero. The ambiguous-looking `q6` onward bytes from §19.1's live dump are conclusively **not**
struct fields at all — `TexBlt`'s own 48-byte local only extends to `rsp+0x58`; everything past that in
the raw dump is leftover stack content from unrelated earlier call frames (explaining why some of it
looked pointer-shaped: real heap/code addresses genuinely were sitting there, just not put there by
`TexBlt`). Added `D3DDDIARG_TEXBLT` as a proper typed struct to `d3d9_ddi.hpp` (`static_assert`-pinned)
documenting this exact layout.

### 19.3. Full live call-sequence trace: no other DDI call carries pixel data either

Per the task's own escalation clause, also traced the **entire** `d3d9_managed_texture_test` run's real
DDI call sequence, not just the narrow window around `TexBlt` already known. Captured `pDeviceFuncs`
(parsed live off this driver's own existing `CreateDevice reached ... pDeviceFuncs=%p` debug string),
dumped all 143 device-func-table entries, and hooked every *unique* address among them (x64 routes every
still-unimplemented slot through one shared, zero-arg `device_stub`, so hooking by slot index instead of
by unique address over-counts — corrected by hooking each unique code address once and disambiguating
shared hits by the return address's own call-site instruction, decoded via idasql, e.g. `mov rax,
[rax+108h]; call cs:__guard_xfg_dispatch_icall_fptr` → offset `0x108/8 = slot 33`).

Full result: `CreateResource` ×10 (every resource the test creates: both texture copies, vertex/index
buffers, render target, etc.), `pfnDestroyResource` ×10 (unimplemented — resolved via
`DdDestroySurfaceLH`'s own decompiled offset, teardown only, no data), `Lock`/`Unlock` ×4 each (texture +
vertex buffer + index buffer + the final render-target readback), `TexBlt` ×1 (§19.2 above),
`SetTexture` ×16, `SetStreamSource` ×16, `SetPixelShaderConst` ×58, `SetRenderState` ×94,
`SetTextureStageState` ×274, `SetViewport`/`SetZRange` ×3 each, `SetScissorRect` ×2, `pfnSetClipPlane`
×12 (unimplemented, resolved via offset math, fixed-function clip-plane defaults, unrelated),
`pfnUpdateWInfo` ×1 and `pfnCreateVertexShaderDecl` ×1 (both unimplemented, matching §17.2's own
"`pfnCreateVertexShaderDecl`, from SetFVF — unrelated" finding), plus the expected one-shot shader/
render-target/clear/draw calls. Every single call is accounted for; none of them — implemented or
stubbed — carries texture pixel bytes for the MANAGED resource. (`Present` never fires: this test reads
back its render target directly via `LockRect`, it never calls `IDirect3DDevice9::Present`.)

### 19.4. Conclusion

Per the task's own escalation clause ("if genuine, thorough investigation shows no viable path exists
through this driver's own DDI surface — that's an acceptable, honest conclusion to report, not a
failure"): **no fix is possible through this driver's DDI surface.** `pfnTexBlt`'s real argument struct
(now fully decompiled, not just empirically dumped) carries no pixel-data pointer, and a full live trace
of this test's entire DDI call sequence confirms no other call this driver receives carries one either.
Combined with §18's finding (the app's own `LockRect`/`UnlockRect` writes never reach this driver at all
for a MANAGED resource), the real pixel data for a `D3DPOOL_MANAGED` texture's sysmem master copy is
**structurally never exposed to any D3DDDI/WDDM driver through any DDI call for this resource kind** —
`d3d9.dll` keeps it entirely inside its own private `CMipMap` buffer, end to end, by design. This closes
the investigative loop opened across Tasks 4/4b/4c: three independently-verified, real architectural
findings, and a final, honest negative result rather than a fabricated fix.

Updated `d3d9_ddi.hpp` (new typed `D3DDDIARG_TEXBLT`), `sogen_d3d9_umd.cpp` (`umd_TexBlt`'s own comment
rewritten with the full trail and conclusion), `d3d9_managed_texture_test.cpp`'s header comment, and
`docs/d3d9-roadmap.md`'s `D3DPOOL_MANAGED` entry. `d3d9_managed_texture_test.cpp` still fails exactly as
before (sampled pixel black, not magenta) — this is now a confirmed, permanent limitation, not an open
lead. Zero regression: every other guest test (x64 `triangle`/`shader`/`const`/`texture` and x86
`triangle`/`shader`/`const`/`texture`), plus the smoke test, all re-verified green.

### 19.5. Code-review fix round: x86 layout, typed struct, and stale README (2026-07-04)

A code-quality review of this task's first commit caught two real issues and one stale-docs issue,
addressed here:

- **Critical, real bug**: `umd_TexBlt` was wired into `slots[18]` unconditionally (not inside this
  file's existing `#ifdef _WIN64`/`#else` split for the rest of the stub table), but its body hardcoded
  8-byte reads for both resource handles. §19.2's own struct comment had explicitly called the x86 shape
  "deliberately left unmodeled" — but nothing actually gated the handler off for x86, so a real x86
  `D3DPOOL_MANAGED` texture would have read `hSrcResource` from the wrong offset (8, not 4) and corrupted
  both resource ids. Fixed properly, not by gating: idasql-decompiled the real 32-bit
  `CD3DDDIDX10::TexBlt` (`d3d9_x86.dll.i64`, address `0x100656d0`) the same way §19.2 decompiled the x64
  one, and found the real 40-byte x86 layout — the exact x64 shape with every `HANDLE` shrunk to 4 bytes
  and every later offset shifted down by 8 (`hDstResource@0`, `hSrcResource@4`, `DstSubResourceIndex@8`,
  `DstPointX@12`, `DstPointY@16`, `SrcRect@20`, `Reserved@36`) — an independent decompile, not a guessed
  extrapolation. Added this as the `#else` branch of `D3DDDIARG_TEXBLT` in `d3d9_ddi.hpp`
  (`static_assert(sizeof == 40)`).
- **Important**: `umd_TexBlt` was refactored to take `CONST D3DDDIARG_TEXBLT* pArgs` (matching every
  other handler in this file, e.g. `umd_SetIndices`/`umd_Clear`/`umd_SetRenderTarget`) instead of `void*`
  + raw `memcpy` against hardcoded byte offsets — this is also the direct fix for the critical bug above,
  since the typed struct now carries the x64/x86 size difference itself instead of a hand-copied
  constant. Rebuilt both `sogen_d3d9um-x64.dll` and `sogen_d3d9um-x86.dll` and confirmed via `objdump`
  disassembly that the generated code reads the right offsets on each arch: x64 does one 16-byte
  `movdqu` load at offset 0 (both handles, offsets 0/8); x86 does `mov (%eax),%edx` (offset 0) then
  `mov 0x4(%eax),%eax` (offset 4) — exactly matching the new struct, not the old hardcoded 8. Re-ran the
  full guest-test regression sweep (x64 `triangle`/`shader`/`const`/`texture`/`managed-texture`, x86
  `triangle`/`shader`/`const`/`texture`, 26/26 smoke test) — all still green, `managed-texture` still
  fails exactly as documented (no behavior change, since the x64 path's bytes are identical either way
  and no x86 `D3DPOOL_MANAGED` test exists yet to exercise the corrected x86 offsets directly).
- **Important**: `README.md`'s `D3DPOOL_MANAGED` entry still said the `CanDriverManageResource` failing
  field "is not yet identified... finding it needs its own dedicated live-RE session" — stale since §18
  (which found and confirmed the exact root cause) and never touched by this task's own first commit.
  Rewritten to state the final, three-layer, confirmed-unfixable conclusion, matching
  `docs/d3d9-roadmap.md`/`HANDOFF_MACBOOK.md`/`umd_TexBlt`'s own comment.
- **Minor (adopted)**: `d3d9_managed_texture_test.cpp`'s failure line now reads "EXPECTED FAILURE (known,
  permanent limitation...)" instead of a bare "FAIL", to read as documented-and-understood rather than a
  fresh regression at a glance. New convention for this one test, not applied elsewhere.

---

## 20. TEXCOORD0 varying-interpolation "bug" investigated — does not reproduce, no host fix needed (2026-07-04)

Task: root-cause and fix the `TEXCOORD0` varying-interpolation bug documented in §16.3/`docs/d3d9-roadmap.md`
(genuine `D3DFVF_XYZ|D3DFVF_TEX1` + `TEXCOORD0` PS input; U interpolated correctly, V consistently did
not). Concrete starting lead: `d3d9_shader_translator.cpp` passes `varying_map_info` to the VS
`compile_stage` call but `nullptr` to the PS one.

### 20.1. The lead investigated and confirmed to be correct-as-is, not a bug

Read `vkd3d_shader.h`'s own doc comment for `vkd3d_shader_varying_map_info`: "This mapping should be
used ... to compile the **first** shader" (the varying-producing stage). Traced the actual mechanism in
`deps/vkd3d/libs/vkd3d-shader/ir.c`'s `vsir_program_remap_output_signature`: it remaps the *compiling
stage's own output signature* target locations to match the next stage's input register indices — it is
never meant to be attached to the consuming (PS) side at all. Confirmed the gate directly: `ir.c`'s
`vsir_program_transform` only runs this transform `if (program->shader_version.type != VKD3D_SHADER_TYPE_PIXEL)`
— vkd3d-shader itself unconditionally skips it for pixel shaders, varying_map_info present or not,
because a PS has no "next stage" to remap its output for. `d3d9_shader_translator.cpp`'s asymmetry
(VS gets the map, PS gets `nullptr`) is therefore correct, documented API usage, not an oversight.

### 20.2. Empirical confirmation: passing the map to the PS call too is a no-op

Temporarily changed the PS `compile_stage` call to pass `&varying_map_info` instead of `nullptr`,
rebuilt, and re-ran both a scratch diagnostic test and the full `d3d9-texture-test.exe` suite: **byte-
identical rendered pixels** in both cases (same HRESULTs, same pixel values to the last bit). This
matches §20.1's source-level finding exactly — the change is empirically inert, not just theoretically
so. Reverted the change; kept a durable comment at the `nullptr` call site explaining why, referencing
this section.

### 20.3. Reproducing the original bug — it does not reproduce today

Wrote a scratch diagnostic test (`texcoord_diag_test.cpp`, not committed) mirroring the exact technique
the original report used (`return float4(input.uv, 0, 1)`, visualizing the raw interpolant), using a
real `D3DFVF_XYZ|D3DFVF_TEX1` vertex format and genuine `TEXCOORD0`. Two scenarios, both against the
*current*, unmodified host:
- A full-canvas quad, sampled at 5 points spanning all four screen quadrants plus center: U and V both
  read back within +-1/255 of the exact expected value (`col/640`, `row/480`) at every point, including
  asymmetric (non-center) locations.
- A small partial quad at the exact screen rect (`40,40`-`240,200`) the original `d3d9_texture_test.cpp`
  quad 0 uses, sampled at local UV `(0.25, 0.25)` and `(0.75, 0.75)` — **the exact two figures the
  original bug report cited** ("expected 0.25 reads back ~0.87, expected 0.75 reads back ~0.37"). Actual
  readback: `(0.2510, 0.2549)` and `(0.7529, 0.7529)` — both U and V correct, no discrepancy at all.

The bug simply does not reproduce against the current host, with the exact geometry and exact UV values
originally cited.

### 20.4. Most likely explanation

This session separately found and fixed "a real Y-flip bug — in the new test itself, not the host"
while building `d3d9_texture_test.cpp` (§16.3's last bullet): this pipeline's Vulkan viewport uses the
unflipped NDC convention (y=-1 at the screen's top), and an early version of that test's own `to_ndc_y`
helper assumed D3D9's opposite convention, which only ever surfaced on asymmetric (non-center-row)
pixel checks. The original `TEXCOORD0` diagnostic (a separate, earlier, not-committed scratch test) was
never re-checked against the corrected convention — an inverted screen-Y-to-NDC mapping in a test's own
geometry placement produces exactly a "U reads fine, V reads a value that isn't a simple flip of what's
expected" symptom (screen position, not the interpolated value itself, ends up wrong), without touching
varying interpolation at all. This is circumstantial (the original scratch test no longer exists to
re-run directly), but it is the only hypothesis consistent with every piece of live evidence gathered:
the varying-map mechanism is confirmed correct by source and by empirical no-op testing, and the
interpolation itself is confirmed correct by direct reproduction using the report's own cited figures.

### 20.5. Outcome

No host-side code change was needed or made (the one experimental change was reverted; only an
explanatory comment was added). Added `d3d9_texcoord_test.cpp` as permanent regression coverage: a real
`D3DFVF_XYZ|D3DFVF_TEX1` + `TEXCOORD0` quad, real `tex2D()` sampling (not the diagnostic-PS technique),
checked at all four UV-quadrant combinations (`u,v` = `0.25`/`0.75` each) so a swapped or one-axis-broken
interpolant would fail at least one check. Passes exactly on both x64 and x86. `d3d9_texture_test.cpp`'s
`D3DFVF_DIFFUSE`-packed UV workaround is left unchanged (still independently proven correct); it's no
longer strictly necessary but there's no reason to remove a working, already-verified path.

Full regression sweep after this task: x64 `spike`/`shader`/`const`/`texture`/`managed-texture`/
`texcoord`, x86 `triangle`/`shader`/`const`/`texture`/`texcoord`, all pass exactly as before (`managed-
texture` still fails exactly as documented, its own known permanent limitation). Smoke test 26/26.

---

## 21. Task 6 — partial-buffer `Lock()` support designed and implemented, permanent limitation from
    §16.1 resolved (2026-07-04)

§16.1 left a documented permanent limitation: `umd_Lock` always treated every vertex/index buffer lock
as an implicit whole-buffer lock (offset 0, size unknown), because `D3DDDIARG_LOCK`'s
`OffsetToLock`/`SizeToLock` don't have one routing-path-independent struct offset, and `umd_Lock` is one
function shared by every resource kind and routing path with no apparent way to tell which shape a given
call used. This task revisited that conclusion and found the real per-call detection question doesn't
actually need answering.

**Key realization: the "ambiguous shape" only matters for calls whose result is discarded anyway.**
§16.1's own evidence already showed that "sysmem-routed" buffer locks (`CVertexBuffer::Lock`/
`CIndexBuffer::Lock`'s own direct dispatch) call `pfnLock` for real, but the app never uses this
driver's returned `pData` for that path -- it reads/writes through the runtime's own separate,
pre-allocated system-memory shadow instead. So an unrelated value read from `OffsetToLock`'s struct
offset in that shape is harmless: nothing dereferences the pointer this driver computes from it. The
only routing path where this driver's own `pData` genuinely matters is "driver-routed"
(`CDriverVertexBuffer::Lock`/`CDriverIndexBuffer::Lock` -> `LockI`) -- and this UMD's own DevCaps bits
(`k_devcaps_driver_managed_pool`/`k_devcaps_driver_managed_index_pool`, both unconditionally set) make
that the routing every real `D3DPOOL_DEFAULT` vertex/index buffer takes, which is the common case
(including every `D3DLOCK_NOOVERWRITE` growing-buffer append). So `umd_Lock` can safely read
`OffsetToLock` (offset 80 on x64, named for the first time in `d3d9_ddi.hpp`) unconditionally for every
buffer resource, without needing to distinguish which shape actually produced a given call -- and the
host's own `lock()` already rejects an out-of-range offset defensively, so a garbage value from the
"other" shape can't misbehave even in the case that's supposed to be harmless anyway.

**`SizeToLock` genuinely isn't needed, not just hard to read.** The wire protocol
(`d3d9_command_protocol.hpp`'s `lock_request`/`unlock_request`) and `d3d9_host::lock`/`unlock` already
treat `size=0` as "from `offset` to the end of the resource" -- discovered to already be fully,
correctly implemented host-side, needing zero host changes for this task. That is exactly the right
semantics for `D3DLOCK_NOOVERWRITE`: the app only ever writes forward from `OffsetToLock` anyway, so
there is no need to know how much it wrote in advance.

**Implementation** (`sogen_d3d9_umd.cpp`): `umd_Lock` now forwards the real `OffsetToLock` (buffers
only -- detected the same way `resolve_buffer_resource_id` already distinguishes a lazily-bound buffer
handle from an already-`pfnCreateResource`-registered texture/render-target/depth-stencil handle, since
`LockRect` uses this same struct region for Rect/Box input, not a byte offset) as the wire protocol's
own `lock_request::offset`, so `g_locked_buffers` now holds only `[offset, end)` of the resource per
outstanding lock instead of the whole thing. A new `g_locked_offsets` map remembers each lock's offset
so `umd_Unlock` writes its data back to the same place. `resolve_buffer_resource_id`'s lazy-bind size
hint now uses `OffsetToLock` as a lower bound (`std::max(byte_size, 64*1024)`) instead of always
defaulting to a flat 64 KiB, in case a never-before-seen buffer's first lock needs more than that. x86
keeps the pre-fix whole-buffer-lock behavior unchanged, since its driver-routed `OffsetToLock` offset
isn't RE-verified yet (see `d3d9_ddi.hpp`'s x86 `D3DDDIARG_LOCK` comment) -- a real per-path fix there
would need the same kind of live-RE pass §16.1/Task 6's x64 work already did, not attempted this task.

**Verified with a new guest test**, `d3d9_partial_lock_test.cpp`: fills a 256-byte chunk with a
`D3DLOCK_DISCARD` lock (offset 0), then appends two more 256-byte chunks at increasing nonzero offsets
with `D3DLOCK_NOOVERWRITE`, each with a distinctive byte pattern (0xAA/0xBB/0xCC); a final whole-buffer
read-only Lock confirms all three chunks still hold exactly their own pattern. This is a pure D3D9-API-
level check (no host-side backdoor needed): since driver-routed buffer locks hand the app this driver's
own `pData` directly, the pre-fix bug (offset always mapped to 0) would have made the second lock's
write land at the buffer's start instead of its real offset, corrupting chunk 0 -- exactly what this
test would catch. Result: `PASS: chunk0/1/2 intact`, `ALL CHECKS PASSED`.

Full regression sweep after this task: x64 `spike`/`shader`/`const`/`texture`/`managed-texture`/
`texcoord`/`triangle`/`triangle-x64`, x86 `triangle`/`shader`/`const`/`texture`/`texcoord`, all pass
exactly as before (`managed-texture` still fails exactly as documented, its own unrelated known
permanent limitation). Smoke test 26/26.

---

## 22. Int (`i#`) / bool (`b#`) shader constant registers — designed, wired, and proven pixel-exact on both x64 and x86 (2026-07-05)

Plan `jazzy-giggling-cloud.md` (session-local, not checked into this repo), Tasks 1-5. Closes the last
open item under "Constant registers" in `docs/d3d9-roadmap.md` — the float (`c#`) path was already done;
this extends the same design to int and bool registers, the ones real shader flow control (loops,
branches) depends on.

### 22.1 Design phase — vkd3d-shader RE findings

Before writing any code, the binding scheme had to be settled by reading how vkd3d-shader's D3DBC
frontend actually consumes constant registers, not by guessing. Key findings (`deps/vkd3d/libs/
vkd3d-shader/`):
- Each constant register **bank** (float `c#`, int `i#`, bool `b#`) is a **separate CBV**, keyed by its
  own `register_index` space starting at 0 — a D3DBC shader that reads `c0`/`i0`/`b0` produces three
  independent constant-buffer reads, not three offsets into one buffer. This is why the original roadmap
  text speculated "up to 4 descriptor sets" (one per bank, times two stages) — a reasonable worst-case
  guess before checking the actual binding granularity vkd3d-shader expects.
- **The locked design is narrower**: vkd3d-shader binds CBVs *within whichever descriptor set the
  calling stage already owns* — it doesn't need or want a set per bank. Since the float (`c#`) path
  already committed to 2 sets (VS = set 0, PS = set 1, binding 0 = float CBV, PS-only binding 1 =
  combined-image-sampler), int and bool just needed two more bindings *in the same two sets*: binding 2
  = int CBV, binding 3 = bool CBV, per stage. No new descriptor sets at all.
- **Both int and bool constants use a 16-byte (std140-style) per-register stride**, matching float's
  existing `float4`-per-register layout — this was not obvious a priori for bool (a single register only
  ever needs 1 bit of real information) but matches how `SetVertexShaderConstantB`'s own D3D9 API shape
  works (`BOOL* pConstantData, UINT BoolCount` — one `BOOL` per logical register, no packing) and keeps
  the host-side storage/wire-protocol code identical in shape to the already-proven float path.
- **Bool convention: non-zero is true.** D3D9's own `BOOL` is a 32-bit int where the API contract is
  "any non-zero value is TRUE" (not strictly `1`) — `vs_const_b`/`ps_const_b` are stored host-side as
  `uint32_t` (mirroring the wire's raw 32-bit `BOOL` payload) and expanded to the 16-byte CBV stride
  unchanged, rather than being normalized to a strict 0/1. vkd3d-shader's own SPIR-V codegen for the D3DBC
  `IF`/`IFC` opcodes already treats the CONSTBOOL operand this way (a `!= 0` comparison, not `== 1`), so
  no host-side normalization was needed for correctness.

### 22.2 Implementation (Tasks 1-3, already committed as `c8847dc6`/`d8cc98df`/`32fdb6e5`)

- Wire protocol: two new opcodes (`set_vertex_shader_const_i`/`_b`, `set_pixel_shader_const_i`/`_b`,
  mirroring the existing float opcodes' `{Register, Count}` header + trailing data array shape).
- `device_state`: `vs_const_i`/`ps_const_i` stored as `int32_t`, `vs_const_b`/`ps_const_b` stored as
  `uint32_t`, both expanded to the 16-byte-per-register stride at write time (matching `vs_const_f`'s
  existing shape).
- Host-side: 2 more UBO buffers per stage (int, bool) and 2 more descriptor bindings per set (2, 3),
  wired into `vulkan_host`'s existing per-draw descriptor-set update path alongside the float CBV and
  (PS-only) sampler.
- UMD: `umd_SetVertexShaderConstI`/`ConstB`/`umd_SetPixelShaderConstI`/`ConstB` added to
  `sogen_d3d9_umd.cpp`, wired to `D3DDDI_DEVICEFUNCS` slots 48/49/65/66 (already present as "real"
  entries in `k_device_func_arity`, arity 12 — `(HANDLE, header*, trailing CONST INT*/BOOL*)`, the same
  shape as the already-proven float `pfnSetVertexShaderConst`/`pfnSetPixelShaderConst`).

### 22.3 A real host bug found while building the guest test (Task 4, fixed in `67e6acff`)

`d3d9_int_bool_const_test.cpp`'s very first run got `SetVertexShaderConstantB hr=0x00000000` and
`SetVertexShaderConstantI hr=0x00000000` (the DDI calls succeeded) but the rendered pixel showed neither
constant had actually reached the shader (default/unset values). Root cause: `gpu_bridge.cpp`'s IOCTL
dispatch `switch` — the function that routes an incoming D3DKMT Escape's opcode to the right host-side
handler — had no `case` for the two new int/bool opcodes at all. They fell through to the `default` path
silently (no error returned, since the runtime doesn't require every escape to do anything), so the UMD's
DDI calls genuinely reached the driver and returned `S_OK`, but the host never decoded or stored the
payload. Fixed by adding the missing `case` labels routing to the same decode-and-store path Task 2 had
already implemented. This is a genuinely different bug class from the WoW64/x86 struct-layout bugs found
earlier in this project (§15, §16.1-16.3) — a dispatch-routing gap, not an ABI mismatch — and would not
have been caught by an HRESULT-only test, only by actually reading back a rendered pixel.

### 22.4 Three `d3dcompiler_43` compiler quirks found while shaping the test shader (Task 4c, fixed in `1e851fc2`/`478e0372`)

Getting `d3d9_int_bool_const_test.cpp`'s vertex shader to compile into bytecode that actually exercised
the real `b0`/`i0` registers (rather than something the compiler could optimize away) took three rounds
of empirical D3DBC disassembly, documented in full in the test file's own header comment:
1. `bool x : register(b0);` is rejected by d3dcompiler_43 for `vs_2_0`/`vs_2_a` (error X4509) — a scalar
   bool used in a runtime `if` must have no explicit register annotation; the compiler auto-allocates it
   (empirically confirmed to land at `b0`, matching `SetVertexShaderConstantB(0, ...)`).
2. An `if (b) { X } else { Y }` shape where both branches merge into one shared trailing write gets
   **flattened by the compiler into `SGE`/`MAD` select-style arithmetic backed by an auto-allocated FLOAT
   (`c#`) register — not the real `b0` CONSTBOOL bank at all**. This was caught red-handed: an earlier
   version of the test used exactly this shape, compiled and ran successfully, but always rendered the
   "false" branch regardless of the runtime `SetVertexShaderConstantB(0, TRUE, 1)` call — a false negative
   that would have gone unnoticed without disassembling the bytecode and finding zero CONSTBOOL operands
   anywhere. Fixed by giving each branch an early `return` instead — not flattenable, and empirically
   forces a genuine D3DBC `IF` instruction whose operand is the real `b0` register (opcode 0x28, operand
   register type 0x0E, number 0).
3. Even with the early-return shape, a narrower quirk remained: if either branch's output color literal
   contains an exact `0.0`/`1.0` in a component, the optimizer pulls just that component out of the real
   `IF`/`ELSE` and recomputes it via `SGE dst, -c#, c#` against a **separate, auto-allocated FLOAT
   constant register that is never `b0` and that this test's own `SetVertexShaderConstantB` call never
   populates** — confirmed via full D3DBC disassembly (a genuine unrelated `c#` register fed into an SGE
   against its own negation) and independently via the shader's own CTAB reflection block, which lists
   **two separate constant-table entries both named `useAltColor`** — one `D3DXRS_BOOL`, one
   `D3DXRS_FLOAT4` — for the same HLSL variable. This is a genuine, reproducible-on-real-hardware
   `d3dcompiler_43` compiler quirk, not a sogen or vkd3d-shader bug (the *other* components in the same
   instructions, not exact `0.0`/`1.0` literals, are correctly gated by real `b0`-conditional branches the
   whole time). Fixed by using `0.999`/`0.001` instead of the exact `0.0`/`1.0` pair — round-trips through
   the 8-bit pixel format identically (within the test's ±2 tolerance) but doesn't trigger the shortcut.
   `d3dcompiler_43.dll` is a pinned filesystem asset (not rebuilt from source), so this exact optimizer
   behavior is stable across runs — no recompiler-version-drift risk.

The finished test shader: a bare (no `register()` annotation) `bool useAltColor`, an `int4 loopTripCount
: register(i0)`, an early-return `if`/else selecting between two colors via `0.999`/`0.001` channel
values, and a `for (k < loopTripCount.x)` loop accumulating into the blue channel. Compiled `vs_2_0`
bytecode is walked as raw D3DBC tokens (opcode in the low 16 bits, length in bits 24-27, per
`vkd3d-shader`'s own `d3dbc.c` shifts/masks) to independently confirm a real `REP`/`ENDREP` pair (0x26/
0x27) and a real `IF` (0x28) reading register type 0x0E (CONSTBOOL) number 0 — not just that HRESULTs
came back clean.

### 22.5 Task 5 — x86/WoW64 port: pixel-exact parity, no new architecture bug

Cross-compiled `d3d9_int_bool_const_test.cpp` unchanged to i686 (`i686-w64-mingw32-g++`, same flags as
every other x86-ported test) and staged it against the already-present genuine 32-bit `d3d9.dll`/
`d3dcompiler_43.dll` in `syswow64/`. The first run genuinely failed: `pixel(320,240)=B=00 G=FF R=00`
(the "false"-branch default color, with the loop accumulator at 0) — i.e. **both** the bool and int
constants silently read back as their never-set defaults, both analytic checks failing.

This looked exactly like the shape of the two previous real x86-only bugs this project found
(`allocate_id()`'s `HANDLE` truncation, §16; `D3DDDIARG_CREATERESOURCE`'s x86 offset, §16.3), so it was
root-caused with the same rigor before assuming anything: comparing file mtimes showed
`sogen_d3d9_umd.cpp` (source) was last modified at `00:43` (Task 1's DDI-handler addition), the x64 UMD
DLL (`sogen_d3d9um-x64.dll`) was rebuilt at `00:44` — one minute later, picking up the change — but the
staged x86 UMD DLL (`sogen_d3d9um-x86.dll`) was still dated `19:29` the *previous* day, predating Task 1
entirely. **This was not a new x86 architecture bug** — the x86 UMD binary simply hadn't been rebuilt
since the int/bool DDI handlers were added to the shared source file (both x64 and x86 UMDs are built
from the exact same `sogen_d3d9_umd.cpp`; only the x64 copy had been refreshed). Rebuilding
`sogen_d3d9um-x86.dll` from current source (`i686-w64-mingw32-g++`, no code change whatsoever) and
re-staging it fixed the mismatch completely:

```
pixel(320,240)=B=26 G=FF R=00 A=FF   (x86, after rebuild — identical to x64)
PASS: pixel R/G matches the alt-branch color
PASS: pixel B=26 matches expected 26
ALL CHECKS PASSED
```

Byte-for-byte identical to the x64 result. Unlike the const-test-x86 and texture-test-x86 ports, this
port needed zero source or host changes — the underlying DDI wiring, descriptor binding, and shader
translation were already architecture-agnostic; the only gap was a stale local build artifact.

### 22.6 Full regression sweep (2026-07-05)

x64: `shader`/`const`/`texture`/`texcoord`/`partial-lock`/`int-bool-const` all `ALL CHECKS PASSED`;
`managed-texture` fails exactly as documented (§17-19, confirmed permanent, not a regression). x86:
`shader`/`const`/`texture`/`texcoord`/`int-bool-const` all `ALL CHECKS PASSED`. Smoke test: 26/26
`Success`. See `docs/d3d9-roadmap.md`'s "Constant registers" entry and
`src/samples/sogen-d3d9-umd/README.md` for the consolidated write-ups.

## 23. Scissor rect, MRT, and multi-stream vertex sources — three M3 DDI-coverage items designed, wired, proven pixel-exact, and ported to x86 (2026-07-05)

A 10-task, session-local plan (not checked into this repo) adding three independent M3 items from
`docs/d3d9-roadmap.md`'s "M3 coverage items" checklist: scissor rects (Tasks 1-2), multiple render
targets/MRT (Tasks 3-5), and multi-stream vertex sources (Tasks 6-9). Task 10 (this section) ports all
three guest tests to i686/WoW64 and runs the full regression sweep. Commits: `c517c685`, `a60f26ec`,
`63ab0030` (scissor); `527d5775`, `d82de78f`, `87548935`, `769b329c`, `274075f8` (MRT); `37685830`,
`76a0913b`, `f32d0e12`, `1a6461ff`, `797caf7d`, `f3652a42`, `93d45040`, `6cd12c79` (multi-stream).

### 23.1 Scissor rect (Tasks 1-2) — the smallest of the three, straightforward

Before this work, `execute_draw` unconditionally forced a Vulkan scissor covering the whole render
target extent, regardless of what the app had set — `SetScissorRect` and `D3DRS_SCISSORTESTENABLE` were
tracked in `device_state` but never consulted. Fixed by gating the draw-time scissor rect: when
`D3DRS_SCISSORTESTENABLE` is on, the app's real `RECT` (`{left, top, right, bottom}`) is converted to a
Vulkan `VkRect2D` (`offset = {left, top}`, `extent = {right-left, bottom-top}`); when it's off, the
full-RT-extent fallback stays exactly as before. `d3d9_scissor_test.cpp` proves both halves in one run:
a center-third scissor rect (`{213,160,427,320}`) drawn with a full-screen quad reads RED at the
center and BLUE (background) at both far corners with the test enabled, then the same draw with
`D3DRS_SCISSORTESTENABLE` set back to FALSE reads RED everywhere (regression safety for the common
no-scissor case).

### 23.2 Multiple render targets / MRT (Tasks 3-5) — a real slot-compaction bug found and fixed mid-implementation

M2's pipeline builders and `execute_draw` only ever built for and wrote to render-target slot 0. Task 3
fanned both out across every bound RT, gated by `D3DCAPS9::NumSimultaneousRTs` (not shader model — D3D9's
`ps_2_0` ISA already defines `oC0`-`oC3` explicitly). **The real bug, caught during implementation, not
by the test**: the first draft stored bound RTs in a compacted, append-only list (RT0 bound → index 0,
RT1 bound → index 1, and so on by binding order). This breaks the moment a guest binds RTs
non-contiguously — e.g. RT0 left unbound while RT1 is bound — because a pixel shader's `oC1` write is
defined by D3D9 semantics to target render-target **slot 1**, not "the second RT the app happened to
bind." A compacted list would have silently routed that `oC1` write to whatever physical attachment
ended up at list index 0, misdrawing into the wrong render target with no error. Fixed (`d82de78f`)
before this ever shipped: bound RTs are now stored in a fixed-size array indexed directly by D3D9 slot
number, preserving gaps — slot 1 bound alone stays at array index 1, array index 0 stays empty. Task 4
(`87548935`) fixed the matching `Clear(D3DCLEAR_TARGET, ...)` gap: it also only touched slot 0
previously, leaving other bound RTs stale after a clear. `d3d9_mrt_test.cpp` (Task 5) proves both fixes
together: a PS returning distinct `oC0`/`oC1` colors with two RTs bound once at startup (never rebound)
confirms both receive their own color (not just RT0 getting drawn into), then `Clear(yellow)` with both
still bound confirms both go yellow (not just RT0 clearing).

### 23.3 Multi-stream vertex sources (Tasks 6-9) — a new declaration parser, a significant vkd3d-shader RE finding, and three real UMD bugs

This was the largest and most consequential of the three. M2 had no real vertex-declaration support at
all — `ensure_programmable_pipeline`'s vertex layout was hardcoded, distinguishing the one or two shapes
existing tests needed purely by `SetStreamSource`'s `Stride` value. Real multi-stream support needed a
genuine `D3DVERTEXELEMENT9` array parser, since a `CreateVertexDeclaration` call is the only place a
guest actually states which stream each vertex attribute comes from.

**Task 6** added `stream_offsets` state storage (per-stream `SetStreamSource` byte offset, previously
discarded). **Task 7** (`76a0913b`, with a guest-controlled-shift fix in `f32d0e12`) wrote
`parse_vertex_decl`: walks a real `D3DVERTEXELEMENT9[]` terminated by `D3DDECL_END()`, extracting
per-element `{Stream, Offset, Type, Usage, UsageIndex}` into Vulkan vertex-input-attribute data.

**The significant RE finding, made empirically while building Task 7/8** (`d3d9_host.cpp`, the comment
immediately above `parse_vertex_decl`): vkd3d-shader assigns a compiled vertex shader's SPIR-V input
`Location` decorations by **declaration order** — each input's `v#` register index, itself decided by
where its HLSL input-struct member (or D3DBC `dcl` instruction) appears — NOT by D3D9 usage semantics
(`D3DDECLUSAGE`/`UsageIndex`). Confirmed with three hand-written HLSL structs reordering the same three
semantics (`POSITION`/`TEXCOORD0`/`COLOR0`), compiled via this repo's own `deps/vkd3d/programs/
vkd3d-compiler` and inspected with `spirv-dis`: all three orderings produced `Location 0/1/2` following
struct order, with `POSITION` getting no special-casing (landing at `Location 1` in one ordering, not
always `Location 0`).

This directly constrains `parse_vertex_decl`, which has no visibility into its paired vertex shader (that
pairing is a draw-time concern, not this standalone parser's): it can only assign each element's
`Location` as its own ordinal position within the `D3DVERTEXELEMENT9` array, under the assumption that a
vertex declaration's element order matches its paired shader's input-struct order. **This is a
documented, currently-true-for-every-shader-in-this-repo assumption, not a fully general fix** — every
existing shader (`d3d9_const_test.cpp`, `d3d9_shader_test.cpp`, `d3d9_texcoord_test.cpp`, etc.) declares
`POSITION` first, matching it, but a future shader/declaration pair that violates it would silently
swap which buffer feeds which shader input with no error — exactly the kind of bug an HRESULT-only test
would miss. The fully general fix (cross-referencing the bound VS's own scanned input signature instead
of assuming declaration order) is flagged as future work in `d3d9_host.cpp`'s own comment, not implemented
here. This is now documented alongside vkd3d-shader's other RE findings in this project (the
CBV/register-index binding scheme in §22.1, the sampler-state DDI demultiplexing in the roadmap's M2
section).

**Task 8** (`1a6461ff`, refined in `797caf7d`) wired the parsed declaration into `execute_draw`'s
multi-stream vertex-buffer binding, binding each stream's buffer at its own bound offset rather than
assuming everything comes from stream 0 at offset 0.

**Task 9** (`f3652a42`/`93d45040`/`6cd12c79`) wrote `d3d9_multistream_test.cpp` — and building it found
**three real, previously-unknown bugs in the guest UMD** (`sogen_d3d9_umd.cpp`), not the host. Tasks
6-8 only ever touched `d3d9_host.cpp`/`.hpp`; nothing before this test had ever called
`CreateVertexDeclaration`/`SetVertexDeclaration` from a guest, so none of the three had ever been
reachable or visible:
1. `pfnCreateVertexShaderDecl` (`D3DDDI_DEVICEFUNCS` slot 45) was still an unwired `device_stub` —
   `CreateVertexDeclaration()` never reached the host at all. Fixed by adding
   `umd_CreateVertexShaderDecl` (mirroring `umd_CreateVertexShaderFunc`/`create_shader_common`'s
   already-proven struct-pointer-plus-trailing-array convention) and wiring slot 45 to it.
2. `D3DDDIARG_CREATEVERTEXSHADERDECL`'s field order was guessed backwards (`ShaderHandle` first) — a
   live byte-dump of the real `pArgs` (once bug 1 was fixed enough to reach it) showed
   `NumVertexElements` actually comes first (offset 0), with the 8-byte `ShaderHandle` at offset 8 (4
   bytes of ordinary x64 alignment padding in between, previously misread as part of `ShaderHandle`).
   Fixed by swapping the field order and pinning it with a `static_assert` (`93d45040`).
3. `pfnSetVertexShaderDecl` (slot 47) was already wired, but as a struct-pointer call — a live dump
   showed the "pArgs" parameter itself receiving the raw, small decl-id value directly (not a real
   pointer), meaning it is actually a DIRECT-VALUE `HANDLE` call, the same convention as
   `umd_SetVertexShaderFunc`/`umd_SetPixelShader`. Every real `SetVertexDeclaration()` call was silently
   forwarding `decl=0` to the host until this was fixed.

All three had to be fixed together before this test produced anything but an unrendered (black) result;
Tasks 6-8's host-side dispatch and wire-protocol structs needed no changes at all. The finished test:
POSITION on stream 0 (12 FLOAT3 positions, two flat-shaded triangles), COLOR on stream 1 bound at a
deliberately NONZERO `SetStreamSource` byte offset (the buffer starts with 20 bytes of a wrong pad
color before the real per-vertex data begins) — the left half of the viewport reads RED, the right half
GREEN, neither reachable unless stream 1 is genuinely bound (not silently collapsed onto stream 0) AND
its nonzero offset is honored (not treated as 0, which would read the pad color instead).

**Explicitly out of scope for this work**: `stream_frequencies`/`SetStreamSourceFreq` (instancing) and
`DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` (user-pointer draws) — neither was touched; both remain open
M3 items.

**A pipeline-cache gap flagged, not fixed, during this work**: `ensure_programmable_pipeline`'s cache key
is `(vertex_shader_id << 32 | pixel_shader_id)` only — it does not include the RT color-format list/
count (also an argument to the same function, needed for the MRT work above) or the vertex declaration
shape. A guest that reused one VS/PS pair across draws with a different RT count or a different vertex
declaration would silently get back a stale cached `VkPipeline`. Neither `d3d9_mrt_test.cpp` nor
`d3d9_multistream_test.cpp` exercises this (each uses one shape throughout), so it's flagged as a still-open,
not-yet-exercised risk in `docs/d3d9-roadmap.md`, not fixed here.

### 23.4 Task 10 — x86/WoW64 ports: all three pixel-exact, zero new architecture bugs

All three tests (`d3d9_scissor_test.cpp`, `d3d9_mrt_test.cpp`, `d3d9_multistream_test.cpp`) were
cross-compiled unchanged to i686 (`i686-w64-mingw32-g++`, identical flags to every other x86-ported
test in this project) and staged against the already-present genuine 32-bit `d3d9.dll`/
`d3dcompiler_43.dll`. All three passed on the very first run, every analytic pixel check matching the
x64 results exactly:

```
d3d9-scissor-test-x86:      6/6 PASS lines, ALL CHECKS PASSED (identical to x64)
d3d9-mrt-test-x86:          12/12 PASS lines, ALL CHECKS PASSED (identical to x64)
d3d9-multistream-test-x86:  2/2 PASS lines, ALL CHECKS PASSED (identical to x64)
```

This is a notable contrast with several earlier ports in this project (`d3d9-const-test-x86` found the
`allocate_id()` 32-bit `HANDLE`-truncation bug, §16; `d3d9-texture-test-x86` found the
`D3DDDIARG_CREATERESOURCE` x86 output-handle-offset bug, §16.3) — it confirms none of this plan's three
features touch an x86/x64-divergent struct field or handle-width-sensitive code path. The three real
bugs Task 9 found (§23.3) live in `sogen_d3d9_umd.cpp`'s DDI slot wiring/struct layout/calling
convention, which is shared, architecture-independent source — already exercised and fixed via the x64
test run before this port, so the x86 build simply inherited the fix with no separate work needed.

### 23.5 Full regression sweep (2026-07-05)

**x64** (`./analyzer -e root -c c:/<test>.exe`): `spike`, `shader`, `const`, `texture`, `texcoord`,
`partial-lock`, `int-bool-const`, `scissor`, `mrt`, `multistream` all `ALL CHECKS PASSED` (or, for
`spike`, `SUCCESS: IDirect3DDevice9 created`); `managed-texture` fails exactly as documented (§17-19,
confirmed permanent, not a regression).

**x86**: `shader`, `const`, `texture`, `texcoord`, `int-bool-const`, `scissor`, `mrt`, `multistream` all
`ALL CHECKS PASSED`. (`partial-lock` and `managed-texture` remain x64-only by design, matching
`src/samples/sogen-d3d9-umd/README.md`'s documented scope.)

**Smoke test**: `./analyzer -e root -s c:/test-sample.exe` — 26/26 `Success`, unchanged.

See `docs/d3d9-roadmap.md`'s "M3 coverage items" checklist and
`src/samples/sogen-d3d9-umd/README.md` for the consolidated write-ups.

## 24. Per-draw/per-clear GPU->CPU readback stall — audited, root-caused, and fixed with a
dirty-flag/deferred-readback model (2026-07-05)

A dedicated performance audit of the D3D9-over-Vulkan translation layer (`d3d9_host.cpp`/`.hpp`,
`vulkan_host.cpp`) — separate from the DDI-coverage work in §23 — found a severe, confirmed
architectural bug: every single `execute_draw` and `d3d9_clear` call performed a mandatory,
unconditional, **synchronous, blocking** GPU->CPU readback of the render target it touched, regardless
of whether the guest app ever actually needed those pixels on the CPU. Concretely, each readback did a
full second command-buffer submit, `vkWaitForFences(UINT64_MAX)`, a full-image
`vkCmdCopyImageToBuffer`, and a CPU `memcpy` — a genuine GPU round trip, not a cheap check. At realistic
game draw counts (500-1000+ draws/frame), that's 1000-2000+ blocking round trips per frame:
single-digit-FPS territory, dominated entirely by CPU-GPU sync stalls rather than actual rendering
work. Nothing else about the pipeline's rendering correctness was in question — this was purely a
"the host does far more synchronization than the guest ever asked for" bug.

### 24.1 Design: dirty-flag / deferred readback

The fix doesn't change *what* gets read back, only *when*: a resource's GPU-rendered pixels should only
ever be copied to its CPU-side backing store lazily, the moment something actually needs them
(`Lock()` or a Present-path snapshot), and only if the GPU side has actually changed since the last
sync. That's a classic dirty-flag: add a `backing_dirty` bit to `resource_entry`, set by every render
that writes to a color render target, and add one single function,
`d3d9_host::sync_backing_from_gpu(resource_entry& rt)`, that does the real GPU->CPU copy if and only if
`backing_dirty` is set, clearing it afterward. Every current or future reader of a resource's CPU
backing calls this one function first; the actual `vkCmdCopyImageToBuffer`/fence-wait/memcpy machinery
that already existed (previously invoked eagerly and unconditionally) is reused unchanged — only the
*call site* and *condition* changed.

### 24.2 Five-task implementation sequence

1. **`596b0b31`** — add the `backing_dirty` flag to `resource_entry` (`d3d9_host.hpp`) and set it at
   `execute_draw`'s and `d3d9_clear`'s existing readback sites, *alongside* the still-unconditional
   eager readback (no behavior change yet — pure groundwork, so the flag's correctness could be
   reviewed independently of removing the old path).
2. **`ecec18fb`** — add `sync_backing_from_gpu` itself and wire it into `lock()`'s wire-command handler,
   right after the resource lookup. Still additive: the eager readback stays in place, so at this point
   the GPU->CPU copy simply runs twice (once eagerly, once conditionally) — correctness-neutral, sets up
   the reader side before the eager path is removed.
3. **`ab8f2f87`** — code-quality pass on `sync_backing_from_gpu`: renamed its `resource_entry&`
   parameter from `e` to `rt` to match this file's naming convention, and corrected a doc comment that
   overstated a construction-time guarantee `readback_render_target`'s own runtime layout check
   actually provides (see 24.3 below for why this mattered).
4. **`0d6282ad`** — wire `sync_backing_from_gpu` into `snapshot_resource`, the Present-path pixel copy.
   Present reads a resource's `.backing` directly, exactly like `lock()` does, so it needed the same
   sync-before-copy — otherwise, once the eager path was removed, a guest that renders then Presents
   without ever calling `Lock()` on the backbuffer would show stale (pre-render) pixels.
5. **`2f16eaf3`** — the actual perf fix: remove the eager, unconditional readback entirely from
   `execute_draw` and `d3d9_clear`. After this commit, `sync_backing_from_gpu` is the *only* place a
   GPU->CPU readback ever happens, gated on `backing_dirty`, called only from `lock()` and
   `snapshot_resource`. `93c42040` followed up to fix three doc comments (the class-level comment, the
   Part-3 draw-path comment, and `sync_backing_from_gpu`'s own) that still described the old
   always-readback model after the code no longer matched it.

### 24.3 A real fragility found during review: the layout-safety check was coincidentally correct, not genuinely verified (`dad9f5f8`)

Removing the eager readback means `sync_backing_from_gpu` is now trusted as the sole gate on when a
readback happens — which makes it worth asking whether the readback itself, `readback_render_target`
(`vulkan_host.cpp`), was ever actually safe to call at arbitrary points, or had just never been
exercised outside the narrow pattern the eager path always used. `readback_render_target` guards its
`vkCmdCopyImageToBuffer` with a check that the source image is currently in
`VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` — but that check reads a CPU-side mirror field,
`render_target_data::current_layout`, not the image's real Vulkan layout (Vulkan has no query for an
image's current layout; host-side tracking is the only option). The review found that
`current_layout` was written in exactly one place: `submit_clear`. Every other layout transition —
critically, `execute_draw`'s own leading/trailing barriers, which are what actually put a render target
into `TRANSFER_SRC_OPTIMAL` before a draw-triggered readback — went through `cmd_pipeline_barrier`, the
single shared choke point every barrier in this codebase is issued through, and `cmd_pipeline_barrier`
never touched `current_layout` at all. In other words: the safety check had been passing, but only
because every draw's barriers happened to be symmetric around the same layout by coincidence, not
because the check was verifying the image's real state. A guest sequencing draws/barriers in a way that
broke that coincidence would have hit a stale-layout readback with no error — silently wrong pixels, not
a crash, the worst kind of latent bug to leave in place right as this fix was making
`sync_backing_from_gpu` the sole readback path.

Fixed (`dad9f5f8`) by having `cmd_pipeline_barrier` itself update `current_layout` whenever the image it
just transitioned is a tracked render target (`vulkan_host.cpp`'s `render_targets` map, shared with
depth-stencils), immediately after issuing the real `vkCmdPipelineBarrier` call:

```cpp
const auto rt = this->impl_->render_targets.find(image);
if (rt != this->impl_->render_targets.end())
{
    rt->second.current_layout = barrier.newLayout;
}
```

This makes the mirror accurate for every barrier a render target goes through — `submit_clear`'s and
`execute_draw`'s alike — rather than only the one call site anyone had originally remembered to update.

### 24.4 Verification and scope

Full regression sweep, independently repeated multiple times: every existing D3D9 guest test passes on
both x64 and x86, pixel values byte-identical to their previously-documented values (`shader`, `const`,
`texture`, `texcoord`, `partial-lock`, `int-bool-const`, `scissor`, `mrt`, `multistream` all pass;
`managed-texture` still fails the same documented, permanent, unrelated limitation from §17-19). Smoke
test: 26/26.

**Explicitly not addressed by this fix** (real, separate, larger remaining work, see
`docs/d3d9-roadmap.md`'s "Performance — D3D9 native path" section): the present/submit path's
busy-spin fence-wait, per-draw buffer/UBO/descriptor-set allocation churn, and (at the time this fix
landed) the total lack of wire-protocol batching for D3D9 DDI calls. This fix eliminates the confirmed
per-draw/per-clear blocking-readback catastrophe; it does not touch either of the first two. The third
item — DDI-call wire batching — was subsequently designed, implemented, and verified; see §25 below.

## 25. D3D9 DDI-call wire batching — designed, implemented in three steps, and verified with a live
1256-byte/15-call batch (2026-07-05)

§24 fixed the per-draw/per-clear GPU->CPU readback stall but explicitly left "no wire-protocol batching
for D3D9 DDI calls" on the table as separate, larger remaining work. This slice closes that item: every
streamed D3D9 opcode (`SetRenderState`, `SetTexture`, `DrawPrimitive`, `Clear`, ~22 call sites in
`sogen_d3d9_umd.cpp`) now batches guest-side instead of crossing the guest/host wire as its own
individual sync Escape call.

### 25.1 Design: Group-A batchable vs. Group-B must-flush

DDI calls split into two groups:
- **Group A (batchable)** — state-setting, draw, and clear calls whose effects the host only needs to
  have observed *before the next thing that actually reads results back*: `SetRenderState`,
  `SetTextureStageState`, `SetSamplerState`, `SetTexture`, `SetStreamSource(Freq)`, `SetIndices`,
  `SetVertexDecl`, `SetVertexShader`/`SetPixelShader`, the `Set{VS,PS}Const{F,I,B}` families,
  `SetRenderTarget`/`SetDepthStencil`, `SetViewport`, `SetScissorRect`, `Clear`,
  `DrawPrimitive`/`DrawIndexedPrimitive`. These append to a single guest-side buffer,
  `g_d3d9_command_batch`, via `record_d3d9()`.
- **Group B (must-flush)** — anything that needs to observe host-side state synchronously before it can
  do its own job correctly: `Lock`/`Unlock`, `Present`, `CreateResource`, `TexBlt`, and
  shader/vertex-declaration creation. These already go through `bridge_call` for their own Escape; no
  Group-B call site needed editing.

The mechanism reused is the **already-existing** `record_commands`/`ioctl_record_commands` wire
protocol — previously only exercised by the generic Vulkan-ICD bridge (`vulkan_shim.cpp`) for batching
command-buffer contents. `record_d3d9` writes a `command_record_header` + payload per call into
`g_d3d9_command_batch`, exactly the record format `d3d9_host`'s `execute_recorded` already knows how to
replay — zero host-side or wire-format changes were needed.

The actual flush point is a single guard added to `bridge_call` itself: every call whose opcode isn't
`ioctl_record_commands` drains any pending batch first (via `flush_d3d9_batch()`), so every Group-B call
site gets the "flush before you run" behavior for free, and `flush_d3d9_batch`'s own recursive call into
`bridge_call` (to send the `ioctl_record_commands` Escape) can't re-trigger itself. A 64 KiB size cap in
`record_d3d9` is a pure backstop in case an unusually long run of Group-A calls happens with no Group-B
call in between (in practice every frame ends in `Present`, so this should never trigger).

### 25.2 Four-task implementation sequence

1. **`ecda4363`** — add the batching infrastructure (`g_d3d9_command_batch`, `record_d3d9`,
   `flush_d3d9_batch`, the `bridge_call` guard) but have `record_d3d9` flush after every single append —
   batch depth of 1, wire-identical to the old per-call path. This proved the wire format carries D3D9
   opcodes correctly through `ioctl_record_commands` with zero behavior change, before touching the part
   that actually changes behavior.
2. **`e1ec179a`** — code-quality pass from review: documented why the `bridge_call` guard exists and why
   its `!=` check prevents `flush_d3d9_batch`'s own recursive Escape from re-entering itself, and
   switched the drain-and-reset in `flush_d3d9_batch` from move+clear to `swap()`, matching
   `vulkan_shim.cpp`'s established idiom for the same operation.
3. **`87863527`** — the real perf change: `record_d3d9` no longer flushes after every append. Group-A
   calls now genuinely accumulate until a Group-B call (or an internal lazy-bind resolver) needs to
   observe them via the `bridge_call` guard. The 64 KiB cap was added here as the backstop described
   above. Full guest test suite (shader/const/texture/texcoord/partial-lock/int-bool-const/scissor/mrt/
   multistream, x64+x86 where applicable) and the 26-subtest smoke test all passed with unchanged pixel
   values.
4. **`5bac1070`** — documented, per review feedback, that `umd_Flush` (`pfnFlush`) deliberately does
   *not* drain `g_d3d9_command_batch`: no query/fence DDI is wired yet that would need the pending batch
   visible, but a future reader wiring one could reasonably assume `Flush()` already interacts with
   batching, so the non-interaction is now explicit rather than silent.

### 25.3 Live-instrumentation verification: a real 15-call, 1256-byte batch

Beyond the regression suite passing pixel-identical, batching was confirmed as *actually happening* (not
just plumbed through a new mechanism that still flushes every call) by adding live instrumentation and
running the `texture` guest test (`d3d9_texture_test.cpp`). The trace showed a single flush of **1256
bytes** covering **15+ accumulated Group-A calls** — `SetRenderTarget`, `SetDepthStencil`,
`SetStreamSource`, `SetIndices`, `SetTexture`, vertex/pixel shader binds, `SetViewport`, `Clear`, and
four `DrawIndexedPrimitive` calls — all collapsing into one `ioctl_record_commands` Escape immediately
before the readback `Lock()` that needed to observe their effects. This is the concrete evidence that the
`bridge_call` guard is doing real batching across a representative real-world call sequence, not merely
routing individual calls through `ioctl_record_commands` one at a time.

### 25.4 Scope

This closes the third and last item §24 left on the table. The other two — the present/submit path's
busy-spin fence-wait and per-draw buffer/UBO/descriptor-set allocation churn — remain real, separate,
unaddressed work; see `docs/d3d9-roadmap.md`'s "Performance — D3D9 native path" section.

## 26. Pipeline-cache-key correctness fix — closed out, proven with a discriminator test, ported to x86
(2026-07-05)

The "Pipeline-key system beyond one shader pair at a time" gap flagged during the MRT/multi-stream work
(§23, `docs/d3d9-roadmap.md`) is now fixed. `ensure_programmable_pipeline`'s `VkPipeline` cache
(`programmable_pipelines_`) was keyed ONLY by `(vertex_shader_id << 32 | pixel_shader_id)`, and
`ensure_pipeline` (the fixed-function sibling) had no per-shape cache at all — a one-shot
`pipeline_ready_` bool reused for the device's whole lifetime. Neither key covered the bound RT
color-format list/count or depth format (baked into `VkPipelineRenderingCreateInfo`/`build_depth_state`)
or the vertex-input shape, so a guest reusing the same VS/PS pair (or, for FF, any draw at all) across a
different bound-RT shape, depth format, or vertex layout would silently get back a stale pipeline built
for an earlier draw's shape.

**Fix (`3809d1c8`):** a `pipeline_cache_key` struct (`vertex_shader`, `pixel_shader`, `color_formats[4]`,
`depth_format`, `vertex_shape`) is now the key for both `programmable_pipelines_` and a new
`ff_pipelines_` map. `vertex_shape_key()` mirrors the exact real-decl-vs-fallback-stride branch the
pipeline builder itself uses, so the computed key can never disagree with what actually gets built on a
cache miss. `5dd05caa` is a follow-up polish pass (dropped a redundant `operator==` now that `<=>` is
defaulted, documented the FF pipeline's create-once fields).

**Discriminator test (`deebf036`/`e0205851`):** new `d3d9_pipeline_cache_test.cpp` compiles one
`vs_2_0`/`ps_2_0` pair (PS writes solid RED to `COLOR0` only). Sub-pass 1 binds RT0 alone, clears it
BLUE, draws — RT0 reads back RED, caching a 1-attachment pipeline for this VS/PS pair. Sub-pass 2 rebinds
to RT0 (slot 0) + a new RT1 (slot 1) — same VS/PS, never recreated, only the RT shape changes — clears
both BLUE, draws again. Before the fix this reused the stale 1-attachment pipeline against a
2-attachment rendering scope; after the fix RT0 stays RED (the primary discriminator — the old bug could
corrupt attachment 0's own output, not just leave attachment 1 wrong) and RT1 correctly stays BLUE,
untouched by a PS that never writes `oC1`. Run against the pre-`3809d1c8` host (`3809d1c8~1`) as a
before/after check: sub-pass 2's RT1 came back black instead of BLUE, a real, observed discrimination of
the bug, not a hypothetical one.

**x86/WoW64 port:** cross-compiled `d3d9_pipeline_cache_test.cpp` unchanged with `i686-w64-mingw32-g++`
(no source edits — this fix is entirely host-side C++, no guest UMD/DDI wire-format change) and staged
it as `d3d9-pipeline-cache-test-x86.exe`. Ran clean on the first try against the real 32-bit `d3d9.dll`:
all nine `PASS:` lines, `[d3d9-pipeline-cache-test] ALL CHECKS PASSED`, exit 0 — pixel-exact parity with
the x64 run (RT0 `B=00 G=00 R=FF` at all three checkpoints in both sub-passes; RT1 `B=FF G=00 R=00` in
sub-pass 2). No new x86-only bug found, consistent with this being a host-only fix.

**Two narrower gaps found while making this fix, deliberately deferred, not fixed here:** (1) D3D9
render-state that's also baked as static pipeline state — `D3DRS_ZENABLE`/`ZWRITEENABLE`/depth-compare
and `D3DRS_ALPHABLENDENABLE`/blend-factor — isn't part of `pipeline_cache_key`, so toggling these with
the same shaders/RT-shape/vertex-shape between draws would still hit a stale cache entry. (2)
`vertex_shape_key()`'s real-vertex-declaration branch fingerprints only the immutable declaration handle,
not the mutable per-stream strides that also feed the pipeline's vertex-binding descriptions — rebinding
a declaration to a differently-strided stream buffer would still hit a stale entry. Neither is exercised
by any current guest test; both are now tracked in `docs/d3d9-roadmap.md`'s "Pipeline-key system"
bullets as explicit, known, not-yet-fixed follow-ups.

Full regression (all host gtests, all 19 guest `d3d9-*-test.exe` on x64 and x86, 26/26 smoke test) passes
identically to before this change.

## 27. D3DPOOL_MANAGED Option-A spike — gate forced open at runtime, but reveals a second, deeper bug
instead of a fix (2026-07-05)

§16.3/§17-19 (referenced from `docs/d3d9-roadmap.md`) concluded the `D3DCAPS2_CANMANAGERESOURCE` gate was
"structurally uncontrollable" — no `D3DCAPS9` value any driver reports survives `d3d9.dll`'s own
`QueryLHDDICaps`, which unconditionally strips bit 28 after querying the driver. That conclusion is about
the *reported-caps* surface specifically and still holds. This spike tested a different mechanism: a live
runtime memory patch, bypassing reported caps entirely.

**Mechanism:** a scratch Python harness (`build/release-py`, `import sogen`) using
`emu.callbacks.on_module_load` to catch `d3d9.dll`'s live image base, then
`emu.hooks.memory_execution_at(base + 0x158b6, cb)` — the instruction right after the caps-strip store —
reading `RSI` (the struct pointer) in the callback and re-OR'ing bit 28 back in via `emu.write_memory`
before `d3d9.dll`'s own code continues. Disassembly at `image_base+0x158af..0x158b3` confirmed the exact
instructions: `btr eax, 0x1c` (clear bit 28) then `mov [rsi+0xc], eax` (the store) — matching the
previously-decompiled `*(a3+12) = v27 & 0xEFFFFFFF` exactly, just compiled as a bit-test-reset rather than
a literal AND. A second write site at `+0x15a1f` only *reads* the field and toggles a different bit, so
one intervention point is sufficient — confirmed structurally, not just empirically.

**The patch works mechanically:** the watched field (`CBaseDevice+444`) reads `0xe4628800` (bit 28 clear)
unpatched, `0xf4628800` (bit 28 set) patched, live, every run, 6 hits/run. `d3d9.dll` genuinely takes a
different internal code path afterward — proven by the *different* failure mode below, not merely by
reading the bit back.

**But the unmodified `d3d9_managed_texture_test.cpp` still does not pass.** Unpatched, it fails the way
§17-19 already documented (app's `LockRect()` pointer diverges from the driver's own pixel buffer — wrong
pixel, not a crash). Patched, it fails *earlier and differently*: `CreateTexture(D3DPOOL_MANAGED)` still
returns `hr=0`, but `Texture->LockRect()` now returns `hr=0x00000000` with `pBits=nullptr`. Forcing the
gate makes `d3d9.dll` hand the lock off to the driver-managed path — and this driver's `umd_Lock` has
never had to serve a driver-managed `D3DPOOL_MANAGED` resource before, so it has no real sysmem backing to
hand back for one. The test file's assertions were not touched; it still correctly reports `FAILED`,
unchanged from before this spike (the patch was never made permanent — it lives only in the scratch
Python harness).

**Net result — a corrected understanding, not a fix:** the gate is not immovable after all, but forcing it
trades one broken path for another. The genuinely new, concrete fact this spike bought: a real fix now has
an addressable target — give `umd_CreateResource`/`umd_Lock` a real sysmem allocation for driver-managed
MANAGED resources — where before, the gate itself was believed unforceable by any means and there was no
candidate mechanism at all. Turning this into a real fix would additionally require productionizing the
bit-forcing patch (a permanent emulator-side hook, not a scratch Python script) and a separate 32-bit RE
pass, since `+0x158b3` was verified only against the staged x64 `system32/d3d9.dll`
(sha256 `bb65372a53445b5607cbd705a29b4671ab1fb250bef32b3fd0377704088c366c`) — real MW2 is 32-bit. Neither
was attempted here; both are real, separately-scoped follow-up work if this path is pursued further.

Scratch harness (not committed, reusable): `managed_spike.py`, `disasm.py`, `d2.py` under this session's
scratchpad, plus a Python 3.14 venv with `capstone`/`pefile`.

## 28. D3DPOOL_MANAGED — the Option-A patch productionized into a permanent hook; the test now genuinely
passes on x64 (2026-07-05)

§27's spike proved the `D3DCAPS2_CANMANAGERESOURCE` gate could be forced open at runtime from outside the
reported-caps mechanism, but left two items as unstarted follow-up: productionizing the scratch Python
patch into a permanent emulator-side hook, and a 32-bit RE pass. The first has now been done (commits
`36e2a8bb`/`c42fabd4`); the second has not (see the x86/WoW64 scope note below).

**The hook:** `windows_emulator::install_d3d9_caps_patch_hook` (`windows_emulator.cpp`), called from
`setup_hooks`'s `on_module_load` callback whenever a module named `d3d9.dll` loads. It gates on
`mod.machine == 0x8664` (AMD64) and re-verifies the exact 7-byte pattern the spike found
(`0F BA F0 1C 89 46 0C` — `btr eax,0x1c` / `mov [rsi+0xc],eax`) at `image_base+0x158af` before installing
anything, logging a warning and bailing out (not forcing anything blind) if the bytes don't match — this
guards against a different `d3d9.dll` build silently getting the wrong RVA patched. On a match, it
installs an `emu().hook_memory_execution` callback at `image_base+0x158b6` (the instruction right after
the strip's store) that reads `RSI` (the struct pointer, per the decompiled calling convention), re-ORs
bit 28 back into `[RSI+0xc]` if it's clear, and writes it back. The hook handle is tracked in a new
`d3d9_caps_hooks_` map keyed by image base and torn down on module unload, matching the existing
`section_first_execution_hooks_` pattern already used elsewhere in this file.

**Independent verification, not just a commit-message claim.** This session ran its own A/B toggle
(temporarily forcing the hook to bail out early right after the machine-type check, rebuilding the release
preset, re-running `d3d9_managed_texture_test.cpp`, then reverting the change and rebuilding again to
confirm the diff was clean) before touching any documentation. Disabled, the test fails exactly as §27
documented (`Texture->LockRect()` pBits non-null but pointing at `d3d9.dll`'s own private shadow, sampled
pixel black, `[d3d9-managed-texture-test] FAILED`). Enabled, `Texture->LockRect()` returns a real, non-null
pointer this driver actually serves, and the final rendered pixel reads back exact solid magenta
(`B=FF G=00 R=FF A=FF`) — `[d3d9-managed-texture-test] ALL CHECKS PASSED`, unqualified. The full x64 and
x86 guest-test regression sweep (all `d3d9-*-test.exe` on both architectures) was also re-run clean with
the hook restored. Commit `c42fabd4`'s message additionally credits a code-quality review pass for the
cross-reference-to-roadmap polish.

**The corrected understanding.** §17-19's "structurally uncontrollable"/"confirmed unfixable" conclusion
was about one specific surface: the *reported* `D3DCAPS9` value, which `QueryLHDDICaps` strips
unconditionally regardless of what any driver reports through `GetCaps`. That conclusion is unchanged and
still correct — no `D3DCAPS9` field survives the strip. What's different here is that this hook is not a
DDI-surface or reported-caps change at all — it's a permanent patch to `d3d9.dll`'s own in-memory
*behavior*, installed and torn down by the emulator itself outside any driver-reported value. That
distinction is exactly what makes the old conclusion (about the reported-caps surface) and this fix (a
runtime behavior patch) both true at once, rather than contradictory.

The other surprise: no new UMD code was needed. §27's spike, run through a scratch Python harness with no
UMD-side changes, saw `Texture->LockRect()` return `pBits=nullptr` once the gate was forced open — the
natural reading at the time was that `umd_Lock`/`g_locked_buffers` would need new code to serve a
driver-managed MANAGED resource. Re-running the *same*, unmodified `umd_Lock`/`g_locked_buffers` machinery
against the *permanent* hook instead produced a real, working pixel backing — this existing machinery,
originally built only for ordinary (non-MANAGED) resources, was already sufficient once the gate stayed
open for the whole run. Nothing on the UMD side changed between the spike and this fix; the diff for
`36e2a8bb` touches only `windows_emulator.cpp`/`windows_emulator.hpp`.

**x86/WoW64 scope — not fixed there, don't read this as MW2-ready.** The pattern match and RVAs
(`+0x158af`/`+0x158b6`) are verified only against the staged 64-bit `system32/d3d9.dll`
(sha256 `bb65372a53445b5607cbd705a29b4671ab1fb250bef32b3fd0377704088c366c`). The 32-bit
`syswow64/d3d9.dll` real MW2 (a 32-bit game) would actually load has not had an equivalent RE pass — the
hook's `mod.machine != machine_amd64` check returns immediately for a 32-bit module, so no patch is even
attempted there yet. This is real, unstarted, separately-scoped follow-up work, not a detail to gloss
over: as things stand today, this fix does not help a real 32-bit game.

Updated to reflect this: `d3d9_managed_texture_test.cpp`'s header comment and its pixel-check branch (the
old "EXPECTED FAILURE" leniency removed — it's a normal strict pass/fail check now, like every other guest
test in this directory), `sogen_d3d9_umd.cpp`'s `umd_TexBlt` comment (conclusion corrected, backstory kept),
and `docs/d3d9-roadmap.md`'s `D3DPOOL_MANAGED` entries (the main WONTFIX bullet flipped to FIXED-on-x64,
the Option-A spike bullet's "not attempted" framing corrected now that it has been, and the M2/M3/M5
milestone-table rows that called this out as a standing MW2 risk all corrected to the x64-fixed/
x86-still-open state). Full regression: every x64 and x86 `d3d9-*-test.exe` green, including
`d3d9-managed-texture-test` now passing for real on x64.
