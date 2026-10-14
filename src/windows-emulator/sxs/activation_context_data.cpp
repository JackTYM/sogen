#include "../std_include.hpp"
#include "activation_context_data.hpp"

#include <cwctype>

namespace sogen::sxs
{
    namespace
    {
        constexpr uint32_t activation_context_data_magic = 0x78746341; // 'Actx'
        constexpr uint32_t string_section_magic = 0x64487353;          // 'SsHd'
        constexpr uint32_t hash_algorithm_x65599 = 1;
        constexpr uint32_t string_section_case_insensitive = 0x1;
        constexpr uint32_t toc_entries_in_order = 0x2;
        constexpr uint32_t roster_entry_invalid = 0x1;
        constexpr uint32_t roster_entry_root = 0x2;
        constexpr uint32_t path_type_win32_file = 2;
        constexpr uint32_t path_type_none = 1;
        constexpr uint32_t section_format_string_table = 1;
        // ACTIVATION_CONTEXT_DATA_DLL_REDIRECTION flag that makes ntdll's sxsisol_SearchActCtxForDllName prefix
        // the path with the assembly's storage root (its WinSxS directory, or the application directory).
        constexpr uint32_t dll_redirection_relative_to_assembly_root = 0x2;

        constexpr uint32_t section_assembly_information = 1;
        constexpr uint32_t section_dll_redirection = 2;
        constexpr uint32_t section_window_class_redirection = 3;
        constexpr uint32_t section_application_settings = 10;

        class byte_writer
        {
          public:
            size_t size() const
            {
                return this->bytes_.size();
            }

            void align(const size_t alignment = 4)
            {
                this->bytes_.resize((this->bytes_.size() + alignment - 1) / alignment * alignment);
            }

            size_t append(const void* data, const size_t length)
            {
                const auto offset = this->bytes_.size();
                const auto* source = static_cast<const std::byte*>(data);
                this->bytes_.insert(this->bytes_.end(), source, source + length);
                return offset;
            }

            size_t append_u32(const uint32_t value)
            {
                return this->append(&value, sizeof(value));
            }

            size_t append_u64(const uint64_t value)
            {
                return this->append(&value, sizeof(value));
            }

            size_t append_string(const std::u16string_view text)
            {
                const auto offset = this->append(text.data(), text.size() * sizeof(char16_t));
                const char16_t terminator = 0;
                this->append(&terminator, sizeof(terminator));
                return offset;
            }

            size_t reserve(const size_t length)
            {
                const auto offset = this->bytes_.size();
                this->bytes_.resize(offset + length);
                return offset;
            }

            void patch_u32(const size_t offset, const uint32_t value)
            {
                std::memcpy(this->bytes_.data() + offset, &value, sizeof(value));
            }

            std::vector<std::byte>& bytes()
            {
                return this->bytes_;
            }

            const std::vector<std::byte>& bytes() const
            {
                return this->bytes_;
            }

          private:
            std::vector<std::byte> bytes_{};
        };

        uint32_t byte_length(const std::u16string_view text)
        {
            return static_cast<uint32_t>(text.size() * sizeof(char16_t));
        }

        // A data blob whose 32-bit fields at `fixups` must receive the section-relative offset of a
        // pooled string, known only once the section layout is final.
        struct section_blob
        {
            byte_writer data{};
            std::vector<std::pair<size_t, size_t>> fixups{};
        };

        class string_section_builder
        {
          public:
            size_t add_pooled_string(std::u16string text)
            {
                this->pool_.push_back(std::move(text));
                return this->pool_.size() - 1;
            }

            void set_user_data(section_blob blob)
            {
                this->user_data_ = std::move(blob);
            }

            bool contains(const std::u16string_view key) const
            {
                const auto hash = hash_section_key(key);
                return std::ranges::any_of(this->entries_, [&](const entry& existing) {
                    return existing.pseudo_key == hash && existing.key.size() == key.size() &&
                           std::equal(existing.key.begin(), existing.key.end(), key.begin(),
                                      [](const char16_t a, const char16_t b) { return std::towupper(a) == std::towupper(b); });
                });
            }

            void add_entry(std::u16string key, section_blob data, const uint32_t roster_index)
            {
                const auto pseudo_key = hash_section_key(key);
                this->entries_.push_back({std::move(key), pseudo_key, std::move(data), roster_index});
            }

            size_t entry_count() const
            {
                return this->entries_.size();
            }

