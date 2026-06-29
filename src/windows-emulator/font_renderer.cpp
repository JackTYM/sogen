#include "font_renderer.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

namespace sogen
{
    namespace
    {
        // k_baseline_offset matches k_default_font_ascent in gdi.cpp (12px above cell top).
        constexpr int k_baseline_offset = 12;
        constexpr int k_pixel_height = 12;

        struct cached_glyph
        {
            int bearing_x{};
            int bearing_y{};
            int bitmap_width{};
            int bitmap_height{};
            std::vector<uint8_t> alpha{};
        };

        uint32_t blend_pixel(const uint32_t bg, const uint32_t fg, const uint8_t alpha)
        {
            const uint32_t a = alpha;
            const uint32_t ia = 255u - a;
            const uint32_t rb = ((fg & 0xFF00FFu) * a + (bg & 0xFF00FFu) * ia) >> 8u;
            const uint32_t g = ((fg & 0x00FF00u) * a + (bg & 0x00FF00u) * ia) >> 8u;
            return 0xFF000000u | (rb & 0xFF00FFu) | (g & 0x00FF00u);
        }

        class ft_font_renderer
        {
        public:
            ft_font_renderer()
            {
                if (FT_Init_FreeType(&library_) != 0)
                {
                    return;
                }

                static constexpr std::array<const char*, 8> k_font_paths = {
                    "/usr/share/fonts/TTF/DejaVuSans.ttf",
                    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
                    "/usr/share/fonts/gnu-free/FreeSans.otf",
                    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
                    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
                    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
                };

                for (const auto* path : k_font_paths)
                {
                    if (FT_New_Face(library_, path, 0, &face_) == 0)
                    {
                        FT_Set_Pixel_Sizes(face_, 0, static_cast<FT_UInt>(k_pixel_height));
                        loaded_ = true;
                        return;
                    }
                }

                FT_Done_FreeType(library_);
                library_ = nullptr;
            }

            ~ft_font_renderer()
            {
                if (loaded_)
                {
                    FT_Done_Face(face_);
                }
                if (library_ != nullptr)
                {
                    FT_Done_FreeType(library_);
                }
            }

            ft_font_renderer(const ft_font_renderer&) = delete;
            ft_font_renderer& operator=(const ft_font_renderer&) = delete;

            bool loaded() const { return loaded_; }

            const cached_glyph* get_glyph(const char32_t codepoint)
            {
                const auto it = cache_.find(codepoint);
                if (it != cache_.end())
                {
                    return it->second.has_value() ? &*it->second : nullptr;
                }

                const FT_UInt index = FT_Get_Char_Index(face_, static_cast<FT_ULong>(codepoint));
                if (index == 0 || FT_Load_Glyph(face_, index, FT_LOAD_RENDER) != 0)
                {
                    cache_.emplace(codepoint, std::nullopt);
                    return nullptr;
                }

                const FT_GlyphSlot slot = face_->glyph;
                const FT_Bitmap& bm = slot->bitmap;
                const int pitch = std::abs(bm.pitch);

                cached_glyph g{
                    .bearing_x = slot->bitmap_left,
                    .bearing_y = slot->bitmap_top,
                    .bitmap_width = static_cast<int>(bm.width),
                    .bitmap_height = static_cast<int>(bm.rows),
                    .alpha = std::vector<uint8_t>(bm.rows * bm.width),
                };

                for (int row = 0; row < g.bitmap_height; ++row)
                {
                    const uint8_t* src = bm.buffer + row * pitch;
                    uint8_t* dst = g.alpha.data() + row * g.bitmap_width;
                    std::copy(src, src + g.bitmap_width, dst);
                }

                auto [ins, _] = cache_.emplace(codepoint, std::move(g));
                return &*ins->second;
            }

            void draw(gdi_bitmap_surface& surface, const int x, const int y, const char32_t codepoint,
                      const uint32_t color, const RECT* clip)
            {
                const auto* g = get_glyph(codepoint);
                if (g == nullptr)
                {
                    return;
                }

                const int baseline_y = y + k_baseline_offset;
                const int gx = x + g->bearing_x;
                const int gy = baseline_y - g->bearing_y;

                for (int row = 0; row < g->bitmap_height; ++row)
                {
                    const int py = gy + row;
                    if (py < 0 || py >= static_cast<int>(surface.height))
                    {
                        continue;
                    }

                    for (int col = 0; col < g->bitmap_width; ++col)
                    {
                        const uint8_t alpha = g->alpha[static_cast<size_t>(row) * g->bitmap_width + col];
                        if (alpha == 0)
                        {
                            continue;
                        }

                        const int px = gx + col;
                        if (px < 0 || px >= static_cast<int>(surface.width))
                        {
                            continue;
                        }

                        if (clip != nullptr &&
                            (px < clip->left || px >= clip->right || py < clip->top || py >= clip->bottom))
                        {
                            continue;
                        }

                        auto& pixel = surface.pixels[static_cast<size_t>(py) * surface.width + px];
                        pixel = alpha == 255u ? color : blend_pixel(pixel, color, alpha);
                    }
                }
            }

        private:
            FT_Library library_{};
            FT_Face face_{};
            bool loaded_{false};
            std::unordered_map<char32_t, std::optional<cached_glyph>> cache_{};
        };

        ft_font_renderer& get_renderer()
        {
            static ft_font_renderer renderer;
            return renderer;
        }
    }

    bool ft_draw_glyph(gdi_bitmap_surface& surface, const int x, const int y, const char32_t codepoint,
                       const uint32_t color, const RECT* clip)
    {
        auto& r = get_renderer();
        if (!r.loaded())
        {
            return false;
        }
        r.draw(surface, x, y, codepoint, color, clip);
        return true;
    }
}
