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

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_managed_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-managed-texture-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_texcoord_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-texcoord-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -shared -O2 -std=c++20 -I../../d3d9-command-protocol -I../../gpu-bridge-protocol \
    sogen_d3d9_umd.cpp sogen_d3d9_umd.def \
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x86.dll

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_triangle_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-triangle-test-x86.exe -ld3d9

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_shader_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-shader-test-x86.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_const_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-const-test-x86.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-texture-test-x86.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_texcoord_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-texcoord-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_partial_lock_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-partial-lock-test-x64.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_int_bool_const_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-int-bool-const-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_int_bool_const_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-int-bool-const-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_scissor_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-scissor-test-x64.exe -ld3d9
```

`d3d9_shader_test.cpp`, `d3d9_const_test.cpp`, `d3d9_texture_test.cpp`, `d3d9_texcoord_test.cpp`, and
`d3d9_int_bool_const_test.cpp` are guest-runtime tests, not driver-side files, so they do not need the
`-I../../d3d9-command-protocol -I../../gpu-bridge-protocol` include paths the UMD build above
requires; they only talk to `d3d9.dll`/`d3dcompiler_43.dll` through the public D3D9 API. The same
applies to `d3d9_triangle_test.cpp` above -- it's a guest-runtime test too. `d3d9_scissor_test.cpp` is
the same kind of guest-runtime test and, like `d3d9_partial_lock_test.cpp`, needs no `d3dcompiler_43`
link either -- it's fixed-function-only (`D3DFVF_XYZRHW|D3DFVF_DIFFUSE`), no shader compile involved.

## Stage

```bash
cp sogen_d3d9um-x64.dll <root>/filesys/c/windows/system32/sogen_d3d9um.dll
cp d3d9-spike-test-x64.exe <root>/filesys/c/d3d9-spike-test.exe
cp d3d9-shader-test-x64.exe <root>/filesys/c/d3d9-shader-test.exe
cp d3d9-const-test-x64.exe <root>/filesys/c/d3d9-const-test.exe
cp d3d9-texture-test-x64.exe <root>/filesys/c/d3d9-texture-test.exe
cp d3d9-managed-texture-test-x64.exe <root>/filesys/c/d3d9-managed-texture-test.exe
cp d3d9-texcoord-test-x64.exe <root>/filesys/c/d3d9-texcoord-test.exe
cp d3d9-partial-lock-test-x64.exe <root>/filesys/c/d3d9-partial-lock-test.exe
cp d3d9-int-bool-const-test-x64.exe <root>/filesys/c/d3d9-int-bool-const-test.exe
cp d3d9-scissor-test-x64.exe <root>/filesys/c/d3d9-scissor-test.exe
cp sogen_d3d9um-x86.dll <root>/filesys/c/windows/syswow64/sogen_d3d9um.dll
cp d3d9-triangle-test-x86.exe <root>/filesys/c/d3d9-triangle-test-x86.exe
cp d3d9-shader-test-x86.exe <root>/filesys/c/d3d9-shader-test-x86.exe
cp d3d9-const-test-x86.exe <root>/filesys/c/d3d9-const-test-x86.exe
cp d3d9-texture-test-x86.exe <root>/filesys/c/d3d9-texture-test-x86.exe
cp d3d9-texcoord-test-x86.exe <root>/filesys/c/d3d9-texcoord-test-x86.exe
cp d3d9-int-bool-const-test-x86.exe <root>/filesys/c/d3d9-int-bool-const-test-x86.exe
```

`<root>` is the emulated filesystem passed to the analyzer via `-e`; the real 64-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/system32/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/system32/d3dcompiler_43.dll` for the
shader, const, texture, and int-bool-const tests. For the x86/WoW64 UMD, the real 32-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/syswow64/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/syswow64/d3dcompiler_43.dll` for the x86
shader, const, texture, texcoord, and int-bool-const tests.

## Run

```bash
./analyzer -e <root> -c c:/d3d9-spike-test.exe
./analyzer -e <root> -c c:/d3d9-shader-test.exe
./analyzer -e <root> -c c:/d3d9-const-test.exe
./analyzer -e <root> -c c:/d3d9-texture-test.exe
./analyzer -e <root> -c c:/d3d9-managed-texture-test.exe
./analyzer -e <root> -c c:/d3d9-texcoord-test.exe
./analyzer -e <root> -c c:/d3d9-int-bool-const-test.exe
./analyzer -e <root> -c c:/d3d9-scissor-test.exe
./analyzer -e <root> -c c:/d3d9-shader-test-x86.exe
./analyzer -e <root> -c c:/d3d9-const-test-x86.exe
./analyzer -e <root> -c c:/d3d9-texture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-texcoord-test-x86.exe
./analyzer -e <root> -c c:/d3d9-int-bool-const-test-x86.exe
./analyzer -e <root> -c c:/d3d9-partial-lock-test.exe
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

