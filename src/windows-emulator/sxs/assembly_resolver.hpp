#pragma once

#include "activation_context_data.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sogen::sxs
{
    class assembly_store
    {
      public:
        virtual ~assembly_store() = default;

        virtual std::optional<std::vector<std::byte>> read_file(std::u16string_view windows_path) = 0;
        // File names (not paths) directly inside the directory.
        virtual std::vector<std::u16string> list_files(std::u16string_view windows_directory) = 0;
    };

    struct resolution_request
    {
        std::vector<std::byte> root_manifest{};
        std::u16string root_manifest_path{};
        std::u16string application_directory{};
        // "x86" or "amd64": what processorArchitecture="*" binds to for this client.
        std::u16string processor_architecture{};
        std::u16string windows_directory{};
        uint32_t resource_name{};
    };

    // Binds the root manifest's dependency graph the way sxs does: publisher policy from
    // %WINDIR%\WinSxS\Manifests redirects requested versions, the matching component manifest
    // supplies the assembly, and assemblies without a public key token are probed next to the
    // application. Returns an NTSTATUS on failure (STATUS_SXS_*).
    std::variant<activation_context_input, uint32_t> resolve_activation_context(const resolution_request& request, assembly_store& store);
}
