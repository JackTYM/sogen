#include "emulation_test_utils.hpp"

#include <sxs/activation_context_data.hpp>
#include <sxs/assembly_resolver.hpp>
#include <sxs/manifest.hpp>

#include <cstring>
#include <map>

namespace sogen::test
{
    namespace
    {
        std::vector<std::byte> to_bytes(const std::string_view text)
        {
            const auto* data = reinterpret_cast<const std::byte*>(text.data());
            return {data, data + text.size()};
        }

        uint32_t read_u32(const std::vector<std::byte>& data, const size_t offset)
        {
            uint32_t value{};
            std::memcpy(&value, data.data() + offset, sizeof(value));
            return value;
        }

        std::u16string read_string(const std::vector<std::byte>& data, const size_t offset, const size_t length)
        {
            std::u16string value(length / 2, u'\0');
            std::memcpy(value.data(), data.data() + offset, length);
            return value;
        }

        // Mirrors ntdll's RtlpLocateActivationContextSection over the default TOC.
        std::optional<size_t> locate_section(const std::vector<std::byte>& data, const uint32_t id)
        {
            const auto toc = read_u32(data, 16);
            const auto count = read_u32(data, toc + 4);
            const auto entries = read_u32(data, toc + 8);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (read_u32(data, entries + i * 16) == id)
                {
                    return read_u32(data, entries + i * 16 + 4);
                }
            }

            return std::nullopt;
        }

        struct found_entry
        {
            size_t data{};
            uint32_t length{};
            uint32_t roster_index{};
        };

        // Mirrors ntdll's RtlpFindUnicodeStringInSection for a section without a search structure.
        std::optional<found_entry> find_string(const std::vector<std::byte>& data, const size_t section, const std::u16string_view key)
        {
            EXPECT_EQ(read_u32(data, section), 0x64487353u);
            const auto count = read_u32(data, section + 20);
            const auto list = section + read_u32(data, section + 24);
            const auto hash = sxs::hash_section_key(key);

            for (uint32_t i = 0; i < count; ++i)
            {
                const auto entry = list + i * 24;
                if (read_u32(data, entry) != hash)
                {
                    continue;
                }

                const auto stored = read_string(data, section + read_u32(data, entry + 4), read_u32(data, entry + 8));
                if (sxs::hash_section_key(stored) == hash && stored.size() == key.size())
                {
                    return found_entry{section + read_u32(data, entry + 12), read_u32(data, entry + 16), read_u32(data, entry + 20)};
                }
            }

            return std::nullopt;
        }

        class memory_store : public sxs::assembly_store
        {
          public:
            std::map<std::u16string, std::string> files{};

            std::optional<std::vector<std::byte>> read_file(const std::u16string_view path) override
            {
                const auto entry = this->files.find(std::u16string(path));
                if (entry == this->files.end())
                {
                    return std::nullopt;
                }

                return to_bytes(entry->second);
            }

            std::vector<std::u16string> list_files(const std::u16string_view directory) override
            {
                std::vector<std::u16string> names{};
                const auto prefix = std::u16string(directory) + u"\\";
                for (const auto& path : this->files | std::views::keys)
                {
                    if (path.starts_with(prefix) && path.find(u'\\', prefix.size()) == std::u16string::npos)
                    {
                        names.push_back(path.substr(prefix.size()));
                    }
                }

                return names;
            }
        };

        constexpr std::string_view application_manifest = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0" xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
  <assemblyIdentity name="Sample.App" version="1.0.0.0" processorArchitecture="amd64" type="win32"/>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0"
                        processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
  <file name="helper.dll"/>
  <asmv3:application>
    <asmv3:windowsSettings xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">
      <dpiAware>true</dpiAware>
    </asmv3:windowsSettings>
  </asmv3:application>
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security><requestedPrivileges><requestedExecutionLevel level="requireAdministrator" uiAccess="false"/></requestedPrivileges></security>
  </trustInfo>
</assembly>)";

        constexpr std::string_view common_controls_policy = R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v3" manifestVersion="1.0">
  <assemblyIdentity name="policy.6.0.Microsoft.Windows.Common-Controls" version="6.0.20348.1" processorArchitecture="amd64"
                    publicKeyToken="6595b64144ccf1df" type="win32-policy"/>
  <dependency><dependentAssembly>
    <assemblyIdentity name="Microsoft.Windows.Common-Controls" processorArchitecture="amd64" publicKeyToken="6595b64144ccf1df" type="win32"/>
    <bindingRedirect oldVersion="6.0.0.0-6.0.20348.1" newVersion="6.0.20348.1"/>
  </dependentAssembly></dependency>
