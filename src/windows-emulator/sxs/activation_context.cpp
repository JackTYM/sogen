#include "../std_include.hpp"
#include "activation_context.hpp"

#include <utils/io.hpp>

#include <charconv>

#include <sys/stat.h>

namespace sogen::sxs
{
    namespace
    {
        constexpr uint32_t activation_context_magic = 0x78746341;
        constexpr uint32_t string_section_magic = 0x64487353;
        constexpr uint32_t guid_section_magic = 0x64487347;
        constexpr uint32_t hash_string_algorithm_x65599 = 1;
        constexpr uint32_t activation_context_section_assembly_information = 1;
        constexpr uint32_t activation_context_section_dll_redirection = 2;
        constexpr uint32_t activation_context_section_window_class_redirection = 3;
        constexpr uint32_t activation_context_section_com_server_redirection = 4;
        constexpr uint32_t activation_context_section_com_interface_redirection = 5;
        constexpr uint32_t activation_context_section_com_type_library_redirection = 6;
        constexpr uint32_t activation_context_section_com_progid_redirection = 7;
        constexpr uint32_t activation_context_section_clr_surrogates = 9;
        constexpr uint32_t activation_context_section_compatibility_information = 11;
        constexpr uint32_t activation_context_section_format_string_table = 1;
        constexpr uint32_t activation_context_section_format_guid_table = 2;
        constexpr uint32_t string_section_case_insensitive = 1;
        constexpr uint32_t string_section_entries_in_pseudokey_order = 2;
        constexpr uint32_t guid_section_entries_in_order = 1;
        constexpr uint32_t roster_entry_invalid = 1;
        constexpr uint32_t roster_entry_root = 2;
        constexpr uint32_t assembly_information_root = 1;
        constexpr uint32_t assembly_information_private = 0x10;
        constexpr uint32_t dll_redirection_path_omits_assembly_root = 2;
        constexpr uint32_t dll_redirection_system_default_redirected_system32_dll = 8;
        constexpr uint32_t activation_context_path_type_none = 1;
        constexpr uint32_t activation_context_path_type_win32_file = 2;
        constexpr uint32_t com_threading_model_both = 4;
        constexpr uint32_t rt_manifest = 24;
        constexpr std::array system_default_window_class_names{
            u"ToolbarWindow32",
            u"ComboBoxEx32",
            u"msctls_trackbar32",
            u"msctls_updown32",
            u"msctls_progress32",
            u"msctls_hotkey32",
            u"msctls_statusbar32",
            u"SysHeader32",
            u"SysListView32",
            u"SysTreeView32",
            u"SysTabControl32",
            u"SysIPAddress32",
            u"SysPager",
            u"NativeFontCtl",
            u"Button",
            u"Static",
            u"Listbox",
            u"ScrollBar",
            u"tooltips_class32",
            u"ButtonListBox",
            u"SysAnimate32",
            u"SysMonthCal32",
            u"SysDateTimePick32",
            u"ReBarWindow32",
            u"Edit",
            u"Combobox",
            u"ComboLBox",
        };

        struct activation_context_data
        {
            uint32_t magic;
            uint32_t header_size;
            uint32_t format_version;
            uint32_t total_size;
            uint32_t default_toc_offset;
            uint32_t extended_toc_offset;
            uint32_t assembly_roster_offset;
            uint32_t flags;
        };

        struct activation_context_toc_header
        {
            uint32_t header_size;
            uint32_t entry_count;
            uint32_t first_entry_offset;
            uint32_t flags;
        };

        struct activation_context_toc_entry
        {
            uint32_t id;
            uint32_t offset;
            uint32_t length;
            uint32_t format;
        };

        struct assembly_roster_header
        {
            uint32_t header_size;
            uint32_t hash_algorithm;
            uint32_t entry_count;
            uint32_t first_entry_offset;
            uint32_t assembly_information_section_offset;
        };

        struct assembly_roster_entry
        {
            uint32_t flags;
            uint32_t pseudokey;
            uint32_t assembly_name_offset;
            uint32_t assembly_name_length;
            uint32_t assembly_information_offset;
            uint32_t assembly_information_length;
        };

        struct string_section_header
        {
            uint32_t magic;
            uint32_t header_size;
            uint32_t format_version;
            uint32_t data_format_version;
            uint32_t flags;
            uint32_t element_count;
            uint32_t element_list_offset;
            uint32_t hash_algorithm;
            uint32_t search_structure_offset;
            uint32_t user_data_offset;
            uint32_t user_data_size;
        };

        struct string_section_entry
        {
            uint32_t pseudokey;
            uint32_t key_offset;
            uint32_t key_length;
            uint32_t offset;
            uint32_t length;
            uint32_t assembly_roster_index;
        };

        struct string_section_hash_table
        {
            uint32_t bucket_table_entry_count;
            uint32_t bucket_table_offset;
        };

        struct string_section_hash_bucket
        {
            uint32_t chain_count;
            uint32_t chain_offset;
        };

        using guid = std::array<std::byte, 16>;

        struct guid_section_header
        {
            uint32_t magic;
            uint32_t header_size;
            uint32_t format_version;
            uint32_t data_format_version;
            uint32_t flags;
            uint32_t element_count;
            uint32_t element_list_offset;
            uint32_t search_structure_offset;
            uint32_t user_data_offset;
            uint32_t user_data_size;
        };

        struct guid_section_entry
        {
            guid key;
            uint32_t offset;
            uint32_t length;
            uint32_t assembly_roster_index;
        };

        struct com_server_redirection
        {
            uint32_t size;
            uint32_t flags;
            uint32_t threading_model;
            guid reference_clsid;
            guid configured_clsid;
            guid implemented_clsid;
            guid type_library_id;
            uint32_t module_length;
            uint32_t module_offset;
            uint32_t progid_length;
            uint32_t progid_offset;
            uint32_t shim_data_length;
            uint32_t shim_data_offset;
            uint32_t misc_status_default;
            uint32_t misc_status_content;
            uint32_t misc_status_thumbnail;
            uint32_t misc_status_icon;
            uint32_t misc_status_doc_print;
        };

        struct com_interface_redirection
        {
            uint32_t size;
            uint32_t mask;
            guid iid;
            uint32_t method_count;
            guid type_library_id;
            guid base_interface;
            uint32_t name_length;
            uint32_t name_offset;
        };

        struct com_type_library_redirection
        {
            uint32_t size;
            uint32_t flags;
            uint32_t name_length;
            uint32_t name_offset;
            uint16_t resource_id;
            uint16_t library_flags;
            uint32_t help_directory_length;
            uint32_t help_directory_offset;
            uint16_t major_version;
            uint16_t minor_version;
        };

        struct com_progid_redirection
        {
            uint32_t size;
            uint32_t flags;
            uint32_t clsid_offset;
        };

        struct assembly_global_information
        {
            uint32_t size;
            uint32_t flags;
            std::array<std::byte, 16> policy_coherency_guid;
            std::array<std::byte, 16> policy_override_guid;
            uint32_t application_directory_path_type;
            uint32_t application_directory_length;
            uint32_t application_directory_offset;
            uint32_t resource_name;
        };

        struct assembly_information
        {
            uint32_t size;
            uint32_t flags;
            uint32_t encoded_identity_length;
            uint32_t encoded_identity_offset;
            uint32_t manifest_path_type;
            uint32_t manifest_path_length;
            uint32_t manifest_path_offset;
            uint32_t manifest_last_write_time_low;
            int32_t manifest_last_write_time_high;
            uint32_t policy_path_type;
            uint32_t policy_path_length;
            uint32_t policy_path_offset;
            uint32_t policy_last_write_time_low;
            int32_t policy_last_write_time_high;
            uint32_t metadata_satellite_roster_index;
            uint32_t unused;
            uint32_t manifest_version_major;
            uint32_t manifest_version_minor;
            uint32_t policy_version_major;
            uint32_t policy_version_minor;
            uint32_t assembly_directory_name_length;
            uint32_t assembly_directory_name_offset;
            uint32_t file_count;
            uint32_t language_length;
            uint32_t language_offset;
            uint32_t run_level;
            uint32_t ui_access;
        };

        struct dll_redirection
        {
            uint32_t size;
            uint32_t flags;
            uint32_t total_path_length;
            uint32_t path_segment_count;
            uint32_t path_segment_offset;
        };

        struct window_class_redirection
        {
            uint32_t size;
            uint32_t flags;
            uint32_t name_length;
            uint32_t name_offset;
            uint32_t module_length;
            uint32_t module_offset;
        };

        static_assert(sizeof(activation_context_data) == 32);
        static_assert(sizeof(activation_context_toc_header) == 16);
        static_assert(sizeof(activation_context_toc_entry) == 16);
        static_assert(sizeof(assembly_roster_header) == 20);
        static_assert(sizeof(assembly_roster_entry) == 24);
        static_assert(sizeof(string_section_header) == 44);
        static_assert(sizeof(string_section_entry) == 24);
        static_assert(sizeof(assembly_global_information) == 56);
        static_assert(sizeof(assembly_information) == 108);
        static_assert(sizeof(dll_redirection) == 20);
        static_assert(sizeof(guid_section_header) == 40);
        static_assert(sizeof(guid_section_entry) == 28);
        static_assert(sizeof(com_server_redirection) == 120);
        static_assert(sizeof(com_interface_redirection) == 68);
        static_assert(sizeof(com_type_library_redirection) == 32);
        static_assert(sizeof(com_progid_redirection) == 12);
        static_assert(sizeof(window_class_redirection) == 24);

        struct version
        {
            std::array<uint16_t, 4> parts{};

            bool operator==(const version& other) const
            {
                return parts == other.parts;
            }

            bool operator<(const version& other) const
            {
                return std::ranges::lexicographical_compare(parts, other.parts);
            }

            bool operator>(const version& other) const
            {
                return other < *this;
            }

            bool operator<=(const version& other) const
            {
                return !(other < *this);
            }

            bool operator>=(const version& other) const
            {
                return !(*this < other);
            }
        };

        struct assembly_identity
        {
            std::string name{};
            std::string processor_architecture{};
            std::string public_key_token{};
            std::string type{};
            std::string language{};
            version assembly_version{};
            std::string version_text{};
        };

        struct assembly_dependency
        {
            assembly_identity identity{};
            bool optional{};
        };

        struct manifest_window_class
        {
            std::u16string name{};
            std::u16string file{};
            bool versioned{};
        };

        struct manifest_document
        {
            std::optional<assembly_identity> identity{};
            std::vector<assembly_dependency> dependencies{};
            std::vector<std::u16string> files{};
            std::vector<manifest_window_class> window_classes{};
        };

        struct xml_element
        {
            std::string name{};
            std::unordered_map<std::string, std::string> attributes{};
            std::string text{};
            std::vector<size_t> children{};
        };

        struct xml_document
        {
            std::vector<xml_element> elements{};
            std::vector<size_t> roots{};
        };

        struct source_document
        {
            std::string contents{};
            std::u16string path{};
        };

        struct resolved_assembly
        {
            assembly_identity identity{};
            std::u16string manifest_path{};
            std::u16string policy_path{};
            std::u16string directory_name{};
            std::vector<std::u16string> files{};
            std::vector<manifest_window_class> window_classes{};
            std::vector<assembly_dependency> dependencies{};
            std::string record_language{};
            std::vector<std::u16string> redirected_directories{};
            uint64_t manifest_last_write_time{};
            uint32_t flags{};
            uint32_t pseudokey{};
            bool keyed{true};
            bool system_default_layout{};
        };

