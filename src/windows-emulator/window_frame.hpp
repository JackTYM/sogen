#pragma once

#include "std_include.hpp"
#include "process_context.hpp"
#include "windows_objects.hpp"

namespace sogen
{
    class windows_emulator;

    // Draws the frame, title bar and close button of a decorated top-level window over the composed surface.
    void draw_window_frame(windows_emulator& win_emu, const window& top_level, gdi_bitmap_surface& composed);

    // Routes a pointer message (top-level-local coordinates) that lands on a decorated window's frame. Returns true
    // when the event was consumed and must not reach the guest.
    bool handle_frame_pointer(windows_emulator& win_emu, const window& top_level, uint32_t message, int32_t x, int32_t y);
}
