#pragma once

#include "std_include.hpp"
#include "process_context.hpp"
#include "windows_objects.hpp"

namespace sogen
{
    class windows_emulator;

    // Presents a top-level window's surface to the host. Windows with a menu get the menu bar and any open popup
    // composed over a copy of the surface, so the guest-owned surface itself never contains menu pixels.
    void present_window_surface(windows_emulator& win_emu, hwnd window, const gdi_bitmap_surface& surface);

    // Routes a pointer message (top-level-local coordinates) to the menu bar and open popups. Returns true when the
    // event was consumed and must not reach the guest.
    bool handle_menu_pointer(windows_emulator& win_emu, const window& top_level, uint32_t message, int32_t x, int32_t y);
}
