#pragma once
#include "std_include.hpp"
#include "process_context.hpp"

namespace sogen
{
    // Render a Unicode codepoint using a FreeType-backed font into a GDI bitmap surface.
    // x, y is the top-left of the character cell (k_default_font_width × k_default_font_height).
    // Returns true if a font is loaded and the glyph was rendered; false if the caller should
    // fall back to the ASCII debug font.
    bool ft_draw_glyph(gdi_bitmap_surface& surface, int x, int y, char32_t codepoint,
                       uint32_t color, const RECT* clip);
}
