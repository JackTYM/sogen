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
        std::uint32_t header_size;
        std::uint32_t entry_count;
        std::uint32_t first_entry_offset;
        std::uint32_t flags;
    };

    struct activation_context_data_toc_entry
    {
        std::uint32_t id;
        std::uint32_t offset;
        std::uint32_t length;
        std::uint32_t format;
    };

    struct activation_context_data_assembly_roster_header
    {
        std::uint32_t header_size;
        std::uint32_t hash_algorithm;
        std::uint32_t entry_count;
        std::uint32_t first_entry_offset;                  // from blob base, NOT biased (fixed-size array, not a section)
        std::uint32_t assembly_information_section_offset; // from blob base, NOT biased
    };

    struct activation_context_data_assembly_roster_entry
    {
        std::uint32_t flags; // 0x1=unused/reserved, 0x2=ROOT (the exe's own identity)
        std::uint32_t pseudo_key;
        std::uint32_t assembly_name_offset; // from blob base, NOT biased; 0 for the unused/ROOT entries
        std::uint32_t assembly_name_length;
        std::uint32_t assembly_information_offset; // from blob base, NOT biased
        std::uint32_t assembly_information_length;
    };

    struct activation_context_string_section_header
    {
        std::uint32_t magic; // 0x64487353 ("SsHd")
        std::uint32_t header_size;
        std::uint32_t format_version;
        std::uint32_t data_format_version;
        std::uint32_t flags;
        std::uint32_t element_count;
        std::uint32_t element_list_offset; // from this section header, NOT biased
        std::uint32_t hash_algorithm;
        std::uint32_t search_structure_offset; // from this section header, NOT biased; 0 = no search structure (linear scan)
        std::uint32_t user_data_offset;
        std::uint32_t user_data_size;
    };

    struct activation_context_string_section_entry
    {
        std::uint32_t pseudo_key;
        std::uint32_t key_offset;            // from the owning section header, NOT biased
        std::uint32_t key_length;            // bytes
        std::uint32_t value_offset;          // from the owning section header, NOT biased
        std::uint32_t value_length;          // bytes
        std::uint32_t assembly_roster_index; // 1-based index into the assembly roster
    };

    struct activation_context_data_assembly_information
    {
        std::uint32_t size;  // 108
        std::uint32_t flags; // 0 for a dependent assembly, 0x11 for the ROOT (self) assembly
        std::uint32_t encoded_assembly_identity_length;
        std::uint32_t encoded_assembly_identity_offset; // from the owning section header
        std::uint32_t manifest_path_type;               // 2 = real file system path
        std::uint32_t manifest_path_length;
        std::uint32_t manifest_path_offset;    // from the owning section header
        std::int64_t manifest_last_write_time; // FILETIME; 0 if unknown
        std::uint32_t policy_path_type;
        std::uint32_t policy_path_length;
        std::uint32_t policy_path_offset;    // from the owning section header
        std::int64_t policy_last_write_time; // FILETIME; 0 if no publisher policy
        std::uint32_t metadata_satellite_roster_index;
        std::uint32_t unused_2;
        std::uint32_t manifest_version_major;
        std::uint32_t manifest_version_minor;
        std::uint32_t policy_version_major;
        std::uint32_t policy_version_minor;
        std::uint32_t assembly_directory_name_length; // 0 for the ROOT assembly (no WinSxS directory)
        std::uint32_t assembly_directory_name_offset; // from the owning section header
        std::uint32_t num_of_files_in_assembly;
        std::uint32_t language_length;
        std::uint32_t language_offset; // from the owning section header
        std::uint32_t run_level;
        std::uint32_t ui_access;
    };

    struct activation_context_data_dll_redirection
    {
        std::uint32_t size;  // 20
        std::uint32_t flags; // 2 = default-redirected (assembly's own directory + the DLL's own file name, no path segments)
        std::uint32_t total_path_length;
        std::uint32_t path_segment_count;
        std::uint32_t path_segment_offset; // from the owning section header
    };

    struct activation_context_data_window_class_redirection
    {
        std::uint32_t size; // 24
        std::uint32_t flags;
        std::uint32_t version_specific_class_name_length;
        std::uint32_t version_specific_class_name_offset; // from this struct's own start
        std::uint32_t dll_name_length;
        std::uint32_t dll_name_offset; // from the owning section header
    };
#pragma pack(pop)

    static_assert(sizeof(activation_context_data_header) == 32);
    static_assert(sizeof(activation_context_data_toc_header) == 16);
    static_assert(sizeof(activation_context_data_toc_entry) == 16);
    static_assert(sizeof(activation_context_data_assembly_roster_header) == 20);
    static_assert(sizeof(activation_context_data_assembly_roster_entry) == 24);
    static_assert(sizeof(activation_context_string_section_header) == 44);
    static_assert(sizeof(activation_context_string_section_entry) == 24);
    static_assert(sizeof(activation_context_data_assembly_information) == 108);
    static_assert(sizeof(activation_context_data_dll_redirection) == 20);
    static_assert(sizeof(activation_context_data_window_class_redirection) == 24);

    constexpr std::uint32_t activation_context_data_magic = 0x78746341;
    constexpr std::uint32_t activation_context_string_section_magic = 0x64487353;
    constexpr std::uint32_t activation_context_data_format_version = 1;

    constexpr std::uint32_t activation_context_section_id_assembly_information = 1;
    constexpr std::uint32_t activation_context_section_id_dll_redirection = 2;
    constexpr std::uint32_t activation_context_section_id_window_class_redirection = 3;
    constexpr std::uint32_t activation_context_section_format_string_table = 1;
}
