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

    class memory_interface;
    std::optional<std::string> find_manifest_resource(memory_interface& memory, std::uint64_t image_base);
}
