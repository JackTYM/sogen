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
        constexpr uint32_t d3dfmt_a8l8 = 51;
        constexpr uint32_t d3dfmt_v8u8 = 60;
        constexpr uint32_t d3dfmt_q8w8v8u8 = 63;
        constexpr uint32_t d3dfmt_d24s8 = 75;
        constexpr uint32_t d3dfmt_d24x8 = 77;
        constexpr uint32_t d3dfmt_a16b16g16r16f = 113;
        constexpr uint32_t d3dfmt_r32f = 114;
        // FOURCC('D','X','T','1'/'3'/'5'), per d3d9types.h's MAKEFOURCC macro (little-endian packing).
        constexpr uint32_t d3dfmt_dxt1 = 0x31545844;
        constexpr uint32_t d3dfmt_dxt3 = 0x33545844;
        constexpr uint32_t d3dfmt_dxt5 = 0x35545844;

        // D3DDECLTYPE values, verified against mingw-w64's d3d9types.h.
        constexpr uint32_t d3ddecltype_float1 = 0;
        constexpr uint32_t d3ddecltype_float2 = 1;
        constexpr uint32_t d3ddecltype_float3 = 2;
        constexpr uint32_t d3ddecltype_float4 = 3;
        constexpr uint32_t d3ddecltype_d3dcolor = 4;
        constexpr uint32_t d3ddecltype_ubyte4 = 5;
        constexpr uint32_t d3ddecltype_short2 = 6;
        constexpr uint32_t d3ddecltype_short4 = 7;
        constexpr uint32_t d3ddecltype_ubyte4n = 8;
        constexpr uint32_t d3ddecltype_short2n = 9;
        constexpr uint32_t d3ddecltype_short4n = 10;
        constexpr uint32_t d3ddecltype_ushort2n = 11;
        constexpr uint32_t d3ddecltype_ushort4n = 12;
        constexpr uint32_t d3ddecltype_udec3 = 13;
        constexpr uint32_t d3ddecltype_dec3n = 14;
        constexpr uint32_t d3ddecltype_float16_2 = 15;
        constexpr uint32_t d3ddecltype_float16_4 = 16;

        // VkFormat values, verified against deps/Vulkan-Headers/include/vulkan/vulkan_core.h.
        constexpr uint32_t vk_format_r5g6b5_unorm_pack16 = 4;
        constexpr uint32_t vk_format_r8_unorm = 9;
        constexpr uint32_t vk_format_r8g8_unorm = 16;
        constexpr uint32_t vk_format_r8g8_snorm = 17;
        constexpr uint32_t vk_format_r8g8b8a8_unorm = 37;
        constexpr uint32_t vk_format_r8g8b8a8_snorm = 38;
        constexpr uint32_t vk_format_r8g8b8a8_uscaled = 39;
        constexpr uint32_t vk_format_b8g8r8a8_unorm = 44;
        constexpr uint32_t vk_format_a2b10g10r10_snorm_pack32 = 65;
        constexpr uint32_t vk_format_a2b10g10r10_uscaled_pack32 = 66;
        constexpr uint32_t vk_format_r16g16_unorm = 77;
        constexpr uint32_t vk_format_r16g16_snorm = 78;
        constexpr uint32_t vk_format_r16g16_sscaled = 80;
        constexpr uint32_t vk_format_r16g16_sfloat = 83;
        constexpr uint32_t vk_format_r16g16b16a16_unorm = 91;
        constexpr uint32_t vk_format_r16g16b16a16_snorm = 92;
        constexpr uint32_t vk_format_r16g16b16a16_sscaled = 94;
        constexpr uint32_t vk_format_r16g16b16a16_sfloat = 97;
        constexpr uint32_t vk_format_r32_sfloat = 100;
        constexpr uint32_t vk_format_r32g32_sfloat = 103;
        constexpr uint32_t vk_format_r32g32b32_sfloat = 106;
        constexpr uint32_t vk_format_r32g32b32a32_sfloat = 109;
        constexpr uint32_t vk_format_d32_sfloat = 126;
        constexpr uint32_t vk_format_d32_sfloat_s8_uint = 130;
        constexpr uint32_t vk_format_bc1_rgba_unorm_block = 133;
        constexpr uint32_t vk_format_bc2_unorm_block = 135;
        constexpr uint32_t vk_format_bc3_unorm_block = 137;

        // VkComponentSwizzle values, verified against deps/Vulkan-Headers/include/vulkan/vulkan_core.h.
        constexpr uint32_t vk_component_swizzle_identity = 0;
        constexpr uint32_t vk_component_swizzle_zero = 1;
        constexpr uint32_t vk_component_swizzle_one = 2;
        constexpr uint32_t vk_component_swizzle_r = 3;
        constexpr uint32_t vk_component_swizzle_g = 4;
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
        case d3dfmt_a8l8:
            // Two-channel luminance-alpha. R8G8_UNORM is the byte-exact match (byte 0 = L -> R, byte 1 =
            // A -> G); d3d9_format_to_vulkan_swizzle below remaps R8G8_UNORM's raw channels to match real
            // D3D9 sampling (R=G=B=L, A=A).
            out_vk_format = vk_format_r8g8_unorm;
            return true;
        case d3dfmt_v8u8:
            out_vk_format = vk_format_r8g8_snorm;
            return true;
        case d3dfmt_q8w8v8u8:
            out_vk_format = vk_format_r8g8b8a8_snorm;
            return true;
        case d3dfmt_d24s8:
            // VK_FORMAT_D24_UNORM_S8_UINT is the byte-exact match, but Apple Silicon's MoltenVK (this
            // project's deployment target) does not support it at all (confirmed via vulkaninfo) --
            // only D32_SFLOAT_S8_UINT, mirroring the same portability substitution already made for
            // D3DFMT_D24X8 below.
            out_vk_format = vk_format_d32_sfloat_s8_uint;
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
        case d3dfmt_r32f:
            out_vk_format = vk_format_r32_sfloat;
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

    d3d9_format_swizzle d3d9_format_to_vulkan_swizzle(const uint32_t d3dfmt)
    {
        switch (d3dfmt)
        {
        case d3dfmt_a8:
            // Real D3D9 D3DFMT_A8 samples as RGB=0, A=the stored byte. R8_UNORM's only channel is R, so
            // without this remap the byte lands in the shader's R component (with A forced to Vulkan's
            // default 1.0 for a format with no alpha channel) -- every alpha-blended draw using this as
            // a coverage mask (e.g. a font glyph atlas) then reads alpha=1.0 regardless of the actual
            // texel, rendering a solid quad instead of the intended shape.
            return {vk_component_swizzle_zero, vk_component_swizzle_zero, vk_component_swizzle_zero, vk_component_swizzle_r};
        case d3dfmt_l8:
            // Real D3D9 D3DFMT_L8 samples as R=G=B=the stored byte, A=1 (opaque). R8_UNORM's identity
            // mapping only fills R, leaving G/B at 0 -- a grayscale texture would sample as pure red
            // instead of gray.
            return {vk_component_swizzle_r, vk_component_swizzle_r, vk_component_swizzle_r, vk_component_swizzle_one};
        case d3dfmt_a8l8:
            // Real D3D9 D3DFMT_A8L8 samples as R=G=B=byte0 (L), A=byte1 (A). R8G8_UNORM's identity
            // mapping puts A in G and leaves B at 0, matching neither channel's real placement.
            return {vk_component_swizzle_r, vk_component_swizzle_r, vk_component_swizzle_r, vk_component_swizzle_g};
        default:
            return {vk_component_swizzle_identity, vk_component_swizzle_identity, vk_component_swizzle_identity,
                    vk_component_swizzle_identity};
        }
    }

    uint32_t vk_format_bytes_per_texel(const uint32_t vk_format)
    {
        switch (vk_format)
        {
        case vk_format_r8_unorm:
            return 1;
        case vk_format_r5g6b5_unorm_pack16:
        case vk_format_r8g8_unorm:
        case vk_format_r8g8_snorm:
            return 2;
        case vk_format_r8g8b8a8_snorm:
        case vk_format_b8g8r8a8_unorm:
        case vk_format_r32_sfloat:
        case vk_format_d32_sfloat:
            return 4;
        case vk_format_r16g16b16a16_sfloat:
        // D32_SFLOAT_S8_UINT is really 5 bytes of depth+stencil, but its render-target readback buffer
        // is only ever allocated (never used for a color readback -- depth surfaces are not locked), so
        // an 8-byte upper-bound keeps its allocation non-zero without any layout meaning.
        case vk_format_d32_sfloat_s8_uint:
            return 8;
        default:
            // Block-compressed (BC1/BC2/BC3) and any unrecognized format: no single-texel byte size.
            return 0;
        }
    }

    bool d3d9_decl_type_to_vulkan(const uint32_t d3ddecltype, uint32_t& out_vk_format)
    {
        switch (d3ddecltype)
        {
        case d3ddecltype_float1:
            out_vk_format = vk_format_r32_sfloat;
            return true;
        case d3ddecltype_float2:
            out_vk_format = vk_format_r32g32_sfloat;
            return true;
        case d3ddecltype_float3:
            out_vk_format = vk_format_r32g32b32_sfloat;
            return true;
        case d3ddecltype_float4:
            out_vk_format = vk_format_r32g32b32a32_sfloat;
            return true;
        case d3ddecltype_d3dcolor:
            // Matches the fixed-function path's existing D3DCOLOR-attribute convention (see
            // ensure_pipeline's attributes array in d3d9_host.cpp).
            out_vk_format = vk_format_b8g8r8a8_unorm;
            return true;
        // The packed/compressed types below are what real content (as opposed to this repo's own
        // float-only guest tests) overwhelmingly uses for anything but POSITION -- normals, tangents,
        // bone weights/indices and texcoords are near-universally stored compressed. Leaving them
        // unmapped is not a harmless omission: parse_vertex_decl skips an element it cannot map while
        // keeping every later element's location (an element's location IS its ordinal in the
        // D3DVERTEXELEMENT9 array), so an unmapped type punches a *hole* in the pipeline's vertex-input
        // locations. A shader input with no matching VkVertexInputAttributeDescription is invalid usage
        // that desktop drivers silently tolerate but Metal rejects outright -- MoltenVK fails the whole
        // vkCreateGraphicsPipelines with "Vertex attribute vN(N) is missing from the vertex descriptor",
        // and every draw using that shader is then dropped. Each mapping below is D3D9's documented
        // memory layout for the type (d3d9types.h), component-for-component:
        //   * UBYTE4/UBYTE4N are byte-order x,y,z,w -> R8G8B8A8 (not the BGRA swap D3DCOLOR needs).
        //   * The unnormalized integer types (UBYTE4, SHORT2, SHORT4) expand to float by value, which is
        //     Vulkan's USCALED/SSCALED, not UINT/SINT (those would feed an int shader input).
        //   * UDEC3/DEC3N pack x into bits 0-9, y into 10-19 and z into 20-29, exactly Vulkan's
        //     A2B10G10R10_*_PACK32 R/G/B placement.
        case d3ddecltype_ubyte4:
            out_vk_format = vk_format_r8g8b8a8_uscaled;
            return true;
        case d3ddecltype_ubyte4n:
            out_vk_format = vk_format_r8g8b8a8_unorm;
            return true;
        case d3ddecltype_short2:
            out_vk_format = vk_format_r16g16_sscaled;
            return true;
        case d3ddecltype_short4:
            out_vk_format = vk_format_r16g16b16a16_sscaled;
            return true;
        case d3ddecltype_short2n:
            out_vk_format = vk_format_r16g16_snorm;
            return true;
        case d3ddecltype_short4n:
            out_vk_format = vk_format_r16g16b16a16_snorm;
            return true;
        case d3ddecltype_ushort2n:
            out_vk_format = vk_format_r16g16_unorm;
            return true;
        case d3ddecltype_ushort4n:
            out_vk_format = vk_format_r16g16b16a16_unorm;
            return true;
        case d3ddecltype_udec3:
            out_vk_format = vk_format_a2b10g10r10_uscaled_pack32;
            return true;
        case d3ddecltype_dec3n:
            out_vk_format = vk_format_a2b10g10r10_snorm_pack32;
            return true;
        case d3ddecltype_float16_2:
            out_vk_format = vk_format_r16g16_sfloat;
            return true;
        case d3ddecltype_float16_4:
            out_vk_format = vk_format_r16g16b16a16_sfloat;
            return true;
        default:
            return false;
        }
    }
} // namespace sogen
