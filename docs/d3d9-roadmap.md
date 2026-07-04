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
| M2 | Textured + depth quad (`SetTexture`/sampler, indexed draw, depth, alpha blend) | **Not started** | Next candidate slice. |
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats, **int/bool constant registers**) | **Not started** | |
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

### Textures (M2)
- [ ] Plain sampled `texture_2d` resources get zero GPU backing today (`d3d9_host::create_resource`
  only allocates real Vulkan images for render-target/depth-stencil usage). Needs `create_image` +
  staging upload via the existing (already-generic) Lock/Unlock path.
- [ ] `umd_SetSamplerState` doesn't exist in the guest UMD — the host-side `d3d9_set_sampler_state`
  wire handler is unreachable from the guest. DDI slot for `pfnSetSamplerState` currently points at
  `device_stub`.
- [ ] `device_state::bound_textures`/`sampler_state`/`texture_stage_state` are written but never read
  by `execute_draw`/the pipeline builders — need real `vulkan_host::create_sampler` +
  combined-image-sampler descriptors wired into the programmable pipeline.
- [ ] D3DFORMAT↔VkFormat conversion table. Locked scope from the original plan (MW2-first):
  `A8R8G8B8, X8R8G8B8, R5G6B5, A8, DXT1/3/5 (BC1/2/3), D24S8/D24X8, A16B16G16R16F, Q8W8V8U8, L8, V8U8`.
- [ ] `d3d9_shader_translator.cpp` needs per-sampler texture-dimension info
  (`vkd3d_shader_d3dbc_source_info`) — currently defaults to "2D" for every sampler, which will
  mispredict cube/volume samplers.
- [ ] Mip-mapping — carried in the resource's own mip-level count, not yet consumed.

### M3 coverage items (beyond int/bool constants above)
- [ ] Multi-stream vertex sources, `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP`.
- [ ] `StretchRect`/`ColorFill`.
- [ ] Scissor rect (state already tracked; not consumed by the draw path).
- [ ] Multiple render targets (MRT).
- [ ] Cube/volume texture formats (beyond the M2 2D format table).
- [ ] SM3.0 caps — `fill_d3d9caps` still reports SM2.0 (`vs_2_0`/`ps_2_0`). Restoring 3.0 needs its
  own caps-gauntlet RE pass, same shape as the SM2.0 one already done (expect more undocumented bits).

### Cross-cutting / infra
- [ ] **WoW64/x86 shader path** — the UMD is x64-only. The 32-bit `sogen_d3d9um` (SysWOW64) DLL needs
  typed `__stdcall` thunks per device-func slot instead of the generic caller-cleanup stub before any
  of this session's shader-DDI work (or M2/M3) applies there. MW2 is a 32-bit game — this is a hard
  blocker for M5, should probably be tackled before M3 gets too far ahead of it.
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

Per the original plan's "de-risk earliest/riskiest first" philosophy: **M2 (textures) before int/bool
constants**, because a textured quad is the more common real-world blocker (nearly every material
needs a texture; flow-control registers are rarer in simple shaders) and because M2's format-table
work is independent of the constant-register binding-contract expansion — doing them in the opposite
order wouldn't unblock anything textures need. WoW64/x86 should be scheduled before M3 goes too deep,
since it's a hard prerequisite for M5 (the actual game) and left too late makes everything after it a
second pass.
