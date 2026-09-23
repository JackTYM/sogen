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

A real SM2 vertex+pixel shader pair, compiled from a tiny HLSL source string at guest runtime via
`D3DCompile()` (real Microsoft `d3dcompiler_43.dll`, already staged in `root/filesys/c/windows/
system32/` — a runtime call to an already-present guest DLL, not a new host build dependency; no
hand-encoded bytecode, avoiding the risk of getting the D3D9 shader token/opcode encoding wrong from
memory), translated by real `vkd3d-shader` into SPIR-V, rendering a triangle functionally identical
to the existing fixed-function triangle (position + diffuse-color passthrough) — verified via the
same host-side analytic pixel readback pattern used for the FF triangle (no DXVK oracle needed for
this slice).

## Non-Goals (explicitly deferred)

- A *host-side* HLSL compilation toolchain (fxc/d3dcompiler as a build-time dependency) — this slice
  uses the already-staged guest `d3dcompiler_43.dll` at runtime instead, which needs no new host
  dependency at all.
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

**Build integration (corrected 2026-07-03):** the real vkd3d source tree (confirmed by cloning it
read-only for inspection) builds with **autotools** (`autogen.sh`/`configure`/`make`), not meson —
there is no `meson.build` anywhere in the tree. `deps/CMakeLists.txt` gains an `ExternalProject_Add`
target running `./autogen.sh && ./configure --disable-tests --disable-demos` then
`make libvkd3d-shader.la`, mirroring the existing `icicle-rust-project` pattern's shape (an
`ExternalProject_Add` wrapping a non-CMake build tool) but with autotools instead of meson/ninja.
`libs/vkd3d-shader` is a genuinely separate library target (`lib_LTLIBRARIES = libvkd3d-shader.la
libvkd3d.la libvkd3d-utils.la` in `Makefile.am`) — only it is built; the full vkd3d runtime
(D3D12-on-Vulkan) is not needed and not built. Produces `.libs/libvkd3d-shader.a` and the public
headers under `include/` (`vkd3d_shader.h` + `vkd3d_types.h`), consumed by the new translator module
below. Never linked into any guest-shipped binary (UMD or guest test) — host-only, matching the
"guest ships official MS DLLs + our thin UMD only" architecture decision already locked in the
original Stage 2 plan.

**Build prerequisites (confirmed present):** `perl` (with the required `JSON` module — checked via
`perl -MJSON -e 1`, present), `flex`, `bison` all already present on this Mac. `widl` (the Wine IDL
compiler) is missing but only gates building header files from `.idl` sources for the full vkd3d D3D12
API surface, which this integration never touches (`vkd3d_shader.h` is a plain header, not
IDL-generated) — `configure` warns, does not fail, without it. `meson` (installed earlier this session
via `brew install meson`) turned out to be unnecessary for this dependency; leave it installed (no
value in uninstalling) but do not document it as a vkd3d-shader prerequisite.

### 2. `d3d9_shader_translator.{hpp,cpp}` (new, host-only)

New files: `src/windows-emulator/devices/d3d9_shader_translator.hpp` and `.cpp`. Thin wrapper,
isolated so `d3d9_host.cpp` never includes vkd3d headers directly (same "keep backend calls confined
to a clean operation set" discipline `vulkan_host.hpp`'s own header comment already documents, and
that this session's `d3d9_host.cpp` already follows for real Vulkan types via
`#include <vulkan/vulkan_core.h>` staying out of the header).

**Public API (corrected 2026-07-03 — confirmed against the real `vkd3d_shader.h` and vkd3d's own
`tests/shader_runner_vulkan.c` reference integration):** legacy SM1-3 bytecode
(`VKD3D_SHADER_SOURCE_D3D_BYTECODE`) has no semantic-based VS→PS linking — that's an SM4+ feature —
so the VS and PS must be scanned (`vkd3d_shader_scan`) to obtain their I/O signatures, linked with
`vkd3d_shader_build_varying_map()`, and only then compiled. This means a shader can't be translated in
isolation; the API is a pair, not two independent single-shader calls:

```cpp
namespace sogen
{
    struct shader_pair_spirv
    {
        std::vector<uint32_t> vertex_spirv;
        std::vector<uint32_t> pixel_spirv;
    };

    // Translates a raw SM1-3 vertex+pixel shader pair (the DDI's pFunction/pCode token blobs, each a
    // DWORD-tagged stream starting with the version token) into SPIR-V bytes
    // vulkan_host::create_shader_module can consume directly. Both shaders are required together
    // because SM1-3 has no semantic-based inter-stage linking; vkd3d-shader instead requires an
    // explicit varying map built from both shaders' scanned signatures. Returns false on translation
    // failure (malformed/unsupported bytecode, or scan/link failure); out is left empty in that case.
    bool translate_d3d9_shader_pair(const void* vs_tokens, size_t vs_token_size_bytes, const void* ps_tokens,
                                    size_t ps_token_size_bytes, shader_pair_spirv& out);
}
```

Internal pipeline (real, confirmed sequence — not all chained structs below are mandatory; the ones
omitted use documented safe defaults, see the Data Flow section):
1. `vkd3d_shader_scan()` on the VS tokens (`source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE`, `next`
   chains a `vkd3d_shader_scan_signature_info`) → VS output signature.
2. `vkd3d_shader_scan()` on the PS tokens the same way → PS input signature.
3. `vkd3d_shader_build_varying_map(&vs_output_signature, &ps_input_signature, &varying_count,
   varying_map)` → the VS→PS register mapping.
4. `vkd3d_shader_compile()` for the VS: `source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE`,
   `target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY`, `next` chains
   `vkd3d_shader_varying_map_info` (built in step 3) → `vkd3d_shader_spirv_target_info`.
5. `vkd3d_shader_compile()` for the PS the same way, but with `next` chaining straight to
   `vkd3d_shader_spirv_target_info` (no varying map needed on the consuming side).
6. Free everything: `vkd3d_shader_free_scan_signature_info()` (x2), `vkd3d_shader_free_shader_code()`
   (x2 on success), `vkd3d_shader_free_messages()` (whenever non-null, success or failure).

`vkd3d_shader_interface_info` (descriptor bindings) and `vkd3d_shader_d3dbc_source_info` (texture
dimensions) are both optional with documented safe defaults (sequential binding-0 mapping; 2D texture
dimension) and are omitted entirely — this slice has no textures or constant buffers to describe.

### 3. `d3d9_host.cpp`/`.hpp` changes

