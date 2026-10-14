#pragma once

#include "manifest.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sogen::sxs
{
    // ACTIVATION_CONTEXT_DATA_ASSEMBLY_INFORMATION flags
    constexpr uint32_t assembly_information_root_assembly = 0x1;
    constexpr uint32_t assembly_information_policy_applied = 0x4;
    constexpr uint32_t assembly_information_private_assembly = 0x10;

    struct resolved_assembly
    {
        assembly_identity identity{};
        manifest content{};
        std::u16string manifest_path{};
        std::u16string directory_name{};
        uint32_t flags{};
        uint64_t manifest_last_write_time{};
    };

    struct activation_context_input
    {
        // The root (application or DLL) assembly comes first, followed by everything it binds to.
        std::vector<resolved_assembly> assemblies{};
        std::u16string application_directory{};
        uint32_t resource_name{};
    };

    // Serializes the binary ACTIVATION_CONTEXT_DATA that csrss hands to RtlCreateActivationContext:
    // header, table of contents, assembly roster, and the string sections ntdll and user32 search
    // (assembly information, DLL redirection, window class redirection, application settings).
    std::vector<std::byte> build_activation_context_data(const activation_context_input& input);

    // The case-insensitive x65599 pseudo-key sxs and ntdll use to index string sections.
    uint32_t hash_section_key(std::u16string_view key);
}