`d3d9-texcoord-test.exe` proves a genuine `TEXCOORD0` varying (`D3DFVF_XYZ|D3DFVF_TEX1`, not
`d3d9_texture_test.cpp`'s `D3DFVF_DIFFUSE`-packed UV workaround) samples a texture correctly via a real
`tex2D()` call. One quad samples the same four-quadrant procedural texture at all four UV combinations
(`u,v` = `0.25/0.75` each) -- a swapped or one-axis-broken interpolant would land in the wrong quadrant
for at least one of these. Expect `CreateTexture hr=0x00000000`, `DrawIndexedPrimitive hr=0x00000000`,
four `PASS:` lines, and `[d3d9-texcoord-test] ALL CHECKS PASSED`. See this test's own header comment
and `docs/d3d9-roadmap.md`/`HANDOFF_MACBOOK.md` §20 for the investigation that closed out the
previously-suspected `TEXCOORD0` interpolation bug (it did not reproduce; no host fix was needed).

`d3d9-int-bool-const-test.exe` proves the D3D9 int (`i#`) and bool (`b#`) shader constant registers
round-trip from a guest `SetVertexShaderConstantI`/`SetVertexShaderConstantB` call, through the wire
protocol and the int/bool CBV descriptor bindings (VS set 0 / PS set 1, bindings 2 and 3), into REAL
runtime shader flow control -- not just raw byte delivery, and that the host's 16-byte-per-register
stride is correct at a NONZERO start register, not just register 0. The vertex shader has two genuine
bool branches (`if (useAltColor)` then `if (useAltColor2)`, every arm an early `return` so the compiler
can't flatten them into arithmetic) driven by `SetVertexShaderConstantB(0, FALSE, 1)` and
`SetVertexShaderConstantB(1, TRUE, 1)`, and a genuine register-bound loop (`for (k < loopTripCount.x)`,
driven by `SetVertexShaderConstantI(1, ...)`) that accumulates a per-iteration delta into the blue
channel. The compiled `vs_2_0` bytecode is walked as raw D3DBC tokens to confirm the compiler actually
emitted a real `REP` opcode reading register `i1` (not unrolled) and two real `IF` instructions reading
the `b0` and `b1` CONSTBOOL registers, not just that the HRESULTs came back clean. Expect
`SetVertexShaderConstantB(0)`/`SetVertexShaderConstantB(1)`/`SetVertexShaderConstantI(1)`
`hr=0x00000000`, `saw_REP/ENDREP=yes saw_IF(b0)=yes saw_IF(b1)=yes` in the bytecode scan, both `PASS:`
lines, and `[d3d9-int-bool-const-test] ALL CHECKS PASSED`. See this test's own header comment for the
three d3dcompiler_43 quirks its exact shader shape works around (and why b0/b1 and i1 are exercised
together), and `docs/d3d9-roadmap.md`/`HANDOFF_MACBOOK.md` §22 for the full design and RE narrative
(int/bool CBV binding scheme, the missing IOCTL-dispatch-routing bug this test caught, and the x64/x86
parity results).

`d3d9-scissor-test.exe` proves real `D3DRS_SCISSORTESTENABLE` + `SetScissorRect` clipping works,
using the fixed-function `D3DFVF_XYZRHW|D3DFVF_DIFFUSE` path (no shader needed). On a 640x480
off-screen render target cleared BLUE (sized to match `pfnCreateResource`'s hardcoded 640x480 KNOWN
LIMITATION, same as `d3d9_texture_test.cpp`), it enables `D3DRS_SCISSORTESTENABLE`, sets the scissor
rect to the center third (`{213,160,427,320}`), and draws a full-screen RED quad -- the center pixel
must be RED (inside the rect) while two corner pixels near `(10,10)` and `(630,470)` must stay BLUE
(outside it). This is the discriminator: before this rect was wired up, `execute_draw` unconditionally
forced a full-render-target-extent scissor regardless of app state, so the whole RT would have come
back RED and the corner checks would fail. A second sub-pass disables `D3DRS_SCISSORTESTENABLE`,
re-clears the RT, and redraws the same quad -- all three checkpoints must now be RED, proving the
default/no-scissor path still fills the whole RT (regression safety for the common case). Expect
`SetRenderState(SCISSORTESTENABLE, TRUE/FALSE) hr=0x00000000`, both `DrawIndexedPrimitive(...)
hr=0x00000000` lines, six `PASS:` lines, and `[d3d9-scissor-test] ALL CHECKS PASSED`.

`d3d9-partial-lock-test.exe` proves a real `D3DLOCK_NOOVERWRITE`-style partial lock on a growing
dynamic vertex buffer only touches the sub-range it requested. It fills a 256-byte chunk with a
`D3DLOCK_DISCARD` lock, then appends two more 256-byte chunks at increasing nonzero offsets with
`D3DLOCK_NOOVERWRITE`, each with a distinctive byte pattern; a final whole-buffer read-only Lock
checks all three chunks still hold exactly their own pattern -- in particular that the first chunk
(the "untouched earlier region") was not disturbed by the later, higher-offset locks. Expect three
`PASS:` lines and `[d3d9-partial-lock-test] ALL CHECKS PASSED`. See the note below (Task 6,
2026-07-04) for what this fixes and how -- previously this class of lock silently got whole-buffer
semantics, which would have corrupted chunk 0 and failed this exact test.

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
- The x86/WoW64 UMD is built with typed `__stdcall` thunks per device-func slot (28 real
  implementations plus a `stub_args_N` per distinct arity for the other 115, keyed by a 143-entry
  lookup table) instead of x64's generic zero-arg caller-cleanup stub, since x86 `__stdcall` is
  callee-cleanup and would desync the stack otherwise. Proven end-to-end: `d3d9-triangle-test-x86.exe`
  reaches the x86 UMD through real WoW64 against the genuine 32-bit Microsoft `d3d9.dll` and reads back
  the same pixel as the x64 triangle test. `d3d9-shader-test-x86.exe` (position+color passthrough
  VS/PS pair, real `D3DCompile()`-produced `vs_2_0`/`ps_2_0`) also reaches the x86 UMD through real
  WoW64 with all HRESULTs (`D3DCompile` x2, `CreateVertexShader`, `CreatePixelShader`, `DrawPrimitive`,
  `Present`) coming back `0x00000000`, proving the shader-create/shader-set DDI slots
  (`pfnCreateVertexShaderFunc`, `pfnCreatePixelShader`, `pfnSetVertexShaderFunc`, `pfnSetPixelShader`)
  and the programmable draw path on x86.
- **`d3d9-const-test-x86.exe` FAILS on real WoW64 -- root-caused and fixed (2026-07-04), and it was NOT
  a constant-register timing/ordering bug.** The pixel-A check (must show the exact PS constant `c0`
  color) failed, reading back the background clear color instead -- the triangle never rasterized at
  all. An earlier pass suspected a worker-thread DP2-batch race around the app's real
  `SetVertexShaderConstantF`/`SetPixelShaderConstantF` calls and an unexplained coalesced
  `{Register=0, Count=5, f0=0.0}` call; re-instrumented host-side logging (real arrival order + full
  per-vector payload, not just the first float) proved that call is a benign, correctly-echoed re-flush
  (its `c1.x`/etc. sub-range carries the app's own already-set values, not zeros) -- byte-identical and
  harmless on both architectures. The real bug: `execute_draw` was silently no-oping on x86 because
  `ensure_programmable_pipeline`'s `shaders_.find()` missed. `d3d9_host::allocate_id()` minted shader ids
  starting at `1ULL << 32` (to avoid colliding with the runtime's own small internal handles), but
  `create_shader_common`'s `pArgs->ShaderHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(
  resp.shader))` round-trips that id through a `HANDLE`, which is only 32 bits wide on an x86 guest --
  silently truncating e.g. `4294967301` to `5`. The app then echoed this truncated value back via
  `SetVertexShaderFunc`/`SetPixelShader`, permanently mismatching `shaders_`'s 64-bit keys. x64's 64-bit
  `HANDLE` never truncated, and `d3d9-shader-test-x86.exe` never caught this either since it only checks
  HRESULTs, not rendered pixels (it was silently exercising this exact same bug the whole time). Fixed by
  starting `next_id_` at `0x10000` instead -- still ~100-300x above the documented "few hundred"
  runtime-handle range, but small enough to survive a 32-bit `HANDLE` round-trip on any guest
  architecture. See `d3d9_host.hpp`'s own comment on `next_id_` for the full account.
- **`d3d9-texture-test-x86.exe`'s `D3DDDIARG_CREATERESOURCE` output-handle offset was wrong on x86,
  RE'd and fixed (2026-07-04).** `umd_CreateResource`'s write-back of the output resource id into
  `D3DDDIARG_CREATERESOURCE` was RE'd and confirmed only against the **x64** struct layout (offset 48).
  On x86, the struct's preceding pointer-sized members are 4 bytes instead of 8, shifting the real
  offset to **44** -- a clean 4-byte shift, not a full reshuffle like `D3DDDIARG_LOCK` needed. Before
  this fix, `Lock()`'s `hResource` never matched the real resource in `g_created_resource_ids`, so
  `resolve_buffer_resource_id` fell through to its generic lazy-bind fallback, minting an undersized
  64 KiB buffer that a real ~1.2 MB texture write then overran, crashing with
  `STATUS_ACCESS_VIOLATION (0xC0000005)`. RE'd via a live sentinel scan (writing a distinct,
  offset-encoding value to every 4-byte-aligned offset 0-48 and reading back which one the next
  `Lock()` call echoed) -- see `umd_CreateResource`'s own comment in `sogen_d3d9_umd.cpp` for the full
  account. This never surfaced on `d3d9-shader-test-x86.exe`/`d3d9-const-test-x86.exe` because neither
  creates a real, non-buffer resource via `pfnCreateResource`. All three of this test's analytic checks
  now pass on x86, matching the x64 test's pixel-exact results for the full M2 feature set (textures,
  indexed draws, depth, blend) together.
- **5 of the 143 device-func-table slots have unverified arities on x86** (`pfnCheckCounter`,
  `pfnSetMarker`, `pfnSetMarkerMode`, `pfnCheckCounterInfo`, `pfnFlush1`) -- a simple triangle-draw
  app never calls them, so their assumed byte counts haven't been checked against the real 32-bit
  `d3d9.dll`. A future guest that hits one of these could still desync the stack; verify via idasql
  against `d3d9_x86.dll.i64` before relying on them.
- **`D3DDDIARG_LOCK`/`D3DDDIARG_UNLOCK` have a genuinely different x86 layout, not just pointer-
  shrunk fields.** The real 32-bit `d3d9.dll` builds an 84-byte struct in `CDriverVertexBuffer::Lock`/
  `CDriverMipSurface::InternalLockRect`, then passes it through an indirect call (a per-device,
  runtime-populated function pointer, invisible to static xref analysis) to `DdLockLH`/`DdUnlockLH`,
  which build their own, separate 48-byte/8-byte struct -- that inner struct, not the outer one, is
  what actually crosses the DDI boundary (`pData` at offset 32, not 40 as the x64-derived first
  attempt assumed). RE'd via live execution trace (sogen's own Python debugger API hooking the real
  call sites), not just decompilation -- see `d3d9_ddi.hpp`'s `D3DDDIARG_LOCK` comment for the full
  method and evidence.
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
    `d3d9_ddi.hpp`'s own comment for the full byte-level capture). At the time of this fix,
    `umd_Lock`/`umd_Unlock` always treated every lock as an implicit whole-buffer lock (offset 0, size
    unknown) instead of trying to read either field, which both sidestepped the ambiguity and is what
    fixed this round-trip bug. **This partial-lock limitation was itself resolved as a follow-up (Task
    6, 2026-07-04, see below) -- it is no longer current.**
- **Partial-buffer `Lock()` support (Task 6, 2026-07-04, see `HANDOFF_MACBOOK.md`/
  `.claude/plans/jazzy-giggling-cloud.md` for the full account).** The whole-buffer-only limitation
  above is fixed: `umd_Lock` now reads `D3DDDIARG_LOCK::OffsetToLock` (offset 80, named for the first
  time -- see `d3d9_ddi.hpp`) and forwards it as the wire protocol's own `lock_request::offset` for any
  resource that isn't already a registered `pfnCreateResource` handle (i.e. every vertex/index buffer;
  texture/render-target/depth-stencil `LockRect` calls are unaffected, since this DDI region means
  Rect/Box input for those, not a byte offset). Real per-call detection of which of the two routing-
  path struct shapes a given Lock used turned out to be unnecessary, not just hard: the "sysmem-routed"
  shape's driver-returned `pData` is discarded by the app regardless of what this driver computes
  (confirmed live, `HANDOFF_MACBOOK.md` #16.1), and both this UMD's DevCaps bits
  (`k_devcaps_driver_managed_pool`/`k_devcaps_driver_managed_index_pool`) are unconditionally set, so
  every `D3DPOOL_DEFAULT` vertex/index buffer -- the common real-world case, including every
  `D3DLOCK_NOOVERWRITE` growing-buffer append -- is always "driver-routed" and this offset is always
  real for it. `SizeToLock` still has no reliable offset in either shape, but none is needed: the wire
  protocol's existing `size=0` convention ("from offset to the end of the resource", already correctly
  implemented host-side in `d3d9_host::lock`/`unlock` before this fix) is exactly the right semantics
  for a tail-append lock, since the app never writes past its own requested range anyway. `g_locked_buffers`
  now holds only `[offset, end)` of the resource per outstanding lock (not the whole thing), and a new
  `g_locked_offsets` map remembers each lock's offset so `umd_Unlock` writes its data back to the same
  place. Proven end-to-end by the new `d3d9-partial-lock-test.exe` (see above): a `D3DLOCK_DISCARD`
  fill of the first chunk survives unmodified after two subsequent `D3DLOCK_NOOVERWRITE` appends at
  higher offsets, each chunk reading back exactly its own distinctive byte pattern -- with the
  pre-fix behavior (offset always treated as 0), the second append would have overwritten the first
  chunk instead. x86 keeps the pre-fix whole-buffer-lock behavior unchanged (its driver-routed
  `OffsetToLock` offset is not yet RE-verified -- see `d3d9_ddi.hpp`'s x86 `D3DDDIARG_LOCK` comment),
  so no x86 test exercises partial locks yet; this is a known, explicitly scoped-out gap, not an
  oversight.
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
- **The suspected `TEXCOORD0` varying-interpolation bug does NOT reproduce (investigated 2026-07-04) --
  no host fix was needed.** Originally found while building `d3d9_texture_test.cpp`: with a genuine
  `D3DFVF_XYZ|D3DFVF_TEX1` vertex format and a `float2`/`float4` `TEXCOORD0` output, a quad's U value
  appeared to interpolate correctly but V did not (e.g. an expected 0.25 read back around 0.87). Against
  the current host, a diagnostic PS (`return float4(input.uv, 0, 1)`) against the exact quad shape and
  exact UV figures originally cited reads back both U and V correctly at every sampled point, and
  `d3d9_texcoord_test.cpp` proves a real `tex2D()` sample through a genuine `TEXCOORD0` varying reads the
  correct texture quadrant at all four UV combinations. The one concrete lead
  (`d3d9_shader_translator.cpp` passing `varying_map_info` to the VS `compile_stage` call but `nullptr`
  to the PS one) was tried both ways -- passing it to the PS call too is confirmed byte-for-byte inert
  (vkd3d-shader's own `ir.c` never applies the transform this struct drives to a pixel shader's output,
  since a PS has no "next stage" to remap for). Most likely (but circumstantial -- the original scratch
  diagnostic no longer exists to re-run directly) explanation: the original report predates
  (or was never re-checked against) this session's separate Y-flip screen-convention bug, found "in the
  new test itself, not the host" while building `d3d9_texture_test.cpp` -- that exact class of bug
  produces a "U fine, V looks wrong" symptom without touching varying interpolation at all.
  `d3d9_texture_test.cpp` keeps its `D3DFVF_DIFFUSE`-packed UV workaround (`COLOR0.rg`) unchanged, since
  it remains an independently-proven-correct path; it's no longer required, just no longer necessary to
  remove. See `docs/d3d9-roadmap.md` and `HANDOFF_MACBOOK.md` §20 for the full investigation.
- **A `D3DPOOL_MANAGED` texture creates two, unrelated DDI resources for what the app sees as one
  `CreateTexture()` call -- confirmed unfixable through this driver's own DDI surface (2026-07-04),
  three decoupled layers, all root-caused.** The double-`pfnCreateResource`/`pfnTexBlt` mechanism itself
  is understood and correctly handled: a real, expected D3D9 MANAGED-pool "sysmem master +
  lazily-created vidmem copy" architecture, with `pfnTexBlt` as the genuine sync call the runtime issues
  between the second `pfnCreateResource` and the following `pfnSetTexture` -- this driver's `pfnTexBlt`
  was previously an unwired no-op stub and is now implemented (`umd_TexBlt`/`d3d9_host::tex_blt`,
  forwarding the source resource's full pixel backing into the destination's). **A second, deeper bug
  sits upstream of it**: `pfnLock`/`pfnUnlock` never carry the app's real pixel writes for a
  `D3DPOOL_MANAGED` texture's sysmem copy at all -- live-confirmed this driver's own `pfnLock` return
  pointer and the app's own `LockRect()` pointer are different addresses, meaning the app actually
  writes into `d3d9.dll`'s own private system-memory allocation, invisible to the driver. Root cause:
  `CBaseDevice::CanDriverManageResource` (the same shape of undocumented capability gate that `DevCaps`
  bits `0x02000000`/`0x04000000` already fixed for vertex/index buffers), which
  `CBaseTexture::CanCreateLightWeight` requires before letting `CMipMap` share one real driver resource
  and route Lock/Unlock through it, is **unconditionally false for any real D3DDDI/WDDM driver** --
  traced live (memory-write watch) all the way back to `d3d9.dll`'s own `QueryLHDDICaps`, which
  unconditionally clears `D3DCAPS2_CANMANAGERESOURCE` after querying the driver regardless of what
  `GetCaps` reports (empirically re-verified by temporarily setting the bit and watching it get stripped
  again live). **A third layer, closing the investigation**: does `pfnTexBlt`'s real argument struct
  carry a sysmem-source pixel pointer that could bypass the broken Lock/Unlock path entirely? A live
  trace plus a full idasql decompile of the real caller (`CD3DDDIDX10::TexBlt`) settled it -- the real,
  now-typed `D3DDDIARG_TEXBLT` struct (see `d3d9_ddi.hpp`) carries only two resource handles, a
  subresource index, a destination point, and a source rect, no pixel-data pointer anywhere, and a full
  live trace of every DDI call across an entire `D3DPOOL_MANAGED` texture test run confirms no other
  call carries pixel bytes either. The real MANAGED-pool sysmem pixel data is structurally never exposed
  to any D3DDDI/WDDM driver through any DDI call for this resource kind -- `d3d9.dll` keeps it entirely
  inside its own private `CMipMap` buffer end to end. No fix is possible through this driver's own DDI
  surface (see `umd_TexBlt`'s own comment in `sogen_d3d9_umd.cpp`, `docs/d3d9-roadmap.md`, and
  `HANDOFF_MACBOOK.md` §17-§19 for the full live-RE trail). `d3d9_managed_texture_test.cpp` (real
  `D3DPOOL_MANAGED`, no workaround) is expected to keep failing for this reason, permanently, until a
  fundamentally different mechanism is found; `d3d9_texture_test.cpp` continues to use
  `D3DUSAGE_DYNAMIC` + `D3DPOOL_DEFAULT` to avoid the whole area.
- **Int (`i#`) / bool (`b#`) shader constant registers, wired end to end and ported to x86 (2026-07-05,
  `jazzy-giggling-cloud.md` Tasks 1-5).** Mirrors the float (`c#`) path: wire protocol opcodes,
  `device_state` storage, and two more UBO/descriptor bindings per set (binding 2 = int CBV, binding 3
  = bool CBV, alongside the existing binding 0 float CBV and PS-only binding 1 sampler) -- still just 2
  descriptor sets total (VS=0, PS=1), not a set per register bank. `d3d9_int_bool_const_test.cpp` proves
  real runtime shader flow control, not just byte delivery: a genuine bool branch compiling to a real
  D3DBC `IF` reading `b0`, and a genuine int-driven loop compiling to a real D3DBC `REP` (both confirmed
  by walking the raw bytecode). Building it caught a real host bug (a missing IOCTL-dispatch-routing case
  for the two new opcodes in `gpu_bridge.cpp`, now fixed) and worked around three real `d3dcompiler_43`
  quirks (not sogen bugs) in the test shader itself: a `bool` variable rejected with an explicit register
  annotation for a vs_2_0 target (must be left auto-allocated); an `if`/`else` that flattens into
  arithmetic unless both arms early-`return`; and a narrower quirk where exact `0.0`/`1.0` literals get
  pulled out via a separate shadow float register, worked around with `0.999`/`0.001` -- see the test's
  own header comment for the full account. **Porting to x86 (Task 5) hit no new architecture bug** --
  unlike the earlier const-test-x86 and texture-test-x86 ports, which each found a genuine x86-only
  DDI/handle bug. The one x86 run that
  initially failed (`pixel(320,240)=B=00 G=FF R=00`, i.e. both the bool and int constants silently read
  back as their unset defaults) was traced to a stale build artifact: `sogen_d3d9um-x86.dll` had not been
  rebuilt since Task 1 added `umd_SetVertexShaderConstI`/`ConstB`/`umd_SetPixelShaderConstI`/`ConstB` to
  the shared `sogen_d3d9_umd.cpp` (the x64 DLL had already been rebuilt and picked up the new DDI slots;
  the x86 DLL predated that rebuild and was still missing them). Rebuilding `sogen_d3d9um-x86.dll` from
  the current source, no code change, produced pixel-exact parity with x64
  (`pixel(320,240)=B=26 G=00 R=FF A=FF`, both analytic checks passing). See `docs/d3d9-roadmap.md` and
  `HANDOFF_MACBOOK.md` §22 for the full design/RE narrative.
