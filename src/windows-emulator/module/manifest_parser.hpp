#pragma once

#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace sogen
{
    struct assembly_identity
    {
        std::string name;
        std::string version;
        std::string processor_architecture;
        std::string public_key_token;
        std::string language;
    };

    std::vector<assembly_identity> parse_dependent_assemblies(std::string_view manifest_text);
    std::vector<std::string> parse_redirected_file_names(std::string_view manifest_text);
    std::vector<std::string> parse_window_classes(std::string_view manifest_text);
    std::optional<assembly_identity> parse_self_identity(std::string_view manifest_text);

    class memory_interface;
    std::optional<std::string> find_manifest_resource(memory_interface& memory, std::uint64_t image_base);

    struct pe_resource_name
    {
        std::uint32_t id{};
        std::u16string text{};
    };

    std::optional<std::vector<std::uint8_t>> find_pe_resource(memory_interface& memory, std::uint64_t image_base, std::uint32_t type,
                                                              const pe_resource_name& name);
}
