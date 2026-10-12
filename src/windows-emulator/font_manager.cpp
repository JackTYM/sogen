#include "std_include.hpp"
#include "font_manager.hpp"
#include <numeric>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H

namespace sogen
{
    namespace
    {
        constexpr int32_t k_default_pixel_size = 12;
        constexpr int32_t k_bold_weight = 600;

        struct font_family
        {
            std::u16string_view name;
            std::array<std::string_view, 4> files; // regular, bold, italic, bold italic
            uint8_t pitch_and_family;
        };

        constexpr uint8_t k_tmpf_vector_truetype = 0x06;
        constexpr uint8_t k_ff_swiss = 0x20;
        constexpr uint8_t k_ff_roman = 0x10;
        constexpr uint8_t k_ff_modern = 0x30;

        constexpr std::array<font_family, 12> k_families{{
            {u"segoe ui", {"segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"tahoma", {"tahoma.ttf", "tahomabd.ttf", "tahoma.ttf", "tahomabd.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"arial", {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"microsoft sans serif", {"micross.ttf", "micross.ttf", "micross.ttf", "micross.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"verdana", {"verdana.ttf", "verdanab.ttf", "verdanai.ttf", "verdanaz.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"courier new", {"cour.ttf", "courbd.ttf", "couri.ttf", "courbi.ttf"}, k_tmpf_vector_truetype | k_ff_modern},
            {u"consolas", {"consola.ttf", "consolab.ttf", "consolai.ttf", "consolaz.ttf"}, k_tmpf_vector_truetype | k_ff_modern},
            {u"lucida console", {"lucon.ttf", "lucon.ttf", "lucon.ttf", "lucon.ttf"}, k_tmpf_vector_truetype | k_ff_modern},
            {u"times new roman", {"times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"}, k_tmpf_vector_truetype | k_ff_roman},
            {u"georgia", {"georgia.ttf", "georgiab.ttf", "georgiai.ttf", "georgiaz.ttf"}, k_tmpf_vector_truetype | k_ff_roman},
            {u"calibri", {"calibri.ttf", "calibrib.ttf", "calibrii.ttf", "calibriz.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
            {u"trebuchet ms", {"trebuc.ttf", "trebucbd.ttf", "trebucit.ttf", "trebucbi.ttf"}, k_tmpf_vector_truetype | k_ff_swiss},
        }};

        // GDI's built-in substitutions: the dialog fonts and legacy bitmap font names resolve to installed TrueType faces.
        std::u16string substitute_family(std::u16string name)
        {
            std::ranges::transform(name, name.begin(),
                                   [](const char16_t ch) { return ch >= u'A' && ch <= u'Z' ? static_cast<char16_t>(ch + 32) : ch; });

            if (name == u"ms shell dlg" || name == u"ms sans serif" || name == u"ms shell dlg 1")
            {
                return u"microsoft sans serif";
            }
            if (name == u"ms shell dlg 2" || name == u"system" || name == u"default" || name.empty())
            {
                return u"tahoma";
            }
            if (name == u"fixedsys" || name == u"terminal" || name == u"ms gothic")
            {
                return u"courier new";
            }
            return name;
        }

        struct glyph_bitmap
        {
            int32_t advance{};
            int32_t bearing_x{};
            int32_t bearing_y{};
            int32_t width{};
            int32_t height{};
            std::vector<uint8_t> alpha{};
        };

        uint32_t blend_pixel(const uint32_t background, const uint32_t foreground, const uint8_t alpha)
        {
            const uint32_t a = alpha;
            const uint32_t inverse = 255u - a;
            const uint32_t rb = ((foreground & 0xFF00FFu) * a + (background & 0xFF00FFu) * inverse) >> 8u;
            const uint32_t g = ((foreground & 0x00FF00u) * a + (background & 0x00FF00u) * inverse) >> 8u;
            return 0xFF000000u | (rb & 0xFF00FFu) | (g & 0x00FF00u);
        }

        int32_t scale_round(const int64_t value, const int32_t pixel_size, const int32_t units_per_em)
        {
            const auto scaled = value * pixel_size;
            const auto half = static_cast<int64_t>(units_per_em) / 2;
            return static_cast<int32_t>(scaled >= 0 ? (scaled + half) / units_per_em : -((-scaled + half) / units_per_em));
        }
    }

    struct font_instance::impl
    {
        FT_Library library{};
        FT_Face face{};
        font_metrics metrics{};
        std::u16string face_name{};
        mutable std::unordered_map<char32_t, glyph_bitmap> glyphs{};

        ~impl()
        {
            if (face != nullptr)
            {
                FT_Done_Face(face);
            }
            if (library != nullptr)
            {
                FT_Done_FreeType(library);
            }
        }

        const glyph_bitmap& glyph(const char32_t codepoint) const
        {
            const auto cached = glyphs.find(codepoint);
            if (cached != glyphs.end())
            {
                return cached->second;
            }

            glyph_bitmap result{};
            const FT_UInt index = FT_Get_Char_Index(face, static_cast<FT_ULong>(codepoint));
            if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT | FT_LOAD_RENDER) == 0)
            {
                const FT_GlyphSlot slot = face->glyph;
                const FT_Bitmap& bitmap = slot->bitmap;
                result.advance = static_cast<int32_t>((slot->advance.x + 32) >> 6);
                result.bearing_x = slot->bitmap_left;
                result.bearing_y = slot->bitmap_top;
                result.width = static_cast<int32_t>(bitmap.width);
                result.height = static_cast<int32_t>(bitmap.rows);
                result.alpha.resize(static_cast<size_t>(result.width) * static_cast<size_t>(result.height));
                for (int32_t row = 0; row < result.height; ++row)
                {
                    std::copy_n(bitmap.buffer + static_cast<ptrdiff_t>(row) * bitmap.pitch, result.width,
                                result.alpha.begin() + static_cast<ptrdiff_t>(row) * result.width);
                }
            }

            return glyphs.emplace(codepoint, std::move(result)).first->second;
        }
    };

    font_instance::font_instance(const std::filesystem::path& file, const int32_t pixel_size, const font_request& request,
                                 const uint8_t family_flags)
        : impl_(new impl(), &destroy)
    {
        if (FT_Init_FreeType(&impl_->library) != 0 || FT_New_Face(impl_->library, file.string().c_str(), 0, &impl_->face) != 0)
        {
            impl_->face = nullptr;
            return;
        }

        FT_Set_Pixel_Sizes(impl_->face, 0, static_cast<FT_UInt>(pixel_size));

        const auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(impl_->face, FT_SFNT_OS2));
        const auto* hhea = static_cast<TT_HoriHeader*>(FT_Get_Sfnt_Table(impl_->face, FT_SFNT_HHEA));
        const int32_t units = impl_->face->units_per_EM != 0 ? impl_->face->units_per_EM : 2048;

        auto& metrics = impl_->metrics;
        const int64_t win_ascent = os2 != nullptr && os2->usWinAscent != 0 ? os2->usWinAscent : impl_->face->ascender;
        const int64_t win_descent = os2 != nullptr && os2->usWinDescent != 0 ? os2->usWinDescent : -impl_->face->descender;
        metrics.ascent = scale_round(win_ascent, pixel_size, units);
        metrics.descent = scale_round(win_descent, pixel_size, units);
        metrics.height = metrics.ascent + metrics.descent;
        metrics.internal_leading = std::max(0, metrics.height - pixel_size);
        const int64_t line_gap = hhea != nullptr ? hhea->Line_Gap : 0;
        metrics.external_leading = std::max<int32_t>(0, scale_round(line_gap, pixel_size, units));
        const int64_t average = os2 != nullptr && os2->xAvgCharWidth != 0 ? os2->xAvgCharWidth : units / 2;
        metrics.average_width = std::max(1, scale_round(average, pixel_size, units));
        metrics.max_width = std::max(metrics.average_width, scale_round(impl_->face->max_advance_width, pixel_size, units));
        metrics.weight = request.weight;
        metrics.italic = request.italic;
        metrics.pitch_and_family = static_cast<uint8_t>(family_flags | (FT_IS_FIXED_WIDTH(impl_->face) ? 0x00 : 0x01));
        impl_->face_name = request.face;
    }

    void font_instance::destroy(impl* instance)
    {
        delete instance;
    }

    bool font_instance::valid() const
    {
        return this->impl_->face != nullptr;
    }

    const font_metrics& font_instance::metrics() const
    {
        return this->impl_->metrics;
    }

    const std::u16string& font_instance::face_name() const
    {
        return this->impl_->face_name;
    }

    int32_t font_instance::advance(const char32_t codepoint) const
    {
        return this->impl_->glyph(codepoint).advance;
    }

    int32_t font_instance::text_width(const std::u16string_view text) const
    {
        return std::accumulate(text.begin(), text.end(), int32_t{0},
                               [this](const int32_t width, const char16_t character) { return width + this->advance(character); });
    }

    void font_instance::draw_glyph(gdi_bitmap_surface& surface, const int32_t pen_x, const int32_t top_y, const char32_t codepoint,
                                   const uint32_t color, const RECT* clip)
    {
        const auto& glyph = this->impl_->glyph(codepoint);
        const int32_t origin_x = pen_x + glyph.bearing_x;
        const int32_t origin_y = top_y + this->impl_->metrics.ascent - glyph.bearing_y;

        for (int32_t row = 0; row < glyph.height; ++row)
        {
            const int32_t y = origin_y + row;
            for (int32_t col = 0; col < glyph.width; ++col)
            {
                const auto alpha = glyph.alpha[static_cast<size_t>(row) * glyph.width + col];
                const int32_t x = origin_x + col;
                if (alpha == 0 || !surface.is_writable(x, y))
                {
                    continue;
                }
                if (clip != nullptr && (x < clip->left || x >= clip->right || y < clip->top || y >= clip->bottom))
                {
                    continue;
                }

                auto& pixel = surface.pixels[static_cast<size_t>(y) * surface.width + static_cast<size_t>(x)];
                pixel = alpha == 255u ? (0xFF000000u | color) : blend_pixel(pixel, color, alpha);
            }
        }
    }

    font_manager::font_manager(const file_system& fs)
        : fs_(fs)
    {
    }

    void font_manager::set_fallback_directory(std::filesystem::path directory)
    {
        this->fallback_directory_ = std::move(directory);
        this->resolved_.clear();
    }

    // Emulation roots may carry one stand-in font under every Windows font file name, so a file only counts when the
    // family recorded inside it is the family its name promises.
    bool font_manager::is_genuine_font_file(const std::filesystem::path& file, const std::u16string_view family)
    {
        const auto key = std::make_pair(file, std::u16string(family));
        if (const auto known = this->genuine_files_.find(key); known != this->genuine_files_.end())
        {
            return known->second;
        }

        bool genuine = false;
        FT_Library library{};
        FT_Face face{};
        if (FT_Init_FreeType(&library) == 0)
        {
            if (FT_New_Face(library, file.string().c_str(), 0, &face) == 0)
            {
                std::string family_name = face->family_name != nullptr ? face->family_name : "";
                std::ranges::transform(family_name, family_name.begin(),
                                       [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                genuine = std::ranges::equal(family_name, family);
                FT_Done_Face(face);
            }
            FT_Done_FreeType(library);
        }

        this->genuine_files_.emplace(key, genuine);
        return genuine;
    }

    font_manager::resolved_file font_manager::find_font_file(const font_request& request)
    {
        const auto family_name = substitute_family(request.face);
        const bool bold = request.weight >= k_bold_weight;
        const size_t style = (bold ? 1u : 0u) | (request.italic ? 2u : 0u);

        const auto preferred = std::ranges::find(k_families, family_name, &font_family::name);
        uint8_t family_flags = k_tmpf_vector_truetype | k_ff_swiss;
        if (preferred != k_families.end())
        {
            family_flags = preferred->pitch_and_family;
            for (const auto candidate : {preferred->files[style], preferred->files[bold ? 1 : 0], preferred->files[0]})
            {
                std::u16string guest_path = u"C:\\Windows\\Fonts\\";
                guest_path.append(candidate.begin(), candidate.end());
                auto path = this->fs_.translate(windows_path(std::filesystem::path(guest_path)));
                if (std::filesystem::exists(path) && this->is_genuine_font_file(path, preferred->name))
                {
                    return {.path = std::move(path), .family_flags = family_flags};
                }
            }
        }

        if (this->fallback_directory_.empty())
        {
            return {};
        }

        const char* family = "LiberationSans";
        if ((family_flags & 0xF0) == k_ff_modern)
        {
            family = "LiberationMono";
        }
        else if ((family_flags & 0xF0) == k_ff_roman)
        {
            family = "LiberationSerif";
        }

        constexpr std::array<const char*, 4> styles{"Regular", "Bold", "Italic", "BoldItalic"};
        auto path = this->fallback_directory_ / (std::string(family) + "-" + styles[style] + ".ttf");
        if (!std::filesystem::exists(path))
        {
            return {};
        }

        return {.path = std::move(path), .family_flags = family_flags};
    }

    // A positive lfHeight is the cell height (ascent + descent); recover the em size that produces it.
    int32_t font_manager::pixel_size_for(const resolved_file& file, const font_request& request)
    {
        if (request.height < 0)
        {
            return -request.height;
        }
        if (request.height == 0)
        {
            return k_default_pixel_size;
        }

        auto ratio = this->height_per_pixel_.find(file.path);
        if (ratio == this->height_per_pixel_.end())
        {
            constexpr int32_t probe_size = 256;
            const font_instance probe(file.path, probe_size, request, file.family_flags);
            ratio =
                this->height_per_pixel_.emplace(file.path, probe.valid() ? static_cast<double>(probe.metrics().height) / probe_size : 1.0)
                    .first;
        }

        return std::max(1, static_cast<int32_t>(std::lround(request.height / ratio->second)));
    }

    std::shared_ptr<font_instance> font_manager::resolve(const font_request& request)
    {
        auto lowered_face = request.face;
        std::ranges::transform(lowered_face, lowered_face.begin(),
                               [](const char16_t ch) { return ch >= u'A' && ch <= u'Z' ? static_cast<char16_t>(ch + 32) : ch; });
        const auto request_key =
            std::make_tuple(std::move(lowered_face), request.height, request.weight >= k_bold_weight ? 1 : 0, request.italic);
        if (const auto known = this->resolved_.find(request_key); known != this->resolved_.end())
        {
            return known->second;
        }

        auto instance = this->create_instance(request);
        this->resolved_.emplace(request_key, instance);
        return instance;
    }

    std::shared_ptr<font_instance> font_manager::create_instance(const font_request& request)
    {
        const auto file = this->find_font_file(request);
        if (file.path.empty())
        {
            return nullptr;
        }

        const auto pixel_size = this->pixel_size_for(file, request);
        const auto key = std::make_tuple(file.path, pixel_size);
        const auto cached = this->instances_.find(key);
        if (cached != this->instances_.end())
        {
            return cached->second;
        }

        auto instance = std::make_shared<font_instance>(file.path, pixel_size, request, file.family_flags);
        if (!instance->valid())
        {
            return nullptr;
        }

        this->instances_.emplace(key, instance);
        return instance;
    }
}
