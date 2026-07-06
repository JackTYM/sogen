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

## Performance — D3D9 native path

This section tracks the D3D9-over-Vulkan translation layer's own performance (`d3d9_host.cpp`/`.hpp`,
`vulkan_host.cpp`). It's separate from the generic Vulkan-ICD bridge's performance work, which lives in
`docs/gpu-paravirtualization.md`.

### Confirmed bug (found and fixed 2026-07-05): mandatory per-draw/per-clear GPU->CPU readback

A dedicated performance audit found that every single `execute_draw` (DrawPrimitive/DrawIndexedPrimitive)
and `d3d9_clear` (`Clear()`) call performed a mandatory, unconditional, synchronous, **blocking**
GPU->CPU readback of the render target it touched — a full second command-buffer submit,
`vkWaitForFences(UINT64_MAX)`, a full-image `vkCmdCopyImageToBuffer`, and a CPU `memcpy` — regardless
of whether the guest app ever actually needed those pixels on the CPU (i.e. regardless of whether it
ever called `Lock()`/`GetRenderTargetData()` on that resource before its next draw). At realistic game
draw counts (500-1000+ draws/frame), this meant 1000-2000+ blocking GPU round trips per frame:
single-digit-FPS territory, dominated entirely by CPU-GPU sync stalls rather than actual rendering
work. This wasn't a minor inefficiency — it made real game framerates structurally impossible
regardless of how fast the actual Vulkan rendering was.

### Fix (genuinely fixed with real code, 2026-07-05): dirty-flag / deferred-readback model

Seven commits: `596b0b31`, `ecec18fb`, `ab8f2f87`, `0d6282ad`, `2f16eaf3`, `93c42040`, `dad9f5f8`.

1. `resource_entry` (`d3d9_host.hpp`) gained a `backing_dirty` flag, set by every draw/clear that
   renders to a color render target (`596b0b31`).
2. `d3d9_host::sync_backing_from_gpu(resource_entry& rt)` (`d3d9_host.cpp`) performs the GPU->CPU
   readback **only if `backing_dirty` is set**, clearing the flag on success. It's wired into both real
   consumers of a resource's CPU-side pixel backing: the `lock()` wire-command handler (`ecec18fb`) and
   `snapshot_resource`, the Present-path pixel copy (`0d6282ad`) — Present reads `.backing` directly,
   same as Lock, so it needs the same sync or a guest that renders-then-Presents without ever locking
   the backbuffer would show stale pixels once the eager path is gone.
