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

### Fixed (genuinely fixed with real code, 2026-07-06): per-draw busy-spin and allocation churn

The two items previously listed here as still-open (busy-spin fence-wait; per-draw buffer/UBO/
descriptor-set allocation churn) are now **CLOSED**. A third item this same section originally left open
below — sampler pooling — has since also closed, same day. Seven commits: `02b28ada`, `0238dfd7`,
`fcfccc00`, `36b03142`, `4b0bc778`, `67a94a48`, `ebf0e622`.

1. **Busy-spin fence-wait → genuine blocking wait** (`02b28ada`). `execute_draw` (and four other
   `d3d9_host.cpp` call sites: depth-stencil-view, texture-upload, staging-upload) had tight,
   empty-bodied `vkGetFenceStatus` polling loops that pinned a CPU core at 100% for the entire GPU wait.
   `vulkan_host` already resolved `vkWaitForFences` internally but never exposed it; a new public
   `wait_for_fence(fence, timeout_ns)` wrapper is now called with `UINT64_MAX` from every site instead
   of spinning.
2. **Descriptor-set pooling** (`0238dfd7`, comment fixup `fcfccc00`). `execute_draw` reset a shared
   descriptor pool and freshly allocated 2 descriptor sets (VS+PS) on every draw. One small pool plus
   its 2 sets are now cached on each `programmable_pipeline_entry`, allocated once on cache miss; draws
   reuse the cached sets and only rewrite their contents. The now-unused shared `descriptor_pool_` was
   removed.
3. **Vertex/index/constant-UBO pooling** (`36b03142`, comment/tradeoff fixup `4b0bc778`). `execute_draw`
   created and destroyed every vertex-stream buffer, the index buffer, and all six VS/PS constant UBOs
   per draw. These are now per-device-lifetime pools (`ensure_pooled_buffer`/`upload_pooled_ubo`,
   `pooled_buffer`), created once and regrown only when a draw needs more capacity. Contents are still
   re-uploaded every draw, so output is unchanged.
