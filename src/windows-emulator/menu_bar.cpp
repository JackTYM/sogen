#include "std_include.hpp"
#include "menu_bar.hpp"
#include "debug_font.hpp"
#include "windows_emulator.hpp"
#include "font_renderer.hpp"
#include "font_manager.hpp"

namespace sogen
{
    namespace
    {
        constexpr uint32_t k_wm_command = 0x0111;
        constexpr uint32_t k_wm_initmenupopup = 0x0117;
        constexpr uint32_t k_mft_separator = 0x00000800;
        constexpr uint32_t k_mfs_disabled = 0x00000003;
        constexpr uint32_t k_mfs_checked = 0x00000008;

        constexpr int32_t k_char_width = 8;
        constexpr int32_t k_char_height = 16;
        constexpr int32_t k_bar_item_padding = 8;
        constexpr int32_t k_popup_row_height = 22;
        constexpr int32_t k_popup_separator_height = 9;
        constexpr int32_t k_popup_margin = 3;
        constexpr int32_t k_popup_check_width = 22;
        constexpr int32_t k_popup_arrow_width = 20;
        constexpr int32_t k_popup_accelerator_gap = 24;
        constexpr int32_t k_popup_min_width = 120;

        constexpr uint32_t k_color_bar = 0xFFF0F0F0;
        constexpr uint32_t k_color_bar_edge = 0xFFD7D7D7;
        constexpr uint32_t k_color_popup = 0xFFF2F2F2;
        constexpr uint32_t k_color_popup_edge = 0xFF979797;
        constexpr uint32_t k_color_highlight = 0xFF91C9F7;
        constexpr uint32_t k_color_text = 0xFF000000;
        constexpr uint32_t k_color_text_disabled = 0xFF6D6D6D;
        constexpr uint32_t k_color_separator = 0xFFA0A0A0;

        struct item_label
        {
            std::u16string text{};
            std::u16string accelerator{};
        };

        item_label split_label(const menu_item& item)
        {
            std::u16string visible{};
            for (size_t i = 0; i < item.text.size(); ++i)
            {
                if (item.text[i] == u'&' && i + 1 < item.text.size())
                {
                    ++i;
                }
                visible.push_back(item.text[i]);
            }

            const auto tab = visible.find(u'\t');
            if (tab == std::u16string::npos)
            {
                return {.text = std::move(visible)};
            }

            return {.text = visible.substr(0, tab), .accelerator = visible.substr(tab + 1)};
        }

        bool is_separator(const menu_item& item)
        {
            return (item.type & k_mft_separator) != 0;
        }

        bool is_enabled(const menu_item& item)
        {
            return (item.state & k_mfs_disabled) == 0 && !is_separator(item);
        }

        // The menu font of the window being drawn or hit-tested; null when the emulation root and the bundle provide no
        // fonts, in which case the fixed 8x16 cell is used.
        font_instance* g_menu_font = nullptr;

        void select_menu_font(windows_emulator& win_emu)
        {
            g_menu_font = win_emu.fonts.resolve({.face = u"Segoe UI", .height = -12}).get();
        }

        int32_t char_height()
        {
            return g_menu_font != nullptr ? g_menu_font->metrics().height : k_char_height;
        }

        int32_t text_width(const std::u16string_view text)
        {
            return g_menu_font != nullptr ? g_menu_font->text_width(text) : static_cast<int32_t>(text.size()) * k_char_width;
        }

        struct slot
        {
            int32_t left{};
            int32_t top{};
            int32_t width{};
            int32_t height{};

            bool contains(const int32_t x, const int32_t y) const
            {
                return x >= this->left && x < this->left + this->width && y >= this->top && y < this->top + this->height;
            }
        };

        std::vector<slot> layout_bar(const menu& bar)
        {
            std::vector<slot> slots{};
            int32_t left = 2;
            for (const auto& item : bar.items)
            {
                const auto width = text_width(split_label(item).text) + 2 * k_bar_item_padding;
                slots.push_back({.left = left, .top = 0, .width = width, .height = window::menu_bar_height});
                left += width;
            }
            return slots;
        }

