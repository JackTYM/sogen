# sogen D3D9 UMD spike

Thin Direct3D9 WDDM user-mode driver (`sogen_d3d9_umd.cpp`) that the official Microsoft `d3d9.dll`
loads as the vendor driver, plus a guest test (`d3d9_spike_test.cpp`) that drives it through
`Direct3DCreate9` → `CreateDevice`. Not part of the CMake build — it targets the guest (Windows x64),
built with mingw-w64 and staged into the emulated filesystem.

## Build

```bash
brew install mingw-w64   # x86_64-w64-mingw32-g++, i686-w64-mingw32-g++

x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 -I../../d3d9-command-protocol -I../../gpu-bridge-protocol \
    sogen_d3d9_umd.cpp sogen_d3d9_umd.def \
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_spike_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-spike-test-x64.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_shader_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-shader-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_const_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-const-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-texture-test-x64.exe -ld3d9 -ld3dcompiler_43
```

`d3d9_shader_test.cpp`, `d3d9_const_test.cpp`, and `d3d9_texture_test.cpp` are guest-runtime tests,
not driver-side files, so they do not need the `-I../../d3d9-command-protocol
-I../../gpu-bridge-protocol` include paths the UMD build above requires; they only talk to
`d3d9.dll`/`d3dcompiler_43.dll` through the public D3D9 API.

## Stage

```bash
cp sogen_d3d9um-x64.dll <root>/filesys/c/windows/system32/sogen_d3d9um.dll
cp d3d9-spike-test-x64.exe <root>/filesys/c/d3d9-spike-test.exe
cp d3d9-shader-test-x64.exe <root>/filesys/c/d3d9-shader-test.exe
cp d3d9-const-test-x64.exe <root>/filesys/c/d3d9-const-test.exe
cp d3d9-texture-test-x64.exe <root>/filesys/c/d3d9-texture-test.exe
```

`<root>` is the emulated filesystem passed to the analyzer via `-e`; the real 64-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/system32/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/system32/d3dcompiler_43.dll` for the
shader, const, and texture tests.

## Run

```bash
./analyzer -e <root> -c c:/d3d9-spike-test.exe
./analyzer -e <root> -c c:/d3d9-shader-test.exe
./analyzer -e <root> -c c:/d3d9-const-test.exe
./analyzer -e <root> -c c:/d3d9-texture-test.exe
```

Expect `[d3d9-spike] CreateDevice hr=0x00000000` and `SUCCESS: IDirect3DDevice9 created`.

`d3d9-shader-test.exe` compiles a position+color passthrough VS/PS pair from embedded HLSL via the
real `D3DCompile()` (`vs_2_0`/`ps_2_0`), creates them via `CreateVertexShader`/`CreatePixelShader`,
and draws a `D3DFVF_XYZ|D3DFVF_DIFFUSE` triangle through the programmable pipeline. Expect
`D3DCompile(vs) hr=0x00000000`, `D3DCompile(ps) hr=0x00000000`, `CreateVertexShader hr=0x00000000`,
and `CreatePixelShader hr=0x00000000`.

`d3d9-const-test.exe` proves a D3D9 float constant register round-trips from a guest
`SetPixelShaderConstantF`/`SetVertexShaderConstantF` call, through the wire protocol and UBO/
descriptor-set binding, into a real SPIR-V shader's read of `c#`, into a rendered pixel. The pixel
shader returns a solid color read straight from PS constant register `c0`; the vertex shader applies
a scale-by-0.5 matrix packed across VS constant registers `c1`-`c4` to the vertex position. It draws
to an off-screen render target and `LockRect`s it to check two pixels: one inside the scaled-down
triangle (must exactly match the `c0` color) and one inside the original, unscaled triangle's bounds
but outside the scaled-down triangle (must show the background clear color, proving the VS-side
matrix actually applied). Expect `SetVertexShaderConstantF hr=0x00000000`,
`SetPixelShaderConstantF hr=0x00000000`, and both `PASS:` lines followed by
`[d3d9-const-test] ALL CHECKS PASSED`.

