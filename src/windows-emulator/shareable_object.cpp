#include "std_include.hpp"
#include "shareable_object.hpp"
#include "windows_emulator.hpp"
#include "cross_process.hpp"
#include "devices/named_pipe.hpp"
#include "memory_utils.hpp"

namespace sogen
{
    namespace syscalls
    {
        std::vector<uint8_t> export_created_token_groups(uint64_t handle_bits);
        void import_created_token_groups(uint64_t handle_bits, const std::vector<uint8_t>& in);
        void mark_adopted_token(uint64_t handle_bits);
    }
    namespace
    {
        template <typename Object>
        NTSTATUS describe_synchronization_object(process_context& process, Object* object, shared_object_description& description)
        {
            if (!object)
            {
                return STATUS_INVALID_HANDLE;
            }

            auto* const arena = process.ensure_shared_arena();
            if (arena && object->state.promote(process.shared_arena))
            {
                description.backing = arena->backing();
                description.arena_slot = object->state.slot()->index();
            }

            utils::buffer_serializer buffer{};
            object->serialize_object(buffer);
            description.object_bytes = buffer.get_buffer();
            return STATUS_SUCCESS;
        }

        // Binds a freshly deserialized object to the sender's arena slot when both processes are part of
        // the same arena; otherwise the deserialized values stay a private snapshot.
        template <typename Object>
        void adopt_shared_state(process_context& process, const shared_object_description& description, Object& object)
        {
            if (description.arena_slot < 0 || !description.backing)
            {
                return;
            }

            const auto incoming = kernel_arena::adopt(description.backing);
            if (!incoming)
            {
                return;
            }

            if (!process.shared_arena)
            {
                process.shared_arena = incoming;
            }
            else if (process.shared_arena->id() != incoming->id())
            {
                return;
            }

            object.state.adopt(process.shared_arena, static_cast<uint32_t>(description.arena_slot));
        }

        std::shared_ptr<shared_backing> get_shareable_section_memory(windows_emulator& win_emu, section_object& object)
        {
            auto& views = win_emu.process.section_views;
            if (object.view_storage_id == 0 && object.backing_address == 0 && object.allocation_attributes != SEC_RESERVE &&
                win_emu.memory.can_alias_host_memory_anywhere())
            {
                object.view_storage_id = views.create_storage(static_cast<size_t>(page_align_up(object.maximum_size)));
            }

            return object.view_storage_id != 0 ? views.storage_memory(object.view_storage_id) : nullptr;
        }

        std::vector<std::byte> read_guest_section_backing(const windows_emulator& win_emu, const section_object& object)
        {
            constexpr size_t page_size = 0x1000;
            std::vector<std::byte> content(static_cast<size_t>(page_align_up(object.maximum_size)));
            for (size_t offset = 0; offset < content.size(); offset += page_size)
            {
                if (!win_emu.memory.try_read_memory(object.backing_address + offset, content.data() + offset, page_size))
                {
                    std::fill_n(content.data() + offset, page_size, std::byte{0});
                }
            }

            return content;
        }

        NTSTATUS place_section_content(windows_emulator& win_emu, section_object& object, std::vector<std::byte> content)
        {
            if (content.empty())
            {
                return STATUS_SUCCESS;
            }

            if (win_emu.memory.can_alias_host_memory_anywhere())
            {
                object.view_storage_id = win_emu.process.section_views.adopt_storage(shared_backing::create_from_content(content));
                return STATUS_SUCCESS;
            }

            const auto protection = map_nt_to_emulator_protection(object.section_page_protection);
            object.backing_address = win_emu.memory.allocate_memory(
                content.size(), protection, false, 0, memory_region_kind::pagefile_section_view, win_emu.highest_user_address());
            if (!object.backing_address)
            {
                return STATUS_NO_MEMORY;
            }

            win_emu.memory.write_memory(object.backing_address, content.data(), content.size());
            return STATUS_SUCCESS;
        }
    }

    bool is_shareable_object_type(const handle_types::type type)
    {
        return type == handle_types::section || type == handle_types::event || type == handle_types::mutant ||
               type == handle_types::semaphore || type == handle_types::device || type == handle_types::file ||
               type == handle_types::process || type == handle_types::token;
    }

