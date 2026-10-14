#include "../std_include.hpp"
#include "csr_activation_context.hpp"
#include "assembly_resolver.hpp"

#include "../windows_emulator.hpp"

namespace sogen
{
    namespace
    {
        constexpr uint32_t basesrv_create_activation_context = 0x1001E;

        // CSR_API_MESSAGE, after the PORT_MESSAGE header.
        constexpr uint64_t csr_api_number_offset = 0x08;
        constexpr uint64_t csr_return_value_offset = 0x0C;
        constexpr uint64_t csr_payload_offset = 0x18;

        // BASE_SXS_CREATE_ACTIVATION_CONTEXT_MSG, as kernel32!BasepCreateActCtx fills it.
        constexpr uint64_t msg_processor_architecture = 0x04;
        constexpr uint64_t msg_manifest_handle_type = 0x18 + 0x02;
        constexpr uint64_t msg_manifest_path = 0x18 + 0x08;
        constexpr uint64_t msg_manifest_file_handle = 0x18 + 0x18;
        constexpr uint64_t msg_manifest_offset = 0x18 + 0x28;
        constexpr uint64_t msg_manifest_size = 0x18 + 0x30;
        constexpr uint64_t msg_assembly_directory = 0x88;
        constexpr uint64_t msg_resource_name = 0xB0;
        constexpr uint64_t msg_activation_context_data = 0xB8;
        constexpr uint64_t msg_minimum_size = 0xC0;

        // BASE_MSG_SXS_STREAM HandleType
        constexpr uint8_t stream_from_resource = 1;
        constexpr uint8_t stream_from_file = 3;

        constexpr uint16_t processor_architecture_amd64 = 9;
        constexpr size_t maximum_manifest_size = 0x800000;

        constexpr NTSTATUS status_sxs_cant_gen_actctx = static_cast<NTSTATUS>(0xC0150002);

        class emulated_assembly_store : public sxs::assembly_store
        {
          public:
            explicit emulated_assembly_store(const file_system& file_sys)
                : file_sys_(file_sys)
            {
            }

            std::optional<std::vector<std::byte>> read_file(const std::u16string_view windows_path_text) override
            {
                const auto host_path = this->translate(windows_path_text);
                if (!host_path)
                {
                    return std::nullopt;
                }

                return read_host_file(*host_path);
            }

            std::vector<std::u16string> list_files(const std::u16string_view windows_directory) override
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

            static std::optional<std::vector<std::byte>> read_host_file(const std::filesystem::path& path)
            {
                std::ifstream stream(path, std::ios::binary);
                if (!stream)
                {
                    return std::nullopt;
                }

                std::vector<std::byte> data{};
                stream.seekg(0, std::ios::end);
                const auto size = static_cast<size_t>(stream.tellg());
                if (size > maximum_manifest_size)
                {
                    return std::nullopt;
                }

                stream.seekg(0, std::ios::beg);
                data.resize(size);
                stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
                return stream ? std::optional(std::move(data)) : std::nullopt;
            }

          private:
            const file_system& file_sys_;

            std::optional<std::filesystem::path> translate(const std::u16string_view windows_path_text) const
            {
                const windows_path path{std::filesystem::path(std::u16string(windows_path_text))};
                if (!path.is_absolute())
                {
                    return std::nullopt;
                }

                return this->file_sys_.translate(path);
            }
        };

        std::u16string strip_nt_prefix(std::u16string path)
        {
            if (path.starts_with(u"\\??\\"))
            {
                path.erase(0, 4);
            }

            return path;
        }

        std::u16string directory_of(const std::u16string_view path)
        {
            const auto separator = path.find_last_of(u'\\');
            return separator == std::u16string_view::npos ? std::u16string{} : std::u16string(path.substr(0, separator));
        }

        std::u16string read_message_string(windows_emulator& win_emu, const uint64_t address)
        {
            UNICODE_STRING<EmulatorTraits<Emu64>> string{};
            if (!win_emu.memory.try_read_memory(address, &string, sizeof(string)))
            {
                return {};
            }

            return read_unicode_string(win_emu.emu(), string);
        }

