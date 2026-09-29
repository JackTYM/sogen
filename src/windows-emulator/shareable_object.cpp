#include "std_include.hpp"
#include "shareable_object.hpp"

namespace sogen
{
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
    }

    bool is_shareable_object_type(const handle_types::type type)
    {
        return type == handle_types::section || type == handle_types::event || type == handle_types::mutant ||
               type == handle_types::semaphore;
    }

    NTSTATUS describe_shareable_object(process_context& process, const handle resolved_handle, shared_object_description& description)
    {
        description.type = static_cast<handle_types::type>(resolved_handle.value.type);

        switch (description.type)
        {
        case handle_types::section: {
            auto* const source = process.sections.get(resolved_handle);
            if (!source || source->object->is_image() || !source->object->file_name.empty())
            {
                return STATUS_NOT_SUPPORTED;
            }

            auto& backing = source->object->ensure_backing(static_cast<size_t>(page_align_up(source->object->maximum_size)));

            utils::buffer_serializer buffer{};
            source->serialize_object(buffer);
            description.object_bytes = buffer.get_buffer();
            description.granted_access = source->granted_access;
            description.backing = source->object->backing;
            if (!backing.is_shared())
            {
                description.fallback_content = backing.content();
            }
            return STATUS_SUCCESS;
        }

        case handle_types::event:
            return describe_synchronization_object(process, process.events.get(resolved_handle), description);

        case handle_types::mutant:
            return describe_synchronization_object(process, process.mutants.get(resolved_handle), description);

        case handle_types::semaphore:
            return describe_synchronization_object(process, process.semaphores.get(resolved_handle), description);

        default:
            return STATUS_NOT_SUPPORTED;
        }
    }

    NTSTATUS adopt_shareable_object(process_context& process, const shared_object_description& description, handle& adopted_handle)
    {
        utils::buffer_deserializer buffer{description.object_bytes};

        switch (description.type)
        {
        case handle_types::section: {
            section s{};
            s.deserialize_object(buffer);
            s.granted_access = description.granted_access;
            s.object->backing =
                description.backing ? description.backing : shared_backing::create_from_content(description.fallback_content);
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
