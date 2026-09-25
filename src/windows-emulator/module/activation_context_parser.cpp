#include "../std_include.hpp"
#include "activation_context_format.hpp"
#include "activation_context_parser.hpp"

#include <cstring>

namespace sogen
{
    std::optional<activation_context_data_toc_entry> find_toc_entry(const std::vector<std::uint8_t>& blob, const std::uint32_t id)
    {
        if (blob.size() < sizeof(activation_context_data_header))
        {
            return std::nullopt;
        }

        activation_context_data_header header{};
        std::memcpy(&header, blob.data(), sizeof(header));

        if (header.magic != activation_context_data_magic || header.format_version != activation_context_data_format_version)
        {
            return std::nullopt;
        }

        if (header.default_toc_offset + sizeof(activation_context_data_toc_header) > blob.size())
        {
            return std::nullopt;
        }

        activation_context_data_toc_header toc_header{};
        std::memcpy(&toc_header, blob.data() + header.default_toc_offset, sizeof(toc_header));

        for (std::uint32_t i = 0; i < toc_header.entry_count; ++i)
        {
            const auto entry_offset =
                static_cast<std::uint64_t>(toc_header.first_entry_offset) + static_cast<std::uint64_t>(i) * toc_header.entry_size;
            if (entry_offset + sizeof(activation_context_data_toc_entry) > blob.size())
            {
                continue;
            }

            activation_context_data_toc_entry entry{};
            std::memcpy(&entry, blob.data() + entry_offset, sizeof(entry));

            // Some TOC slots (observed for section ids not present in a given blob, e.g. ids 7/8
            // when only 1/2/3/4/5/6/9 are populated) contain stale/uninitialized data rather than
            // a real section - the offset+length sanity check filters those out reliably.
            if (static_cast<std::uint64_t>(entry.offset) + entry.length > blob.size())
            {
                continue;
            }

            if (entry.id == id)
            {
                return entry;
            }
        }

        return std::nullopt;
    }

    std::vector<std::uint8_t> get_section_bytes(const std::vector<std::uint8_t>& blob, const activation_context_data_toc_entry& entry)
    {
        // See activation_context_data_toc_entry_offset_bias - the real content starts one byte
        // after the entry's declared offset, consistently, across every section observed.
        const auto real_start = static_cast<std::uint64_t>(entry.offset) + activation_context_data_toc_entry_offset_bias;
        const auto real_length =
            entry.length > activation_context_data_toc_entry_offset_bias ? entry.length - activation_context_data_toc_entry_offset_bias : 0;

        if (real_start + real_length > blob.size())
        {
            return {};
        }

        return {blob.begin() + static_cast<std::ptrdiff_t>(real_start),
                blob.begin() + static_cast<std::ptrdiff_t>(real_start + real_length)};
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
