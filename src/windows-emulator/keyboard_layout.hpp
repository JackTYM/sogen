#pragma once

#include "std_include.hpp"

namespace sogen
{
    struct key_modifiers
    {
        bool shift{};
        bool caps_lock{};
        bool control{};
    };

    // Translates a virtual key to the UTF-16 character the US keyboard layout produces for it, mirroring what
    // TranslateMessage derives from a WM_KEYDOWN. Returns nullopt for keys without a character.
    std::optional<char16_t> translate_virtual_key(uint8_t virtual_key, const key_modifiers& modifiers);
}
