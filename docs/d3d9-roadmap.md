# D3D9-over-Vulkan roadmap

Single source of truth for what's done and what's left on the path to running a real D3D9 game
(target: MW2, 32-bit, SM3) through sogen's D3D9-over-Vulkan translation layer. Update this doc at
the end of every slice — don't let deferred items scatter across commit messages and handoff notes
again.

Architecture reference: `.claude/plans/scalable-giggling-fern.md` (original milestone design, M1-M5
below map to its `## Milestones & verification` table). Session-by-session narrative/RE detail lives
in `HANDOFF_MACBOOK.md` — this doc is the rollup, that one is the diary.

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
| M2 | Textured + depth quad (`SetTexture`/sampler, indexed draw, depth, alpha blend) | **Done** | Real GPU-backed 2D textures (single mip, lazy staging upload), real `vulkan_host::create_sampler` + combined-image-sampler binding (RE'd sampler-state DDI encoding), indexed draws, real depth testing (D32_SFLOAT_S8_UINT in place of D24S8 for MoltenVK), real alpha blending, and the SPIR-V-side combined-sampler binding in the shader translator — all proven together by `d3d9_texture_test.cpp` (4/4 analytic pixel checks exact). D3DFORMAT↔VkFormat table covers exactly the locked MW2-first set (13 D3DFORMAT values, collapsing to 11 distinct VkFormat outputs since A8R8G8B8/X8R8G8B8 both map to B8G8R8A8_UNORM and L8/A8 both map to R8_UNORM — see below). Two real bugs found building the terminal test remain open, and the index/vertex Lock fix is a permanent partial-buffer limitation, not a full fix — see "Known bugs & limitations carried out of M2" below. |
| WoW64 | WoW64/x86 D3D9 UMD port (32-bit `sogen_d3d9um`, SysWOW64) | **Done** | Not in the original plan's M1-M5 table; pulled forward per the user's own priority order since real games (MW2/BO2/GTA SA) are 32-bit and nothing else matters without a working x86 guest path. Proven: typed per-slot `__stdcall` thunk arities for all 143 `D3DDDI_DEVICEFUNCS` slots (28 real implementations unchanged, the other 115 now get a correctly-sized `stub_args_N` instead of x64's zero-arg `device_stub`, which would desync callee-cleanup x86's stack); x86-specific `D3DDDIARG_*`/`D3DDDI_*` struct layout for every real slot with a HANDLE/pointer field, pinned by `#ifndef _WIN64` `static_assert`s, including a live-RE'd fix for `D3DDDIARG_LOCK`/`D3DDDIARG_UNLOCK` (x86 is a genuinely different two-tier struct shape via `DdLockLH`, not just a pointer-shrunk copy of the x64 layout); `i686-w64-mingw32-g++` build tooling (parallel commands in `src/samples/sogen-d3d9-umd/README.md`) producing `sogen_d3d9um-x86.dll`. End-to-end proof: `d3d9-triangle-test-x86.exe`, cross-compiled to i686 and run through the real WoW64 path against the genuine 32-bit Microsoft `d3d9.dll` (not DXVK), reaches the x86 UMD and reads back the exact same pixel (`B=FF G=80 R=40 A=FF`) as the x64 `d3d9-triangle-test` — full pixel parity. **Not yet covered:** only the triangle guest test has been ported/verified on x86 — `d3d9-shader-test`, `d3d9-const-test`, and `d3d9-texture-test` (real shaders, constant registers, textures, depth, blending) have not been cross-compiled or run on x86 yet, so M1.5/M2's features are unconfirmed on the x86 path. Five DDI slot arities flagged low-confidence during design (`pfnCheckCounter`, `pfnSetMarker`, `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified against the real `d3d9.dll`, since a fixed-function triangle draw never calls them — a future x86 test that hits one of these slots could still find a stack-corrupting arity mismatch. |
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats, **int/bool constant registers**) | **Not started** | Inherits M2's two open bugs and the partial-Lock limitation (see below) — none of this is M3-net-new work, it's M2 debt M3 must not silently re-break. |
| M4 | Fixed-function synthesis | **Deferred by design** | Original plan: "only if MW2 needs it." MW2 is SM3-heavy; likely skippable. Revisit if a real game draw path turns out to need FF after all. |
| M5 | MW2 integration (WoW64, SM3, 32-bit UMD) | **Not started, blocked on M3** | The WoW64/x86 UMD port itself is done (see the WoW64 row above) — the remaining blocker is real MW2 integration work: M3's DDI coverage, SM3 caps, and re-verifying M1.5/M2's shader/texture/constant-register features on the x86 path specifically (only the fixed-function triangle draw has been proven there so far). |

## Remaining work, consolidated (supersedes scattered "Deferred work" notes in HANDOFF_MACBOOK.md)

### Constant registers
- [x] Float (`c#`) — done, this session.
- [ ] **Int (`i#`) / bool (`b#`)** — nothing exists yet: no wire protocol structs, no `device_state`
  storage, no host-side UBO/descriptor binding, no UMD-side DDI functions (not even wired to a slot).
  Each register *set* maps to its own CBV in vkd3d-shader, so this expands the binding contract from
  2 descriptor sets to up to 4. Used for shader flow control (loops/branches) in vs_2_x+ — lower
  priority than textures for a first real draw, but real games will hit it eventually (M3 territory).
  `D3DDDIARG_SETVERTEXSHADERCONSTI`/`...CONSTB` structs in `d3d9_ddi.hpp` are explicitly flagged as
  unverified (mirror the float struct's fixed bug, but never confirmed empirically).

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

### Known bugs & limitations carried out of M2 (real, open, not lost)
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
  (but circumstantial -- the original scratch diagnostic no longer exists to re-run directly) explanation
  for the original report: it predates (or was never re-checked against) this session's separate "real
  Y-flip bug — in the new test itself, not the host" finding (an inverted screen-Y-to-NDC convention in a
  test's own geometry placement produces exactly this "U fine, V looks wrong" symptom without touching
  varying interpolation at all). `d3d9_texture_test.cpp` keeps its `COLOR0`-packed UV
  workaround unchanged (still proven correct, zero regression) — the workaround is no longer necessary
  but also no longer required to be removed, since it's an independently-verified-correct path. Full
  trace: `HANDOFF_MACBOOK.md` §20.
- [ ] **`D3DPOOL_MANAGED` texture bug — confirmed unfixable through this driver's DDI surface (2026-07-04),
  three decoupled layers, all root-caused.** `CreateTexture()`
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
- [ ] **Partial-buffer `Lock()` is a permanent limitation, not a stopgap.** `D3DDDIARG_LOCK`'s
  `OffsetToLock`/`SizeToLock` have no single, routing-path-independent struct offset — real `d3d9.dll`
  builds this 104-byte struct two genuinely different ways depending on internal buffer routing
  (sysmem-routed vs. driver-routed), and `umd_Lock`/`umd_Unlock` is one routing-path-agnostic function
  that cannot statically tell which shape a given call used (full byte-level capture: `d3d9_ddi.hpp`'s
  `D3DDDIARG_LOCK` comment). Fixed by always treating every lock as an implicit whole-buffer lock —
  `OffsetToLock`/`SizeToLock` are never read at all now, unconditionally, for every vertex/index
  buffer. A real game relying on partial-range locks (the common `D3DLOCK_NOOVERWRITE`
  growing-dynamic-buffer pattern) will silently get whole-buffer semantics with no error. Supporting
  real partial locks would need per-routing-path struct detection at the `pfnLock` call site itself —
  not attempted. Full trace: `HANDOFF_MACBOOK.md` §16.1.

### M3 coverage items (beyond int/bool constants above)
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
  parity with the x64 triangle test under real WoW64 against the genuine 32-bit `d3d9.dll`. **Still
  open:** only the triangle test has been verified on x86 — the shader/const/texture guest tests
  (M1.5/M2's features: real shaders, constant registers, textures, depth, blending) have not been
  ported to x86 yet, and 5 low-confidence DDI slot arities (`pfnCheckCounter`, `pfnSetMarker`,
  `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified since the triangle draw
  never hits them.
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

M2 and the WoW64/x86 UMD port are both done. Per the original plan's "de-risk earliest/riskiest first"
philosophy, the next slice should be **porting the shader/const/texture guest tests to x86** before
leaning further into M3 — the WoW64 port has only proven the fixed-function triangle path so far, and
M2's texture/shader/constant-register features (plus the two DevCaps-routing-bit findings and the
`D3DDDIARG_LOCK` ambiguity, both originally found via x64-only RE) still need x86-side confirmation
before assuming they transfer unchanged. Once that's confirmed, **int/bool constant registers before
further M3 coverage**, same reasoning as before: real games hit shader flow control before most of
M3's other items (multi-stream, `*_UP` draws, MRT, cube/volume). All three M2-carried findings are now
fully root-caused — `TEXCOORD0` interpolation turned out not to be a real bug (investigated, does not
reproduce; see above), `D3DPOOL_MANAGED` specifically is confirmed unfixable through this driver's own
DDI surface (see above) and will need a fundamentally different mechanism before MW2 integration, since
it is likely to block real game asset loading outright, not just degrade a corner case, and
partial-buffer Lock remains a permanent, real limitation (whole-buffer semantics only).
