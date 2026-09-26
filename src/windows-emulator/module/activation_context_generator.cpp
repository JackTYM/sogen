#include "../std_include.hpp"
#include "activation_context_format.hpp"
#include "activation_context_generator.hpp"

#include <cctype>

namespace sogen
{
    namespace
    {
        std::vector<std::uint8_t> to_utf16le(const std::string& utf8)
        {
            std::vector<std::uint8_t> result{};
            result.reserve(utf8.size() * 2);
            for (const char c : utf8)
            {
                result.push_back(static_cast<std::uint8_t>(c));
                result.push_back(0);
            }
            return result;
        }

        // The real Fusion/SxS pseudo-key hash - confirmed byte-exact against every real
        // PseudoKey value in the golden fixture (both assembly-identity keys and DLL-name
        // keys): the classic RtlHashUnicodeString x65599 case-insensitive hash, computed over
        // UTF-16 code units.
        std::uint32_t compute_pseudo_key(const std::string& utf8)
        {
            std::uint32_t hash = 0;
            for (const char c : utf8)
            {
                const auto upper = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c)));
                hash = hash * 65599u + upper;
            }
            return hash;
        }

        std::string encode_assembly_identity(const std::string& name, const std::string& processor_architecture,
                                             const std::string& public_key_token, const std::string& version)
        {
            std::string result = name;
            if (!processor_architecture.empty())
            {
                result += ",processorArchitecture=\"" + processor_architecture + "\"";
            }
            if (!public_key_token.empty())
            {
                result += ",publicKeyToken=\"" + public_key_token + "\"";
            }
            result += ",type=\"win32\"";
            if (!version.empty())
            {
                result += ",version=\"" + version + "\"";
            }
            return result;
        }

        void append(std::vector<std::uint8_t>& buffer, const void* data, const std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            buffer.insert(buffer.end(), bytes, bytes + size);
        }

        template <typename T>
        void append(std::vector<std::uint8_t>& buffer, const T& value)
        {
            append(buffer, &value, sizeof(value));
        }

        struct assembly_info_piece
        {
            std::string key_name; // empty for the ROOT assembly
            activation_context_data_assembly_information info{};
            std::string encoded_identity;
            std::string manifest_path;
            std::string directory_name;
        };

        // Builds the id=1 (assembly information) string section, self-contained: every offset
        // inside is relative to this section's own header, matching the real format. Returns
        // the section bytes plus, per piece (in the same order as `pieces`), the key/value
        // offsets and lengths relative to this section - the caller needs those to build the
        // assembly roster's blob-relative pointers afterwards.
        struct built_section
        {
            std::vector<std::uint8_t> bytes;
            struct element
            {
                std::uint32_t key_offset;
                std::uint32_t key_length;
                std::uint32_t value_offset;
                std::uint32_t value_length;
            };
            std::vector<element> elements;
        };

        built_section build_assembly_information_section(const std::vector<assembly_info_piece>& pieces)
        {
            built_section result{};

            const auto element_count = static_cast<std::uint32_t>(pieces.size());
            const std::uint32_t header_size = sizeof(activation_context_string_section_header);
            const std::uint32_t element_list_offset = header_size;
            const std::uint32_t element_list_size = element_count * sizeof(activation_context_string_section_entry);

            std::vector<std::uint8_t> tail{};
            std::vector<built_section::element> elements{};

            for (const auto& piece : pieces)
            {
                std::uint32_t key_offset = 0;
                std::uint32_t key_length = 0;
                if (!piece.key_name.empty())
                {
                    const auto key_bytes = to_utf16le(piece.key_name);
                    key_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());
                    key_length = static_cast<std::uint32_t>(key_bytes.size());
                    tail.insert(tail.end(), key_bytes.begin(), key_bytes.end());
                }

                const auto value_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());

                const auto encoded_identity_bytes = to_utf16le(piece.encoded_identity);
                const auto manifest_path_bytes = to_utf16le(piece.manifest_path);
                const auto directory_name_bytes = to_utf16le(piece.directory_name);

                auto info = piece.info;
                info.encoded_assembly_identity_offset = value_offset + sizeof(info);
                info.encoded_assembly_identity_length = static_cast<std::uint32_t>(encoded_identity_bytes.size());
                info.manifest_path_offset = info.encoded_assembly_identity_offset + info.encoded_assembly_identity_length;
                info.manifest_path_length = static_cast<std::uint32_t>(manifest_path_bytes.size());
                info.assembly_directory_name_offset = info.manifest_path_offset + info.manifest_path_length;
                info.assembly_directory_name_length = static_cast<std::uint32_t>(directory_name_bytes.size());

                append(tail, info);
                tail.insert(tail.end(), encoded_identity_bytes.begin(), encoded_identity_bytes.end());
                tail.insert(tail.end(), manifest_path_bytes.begin(), manifest_path_bytes.end());
                tail.insert(tail.end(), directory_name_bytes.begin(), directory_name_bytes.end());

                const auto value_length = static_cast<std::uint32_t>(sizeof(info) + encoded_identity_bytes.size() +
                                                                     manifest_path_bytes.size() + directory_name_bytes.size());

                elements.push_back(
                    {.key_offset = key_offset, .key_length = key_length, .value_offset = value_offset, .value_length = value_length});
            }

            activation_context_string_section_header header{};
            header.magic = activation_context_string_section_magic;
            header.header_size = header_size;
            header.format_version = 1;
            header.data_format_version = 1;
            header.flags = 1;
            header.element_count = element_count;
            header.element_list_offset = element_list_offset;
            header.hash_algorithm = 1;
            header.search_structure_offset = 0; // no search structure - assembly info is reached via the roster's
                                                // direct pointers, never a by-name lookup into this section, for
                                                // the DLL-redirection flow this generator implements
            header.user_data_offset = 0;
            header.user_data_size = 0;

            append(result.bytes, header);

            for (std::size_t i = 0; i < pieces.size(); ++i)
            {
                activation_context_string_section_entry entry{};
                entry.pseudo_key = pieces[i].key_name.empty() ? 0 : compute_pseudo_key(pieces[i].key_name);
                entry.key_offset = elements[i].key_offset;
                entry.key_length = elements[i].key_length;
                entry.value_offset = elements[i].value_offset;
                entry.value_length = elements[i].value_length;
                entry.assembly_roster_index = static_cast<std::uint32_t>(i + 1);
                append(result.bytes, entry);
            }

            result.bytes.insert(result.bytes.end(), tail.begin(), tail.end());
            result.elements = std::move(elements);
            return result;
        }

        struct dll_redirection_piece
        {
            std::string dll_name;
            std::uint32_t assembly_roster_index;
        };

        built_section build_dll_redirection_section(const std::vector<dll_redirection_piece>& pieces)
        {
            built_section result{};

            const auto element_count = static_cast<std::uint32_t>(pieces.size());
            const std::uint32_t header_size = sizeof(activation_context_string_section_header);
            const std::uint32_t element_list_offset = header_size;
            const std::uint32_t element_list_size = element_count * sizeof(activation_context_string_section_entry);

            std::vector<std::uint8_t> tail{};
            std::vector<built_section::element> elements{};

            for (const auto& piece : pieces)
            {
                const auto key_bytes = to_utf16le(piece.dll_name);
                const auto key_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());
                const auto key_length = static_cast<std::uint32_t>(key_bytes.size());
                tail.insert(tail.end(), key_bytes.begin(), key_bytes.end());

                const auto value_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());

                activation_context_data_dll_redirection redirection{};
                redirection.size = sizeof(redirection);
                redirection.flags = 2; // default-redirected: <assembly directory>\<this DLL's own file name>, no
                                       // explicit path segments needed
                redirection.total_path_length = 0;
                redirection.path_segment_count = 0;
                redirection.path_segment_offset = 0;
                append(tail, redirection);

                const auto value_length = static_cast<std::uint32_t>(sizeof(redirection));

                elements.push_back(
                    {.key_offset = key_offset, .key_length = key_length, .value_offset = value_offset, .value_length = value_length});
            }

            activation_context_string_section_header header{};
            header.magic = activation_context_string_section_magic;
            header.header_size = header_size;
            header.format_version = 1;
            header.data_format_version = 1;
            header.flags = 3;
            header.element_count = element_count;
            header.element_list_offset = element_list_offset;
            header.hash_algorithm = 1;
            header.search_structure_offset = 0; // confirmed 0 (no search structure - linear scan) in the golden
                                                // fixture's own real DLL-redirection section
            header.user_data_offset = 0;
            header.user_data_size = 0;

            append(result.bytes, header);

            for (std::size_t i = 0; i < pieces.size(); ++i)
            {
                activation_context_string_section_entry entry{};
                entry.pseudo_key = compute_pseudo_key(pieces[i].dll_name);
                entry.key_offset = elements[i].key_offset;
                entry.key_length = elements[i].key_length;
                entry.value_offset = elements[i].value_offset;
                entry.value_length = elements[i].value_length;
                entry.assembly_roster_index = pieces[i].assembly_roster_index;
                append(result.bytes, entry);
            }

            result.bytes.insert(result.bytes.end(), tail.begin(), tail.end());
            result.elements = std::move(elements);
            return result;
        }

        struct window_class_redirection_piece
        {
            std::string class_name;
            std::string version_specific_class_name;
            std::string dll_name;
            std::uint32_t assembly_roster_index;
        };

        built_section build_window_class_redirection_section(const std::vector<window_class_redirection_piece>& pieces)
        {
            built_section result{};

            const auto element_count = static_cast<std::uint32_t>(pieces.size());
            const std::uint32_t header_size = sizeof(activation_context_string_section_header);
            const std::uint32_t element_list_offset = header_size;
            const std::uint32_t element_list_size = element_count * sizeof(activation_context_string_section_entry);

            std::vector<std::uint8_t> tail{};
            std::vector<built_section::element> elements{};

            for (const auto& piece : pieces)
            {
                const auto key_bytes = to_utf16le(piece.class_name);
                const auto key_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());
                const auto key_length = static_cast<std::uint32_t>(key_bytes.size());
                tail.insert(tail.end(), key_bytes.begin(), key_bytes.end());

                const auto value_offset = element_list_offset + element_list_size + static_cast<std::uint32_t>(tail.size());

                const auto version_name_bytes = to_utf16le(piece.version_specific_class_name);
                const auto dll_name_bytes = to_utf16le(piece.dll_name);

                activation_context_data_window_class_redirection redirection{};
                redirection.size = sizeof(redirection);
                redirection.flags = 0;
                redirection.version_specific_class_name_length = static_cast<std::uint32_t>(version_name_bytes.size());
                redirection.version_specific_class_name_offset = sizeof(redirection); // right after this leaf struct
                redirection.dll_name_length = static_cast<std::uint32_t>(dll_name_bytes.size());
                redirection.dll_name_offset = value_offset + sizeof(redirection) + redirection.version_specific_class_name_length;

                append(tail, redirection);
                tail.insert(tail.end(), version_name_bytes.begin(), version_name_bytes.end());
                tail.insert(tail.end(), dll_name_bytes.begin(), dll_name_bytes.end());

                const auto value_length =
                    static_cast<std::uint32_t>(sizeof(redirection) + version_name_bytes.size() + dll_name_bytes.size());

                elements.push_back(
                    {.key_offset = key_offset, .key_length = key_length, .value_offset = value_offset, .value_length = value_length});
            }

            activation_context_string_section_header header{};
            header.magic = activation_context_string_section_magic;
            header.header_size = header_size;
            header.format_version = 1;
            header.data_format_version = 1;
            header.flags = 1;
            header.element_count = element_count;
            header.element_list_offset = element_list_offset;
            header.hash_algorithm = 1;
            header.search_structure_offset = 0; // this generator doesn't build the optional hash search
                                                // structure the real format supports for this section
                                                // (confirmed present in the golden fixture) - real ntdll
                                                // falls back to a linear scan when this is 0, same as the
                                                // DLL-redirection section above
            header.user_data_offset = 0;
            header.user_data_size = 0;

            append(result.bytes, header);

            for (std::size_t i = 0; i < pieces.size(); ++i)
            {
                activation_context_string_section_entry entry{};
                entry.pseudo_key = compute_pseudo_key(pieces[i].class_name);
                entry.key_offset = elements[i].key_offset;
                entry.key_length = elements[i].key_length;
                entry.value_offset = elements[i].value_offset;
                entry.value_length = elements[i].value_length;
                entry.assembly_roster_index = pieces[i].assembly_roster_index;
                append(result.bytes, entry);
            }

            result.bytes.insert(result.bytes.end(), tail.begin(), tail.end());
            result.elements = std::move(elements);
            return result;
        }
    }

    std::vector<std::uint8_t> generate_activation_context_blob(const root_assembly_info& root,
                                                               const std::vector<resolved_assembly>& assemblies)
    {
        std::vector<assembly_info_piece> info_pieces{};

        assembly_info_piece root_piece{};
        root_piece.info.size = sizeof(activation_context_data_assembly_information);
        root_piece.info.flags = 0x11;
        root_piece.info.manifest_path_type = 2;
        root_piece.info.policy_path_type = 1;
        root_piece.info.manifest_version_major = 1;
        root_piece.encoded_identity = encode_assembly_identity(root.name, root.processor_architecture, "", root.version);
        root_piece.manifest_path = root.exe_path;
        info_pieces.push_back(std::move(root_piece));

        std::vector<dll_redirection_piece> dll_pieces{};
        std::vector<window_class_redirection_piece> window_class_pieces{};

        for (std::size_t i = 0; i < assemblies.size(); ++i)
        {
            const auto& resolved = assemblies[i];
            const auto roster_index = static_cast<std::uint32_t>(i + 2); // 1-based; entry 1 is the ROOT

            assembly_info_piece piece{};
            piece.key_name = resolved.identity.name;
            piece.info.size = sizeof(activation_context_data_assembly_information);
            piece.info.flags = 0;
            piece.info.manifest_path_type = 2;
            piece.info.policy_path_type = 1;
            piece.info.manifest_version_major = 1;
            piece.encoded_identity = encode_assembly_identity(resolved.identity.name, resolved.identity.processor_architecture,
                                                              resolved.identity.public_key_token, resolved.identity.version);
            piece.manifest_path = resolved.manifest_path;
            piece.directory_name = resolved.concrete_directory_name;
            info_pieces.push_back(std::move(piece));

            for (const auto& dll_name : resolved.redirected_dlls)
            {
                dll_pieces.push_back({.dll_name = dll_name, .assembly_roster_index = roster_index});
            }

            // <windowClass> entries are nested inside a specific <file> in the real manifest
            // schema; this generator doesn't track that per-file association and instead
            // attributes every window class to the assembly's first redirected DLL, which
            // matches every real-world case seen so far (Common-Controls has exactly one
            // <file>, comctl32.dll, with all of its window classes nested inside it).
            if (!resolved.window_classes.empty() && !resolved.redirected_dlls.empty())
            {
                const auto& owning_dll = resolved.redirected_dlls.front();
                for (const auto& class_name : resolved.window_classes)
                {
                    window_class_pieces.push_back({
                        .class_name = class_name,
                        .version_specific_class_name = resolved.resolved_version + "!" + class_name,
                        .dll_name = owning_dll,
                        .assembly_roster_index = roster_index,
                    });
                }
            }
        }

        const auto assembly_info_section = build_assembly_information_section(info_pieces);
        const auto dll_redirection_section = build_dll_redirection_section(dll_pieces);
        const auto window_class_section = build_window_class_redirection_section(window_class_pieces);
        const auto has_window_class_section = !window_class_pieces.empty();

        const auto roster_entry_count = static_cast<std::uint32_t>(2 + assemblies.size()); // reserved + ROOT + dependents

        constexpr std::uint32_t header_size = sizeof(activation_context_data_header);
        constexpr std::uint32_t toc_header_size = sizeof(activation_context_data_toc_header);
        const std::uint32_t toc_entry_count = has_window_class_section ? 3 : 2;
        const std::uint32_t toc_size = toc_header_size + toc_entry_count * sizeof(activation_context_data_toc_entry);
        const std::uint32_t roster_header_size = sizeof(activation_context_data_assembly_roster_header);
        const std::uint32_t roster_entries_size = roster_entry_count * sizeof(activation_context_data_assembly_roster_entry);

        const std::uint32_t default_toc_offset = header_size;
        const std::uint32_t assembly_roster_offset = default_toc_offset + toc_size;
        const std::uint32_t assembly_information_section_start = assembly_roster_offset + roster_header_size + roster_entries_size;
        const std::uint32_t dll_redirection_section_start =
            assembly_information_section_start + static_cast<std::uint32_t>(assembly_info_section.bytes.size());
        const std::uint32_t window_class_section_start =
            dll_redirection_section_start + static_cast<std::uint32_t>(dll_redirection_section.bytes.size());
        const std::uint32_t total_size =
            window_class_section_start + (has_window_class_section ? static_cast<std::uint32_t>(window_class_section.bytes.size()) : 0);

        std::vector<std::uint8_t> blob{};
        blob.reserve(total_size);

        activation_context_data_header header{};
        header.magic = activation_context_data_magic;
        header.header_size = header_size;
        header.format_version = activation_context_data_format_version;
        header.total_size = total_size;
        header.default_toc_offset = default_toc_offset;
        header.extended_toc_offset = 0;
        header.assembly_roster_offset = assembly_roster_offset;
        header.flags = 0;
        append(blob, header);

        activation_context_data_toc_header toc_header{};
        toc_header.header_size = toc_header_size;
        toc_header.entry_count = toc_entry_count;
        toc_header.first_entry_offset = default_toc_offset + toc_header_size;
        toc_header.flags = 2;
        append(blob, toc_header);

        // TOC.offset is read by real ntdll's RtlpLocateActivationContextSection as an exact,
        // unbiased offset from the blob base (confirmed via disassembly of a real ntdll.dll -
        // see the format doc's TOC-offset section). The roster fields below use the same
        // zero-bias convention.
        activation_context_data_toc_entry assembly_info_toc_entry{};
        assembly_info_toc_entry.id = activation_context_section_id_assembly_information;
        assembly_info_toc_entry.offset = assembly_information_section_start;
        assembly_info_toc_entry.length = static_cast<std::uint32_t>(assembly_info_section.bytes.size());
        assembly_info_toc_entry.format = activation_context_section_format_string_table;
        append(blob, assembly_info_toc_entry);

        activation_context_data_toc_entry dll_redirection_toc_entry{};
        dll_redirection_toc_entry.id = activation_context_section_id_dll_redirection;
        dll_redirection_toc_entry.offset = dll_redirection_section_start;
        dll_redirection_toc_entry.length = static_cast<std::uint32_t>(dll_redirection_section.bytes.size());
        dll_redirection_toc_entry.format = activation_context_section_format_string_table;
        append(blob, dll_redirection_toc_entry);

        if (has_window_class_section)
        {
            activation_context_data_toc_entry window_class_toc_entry{};
            window_class_toc_entry.id = activation_context_section_id_window_class_redirection;
            window_class_toc_entry.offset = window_class_section_start;
            window_class_toc_entry.length = static_cast<std::uint32_t>(window_class_section.bytes.size());
            window_class_toc_entry.format = activation_context_section_format_string_table;
            append(blob, window_class_toc_entry);
        }

        activation_context_data_assembly_roster_header roster_header{};
        roster_header.header_size = roster_header_size;
        roster_header.hash_algorithm = 1;
        roster_header.entry_count = roster_entry_count;
        roster_header.first_entry_offset = assembly_roster_offset + roster_header_size;
        roster_header.assembly_information_section_offset = assembly_information_section_start;
        append(blob, roster_header);

        activation_context_data_assembly_roster_entry reserved_entry{};
        reserved_entry.flags = 1;
        append(blob, reserved_entry);

        activation_context_data_assembly_roster_entry root_entry{};
        root_entry.flags = 2;
        root_entry.assembly_information_offset = assembly_information_section_start + assembly_info_section.elements[0].value_offset;
        root_entry.assembly_information_length = assembly_info_section.elements[0].value_length;
        append(blob, root_entry);

        for (std::size_t i = 0; i < assemblies.size(); ++i)
        {
            const auto& element = assembly_info_section.elements[i + 1];

            activation_context_data_assembly_roster_entry entry{};
            entry.flags = 0;
            entry.pseudo_key = compute_pseudo_key(assemblies[i].identity.name);
            entry.assembly_name_offset = assembly_information_section_start + element.key_offset;
            entry.assembly_name_length = element.key_length;
            entry.assembly_information_offset = assembly_information_section_start + element.value_offset;
            entry.assembly_information_length = element.value_length;
            append(blob, entry);
        }

        blob.insert(blob.end(), assembly_info_section.bytes.begin(), assembly_info_section.bytes.end());
        blob.insert(blob.end(), dll_redirection_section.bytes.begin(), dll_redirection_section.bytes.end());
        if (has_window_class_section)
        {
            blob.insert(blob.end(), window_class_section.bytes.begin(), window_class_section.bytes.end());
        }

        return blob;
    }
}
