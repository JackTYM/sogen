#include "../std_include.hpp"
#include "activation_context_format.hpp"
#include "activation_context_parser.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace sogen
{
    namespace
    {
        constexpr std::array<const char*, 2> section_magic_tags = {"SsHd", "GsHd"};
    }

    std::vector<located_section> find_all_sections(const std::vector<std::uint8_t>& blob)
    {
        std::vector<std::uint64_t> positions{};

        if (blob.size() >= sizeof(activation_context_data_header))
        {
            activation_context_data_header header{};
            std::memcpy(&header, blob.data(), sizeof(header));
            if (header.magic != activation_context_data_magic || header.format_version != activation_context_data_format_version)
            {
                return {};
            }
        }
        else
        {
            return {};
        }

        for (const auto* tag : section_magic_tags)
        {
            const auto tag_length = std::strlen(tag);
            if (blob.size() < tag_length)
            {
                continue;
            }

            for (std::size_t i = 0; i + tag_length <= blob.size(); ++i)
            {
                if (std::memcmp(blob.data() + i, tag, tag_length) == 0)
                {
                    positions.push_back(i);
                }
            }
        }

        std::sort(positions.begin(), positions.end());

        std::vector<located_section> result{};
        result.reserve(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i)
        {
            const auto end = (i + 1 < positions.size()) ? positions[i + 1] : blob.size();
            result.push_back({positions[i], end});
        }

        return result;
    }

    std::vector<std::uint8_t> section_bytes(const std::vector<std::uint8_t>& blob, const located_section& section)
    {
        if (section.start > section.end || section.end > blob.size())
        {
            return {};
        }

        return {blob.begin() + static_cast<std::ptrdiff_t>(section.start), blob.begin() + static_cast<std::ptrdiff_t>(section.end)};
    }

    std::optional<std::string> find_wide_string_in_section(const std::vector<std::uint8_t>& section, const std::string& needle_utf8)
    {
        std::u16string needle{};
        for (const char c : needle_utf8)
        {
            needle.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
        }

        const auto needle_bytes = needle.size() * sizeof(char16_t);
        if (needle_bytes == 0 || section.size() < needle_bytes)
        {
            return std::nullopt;
        }

        for (std::size_t i = 0; i + needle_bytes <= section.size(); ++i)
        {
            if (std::memcmp(section.data() + i, needle.data(), needle_bytes) == 0)
            {
                return needle_utf8;
            }
        }

        return std::nullopt;
    }
}
