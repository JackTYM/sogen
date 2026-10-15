#pragma once

#include "assembly_resolver.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sogen
{
    class file_system;
    class windows_emulator;
    struct mapped_module;
}

namespace sogen::sxs
{
    class emulated_assembly_store : public assembly_store
    {
      public:
        explicit emulated_assembly_store(const file_system& file_sys);

        std::optional<std::vector<std::byte>> read_file(std::u16string_view windows_path_text) override;
        std::vector<std::u16string> list_files(std::u16string_view windows_directory) override;

      private:
        const file_system& file_sys_;

        std::optional<std::filesystem::path> translate(std::u16string_view windows_path_text) const;
    };

    std::optional<std::vector<std::byte>> read_host_file(const std::filesystem::path& path);
    std::u16string directory_of(std::u16string_view path);

    // Maps activation context data into the emulated process the way csrss maps its section view: read-only,
    // and below 2 GB for WoW64 processes, whose 32-bit side receives the pointer narrowed. Returns 0 on failure.
    uint64_t map_activation_context_data(windows_emulator& win_emu, const std::vector<std::byte>& data);

    // csrss builds the activation context of an executable's manifest during process creation and publishes it
    // in PEB.ActivationContextData before the loader resolves the executable's imports.
    void create_process_default_activation_context(windows_emulator& win_emu, const mapped_module& executable);
}