        struct popup_layout
        {
            slot bounds{};
            std::vector<slot> rows{};
        };

        popup_layout layout_popup(const menu& popup, const int32_t left, const int32_t top, const int32_t surface_width)
        {
            int32_t label_width = 0;
            int32_t accelerator_width = 0;
            int32_t height = 2 * k_popup_margin;
            for (const auto& item : popup.items)
            {
                const auto label = split_label(item);
                label_width = std::max(label_width, text_width(label.text));
                accelerator_width = std::max(accelerator_width, text_width(label.accelerator));
                height += is_separator(item) ? k_popup_separator_height : k_popup_row_height;
            }

            auto width = k_popup_check_width + label_width + k_popup_arrow_width + 2 * k_popup_margin;
            if (accelerator_width != 0)
            {
                width += k_popup_accelerator_gap + accelerator_width;
            }
            width = std::max(width, k_popup_min_width);

            popup_layout layout{};
            layout.bounds = {.left = std::max(0, std::min(left, surface_width - width)), .top = top, .width = width, .height = height};

            int32_t row_top = top + k_popup_margin;
            for (const auto& item : popup.items)
            {
                const auto row_height = is_separator(item) ? k_popup_separator_height : k_popup_row_height;
                layout.rows.push_back({.left = layout.bounds.left + k_popup_margin,
                                       .top = row_top,
                                       .width = width - 2 * k_popup_margin,
                                       .height = row_height});
                row_top += row_height;
            }

            return layout;
        }

        void fill_rect(gdi_bitmap_surface& surface, const slot& area, const uint32_t color)
        {
            const auto left = std::max(0, area.left);
            const auto top = std::max(0, area.top);
            const auto right = std::min(static_cast<int32_t>(surface.width), area.left + area.width);
            const auto bottom = std::min(static_cast<int32_t>(surface.height), area.top + area.height);
            for (auto y = top; y < bottom; ++y)
            {
                for (auto x = left; x < right; ++x)
                {
                    surface.pixels[static_cast<size_t>(y) * surface.width + static_cast<size_t>(x)] = color;
                }
            }
        }

        void draw_debug_glyph(gdi_bitmap_surface& surface, const int32_t x, const int32_t y, char32_t codepoint, const uint32_t color)
        {
            if (codepoint < debug_font::first_codepoint || codepoint > debug_font::last_codepoint)
            {
                codepoint = U'?';
            }

            const auto& glyph = debug_font::glyphs[static_cast<size_t>(codepoint - debug_font::first_codepoint)];
            for (int32_t row = 0; row < debug_font::glyph_height; ++row)
            {
                for (int32_t col = 0; col < debug_font::glyph_width; ++col)
                {
                    if ((glyph[static_cast<size_t>(row)] & (0x01u << col)) != 0)
                    {
                        fill_rect(surface, {.left = x + col, .top = y + row, .width = 1, .height = 1}, color);
                    }
                }
            }
        }

        void draw_text(gdi_bitmap_surface& surface, int32_t x, const int32_t y, const std::u16string_view text, const uint32_t color)
        {
            for (const auto character : text)
            {
                if (g_menu_font != nullptr)
                {
                    g_menu_font->draw_glyph(surface, x, y, character, color, nullptr);
                    x += g_menu_font->advance(character);
                    continue;
                }

                if (!ft_draw_glyph(surface, x, y, character, color, nullptr))
                {
                    draw_debug_glyph(surface, x, y + (k_char_height - debug_font::glyph_height) / 2, character, color);
                }
                x += k_char_width;
            }
        }

        void draw_check_mark(gdi_bitmap_surface& surface, const int32_t x, const int32_t y)
        {
            constexpr std::array<std::pair<int32_t, int32_t>, 9> points = {
                {{0, 4}, {1, 5}, {2, 6}, {3, 5}, {4, 4}, {5, 3}, {6, 2}, {7, 1}, {8, 0}}};
            for (const auto& [dx, dy] : points)
            {
                fill_rect(surface, {.left = x + dx, .top = y + dy, .width = 1, .height = 2}, k_color_text);
            }
        }