    NTSTATUS describe_shareable_object(windows_emulator& win_emu, const handle resolved_handle, shared_object_description& description)
    {
        auto& process = win_emu.process;
        description.type = static_cast<handle_types::type>(resolved_handle.value.type);

        switch (description.type)
        {
        case handle_types::section: {
            auto* const source = process.sections.get(resolved_handle);
            if (!source || source->object->is_image() || !source->object->file_name.empty())
            {
                return STATUS_NOT_SUPPORTED;
            }

            const auto memory = get_shareable_section_memory(win_emu, *source->object);

            utils::buffer_serializer buffer{};
            source->serialize_object(buffer);
            description.object_bytes = buffer.get_buffer();
            description.granted_access = source->granted_access;
            if (memory && memory->is_shared())
            {
                description.backing = memory;
            }
            else if (memory)
            {
                description.fallback_content = memory->content();
            }
            else if (source->object->backing_address != 0)
            {
                description.fallback_content = read_guest_section_backing(win_emu, *source->object);
            }
            return STATUS_SUCCESS;
        }

        case handle_types::event:
            return describe_synchronization_object(process, process.events.get(resolved_handle), description);

        case handle_types::mutant:
            return describe_synchronization_object(process, process.mutants.get(resolved_handle), description);

        case handle_types::semaphore:
            return describe_synchronization_object(process, process.semaphores.get(resolved_handle), description);

        case handle_types::file: {
            const auto* const source = process.files.get(resolved_handle);
            if (!source)
            {
                return STATUS_INVALID_HANDLE;
            }

            // The receiver reopens the same host file at the same position.
            utils::buffer_serializer buffer{};
            source->serialize_object(buffer);
            description.object_bytes = buffer.get_buffer();
            description.granted_access = source->granted_access;
            return STATUS_SUCCESS;
        }

        case handle_types::process: {
            uint32_t pid{};
            if (process.is_current_process_handle(resolved_handle))
            {
                pid = process.process_id;
                description.granted_access = resolve_granted_process_access(GENERIC_ALL);
            }
            else
            {
                const auto record = process.child_processes.find(static_cast<uint32_t>(resolved_handle.value.id));
                if (!resolved_handle.value.is_pseudo || record == process.child_processes.end())
                {
                    return STATUS_INVALID_HANDLE;
                }

                pid = record->second.pid;
                description.granted_access = record->second.granted_access;
            }

            utils::buffer_serializer buffer{};
            buffer.write(pid);
            description.object_bytes = buffer.get_buffer();
            return STATUS_SUCCESS;
        }

        case handle_types::token: {
            const bool impersonation = resolved_handle == DUMMY_IMPERSONATION_TOKEN;
            if (!impersonation && !process.tokens.get(resolved_handle))
            {
                return STATUS_INVALID_HANDLE;
            }

            utils::buffer_serializer buffer{};
            buffer.write(impersonation);
            buffer.write_vector(impersonation ? std::vector<uint8_t>{} : syscalls::export_created_token_groups(resolved_handle.bits));
            description.object_bytes = buffer.get_buffer();
            return STATUS_SUCCESS;
        }

        case handle_types::device: {
            auto* const container = process.devices.get(resolved_handle);
            auto* const pipe = container ? container->get_internal_device<named_pipe>() : nullptr;
            if (!pipe)
            {
                return STATUS_NOT_SUPPORTED;
            }

            utils::buffer_serializer buffer{};
            export_named_pipe_end(process, *pipe).serialize(buffer);
            description.object_bytes = buffer.get_buffer();
            return STATUS_SUCCESS;
        }

        default:
            return STATUS_NOT_SUPPORTED;
        }
    }

