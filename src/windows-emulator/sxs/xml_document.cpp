#include "../std_include.hpp"
#include "xml_document.hpp"

namespace sogen::sxs
{
    namespace
    {
        constexpr std::u16string_view xmlns_attribute = u"xmlns";

        std::u16string decode_document(const std::span<const std::byte> data)
        {
            const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
            const auto size = data.size();

            if (size >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
            {
                std::u16string result((size - 2) / 2, u'\0');
                std::memcpy(result.data(), bytes + 2, result.size() * sizeof(char16_t));
                return result;
            }

            if (size >= 2 && bytes[0] == '<' && bytes[1] == 0)
            {
                std::u16string result(size / 2, u'\0');
                std::memcpy(result.data(), bytes, result.size() * sizeof(char16_t));
                return result;
            }

            std::string_view text(reinterpret_cast<const char*>(bytes), size);
            if (text.starts_with("\xEF\xBB\xBF"))
            {
                text.remove_prefix(3);
            }

            return u8_to_u16(text);
        }

        bool is_space(const char16_t c)
        {
            return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
        }

        bool is_name_char(const char16_t c)
        {
            return !is_space(c) && c != u'=' && c != u'>' && c != u'/' && c != u'<' && c != u'"' && c != u'\'' && c != u'?';
        }

        std::u16string_view local_part(const std::u16string_view name)
        {
            const auto separator = name.find(u':');
            return separator == std::u16string_view::npos ? name : name.substr(separator + 1);
        }

        std::u16string_view prefix_part(const std::u16string_view name)
        {
            const auto separator = name.find(u':');
            return separator == std::u16string_view::npos ? std::u16string_view{} : name.substr(0, separator);
        }

        void append_code_point(std::u16string& out, const uint32_t code_point)
        {
            if (code_point < 0x10000)
            {
                out.push_back(static_cast<char16_t>(code_point));
                return;
            }

            const auto value = code_point - 0x10000;
            out.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
            out.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
        }

        std::u16string decode_entities(const std::u16string_view text)
        {
            std::u16string result{};
            result.reserve(text.size());

            for (size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] != u'&')
                {
                    result.push_back(text[i]);
                    continue;
                }

                const auto end = text.find(u';', i);
                if (end == std::u16string_view::npos)
                {
                    result.push_back(text[i]);
                    continue;
                }

                const auto entity = text.substr(i + 1, end - i - 1);
                if (entity == u"lt")
                {
                    result.push_back(u'<');
                }
                else if (entity == u"gt")
                {
                    result.push_back(u'>');
                }
                else if (entity == u"amp")
                {
                    result.push_back(u'&');
                }
                else if (entity == u"quot")
                {
                    result.push_back(u'"');
                }
                else if (entity == u"apos")
                {
                    result.push_back(u'\'');
                }
                else if (entity.starts_with(u"#"))
                {
                    const bool hex = entity.size() > 1 && (entity[1] == u'x' || entity[1] == u'X');
                    uint32_t code_point = 0;
                    for (const auto c : entity.substr(hex ? 2 : 1))
                    {
                        uint32_t digit = 0;
                        if (c >= u'0' && c <= u'9')
                        {
                            digit = c - u'0';
                        }
                        else if (hex && c >= u'a' && c <= u'f')
                        {
                            digit = c - u'a' + 10;
                        }
                        else if (hex && c >= u'A' && c <= u'F')
                        {
                            digit = c - u'A' + 10;
                        }
                        code_point = code_point * (hex ? 16 : 10) + digit;
                    }
                    append_code_point(result, code_point);
                }
                else
                {
                    result.append(text.substr(i, end - i + 1));
                }

                i = end;
            }

            return result;
        }

        using namespace_scope = std::vector<std::pair<std::u16string, std::u16string>>;

        class parser
        {
          public:
            explicit parser(std::u16string text)
                : text_(std::move(text))
            {
            }

            std::optional<xml_element> parse_document()
            {
                this->skip_misc();
                if (!this->at(u'<'))
                {
                    return std::nullopt;
                }

                namespace_scope scope{};
                auto root = this->parse_element(scope);
                if (!root)
                {
                    return std::nullopt;
                }

                return root;
            }

          private:
            std::u16string text_{};
            size_t pos_{};

            bool at(const char16_t c) const
            {
                return this->pos_ < this->text_.size() && this->text_[this->pos_] == c;
            }

            bool starts_with(const std::u16string_view prefix) const
            {
                return std::u16string_view(this->text_).substr(this->pos_).starts_with(prefix);
            }

            void skip_spaces()
            {
                while (this->pos_ < this->text_.size() && is_space(this->text_[this->pos_]))
                {
                    ++this->pos_;
                }
            }

            bool skip_past(const std::u16string_view terminator)
            {
                const auto end = this->text_.find(terminator, this->pos_);
                if (end == std::u16string::npos)
                {
                    this->pos_ = this->text_.size();
                    return false;
                }

                this->pos_ = end + terminator.size();
                return true;
            }

            void skip_misc()
            {
                while (true)
                {
                    this->skip_spaces();
                    if (this->starts_with(u"<?"))
                    {
                        this->skip_past(u"?>");
                    }
                    else if (this->starts_with(u"<!--"))
                    {
                        this->skip_past(u"-->");
                    }
                    else if (this->starts_with(u"<!"))
                    {
                        this->skip_past(u">");
                    }
                    else
                    {
                        return;
                    }
                }
            }

