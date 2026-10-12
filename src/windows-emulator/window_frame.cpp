#include "std_include.hpp"
#include "window_frame.hpp"
#include "windows_emulator.hpp"
#include "font_manager.hpp"

namespace sogen
{
    namespace
    {
        constexpr uint32_t k_color_frame_fill = 0xFFF0F0F0;
        constexpr uint32_t k_color_frame_edge = 0xFF646464;
        constexpr uint32_t k_color_caption_active = 0xFF99B4D1;
        constexpr uint32_t k_color_caption_inactive = 0xFFBFCDDB;
        constexpr uint32_t k_color_caption_text = 0xFF000000;
        constexpr uint32_t k_color_caption_text_inactive = 0xFF434E54;
        constexpr int32_t k_close_button_width = 32;
        constexpr int32_t k_caption_text_padding = 8;
        constexpr uint32_t k_wm_syscommand = 0x0112;
        constexpr uint64_t k_sc_close = 0xF060;

        struct area
        {
            int32_t left{};
            int32_t top{};
            int32_t right{};
            int32_t bottom{};

            bool contains(const int32_t x, const int32_t y) const
            {
                return x >= left && x < right && y >= top && y < bottom;
            }
        };

        area caption_area(const window& top_level)
        {
            const auto frame = top_level.frame();
            return {
                .left = frame.border, .top = frame.border, .right = top_level.width - frame.border, .bottom = frame.border + frame.caption};
        }

        area close_button_area(const window& top_level)
        {
            const auto caption = caption_area(top_level);
            return {.left = caption.right - k_close_button_width, .top = caption.top, .right = caption.right, .bottom = caption.bottom - 1};
        }

        void fill(gdi_bitmap_surface& surface, const area& rect, const uint32_t color)
        {
            const auto left = std::max(0, rect.left);
            const auto top = std::max(0, rect.top);
            const auto right = std::min(static_cast<int32_t>(surface.width), rect.right);
            const auto bottom = std::min(static_cast<int32_t>(surface.height), rect.bottom);
            for (auto y = top; y < bottom; ++y)
            {
                for (auto x = left; x < right; ++x)
                {
                    surface.pixels.at(static_cast<size_t>(y) * surface.width + static_cast<size_t>(x)) = color;
                }
            }
        }

        bool is_active(const windows_emulator& win_emu, const window& top_level)
        {
            if (win_emu.process.foreground_window == top_level.handle)
            {
                return true;
            }

            for (auto current = win_emu.process.focus_window; current != 0;)
            {
                if (current == top_level.handle)
                {
                    return true;
                }

                const auto* focused = win_emu.process.windows.get(current);
                current = focused != nullptr && (focused->style & WS_CHILD) != 0 ? focused->parent_handle : 0;
            }

            return false;
        }

        void draw_close_glyph(gdi_bitmap_surface& surface, const area& button, const uint32_t color)
        {
            constexpr int32_t half = 5;
            const auto center_x = (button.left + button.right) / 2;
            const auto center_y = (button.top + button.bottom) / 2;
            for (int32_t i = -half; i <= half; ++i)
            {
                fill(surface, {.left = center_x + i, .top = center_y + i, .right = center_x + i + 1, .bottom = center_y + i + 1}, color);
                fill(surface, {.left = center_x + i, .top = center_y - i, .right = center_x + i + 1, .bottom = center_y - i + 1}, color);
            }
        }
    }

    void draw_window_frame(windows_emulator& win_emu, const window& top_level, gdi_bitmap_surface& composed)
    {
        const auto frame = top_level.frame();
        const auto width = static_cast<int32_t>(composed.width);
        const auto height = static_cast<int32_t>(composed.height);
        const bool active = is_active(win_emu, top_level);

        fill(composed, {.left = 0, .top = 0, .right = width, .bottom = frame.border}, k_color_frame_fill);
        fill(composed, {.left = 0, .top = height - frame.border, .right = width, .bottom = height}, k_color_frame_fill);
        fill(composed, {.left = 0, .top = 0, .right = frame.border, .bottom = height}, k_color_frame_fill);
        fill(composed, {.left = width - frame.border, .top = 0, .right = width, .bottom = height}, k_color_frame_fill);
        fill(composed, {.left = 0, .top = 0, .right = width, .bottom = 1}, k_color_frame_edge);
        fill(composed, {.left = 0, .top = height - 1, .right = width, .bottom = height}, k_color_frame_edge);
        fill(composed, {.left = 0, .top = 0, .right = 1, .bottom = height}, k_color_frame_edge);
        fill(composed, {.left = width - 1, .top = 0, .right = width, .bottom = height}, k_color_frame_edge);

        if (frame.caption == 0)
        {
            return;
        }

        const auto caption = caption_area(top_level);
        fill(composed, caption, k_color_frame_fill);
        fill(composed, {.left = caption.left, .top = caption.top, .right = caption.right, .bottom = caption.bottom - 1},
             active ? k_color_caption_active : k_color_caption_inactive);

        const auto text_color = active ? k_color_caption_text : k_color_caption_text_inactive;
        if (auto* font = win_emu.fonts.resolve({.face = u"Segoe UI", .height = -12}).get())
        {
            const auto text_top = caption.top + (caption.bottom - 1 - caption.top - font->metrics().height) / 2;
            auto pen_x = caption.left + k_caption_text_padding;
            const auto close = close_button_area(top_level);
            for (const auto character : top_level.name)
            {
                if (pen_x + font->advance(character) > close.left)
                {
                    break;
                }

                font->draw_glyph(composed, pen_x, text_top, character, text_color, nullptr);
                pen_x += font->advance(character);
            }
        }

        draw_close_glyph(composed, close_button_area(top_level), text_color);
    }

    bool handle_frame_pointer(windows_emulator& win_emu, const window& top_level, const uint32_t message, const int32_t x, const int32_t y)
    {
        if (!top_level.is_decorated())
        {
            return false;
        }

        const auto frame = top_level.frame();
        const area client{.left = frame.border,
                          .top = top_level.client_top(),
                          .right = top_level.width - frame.border,
                          .bottom = top_level.height - frame.border};
        if (client.contains(x, y))
        {
            return false;
        }

        win_emu.process.foreground_window = top_level.handle;

        if (message == WM_LBUTTONUP && close_button_area(top_level).contains(x, y))
        {
            if (auto* thread = win_emu.process.find_thread_by_id(top_level.thread_id))
            {
                msg posted{};
                posted.window = top_level.handle;
                posted.message = k_wm_syscommand;
                posted.wParam = k_sc_close;
                posted.lParam = 0;
                thread->post_message(win_emu, posted);
            }
        }

        return true;
    }
}