</assembly>)";

        constexpr std::string_view common_controls = R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v3" manifestVersion="1.0">
  <assemblyIdentity name="Microsoft.Windows.Common-Controls" version="6.0.20348.1" processorArchitecture="amd64"
                    publicKeyToken="6595b64144ccf1df" type="win32"/>
  <file name="comctl32.dll">
    <windowClass>Button</windowClass>
    <windowClass versioned="no">ToolbarWindow32</windowClass>
  </file>
</assembly>)";

        constexpr std::u16string_view manifests = u"C:\\Windows\\WinSxS\\Manifests";
        constexpr std::u16string_view controls_directory =
            u"amd64_microsoft.windows.common-controls_6595b64144ccf1df_6.0.20348.1_none_1234567890abcdef";

        memory_store make_store()
        {
            memory_store store{};
            store.files[std::u16string(manifests) +
                        u"\\amd64_policy.6.0.microsoft.windows.common-controls_6595b64144ccf1df_6.0.20348.1_none_00aa.manifest"] =
                common_controls_policy;
            store.files[std::u16string(manifests) + u"\\" + std::u16string(controls_directory) + u".manifest"] = common_controls;
            return store;
        }

        sxs::resolution_request make_request()
        {
            sxs::resolution_request request{};
            request.root_manifest = to_bytes(application_manifest);
            request.root_manifest_path = u"C:\\App\\app.exe";
            request.application_directory = u"C:\\App\\";
            request.processor_architecture = u"amd64";
            request.windows_directory = u"C:\\Windows";
            return request;
        }
    }

    TEST(SxsManifestTest, ParsesIdentityDependenciesSettingsAndRunLevel)
    {
        const auto parsed = sxs::parse_manifest(to_bytes(application_manifest));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed->identity.name, u"Sample.App");
        ASSERT_EQ(parsed->dependencies.size(), 1u);
        EXPECT_EQ(parsed->dependencies[0].identity.public_key_token, u"6595b64144ccf1df");
        ASSERT_EQ(parsed->settings.size(), 1u);
        EXPECT_EQ(parsed->settings[0].namespace_uri, u"http://schemas.microsoft.com/SMI/2005/WindowsSettings");
        EXPECT_EQ(parsed->settings[0].name, u"dpiAware");
        EXPECT_EQ(parsed->settings[0].value, u"true");
        EXPECT_EQ(parsed->requested_run_level, sxs::run_level::require_admin);
    }

    TEST(SxsResolverTest, AppliesPublisherPolicyAndBindsTheStoreAssembly)
    {
        auto store = make_store();
        const auto resolved = sxs::resolve_activation_context(make_request(), store);
        const auto* input = std::get_if<sxs::activation_context_input>(&resolved);
        ASSERT_NE(input, nullptr);
        ASSERT_EQ(input->assemblies.size(), 2u);
        EXPECT_EQ(input->assemblies[1].identity.version, u"6.0.20348.1");
        EXPECT_EQ(input->assemblies[1].directory_name, controls_directory);
    }

    TEST(SxsResolverTest, WithoutPolicyBindsTheNewestVersionOfTheRequestedFamily)
    {
        constexpr std::string_view application = R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <dependency><dependentAssembly>
    <assemblyIdentity type="win32" name="Microsoft.Windows.GdiPlus" version="1.0.0.0" processorArchitecture="x86"
                      publicKeyToken="6595b64144ccf1df" language="*"/>
  </dependentAssembly></dependency>
</assembly>)";

        const auto gdiplus = [](const std::string_view version) {
            return std::string(R"(<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <assemblyIdentity name="Microsoft.Windows.GdiPlus" version=")") +
                   std::string(version) + R"(" processorArchitecture="x86" publicKeyToken="6595b64144ccf1df" type="win32"/>
  <file name="gdiplus.dll"/>
</assembly>)";
        };

        memory_store store{};
        const auto directory = std::u16string(manifests) + u"\\x86_microsoft.windows.gdiplus_6595b64144ccf1df_";
        store.files[directory + u"1.0.20348.1_none_01.manifest"] = gdiplus("1.0.20348.1");
        store.files[directory + u"1.0.20348.5622_none_02.manifest"] = gdiplus("1.0.20348.5622");
        store.files[directory + u"1.1.20348.5622_none_03.manifest"] = gdiplus("1.1.20348.5622");

        auto request = make_request();
        request.root_manifest = to_bytes(application);
        request.processor_architecture = u"x86";

        const auto resolved = sxs::resolve_activation_context(request, store);
        const auto* input = std::get_if<sxs::activation_context_input>(&resolved);
        ASSERT_NE(input, nullptr);
        ASSERT_EQ(input->assemblies.size(), 2u);
        EXPECT_EQ(input->assemblies[1].identity.version, u"1.0.20348.5622");
        EXPECT_EQ(input->assemblies[1].directory_name, u"x86_microsoft.windows.gdiplus_6595b64144ccf1df_1.0.20348.5622_none_02");
    }

    TEST(SxsResolverTest, MissingAssemblyFailsTheContext)
    {
        memory_store store{};
        const auto resolved = sxs::resolve_activation_context(make_request(), store);
        ASSERT_TRUE(std::holds_alternative<uint32_t>(resolved));
        EXPECT_EQ(std::get<uint32_t>(resolved), 0xC0150004u);
    }

    TEST(SxsActivationContextDataTest, SectionsResolveLikeNtdll)
    {
        auto store = make_store();
        const auto resolved = sxs::resolve_activation_context(make_request(), store);
        const auto data = sxs::build_activation_context_data(std::get<sxs::activation_context_input>(resolved));

        ASSERT_EQ(read_u32(data, 0), 0x78746341u);
        ASSERT_EQ(read_u32(data, 8), 1u);
        ASSERT_EQ(read_u32(data, 12), data.size());

        const auto roster = read_u32(data, 24);
        ASSERT_EQ(read_u32(data, roster + 8), 3u);

        const auto information_section = locate_section(data, 1);
        ASSERT_TRUE(information_section.has_value());
        ASSERT_EQ(read_u32(data, roster + 16), *information_section);
        const auto information = read_u32(data, roster + 12) + 2 * 24 + 16;
        const auto controls_information = read_u32(data, information);
        EXPECT_EQ(
            read_string(data, *information_section + read_u32(data, controls_information + 84), read_u32(data, controls_information + 80)),
            controls_directory);

        const auto dll_section = locate_section(data, 2);
        ASSERT_TRUE(dll_section.has_value());
        const auto comctl = find_string(data, *dll_section, u"COMCTL32.DLL");
        ASSERT_TRUE(comctl.has_value());
        EXPECT_EQ(comctl->roster_index, 2u);
        EXPECT_EQ(read_u32(data, comctl->data), 20u);
        EXPECT_EQ(read_u32(data, comctl->data + 4), 2u);
        const auto helper = find_string(data, *dll_section, u"helper.dll");
        ASSERT_TRUE(helper.has_value());
        EXPECT_EQ(helper->roster_index, 1u);

        const auto class_section = locate_section(data, 3);
        ASSERT_TRUE(class_section.has_value());
        const auto button = find_string(data, *class_section, u"button");
        ASSERT_TRUE(button.has_value());
        EXPECT_EQ(read_string(data, button->data + read_u32(data, button->data + 12), read_u32(data, button->data + 8)),
                  u"6.0.20348.1!Button");
        EXPECT_EQ(read_string(data, *class_section + read_u32(data, button->data + 20), read_u32(data, button->data + 16)),
                  u"comctl32.dll");
        const auto toolbar = find_string(data, *class_section, u"ToolbarWindow32");
        ASSERT_TRUE(toolbar.has_value());
        EXPECT_EQ(read_string(data, toolbar->data + read_u32(data, toolbar->data + 12), read_u32(data, toolbar->data + 8)),
                  u"ToolbarWindow32");

        const auto settings_section = locate_section(data, 10);
        ASSERT_TRUE(settings_section.has_value());
        const auto dpi_aware = find_string(data, *settings_section, u"dpiAware");
        ASSERT_TRUE(dpi_aware.has_value());
        EXPECT_EQ(read_string(data, dpi_aware->data + read_u32(data, dpi_aware->data + 28), read_u32(data, dpi_aware->data + 24)), u"true");
    }
}