            struct layout
            {
                std::vector<std::byte> bytes{};
                std::vector<uint32_t> key_offsets{};
                std::vector<uint32_t> data_offsets{};
            };

            layout finish()
            {
                constexpr uint32_t header_size = 0x2C;
                constexpr uint32_t entry_size = 24;

                byte_writer out{};
                out.reserve(header_size);

                uint32_t user_data_offset = 0;
                if (this->user_data_)
                {
                    user_data_offset =
                        static_cast<uint32_t>(out.append(this->user_data_->data.bytes().data(), this->user_data_->data.size()));
                    out.align();
                }

                const auto element_list_offset = static_cast<uint32_t>(out.reserve(entry_size * this->entries_.size()));

                layout result{};
                for (const auto& element : this->entries_)
                {
                    result.key_offsets.push_back(static_cast<uint32_t>(out.append_string(element.key)));
                    out.align();
                }

                for (const auto& element : this->entries_)
                {
                    result.data_offsets.push_back(
                        static_cast<uint32_t>(out.append(element.data.data.bytes().data(), element.data.data.size())));
                    out.align();
                }

                std::vector<uint32_t> pool_offsets{};
                for (const auto& text : this->pool_)
                {
                    pool_offsets.push_back(static_cast<uint32_t>(out.append_string(text)));
                    out.align();
                }

                const auto apply_fixups = [&](const section_blob& blob, const uint32_t blob_offset) {
                    for (const auto& [position, pool_index] : blob.fixups)
                    {
                        out.patch_u32(blob_offset + position, pool_offsets.at(pool_index));
                    }
                };

                if (this->user_data_)
                {
                    apply_fixups(*this->user_data_, user_data_offset);
                }

                for (size_t i = 0; i < this->entries_.size(); ++i)
                {
                    const auto& element = this->entries_[i];
                    const auto entry_offset = element_list_offset + i * entry_size;
                    apply_fixups(element.data, result.data_offsets[i]);

                    out.patch_u32(entry_offset + 0, element.pseudo_key);
                    out.patch_u32(entry_offset + 4, result.key_offsets[i]);
                    out.patch_u32(entry_offset + 8, byte_length(element.key));
                    out.patch_u32(entry_offset + 12, result.data_offsets[i]);
                    out.patch_u32(entry_offset + 16, static_cast<uint32_t>(element.data.data.size()));
                    out.patch_u32(entry_offset + 20, element.roster_index);
                }

                out.patch_u32(0, string_section_magic);
                out.patch_u32(4, header_size);
                out.patch_u32(8, 1);
                out.patch_u32(12, 1);
                out.patch_u32(16, string_section_case_insensitive);
                out.patch_u32(20, static_cast<uint32_t>(this->entries_.size()));
                out.patch_u32(24, this->entries_.empty() ? 0 : element_list_offset);
                out.patch_u32(28, hash_algorithm_x65599);
                out.patch_u32(32, 0);
                out.patch_u32(36, user_data_offset);
                out.patch_u32(40, this->user_data_ ? static_cast<uint32_t>(this->user_data_->data.size()) : 0);

                result.bytes = std::move(out.bytes());
                return result;
            }

          private:
            struct entry
            {
                std::u16string key{};
                uint32_t pseudo_key{};
                section_blob data{};
                uint32_t roster_index{};
            };

            std::vector<entry> entries_{};
            std::vector<std::u16string> pool_{};
            std::optional<section_blob> user_data_{};
        };

        void append_pooled_string_field(section_blob& blob, string_section_builder& section, const std::u16string& text)
        {
            blob.data.append_u32(byte_length(text));
            const auto position = blob.data.append_u32(0);
            if (!text.empty())
            {
                blob.fixups.emplace_back(position, section.add_pooled_string(text));
            }
        }

        section_blob build_assembly_information(const resolved_assembly& assembly, string_section_builder& section)
        {
            constexpr uint32_t structure_size = 108;

            section_blob blob{};
            blob.data.append_u32(structure_size);
            blob.data.append_u32(assembly.flags);
            append_pooled_string_field(blob, section, assembly.identity.encode());
            blob.data.append_u32(path_type_win32_file);
            append_pooled_string_field(blob, section, assembly.manifest_path);
            blob.data.append_u64(assembly.manifest_last_write_time);
            blob.data.append_u32(path_type_none);
            blob.data.append_u32(0);
            blob.data.append_u32(0);
            blob.data.append_u64(0);
            blob.data.append_u32(0);
            blob.data.append_u32(0);
            blob.data.append_u32(assembly.content.manifest_version_major);
            blob.data.append_u32(assembly.content.manifest_version_minor);
            blob.data.append_u32(0);
            blob.data.append_u32(0);
            append_pooled_string_field(blob, section, assembly.directory_name);
            blob.data.append_u32(static_cast<uint32_t>(assembly.content.files.size()));
            append_pooled_string_field(blob, section, assembly.identity.language);
            blob.data.append_u32(static_cast<uint32_t>(assembly.content.requested_run_level));
            blob.data.append_u32(assembly.content.ui_access ? 1 : 0);
            return blob;
        }

