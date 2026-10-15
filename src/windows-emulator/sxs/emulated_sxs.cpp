#include "../std_include.hpp"
#include "emulated_sxs.hpp"

#include "../windows_emulator.hpp"

#include <platform/win_pefile.hpp>

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

        std::optional<uint32_t> find_resource_entry(windows_emulator& win_emu, const uint64_t resource_root,
                                                    const uint32_t directory_offset, const std::optional<uint32_t> id)
        {
            const auto directory = resource_root + directory_offset;
            const auto named_entries = win_emu.emu().read_memory<uint16_t>(directory + 12);
            const auto id_entries = win_emu.emu().read_memory<uint16_t>(directory + 14);

            for (uint32_t i = 0; i < static_cast<uint32_t>(named_entries + id_entries); ++i)
            {
                const auto entry =
                    win_emu.emu().read_memory<resource_directory_entry>(directory + 16 + i * sizeof(resource_directory_entry));
                if (!id || (i >= named_entries && entry.name == *id))
                {
                    return entry.offset;
                }
            }

            return std::nullopt;
        }

        std::optional<std::vector<std::byte>> read_manifest_resource(windows_emulator& win_emu, const mapped_module& executable)
        {
            const auto image = executable.image_base;
            const auto nt_headers = image + win_emu.emu().read_memory<uint32_t>(image + 0x3C);
            const auto optional_header = nt_headers + 0x18;
            const auto magic = win_emu.emu().read_memory<uint16_t>(optional_header);
            const auto data_directories = optional_header + (magic == 0x20B ? 112 : 96);
            const auto resource_rva = win_emu.emu().read_memory<uint32_t>(data_directories + IMAGE_DIRECTORY_ENTRY_RESOURCE * 8);
            if (resource_rva == 0)
            {
                return std::nullopt;
            }

            const auto root = image + resource_rva;
            const auto type = find_resource_entry(win_emu, root, 0, rt_manifest);
            if (!type || !(*type & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto name = find_resource_entry(win_emu, root, *type & ~resource_subdirectory_flag, createprocess_manifest_resource_id);
            if (!name || !(*name & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto language = find_resource_entry(win_emu, root, *name & ~resource_subdirectory_flag, std::nullopt);
            if (!language || (*language & resource_subdirectory_flag))
            {
                return std::nullopt;
            }

            const auto data_rva = win_emu.emu().read_memory<uint32_t>(root + *language);
            const auto size = win_emu.emu().read_memory<uint32_t>(root + *language + 4);
            if (size == 0 || size > maximum_manifest_size)
            {
                return std::nullopt;
            }

            std::vector<std::byte> data(size);
            if (!win_emu.memory.try_read_memory(image + data_rva, data.data(), data.size()))
            {
                return std::nullopt;
            }

            return data;
        }
    }

    emulated_assembly_store::emulated_assembly_store(const file_system& file_sys)
        : file_sys_(file_sys)
    {
    }

    std::optional<std::vector<std::byte>> emulated_assembly_store::read_file(const std::u16string_view windows_path_text)
    {
        const auto host_path = this->translate(windows_path_text);
        if (!host_path)
        {
            return std::nullopt;
        }

        return read_host_file(*host_path);
    }

    std::vector<std::u16string> emulated_assembly_store::list_files(const std::u16string_view windows_directory)
    {
        std::vector<std::u16string> names{};
        const auto host_path = this->translate(windows_directory);
        if (!host_path)
        {
            return names;
        }

        std::error_code ec{};
        for (const auto& entry : std::filesystem::directory_iterator(*host_path, ec))
        {
            if (entry.is_regular_file(ec))
            {
                names.push_back(entry.path().filename().u16string());
            }
        }

        return names;
    }

    std::optional<std::filesystem::path> emulated_assembly_store::translate(const std::u16string_view windows_path_text) const
    {
        const windows_path path{std::filesystem::path(std::u16string(windows_path_text))};
        if (!path.is_absolute())
        {
            return std::nullopt;
        }

        return this->file_sys_.translate(path);
    }

    std::optional<std::vector<std::byte>> read_host_file(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            return std::nullopt;
        }

        stream.seekg(0, std::ios::end);
        const auto size = static_cast<size_t>(stream.tellg());
        if (size > maximum_manifest_size)
        {
            return std::nullopt;
        }

        std::vector<std::byte> data(size);
        stream.seekg(0, std::ios::beg);
        stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        return stream ? std::optional(std::move(data)) : std::nullopt;
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
