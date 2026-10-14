#include "../std_include.hpp"
#include "manifest.hpp"
#include "xml_document.hpp"

namespace sogen::sxs
{
    namespace
    {
        std::u16string trim(const std::u16string_view text)
        {
            const auto is_space = [](const char16_t c) { return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n'; };

            size_t start = 0;
            size_t end = text.size();
            while (start < end && is_space(text[start]))
            {
                ++start;
            }
            while (end > start && is_space(text[end - 1]))
            {
                --end;
            }

            return std::u16string(text.substr(start, end - start));
        }

        bool equals_ignore_case(const std::u16string_view left, const std::u16string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t i = 0; i < left.size(); ++i)
            {
                const auto a = left[i] >= u'A' && left[i] <= u'Z' ? static_cast<char16_t>(left[i] + 32) : left[i];
                const auto b = right[i] >= u'A' && right[i] <= u'Z' ? static_cast<char16_t>(right[i] + 32) : right[i];
                if (a != b)
                {
                    return false;
                }
            }

            return true;
        }

        std::u16string attribute_or_empty(const xml_element& element, const std::u16string_view name)
        {
            const auto* value = element.attribute(name);
            return value ? *value : std::u16string{};
        }

        assembly_identity read_identity(const xml_element& element)
        {
            assembly_identity identity{};
            identity.name = attribute_or_empty(element, u"name");
            identity.type = attribute_or_empty(element, u"type");
            identity.version = attribute_or_empty(element, u"version");
            identity.processor_architecture = attribute_or_empty(element, u"processorArchitecture");
            identity.public_key_token = attribute_or_empty(element, u"publicKeyToken");
            identity.language = attribute_or_empty(element, u"language");
            return identity;
        }

        void read_dependencies(const xml_element& assembly, manifest& result)
        {
            for (const auto* dependency_element : assembly.children_named(u"dependency"))
            {
                const auto* optional_attribute = dependency_element->attribute(u"optional");
                const bool optional = optional_attribute && equals_ignore_case(*optional_attribute, u"yes");

                for (const auto* dependent : dependency_element->children_named(u"dependentAssembly"))
                {
                    const auto* identity_element = dependent->child(u"assemblyIdentity");
                    if (!identity_element)
                    {
                        continue;
                    }

                    const auto identity = read_identity(*identity_element);

                    for (const auto* redirect : dependent->children_named(u"bindingRedirect"))
                    {
                        const auto old_range = attribute_or_empty(*redirect, u"oldVersion");
                        const auto new_version = parse_version(attribute_or_empty(*redirect, u"newVersion"));
                        const auto separator = old_range.find(u'-');
                        const auto old_low = parse_version(std::u16string_view(old_range).substr(0, separator));
                        const auto old_high = separator == std::u16string::npos
                                                  ? old_low
                                                  : parse_version(std::u16string_view(old_range).substr(separator + 1));

                        if (old_low && old_high && new_version)
                        {
                            result.binding_redirects.push_back({*old_low, *old_high, *new_version});
                        }
                    }

                    if (dependent->children_named(u"bindingRedirect").empty())
                    {
                        result.dependencies.push_back({identity, optional});
                    }
                }
            }
        }

        void read_files(const xml_element& assembly, manifest& result)
        {
            for (const auto* file_element : assembly.children_named(u"file"))
            {
                manifest_file file{};
                file.name = attribute_or_empty(*file_element, u"name");
                if (file.name.empty())
                {
                    continue;
                }

                for (const auto* class_element : file_element->children_named(u"windowClass"))
                {
                    window_class entry{};
                    entry.name = trim(class_element->text);
                    const auto* versioned = class_element->attribute(u"versioned");
                    entry.versioned = !versioned || !equals_ignore_case(*versioned, u"no");
                    if (!entry.name.empty())
                    {
                        file.window_classes.push_back(std::move(entry));
                    }
                }

                result.files.push_back(std::move(file));
            }
        }