        section_blob build_assembly_global_information(const activation_context_input& input, string_section_builder& section)
        {
            constexpr uint32_t structure_size = 56;

            section_blob blob{};
            blob.data.append_u32(structure_size);
            blob.data.append_u32(0);
            blob.data.reserve(32);
            blob.data.append_u32(path_type_win32_file);
            append_pooled_string_field(blob, section, input.application_directory);
            blob.data.append_u32(input.resource_name);
            return blob;
        }

        section_blob build_dll_redirection()
        {
            constexpr uint32_t structure_size = 20;

            section_blob blob{};
            blob.data.append_u32(structure_size);
            blob.data.append_u32(dll_redirection_relative_to_assembly_root);
            blob.data.append_u32(0);
            blob.data.append_u32(0);
            blob.data.append_u32(0);
            return blob;
        }

        section_blob build_window_class_redirection(const std::u16string& versioned_name, const std::u16string& dll_name,
                                                    string_section_builder& section)
        {
            constexpr uint32_t structure_size = 24;

            section_blob blob{};
            blob.data.append_u32(structure_size);
            blob.data.append_u32(0);
            blob.data.append_u32(byte_length(versioned_name));
            blob.data.append_u32(structure_size);
            append_pooled_string_field(blob, section, dll_name);
            blob.data.append_string(versioned_name);
            return blob;
        }

        section_blob build_application_setting(const application_setting& setting)
        {
            constexpr uint32_t structure_size = 32;

            section_blob blob{};
            const auto namespace_offset = structure_size;
            const auto name_offset = namespace_offset + byte_length(setting.namespace_uri) + 2;
            const auto value_offset = name_offset + byte_length(setting.name) + 2;

            blob.data.append_u32(structure_size);
            blob.data.append_u32(0);
            blob.data.append_u32(byte_length(setting.namespace_uri));
            blob.data.append_u32(namespace_offset);
            blob.data.append_u32(byte_length(setting.name));
            blob.data.append_u32(name_offset);
            blob.data.append_u32(byte_length(setting.value));
            blob.data.append_u32(value_offset);
            blob.data.append_string(setting.namespace_uri);
            blob.data.append_string(setting.name);
            blob.data.append_string(setting.value);
            return blob;
        }
    }

    uint32_t hash_section_key(const std::u16string_view key)
    {
        uint32_t hash = 0;
        for (auto c : key)
        {
            if (c >= u'a' && c <= u'z')
            {
                c = static_cast<char16_t>(c - 32);
            }
            else if (c >= 0xC0)
            {
                c = static_cast<char16_t>(std::towupper(c));
            }

            hash = hash * 65599 + c;
        }

        return hash;
    }

