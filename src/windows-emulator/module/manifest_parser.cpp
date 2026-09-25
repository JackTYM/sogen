#include "../std_include.hpp"
#include "manifest_parser.hpp"

#include <memory_interface.hpp>
#include <platform/win_pefile.hpp>

namespace sogen
{
    namespace
    {
        std::optional<std::string> extract_attribute(std::string_view tag, std::string_view attr_name)
        {
            const auto needle = std::string(attr_name) + "=\"";
            const auto pos = tag.find(needle);
            if (pos == std::string_view::npos)
            {
                return std::nullopt;
            }

            const auto value_start = pos + needle.size();
            const auto value_end = tag.find('"', value_start);
            if (value_end == std::string_view::npos)
            {
                return std::nullopt;
            }

            return std::string(tag.substr(value_start, value_end - value_start));
        }

        constexpr std::uint32_t k_resource_type_manifest =
            24; // RT_MANIFEST - named to avoid colliding with the real winuser.h macro of the same name on Windows builds
        constexpr std::uint32_t RESOURCE_DATA_IS_DIRECTORY = 0x80000000;

        struct IMAGE_RESOURCE_DIRECTORY
        {
            std::uint32_t Characteristics;
            std::uint32_t TimeDateStamp;
            std::uint16_t MajorVersion;
            std::uint16_t MinorVersion;
            std::uint16_t NumberOfNamedEntries;
            std::uint16_t NumberOfIdEntries;
        };

        struct IMAGE_RESOURCE_DIRECTORY_ENTRY
        {
            std::uint32_t Name;
            std::uint32_t OffsetToData;
        };

        struct IMAGE_RESOURCE_DATA_ENTRY
        {
            std::uint32_t OffsetToData;
            std::uint32_t Size;
            std::uint32_t CodePage;
            std::uint32_t Reserved;
        };

        std::optional<std::uint32_t> find_entry_offset_by_id(memory_interface& memory, const std::uint64_t directory_address,
                                                             const std::uint32_t id)
        {
            IMAGE_RESOURCE_DIRECTORY directory{};
            if (!memory.try_read_memory(directory_address, &directory, sizeof(directory)))
            {
                return std::nullopt;
            }

            const auto entry_count =
                static_cast<std::uint32_t>(directory.NumberOfNamedEntries) + static_cast<std::uint32_t>(directory.NumberOfIdEntries);
            const auto entries_address = directory_address + sizeof(IMAGE_RESOURCE_DIRECTORY);

            for (std::uint32_t i = 0; i < entry_count; ++i)
            {
                IMAGE_RESOURCE_DIRECTORY_ENTRY entry{};
                if (!memory.try_read_memory(entries_address + i * sizeof(entry), &entry, sizeof(entry)))
                {
                    return std::nullopt;
                }

                if (entry.Name == id)
                {
                    return entry.OffsetToData;
                }
            }

            return std::nullopt;
        }

        std::optional<std::uint32_t> find_first_entry_offset(memory_interface& memory, const std::uint64_t directory_address)
        {
            IMAGE_RESOURCE_DIRECTORY directory{};
            if (!memory.try_read_memory(directory_address, &directory, sizeof(directory)))
            {
                return std::nullopt;
            }

            const auto entry_count =
                static_cast<std::uint32_t>(directory.NumberOfNamedEntries) + static_cast<std::uint32_t>(directory.NumberOfIdEntries);
            if (entry_count == 0)
            {
                return std::nullopt;
            }

            IMAGE_RESOURCE_DIRECTORY_ENTRY entry{};
            if (!memory.try_read_memory(directory_address + sizeof(IMAGE_RESOURCE_DIRECTORY), &entry, sizeof(entry)))
            {
                return std::nullopt;
            }

            return entry.OffsetToData;
        }

        std::optional<PEDirectory_t2> find_resource_directory(memory_interface& memory, const std::uint64_t image_base)
        {
            PEDosHeader_t dos_header{};
            if (!memory.try_read_memory(image_base, &dos_header, sizeof(dos_header)) || dos_header.e_magic != PEDosHeader_t::k_Magic)
            {
                return std::nullopt;
            }

            const auto nt_headers_address = image_base + dos_header.e_lfanew;

            std::uint32_t signature{};
            if (!memory.try_read_memory(nt_headers_address, &signature, sizeof(signature)) ||
                signature != PENTHeaders_t<std::uint32_t>::k_Signature)
            {
                return std::nullopt;
            }

            const auto optional_header_address = nt_headers_address + sizeof(std::uint32_t) + sizeof(PEFileHeader_t);

            std::uint16_t magic{};
            if (!memory.try_read_memory(optional_header_address, &magic, sizeof(magic)))
            {
                return std::nullopt;
            }

            if (magic == PEOptionalHeader_t<std::uint32_t>::k_Magic)
            {
                PENTHeaders_t<std::uint32_t> nt_headers{};
                if (!memory.try_read_memory(nt_headers_address, &nt_headers, sizeof(nt_headers)))
                {
                    return std::nullopt;
                }

                return nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
            }

            if (magic == PEOptionalHeader_t<std::uint64_t>::k_Magic)
            {
                PENTHeaders_t<std::uint64_t> nt_headers{};
                if (!memory.try_read_memory(nt_headers_address, &nt_headers, sizeof(nt_headers)))
                {
                    return std::nullopt;
                }

                return nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
            }

            return std::nullopt;
        }
    }

