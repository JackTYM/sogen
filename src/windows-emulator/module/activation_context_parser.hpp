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
    //
    // Section location is done by scanning the blob for each section's own magic tag
    // ("SsHd"/"GsHd") directly, NOT by trusting the TOC entry array's offset/length fields.
    // Differential analysis across three independent real captures found that the TOC array's
    // per-entry byte position carries a real but *inconsistent* bias (0, 1, or more bytes,
    // varying per section and per capture) that couldn't be resolved to a fixed formula, while
    // whole-blob magic-tag scanning located every real section correctly, with zero exceptions,
    // in all three captures. See the format doc's "Differential analysis" section.

    struct located_section
    {
        std::uint64_t start; // the magic tag's own position - the section's real content start
        std::uint64_t end;   // the next section's start, or the blob's total_size for the last one
    };

    // Every "SsHd"/"GsHd"-tagged section in the blob, in the order they appear. The first entry
    // is always the assembly-identity-strings section in every capture observed (this is the
    // real TOC's own id=1, but that correspondence is inferred from ordering, not re-derived
    // from the TOC array here).
    std::vector<located_section> find_all_sections(const std::vector<std::uint8_t>& blob);

    std::vector<std::uint8_t> section_bytes(const std::vector<std::uint8_t>& blob, const located_section& section);

    std::optional<std::string> find_wide_string_in_section(const std::vector<std::uint8_t>& section, const std::string& needle_utf8);
}
