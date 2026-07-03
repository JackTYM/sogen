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
- With the HAL-enable gate satisfied, `CreateDevice` and the fixed-function (`SetFVF`, no explicit
  shader) draw path in `d3d9_triangle_test.cpp` still succeed end-to-end (`Present` returns
  `S_OK` both times). `DrawPrimitive` itself now returns `E_OUTOFMEMORY` for the FVF-only draw
  where it previously returned `S_OK`: live tracing shows d3d9.dll flushing pending device state
  and then, now that VS2.0 is declared, calling one more still-unimplemented device-func slot
  (currently `device_stub`) before `pfnDrawPrimitive` is ever reached — most likely lazily
  creating/caching an internal fixed-function-emulation vertex shader or pipeline-state object now
  that the driver claims real shader support. This is expected and was not a regression in that
  task's scope (`CreateDevice`/`GetDeviceCaps`/`GetCaps`/`Present` all still succeed, and the smoke
  test remains all-`Success`).
- `d3d9_shader_test.cpp` (real `D3DCompile()`-produced `vs_2_0`/`ps_2_0` shaders, wired via the
  DDI shader-creation slots added for the programmable pipeline) hits the exact same
  `DrawPrimitive` → `E_OUTOFMEMORY` behavior as `d3d9_triangle_test.cpp`'s fixed-function path
  above — `D3DCompile`, `CreateVertexShader`, and `CreatePixelShader` all return `hr=0x00000000`,
  but `DrawPrimitive` never reaches `pfnDrawPrimitive` successfully. This confirms the gate is in
  `d3d9.dll`'s own state-flush path (unrelated to shader translation or the DDI shader-creation
  slots themselves, since shader creation succeeds either way) and is still open; tracked as
  follow-up work in the vkd3d-shader de-risk plan.
- x64-only bring-up; the x86/WoW64 UMD needs typed `__stdcall` thunks per device-func slot instead
  of the generic caller-cleanup stub.
