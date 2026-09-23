#pragma once

#include <cstdint>

namespace sogen
{
    // Maps a D3DFORMAT value (as sent over the wire by the guest UMD) to the matching VkFormat
    // constant. Returns false for formats not in the supported set, leaving out_vk_format untouched,
    // so callers can fail cleanly instead of silently falling back to some default format.
    bool d3d9_format_to_vulkan(uint32_t d3dfmt, uint32_t& out_vk_format);

    // The sRGB-encoding VkFormat that shares `d3dfmt`'s memory layout, for the formats real D3D9
    // hardware applies D3DRS_SRGBWRITEENABLE's linear->sRGB conversion to. A colour attachment viewed
    // through this format gets that conversion from Vulkan itself on every write, which is what makes
    // D3DRS_SRGBWRITEENABLE implementable without touching a single shader. Returns false (leaving
    // out_vk_format untouched) for every format that has no sRGB counterpart -- the same set real D3D9
    // documents the render state as being ignored for -- so callers keep the linear format for those.
    bool d3d9_format_to_vulkan_srgb(uint32_t d3dfmt, uint32_t& out_vk_format);

    // Bytes occupied by a single texel of `vk_format`, for the non-block-compressed VkFormat constants
    // d3d9_format_to_vulkan can produce. Shared across translation units (d3d9_host.cpp and
    // vulkan_host.cpp) so render-target sizing/readback paths agree on per-format stride instead of
    // hardcoding 4 bytes/texel BGRA8. Render targets are never block-compressed, so a plain
    // bytes-per-texel value is sufficient; returns 0 for block-compressed or unrecognized formats so
    // callers can fail cleanly (texture block sizing stays in d3d9_host.cpp's vk_texture_data_size).
    uint32_t vk_format_bytes_per_texel(uint32_t vk_format);

    // Maps a D3DDECLTYPE value (a D3DVERTEXELEMENT9::Type field, see d3d9_cmd::vertex_element) to the
    // matching VkFormat constant. Covers every D3DDECLTYPE except the D3DDECLTYPE_UNUSED terminator;
    // returning false for a type a real declaration uses is not a graceful degradation but a hard
    // failure, because parse_vertex_decl's skip leaves a gap in the vertex-input locations that
    // MoltenVK rejects the entire pipeline over (see the mapping's own comment in the .cpp). Returns
    // false for any other D3DDECLTYPE, leaving out_vk_format untouched, mirroring d3d9_format_to_vulkan.
    bool d3d9_decl_type_to_vulkan(uint32_t d3ddecltype, uint32_t& out_vk_format);

    // Component swizzle (VkComponentSwizzle values) an image view needs so a texture sampled through
    // d3d9_format_to_vulkan's chosen VkFormat reproduces real D3D9's channel placement. Most formats
    // are a straight identity mapping, but the single/dual-channel luminance/alpha formats (D3DFMT_A8,
    // D3DFMT_L8, D3DFMT_A8L8) have no direct VkFormat equivalent -- they're stored in R8/R8G8_UNORM,
    // whose channels don't line up with where D3D9 expects the data to sample from (e.g. D3DFMT_A8's
    // single byte is real D3D9's ALPHA channel with RGB=0, not R8_UNORM's default R-channel-with-
    // alpha-forced-to-1) -- and the alpha-less D3DFMT_X8R8G8B8, whose byte-exact VkFormat does have an
    // alpha channel that D3D9 says must not be sampled. Always succeeds; unrecognized formats get the
    // identity mapping. A view carrying a non-identity mapping must never be used as a render-pass
    // attachment (Vulkan requires identity swizzle there), which is why resource_entry keeps the
    // shader-read view separate from the attachment ones.
    struct d3d9_format_swizzle
    {
        uint32_t r, g, b, a;
    };

    d3d9_format_swizzle d3d9_format_to_vulkan_swizzle(uint32_t d3dfmt);
} // namespace sogen
