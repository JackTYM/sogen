#pragma once

#include <memory>
#include <arch_emulator.hpp>
#include "platform/platform.hpp"

#ifdef FEX_EMULATOR_IMPL
#define FEX_EMULATOR_DLL_STORAGE EXPORT_SYMBOL
#else
#define FEX_EMULATOR_DLL_STORAGE IMPORT_SYMBOL
#endif

namespace sogen::fex
{
    enum class execution_mode
    {
        in_process,
        // The JIT output runs inside a Hypervisor.framework VM with hardware TSO. macOS 15+ on Apple Silicon.
        hypervisor,
    };

#if !SOGEN_BUILD_STATIC
    FEX_EMULATOR_DLL_STORAGE
#endif
    std::unique_ptr<x86_64_emulator> create_x86_64_emulator(execution_mode mode = execution_mode::in_process);
} // namespace sogen::fex
