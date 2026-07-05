#include <gtest/gtest.h>

#include "devices/d3d9_host.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace sogen::test
{
    namespace
    {
        // Mirrors d3d9_cmd::vertex_element's 8-byte wire layout (stream/offset:u16, then four u8
        // fields) without pulling in d3d9-command-protocol's target -- this test only needs the raw
        // bytes parse_vertex_decl itself operates on.
        void append_element(std::vector<std::byte>& blob, const uint16_t stream, const uint16_t offset,
                            const uint8_t type, const uint8_t usage, const uint8_t usage_index)
        {
            const std::array<uint8_t, 8> raw{
                static_cast<uint8_t>(stream & 0xFF),   static_cast<uint8_t>(stream >> 8),
                static_cast<uint8_t>(offset & 0xFF),   static_cast<uint8_t>(offset >> 8),
                type,                                  0, // D3DDECLMETHOD_DEFAULT
                usage,                                 usage_index,
            };
            const auto* bytes = reinterpret_cast<const std::byte*>(raw.data());
            blob.insert(blob.end(), bytes, bytes + raw.size());
        }

        constexpr uint8_t d3ddecltype_float2 = 1;
        constexpr uint8_t d3ddecltype_float3 = 2;
        constexpr uint8_t d3ddecltype_d3dcolor = 4;
        constexpr uint8_t d3ddecltype_unsupported = 99;

        constexpr uint8_t d3ddeclusage_position = 0;
        constexpr uint8_t d3ddeclusage_texcoord = 5;
        constexpr uint8_t d3ddeclusage_color = 10;

        constexpr uint32_t vk_format_r32g32_sfloat = 103;
        constexpr uint32_t vk_format_r32g32b32_sfloat = 106;
        constexpr uint32_t vk_format_b8g8r8a8_unorm = 44;
    } // namespace

    TEST(D3D9VertexDeclTest, TwoStreamPositionAndColor)
    {
        // Exactly the shape Task 9's multi-stream test will exercise: position on stream 0, a
        // D3DCOLOR diffuse on stream 1.
        std::vector<std::byte> blob;
        append_element(blob, /*stream=*/0, /*offset=*/0, d3ddecltype_float3, d3ddeclusage_position, 0);
        append_element(blob, /*stream=*/1, /*offset=*/4, d3ddecltype_d3dcolor, d3ddeclusage_color, 0);

        const parsed_vertex_decl parsed = parse_vertex_decl(blob);

        ASSERT_EQ(parsed.attributes.size(), 2u);

        EXPECT_EQ(parsed.attributes[0].location, 0u);
        EXPECT_EQ(parsed.attributes[0].binding, 0u);
        EXPECT_EQ(parsed.attributes[0].vk_format, vk_format_r32g32b32_sfloat);
        EXPECT_EQ(parsed.attributes[0].offset, 0u);

        EXPECT_EQ(parsed.attributes[1].location, 1u);
        EXPECT_EQ(parsed.attributes[1].binding, 1u);
        EXPECT_EQ(parsed.attributes[1].vk_format, vk_format_b8g8r8a8_unorm);
        EXPECT_EQ(parsed.attributes[1].offset, 4u);

        EXPECT_EQ(parsed.used_binding_mask, 0b11u);
    }

    TEST(D3D9VertexDeclTest, UnrecognizedTypeIsSkippedWithoutRenumberingLocations)
    {
        // location must track each element's own ordinal position in the declaration (see
        // parse_vertex_decl's comment for why), not the position among successfully-parsed
        // attributes -- so an unrecognized element in the middle must NOT shift later locations down.
        std::vector<std::byte> blob;
        append_element(blob, /*stream=*/0, /*offset=*/0, d3ddecltype_float3, d3ddeclusage_position, 0);
        append_element(blob, /*stream=*/0, /*offset=*/12, d3ddecltype_unsupported, /*usage=*/1, 0);
        append_element(blob, /*stream=*/1, /*offset=*/0, d3ddecltype_float2, d3ddeclusage_texcoord, 0);

        const parsed_vertex_decl parsed = parse_vertex_decl(blob);

        ASSERT_EQ(parsed.attributes.size(), 2u);

        EXPECT_EQ(parsed.attributes[0].location, 0u);
        EXPECT_EQ(parsed.attributes[0].binding, 0u);

        EXPECT_EQ(parsed.attributes[1].location, 2u); // NOT 1 -- the skipped element still counts.
        EXPECT_EQ(parsed.attributes[1].binding, 1u);
        EXPECT_EQ(parsed.attributes[1].vk_format, vk_format_r32g32_sfloat);
        EXPECT_EQ(parsed.attributes[1].offset, 0u);

        EXPECT_EQ(parsed.used_binding_mask, 0b11u);
    }

    TEST(D3D9VertexDeclTest, EmptyDeclarationProducesNoAttributes)
    {
        const parsed_vertex_decl parsed = parse_vertex_decl({});

        EXPECT_TRUE(parsed.attributes.empty());
        EXPECT_EQ(parsed.used_binding_mask, 0u);
    }
} // namespace sogen::test
