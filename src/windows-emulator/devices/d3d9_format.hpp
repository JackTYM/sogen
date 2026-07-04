#pragma once

#include <cstdint>

namespace sogen
{
    // Maps a D3DFORMAT value (as sent over the wire by the guest UMD) to the matching VkFormat
    // constant. Returns false for formats not in the supported set, leaving out_vk_format untouched,
    // so callers can fail cleanly instead of silently falling back to some default format.
    bool d3d9_format_to_vulkan(uint32_t d3dfmt, uint32_t& out_vk_format);
} // namespace sogen
