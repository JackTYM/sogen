# vkd3d-shader De-risk Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers-extended-cc:subagent-driven-development (recommended) or superpowers-extended-cc:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Translate a real, `D3DCompile()`-produced SM2 vertex+pixel shader pair to SPIR-V via vkd3d-shader and render it through the existing D3D9-over-Vulkan pipeline, proving the programmable-shader path end to end for a position+color passthrough triangle.

**Architecture:** A new host-only `libvkd3d-shader` dependency (built via autotools through `ExternalProject_Add`, mirroring the `icicle-bridge` pattern) is wrapped by a new `d3d9_shader_translator` module that scans a VS/PS pair together (SM1-3 has no semantic-based inter-stage linking), links them with a vkd3d-shader-built varying map, and compiles both to SPIR-V. `d3d9_host`'s existing `ensure_pipeline`/`execute_draw` gain a second, lazily-built-and-cached programmable pipeline path selected whenever both a vertex and pixel shader are bound, alongside the existing hardcoded fixed-function path (unchanged). The UMD gains the two currently-missing DDI marshaling functions (`CreateVertexShaderFunc`/`CreatePixelShader`) needed to get real shader bytecode from the guest to the host at all — the wire protocol and host-side dispatch for shader creation already exist and are unused today. A new guest test compiles a tiny HLSL passthrough shader with the real `D3DCompile()` (already-staged `d3dcompiler_43.dll`) and verifies the result the same way the fixed-function triangle was verified: an analytic host-side pixel readback, no external oracle.

**Tech Stack:** vkd3d-shader (Wine's `vkd3d` repo, `libvkd3d-shader.a` only, autotools build), existing Vulkan dynamic-rendering pipeline machinery (`vulkan_host`), mingw-w64 `d3dcompiler_43.dll`/`d3dcompiler.h` for guest-side HLSL compilation, sogen's existing D3D9 wire protocol (`d3d9-command-protocol`) and `gpu_bridge`/`d3d9_host` host-side dispatch.

**User decisions (already made):**
- De-risk slice first: position+color passthrough only, no constant buffers/UBOs, no textures — a separate follow-on slice covers the general binding contract.
- Shader source comes from real `D3DCompile()` at guest runtime, not hand-assembled SM2 bytecode.
- vkd3d-shader is built via autotools (`autogen.sh`/`configure`/`make`), not meson — meson was installed for nothing; do not document it as a prerequisite for this dependency.
- The translator implements the real vkd3d-shader scan+build-varying-map+compile pipeline (not a simplified alternative) — confirmed as the only correct approach for SM1-3 bytecode.
- Task 0 (restoring real `VertexShaderVersion`/`PixelShaderVersion` caps) is in scope for this plan, as an open-ended RE investigation matching this project's established live-instrumentation methodology, not deferred to a separate plan.

---

## Task 0: Restore SM2.0 vertex/pixel shader caps and fix whatever the caps gauntlet breaks

**Goal:** `D3DCAPS9::VertexShaderVersion`/`PixelShaderVersion` report real SM2.0 support instead of the current `0` (fixed-function-only) sentinel, and `GetDeviceCaps`/`GetCaps` still succeed afterward — this is a hard prerequisite for every later task, since a caps-respecting or caps-gated runtime may never reach `pfnCreateVertexShaderFunc`/`pfnCreatePixelShader` otherwise.

**Context:** `sogen_d3d9_umd.cpp:276-280`'s own comment says reporting `VertexShaderVersion = 0` was a deliberate workaround "to sidestep the VS2.0+ HAL-disable caps gauntlet," and that restoring real values "re-triggers that gauntlet elsewhere (confirmed: GetDeviceCaps itself starts failing) and needs its own follow-up investigation." This project has hit two prior undocumented-caps-gate bugs this session (`DevCaps2` STREAMOFFSET bit, `DevCaps` bit `0x02000000`), both root-caused via live GDB-stub tracing (see `HANDOFF_MACBOOK.md` §10.6 for the exact Python `sogen` module methodology: `app.hooks.memory_execution_at`/`memory_write` on d3d9.dll's own decompiled functions). This task uses the same methodology since the exact failure is unknown until captured — do not guess a fix.

**Files:**
- Modify: `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp:279-280` (`fill_d3d9caps`)
- Modify: `src/samples/sogen-d3d9-umd/README.md:46-49` (Notes section, currently documents the `VertexShaderVersion=0` workaround as still-necessary)

**Acceptance Criteria:**
- [ ] `caps->VertexShaderVersion = D3DVS_VERSION(2, 0);` and `caps->PixelShaderVersion = D3DPS_VERSION(2, 0);` in `fill_d3d9caps`.
- [ ] `d3d9-triangle-test.exe` (the existing fixed-function regression test, unmodified) still prints `SUCCESS: IDirect3DDevice9 created` and completes both `Present` calls with `hr=0x00000000`, run via `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe`.
- [ ] `analyzer -e root -s c:/test-sample.exe` smoke test still reports 26/26 `Success`.
- [ ] `README.md`'s Notes section no longer describes `VertexShaderVersion`/`PixelShaderVersion = 0` as the current state; it documents whatever the real, final caps values and any accompanying fix turned out to be.

**Verify:** `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe` → `SUCCESS: IDirect3DDevice9 created`, both `Present hr=0x00000000` lines present, no new `FAIL:` lines versus the pre-Task-0 baseline run.

**Steps:**

- [ ] **Step 1: Capture the pre-change baseline**

Run the existing triangle test once before touching anything, and save the output for comparison:

```bash
cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe > /tmp/task0-baseline.log 2>&1
cat /tmp/task0-baseline.log
```

Confirm it ends with `SUCCESS: Present completed` (or equivalent) exactly as today, with no `FAIL:` lines. This is the regression bar Step 4 must clear.

- [ ] **Step 2: Restore real shader-version caps**

In `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp`, replace:

```cpp
        // Report a fixed-function device (no VS/PS) to sidestep the VS2.0+ HAL-disable caps gauntlet.
        // Restoring D3DVS/PS_VERSION(3,0) here re-triggers that gauntlet elsewhere (confirmed: GetDeviceCaps
        // itself starts failing) and needs its own follow-up investigation before SM3.0 can be reported.
        caps->VertexShaderVersion = 0;
        caps->PixelShaderVersion = 0;
```

with:

```cpp
        caps->VertexShaderVersion = D3DVS_VERSION(2, 0);
        caps->PixelShaderVersion = D3DPS_VERSION(2, 0);
```

Rebuild the UMD and re-stage it (same recipe `README.md` already documents):

```bash
cd src/samples/sogen-d3d9-umd
x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 sogen_d3d9_umd.cpp sogen_d3d9_umd.def \
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll
cp sogen_d3d9um-x64.dll ../../../build/release/artifacts/root/filesys/c/windows/system32/sogen_d3d9um.dll
```

- [ ] **Step 3: Re-run and observe what breaks**

```bash
cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe > /tmp/task0-after.log 2>&1
diff /tmp/task0-baseline.log /tmp/task0-after.log
```