        void draw_submenu_arrow(gdi_bitmap_surface& surface, const int32_t x, const int32_t y, const uint32_t color)
        {
            for (int32_t i = 0; i < 4; ++i)
            {
                fill_rect(surface, {.left = x + i, .top = y + i, .width = 1, .height = 7 - 2 * i}, color);
            }
        }

        void draw_bar(const process_context& process, const window& top_level, const menu& bar, gdi_bitmap_surface& surface)
        {
            const slot strip{.left = 0, .top = 0, .width = static_cast<int32_t>(surface.width), .height = window::menu_bar_height};
            fill_rect(surface, strip, k_color_bar);
            fill_rect(surface, {.left = 0, .top = window::menu_bar_height - 1, .width = strip.width, .height = 1}, k_color_bar_edge);

            const auto slots = layout_bar(bar);
            const auto& interaction = process.open_menu;
            for (size_t i = 0; i < bar.items.size(); ++i)
            {
                const auto& item = bar.items[i];
                const bool open = interaction.window == top_level.handle && interaction.bar_index == static_cast<int32_t>(i);
                if (open)
                {
                    fill_rect(surface, slots[i], k_color_highlight);
                }

                draw_text(surface, slots[i].left + k_bar_item_padding, (window::menu_bar_height - char_height()) / 2,
                          split_label(item).text, is_enabled(item) ? k_color_text : k_color_text_disabled);
            }
        }

        void draw_popup(const popup_layout& layout, const menu& popup, const int32_t highlighted, gdi_bitmap_surface& surface)
        {
            const auto& bounds = layout.bounds;
            fill_rect(surface, bounds, k_color_popup_edge);
            fill_rect(surface, {.left = bounds.left + 1, .top = bounds.top + 1, .width = bounds.width - 2, .height = bounds.height - 2},
                      k_color_popup);

            for (size_t i = 0; i < popup.items.size(); ++i)
            {
                const auto& item = popup.items[i];
                const auto& row = layout.rows[i];

                if (is_separator(item))
                {
                    fill_rect(surface, {.left = row.left + 4, .top = row.top + row.height / 2, .width = row.width - 8, .height = 1},
                              k_color_separator);
                    continue;
                }

                const auto enabled = is_enabled(item);
                if (static_cast<int32_t>(i) == highlighted && enabled)
                {
                    fill_rect(surface, row, k_color_highlight);
                }

                const auto color = enabled ? k_color_text : k_color_text_disabled;
                const auto text_top = row.top + (row.height - char_height()) / 2;
                const auto label = split_label(item);

                if ((item.state & k_mfs_checked) != 0)
                {
                    draw_check_mark(surface, row.left + 6, row.top + (row.height - 7) / 2);
                }

                draw_text(surface, row.left + k_popup_check_width, text_top, label.text, color);
                if (!label.accelerator.empty())
                {
                    draw_text(surface, row.left + row.width - k_popup_arrow_width - text_width(label.accelerator), text_top,
                              label.accelerator, color);
                }
                if (item.submenu != 0)
                {
                    draw_submenu_arrow(surface, row.left + row.width - 12, row.top + (row.height - 7) / 2, color);
                }
            }
        }

        std::vector<popup_layout> layout_popups(const process_context& process, const menu_interaction& interaction,
                                                const int32_t surface_width)
        {
            std::vector<popup_layout> layouts{};
            for (const auto& state : interaction.popups)
            {
                const auto* popup = process.menus.get(state.menu);
                layouts.push_back(popup ? layout_popup(*popup, state.left, state.top, surface_width) : popup_layout{});
            }
            return layouts;
        }

        std::optional<size_t> row_at(const popup_layout& layout, const int32_t x, const int32_t y)
        {
            if (!layout.bounds.contains(x, y))
            {
                return std::nullopt;
            }

            for (size_t i = 0; i < layout.rows.size(); ++i)
            {
                if (layout.rows[i].contains(x, y))
                {
                    return i;
                }
            }

            return std::nullopt;
        }

