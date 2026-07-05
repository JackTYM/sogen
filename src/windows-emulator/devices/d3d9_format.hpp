#pragma once

#include <cstdint>

namespace sogen
{
    // Maps a D3DFORMAT value (as sent over the wire by the guest UMD) to the matching VkFormat
    // constant. Returns false for formats not in the supported set, leaving out_vk_format untouched,
    // so callers can fail cleanly instead of silently falling back to some default format.
    bool d3d9_format_to_vulkan(uint32_t d3dfmt, uint32_t& out_vk_format);

    // Maps a D3DDECLTYPE value (a D3DVERTEXELEMENT9::Type field, see d3d9_cmd::vertex_element) to the
    // matching VkFormat constant. Minimal, YAGNI set -- only the types this project's own vertex
    // declarations currently use; extend as new declaration shapes are needed. Returns false for any
    // other D3DDECLTYPE, leaving out_vk_format untouched, mirroring d3d9_format_to_vulkan above.
    bool d3d9_decl_type_to_vulkan(uint32_t d3ddecltype, uint32_t& out_vk_format);
} // namespace sogen
