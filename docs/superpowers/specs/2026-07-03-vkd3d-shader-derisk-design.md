# Part 4 De-Risk Slice: Real vkd3d-shader Translation Design

## Context

Stage 2's D3D9-over-Vulkan work has, through the previous de-risk slice, reached a real
GPU-rendered triangle from genuine app-authored vertex data using a single hardcoded
fixed-function (FF) shader pair (`D3DFVF_XYZRHW|D3DFVF_DIFFUSE`, position + diffuse-color
passthrough). The original architecture plan (`.claude/plans/scalable-giggling-fern.md`, §222-244)
already scopes "Part 4 — Shader translation": integrate `vkd3d-shader` (the SM1-3 `d3dbc`-front-end
→ SPIR-V-back-end *compiler*, not the vkd3d/DXVK runtime) as a host-only dependency, translate real
SM2/3 bytecode, and pin a binding contract between the translated SPIR-V's descriptor expectations
and the pipeline layout the decoder builds.

That original plan's Part 4 scope (full binding-contract generality, shader-hash-keyed caching, a
programmable SM2/3 triangle pixel-diffed against a DXVK oracle) is larger than what actually exists
today. The current implementation diverged from Part 3's own original design in real ways: no
`dxgk_state.allocations`-based resource model, no pipeline-key/dynamic-state caching system, no
`D3DKMTPresent`-reuse for present (a new custom snapshot+`present_surface` path was built instead),
and no DXVK oracle (`root_vkspike`) exists anywhere in the repo. Building the full Part 4 as
originally scoped would be premature given how much smaller the actual pipeline is.

This spec covers a **de-risk slice**: prove real SM2 bytecode → vkd3d-shader → SPIR-V → a working
Vulkan pipeline, end-to-end, with a real (if minimal) constant-buffer binding — verified the same
analytic way the FF triangle was, since there is still no DXVK oracle to diff against.
**Programmable-shader generality (textures, samplers, multiple constant registers, HLSL compilation,
shader-hash caching, the full pipeline-key system, fixed-function shader synthesis) is explicitly
deferred** to a follow-on slice once this one proves the plumbing.

---

## Goal

A real SM2 vertex+pixel shader pair, hand-assembled (no HLSL compiler in this slice), translated by
real `vkd3d-shader` into SPIR-V, rendering a triangle functionally identical to the existing
fixed-function triangle (position + diffuse-color passthrough) — verified via the same host-side
analytic pixel readback pattern used for the FF triangle (no DXVK oracle needed for this slice).

## Non-Goals (explicitly deferred)

- HLSL-to-SM2 compilation (fxc/d3dcompiler or an open alternative) — a separate, later toolchain
  decision.
- Textures, samplers, or more than one constant register/UBO.
- The DXVK oracle (`root_vkspike`) and pixel-diff harness.
- The general pipeline-key/dynamic-state caching system from the original Part 3 design — this slice
  keeps the existing single-cached-pipeline model, just adds a second (programmable) cached slot
  alongside the existing FF one.
- Shader-hash-keyed translation caching (translate once per `create_vertex_shader`/
  `create_pixel_shader` call, no cross-call cache).
- Fixed-function shader synthesis (M4 in the original plan) — unrelated to this slice.
- WASM/emscripten build of vkd3d-shader — desktop only for now.
- 32-bit/WoW64 shader translation — x64 only, matching the rest of Stage 2 so far.

---

## Architecture

Four pieces, each with a clear, narrow boundary:

### 1. `deps/vkd3d` host dependency

A git submodule at `deps/vkd3d` (upstream: `https://gitlab.winehq.org/wine/vkd3d.git` or the current
canonical vkd3d-shader source location — confirm exact URL when adding the submodule). Only
`libvkd3d-shader` is built (the `d3dbc` SM1-3 bytecode front-end + SPIR-V back-end); the full
vkd3d runtime (D3D12-on-Vulkan) is not needed and not built.

**Build integration:** `deps/CMakeLists.txt` gains an `ExternalProject_Add` target invoking
`meson setup` + `ninja` as a subprocess, mirroring the existing `icicle-rust-project` pattern (a Rust
crate built via `cargo` through the same `ExternalProject_Add` mechanism) — same shape, different
underlying build tool. Produces a static library (`libvkd3d-shader.a`) and its public headers,
consumed by the new translator module below. Never linked into any guest-shipped binary (UMD or
guest test) — host-only, matching the "guest ships official MS DLLs + our thin UMD only" architecture
decision already locked in the original Stage 2 plan.