    std::vector<assembly_identity> parse_dependent_assemblies(const std::string_view manifest_text)
    {
        std::vector<assembly_identity> result{};

        constexpr std::string_view dependent_assembly_tag = "<dependentAssembly>";
        constexpr std::string_view dependent_assembly_end_tag = "</dependentAssembly>";
        constexpr std::string_view identity_tag_name = "<assemblyIdentity";

        std::size_t search_pos = 0;
        while (true)
        {
            const auto block_start = manifest_text.find(dependent_assembly_tag, search_pos);
            if (block_start == std::string_view::npos)
            {
                break;
            }

            const auto block_end = manifest_text.find(dependent_assembly_end_tag, block_start);
            if (block_end == std::string_view::npos)
            {
                break;
            }

            const auto block = manifest_text.substr(block_start, block_end - block_start);
            search_pos = block_end + dependent_assembly_end_tag.size();

            const auto identity_start = block.find(identity_tag_name);
            if (identity_start == std::string_view::npos)
            {
                continue;
            }

            const auto identity_tag_end = block.find('>', identity_start);
            if (identity_tag_end == std::string_view::npos)
            {
                continue;
            }

            const auto identity_tag = block.substr(identity_start, identity_tag_end - identity_start);

            assembly_identity identity{};
            identity.name = extract_attribute(identity_tag, "name").value_or("");
            identity.version = extract_attribute(identity_tag, "version").value_or("");
            identity.processor_architecture = extract_attribute(identity_tag, "processorArchitecture").value_or("");
            identity.public_key_token = extract_attribute(identity_tag, "publicKeyToken").value_or("");
            identity.language = extract_attribute(identity_tag, "language").value_or("");

            if (!identity.name.empty())
            {
                result.push_back(std::move(identity));
            }
        }

        return result;
    }

    std::vector<std::string> parse_redirected_file_names(const std::string_view manifest_text)
    {
        std::vector<std::string> result{};

        constexpr std::string_view file_tag = "<file";

        std::size_t search_pos = 0;
        while (true)
        {
            const auto tag_start = manifest_text.find(file_tag, search_pos);
            if (tag_start == std::string_view::npos)
            {
                break;
            }

            const auto tag_end = manifest_text.find('>', tag_start);
            if (tag_end == std::string_view::npos)
            {
                break;
            }

            const auto tag = manifest_text.substr(tag_start, tag_end - tag_start);
            search_pos = tag_end + 1;

            if (const auto name = extract_attribute(tag, "name"); name.has_value())
            {
                result.push_back(*name);
            }
        }

        return result;
    }

    std::optional<assembly_identity> parse_self_identity(const std::string_view manifest_text)
    {
        constexpr std::string_view identity_tag_name = "<assemblyIdentity";

        const auto identity_start = manifest_text.find(identity_tag_name);
        if (identity_start == std::string_view::npos)
        {
            return std::nullopt;
        }

        const auto identity_tag_end = manifest_text.find('>', identity_start);
        if (identity_tag_end == std::string_view::npos)
        {
            return std::nullopt;
        }

        const auto identity_tag = manifest_text.substr(identity_start, identity_tag_end - identity_start);

        assembly_identity identity{};
        identity.name = extract_attribute(identity_tag, "name").value_or("");
        identity.version = extract_attribute(identity_tag, "version").value_or("");
        identity.processor_architecture = extract_attribute(identity_tag, "processorArchitecture").value_or("");
        identity.public_key_token = extract_attribute(identity_tag, "publicKeyToken").value_or("");
        identity.language = extract_attribute(identity_tag, "language").value_or("");

        if (identity.name.empty())
        {
            return std::nullopt;
        }

        return identity;
    }

    std::optional<std::string> find_manifest_resource(memory_interface& memory, const std::uint64_t image_base)
    {
        const auto resource_directory_entry = find_resource_directory(memory, image_base);
        if (!resource_directory_entry || resource_directory_entry->VirtualAddress == 0 || resource_directory_entry->Size == 0)
        {
            return std::nullopt;
        }

        const auto resource_directory_address = image_base + resource_directory_entry->VirtualAddress;

        const auto type_offset = find_entry_offset_by_id(memory, resource_directory_address, k_resource_type_manifest);
        if (!type_offset || (*type_offset & RESOURCE_DATA_IS_DIRECTORY) == 0)
        {
            return std::nullopt;
        }

        const auto name_directory_address = image_base + (*type_offset & ~RESOURCE_DATA_IS_DIRECTORY);

        const auto name_offset = find_first_entry_offset(memory, name_directory_address);
        if (!name_offset || (*name_offset & RESOURCE_DATA_IS_DIRECTORY) == 0)
        {
            return std::nullopt;
        }

        const auto language_directory_address = image_base + (*name_offset & ~RESOURCE_DATA_IS_DIRECTORY);

        const auto language_offset = find_first_entry_offset(memory, language_directory_address);
        if (!language_offset)
        {
            return std::nullopt;
        }

        const auto data_entry_address = image_base + *language_offset;

        IMAGE_RESOURCE_DATA_ENTRY data_entry{};
        if (!memory.try_read_memory(data_entry_address, &data_entry, sizeof(data_entry)))
        {
            return std::nullopt;
        }

        std::string manifest{};
        manifest.resize(data_entry.Size);
        if (!memory.try_read_memory(image_base + data_entry.OffsetToData, manifest.data(), manifest.size()))
        {
            return std::nullopt;
        }

        return manifest;
    }
}
