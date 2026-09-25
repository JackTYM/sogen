#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace sogen
{
    class memory_interface;
    class registry_manager;
    class file_system;
    class windows_path;

    // Parses the module's embedded manifest, resolves any dependent WinSxS assemblies (e.g.
    // Common-Controls v6), and builds a real ACTIVATION_CONTEXT_DATA blob so the guest's own
    // ntdll can redirect LoadLibrary calls (e.g. COMCTL32.dll) through the WinSxS path. Returns
    // std::nullopt when the module has no manifest, or the manifest requests no dependencies
    // that could be resolved - callers should leave PEB->ActivationContextData null in that case,
    // matching real Windows' behavior for unmanifested/dependency-free processes.
    std::optional<std::vector<std::uint8_t>> build_activation_context_blob(memory_interface& memory, registry_manager& registry,
                                                                           const file_system& files, std::uint64_t image_base,
                                                                           const windows_path& image_path);
}