    std::vector<std::byte> build_activation_context_data(const activation_context_input& input)
    {
        string_section_builder assembly_information{};
        string_section_builder dll_redirection{};
        string_section_builder window_classes{};
        string_section_builder application_settings{};

        assembly_information.set_user_data(build_assembly_global_information(input, assembly_information));

        for (size_t i = 0; i < input.assemblies.size(); ++i)
        {
            const auto& assembly = input.assemblies[i];
            const auto roster_index = static_cast<uint32_t>(i + 1);

            assembly_information.add_entry(assembly.identity.encode(), build_assembly_information(assembly, assembly_information),
                                           roster_index);

            for (const auto& file : assembly.content.files)
            {
                if (!dll_redirection.contains(file.name))
                {
                    dll_redirection.add_entry(file.name, build_dll_redirection(), roster_index);
                }

                for (const auto& window_class : file.window_classes)
                {
                    if (window_classes.contains(window_class.name))
                    {
                        continue;
                    }

                    const auto versioned_name =
                        window_class.versioned ? assembly.identity.version + u"!" + window_class.name : window_class.name;
                    window_classes.add_entry(window_class.name, build_window_class_redirection(versioned_name, file.name, window_classes),
                                             roster_index);
                }
            }
        }

        if (!input.assemblies.empty())
        {
            for (const auto& setting : input.assemblies.front().content.settings)
            {
                if (!application_settings.contains(setting.name))
                {
                    application_settings.add_entry(setting.name, build_application_setting(setting), 1);
                }
            }
        }

        struct section
        {
            uint32_t id{};
            string_section_builder::layout layout{};
        };

        std::vector<section> sections{};
        sections.push_back({section_assembly_information, assembly_information.finish()});
        if (dll_redirection.entry_count() != 0)
        {
            sections.push_back({section_dll_redirection, dll_redirection.finish()});
        }
        if (window_classes.entry_count() != 0)
        {
            sections.push_back({section_window_class_redirection, window_classes.finish()});
        }
        if (application_settings.entry_count() != 0)
        {
            sections.push_back({section_application_settings, application_settings.finish()});
        }

        constexpr uint32_t header_size = 0x20;
        constexpr uint32_t toc_header_size = 16;
        constexpr uint32_t toc_entry_size = 16;
        constexpr uint32_t roster_header_size = 20;
        constexpr uint32_t roster_entry_size = 24;

        byte_writer out{};
        out.reserve(header_size);

        const auto toc_offset = static_cast<uint32_t>(out.reserve(toc_header_size));
        const auto toc_entries_offset = static_cast<uint32_t>(out.reserve(toc_entry_size * sections.size()));

        const auto roster_count = static_cast<uint32_t>(input.assemblies.size() + 1);
        const auto roster_offset = static_cast<uint32_t>(out.reserve(roster_header_size));
        const auto roster_entries_offset = static_cast<uint32_t>(out.reserve(roster_entry_size * roster_count));

        std::vector<uint32_t> section_offsets{};
        for (const auto& entry : sections)
        {
            out.align(8);
            section_offsets.push_back(static_cast<uint32_t>(out.append(entry.layout.bytes.data(), entry.layout.bytes.size())));
        }
        out.align();

        for (size_t i = 0; i < sections.size(); ++i)
        {
            const auto entry_offset = toc_entries_offset + i * toc_entry_size;
            out.patch_u32(entry_offset + 0, sections[i].id);
            out.patch_u32(entry_offset + 4, section_offsets[i]);
            out.patch_u32(entry_offset + 8, static_cast<uint32_t>(sections[i].layout.bytes.size()));
            out.patch_u32(entry_offset + 12, section_format_string_table);
        }

        out.patch_u32(toc_offset + 0, toc_header_size);
        out.patch_u32(toc_offset + 4, static_cast<uint32_t>(sections.size()));
        out.patch_u32(toc_offset + 8, toc_entries_offset);
        out.patch_u32(toc_offset + 12, toc_entries_in_order);

        const auto& information_layout = sections.front().layout;
        const auto information_offset = section_offsets.front();

        out.patch_u32(roster_offset + 0, roster_header_size);
        out.patch_u32(roster_offset + 4, hash_algorithm_x65599);
        out.patch_u32(roster_offset + 8, roster_count);
        out.patch_u32(roster_offset + 12, roster_entries_offset);
        out.patch_u32(roster_offset + 16, information_offset);

        out.patch_u32(roster_entries_offset + 0, roster_entry_invalid);
        for (size_t i = 0; i < input.assemblies.size(); ++i)
        {
            const auto entry_offset = roster_entries_offset + (i + 1) * roster_entry_size;
            const auto encoded_identity = input.assemblies[i].identity.encode();

            out.patch_u32(entry_offset + 0, i == 0 ? roster_entry_root : 0);
            out.patch_u32(entry_offset + 4, hash_section_key(encoded_identity));
            out.patch_u32(entry_offset + 8, information_offset + information_layout.key_offsets[i]);
            out.patch_u32(entry_offset + 12, byte_length(encoded_identity));
            out.patch_u32(entry_offset + 16, information_offset + information_layout.data_offsets[i]);
            out.patch_u32(entry_offset + 20, 108);
        }

        out.patch_u32(0, activation_context_data_magic);
        out.patch_u32(4, header_size);
        out.patch_u32(8, 1);
        out.patch_u32(12, static_cast<uint32_t>(out.size()));
        out.patch_u32(16, toc_offset);
        out.patch_u32(20, 0);
        out.patch_u32(24, roster_offset);
        out.patch_u32(28, 0);

        return std::move(out.bytes());
    }
}
