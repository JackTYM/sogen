#include "d3d9_bc_decode.hpp"

#include <vulkan/vulkan_core.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace sogen
{
    namespace
    {
        using rgba = std::array<uint8_t, 4>;

        constexpr size_t bc1_block_bytes = 8;
        constexpr size_t bc2_bc3_block_bytes = 16;

        uint16_t read_u16(const std::byte* data)
        {
            uint16_t value{};
            std::memcpy(&value, data, sizeof(value));
            return value;
        }

        rgba expand_rgb565(const uint16_t color)
        {
            const auto r = static_cast<uint8_t>((color >> 11) & 0x1F);
            const auto g = static_cast<uint8_t>((color >> 5) & 0x3F);
            const auto b = static_cast<uint8_t>(color & 0x1F);
            return {static_cast<uint8_t>((r << 3) | (r >> 2)), static_cast<uint8_t>((g << 2) | (g >> 4)),
                    static_cast<uint8_t>((b << 3) | (b >> 2)), 0xFF};
        }

        rgba mix(const rgba& a, const rgba& b, const unsigned weight_a, const unsigned weight_b)
        {
            const unsigned total = weight_a + weight_b;
            return {static_cast<uint8_t>((a[0] * weight_a + b[0] * weight_b) / total),
                    static_cast<uint8_t>((a[1] * weight_a + b[1] * weight_b) / total),
                    static_cast<uint8_t>((a[2] * weight_a + b[2] * weight_b) / total), 0xFF};
        }

        // BC2/BC3 colour blocks are always four-colour; only BC1's c0 <= c1 selects the punch-through form.
        std::array<rgba, 4> bc1_palette(const std::byte* block, const bool allow_punch_through)
        {
            const uint16_t c0 = read_u16(block);
            const uint16_t c1 = read_u16(block + 2);
            const rgba color0 = expand_rgb565(c0);
            const rgba color1 = expand_rgb565(c1);

            if (allow_punch_through && c0 <= c1)
            {
                return {color0, color1, mix(color0, color1, 1, 1), rgba{0, 0, 0, 0}};
            }
            return {color0, color1, mix(color0, color1, 2, 1), mix(color0, color1, 1, 2)};
        }

        std::array<uint8_t, 8> bc3_alpha_palette(const uint8_t a0, const uint8_t a1)
        {
            std::array<uint8_t, 8> palette{a0, a1};
            if (a0 > a1)
            {
                for (unsigned i = 1; i <= 6; ++i)
                {
                    palette[1 + i] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
                }
            }
            else
            {
                for (unsigned i = 1; i <= 4; ++i)
                {
                    palette[1 + i] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
                }
                palette[6] = 0;
                palette[7] = 0xFF;
            }
            return palette;
        }

        void decode_block(const uint32_t vk_format, const std::byte* block, std::array<rgba, 16>& texels)
        {
            if (vk_format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK)
            {
                const auto palette = bc1_palette(block, true);
                uint32_t indices{};
                std::memcpy(&indices, block + 4, sizeof(indices));
                for (unsigned i = 0; i < 16; ++i)
                {
                    texels[i] = palette[(indices >> (2 * i)) & 3];
                }
                return;
            }

            const auto palette = bc1_palette(block + 8, false);
            uint32_t indices{};
            std::memcpy(&indices, block + 12, sizeof(indices));
            for (unsigned i = 0; i < 16; ++i)
            {
                texels[i] = palette[(indices >> (2 * i)) & 3];
            }

            if (vk_format == VK_FORMAT_BC2_UNORM_BLOCK)
            {
                for (unsigned i = 0; i < 16; ++i)
                {
                    const auto nibble = static_cast<uint8_t>((static_cast<uint8_t>(block[i / 2]) >> (4 * (i & 1))) & 0xF);
                    texels[i][3] = static_cast<uint8_t>(nibble * 17);
                }
                return;
            }

            const auto alphas = bc3_alpha_palette(static_cast<uint8_t>(block[0]), static_cast<uint8_t>(block[1]));
            uint64_t alpha_indices{};
            std::memcpy(&alpha_indices, block, sizeof(alpha_indices));
            alpha_indices >>= 16;
            for (unsigned i = 0; i < 16; ++i)
            {
                texels[i][3] = alphas[(alpha_indices >> (3 * i)) & 7];
            }
        }
    }

    bool vk_format_is_bc1_to_bc3(const uint32_t vk_format)
    {
        return vk_format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK || vk_format == VK_FORMAT_BC2_UNORM_BLOCK ||
               vk_format == VK_FORMAT_BC3_UNORM_BLOCK;
    }

    void decode_bc1_to_bc3_rgba8(const uint32_t vk_format, const std::span<const std::byte> src, const uint32_t width,
                                 const uint32_t height, const uint32_t depth, const std::span<std::byte> dst)
    {
        const size_t block_bytes = vk_format == VK_FORMAT_BC1_RGBA_UNORM_BLOCK ? bc1_block_bytes : bc2_bc3_block_bytes;
        const uint32_t blocks_x = (width + 3) / 4;
        const uint32_t blocks_y = (height + 3) / 4;
        const size_t slice_source_bytes = static_cast<size_t>(blocks_x) * blocks_y * block_bytes;
        const size_t slice_texels = static_cast<size_t>(width) * height;

        std::array<rgba, 16> texels{};
        for (uint32_t slice = 0; slice < depth; ++slice)
        {
            const std::byte* slice_src = src.data() + slice * slice_source_bytes;
            std::byte* slice_dst = dst.data() + slice * slice_texels * 4;

            for (uint32_t by = 0; by < blocks_y; ++by)
            {
                for (uint32_t bx = 0; bx < blocks_x; ++bx)
                {
                    decode_block(vk_format, slice_src + (static_cast<size_t>(by) * blocks_x + bx) * block_bytes, texels);

                    const uint32_t columns = std::min(4u, width - bx * 4);
                    const uint32_t rows = std::min(4u, height - by * 4);
                    for (uint32_t y = 0; y < rows; ++y)
                    {
                        for (uint32_t x = 0; x < columns; ++x)
                        {
                            std::byte* out = slice_dst + (static_cast<size_t>(by * 4 + y) * width + bx * 4 + x) * 4;
                            std::memcpy(out, texels[y * 4 + x].data(), 4);
                        }
                    }
                }
            }
        }
    }
}
