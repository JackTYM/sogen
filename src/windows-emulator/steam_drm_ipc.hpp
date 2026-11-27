#pragma once

#include "syscall_utils.hpp"

namespace sogen::steam_drm_ipc
{
    // Plays the start-acknowledgement role of Proton's steam.exe (steam_helper, steam_drm_thread) for a game
    // that Steam launched through a compatibility tool.
    void on_semaphore_released(const syscall_context& c, semaphore& released);
}