            std::u16string read_name()
            {
                const auto start = this->pos_;
                while (this->pos_ < this->text_.size() && is_name_char(this->text_[this->pos_]))
                {
                    ++this->pos_;
                }

                return this->text_.substr(start, this->pos_ - start);
            }

            static std::u16string resolve(const namespace_scope& scope, const std::u16string_view prefix)
            {
                for (auto it = scope.rbegin(); it != scope.rend(); ++it)
                {
                    if (it->first == prefix)
                    {
                        return it->second;
                    }
                }

                return {};
            }

            std::optional<xml_element> parse_element(namespace_scope& scope)
            {
                ++this->pos_;
                const auto qualified_name = this->read_name();
                if (qualified_name.empty())
                {
                    return std::nullopt;
                }

                std::vector<std::pair<std::u16string, std::u16string>> raw_attributes{};
                bool self_closing = false;

                while (true)
                {
                    this->skip_spaces();
                    if (this->pos_ >= this->text_.size())
                    {
                        return std::nullopt;
                    }

                    if (this->starts_with(u"/>"))
                    {
                        this->pos_ += 2;
                        self_closing = true;
                        break;
                    }

                    if (this->at(u'>'))
                    {
                        ++this->pos_;
                        break;
                    }

                    auto attribute_name = this->read_name();
                    this->skip_spaces();
                    if (attribute_name.empty() || !this->at(u'='))
                    {
                        return std::nullopt;
                    }

                    ++this->pos_;
                    this->skip_spaces();
                    if (!this->at(u'"') && !this->at(u'\''))
                    {
                        return std::nullopt;
                    }

                    const auto quote = this->text_[this->pos_++];
                    const auto end = this->text_.find(quote, this->pos_);
                    if (end == std::u16string::npos)
                    {
                        return std::nullopt;
                    }

                    raw_attributes.emplace_back(std::move(attribute_name),
                                                decode_entities(std::u16string_view(this->text_).substr(this->pos_, end - this->pos_)));
                    this->pos_ = end + 1;
                }

                const auto scope_size = scope.size();
                xml_element element{};

                for (auto& [attribute_name, value] : raw_attributes)
                {
                    if (attribute_name == xmlns_attribute)
                    {
                        scope.emplace_back(std::u16string{}, value);
                    }
                    else if (prefix_part(attribute_name) == xmlns_attribute)
                    {
                        scope.emplace_back(std::u16string(local_part(attribute_name)), value);
                    }
                }

                for (auto& [attribute_name, value] : raw_attributes)
                {
                    if (attribute_name != xmlns_attribute && prefix_part(attribute_name) != xmlns_attribute)
                    {
                        element.attributes.emplace_back(std::u16string(local_part(attribute_name)), std::move(value));
                    }
                }

                element.name = local_part(qualified_name);
                element.namespace_uri = resolve(scope, prefix_part(qualified_name));

                if (!self_closing && !this->parse_content(element, qualified_name, scope))
                {
                    return std::nullopt;
                }

                scope.resize(scope_size);
                return element;
            }

            bool parse_content(xml_element& element, const std::u16string_view qualified_name, namespace_scope& scope)
            {
                while (this->pos_ < this->text_.size())
                {
                    if (this->starts_with(u"</"))
                    {
                        this->pos_ += 2;
                        const auto closing = this->read_name();
                        this->skip_spaces();
                        if (closing != qualified_name || !this->at(u'>'))
                        {
                            return false;
                        }

                        ++this->pos_;
                        return true;
                    }

                    if (this->starts_with(u"<!--"))
                    {
                        this->skip_past(u"-->");
                        continue;
                    }

                    if (this->starts_with(u"<![CDATA["))
                    {
                        this->pos_ += 9;
                        const auto end = this->text_.find(u"]]>", this->pos_);
                        if (end == std::u16string::npos)
                        {
                            return false;
                        }

                        element.text.append(this->text_, this->pos_, end - this->pos_);
                        this->pos_ = end + 3;
                        continue;
                    }

                    if (this->starts_with(u"<?"))
                    {
                        this->skip_past(u"?>");
                        continue;
                    }

                    if (this->at(u'<'))
                    {
                        auto child = this->parse_element(scope);
                        if (!child)
                        {
                            return false;
                        }

                        element.children.push_back(std::move(*child));
                        continue;
                    }

                    const auto end = this->text_.find(u'<', this->pos_);
                    const auto length = (end == std::u16string::npos ? this->text_.size() : end) - this->pos_;
                    element.text += decode_entities(std::u16string_view(this->text_).substr(this->pos_, length));
                    this->pos_ += length;
                }

                return false;
            }
        };
    }

    const std::u16string* xml_element::attribute(const std::u16string_view attribute_name) const
    {
        for (const auto& [key, value] : this->attributes)
        {
            if (key == attribute_name)
            {
                return &value;
            }
        }

        return nullptr;
    }

    const xml_element* xml_element::child(const std::u16string_view child_name) const
    {
        for (const auto& entry : this->children)
        {
            if (entry.name == child_name)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    std::vector<const xml_element*> xml_element::children_named(const std::u16string_view child_name) const
    {
        std::vector<const xml_element*> result{};
        for (const auto& entry : this->children)
        {
            if (entry.name == child_name)
            {
                result.push_back(&entry);
            }
        }

        return result;
    }

    std::optional<xml_element> parse_xml(const std::span<const std::byte> data)
    {
        return parser(decode_document(data)).parse_document();
    }
}
