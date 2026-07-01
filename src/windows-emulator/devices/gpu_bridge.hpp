#pragma once

#include "../io_device.hpp"

namespace sogen
{
    class windows_emulator;

    std::unique_ptr<io_device> create_gpu_bridge();

    // Transport-agnostic GPU command path shared by \\.\SogenGpu and the D3DKMTEscape channel.
    // The processor's concrete type is internal to gpu_bridge.cpp, so it is held opaquely here.
    std::shared_ptr<void> create_gpu_command_processor();
    NTSTATUS dispatch_gpu_command(void* processor, windows_emulator& win_emu, const io_device_context& context);
    void pump_gpu_presents(void* processor, windows_emulator& win_emu);
}
