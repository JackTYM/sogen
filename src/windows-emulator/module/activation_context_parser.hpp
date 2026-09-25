#pragma once

#include "activation_context_format.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sogen
{
    // Test/validation-only parsing helpers for ACTIVATION_CONTEXT_DATA blobs (not used at
    // runtime by the emulator - see docs/superpowers/specs/2026-09-24-activation-context-data-format.md
    // for what's confirmed vs. still undetermined about this format).

    std::optional<activation_context_data_toc_entry> find_toc_entry(const std::vector<std::uint8_t>& blob, std::uint32_t id);

    std::vector<std::uint8_t> get_section_bytes(const std::vector<std::uint8_t>& blob, const activation_context_data_toc_entry& entry);

    std::optional<std::string> find_wide_string_in_section(const std::vector<std::uint8_t>& section, const std::string& needle_utf8);
}