`d3d9-texture-test.exe` proves the M2 milestone's four features -- real textures, indexed draws, real
depth testing, and real alpha blending -- work together in one real render. It draws four quads (one
16-vertex buffer, one 6-index buffer, reused across draws via `DrawIndexedPrimitive`'s
`BaseVertexIndex`) with the same real `D3DCompile()`-produced `vs_2_0`/`ps_2_0` shader pair sampling a
procedural 640x480 texture (four solid quadrants: RED/GREEN/BLUE/WHITE, written via `LockRect`) and
multiplying it by a per-draw PS constant `c0` "tint". Expect `CreateTexture hr=0x00000000`, `Texture
LockRect hr=0x00000000`, all four `DrawIndexedPrimitive(...) hr=0x00000000` lines, three `PASS:` lines,
and `[d3d9-texture-test] ALL CHECKS PASSED`:
- **Textured quad**: samples the RED quadrant with tint `(1,1,1,1)` -- checks the pixel matches the
  known texture color exactly.
- **Depth pair**: a near quad (z=0.2, greenish tint) drawn first, then a farther, fully overlapping
  quad (z=0.8, reddish tint) drawn second -- checks the overlap pixel is the near quad's color, which
  only happens with a real depth test (without one, the farther quad, drawn later, would simply
  overwrite it).
- **Blend quad**: a semi-transparent green (tint `(0,1,0,0.5)`) drawn over the plain clear-color
  background -- checks the pixel matches the analytically-computed `SRCALPHA`/`INVSRCALPHA`/`ADD`
  blend exactly.

## Notes