        void present_with_menu(windows_emulator& win_emu, const window& top_level, const gdi_bitmap_surface& surface)
        {
            const auto* bar = win_emu.process.menus.get(top_level.menu_handle);
            if (!bar || surface.pixels.empty())
            {
                return;
            }

            gdi_bitmap_surface composed{};
            composed.width = surface.width;
            composed.height = surface.height;
            composed.pixels = surface.pixels;

            draw_bar(win_emu.process, top_level, *bar, composed);

            const auto& interaction = win_emu.process.open_menu;
            if (interaction.window == top_level.handle)
            {
                const auto layouts = layout_popups(win_emu.process, interaction, static_cast<int32_t>(surface.width));
                for (size_t level = 0; level < layouts.size(); ++level)
                {
                    if (const auto* popup = win_emu.process.menus.get(interaction.popups[level].menu))
                    {
                        draw_popup(layouts[level], *popup, interaction.popups[level].highlighted, composed);
                    }
                }
            }

            win_emu.ui().present_surface(top_level.handle, ui_surface_desc{.width = static_cast<int>(composed.width),
                                                                           .height = static_cast<int>(composed.height),
                                                                           .stride = static_cast<int>(composed.width * sizeof(uint32_t)),
                                                                           .format = ui_surface_format::bgra8,
                                                                           .pixels = composed.pixels.data()});
        }

        void refresh(windows_emulator& win_emu, const window& top_level)
        {
            const auto surface = win_emu.process.gdi_window_surfaces.find(static_cast<uint32_t>(top_level.handle));
            if (surface != win_emu.process.gdi_window_surfaces.end())
            {
                present_with_menu(win_emu, top_level, surface->second);
            }
        }

        void post_to_window(windows_emulator& win_emu, const window& top_level, const uint32_t message, const uint64_t w_param,
                            const uint64_t l_param)
        {
            if (auto* thread = win_emu.process.find_thread_by_id(top_level.thread_id))
            {
                msg posted{};
                posted.window = top_level.handle;
                posted.message = message;
                posted.wParam = w_param;
                posted.lParam = l_param;
                thread->post_message(win_emu, posted);
            }
        }

        void open_popup(windows_emulator& win_emu, const window& top_level, const hmenu popup_handle, const int32_t left, const int32_t top,
                        const uint64_t index)
        {
            win_emu.process.open_menu.popups.push_back({.menu = popup_handle, .left = left, .top = top, .highlighted = -1});
            post_to_window(win_emu, top_level, k_wm_initmenupopup, popup_handle, index & 0xFFFF);
        }

        void open_bar_item(windows_emulator& win_emu, const window& top_level, const menu& bar, const size_t index)
        {
            auto& interaction = win_emu.process.open_menu;
            interaction.close();

            const auto& item = bar.items[index];
            if (!is_enabled(item))
            {
                return;
            }

            if (item.submenu == 0)
            {
                post_to_window(win_emu, top_level, k_wm_command, item.id & 0xFFFF, 0);
                return;
            }

            interaction.window = top_level.handle;
            interaction.bar_index = static_cast<int32_t>(index);
            open_popup(win_emu, top_level, item.submenu, layout_bar(bar)[index].left, window::menu_bar_height, index);
        }

        bool handle_popup_press(windows_emulator& win_emu, const window& top_level, const std::vector<popup_layout>& layouts,
                                const int32_t x, const int32_t y)
        {
            auto& interaction = win_emu.process.open_menu;
            for (size_t level = layouts.size(); level-- > 0;)
            {
                const auto row = row_at(layouts[level], x, y);
                if (!row)
                {
                    continue;
                }

                const auto* popup = win_emu.process.menus.get(interaction.popups[level].menu);
                if (!popup || *row >= popup->items.size() || !is_enabled(popup->items[*row]))
                {
                    return true;
                }

                const auto item = popup->items[*row];
                interaction.popups.resize(level + 1);
                if (item.submenu != 0)
                {
                    interaction.popups[level].highlighted = static_cast<int32_t>(*row);
                    const auto& bounds = layouts[level].bounds;
                    open_popup(win_emu, top_level, item.submenu, bounds.left + bounds.width - 2, layouts[level].rows[*row].top - 2, *row);
                    return true;
                }

                interaction.close();
                post_to_window(win_emu, top_level, k_wm_command, item.id & 0xFFFF, 0);
                return true;
            }

            return false;
        }

