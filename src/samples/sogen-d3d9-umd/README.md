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

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_managed_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-managed-texture-test-x86.exe -ld3d9 -ld3dcompiler_43

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

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_mrt_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-mrt-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_multistream_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-multistream-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_pipeline_cache_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-pipeline-cache-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_scissor_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-scissor-test-x86.exe -ld3d9

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_mrt_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-mrt-test-x86.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_multistream_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-multistream-test-x86.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_pipeline_cache_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-pipeline-cache-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_dimension_discriminator_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-dim-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_dimension_discriminator_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-dim-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_multitexture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-multitexture-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_multitexture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-multitexture-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_pipeline_cache_rs_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-pipeline-cache-rs-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_pipeline_cache_stride_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-pipeline-cache-stride-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_drawprimitiveup_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-drawprimitiveup-test-x64.exe -ld3d9

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_drawprimitiveup_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-drawprimitiveup-test-x86.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_colorfill_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-colorfill-test-x64.exe -ld3d9

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_colorfill_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-colorfill-test-x86.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_stretchrect_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-stretchrect-test-x64.exe -ld3d9

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_stretchrect_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-stretchrect-test-x86.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_miptexture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-miptexture-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_miptexture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-miptexture-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_manydraws_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-manydraws-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_manydraws_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-manydraws-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_sm3_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-sm3-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_sm3_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-sm3-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_instancing_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-instancing-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_instancing_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-instancing-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_format_coverage_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-format-coverage-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_format_coverage_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-format-coverage-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_vertex_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-vertex-texture-test-x64.exe -ld3d9 -ld3dcompiler_43

i686-w64-mingw32-g++ -O2 -std=c++20 d3d9_vertex_texture_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-vertex-texture-test-x86.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_cube_volume_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-cube-volume-test-x64.exe -ld3d9

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_cube_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-cube-test-x64.exe -ld3d9 -ld3dcompiler_43

x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_volume_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-volume-test-x64.exe -ld3d9 -ld3dcompiler_43
```

`d3d9_shader_test.cpp`, `d3d9_const_test.cpp`, `d3d9_texture_test.cpp`, `d3d9_texcoord_test.cpp`,
`d3d9_int_bool_const_test.cpp`, `d3d9_mrt_test.cpp`, `d3d9_multistream_test.cpp`,
`d3d9_pipeline_cache_test.cpp`, `d3d9_dimension_discriminator_test.cpp`,
`d3d9_multitexture_test.cpp`, `d3d9_vertex_texture_test.cpp`, and `d3d9_pipeline_cache_rs_test.cpp` are guest-runtime tests,
not driver-side files, so they do not need the
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
cp d3d9-mrt-test-x64.exe <root>/filesys/c/d3d9-mrt-test.exe
cp d3d9-multistream-test-x64.exe <root>/filesys/c/d3d9-multistream-test.exe
cp d3d9-pipeline-cache-test-x64.exe <root>/filesys/c/d3d9-pipeline-cache-test.exe
cp d3d9-multitexture-test-x64.exe <root>/filesys/c/d3d9-multitexture-test.exe
cp sogen_d3d9um-x86.dll <root>/filesys/c/windows/syswow64/sogen_d3d9um.dll
cp d3d9-triangle-test-x86.exe <root>/filesys/c/d3d9-triangle-test-x86.exe
cp d3d9-shader-test-x86.exe <root>/filesys/c/d3d9-shader-test-x86.exe
cp d3d9-const-test-x86.exe <root>/filesys/c/d3d9-const-test-x86.exe
cp d3d9-texture-test-x86.exe <root>/filesys/c/d3d9-texture-test-x86.exe
cp d3d9-managed-texture-test-x86.exe <root>/filesys/c/d3d9-managed-texture-test-x86.exe
cp d3d9-texcoord-test-x86.exe <root>/filesys/c/d3d9-texcoord-test-x86.exe
cp d3d9-int-bool-const-test-x86.exe <root>/filesys/c/d3d9-int-bool-const-test-x86.exe
cp d3d9-scissor-test-x86.exe <root>/filesys/c/d3d9-scissor-test-x86.exe
cp d3d9-mrt-test-x86.exe <root>/filesys/c/d3d9-mrt-test-x86.exe
cp d3d9-multistream-test-x86.exe <root>/filesys/c/d3d9-multistream-test-x86.exe
cp d3d9-pipeline-cache-test-x86.exe <root>/filesys/c/d3d9-pipeline-cache-test-x86.exe
cp d3d9-multitexture-test-x86.exe <root>/filesys/c/d3d9-multitexture-test-x86.exe
cp d3d9-pipeline-cache-rs-test-x64.exe <root>/filesys/c/d3d9-pipeline-cache-rs-test.exe
cp d3d9-pipeline-cache-stride-test-x64.exe <root>/filesys/c/d3d9-pipeline-cache-stride-test.exe
cp d3d9-drawprimitiveup-test-x64.exe <root>/filesys/c/d3d9-drawprimitiveup-test.exe
cp d3d9-drawprimitiveup-test-x86.exe <root>/filesys/c/d3d9-drawprimitiveup-test-x86.exe
cp d3d9-colorfill-test-x64.exe <root>/filesys/c/d3d9-colorfill-test.exe
cp d3d9-colorfill-test-x86.exe <root>/filesys/c/d3d9-colorfill-test-x86.exe
cp d3d9-stretchrect-test-x64.exe <root>/filesys/c/d3d9-stretchrect-test.exe
cp d3d9-stretchrect-test-x86.exe <root>/filesys/c/d3d9-stretchrect-test-x86.exe
cp d3d9-miptexture-test-x64.exe <root>/filesys/c/d3d9-miptexture-test.exe
cp d3d9-miptexture-test-x86.exe <root>/filesys/c/d3d9-miptexture-test-x86.exe
cp d3d9-manydraws-test-x64.exe <root>/filesys/c/d3d9-manydraws-test.exe
cp d3d9-manydraws-test-x86.exe <root>/filesys/c/d3d9-manydraws-test-x86.exe
cp d3d9-sm3-test-x64.exe <root>/filesys/c/d3d9-sm3-test.exe
cp d3d9-sm3-test-x86.exe <root>/filesys/c/d3d9-sm3-test-x86.exe
cp d3d9-instancing-test-x64.exe <root>/filesys/c/d3d9-instancing-test.exe
cp d3d9-instancing-test-x86.exe <root>/filesys/c/d3d9-instancing-test-x86.exe
cp d3d9-format-coverage-test-x64.exe <root>/filesys/c/d3d9-format-coverage-test.exe
cp d3d9-format-coverage-test-x86.exe <root>/filesys/c/d3d9-format-coverage-test-x86.exe
cp d3d9-vertex-texture-test-x64.exe <root>/filesys/c/d3d9-vertex-texture-test.exe
cp d3d9-vertex-texture-test-x86.exe <root>/filesys/c/d3d9-vertex-texture-test-x86.exe
cp d3d9-cube-volume-test-x64.exe <root>/filesys/c/d3d9-cube-volume-test.exe
cp d3d9-cube-test-x64.exe <root>/filesys/c/d3d9-cube-test.exe
cp d3d9-volume-test-x64.exe <root>/filesys/c/d3d9-volume-test.exe
```