- `d3d9_ddi.hpp` is a clean hand-transcription of the WDK `d3dumddi.h` DDI subset needed for
  adapter negotiation (mingw-w64 doesn't ship the WDK header). Do not add the real
  Microsoft-copyrighted `d3dumddi.h` to this directory.
- `OpenAdapter` reports `DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION` (our own implemented
  interface version), not the runtime's offered `Version` — echoing the runtime's value makes it
  validate device-func slots beyond what our `D3DDDI_DEVICEFUNCS` table declares.
- `fill_d3d9caps` reports real SM2.0 shader support (`VertexShaderVersion = D3DVS_VERSION(2, 0)`,
  `PixelShaderVersion = D3DPS_VERSION(2, 0)`). This re-triggers an internal, undocumented
  `d3d9.dll` HAL-enable validator (found via `objdump` disassembly, live-confirmed via sogen's
  Python debugger API) that runs once VS2.0+ is declared and additionally requires:
  `PrimitiveMiscCaps` bit `0x2000` plus `D3DPMISCCAPS_MASKZ`; `RasterCaps` to include
  `D3DPRASTERCAPS_FOGVERTEX`; `Src`/`DestBlendCaps` to include `D3DPBLENDCAPS_BLENDFACTOR`; and
  `GuardBand{Left,Top,Right,Bottom}` to each satisfy `abs(value) >= 8192.0`. `fill_d3d9caps` now
  sets all of these, and `CreateDevice`/`GetDeviceCaps`/`GetCaps` succeed with real SM2.0 caps
  reported.
- With the HAL-enable gate satisfied, declaring VS2.0+ made `DrawPrimitive` start returning
  `E_OUTOFMEMORY` for every draw, fixed-function or shader-bound alike (`CD3DBase::DrawPrimitive`'s
  shared state-flush block calls into a per-draw shader-cache resolution path regardless of which
  path drew). Root-caused to two DDI calling-convention bugs: `pfnCreateVertexShaderFunc`/
  `pfnCreatePixelShader` are struct-pointer calls (`D3DDDIARG_CREATESHADERFUNC*`, with the
  driver-written `ShaderHandle` at offset 8, not offset 0 as first assumed), and
  `pfnSetPixelShader`/`pfnSetVertexShaderFunc` are direct-value `HANDLE` calls, not struct-pointer.
  Both are now fixed; see `HANDOFF_MACBOOK.md` §15 for the full RE trace.
- With both fixes in place, `d3d9_triangle_test.cpp` (fixed-function) and `d3d9_shader_test.cpp`
  (programmable, real `D3DCompile()`-produced `vs_2_0`/`ps_2_0` shaders) both get
  `DrawPrimitive hr=0x00000000` and `Present hr=0x00000000`. The shader test's rendered pixel was
  verified against the hand-computed barycentric blend of the triangle's vertex colors, confirming
  real SM2 shader translation end to end.
- x64-only bring-up; the x86/WoW64 UMD needs typed `__stdcall` thunks per device-func slot instead
  of the generic caller-cleanup stub.
- `pfnSetVertexShaderConst`/`pfnSetPixelShaderConst` take the float array as a separate third DDI
  argument (`CONST FLOAT*`), not trailing bytes after the `{Register, Count}` header -- the same
  header-plus-separate-array-pointer shape already used by `pfnClear`. A first attempt assumed the
  floats trailed the header in memory; this silently read zeroed/garbage stack data instead of the
  real constant values, invisible until `d3d9-const-test.exe` round-tripped a distinctive non-zero
  constant end to end (the runtime's own automatic shadow-init calls are always zero, so this bug
  never surfaced with zero data). `umd_SetVertexShaderConst`/`umd_SetPixelShaderConst` now take the
  data pointer as their own third parameter.
- **Index-buffer `Lock()` round-trip, RE'd and fixed (2026-07-04, see `HANDOFF_MACBOOK.md` for the
  full live-RE trace).** Two real, independent bugs, both confirmed via live GDB/Python-hook tracing
  against the real staged `d3d9.dll`:
  - Index buffers (unlike vertex buffers) routed through `CreateSysmemIndexBuffer` instead of
    `CreateDriverIndexBuffer`, because `CIndexBuffer::Create`'s own routing check reads a *different*
    DevCaps bit (`0x04000000`) than `CVertexBuffer::Create`'s (`0x02000000`, already set). Without it,
    an index buffer's `Lock()`/`Unlock()` still call `pfnLock`/`pfnUnlock` (confirmed: always
    `hr=S_OK`) but purely as vestigial bookkeeping -- the app-visible pointer comes from the runtime's
    own pre-allocated system-memory shadow, never from anything the driver returns. `fill_d3d9caps`
    now also sets this bit.
  - `D3DDDIARG_LOCK`'s `OffsetToLock`/`SizeToLock` do not have a single, routing-path-independent
    struct offset -- two different real `d3d9.dll` code paths build this struct differently (see
    `d3d9_ddi.hpp`'s own comment for the full byte-level capture). `umd_Lock`/`umd_Unlock` now always
    treat every lock as an implicit whole-buffer lock (offset 0, size unknown) instead of trying to
    read either field, which both sidesteps the ambiguity and is what actually fixes the round-trip.
    **This is a permanent limitation, not a stopgap**: partial-range locks (e.g. the common
    `D3DLOCK_NOOVERWRITE` growing-buffer pattern) silently get whole-buffer semantics instead of an
    error -- a real game relying on partial locks will see incorrect behavior. See `HANDOFF_MACBOOK.md`
    §16.1 for what real per-path fix would require.
- **Depth-stencil surface resource-id resolution, RE'd and fixed (2026-07-04).** `CreateDepthStencilSurface`
  does call `pfnCreateResource` (confirmed: `Format=75`/`D3DFMT_D24S8` correctly captured), but its
  DDI handle does not reach `pfnSetDepthStencil`'s `hZBuffer` as the same value -- `SetDepthStencil`
  is itself only dispatched once a real `Clear()`/draw actually references the bound Z-buffer (the
  same worker-thread DP2-batch deferral this file already documents for other state), and `hZBuffer`
  is an unrelated small handle. `resolve_resource_id`'s generic lazy-bind fallback (640x480
  X8R8G8B8 RENDERTARGET) previously minted a wrong-shaped resource for it. Fixed with a dedicated
  `resolve_depth_stencil_resource_id` (D3DFMT_D24S8 + `D3DUSAGE_DEPTHSTENCIL`), mirroring
  `resolve_buffer_resource_id`'s existing pattern.