If it's identical (or only differs in expected ways, e.g. no `FAIL:` lines added) — the caps gauntlet the comment warned about doesn't actually trigger for this DDI negotiation path, skip to Step 5. If `GetDeviceCaps`/`CreateDevice`/anything else now fails, continue to Step 4.

- [ ] **Step 4: If something broke, instrument and RE it live (same methodology as `HANDOFF_MACBOOK.md` §10.6)**

Write a Python script using sogen's own GDB-stub bindings (mirroring `trace_write.py`'s pattern already used this session — hook the d3d9.dll entry points around whichever call started failing per Step 3's diff, e.g. `GetDeviceCaps`/`GetCaps`, dump register/memory state at the failure point) to `scratchpad/task0_caps_trace.py`, run it against the real d3d9.dll (`sys.path.insert(0, ".../build/release-py/artifacts"); import sogen`), and find the exact gate — a specific caps field value, DevCaps bit, or code path branch — the same way the `DevCaps` `0x02000000` bit and `DevCaps2` STREAMOFFSET bit were found. Fix `fill_d3d9caps` accordingly (add whatever caps field/bit the trace reveals is missing). This step's exact content cannot be written in advance — the failure, if any, determines the fix.

- [ ] **Step 5: Re-verify no regression**

```bash
cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe
./analyzer -e root -s c:/test-sample.exe
```

Confirm the triangle test still succeeds end-to-end and the smoke test is still 26/26.

- [ ] **Step 6: Update README.md's Notes section**

Replace the stale note (`src/samples/sogen-d3d9-umd/README.md:46-49`):

```markdown
- `fill_d3d9caps` reports a fixed-function device (`VertexShaderVersion`/`PixelShaderVersion = 0`)
  to sidestep d3d9's SM2.0+ HAL-disable caps gauntlet. Restoring `D3DVS_VERSION(3, 0)`/
  `D3DPS_VERSION(3, 0)` re-triggers that gauntlet elsewhere (`GetDeviceCaps` itself starts failing)
  and needs its own follow-up before SM3.0 caps can be reported.
```

with a note describing the actual final state (real SM2.0 caps reported; whatever Step 4 found and fixed, if anything — write this once Step 4's outcome is known, do not pre-write it now).

- [ ] **Step 7: Commit**

```bash
git add src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp src/samples/sogen-d3d9-umd/README.md
git commit --no-gpg-sign -m "fix(d3d9): restore real SM2.0 vertex/pixel shader caps"
```

---

## Task 1: vkd3d-shader dependency — submodule + autotools build integration

**Goal:** `libvkd3d-shader.a` and its public headers are available to link against from `windows-emulator`, built automatically as part of the normal CMake build.

**Files:**
- Create: `deps/vkd3d` (git submodule, `https://gitlab.winehq.org/wine/vkd3d.git`)
- Create: `deps/vkd3d-shader-bridge/CMakeLists.txt`
- Modify: `deps/CMakeLists.txt`
- Modify: `.gitmodules`
- Modify: `CLAUDE.md` or `README.md` (document `perl`, `flex`, `bison` as new build prerequisites — all three already confirmed present on this Mac, but must be documented for other environments)

**Acceptance Criteria:**
- [ ] `git submodule status deps/vkd3d` shows a pinned commit, not `-` (uninitialized).
- [ ] `cmake --build --preset=release` builds `libvkd3d-shader.a` as part of the normal build, with no manual pre-build step.
- [ ] The build target `vkd3d-shader-bridge` (an `INTERFACE` library) exists and can be linked via `target_link_libraries`.
- [ ] A trivial `#include <vkd3d_shader.h>` in a scratch `.cpp` linked against `vkd3d-shader-bridge` compiles and links successfully (verified in Task 2, which is this dependency's first real consumer).

**Verify:** `cmake --build --preset=release 2>&1 | grep -i vkd3d` shows `libvkd3d-shader.la`/`libvkd3d-shader.a` being built; the full `cmake --build --preset=release` exits 0.

**Steps:**

- [ ] **Step 1: Add the vkd3d submodule**

```bash
git submodule add https://gitlab.winehq.org/wine/vkd3d.git deps/vkd3d
git -C deps/vkd3d log -1 --format="%H %cd"
```

- [ ] **Step 2: Write `deps/vkd3d-shader-bridge/CMakeLists.txt`**

```cmake
include(ExternalProject)

set(VKD3D_SHADER_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../vkd3d)
set(VKD3D_SHADER_LIB ${VKD3D_SHADER_SOURCE_DIR}/libs/vkd3d-shader/.libs/libvkd3d-shader.a)

ExternalProject_Add(
    vkd3d-shader-project
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    SOURCE_DIR ${VKD3D_SHADER_SOURCE_DIR}
    BINARY_DIR ${VKD3D_SHADER_SOURCE_DIR}
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir ${VKD3D_SHADER_SOURCE_DIR} ./autogen.sh
        COMMAND ${CMAKE_COMMAND} -E chdir ${VKD3D_SHADER_SOURCE_DIR} ./configure --disable-tests --disable-demos
    BUILD_COMMAND ${CMAKE_COMMAND} -E chdir ${VKD3D_SHADER_SOURCE_DIR} make libvkd3d-shader.la
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
    USES_TERMINAL_CONFIGURE 1
    USES_TERMINAL_BUILD 1
    BUILD_BYPRODUCTS ${VKD3D_SHADER_LIB}
)

add_library(vkd3d-shader-bridge INTERFACE)
add_dependencies(vkd3d-shader-bridge vkd3d-shader-project)
target_link_libraries(vkd3d-shader-bridge INTERFACE ${VKD3D_SHADER_LIB})
target_include_directories(vkd3d-shader-bridge INTERFACE ${VKD3D_SHADER_SOURCE_DIR}/include)
```

- [ ] **Step 3: Wire it into `deps/CMakeLists.txt`**

Add near the end of `deps/CMakeLists.txt` (after the existing `capstone`/`googletest`/`zstd` blocks):

```cmake
##########################################

add_subdirectory(vkd3d-shader-bridge)
```

- [ ] **Step 4: Build and confirm the library artifact exists**

```bash
cmake --build --preset=release 2>&1 | tee /tmp/task1-build.log
ls -la deps/vkd3d/libs/vkd3d-shader/.libs/libvkd3d-shader.a
```

If `autogen.sh`/`configure` fails, capture the exact error (missing tool, version mismatch) and fix — `perl`, `flex`, `bison` are already confirmed present on this Mac (checked via `which perl flex bison` and `perl -MJSON -e 1` for the required JSON module), so a fresh failure here means something else (report and resolve before continuing, since every later task depends on this build succeeding).

- [ ] **Step 5: Document new prerequisites**

Add to `CLAUDE.md`'s Build section (or `README.md`, whichever the project's existing prerequisite list lives in — check first):

```markdown
- vkd3d-shader (host-only dependency, `deps/vkd3d` submodule) requires `perl` (with the `JSON` module), `flex`, and `bison` to build. Install via `brew install perl flex bison` if missing.
```

- [ ] **Step 6: Commit**

```bash
git add .gitmodules deps/vkd3d deps/vkd3d-shader-bridge/CMakeLists.txt deps/CMakeLists.txt CLAUDE.md
git commit --no-gpg-sign -m "build(deps): add vkd3d-shader via autotools ExternalProject_Add"
```

---

## Task 2: `d3d9_shader_translator` — real vkd3d-shader scan+link+compile wrapper

**Goal:** A clean, isolated module that turns a raw SM1-3 vertex+pixel shader token-blob pair into two SPIR-V binaries, with all vkd3d-shader API usage confined to this module's `.cpp` (mirroring `vulkan_host.hpp`'s own "keep backend calls confined" discipline).