`<root>` is the emulated filesystem passed to the analyzer via `-e`; the real 64-bit Microsoft
`d3d9.dll` must already exist at `<root>/filesys/c/windows/system32/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/system32/d3dcompiler_43.dll` for the
shader, const, texture, int-bool-const, mrt, multistream, pipeline-cache, and multitexture tests. For the x86/WoW64 UMD, the real
32-bit Microsoft `d3d9.dll` must already exist at `<root>/filesys/c/windows/syswow64/d3d9.dll`, and
`d3dcompiler_43.dll` must exist at `<root>/filesys/c/windows/syswow64/d3dcompiler_43.dll` for the x86
shader, const, texture, managed-texture, texcoord, int-bool-const, mrt, multistream, pipeline-cache, and multitexture tests. (The scissor test is
fixed-function-only and needs no `d3dcompiler_43` on either architecture.)

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
./analyzer -e <root> -c c:/d3d9-mrt-test.exe
./analyzer -e <root> -c c:/d3d9-multistream-test.exe
./analyzer -e <root> -c c:/d3d9-pipeline-cache-test.exe
./analyzer -e <root> -c c:/d3d9-multitexture-test.exe
./analyzer -e <root> -c c:/d3d9-shader-test-x86.exe
./analyzer -e <root> -c c:/d3d9-const-test-x86.exe
./analyzer -e <root> -c c:/d3d9-texture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-managed-texture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-texcoord-test-x86.exe
./analyzer -e <root> -c c:/d3d9-int-bool-const-test-x86.exe
./analyzer -e <root> -c c:/d3d9-scissor-test-x86.exe
./analyzer -e <root> -c c:/d3d9-mrt-test-x86.exe
./analyzer -e <root> -c c:/d3d9-multistream-test-x86.exe
./analyzer -e <root> -c c:/d3d9-pipeline-cache-test-x86.exe
./analyzer -e <root> -c c:/d3d9-multitexture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-partial-lock-test.exe
./analyzer -e <root> -c c:/d3d9-pipeline-cache-rs-test.exe
./analyzer -e <root> -c c:/d3d9-pipeline-cache-stride-test.exe
./analyzer -e <root> -c c:/d3d9-drawprimitiveup-test.exe
./analyzer -e <root> -c c:/d3d9-drawprimitiveup-test-x86.exe
./analyzer -e <root> -c c:/d3d9-colorfill-test.exe
./analyzer -e <root> -c c:/d3d9-colorfill-test-x86.exe
./analyzer -e <root> -c c:/d3d9-stretchrect-test.exe
./analyzer -e <root> -c c:/d3d9-stretchrect-test-x86.exe
./analyzer -e <root> -c c:/d3d9-miptexture-test.exe
./analyzer -e <root> -c c:/d3d9-miptexture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-manydraws-test.exe
./analyzer -e <root> -c c:/d3d9-manydraws-test-x86.exe
./analyzer -e <root> -c c:/d3d9-sm3-test.exe
./analyzer -e <root> -c c:/d3d9-sm3-test-x86.exe
./analyzer -e <root> -c c:/d3d9-instancing-test.exe
./analyzer -e <root> -c c:/d3d9-instancing-test-x86.exe
./analyzer -e <root> -c c:/d3d9-format-coverage-test.exe
./analyzer -e <root> -c c:/d3d9-format-coverage-test-x86.exe
./analyzer -e <root> -c c:/d3d9-vertex-texture-test.exe
./analyzer -e <root> -c c:/d3d9-vertex-texture-test-x86.exe
./analyzer -e <root> -c c:/d3d9-cube-volume-test.exe
./analyzer -e <root> -c c:/d3d9-cube-test.exe
./analyzer -e <root> -c c:/d3d9-volume-test.exe
```

`d3d9-drawprimitiveup-test.exe` proves `DrawPrimitiveUP` and `DrawIndexedPrimitiveUP` (user-memory
vertex/index arrays, no vertex/index buffers). Real `d3d9.dll` implements these with no dedicated "UP"
DDI: it binds the user vertex array via `pfnSetStreamSourceUm` (device-func-table slot 7) and the user
index array via `pfnSetIndicesUm` (slot 9), then reuses the ordinary `pfnDrawPrimitive`/
`pfnDrawIndexedPrimitive` slot -- RE-verified live in `d3d9_x86.dll` (that RE also corrected
`pfnDrawPrimitive` to its true 3-arg WDK shape `(HANDLE, D3DDDIARG_DRAWPRIMITIVE*, const UINT* pFlags)`;
the earlier 2-arg guess desynced the x86 __stdcall stack, invisibly on x64 caller-cleanup). The UMD
transports the inline user bytes over new `set_stream_source_um`/`set_indices_um` wire records; the host
stashes them as transient UM-backed stream/index sources that `execute_draw` uploads as throwaway Vulkan
buffers, composing with the existing resource-id-backed path (fixed-function
`D3DFVF_XYZRHW|D3DFVF_DIFFUSE`, no shader compile). It draws a RED triangle via `DrawPrimitiveUP` and a
GREEN quad via `DrawIndexedPrimitiveUP` (both `D3DFMT_INDEX16` and `D3DFMT_INDEX32`), reading each back
with `LockRect` to check interior pixels match the geometry and corners stay the clear color. Expect all
`PASS:` lines and `[d3d9-drawprimitiveup-test] ALL CHECKS PASSED`.

`d3d9-colorfill-test.exe` proves `IDirect3DDevice9::ColorFill` -> real `d3d9.dll` `pfnColorFill`
(device-func-table slot 56, `D3DDDIARG_COLORFILL` RE'd in `d3d9_ddi.hpp`) -> UMD `color_fill` wire
record -> host rect-scoped fill of the render target's Vulkan image (a buffer->image transfer copy on
the shared draw command buffer, through the same `cmd_pipeline_barrier` layout choke point
`execute_draw` uses). A 640x480 RT is cleared BLUE, then `ColorFill` fills the center rect
`{160,120,480,360}` RED. Four interior checkpoints must read RED and four exterior ones must stay BLUE,
so a whole-surface fill or an off-by-one rect fails at least one check. Fixed-function only (no
`d3dcompiler_43`). Expect `ColorFill(...) hr=0x00000000`, eight `PASS:` lines, and
`[d3d9-colorfill-test] ALL CHECKS PASSED`. (No `d3dcompiler_43` needed on either architecture.)

`d3d9-stretchrect-test.exe` proves `IDirect3DDevice9::StretchRect` -> real `d3d9.dll` `pfnBlt`
(device-func-table slot 55, `D3DDDIARG_BLT` RE'd in `d3d9_ddi.hpp` -- SRC-first-then-DST field order) ->
UMD `blt` wire record -> host `vkCmdBlitImage`, both as a same-size 1:1 copy and a genuinely scaled
stretch. A src RT is given distinctive content by a REAL fixed-function draw (BLUE clear + RED quad over
the LEFT HALF only), so the test does not depend on ColorFill. Sub-pass A blits src whole->dst whole
(`D3DTEXF_POINT`): dst must become RED-left/BLUE-right, mirroring src. Sub-pass B blits the RED half
`{0,0,320,480}` -> dst whole `{0,0,640,480}`: the 2x horizontal magnification must make the whole dst
RED, and the (480,240) checkpoint that read BLUE in sub-pass A now reading RED is the discriminator
proving genuine scaling occurred. Scaled StretchRect only reaches `pfnBlt` because `fill_d3d9caps` now
advertises `StretchRectFilterCaps` (point+linear); with that field 0 the runtime returns
`D3DERR_INVALIDCALL` for any different-size-rect StretchRect before the driver is ever called (same-size
copies always dispatch). Fixed-function only (no `d3dcompiler_43`). Expect both
`StretchRect(...) hr=0x00000000`, seven `PASS:` lines, and `[d3d9-stretchrect-test] ALL CHECKS PASSED`.

`d3d9-miptexture-test.exe` proves real mip-mapping: a texture created with several REAL mip levels
gets each level's own pixel data all the way to the GPU (per-subresource `LockRect(level)` -> UMD
`D3DDDIARG_LOCK::SubResourceIndex` -> wire `subresource` -> host per-mip `extra_mips` backing ->
per-level `vkCmdCopyBufferToImage`), and the sampler actually selects a non-zero level (real
`min_lod`/`max_lod` range in `build_sampler`, not the old hardcoded `0`/`0` pinning). A 64x64 3-level
texture is filled RED (level 0, 64x64) / GREEN (level 1, 32x32) / BLUE (level 2, 16x16) via three
separate `LockRect(level, ...)` calls. Three sub-passes use `D3DSAMP_MIPFILTER=D3DTEXF_NONE` with
`D3DSAMP_MAXMIPLEVEL` = 0/1/2 to pin the sampler to exactly one level each (host maps MIPFILTER=NONE +
MAXMIPLEVEL=k to `min_lod==max_lod==k`), so the readback must be RED/GREEN/BLUE respectively -- the
GREEN and BLUE passes are the discriminator (broken per-level upload would read garbage/level-0 red;
the old LOD pinning would read RED for all three). A fourth sub-pass drives genuine minification (a
16x16-screen quad sampling the whole texture with `D3DSAMP_MIPFILTER=D3DTEXF_POINT` and the full LOD
range) so the GPU's own screen-space-derivative LOD (`log2(64/16)=2`) selects level 2 -- BLUE -- with
no MAXMIPLEVEL clamp. Expect `CreateTexture(64x64, 3 levels) hr=0x00000000`, three
`LockRect(level=...) hr=0x00000000` lines, four `PASS:` lines, and
`[d3d9-miptexture-test] ALL CHECKS PASSED`. Pixel-exact parity confirmed on x64 and x86/WoW64 (the
`SubResourceIndex` DDI offset differs -- 8 on x64, 4 on x86 -- but the `#ifdef _WIN64` struct split in
`d3d9_ddi.hpp` makes the UMD read the correct one automatically).

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

