#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sogen
{
    // Within a shader stage's descriptor set (VS = set 0, PS = set 1; see d3d9_host.cpp's
    // ensure_programmable_pipeline and this header's own translate_d3d9_shader_pair, below), the
    // UBO/sampler bindings are laid out identically: binding 0 = float-const CBV, binding 1 = sampler
    // s0, binding 2 = int-const CBV, binding 3 = bool-const CBV, and each additional sampler stage k>=1
    // at binding 3+k. BOTH shader stages share this ONE formula, each in its own descriptor set.
    //
    // The formula is centralized here -- rather than being duplicated independently in
    // d3d9_shader_translator.cpp (which builds each stage's combined-image-sampler bindings) and
    // d3d9_host.cpp (which builds the matching Vulkan descriptor set layout/pool) -- so those sides
    // can never silently drift on which Vulkan binding number corresponds to which D3D9 sampler
    // register. A drift here wouldn't produce a build error or a validation-layer message; it would
    // produce the exact "graceful degradation" failure this feature exists to avoid (a silently
    // skipped draw or a wrong pixel, no error visible to the guest).
    constexpr uint32_t sampler_binding_for_stage(const uint32_t stage)
    {
        return stage == 0 ? 1u : 3u + stage;
    }

    // The two stages have genuinely different D3D9 sampler-register counts, so their caps are separate
    // constants rather than one shared value:
    //
    //   * The pixel stage covers D3D9's full ps_2_0/ps_3_0 sampler file, s0..s15. This is not headroom
    //     for its own sake: vkd3d-shader fails the WHOLE pixel-shader compile with
    //     "E2000: Could not find descriptor binding for type 0, space 0, registers [k:k]" if the shader
    //     statically samples any register k this side did not declare a combined-image-sampler binding
    //     for. That failure propagates as translate_d3d9_shader_pair -> false ->
    //     ensure_programmable_pipeline -> nullptr -> execute_draw silently returns D3D_OK, i.e. the draw
    //     is dropped with no error visible to the guest and no pixels on screen. Real content routinely
    //     exceeds four samplers (Modern Warfare 2's world/model shaders sample s4 and beyond), so a cap
    //     below D3D9's own is a correctness bug, not a resource-budget tradeoff.
    //   * The vertex stage covers D3DVERTEXTEXTURESAMPLER0..3 only -- SM3.0 vertex texture fetch defines
    //     exactly four such registers, so four is the real hardware cap here, not an arbitrary limit.
    //
    // Over-declaring a binding a given shader does not sample is inert (vkd3d only emits a SPIR-V
    // sampler variable for a statically-referenced register, and Vulkan permits a layout/set to declare
    // bindings the bound shader never accesses), so the pixel stage declaring all 16 unconditionally
    // costs nothing for the common single-texture shader beyond descriptor-pool capacity, which
    // d3d9_host.cpp's ensure_frame_descriptor_pool already derives from these constants.
    constexpr uint32_t max_ps_sampler_stages = 16;
    constexpr uint32_t max_vs_sampler_stages = 4;

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
    bool translate_d3d9_shader_pair(const void* vs_tokens, size_t vs_token_size_bytes, const void* ps_tokens, size_t ps_token_size_bytes,
                                    shader_pair_spirv& out);

    // Disassembles one SM1-3 token blob back to D3D shader assembly text (vkd3d-shader's own D3D_ASM
    // target). A guest ships shaders as opaque bytecode, so when a title renders the wrong pixels with
    // every counter clean -- no dropped draws, no unbound samplers, no truncated constant file -- the
    // shader's own arithmetic is the only remaining place the answer can be, and this is the only way
    // to read it. Returns false if vkd3d-shader cannot parse the blob.
    bool disassemble_d3d9_shader(const void* tokens, size_t token_size_bytes, std::string& out_text);
} // namespace sogen