        constexpr guid make_guid(const std::string_view text)
        {
            constexpr std::array<size_t, 16> text_offsets{6, 4, 2, 0, 11, 9, 16, 14, 19, 21, 24, 26, 28, 30, 32, 34};
            const auto hex_digit = [](const char character) {
                return static_cast<uint8_t>(character <= '9' ? character - '0' : character - 'a' + 10);
            };
            guid result{};
            for (size_t index = 0; index < result.size(); ++index)
            {
                const auto offset = text_offsets[index];
                result[index] = static_cast<std::byte>((hex_digit(text[offset]) << 4) | hex_digit(text[offset + 1]));
            }
            return result;
        }

        struct system_default_assembly_source
        {
            std::string_view version;
            std::u16string_view directory;
            uint64_t manifest_last_write_time;
        };

        struct system_default_architecture_source
        {
            std::string_view processor_architecture;
            std::array<system_default_assembly_source, 6> assemblies;
            std::array<guid, 2> configured_class_ids;
            std::array<size_t, 4> com_server_record_order;
            bool has_wow64_directory;
        };

        struct system_default_assembly_name
        {
            std::string_view name;
            std::string_view language;
            std::string_view record_language;
            std::span<const std::u16string_view> files;
        };

        constexpr std::array<std::u16string_view, 0> no_files{};
        constexpr std::array isolation_automation_files{u"sxsoa.dll"sv};
        constexpr std::array gdi_plus_files{u"GdiPlus.dll"sv};
        constexpr std::array common_controls_files{u"comctl32.dll"sv};
        constexpr std::array proxy_stub_files{u"sxsoaps.dll"sv, u"sxsoaps.tlb"sv};
        constexpr std::array common_controls_resources_files{u"comctl32.dll.mui"sv};

        constexpr std::array system_default_assembly_names{
            system_default_assembly_name{
                .name = "Microsoft.Windows.SystemCompatible", .language = "", .record_language = "", .files = no_files},
            system_default_assembly_name{.name = "Microsoft.Windows.IsolationAutomation",
                                         .language = "",
                                         .record_language = "",
                                         .files = isolation_automation_files},
            system_default_assembly_name{
                .name = "Microsoft.Windows.GdiPlus", .language = "", .record_language = "", .files = gdi_plus_files},
            system_default_assembly_name{
                .name = "Microsoft.Windows.Common-Controls", .language = "", .record_language = "", .files = common_controls_files},
            system_default_assembly_name{.name = "Microsoft.Windows.IsolationAutomation.ProxyStub",
                                         .language = "",
                                         .record_language = "",
                                         .files = proxy_stub_files},
            system_default_assembly_name{.name = "Microsoft.Windows.Common-Controls.Resources",
                                         .language = "en-us",
                                         .record_language = "en-US",
                                         .files = common_controls_resources_files},
        };

        constexpr size_t system_default_isolation_automation_index = 1;
        constexpr size_t system_default_common_controls_index = 3;
        constexpr size_t system_default_proxy_stub_index = 4;

        constexpr std::array system_default_architectures{
            system_default_architecture_source{
                .processor_architecture = "amd64",
                .assemblies = {{
                    {.version = "6.0.26100.1",
                     .directory = u"amd64_microsoft.windows.systemcompatible_6595b64144ccf1df_6.0.26100.1_none_03c982bdcc6b4e7d",
                     .manifest_last_write_time = 0x01da840542e64026},
                    {.version = "1.0.26100.5074",
                     .directory = u"amd64_microsoft.windows.isolationautomation_6595b64144ccf1df_1.0.26100.5074_none_ef991ef911793267",
                     .manifest_last_write_time = 0x01dc23d7443042cd},
                    {.version = "1.1.26100.9974",
                     .directory = u"amd64_microsoft.windows.gdiplus_6595b64144ccf1df_1.1.26100.9974_none_6ef868b23dc24782",
                     .manifest_last_write_time = 0x01dd7167a3014ec2},
                    {.version = "5.82.26100.8328",
                     .directory = u"amd64_microsoft.windows.common-controls_6595b64144ccf1df_5.82.26100.8328_none_87eede957a2ccddd",
                     .manifest_last_write_time = 0x01dce27d05a559ed},
                    {.version = "1.0.26100.5074",
                     .directory = u"amd64_microsoft.windows.i..utomation.proxystub_6595b64144ccf1df_1.0.26100.5074_none_9910702c70834c7b",
                     .manifest_last_write_time = 0x01dc23d7443042cd},
                    {.version = "5.82.26100.8117",
                     .directory = u"amd64_microsoft.windows.c..-controls.resources_6595b64144ccf1df_5.82.26100.8117_en-us_9df83bc8a6d4b654",
                     .manifest_last_write_time = 0x01dccc7d31e9af8f},
                }},
                .configured_class_ids = {make_guid("eb9a1a5b-95ab-4575-b4a3-00f70fef6436"),
                                         make_guid("755e2bd1-db56-4583-86b3-57c653486d3b")},
                .com_server_record_order = {0, 2, 1, 3},
                .has_wow64_directory = false,
            },
            system_default_architecture_source{
                .processor_architecture = "x86",
                .assemblies = {{
                    {.version = "6.0.26100.1",
                     .directory = u"x86_microsoft.windows.systemcompatible_6595b64144ccf1df_6.0.26100.1_none_4b76b994e0e77783",
                     .manifest_last_write_time = 0x01da8405428ba612},
                    {.version = "1.0.26100.5074",
                     .directory = u"x86_microsoft.windows.isolationautomation_6595b64144ccf1df_1.0.26100.5074_none_374655d025f55b6d",
                     .manifest_last_write_time = 0x01dc23d744ba3dbc},
                    {.version = "1.1.26100.9974",
                     .directory = u"x86_microsoft.windows.gdiplus_6595b64144ccf1df_1.1.26100.9974_none_b6a59f89523e7088",
                     .manifest_last_write_time = 0x01dd7167a66c9b9c},
                    {.version = "5.82.26100.8521",
                     .directory = u"x86_microsoft.windows.common-controls_6595b64144ccf1df_5.82.26100.8521_none_cf9e08b68ea73d80",
                     .manifest_last_write_time = 0x01dcf83e35cbbc75},
                    {.version = "1.0.26100.5074",
                     .directory = u"x86_microsoft.windows.i..utomation.proxystub_6595b64144ccf1df_1.0.26100.5074_none_e0bda70384ff7581",
                     .manifest_last_write_time = 0x01dc23d744ba64fd},
                    {.version = "5.82.26100.8117",
                     .directory = u"x86_microsoft.windows.c..-controls.resources_6595b64144ccf1df_5.82.26100.8117_en-us_e5a5729fbb50df5a",
                     .manifest_last_write_time = 0x01dccc7d31c38076},
                }},
                .configured_class_ids = {make_guid("324a81ea-52ad-4efe-87ab-78e95d62fbd9"),
                                         make_guid("cc8ecddc-41c1-4814-820d-07a9fdd078fd")},
                .com_server_record_order = {0, 1, 2, 3},
                .has_wow64_directory = true,
            },
        };

        struct system_default_com_class
        {
            size_t assembly_index;
            size_t configured_class_slot;
            guid clsid;
            guid type_library_id;
            std::u16string_view module;
            std::u16string_view progid;
            std::u16string_view version_independent_progid;
        };

        constexpr std::array system_default_com_classes{
            system_default_com_class{.assembly_index = system_default_isolation_automation_index,
                                     .configured_class_slot = 0,
                                     .clsid = make_guid("85bb4477-6dc3-4a8d-84d6-86a0fa1aaf8b"),
                                     .type_library_id = make_guid("5e0598d8-34a0-4329-bdbd-9d165c5c1554"),
                                     .module = u"sxsoa.dll",
                                     .progid = u"Microsoft.Windows.ActCtx.1",
                                     .version_independent_progid = u"Microsoft.Windows.ActCtx"},
            system_default_com_class{.assembly_index = system_default_proxy_stub_index,
                                     .configured_class_slot = 1,
                                     .clsid = make_guid("8fa7728f-b69b-4ee5-99f2-e2aa021bef28"),
                                     .type_library_id = make_guid("8143c9aa-38f8-4729-b935-df6823c616c6"),
                                     .module = u"sxsoaps.dll",
                                     .progid = {},
                                     .version_independent_progid = {}},
        };

        struct system_default_com_interface
        {
            size_t assembly_index;
            guid iid;
            guid type_library_id;
            std::u16string_view name;
        };

        constexpr std::array system_default_com_interfaces{
            system_default_com_interface{.assembly_index = system_default_proxy_stub_index,
                                         .iid = make_guid("8fa7728f-b69b-4ee5-99f2-e2aa021bef28"),
                                         .type_library_id = make_guid("8143c9aa-38f8-4729-b935-df6823c616c6"),
                                         .name = u"IActCtx"},
        };

        struct system_default_type_library
        {
            size_t assembly_index;
            guid type_library_id;
            std::u16string_view file;
        };

        constexpr std::array system_default_type_libraries{
            system_default_type_library{.assembly_index = system_default_isolation_automation_index,
                                        .type_library_id = make_guid("5e0598d8-34a0-4329-bdbd-9d165c5c1554"),
                                        .file = u"sxsoa.dll"},
            system_default_type_library{.assembly_index = system_default_proxy_stub_index,
                                        .type_library_id = make_guid("8143c9aa-38f8-4729-b935-df6823c616c6"),
                                        .file = u"sxsoaps.tlb"},
        };

        class byte_buffer
        {
          public:
            template <typename T>
            size_t append(const T& value = {})
            {
                const auto offset = this->data_.size();
                this->data_.resize(offset + sizeof(T));
                std::memcpy(this->data_.data() + offset, &value, sizeof(value));
                return offset;
            }

            template <typename T>
            size_t append_array(const size_t count)
            {
                const auto offset = this->data_.size();
                this->data_.resize(offset + sizeof(T) * count);
                return offset;
            }

            template <typename T>
            void write(const size_t offset, const T& value)
            {
                if (offset + sizeof(value) > this->data_.size())
                {
                    throw std::runtime_error("Invalid activation context buffer offset");
                }
                std::memcpy(this->data_.data() + offset, &value, sizeof(value));
            }

            size_t append_string(const std::u16string_view value)
            {
                const auto offset = this->data_.size();
                const auto byte_length = value.size() * sizeof(char16_t);
                this->data_.resize(offset + byte_length + sizeof(char16_t));
                std::memcpy(this->data_.data() + offset, value.data(), byte_length);
                return offset;
            }

            size_t append_characters(const std::u16string_view value)
            {
                const auto offset = this->data_.size();
                const auto byte_length = value.size() * sizeof(char16_t);
                this->data_.resize(offset + byte_length);
                std::memcpy(this->data_.data() + offset, value.data(), byte_length);
                return offset;
            }

            void align(const size_t alignment)
            {
                this->data_.resize((this->data_.size() + alignment - 1) & ~(alignment - 1));
            }

            size_t size() const
            {
                return this->data_.size();
            }

            void append(const std::vector<std::byte>& data)
            {
                this->data_.insert(this->data_.end(), data.begin(), data.end());
            }

            std::vector<std::byte> release()
            {
                return std::move(this->data_);
            }

          private:
            std::vector<std::byte> data_{};
        };