3. The eager, unconditional readback was then removed entirely from `execute_draw` and `d3d9_clear`
   (`2f16eaf3`), making `sync_backing_from_gpu` the **sole** place a readback ever happens — and only
   when something (Lock/Present) actually needs the pixels and the resource is actually dirty. A
   follow-up commit (`93c42040`) corrected three doc comments (class-level, the Part-3 draw-path
   comment, and `sync_backing_from_gpu`'s own) that still described the old eager model after the code
   changed.

**Net effect**: a game issuing hundreds of draws/frame now does **zero** GPU->CPU readbacks per frame
unless it actually calls `Lock()`/`GetRenderTargetData()` — the confirmed per-draw/per-clear stall is
eliminated.

### Layout-tracking fragility found and fixed while closing this out (`dad9f5f8`)

Removing the eager readback made `sync_backing_from_gpu`'s correctness depend entirely on
`vulkan_host::readback_render_target`'s own safety check (verifying an image is in
`VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` before issuing the copy) actually reflecting reality. It didn't:
that check reads a CPU-side mirror field, `render_target_data::current_layout`, which was previously
updated **only by `submit_clear`** — never by `cmd_pipeline_barrier`, the single choke point every other
layout transition (including every one of `execute_draw`'s own leading/trailing barriers) flows through.
The check was therefore only correct by coincidence (every draw barrier happened to be symmetric around
the same layout), not because it verified the image's actual current layout. Fixed by having
`cmd_pipeline_barrier` itself update `current_layout` whenever the transitioned image is a tracked
render target, making the mirror accurate for every barrier, not just `submit_clear`'s.

### Verification

Full regression sweep (independently repeated multiple times): every existing D3D9 guest test passes on
**both x64 and x86**, pixel values byte-identical to their previously-documented values — `shader`,
`const`, `texture`, `texcoord`, `partial-lock`, `int-bool-const`, `scissor`, `mrt`, `multistream` all
pass; `managed-texture` still fails the same documented, permanent, unrelated limitation (see
"Known bugs & limitations" below). Smoke test: 26/26.

### Known remaining limitation: this fix does NOT address the rest of the performance story

This slice was identified as the single highest-leverage fix toward a fast GPU-paravirtualized D3D9
pipeline, but it is one slice, not the whole story. Explicitly **not** touched by this fix, and still
real, separate, larger pieces of remaining work:
- **Busy-spin fence-wait** — the present/submit path still waits on Vulkan fences via a busy-spin poll
  rather than a genuine blocking wait or async completion callback.
- **Per-draw buffer/UBO/descriptor-set allocation churn** — vertex/constant buffers and descriptor sets
  are still allocated fresh per draw rather than pooled/reused across frames.

Do not read this fix as having addressed either of the two items above — it didn't. A further planned
slice (persistent GPU resource pooling + pipelined multi-frame submission) would need to address them
and is separate, larger work.

### Fixed (genuinely fixed with real code, 2026-07-05): D3D9 DDI-call wire batching

The item previously listed here as a known remaining limitation — "no wire-protocol batching for D3D9
DDI calls, each DDI call crosses the guest/host wire individually" — is now fixed. Four commits:
`ecda4363`, `e1ec179a`, `87863527`, `5bac1070`.

State-setting/draw/clear DDI calls (`SetRenderState`, `SetTexture`, `DrawPrimitive`, `Clear`, ~22 call
sites in `sogen_d3d9_umd.cpp`) now accumulate guest-side in `g_d3d9_command_batch` via `record_d3d9`,
reusing the already-existing `record_commands`/`ioctl_record_commands` wire mechanism (previously only
used by the generic Vulkan-ICD bridge) with zero host or wire-format changes. `bridge_call` gained a
guard that flushes any pending batch as a single `ioctl_record_commands` Escape before every call that
needs the host to observe synchronous state first — Lock, Unlock, Present, CreateResource, TexBlt, and
shader/vertex-declaration creation.

**Verification**: live instrumentation on the `texture` guest test (`d3d9_texture_test.cpp`) showed a
real batch of 15+ DDI calls (SetRenderTarget, SetDepthStencil, SetStreamSource, SetIndices, SetTexture,
shader binds, viewport, Clear, 4x DrawIndexedPrimitive) collapsing into one 1256-byte flush right before
the readback `Lock()` that needed to observe them — confirming genuine batching across multiple calls,
not just per-call flushing routed through a new mechanism. Full regression sweep (shader, const,
texture, texcoord, partial-lock, int-bool-const, scissor, mrt, multistream, x64+x86 where applicable)
and the 26-subtest smoke test all pass with unchanged pixel values.

## Status by milestone

| # | Milestone | Status | Notes |
|---|-----------|--------|-------|
| M1 | Programmable SM2/3 triangle (VS+PS bytecode, DrawPrimitive, Present) | **Done** | Real `D3DCompile()` → `vkd3d-shader` → SPIR-V → Vulkan pipeline, pixel-verified. No DXVK oracle was built (dropped in favor of analytic pixel-readback checks, same rigor, no new dependency). |
| M1.5 | Float constant registers (`c#`) | **Done** | Not in the original plan as a separate milestone; pulled forward because a WVP matrix is needed for M2 anyway. UBO + descriptor-set binding contract now proven for one register set. |
| M2 | Textured + depth quad (`SetTexture`/sampler, indexed draw, depth, alpha blend) | **Done** | Real GPU-backed 2D textures (single mip, lazy staging upload), real `vulkan_host::create_sampler` + combined-image-sampler binding (RE'd sampler-state DDI encoding), indexed draws, real depth testing (D32_SFLOAT_S8_UINT in place of D24S8 for MoltenVK), real alpha blending, and the SPIR-V-side combined-sampler binding in the shader translator — all proven together by `d3d9_texture_test.cpp` (4/4 analytic pixel checks exact). D3DFORMAT↔VkFormat table covers exactly the locked MW2-first set (13 D3DFORMAT values, collapsing to 11 distinct VkFormat outputs since A8R8G8B8/X8R8G8B8 both map to B8G8R8A8_UNORM and L8/A8 both map to R8_UNORM — see below). Of the two bugs found building the terminal test: `D3DPOOL_MANAGED` got a real code fix for its double-resource-creation half (`pfnTexBlt` now syncs the sysmem/vidmem copies), and its deeper remaining symptom (managed textures sample black) — for a long time confirmed permanently unfixable through this driver's own DDI surface — is now FIXED on both x64 and x86/WoW64 by a different mechanism entirely, a permanent runtime memory patch to `d3d9.dll` itself (see the `D3DPOOL_MANAGED` entry below); TEXCOORD0 interpolation turned out not to be a real bug at all — investigated and does not reproduce, no code change needed. Both are fully investigated, see "Known bugs & limitations carried out of M2" below for the three-way distinction (fixed / confirmed non-bug / confirmed permanent) — `D3DPOOL_MANAGED` has since moved from that third category into the first, on both x64 and x86. The partial-buffer Lock limitation is now genuinely fixed on x64 (still whole-buffer-only on x86, a scoped-out gap) — also below. |
| WoW64 | WoW64/x86 D3D9 UMD port (32-bit `sogen_d3d9um`, SysWOW64) | **Done** | Not in the original plan's M1-M5 table; pulled forward per the user's own priority order since real games (MW2/BO2/GTA SA) are 32-bit and nothing else matters without a working x86 guest path. Proven: typed per-slot `__stdcall` thunk arities for all 143 `D3DDDI_DEVICEFUNCS` slots (28 real implementations unchanged, the other 115 now get a correctly-sized `stub_args_N` instead of x64's zero-arg `device_stub`, which would desync callee-cleanup x86's stack); x86-specific `D3DDDIARG_*`/`D3DDDI_*` struct layout for every real slot with a HANDLE/pointer field, pinned by `#ifndef _WIN64` `static_assert`s, including a live-RE'd fix for `D3DDDIARG_LOCK`/`D3DDDIARG_UNLOCK` (x86 is a genuinely different two-tier struct shape via `DdLockLH`, not just a pointer-shrunk copy of the x64 layout); `i686-w64-mingw32-g++` build tooling (parallel commands in `src/samples/sogen-d3d9-umd/README.md`) producing `sogen_d3d9um-x86.dll`. End-to-end proof: `d3d9-triangle-test-x86.exe`, cross-compiled to i686 and run through the real WoW64 path against the genuine 32-bit Microsoft `d3d9.dll` (not DXVK), reaches the x86 UMD and reads back the exact same pixel (`B=FF G=80 R=40 A=FF`) as the x64 `d3d9-triangle-test` — full pixel parity. **Now also covered:** `d3d9-shader-test`, `d3d9-const-test`, `d3d9-texture-test`, and `d3d9-texcoord-test` have all been cross-compiled to i686 and pass on x86 too (all analytic pixel checks exact, matching the x64 results) — M1.5/M2's shader, constant-register, texture, depth, blend, and real-`TEXCOORD0` features are now confirmed on the x86 path, not just the fixed-function triangle. Porting these caught two real, x86-only bugs along the way (both fixed): `d3d9_host::allocate_id()`'s shared id counter started at `1ULL << 32`, silently truncating through a 32-bit `HANDLE` on x86 guests (fixed, starts at `0x10000`; commit `c35871ca`); and `D3DDDIARG_CREATERESOURCE`'s output-handle offset is 44 on x86, not 48 as RE'd for x64 (a clean 4-byte shift, fixed; commit `c5dd3d27`) — full narrative for both in `src/samples/sogen-d3d9-umd/README.md`. Full regression sweep after all this session's fixes landed: every x64 and x86 guest test green, smoke test 26/26 (2026-07-04). Partial-buffer Lock support (see below) is x64-only — x86 keeps whole-buffer-lock semantics, a known, explicitly scoped-out gap (its driver-routed `OffsetToLock` offset isn't RE-verified). Five DDI slot arities flagged low-confidence during design (`pfnCheckCounter`, `pfnSetMarker`, `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified against the real `d3d9.dll`, since none of the current x86 tests call them — a future x86 test that hits one of these slots could still find a stack-corrupting arity mismatch. **Now also covered (2026-07-05):** `d3d9-scissor-test`, `d3d9-mrt-test`, and `d3d9-multistream-test` have all been cross-compiled to i686 and pass on x86 too, pixel-exact against x64 — unlike several earlier ports, this one found ZERO new x86-only bugs; all three worked unchanged on the first run. Full regression sweep after this session's work: every x64 and x86 guest test green (`d3d9-managed-texture-test` fails by design, permanent limitation, at the time of this sweep — since fixed on both x64 and x86, see the `D3DPOOL_MANAGED` entry below), smoke test 26/26 (2026-07-05). **Now also covered (2026-07-05):** `d3d9-managed-texture-test` has been cross-compiled to i686 and passes on x86 too, pixel-exact against x64 — the `install_d3d9_caps_patch_hook` I386 branch (see the `D3DPOOL_MANAGED` entry below) makes the 32-bit `syswow64/d3d9.dll` path work, closing the last x86/WoW64 D3DPOOL_MANAGED gap. |
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats) | **In progress** | Int/bool constant registers, originally scoped as part of this milestone, are done (see "Constant registers" below) — pulled forward for the same reason float constants were (real games hit shader flow control early). **Three more M3 items are now done**: multi-stream vertex sources (real per-stream `SetStreamSource` offsets + a new `D3DVERTEXELEMENT9` declaration parser), scissor rect (`D3DRS_SCISSORTESTENABLE`-gated real clipping), and multiple render targets (real N-RT rendering + all-bound-RTs `Clear()`) — see the "M3 coverage items" checklist below for the full account of each, including the 3 UMD bugs the multi-stream work found/fixed and the RT-slot-compaction bug found/fixed during MRT work. Proven pixel-exact on both x64 and x86/WoW64 (all three ported to i686 with zero source changes, zero new x86 bugs). **Still not started**: `*_UP` draws, `StretchRect`/`ColorFill`, cube/volume textures, formats beyond the M2 table, SM3.0 caps, mip-mapping, and `stream_frequencies`/instancing. The remaining M3 work inherits (on x86 only) the partial-Lock gap; the pipeline-cache-key system itself has no known open gaps left (RT/vertex-decl-shape, static blend/depth render-state, and real-vertex-decl stream strides are all now fixed — see below) — none of this is M3-net-new work, it's M2 debt M3 must not silently re-break. |
| M4 | Fixed-function synthesis | **Deferred by design** | Original plan: "only if MW2 needs it." MW2 is SM3-heavy; likely skippable. Revisit if a real game draw path turns out to need FF after all. |
| M5 | MW2 integration (WoW64, SM3, 32-bit UMD) | **Not started, blocked on M3** | The WoW64/x86 UMD port itself is done (see the WoW64 row above), and its shader/texture/constant-register/texcoord features are now confirmed on the x86 path specifically (not just the fixed-function triangle) — the remaining blocker is real MW2 integration work: M3's DDI coverage and SM3 caps. `D3DPOOL_MANAGED` is now fixed on both x64 and x86/WoW64 (see below) via a permanent runtime memory patch, not more DDI coverage — and since the 32-bit `syswow64/d3d9.dll` branch real MW2 (a 32-bit game) actually uses is now RE'd and wired too, MANAGED-pool asset loading is no longer a standing x86-only risk for MW2 integration. |

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
- [x] `vulkan_host::create_sampler` + combined-image-sampler descriptors wired into the programmable
  pipeline for **all four PS sampler stages, s0..s3 — extended from s0-only (fixed 2026-07-06, commits
  `fd24dcea`/`2b80506e`/`6ffa2d9a`/`fb7999c6`).** Previously exactly one hardcoded sampler binding (s0,
  PS set 1 binding 1) existed; a real pixel shader referencing s1 or higher (diffuse+normal, real
  multi-texturing) failed translation and silently degraded to an unrendered draw — host-side graceful
  degradation, not a crash (`hr=0` from `CreatePixelShader`/`DrawIndexedPrimitive`, but the draw itself
  was silently skipped by `ensure_programmable_pipeline` returning `nullptr`, leaving stale/clear-color
  pixels). **Binding scheme** (PS descriptor set 1): binding 0 = float CBV, binding 1 = sampler s0,
  binding 2 = int CBV, binding 3 = bool CBV, and each additional sampler stage k>=1 at binding 3+k
  (s1/s2/s3 -> 4/5/6, stepping over the int/bool-const UBOs) — centralized as `max_ps_sampler_stages`/
  `ps_sampler_binding_for_stage()` in `d3d9_shader_translator.hpp` (`6ffa2d9a`, a follow-up refactor
  resolving a drift risk from the same constants being independently duplicated across
  `d3d9_shader_translator.cpp` and `d3d9_host.cpp`). **Safe/backward-compatible for single-sampler
  shaders**: over-declaring sampler bindings a shader doesn't statically reference is empirically inert —
  vkd3d-shader only emits SPIR-V for a resource the shader actually declares, confirmed against the real
  vkd3d-shader build (not assumed) via SPIR-V disassembly; a single binding with `descriptor_count>1` was
  tried and rejected by vkd3d, which is why this is four separate bindings rather than one array binding.
  Proven by a new discriminator test, `d3d9_multitexture_test.cpp` (`2b80506e`): two solid-color textures
  (RED->s0, GREEN->s1), one `D3DCompile()`'d `ps_2_0` shader summing both samples, checked analytically
  for YELLOW on the read-back render target — passes on both x64 and x86/WoW64. The test fails against
  the pre-fix host code and passes against the fixed code, confirming the discriminator is real; the
  pre-fix failure mode was corrected post-hoc (`fb7999c6`) from an initially predicted vkd3d-shader crash
  to the actual observed graceful degradation described above. Full x64/x86 regression sweep and the
  26-subtest smoke test pass unchanged at every stage. Full narrative: `HANDOFF_MACBOOK.md` §30.
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
- [x] **`D3DPOOL_MANAGED` texture bug — FIXED on both x64 and x86/WoW64 (2026-07-05).** Kept here with its
  full backstory since the investigation trail explains *why* the gate exists and why the obvious
  DDI-surface fixes don't work — only the final conclusion below is corrected, not the history. Three
  decoupled layers, all root-caused. `CreateTexture()` with `D3DPOOL_MANAGED` causes `pfnCreateResource`
  to fire *twice* for what the app sees as one call — understood to be real, expected D3D9 architecture
  (a sysmem "master" copy created immediately, a vidmem copy created lazily on first bind, synced by a
  real `pfnTexBlt` call this driver previously left as a no-op stub). **Fixed**: `pfnTexBlt` is now
  implemented (`umd_TexBlt`/`d3d9_host::tex_blt`), forwarding the sysmem copy's full pixel backing into
  the vidmem copy. **A second, deeper bug found while fixing the first**: `pfnLock`/`pfnUnlock` never
  carry the app's real pixel writes for the sysmem copy at all — live-confirmed the app's own
  `LockRect()` pointer differs from this driver's own `pfnLock` return pointer, so the app writes into
  `d3d9.dll`'s own private system-memory allocation instead. Root cause: `CBaseDevice::CanDriverManageResource`
  (gating `CanCreateLightWeight`, the same shape of undocumented capability check the `DevCaps`
  `0x02000000`/`0x04000000` bits already fixed for vertex/index buffers) is not satisfied by this driver.
  **Root-caused (2026-07-04)**: live-traced (memory-write watch) the failing field (`CBaseDevice+444`,
  `D3DCAPS9::Caps2` bit `0x10000000` = `D3DCAPS2_CANMANAGERESOURCE`) all the way back through
  `d3d9.dll`'s own `QueryLHDDICaps`, which unconditionally clears that bit (`& 0xEFFFFFFF`) after
  querying the driver, regardless of what `GetCaps` reports — empirically re-verified by temporarily
  setting the bit in `fill_d3d9caps` and watching it get stripped live. This part remains true and
  permanent: no `D3DCAPS9` field any D3DDDI/WDDM driver reports can make this gate pass through the
  *reported-caps* mechanism specifically; this matches real D3D9/WDDM history (the OS's own video memory
  manager owns residency under WDDM, not the driver). `D3DPOOL_MANAGED` textures are the common case for
  real game asset loading — MW2 will very likely hit this. **Confirmed unfixable through the reported-caps
  or TexBlt-argument DDI surface (2026-07-04)**: the natural remaining DDI-surface candidate — a fuller RE
  of `pfnTexBlt`'s argument struct for a system-memory-source pointer field — was carried out in full. A
  live trace captured `pfnTexBlt`'s return address into `d3d9.dll` and idasql-decompiled the real caller,
  `CD3DDDIDX10::TexBlt`; the genuine 48-byte `D3DDDIARG_TEXBLT` struct it builds (now typed in
  `d3d9_ddi.hpp`) carries only two resource handles, a subresource index, a destination point, and a
  source rect — no pixel-data pointer anywhere, confirmed against the real function's own decompiled
  source, not just an empirical byte dump. A second live trace instrumented every one of this driver's
  143 device-func-table slots across this test's entire run and confirmed no other DDI call carries
  texture pixel bytes either. The real MANAGED-pool sysmem pixel data is structurally never exposed to
  any D3DDDI/WDDM driver through any DDI call for this resource kind — `d3d9.dll` keeps it entirely
  inside its own private `CMipMap` buffer end to end, as long as the driver-managed gate stays closed.
  Through the DDI surface alone, this remained permanently unfixable. **FIXED, a fundamentally different
  mechanism (2026-07-05, commits `36e2a8bb`/`c42fabd4`)**: not a DDI-surface or reported-caps change at
  all, but a permanent runtime memory patch — `windows_emulator::install_d3d9_caps_patch_hook`
  (`windows_emulator.cpp`) installs an execution hook on x64 `d3d9.dll` load that re-sets
  `D3DCAPS2_CANMANAGERESOURCE` immediately after `QueryLHDDICaps`'s own strip, bypassing the reported-caps
  mechanism entirely. This routes the MANAGED-pool lock through the driver-managed path, and this driver's
  existing `umd_Lock`/`g_locked_buffers` machinery — built for ordinary resources, unmodified for this fix
  — turned out to already serve a real pixel backing once that path is taken; no new UMD code was needed.
  `d3d9_managed_texture_test.cpp` (real `D3DPOOL_MANAGED`, no workaround, no test-side leniency) now
  genuinely passes (magenta, not black) — independently verified via A/B (disabling the hook reproduces
  the old black-pixel failure) plus a full x64/x86 guest-test regression sweep; `d3d9_texture_test.cpp`
  continues to avoid the whole area via `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT` regardless. **x86/WoW64
  now covered too (2026-07-05)**: `install_d3d9_caps_patch_hook` gained a parallel I386 branch that
  gates on `mod.machine == 0x014c`, re-verifies the 32-bit `syswow64/d3d9.dll`'s own strip+store site
  (sha256 `99840c2a6b9b75011dfbb3456644e90fa7c2728b10480db1b87f7fd2e8897302`; `and eax, 0xEFFFFFFF` /
  `mov [ebx+0xc], eax` at RVA `0x51c91`, an 8-byte guard confirmed to occur exactly once in the DLL),
  and re-ORs the bit back into `[ebx+0xc]` after the store — the same mechanism as x64, just with the
  32-bit register (`EBX`, not `RSI`) and this build's RVAs. `d3d9_managed_texture_test.cpp` cross-compiled
  to i686 with zero source changes now genuinely passes on real WoW64 against the genuine 32-bit
  `d3d9.dll` (magenta pixel, `ALL CHECKS PASSED`), independently verified via the same A/B toggle
  (disabling just the I386 branch reproduces the old black-pixel failure, `B=00 G=00 R=00`). Real MW2 is
  a 32-bit game, so this is the branch that actually matters for it. Full trace: `HANDOFF_MACBOOK.md`
  §16.3, §17, §18, §19, §27, §28, and the newest dated entry.
- [x] **D3DPOOL_MANAGED — Option-A spike run (2026-07-05): the caps gate CAN be forced open at runtime;
  productionized into a permanent fix the same day (see the entry above).** The prior entry's
  "structurally uncontrollable" conclusion was about the *reported-caps* surface specifically (no
  `D3DCAPS9` value any driver reports survives `QueryLHDDICaps`'s unconditional strip) — that part still
  stands, unchanged. This spike tested a genuinely different mechanism: a live, in-memory, runtime patch
  (a Python `emu.hooks.memory_execution_at()` callback watching `d3d9.dll+0x158b3`'s caps-strip site and
  re-setting bit 28 immediately after) that never touches any reported-caps value at all. Disassembly
  confirmed the exact instruction (`btr eax, 0x1c` / `mov [rsi+0xc], eax` at image-relative
  `+0x158af`/`+0x158b3`, x64 build only, sha256 `bb65372a…`) and the patch worked mechanically:
  `CBaseDevice+444`'s bit flipped from `0xe4628800` to `0xf4628800` live, every run, and `d3d9.dll`
  demonstrably took a different internal code path afterward. **At spike time, the unmodified
  `d3d9_managed_texture_test.cpp` did not yet pass with the gate forced open** — it failed earlier and
  differently: `CreateTexture(D3DPOOL_MANAGED)` still succeeded, but `Texture->LockRect()` returned
  `hr=0x00000000` with `pBits=nullptr` (previously it returned a real, non-null pointer into `d3d9.dll`'s
  own private sysmem shadow — the old bug), because forcing the gate hands the lock to the
  driver-managed path, and the scratch Python harness didn't change anything on the UMD side to serve
  one. **This turned out to need no new UMD code after all**: the same-day follow-up (commits
  `36e2a8bb`/`c42fabd4`, see the entry above) productionized the bit-forcing into a permanent
  emulator-side hook and re-ran the unmodified test — it passed outright. The existing `umd_Lock`/
  `g_locked_buffers` machinery (built for ordinary resources, never changed for this fix) turned out to
  already be sufficient; why the productionized hook succeeds where the scratch spike's `pBits=nullptr`
  result suggested a missing UMD-side allocation was not separately re-investigated — the working,
  reproducible end state (confirmed by this session's own A/B toggle) is what matters here, not a full
  explanation of the spike's differing intermediate symptom. The last remaining piece of this spike's
  follow-up list — the 32-bit RE pass, since `+0x158b3` was verified only against the staged x64
  `system32/d3d9.dll` (sha256 `bb65372a53445b5607cbd705a29b4671ab1fb250bef32b3fd0377704088c366c`), and
  real MW2 is 32-bit — is now also done: the equivalent site in the 32-bit `syswow64/d3d9.dll` (sha256
  `99840c2a6b9b75011dfbb3456644e90fa7c2728b10480db1b87f7fd2e8897302`, `and eax, 0xEFFFFFFF` /
  `mov [ebx+0xc], eax` at RVA `0x51c91`) was RE'd and wired into a parallel I386 branch of the hook, and
  the ported `d3d9_managed_texture_test-x86.exe` passes on real WoW64 (see the entry above). Full spike
  detail: `HANDOFF_MACBOOK.md` §27, §28, and the newest dated entry.
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
- [x] **Multi-stream vertex sources** — done. Real per-stream `SetStreamSource` byte offsets
  (`stream_offsets` state), a genuine NEW `D3DVERTEXELEMENT9` declaration parser
  (`parse_vertex_decl`, did not exist before this work — M2 had no vertex-declaration support at
  all, only hardcoded FVF-shape detection by stride), and multi-stream binding wired all the way
  through `execute_draw`. Proven by `d3d9_multistream_test.cpp`: POSITION on stream 0, COLOR on
  stream 1 at a deliberately NONZERO `SetStreamSource` byte offset (stream 1's buffer starts with
  20 bytes of a wrong pad color before the real per-vertex data) — the left/right half-viewport
  colors are only reachable if stream 1 is genuinely bound (not silently collapsed onto stream 0)
  and its offset is honored (not treated as 0). **Significant RE finding**: vkd3d-shader assigns
  SPIR-V input `Location` decorations to a compiled vertex shader's inputs by **declaration order**
  (each input's `v#` register index, itself decided by where its HLSL input struct/D3DBC `dcl`
  instructions place that semantic) — NOT by D3D9 usage semantics (`D3DDECLUSAGE`/`UsageIndex`).
  Empirically confirmed, not assumed: three hand-written HLSL structs reordering the same three
  semantics (`POSITION`/`TEXCOORD0`/`COLOR0`) all produced `Location 0/1/2` following struct order,
  with `POSITION` getting no special-casing (landing at `Location 1` in one ordering) — see
  `d3d9_host.cpp`'s comment immediately above `parse_vertex_decl` for the full experiment and
  `HANDOFF_MACBOOK.md` for the write-up. Since `parse_vertex_decl` has no visibility into its paired
  vertex shader's own HLSL input-struct order (that pairing is a draw-time concern, not this parser's),
  it produces each element's `Location` as its own ordinal position within the `D3DVERTEXELEMENT9`
  array, under the currently-true-for-every-shader-in-this-repo assumption that a vertex declaration's
  element order matches its paired shader's input-struct order. **This is a documented, currently-safe
  but not cross-checked assumption, not a general fix** — a future shader/declaration pair that
  violates it would silently mis-bind vertex attributes with no error; the fully general fix (cross-
  referencing the bound VS's own scanned input signature) is flagged as future work in `d3d9_host.cpp`'s
  own comment, not done here.
  Building the test also found and fixed **3 real, previously-unknown bugs in the guest UMD**
  (`sogen_d3d9_umd.cpp`), not the host — nothing before this test ever exercised
  `CreateVertexDeclaration`/`SetVertexDeclaration` from a guest, so none had ever been reachable:
  (1) `pfnCreateVertexShaderDecl` (device-func slot 45) was still an unwired `device_stub`, so
  `CreateVertexDeclaration()` never reached the host at all; (2)
  `D3DDDIARG_CREATEVERTEXSHADERDECL`'s field order was guessed backwards (`ShaderHandle` first
  instead of `NumVertexElements`); (3) `pfnSetVertexShaderDecl` (slot 47) was wired as a
  struct-pointer call when it is actually a direct-value `HANDLE` call, so every real
  `SetVertexDeclaration()` silently forwarded `decl=0` to the host. All three had to be fixed
  together before anything but a black/unrendered result came back. **Explicitly NOT touched by
  this work**: `stream_frequencies` (instancing) and `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` —
  see the still-open bullet below. Ported to i686/WoW64 with zero source changes and zero new x86
  bugs found — pixel-exact parity with x64 on the first run (see the WoW64 row and
  `src/samples/sogen-d3d9-umd/README.md`).
- [ ] `stream_frequencies` / instancing (`SetStreamSourceFreq`) — out of scope for the multi-stream
  work above; per-stream byte offsets and vertex-declaration parsing were the target, not instanced
  draws.
- [ ] `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` (user-pointer draws, no vertex buffer object).
- [ ] `StretchRect`/`ColorFill`.
- [x] **Scissor rect** — done. `D3DRS_SCISSORTESTENABLE` now gates whether `execute_draw`'s
  Vulkan scissor uses the app's real `SetScissorRect` RECT (converted to a Vulkan
  offset/extent) or falls back to the full render-target extent; previously the draw path
  unconditionally forced a full-RT scissor regardless of app state. Proven by
  `d3d9_scissor_test.cpp`: a center-rect draw checks in/out-of-rect pixels with scissor enabled,
  then repeats with it disabled to confirm the full-RT default still works (regression safety).
  Ported to i686/WoW64 with zero source changes and zero new x86 bugs found — pixel-exact parity
  with x64 on the first run.
- [x] **Multiple render targets (MRT)** — done. Real N-render-target rendering (pipeline builders
  and `execute_draw` fan-out across all bound RTs, gated by `D3DCAPS9::NumSimultaneousRTs`, not
  shader model) plus an all-bound-RTs `Clear()` fix (previously `Clear()` only touched RT slot 0,
  leaving other bound RTs stale). **A real bug was found and fixed mid-implementation**: the bound-RT
  slots are stored in a fixed-size array indexed by RT slot number, not compacted/appended — an
  initial naive design that compacted bound RTs into a dense list would have broken the
  `oC0`-`oC3` shader-output-to-slot correspondence for any non-contiguous binding (e.g. RT0 unbound,
  RT1 bound), silently misrouting a pixel shader's `oC1` write to the wrong physical attachment. The
  fixed-slot, index-preserving design avoids this by construction. Proven by `d3d9_mrt_test.cpp`:
  two RTs bound once at startup and never rebound, a shader writing distinct `oC0`/`oC1` colors
  confirms both RTs receive their own color (not just RT0), and `Clear()` with both RTs still bound
  confirms both go yellow (not just RT0). Ported to i686/WoW64 with zero source changes and zero new
  x86 bugs found — pixel-exact parity with x64 on the first run.
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
  output-handle offset). **Since extended again (2026-07-05):** the scissor/MRT/multi-stream guest
  tests have also been ported to i686 and pass with the same pixel-exact results as x64 — this round
  found zero new x86-only bugs (a contrast worth noting given how many earlier ports did). **Still
  open:** partial-buffer Lock support is x64-only (see "Known bugs &
  limitations" above), and 5 low-confidence DDI slot arities (`pfnCheckCounter`, `pfnSetMarker`,
  `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) remain unverified since none of the current x86
  tests hit them.
- [x] Pipeline-key system beyond one shader pair at a time — **fixed (2026-07-05, commits `3809d1c8`/
  `5dd05caa`, regression test `deebf036`/`e0205851`).** Flagged during the MRT work (2026-07-05) as an
  open gap: `ensure_programmable_pipeline`'s cache (`programmable_pipelines_`) was keyed ONLY by
  `(vertex_shader_id << 32 | pixel_shader_id)`, and `ensure_pipeline` (the fixed-function sibling) had
  no per-shape cache at all — a single one-shot `pipeline_ready_` bool reused for the device's entire
  lifetime. Neither key covered the RT color-format list/count or depth format the pipeline is also
  built from (`VkPipelineRenderingCreateInfo`), nor the vertex-input shape (real vertex declaration or
  the stream-0 stride fallback). A guest reusing the same VS/PS pair (or, for FF, any draw at all)
  across a different bound RT count/format, depth format, or vertex layout would get back a stale
  cached `VkPipeline` built for an earlier draw's shape — silently wrong rendering, not a crash. Fixed
  by adding a `pipeline_cache_key` struct (`vertex_shader`, `pixel_shader`, `color_formats[4]`,
  `depth_format`, `vertex_shape` — see `d3d9_host.hpp`) used as the key for both
  `programmable_pipelines_` and a new `ff_pipelines_` map; `vertex_shape_key()` mirrors the exact
  real-decl-vs-fallback-stride branch the pipeline builder itself uses, so the computed key can never
  disagree with what actually gets built on a cache miss. Proven with a new discriminator guest test,
  `d3d9_pipeline_cache_test.cpp`: draws the SAME `vs_2_0`/`ps_2_0` pair first with 1 RT bound, then
  rebinds to 2 RTs and draws again — before the fix this would reuse the stale 1-attachment pipeline
  against a 2-attachment rendering scope; after the fix RT0 stays correctly RED in both sub-passes and
  RT1 correctly retains its own BLUE clear color rather than being corrupted by the stale pipeline.
  Ported to x86/WoW64 with zero source changes, pixel-exact parity with x64 (`d3d9-pipeline-cache-test-
  x86.exe`, `ALL CHECKS PASSED`, exit 0). Full regression (all host gtests, all 19 guest
  `d3d9-*-test.exe` x64/x86, and the 26/26 smoke test) passes identically to before this change.
- [x] Pipeline-key system — static blend/depth render-state — **fixed (2026-07-06, gate test
  `157831bf`, fix `5256f980`, polish `76c06d53`).** One of two narrower gaps found while fixing the
  RT/vertex-shape cache key above, deliberately deferred at the time: `pipeline_cache_key` covered
  `vertex_shader`/`pixel_shader`/`color_formats[4]`/`depth_format`/`vertex_shape` but no `D3DRS_*`
  render state, even though `build_depth_state`/`build_blend_state` bake
  `D3DRS_ZENABLE`/`D3DRS_ZWRITEENABLE`/depth-compare-func and
  `D3DRS_ALPHABLENDENABLE`/blend-factor state as STATIC pipeline state into every `VkPipeline`. A guest
  drawing with the same VS/PS/RT-shape/vertex-shape but different blend or depth render state between
  draws would collapse to the same cache key and silently reuse the first draw's stale pipeline — e.g.
  a second, blend-enabled draw reading back opaque instead of blended output. Proven real first with a
  deliberately-failing gate test, `d3d9_pipeline_cache_rs_test.cpp` (`d3d9-pipeline-cache-rs-test.exe`):
  sub-pass 1 (blend disabled) passes as expected, but sub-pass 2 (blend enabled, same VS/PS) still read
  back the unblended `G=FF` instead of the analytically-correct blended `G=80` — confirmed failing
  against the unmodified host. Fixed by adding `depth`/`blend` fields (the resolved
  `vulkan_host::depth_state`/`color_blend_attachment`, each given a defaulted `operator<=>`) to
  `pipeline_cache_key`, and computing the resolved depth/blend state ONCE at each cache-key site
  (`ensure_pipeline`, `ensure_programmable_pipeline`) so the values that key the cache are always the
  same ones fed to `create_graphics_pipeline` on a miss — the key can never disagree with what gets
  built. Gate test now passes; full x64+x86 D3D9 guest suite and the smoke test stay green.
- [x] Pipeline-key system — real-vertex-decl stream strides — **fixed (2026-07-06, gate test
  `6f723bc1`, fix `02d33bba`).** The second of the two narrower gaps found during the RT/vertex-shape
  cache-key fix, not addressed by the render-state fix above. `vertex_shape_key()`'s
  real-vertex-declaration branch fingerprinted the pipeline's vertex-input shape with ONLY the
  immutable `D3DVERTEXELEMENT9` declaration handle, not the mutable per-stream strides
  (`state_.stream_strides`) that `ensure_programmable_pipeline` ALSO reads to bake each
  `VkVertexInputBindingDescription::stride` at build time. `SetStreamSource` can change a stream's
  stride without touching the declaration handle, so two draws with the same declaration/VS/PS but
  different bound strides collapsed to one cache key and reused a stale pipeline built for the first
  stride, misfetching every vertex past index 0. Proven real first with a deliberately-failing gate
  test, `d3d9_pipeline_cache_stride_test.cpp` (`d3d9-pipeline-cache-stride-test.exe`): sub-pass 1 binds a
  tightly-packed 12-byte-stride buffer and draws a left-half quad (builds/caches the pipeline; the
  checkpoint correctly reads RED), sub-pass 2 rebinds the same stream to a 32-byte-stride buffer and
  draws a right-half quad with the same declaration/VS/PS — the checkpoint read back BLACK (the
  untouched background) instead of RED, confirmed failing against the unmodified host. Fixed by widening
  `pipeline_cache_key::vertex_shape` from a bare `uint64_t` handle to a `vertex_input_shape` struct
  (`id` plus a fixed-size `std::array<uint32_t, max_vertex_streams>` of the per-stream strides the build
  actually reads), snapshotting the current stride of every stream the bound declaration's
  `used_binding_mask` references, compared via the struct's own defaulted `operator<=>` — the same
  collision-free fixed-array approach as `color_formats`, deliberately avoiding new hashing machinery.
  The no-real-declaration fallback needed no stride folding: it hardcodes its binding stride to 16 or 20
  and only ever reads stream 0, so its existing 1/2 tag already captures its full stride-dependence. Gate
  test now passes (BLACK/`R=00` before the fix, RED/`R=FF` after); full x64+x86 D3D9 guest-test suite,
  including `d3d9-multistream-test` on both arches, passes unchanged. This closes the pipeline-cache-key
  system's last known gap — see below.

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
got a real fix for the double-resource-creation sync (via `pfnTexBlt`) and, since 2026-07-05, a real fix
for its deeper remaining symptom too — a permanent runtime memory patch to `d3d9.dll` (see above), not
more DDI coverage — on both x64 and x86/WoW64. The 32-bit `syswow64/d3d9.dll` real MW2 would use has now
had the equivalent RE pass, and its I386 hook branch is wired and proven by the ported
`d3d9_managed_texture_test-x86.exe`, so this is a real fix for a real 32-bit game too — no longer a
standing follow-up gap.

Per the original plan's "de-risk earliest/riskiest first" philosophy, int/bool constant registers were
taken ahead of the rest of M3 (real games hit shader flow control before most of M3's other items —
multi-stream, `*_UP` draws, MRT, cube/volume) and are now done, proven pixel-exact on both x64 and x86.

Three more M3 items are now also done, all proven pixel-exact on x64 and x86: scissor rect
(`D3DRS_SCISSORTESTENABLE`-gated real clipping), multiple render targets (real N-RT rendering + the
all-bound-RTs `Clear()` fix, with a real fixed-slot/index-preserving design bug found and fixed along
the way), and multi-stream vertex sources (a new `D3DVERTEXELEMENT9` parser, the vkd3d-shader
declaration-order location-assignment finding, and 3 real UMD bugs found/fixed enabling
`CreateVertexDeclaration`/`SetVertexDeclaration` to work at all — see "M3 coverage items" above for the
full account of each). This work also surfaced a gap — `ensure_programmable_pipeline`'s cache key
didn't cover RT format/count or vertex-decl shape, so a guest that varied either mid-session with a
repeated VS/PS pair could get a stale pipeline — which has since been fixed (2026-07-05, commits
`3809d1c8`/`5dd05caa`, proven by `d3d9_pipeline_cache_test.cpp` on both x64 and x86). Of the two
narrower gaps found during that fix, the static blend/depth render-state gap was fixed next
(2026-07-06, gate test `157831bf`, fix `5256f980`, polish `76c06d53`), and the real-vertex-decl branch's
ignoring of mutable per-stream strides is now also fixed (2026-07-06, gate test `6f723bc1`, fix
`02d33bba`) — the pipeline-cache-key system has no known open gaps remaining. See the "Pipeline-key
system" bullets above.

M3's remaining DDI-coverage items (`*_UP` draws, `StretchRect`/`ColorFill`, `stream_frequencies`/
instancing, cube/volume textures, more formats, SM3.0 caps, mip-mapping) can proceed in roughly the
order listed above. `D3DPOOL_MANAGED` is now fixed on both x64 and x86/WoW64 (see above) — the 32-bit RE
pass that was previously the standing MW2-integration risk has been done (the caps-strip site in
`syswow64/d3d9.dll` was disassembled, its 8-byte pattern confirmed unique, and a second hook branch
gated on the x86 machine type wired up and proven by the ported `d3d9_managed_texture_test-x86.exe`), so
this is no longer a budgeted risk for MW2 integration. If x86 partial-buffer Lock support becomes necessary before MW2 integration, budget for
the same kind of live-RE pass (`D3DDDIARG_LOCK`'s x86 driver-routed `OffsetToLock` offset) that resolved
the x64 case.
