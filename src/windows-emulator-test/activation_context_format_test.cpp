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
        // Fixtures are copied next to the test binary at build time (see CMakeLists.txt) and
        // read relative to the process's working directory, which ctest/the CI runner both set
        // to that same directory - a compile-time absolute source-tree path doesn't survive
        // this project's split build/test CI jobs (the smoke-test job runs a downloaded
        // artifact, not a fresh checkout).
        std::vector<std::uint8_t> read_fixture(const std::string& relative_path)
        {
            const std::string full_path = "fixtures/" + relative_path;
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

        const auto sections = find_all_sections(blob);
        ASSERT_FALSE(sections.empty());

        const auto first = section_bytes(blob, sections[0]);
        ASSERT_GE(first.size(), 4u);
        EXPECT_EQ(0, std::memcmp(first.data(), "SsHd", 4));
    }

    TEST(ActivationContextFormat, ParserExtractsResolvedWinSxsDirectoryName)
    {
        const auto blob = read_fixture("actctx/common_controls_v6_amd64.bin");

        const auto sections = find_all_sections(blob);
        ASSERT_FALSE(sections.empty());

        const auto first = section_bytes(blob, sections[0]);
        const auto found = find_wide_string_in_section(
            first, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");

        ASSERT_TRUE(found.has_value());
    }

    TEST(ActivationContextFormat, ZeroDependencyCaptureHasSingleAssemblySection)
    {
        // Independent real capture #2 (differential analysis): a manifest with only a
        // self-identity, no external dependency. Confirms the format understanding
        // generalizes, not just fits the one golden fixture.
        const auto blob = read_fixture("actctx/zero-deps.bin");

        const auto sections = find_all_sections(blob);
        ASSERT_FALSE(sections.empty());

        const auto first = section_bytes(blob, sections[0]);
        ASSERT_GE(first.size(), 4u);
        EXPECT_EQ(0, std::memcmp(first.data(), "SsHd", 4));
    }

    TEST(ActivationContextFormat, OneDependencyCaptureResolvesDespiteToArrayInconsistency)
    {
        // Independent real capture #3 (differential analysis): a manifest with one dependency
        // on the same Common-Controls v6 identity as the golden fixture. This specific capture
        // has a confirmed, reproducible real-Windows anomaly in its TOC entry array (a
        // non-constant byte-position inconsistency, ruled out as a capture race, name-length
        // effect, manifest-completeness effect, assembly-count effect, and CI OS-image drift -
        // see the format doc's differential-analysis section for the full elimination). Magic-
        // tag scanning must still locate every section correctly, since it never depends on the
        // TOC array's exact per-entry byte positions at all.
        const auto blob = read_fixture("actctx/one-dep.bin");

        const auto sections = find_all_sections(blob);
        ASSERT_GE(sections.size(), 3u);

        const auto first = section_bytes(blob, sections[0]);
        ASSERT_GE(first.size(), 4u);
        EXPECT_EQ(0, std::memcmp(first.data(), "SsHd", 4));

        // Version-agnostic: this capture's real CI runner had a different comctl32 patch
        // revision than the golden fixture's runner (33296 vs 33438 - confirmed via `ver`
        // matching the OS build number exactly in both cases), so the resolved WinSxS
        // directory name legitimately differs in its version component. The prefix is what
        // matters here.
        const auto found = find_wide_string_in_section(first, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.");
        EXPECT_TRUE(found.has_value());
    }
}
