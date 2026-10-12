#pragma once

#include "std_include.hpp"
#include "file_system.hpp"
#include "process_context.hpp"

namespace sogen
{
    struct font_request
    {
        std::u16string face{};
        int32_t height{};
        int32_t weight{400};
        bool italic{};
    };

    struct font_metrics
    {
        int32_t height{};
        int32_t ascent{};
        int32_t descent{};
        int32_t internal_leading{};
        int32_t external_leading{};
        int32_t average_width{};
        int32_t max_width{};
        int32_t weight{400};
        bool italic{};
        uint8_t pitch_and_family{};
    };

    // A TrueType face rasterised at one pixel size. Metrics and advances follow GDI's integer, hinted rounding so
    // guest layout code (dialog units, DrawText, controls) sees the sizes it would see on Windows.
    class font_instance
    {
      public:
        font_instance(const std::filesystem::path& file, int32_t pixel_size, const font_request& request, uint8_t family_flags);
        ~font_instance() = default;

        font_instance(const font_instance&) = delete;
        font_instance& operator=(const font_instance&) = delete;

        bool valid() const;
        const font_metrics& metrics() const;
        const std::u16string& face_name() const;
        int32_t advance(char32_t codepoint) const;
        int32_t text_width(std::u16string_view text) const;

        // pen_x is the glyph origin, top_y the top of the character cell.
        void draw_glyph(gdi_bitmap_surface& surface, int32_t pen_x, int32_t top_y, char32_t codepoint, uint32_t color, const RECT* clip);

      private:
        struct impl;
        static void destroy(impl* instance);
        std::unique_ptr<impl, void (*)(impl*)> impl_;
    };

    class font_manager
    {
      public:
        explicit font_manager(const file_system& fs);

        // Directory holding the bundled LiberationSans/Mono/Serif faces that stand in for Windows fonts the emulation
        // root does not provide (or only provides as placeholders).
        void set_fallback_directory(std::filesystem::path directory);

        // Returns nullptr when the emulation root has no usable font files; callers then keep their fixed-cell fallback.
        std::shared_ptr<font_instance> resolve(const font_request& request);

      private:
        struct resolved_file
        {
            std::filesystem::path path{};
            uint8_t family_flags{};
        };

        resolved_file find_font_file(const font_request& request);
        bool is_genuine_font_file(const std::filesystem::path& file, std::u16string_view family);
        int32_t pixel_size_for(const resolved_file& file, const font_request& request);
        std::shared_ptr<font_instance> create_instance(const font_request& request);

        const file_system& fs_;
        std::filesystem::path fallback_directory_{};
        std::map<std::pair<std::filesystem::path, std::u16string>, bool> genuine_files_{};
        std::map<std::tuple<std::filesystem::path, int32_t>, std::shared_ptr<font_instance>> instances_{};
        std::map<std::filesystem::path, double> height_per_pixel_{};
        std::map<std::tuple<std::u16string, int32_t, int32_t, bool>, std::shared_ptr<font_instance>> resolved_{};
    };
}
