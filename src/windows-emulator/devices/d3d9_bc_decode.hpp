#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace sogen
{
    bool vk_format_is_bc1_to_bc3(uint32_t vk_format);

    // Expands one tightly packed BC1/BC2/BC3 surface (the VkFormats DXT1/DXT3/DXT5 map to) of
    // `depth` slices into tightly packed R8G8B8A8, for GPUs whose Metal/Vulkan implementation has no
    // block-compressed sampling (every iOS GPU). `dst` must hold width * height * depth * 4 bytes.
    void decode_bc1_to_bc3_rgba8(uint32_t vk_format, std::span<const std::byte> src, uint32_t width, uint32_t height, uint32_t depth,
                                 std::span<std::byte> dst);
}
