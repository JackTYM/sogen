# D3D9-over-Vulkan roadmap

Single source of truth for what's done and what's left on the path to running a real D3D9 game
(target: MW2, 32-bit, SM3) through sogen's D3D9-over-Vulkan translation layer. Update this doc at
the end of every slice — don't let deferred items scatter across commit messages and handoff notes
again.

Architecture reference: `.claude/plans/scalable-giggling-fern.md` (original milestone design, M1-M5
below map to its `## Milestones & verification` table). The follow-on plan that closed out the WoW64
row's x86 test parity and the three M2-carried bugs below (session-local, not checked into this repo)
was `jazzy-giggling-cloud.md`. Session-by-session narrative/RE detail lives in `HANDOFF_MACBOOK.md` —
this doc is the rollup, that one is the diary.

## How the guest→host pipeline works today

Real Microsoft `d3d9.dll` (unmodified) loads sogen's own vendor driver (`sogen_d3d9um.dll`), which
marshals D3D9 DDI calls over a wire protocol to host-side `d3d9_host`/`d3d9_shader_translator`,
which do the real work via Vulkan (MoltenVK → Metal on this Mac). A separate, independent path
(`vulkan-shim`) lets a guest app that talks Vulkan directly reach the same host Vulkan backend via a
registered ICD — not part of this roadmap, already working, unrelated to D3D9.

