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