4. **Sampler pooling** (`67a94a48`, polish `ebf0e622`). `build_sampler` created a fresh `VkSampler` every
   draw and destroyed it after (`execute_draw`'s per-draw `destroy_tex_samplers` cleanup). Unlike the
   VB/IB/UBO pools above (one object per slot, contents rewritten every draw), a `VkSampler` is immutable
   once created, so differing filter/address-mode/anisotropy/LOD state genuinely needs a distinct
   object — this is a content-addressed cache, not a positional pool. A new `sampler_cache_key` struct
   (mag/min/mip filter, address U/V/W, anisotropy enable/max, min/max LOD — every dimension
   `build_sampler` actually varies the `VkSampler` on) with a defaulted `operator<=>`, plus a
   `std::map<sampler_cache_key, uint64_t> sampler_cache_` member mirroring `programmable_pipelines_`/
   `ff_pipelines_`, replace the old create-then-destroy cycle: `build_sampler` resolves the key, returns
   the cached sampler on a hit, and only calls `create_sampler` on a miss, caching the result. Cached
   samplers persist for the device's lifetime; the per-draw destroy is gone.

All four are safe because each draw still submits and blocks on a fence before returning, so a prior
draw's GPU read of a pooled/cached object has completed before a later draw rewrites it. The sampler
cache has an even simpler safety argument than the other three: nothing ever mutates a `VkSampler` after
creation, so reuse across draws and frames has no synchronization hazard at all — there's no "prior read
must complete" question to answer in the first place. Net effect: the confirmed per-draw allocation
churn (a dozen-plus `vkAllocateMemory`/`vkFreeMemory`/buffer create+destroy pairs per draw, plus one
`vkCreateSampler`/`vkDestroySampler` pair per draw) drops to essentially zero after the first draw of a
given shader/stream/UBO/sampler-state shape, and the CPU-pinning busy-spin is gone.

**Evidence**: a new guest test, `src/samples/sogen-d3d9-umd/d3d9_manydraws_test.cpp`, issues 768
`DrawIndexedPrimitive` calls through the SAME cached pipeline in one `BeginScene`/`EndScene`, each draw
filling a distinct grid cell with a distinct, index-derived color driven by a real per-draw VS constant
(cell position) and PS constant (cell color) — a correctness discriminator that would show wrong/stale
colors if the pooling reused contents incorrectly. Eight spread cells read back byte-exact on both x64
and x86/WoW64. Bracketed by `QueryPerformanceCounter`: the same 768-draw loop measured ~383 ms
(~0.50 ms/draw) against a temporarily-reverted pre-fix host (`b6809cee` = `02b28ada~1`) and ~279 ms
(~0.36 ms/draw) against fixed HEAD — a real, repeatable ~27% reduction in per-frame draw-loop time
(emulated guest wall-clock), with identical all-PASS pixel output in both. The sampler cache specifically
is proven by the pre-existing `d3d9_miptexture_test.cpp`, which exercises 4 sub-passes that each pin a
different `D3DSAMP_MAXMIPLEVEL`/`MIPFILTER` combination within one test run — 4 distinct cache keys, each
correctly resolving to its own newly-created sampler (a stale/misrouted cache-miss path would read back
the wrong mip level's color, exactly the discriminator this test was built for); every other sweep test
that samples the same texture across more than one draw (`texture`, `multitexture`, etc.) exercises the
cache-hit path the same way, still pixel-identical after the cache landed.

### Known remaining limitation: this fix does NOT complete the performance story either

This slice reduced the per-round-trip COST but is still not the whole story. Explicitly **not** touched,
and still real, separate remaining work:
- **Multi-frame-in-flight pipelining** — every draw is still FULLY SYNCHRONOUS (submit, then block until
  the GPU completes, before the next draw starts). The NUMBER of GPU round-trips per frame is unchanged;
  only each round-trip's cost improved. Having multiple frames' GPU work in flight simultaneously is a
  separate, larger slice.

Do not read this fix as having made the D3D9 native path fully pipelined — it didn't.

### Risk analysis + measurement spike (2026-07-06): multi-frame-in-flight deliberately NOT attempted next; safer alternative identified instead

With busy-spin, descriptor-set, VB/IB/UBO, and sampler-cache pooling all closed (see the per-draw-overhead
and sampler-pooling entries above), the remaining item on this list — multi-frame-in-flight pipelining —
was evaluated as the next logical step and deliberately **not** started this session, after a risk
analysis and a measurement spike. Neither changes the status of that item: **it remains open**, exactly
as listed above.

**Risk analysis.** Full multi-frame-in-flight pipelining requires every one of this session's
newly-pooled, single-slot-reused resources (VB/IB/UBO pools, descriptor sets) to become N-buffered — one
copy per frame-in-flight — instead of today's one-copy-reused-every-draw model. A bug in that redesign
would manifest as a **silent, timing-dependent, non-deterministic GPU-side data race**: a later frame's
draw reading a pooled resource while an earlier, not-yet-synchronized frame's draw is concurrently
rewriting it. This is a fundamentally different failure class from every bug fixed this session (busy-spin,
allocation churn, sampler lifetime) — all of those failed loudly and reproducibly (wrong pixels, a crash, a
hang) and were caught by this codebase's deterministic pixel-readback test methodology, which runs one
fixed instruction sequence with no real scheduling jitter. A cross-frame race is not reliably caught by
that methodology: it could pass every existing test, every time, in this deterministic environment, and
still be a live bug once real, variable frame-pacing timing is involved. **Recommendation: do not attempt
full multi-frame-in-flight now.**

**Measurement spike (temporary instrumentation, reverted after gathering data — confirmed clean via `git
status`, no commits).** Instead of guessing where the remaining per-draw time goes,
`d3d9_manydraws_test.cpp` (768 draws in one frame) was run 3 times with timing instrumentation around
`execute_draw`'s own submit+wait and around the guest's own outer draw loop. Findings, consistent across
all 3 runs:
- `execute_draw`'s own submit+wait (`queue_submit` + `wait_for_fence`, i.e. the actual GPU round-trip)
  accounts for **95.8%-97.6%** of `execute_draw`'s own total time — CPU-side work (descriptor writes,
  buffer/UBO uploads, pipeline lookup) is only **2.4%-4.2%**, confirming this session's pooling fixes
  successfully minimized CPU-side per-draw cost.
- `execute_draw`'s own total time (182-190 ms across the 3 runs) accounts for only **~63-65%** of the full
  guest-observed 768-draw loop wall-clock (287-294 ms) — the remaining ~35% is overhead entirely OUTSIDE
  `execute_draw`: guest-side instruction emulation for the per-draw `SetVertexShaderConstantF`/
  `SetPixelShaderConstantF` calls, DDI/wire-protocol dispatch, and the guest's own
  `QueryPerformanceCounter` calls. No GPU-side change (batching or pipelining) can touch this portion at
  all.
- `submit_count == draw_count == 768` exactly in every run, confirming the current model really is one
  full submit+wait round-trip per individual draw, with no batching at all.

**Conclusion.** GPU round-trip time is still clearly the dominant cost within the part any GPU-side fix
could address (~96%+ of `execute_draw`'s own time, ~62% of total loop time) — real evidence FOR the value
of a follow-up, IF this work is ever prioritized again, but NOT evidence for going straight to full
multi-frame-in-flight. The identified safer alternative: **batch multiple draws into ONE submission per
frame, remaining fully synchronous** (submit once, then block until that one submission's fence signals,
before the next batch starts) — no cross-draw or cross-frame concurrency at all, so none of the
silent-data-race risk profile above applies. This is still real, non-trivial, **not yet attempted** work
of its own: it requires converting today's single-slot pooled VB/IB/UBO/descriptor-set resources into
per-draw sub-allocated ranges within a per-frame arena, so N draws batched into one submission each get
their own slice of a shared buffer instead of colliding on the one pooled slot — a smaller, fail-loud
correctness surface than multi-frame-in-flight's (a sizing/offset bug would misrender immediately and
deterministically, not race), but genuine implementation work, deliberately deferred, not started.

Multi-frame-in-flight pipelining remains the one open item on this list; nothing in this entry closes it
or reduces its scope — this entry only records why it wasn't attempted next and what was found instead.

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
| M3 | DDI coverage (multi-stream, `*_UP` draws, StretchRect/ColorFill, scissor, MRT, cube/vol, more formats) | **In progress** | Int/bool constant registers, originally scoped as part of this milestone, are done (see "Constant registers" below) — pulled forward for the same reason float constants were (real games hit shader flow control early). **Four more M3 items are now done**: multi-stream vertex sources (real per-stream `SetStreamSource` offsets + a new `D3DVERTEXELEMENT9` declaration parser), scissor rect (`D3DRS_SCISSORTESTENABLE`-gated real clipping), multiple render targets (real N-RT rendering + all-bound-RTs `Clear()`), and `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` (real d3d9.dll reuses the ordinary `pfnDrawPrimitive`/`pfnDrawIndexedPrimitive` slots via two new UM-binding DDI calls, `pfnSetStreamSourceUm`/`pfnSetIndicesUm` — no dedicated UP-draw DDI call exists at all) — see the "M3 coverage items" checklist below for the full account of each, including the 3 UMD bugs the multi-stream work found/fixed, the RT-slot-compaction bug found/fixed during MRT work, and the pre-existing `pfnDrawPrimitive` arity bug the UP-draw work incidentally uncovered. Proven pixel-exact on both x64 and x86/WoW64 — the multi-stream/scissor/MRT trio ported to i686 with zero source changes and zero new x86 bugs; the new `DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` test was written for x64 and x86 from the start and, on x86 specifically, is what surfaced the `pfnDrawPrimitive` arity bug fixed alongside it (a pre-existing bug, not introduced by this work — see below). **A fifth M3 item is now done: `StretchRect`/`ColorFill`** (2026-07-06, commits `b915658a`/`3dd369f9`/`f7b9696e`/`83c518c6`) — live RE confirmed real `d3d9.dll` routes these through device-func-table slots 56/55 (`pfnColorFill`/`pfnBlt`), with a live-confirmed correction to `D3DDDIARG_BLT`'s field order (SRC-first-then-DST, which static decompilation alone would have gotten backwards), plus a caps-bit fix (`D3DCAPS9::StretchRectFilterCaps`, previously left unset) that unlocks genuine *scaled* StretchRect — see the "M3 coverage items" checklist below for the full account, including the two known, deliberate scope limitations this slice documented rather than silently left. **A sixth M3 item is now done: mip-mapping** (2026-07-06, commits `256ea51e`/`080bbbfe`/`8ffb306c`/`625ae525`/`d2d29cd2`) — a gated RE pass confirmed `D3DDDIARG_LOCK::SubResourceIndex` (offset 8 x64 / 4 x86, previously modeled as `Reserved0`), which unblocks real per-mip-level texture data upload via `LockRect(level, ...)`, a real Vulkan mip-chain image/view per texture, and a real sampler LOD range engaging genuine GPU minification — proven pixel-exact on both x64 and x86/WoW64 by a 4-sub-pass discriminator test; this also substantially de-risks cube/volume textures (below), which need the exact same `SubResourceIndex` mechanism for per-face/per-slice uploads. **A seventh M3 item is now done: SM3.0 caps** (2026-07-06, commits `1b940580`/`d82434ff`/`c468e80d`) — real `d3d9.dll`'s `IsD3DHALSupported` SM3.0 validation branch turned out to read its required caps fields directly out of the same UMD-filled `GetCaps` buffer, with no opaque cache or transform in the way (unlike cube/volume's gate below), making the confirming RE pass the same tractable shape as the original SM2.0 gauntlet; proven end to end by a real `vs_3_0`/`ps_3_0` pair whose pixel shader uses a genuine SM3.0-only construct (a runtime-count loop that fails to compile at `ps_2_0` but succeeds and renders correctly at `ps_3_0`), pixel-exact on both x64 and x86/WoW64 with zero regression across all 40 pre-existing SM2.0 guest tests — see the "M3 coverage items" checklist below for the full 12-field account and an honest note on residual, untested SM3.0 surface. **An eighth M3 item is now done: `stream_frequencies`/instancing (`SetStreamSourceFreq`)** (2026-07-06, commits `d3a0318c`/`a6062d66`/`1d4d0ab9`) — the DDI transport for `SetStreamSourceFreq` (UMD, wire protocol, host-side `state_.stream_frequencies` storage) was already fully wired by the multi-stream work above, but the Vulkan draw path never consumed it: every draw used a hardcoded `instance_count=1` and `VK_VERTEX_INPUT_RATE_VERTEX` for every stream regardless of what the app requested. Fixed via a single shared helper, `resolve_instancing()`, decoding the stored frequencies into an effective instance count and a per-instance binding mask, consulted identically by the pipeline-cache-key builder, the vertex-binding-rate builder, and the draw call so the three can never disagree — proven by a new discriminator test (`d3d9_instancing_test.cpp`) whose pre-fix-host run genuinely fails (all four instanced quadrants read back BLACK instead of four distinct colors), independently reproduced by two reviewers; see the "M3 coverage items" checklist below for the full account, including the explicit non-1-divisor scope limitation. **Still not started**: cube/volume textures (investigated 2026-07-06 and found blocked by a deeper gate than previously believed — real `d3d9.dll` rejects `CreateCubeTexture`/`CreateVolumeTexture` before any driver call, in `CEnum::CheckDeviceFormat`'s internal per-format capability table; see the "M3 coverage items" checklist below for the corrected scope). **A ninth M3 item is now done: D3DFORMAT advertisement** (2026-07-06, commits `ba83be93`/`18b74fcb`/`a9c2f8d3`/`197cfbd3`) — the host's `d3d9_format_to_vulkan` already correctly mapped 13 D3DFORMATs, but the UMD's `g_formats` FORMATOP table only advertised 3-4 of them; an RE gate confirmed advertising a new format is a plain mechanical table extension with no opaque wall behind it (unlike cube/volume above), and the remaining 9 formats were then added. A code-quality review caught a real bug in that same expansion — `R5G6B5` was incorrectly given render-target capability despite the host's RT readback/Present/ColorFill paths all hardcoding a 4-bytes-per-texel assumption `R5G6B5` (2 bytes/texel) would violate — fixed before it shipped; see the "M3 coverage items" checklist below for the full account, including the "Known limitation" bullet on the underlying 4-bytes-per-texel host constraint. Genuinely new formats beyond that locked 13-format set (e.g. paletted) remain unadvertised and are not believed to block MW2. **A tenth M3 item is now done: vertex texture fetch** (2026-07-06, commits `fd1fcb46`/`e3aa2adf`/`8a41b682`) — a gated DDI trace confirmed real `d3d9.dll` passes `SetTexture(D3DVERTEXTEXTURESAMPLER0..3, tex)` through to the DDI as an identity pass-through, and the existing PS-only multi-sampler binding scheme was generalized into one shared source of truth used by both shader stages, proven pixel-exact on both x64 and x86/WoW64 by an "unfakeable by a pixel shader" vertex-displacement discriminator test — see the "M3 coverage items" checklist below for the full account, including the separate, still-open `CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE)` FORMATOP gap this work deliberately left open. The remaining M3 work inherits (on x86 only) the partial-Lock gap; the pipeline-cache-key system itself has no known open gaps left (RT/vertex-decl-shape, static blend/depth render-state, and real-vertex-decl stream strides are all now fixed — see below) — none of this is M3-net-new work, it's M2 debt M3 must not silently re-break. |
| M4 | Fixed-function synthesis | **Deferred by design** | Original plan: "only if MW2 needs it." MW2 is SM3-heavy; likely skippable. Revisit if a real game draw path turns out to need FF after all. |
| M5 | MW2 integration (WoW64, SM3, 32-bit UMD) | **Not started, blocked on M3** | The WoW64/x86 UMD port itself is done (see the WoW64 row above), and its shader/texture/constant-register/texcoord features are now confirmed on the x86 path specifically (not just the fixed-function triangle) — the remaining blocker is real MW2 integration work: M3's remaining DDI coverage (cube/volume textures — see the M3 row above; D3DFORMAT advertisement for the locked 13-format set is now done, 2026-07-06, closing that specific gap, including a real R5G6B5 render-target-capability bug caught and fixed by code-quality review before shipping — see the M3 row above and "M3 coverage items" below). SM3.0 caps acceptance itself is now done (2026-07-06, see the M3 row above and "M3 coverage items" below) — real `d3d9.dll` accepts `vs_3_0`/`ps_3_0` and a genuine SM3.0-only shader construct renders correctly end to end on both x64 and x86/WoW64; the one residual, smaller uncertainty is that this doesn't guarantee every specific SM3.0 instruction a real, complex MW2 shader uses has been exercised. **Vertex texture fetch — one of the specific residual constructs this uncertainty called out — is now also done** (2026-07-06, commits `fd1fcb46`/`e3aa2adf`/`8a41b682`): a real `vs_3_0` shader samples a texture via `tex2Dlod` bound to `D3DVERTEXTEXTURESAMPLER0..3`, proven pixel-exact on both x64 and x86/WoW64 with an unfakeable-by-a-pixel-shader discriminator (see the M3 row and "M3 coverage items" below). This closed the DDI/rendering path first, and the separate `CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE, ...)` FORMATOP capability-advertisement gap (commit `fa1c9833`) is now also closed for `A16B16G16R16F`, the one format this session's VTF test proves works — a well-behaved real app that gates on `CheckDeviceFormat` before using vertex textures is no longer refused for this format. Other floating-point vertex-texture formats (`R32F`/`A32B32G32R32F`) remain unadvertised since they're not yet host-supported by `d3d9_format_to_vulkan` — a narrower, separate gap if a real MW2 shader ever needs one of those specifically. `D3DPOOL_MANAGED` is now fixed on both x64 and x86/WoW64 (see below) via a permanent runtime memory patch, not more DDI coverage — and since the 32-bit `syswow64/d3d9.dll` branch real MW2 (a 32-bit game) actually uses is now RE'd and wired too, MANAGED-pool asset loading is no longer a standing x86-only risk for MW2 integration. |

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
  this work**: `stream_frequencies` (instancing) — see the bullet below (`DrawPrimitiveUP`/
  `DrawIndexedPrimitiveUP` was also out of scope here, but has since been closed — see below). Ported
  to i686/WoW64 with zero source changes and zero new x86 bugs found — pixel-exact parity with x64 on
  the first run (see the WoW64 row and `src/samples/sogen-d3d9-umd/README.md`).
- [x] **`stream_frequencies` / instancing (`SetStreamSourceFreq`) — done (2026-07-06, commits
  `d3a0318c`/`a6062d66`/`1d4d0ab9`).** Out of scope for the multi-stream work above (per-stream byte
  offsets and vertex-declaration parsing were that work's target, not instanced draws) — the DDI
  transport for `SetStreamSourceFreq` (UMD, wire protocol, host-side `state_.stream_frequencies`
  storage) was already fully wired, but the Vulkan draw path never consumed it: every draw used a
  hardcoded `instance_count=1` and `VK_VERTEX_INPUT_RATE_VERTEX` for every stream regardless of what
  the app requested. **Fix (`d3a0318c`)**: a single shared helper, `resolve_instancing()`, decodes
  `state_.stream_frequencies` into the effective instance count (the `D3DSTREAMSOURCE_INDEXEDDATA`
  stream's low 30 bits, default 1) and a per-instance binding mask (`D3DSTREAMSOURCE_INSTANCEDATA`
  streams with divider exactly 1) — consulted identically by the pipeline-cache-key builder
  (`vertex_shape_key`, which folds the mask into `pipeline_cache_key::vertex_input_shape` so a stream
  that flips between per-vertex and per-instance gets a distinct pipeline instead of reusing a stale
  one), the vertex-binding-rate builder (`ensure_programmable_pipeline`, which sets
  `VK_VERTEX_INPUT_RATE_INSTANCE` for masked streams), and the draw call (`execute_draw`, which passes
  the resolved instance count to `cmd_draw`/`cmd_draw_indexed`) — the same builder/consumer-agreement
  pattern this session's other pipeline-cache-key fixes already rely on (see "Pipeline-key system"
  below). **Known, documented limitation**: only an `INSTANCEDATA` divider of exactly 1 is honored — a
  non-1 divider needs `VK_EXT_vertex_attribute_divisor`, not enabled on this Vulkan device, and is left
  per-vertex rather than silently rendering wrong per-instance data. **Test (`a6062d66`,
  `d3d9_instancing_test.cpp`)**: one `DrawIndexedPrimitive` draws 4 instances of a quad into 4 screen
  quadrants, each with a distinct per-instance color via a real `D3DVERTEXELEMENT9` declaration
  (POSITION on stream 0 `INDEXEDDATA|4`, per-instance offset+color on stream 1 `INSTANCEDATA|1`). Run
  against the pre-fix host behavior (`instance_count` forced to 1 and `inputRate` forced to `VERTEX`),
  all four quadrant centers read back BLACK and the test FAILED — a real, observed before/after
  discrimination, independently reproduced by two reviewers. Passes pixel-byte-identical on both x64
  and x86/WoW64. `1d4d0ab9` is a follow-up polish commit documenting two further accepted edge cases in
  `resolve_instancing()` (multiple `INDEXEDDATA` streams: last-wins via `unordered_map` iteration order,
  harmless since that usage is itself invalid D3D9; and `instance_count>1` on a non-indexed draw: only
  reachable under invalid D3D9 usage, since real instancing requires an indexed draw). Zero regression
  across the full existing D3D9 guest-test sweep — the new cache-key field and resolved instance count
  both default to their pre-change values (0 / 1) for every non-instanced draw.
- [x] **`DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` (user-pointer draws, no vertex buffer object) — done
  (2026-07-06, commits `f36af2b7`/`1c2bd176`/`91f1ded5`/`bc86b91b`).** The gap as originally scoped
  assumed a dedicated "UP draw" DDI call carrying the inline vertex/index bytes — the previously-existing
  wire-protocol scaffolding (`draw_primitive_up_record`/`draw_indexed_primitive_up_record`) was modeled on
  that assumption and was never wired to a real host handler. Live RE of real `d3d9.dll` (x64 and x86,
  `f36af2b7`) found this assumption wrong: there is no dedicated UP-draw DDI call at all. Real `d3d9.dll`
  instead binds the user vertex array via `pfnSetStreamSourceUm` (device-func-table slot 7, a
  struct-based call, `D3DDDIARG_SETSTREAMSOURCEUM = {StreamNumber, Stride}`, identical x64/x86, vertex
  pointer passed as a separate 3rd argument) and, for the indexed variant, the user index array via
  `pfnSetIndicesUm` (slot 9) — then reuses the ALREADY-WIRED `pfnDrawPrimitive`/`pfnDrawIndexedPrimitive`
  slots (10/11) exactly as a normal buffer-backed draw would. **Correction worth flagging explicitly**:
  `pfnSetIndicesUm` was initially assumed to be struct-based like its sibling; live RE showed it is
  actually a plain SCALAR call, `(HANDLE, UINT Stride, CONST VOID* pUMIndices)` — no
  `D3DDDIARG_SETINDICESUM` struct exists. Fixed (`1c2bd176`): the old, incorrectly-modeled
  `draw_primitive_up_record`/`draw_indexed_primitive_up_record` wire records and their inert host stubs
  were retired outright and replaced with `set_stream_source_um_record`/`set_indices_um_record` (new
  opcodes `d3d9_set_stream_source_um`/`d3d9_set_indices_um`); the UMD wires `umd_SetStreamSourceUm`/
  `umd_SetIndicesUm` at slots 7/9, stashing the user pointer/stride until the subsequent (reused)
  `umd_DrawPrimitive`/`umd_DrawIndexedPrimitive` call — which carries the vertex/index counts — copies
  exactly the referenced bytes into the new wire records; `d3d9_host`'s `execute_draw` gained transient
  UM-backed vertex/index state that composes with the existing resource-id path (a real buffer bind
  supersedes a pending UM binding and vice versa), so **no new draw-time DDI handler was needed at all** —
  real `d3d9.dll`'s own reuse of the normal draw slots meant the existing `execute_draw` path just needed
  to learn a second source for its vertex/index bytes. **Incidental bug fix, precisely scoped**: this work
  also fixed a genuinely pre-existing bug, unrelated in origin to the UP-draw feature itself —
  `pfnDrawPrimitive` (device-func slot 10)'s arity table entry was wrong (declared 2 args; the real,
  WDK-standard shape is 3 args, `(HANDLE, CONST D3DDDIARG_DRAWPRIMITIVE*, CONST UINT* pFlags)`, confirmed
  by live IDA disassembly of both x64 and x86 `d3d9.dll`, which push three args on every call site). The
  wrong arity caused an x86-only `__stdcall` stack desync (`STATUS_STACK_BUFFER_OVERRUN`) — invisible on
  x64's caller-cleanup convention, but fatal on x86's callee-cleanup one. This bug was **latent and
  unreachable until this slice's new UP-draw call sequence exercised the affected code path**: it did
  **not** affect `d3d9-triangle-test-x86` or any other pre-existing test — independently verified by
  reverting to the 2-arg declaration and re-running `triangle-test-x86` (still passes) against
  `drawprimitiveup-test-x86` (crashes with the 2-arg declaration, passes with the 3-arg fix). Proven
  end-to-end by a new test, `d3d9_drawprimitiveup_test.cpp` (`91f1ded5`): a RED triangle via
  `DrawPrimitiveUP` and a GREEN indexed quad via `DrawIndexedPrimitiveUP` (both `D3DFMT_INDEX16` and
  `D3DFMT_INDEX32`), all with NO vertex or index buffer object ever created, pixel-exact on both x64 and
  x86. Polish (`bc86b91b`) added cross-reference comments documenting the UM/real-bind
  mutual-exclusivity coupling from both directions. Full regression sweep (all existing D3D9 guest tests,
  x64+x86) stayed clean at every stage.
