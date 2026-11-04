#include "../std_include.hpp"
#include "emulated_sxs.hpp"

#include "../windows_emulator.hpp"

#include <platform/win_pefile.hpp>

#include <functional>

namespace sogen::sxs
{
    namespace
    {
        constexpr size_t maximum_manifest_size = 0x800000;
        constexpr uint32_t rt_manifest = 24;
        constexpr uint32_t createprocess_manifest_resource_id = 1;
        constexpr uint32_t resource_subdirectory_flag = 0x80000000;

        struct resource_directory_entry
        {
            uint32_t name{};
            uint32_t offset{};
        };

        using rva_reader = std::function<bool(uint64_t rva, void* destination, size_t length)>;

        template <typename T>
        std::optional<T> read_value(const rva_reader& read_rva, const uint64_t rva)
        {
            T value{};
            if (!read_rva(rva, &value, sizeof(value)))
            {
                return std::nullopt;
            }

            return value;
        }

        std::optional<uint32_t> find_resource_entry(const rva_reader& read_rva, const uint64_t resource_root,
                                                    const uint32_t directory_offset, const std::optional<uint32_t> id)
        {
            const auto directory = resource_root + directory_offset;
            const auto named_entries = read_value<uint16_t>(read_rva, directory + 12);
            const auto id_entries = read_value<uint16_t>(read_rva, directory + 14);
            if (!named_entries || !id_entries)
            {
                return std::nullopt;
            }

            for (uint32_t i = 0; i < static_cast<uint32_t>(*named_entries + *id_entries); ++i)
            {
                const auto entry = read_value<resource_directory_entry>(read_rva, directory + 16 + i * sizeof(resource_directory_entry));
                if (!entry)
                {
                    return std::nullopt;
                }

                if (!id || (i >= *named_entries && entry->name == *id))
                {
                    return entry->offset;
                }
            }

            return std::nullopt;
        }