**`create_vertex_shader`/`create_pixel_shader` are unchanged** (corrected 2026-07-03): since
translation now requires the VS and PS *together* (they're scanned and linked as a pair, see above),
translating at creation time is no longer possible — the app creates each shader independently, often
long before the other half of the pair exists or is bound. `create_vertex_shader`/`create_pixel_shader`
keep stashing raw tokens into `shader_entry{tokens}` exactly as they do today; no new field, no
behavior change.

**Translation happens lazily, at first draw with both shaders bound**, inside the new programmable
pipeline path. `ensure_pipeline` currently builds and caches exactly one pipeline (the hardcoded FF
pair, `pipeline_`/`pipeline_ready_`). Add a second cache keyed by `(vertex_shader_id, pixel_shader_id)`
→ `{uint64_t vs_module; uint64_t fs_module; uint64_t pipeline;}` (an `std::unordered_map`, not a single
slot — trivial to key correctly since the shader ids are already stable, and avoids re-translating on
every draw once a pair has been seen once). On a cache miss for the currently-bound
`(state_.vertex_shader, state_.pixel_shader)` pair, look up both `shader_entry::tokens` and call
`translate_d3d9_shader_pair`; on success, create two shader modules via `vulkan_host::create_shader_module`
and one pipeline via `create_graphics_pipeline`, then cache the triplet. On any failure (translation or
pipeline creation), do not cache a bad entry — fall through to "no draw" (`d3d_ok`, silently degrade)
the same way the rest of `d3d9_host` already handles GPU-unavailable conditions.

**Selection in `execute_draw`:** if `state_.vertex_shader != 0 && state_.pixel_shader != 0`, use the
programmable pipeline/modules; otherwise fall back to the existing FF path unchanged. This is an
`if`/`else` at the top of the pipeline-selection logic, not a rewrite of `execute_draw`'s existing
vertex-upload/barrier/draw/readback sequence, which is shader-path-agnostic already.

**No constant buffer in this slice.** Per the brainstorming decision ("position + color passthrough
only," not the "include one float constant register" alternative), the test shader reads no `c#`
registers, so there is no UBO, no descriptor set, and `state_.vs_const_f`/`ps_const_f` stay unread —
same as today. The full binding contract (constant UBO at a fixed binding, textures/samplers) is
deferred to a follow-on slice; this one is scoped to proving translation + a real, working
(descriptor-set-free) pipeline. `create_graphics_pipeline`'s pipeline layout for the programmable path
therefore needs no descriptor set layout at all — simpler than the FF path's own push-constant layout,
not more.

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

Marshaling shape (corrected 2026-07-03): `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`D3DDDIARG_CREATEPIXELSHADERFUNC`
**already exist** in `d3d9_ddi.hpp` (added during the original Stage 2 Part 2 DDI scaffolding, lines
361-371) — no new struct definitions needed. Neither has been RE-verified against a live forcing
function yet (unlike `D3DDDIARG_LOCK`/`PRESENT`, which were), so Task 5 still applies this session's
"verify empirically, don't trust blindly" methodology, just as a confirmation pass on an existing
struct rather than authoring a new one:
```c
typedef struct _D3DDDIARG_CREATEVERTEXSHADERFUNC { HANDLE ShaderHandle; UINT Values[1]; } D3DDDIARG_CREATEVERTEXSHADERFUNC;
typedef struct _D3DDDIARG_CREATEPIXELSHADERFUNC { HANDLE ShaderHandle; UINT CodeSize; } D3DDDIARG_CREATEPIXELSHADERFUNC;
```
`ShaderHandle` is an out-parameter; `Values[0]`/`CodeSize` is the token blob size in bytes, with the
token DWORDs themselves following the struct in memory — the same trailing-payload convention already
proven correct for `D3DDDIARG_SETVERTEXSHADERCONST` (`umd_SetVertexShaderConst`, reads
`reinterpret_cast<const float*>(pArgs + 1)`), so this is a well-precedented shape, not a blind guess —
still confirmed live (not assumed) before being trusted, per Task 5.
`D3DDDIARG_DELETEVERTEXSHADERFUNC` does **not** yet exist and needs a trivial one-field definition
(`{HANDLE ShaderHandle;}`), matching the existing `D3DDDIARG_DELETEPIXELSHADERFUNC`.
The wire side needs no new opcodes or structs at all: `ioctl_d3d9_create_vertex_shader`/
`ioctl_d3d9_create_pixel_shader` and `d3d9_cmd::create_shader_request`/`create_shader_response` are
already fully wired end-to-end on the host side (`gpu_bridge.cpp`'s `handle_d3d9_create_vertex_shader`/
`handle_d3d9_create_pixel_shader` already call `d3d9_host::create_vertex_shader`/`create_pixel_shader`)
— only the UMD-side marshaling functions (`umd_CreateVertexShaderFunc`/`umd_CreatePixelShaderFunc`)
and their `bridge_call` sites are missing.

**New guest test** (`src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp`, compiled to
`d3d9-shader-test.exe` via the same standalone mingw recipe as the existing FF test) — deliberately
**separate** from `d3d9_triangle_test.cpp` so the FF triangle stays a working regression baseline
throughout this slice's development, not modified in place. Same window/device/render-target setup as
the FF test; the difference is `CreateVertexShader`/`CreatePixelShader` with real SM2 bytecode instead
of `SetFVF`. **The bytecode comes from `D3DCompile()`** (declared in mingw-w64's own
`d3dcompiler.h`/`libd3dcompiler_43.a` import stub, calling the real `d3dcompiler_43.dll` already
staged at `root/filesys/c/windows/system32/d3dcompiler_43.dll`) compiling a tiny HLSL source string
embedded in the test, at guest runtime — not hand-encoded token DWORDs, avoiding the real risk of
getting the D3D9 shader opcode/parameter-token encoding wrong from memory. To sidestep replicating
`D3DFVF_XYZRHW`'s screen-space-to-clip-space semantics inside a programmable vertex shader (a
different, more involved transform than the fixed-function pipeline's own pretransformed-vertex
handling), this test uses a plain (non-`RHW`) vertex format with position authored directly in clip
space (`[-1, 1]` range) and a diffuse color, so the HLSL is pure passthrough: `output.pos =
input.pos; output.color = input.color;` for the vertex shader, `return input.color;` for the pixel
shader. Simpler than the FF shader pair, not equivalent to it — this slice tests translation
correctness, not XYZRHW-transform equivalence.

---

## Data Flow (one draw call, programmable path)

1. Guest: `D3DCompile(hlslSource, ..., &vsBlob, ...)` (real `d3dcompiler_43.dll`) produces real SM2
   vertex shader bytecode; same for the pixel shader. Pure host-Windows-API calls, no sogen
   involvement yet.
2. Guest: `CreateVertexShader(vsBlob->GetBufferPointer(), &vs)` → `umd_CreateVertexShaderFunc`
   marshals the token blob → `ioctl_d3d9_create_vertex_shader` → `d3d9_host::create_vertex_shader`
   → tokens stashed in `shader_entry`, wire shader id returned, **no translation yet** (corrected
   2026-07-03 — translation needs both shaders together, see Architecture §3). Same for
   `CreatePixelShader`.
3. Guest: `SetVertexShader(vs)`/`SetPixelShader(ps)` → already-wired `umd_SetVertexShaderFunc`/
   `umd_SetPixelShader` → `state_.vertex_shader`/`pixel_shader` updated (existing code path,
   unchanged).
4. Guest: `DrawPrimitive(...)` → `execute_draw` → sees both shaders bound → `ensure_pipeline`'s
   programmable branch: on cache miss for `(state_.vertex_shader, state_.pixel_shader)`, calls
   `translate_d3d9_shader_pair` (real vkd3d-shader scan+link+compile call) on both `shader_entry`
   token blobs, then `vulkan_host::create_shader_module` x2 and `create_graphics_pipeline`, caches the
   triplet — build-or-reuse cached programmable pipeline, no descriptor sets → bind pipeline + vertex
   buffer → draw → readback into the render target's backing (existing pattern, unchanged).
5. Host-side temporary diagnostic (same pattern as the FF triangle's verification, removed after
   confirming): read back the centroid pixel, compare against the hand-computed barycentric color
   average for this test's own vertex colors (same technique as the FF triangle's verification, not
   necessarily the same numeric expected value, since this test's vertex positions/colors don't have
   to numerically match the FF test's).

---

## Error Handling

- Translation failure (`translate_d3d9_shader_pair` returns false, discovered lazily at first draw
  with both shaders bound, not at `CreateVertexShader`/`CreatePixelShader` time — corrected
  2026-07-03): `ensure_pipeline`'s programmable branch does not cache an entry and `execute_draw`
  degrades to a no-op draw (`d3d_ok`, matching every other GPU-unavailable path in `d3d9_host`) rather
  than returning a failure `HRESULT` to the app — `DrawPrimitive` itself has already returned `S_OK`
  to the guest by the time this is discovered (the draw is asynchronous from the app's perspective,
  recorded and flushed later), so there is no app-visible `HRESULT` slot left to fail through.
- Draw with one shader bound but not the other (e.g. VS set, PS still fixed-function/null): falls
  back to the existing FF path (the `&&` condition in the selection logic) rather than attempting a
  mixed programmable/fixed-function pipeline, which real D3D9 doesn't support either (VS+PS are
  bound as a pair for the programmable pipeline in real hardware terms).
- `D3DCompile()` failure (malformed HLSL, wrong target profile string, etc.): surfaces as a normal
  `HRESULT` failure at the D3D9-API level inside the guest test itself, before any sogen-specific code
  runs — the same as it would for any real D3D9 app; no special handling needed on sogen's side.

---

## Testing / Verification

Same analytic host-side pixel-readback pattern as the FF triangle (no new test infrastructure):
1. Build `d3d9-shader-test.exe`, stage it into `root/filesys/c/`.
2. `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-shader-test.exe`.
3. Temporary host-side diagnostic (added to `d3d9_host.cpp`'s draw-readback path, removed once
   confirmed) prints the centroid pixel; compare against the hand-computed expected barycentric color
   average for this test's own three vertex colors (same verification technique as the FF triangle,
   computed fresh for this test's own vertex data).
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
- `deps/CMakeLists.txt` — new `ExternalProject_Add` target for `libvkd3d-shader` (autotools: `autogen.sh`/`configure`/`make`).
- `src/windows-emulator/CMakeLists.txt` (or wherever `windows-emulator`'s target sources/link
  libraries are declared) — add the new translator `.cpp`, link `libvkd3d-shader`.
- `src/windows-emulator/devices/d3d9_host.hpp` — new private `std::unordered_map` cache keyed by
  `(vertex_shader_id, pixel_shader_id)` for the programmable pipeline (module ids, pipeline id); no
  change to `shader_entry` (it stays token-only).
- `src/windows-emulator/devices/d3d9_host.cpp` — `ensure_pipeline` gains the programmable branch
  (lazy pair-translate-and-cache on first draw with both shaders bound); `execute_draw` gains the
  FF-vs-programmable pipeline selection. `create_vertex_shader`/`create_pixel_shader` are unchanged.
- `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp` — new `umd_CreateVertexShaderFunc`/
  `umd_DeleteVertexShaderFunc`/`umd_CreatePixelShader`/`umd_DeletePixelShader`, wired at slots
  42/43/67/68.
- `src/samples/sogen-d3d9-umd/d3d9_ddi.hpp` — new `D3DDDIARG_DELETEVERTEXSHADERFUNC` (trivial,
  `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`CREATEPIXELSHADERFUNC` already exist and only need live
  RE-verification, not authoring — do not trust the existing, never-exercised layout blind, matching
  this session's established methodology (the `D3DDDIARG_LOCK` struct guessed from WDK-adjacent docs
  turned out wrong; live verification is what actually worked).
- `README.md`/`CLAUDE.md` — document `perl`/`flex`/`bison` (autotools build deps for vkd3d-shader;
  all confirmed already present on this Mac) — do not document `meson`, it is not needed for this
  dependency after all.

---

## Open Questions — resolved 2026-07-03 during plan-writing

- ~~Exact vkd3d-shader public API shape~~ — resolved by cloning the real vkd3d repo read-only for
  inspection: `vkd3d_shader_compile()`/`vkd3d_shader_scan()`/`vkd3d_shader_build_varying_map()`, see
  Architecture §2's revised pipeline.
- ~~Exact `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`CREATEPIXELSHADERFUNC` struct layouts~~ — these structs
  already exist in `d3d9_ddi.hpp` (added during Part 2, never exercised). Still requires the Task 5
  live-verification pass (never trust an unexercised struct), but authoring is not needed.
- ~~vkd3d submodule's exact upstream URL~~ — resolved: `https://gitlab.winehq.org/wine/vkd3d.git`,
  confirmed reachable and cloneable.
- ~~Build system (meson vs. something else)~~ — resolved: autotools, not meson (see Architecture §1).
