#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sogen::sxs
{
    struct xml_element
    {
        std::u16string name{};
        std::u16string namespace_uri{};
        std::vector<std::pair<std::u16string, std::u16string>> attributes{};
        std::u16string text{};
        std::vector<xml_element> children{};

        const std::u16string* attribute(std::u16string_view attribute_name) const;
        const xml_element* child(std::u16string_view child_name) const;
        std::vector<const xml_element*> children_named(std::u16string_view child_name) const;
    };

    // Parses a manifest document (UTF-8 or UTF-16LE, with or without BOM) into its root element.
    // Element and attribute names are reduced to their local part; every element carries the namespace
    // URI its prefix (or the default namespace) resolves to.
    std::optional<xml_element> parse_xml(std::span<const std::byte> data);
}