## Status by milestone

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| M1 | Programmable SM2/3 triangle (VS+PS bytecode, DrawPrimitive, Present) | **Done** | Real `D3DCompile()` → `vkd3d-shader` → SPIR-V → Vulkan pipeline, pixel-verified. No DXVK oracle was built (dropped in favor of analytic pixel-readback checks, same rigor, no new dependency). |
| M1.5 | Float constant registers (`c#`) | **Done** | Not in the original plan as a separate milestone; pulled forward because a WVP matrix is needed for M2 anyway. UBO + descriptor-set binding contract now proven for one register set. |
| M2 | Textured + depth quad (`SetTexture`/sampler, indexed draw, depth, alpha blend) | **Done** | Real GPU-backed 2D textures (single mip, lazy staging upload), real `vulkan_host::create_sampler` + combined-image-sampler binding (RE'd sampler-state DDI encoding), indexed draws, real depth testing (D32_SFLOAT_S8_UINT in place of D24S8 for MoltenVK), real alpha blending, and the SPIR-V-side combined-sampler binding in the shader translator — all proven together by `d3d9_texture_test.cpp` (4/4 analytic pixel checks exact). D3DFORMAT↔VkFormat table covers exactly the locked MW2-first set (13 D3DFORMAT values, collapsing to 11 distinct VkFormat outputs since A8R8G8B8/X8R8G8B8 both map to B8G8R8A8_UNORM and L8/A8 both map to R8_UNORM — see below). Of the two bugs found building the terminal test: `D3DPOOL_MANAGED` got a real code fix for its double-resource-creation half (`pfnTexBlt` now syncs the sysmem/vidmem copies), but its deeper remaining symptom (managed textures sample black) is confirmed a permanent, unfixable-via-this-DDI limitation, not something more code will resolve; TEXCOORD0 interpolation turned out not to be a real bug at all — investigated and does not reproduce, no code change needed. Both are fully investigated, see "Known bugs & limitations carried out of M2" below for the three-way distinction (fixed / confirmed non-bug / confirmed permanent). The partial-buffer Lock limitation is now genuinely fixed on x64 (still whole-buffer-only on x86, a scoped-out gap) — also below. |
| WoW64 | WoW64/x86 D3D9 UMD port (32-bit `sogen_d3d9um`, SysWOW64) | **Done** | Not in the original plan's M1-M5 table; pulled forward per the user's own priority order since real games (MW2/BO2/GTA SA) are 32-bit and nothing else matters without a working x86 guest path. Proven: typed per-slot `__stdcall` thunk arities for all 143 `D3DDDI_DEVICEFUNCS` slots (28 real implementations unchanged, the other 115 now get a correctly-sized `stub_args_N` instead of x64's zero-arg `device_stub`, which would desync callee-cleanup x86's stack); x86-specific `D3DDDIARG_*`/`D3DDDI_*` struct layout for every real slot with a HANDLE/pointer field, pinned by `#ifndef _WIN64` `static_assert`s, including a live-RE'd fix for `D3DDDIARG_LOCK`/`D3DDDIARG_UNLOCK` (x86 is a genuinely different two-tier struct shape via `DdLockLH`, not just a pointer-shrunk copy of the x64 layout); `i686-w64-mingw32-g++` build tooling (parallel commands in `src/samples/sogen-d3d9-umd/README.md`) producing `sogen_d3d9um-x86.dll`. End-to-end proof: `d3d9-triangle-test-x86.exe`, cross-compiled to i686 and run through the real WoW64 path against the genuine 32-bit Microsoft `d3d9.dll` (not DXVK), reaches the x86 UMD and reads back the exact same pixel (`B=FF G=80 R=40 A=FF`) as the x64 `d3d9-triangle-test` — full pixel parity. **Now also covered:** `d3d9-shader-test`, `d3d9-const-test`, `d3d9-texture-test`, and `d3d9-texcoord-test` have all been cross-compiled to i686 and pass on x86 too (all analytic pixel checks exact, matching the x64 results) — M1.5/M2's shader, constant-register, texture, depth, blend, and real-`TEXCOORD0` features are now confirmed on the x86 path, not just the fixed-function triangle. Porting these caught two real, x86-only bugs along the way (both fixed): `d3d9_host::allocate_id()`'s shared id counter started at `1ULL << 32`, silently truncating through a 32-bit `HANDLE` on x86 guests (fixed, starts at `0x10000`; commit `c35871ca`); and `D3DDDIARG_CREATERESOURCE`'s output-handle offset is 44 on x86, not 48 as RE'd for x64 (a clean 4-byte shift, fixed; commit `c5dd3d27`) — full narrative for both in `src/samples/sogen-d3d9-umd/README.md`. Full regression sweep after all this session's fixes landed: every x64 and x86 guest test green, smoke test 26/26 (2026-07-04). Partial-buffer Lock support (see below) is x64-only — x86 keeps whole-buffer-lock semantics, a known, explicitly scoped-out gap (its driver-routed `OffsetToLock` offset isn't RE-verified). Five DDI slot arities flagged low-confidence during design (`pfnCheckCounter`, `pfnSetMarker`, `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified against the real `d3d9.dll`, since none of the current x86 tests call them — a future x86 test that hits one of these slots could still find a stack-corrupting arity mismatch. |
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats) | **Not started** | Int/bool constant registers, originally scoped as part of this milestone, are done (see "Constant registers" below) — pulled forward for the same reason float constants were (real games hit shader flow control early). The rest of M3 inherits the `D3DPOOL_MANAGED` permanent limitation and (on x86 only) the partial-Lock gap (see below) — none of this is M3-net-new work, it's M2 debt M3 must not silently re-break. |
| M4 | Fixed-function synthesis | **Deferred by design** | Original plan: "only if MW2 needs it." MW2 is SM3-heavy; likely skippable. Revisit if a real game draw path turns out to need FF after all. |
| M5 | MW2 integration (WoW64, SM3, 32-bit UMD) | **Not started, blocked on M3** | The WoW64/x86 UMD port itself is done (see the WoW64 row above), and its shader/texture/constant-register/texcoord features are now confirmed on the x86 path specifically (not just the fixed-function triangle) — the remaining blocker is real MW2 integration work: M3's DDI coverage and SM3 caps. The confirmed-permanent `D3DPOOL_MANAGED` limitation (see below) is likely to be hit by real MW2 asset loading and will need a fundamentally different mechanism, not just more DDI coverage. |

## Remaining work, consolidated (supersedes scattered "Deferred work" notes in HANDOFF_MACBOOK.md)

### Constant registers
- [x] Float (`c#`) — done, this session.
- [x] **Int (`i#`) / bool (`b#`)** — done (`jazzy-giggling-cloud.md`, Tasks 1-5). Wire protocol
  opcodes, `device_state` storage (`vs_const_i`/`ps_const_i` as `int32_t`, `vs_const_b`/`ps_const_b`
  as `uint32_t` expanded to a 16-byte std140 stride), UMD-side DDI handlers, and host-side UBO/
  descriptor binding are all wired end to end. The locked design is narrower than this doc originally
  speculated: **2 descriptor sets, not "up to 4"** — VS keeps set 0 and PS keeps set 1, and each set
  simply grew two more bindings (binding 2 = int CBV, binding 3 = bool CBV) alongside the pre-existing
  binding 0 (float CBV) and PS-only binding 1 (combined-image-sampler), since vkd3d-shader keys a CBV
  by `register_index` within whichever set the stage already owns, not by a whole extra set per
  register bank. Proven with a REAL runtime flow-control test
  (`src/samples/sogen-d3d9-umd/d3d9_int_bool_const_test.cpp`), not just raw byte delivery: a genuine
  bool branch (`if (useAltColor)`, driven by `SetVertexShaderConstantB(0,...)`) that compiles to a real
  D3DBC `IF` instruction reading the `b0` CONSTBOOL register, and a genuine int loop
  (`for(k<loopTripCount.x)`, driven by `SetVertexShaderConstantI(0,...)`) that compiles to a real D3DBC
  `REP` opcode — both confirmed by walking the raw D3DBC token stream, not by trusting HRESULTs alone.
  Four findings surfaced while building that test, all already documented in the test's own header
  comment and `HANDOFF_MACBOOK.md`, and meaningfully different in kind: **one genuine host bug** — a
  missing IOCTL-dispatch-routing case in `gpu_bridge.cpp` for the two new opcodes, fixed with real code
  — plus **three confirmed `d3dcompiler_43` compiler quirks, none of them sogen bugs** (all reproducible
  on real hardware): (1) `bool x : register(b0)` is rejected outright for a vs_2_0 target and must be
  left auto-allocated; (2) an `if`/`else` merging into one trailing write gets flattened into `SGE`/`MAD`
  arithmetic against an auto-allocated FLOAT register instead of ever reading the real `b0` CONSTBOOL
  bank (worked around with early-`return` branches in the test shader, without weakening what the test
  proves); (3) a narrower quirk where exact `0.0`/`1.0` literals inside a branch get pulled out via a
  separate, never-set shadow float register (worked around with `0.999`/`0.001`).
  Confirmed pixel-exact on **both x64 and x86/WoW64** through the real 32-bit Microsoft `d3d9.dll`
  (Task 5): `pixel(320,240)=B=26 G=FF R=00 A=FF` and both analytic checks pass identically on both
  architectures (a later follow-up test extension, proving the CBV stride math at a nonzero register
  in addition to register 0, swapped which branch's color the test now expects here). The one x86 wrinkle Task 5 hit was not a new architecture bug: the staged x86 UMD DLL
  (`sogen_d3d9um-x86.dll`) had simply not been rebuilt since Task 1 added the new DDI handlers to
  `sogen_d3d9_umd.cpp` — rebuilding it (no source change) fixed the mismatch. See
  `src/samples/sogen-d3d9-umd/README.md` and `HANDOFF_MACBOOK.md` §22 for the full narrative.
  `D3DDDIARG_SETVERTEXSHADERCONSTI`/`...CONSTB` structs in `d3d9_ddi.hpp`, previously flagged
  unverified, are now confirmed correct by this end-to-end test.

### M2 — delivered
- [x] Plain sampled `texture_2d` resources get real GPU backing (`create_image` + lazy staging upload,
  re-uploaded unconditionally whenever a draw binds them — no dirty tracking yet, see
  `d3d9_host.cpp`'s `ensure_texture_uploaded` comment).
- [x] Sampler state reaches the driver — **live RE finding**: there is no separate `pfnSetSamplerState`
  DDI slot at all; sampler state and texture-stage state share one interleaved
  `D3DDDITEXTURESTAGESTATETYPE` enum, told apart only by which `State` value arrives, not a numeric
  range split. `umd_SetTextureStageState` now demultiplexes via `sampler_state_for_ddi_tss_state()`
  and routes real sampler calls over the wire's `set_sampler_state` opcode.
- [x] `vulkan_host::create_sampler` + a combined-image-sampler descriptor (PS set 1, binding 1) wired
  into the programmable pipeline; `d3d9_shader_translator.cpp` pins the matching SPIR-V binding for any
  PS that actually reads sampler register s0.
- [x] Indexed draw execution (`cmd_bind_index_buffer`/`cmd_draw_indexed`).
- [x] Real depth testing (depth attachment + pipeline depth state). `D3DFMT_D24S8`/`D24X8` map to
  `VK_FORMAT_D32_SFLOAT_S8_UINT`/`D32_SFLOAT`, not their byte-exact Vulkan equivalents — MoltenVK/
  Apple Silicon doesn't support `D24_UNORM_S8_UINT` at all (confirmed via `vulkaninfo`) and has no
  native 24-bit depth format for the packed X8D24 variant either.
- [x] Real alpha blending, driven from `render_state` (`D3DRS_ALPHABLENDENABLE`/`SRCBLEND`/`DESTBLEND`/
  `BLENDOP`).
- [x] D3DFORMAT↔VkFormat conversion table — the full locked MW2-first scope, all 13 formats:
  `A8R8G8B8, X8R8G8B8, R5G6B5, A8, DXT1/3/5 (BC1/2/3), D24S8/D24X8, A16B16G16R16F, Q8W8V8U8, L8, V8U8`.
- [x] Terminal integration test (`d3d9_texture_test.cpp`): textures, indexed draws, real depth, and
  real blending proven together in one render, 4/4 analytic pixel checks exact.

### Known bugs & limitations carried out of M2 (all three now resolved one way or another — none silently dropped)
- [x] **`TEXCOORD0` varying interpolation "bug" — investigated (2026-07-04), does NOT reproduce; no host
  fix needed.** The originally-reported symptom (genuine `D3DFVF_XYZ|D3DFVF_TEX1` + `TEXCOORD0`, U
  interpolates correctly but V does not) could not be reproduced against the current host. Live
  evidence: a diagnostic PS (`return float4(input.uv, 0, 1)`) against the exact quad shape/position and
  exact UV figures the original report cited (0.25, 0.75) reads back both U and V correctly at every
  sampled point, including asymmetric (non-center) screen locations — see `d3d9_texcoord_test.cpp`,
  which also proves a real `tex2D()` sample through a genuine `TEXCOORD0` varying reads the correct
  texture quadrant at all four UV combinations. The one concrete lead (`d3d9_shader_translator.cpp`
  passing `varying_map_info` to the VS `compile_stage` call but `nullptr` to the PS one) was tried both
  ways — passing the same map to the PS call too produces byte-identical SPIR-V and byte-identical
  rendered pixels, because vkd3d-shader's own `ir.c` (`shader_version.type != VKD3D_SHADER_TYPE_PIXEL`
  gate) never applies the transform `varying_map_info` drives to a pixel shader's own output signature —
  a PS has no "next stage" to remap for. The asymmetry is correct API usage, not a bug. Most likely
  (but circumstantial — the original scratch diagnostic no longer exists to re-run directly) explanation
  for the original report: it predates (or was never re-checked against) this session's separate "real
  Y-flip bug — in the new test itself, not the host" finding (an inverted screen-Y-to-NDC convention in a
  test's own geometry placement produces exactly this "U fine, V looks wrong" symptom without touching
  varying interpolation at all). `d3d9_texture_test.cpp` keeps its `COLOR0`-packed UV
  workaround unchanged (still proven correct, zero regression) — the workaround is no longer necessary
  but also no longer required to be removed, since it's an independently-verified-correct path. Full
  trace: `HANDOFF_MACBOOK.md` §20.
- [x] **`D3DPOOL_MANAGED` texture bug — WONTFIX: permanently closed, not code-fixable through this
  driver's DDI surface (2026-07-04). Checked here because the investigation is complete and no further
  action is expected, not because the symptom is gone — see `d3d9_managed_texture_test.cpp`, which is
  expected to keep failing forever.** Three decoupled layers, all root-caused. `CreateTexture()`
  with `D3DPOOL_MANAGED` causes `pfnCreateResource` to fire *twice* for what the app sees as one call —
  now understood to be real, expected D3D9 architecture (a sysmem "master" copy created immediately, a
  vidmem copy created lazily on first bind, synced by a real `pfnTexBlt` call this driver previously
  left as a no-op stub). **Fixed**: `pfnTexBlt` is now implemented (`umd_TexBlt`/`d3d9_host::tex_blt`),
  forwarding the sysmem copy's full pixel backing into the vidmem copy. **Still open, a second, deeper
  bug found while fixing the first**: `pfnLock`/`pfnUnlock` never carry the app's real pixel writes for
  the sysmem copy at all — live-confirmed the app's own `LockRect()` pointer differs from this driver's
  own `pfnLock` return pointer, so the app writes into `d3d9.dll`'s own private system-memory allocation
  instead. Root cause: `CBaseDevice::CanDriverManageResource` (gating `CanCreateLightWeight`, the same
  shape of undocumented capability check the `DevCaps` `0x02000000`/`0x04000000` bits already fixed for
  vertex/index buffers) is not satisfied by this driver. **Now root-caused and confirmed structurally
  uncontrollable (2026-07-04), not just unidentified**: live-traced (memory-write watch) the failing
  field (`CBaseDevice+444`, `D3DCAPS9::Caps2` bit `0x10000000` = `D3DCAPS2_CANMANAGERESOURCE`) all the
  way back through `d3d9.dll`'s own `QueryLHDDICaps`, which unconditionally clears that bit
  (`& 0xEFFFFFFF`) after querying the driver, regardless of what `GetCaps` reports — empirically
  re-verified by temporarily setting the bit in `fill_d3d9caps` and watching it get stripped live. No
  `D3DCAPS9` field any D3DDDI/WDDM driver reports can make this gate pass; this matches real D3D9/WDDM
  history (the OS's own video memory manager owns residency under WDDM, not the driver). `D3DPOOL_MANAGED`
  textures are the common case for real game asset loading — MW2 will very likely hit this. **Confirmed
  unfixable through this driver's own DDI surface (2026-07-04)**: the natural remaining candidate — a
  fuller RE of `pfnTexBlt`'s argument struct for a system-memory-source pointer field — was carried out
  in full. A live trace captured `pfnTexBlt`'s return address into `d3d9.dll` and idasql-decompiled the
  real caller, `CD3DDDIDX10::TexBlt`; the genuine 48-byte `D3DDDIARG_TEXBLT` struct it builds (now typed
  in `d3d9_ddi.hpp`) carries only two resource handles, a subresource index, a destination point, and a
  source rect — no pixel-data pointer anywhere, confirmed against the real function's own decompiled
  source, not just an empirical byte dump. A second live trace instrumented every one of this driver's
  143 device-func-table slots across this test's entire run and confirmed no other DDI call carries
  texture pixel bytes either. The real MANAGED-pool sysmem pixel data is structurally never exposed to
  any D3DDDI/WDDM driver through any DDI call for this resource kind — `d3d9.dll` keeps it entirely
  inside its own private `CMipMap` buffer end to end. `d3d9_managed_texture_test.cpp` (real
  `D3DPOOL_MANAGED`, no workaround) is expected to keep failing (sampling black, not magenta) — this is
  now a confirmed, permanent limitation, not an open lead; `d3d9_texture_test.cpp` continues to avoid the
  whole area via `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT`. Full trace: `HANDOFF_MACBOOK.md` §16.3, §17,
  §18, §19.
- [x] **Partial-buffer `Lock()` — fixed on x64 (2026-07-04, Task 6); x86 keeps the old whole-buffer-only
  behavior, a known, scoped-out gap.** `D3DDDIARG_LOCK`'s `OffsetToLock`/`SizeToLock` have no single,
  routing-path-independent struct offset — real `d3d9.dll` builds this struct two genuinely different
  ways depending on internal buffer routing (sysmem-routed vs. driver-routed), and `umd_Lock`/`umd_Unlock`
  is one routing-path-agnostic function that cannot statically tell which shape a given call used (full
  byte-level capture: `d3d9_ddi.hpp`'s `D3DDDIARG_LOCK` comment). The real fix didn't need to resolve
  that ambiguity: the "sysmem-routed" shape's driver-returned `pData` is discarded by the app regardless
  of what this driver computes (confirmed live), and this UMD's own DevCaps bits
  (`k_devcaps_driver_managed_pool`/`k_devcaps_driver_managed_index_pool`, both unconditionally set) make
  every real `D3DPOOL_DEFAULT` vertex/index buffer take the "driver-routed" path — so `umd_Lock` can
  safely read `OffsetToLock` (offset 80 on x64) unconditionally for every buffer resource without needing
  to distinguish which shape produced a given call. `SizeToLock` isn't needed either: the wire protocol's
  existing `size=0` convention ("from offset to the end of the resource") is already exactly the right
  semantics for a tail-append lock. Proven end-to-end by `d3d9_partial_lock_test.cpp`: a
  `D3DLOCK_DISCARD` fill of a buffer's first chunk survives unmodified after two subsequent
  `D3DLOCK_NOOVERWRITE` appends at higher offsets, each reading back its own distinctive byte pattern —
  with the pre-fix behavior (offset always treated as 0) the second append would have overwritten the
  first chunk instead. **x86 is explicitly NOT covered by this fix** — its driver-routed `OffsetToLock`
  struct offset isn't RE-verified (would need the same kind of live-RE pass the x64 `D3DDDIARG_LOCK` work
  already did), so x86 buffer locks keep treating every lock as an implicit whole-buffer lock,
  unconditionally, for every vertex/index buffer; a real game relying on partial-range locks on the x86
  path will silently get whole-buffer semantics with no error. Full trace: `HANDOFF_MACBOOK.md` §16.1,
  §21.

### M3 coverage items (constant registers, above, are now done)
- [ ] Multi-stream vertex sources, `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP`.
- [ ] `StretchRect`/`ColorFill`.
- [ ] Scissor rect (state already tracked; not consumed by the draw path).
- [ ] Multiple render targets (MRT).
- [ ] Mip-mapping — carried in the resource's own mip-level count, not yet consumed; every texture is
  treated as single-mip today.
- [ ] Cube/volume textures — both the resource kind (M2 only handles `texture_2d`) and
  `d3d9_shader_translator.cpp`'s per-sampler texture-dimension info (`vkd3d_shader_d3dbc_source_info`
  currently defaults to "2D" for every sampler, which will mispredict cube/volume samplers).
- [ ] Formats beyond M2's locked 11-format MW2-first table (see above) — e.g. paletted formats or
  additional compressed/HDR formats a real game's asset pipeline may use.
- [ ] SM3.0 caps — `fill_d3d9caps` still reports SM2.0 (`vs_2_0`/`ps_2_0`). Restoring 3.0 needs its
  own caps-gauntlet RE pass, same shape as the SM2.0 one already done (expect more undocumented bits).

### Cross-cutting / infra
- [x] **WoW64/x86 shader path** — done, see the WoW64 row in the milestone table above. Typed
  `__stdcall` thunk arities, x86 struct layout (including a live-RE'd fix for `D3DDDIARG_LOCK`/
  `D3DDDIARG_UNLOCK`'s genuinely different x86 shape, not just a pointer-shrunk x64 copy), and i686
  build tooling are all in place, proven end-to-end by `d3d9-triangle-test-x86.exe` reaching pixel
  parity with the x64 triangle test under real WoW64 against the genuine 32-bit `d3d9.dll`. **Since
  extended:** the shader/const/texture/texcoord guest tests (M1.5/M2's features: real shaders, constant
  registers, textures, depth, blending, real `TEXCOORD0`) have all been ported to x86 too and pass with
  the same analytic pixel-exact results as x64 — see the WoW64 row above for the two x86-only bugs this
  caught and fixed (`allocate_id()`'s 32-bit `HANDLE` truncation; `D3DDDIARG_CREATERESOURCE`'s x86
  output-handle offset). **Still open:** partial-buffer Lock support is x64-only (see "Known bugs &
  limitations" above), and 5 low-confidence DDI slot arities (`pfnCheckCounter`, `pfnSetMarker`,
  `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified since none of the current x86
  tests hit them.
- [ ] Pipeline-key system beyond one shader pair at a time — **not actually a gap**, just not yet
  exercised. `ensure_programmable_pipeline`'s cache is already a real per-pair cache
  (`programmable_pipelines_`, keyed by `(vertex_shader_id << 32 | pixel_shader_id)`); it just hasn't
  seen more than one distinct VS/PS pair in a single run yet. No rework needed, just more testing.

### Explicitly not planned (per the original architecture's own risk sequencing)
- Fixed-function shader synthesis (M4) — the original plan's own words: "defer until a real MW2 draw
  needs it." Not free-floating scope creep; a deliberate call given MW2 is SM3-heavy.
- A DXVK pixel-diff oracle — dropped in favor of analytic pixel-readback checks throughout M1/M1.5.
  Revisit only if analytic checks stop being sufficient for a more complex milestone.

## Sequencing recommendation

M2, the WoW64/x86 UMD port, and the x86 porting of the shader/const/texture/texcoord guest tests are all
done — the WoW64 path now has the same feature coverage proof as x64 (shaders, constant registers,
textures, depth, blending, real `TEXCOORD0`), not just the fixed-function triangle. All three
M2-carried findings are now fully root-caused, each to a different outcome: `TEXCOORD0` interpolation
turned out not to be a real bug at all (investigated, does not reproduce; see above, no code changed);
partial-buffer Lock is now genuinely fixed with real code (on x64 — whole-buffer semantics remain an
x86-only, scoped-out gap, not RE-verified yet, not a blocker for further x64 work); and `D3DPOOL_MANAGED`
got a real partial fix (the double-resource-creation sync, via `pfnTexBlt`) but its deeper remaining
symptom is confirmed permanently unfixable through this driver's own DDI surface (see above) and will
need a fundamentally different mechanism before MW2 integration, since it is likely to block real game
asset loading outright, not just degrade a corner case.

Per the original plan's "de-risk earliest/riskiest first" philosophy, int/bool constant registers were
taken ahead of the rest of M3 (real games hit shader flow control before most of M3's other items —
multi-stream, `*_UP` draws, MRT, cube/volume) and are now done, proven pixel-exact on both x64 and x86.
M3's remaining DDI-coverage items can proceed in roughly the order listed above. `D3DPOOL_MANAGED`'s
confirmed-permanent limitation should be treated as a standing MW2-integration risk to design around
(e.g. a different resource-management strategy for managed-pool assets) rather than something more DDI
coverage will incidentally fix — it won't. If x86 partial-buffer Lock support becomes necessary before
MW2 integration, budget for the same kind of live-RE pass (`D3DDDIARG_LOCK`'s x86 driver-routed
`OffsetToLock` offset) that resolved the x64 case.