**Files:**
- Create: `src/windows-emulator/devices/d3d9_shader_translator.hpp`
- Create: `src/windows-emulator/devices/d3d9_shader_translator.cpp`
- Modify: `src/windows-emulator/CMakeLists.txt:31` (link `vkd3d-shader-bridge`)

**Acceptance Criteria:**
- [ ] `d3d9_shader_translator.hpp` contains no vkd3d-shader types (`vkd3d_shader_compile_info`, etc.) — only `shader_pair_spirv` and `translate_d3d9_shader_pair`'s declaration, using plain C++ standard types.
- [ ] `translate_d3d9_shader_pair` performs the real scan → build-varying-map → compile(VS) → compile(PS) → free-everything sequence (no simplified stand-in).
- [ ] Passing malformed/truncated token data returns `false` with `out` left empty (does not crash).
- [ ] `cmake --build --preset=release` compiles and links this module with no warnings under the project's normal compile flags.

**Verify:** `cmake --build --preset=release` succeeds with `d3d9_shader_translator.cpp` in the build; a temporary `static_assert`/smoke call from `d3d9_host.cpp` (removed before Task 2 is done, added properly by Task 3) confirms the function is linkable and callable.

**Steps:**

- [ ] **Step 1: Write the header**

`src/windows-emulator/devices/d3d9_shader_translator.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sogen
{
    struct shader_pair_spirv
    {
        std::vector<uint32_t> vertex_spirv;
        std::vector<uint32_t> pixel_spirv;
    };

    // Translates a raw SM1-3 vertex+pixel shader pair (the DDI's pFunction/pCode token blobs, each a
    // DWORD-tagged stream starting with the version token) into SPIR-V bytes vulkan_host::
    // create_shader_module can consume directly. Both shaders are required together because SM1-3 has
    // no semantic-based inter-stage linking; vkd3d-shader instead requires an explicit varying map
    // built from both shaders' scanned signatures. Returns false on translation failure (malformed or
    // unsupported bytecode, or a scan/link/compile failure); out is left empty in that case.
    bool translate_d3d9_shader_pair(const void* vs_tokens, size_t vs_token_size_bytes, const void* ps_tokens,
                                    size_t ps_token_size_bytes, shader_pair_spirv& out);
} // namespace sogen
```

- [ ] **Step 2: Write the implementation**

`src/windows-emulator/devices/d3d9_shader_translator.cpp`:

```cpp
#include "d3d9_shader_translator.hpp"

extern "C"
{
#include <vkd3d_shader.h>
}

#include <cstring>

namespace sogen
{
    namespace
    {
        bool scan_signature(const void* tokens, const size_t token_size_bytes, vkd3d_shader_scan_signature_info& info,
                            vkd3d_shader_signature& out_output_or_input, const bool want_output)
        {
            info = vkd3d_shader_scan_signature_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SCAN_SIGNATURE_INFO};

            vkd3d_shader_compile_info compile_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
            compile_info.next = &info;
            compile_info.source.code = tokens;
            compile_info.source.size = token_size_bytes;
            compile_info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
            compile_info.log_level = VKD3D_SHADER_LOG_NONE;

            char* messages = nullptr;
            const int result = vkd3d_shader_scan(&compile_info, &messages);
            if (messages != nullptr)
            {
                vkd3d_shader_free_messages(messages);
            }
            if (result < 0)
            {
                return false;
            }
            out_output_or_input = want_output ? info.output : info.input;
            return true;
        }

        bool compile_stage(const void* tokens, const size_t token_size_bytes,
                           const vkd3d_shader_varying_map_info* varying_map_info, std::vector<uint32_t>& out_spirv)
        {
            vkd3d_shader_spirv_target_info spirv_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
            spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;

            vkd3d_shader_compile_info compile_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO};
            compile_info.source.code = tokens;
            compile_info.source.size = token_size_bytes;
            compile_info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
            compile_info.target_type = VKD3D_SHADER_TARGET_SPIRV_BINARY;
            compile_info.log_level = VKD3D_SHADER_LOG_NONE;

            if (varying_map_info != nullptr)
            {
                compile_info.next = varying_map_info;
                // varying_map_info->next is set by the caller to chain onward to spirv_info.
            }
            else
            {
                compile_info.next = &spirv_info;
            }

            vkd3d_shader_code out{};
            char* messages = nullptr;
            const int result = vkd3d_shader_compile(&compile_info, &out, &messages);
            if (messages != nullptr)
            {
                vkd3d_shader_free_messages(messages);
            }
            if (result < 0)
            {
                return false;
            }

            out_spirv.resize(out.size / sizeof(uint32_t));
            std::memcpy(out_spirv.data(), out.code, out.size);
            vkd3d_shader_free_shader_code(&out);
            return true;
        }
    } // namespace

    bool translate_d3d9_shader_pair(const void* vs_tokens, const size_t vs_token_size_bytes, const void* ps_tokens,
                                    const size_t ps_token_size_bytes, shader_pair_spirv& out)
    {
        out.vertex_spirv.clear();
        out.pixel_spirv.clear();

        if (vs_tokens == nullptr || vs_token_size_bytes == 0 || ps_tokens == nullptr || ps_token_size_bytes == 0)
        {
            return false;
        }

        vkd3d_shader_scan_signature_info vs_scan{};
        vkd3d_shader_signature vs_output{};
        if (!scan_signature(vs_tokens, vs_token_size_bytes, vs_scan, vs_output, /*want_output=*/true))
        {
            return false;
        }

        vkd3d_shader_scan_signature_info ps_scan{};
        vkd3d_shader_signature ps_input{};
        if (!scan_signature(ps_tokens, ps_token_size_bytes, ps_scan, ps_input, /*want_output=*/false))
        {
            vkd3d_shader_free_scan_signature_info(&vs_scan);
            return false;
        }

        std::vector<vkd3d_shader_varying_map> varying_map(16);
        unsigned int varying_count = 0;
        vkd3d_shader_build_varying_map(&vs_output, &ps_input, &varying_count, varying_map.data());
        varying_map.resize(varying_count);

        vkd3d_shader_free_scan_signature_info(&vs_scan);
        vkd3d_shader_free_scan_signature_info(&ps_scan);

        vkd3d_shader_spirv_target_info vs_spirv_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_SPIRV_TARGET_INFO};
        vs_spirv_info.environment = VKD3D_SHADER_SPIRV_ENVIRONMENT_VULKAN_1_0;

        vkd3d_shader_varying_map_info varying_map_info{.type = VKD3D_SHADER_STRUCTURE_TYPE_VARYING_MAP_INFO};
        varying_map_info.next = &vs_spirv_info;
        varying_map_info.varying_map = varying_map.data();
        varying_map_info.varying_count = varying_count;

        if (!compile_stage(vs_tokens, vs_token_size_bytes, &varying_map_info, out.vertex_spirv))
        {
            return false;
        }
        if (!compile_stage(ps_tokens, ps_token_size_bytes, nullptr, out.pixel_spirv))
        {
            out.vertex_spirv.clear();
            return false;
        }
        return true;
    }
} // namespace sogen
```

