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
```

`d3d9_shader_test.cpp` is a guest-runtime test, not a driver-side file, so it does not need the
`-I../../d3d9-command-protocol -I../../gpu-bridge-protocol` include paths the UMD build above
requires; it only talks to `d3d9.dll`/`d3dcompiler_43.dll` through the public D3D9 API.

## Stage

```bash
cp sogen_d3d9um-x64.dll <root>/filesys/c/windows/system32/sogen_d3d9um.dll
cp d3d9-spike-test-x64.exe <root>/filesys/c/d3d9-spike-test.exe
cp d3d9-shader-test-x64.exe <root>/filesys/c/d3d9-shader-test.exe
```

`<root>` is the emulated filesystem passed to the analyzer via `-e`; the real 64-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/system32/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/system32/d3dcompiler_43.dll` for the
shader test.

## Run

```bash
./analyzer -e <root> -c c:/d3d9-spike-test.exe
./analyzer -e <root> -c c:/d3d9-shader-test.exe
```

Expect `[d3d9-spike] CreateDevice hr=0x00000000` and `SUCCESS: IDirect3DDevice9 created`.

`d3d9-shader-test.exe` compiles a position+color passthrough VS/PS pair from embedded HLSL via the
real `D3DCompile()` (`vs_2_0`/`ps_2_0`), creates them via `CreateVertexShader`/`CreatePixelShader`,
and draws a `D3DFVF_XYZ|D3DFVF_DIFFUSE` triangle through the programmable pipeline. Expect
`D3DCompile(vs) hr=0x00000000`, `D3DCompile(ps) hr=0x00000000`, `CreateVertexShader hr=0x00000000`,
and `CreatePixelShader hr=0x00000000`.

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
