#include "std_include.hpp"
#include "module/activation_context_format.hpp"
#include "module/activation_context_parser.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <cstring>

namespace sogen
{
    namespace
    {
        std::vector<std::uint8_t> read_fixture(const std::string& relative_path)
        {
            const std::string full_path = std::string(SOGEN_TEST_FIXTURES_DIR) + "/" + relative_path;
            std::ifstream file(full_path, std::ios::binary);
            return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        }
    }

    TEST(ActivationContextFormat, GoldenFixtureHasExpectedHeader)
    {
        const auto blob = read_fixture("actctx/common_controls_v6_amd64.bin");
        ASSERT_GE(blob.size(), sizeof(activation_context_data_header));

        activation_context_data_header header{};
        std::memcpy(&header, blob.data(), sizeof(header));

        EXPECT_EQ(header.magic, activation_context_data_magic);
        EXPECT_EQ(header.format_version, activation_context_data_format_version);
        EXPECT_EQ(header.total_size, blob.size());
        EXPECT_EQ(header.header_size, 32u);
    }

    TEST(ActivationContextFormat, ParserFindsAssemblyIdentityStringsSection)
    {
        const auto blob = read_fixture("actctx/common_controls_v6_amd64.bin");

        const auto entry = find_toc_entry(blob, 1);
        ASSERT_TRUE(entry.has_value());

        const auto section = get_section_bytes(blob, *entry);
        ASSERT_GE(section.size(), 4u);
        EXPECT_EQ(0, std::memcmp(section.data(), "SsHd", 4));
    }

    TEST(ActivationContextFormat, ParserExtractsResolvedWinSxsDirectoryName)
    {
        const auto blob = read_fixture("actctx/common_controls_v6_amd64.bin");

        const auto entry = find_toc_entry(blob, 1);
        ASSERT_TRUE(entry.has_value());

        const auto section = get_section_bytes(blob, *entry);
        const auto found = find_wide_string_in_section(
            section, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");

        ASSERT_TRUE(found.has_value());
    }

    TEST(ActivationContextFormat, InvalidTocEntriesAreSkipped)
    {
        // The golden fixture's TOC declares 9 slots but only 7 (ids 1,2,3,4,5,6,9) have real,
        // in-bounds data - ids 7 and 8's slots contain stale offset/length values from
        // uninitialized memory that would read far past the blob's actual size. A correct
        // parser must not return those as if they were real sections.
        const auto blob = read_fixture("actctx/common_controls_v6_amd64.bin");

        EXPECT_FALSE(find_toc_entry(blob, 7).has_value());
        EXPECT_FALSE(find_toc_entry(blob, 8).has_value());
    }
}
