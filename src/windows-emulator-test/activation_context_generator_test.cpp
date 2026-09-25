#include "std_include.hpp"
#include "module/activation_context_format.hpp"
#include "module/activation_context_generator.hpp"
#include "module/activation_context_parser.hpp"
#include <gtest/gtest.h>
#include <cstring>

namespace sogen
{
    namespace
    {
        std::vector<std::uint8_t> generate_notepad_plus_plus_blob()
        {
            const root_assembly_info root{
                .name = "Notepad++",
                .version = "1.0.0.0",
                .processor_architecture = "amd64",
                .exe_path = "C:\\Program Files\\Notepad++\\notepad++.exe",
            };

            const resolved_assembly common_controls{
                .identity = {.name = "Microsoft.Windows.Common-Controls",
                             .version = "6.0.0.0",
                             .processor_architecture = "amd64",
                             .public_key_token = "6595b64144ccf1df",
                             .language = "*"},
                .concrete_directory_name = "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a",
                .manifest_path = "C:\\Windows\\WinSxS\\manifests\\amd64_microsoft.windows.common-controls_"
                                 "6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a.manifest",
                .redirected_dlls = {"comctl32.dll"},
            };

            return generate_activation_context_blob(root, {common_controls});
        }
    }

    TEST(ActivationContextGenerator, HeaderIsValid)
    {
        const auto blob = generate_notepad_plus_plus_blob();
        ASSERT_GE(blob.size(), sizeof(activation_context_data_header));

        activation_context_data_header header{};
        std::memcpy(&header, blob.data(), sizeof(header));

        EXPECT_EQ(header.magic, activation_context_data_magic);
        EXPECT_EQ(header.format_version, activation_context_data_format_version);
        EXPECT_EQ(header.total_size, blob.size());
    }

    TEST(ActivationContextGenerator, TocEntriesResolveToSectionStartsWithZeroBias)
    {
        // Confirmed via real ntdll.dll disassembly (RtlpLocateActivationContextSection,
        // called through RtlFindActivationContextSectionString -> RtlpFindNextActivationContextSection):
        // the section pointer real ntdll computes is exactly blob_base + TocEntry.Offset, with
        // no adjustment - unlike the roster fields, which do need a bias (see
        // activation_context_data_roster_offset_bias).
        const auto blob = generate_notepad_plus_plus_blob();

        activation_context_data_header header{};
        std::memcpy(&header, blob.data(), sizeof(header));

        activation_context_data_toc_header toc_header{};
        std::memcpy(&toc_header, blob.data() + header.default_toc_offset, sizeof(toc_header));
        ASSERT_EQ(toc_header.entry_count, 2u);

        for (std::uint32_t i = 0; i < toc_header.entry_count; ++i)
        {
            activation_context_data_toc_entry entry{};
            std::memcpy(&entry, blob.data() + toc_header.first_entry_offset + i * sizeof(entry), sizeof(entry));

            ASSERT_LE(entry.offset + 4, blob.size());
            EXPECT_EQ(0, std::memcmp(blob.data() + entry.offset, "SsHd", 4)) << "TOC entry id=" << entry.id;
        }
    }

    TEST(ActivationContextGenerator, MagicTagScanFindsBothSections)
    {
        const auto blob = generate_notepad_plus_plus_blob();

        const auto sections = find_all_sections(blob);
        ASSERT_EQ(sections.size(), 2u);

        const auto assembly_info = section_bytes(blob, sections[0]);
        ASSERT_GE(assembly_info.size(), 4u);
        EXPECT_EQ(0, std::memcmp(assembly_info.data(), "SsHd", 4));

        const auto dll_redirection = section_bytes(blob, sections[1]);
        ASSERT_GE(dll_redirection.size(), 4u);
        EXPECT_EQ(0, std::memcmp(dll_redirection.data(), "SsHd", 4));
    }

    TEST(ActivationContextGenerator, AssemblyInformationSectionContainsResolvedDirectoryName)
    {
        const auto blob = generate_notepad_plus_plus_blob();

        const auto sections = find_all_sections(blob);
        ASSERT_EQ(sections.size(), 2u);

        const auto assembly_info = section_bytes(blob, sections[0]);
        const auto found = find_wide_string_in_section(
            assembly_info, "amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.26100.33438_none_ee36e391daefe08a");
        ASSERT_TRUE(found.has_value());
    }

    TEST(ActivationContextGenerator, DllRedirectionSectionMapsComctl32ToCommonControlsRoster)
    {
        const auto blob = generate_notepad_plus_plus_blob();

        const auto sections = find_all_sections(blob);
        ASSERT_EQ(sections.size(), 2u);

        const auto dll_redirection = section_bytes(blob, sections[1]);

        activation_context_string_section_header header{};
        std::memcpy(&header, dll_redirection.data(), sizeof(header));
        ASSERT_EQ(header.element_count, 1u);

        activation_context_string_section_entry entry{};
        std::memcpy(&entry, dll_redirection.data() + header.element_list_offset, sizeof(entry));

        std::u16string key{};
        for (std::uint32_t i = 0; i < entry.key_length; i += 2)
        {
            std::uint16_t code_unit{};
            std::memcpy(&code_unit, dll_redirection.data() + entry.key_offset + i, sizeof(code_unit));
            key.push_back(static_cast<char16_t>(code_unit));
        }
        std::string key_utf8{};
        for (const auto c : key)
        {
            key_utf8.push_back(static_cast<char>(c));
        }
        EXPECT_EQ(key_utf8, "comctl32.dll");

        // Roster index 1 is always the ROOT; index 2 is the first (and here, only) resolved
        // dependent assembly - Common-Controls.
        EXPECT_EQ(entry.assembly_roster_index, 2u);

        activation_context_data_dll_redirection redirection{};
        std::memcpy(&redirection, dll_redirection.data() + entry.value_offset, sizeof(redirection));
        EXPECT_EQ(redirection.size, sizeof(redirection));
        EXPECT_EQ(redirection.path_segment_count, 0u);
    }

    TEST(ActivationContextGenerator, AssemblyRosterEntryForCommonControlsPointsBackIntoAssemblyInfoSection)
    {
        const auto blob = generate_notepad_plus_plus_blob();

        activation_context_data_header header{};
        std::memcpy(&header, blob.data(), sizeof(header));

        activation_context_data_assembly_roster_header roster_header{};
        std::memcpy(&roster_header, blob.data() + header.assembly_roster_offset, sizeof(roster_header));
        ASSERT_EQ(roster_header.entry_count, 3u); // reserved + ROOT + Common-Controls

        activation_context_data_assembly_roster_entry common_controls_entry{};
        std::memcpy(&common_controls_entry, blob.data() + roster_header.first_entry_offset + 2 * sizeof(common_controls_entry),
                    sizeof(common_controls_entry));

        const auto info_offset = common_controls_entry.assembly_information_offset + activation_context_data_roster_offset_bias;
        ASSERT_LE(info_offset + common_controls_entry.assembly_information_length, blob.size());

        activation_context_data_assembly_information info{};
        std::memcpy(&info, blob.data() + info_offset, sizeof(info));
        EXPECT_EQ(info.size, sizeof(info));
        EXPECT_EQ(info.flags, 0u);
    }
}