        std::optional<std::vector<std::byte>> read_manifest_resource(const rva_reader& read_rva, const uint32_t resource_rva)
        {
            if (resource_rva == 0)
            {
                return std::nullopt;
            }

            const uint64_t root = resource_rva;
            const auto type = find_resource_entry(read_rva, root, 0, rt_manifest);
            if (!type || !(*type & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto name = find_resource_entry(read_rva, root, *type & ~resource_subdirectory_flag, createprocess_manifest_resource_id);
            if (!name || !(*name & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto language = find_resource_entry(read_rva, root, *name & ~resource_subdirectory_flag, std::nullopt);
            if (!language || (*language & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto data_rva = read_value<uint32_t>(read_rva, root + *language);
            const auto size = read_value<uint32_t>(read_rva, root + *language + 4);
            if (!data_rva || !size || *size == 0 || *size > maximum_manifest_size)
            {
                return std::nullopt;
            }

            std::vector<std::byte> data(*size);
            if (!read_rva(*data_rva, data.data(), data.size()))
            {
                return std::nullopt;
            }

            return data;
        }

        std::optional<std::vector<std::byte>> read_manifest_resource(windows_emulator& win_emu, const mapped_module& executable)
        {
            const auto image = executable.image_base;
            const rva_reader read_rva = [&](const uint64_t rva, void* destination, const size_t length) {
                return win_emu.memory.try_read_memory(image + rva, destination, length);
            };

            const auto nt_headers = read_value<uint32_t>(read_rva, 0x3C);
            if (!nt_headers)
            {
                return std::nullopt;
            }

            const auto optional_header = *nt_headers + 0x18;
            const auto magic = read_value<uint16_t>(read_rva, optional_header);
            if (!magic)
            {
                return std::nullopt;
            }

            const auto data_directories = optional_header + (*magic == 0x20B ? 112 : 96);
            const auto resource_rva = read_value<uint32_t>(read_rva, data_directories + IMAGE_DIRECTORY_ENTRY_RESOURCE * 8);
            return resource_rva ? read_manifest_resource(read_rva, *resource_rva) : std::nullopt;
        }

        template <typename T>
        std::optional<T> read_file_value(const std::vector<std::byte>& file, const size_t offset)
        {
            if (offset > file.size() || file.size() - offset < sizeof(T))
            {
                return std::nullopt;
            }

            T value{};
            std::memcpy(&value, file.data() + offset, sizeof(value));
            return value;
        }

        std::optional<std::vector<std::byte>> read_manifest_resource_from_file(const std::vector<std::byte>& file)
        {
            const auto nt_headers = read_file_value<uint32_t>(file, 0x3C);
            if (!nt_headers)
            {
                return std::nullopt;
            }

            const auto section_count = read_file_value<uint16_t>(file, *nt_headers + 6);
            const auto optional_header_size = read_file_value<uint16_t>(file, *nt_headers + 20);
            const auto magic = read_file_value<uint16_t>(file, *nt_headers + 0x18);
            if (!section_count || !optional_header_size || !magic)
            {
                return std::nullopt;
            }

            const auto data_directories = *nt_headers + 0x18 + (*magic == 0x20B ? 112 : 96);
            const auto resource_rva = read_file_value<uint32_t>(file, data_directories + IMAGE_DIRECTORY_ENTRY_RESOURCE * 8);
            if (!resource_rva)
            {
                return std::nullopt;
            }

            struct section_header
            {
                uint32_t virtual_size{};
                uint32_t virtual_address{};
                uint32_t raw_size{};
                uint32_t raw_offset{};
            };

            std::vector<section_header> sections{};
            const size_t table = static_cast<size_t>(*nt_headers) + 0x18 + *optional_header_size;
            for (uint32_t i = 0; i < *section_count; ++i)
            {
                const auto entry = table + i * 40;
                const auto virtual_size = read_file_value<uint32_t>(file, entry + 8);
                const auto virtual_address = read_file_value<uint32_t>(file, entry + 12);
                const auto raw_size = read_file_value<uint32_t>(file, entry + 16);
                const auto raw_offset = read_file_value<uint32_t>(file, entry + 20);
                if (!virtual_size || !virtual_address || !raw_size || !raw_offset)
                {
                    return std::nullopt;
                }

                sections.push_back({*virtual_size, *virtual_address, *raw_size, *raw_offset});
            }

            const rva_reader read_rva = [&](const uint64_t rva, void* destination, const size_t length) {
                for (const auto& section : sections)
                {
                    const auto extent = std::max(section.virtual_size, section.raw_size);
                    if (rva >= section.virtual_address && rva + length <= static_cast<uint64_t>(section.virtual_address) + extent)
                    {
                        const auto offset = static_cast<size_t>(section.raw_offset + (rva - section.virtual_address));
                        if (offset > file.size() || file.size() - offset < length)
                        {
                            return false;
                        }

                        std::memcpy(destination, file.data() + offset, length);
                        return true;
                    }
                }

                return false;
            };

            return read_manifest_resource(read_rva, *resource_rva);
        }

        struct known_operating_system
        {
            std::u16string_view guid;
            uint16_t major;
            uint16_t minor;
        };

        constexpr std::array known_operating_systems{
            known_operating_system{u"{e2011457-1546-43c5-a5fe-008deee3d3f0}", 6, 0},
            known_operating_system{u"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}", 6, 1},
            known_operating_system{u"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}", 6, 2},
            known_operating_system{u"{1f676c76-80e1-4239-95bb-83d0f6d0da78}", 6, 3},
            known_operating_system{u"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}", 10, 0},
        };
    }

    emulated_assembly_store::emulated_assembly_store(const file_system& file_sys)
        : file_sys_(file_sys)
    {
    }

    std::optional<std::vector<std::byte>> emulated_assembly_store::read_file(const std::u16string_view windows_path_text)
    {
        const auto path = to_absolute_path(windows_path_text);
        if (!path)
        {
            return std::nullopt;
        }

        return read_guest_file(this->file_sys_, *path);
    }

    std::vector<std::u16string> emulated_assembly_store::list_files(const std::u16string_view windows_directory)
    {
        std::vector<std::u16string> names{};
        const auto path = to_absolute_path(windows_directory);
        if (!path)
        {
            return names;
        }

        for (auto& entry : this->file_sys_.list_directory(*path))
        {
            if (!entry.is_directory)
            {
                names.push_back(std::move(entry.name));
            }
        }

        return names;
    }

    std::optional<windows_path> emulated_assembly_store::to_absolute_path(const std::u16string_view windows_path_text)
    {
        windows_path path{std::filesystem::path(std::u16string(windows_path_text))};
        if (!path.is_absolute())
        {
            return std::nullopt;
        }

        return path;
    }

    std::optional<std::vector<std::byte>> read_guest_file(const file_system& file_sys, const windows_path& path)
    {
        const auto status = file_sys.stat(path);
        if (!status || static_cast<size_t>(status->st_size) > maximum_manifest_size)
        {
            return std::nullopt;
        }

        std::vector<std::byte> data{};
        if (!file_sys.read_file(path, &data))
        {
            return std::nullopt;
        }

        return data;
    }

    std::u16string directory_of(const std::u16string_view path)
    {
        const auto separator = path.find_last_of(u'\\');
        return separator == std::u16string_view::npos ? std::u16string{} : std::u16string(path.substr(0, separator));
    }

    uint64_t map_activation_context_data(windows_emulator& win_emu, const std::vector<std::byte>& data)
    {
        const auto size = static_cast<size_t>(page_align_up(data.size()));
        const auto highest = win_emu.process.is_wow64_process ? 0x7FFEFFFFULL : MAX_ALLOCATION_ADDRESS;
        const auto base = win_emu.memory.find_free_host_allocation_base(size, MIN_ALLOCATION_ADDRESS, highest);
        if (!base || !win_emu.memory.allocate_memory(base, size, memory_permission::read, false, memory_region_kind::pagefile_section_view))
        {
            return 0;
        }

        win_emu.memory.write_memory(base, data.data(), data.size());
        return base;
    }

    void create_process_default_activation_context(windows_emulator& win_emu, const mapped_module& executable)
    {
        const auto executable_path = executable.module_path.u16string();

        resolution_request request{};
        request.root_manifest_path = executable_path;
        request.application_directory = directory_of(executable_path);
        request.processor_architecture = win_emu.process.is_wow64_process ? u"x86" : u"amd64";
        request.windows_directory = win_emu.version.get_system_root().u16string();

        emulated_assembly_store store{win_emu.file_sys};
        if (auto manifest = read_manifest_resource(win_emu, executable))
        {
            request.root_manifest = std::move(*manifest);
            request.resource_name = createprocess_manifest_resource_id;
        }
        else if (auto external = store.read_file(executable_path + u".manifest"))
        {
            request.root_manifest = std::move(*external);
            request.root_manifest_path = executable_path + u".manifest";
        }
        else
        {
            return;
        }

        auto resolved = resolve_activation_context(request, store);
        if (const auto* status = std::get_if<uint32_t>(&resolved))
        {
            win_emu.log.warn("Side-by-side configuration of %s could not be bound (status 0x%X)\n", executable.name.c_str(), *status);
            return;
        }

        const auto base = map_activation_context_data(win_emu, build_activation_context_data(std::get<activation_context_input>(resolved)));
        if (!base)
        {
            return;
        }

        win_emu.process.peb64.access([&](PEB64& peb) { peb.ActivationContextData = base; });
        if (win_emu.process.peb32)
        {
            win_emu.process.peb32->access([&](PEB32& peb) { peb.ActivationContextData = static_cast<uint32_t>(base); });
        }
    }
}

namespace sogen::sxs
{
    std::optional<std::vector<std::byte>> read_image_file_manifest(const file_system& file_sys, const windows_path& image)
    {
        std::vector<std::byte> file{};
        return file_sys.read_file(image, &file) ? read_manifest_resource_from_file(file) : std::nullopt;
    }

    std::optional<process_compatibility> query_process_compatibility(const manifest& content)
    {
        std::optional<process_compatibility> result{};

        for (const auto& element : content.compatibility)
        {
            if (element.type == compatibility_element_type::max_version_tested)
            {
                if (!result)
                {
                    result = process_compatibility{};
                }

                result->max_version_tested = std::max(result->max_version_tested, element.max_version_tested);
                continue;
            }

            for (const auto& system : known_operating_systems)
            {
                if (parse_guid(system.guid) != element.id)
                {
                    continue;
                }

                if (!result)
                {
                    result = process_compatibility{};
                }

                if (std::pair{system.major, system.minor} > std::pair{result->os_major, result->os_minor})
                {
                    result->os_major = system.major;
                    result->os_minor = system.minor;
                }
            }
        }

        return result;
    }
}
