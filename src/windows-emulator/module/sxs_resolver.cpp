#include "../std_include.hpp"
#include "sxs_resolver.hpp"

#include <algorithm>
#include <cctype>

namespace sogen
{
    namespace
    {
        std::string to_lower(std::string value)
        {
            std::ranges::transform(value, value.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }

        std::string arch_prefix(const std::string& processor_architecture)
        {
            const auto arch = to_lower(processor_architecture);
            if (arch == "x86")
            {
                return "x86_";
            }
            // "amd64", "*", and anything else requesting a 64-bit process resolve against the
            // amd64_ prefix - "*" (any architecture) is what most manifests actually specify,
            // and the caller only ever resolves within a single process's own bitness.
            return "amd64_";
        }

        std::string identity_prefix(const assembly_identity& identity)
        {
            return arch_prefix(identity.processor_architecture) + to_lower(identity.name) + "_" + to_lower(identity.public_key_token) + "_";
        }

        std::optional<std::string> strip_manifest_suffix(const std::string& filename)
        {
            constexpr std::string_view suffix = ".manifest";
            if (filename.size() <= suffix.size() || filename.substr(filename.size() - suffix.size()) != suffix)
            {
                return std::nullopt;
            }
            return filename.substr(0, filename.size() - suffix.size());
        }
    }

    std::optional<std::string> resolve_assembly(const assembly_identity& identity, const winners_lookup_fn& winners,
                                                const std::vector<std::string>& manifest_filenames)
    {
        const auto prefix = identity_prefix(identity);

        std::vector<std::string> matching_dirs{};
        for (const auto& filename : manifest_filenames)
        {
            const auto dir_name = strip_manifest_suffix(filename);
            if (!dir_name.has_value())
            {
                continue;
            }
            if (to_lower(*dir_name).starts_with(prefix))
            {
                matching_dirs.push_back(*dir_name);
            }
        }

        if (matching_dirs.empty())
        {
            return std::nullopt;
        }

        // The Winners key is the identity prefix with the trailing underscore removed - real
        // Windows keys this table by "arch_name_token" (no version, no "_none_hash" suffix).
        const auto winners_key = prefix.substr(0, prefix.size() - 1);
        if (const auto winner = winners(winners_key); winner.has_value())
        {
            const auto it = std::ranges::find(matching_dirs, *winner);
            if (it != matching_dirs.end())
            {
                return *it;
            }
            // Winners named a version this root didn't actually collect - fall through to the
            // highest-version fallback below rather than failing outright.
        }

        std::ranges::sort(matching_dirs);
        return matching_dirs.back();
    }
}
