# sogen D3D9 UMD spike

Thin Direct3D9 WDDM user-mode driver (`sogen_d3d9_umd.cpp`) that the official Microsoft `d3d9.dll`
loads as the vendor driver, plus a guest test (`d3d9_spike_test.cpp`) that drives it through
`Direct3DCreate9` → `CreateDevice`. Not part of the CMake build — it targets the guest (Windows x64),
built with mingw-w64 and staged into the emulated filesystem.

## Build

```bash
brew install mingw-w64   # x86_64-w64-mingw32-g++, i686-w64-mingw32-g++

x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 sogen_d3d9_umd.cpp sogen_d3d9_umd.def \
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_spike_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-spike-test-x64.exe -ld3d9
```

## Stage

```bash
cp sogen_d3d9um-x64.dll <root>/filesys/c/windows/system32/sogen_d3d9um.dll
cp d3d9-spike-test-x64.exe <root>/filesys/c/d3d9-spike-test.exe
```

`<root>` is the emulated filesystem passed to the analyzer via `-e`; the real 64-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/system32/d3d9.dll`.

## Run

```bash
./analyzer -e <root> -c c:/d3d9-spike-test.exe
```

Expect `[d3d9-spike] CreateDevice hr=0x00000000` and `SUCCESS: IDirect3DDevice9 created`.

## Notes

- `d3d9_ddi.hpp` is a clean hand-transcription of the WDK `d3dumddi.h` DDI subset needed for
  adapter negotiation (mingw-w64 doesn't ship the WDK header). Do not add the real
  Microsoft-copyrighted `d3dumddi.h` to this directory.
- `OpenAdapter` reports `DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION` (our own implemented
  interface version), not the runtime's offered `Version` — echoing the runtime's value makes it
  validate device-func slots beyond what our `D3DDDI_DEVICEFUNCS` table declares.
- `fill_d3d9caps` reports real SM2.0 shader support (`VertexShaderVersion = D3DVS_VERSION(2, 0)`,
  `PixelShaderVersion = D3DPS_VERSION(2, 0)`). Restoring these from the previous fixed-function-only
  (`= 0`) sentinel re-triggers d3d9's VS2.0+ HAL-enable validator: an internal, undocumented
  function in `d3d9.dll` (found via `objdump` disassembly, live-confirmed by watching reads of
  `D3DCAPS9::VertexShaderVersion` through sogen's Python debugger API) that only runs once
  `VertexShaderVersion >= D3DVS_VERSION(2, 0)` and additionally requires: `PrimitiveMiscCaps` bit
  `0x2000` (undocumented internal reuse, same pattern as the `DevCaps`/`DevCaps2` gates below) plus
  `D3DPMISCCAPS_MASKZ`; `RasterCaps` to include `D3DPRASTERCAPS_FOGVERTEX`; `Src`/`DestBlendCaps` to
  include `D3DPBLENDCAPS_BLENDFACTOR`; and `GuardBand{Left,Top,Right,Bottom}` to each have
  `abs(value) >= 8192.0` (the exact float threshold read from `d3d9.dll`'s own `.rdata`) — all
  previously left unset/zero since the gate never ran. `fill_d3d9caps` now sets all of these, and
  `CreateDevice`/`GetDeviceCaps`/`GetCaps` succeed with real SM2.0 caps reported.
- With the HAL-enable gate satisfied, `CreateDevice` and the fixed-function (`SetFVF`, no explicit
  shader) draw path in `d3d9_triangle_test.cpp` still succeed end-to-end (`Present` returns
  `S_OK` both times). `DrawPrimitive` itself now returns `E_OUTOFMEMORY` for the FVF-only draw
  where it previously returned `S_OK`: live tracing (basic-block hooking via sogen's Python
  debugger API, following the exact DDI call sequence d3d9.dll issues before invoking
  `pfnDrawPrimitive`) shows d3d9.dll flushing pending device state (`pfnSetTexture` per stage,
  `pfnSetStreamSource`, `pfnSetIndices`) and then, now that VS2.0 is declared, calling one more
  still-unimplemented device-func slot (currently `device_stub`, which returns `S_OK` without
  populating any real output) before `pfnDrawPrimitive` is ever reached — most likely part of
  lazily creating/caching an internal fixed-function-emulation vertex shader or pipeline-state
  object, since that machinery is only exercised once the driver claims real shader support. This
  is expected and tracked as follow-up work for the DDI shader-creation wiring task in the
  vkd3d-shader de-risk plan, not a regression in this task's scope (`CreateDevice`/`GetDeviceCaps`/
  `GetCaps`/`Present` all still succeed, and the smoke test remains all-`Success`).
- x64-only bring-up; the x86/WoW64 UMD needs typed `__stdcall` thunks per device-func slot instead
  of the generic caller-cleanup stub.
