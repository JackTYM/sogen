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
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats, **int/bool constant registers**) | **Not started** | Inherits M2's two open bugs and the partial-Lock limitation (see below) — none of this is M3-net-new work, it's M2 debt M3 must not silently re-break. |
| M4 | Fixed-function synthesis | **Deferred by design** | Original plan: "only if MW2 needs it." MW2 is SM3-heavy; likely skippable. Revisit if a real game draw path turns out to need FF after all. |
| M5 | MW2 integration (WoW64, SM3, 32-bit UMD) | **Not started, blocked on M2-M3** | Needs the 32-bit `sogen_d3d9um`/SysWOW64 port (typed `__stdcall` thunks, not yet built) before any x64-only shader-DDI work applies there. |

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
- [ ] **`TEXCOORD0` varying interpolation bug.** With a genuine `D3DFVF_XYZ|D3DFVF_TEX1` vertex format
  and a `float2`/`float4` `TEXCOORD0` PS input, the U component interpolates correctly across screen
  space but V consistently does not (e.g. an expected 0.25 reads back around 0.87). Isolated to the
  varying itself via a visualize-the-interpolant diagnostic shader — geometry, the texture, the sampler
  descriptor, and the PS constant register were all independently confirmed correct. Not root-caused;
  `d3d9_texture_test.cpp` works around it by routing UV through the vertex's `D3DFVF_DIFFUSE` color
  channel (`COLOR0.rg`) instead of a real `TEXCOORD0`, which every prior guest test proves interpolates
  correctly. Any future test or real game content that relies on a genuine `TEXCOORD0` varying will hit
  this. Full trace: `HANDOFF_MACBOOK.md` §16.3.
- [ ] **`D3DPOOL_MANAGED` texture double-resource-creation bug.** `CreateTexture()` with
  `D3DPOOL_MANAGED` causes `pfnCreateResource` to fire *twice* for what the app sees as one call, with
  two different output handles — one that `LockRect()` uses (and correctly receives app pixel writes)
  and a different, never-written one that `SetTexture()` uses, so a `D3DPOOL_MANAGED` sampled texture
  reads back as entirely black/transparent. Not root-caused; `d3d9_texture_test.cpp` avoids it by using
  `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT` (confirmed live to issue exactly one `pfnCreateResource` call).
  `D3DPOOL_MANAGED` textures are the common case for real game asset loading — MW2 will very likely hit
  this. Full trace: `HANDOFF_MACBOOK.md` §16.3.
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
- [ ] **WoW64/x86 shader path** — the UMD is x64-only. The 32-bit `sogen_d3d9um` (SysWOW64) DLL needs
  typed `__stdcall` thunks per device-func slot instead of the generic caller-cleanup stub before any
  of this session's shader-DDI work (or M2/M3) applies there. MW2 is a 32-bit game — this is a hard
  blocker for M5, should probably be tackled before M3 gets too far ahead of it. **New concern from
  M2:** the two DevCaps-routing-bit findings (vertex buffers and index buffers each gate through their
  own, separately-discovered bit) and the `D3DDDIARG_LOCK` two-shapes-for-one-struct ambiguity were
  both found via x64 live RE against the real `d3d9.dll`'s x64 code paths. The x86 build of the same
  DLL is not guaranteed to share either finding's exact bits/offsets/calling convention — the WoW64
  port should budget time to re-verify both from scratch on x86, not assume the x64 findings transfer
  unchanged.
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

M2 is done. Per the original plan's "de-risk earliest/riskiest first" philosophy, the next slice
should be **WoW64/x86, not deeper M3 coverage** — it's a hard prerequisite for M5 (the actual game,
32-bit), and M2 surfaced two separate x64-specific DDI findings (the per-buffer-kind DevCaps routing
bits, the `D3DDDIARG_LOCK` two-shapes ambiguity) that the x86 port cannot simply inherit; left until
after M3 balloons, every one of M3's DDI additions would need a second, x86-specific RE pass on top of
its x64 one. Once WoW64 is up, **int/bool constant registers before further M3 coverage**, same
reasoning as before: real games hit shader flow control before most of M3's other items (multi-stream,
`*_UP` draws, MRT, cube/volume). The three open M2 bugs (`TEXCOORD0` interpolation, `D3DPOOL_MANAGED`
double-creation, partial-buffer Lock) should be root-caused before M3 leans on any of them further —
`D3DPOOL_MANAGED` in particular is likely to block real game asset loading outright, not just degrade
a corner case.