`d3d9-mrt-test.exe` proves real multiple-render-target (MRT) rendering and the all-bound-RTs
`Clear()` fix both work end to end, using a real `vs_2_0`/`ps_2_0` shader pair (`D3DFVF_XYZ|
D3DFVF_DIFFUSE`, matching `d3d9_const_test.cpp`'s recognized vertex layout -- a fixed-function VS
combined with a programmable PS doesn't reach the real PS at all, since `execute_draw` only takes the
programmable-pipeline path when both a VS and a PS are bound). The PS writes two distinct constant
colors to a struct return with `COLOR0`/`COLOR1` semantics (D3D9's `ps_2_0` ISA defines `oC0`-`oC3`
explicitly; MRT output is gated by `D3DCAPS9::NumSimultaneousRTs`, not by shader model). Two 640x480
off-screen render targets (RT0, RT1, sized to match `pfnCreateResource`'s hardcoded 640x480 KNOWN
LIMITATION) use `D3DFMT_X8R8G8B8`. (This passage originally noted that `D3DFMT_A8R8G8B8` render
targets failed client-side with `D3DERR_INVALIDCALL` because the FORMATOP table only flagged
X8R8G8B8 as an offscreen render target. As of the format-expansion work that is NO LONGER true --
A8R8G8B8 now carries the offscreen-RT op-bit and renders correctly; see the `d3d9-format-coverage-test`
section below, whose sub-pass 2 proves an A8R8G8B8 render target end to end. This test simply keeps
using X8R8G8B8; the checks are RGB-only, so the choice doesn't affect what it proves.) They are bound
ONCE at the start
(`SetRenderTarget(0, RT0)` / `SetRenderTarget(1, RT1)`) and
never rebound for the rest of the test. Sub-pass 1 draws a full-screen quad and `LockRect`-reads back
both RTs: RT0 must be entirely RED (`oC0`), RT1 must be entirely GREEN (`oC1`) -- the old, pre-Task-3
code only rendered into RT0, so RT1 would have stayed black. Sub-pass 2 calls
`Clear(D3DCLEAR_TARGET, yellow)` with both RTs still bound and re-reads both: both must now be
entirely YELLOW -- the old, pre-Task-4 code only cleared RT0, so RT1 would have stayed GREEN. Expect
`CreateVertexShader`/`CreatePixelShader`/both `CreateRenderTarget`/both `SetRenderTarget`/
`DrawIndexedPrimitive`/`Clear(yellow)` all `hr=0x00000000`, twelve `PASS:` lines (three checkpoints
per RT per sub-pass), and `[d3d9-mrt-test] ALL CHECKS PASSED`.

`d3d9-multistream-test.exe` proves a real multi-stream `D3DVERTEXELEMENT9` vertex declaration --
POSITION on stream 0, COLOR on stream 1, with stream 1 bound at a NONZERO `SetStreamSource` byte
offset -- actually reaches a real draw (Task 9, the empirical gate for Tasks 6-8's host-side
`stream_offsets`/`parse_vertex_decl`/multi-stream `execute_draw` work). Stream 0 holds 12 FLOAT3
positions (two flat-shaded, non-indexed triangles per half of the viewport); stream 1's buffer starts
with 20 bytes of a deliberately wrong pad color before its real 12 per-vertex `D3DCOLOR` entries begin,
and `SetStreamSource(1, ..., 20, sizeof(DWORD))` points past that pad at the real data. The left half
of the viewport must read back RED, the right half GREEN -- neither color is reachable unless BOTH the
second stream is genuinely bound (not silently dropped/falled-back-to-stream-0) AND its real, nonzero
offset is honored (not ignored/treated as 0, which would read the pad color instead). Expect
`CreateVertexDeclaration`/`DrawPrimitive` `hr=0x00000000`, two `PASS:` lines, and
`[d3d9-multistream-test] ALL CHECKS PASSED`. See this test's own header comment for the full account of
THREE genuine, independent guest-UMD bugs this task found and fixed while building it -- Tasks 6-8 never
touched the guest UMD (only `d3d9_host.cpp`/`.hpp`), and nothing before this test ever called
`CreateVertexDeclaration`/`SetVertexDeclaration` from a guest, so none of the three had ever been
reachable or visible:
1. `pfnCreateVertexShaderDecl` (D3DDDI_DEVICEFUNCS slot 45) was still an unwired `device_stub`, so
   `CreateVertexDeclaration()` never reached the host at all. Fixed by adding
   `umd_CreateVertexShaderDecl` (mirrors `umd_CreateVertexShaderFunc`/`create_shader_common`'s already-
   verified struct-pointer-plus-trailing-array convention) and wiring slot 45 to it.
2. `D3DDDIARG_CREATEVERTEXSHADERDECL`'s field order was guessed backwards (`ShaderHandle` first) --
   a live byte-dump of the real `pArgs` (once bug 1 was fixed enough to reach it) showed
   `NumVertexElements` actually comes first (offset 0), with the 8-byte `ShaderHandle` at offset 8 (4
   bytes of ordinary x64 alignment padding in between, previously misread as part of `ShaderHandle`).
   Fixed by swapping the field order.
3. `pfnSetVertexShaderDecl` (slot 47) was already wired, but as a struct-pointer call -- a live dump
   showed the "pArgs" parameter itself receiving the raw, small decl-id value directly (not a real
   pointer), meaning it is actually a DIRECT-VALUE `HANDLE` call, the same convention as
   `umd_SetVertexShaderFunc`/`umd_SetPixelShader`. Every real `SetVertexDeclaration()` call was silently
   forwarding `decl=0` to the host until this was fixed. Fixed by changing `umd_SetVertexShaderDecl`'s
   signature to take a plain `HANDLE` directly instead of a struct pointer.

All three had to be fixed together before this test produced anything but an unrendered (black) result;
the host-side dispatch and wire-protocol structs Tasks 6-8 built needed no changes at all.

`d3d9-pipeline-cache-test.exe` is a regression test for the `ensure_programmable_pipeline`/
`ensure_pipeline` `VkPipeline` cache-key fix (commits `3809d1c8`/`5dd05caa`). Before that fix, the
cache key for a programmable pipeline was only `(vertex_shader<<32)|pixel_shader` -- it ignored the
bound render-target count/formats and depth format, even though those are also baked into the built
pipeline (`VkPipelineRenderingCreateInfo::pColorAttachmentFormats`/`colorAttachmentCount`), so a draw
with 1 RT bound followed by a draw with 2 RTs bound using the SAME `vs_2_0`/`ps_2_0` pair (the PS
writes a single solid RED to `COLOR0` only, no `COLOR1` output) would incorrectly reuse the
first draw's stale 1-color-attachment `VkPipeline` against a 2-attachment dynamic-rendering scope.
Sub-pass 1 binds RT0 alone, clears it BLUE, draws the full-screen quad, and checks RT0 comes back RED.
Sub-pass 2 rebinds to BOTH RT0 (slot 0) and a new RT1 (slot 1) -- same VS/PS, never recreated, only the
bound-RT shape changes -- clears both BLUE, and draws the same quad again: RT0 must still be RED (the
primary, unambiguous discriminator -- the old bug's stale-pipeline reuse could corrupt attachment 0's
own output, not just leave a second attachment wrong), and RT1 must read back BLUE, unchanged from its
own `Clear()`. RT1's expected BLUE (not RED, and not undefined garbage) follows directly from how this
codebase's shader translation and pipeline creation actually work: `translate_d3d9_shader_pair`
(`d3d9_shader_translator.cpp`) compiles the PS's SPIR-V straight from its `ps_2_0` tokens with no
output-signature remapping on the pixel-shader side, so a PS that only writes `oC0` produces a
fragment module with exactly one declared output, at location 0; `create_graphics_pipeline`
(`vulkan_host.cpp`) still builds the pipeline with a 2-entry `color_formats`/blend-attachment array
regardless, so this is a real, valid 2-attachment pipeline whose fragment shader simply has no output
for attachment 1 -- Vulkan's fragment-output-interface matching leaves an attachment with no matching
shader output location untouched by that draw, rather than filling it with any fallback value. Before
committing this test, its own before/after check was run against the pre-`3809d1c8` host code (built
from `3809d1c8~1`): sub-pass 2's RT0 read back correctly, but RT1 read back all-zero/black instead of
its BLUE clear color -- a real, observable discrimination of the fixed bug, not a hypothetical one.
Expect `CreateVertexShader`/`CreatePixelShader`/both `CreateRenderTarget`/every `SetRenderTarget`/both
`DrawIndexedPrimitive` `hr=0x00000000`, nine `PASS:` lines (three checkpoints for RT0 in sub-pass 1,
plus three each for RT0 and RT1 in sub-pass 2), and `[d3d9-pipeline-cache-test] ALL CHECKS PASSED`.
`d3d9-pipeline-cache-test-x86.exe` was cross-compiled unchanged (this fix is entirely host-side C++, no
guest UMD/DDI wire-format changes) and passed on the first run against the real 32-bit `d3d9.dll`,
pixel-exact parity with the x64 result (all nine `PASS:` lines, `ALL CHECKS PASSED`, exit 0).

`d3d9-partial-lock-test.exe` proves a real `D3DLOCK_NOOVERWRITE`-style partial lock on a growing
dynamic vertex buffer only touches the sub-range it requested. It fills a 256-byte chunk with a
`D3DLOCK_DISCARD` lock, then appends two more 256-byte chunks at increasing nonzero offsets with
`D3DLOCK_NOOVERWRITE`, each with a distinctive byte pattern; a final whole-buffer read-only Lock
checks all three chunks still hold exactly their own pattern -- in particular that the first chunk
(the "untouched earlier region") was not disturbed by the later, higher-offset locks. Expect three
`PASS:` lines and `[d3d9-partial-lock-test] ALL CHECKS PASSED`. See the note below (Task 6,
2026-07-04) for what this fixes and how -- previously this class of lock silently got whole-buffer
semantics, which would have corrupted chunk 0 and failed this exact test.

`d3d9-multitexture-test.exe` proves a single pixel shader can sample TWO textures bound to different
D3D9 sampler registers (`s0` and `s1`) in one draw -- the multi-sampler binding work in
`d3d9_shader_translator.cpp` (`ps_sampler_bindings`) and `d3d9_host.cpp` (`ps_bindings` +
`execute_draw`'s per-stage sampler loop). Previously the translator declared only `s0`'s
combined-image-sampler (set 1, binding 1); the scheme now over-declares `s0`..`s3` at bindings
1/4/5/6 (stepping over the int/bool-const UBOs at bindings 2/3), which is proven inert for shaders that
don't reference the extra stages. The test builds two solid-color textures -- `texA` pure RED, `texB`
pure GREEN -- binds them to `s0`/`s1`, and draws one quad with a real `D3DCompile()`'d `ps_2_0` that
outputs `s0.rgb + s1.rgb`. RED + GREEN = YELLOW (`B=00 G=FF R=FF`), a third color distinct from either
input, so a correct combined result is unambiguous. Solid textures make the check independent of UV
interpolation, isolating the test to the sampler-binding path. Expect `CreateTexture(RED/s0)`/
`CreateTexture(GREEN/s1)`/`DrawIndexedPrimitive` `hr=0x00000000`, one `PASS:` line, and
`[d3d9-multitexture-test] ALL CHECKS PASSED`.

Before/after discriminator (run against the pre-multi-sampler host code): the test FAILS on the old
code and PASSES on the new. NOTE: the failure mode is graceful degradation, NOT the vkd3d-shader crash
the design investigation predicted. On this vkd3d build, referencing `s1` with no `s1` combined-sampler
binding supplied makes `vkd3d_shader_compile` return an error code (`result < 0`), which
`compile_stage` already handles by returning false; `translate_d3d9_shader_pair` then fails,
`ensure_programmable_pipeline` returns nullptr, and `execute_draw` degrades silently (returns `d3d_ok`
without drawing). The rendered pixel stays the clear color (black), so the analytic YELLOW check still
fails cleanly on old code -- a valid pass/fail discriminator, just not a crash. `d3d9-multitexture-test-
x86.exe` was cross-compiled unchanged and passed on the first run against the real 32-bit `d3d9.dll`,
pixel-exact parity with x64 (`center pixel=B=00 G=FF R=FF A=FF`, `ALL CHECKS PASSED`, exit 0).

`d3d9-vertex-texture-test.exe` proves SM3.0 VERTEX texture fetch (VTF): a real `vs_3_0` VERTEX shader
samples a texture bound to `D3DVERTEXTEXTURESAMPLER0` (D3D9 sampler stage 257, which real `d3d9.dll`
forwards through the DDI unmodified) with `tex2Dlod` and uses the sampled height to DISPLACE a vertex
position. This exercises the vertex-side combined-image-sampler wiring in
`d3d9_shader_translator.cpp` (VS samplers declared into descriptor set 0 via the shared
`sampler_binding_for_stage` formula) and `d3d9_host.cpp` (`vs_bindings` sampler slots, the
combined-image-sampler descriptor-pool count bumped to cover both stages' sets, and `execute_draw`'s
per-stage vertex-texture upload/descriptor-write loop keyed off `bound_textures[257 + k]`). A 2x2
`A16B16G16R16F` heightmap (`D3DPOOL_MANAGED`, bound directly without a `CheckDeviceFormat`
`D3DUSAGE_QUERY_VERTEXTEXTURE` query -- that capability-advertisement gap is now closed for this
format, see the format-coverage-test section below) stores height 0.0 in one
texel and 1.0 in another. One triangle's two base vertices sample the 0.0 texel (stay put) while its
apex samples the 1.0 texel and is displaced up by `height * 0.8333` NDC (200 screen px), moving from
baseline screen y=300 to y=100. Because ONLY the vertex whose per-vertex UV points at the high texel
moves, a correct result cannot be faked by a constant offset or by a pixel shader -- it requires the
vertex stage to fetch the texel that vertex's own UV selects. The discriminator probe `P_HIGH(320,180)`
is above the un-displaced apex (y=300) but inside the displaced triangle: it reads ORANGE
(`B=00 G=80 R=FF`) only if VTF really moved the apex, and the CLEAR color (`B=40`) otherwise. Before/after
verified: with the VS-sampler binding removed from the translator, `translate_d3d9_shader_pair` fails on
the VS `texldl` and `execute_draw` degrades gracefully (whole draw skipped), so `P_HIGH` and the
`P_BASE(320,370)` control both read the clear color and the test FAILS -- a real pass/fail discriminator.
Expect `VertexShaderVersion=0xfffe0300`, `D3DCompile(vs_3_0)`/`CreateVertexShader`/`CreateTexture(2x2
A16B16G16R16F)`/`SetTexture(D3DVERTEXTEXTURESAMPLER0)`/`DrawPrimitive` all `hr=0x00000000`, three `PASS:`
lines, and `[d3d9-vertex-texture-test] ALL CHECKS PASSED`. `d3d9-vertex-texture-test-x86.exe` was
cross-compiled unchanged and passed on the first run against the real 32-bit `d3d9.dll`, pixel-exact
parity with x64 (`P_HIGH pixel=B=00 G=80 R=FF`, `ALL CHECKS PASSED`, exit 0).

`d3d9-pipeline-cache-rs-test.exe` is a GATE test proving `pipeline_cache_key` (`d3d9_host.hpp`) has a
real, currently-unfixed cache-key gap: it's keyed by `vertex_shader`/`pixel_shader`/`color_formats[4]`/
`depth_format`/`vertex_shape` but NOT by any `D3DRS_*` render-state field, even though
`build_depth_state`/`build_blend_state` (`d3d9_host.cpp`) read `D3DRS_ZENABLE`/`ZWRITEENABLE`/`ZFUNC`/
`ALPHABLENDENABLE`/`SRCBLEND`/`DESTBLEND`/`BLENDOP` out of the same app-accumulated render state and bake
the result as STATIC pipeline state into every `VkPipeline` `ensure_programmable_pipeline` builds. It
compiles ONE `vs_2_0`/`ps_2_0` pair (an NDC-passthrough VS, and a PS hardcoded to output solid GREEN at
alpha 0.5, `float4(0,1,0,0.5)`) and never recreates the VS/PS objects. Sub-pass 1 draws with
`D3DRS_ALPHABLENDENABLE` at its real default (disabled) -- builds/caches a blend-DISABLED pipeline, and
RT0 correctly reads back unblended solid GREEN. Sub-pass 2 rebinds `D3DRS_ALPHABLENDENABLE=TRUE`/
`D3DRS_SRCBLEND=D3DBLEND_SRCALPHA`/`D3DRS_DESTBLEND=D3DBLEND_INVSRCALPHA` (same VS/PS/RT/vertex-shape,
so the current cache key is unchanged) and draws the same quad again -- the test asserts the
analytically-correct `SRCALPHA`/`INVSRCALPHA` blend of GREEN(a=0.5) over the BLACK clear
(`B=00 G=80 R=00`). Run against the current, unmodified host code: sub-pass 1's three checkpoints PASS
(`G=FF`, unblended, as expected), but sub-pass 2's three checkpoints all FAIL -- RT0 reads back `G=FF`
again, i.e. `ensure_programmable_pipeline` incorrectly reused sub-pass 1's blend-DISABLED `VkPipeline`
instead of building a blend-ENABLED one, exactly the predicted bug. This test is EXPECTED to fail
(`[d3d9-pipeline-cache-rs-test] FAILED`, exit 1) until `pipeline_cache_key` is extended to also fold in
the depth/blend render-state fields that `build_depth_state`/`build_blend_state` actually read; it should
start passing once that fix lands, with no changes to the test itself. Only built/run on x64 so far.

`d3d9-pipeline-cache-stride-test.exe` is a GATE test proving `vertex_shape_key()`'s (`d3d9_host.cpp`)
real-declaration branch has a real, currently-unfixed cache-key gap of its own, distinct from both
`d3d9-pipeline-cache-test.exe` (RT shape) and `d3d9-pipeline-cache-rs-test.exe` (render state): when a
real `D3DVERTEXELEMENT9` declaration is bound, it fingerprints the pipeline's vertex-input shape with
ONLY the declaration HANDLE (`return this->state_.vertex_decl;`), reasoning that the same handle always
implies the same element types/offsets/usages -- true for the declaration's own shape, but the actual
built `VkVertexInputBindingDescription::stride` for each binding is ALSO read from
`state_.stream_strides[stream]` (`ensure_programmable_pipeline`'s real-decl branch), which
`SetStreamSource(stream, buffer, offset, stride)` can change independently of the declaration handle. It
compiles ONE `vs_2_0`/`ps_2_0` pair (a position-only passthrough VS matching a real declaration with a
single POSITION element, and a PS hardcoded to output solid RED) and creates the declaration ONCE, never
recreating VS/PS/declaration across the two sub-passes. Sub-pass 1 binds a 4-vertex buffer with a
tightly-packed 12-byte stride (`strideA`) forming a quad over the LEFT half of the viewport via
`SetStreamSource(0, bufA, 0, 12)`, clears the RT BLACK, and draws -- this builds/caches a `VkPipeline`
with binding-0 stride baked to 12; the left-half checkpoint correctly reads back RED. Sub-pass 2 (the
actual discriminator) rebinds stream 0 to a SECOND buffer with a distinctively different, larger 32-byte
stride (`strideB` -- 12 real position bytes plus 20 zeroed pad bytes per record) forming a quad over the
RIGHT half of the viewport via `SetStreamSource(0, bufB, 0, 32)` -- same declaration/VS/PS/RT-shape as
sub-pass 1, so `pipeline_cache_key` collapses to the identical value under the current, unfixed
`vertex_shape_key()`. Re-clears the SAME RT BLACK and draws again: the right-half checkpoint is asserted
to read back RED (the analytically-correct result once the real stride-32 layout is honored). Run against
the current, unmodified host code (rebuilt `sogen_d3d9um-x64.dll`/`analyzer` from unmodified
`d3d9_host.cpp`/`.hpp`, confirmed via `git status`/mtime immediately before the run): sub-pass 1's
checkpoint correctly reads `B=00 G=00 R=FF` (RED, PASS), but sub-pass 2's checkpoint reads back
`B=00 G=00 R=00` (BLACK -- its own untouched Clear color) instead of RED, FAIL -- exactly the predicted
bug. This is not a coincidental result: `ensure_programmable_pipeline` reused sub-pass 1's stale
stride-12-baked `VkPipeline` against buffer B's real stride-32 data, so index 0's fetch happens to land on
buffer B's real position (coincidentally correct at offset 0), but indices 1-3 are fetched at the wrong
byte offsets entirely -- one degenerate (zero-area, never rasterized) triangle and one real-area triangle
whose vertices (computed from buffer B's actual bytes at the stale 12-byte stride: `(0,-1)`, `(0,0)`,
`(-1,0.5)` in NDC) all have `x <= 0`, so neither triangle's convex hull can ever reach the right-half
checkpoint at NDC `x = 0.5` -- it deterministically stays BLACK under the bug, not merely "some wrong
color". This test is EXPECTED to fail (`[d3d9-pipeline-cache-stride-test] FAILED`, exit 1) until
`vertex_shape_key()`'s real-declaration branch is extended to also fold in the real, current per-stream
strides of every stream the bound declaration references; it should start passing once that fix lands,
with no changes to the test itself. Only built/run on x64 so far.

`d3d9-manydraws-test.exe` is the correctness+timing evidence for the per-draw-overhead performance
slice (commits `02b28ada`/`0238dfd7`/`fcfccc00`/`36b03142`/`4b0bc778`: a blocking fence-wait replacing
the CPU-pinning busy-spin, plus per-pipeline descriptor-set pooling and per-device vertex/index/UBO
buffer pooling in `execute_draw`). Within ONE `BeginScene`/`EndScene` it issues 768
`DrawIndexedPrimitive` calls (a 32x24 grid of 20x20-pixel cells), ALL through the SAME cached
programmable pipeline -- same `vs_2_0`/`ps_2_0` pair, same 640x480 RT shape, same 4-vertex/6-index
unit-quad vertex shape, same two constant UBOs -- i.e. exactly the case the pooling optimizes: after the
first draw nothing is (re)allocated; the pooled VB/IB/descriptor-sets/UBOs are reused and only their
CONTENTS are rewritten per draw. Each draw fills a distinct cell with a distinct, index-derived color,
driven per-draw by a real changing VS constant (`c0` = the cell's NDC offset+scale, so the pooled vertex
data lands in a different place every draw) AND a real changing PS constant (`c0` = the cell color). This
is the correctness discriminator: if the pooling reused stale contents (a later draw seeing an earlier
draw's UBO bytes because the pool was rewritten before the GPU finished reading it, or a buffer not
actually re-uploaded), cells would show the WRONG color or land in the WRONG place. Eight cells spread
across the whole grid (four corners, center, three interior) are read back and checked against their own
analytically-derived colors; all eight read back byte-exact on both x64 and x86/WoW64 (e.g.
`cell(16,12)` = `B=84 G=85 R=83`, `cell(8,5)` = `B=B3 G=37 R=41`, identical on both architectures).
The draw loop is bracketed by `QueryPerformanceCounter` and prints its wall-clock time; every draw is
still FULLY SYNCHRONOUS (submit, then block until the GPU completes, before the next draw starts), so
this slice does NOT change the NUMBER of GPU round-trips per frame -- only the per-round-trip COST. Run
against a temporarily-reverted pre-fix host (the 4 host files checked out at `02b28ada~1` = `b6809cee`,
`analyzer` rebuilt), the same 768-draw loop measured ~383 ms (~0.50 ms/draw); against the fixed HEAD it
measured ~279 ms (~0.36 ms/draw) -- a real, repeatable ~27% reduction in per-frame draw-loop time, with
identical (all-PASS) pixel output in both. This is emulated guest wall-clock (the guest's own
`QueryPerformanceCounter` under the analyzer), not host CPU time, so it captures the emulated cost of the
busy-spin's polling loop and the per-draw allocation churn, not a raw hardware GPU-stall number. Expect
all `hr=0x00000000` setup lines, `TIMING: 768 draws in ... ms`, eight `PASS:` lines, and
`[d3d9-manydraws-test] ALL CHECKS PASSED`.

`d3d9-sm3-test.exe` proves the Shader Model 3.0 caps delta in `fill_d3d9caps` (`sogen_d3d9_umd.cpp`)
makes real Microsoft `d3d9.dll` accept AND render a genuine `vs_3_0`/`ps_3_0` shader pair end to end.
Raising `VertexShaderVersion`/`PixelShaderVersion` to `D3DVS_VERSION(3,0)`/`D3DPS_VERSION(3,0)` opens
`IsD3DHALSupported`'s SM3.0 validation branch, which reads a dozen further `D3DCAPS9` fields directly
out of the same GetCaps(type=13) buffer; the delta adds exactly the fields that branch requires
(`DevCaps2` VERTEXELEMENTSCANSHARESTREAMOFFSET, `RasterCaps` COLORPERSPECTIVE, `TextureCaps`
PERSPECTIVE/TEXREPEATNOTSCALEDBYSIZE/PROJECTED, `PrimitiveMiscCaps` INDEPENDENTWRITEMASKS/
MRTPOSTPIXELSHADERBLENDING, `Cube`/`VolumeTextureFilterCaps`, `TextureAddressCaps`, `StencilCaps`, and
the SM3.0 instruction-slot caps raised from 0 to 32768) while leaving every SM2.0 field untouched --
the change is purely additive and byte-for-byte pixel-identical for all existing SM2.0 tests. The test
has four independent proofs: (1) `GetDeviceCaps(HAL)` must report `VertexShaderVersion=0xFFFE0300`/
`PixelShaderVersion=0xFFFF0300`; (2) the pixel shader carries a real runtime-count loop
(`for i < loopCount.x`, driven by `SetPixelShaderConstantI`), so `D3DCompile` of the SAME source at
`ps_2_0` MUST FAIL (ps_2_0 has neither loop/rep nor integer constant registers) while `ps_3_0`
SUCCEEDS -- a genuine SM3.0-only discriminator; (3) the `ps_3_0` bytecode is walked as raw D3DBC tokens
to confirm a real LOOP/REP opcode was emitted (not unrolled/closed-formed); (4) a triangle is drawn to
an off-screen RT with this pair and `LockRect`-read back -- the PS accumulates `0.1` per iteration over
5 runtime-supplied iterations and returns `float4(acc, acc*0.5, acc*1.5, 1)`, whose three DISTINCT
channel bytes (`B=BF G=40 R=80`, computed by replaying the identical float loop C++-side) prove the
whole path -- caps acceptance, `ps_3_0` SPIR-V translation, PS integer constant register (set 1 /
binding 2) delivery, and the real GPU loop -- worked. Expect the `ps_2_0`-rejected/`ps_3_0`-accepted
lines, `saw_LOOP/REP=yes`, `CreateVertexShader`/`CreatePixelShader` `hr=0x00000000` with non-null
handles, four `PASS:` lines, and `[d3d9-sm3-test] ALL CHECKS PASSED`. Pixel-exact parity confirmed on
x64 and x86/WoW64 (`interior pixel(320,240)=B=BF G=40 R=80` on both). Needs `d3dcompiler_43` on both
architectures.

## Notes

- `d3d9_ddi.hpp` is a clean hand-transcription of the WDK `d3dumddi.h` DDI subset needed for
  adapter negotiation (mingw-w64 doesn't ship the WDK header). Do not add the real
  Microsoft-copyrighted `d3dumddi.h` to this directory.
- `OpenAdapter` reports `DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION` (our own implemented
  interface version), not the runtime's offered `Version` — echoing the runtime's value makes it
  validate device-func slots beyond what our `D3DDDI_DEVICEFUNCS` table declares.
- `fill_d3d9caps` reports real SM3.0 shader support (`VertexShaderVersion = D3DVS_VERSION(3, 0)`,
  `PixelShaderVersion = D3DPS_VERSION(3, 0)` -- see the SM3.0 delta writeup further down this file,
  around the `d3d9-sm3-test.exe` description, for the full SM3.0-specific gate list). This first
  re-triggered an internal, undocumented `d3d9.dll` HAL-enable validator (found via `objdump`
  disassembly, live-confirmed via sogen's Python debugger API) that runs once VS2.0+ is declared
  and additionally requires: `PrimitiveMiscCaps` bit `0x2000` plus `D3DPMISCCAPS_MASKZ`;
  `RasterCaps` to include `D3DPRASTERCAPS_FOGVERTEX`; `Src`/`DestBlendCaps` to include
  `D3DPBLENDCAPS_BLENDFACTOR`; and `GuardBand{Left,Top,Right,Bottom}` to each satisfy
  `abs(value) >= 8192.0`. `fill_d3d9caps` sets all of these (the SM2.0 gate), plus the further
  SM3.0-specific gate documented below, and `CreateDevice`/`GetDeviceCaps`/`GetCaps` succeed with
  real SM3.0 caps reported.
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
  (`pixel(320,240)=B=26 G=FF R=00 A=FF`, both analytic checks passing). See `docs/d3d9-roadmap.md` and
  `HANDOFF_MACBOOK.md` §22 for the full design/RE narrative.
- **Scissor rect, MRT, and multi-stream vertex sources, ported to x86/WoW64 (Task 10, 2026-07-05) --
  ALL THREE found ZERO new x86-only bugs, unlike several earlier ports.** `d3d9-scissor-test-x86.exe`,
  `d3d9-mrt-test-x86.exe`, and `d3d9-multistream-test-x86.exe` were cross-compiled unchanged (no source
  edits to any of the three `.cpp` test files, matching this project's established zero-source-change
  porting pattern) and passed on the first run against the real 32-bit `d3d9.dll`, every analytic pixel
  check matching x64 exactly. This is a genuine (if unglamorous) finding in its own right: it confirms
  the scissor-rect draw-time gating, the fixed-slot MRT array/`Clear()` fan-out, and the vertex-decl
  parser + multi-stream `SetStreamSource` offset plumbing are all architecture-agnostic as designed --
  none of them touch a HANDLE-width-sensitive struct field the way `d3d9_host::allocate_id()` and
  `D3DDDIARG_CREATERESOURCE`'s output-handle offset did for the const/texture tests. The three real bugs
  the multi-stream work found (see the `d3d9-multistream-test.exe` entry above) are all in the
  architecture-independent parts of `sogen_d3d9_umd.cpp` (DDI slot wiring, struct field order, calling
  convention), so they were already exercised by the shared x64 test run before this port and needed no
  x86-specific fix. Full regression sweep after this port (every x64/x86 guest test plus the 26/26 smoke
  test) is documented in `docs/d3d9-roadmap.md` and `HANDOFF_MACBOOK.md`.

`d3d9-instancing-test.exe` proves real D3D9 hardware instancing: a single `DrawIndexedPrimitive` draws
N geometry instances driven entirely by `SetStreamSourceFreq` (no explicit instance-count draw
argument exists in D3D9 -- the count is the low 30 bits of the `D3DSTREAMSOURCE_INDEXEDDATA` stream's
divider, and each `D3DSTREAMSOURCE_INSTANCEDATA` stream advances once per instance). This exercises the
host path added in `resolve_instancing()`/`vertex_shape_key()`/`ensure_programmable_pipeline`/
`execute_draw` (`d3d9_host.cpp`): the per-instance stream binding gets
`VK_VERTEX_INPUT_RATE_INSTANCE`, the instance-rate mask is folded into `pipeline_cache_key`'s
`vertex_input_shape`, and the decoded instance count is passed to `vkCmdDrawIndexed`. A real
`D3DVERTEXELEMENT9` declaration puts POSITION on stream 0 (`INDEXEDDATA | 4`, per-vertex quad geometry,
indexed) and a per-instance `float2` offset + `D3DCOLOR` on stream 1 (`INSTANCEDATA | 1`). The VS adds
the per-instance offset to the per-vertex local position and the PS outputs the per-instance color, so
one draw paints four disjoint solid quads (RED/GREEN/BLUE/YELLOW) into the four screen quadrants. Double
discriminator: (a) if the instance count were still 1, only the first instance would draw -- one
quadrant painted, three left at the BLACK clear color; (b) if the per-instance stream stayed
`VK_VERTEX_INPUT_RATE_VERTEX`, each of the quad's four corners would pick up a different instance's
offset+color, rendering one big color-interpolated quad instead of four flat solid ones. The test probes
all four quadrant centers (plus an interior point per quadrant) and requires four distinct, pure, solid
colors -- both wrong implementations fail this. Verified against the pre-task host behavior
(instance_count forced to 1 AND inputRate forced to VERTEX): all four centers read back BLACK, the test
`FAILED` -- a real, observed discrimination, not a hypothetical one. Expect `CreateVertexDeclaration`/
`DrawIndexedPrimitive` `hr=0x00000000`, four `PASS:` lines, and `[d3d9-instancing-test] ALL CHECKS
PASSED`. Pixel-byte-identical parity confirmed on x64 and x86/WoW64 (the feature is entirely host-side
C++ plus the already-wired `SetStreamSourceFreq` transport, no guest UMD/DDI change). KNOWN LIMITATION:
only an `INSTANCEDATA` divider of exactly 1 is honored -- a non-1 divider needs
`VK_EXT_vertex_attribute_divisor`, which is not enabled on the D3D9 Vulkan device, so such a stream is
left per-vertex (see `resolve_instancing()`'s comment).

`d3d9-format-coverage-test.exe` proves the `g_formats` FORMATOP expansion (`sogen_d3d9_umd.cpp`) lets
real `d3d9.dll` create/sample/render the additional `D3DFORMAT`s the host's `d3d9_format_to_vulkan`
(`d3d9_format.cpp`) already maps. Each of three sub-passes is a genuine before/after discriminator --
with the pre-expansion table (DXT1 + texture-only A8R8G8B8), the underlying `CreateTexture`/
`CreateRenderTarget` calls returned `D3DERR_NOTAVAILABLE` (0x8876086c) and the sub-pass could not even
start (observed live against the pre-change UMD). All three source textures are tiny 4x4 solid-color
`D3DPOOL_MANAGED` textures sampled POINT/CLAMP across a full-screen quad into an off-screen RT, then read
back with `LockRect` (same proven path as `d3d9_texture_test.cpp`/`d3d9_managed_texture_test.cpp`):
- **DXT5 texture** (`FMT_OP_TEXTURE`): a 4x4 BC3 block encoding solid RED -- the GPU's BC3 decode must
  reproduce RED (host maps DXT5 -> `VK_FORMAT_BC3_UNORM_BLOCK`).
- **A8R8G8B8 render target** (the `RT_TEX` op-bit upgrade of the existing A8R8G8B8 row): render a solid
  CYAN A8R8G8B8 source texture INTO an A8R8G8B8 render target (previously un-creatable), then read it
  back. A8R8G8B8 is host-side `B8G8R8A8_UNORM` (4 bytes/texel), so the readback path needs no change --
  only the FORMATOP advertisement gated it.
- **L8 texture** (`FMT_OP_TEXTURE`): a 4x4 single-channel luminance texture (value 200). Host maps L8 ->
  `VK_FORMAT_R8_UNORM` sampled with identity swizzle, so the value lands in R and G/B read 0.
- **R5G6B5 negative discriminator** (`FMT_OP_TEXTURE` only): `CreateTexture(D3DFMT_R5G6B5)` must SUCCEED
  but `CreateRenderTarget(D3DFMT_R5G6B5)` must FAIL. R5G6B5 is 2 bytes/texel host-side
  (`VK_FORMAT_R5G6B5_UNORM_PACK16`), while the RT readback/Present/ColorFill paths hardcode 4 bytes/texel
  BGRA8 (same limitation as `A16B16G16R16F`, see below), so its FORMATOP row is scoped texture-only. This
  sub-pass is the before/after proof of that scoping: the row previously carried `RT_TEX`, so the RT
  creation would have silently succeeded with corrupt readback.
- **Vertex-texture-fetch capability** (`FMT_OP_VERTEXTEXTURE` on the `A16B16G16R16F` row): a pure
  `CheckDeviceFormat` capability query, no render. `CheckDeviceFormat(D3DUSAGE_QUERY_VERTEXTEXTURE,
  D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)` must return `S_OK`, while the same query for `D3DFMT_L8` (which
  carries no such bit) must still return `D3DERR_NOTAVAILABLE` (0x8876086a) -- proving the capability is
  format-specific. This closes the FORMATOP gap the SM3.0 vertex-texture-fetch work left open: the DDI
  bind/draw path was already proven (see `d3d9-vertex-texture-test.exe`), but a well-behaved app that
  gates on `CheckDeviceFormat` first would have refused to use vertex textures because no format advertised
  the bit. Before/after: run against the pre-change UMD, this sub-pass genuinely FAILS with
  `A16B16G16R16F hr=0x8876086a`; after, `hr=0x00000000`. The exact op-bit value (`0x00800000`, matching
  ReactOS `ddrawint.h` `D3DFORMAT_OP_VERTEXTEXTURE`, **not** the `0x00400000` a first pass assumed -- that
  is `D3DFORMAT_OP_AUTOGENMIPMAP`) was RE-confirmed against real `d3d9.dll`'s `CEnum::CheckDeviceFormat`,
  which tests its internal per-format op-word (a verbatim copy of this driver FORMATOP) for bit
  `0x00800000` on a `D3DUSAGE_QUERY_VERTEXTEXTURE` query -- advertising `0x00400000` does not satisfy it
  (verified live: it still returned `D3DERR_NOTAVAILABLE`).

`A16B16G16R16F` is advertised `FMT_OP_TEXTURE | FMT_OP_VERTEXTEXTURE` (sampled HDR texture, usable as a
vertex texture), deliberately NOT a render target: the host Present/snapshot readback path
(`vulkan_host::create_render_target`'s readback buffer, plus `d3d9_host`'s RT backing and ColorFill
snapshot copy) hardcodes 4 bytes/texel BGRA8, so an 8-byte/texel HDR render target would undersize those
buffers -- renderable HDR support needs that host work first, out of scope for this format-advertisement
slice (the R5G6B5 sub-pass above is the negative-case counterpart of this same 4-bytes/texel constraint).
Expect the three render sub-passes' `CreateTexture`/`CreateRenderTarget`/`DrawIndexedPrimitive` all
`hr=0x00000000`, the R5G6B5 `CreateRenderTarget` to FAIL (a non-zero `hr`, expected), the
`CheckDeviceFormat(QUERY_VERTEXTEXTURE, A16B16G16R16F)` to return `hr=0x00000000`, five `PASS:` lines, and
`[d3d9-format-coverage-test] ALL CHECKS PASSED`. Pixel-byte-identical parity confirmed on x64 and
x86/WoW64.

`d3d9-cube-volume-test.exe` proves the cube/volume half of that FORMATOP expansion plus the
`umd_CreateResource` Flags-based kind classification (`resource_flags_to_kind` in `sogen_d3d9_umd.cpp`):
before the change the `g_formats` table applied `FMT_OP_CUBETEXTURE`/`FMT_OP_VOLUMETEXTURE` to zero rows,
so real `d3d9.dll` rejected `CreateCubeTexture`/`CreateVolumeTexture` for **every** format with
`D3DERR_INVALIDCALL` (0x8876086c) -- it gates those entry points on the driver's per-format op-word.
Scope is **creation only**: cube/volume resources have no host-side GPU backing yet (the host
`create_resource`/`ensure_texture_uploaded` paths only back plain `texture_2d`), so the test never
Locks/samples/draws them -- it asserts the creation `HRESULT` and releases. The `A8R8G8B8` cube +
volume creates must SUCCEED; `L8` (no cube/volume op-bit) must still FAIL both -- the discriminator
proving the capability is format-specific, not "everything works now"; and `DXT1` must SUCCEED as a cube
but FAIL as a volume, proving the deliberate cube-yes/volume-no scope for the compressed rows (compressed
volume textures are vanishingly rare in real D3D9 usage). Expect `CreateCubeTexture(A8R8G8B8)`/
`CreateVolumeTexture(A8R8G8B8)`/`CreateCubeTexture(DXT1)` `hr=0x00000000`, the three negatives
`hr=0x8876086c`, and `[d3d9-cube-volume-test] ALL CHECKS PASSED`. x64 only for now (x86/WoW64 port is a
later task, together with the host-side cube/volume GPU image + sampling work).

`d3d9-cube-test.exe` proves cube-texture SAMPLING end to end (the host-side cube GPU image/upload/view
work, commits `39c8728a`/`fab1bcaa`, on top of the creation-only `d3d9-cube-volume-test.exe` above): a
real `IDirect3DCubeTexture9` gets each of its six faces' distinct pixel data all the way to the GPU
(per-face `LockRect(D3DCUBEMAP_FACE_POSITIVE_X + f, 0, ...)` -> UMD `D3DDDIARG_LOCK::SubResourceIndex ==
face*mip_levels + level` -> host per-subresource backing -> a single 6-array-layer
`VK_IMAGE_VIEW_TYPE_CUBE` image, via the shared `texture_subresource_layout`/`sampled_view_shape_for_kind`
in `d3d9_host.cpp`), and `texCUBE()` selects the correct face for a sample direction. A 64x64 single-mip
cube (`D3DUSAGE_DYNAMIC | D3DPOOL_DEFAULT`, the proven-lockable path -- the creation-only test used
`Usage=0` because it never locked) is filled RED/GREEN/BLUE/YELLOW/MAGENTA/CYAN in the standard D3D9 face
order `+X/-X/+Y/-Y/+Z/-Z`. Six sub-passes each point `texCUBE` at one face's center direction via a
`float3` PS constant `c0` (`SetPixelShaderConstantF`, register c0) -- `(1,0,0)`/`(-1,0,0)`/`(0,1,0)`/
`(0,-1,0)`/`(0,0,1)`/`(0,0,-1)` -- draw a full-screen quad, and read back the center pixel. This is the
discriminator: each sub-pass is asserted against its OWN face color, so a wrong flattened subresource
index or Vulkan array-layer assignment (swapped, or every face reading layer 0) would read the SAME wrong
color across multiple sub-passes instead of six distinct correct ones. Confirmed live: all six read back
byte-exact (`+X=B00 G00 RFF`, `-X=B00 GFF R00`, `+Y=BFF G00 R00`, `-Y=B00 GFF RFF`, `+Z=BFF G00 RFF`,
`-Z=BFF GFF R00`), and the per-face `LockRect` `pBits` differ, confirming genuinely separate
per-subresource backing. Expect `CreateCubeTexture(64, 1 level)`/six `LockRect(face=...)`/
`CreateRenderTarget`/all six `DrawIndexedPrimitive` `hr=0x00000000`, six `PASS:` lines, and
`[d3d9-cube-test] ALL CHECKS PASSED`. x64 only for now (x86/WoW64 port is a later task); needs
`d3dcompiler_43`.

`d3d9-volume-test.exe` proves volume-texture SAMPLING end to end (same host-side commits): a real
`IDirect3DVolumeTexture9` gets all of its depth slices' distinct pixel data to the GPU (one `LockBox(0)`
writing every slice -> UMD `D3DDDIARG_LOCK` subresource 0 spanning the whole depth extent -> host single
`VK_IMAGE_TYPE_3D` image with a `VK_IMAGE_VIEW_TYPE_3D` view), and `tex3D()` selects the correct depth
slice for a given `w`. A 32x32x4 single-mip volume (`D3DUSAGE_DYNAMIC | D3DPOOL_DEFAULT`) is filled
RED/GREEN/BLUE/YELLOW, one solid color per depth slice, in ONE `LockBox(0)` call. The host backs
subresource 0 as one tightly-packed slice-major block (slice `d` at byte offset `d*width*height*4`), and
this UMD's Lock DDI does NOT populate `D3DLOCKED_BOX::RowPitch`/`SlicePitch` (both came back 0, same
known gap `d3d9_miptexture_test.cpp` documents for `D3DLOCKED_RECT`), so the test writes each slice at
`pBits + d*(width*height*4)` with self-computed pitches -- exactly the tight layout the host's
buffer->3D-image copy reads. Four sub-passes each sample `tex3D` at the texture center (`u,v = 0.5,0.5`)
with `w = (d + 0.5) / 4` (0.125/0.375/0.625/0.875) via a `float3` PS constant `c0`, draw a full-screen
quad, and read back the center pixel. This is the discriminator targeting the "depth extent collapsed to
1, or always sample slice 0" failure mode: each sub-pass is asserted against its OWN slice color, so a
broken volume image would read the SAME (slice-0) color across all four instead of four distinct correct
ones. Confirmed live: all four read back byte-exact (`slice0=B00 G00 RFF`, `slice1=B00 GFF R00`,
`slice2=BFF G00 R00`, `slice3=B00 GFF RFF`). Expect `CreateVolumeTexture(32x32x4, 1 level)`/`LockBox(0)`/
`CreateRenderTarget`/all four `DrawIndexedPrimitive` `hr=0x00000000`, four `PASS:` lines, and
`[d3d9-volume-test] ALL CHECKS PASSED`. x64 only for now (x86/WoW64 port is a later task); needs
`d3dcompiler_43`.