- **A real `pfnCreateResource`/lazy-bind namespace collision, found while fixing the above.**
  `umd_CreateResource` never recorded its own output handles anywhere `resolve_resource_id`/
  `resolve_buffer_resource_id` would check, so Lock()/Unlock() on any *real*, `pfnCreateResource`-backed
  resource (confirmed for plain textures) silently fell into the wrong lazy-bind branch and minted a
  second, unrelated, wrong-shaped resource -- the app's real pixel writes landed in a resource nothing
  else ever read. Fixed with a dedicated `g_created_resource_ids` map, kept deliberately separate from
  `resolve_buffer_resource_id`'s own lazy-bind cache (`g_resource_ids`): merging them caused a second,
  independently-confirmed collision, since `d3d9_host::allocate_id()`'s sequential counter and the
  runtime's own small-integer internal vertex/index-buffer handles are different, unrelated numbering
  spaces that can coincide numerically. `allocate_id()` now starts at `1ULL << 32` so a real resource id
  can never collide with one of those handles again. `CreateVertexBuffer`/`CreateIndexBuffer` were also
  found to call `pfnCreateResource` after all, with the internal-only formats `D3DFMT_VERTEXDATA` (100)
  and `D3DFMT_INDEX16`/`D3DFMT_INDEX32` (101/102) -- `umd_CreateResource` excludes exactly these three
  formats from the new registry so those handles keep resolving through the correctly-shaped buffer
  lazy-bind instead.
- **`ensure_programmable_pipeline`'s vertex layout was hardcoded** to the one shape
  `d3d9_const_test.cpp`/`d3d9_shader_test.cpp` use (`D3DFVF_XYZ|D3DFVF_DIFFUSE`, 16-byte stride). Since
  there is no `pfnCreateVertexShaderDecl` wiring yet to learn a real vertex declaration, the host now
  distinguishes the one other shape this milestone needs by `SetStreamSource`'s `Stride` (20 bytes for
  `D3DFVF_XYZ|D3DFVF_TEX1`) -- a value the wire protocol already carried but previously discarded.
- **A real, still-open bug in `TEXCOORD0` varying interpolation**, found and worked around while
  building `d3d9_texture_test.cpp`: with a genuine `D3DFVF_XYZ|D3DFVF_TEX1` vertex format and a
  `float2`/`float4` `TEXCOORD0` output, a quad's U value interpolated correctly across screen space but
  V consistently did not (e.g. an expected 0.25 read back around 0.87). Isolated to the varying itself
  via a visualize-the-interpolant diagnostic shader (independently ruled out: geometry, the texture,
  the sampler descriptor, and the PS constant register all traced as correct). Not root-caused further
  within this session's budget -- `d3d9_texture_test.cpp` instead packs UV into the vertex's
  `D3DFVF_DIFFUSE` color channel (`COLOR0.rg`), which is proven correct by every prior guest test that
  passes a `COLOR0` varying through a real compiled shader.
- **A `D3DPOOL_MANAGED` texture creates two, unrelated DDI resources for what the app sees as one
  `CreateTexture()` call** -- confirmed live: `pfnCreateResource(Format=21/A8R8G8B8)` fires twice with
  two different output handles, one that `LockRect()` later uses (and correctly receives the app's
  pixel writes) and a different one that `SetTexture()` uses (which stays empty, so the sampled texture
  reads as black/transparent). Not root-caused or fixed -- `d3d9_texture_test.cpp` uses
  `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT` instead, which was confirmed live to issue only one
  `pfnCreateResource` call.
