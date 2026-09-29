#include "std_include.hpp"
#include "object_name.hpp"

#include <cwctype>

namespace sogen
{
    namespace
    {
        constexpr std::u16string_view global_directory = u"\\basenamedobjects\\";
        constexpr std::u16string_view session_prefix = u"\\sessions\\";
        constexpr std::u16string_view session_directory_suffix = u"\\basenamedobjects\\";
        constexpr std::u16string_view own_session = u"1";

        std::u16string lowercase(const std::u16string_view name)
        {
            std::u16string result(name);
            for (auto& character : result)
            {
                character = static_cast<char16_t>(std::towlower(static_cast<wint_t>(character)));
            }
            return result;
        }

        bool starts_with(const std::u16string_view text, const std::u16string_view prefix)
        {
            return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
        }

        std::u16string session_directory(const std::u16string_view session)
        {
            std::u16string result(session_prefix);
            result += session;
            result += session_directory_suffix;
            return result;
        }
    }

    std::u16string canonical_object_name(const std::u16string_view name)
    {
        const auto lower = lowercase(name);
        const std::u16string_view view = lower;

        if (view.empty() || view.front() == u'\\')
        {
            return lower;
        }

        if (starts_with(view, u"global\\"))
        {
            return std::u16string(global_directory) + std::u16string(view.substr(7));
        }

        if (starts_with(view, u"local\\"))
        {
            return session_directory(own_session) + std::u16string(view.substr(6));
        }

        if (starts_with(view, u"session\\"))
        {
            const auto rest = view.substr(8);
            const auto separator = rest.find(u'\\');
            if (separator != std::u16string_view::npos && separator > 0)
            {
                return session_directory(rest.substr(0, separator)) + std::u16string(rest.substr(separator + 1));
            }
        }

        return session_directory(own_session) + lower;
    }

    bool object_names_equal(const std::u16string_view left, const std::u16string_view right)
    {
        return canonical_object_name(left) == canonical_object_name(right);
    }
}