    NTSTATUS adopt_shareable_object(windows_emulator& win_emu, const shared_object_description& description, handle& adopted_handle)
    {
        auto& process = win_emu.process;
        utils::buffer_deserializer buffer{description.object_bytes};
        buffer.register_factory<file_system_wrapper>([&win_emu] { return file_system_wrapper{win_emu.file_sys}; });

        switch (description.type)
        {
        case handle_types::section: {
            section s{};
            s.deserialize_object(buffer);
            s.granted_access = description.granted_access;
            s->identity = 0;
            s->backing_address = 0;
            s->mapped_view_count = 0;
            s->view_storage_id = 0;

            if (description.backing)
            {
                s->view_storage_id = process.section_views.adopt_storage(description.backing);
            }
            else if (const auto status = place_section_content(win_emu, *s.object, description.fallback_content); !NT_SUCCESS(status))
            {
                return status;
            }

            adopted_handle = process.sections.store(std::move(s));
            return STATUS_SUCCESS;
        }

        case handle_types::event: {
            event e{};
            e.deserialize_object(buffer);
            adopt_shared_state(process, description, e);
            adopted_handle = process.events.store(std::move(e));
            return STATUS_SUCCESS;
        }

        case handle_types::mutant: {
            mutant m{};
            m.deserialize_object(buffer);
            adopt_shared_state(process, description, m);
            adopted_handle = process.mutants.store(std::move(m));
            return STATUS_SUCCESS;
        }

        case handle_types::semaphore: {
            semaphore s{};
            s.deserialize_object(buffer);
            adopt_shared_state(process, description, s);
            adopted_handle = process.semaphores.store(std::move(s));
            return STATUS_SUCCESS;
        }

        case handle_types::file: {
            file f{};
            try
            {
                f.deserialize_object(buffer);
            }
            catch (const std::exception&)
            {
                return STATUS_OBJECT_NAME_NOT_FOUND;
            }

            adopted_handle = process.files.store(std::move(f));
            return STATUS_SUCCESS;
        }

        case handle_types::process: {
            uint32_t pid{};
            buffer.read(pid);
            adopted_handle = process.get_process_handle_by_pid(pid, description.granted_access);
            return STATUS_SUCCESS;
        }

        case handle_types::token: {
            // A token's identity is that of the process querying it, so the receiver's token needs no
            // state from the sender's. It is still a token distinct from the receiver's own, so adjusting
            // its privileges must not change the receiver's.
            bool impersonation{};
            buffer.read(impersonation);
            if (impersonation)
            {
                adopted_handle = DUMMY_IMPERSONATION_TOKEN;
                return STATUS_SUCCESS;
            }

            std::vector<uint8_t> carried_groups{};
            buffer.read_vector(carried_groups);
            process_token token{};
            token.privileges = process.privileges;
            adopted_handle = process.tokens.store(std::move(token));
            syscalls::import_created_token_groups(adopted_handle.bits, carried_groups);
            syscalls::mark_adopted_token(adopted_handle.bits);
            return STATUS_SUCCESS;
        }

        case handle_types::device: {
            io_device_creation_data data{};
            io_device_container container{u"NamedPipe", win_emu, data};
            auto* const pipe = container.get_internal_device<named_pipe>();
            if (!pipe)
            {
                return STATUS_NOT_SUPPORTED;
            }

            named_pipe_end_transfer transfer{};
            transfer.deserialize(buffer);
            import_named_pipe_end(process, *pipe, transfer);

            adopted_handle = process.devices.store(std::move(container));
            return STATUS_SUCCESS;
        }

        default:
            return STATUS_NOT_SUPPORTED;
        }
    }

    namespace
    {
        std::vector<std::byte> join_payload(const shared_object_description& description)
        {
            auto payload = description.object_bytes;
            payload.insert(payload.end(), description.fallback_content.begin(), description.fallback_content.end());
            return payload;
        }

        void split_payload(const std::vector<std::byte>& payload, const uint64_t object_size, shared_object_description& description)
        {
            const auto split = std::min<size_t>(static_cast<size_t>(object_size), payload.size());
            description.object_bytes.assign(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(split));
            description.fallback_content.assign(payload.begin() + static_cast<std::ptrdiff_t>(split), payload.end());
        }
    }

    void write_description_to_request(const shared_object_description& description, process_control_request& request)
    {
        request.op = process_control_op::adopt_object;
        request.info_class = static_cast<uint32_t>(description.type);
        request.granted_access = description.granted_access;
        request.address = static_cast<uint64_t>(description.arena_slot);
        request.size = description.object_bytes.size();
        request.payload = join_payload(description);
        request.backing = description.backing;
    }

    shared_object_description read_description_from_request(const process_control_request& request)
    {
        shared_object_description description{};
        description.type = static_cast<handle_types::type>(request.info_class);
        description.granted_access = request.granted_access;
        description.arena_slot = static_cast<int64_t>(request.address);
        description.backing = request.backing;
        split_payload(request.payload, request.size, description);
        return description;
    }

    void write_description_to_response(const shared_object_description& description, process_control_response& response)
    {
        response.exported_object_type = static_cast<uint32_t>(description.type);
        response.granted_access = description.granted_access;
        response.base_address = static_cast<uint64_t>(description.arena_slot);
        response.size = description.object_bytes.size();
        response.payload = join_payload(description);
        response.backing = description.backing;
    }

    shared_object_description read_description_from_response(const process_control_response& response)
    {
        shared_object_description description{};
        description.type = static_cast<handle_types::type>(response.exported_object_type);
        description.granted_access = response.granted_access;
        description.arena_slot = static_cast<int64_t>(response.base_address);
        description.backing = response.backing;
        split_payload(response.payload, response.size, description);
        return description;
    }
}