        std::optional<std::vector<std::byte>> read_manifest(windows_emulator& win_emu, const uint64_t message)
        {
            const auto handle_type = win_emu.emu().read_memory<uint8_t>(message + msg_manifest_handle_type);
            const auto size = win_emu.emu().read_memory<uint64_t>(message + msg_manifest_size);
            if (size == 0 || size > maximum_manifest_size)
            {
                return std::nullopt;
            }

            if (handle_type == stream_from_resource)
            {
                std::vector<std::byte> data(static_cast<size_t>(size));
                if (!win_emu.memory.try_read_memory(win_emu.emu().read_memory<uint64_t>(message + msg_manifest_offset), data.data(),
                                                    data.size()))
                {
                    return std::nullopt;
                }

                return data;
            }

            if (handle_type == stream_from_file)
            {
                const auto file_handle = win_emu.emu().read_memory<handle>(message + msg_manifest_file_handle);
                const auto* file = win_emu.process.files.get(file_handle);
                if (!file || file->host_path.empty())
                {
                    return std::nullopt;
                }

                return emulated_assembly_store::read_host_file(file->host_path);
            }

            return std::nullopt;
        }

        NTSTATUS create_activation_context(windows_emulator& win_emu, const uint64_t message)
        {
            auto manifest = read_manifest(win_emu, message);
            if (!manifest)
            {
                return status_sxs_cant_gen_actctx;
            }

            sxs::resolution_request request{};
            request.root_manifest = std::move(*manifest);
            request.root_manifest_path = strip_nt_prefix(read_message_string(win_emu, message + msg_manifest_path));
            request.application_directory = strip_nt_prefix(read_message_string(win_emu, message + msg_assembly_directory));
            if (request.application_directory.empty())
            {
                request.application_directory = directory_of(request.root_manifest_path);
            }

            const auto architecture = win_emu.emu().read_memory<uint16_t>(message + msg_processor_architecture);
            request.processor_architecture = architecture == processor_architecture_amd64 ? u"amd64" : u"x86";
            request.windows_directory = win_emu.version.get_system_root().u16string();
            request.resource_name = static_cast<uint32_t>(win_emu.emu().read_memory<uint64_t>(message + msg_resource_name));

            win_emu.callbacks.on_generic_access("Creating activation context", request.root_manifest_path);

            emulated_assembly_store store{win_emu.file_sys};
            auto resolved = sxs::resolve_activation_context(request, store);
            if (const auto* status = std::get_if<uint32_t>(&resolved))
            {
                return static_cast<NTSTATUS>(*status);
            }

            const auto data = sxs::build_activation_context_data(std::get<sxs::activation_context_input>(resolved));
            const auto size = static_cast<size_t>(page_align_up(data.size()));
            // wow64's NtWow64CsrClientCallServer thunk hands this pointer back to the 32-bit caller narrowed.
            const auto highest = win_emu.process.is_wow64_process ? 0x7FFEFFFFULL : MAX_ALLOCATION_ADDRESS;
            const auto base = win_emu.memory.find_free_host_allocation_base(size, MIN_ALLOCATION_ADDRESS, highest);
            if (!base ||
                !win_emu.memory.allocate_memory(base, size, memory_permission::read, false, memory_region_kind::pagefile_section_view))
            {
                return STATUS_NO_MEMORY;
            }

            win_emu.memory.write_memory(base, data.data(), data.size());

            const auto destination = win_emu.emu().read_memory<uint64_t>(message + msg_activation_context_data);
            if (!win_emu.memory.try_write_memory(destination, &base, sizeof(base)))
            {
                win_emu.memory.release_memory(base, 0);
                return STATUS_INVALID_PARAMETER;
            }

            return STATUS_SUCCESS;
        }
    }

    std::optional<lpc_request_result> try_handle_sxs_create_activation_context(windows_emulator& win_emu, const lpc_request_context& c)
    {
        if (c.send_buffer_length < csr_payload_offset + msg_minimum_size || c.recv_buffer == 0)
        {
            return std::nullopt;
        }

        if (win_emu.emu().read_memory<uint32_t>(c.send_buffer + csr_api_number_offset) != basesrv_create_activation_context)
        {
            return std::nullopt;
        }

        const auto status = create_activation_context(win_emu, c.send_buffer + csr_payload_offset);
        win_emu.emu().write_memory(c.recv_buffer + csr_return_value_offset, status);
        return lpc_request_result{STATUS_SUCCESS, lpc_request_result::reply_in_place};
    }
}