**New build prerequisite:** `meson` (already installed this session via `brew install meson`;
`ninja` was already present). Document in `README.md`/`CLAUDE.md`'s build prerequisites list
alongside the existing mingw/glslang/etc. entries.

### 2. `d3d9_shader_translator.{hpp,cpp}` (new, host-only)

New files: `src/windows-emulator/devices/d3d9_shader_translator.hpp` and `.cpp`. Thin wrapper,
isolated so `d3d9_host.cpp` never includes vkd3d headers directly (same "keep backend calls confined
to a clean operation set" discipline `vulkan_host.hpp`'s own header comment already documents, and
that this session's `d3d9_host.cpp` already follows for real Vulkan types via
`#include <vulkan/vulkan_core.h>` staying out of the header).

**Public API** (exact signature to be finalized once the vendored `vkd3d_shader.h` is available to
read directly — this is the intended shape):

```cpp
namespace sogen
{
    // Translates raw SM1-3 shader bytecode (the DDI's pFunction/pCode token blob, a DWORD-tagged
    // stream starting with the version token) into SPIR-V bytes vulkan_host::create_shader_module
    // can consume directly. Returns false on translation failure (malformed/unsupported bytecode);
    // out_spirv is left empty in that case.
    bool translate_d3d9_shader(const void* tokens, size_t token_size_bytes, std::vector<uint32_t>& out_spirv);
}
```

Internally calls `vkd3d_shader_compile()` (or the current vkd3d-shader public entry point — confirm
exact function/struct names against the vendored header; the original architecture plan's §4.1
already asserts this shape from earlier research, treat as a starting point to verify, not a fact to
assume blind) with a `d3dbc`-shaped input descriptor and a `SPIRV_BINARY` output target.

### 3. `d3d9_host.cpp`/`.hpp` changes

**`create_vertex_shader`/`create_pixel_shader`** (currently: just stash the raw token blob into a
`shader_entry{tokens}`, translated by nothing, never read by `execute_draw`) now call
`translate_d3d9_shader` immediately at creation time, and on success additionally call
`vulkan_host::create_shader_module` to get a real Vulkan shader module id, stored alongside the raw
tokens in `shader_entry` (new field, e.g. `uint64_t vk_module_id{}`). On translation failure, the
wire response's `hr` is a failure code and no module is created — the shader id still exists (for
API-level symmetry with the app's shader-handle lifetime) but can never be successfully bound for a
programmable draw.