- [x] **`StretchRect`/`ColorFill` — done (2026-07-06, commits `b915658a`/`3dd369f9`/`f7b9696e`/`83c518c6`).**
  The gap as originally scoped was the last unimplemented pair of surface-transfer DDIs in M3.
  **RE (`b915658a`)**: live RE confirmed real `d3d9.dll` routes `IDirect3DDevice9::ColorFill` and
  `IDirect3DDevice9::StretchRect` through device-func-table slots 56 and 55 (`pfnColorFill`/`pfnBlt`),
  cross-checked both statically (idasql-decompiled `CD3DDDIDX10::Colorfill`/`::Blt`'s own indirect
  device-func-table calls at the byte offsets that resolve to those slot indices, plus the independent
  batch consumers `LHBatchColorFill`/`LHBatchBlt`) and live (a guest probe drove real
  ColorFill/StretchRect with distinctive rects/color and dumped the raw DDI arg bytes). **The live trace
  corrected `D3DDDIARG_BLT`'s field ordering to SRC-first-then-DST** (matching the WDK) — a correction
  the bare decompile alone could not have disambiguated and would have gotten backwards. Additive-only
  RE gate commit: no wire/UMD/host changes, both arches compile.
  **Implementation (`3dd369f9`)**: new wire opcodes `d3d9_color_fill` (`0x939`)/`d3d9_blt` (`0x93A`) with
  `color_fill_record`/`blt_record` payloads (blt is SRC-first, matching the RE finding); UMD handlers
  `umd_ColorFill`/`umd_Blt` registered at slots 56/55; host-side `d3d9_host::color_fill` (a scoped
  buffer->image transfer copy — the RT is B8G8R8A8, so a solid `D3DCOLOR` dword needs no channel
  juggling) and `d3d9_host::blt` (`vkCmdBlitImage`, which scales natively when the src/dst rects differ
  — a reusable primitive, correctly routed through the existing `cmd_pipeline_barrier` layout-tracking
  choke point so `render_targets[image].current_layout` stays authoritative, the same discipline every
  other transfer op in this codebase follows). **`StretchRectFilterCaps` discovery and fix**: real,
  genuinely-scaled StretchRect (differently-sized src/dst rects) is gated behind
  `D3DCAPS9::StretchRectFilterCaps`, which this UMD previously left unset (the `memset` default) —
  `CD3DDDIDX10::StretchRect`'s own validation rejects any scaled stretch with `D3DERR_INVALIDCALL`
  before `pfnBlt` is ever called when this field is 0, while same-size copies always dispatched
  regardless. `fill_d3d9caps` now advertises `MINFPOINT|MAGFPOINT|MINFLINEAR|MAGFLINEAR`, letting a
  genuine stretch reach the driver.
  **Test evidence (`f7b9696e`)**: `d3d9_colorfill_test.cpp` clears a 640x480 RT BLUE, ColorFills a center
  rect RED, and checks four interior points read RED while four exterior points stay BLUE — an
  interior/exterior discriminator that a whole-surface fill or an off-by-one rect would fail.
  `d3d9_stretchrect_test.cpp` gives a src RT distinctive content (BLUE clear + RED left-half quad), then
  StretchRects it twice: sub-pass A (same-size 1:1 copy) checks dst mirrors src; sub-pass B (a genuine 2x
  horizontal stretch of the RED half) checks the whole dst is RED, with one checkpoint flipping from
  BLUE (1:1) to RED (scaled) as the discriminator proving real scaling, not just a same-size copy. Both
  tests pass pixel-exact on x64 and x86/WoW64 against the genuine Microsoft `d3d9.dll`; full regression
  sweep of every existing D3D9 guest test (x64+x86) verified clean at every stage by an independent,
  adversarial reviewer.
  **Two known limitations, documented as deliberate scope boundaries at the time (`83c518c6`)**:
  (1) `color_fill` hardcoded 4 bytes/texel — closed 2026-07-06, see the "Host render-target/readback
  path's 4-bytes-per-texel (BGRA8) assumption" entry in "M3 coverage items" below (it caused an actual
  bug during the later D3DFORMAT-advertisement work, `197cfbd3`, before this fix landed); (2)
  both handlers' `subresource`/`dst_subresource`/`src_subresource` parameters are always 0 and unused —
  single-mip, single-layer resources only, not yet plumbed to a real mip/array level. (Mip-mapping, once
  it landed, took a different path entirely — see the mip-mapping bullet below — so this `blt()`
  limitation remains open on its own terms, not superseded by that work.)
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
- [x] **Mip-mapping — done (2026-07-06, commits `256ea51e`/`080bbbfe`/`8ffb306c`/`625ae525`/`d2d29cd2`).**
  The old limitation: the resource's mip-level count was plumbed through `create_resource` but never
  consumed — every texture got exactly one Vulkan mip level regardless of how many the app requested, and
  `build_sampler` pinned `min_lod`/`max_lod` to 0/0 unconditionally, so a sampler could never select
  anything but level 0. This blocked BOTH remaining Tier-3 M3 items (this one and cube/volume, below) on
  the same unresolved question: `D3DDDIARG_LOCK` had no known field carrying *which* mip level (or
  cube face / volume slice) a given `LockRect`/`LockBox` call targeted, so there was no way to route a
  per-level `LockRect(level, ...)` write to the right place even if the host could store one.
  **RE (`256ea51e`)**: a gated live-RE pass resolved that question. The field previously modeled as
  `Reserved0` in `D3DDDIARG_LOCK` is `SubResourceIndex`, at offset 8 (x64) / 4 (x86) — confirmed BOTH
  statically (`DdLockLH`, the single builder of the struct crossing into `pfnLock` for every resource kind
  on the driver-routed path, writes the per-subresource index there) and live (a 3-mip texture locked at
  levels 0/1/2 showed exactly `{0,1,2}` at that offset, nothing else varying; a real vertex-buffer lock
  reads 0, since buffers have no subresources). **This initially looked like a NO-GO from static analysis
  alone**: a first static-only reading of `CDriverMipSurface::InternalLockRect`'s 84-byte outer
  bookkeeping struct genuinely carries no such field — the level only reaches the driver through the much
  smaller inner struct `DdLockLH` itself builds, which the static-only pass hadn't yet isolated. The live
  confirmation step — hooking `umd_Lock`'s entry and dumping `pArgs` across three real `LockRect(level)`
  calls — was the decisive evidence that flipped this from an apparent dead end to a confirmed, safe,
  unconditionally-readable field (same argument that already justified `OffsetToLock@80`). Additive-only
  commit: new struct field + `static_assert`s in `d3d9_ddi.hpp`, no wire/UMD/host behavior change.
  **Implementation (`080bbbfe`)**: `umd_Lock`/`umd_Unlock` read the real `SubResourceIndex` instead of
  hardcoding 0 and carry it over the wire's `subresource` field (the `#ifdef _WIN64` struct split makes
  the x64/x86 offset difference automatic); the UMD's per-lock backing maps are now keyed by
  `(resource, subresource)` so several mip levels of one texture can be locked at once. Host-side,
  `resource_entry` gained `extra_mips` (per-level backing for subresources 1..N-1, each sized for that
  level's own halved dimensions) plus a `subresource_backing()` accessor; `create_resource` sizes the
  whole mip chain and creates the Vulkan image with the real mip count (was hardcoded 1); the sampling
  image view spans the full mip chain (`levelCount = mip_levels`); `ensure_texture_uploaded` uploads every
  level to its own mip via one shared staging buffer; `build_sampler` derives a real `min_lod`/`max_lod`
  from the bound texture's actual mip count and the app's `D3DSAMP_MIPFILTER`/`MAXMIPLEVEL` state (was
  pinned to 0/0) — a single-mip resource still collapses to `min==max==0`, byte-for-byte unchanged from
  the old hardcoded behavior. This is real per-app-authored mip data reaching the GPU via the
  newly-unblocked Lock path, not a GPU-auto-generated fallback — the earlier tentative plan (see the
  `StretchRect`/`ColorFill` entry above) had flagged `d3d9_host::blt()`'s `vkCmdBlitImage` primitive as
  *plausibly* reusable for a future successive-blit mip-generation scheme; that path was not needed and
  was not used. The fix that shipped is more complete/correct than that fallback would have been, since it
  carries the app's own authored per-level content instead of a synthesized box-filter approximation.
  **Test evidence (`8ffb306c`)**: `d3d9_miptexture_test.cpp` creates a 64x64 3-level texture, fills each
  level a distinct solid color (level 0 RED, level 1 GREEN, level 2 BLUE) via its own real
  `LockRect(level, ...)` call, then renders four sub-passes — three pin `D3DSAMP_MIPFILTER=NONE` +
  `D3DSAMP_MAXMIPLEVEL=0/1/2` to force the sampler to exactly one level each (readback must be
  RED/GREEN/BLUE respectively — the GREEN/BLUE passes prove per-level data actually reached the GPU and
  is sampled; the old hardcoded-0 LOD would have read RED for all three), and a fourth drives genuine
  minification (a small on-screen quad, full LOD range, no `MAXMIPLEVEL` clamp) so the GPU's own
  screen-space-derivative LOD selection picks level 2 (BLUE) on its own. All four checks pass
  pixel-identically on both x64 and x86/WoW64. **Polish (`625ae525`/`d2d29cd2`)**: comment-accuracy fixes
  found during review — corrected `ensure_texture_uploaded`'s inaccurate "bails on incomplete mip data"
  claim (every level's backing is pre-sized to its exact tight size at creation, so an app-unwritten level
  uploads as zero-initialized black, not a detected "incomplete" case; the guard only ever catches a
  genuine degenerate zero-size case), fixed stale pre-mip-mapping comments in `create_resource` (still
  said "single mip/layer") and both `D3DDDIARG_LOCK` struct definitions (still said "NOT yet consumed by
  umd_Lock" directly below a block comment saying the opposite), and added a rationale comment for the
  storage-model design (why mip level 0 stays in `backing` instead of folding into `extra_mips[0]`, so
  every pre-existing RT/buffer call site addressing `.backing` directly needed zero changes).
  **Verification**: full regression sweep — every existing D3D9 guest test, both x64 and x86 — verified
  clean at every stage by an independent, adversarial reviewer, with `build_sampler`'s single-mip case
  being byte-identical to the old hardcoded behavior specifically, rigorously re-checked.
- [ ] **Cube/volume textures — still open; investigated 2026-07-06 and found blocked by a deeper,
  newly-discovered gate than a prior planning pass believed.** Two independent unknowns previously
  blocked this: (a) which field carries a per-face/per-slice subresource index for
  `LockRect(face,level)`/`LockBox(level)` — resolved, since it's the exact same `SubResourceIndex`
  mechanism the mip-mapping work above just RE-confirmed and wired (`SubResourceIndex` is documented,
  from the static side, as the flattened `FaceType*MipLevels + Level` for cube/array — inferred from
  `CCubeMap::LockRect`'s own array-index formula, not yet live-confirmed for cube/volume specifically, but
  the mechanism and struct offset are no longer an open question); and (b) the Vulkan image-type/view-type
  branching (`VK_IMAGE_TYPE_3D`, `VK_IMAGE_VIEW_TYPE_CUBE`, etc.) needed to back a non-2D resource, which
  needs no new `vulkan_host` signature changes — `create_image`/`create_image_view` already take an
  image-type/view-type parameter each, currently always called with `VK_IMAGE_TYPE_2D`/
  `VK_IMAGE_VIEW_TYPE_2D`; the primitives are already fully parameterized for this.
  A prior planning pass, going into 2026-07-06, believed that with (a) and (b) accounted for, all that
  remained was "confirm one classifier bit, then straightforward plumbing": live-confirm the CubeMap/
  Volume bit positions within the already-RE'd `D3DDDIARG_CREATERESOURCE::Flags` field (offset 56 x64 /
  48 x86, a `D3DDDI_RESOURCEFLAGS` bitfield, already RE'd and live-confirmed for other bits), then extend
  the classification + Vulkan image-type/view-type plumbing and `d3d9_shader_translator.cpp`'s per-sampler
  texture-dimension info (`vkd3d_shader_d3dbc_source_info` currently defaults to "2D" for every sampler).
  **A live-verified investigation on 2026-07-06 found this framing wrong — the real gap sits earlier and
  is deeper.** Real `d3d9.dll` REJECTS `IDirect3DDevice9::CreateCubeTexture()`/`CreateVolumeTexture()` with
  `D3DERR_INVALIDCALL` *before* `pfnCreateResource` is ever called, so the driver/UMD never even sees a
  cube/volume resource-creation request to classify — the `Flags`-bit question above is moot until this
  new gate is passed. Traced live and via static decompile: the rejection happens in
  `CBaseTexture::Validate` → `CBaseDevice::CheckDeviceFormat` → `CEnum::CheckDeviceFormat` (returning via
  `D3DRecordHRESULT`), which tests an INTERNAL, runtime-side per-format capability table — NOT the same
  thing as the driver's advertised `FORMATOP` bits sogen's UMD already sets in `g_formats` — and that
  internal table currently lacks cube (`0x10000`)/volume (`0x8000`) capability bits for the formats
  tested. This is specifically a per-FORMAT capability-table issue, not a device-level caps strip:
  `D3DCAPS9::TextureCaps` already correctly advertises `CUBEMAP`/`VOLUMEMAP` (confirmed live:
  `0x0001e804` includes both bits). A direct attempt to patch the UMD's `g_formats` FORMATOP bits — both
  the driver-facing encoding and the runtime's internal `0x10000`/`0x8000` encoding — did NOT unblock
  this: the runtime evidently consults a cached/transformed internal table, not a direct read of the
  driver's advertised op-word, so a simple UMD-side format-table edit doesn't reach the actual gate.
  Pinning down that transformation — how `d3d9.dll` builds its internal per-format cube/volume capability
  cache from whatever the UMD's `pfnGetCaps`/`GETFORMATDATA` DDI response actually contains — is a
  genuinely separate, deeper RE task with real uncertainty about whether it's solvable via a UMD-side
  change at all, or whether it needs a different category of intervention (e.g. a runtime patch similar in
  *kind* to the `D3DPOOL_MANAGED` caps-forcing hook below, but for a different code path).
  **Confirmed non-blocker from the same pass**: `d3d9_shader_translator.cpp` needs ZERO changes for
  cube/volume sampler support — vkd3d-shader derives sampler dimension from the shader bytecode's own
  `dcl_cube`/`dcl_volume` declaration tokens for SM2.0+ (the host-side dimension-hint field is documented
  as "ignored for shader models 2 and higher," and sogen only targets SM2.0+); the
  `vkd3d_shader_d3dbc_source_info` extension the prior planning pass flagged as needed does not apply.
  See `HANDOFF_MACBOOK.md` (2026-07-06 entry) for the full investigation trail, including scratch-tooling
  pointers for a future attempt. What remains open before cube/volume can even reach the
  previously-identified plumbing work: RE-ing the `CEnum::CheckDeviceFormat` internal-table
  transformation (or finding an alternative intervention point) — a genuinely uncertain task, not merely
  "another Flags bit to find."
- [x] **D3DFORMAT advertisement — done (2026-07-06, commits `ba83be93`/`18b74fcb`/`a9c2f8d3`/`197cfbd3`).**
  The old gap: the host's `d3d9_format_to_vulkan` (`d3d9_format.cpp`) already correctly mapped 13
  D3DFORMAT values to VkFormat, but the UMD's own `g_formats` FORMATOP table — the thing real
  `d3d9.dll` actually consults via `CheckDeviceFormat`/`CreateTexture`/`CreateRenderTarget` — only
  advertised 3-4 of them. Apps could not create textures or render targets in most formats the host
  could already handle, even though the host-side plumbing for them existed.
  **RE-gate finding (`ba83be93`)**: before committing to the full expansion, one new FORMATOP row
  (`D3DFMT_DXT1`, texture-only) was added as a throwaway gate and verified live against the real
  Microsoft `d3d9.dll`: `CheckDeviceFormat`/`CreateTexture` flipped from
  `D3DERR_NOTAVAILABLE`(`0x8876086a`)/`D3DERR_INVALIDCALL`(`0x8876086c`) to `S_OK`. This resolved the
  open question in the SAFE direction — advertising a brand-new format is a plain, mechanical table
  extension with no hidden wall behind it, unlike cube/volume textures (above), which hit an opaque,
  transformed internal capability cache no UMD-side table edit could reach.
  **Full expansion (`18b74fcb`)**: added the remaining 9 formats, each with the op-bit class matching
  realistic + host-supported usage: `D24X8` (FMT_OP_ZSTENCIL, depth-only variant matching `D24S8`),
  `R5G6B5` (initially RT_TEX — see the bug below), `A8`/`L8` (FMT_OP_TEXTURE, single-channel),
  `V8U8`/`Q8W8V8U8` (FMT_OP_TEXTURE, bump/normal), `A16B16G16R16F` (FMT_OP_TEXTURE only, deliberately —
  see "Known limitation" below), `DXT3`/`DXT5` (FMT_OP_TEXTURE, matching the `DXT1` precedent); plus
  upgrading the existing `A8R8G8B8` row from texture-only to RT_TEX (host-side `B8G8R8A8_UNORM`, 4
  bytes/texel, so its readback path is unaffected).
  **Test (`a9c2f8d3`)**: `d3d9_format_coverage_test.cpp`, three sub-passes each confirmed to fail
  against the pre-expansion table and pass after it — a DXT5 solid-color sample+readback, an
  A8R8G8B8 render-target render+readback, and an L8 luminance sample+readback.
  **A real bug, caught by code-quality review rather than by gate-verification (`197cfbd3`)**: the
  full expansion (`18b74fcb`) gave `R5G6B5` render-target capability (RT_TEX). This was wrong —
  `R5G6B5` is 2 bytes/texel, and every host-side render-target readback/Present/ColorFill path
  hardcodes a 4-bytes-per-texel (BGRA8) assumption (see "Known limitation" below). Had this shipped,
  a real app creating an `R5G6B5` render target would not have crashed or returned an error —
  `CreateRenderTarget` would have silently succeeded, and every subsequent readback/Present would have
  read the tightly-packed 2-byte-per-texel image through a stride and format computed for 4
  bytes/texel: silently corrupted output, not a crash. Fixed by scoping `R5G6B5` to FMT_OP_TEXTURE-only,
  matching how `A16B16G16R16F` (8 bytes/texel) was already correctly scoped in the same `18b74fcb`
  commit. A negative-case sub-pass was added to `d3d9_format_coverage_test.cpp`: `CreateTexture(R5G6B5)`
  must succeed but `CreateRenderTarget(R5G6B5)` must now fail (`D3DERR_NOTAVAILABLE`), proving the
  capability was genuinely withdrawn rather than merely left undocumented. Two stale comments left by
  the format-expansion work were also corrected in this commit: `classify_resource_usage`'s claim that
  `X8R8G8B8` is the "only" advertised RT format, and a README passage still claiming `A8R8G8B8` render
  targets fail.
  Full x64+x86 D3D9 guest-test regression sweep (all ~42 existing test runs) verified clean at every
  stage by two independent reviewers.
  **Deliberate scope boundary at the time, since closed** (see below): `A16B16G16R16F` and `R5G6B5` were
  BOTH texture-only, not render-target, for the identical real reason.
  **Genuinely still open, distinct from the above**: formats with no host-side VkFormat mapping at all
  yet — e.g. paletted formats (`P8`) — are not part of this closure; this slice only advertised the 13
  formats `d3d9_format_to_vulkan` already handled.
- [x] **Host render-target/readback path's 4-bytes-per-texel (BGRA8) assumption — closed for off-screen
  RTs (2026-07-06, commits `c845091e`/`e7248550`, code-quality follow-up `4c933513`).** The old
  limitation: every RT-sizing site hardcoded `* 4`, so `R5G6B5` (2 bytes/texel) and `A16B16G16R16F` (8
  bytes/texel) had to stay texture-only — giving either render-target capability would have silently
  corrupted readback output (the exact bug caught and reverted for `R5G6B5` in `197cfbd3` above).
  **Fix**: a new shared `vk_format_bytes_per_texel` helper (`d3d9_format.cpp`) is now the single source
  of truth for every RT-sizing site — `d3d9_host::create_resource`'s RT backing store,
  `vulkan_host::create_render_target`/`readback_render_target`'s CPU-side buffer sizing, and
  `d3d9_host::color_fill`, which was rewritten with a genuine per-format texel encoder
  (`encode_fill_texel` + a round-to-nearest-even `float_to_half` for the half-float case) instead of a
  raw D3DCOLOR-dword fill. `g_formats` flips `R5G6B5`/`A16B16G16R16F` to render-target-capable
  (re-confirmed neither sets `3DACCELERATION` without `DISPLAYMODE`, so the HAL-disable gauntlet stays
  satisfied). Proven by an inverted `d3d9_format_coverage_test.cpp` sub-pass: both formats now byte-exact
  round-trip through Clear + ColorFill + LockRect at their real tight stride, on x64 and x86/WoW64, with
  the full existing regression sweep (including `d3d9-colorfill-test`) confirmed pixel-identical to
  baseline. Independently verified by a spec-compliance reviewer (re-derived the 565-packing and
  half-float bit patterns by hand, exhaustively tested `float_to_half` against a brute-force reference
  for all 256 possible ColorFill input values) and a code-quality reviewer (one follow-up applied:
  collapsed a duplicate format→byte-size map inside `color_fill` down to the one shared helper).
  **Deliberately still out of scope**: making a non-BGRA8 render target *presentable* to the actual
  screen. The Present-path `ui_surface_desc` construction (`syscalls/gdi.cpp` and `gpu_bridge.cpp`)
  still builds `.stride = width * 4` and a fixed BGRA8 format unconditionally — `ui_surface_format` has
  no HDR/tone-mapping conversion stage, only bgra8/rgba8. `R5G6B5`/`A16B16G16R16F` render targets are
  therefore usable for off-screen work (ColorFill, StretchRect, Lock-readback) but never as the actual
  swap-chain back buffer. Not believed to block MW2 (a BGRA8-presenting game); a real future need for a
  non-BGRA8 swapchain would be a separate, larger task (a genuine present-path format/tone-map stage).
- [x] **SM3.0 caps — done (2026-07-06, commits `1b940580`/`d82434ff`/`c468e80d`).** The old limitation:
  `fill_d3d9caps` reported SM2.0 only (`VertexShaderVersion`/`PixelShaderVersion` = `D3DVS_VERSION(2,0)`/
  `D3DPS_VERSION(2,0)`), so real `d3d9.dll` would never accept a real `vs_3_0`/`ps_3_0` shader pair — a
  hard blocker for MW2, which is SM3-heavy.
  **RE finding, and why this was the same tractable shape as the SM2.0 gauntlet, not cube/volume's**:
  cube/volume textures (above) are blocked by an opaque, transformed internal capability cache inside
  `d3d9.dll` that a direct UMD-side write couldn't reach — a genuine wall. SM3.0 caps turned out to be a
  different shape entirely: `IsD3DHALSupported`'s SM3.0 validation branch reads its required `D3DCAPS9`
  fields DIRECTLY out of the same `GetCaps(type=13)` buffer `fill_d3d9caps` fills — no cache, no
  transform, no indirection — live-traced field by field, the exact same tractable shape as the SM2.0
  gauntlet this UMD already passed. That's what made this a one-pass RE job rather than an open-ended one.
  **The 12 confirmed fields** (`1b940580`; each carries its own validator-gate comment in
  `fill_d3d9caps`): the two version fields (`VertexShaderVersion`/`PixelShaderVersion`, raised from `2.0`
  to `D3DVS_VERSION(3,0)`/`D3DPS_VERSION(3,0)`, i.e. `0xFFFE0300`/`0xFFFF0300`); `DevCaps2 |=
  VERTEXELEMENTSCANSHARESTREAMOFFSET`; `RasterCaps |= COLORPERSPECTIVE`; three added `TextureCaps` bits
  (PERSPECTIVE/TEXREPEATNOTSCALEDBYSIZE/PROJECTED); two MRT-specific `PrimitiveMiscCaps` bits
  (INDEPENDENTWRITEMASKS/MRTPOSTPIXELSHADERBLENDING — required once VS/PS report 3.0 and
  `NumSimultaneousRTs>1`, which this UMD already advertises); `Cube`/`VolumeTextureFilterCaps`,
  `TextureAddressCaps`, and `StencilCaps` (all previously left at the memset-0 default — unread by the
  SM2.0 path, but rejected as HAL-unavailable if still 0 once SM3.0 is declared); and the **instruction-
  slot count inversion** — `MaxVertex/PixelShader30InstructionSlots` had to be 0 under SM2.0 (the
  aggregate validator required them clear) and now has to be nonzero (raised to 32768, the documented
  ceiling) under SM3.0 — the same fields, opposite requirement, depending on the declared shader model.
  **Test (`d82434ff`, `d3d9_sm3_test.cpp`)**: four independent proofs, anchored by a genuine SM3.0-only
  discriminator — the pixel shader has a real runtime-count loop (`SetPixelShaderConstantI`-driven), so
  the SAME source `D3DCompile`s successfully at `ps_3_0` but MUST FAIL at `ps_2_0` (no loop/rep, no
  integer constant registers at SM2.0) — not merely "shader creation succeeds," but a construct SM2.0
  cannot express at all. The other three proofs: (1) `GetDeviceCaps(HAL)` reports back exactly
  `VertexShaderVersion=0xFFFE0300`/`PixelShaderVersion=0xFFFF0300`; (2) the `ps_3_0` bytecode is walked as
  raw D3DBC tokens to confirm a real LOOP/REP opcode was actually emitted, not unrolled; (3) a real
  off-screen GPU draw with this pair (5 runtime-supplied loop iterations, PS int constant register at
  set 1/binding 2) reads back three analytically-recomputed, distinct channel bytes (`B=BF G=40 R=80`)
  pixel-exact on both x64 and x86/WoW64. Purely additive: all 40 pre-existing SM2.0 guest tests stayed
  byte-for-byte pixel-identical on both architectures.
  **Polish (`c468e80d`)**: code-quality review of `1b940580` caught a comment that overclaimed — it
  asserted this toolchain's `D3DPMISCCAPS_*` symbols resolve to a different bit than MSDN for one of the
  MRT fields, but checked against this repo's actual mingw-w64 14.0.0 `d3d9caps.h`, the symbols match
  MSDN exactly here; reworded to keep the raw-hex literals (still a sound defensive pin against a
  *future* toolchain regression) without asserting a discrepancy that doesn't currently hold. Also fixed
  a stale README note that still described the caps as SM2.0.
  **Residual uncertainty, stated honestly**: this closes SM3.0 CAPS acceptance and proves one genuine
  SM3.0-only construct (a runtime-count loop) compiles and renders correctly end to end. It does NOT
  prove every SM3.0 feature or instruction a real, complex MW2 shader might use works — other SM3.0-only
  constructs (e.g. additional interpolator behavior, other SM3.0-only opcodes) this minimal test didn't
  exercise could still surface their own gaps once real MW2 shaders are actually run. **Vertex texture
  fetch (`tex2Dlod` sampling `D3DVERTEXTEXTURESAMPLER0..3`) — named here at the time as exactly this kind
  of untested gap — has since been closed**; see the "Vertex texture fetch" bullet immediately below for
  the DDI-trace finding and the closure itself, including the separate, still-open FORMATOP
  `CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE)` capability-advertisement gap it left behind. Caps
  acceptance plus these two working discriminators is real, major progress toward MW2 — it is not a claim
  of complete SM3.0 instruction-set coverage.
- [x] **Vertex texture fetch (SM3.0 VS texture sampling via `tex2Dlod`) — done (2026-07-06, commits
  `fd1fcb46`/`e3aa2adf`/`8a41b682`).** The old gap: explicitly flagged as out of scope when SM3.0 caps
  (above) closed — "needed only if a specific MW2 vertex shader samples textures." A gated DDI trace run
  this session answered that question: real `d3d9.dll` passes `SetTexture(D3DVERTEXTEXTURESAMPLER0..3,
  tex)` (API sampler-stage constants 257-260) through to the DDI completely unmodified — an identity
  pass-through, the same shape as the existing PS sampler stages.
  **Design**: rather than build a second, parallel VS-only binding scheme, the existing PS-only
  multi-sampler scheme (`max_ps_sampler_stages`/`ps_sampler_binding_for_stage`, see the PS multi-sampler
  bullet above) was generalized in `d3d9_shader_translator.hpp` into one shared source of truth
  (`max_sampler_stages`/`sampler_binding_for_stage`), with `ps_`/`vs_`-prefixed names now forwarding
  aliases to it — so the translator and host sides for both stages can never independently drift the way
  the pre-refactor PS-only constants once risked. VS combined-image-samplers are declared into
  descriptor set 0 (the VS's own set), keyed off the VS's own `s0`..`s3` registers, with the same
  over-declaration-is-inert safety the PS array already relies on (vkd3d-shader only emits a SPIR-V
  sampler for a statically-referenced register). The host's VS descriptor-set-0 layout, the descriptor
  pool's combined-image-sampler sizing, and `execute_draw` all gained a VS-side texture-upload/
  descriptor-write loop keyed off `bound_textures[257 + k]` (`D3DVERTEXTEXTURESAMPLER0`), mirroring the
  existing PS loop; `build_sampler` is reused unchanged (defaults to POINT filtering/no mipmap, matching
  real D3D9's VTF sampling restriction).
  **Test (`e3aa2adf`, `d3d9_vertex_texture_test.cpp`)**: a genuinely "unfakeable by a pixel shader"
  discriminator — a real `vs_3_0` vertex shader samples a 2x2 `A16B16G16R16F` heightmap bound to
  `D3DVERTEXTEXTURESAMPLER0` with `tex2Dlod` and displaces one triangle vertex's position by the sampled
  height; only the vertex whose own per-vertex UV selects the high texel moves, so a correct result
  requires the vertex stage to have genuinely fetched that specific texel — not reproducible by a constant
  offset, and not reproducible by a pixel shader at all. Before/after confirmed: removing the VS-sampler
  binding makes `translate_d3d9_shader_pair` fail on the VS `texldl` and `execute_draw` degrade gracefully
  (the whole draw skipped), so the discriminator probe reads the clear color and the test fails on the old
  code. Passes pixel-exact on both x64 and x86/WoW64, independently reproduced by two separate reviewers,
  including a full regression sweep of every existing D3D9 guest test on both architectures — specifically
  including the PS multi-sampler test above — with zero regression. `8a41b682` added a citation for the
  DDI-passthrough claim that code-quality review caught missing.
  **FORMATOP capability-advertisement gap — now also closed (2026-07-06, follow-up commit).** The
  companion gap this slice deliberately left open: real `d3d9.dll`'s
  `CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE, ...)` advertised NO format as vertex-texture-usable
  (returned `D3DERR_NOTAVAILABLE` for every format), so a well-behaved app that gates VTF usage on
  `CheckDeviceFormat` succeeding first would refuse to use vertex textures even though the DDI bind/draw
  path above works. Resolved by adding a single FORMATOP op-bit — `FMT_OP_VERTEXTEXTURE` — to the
  `A16B16G16R16F` row of `g_formats` (the format the VTF test already exercises; no new host-side
  `d3d9_format_to_vulkan` mapping needed). It turned out to be the SAFE, mechanical-table-extension case
  (like the D3DFORMAT advertisement bullet above), NOT an opaque internal wall like cube/volume — but
  with a real RE twist: the correct op-bit is `0x00800000` (ReactOS `ddrawint.h`
  `D3DFORMAT_OP_VERTEXTEXTURE`), NOT the `0x00400000` a first pass assumed (that value is
  `D3DFORMAT_OP_AUTOGENMIPMAP`). A gated RE pass on real `d3d9.dll`'s `CEnum::CheckDeviceFormat` showed it
  tests an internal per-format op-word (a VERBATIM copy of the driver's advertised FORMATOP — proven by
  every other required bit in its mask-construction matching the DDI value exactly: TEXTURE=0x1,
  ZSTENCIL=0x40, SRGBREAD=0x8000, SRGBWRITE=0x100000, …) for bit `0x00800000` on a
  `D3DUSAGE_QUERY_VERTEXTEXTURE` query; advertising `0x00400000` was verified live to STILL return
  `D3DERR_NOTAVAILABLE`, while `0x00800000` flips it to `S_OK`. Proven by a new sub-pass added to
  `d3d9_format_coverage_test.cpp` (`CheckDeviceFormat(QUERY_VERTEXTEXTURE, A16B16G16R16F)` → `S_OK`,
  `L8` → `D3DERR_NOTAVAILABLE`), a genuine before/after discriminator (the new test FAILS against the
  pre-change UMD with `A16B16G16R16F hr=0x8876086a`), pixel-byte-identical on x64 and x86/WoW64 with a
  full regression sweep of every existing D3D9 guest test on both architectures showing zero regression.
  Purely a capability-advertisement bit: no host-side change (the DDI path was already functional).

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

`DrawPrimitiveUP`/`DrawIndexedPrimitiveUP` is also now done (2026-07-06, commits `f36af2b7`/
`1c2bd176`/`91f1ded5`/`bc86b91b`) — real `d3d9.dll` reuses the ordinary draw slots via two new
UM-binding DDI calls rather than a dedicated UP-draw call, needing zero new draw-time host handler; this
also incidentally fixed a pre-existing, x86-only `pfnDrawPrimitive` arity bug that no existing test had
ever reached. See "M3 coverage items" above for the full account.

`StretchRect`/`ColorFill` is also now done (2026-07-06, commits `b915658a`/`3dd369f9`/`f7b9696e`/
`83c518c6`) — live RE confirmed the real device-func-table slots (56/55) and corrected `D3DDDIARG_BLT`'s
field order to SRC-first-then-DST, and a `StretchRectFilterCaps` caps-bit fix unlocked genuine scaled
StretchRect; see "M3 coverage items" above for the full account, including the two deliberately-scoped
known limitations it documented rather than left silent (4-byte-texel `color_fill`, since generalized —
see below — and subresource always 0, still open).

Mip-mapping is also now done (2026-07-06, commits `256ea51e`/`080bbbfe`/`8ffb306c`/`625ae525`/
`d2d29cd2`) — a gated RE pass resolved the same `D3DDDIARG_LOCK::SubResourceIndex` question that had
blocked BOTH remaining Tier-3 M3 items, unblocking real per-mip-level texture upload, a real Vulkan
mip-chain image/view, and real sampler LOD selection under genuine minification, proven pixel-exact on
x64 and x86/WoW64 by a 4-sub-pass discriminator test; see "M3 coverage items" above for the full account,
including why this initially looked like a NO-GO from static analysis alone. This resolved one of the two
independent unknowns that used to block cube/volume textures (below) — the exact same `SubResourceIndex`
mechanism covers its per-face/per-slice uploads — but a separate 2026-07-06 investigation (see the
cube/volume bullet below) found a deeper, previously-unknown blocker sitting in front of that: real
`d3d9.dll` rejects cube/volume texture creation entirely, before any driver call, in
`CEnum::CheckDeviceFormat`'s internal per-format capability table.

SM3.0 caps is also now done (2026-07-06, commits `1b940580`/`d82434ff`/`c468e80d`) — real `d3d9.dll`'s
`IsD3DHALSupported` SM3.0 validation branch turned out to read its required `D3DCAPS9` fields directly
out of the same UMD-filled `GetCaps` buffer, with no opaque cache or transform standing in the way
(unlike cube/volume's `CEnum::CheckDeviceFormat` gate above) — the same tractable RE shape as the
original SM2.0 gauntlet, not a repeat of cube/volume's genuine wall. The 12-field delta (the two version
fields, the `DevCaps2`/`RasterCaps`/`TextureCaps` bits, the two MRT-specific `PrimitiveMiscCaps` bits,
`Cube`/`VolumeTextureFilterCaps`/`TextureAddressCaps`/`StencilCaps`, and the SM3.0 instruction-slot-count
inversion from 0 to 32768) is proven end to end by `d3d9_sm3_test.cpp`: a real `vs_3_0`/`ps_3_0` pair
whose pixel shader uses a genuine SM3.0-only construct — a runtime-count loop that fails to compile at
`ps_2_0` but succeeds and renders correctly at `ps_3_0` — pixel-exact on both x64 and x86/WoW64, with
zero regression across all 40 pre-existing SM2.0 guest tests. This is a genuinely major milestone for MW2
integration specifically, since MW2 is SM3-heavy — but it is CAPS acceptance plus one working SM3.0-only
construct, not proof every SM3.0 instruction a real, complex MW2 shader might use has been exercised;
that smaller, residual uncertainty stays open until real MW2 shaders are actually run. See the "M3
coverage items" checklist above for the full account.

Vertex texture fetch — one of the specific SM3.0-only constructs that closure's residual-uncertainty note
called out as untested — also closed out 2026-07-06 (commits `fd1fcb46`/`e3aa2adf`/`8a41b682`). A gated
DDI trace confirmed real `d3d9.dll` passes `SetTexture(D3DVERTEXTEXTURESAMPLER0..3, tex)` through to the
DDI as an identity pass-through, no different from an ordinary PS sampler stage. The existing PS-only
multi-sampler binding scheme was generalized into one shared source of truth used by both shader stages
(`max_sampler_stages`/`sampler_binding_for_stage` in `d3d9_shader_translator.hpp`), and a genuinely
unfakeable-by-a-pixel-shader discriminator test (a vertex shader that displaces one triangle vertex by a
real sampled height value) proved the fetch pixel-exact on both x64 and x86/WoW64, independently
reproduced by two reviewers, with zero regression across the full existing D3D9 guest-test sweep. See the
"M3 coverage items" checklist above for the full design/test account. The companion **FORMATOP
capability-advertisement gap is now also closed** (2026-07-06 follow-up): real `d3d9.dll`'s
`CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE, ...)` advertised no format as vertex-texture-usable,
which would block a well-behaved app that queries capability before binding. Fixed by advertising the
`FMT_OP_VERTEXTEXTURE` op-bit on the `A16B16G16R16F` `g_formats` row — the safe, mechanical-table-extension
case, with the RE twist that the correct bit is `0x00800000` (`D3DFORMAT_OP_VERTEXTEXTURE`), not the
`0x00400000` (`D3DFORMAT_OP_AUTOGENMIPMAP`) a first pass assumed; RE-confirmed against
`CEnum::CheckDeviceFormat`, which checks its internal per-format op-word (a verbatim copy of the driver
FORMATOP) for `0x00800000`. Proven by a new `d3d9_format_coverage_test.cpp` sub-pass, before/after
discriminator, pixel-identical on x64 and x86/WoW64 with a zero-regression full sweep. See the "M3
coverage items" checklist above for the full account.

`stream_frequencies`/instancing closed out 2026-07-06 (commits `d3a0318c`/`a6062d66`/`1d4d0ab9`; see the
M3 row above and the "M3 coverage items" checklist for the full account).

D3DFORMAT advertisement also closed out 2026-07-06 (commits `ba83be93`/`18b74fcb`/`a9c2f8d3`/`197cfbd3`;
see the M3 row above and the "M3 coverage items" checklist for the full account) — an RE gate confirmed
advertising a new FORMATOP row is a plain mechanical extension with no opaque wall behind it (unlike
cube/volume, below), the remaining 9 host-mapped formats were added, and a code-quality review caught and
fixed a real bug in that same expansion: `R5G6B5` was briefly given render-target capability it should
not have had, given the host's then-hardcoded 4-bytes-per-texel RT readback/Present/ColorFill assumption.
**That underlying constraint is now closed** (2026-07-06, commits `c845091e`/`e7248550`/`4c933513`):
`R5G6B5` and `A16B16G16R16F` are both real, byte-exact off-screen render targets today via a shared
`vk_format_bytes_per_texel` helper — see the "Host render-target/readback path's 4-bytes-per-texel
(BGRA8) assumption" bullet in "M3 coverage items" for the full account, including the still-open,
deliberately-scoped-out Present-path (non-BGRA8 swapchain) gap.

M3's one remaining DDI-coverage item — cube/volume textures — can proceed next; it is now the only item
left on this list, since D3DFORMAT advertisement (the other item this sentence used to list before this
session) is done. Cube/volume
textures were believed, going into
2026-07-06, to be meaningfully lower-risk than before the mip-mapping slice: the per-subresource
addressing mechanism they need (`D3DDDIARG_LOCK::SubResourceIndex`) is RE-confirmed and wired end to end,
and the Vulkan image-type/view-type branching they'll need requires no new `vulkan_host` signature changes
(`create_image`/`create_image_view` already take an image-type/view-type parameter, currently always
called with the 2D values). A 2026-07-06 investigation found that framing incomplete: real `d3d9.dll`
rejects `CreateCubeTexture`/`CreateVolumeTexture` with `D3DERR_INVALIDCALL` before `pfnCreateResource` is
ever reached, via `CEnum::CheckDeviceFormat`'s internal per-format capability table (distinct from the
driver's advertised `FORMATOP` bits) — a genuinely deeper, not-yet-solved gate that must be passed before
the `D3DDDIARG_CREATERESOURCE::Flags` classifier-bit question even becomes relevant; see the cube/volume
bullet above for the full account, including a confirmed non-blocker (the shader translator needs no
changes for cube/volume samplers under SM2.0+) and a failed patch attempt. `D3DPOOL_MANAGED` is now fixed
on both x64 and
x86/WoW64 (see above) — the 32-bit RE pass that was previously the standing MW2-integration risk has been
done (the caps-strip site in `syswow64/d3d9.dll` was disassembled, its 8-byte pattern confirmed unique,
and a second hook branch gated on the x86 machine type wired up and proven by the ported
`d3d9_managed_texture_test-x86.exe`), so this is no longer a budgeted risk for MW2 integration. If x86
partial-buffer Lock support becomes necessary before MW2 integration, budget for the same kind of live-RE
pass (`D3DDDIARG_LOCK`'s x86 driver-routed `OffsetToLock` offset) that resolved the x64 case.
