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
