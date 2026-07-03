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
- `fill_d3d9caps` reports a fixed-function device (`VertexShaderVersion`/`PixelShaderVersion = 0`)
  to sidestep d3d9's SM2.0+ HAL-disable caps gauntlet. Restoring `D3DVS_VERSION(3, 0)`/
  `D3DPS_VERSION(3, 0)` re-triggers that gauntlet elsewhere (`GetDeviceCaps` itself starts failing)
  and needs its own follow-up before SM3.0 caps can be reported.
- x64-only bring-up; the x86/WoW64 UMD needs typed `__stdcall` thunks per device-func slot instead
  of the generic caller-cleanup stub.
