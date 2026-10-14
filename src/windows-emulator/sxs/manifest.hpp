#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sogen::sxs
{
    using assembly_version = std::array<uint16_t, 4>;

    std::optional<assembly_version> parse_version(std::u16string_view text);
    std::u16string format_version(const assembly_version& version);

    struct assembly_identity
    {
        std::u16string name{};
        std::u16string type{};
        std::u16string version{};
        std::u16string processor_architecture{};
        std::u16string public_key_token{};
        std::u16string language{};

        // The textual form sxs uses for an identity: the name followed by its attributes in
        // alphabetical order, e.g. Microsoft.Windows.Common-Controls,processorArchitecture="amd64",...
        std::u16string encode() const;
    };

    struct window_class
    {
        std::u16string name{};
        bool versioned{true};
    };

    struct manifest_file
    {
        std::u16string name{};
        std::vector<window_class> window_classes{};
    };

    struct dependency
    {
        assembly_identity identity{};
        bool optional{};
    };

    struct application_setting
    {
        std::u16string namespace_uri{};
        std::u16string name{};
        std::u16string value{};
    };

    struct binding_redirect
    {
        assembly_version old_low{};
        assembly_version old_high{};
        assembly_version new_version{};
    };

    // ACTCTX_REQUESTED_RUN_LEVEL
    enum class run_level : uint32_t
    {
        unspecified = 0,
        as_invoker = 1,
        highest_available = 2,
        require_admin = 3,
    };

    struct manifest
    {
        assembly_identity identity{};
        uint32_t manifest_version_major{1};
        uint32_t manifest_version_minor{0};
        std::vector<dependency> dependencies{};
        std::vector<manifest_file> files{};
        std::vector<application_setting> settings{};
        std::vector<binding_redirect> binding_redirects{};
        run_level requested_run_level{run_level::unspecified};
        bool ui_access{};
    };

    std::optional<manifest> parse_manifest(std::span<const std::byte> data);
}
