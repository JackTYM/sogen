#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace sogen
{
    // Character mapping of the US (00000409) keyboard layout, the only layout the emulator translates.
    namespace keyboard_layout
    {
        constexpr uint16_t vk_back = 0x08;
        constexpr uint16_t vk_tab = 0x09;
        constexpr uint16_t vk_return = 0x0D;
        constexpr uint16_t vk_shift = 0x10;
        constexpr uint16_t vk_control = 0x11;
        constexpr uint16_t vk_menu = 0x12;
        constexpr uint16_t vk_capital = 0x14;
        constexpr uint16_t vk_escape = 0x1B;
        constexpr uint16_t vk_space = 0x20;
        constexpr uint16_t vk_numpad0 = 0x60;
        constexpr uint16_t vk_multiply = 0x6A;
        constexpr uint16_t vk_add = 0x6B;
        constexpr uint16_t vk_subtract = 0x6D;
        constexpr uint16_t vk_decimal = 0x6E;
        constexpr uint16_t vk_divide = 0x6F;
        constexpr uint16_t vk_lshift = 0xA0;
        constexpr uint16_t vk_rshift = 0xA1;
        constexpr uint16_t vk_lcontrol = 0xA2;
        constexpr uint16_t vk_rcontrol = 0xA3;
        constexpr uint16_t vk_packet = 0xE7;

        struct oem_key
        {
            uint16_t vk;
            char16_t normal;
            char16_t shifted;
        };

        constexpr std::array<oem_key, 11> oem_keys{{
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

        constexpr std::array<char16_t, 10> shifted_digits{u')', u'!', u'@', u'#', u'$', u'%', u'^', u'&', u'*', u'('};

        struct modifiers
        {
            bool shift{};
            bool control{};
            bool caps_lock{};
        };

        inline modifiers modifiers_from_key_state(const std::array<uint8_t, 256>& key_state)
        {
            const auto down = [&](const uint16_t vk) { return (key_state[vk] & 0x80) != 0; };
            return {
                .shift = down(vk_shift) || down(vk_lshift) || down(vk_rshift),
                .control = down(vk_control) || down(vk_lcontrol) || down(vk_rcontrol),
                .caps_lock = (key_state[vk_capital] & 0x01) != 0,
            };
        }

        inline std::optional<char16_t> character_for_virtual_key(const uint16_t vk, const modifiers& mods)
        {
            if (vk >= 'A' && vk <= 'Z')
            {
                if (mods.control)
                {
                    return static_cast<char16_t>(vk - 'A' + 1);
                }

                const bool upper = mods.shift != mods.caps_lock;
                return static_cast<char16_t>(upper ? vk : vk - 'A' + 'a');
            }

            if (mods.control)
            {
                switch (vk)
                {
                case vk_back:
                    return u'\x7F';
                case vk_return:
                    return u'\n';
                case vk_escape:
                    return u'\x1B';
                default:
                    return std::nullopt;
                }
            }

            if (vk >= '0' && vk <= '9')
            {
                return mods.shift ? shifted_digits[vk - '0'] : static_cast<char16_t>(vk);
            }

            if (vk >= vk_numpad0 && vk <= vk_numpad0 + 9)
            {
                return static_cast<char16_t>(u'0' + (vk - vk_numpad0));
            }

            switch (vk)
            {
            case vk_back:
                return u'\b';
            case vk_tab:
                return u'\t';
            case vk_return:
                return u'\r';
            case vk_escape:
                return u'\x1B';
            case vk_space:
                return u' ';
            case vk_multiply:
                return u'*';
            case vk_add:
                return u'+';
            case vk_subtract:
                return u'-';
            case vk_decimal:
                return u'.';
            case vk_divide:
                return u'/';
            default:
                break;
            }

            for (const auto& key : oem_keys)
            {
                if (key.vk == vk)
                {
                    return mods.shift ? key.shifted : key.normal;
                }
            }

            return std::nullopt;
        }

        struct key_stroke
        {
            uint16_t vk{};
            bool shift{};
        };

        inline std::optional<key_stroke> key_stroke_for_character(const char16_t character)
        {
            if (character >= u'a' && character <= u'z')
            {
                return key_stroke{.vk = static_cast<uint16_t>(character - u'a' + 'A')};
            }

            if (character >= u'A' && character <= u'Z')
            {
                return key_stroke{.vk = static_cast<uint16_t>(character), .shift = true};
            }

            if (character >= u'0' && character <= u'9')
            {
                return key_stroke{.vk = static_cast<uint16_t>(character)};
            }

            for (uint16_t digit = 0; digit < shifted_digits.size(); ++digit)
            {
                if (shifted_digits[digit] == character)
                {
                    return key_stroke{.vk = static_cast<uint16_t>('0' + digit), .shift = true};
                }
            }

            switch (character)
            {
            case u' ':
                return key_stroke{.vk = vk_space};
            case u'\t':
                return key_stroke{.vk = vk_tab};
            case u'\r':
            case u'\n':
                return key_stroke{.vk = vk_return};
            default:
                break;
            }

            for (const auto& key : oem_keys)
            {
                if (key.normal == character)
                {
                    return key_stroke{.vk = key.vk};
                }

                if (key.shifted == character)
                {
                    return key_stroke{.vk = key.vk, .shift = true};
                }
            }

            return std::nullopt;
        }

        // PS/2 set-1 make code of a virtual key, 0 when unknown.
        inline uint16_t scan_code_for_virtual_key(const uint16_t vk)
        {
            constexpr std::array<uint8_t, 26> letter_scan_codes{0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                                                0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                                                0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};

            if (vk >= 'A' && vk <= 'Z')
            {
                return letter_scan_codes[vk - 'A'];
            }

            if (vk >= '1' && vk <= '9')
            {
                return static_cast<uint16_t>(0x02 + (vk - '1'));
            }

            switch (vk)
            {
            case '0':
                return 0x0B;
            case vk_escape:
                return 0x01;
            case vk_back:
                return 0x0E;
            case vk_tab:
                return 0x0F;
            case vk_return:
                return 0x1C;
            case vk_space:
                return 0x39;
            case vk_shift:
            case vk_lshift:
                return 0x2A;
            case vk_rshift:
                return 0x36;
            case vk_control:
            case vk_lcontrol:
                return 0x1D;
            case 0xBA:
                return 0x27;
            case 0xBB:
                return 0x0D;
            case 0xBC:
                return 0x33;
            case 0xBD:
                return 0x0C;
            case 0xBE:
                return 0x34;
            case 0xBF:
                return 0x35;
            case 0xC0:
                return 0x29;
            case 0xDB:
                return 0x1A;
            case 0xDC:
                return 0x2B;
            case 0xDD:
                return 0x1B;
            case 0xDE:
                return 0x28;
            default:
                return 0;
            }
        }
    }
}