- [ ] **Step 3: Link the translator against vkd3d-shader-bridge**

In `src/windows-emulator/CMakeLists.txt:31`, change:

```cmake
target_link_libraries(windows-emulator PRIVATE gpu-bridge-protocol dxgk-command-protocol d3d9-command-protocol vulkan-headers Freetype::Freetype)
```

to:

```cmake
target_link_libraries(windows-emulator PRIVATE gpu-bridge-protocol dxgk-command-protocol d3d9-command-protocol vulkan-headers Freetype::Freetype vkd3d-shader-bridge)
```

(`d3d9_shader_translator.cpp` is picked up automatically by `windows-emulator/CMakeLists.txt`'s existing `GLOB_RECURSE *.cpp` — no source-list edit needed.)

- [ ] **Step 4: Build and fix any compile errors**

```bash
cmake --build --preset=release 2>&1 | tee /tmp/task2-build.log
```

If `vkd3d_shader_scan_signature_info`'s designated-initializer field names, `vkd3d_shader_varying_map_info`'s exact field names, or any other struct field differs from what's written above, fix against the real header at `deps/vkd3d/include/vkd3d_shader.h` (this plan's field names were confirmed by reading that exact header during planning, but re-check if the build disagrees — headers are the ground truth, not this document).

- [ ] **Step 5: Commit**

```bash
git add src/windows-emulator/devices/d3d9_shader_translator.hpp src/windows-emulator/devices/d3d9_shader_translator.cpp src/windows-emulator/CMakeLists.txt
git commit --no-gpg-sign -m "feat(d3d9): add vkd3d-shader SM1-3-to-SPIRV translator wrapper"
```

---

## Task 3: `d3d9_host` programmable pipeline — lazy translate-and-cache + `execute_draw` selection

**Goal:** When both a vertex and pixel shader are bound, `execute_draw` renders with a real, translated, cached programmable pipeline instead of the hardcoded fixed-function pair — with zero behavior change when no shaders are bound.

**Files:**
- Modify: `src/windows-emulator/devices/d3d9_host.hpp`
- Modify: `src/windows-emulator/devices/d3d9_host.cpp`

