#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sogen
{
    // Within a shader stage's descriptor set (VS = set 0, PS = set 1; see d3d9_host.cpp's
    // ensure_programmable_pipeline and this header's own translate_d3d9_shader_pair, below), the four
    // UBO/sampler bindings are laid out identically: binding 0 = float-const CBV, binding 1 = sampler
    // s0, binding 2 = int-const CBV, binding 3 = bool-const CBV, and each additional sampler stage k>=1
    // at binding 3+k. max_sampler_stages caps how many D3D9 sampler registers (s0..s{N-1}) this scheme
    // covers. BOTH shader stages use this ONE formula: the pixel stage samples s0..s3 (diffuse+normal,
    // multi-texturing) and the vertex stage samples D3DVERTEXTEXTURESAMPLER0..3 (SM3.0 vertex texture
    // fetch, e.g. tex2Dlod height-map displacement), each in its own descriptor set.
    //
    // The formula is centralized here -- rather than being duplicated independently in
    // d3d9_shader_translator.cpp (which builds each stage's combined-image-sampler bindings) and
    // d3d9_host.cpp (which builds the matching Vulkan descriptor set layout/pool) -- so those sides
    // can never silently drift on which Vulkan binding number corresponds to which D3D9 sampler
    // register. A drift here wouldn't produce a build error or a validation-layer message; it would
    // produce the exact "graceful degradation" failure this feature exists to avoid (a silently
    // skipped draw or a wrong pixel, no error visible to the guest).
    //
    // Raising max_sampler_stages beyond 4 (toward D3D9's 16-sampler cap) requires, in addition to
    // bumping this constant: (a) verifying d3d9_host.cpp's vs_bindings/ps_bindings are actually
    // generated from this constant/function (not hand-typed aggregate entries), and (b) bumping the
    // descriptor pool's combined-image-sampler count (also in d3d9_host.cpp) to match.
    constexpr uint32_t max_sampler_stages = 4;

    constexpr uint32_t sampler_binding_for_stage(const uint32_t stage)
    {
        return stage == 0 ? 1u : 3u + stage;
    }

    // Stage-named aliases: identical formula/cap for both stages (the layout within each set is the
    // same). They forward to the single definitions above, so the two stages and the two sides
    // (translator/host) can never drift; the distinct names only document intent at each call site.
    constexpr uint32_t max_ps_sampler_stages = max_sampler_stages;
    constexpr uint32_t max_vs_sampler_stages = max_sampler_stages;

    constexpr uint32_t ps_sampler_binding_for_stage(const uint32_t stage)
    {
        return sampler_binding_for_stage(stage);
    }

    constexpr uint32_t vs_sampler_binding_for_stage(const uint32_t stage)
    {
        return sampler_binding_for_stage(stage);
    }

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
