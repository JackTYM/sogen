#include "../std_include.hpp"
#include "activation_context_builder.hpp"

#include "activation_context_generator.hpp"
#include "manifest_parser.hpp"
#include "sxs_resolver.hpp"

#include "../file_system.hpp"
#include "../registry/registry_utils.hpp"
#include "../windows_path.hpp"
#include <memory_interface.hpp>

#include <fstream>

namespace sogen
{
    namespace
    {
        std::vector<std::string> list_winsxs_manifest_filenames(const file_system& files)
        {
            std::vector<std::string> result{};

            std::error_code ec{};
            const auto host_dir = files.translate(windows_path(uR"(C:\Windows\WinSxS\Manifests)"));
            for (const auto& entry : std::filesystem::directory_iterator(host_dir, ec))
            {
                if (entry.is_regular_file())
                {
                    result.push_back(entry.path().filename().string());
                }
            }

            return result;
        }

        std::optional<std::string> read_winners_value(registry_manager& registry, const std::string& key)
        {
            const std::filesystem::path winners_path = R"(\Registry\Machine\Software\SideBySide\Winners)";
            const auto value = registry_utils::read_registry_string(registry, winners_path / key, "");
            if (!value.has_value() || value->empty())
            {
                return std::nullopt;
            }

            return u16_to_u8(*value);
        }

        std::optional<std::string> read_file_as_string(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                return std::nullopt;
            }

            return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
    }

    std::optional<std::vector<std::uint8_t>> build_activation_context_blob(memory_interface& memory, registry_manager& registry,
                                                                           const file_system& files, const std::uint64_t image_base,
                                                                           const windows_path& image_path)
    {
        const auto manifest_text = find_manifest_resource(memory, image_base);
        if (!manifest_text.has_value())
        {
            return std::nullopt;
        }

        const auto dependent_identities = parse_dependent_assemblies(*manifest_text);
        if (dependent_identities.empty())
        {
            return std::nullopt;
        }

        const auto manifest_filenames = list_winsxs_manifest_filenames(files);
        const winners_lookup_fn winners = [&registry](const std::string& key) { return read_winners_value(registry, key); };

        std::vector<resolved_assembly> resolved_assemblies{};

        for (const auto& identity : dependent_identities)
        {
            const auto directory_name = resolve_assembly(identity, winners, manifest_filenames);
            if (!directory_name.has_value())
            {
                continue;
            }

            const windows_path manifest_guest_path =
                windows_path(uR"(C:\Windows\WinSxS\Manifests)") / windows_path(u8_to_u16(*directory_name + ".manifest"));
            const auto host_manifest_path = files.translate(manifest_guest_path);

            std::vector<std::string> redirected_dlls{};
            if (const auto assembly_manifest_text = read_file_as_string(host_manifest_path); assembly_manifest_text.has_value())
            {
                redirected_dlls = parse_redirected_file_names(*assembly_manifest_text);
            }

            resolved_assemblies.push_back(resolved_assembly{
                .identity = identity,
                .concrete_directory_name = *directory_name,
                .manifest_path = manifest_guest_path.string(),
                .redirected_dlls = std::move(redirected_dlls),
            });
        }

        if (resolved_assemblies.empty())
        {
            return std::nullopt;
        }

        const auto self_identity = parse_self_identity(*manifest_text);

        const root_assembly_info root{
            .name = self_identity.has_value() ? self_identity->name : "",
            .version = self_identity.has_value() ? self_identity->version : "",
            .processor_architecture = self_identity.has_value() ? self_identity->processor_architecture : "",
            .exe_path = image_path.string(),
        };

        return generate_activation_context_blob(root, resolved_assemblies);
    }
}
