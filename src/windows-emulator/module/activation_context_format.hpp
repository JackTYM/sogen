#pragma once

#include <cstdint>

namespace sogen
{
#pragma pack(push, 1)
    struct activation_context_data_header
    {
        std::uint32_t magic; // 0x78746341 ("Actx")
        std::uint32_t header_size;
        std::uint32_t format_version;
        std::uint32_t total_size;
        std::uint32_t default_toc_offset;
        std::uint32_t extended_toc_offset;
        std::uint32_t assembly_roster_offset;
        std::uint32_t flags;
    };

    struct activation_context_data_toc_header
    {
        std::uint32_t entry_size;
        std::uint32_t entry_count;
        std::uint32_t first_entry_offset;
    };

    struct activation_context_data_toc_entry
    {
        std::uint32_t id;
        std::uint32_t offset;
        std::uint32_t length;
        std::uint32_t format;
    };
#pragma pack(pop)

    static_assert(sizeof(activation_context_data_header) == 32);
    static_assert(sizeof(activation_context_data_toc_header) == 12);
    static_assert(sizeof(activation_context_data_toc_entry) == 16);

    constexpr std::uint32_t activation_context_data_magic = 0x78746341;
    constexpr std::uint32_t activation_context_data_format_version = 1;

    // Every TOC entry's declared `offset` is empirically one byte less than the real start of
    // the section's content (confirmed against the golden fixture: every section's real magic
    // tag - "SsHd" or "GsHd" - and every string's real first character sit at offset+1, never
    // at offset itself). The reason isn't known; the relationship is consistent and reliable.
    constexpr std::uint32_t activation_context_data_toc_entry_offset_bias = 1;
}