        std::string to_lower(std::string value)
        {
            std::ranges::transform(value, value.begin(),
                                   [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return value;
        }

        bool equal_insensitive(const std::string_view left, const std::string_view right)
        {
            return std::ranges::equal(left, right, [](const unsigned char left_character, const unsigned char right_character) {
                return std::tolower(left_character) == std::tolower(right_character);
            });
        }

        std::string decode_xml_entity(std::string value)
        {
            const std::array<std::pair<std::string_view, std::string_view>, 5> entities{{
                {"&quot;", "\""},
                {"&apos;", "'"},
                {"&lt;", "<"},
                {"&gt;", ">"},
                {"&amp;", "&"},
            }};
            for (const auto& [encoded, decoded] : entities)
            {
                for (size_t offset = 0; (offset = value.find(encoded, offset)) != std::string::npos; offset += decoded.size())
                {
                    value.replace(offset, encoded.size(), decoded);
                }
            }
            return value;
        }

        bool is_xml_name_character(const char character)
        {
            return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' || character == '.' || character == '-' ||
                   character == ':';
        }

        void skip_xml_whitespace(const std::string_view text, size_t& offset)
        {
            while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset])) != 0)
            {
                ++offset;
            }
        }

        std::string local_xml_name(const std::string_view name)
        {
            const auto separator = name.rfind(':');
            return to_lower(std::string(name.substr(separator == std::string_view::npos ? 0 : separator + 1)));
        }

        size_t find_xml_tag_end(const std::string_view xml, size_t offset)
        {
            char quote = 0;
            for (; offset < xml.size(); ++offset)
            {
                const auto character = xml[offset];
                if (quote != 0)
                {
                    if (character == quote)
                    {
                        quote = 0;
                    }
                }
                else if (character == '\'' || character == '"')
                {
                    quote = character;
                }
                else if (character == '>')
                {
                    return offset;
                }
            }
            return std::string_view::npos;
        }

        std::unordered_map<std::string, std::string> parse_attributes(const std::string_view text, size_t offset)
        {
            std::unordered_map<std::string, std::string> attributes{};
            while (offset < text.size())
            {
                skip_xml_whitespace(text, offset);
                if (offset >= text.size() || text[offset] == '/')
                {
                    break;
                }
                const auto name_start = offset;
                while (offset < text.size() && is_xml_name_character(text[offset]))
                {
                    ++offset;
                }
                if (name_start == offset)
                {
                    ++offset;
                    continue;
                }
                const auto name = local_xml_name(text.substr(name_start, offset - name_start));
                skip_xml_whitespace(text, offset);
                if (offset >= text.size() || text[offset] != '=')
                {
                    continue;
                }
                ++offset;
                skip_xml_whitespace(text, offset);
                if (offset >= text.size() || (text[offset] != '\'' && text[offset] != '"'))
                {
                    continue;
                }
                const auto quote = text[offset++];
                const auto value_start = offset;
                while (offset < text.size() && text[offset] != quote)
                {
                    ++offset;
                }
                attributes[name] = decode_xml_entity(std::string(text.substr(value_start, offset - value_start)));
                if (offset < text.size())
                {
                    ++offset;
                }
            }
            return attributes;
        }

        std::string read_element_text(const std::string_view xml, const size_t start)
        {
            const auto end = std::min(xml.find('<', start), xml.size());
            auto first = start;
            skip_xml_whitespace(xml, first);
            auto last = end;
            while (last > first && std::isspace(static_cast<unsigned char>(xml[last - 1])) != 0)
            {
                --last;
            }
            return decode_xml_entity(std::string(xml.substr(first, last - first)));
        }

        xml_document parse_xml(const std::string_view xml)
        {
            xml_document document{};
            std::vector<size_t> stack{};
            for (size_t offset = 0; (offset = xml.find('<', offset)) != std::string_view::npos;)
            {
                if (xml.substr(offset).starts_with("<!--"))
                {
                    const auto end = xml.find("-->", offset + 4);
                    offset = end == std::string_view::npos ? xml.size() : end + 3;
                    continue;
                }
                if (xml.substr(offset).starts_with("<![CDATA["))
                {
                    const auto end = xml.find("]]>", offset + 9);
                    offset = end == std::string_view::npos ? xml.size() : end + 3;
                    continue;
                }
                const auto end = find_xml_tag_end(xml, offset + 1);
                if (end == std::string_view::npos)
                {
                    break;
                }
                auto position = offset + 1;
                skip_xml_whitespace(xml, position);
                if (position >= end || xml[position] == '?' || xml[position] == '!')
                {
                    offset = end + 1;
                    continue;
                }
                if (xml[position] == '/')
                {
                    ++position;
                    skip_xml_whitespace(xml, position);
                    const auto name_start = position;
                    while (position < end && is_xml_name_character(xml[position]))
                    {
                        ++position;
                    }
                    if (!stack.empty() && name_start != position &&
                        document.elements[stack.back()].name == local_xml_name(xml.substr(name_start, position - name_start)))
                    {
                        stack.pop_back();
                    }
                    offset = end + 1;
                    continue;
                }
                const auto name_start = position;
                while (position < end && is_xml_name_character(xml[position]))
                {
                    ++position;
                }
                if (name_start == position)
                {
                    offset = end + 1;
                    continue;
                }
                xml_element element{};
                element.name = local_xml_name(xml.substr(name_start, position - name_start));
                element.attributes = parse_attributes(xml.substr(0, end), position);
                element.text = read_element_text(xml, end + 1);
                const auto index = document.elements.size();
                document.elements.push_back(std::move(element));
                if (stack.empty())
                {
                    document.roots.push_back(index);
                }
                else
                {
                    document.elements[stack.back()].children.push_back(index);
                }
                auto closing = end;
                while (closing > position && std::isspace(static_cast<unsigned char>(xml[closing - 1])) != 0)
                {
                    --closing;
                }
                if (closing == position || xml[closing - 1] != '/')
                {
                    stack.push_back(index);
                }
                offset = end + 1;
            }
            return document;
        }

        const xml_element* find_root_xml_element(const xml_document& document, const std::string_view name)
        {
            for (const auto index : document.roots)
            {
                if (document.elements[index].name == name)
                {
                    return &document.elements[index];
                }
            }
            return nullptr;
        }

        std::vector<const xml_element*> find_xml_children(const xml_document& document, const xml_element& parent,
                                                          const std::string_view name)
        {
            std::vector<const xml_element*> result{};
            for (const auto index : parent.children)
            {
                if (document.elements[index].name == name)
                {
                    result.push_back(&document.elements[index]);
                }
            }
            return result;
        }

        std::vector<const xml_element*> find_xml_elements(const xml_document& document, const std::string_view name)
        {
            std::vector<const xml_element*> result{};
            for (const auto& element : document.elements)
            {
                if (element.name == name)
                {
                    result.push_back(&element);
                }
            }
            return result;
        }

        const xml_element* find_xml_descendant(const xml_document& document, const xml_element& parent, const std::string_view name)
        {
            std::vector<size_t> pending(parent.children.begin(), parent.children.end());
            while (!pending.empty())
            {
                const auto index = pending.back();
                pending.pop_back();
                const auto& element = document.elements[index];
                if (element.name == name)
                {
                    return &element;
                }
                pending.insert(pending.end(), element.children.begin(), element.children.end());
            }
            return nullptr;
        }

        std::vector<const xml_element*> find_xml_descendants(const xml_document& document, const xml_element& parent,
                                                             const std::string_view name)
        {
            std::vector<const xml_element*> result{};
            std::vector<size_t> pending(parent.children.begin(), parent.children.end());
            while (!pending.empty())
            {
                const auto index = pending.back();
                pending.pop_back();
                const auto& element = document.elements[index];
                if (element.name == name)
                {
                    result.push_back(&element);
                }
                pending.insert(pending.end(), element.children.begin(), element.children.end());
            }
            return result;
        }

        std::optional<version> parse_version(const std::string_view text)
        {
            version result{};
            size_t start = 0;
            for (size_t index = 0; index < result.parts.size(); ++index)
            {
                const auto end = text.find('.', start);
                const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start : end - start);
                if (part.empty())
                {
                    return std::nullopt;
                }
                uint32_t value{};
                const auto [parse_end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
                if (error != std::errc{} || parse_end != part.data() + part.size() || value > std::numeric_limits<uint16_t>::max())
                {
                    return std::nullopt;
                }
                result.parts[index] = static_cast<uint16_t>(value);
                if (index + 1 < result.parts.size())
                {
                    if (end == std::string_view::npos)
                    {
                        return std::nullopt;
                    }
                    start = end + 1;
                }
                else if (end != std::string_view::npos)
                {
                    return std::nullopt;
                }
            }
            return result;
        }

        std::optional<assembly_identity> parse_identity(const std::unordered_map<std::string, std::string>& attributes)
        {
            const auto name = attributes.find("name");
            if (name == attributes.end())
            {
                return std::nullopt;
            }

            assembly_identity identity{};
            identity.name = name->second;
            if (const auto entry = attributes.find("processorarchitecture"); entry != attributes.end())
            {
                identity.processor_architecture = entry->second;
            }
            if (const auto entry = attributes.find("publickeytoken"); entry != attributes.end())
            {
                identity.public_key_token = entry->second;
            }
            if (const auto entry = attributes.find("type"); entry != attributes.end())
            {
                identity.type = entry->second;
            }
            if (const auto entry = attributes.find("language"); entry != attributes.end())
            {
                identity.language = entry->second;
            }
            if (const auto entry = attributes.find("version"); entry != attributes.end())
            {
                const auto parsed = parse_version(entry->second);
                if (!parsed)
                {
                    return std::nullopt;
                }
                identity.assembly_version = *parsed;
                identity.version_text = entry->second;
            }
            return identity;
        }

        std::string decode_manifest_text(const std::span<const std::byte> data)
        {
            const auto is_utf16_little_endian = data.size() >= 2 && data[0] == std::byte{0xff} && data[1] == std::byte{0xfe};
            const auto is_utf16_big_endian = data.size() >= 2 && data[0] == std::byte{0xfe} && data[1] == std::byte{0xff};
            if (is_utf16_little_endian || is_utf16_big_endian)
            {
                const auto length = (data.size() - 2) / sizeof(char16_t);
                std::u16string text(length, u'\0');
                for (size_t index = 0; index < length; ++index)
                {
                    const auto first = std::to_integer<uint8_t>(data[2 + index * 2]);
                    const auto second = std::to_integer<uint8_t>(data[3 + index * 2]);
                    text[index] = static_cast<char16_t>(is_utf16_little_endian ? first | (second << 8) : (first << 8) | second);
                }
                return u16_to_u8(text);
            }

            size_t offset = 0;
            if (data.size() >= 3 && data[0] == std::byte{0xef} && data[1] == std::byte{0xbb} && data[2] == std::byte{0xbf})
            {
                offset = 3;
            }
            return {reinterpret_cast<const char*>(data.data() + offset), data.size() - offset};
        }

        manifest_document parse_manifest(const std::string& xml)
        {
            manifest_document manifest{};
            const auto document = parse_xml(xml);
            const auto* assembly = find_root_xml_element(document, "assembly");
            if (assembly == nullptr)
            {
                return manifest;
            }
            const auto identities = find_xml_children(document, *assembly, "assemblyidentity");
            if (!identities.empty())
            {
                manifest.identity = parse_identity(identities.front()->attributes);
            }

            for (const auto* dependency : find_xml_children(document, *assembly, "dependency"))
            {
                if (const auto* dependency_identity = find_xml_descendant(document, *dependency, "assemblyidentity"))
                {
                    if (auto identity = parse_identity(dependency_identity->attributes))
                    {
                        const auto optional = dependency->attributes.find("optional");
                        manifest.dependencies.push_back(
                            {.identity = std::move(*identity),
                             .optional = optional != dependency->attributes.end() && equal_insensitive(optional->second, "yes")});
                    }
                }
            }

            for (const auto* file : find_xml_children(document, *assembly, "file"))
            {
                const auto name = file->attributes.find("name");
                if (name == file->attributes.end())
                {
                    continue;
                }
                manifest.files.push_back(u8_to_u16(name->second));
                for (const auto* window_class_element : find_xml_children(document, *file, "windowclass"))
                {
                    if (window_class_element->text.empty())
                    {
                        continue;
                    }
                    const auto versioned = window_class_element->attributes.find("versioned");
                    manifest.window_classes.push_back(
                        {.name = u8_to_u16(window_class_element->text),
                         .file = u8_to_u16(name->second),
                         .versioned = versioned == window_class_element->attributes.end() || !equal_insensitive(versioned->second, "no")});
                }
            }
            return manifest;
        }

        template <typename T>
        std::optional<T> read_object(const std::vector<std::byte>& data, const size_t offset)
        {
            if (offset > data.size() || sizeof(T) > data.size() - offset)
            {
                return std::nullopt;
            }
            T result{};
            std::memcpy(&result, data.data() + offset, sizeof(result));
            return result;
        }

        std::optional<size_t> rva_to_file_offset(const std::vector<std::byte>& image, const size_t nt_offset, const uint32_t rva)
        {
            const auto section_count = read_object<uint16_t>(image, nt_offset + 6);
            const auto optional_size = read_object<uint16_t>(image, nt_offset + 20);
            const auto header_size = read_object<uint32_t>(image, nt_offset + 24 + 60);
            if (!section_count || !optional_size || !header_size)
            {
                return std::nullopt;
            }
            if (rva < *header_size)
            {
                return rva;
            }

            const auto first_section = nt_offset + 24 + *optional_size;
            for (uint16_t index = 0; index < *section_count; ++index)
            {
                const auto section = first_section + index * 40;
                const auto virtual_size = read_object<uint32_t>(image, section + 8);
                const auto virtual_address = read_object<uint32_t>(image, section + 12);
                const auto raw_size = read_object<uint32_t>(image, section + 16);
                const auto raw_offset = read_object<uint32_t>(image, section + 20);
                if (!virtual_size || !virtual_address || !raw_size || !raw_offset)
                {
                    return std::nullopt;
                }
                const auto mapped_size = std::max(*virtual_size, *raw_size);
                if (rva >= *virtual_address && rva - *virtual_address < mapped_size)
                {
                    return static_cast<size_t>(*raw_offset) + (rva - *virtual_address);
                }
            }
            return std::nullopt;
        }

        std::optional<uint32_t> find_resource_entry(const std::vector<std::byte>& image, const size_t resource_base,
                                                    const uint32_t directory_offset, const std::optional<uint16_t> id)
        {
            const auto named_count = read_object<uint16_t>(image, resource_base + directory_offset + 12);
            const auto id_count = read_object<uint16_t>(image, resource_base + directory_offset + 14);
            if (!named_count || !id_count)
            {
                return std::nullopt;
            }
            const auto total = static_cast<uint32_t>(*named_count) + *id_count;
            for (uint32_t index = 0; index < total; ++index)
            {
                const auto entry_offset = resource_base + directory_offset + 16 + index * 8;
                const auto name = read_object<uint32_t>(image, entry_offset);
                const auto target = read_object<uint32_t>(image, entry_offset + 4);
                if (!name || !target)
                {
                    return std::nullopt;
                }
                if (!id || (!(*name & 0x80000000) && static_cast<uint16_t>(*name) == *id))
                {
                    return target;
                }
            }
            return std::nullopt;
        }

        std::optional<std::string> read_embedded_manifest(const std::vector<std::byte>& image, const uint16_t resource_id = 1)
        {
            if (image.size() < 0x40)
            {
                return std::nullopt;
            }
            const auto nt_offset = read_object<uint32_t>(image, 0x3c);
            if (!nt_offset)
            {
                return std::nullopt;
            }
            const auto signature = read_object<uint32_t>(image, *nt_offset);
            if (!signature || *signature != 0x4550)
            {
                return std::nullopt;
            }
            const auto optional = static_cast<size_t>(*nt_offset) + 24;
            const auto magic = read_object<uint16_t>(image, optional);
            if (!magic || (*magic != 0x10b && *magic != 0x20b))
            {
                return std::nullopt;
            }
            const auto directories = optional + (*magic == 0x10b ? 96 : 112);
            const auto resource_rva = read_object<uint32_t>(image, directories + 2 * 8);
            if (!resource_rva || !*resource_rva)
            {
                return std::nullopt;
            }
            const auto resource_base = rva_to_file_offset(image, *nt_offset, *resource_rva);
            if (!resource_base)
            {
                return std::nullopt;
            }

            const auto type = find_resource_entry(image, *resource_base, 0, static_cast<uint16_t>(rt_manifest));
            if (!type || !(*type & 0x80000000))
            {
                return std::nullopt;
            }
            auto name = find_resource_entry(image, *resource_base, *type & 0x7fffffff, resource_id);
            if (!name)
            {
                name = find_resource_entry(image, *resource_base, *type & 0x7fffffff, std::nullopt);
            }
            if (!name || !(*name & 0x80000000))
            {
                return std::nullopt;
            }
            const auto language = find_resource_entry(image, *resource_base, *name & 0x7fffffff, std::nullopt);
            if (!language || (*language & 0x80000000))
            {
                return std::nullopt;
            }
            const auto data_entry = *resource_base + *language;
            const auto data_rva = read_object<uint32_t>(image, data_entry);
            const auto data_size = read_object<uint32_t>(image, data_entry + 4);
            if (!data_rva || !data_size)
            {
                return std::nullopt;
            }
            const auto data_offset = rva_to_file_offset(image, *nt_offset, *data_rva);
            if (!data_offset || *data_offset > image.size() || *data_size > image.size() - *data_offset)
            {
                return std::nullopt;
            }
            const auto manifest_begin = std::next(image.begin(), static_cast<std::ptrdiff_t>(*data_offset));
            const auto manifest_end = std::next(manifest_begin, static_cast<std::ptrdiff_t>(*data_size));
            std::vector<std::byte> manifest(manifest_begin, manifest_end);
            return decode_manifest_text(manifest);
        }

        std::optional<std::string> read_embedded_manifest(const file_system& file_system, const windows_path& path,
                                                          const uint16_t resource_id = 1)
        {
            std::vector<std::byte> image{};
            if (!file_system.read_file(path, &image))
            {
                return std::nullopt;
            }
            return read_embedded_manifest(image, resource_id);
        }

        std::optional<std::string> read_text_file(const file_system& file_system, const windows_path& path)
        {
            std::vector<std::byte> data{};
            if (!file_system.read_file(path, &data))
            {
                return std::nullopt;
            }
            return decode_manifest_text(data);
        }

        // windows_path::u16string() is lower case, but the blob stores the root spelled the way Windows does.
        std::u16string spelled_path(const windows_path& path)
        {
            auto spelled = path.to_portable_path().u16string();
            spelled.insert(1, 1, u':');
            auto& drive = spelled.front();
            if (drive >= u'a' && drive <= u'z')
            {
                drive = static_cast<char16_t>(drive - u'a' + u'A');
            }
            std::ranges::replace(spelled, u'/', u'\\');
            return spelled;
        }

        windows_path with_suffix(const windows_path& path, const std::u16string_view suffix)
        {
            return windows_path{path.u16string() + std::u16string{suffix}};
        }

        std::optional<source_document> read_application_manifest(const file_system& file_system, const mapped_module& executable)
        {
            if (auto embedded = read_embedded_manifest(file_system, executable.module_path))
            {
                return source_document{.contents = std::move(*embedded), .path = executable.module_path.u16string()};
            }
            for (const auto* suffix : {u".manifest", u".Manifest"})
            {
                const auto path = with_suffix(executable.module_path, suffix);
                if (auto manifest = read_text_file(file_system, path))
                {
                    return source_document{.contents = std::move(*manifest), .path = path.u16string()};
                }
            }
            return std::nullopt;
        }

        std::optional<source_document> read_application_config(const file_system& file_system, const mapped_module& executable)
        {
            for (const auto* suffix : {u".config", u".Config"})
            {
                const auto path = with_suffix(executable.module_path, suffix);
                if (auto contents = read_text_file(file_system, path))
                {
                    return source_document{.contents = std::move(*contents), .path = path.u16string()};
                }
            }
            return std::nullopt;
        }

        bool identity_matches(const assembly_identity& candidate, const assembly_identity& requested, const bool compare_version)
        {
            if (!equal_insensitive(candidate.name, requested.name) ||
                (!requested.public_key_token.empty() && !equal_insensitive(candidate.public_key_token, requested.public_key_token)) ||
                (!requested.processor_architecture.empty() && requested.processor_architecture != "*" &&
                 !equal_insensitive(candidate.processor_architecture, requested.processor_architecture)) ||
                (!requested.type.empty() && !equal_insensitive(candidate.type, requested.type)) ||
                (!requested.language.empty() && requested.language != "*" && !equal_insensitive(candidate.language, requested.language)))
            {
                return false;
            }
            return !compare_version || requested.version_text.empty() || candidate.assembly_version == requested.assembly_version;
        }

        bool is_unified_system_assembly(const assembly_identity& identity)
        {
            return to_lower(identity.name).starts_with("microsoft.windows.") &&
                   equal_insensitive(identity.public_key_token, "6595b64144ccf1df");
        }

        bool has_compatible_system_version(const assembly_identity& candidate, const assembly_identity& requested)
        {
            return is_unified_system_assembly(requested) && candidate.assembly_version.parts[0] == requested.assembly_version.parts[0] &&
                   candidate.assembly_version.parts[1] == requested.assembly_version.parts[1];
        }

        std::string winsxs_name(std::string name)
        {
            name = to_lower(std::move(name));
            if (name.size() > 40)
            {
                name = name.substr(0, 19) + ".." + name.substr(name.size() - 19);
            }
            return name;
        }

        bool is_better_system_candidate(const assembly_identity& candidate, const assembly_identity& current,
                                        const std::string_view preferred_language)
        {
            const auto candidate_uses_preferred_language = equal_insensitive(candidate.language, preferred_language);
            const auto current_uses_preferred_language = equal_insensitive(current.language, preferred_language);
            if (candidate_uses_preferred_language != current_uses_preferred_language)
            {
                return candidate_uses_preferred_language;
            }
            return candidate.assembly_version > current.assembly_version;
        }

        bool version_in_range(const version& value, const std::string_view range)
        {
            const auto separator = range.find('-');
            const auto minimum = parse_version(range.substr(0, separator));
            const auto maximum = parse_version(separator == std::string_view::npos ? range : range.substr(separator + 1));
            return minimum && maximum && value >= *minimum && value <= *maximum;
        }

        bool apply_binding_redirects(const std::string& xml, assembly_identity& identity)
        {
            const auto document = parse_xml(xml);
            for (const auto* dependent_assembly : find_xml_elements(document, "dependentassembly"))
            {
                const auto* identity_element = find_xml_descendant(document, *dependent_assembly, "assemblyidentity");
                if (identity_element == nullptr)
                {
                    continue;
                }
                const auto policy_identity = parse_identity(identity_element->attributes);
                if (!policy_identity || !identity_matches(*policy_identity, identity, false))
                {
                    continue;
                }
                for (const auto* redirect : find_xml_descendants(document, *dependent_assembly, "bindingredirect"))
                {
                    const auto old_version = redirect->attributes.find("oldversion");
                    const auto new_version = redirect->attributes.find("newversion");
                    if (old_version == redirect->attributes.end() || new_version == redirect->attributes.end() ||
                        !version_in_range(identity.assembly_version, old_version->second))
                    {
                        continue;
                    }
                    const auto parsed = parse_version(new_version->second);
                    if (parsed)
                    {
                        identity.assembly_version = *parsed;
                        identity.version_text = new_version->second;
                        return true;
                    }
                }
            }
            return false;
        }

        bool allows_publisher_policy(const std::string& xml, const assembly_identity& identity)
        {
            const auto document = parse_xml(xml);
            for (const auto* dependent_assembly : find_xml_elements(document, "dependentassembly"))
            {
                const auto* identity_element = find_xml_descendant(document, *dependent_assembly, "assemblyidentity");
                if (identity_element == nullptr)
                {
                    continue;
                }
                const auto policy_identity = parse_identity(identity_element->attributes);
                if (!policy_identity || !identity_matches(*policy_identity, identity, false))
                {
                    continue;
                }
                for (const auto* policy : find_xml_descendants(document, *dependent_assembly, "publisherpolicy"))
                {
                    const auto apply = policy->attributes.find("apply");
                    if (apply != policy->attributes.end() && equal_insensitive(apply->second, "no"))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        std::string architecture_name(const assembly_identity& identity, const mapped_module& executable)
        {
            if (!identity.processor_architecture.empty() && identity.processor_architecture != "*")
            {
                return to_lower(identity.processor_architecture);
            }
            switch (executable.machine)
            {
            case IMAGE_FILE_MACHINE_I386:
                return "x86";
            case IMAGE_FILE_MACHINE_ARM64:
                return "arm64";
            default:
                return "amd64";
            }
        }

        struct manifest_entry
        {
            std::u16string name;
            std::string filename;
        };

        class manifest_directory
        {
          public:
            const std::vector<manifest_entry>& entries(const file_system& file_system, const windows_path& directory)
            {
                if (!this->entries_)
                {
                    auto names = file_system.list_directory_names(directory);
                    std::ranges::sort(names);
                    std::vector<manifest_entry> entries{};
                    entries.reserve(names.size());
                    for (auto& name : names)
                    {
                        auto filename = to_lower(u16_to_u8(name));
                        if (filename.ends_with(".manifest"))
                        {
                            entries.push_back({.name = std::move(name), .filename = std::move(filename)});
                        }
                    }
                    this->entries_ = std::move(entries);
                }
                return *this->entries_;
            }

          private:
            std::optional<std::vector<manifest_entry>> entries_{};
        };

        void apply_system_policy(const file_system& file_system, const windows_path& manifests_path, assembly_identity& identity,
                                 const mapped_module& executable, manifest_directory& manifests)
        {
            const auto name_fragment = winsxs_name(identity.name);
            const auto token_fragment = to_lower(identity.public_key_token);
            const auto architecture = architecture_name(identity, executable);
            const auto version_prefix =
                std::to_string(identity.assembly_version.parts[0]) + "." + std::to_string(identity.assembly_version.parts[1]) + ".";
            const auto prefix = architecture + "_policy." + version_prefix + name_fragment + "_";
            for (const auto& entry : manifests.entries(file_system, manifests_path))
            {
                const auto& filename = entry.filename;
                if (!filename.starts_with(prefix) || (!token_fragment.empty() && filename.find(token_fragment) == std::string::npos))
                {
                    continue;
                }
                if (const auto policy = read_text_file(file_system, manifests_path / windows_path(entry.name)))
                {
                    apply_binding_redirects(*policy, identity);
                }
            }
        }

        bool is_plain_file_name(const std::u16string_view name)
        {
            return !name.empty() && name != u"." && name != u".." && name.find_first_of(u"\\/:") == std::u16string_view::npos;
        }

        std::optional<resolved_assembly> resolve_private_assembly(const file_system& file_system, const mapped_module& executable,
                                                                  const assembly_identity& identity)
        {
            const auto identity_name = u8_to_u16(identity.name);
            if (!is_plain_file_name(identity_name))
            {
                return std::nullopt;
            }

            struct candidate
            {
                windows_path relative_path;
                std::u16string directory_name;
            };

            const auto make_candidates = [&](const std::u16string_view extension) {
                const auto file_name = identity_name + std::u16string(extension);
                return std::array{
                    candidate{.relative_path = windows_path{file_name}, .directory_name = {}},
                    candidate{.relative_path = windows_path{identity_name} / windows_path{file_name}, .directory_name = identity_name},
                };
            };

            const auto application_directory = executable.module_path.parent();

            const auto make_result = [&](const candidate& entry, manifest_document manifest) {
                resolved_assembly result{};
                result.identity = std::move(*manifest.identity);
                result.manifest_path = (application_directory / entry.relative_path).u16string();
                result.directory_name = entry.directory_name;
                result.files = std::move(manifest.files);
                result.window_classes = std::move(manifest.window_classes);
                result.dependencies = std::move(manifest.dependencies);
                result.flags = assembly_information_private;
                return result;
            };

            for (const auto& entry : make_candidates(u".manifest"))
            {
                const auto xml = read_text_file(file_system, application_directory / entry.relative_path);
                if (!xml)
                {
                    continue;
                }
                auto manifest = parse_manifest(*xml);
                if (!manifest.identity || !identity_matches(*manifest.identity, identity, true))
                {
                    continue;
                }
                return make_result(entry, std::move(manifest));
            }

            for (const auto& entry : make_candidates(u".dll"))
            {
                const auto xml = read_embedded_manifest(file_system, application_directory / entry.relative_path, 2);
                if (!xml)
                {
                    continue;
                }
                auto manifest = parse_manifest(*xml);
                if (!manifest.identity || !identity_matches(*manifest.identity, identity, true))
                {
                    continue;
                }
                return make_result(entry, std::move(manifest));
            }
            return std::nullopt;
        }

        std::optional<resolved_assembly> resolve_winsxs_assembly(const file_system& file_system, const windows_path& manifests_path,
                                                                 const windows_path& winsxs_path, const assembly_identity& identity,
                                                                 const mapped_module& executable, const std::string_view preferred_language,
                                                                 manifest_directory& manifests)
        {
            const auto architecture = architecture_name(identity, executable);
            const auto prefix = architecture + "_" + winsxs_name(identity.name) + "_";
            const auto token_fragment = "_" + to_lower(identity.public_key_token) + "_";
            std::optional<resolved_assembly> exact{};
            std::optional<resolved_assembly> compatible{};
            for (const auto& entry : manifests.entries(file_system, manifests_path))
            {
                const auto& filename = entry.filename;
                if (!filename.starts_with(prefix) ||
                    (!identity.public_key_token.empty() && filename.find(token_fragment) == std::string::npos))
                {
                    continue;
                }
                const auto xml = read_text_file(file_system, manifests_path / windows_path(entry.name));
                if (!xml)
                {
                    continue;
                }
                auto manifest = parse_manifest(*xml);
                if (!manifest.identity || !identity_matches(*manifest.identity, identity, false))
                {
                    continue;
                }
                const auto stem = std::filesystem::path(entry.name).stem().u16string();
                const auto assembly_directory = file_system.stat(winsxs_path / windows_path(stem));
                if (!assembly_directory || (assembly_directory->st_mode & S_IFMT) != S_IFDIR)
                {
                    continue;
                }
                resolved_assembly result{};
                result.identity = std::move(*manifest.identity);
                result.manifest_path = (manifests_path / windows_path(entry.name)).u16string();
                result.directory_name = stem;
                result.files = std::move(manifest.files);
                result.window_classes = std::move(manifest.window_classes);
                result.dependencies = std::move(manifest.dependencies);
                if (identity_matches(result.identity, identity, true))
                {
                    if (identity.language != "*" || equal_insensitive(result.identity.language, preferred_language))
                    {
                        return result;
                    }
                    if (!exact || is_better_system_candidate(result.identity, exact->identity, preferred_language))
                    {
                        exact = std::move(result);
                    }
                    continue;
                }
                if (has_compatible_system_version(result.identity, identity) &&
                    (!compatible || is_better_system_candidate(result.identity, compatible->identity, preferred_language)))
                {
                    compatible = std::move(result);
                }
            }
            if (exact)
            {
                return exact;
            }
            return compatible;
        }

        class case_table
        {
          public:
            case_table() = default;

            explicit case_table(std::vector<uint16_t> words)
                : words_(std::move(words))
            {
                constexpr size_t block_size = 16;
                if (this->words_.size() < header_words + 256)
                {
                    throw std::runtime_error("Invalid l_intl.nls case table");
                }
                const auto in_range = [&](const size_t offset) { return offset + block_size <= this->words_.size() - header_words; };
                for (size_t high = 0; high < 256; ++high)
                {
                    if (!in_range(this->level(high)))
                    {
                        throw std::runtime_error("Invalid l_intl.nls case table");
                    }
                    for (size_t middle = 0; middle < block_size; ++middle)
                    {
                        if (!in_range(this->level(this->level(high) + middle)))
                        {
                            throw std::runtime_error("Invalid l_intl.nls case table");
                        }
                    }
                }
            }

            char16_t upcase(const char16_t character) const
            {
                if (this->words_.empty())
                {
                    return character >= u'a' && character <= u'z' ? static_cast<char16_t>(character - (u'a' - u'A')) : character;
                }

                const auto middle = this->level(character >> 8) + ((character >> 4) & 0xf);
                return static_cast<char16_t>(character + this->level(this->level(middle) + (character & 0xf)));
            }

          private:
            static constexpr size_t header_words = 2;

            size_t level(const size_t index) const
            {
                return this->words_[header_words + index];
            }

            std::vector<uint16_t> words_{};
        };

        case_table read_case_table(const file_system& file_system, const windows_path& system_root)
        {
            std::vector<std::byte> data{};
            if (!file_system.read_file(system_root / windows_path(u"System32") / windows_path(u"l_intl.nls"), &data))
            {
                return {};
            }
            std::vector<uint16_t> words(data.size() / sizeof(uint16_t));
            std::memcpy(words.data(), data.data(), words.size() * sizeof(uint16_t));
            return case_table{std::move(words)};
        }

        uint32_t hash_string(const std::u16string_view value, const case_table& casing)
        {
            uint32_t hash = 0;
            for (const auto character : value)
            {
                hash = hash * 65599 + casing.upcase(character);
            }
            return hash;
        }

        std::u16string encode_identity(const assembly_identity& identity)
        {
            std::string encoded = identity.name;
            if (!identity.language.empty() && identity.language != "*")
            {
                encoded += ",language=\"" + identity.language + "\"";
            }
            if (!identity.processor_architecture.empty())
            {
                encoded += ",processorArchitecture=\"" + identity.processor_architecture + "\"";
            }
            if (!identity.public_key_token.empty())
            {
                encoded += ",publicKeyToken=\"" + identity.public_key_token + "\"";
            }
            if (!identity.type.empty())
            {
                encoded += ",type=\"" + identity.type + "\"";
            }
            if (!identity.version_text.empty())
            {
                encoded += ",version=\"" + identity.version_text + "\"";
            }
            return u8_to_u16(encoded);
        }

        uint32_t checked_u32(const size_t value)
        {
            if (value > std::numeric_limits<uint32_t>::max())
            {
                throw std::runtime_error("Activation context is too large");
            }
            return static_cast<uint32_t>(value);
        }

        uint32_t byte_length(const std::u16string_view value)
        {
            return checked_u32(value.size() * sizeof(char16_t));
        }

        struct hashed_entry
        {
            uint32_t pseudokey{};
            uint32_t entry_offset{};
        };

        // Window class sections captured from Windows for 1..40, 48, 50, 64, 80, 100 and 128 elements; counts between 81 and 99 are
        // not captured.
        size_t string_section_bucket_count(const size_t element_count)
        {
            if (element_count < 3)
            {
                return 0;
            }
            if (element_count < 15)
            {
                return 3;
            }
            return element_count < 100 ? 11 : 101;
        }

        size_t append_hash_table(byte_buffer& section, const std::vector<hashed_entry>& entries, const size_t bucket_count)
        {
            const auto hash_table_offset = section.append<string_section_hash_table>();
            const auto buckets_offset = section.append_array<string_section_hash_bucket>(bucket_count);
            std::vector<std::vector<uint32_t>> chains(bucket_count);
            for (const auto& entry : entries)
            {
                chains[entry.pseudokey % bucket_count].push_back(entry.entry_offset);
            }
            for (size_t index = 0; index < chains.size(); ++index)
            {
                string_section_hash_bucket bucket{};
                bucket.chain_count = checked_u32(chains[index].size());
                if (!chains[index].empty())
                {
                    bucket.chain_offset = checked_u32(section.size());
                    for (const auto entry_offset : chains[index])
                    {
                        section.append(entry_offset);
                    }
                }
                section.write(buckets_offset + index * sizeof(bucket), bucket);
            }
            section.write(hash_table_offset, string_section_hash_table{.bucket_table_entry_count = checked_u32(bucket_count),
                                                                       .bucket_table_offset = checked_u32(buckets_offset)});
            return hash_table_offset;
        }

        struct string_section_search
        {
            uint32_t flags{};
            uint32_t search_structure_offset{};
        };

        string_section_search finish_string_section(byte_buffer& section, const size_t elements_offset,
                                                    std::vector<string_section_entry> elements)
        {
            const auto bucket_count = string_section_bucket_count(elements.size());
            if (bucket_count == 0)
            {
                std::ranges::stable_sort(elements, {}, &string_section_entry::pseudokey);
            }

            std::vector<hashed_entry> hashed{};
            hashed.reserve(elements.size());
            for (size_t index = 0; index < elements.size(); ++index)
            {
                const auto entry_offset = checked_u32(elements_offset + index * sizeof(string_section_entry));
                section.write(entry_offset, elements[index]);
                hashed.push_back({.pseudokey = elements[index].pseudokey, .entry_offset = entry_offset});
            }

            if (bucket_count == 0)
            {
                return {.flags = string_section_case_insensitive | string_section_entries_in_pseudokey_order, .search_structure_offset = 0};
            }
            return {.flags = string_section_case_insensitive,
                    .search_structure_offset = checked_u32(append_hash_table(section, hashed, bucket_count))};
        }

        string_section_header make_string_section_header(const size_t element_count, const size_t elements_offset,
                                                         const string_section_search& search)
        {
            string_section_header header{};
            header.magic = string_section_magic;
            header.header_size = sizeof(header);
            header.format_version = 1;
            header.data_format_version = 1;
            header.flags = search.flags;
            header.element_count = checked_u32(element_count);
            header.element_list_offset = checked_u32(elements_offset);
            header.hash_algorithm = hash_string_algorithm_x65599;
            header.search_structure_offset = search.search_structure_offset;
            return header;
        }

        struct assembly_section_result
        {
            std::vector<std::byte> data{};
            std::vector<string_section_entry> entries{};
        };

        assembly_section_result build_assembly_section(const std::vector<resolved_assembly>& assemblies,
                                                       const std::u16string_view application_directory)
        {
            byte_buffer section{};
            const auto header_offset = section.append<string_section_header>();

            assembly_global_information global{};
            global.application_directory_path_type = activation_context_path_type_win32_file;
            global.application_directory_length = byte_length(application_directory);
            global.application_directory_offset = sizeof(global);
            global.resource_name = assemblies.front().system_default_layout ? 0 : 1;
            const auto global_offset = section.append(global);
            section.append_string(application_directory);
            global.size = checked_u32(section.size() - global_offset);
            section.write(global_offset, global);
            section.align(4);

            const auto entries_offset = section.append_array<string_section_entry>(assemblies.size());
            std::vector<string_section_entry> entries(assemblies.size());
            for (size_t index = 0; index < assemblies.size(); ++index)
            {
                const auto& assembly = assemblies[index];
                auto& entry = entries[index];
                entry.pseudokey = assembly.pseudokey;
                entry.assembly_roster_index = checked_u32(index + 1);
                if (assembly.keyed)
                {
                    const auto name = u8_to_u16(assembly.identity.name);
                    entry.key_offset = checked_u32(section.append_string(name));
                    entry.key_length = byte_length(name);
                    section.align(4);
                }

                entry.offset = checked_u32(section.size());
                assembly_information information{};
                information.size = sizeof(information);
                information.flags = assembly.flags;
                information.manifest_version_major = 1;
                if (assembly.system_default_layout)
                {
                    information.policy_path_type = activation_context_path_type_none;
                    information.manifest_last_write_time_low = static_cast<uint32_t>(assembly.manifest_last_write_time);
                    information.manifest_last_write_time_high = static_cast<int32_t>(assembly.manifest_last_write_time >> 32);
                }
                else
                {
                    information.file_count = checked_u32(assembly.files.size());
                }
                const auto information_offset = section.append(information);

                const auto append_text = [&](const std::u16string_view text, uint32_t& length, uint32_t& offset, const bool terminated) {
                    length = byte_length(text);
                    offset = checked_u32(terminated ? section.append_string(text) : section.append_characters(text));
                    if (!assembly.system_default_layout)
                    {
                        section.align(4);
                    }
                };

                const auto identity = assembly.keyed ? encode_identity(assembly.identity) : std::u16string{};
                if (!identity.empty())
                {
                    append_text(identity, information.encoded_identity_length, information.encoded_identity_offset,
                                !assembly.system_default_layout);
                }
                if (!assembly.manifest_path.empty())
                {
                    information.manifest_path_type = activation_context_path_type_win32_file;
                    append_text(assembly.manifest_path, information.manifest_path_length, information.manifest_path_offset, true);
                }
                if (!assembly.policy_path.empty())
                {
                    information.policy_path_type = activation_context_path_type_win32_file;
                    append_text(assembly.policy_path, information.policy_path_length, information.policy_path_offset, true);
                }
                if (!assembly.directory_name.empty())
                {
                    append_text(assembly.directory_name, information.assembly_directory_name_length,
                                information.assembly_directory_name_offset, true);
                }
                const auto& language_text = assembly.record_language.empty() ? assembly.identity.language : assembly.record_language;
                if (!language_text.empty() && language_text != "*")
                {
                    append_text(u8_to_u16(language_text), information.language_length, information.language_offset, true);
                }
                section.write(information_offset, information);
                entry.length = checked_u32(section.size() - entry.offset);
                section.align(4);
                section.write(entries_offset + index * sizeof(entry), entry);
            }

            uint32_t hash_table_offset{};
            std::vector<hashed_entry> keyed_entries{};
            for (size_t index = 0; index < entries.size(); ++index)
            {
                if (assemblies[index].keyed)
                {
                    keyed_entries.push_back({.pseudokey = entries[index].pseudokey,
                                             .entry_offset = checked_u32(entries_offset + index * sizeof(entries[index]))});
                }
            }
            if (!keyed_entries.empty())
            {
                const auto bucket_count =
                    keyed_entries.size() > 2 ? string_section_bucket_count(keyed_entries.size()) : keyed_entries.size();
                hash_table_offset = checked_u32(append_hash_table(section, keyed_entries, bucket_count));
            }

            string_section_header header{};
            header.magic = string_section_magic;
            header.header_size = sizeof(header);
            header.format_version = 1;
            header.data_format_version = 1;
            header.flags = string_section_case_insensitive;
            if (std::ranges::is_sorted(keyed_entries, {}, &hashed_entry::pseudokey))
            {
                header.flags |= string_section_entries_in_pseudokey_order;
            }
            header.element_count = checked_u32(entries.size());
            header.element_list_offset = checked_u32(entries_offset);
            header.hash_algorithm = hash_string_algorithm_x65599;
            header.search_structure_offset = hash_table_offset;
            header.user_data_offset = checked_u32(global_offset);
            header.user_data_size = global.size;
            section.write(header_offset, header);
            return {.data = section.release(), .entries = std::move(entries)};
        }

        struct dll_entry
        {
            std::u16string name{};
            uint32_t roster_index{};
            uint32_t flags{};
        };

        std::vector<std::byte> build_dll_section(const std::vector<resolved_assembly>& assemblies, const case_table& casing)
        {
            std::vector<dll_entry> dlls{};
            for (size_t index = 0; index < assemblies.size(); ++index)
            {
                const auto roster_index = checked_u32(index + 1);
                for (const auto& file : assemblies[index].files)
                {
                    dlls.push_back({.name = file, .roster_index = roster_index, .flags = dll_redirection_path_omits_assembly_root});
                    for (const auto& directory : assemblies[index].redirected_directories)
                    {
                        auto path = directory;
                        path += u'\\';
                        path += file;
                        dlls.push_back(
                            {.name = std::move(path),
                             .roster_index = roster_index,
                             .flags = dll_redirection_path_omits_assembly_root | dll_redirection_system_default_redirected_system32_dll});
                    }
                }
            }
            if (dlls.empty())
            {
                return {};
            }

            byte_buffer section{};
            const auto header_offset = section.append<string_section_header>();
            const auto entries_offset = section.append_array<string_section_entry>(dlls.size());
            std::vector<string_section_entry> elements{};
            elements.reserve(dlls.size());
            for (const auto& dll : dlls)
            {
                string_section_entry entry{};
                entry.pseudokey = hash_string(dll.name, casing);
                entry.key_offset = checked_u32(section.append_string(dll.name));
                entry.key_length = byte_length(dll.name);
                entry.assembly_roster_index = dll.roster_index;
                section.align(4);
                dll_redirection redirection{};
                redirection.size = sizeof(redirection);
                redirection.flags = dll.flags;
                entry.offset = checked_u32(section.append(redirection));
                entry.length = sizeof(dll_redirection);
                elements.push_back(entry);
            }

            const auto search = finish_string_section(section, entries_offset, std::move(elements));
            section.write(header_offset, make_string_section_header(dlls.size(), entries_offset, search));
            return section.release();
        }

        std::u16string format_assembly_version(const version& value)
        {
            std::string text{};
            for (size_t index = 0; index < value.parts.size(); ++index)
            {
                text += (index == 0 ? "" : ".") + std::to_string(value.parts[index]);
            }
            return u8_to_u16(text);
        }

        std::vector<std::byte> build_window_class_section(const std::vector<resolved_assembly>& assemblies, const case_table& casing)
        {
            struct class_source
            {
                const manifest_window_class* window_class{};
                uint32_t roster_index{};
                std::u16string version{};
            };

            std::vector<class_source> classes{};
            for (size_t index = 0; index < assemblies.size(); ++index)
            {
                const auto version = format_assembly_version(assemblies[index].identity.assembly_version);
                for (const auto& window_class : assemblies[index].window_classes)
                {
                    classes.push_back({.window_class = &window_class, .roster_index = checked_u32(index + 1), .version = version});
                }
            }
            if (classes.empty())
            {
                return {};
            }

            byte_buffer section{};
            const auto header_offset = section.append<string_section_header>();
            const auto entries_offset = section.append_array<string_section_entry>(classes.size());
            std::vector<string_section_entry> elements{};
            elements.reserve(classes.size());
            for (const auto& source : classes)
            {
                const auto& window_class = *source.window_class;
                string_section_entry entry{};
                entry.pseudokey = hash_string(window_class.name, casing);
                entry.key_offset = checked_u32(section.append_string(window_class.name));
                entry.key_length = byte_length(window_class.name);
                entry.assembly_roster_index = source.roster_index;
                section.align(4);

                const auto name = window_class.versioned ? source.version + u"!" + window_class.name : window_class.name;
                window_class_redirection redirection{};
                redirection.size = sizeof(redirection);
                redirection.name_length = byte_length(name);
                redirection.name_offset = sizeof(redirection);
                redirection.module_length = byte_length(window_class.file);
                entry.offset = checked_u32(section.append(redirection));
                section.append_string(name);
                redirection.module_offset = checked_u32(section.append_string(window_class.file));
                section.write(entry.offset, redirection);
                entry.length = checked_u32(section.size() - entry.offset);
                section.align(4);
                elements.push_back(entry);
            }

            const auto search = finish_string_section(section, entries_offset, std::move(elements));
            section.write(header_offset, make_string_section_header(classes.size(), entries_offset, search));
            return section.release();
        }

        struct guid_record
        {
            guid key{};
            std::vector<std::byte> data{};
            uint32_t roster_index{};
        };

        class guid_name_pool
        {
          public:
            explicit guid_name_pool(const std::span<const std::u16string_view> names_in_roster_order)
            {
                for (const auto name : names_in_roster_order)
                {
                    if (std::ranges::find(this->names_, name) == this->names_.end())
                    {
                        this->names_.push_back(name);
                    }
                }
                // Windows stores the names in the reverse order of their first use.
                std::ranges::reverse(this->names_);
            }

            uint32_t offset_of(const std::u16string_view name) const
            {
                size_t offset = sizeof(guid_section_header);
                for (const auto candidate : this->names_)
                {
                    if (candidate == name)
                    {
                        return checked_u32(offset);
                    }
                    offset += (candidate.size() + 1) * sizeof(char16_t);
                }
                throw std::runtime_error("Name missing from the activation context name pool");
            }

            std::vector<std::byte> bytes() const
            {
                byte_buffer buffer{};
                for (const auto name : this->names_)
                {
                    buffer.append_string(name);
                }
                return buffer.release();
            }

          private:
            std::vector<std::u16string_view> names_{};
        };

        template <typename T>
        std::vector<std::byte> serialize_record(const T& header, const std::u16string_view tail = {})
        {
            std::vector<std::byte> record(sizeof(T) + (tail.empty() ? 0 : (tail.size() + 1) * sizeof(char16_t)));
            std::memcpy(record.data(), &header, sizeof(T));
            if (!tail.empty())
            {
                std::memcpy(record.data() + sizeof(T), tail.data(), tail.size() * sizeof(char16_t));
            }
            return record;
        }

        std::vector<std::byte> build_guid_section(const std::vector<guid_record>& records, const guid_name_pool& pool)
        {
            byte_buffer section{};
            const auto header_offset = section.append<guid_section_header>();
            guid_section_header header{};
            header.magic = guid_section_magic;
            header.header_size = sizeof(header);
            header.format_version = 1;
            header.data_format_version = 1;
            if (!records.empty())
            {
                const auto pool_bytes = pool.bytes();
                if (!pool_bytes.empty())
                {
                    header.user_data_offset = checked_u32(section.size());
                    header.user_data_size = checked_u32(pool_bytes.size());
                    section.append(pool_bytes);
                    section.align(4);
                }

                header.flags = guid_section_entries_in_order;
                header.element_count = checked_u32(records.size());
                const auto entries_offset = section.append_array<guid_section_entry>(records.size());
                header.element_list_offset = checked_u32(entries_offset);

                std::vector<guid_section_entry> entries{};
                entries.reserve(records.size());
                for (const auto& record : records)
                {
                    entries.push_back({.key = record.key,
                                       .offset = checked_u32(section.size()),
                                       .length = checked_u32(record.data.size()),
                                       .assembly_roster_index = record.roster_index});
                    section.append(record.data);
                    section.align(4);
                }
                std::ranges::sort(entries, {}, &guid_section_entry::key);
                for (size_t index = 0; index < entries.size(); ++index)
                {
                    section.write(entries_offset + index * sizeof(guid_section_entry), entries[index]);
                }
            }
            section.write(header_offset, header);
            return section.release();
        }

        std::vector<std::byte> build_com_server_section(const system_default_architecture_source& architecture)
        {
            std::vector<std::u16string_view> modules{};
            modules.reserve(system_default_com_classes.size());
            for (const auto& com_class : system_default_com_classes)
            {
                modules.push_back(com_class.module);
            }
            const guid_name_pool pool{modules};

            std::vector<guid_record> records{};
            for (const auto slot : architecture.com_server_record_order)
            {
                const auto& com_class = system_default_com_classes[slot / 2];
                const auto& configured_clsid = architecture.configured_class_ids[com_class.configured_class_slot];
                com_server_redirection record{};
                record.size = sizeof(record);
                record.threading_model = com_threading_model_both;
                record.reference_clsid = com_class.clsid;
                record.configured_clsid = configured_clsid;
                record.implemented_clsid = com_class.clsid;
                record.type_library_id = com_class.type_library_id;
                record.module_length = byte_length(com_class.module);
                record.module_offset = pool.offset_of(com_class.module);
                if (!com_class.progid.empty())
                {
                    record.progid_length = byte_length(com_class.progid);
                    record.progid_offset = sizeof(record);
                }
                records.push_back({.key = slot % 2 == 0 ? configured_clsid : com_class.clsid,
                                   .data = serialize_record(record, com_class.progid),
                                   .roster_index = checked_u32(com_class.assembly_index + 1)});
            }
            return build_guid_section(records, pool);
        }

        std::vector<std::byte> build_com_interface_section()
        {
            std::vector<guid_record> records{};
            for (const auto& com_interface : system_default_com_interfaces)
            {
                com_interface_redirection record{};
                record.size = sizeof(record);
                record.iid = com_interface.iid;
                record.type_library_id = com_interface.type_library_id;
                record.name_length = byte_length(com_interface.name);
                record.name_offset = sizeof(record);
                records.push_back({.key = com_interface.iid,
                                   .data = serialize_record(record, com_interface.name),
                                   .roster_index = checked_u32(com_interface.assembly_index + 1)});
            }
            return build_guid_section(records, guid_name_pool{std::span<const std::u16string_view>{}});
        }

        std::vector<std::byte> build_com_type_library_section()
        {
            std::vector<std::u16string_view> files{};
            files.reserve(system_default_type_libraries.size());
            for (const auto& type_library : system_default_type_libraries)
            {
                files.push_back(type_library.file);
            }
            const guid_name_pool pool{files};

            std::vector<guid_record> records{};
            for (const auto& type_library : system_default_type_libraries)
            {
                com_type_library_redirection record{};
                record.size = sizeof(record);
                record.name_length = byte_length(type_library.file) + sizeof(char16_t);
                record.name_offset = pool.offset_of(type_library.file);
                record.resource_id = 1;
                record.major_version = 1;
                records.push_back({.key = type_library.type_library_id,
                                   .data = serialize_record(record),
                                   .roster_index = checked_u32(type_library.assembly_index + 1)});
            }
            std::ranges::sort(records, {}, &guid_record::key);
            return build_guid_section(records, pool);
        }

        std::vector<std::byte> build_com_progid_section(const system_default_architecture_source& architecture, const case_table& casing)
        {
            struct progid_source
            {
                std::u16string_view name;
                const guid* configured_clsid;
                uint32_t roster_index;
            };

            std::vector<progid_source> progids{};
            for (const auto& com_class : system_default_com_classes)
            {
                const auto roster_index = checked_u32(com_class.assembly_index + 1);
                const auto* configured_clsid = &architecture.configured_class_ids[com_class.configured_class_slot];
                for (const auto name : {com_class.progid, com_class.version_independent_progid})
                {
                    if (!name.empty())
                    {
                        progids.push_back({.name = name, .configured_clsid = configured_clsid, .roster_index = roster_index});
                    }
                }
            }
            if (progids.empty())
            {
                return {};
            }

            byte_buffer section{};
            const auto header_offset = section.append<string_section_header>();
            const auto user_data_offset = section.size();
            std::vector<const guid*> clsids{};
            for (const auto& progid : progids)
            {
                if (std::ranges::find(clsids, progid.configured_clsid) == clsids.end())
                {
                    clsids.push_back(progid.configured_clsid);
                    section.append(*progid.configured_clsid);
                }
            }
            const auto user_data_size = section.size() - user_data_offset;
            const auto entries_offset = section.append_array<string_section_entry>(progids.size());

            std::vector<string_section_entry> elements{};
            elements.reserve(progids.size());
            for (const auto& progid : progids)
            {
                string_section_entry entry{};
                entry.pseudokey = hash_string(progid.name, casing);
                entry.key_offset = checked_u32(section.append_string(progid.name));
                entry.key_length = byte_length(progid.name);
                entry.assembly_roster_index = progid.roster_index;
                section.align(4);
                const auto clsid_slot = std::ranges::find(clsids, progid.configured_clsid) - clsids.begin();
                com_progid_redirection record{};
                record.size = sizeof(record);
                record.clsid_offset = checked_u32(user_data_offset + static_cast<size_t>(clsid_slot) * sizeof(guid));
                entry.offset = checked_u32(section.append(record));
                entry.length = sizeof(record);
                elements.push_back(entry);
            }

            const auto search = finish_string_section(section, entries_offset, std::move(elements));
            auto header = make_string_section_header(progids.size(), entries_offset, search);
            header.user_data_offset = checked_u32(user_data_offset);
            header.user_data_size = checked_u32(user_data_size);
            section.write(header_offset, header);
            return section.release();
        }

        struct blob_section
        {
            uint32_t id{};
            uint32_t format{};
            std::vector<std::byte> data{};
        };

        std::vector<std::byte> build_blob(const std::vector<resolved_assembly>& assemblies, const std::u16string_view application_directory,
                                          const case_table& casing, std::vector<blob_section> extra_sections = {})
        {
            auto assembly_section = build_assembly_section(assemblies, application_directory);
            std::vector<blob_section> sections{};
            sections.push_back({.id = activation_context_section_assembly_information,
                                .format = activation_context_section_format_string_table,
                                .data = std::move(assembly_section.data)});
            if (auto dll_section = build_dll_section(assemblies, casing); !dll_section.empty())
            {
                sections.push_back({.id = activation_context_section_dll_redirection,
                                    .format = activation_context_section_format_string_table,
                                    .data = std::move(dll_section)});
            }
            if (auto window_class_section = build_window_class_section(assemblies, casing); !window_class_section.empty())
            {
                sections.push_back({.id = activation_context_section_window_class_redirection,
                                    .format = activation_context_section_format_string_table,
                                    .data = std::move(window_class_section)});
            }
            for (auto& extra_section : extra_sections)
            {
                sections.push_back(std::move(extra_section));
            }

            byte_buffer result{};
            const auto header_offset = result.append<activation_context_data>();
            const auto roster_header_offset = result.append<assembly_roster_header>();
            const auto roster_entries_offset = result.append_array<assembly_roster_entry>(assemblies.size() + 1);
            const auto toc_header_offset = result.append<activation_context_toc_header>();
            const auto toc_entries_offset = result.append_array<activation_context_toc_entry>(sections.size());
            result.align(4);

            std::vector<size_t> section_offsets{};
            for (const auto& section : sections)
            {
                section_offsets.push_back(result.size());
                result.append(section.data);
            }
            const auto assembly_section_offset = section_offsets.front();

            assembly_roster_entry invalid_roster_entry{};
            invalid_roster_entry.flags = roster_entry_invalid;
            result.write(roster_entries_offset, invalid_roster_entry);
            for (size_t index = 0; index < assemblies.size(); ++index)
            {
                const auto& source = assembly_section.entries[index];
                assembly_roster_entry roster{};
                roster.flags = index == 0 ? roster_entry_root : 0;
                roster.pseudokey = source.pseudokey;
                roster.assembly_name_offset = source.key_offset ? checked_u32(assembly_section_offset + source.key_offset) : 0;
                roster.assembly_name_length = source.key_length;
                roster.assembly_information_offset = checked_u32(assembly_section_offset + source.offset);
                roster.assembly_information_length = source.length;
                result.write(roster_entries_offset + (index + 1) * sizeof(roster), roster);
            }

            result.write(roster_header_offset,
                         assembly_roster_header{.header_size = sizeof(assembly_roster_header),
                                                .hash_algorithm = hash_string_algorithm_x65599,
                                                .entry_count = checked_u32(assemblies.size() + 1),
                                                .first_entry_offset = checked_u32(roster_entries_offset),
                                                .assembly_information_section_offset = checked_u32(assembly_section_offset)});
            result.write(toc_header_offset, activation_context_toc_header{.header_size = sizeof(activation_context_toc_header),
                                                                          .entry_count = checked_u32(sections.size()),
                                                                          .first_entry_offset = checked_u32(toc_entries_offset),
                                                                          .flags = 2});
            for (size_t index = 0; index < sections.size(); ++index)
            {
                result.write(toc_entries_offset + index * sizeof(activation_context_toc_entry),
                             activation_context_toc_entry{.id = sections[index].id,
                                                          .offset = checked_u32(section_offsets[index]),
                                                          .length = checked_u32(sections[index].data.size()),
                                                          .format = sections[index].format});
            }
            activation_context_data header{};
            header.magic = activation_context_magic;
            header.header_size = sizeof(header);
            header.format_version = 1;
            header.total_size = checked_u32(result.size());
            header.default_toc_offset = checked_u32(toc_header_offset);
            header.assembly_roster_offset = checked_u32(roster_header_offset);
            result.write(header_offset, header);
            return result.release();
        }

        std::vector<std::byte> build_activation_context_from_documents(const file_system& file_system, const mapped_module& executable,
                                                                       const source_document& manifest_source,
                                                                       const std::optional<source_document>& config,
                                                                       const windows_path& system_root,
                                                                       const std::string_view preferred_language)
        {
            auto manifest = parse_manifest(manifest_source.contents);
            const auto winsxs_path = system_root / windows_path(u"WinSxS");
            const auto manifests_path = winsxs_path / windows_path(u"Manifests");
            const auto casing = read_case_table(file_system, system_root);
            manifest_directory manifests{};

            resolved_assembly root{};
            if (manifest.identity)
            {
                root.identity = *manifest.identity;
            }
            root.manifest_path = manifest_source.path;
            if (config)
            {
                root.policy_path = config->path;
            }
            root.files = std::move(manifest.files);
            root.window_classes = std::move(manifest.window_classes);
            root.flags = assembly_information_root | assembly_information_private;
            root.keyed = false;

            std::vector<resolved_assembly> assemblies{};
            assemblies.push_back(std::move(root));
            std::deque<assembly_dependency> pending_dependencies(manifest.dependencies.begin(), manifest.dependencies.end());
            while (!pending_dependencies.empty())
            {
                auto dependency = std::move(pending_dependencies.front());
                pending_dependencies.pop_front();
                auto& identity = dependency.identity;
                if (identity.processor_architecture.empty() || identity.processor_architecture == "*")
                {
                    identity.processor_architecture = architecture_name(identity, executable);
                }

                if (!config || allows_publisher_policy(config->contents, identity))
                {
                    apply_system_policy(file_system, manifests_path, identity, executable, manifests);
                }
                if (config)
                {
                    apply_binding_redirects(config->contents, identity);
                }

                auto resolved = resolve_private_assembly(file_system, executable, identity);
                if (!resolved)
                {
                    resolved = resolve_winsxs_assembly(file_system, manifests_path, winsxs_path, identity, executable, preferred_language,
                                                       manifests);
                }
                if (!resolved)
                {
                    if (dependency.optional)
                    {
                        continue;
                    }
                    throw std::runtime_error("Unable to resolve side-by-side assembly: " + identity.name +
                                             ", version=" + identity.version_text);
                }
                if (std::ranges::any_of(assemblies.begin() + 1, assemblies.end(), [&](const resolved_assembly& assembly) {
                        return identity_matches(assembly.identity, resolved->identity, true) ||
                               (!assembly.manifest_path.empty() && assembly.manifest_path == resolved->manifest_path);
                    }))
                {
                    continue;
                }
                for (auto& nested_dependency : resolved->dependencies)
                {
                    pending_dependencies.push_back(std::move(nested_dependency));
                }
                resolved->pseudokey = hash_string(u8_to_u16(resolved->identity.name), casing);
                assemblies.push_back(std::move(*resolved));
            }

            if (assemblies.size() > 2)
            {
                std::ranges::stable_sort(assemblies.begin() + 1, assemblies.end(), {}, &resolved_assembly::pseudokey);
            }

            auto application_directory = executable.module_path.parent().u16string();
            if (!application_directory.empty() && application_directory.back() != u'\\')
            {
                application_directory.push_back(u'\\');
            }
            return build_blob(assemblies, application_directory, casing);
        }
    }

    std::vector<std::byte> obtain_system_default_activation_context(const std::filesystem::path& emulation_root,
                                                                    const file_system& file_system, const windows_path& system_root,
                                                                    const system_default_architecture architecture)
    {
        if (!emulation_root.empty())
        {
            const auto file_name =
                architecture == system_default_architecture::amd64 ? "system-default-amd64.bin" : "system-default-x86.bin";
            auto captured = utils::io::read_file(emulation_root / "sxs" / file_name);
            constexpr size_t total_size_offset = 12;
            if (captured.size() > total_size_offset + sizeof(uint32_t))
            {
                uint32_t magic{};
                uint32_t total_size{};
                std::memcpy(&magic, captured.data(), sizeof(magic));
                std::memcpy(&total_size, captured.data() + total_size_offset, sizeof(total_size));
                if (magic == activation_context_magic && total_size == captured.size())
                {
                    return captured;
                }
            }
        }

        return build_system_default_activation_context(file_system, system_root, architecture);
    }

    std::vector<std::byte> build_process_activation_context(const file_system& file_system, const mapped_module& executable,
                                                            const windows_path& system_root, const std::string_view preferred_language)
    {
        const auto manifest = read_application_manifest(file_system, executable);
        if (!manifest)
        {
            return {};
        }
        return build_activation_context_from_documents(file_system, executable, *manifest, read_application_config(file_system, executable),
                                                       system_root, preferred_language);
    }

    std::vector<std::byte> build_system_default_activation_context(const file_system& file_system, const windows_path& system_root,
                                                                   const system_default_architecture architecture)
    {
        const auto& source = system_default_architectures[architecture == system_default_architecture::amd64 ? 0 : 1];
        const case_table casing{};
        const auto root = spelled_path(system_root);
        const auto winsxs = root + u"\\WinSxS";

        std::vector<resolved_assembly> assemblies{};
        for (size_t index = 0; index < system_default_assembly_names.size(); ++index)
        {
            const auto& name = system_default_assembly_names[index];
            const auto& assembly_source = source.assemblies[index];

            resolved_assembly assembly{};
            assembly.identity.name = name.name;
            assembly.identity.processor_architecture = source.processor_architecture;
            assembly.identity.public_key_token = "6595b64144ccf1df";
            assembly.identity.type = "win32";
            assembly.identity.language = name.language;
            assembly.identity.version_text = assembly_source.version;
            assembly.record_language = name.record_language;
            assembly.directory_name = assembly_source.directory;
            assembly.manifest_path = index == 0 ? u"System Default Context"
                                                : winsxs + u"\\manifests\\" + std::u16string{assembly_source.directory} + u".manifest";
            assembly.manifest_last_write_time = assembly_source.manifest_last_write_time;
            assembly.pseudokey = hash_string(u8_to_u16(assembly.identity.name), casing);
            assembly.flags = index == 0 ? assembly_information_root : 0;
            assembly.system_default_layout = true;

            const auto assembly_directory =
                file_system.stat(system_root / windows_path(u"WinSxS") / windows_path(assembly_source.directory));
            if (assembly_directory && (assembly_directory->st_mode & S_IFMT) == S_IFDIR)
            {
                assembly.files.assign(name.files.begin(), name.files.end());
                assembly.redirected_directories.push_back(root + u"\\system32");
                if (source.has_wow64_directory)
                {
                    assembly.redirected_directories.push_back(root + u"\\SysWOW64");
                }
            }
            assemblies.push_back(std::move(assembly));
        }

        auto& common_controls = assemblies[system_default_common_controls_index];
        for (const auto* const name : system_default_window_class_names)
        {
            common_controls.window_classes.push_back({.name = name, .file = u"comctl32.dll", .versioned = false});
        }

        std::vector<blob_section> com_sections{};
        com_sections.push_back({.id = activation_context_section_com_server_redirection,
                                .format = activation_context_section_format_guid_table,
                                .data = build_com_server_section(source)});
        com_sections.push_back({.id = activation_context_section_com_interface_redirection,
                                .format = activation_context_section_format_guid_table,
                                .data = build_com_interface_section()});
        com_sections.push_back({.id = activation_context_section_com_type_library_redirection,
                                .format = activation_context_section_format_guid_table,
                                .data = build_com_type_library_section()});
        com_sections.push_back({.id = activation_context_section_com_progid_redirection,
                                .format = activation_context_section_format_string_table,
                                .data = build_com_progid_section(source, casing)});
        com_sections.push_back({.id = activation_context_section_clr_surrogates,
                                .format = activation_context_section_format_guid_table,
                                .data = build_guid_section({}, guid_name_pool{std::span<const std::u16string_view>{}})});
        com_sections.push_back({.id = activation_context_section_compatibility_information,
                                .format = activation_context_section_format_string_table,
                                .data = std::vector<std::byte>(8)});
        return build_blob(assemblies, root + u"\\WinSxs\\", casing, std::move(com_sections));
    }

    std::vector<std::byte> build_activation_context(const file_system& file_system, const mapped_module& executable,
                                                    const activation_context_source& manifest,
                                                    const std::optional<activation_context_source>& config, const windows_path& system_root,
                                                    const std::string_view preferred_language)
    {
        const source_document manifest_document{.contents = decode_manifest_text(manifest.contents), .path = manifest.path.u16string()};
        std::optional<source_document> config_document{};
        if (config)
        {
            config_document = source_document{.contents = decode_manifest_text(config->contents), .path = config->path.u16string()};
        }
        return build_activation_context_from_documents(file_system, executable, manifest_document, config_document, system_root,
                                                       preferred_language);
    }
}
