#include "std_include.hpp"
#include "keyboard_layout.hpp"

namespace sogen
{
    namespace
    {
        struct oem_key
        {
            uint8_t virtual_key;
            char16_t normal;
            char16_t shifted;
        };

        constexpr std::array<oem_key, 11> k_oem_keys{{
            {0xBA, u';', u':'},
            {0xBB, u'=', u'+'},
            {0xBC, u',', u'<'},
            {0xBD, u'-', u'_'},
            {0xBE, u'.', u'>'},
            {0xBF, u'/', u'?'},
            {0xC0, u'`', u'~'},
            {0xDB, u'[', u'{'},
            {0xDC, u'\\', u'|'},
            {0xDD, u']', u'}'},
            {0xDE, u'\'', u'"'},
        }};

        constexpr std::array<char16_t, 10> k_shifted_digits{u')', u'!', u'@', u'#', u'$', u'%', u'^', u'&', u'*', u'('};

        constexpr uint8_t vk_back = 0x08;
        constexpr uint8_t vk_tab = 0x09;
        constexpr uint8_t vk_return = 0x0D;
        constexpr uint8_t vk_escape = 0x1B;
        constexpr uint8_t vk_space = 0x20;
        constexpr uint8_t vk_numpad0 = 0x60;
        constexpr uint8_t vk_numpad9 = 0x69;
        constexpr uint8_t vk_multiply = 0x6A;
        constexpr uint8_t vk_divide = 0x6F;
        constexpr uint8_t vk_separator = 0x6C;
        constexpr std::array<char16_t, 6> k_numpad_operators{u'*', u'+', u'\0', u'-', u'.', u'/'};

        std::optional<char16_t> control_character(const char16_t base)
        {
            if (base >= u'a' && base <= u'z')
            {
                return static_cast<char16_t>(base - u'a' + 1);
            }

            switch (base)
            {
            case u'[':
                return 0x1B;
            case u'\\':
                return 0x1C;
            case u']':
                return 0x1D;
            case u' ':
                return u' ';
            default:
                return std::nullopt;
            }
        }
    }

    std::optional<char16_t> translate_virtual_key(const uint8_t virtual_key, const key_modifiers& modifiers)
    {
        char16_t character{};

        if (virtual_key >= 'A' && virtual_key <= 'Z')
        {
            const bool upper = modifiers.shift != modifiers.caps_lock;
            character = static_cast<char16_t>((upper ? u'A' : u'a') + (virtual_key - 'A'));
        }
        else if (virtual_key >= '0' && virtual_key <= '9')
        {
            character = modifiers.shift ? k_shifted_digits[virtual_key - '0'] : static_cast<char16_t>(virtual_key);
        }
        else if (virtual_key >= vk_numpad0 && virtual_key <= vk_numpad9)
        {
            character = static_cast<char16_t>(u'0' + (virtual_key - vk_numpad0));
        }
        else if (virtual_key >= vk_multiply && virtual_key <= vk_divide)
        {
            if (virtual_key == vk_separator)
            {
                return std::nullopt;
            }
            character = k_numpad_operators.at(virtual_key - vk_multiply);
        }
        else if (virtual_key == vk_space)
        {
            character = u' ';
        }
        else if (virtual_key == vk_back)
        {
            return 0x08;
        }
        else if (virtual_key == vk_tab)
        {
            return 0x09;
        }
        else if (virtual_key == vk_return)
        {
            return modifiers.control ? 0x0A : 0x0D;
        }
        else if (virtual_key == vk_escape)
        {
            return 0x1B;
        }
        else
        {
            const auto entry = std::ranges::find(k_oem_keys, virtual_key, &oem_key::virtual_key);
            if (entry == k_oem_keys.end())
            {
                return std::nullopt;
            }
            character = modifiers.shift ? entry->shifted : entry->normal;
        }

        if (modifiers.control)
        {
            return control_character(character >= u'A' && character <= u'Z' ? static_cast<char16_t>(character + 32) : character);
        }

        return character;
    }
}
