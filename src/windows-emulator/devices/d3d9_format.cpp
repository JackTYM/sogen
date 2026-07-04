#include "d3d9_format.hpp"

namespace sogen
{
    namespace
    {
        // D3DFORMAT values, verified against mingw-w64's d3d9types.h.
        constexpr uint32_t d3dfmt_a8r8g8b8 = 21;
        constexpr uint32_t d3dfmt_x8r8g8b8 = 22;
        constexpr uint32_t d3dfmt_r5g6b5 = 23;
        constexpr uint32_t d3dfmt_a8 = 28;
        constexpr uint32_t d3dfmt_l8 = 50;
        constexpr uint32_t d3dfmt_v8u8 = 60;
        constexpr uint32_t d3dfmt_q8w8v8u8 = 63;
        constexpr uint32_t d3dfmt_d24s8 = 75;
        constexpr uint32_t d3dfmt_d24x8 = 77;
        constexpr uint32_t d3dfmt_a16b16g16r16f = 113;
        // FOURCC('D','X','T','1'/'3'/'5'), per d3d9types.h's MAKEFOURCC macro (little-endian packing).
        constexpr uint32_t d3dfmt_dxt1 = 0x31545844;
        constexpr uint32_t d3dfmt_dxt3 = 0x33545844;
        constexpr uint32_t d3dfmt_dxt5 = 0x35545844;

        // VkFormat values, verified against deps/Vulkan-Headers/include/vulkan/vulkan_core.h.
        constexpr uint32_t vk_format_r5g6b5_unorm_pack16 = 4;
        constexpr uint32_t vk_format_r8_unorm = 9;
        constexpr uint32_t vk_format_r8g8_snorm = 17;
        constexpr uint32_t vk_format_r8g8b8a8_snorm = 38;
        constexpr uint32_t vk_format_b8g8r8a8_unorm = 44;
        constexpr uint32_t vk_format_r16g16b16a16_sfloat = 97;
        constexpr uint32_t vk_format_d32_sfloat = 126;
        constexpr uint32_t vk_format_d24_unorm_s8_uint = 129;
        constexpr uint32_t vk_format_bc1_rgba_unorm_block = 133;
        constexpr uint32_t vk_format_bc2_unorm_block = 135;
        constexpr uint32_t vk_format_bc3_unorm_block = 137;
    } // namespace

    bool d3d9_format_to_vulkan(const uint32_t d3dfmt, uint32_t& out_vk_format)
    {
        switch (d3dfmt)
        {
        // D3D9's A8R8G8B8/X8R8G8B8 are byte-order BGRA, matching Vulkan's B8G8R8A8, not R8G8B8A8.
        case d3dfmt_a8r8g8b8:
        case d3dfmt_x8r8g8b8:
            out_vk_format = vk_format_b8g8r8a8_unorm;
            return true;
        case d3dfmt_r5g6b5:
            out_vk_format = vk_format_r5g6b5_unorm_pack16;
            return true;
        case d3dfmt_a8:
            out_vk_format = vk_format_r8_unorm;
            return true;
        case d3dfmt_l8:
            out_vk_format = vk_format_r8_unorm;
            return true;
        case d3dfmt_v8u8:
            out_vk_format = vk_format_r8g8_snorm;
            return true;
        case d3dfmt_q8w8v8u8:
            out_vk_format = vk_format_r8g8b8a8_snorm;
            return true;
        case d3dfmt_d24s8:
            out_vk_format = vk_format_d24_unorm_s8_uint;
            return true;
        case d3dfmt_d24x8:
            // VK_FORMAT_X8_D24_UNORM_PACK32 is the byte-exact match, but Vulkan only guarantees one of
            // it or D32_SFLOAT is supported for depth-only formats -- Apple/MoltenVK (this project's
            // deployment target) has no native 24-bit depth format and does not expose the packed
            // variant. D32_SFLOAT is a strict-superset-precision, single-channel substitute with no
            // stencil/alpha to worry about, so it's the portable choice here.
            out_vk_format = vk_format_d32_sfloat;
            return true;
        case d3dfmt_a16b16g16r16f:
            out_vk_format = vk_format_r16g16b16a16_sfloat;
            return true;
        case d3dfmt_dxt1:
            out_vk_format = vk_format_bc1_rgba_unorm_block;
            return true;
        case d3dfmt_dxt3:
            out_vk_format = vk_format_bc2_unorm_block;
            return true;
        case d3dfmt_dxt5:
            out_vk_format = vk_format_bc3_unorm_block;
            return true;
        default:
            return false;
        }
    }
} // namespace sogen
