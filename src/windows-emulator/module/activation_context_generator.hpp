#pragma once

#include "manifest_parser.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sogen
{
    struct resolved_assembly
    {
        assembly_identity identity;
        std::string concrete_directory_name;      // WinSxS directory name, e.g. "amd64_..._none_..."
        std::string manifest_path;                // full path to the assembly's own .manifest file
        std::string resolved_version;             // the assembly's own real version, e.g. "6.0.26100.33438" -
                                                    // from its own manifest's <assemblyIdentity>, not the
                                                    // requesting exe's manifest (which may just say "6.0.0.0")
        std::vector<std::string> redirected_dlls;  // DLL file names this assembly redirects (its own manifest's <file> entries)
        std::vector<std::string> window_classes;   // window class names this assembly redirects (its own
                                                    // manifest's <windowClass> entries, nested inside <file>)
    };

    struct root_assembly_info
    {
        std::string name;
        std::string version;
        std::string processor_architecture;
        std::string exe_path;
    };

    std::vector<std::uint8_t> generate_activation_context_blob(const root_assembly_info& root,
                                                               const std::vector<resolved_assembly>& assemblies);
}
