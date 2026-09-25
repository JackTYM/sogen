#pragma once

#include "manifest_parser.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sogen
{
    using winners_lookup_fn = std::function<std::optional<std::string>(const std::string& winners_key)>;

    std::optional<std::string> resolve_assembly(const assembly_identity& identity, const winners_lookup_fn& winners,
                                                const std::vector<std::string>& manifest_filenames);
}