        void read_settings(const xml_element& assembly, manifest& result)
        {
            for (const auto* application : assembly.children_named(u"application"))
            {
                for (const auto* windows_settings : application->children_named(u"windowsSettings"))
                {
                    for (const auto& setting : windows_settings->children)
                    {
                        result.settings.push_back({setting.namespace_uri, setting.name, trim(setting.text)});
                    }
                }
            }
        }

        void read_trust_info(const xml_element& assembly, manifest& result)
        {
            const auto* trust_info = assembly.child(u"trustInfo");
            const auto* security = trust_info ? trust_info->child(u"security") : nullptr;
            const auto* privileges = security ? security->child(u"requestedPrivileges") : nullptr;
            const auto* level_element = privileges ? privileges->child(u"requestedExecutionLevel") : nullptr;
            if (!level_element)
            {
                return;
            }

            const auto level = attribute_or_empty(*level_element, u"level");
            if (equals_ignore_case(level, u"asInvoker"))
            {
                result.requested_run_level = run_level::as_invoker;
            }
            else if (equals_ignore_case(level, u"highestAvailable"))
            {
                result.requested_run_level = run_level::highest_available;
            }
            else if (equals_ignore_case(level, u"requireAdministrator"))
            {
                result.requested_run_level = run_level::require_admin;
            }

            const auto* ui_access = level_element->attribute(u"uiAccess");
            result.ui_access = ui_access && equals_ignore_case(*ui_access, u"true");
        }
    }

    std::optional<assembly_version> parse_version(const std::u16string_view text)
    {
        assembly_version version{};
        size_t part = 0;
        uint32_t value = 0;
        bool has_digit = false;

        for (const auto c : text)
        {
            if (c == u'.')
            {
                if (!has_digit || part >= 3)
                {
                    return std::nullopt;
                }

                version[part++] = static_cast<uint16_t>(value);
                value = 0;
                has_digit = false;
                continue;
            }

            if (c < u'0' || c > u'9')
            {
                return std::nullopt;
            }

            value = value * 10 + static_cast<uint32_t>(c - u'0');
            if (value > 0xFFFF)
            {
                return std::nullopt;
            }

            has_digit = true;
        }

        if (!has_digit || part != 3)
        {
            return std::nullopt;
        }

        version[3] = static_cast<uint16_t>(value);
        return version;
    }

    std::u16string format_version(const assembly_version& version)
    {
        std::u16string result{};
        for (size_t i = 0; i < version.size(); ++i)
        {
            if (i != 0)
            {
                result.push_back(u'.');
            }

            result += u8_to_u16(std::to_string(version[i]));
        }

        return result;
    }

    std::u16string assembly_identity::encode() const
    {
        std::u16string result = this->name;
        const auto append = [&](const std::u16string_view key, const std::u16string& value) {
            if (!value.empty())
            {
                result += u",";
                result += key;
                result += u"=\"";
                result += value;
                result += u"\"";
            }
        };

        append(u"language", this->language);
        append(u"processorArchitecture", this->processor_architecture);
        append(u"publicKeyToken", this->public_key_token);
        append(u"type", this->type);
        append(u"version", this->version);
        return result;
    }

    std::optional<manifest> parse_manifest(const std::span<const std::byte> data)
    {
        const auto document = parse_xml(data);
        if (!document || document->name != u"assembly")
        {
            return std::nullopt;
        }

        manifest result{};

        if (const auto version = parse_version(attribute_or_empty(*document, u"manifestVersion") + u".0.0"))
        {
            result.manifest_version_major = (*version)[0];
            result.manifest_version_minor = (*version)[1];
        }

        if (const auto* identity = document->child(u"assemblyIdentity"))
        {
            result.identity = read_identity(*identity);
        }

        read_dependencies(*document, result);
        read_files(*document, result);
        read_settings(*document, result);
        read_trust_info(*document, result);
        return result;
    }
}
