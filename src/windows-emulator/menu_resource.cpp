#include "std_include.hpp"
#include "menu_resource.hpp"

namespace sogen
{
    namespace
    {
        constexpr uint16_t mf_grayed = 0x0001;
        constexpr uint16_t mf_disabled = 0x0002;
        constexpr uint16_t mf_checked = 0x0008;
        constexpr uint16_t mf_popup = 0x0010;
        constexpr uint16_t mf_end = 0x0080;
        constexpr uint32_t mft_separator = 0x0800;
        constexpr uint32_t mfs_grayed = 0x0003;
        constexpr uint32_t mfs_checked = 0x0008;
        constexpr uint32_t mfr_popup = 0x0001;
        constexpr uint32_t mfr_end = 0x0080;

        class template_reader
        {
          public:
            explicit template_reader(const std::span<const uint8_t> data)
                : data_(data)
            {
            }

            bool ok() const
            {
                return !this->failed_;
            }

            template <typename T>
            T read()
            {
                T value{};
                if (this->offset_ + sizeof(T) > this->data_.size())
                {
                    this->failed_ = true;
                    return value;
                }

                memcpy(&value, this->data_.data() + this->offset_, sizeof(T));
                this->offset_ += sizeof(T);
                return value;
            }

            std::u16string read_string()
            {
                std::u16string text{};
                while (this->ok())
                {
                    const auto ch = this->read<char16_t>();
                    if (ch == 0)
                    {
                        break;
                    }

                    text.push_back(ch);
                }

                return text;
            }

            void align(const size_t alignment)
            {
                this->offset_ = (this->offset_ + alignment - 1) & ~(alignment - 1);
            }

            void skip(const size_t count)
            {
                this->offset_ += count;
            }

          private:
            std::span<const uint8_t> data_;
            size_t offset_{};
            bool failed_{};
        };

        hmenu build_menu(process_context& process, memory_manager& memory, std::vector<menu_item> items, const bool popup)
        {
            auto [handle, menu_obj] = process.menus.create(memory);
            menu_obj.handle = handle.bits;
            menu_obj.popup = popup;
            menu_obj.items = std::move(items);
            menu_obj.sync_guest_items(memory);
            menu_obj.init_guest();
            return handle.bits;
        }

        void attach_submenu(process_context& process, menu_item& item, const hmenu submenu)
        {
            item.submenu = submenu;
            item.submenu_ptr = process.menus.get(submenu)->guest.value();
        }

        std::vector<menu_item> read_items(template_reader& reader, process_context& process, memory_manager& memory)
        {
            std::vector<menu_item> items{};

            while (reader.ok())
            {
                const auto flags = reader.read<uint16_t>();
                menu_item item{};

                if ((flags & mf_popup) == 0)
                {
                    item.id = reader.read<uint16_t>();
                }

                item.text = reader.read_string();
                item.state = ((flags & (mf_grayed | mf_disabled)) != 0 ? mfs_grayed : 0) | ((flags & mf_checked) != 0 ? mfs_checked : 0);

                if ((flags & mf_popup) != 0)
                {
                    attach_submenu(process, item, build_menu(process, memory, read_items(reader, process, memory), true));
                }
                else if (item.id == 0 && item.text.empty())
                {
                    item.type = mft_separator;
                }

                items.push_back(std::move(item));

                if ((flags & mf_end) != 0)
                {
                    break;
                }
            }

            return items;
        }

        std::vector<menu_item> read_items_ex(template_reader& reader, process_context& process, memory_manager& memory)
        {
            std::vector<menu_item> items{};

            while (reader.ok())
            {
                reader.align(4);
                menu_item item{};
                item.type = reader.read<uint32_t>();
                item.state = reader.read<uint32_t>();
                item.id = reader.read<uint32_t>();
                const auto info = reader.read<uint16_t>();
                item.text = reader.read_string();

                if ((info & mfr_popup) != 0)
                {
                    reader.align(4);
                    reader.skip(sizeof(uint32_t));
                    attach_submenu(process, item, build_menu(process, memory, read_items_ex(reader, process, memory), true));
                }

                items.push_back(std::move(item));

                if ((info & mfr_end) != 0)
                {
                    break;
                }
            }

            return items;
        }
    }

    hmenu create_menu_from_template(process_context& process, memory_manager& memory, const std::span<const uint8_t> menu_template)
    {
        template_reader reader{menu_template};
        const auto version = reader.read<uint16_t>();
        const auto header_size = reader.read<uint16_t>();
        reader.skip(header_size);

        auto items = version == 0 ? read_items(reader, process, memory) : read_items_ex(reader, process, memory);
        if (!reader.ok())
        {
            return 0;
        }

        return build_menu(process, memory, std::move(items), false);
    }
}