        void update_hover(windows_emulator& win_emu, const std::vector<popup_layout>& layouts, const int32_t x, const int32_t y)
        {
            auto& interaction = win_emu.process.open_menu;
            for (size_t level = layouts.size(); level-- > 0;)
            {
                if (const auto row = row_at(layouts[level], x, y))
                {
                    interaction.popups[level].highlighted = static_cast<int32_t>(*row);
                    return;
                }
            }
        }
    } // namespace

    void present_window_surface(windows_emulator& win_emu, const hwnd window_handle, const gdi_bitmap_surface& surface)
    {
        select_menu_font(win_emu);
        const auto* top_level = win_emu.process.windows.get(window_handle);
        if (top_level && top_level->has_menu_bar())
        {
            present_with_menu(win_emu, *top_level, surface);
            return;
        }

        win_emu.ui().present_surface(window_handle, ui_surface_desc{.width = static_cast<int>(surface.width),
                                                                    .height = static_cast<int>(surface.height),
                                                                    .stride = static_cast<int>(surface.width * sizeof(uint32_t)),
                                                                    .format = ui_surface_format::bgra8,
                                                                    .pixels = surface.pixels.data()});
    }

    bool handle_menu_pointer(windows_emulator& win_emu, const window& top_level, const uint32_t message, const int32_t x, const int32_t y)
    {
        select_menu_font(win_emu);
        if (!top_level.has_menu_bar())
        {
            return false;
        }

        auto& interaction = win_emu.process.open_menu;
        const bool owns_open_menu = interaction.window == top_level.handle && interaction.is_open();
        const bool in_bar = y >= 0 && y < window::menu_bar_height;

        if (message == WM_LBUTTONUP)
        {
            const bool swallow = interaction.swallow_button_up || owns_open_menu;
            interaction.swallow_button_up = false;
            return swallow;
        }

        if (!owns_open_menu && !in_bar)
        {
            return false;
        }

        const auto* bar = win_emu.process.menus.get(top_level.menu_handle);
        if (!bar)
        {
            return false;
        }

        const auto surface_width = static_cast<int32_t>(top_level.width);
        const auto layouts = owns_open_menu ? layout_popups(win_emu.process, interaction, surface_width) : std::vector<popup_layout>{};
        const auto slots = layout_bar(*bar);
        const auto bar_hit = [&]() -> std::optional<size_t> {
            if (!in_bar)
            {
                return std::nullopt;
            }
            for (size_t i = 0; i < slots.size(); ++i)
            {
                if (slots[i].contains(x, y))
                {
                    return i;
                }
            }
            return std::nullopt;
        }();

        if (message == WM_MOUSEMOVE)
        {
            if (owns_open_menu)
            {
                if (bar_hit && static_cast<int32_t>(*bar_hit) != interaction.bar_index)
                {
                    open_bar_item(win_emu, top_level, *bar, *bar_hit);
                }
                else
                {
                    update_hover(win_emu, layouts, x, y);
                }
                refresh(win_emu, top_level);
            }
            return true;
        }

        if (message != WM_LBUTTONDOWN)
        {
            if (owns_open_menu)
            {
                interaction.close();
                refresh(win_emu, top_level);
            }
            return owns_open_menu || in_bar;
        }

        interaction.swallow_button_up = true;
        if (owns_open_menu && handle_popup_press(win_emu, top_level, layouts, x, y))
        {
            refresh(win_emu, top_level);
            return true;
        }

        if (bar_hit)
        {
            const bool same_item = owns_open_menu && static_cast<int32_t>(*bar_hit) == interaction.bar_index;
            if (same_item)
            {
                interaction.close();
            }
            else
            {
                open_bar_item(win_emu, top_level, *bar, *bar_hit);
            }
        }
        else
        {
            interaction.close();
        }

        refresh(win_emu, top_level);
        return true;
    }
}
