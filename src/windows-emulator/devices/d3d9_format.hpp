#pragma once

#include <cstdint>

namespace sogen
{
    // Maps a D3DFORMAT value (as sent over the wire by the guest UMD) to the matching VkFormat
    // constant. Returns false for formats not in the supported set, leaving out_vk_format untouched,
    // so callers can fail cleanly instead of silently falling back to some default format.
    bool d3d9_format_to_vulkan(uint32_t d3dfmt, uint32_t& out_vk_format);

    // Bytes occupied by a single texel of `vk_format`, for the non-block-compressed VkFormat constants
    // d3d9_format_to_vulkan can produce. Shared across translation units (d3d9_host.cpp and
    // vulkan_host.cpp) so render-target sizing/readback paths agree on per-format stride instead of
    // hardcoding 4 bytes/texel BGRA8. Render targets are never block-compressed, so a plain
    // bytes-per-texel value is sufficient; returns 0 for block-compressed or unrecognized formats so
    // callers can fail cleanly (texture block sizing stays in d3d9_host.cpp's vk_texture_data_size).
    uint32_t vk_format_bytes_per_texel(uint32_t vk_format);

    // Maps a D3DDECLTYPE value (a D3DVERTEXELEMENT9::Type field, see d3d9_cmd::vertex_element) to the
    // matching VkFormat constant. Minimal, YAGNI set -- only the types this project's own vertex
    // declarations currently use; extend as new declaration shapes are needed. Returns false for any
    // other D3DDECLTYPE, leaving out_vk_format untouched, mirroring d3d9_format_to_vulkan above.
    bool d3d9_decl_type_to_vulkan(uint32_t d3ddecltype, uint32_t& out_vk_format);
} // namespace sogen