**New programmable pipeline path.** `ensure_pipeline` currently builds and caches exactly one
pipeline (the hardcoded FF pair, `pipeline_`/`pipeline_ready_`). Add a second, parallel cached slot
(`programmable_pipeline_`/`programmable_pipeline_ready_`, plus its own `pipeline_layout_`-equivalent
if the binding contract needs a different layout — see below) built from the *real* translated VS/PS
modules bound via `state_.vertex_shader`/`state_.pixel_shader`, instead of `vs_module_`/`fs_module_`.
Still a single cached instance (no pipeline-key system yet, matching this slice's deferred scope) —
correctness for this slice's one shader pair, not generality across arbitrary shader combinations.

**Selection in `execute_draw`:** if `state_.vertex_shader != 0 && state_.pixel_shader != 0`, use the
programmable pipeline/modules; otherwise fall back to the existing FF path unchanged. This is an
`if`/`else` at the top of the pipeline-selection logic, not a rewrite of `execute_draw`'s existing
vertex-upload/barrier/draw/readback sequence, which is shader-path-agnostic already.

**Constant-buffer binding contract (the one piece of "the binding contract" this slice actually
exercises).** One UBO at a fixed `(set, binding)`, sized generously enough for a handful of `c#`
registers (e.g. 16 `float4` = 256 bytes, matching a modest constant-register budget — exact count
not critical for this slice since the test shader uses at most one), populated per-draw from
`state_.vs_const_f` (already tracked via `set_vs_const_f`, currently written but never read anywhere)
via `create_buffer` + `upload_memory`, mirroring the existing per-draw vertex-buffer upload pattern in
`execute_draw`. The SPIR-V `vkd3d_shader_compile()` produces must declare its constant-register UBO
at this same fixed binding — pinned once, matching the original plan's §4.2 warning that a mismatch
here is wrong pixels, not a crash (exactly why this slice's own analytic pixel check matters).

No sampler/texture descriptor sets in this slice (Non-Goals).

### 4. Guest side

**UMD (`sogen_d3d9_umd.cpp`) additions.** Wire the currently-unimplemented DDI slots:
- `pfnCreateVertexShaderFunc` (slot 42) / `pfnDeleteVertexShaderFunc` (slot 43) — only
  `pfnSetVertexShaderFunc` (slot 44) is wired today; a shader handle gets *set* but is never actually
  *created* via a proper marshaled `pfnCreateVertexShaderFunc` call, so nothing currently reaches
  `d3d9_host::create_vertex_shader` with real token data.
- `pfnCreatePixelShader` (slot 67) / `pfnDeletePixelShader` (slot 68) — same gap on the pixel-shader
  side (note the DDI's own naming asymmetry: `...VertexShaderFunc` vs. plain `...PixelShader`, already
  reflected in `d3d9_ddi.hpp`'s field names).

Marshaling shape: `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`D3DDDIARG_CREATEPIXELSHADER`-equivalent structs
(carry the raw token blob pointer + size — exact struct layout needs the same "hand-define against
WDK/Wine `d3dumddi.h`, cross-check via idasql if the layout doesn't dispatch correctly" methodology
already used successfully for `D3DDDIARG_LOCK`/`PRESENT` this session) → `bridge_call` with
`ioctl_d3d9_create_vertex_shader`/`ioctl_d3d9_create_pixel_shader` (both already defined as wire
opcodes, currently unused by any caller) → `d3d9_host::create_vertex_shader`/`create_pixel_shader`.

**New guest test** (`src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp`, compiled to
`d3d9-shader-test.exe` via the same standalone mingw recipe as the existing FF test) — deliberately
**separate** from `d3d9_triangle_test.cpp` so the FF triangle stays a working regression baseline
throughout this slice's development, not modified in place. Same window/device/render-target setup as
the FF test; the only difference is `CreateVertexShader`/`CreatePixelShader` with hand-assembled SM2
bytecode instead of `SetFVF`. **Hand-assembled bytecode, not HLSL-compiled** (Non-Goal): a minimal SM2
vertex shader that transforms/passes through a pre-transformed screen-space position (matching the
existing `D3DFVF_XYZRHW`-style input the FF path already uses) and passes through a per-vertex
diffuse color, and a pixel shader that outputs that color unchanged — functionally identical to the
FF shader pair, so translation correctness is the only new variable under test, not shader behavior.
Token-level assembly follows the D3D9 shader bytecode format directly (version token, `dcl_*`
declarations, `mov`/`dp4`-family instructions) — no external assembler tool, hand-written DWORD
arrays in the test source, matching this session's own hand-computed SPIR-V constant-array pattern for
the FF shaders.

---

## Data Flow (one draw call, programmable path)

1. Guest: `CreateVertexShader(vsTokens, &vs)` → `umd_CreateVertexShaderFunc` marshals tokens →
   `ioctl_d3d9_create_vertex_shader` → `d3d9_host::create_vertex_shader` → `translate_d3d9_shader`
   (real vkd3d-shader call) → SPIR-V bytes → `vulkan_host::create_shader_module` → `shader_entry`
   stored, wire shader id returned. Same for `CreatePixelShader`.
2. Guest: `SetVertexShader(vs)`/`SetPixelShader(ps)` → already-wired `umd_SetVertexShaderFunc`/
   `umd_SetPixelShader` → `state_.vertex_shader`/`pixel_shader` updated (existing code path,
   unchanged).
3. Guest: `SetVertexShaderConstantF(0, color, 1)` → already-wired `umd_SetVertexShaderConst` →
   `state_.vs_const_f` updated (existing code path, unchanged — currently unread downstream; this
   slice adds the reader).
4. Guest: `DrawPrimitive(...)` → `execute_draw` → sees both shaders bound → `ensure_pipeline`'s
   programmable branch (build-or-reuse cached programmable pipeline from the two real shader
   modules) → upload `state_.vs_const_f` into the fixed-binding UBO → bind pipeline + UBO descriptor
   + vertex buffer → draw → readback into the render target's backing (existing pattern, unchanged).
5. Host-side temporary diagnostic (same pattern as the FF triangle's verification, removed after
   confirming): read back the centroid pixel, compare against the same hand-computed barycentric
   color average used for the FF triangle (the test shader is functionally identical, so the expected
   value is identical too).

---

## Error Handling

- Translation failure (`translate_d3d9_shader` returns false): `create_vertex_shader`/
  `create_pixel_shader`'s wire response carries a failure `hr`; the UMD's marshaling function
  returns the corresponding `HRESULT` up to the app (matching the existing pattern for other
  `create_*` wire calls). No partial/corrupt shader module is ever created.
- Draw with one shader bound but not the other (e.g. VS set, PS still fixed-function/null): falls
  back to the existing FF path (the `&&` condition in the selection logic) rather than attempting a
  mixed programmable/fixed-function pipeline, which real D3D9 doesn't support either (VS+PS are
  bound as a pair for the programmable pipeline in real hardware terms).
- Constant buffer smaller than what the shader declares (e.g. app never called
  `SetVertexShaderConstantF` for a register the shader reads): `state_.vs_const_f` stays whatever
  size it was last resized to (existing behavior, unchanged by this slice) — out-of-range reads in
  the shader read zero-initialized UBO memory, not a host-side crash, since the UBO is sized
  generously up front (256 bytes) regardless of how much of it the app has actually written.

---

## Testing / Verification

Same analytic host-side pixel-readback pattern as the FF triangle (no new test infrastructure):
1. Build `d3d9-shader-test.exe`, stage it into `root/filesys/c/`.
2. `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-shader-test.exe`.
3. Temporary host-side diagnostic (added to `d3d9_host.cpp`'s draw-readback path, removed once
   confirmed) prints the centroid pixel; compare against the same hand-computed expected barycentric
   color average as the FF triangle (identical shader behavior, so identical expected value).
4. `analyzer -e root -s c:/test-sample.exe` (smoke test) stays green throughout — no regression to
   the existing FF path, confirmed by also re-running `d3d9-triangle-test.exe` (unmodified) after
   this slice's changes land.

---

## File Inventory

**Create:**
- `deps/vkd3d/` (git submodule)
- `src/windows-emulator/devices/d3d9_shader_translator.hpp`
- `src/windows-emulator/devices/d3d9_shader_translator.cpp`
- `src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp`

**Modify:**
- `deps/CMakeLists.txt` — new `ExternalProject_Add` target for `libvkd3d-shader` (meson/ninja).
- `src/windows-emulator/CMakeLists.txt` (or wherever `windows-emulator`'s target sources/link
  libraries are declared) — add the new translator `.cpp`, link `libvkd3d-shader`.
- `src/windows-emulator/devices/d3d9_host.hpp` — new private members for the programmable pipeline
  slot (module ids, pipeline/layout ids, readiness flag); `shader_entry` gains a `vk_module_id`
  field.
- `src/windows-emulator/devices/d3d9_host.cpp` — `create_vertex_shader`/`create_pixel_shader`
  translate immediately; `ensure_pipeline` gains the programmable branch; `execute_draw` gains the
  FF-vs-programmable selection and the constant-UBO upload.
- `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp` — new `umd_CreateVertexShaderFunc`/
  `umd_DeleteVertexShaderFunc`/`umd_CreatePixelShader`/`umd_DeletePixelShader`, wired at slots
  42/43/67/68.
- `src/samples/sogen-d3d9-umd/d3d9_ddi.hpp` — new `D3DDDIARG_CREATEVERTEXSHADERFUNC`/
  `D3DDDIARG_CREATEPIXELSHADER`-equivalent struct definitions (hand-defined, RE-verified against a
  real forcing function the same way `D3DDDIARG_LOCK`/`PRESENT` were this session — do not guess the
  layout and ship it unverified).
- `README.md`/`CLAUDE.md` — document `meson` as a new build prerequisite.

---

## Open Questions to Resolve During Implementation (not blocking design approval)

- Exact vkd3d-shader public API shape (`vkd3d_shader_compile()` or current equivalent) — confirm
  against the vendored header once the submodule is added, before writing
  `d3d9_shader_translator.cpp` against assumed signatures.
- Exact `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`CREATEPIXELSHADER` struct layouts — RE via a forcing
  function (this test's own `CreateVertexShader`/`CreatePixelShader` calls), not guessed from WDK
  docs alone, matching this session's established methodology (the `D3DDDIARG_LOCK` struct guessed
  from WDK-adjacent docs turned out wrong; live verification is what actually worked).
- vkd3d submodule's exact upstream URL/pinned commit — confirm current canonical location when adding
  the submodule (Wine's GitLab mirror is the most likely candidate as of this project's other
  vkd3d-shader references, but verify before pinning).