**Acceptance Criteria:**
- [ ] A new private cache (keyed by `{vertex_shader_id, pixel_shader_id}`) exists on `d3d9_host`, holding `{vs_module, fs_module, pipeline}` triplets.
- [ ] `execute_draw` uses the programmable pipeline whenever `state_.vertex_shader != 0 && state_.pixel_shader != 0`, and the existing hardcoded FF pipeline otherwise — verified by code inspection (the `if`/`else` is the only change to `execute_draw`'s control flow; the vertex-upload/barrier/draw/readback sequence is untouched).
- [ ] A cache miss calls `translate_d3d9_shader_pair` exactly once per distinct `(vertex_shader_id, pixel_shader_id)` pair; a second draw with the same pair reuses the cached pipeline (no re-translation) — verified by a temporary counter/log during Task 6's verification pass, removed afterward.
- [ ] Translation or pipeline-creation failure degrades to a no-op draw (`d3d_ok`), matching every other GPU-unavailable path in this file — does not crash, does not corrupt `resources_`/`state_`.
- [ ] `cmake --build --preset=release` succeeds; `d3d9-triangle-test.exe` (fixed-function, unmodified) still renders correctly (regression check — Task 3 must not touch the FF path's behavior).

**Verify:** `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe` still shows the FF triangle rendering correctly (same centroid-pixel verification as before Task 3); `cmake --build --preset=release` exits 0.

**Steps:**

- [ ] **Step 1: Add the programmable pipeline cache to `d3d9_host.hpp`**

In `src/windows-emulator/devices/d3d9_host.hpp`, add `#include "d3d9_shader_translator.hpp"` near the top (after `#include "vulkan_host.hpp"`), and add this private struct + member after the existing `bool pipeline_ready_{false};` (end of the FF pipeline block, around line 156):

```cpp
        struct programmable_pipeline_entry
        {
            uint64_t vs_module{};
            uint64_t fs_module{};
            uint64_t pipeline{};
        };

        // Keyed by (vertex_shader_id << 32 | pixel_shader_id) -- translation is lazy, on first draw
        // with both shaders bound, since SM1-3 requires the VS/PS pair together to build the
        // inter-stage varying map (see d3d9_shader_translator.hpp).
        std::unordered_map<uint64_t, programmable_pipeline_entry> programmable_pipelines_{};
```

Add a new private method declaration next to `bool ensure_pipeline(...)`:

```cpp
        const programmable_pipeline_entry* ensure_programmable_pipeline(uint32_t color_format, uint32_t width,
                                                                         uint32_t height);
```

- [ ] **Step 2: Implement `ensure_programmable_pipeline` in `d3d9_host.cpp`**

Add right after the existing `ensure_pipeline` function (after its closing brace, before the `namespace { ... find_memory_type_index ... }` block):

```cpp
    const d3d9_host::programmable_pipeline_entry* d3d9_host::ensure_programmable_pipeline(const uint32_t color_format,
                                                                                           const uint32_t width,
                                                                                           const uint32_t height)
    {
        const uint64_t key = (this->state_.vertex_shader << 32) | this->state_.pixel_shader;
        const auto cached = this->programmable_pipelines_.find(key);
        if (cached != this->programmable_pipelines_.end())
        {
            return &cached->second;
        }

        const auto vs_it = this->shaders_.find(this->state_.vertex_shader);
        const auto ps_it = this->shaders_.find(this->state_.pixel_shader);
        if (vs_it == this->shaders_.end() || ps_it == this->shaders_.end())
        {
            return nullptr;
        }

        shader_pair_spirv spirv{};
        if (!translate_d3d9_shader_pair(vs_it->second.tokens.data(), vs_it->second.tokens.size() * sizeof(uint32_t),
                                        ps_it->second.tokens.data(), ps_it->second.tokens.size() * sizeof(uint32_t), spirv))
        {
            return nullptr;
        }

        const uint64_t device = this->ensure_vk_device();
        if (device == 0)
        {
            return nullptr;
        }

        programmable_pipeline_entry entry{};
        if (this->vulkan_.create_shader_module(device, spirv.vertex_spirv.data(),
                                               spirv.vertex_spirv.size() * sizeof(uint32_t), entry.vs_module) != 0 ||
            entry.vs_module == 0)
        {
            return nullptr;
        }
        if (this->vulkan_.create_shader_module(device, spirv.pixel_spirv.data(),
                                               spirv.pixel_spirv.size() * sizeof(uint32_t), entry.fs_module) != 0 ||
            entry.fs_module == 0)
        {
            return nullptr;
        }

        uint64_t layout = 0;
        if (this->vulkan_.create_pipeline_layout(device, 0, 0, {}, layout) != 0 || layout == 0)
        {
            return nullptr;
        }

        // D3DFVF_XYZ|D3DFVF_DIFFUSE: 12-byte {x,y,z} clip-space position + 4-byte D3DCOLOR diffuse,
        // stride 16 -- this slice's one supported programmable vertex format (position+color
        // passthrough only, see the design spec's Non-Goals).
        const std::array<vulkan_host::vertex_binding, 1> bindings{
            {{.binding = 0, .stride = 16, .input_rate = VK_VERTEX_INPUT_RATE_VERTEX}}};
        const std::array<vulkan_host::vertex_attribute, 2> attributes{{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
            {.location = 1, .binding = 0, .format = VK_FORMAT_B8G8R8A8_UNORM, .offset = 12},
        }};
        const std::array<uint32_t, 1> color_formats{color_format};
        const std::array<uint32_t, 2> dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        const vulkan_host::depth_state depth{.test_enable = 0, .write_enable = 0, .compare_op = 0};
        const std::array<vulkan_host::color_blend_attachment, 1> blend{{{
            .blend_enable = 0,
            .src_color_blend_factor = 0,
            .dst_color_blend_factor = 0,
            .color_blend_op = 0,
            .src_alpha_blend_factor = 0,
            .dst_alpha_blend_factor = 0,
            .alpha_blend_op = 0,
            .color_write_mask = 0xF,
        }}};
        const vulkan_host::specialization empty_spec{};

        const int32_t result = this->vulkan_.create_graphics_pipeline(
            device, /*render_pass=*/0, layout, entry.vs_module, entry.fs_module, width, height, bindings, attributes,
            depth, color_formats, /*depth_format=*/0, /*stencil_format=*/0, /*rasterization_samples=*/1,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, /*primitive_restart_enable=*/0, dynamic_states, empty_spec, empty_spec,
            blend, entry.pipeline);
        if (result != 0 || entry.pipeline == 0)
        {
            return nullptr;
        }

        return &this->programmable_pipelines_.emplace(key, entry).first->second;
    }
```

- [ ] **Step 3: Wire the selection into `execute_draw`**

In `execute_draw` (`d3d9_host.cpp`), the current pipeline-binding sequence is:

```cpp
        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra() || !this->ensure_pipeline(VK_FORMAT_B8G8R8A8_UNORM, rt.width, rt.height))
        {
            return d3d_ok; // GPU unavailable; degrade silently like the rest of this host does
        }
```//...
```cpp
        this->vulkan_.cmd_bind_pipeline(this->command_buffer_, this->pipeline_, VK_PIPELINE_BIND_POINT_GRAPHICS);
```

Replace both with shader-path selection. First, the `ensure_pipeline` guard becomes:

```cpp
        const uint64_t device = this->ensure_vk_device();
        if (device == 0 || !this->ensure_draw_infra())
        {
            return d3d_ok; // GPU unavailable; degrade silently like the rest of this host does
        }

        const bool use_programmable = this->state_.vertex_shader != 0 && this->state_.pixel_shader != 0;
        const programmable_pipeline_entry* programmable = nullptr;
        if (use_programmable)
        {
            programmable = this->ensure_programmable_pipeline(VK_FORMAT_B8G8R8A8_UNORM, rt.width, rt.height);
            if (programmable == nullptr)
            {
                return d3d_ok; // translation/pipeline failure; degrade silently
            }
        }
        else if (!this->ensure_pipeline(VK_FORMAT_B8G8R8A8_UNORM, rt.width, rt.height))
        {
            return d3d_ok;
        }
```

Then change the bind call to:

```cpp
        this->vulkan_.cmd_bind_pipeline(this->command_buffer_, use_programmable ? programmable->pipeline : this->pipeline_,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS);
```

The push-constant call (`cmd_push_constants` for `viewport_size`, tied to the FF path's `pipeline_layout_`) must only run for the fixed-function path, since the programmable pipeline's layout has zero push-constant ranges (`create_pipeline_layout(device, 0, 0, {}, layout)` in Step 2). Guard it:

```cpp
        if (!use_programmable)
        {
            const std::array<float, 2> viewport_size{static_cast<float>(rt.width), static_cast<float>(rt.height)};
            this->vulkan_.cmd_push_constants(this->command_buffer_, this->pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                             sizeof(viewport_size), viewport_size.data());
        }
```

- [ ] **Step 4: Build**

```bash
cmake --build --preset=release 2>&1 | tee /tmp/task3-build.log
```

Fix any compile errors (e.g. if `vulkan_host::vertex_binding`/`vertex_attribute`/etc. field names differ from what Task 3's code assumes — cross-check against `vulkan_host.hpp`'s real declarations, already read in full during this plan's fact-gathering).

- [ ] **Step 5: Regression-check the fixed-function path**

```bash
cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-triangle-test.exe
```

Confirm the FF triangle still renders identically to before this task (no shaders are created/bound by this test, so `use_programmable` stays `false` throughout — this exercises the `else` branch only, proving no regression).

- [ ] **Step 6: Commit**

```bash
git add src/windows-emulator/devices/d3d9_host.hpp src/windows-emulator/devices/d3d9_host.cpp
git commit --no-gpg-sign -m "feat(d3d9): add lazily-translated programmable pipeline path to execute_draw"
```

---

## Task 4: UMD DDI wiring for shader creation

**Goal:** `CreateVertexShader`/`CreatePixelShader` calls from the guest app actually reach `d3d9_host::create_vertex_shader`/`create_pixel_shader` with real token data — today nothing does, since `pfnCreateVertexShaderFunc`/`pfnCreatePixelShader` are unwired even though the wire protocol and host dispatch already exist.

**Files:**
- Modify: `src/samples/sogen-d3d9-umd/d3d9_ddi.hpp` (add `D3DDDIARG_DELETEVERTEXSHADERFUNC`)
- Modify: `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp` (add 4 marshaling functions, wire slots 42/43/67/68)

**Acceptance Criteria:**
- [ ] `umd_CreateVertexShaderFunc`/`umd_CreatePixelShader`/`umd_DeleteVertexShaderFunc`/`umd_DeletePixelShader` exist and are wired at slots 42/43/67/68 respectively.
- [ ] A live diagnostic (added temporarily, per Step 3) confirms `D3DDDIARG_CREATEVERTEXSHADERFUNC`/`CREATEPIXELSHADERFUNC`'s existing field layout (`ShaderHandle` out-param, `Values[0]`/`CodeSize` = byte size, tokens trailing in memory) is actually what the real runtime sends — not assumed correct just because the struct compiles.
- [ ] If the live diagnostic contradicts the existing struct layout, the struct is corrected before proceeding (same "verify, don't guess" bar as `D3DDDIARG_LOCK`/`PRESENT` this session).
- [ ] `CreateVertexShader`/`CreatePixelShader` return `S_OK` from a real guest D3D9 app for well-formed SM2 bytecode (verified in Task 6, not this task, since Task 5's test doesn't exist yet).

**Verify:** `x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 sogen_d3d9_umd.cpp sogen_d3d9_umd.def -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll` compiles clean; the live-diagnostic run in Step 3 shows byte-for-byte matching field values (documented inline in the commit or `HANDOFF_MACBOOK.md`).

**Steps:**

- [ ] **Step 1: Add the missing `D3DDDIARG_DELETEVERTEXSHADERFUNC` struct**

In `src/samples/sogen-d3d9-umd/d3d9_ddi.hpp`, right after the existing `D3DDDIARG_SETVERTEXSHADERFUNC` block (after its closing `} D3DDDIARG_SETVERTEXSHADERFUNC;` line), add:

```c
typedef struct _D3DDDIARG_DELETEVERTEXSHADERFUNC
{
    HANDLE ShaderHandle;
} D3DDDIARG_DELETEVERTEXSHADERFUNC;
```

(`D3DDDIARG_CREATEVERTEXSHADERFUNC`/`D3DDDIARG_CREATEPIXELSHADERFUNC`/`D3DDDIARG_DELETEPIXELSHADERFUNC` already exist — no changes needed to those three.)

- [ ] **Step 2: Write the four marshaling functions**

In `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp`, add near the existing `umd_SetVertexShaderFunc`/`umd_SetVertexShaderDecl` functions:

```cpp
    HRESULT APIENTRY umd_CreateVertexShaderFunc(HANDLE /*hDevice*/, D3DDDIARG_CREATEVERTEXSHADERFUNC* pArgs)
    {
        if (pArgs == nullptr)
        {
            return E_INVALIDARG;
        }
        const UINT token_size_bytes = pArgs->Values[0];
        const auto* tokens = reinterpret_cast<const uint8_t*>(pArgs + 1);

        std::vector<uint8_t> buf(sizeof(d3d9c::create_shader_request) + token_size_bytes);
        auto* req = reinterpret_cast<d3d9c::create_shader_request*>(buf.data());
        req->token_size_bytes = token_size_bytes;
        req->reserved = 0;
        std::memcpy(buf.data() + sizeof(*req), tokens, token_size_bytes);

        d3d9c::create_shader_response resp{};
        bridge_call(gb::ioctl_d3d9_create_vertex_shader, buf.data(), static_cast<DWORD>(buf.size()), &resp, sizeof(resp));
        pArgs->ShaderHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(resp.shader));
        return resp.hr;
    }

    HRESULT APIENTRY umd_CreatePixelShader(HANDLE /*hDevice*/, D3DDDIARG_CREATEPIXELSHADERFUNC* pArgs)
    {
        if (pArgs == nullptr)
        {
            return E_INVALIDARG;
        }
        const UINT token_size_bytes = pArgs->CodeSize;
        const auto* tokens = reinterpret_cast<const uint8_t*>(pArgs + 1);

        std::vector<uint8_t> buf(sizeof(d3d9c::create_shader_request) + token_size_bytes);
        auto* req = reinterpret_cast<d3d9c::create_shader_request*>(buf.data());
        req->token_size_bytes = token_size_bytes;
        req->reserved = 0;
        std::memcpy(buf.data() + sizeof(*req), tokens, token_size_bytes);

        d3d9c::create_shader_response resp{};
        bridge_call(gb::ioctl_d3d9_create_pixel_shader, buf.data(), static_cast<DWORD>(buf.size()), &resp, sizeof(resp));
        pArgs->ShaderHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(resp.shader));
        return resp.hr;
    }

    HRESULT APIENTRY umd_DeleteVertexShaderFunc(HANDLE /*hDevice*/, CONST D3DDDIARG_DELETEVERTEXSHADERFUNC* /*pArgs*/)
    {
        // No host-side shader-destroy wire call exists yet (matching create_resource/destroy_resource's
        // asymmetry precedent elsewhere in this file is out of scope for this slice) -- the shader_entry
        // simply stays allocated for the process lifetime, same as every other DDI object this UMD
        // never explicitly frees today.
        return S_OK;
    }

    HRESULT APIENTRY umd_DeletePixelShader(HANDLE /*hDevice*/, CONST D3DDDIARG_DELETEPIXELSHADERFUNC* /*pArgs*/)
    {
        return S_OK;
    }
```

Add the `#include <vector>` and `#include <cstring>` at the top of the file if not already present (check first — this file already uses `std::vector`/`std::memcpy` elsewhere per the `umd_SetVertexShaderConst` pattern, so they're very likely already included).

- [ ] **Step 3: Wire the slots and add a temporary live-verification diagnostic**

In the `slots[...]` block (`sogen_d3d9_umd.cpp`, near line 831), add:

```cpp
            slots[42] = reinterpret_cast<void*>(&umd_CreateVertexShaderFunc); // pfnCreateVertexShaderFunc
            slots[43] = reinterpret_cast<void*>(&umd_DeleteVertexShaderFunc); // pfnDeleteVertexShaderFunc
            slots[67] = reinterpret_cast<void*>(&umd_CreatePixelShader);      // pfnCreatePixelShader
            slots[68] = reinterpret_cast<void*>(&umd_DeletePixelShader);      // pfnDeletePixelShader
```

Temporarily add a `log_line` at the top of `umd_CreateVertexShaderFunc` (before the `nullptr` check) printing `pArgs`, `pArgs->Values[0]`, and the first 16 bytes of the trailing token data as hex — this is the live forcing-function confirmation this task's Acceptance Criteria requires. Example:

```cpp
        log_line("[sogen-d3d9-umd] CreateVertexShaderFunc pArgs=%p Values[0]=%u first16=%02x%02x...\n", ...);
```

Rebuild, stage, and run against Task 5's guest test once it exists (this diagnostic stays in place until Task 6's verification pass; remove it then). If this task is being executed before Task 5 exists, defer running this diagnostic to Task 6 and note that in a comment — do not skip writing the diagnostic itself.

- [ ] **Step 4: Build**

```bash
cd src/samples/sogen-d3d9-umd
x86_64-w64-mingw32-g++ -shared -O2 -std=c++20 sogen_d3d9_umd.cpp sogen_d3d9_umd.def \
    -static -static-libgcc -static-libstdc++ -o sogen_d3d9um-x64.dll
```

- [ ] **Step 5: Commit**

```bash
git add src/samples/sogen-d3d9-umd/d3d9_ddi.hpp src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp
git commit --no-gpg-sign -m "feat(d3d9): wire pfnCreateVertexShaderFunc/pfnCreatePixelShader DDI slots"
```

---

## Task 5: Guest shader test — real `D3DCompile()` passthrough triangle

**Goal:** A new, standalone guest test that compiles a tiny HLSL passthrough shader pair with the real `D3DCompile()`, creates/binds them via the DDI slots wired in Task 4, and draws a triangle through the programmable pipeline built in Task 3.

**Files:**
- Create: `src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp`
- Modify: `src/samples/sogen-d3d9-umd/README.md` (document the new test's build/stage/run commands)

**Acceptance Criteria:**
- [ ] The test compiles a vertex shader (`output.pos = input.pos; output.color = input.color;`) and pixel shader (`return input.color;`) from embedded HLSL source via `D3DCompile(..., "vs_2_0", ...)`/`D3DCompile(..., "ps_2_0", ...)`.
- [ ] `D3DCompile()` returns `S_OK` for both shaders (verified by the test's own `printf` + Task 6's run).
- [ ] The test uses `D3DFVF_XYZ|D3DFVF_DIFFUSE` (plain clip-space position, no RHW) with vertex positions authored directly in `[-1, 1]` NDC range.
- [ ] `x86_64-w64-mingw32-g++` compiles and links the test with `-ld3dcompiler_43 -ld3d9`.
- [ ] `README.md` documents the new test's build/stage/run commands, matching the existing FF triangle test's documentation style.

**Verify:** `x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_shader_test.cpp -static -static-libgcc -static-libstdc++ -o d3d9-shader-test-x64.exe -ld3d9 -ld3dcompiler_43` compiles and links with no errors.

**Steps:**

- [ ] **Step 1: Write the test**

`src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp`:

```cpp
// D3D9-over-Vulkan de-risk slice: programmable-shader triangle (see
// docs/superpowers/plans/2026-07-03-vkd3d-shader-derisk.md). Real D3DCompile()-produced SM2 vertex +
// pixel shaders, position+color passthrough only (no constant registers, no textures) -- deliberately
// separate from d3d9_triangle_test.cpp so the fixed-function triangle stays a working regression
// baseline throughout this slice's development.

#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>

namespace
{
    const char* const k_vertex_shader_hlsl = R"(
struct VSInput { float3 pos : POSITION; float4 color : COLOR0; };
struct VSOutput { float4 pos : POSITION; float4 color : COLOR0; };
VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}
)";

    const char* const k_pixel_shader_hlsl = R"(
struct PSInput { float4 pos : POSITION; float4 color : COLOR0; };
float4 main(PSInput input) : COLOR0
{
    return input.color;
}
)";
} // namespace

int main()
{
    setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
    printf("[d3d9-shader-test] start\n");

    WNDCLASSA wc{};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "sogend3d9shadertest";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "shader-triangle", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        printf("[d3d9-shader-test] FAIL: Direct3DCreate9 returned null\n");
        return 1;
    }

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.hDeviceWindow = hwnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
    printf("[d3d9-shader-test] CreateDevice hr=0x%08lx\n", static_cast<unsigned long>(hr));
    if (FAILED(hr) || !dev)
    {
        d3d->Release();
        return 1;
    }

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* vs_errors = nullptr;
    HRESULT hvsc = D3DCompile(k_vertex_shader_hlsl, strlen(k_vertex_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "vs_2_0", 0, 0, &vs_blob, &vs_errors);
    printf("[d3d9-shader-test] D3DCompile(vs) hr=0x%08lx\n", static_cast<unsigned long>(hvsc));
    if (FAILED(hvsc))
    {
        if (vs_errors)
        {
            printf("[d3d9-shader-test] VS compile errors: %s\n", static_cast<const char*>(vs_errors->GetBufferPointer()));
        }
        return 1;
    }

    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* ps_errors = nullptr;
    HRESULT hpsc = D3DCompile(k_pixel_shader_hlsl, strlen(k_pixel_shader_hlsl), nullptr, nullptr, nullptr, "main",
                              "ps_2_0", 0, 0, &ps_blob, &ps_errors);
    printf("[d3d9-shader-test] D3DCompile(ps) hr=0x%08lx\n", static_cast<unsigned long>(hpsc));
    if (FAILED(hpsc))
    {
        if (ps_errors)
        {
            printf("[d3d9-shader-test] PS compile errors: %s\n", static_cast<const char*>(ps_errors->GetBufferPointer()));
        }
        return 1;
    }

    IDirect3DVertexShader9* vs = nullptr;
    HRESULT hcvs = dev->CreateVertexShader(static_cast<const DWORD*>(vs_blob->GetBufferPointer()), &vs);
    printf("[d3d9-shader-test] CreateVertexShader hr=0x%08lx\n", static_cast<unsigned long>(hcvs));

    IDirect3DPixelShader9* ps = nullptr;
    HRESULT hcps = dev->CreatePixelShader(static_cast<const DWORD*>(ps_blob->GetBufferPointer()), &ps);
    printf("[d3d9-shader-test] CreatePixelShader hr=0x%08lx\n", static_cast<unsigned long>(hcps));

    if (FAILED(hcvs) || FAILED(hcps))
    {
        dev->Release();
        d3d->Release();
        return 1;
    }

    struct Vertex
    {
        float x, y, z;
        DWORD color;
    };
    constexpr DWORD kFvf = D3DFVF_XYZ | D3DFVF_DIFFUSE;
    IDirect3DVertexBuffer9* vb = nullptr;
    dev->CreateVertexBuffer(3 * sizeof(Vertex), 0, kFvf, D3DPOOL_DEFAULT, &vb, nullptr);
    if (vb)
    {
        Vertex* verts = nullptr;
        vb->Lock(0, 3 * sizeof(Vertex), reinterpret_cast<void**>(&verts), 0);
        if (verts)
        {
            verts[0] = {0.0f, 0.5f, 0.5f, D3DCOLOR_XRGB(255, 0, 0)};
            verts[1] = {0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 255, 0)};
            verts[2] = {-0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 0, 255)};
            vb->Unlock();
        }

        dev->SetFVF(kFvf);
        dev->SetStreamSource(0, vb, 0, sizeof(Vertex));
        dev->SetVertexShader(vs);
        dev->SetPixelShader(ps);

        dev->BeginScene();
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(64, 128, 255), 1.0f, 0);
        HRESULT hdp = dev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, 1);
        printf("[d3d9-shader-test] DrawPrimitive hr=0x%08lx\n", static_cast<unsigned long>(hdp));
        dev->EndScene();

        HRESULT hp = dev->Present(nullptr, nullptr, nullptr, nullptr);
        printf("[d3d9-shader-test] Present hr=0x%08lx\n", static_cast<unsigned long>(hp));

        vb->Release();
    }

    if (vs)
    {
        vs->Release();
    }
    if (ps)
    {
        ps->Release();
    }
    dev->Release();
    d3d->Release();
    printf("[d3d9-shader-test] done\n");
    return 0;
}
```

- [ ] **Step 2: Build**

```bash
cd src/samples/sogen-d3d9-umd
x86_64-w64-mingw32-g++ -O2 -std=c++20 d3d9_shader_test.cpp \
    -static -static-libgcc -static-libstdc++ -o d3d9-shader-test-x64.exe -ld3d9 -ld3dcompiler_43
```

Fix any compile errors (e.g. `D3DBlob`/`D3DCompile` signature mismatches against the real mingw-w64 header at `/opt/homebrew/Cellar/mingw-w64/14.0.0_1/toolchain-x86_64/x86_64-w64-mingw32/include/d3dcompiler.h`, already confirmed present).

- [ ] **Step 3: Stage it**

```bash
cp d3d9-shader-test-x64.exe ../../../build/release/artifacts/root/filesys/c/d3d9-shader-test.exe
```

- [ ] **Step 4: Update README.md**

Add a new section to `src/samples/sogen-d3d9-umd/README.md` (after the existing Build/Stage/Run sections), documenting the new test's build/stage/run commands in the same style as the existing entries.

- [ ] **Step 5: Commit**

```bash
git add src/samples/sogen-d3d9-umd/d3d9_shader_test.cpp src/samples/sogen-d3d9-umd/README.md
git commit --no-gpg-sign -m "test(d3d9): add D3DCompile()-based programmable-shader triangle test"
```

---

## Task 6: End-to-end verification, diagnostic cleanup, docs

**Goal:** The full chain — `D3DCompile()` → `CreateVertexShader`/`CreatePixelShader` → vkd3d-shader translation → programmable Vulkan pipeline → `DrawPrimitive` → `Present` — produces a real, analytically-verified triangle on screen, with no regressions and no leftover debug diagnostics.

**Files:**
- Modify: `src/windows-emulator/devices/d3d9_host.cpp` (temporary diagnostic, added and removed within this task)
- Modify: `src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp` (remove Task 4's temporary `log_line` diagnostic)
- Modify: `HANDOFF_MACBOOK.md` (document the completed milestone)

**Acceptance Criteria:**
- [ ] `d3d9-shader-test.exe` runs to completion with `DrawPrimitive hr=0x00000000` and `Present hr=0x00000000`.
- [ ] A host-side analytic readback at screen pixel `(320, 240)` (NDC `(0,0)`) matches the hand-computed barycentric blend of the test's three vertex colors at that exact point: `B=0x40 G=0x40 R=0x80 A=0xFF` (see Task 6 Step 3 for the full derivation) — within ±2 per channel for rounding.
- [ ] Task 4's temporary `log_line` diagnostic in `umd_CreateVertexShaderFunc` is removed.
- [ ] Any temporary diagnostic added to `d3d9_host.cpp` for the readback check in this task is removed after confirming correctness.
- [ ] `analyzer -e root -c c:/d3d9-triangle-test.exe` (fixed-function test) still passes with no regression.
- [ ] `analyzer -e root -s c:/test-sample.exe` smoke test still reports 26/26 `Success`.
- [ ] `HANDOFF_MACBOOK.md` documents the milestone: real SM2 shader translation working end-to-end, what remains deferred (constant buffers/UBOs, textures, SM3.0, WoW64/x86 shader path).

**Verify:** `cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-shader-test.exe` shows the centroid-pixel value matching the expected barycentric color average; `./analyzer -e root -s c:/test-sample.exe` shows 26/26.

**Steps:**

- [ ] **Step 1: Add a temporary readback diagnostic**

In `d3d9_host.cpp`'s `execute_draw`, right after the `readback_render_target` call (the existing `if (this->vulkan_.readback_render_target(...) == 0) { rt.backing = std::move(pixels); }` block), temporarily add:

```cpp
        if (use_programmable)
        {
            const size_t centroid_offset = (static_cast<size_t>(rt.height / 2) * rt.width + rt.width / 2) * 4;
            if (centroid_offset + 4 <= rt.backing.size())
            {
                const auto* p = reinterpret_cast<const uint8_t*>(rt.backing.data() + centroid_offset);
                std::fprintf(stderr, "[d3d9_host][DIAG] centroid B=%02X G=%02X R=%02X A=%02X\n", p[0], p[1], p[2], p[3]);
            }
        }
```

- [ ] **Step 2: Rebuild and run**

```bash
cmake --build --preset=release
cd build/release/artifacts && ./analyzer -e root -c c:/d3d9-shader-test.exe 2>&1 | tee /tmp/task6-run.log
```

- [ ] **Step 3: Compute the expected sampled color and compare**

Step 1's diagnostic samples screen pixel `(rt.width/2, rt.height/2)` = `(320, 240)`, which corresponds to NDC point `(0, 0)` under the standard D3D9 viewport transform (`screen_x = (ndc_x*0.5+0.5)*width`, `screen_y = (0.5-ndc_y*0.5)*height`). This is **not** the triangle's geometric centroid — compute the barycentric weights of NDC `(0,0)` against Task 5's actual vertex positions `A=(0,0.5)` (red), `B=(0.5,-0.5)` (green), `C=(-0.5,-0.5)` (blue) directly:

```
D   = (B.y-C.y)(A.x-C.x) + (C.x-B.x)(A.y-C.y) = (0)(0.5) + (-1)(1) = -1
w_A = [(B.y-C.y)(P.x-C.x) + (C.x-B.x)(P.y-C.y)] / D = [(0)(0.5) + (-1)(0.5)] / -1 = 0.5
w_B = [(C.y-A.y)(P.x-C.x) + (A.x-C.x)(P.y-C.y)] / D = [(-1)(0.5) + (0.5)(0.5)] / -1 = 0.25
w_C = 1 - w_A - w_B = 0.25
```

Expected color = `0.5*red + 0.25*green + 0.25*blue` = R: `0.5*255 ≈ 128 (0x80)`, G: `0.25*255 ≈ 64 (0x40)`, B: `0.25*255 ≈ 64 (0x40)`, A: `0xFF` (opaque). In the project's established BGRA8 log order (matching the FF triangle test's own `B=FF G=80 R=40` comment convention): **expected `B=40 G=40 R=80 A=FF`**. Compare this exact value against Step 2's logged `[DIAG] centroid ...` line (within ±2 per channel for rounding). If they don't match, debug (check `d3d9_shader_translator`'s varying-map output order, the vertex attribute `format`/`offset` values in Task 3's `ensure_programmable_pipeline`, or the DDI marshaling in Task 4) before proceeding — this is the actual correctness gate for the whole slice.

- [ ] **Step 4: Remove all temporary diagnostics**

Remove Step 1's diagnostic from `d3d9_host.cpp`, and Task 4 Step 3's `log_line` from `umd_CreateVertexShaderFunc` in `sogen_d3d9_umd.cpp`. Rebuild and re-run to confirm the test still passes without the diagnostics (proves the diagnostics weren't load-bearing).

- [ ] **Step 5: Regression sweep**

```bash
cd build/release/artifacts
./analyzer -e root -c c:/d3d9-triangle-test.exe
./analyzer -e root -s c:/test-sample.exe
```

Confirm both are unchanged from their pre-this-plan baselines.

- [ ] **Step 6: Update HANDOFF_MACBOOK.md**

Add a new section documenting: the milestone (real SM2 shader translation, end to end); the vkd3d-shader integration shape (autotools, scan+varying-map+compile); what's still deferred (constant buffers/UBOs and the general binding contract, textures/samplers, SM3.0, the WoW64/x86 shader path, a pipeline-key system for multiple distinct shader pairs beyond this slice's single-pair-at-a-time caching — note the cache Task 3 built is already keyed per-pair and doesn't need rework, just more pairs need testing).

- [ ] **Step 7: Final commit**

```bash
git add src/windows-emulator/devices/d3d9_host.cpp src/samples/sogen-d3d9-umd/sogen_d3d9_umd.cpp HANDOFF_MACBOOK.md
git commit --no-gpg-sign -m "feat(d3d9): verify real SM2 shader translation end-to-end; remove diagnostics"
```
