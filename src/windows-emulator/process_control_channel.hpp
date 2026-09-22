#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "serialization.hpp"

namespace sogen
{
    enum class process_control_op : uint8_t
    {
        read_memory = 0,
        write_memory = 1,
        allocate_memory = 2,
        protect_memory = 3,
        free_memory = 4,
        query_memory = 5,
        terminate = 6,
        resume_thread = 7,
        adopt_section = 8,
        query_wow64_info = 9,
        query_cycle_time = 10,
        adopt_event = 11,
        adopt_mutant = 12,
        export_handle = 13,
    };

    inline constexpr int process_control_default_timeout_ms = 10000;

    struct process_control_request
    {
        uint64_t request_id{};
        process_control_op op{};
        uint64_t address{};
        uint64_t size{};
        uint32_t allocation_type{};
        uint32_t protection{};
        uint32_t free_type{};
        uint32_t info_class{};
        int32_t exit_status{};
        uint64_t maximum_size{};
        uint32_t page_protection{};
        uint32_t allocation_attributes{};
        uint32_t granted_access{};
        std::vector<std::byte> payload{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->request_id);
            buffer.write(static_cast<uint8_t>(this->op));
            buffer.write(this->address);
            buffer.write(this->size);
            buffer.write(this->allocation_type);
            buffer.write(this->protection);
            buffer.write(this->free_type);
            buffer.write(this->info_class);
            buffer.write(this->exit_status);
            buffer.write(this->maximum_size);
            buffer.write(this->page_protection);
            buffer.write(this->allocation_attributes);
            buffer.write(this->granted_access);
            buffer.write_vector(this->payload);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->request_id);
            uint8_t op{};
            buffer.read(op);
            this->op = static_cast<process_control_op>(op);
            buffer.read(this->address);
            buffer.read(this->size);
            buffer.read(this->allocation_type);
            buffer.read(this->protection);
            buffer.read(this->free_type);
            buffer.read(this->info_class);
            buffer.read(this->exit_status);
            buffer.read(this->maximum_size);
            buffer.read(this->page_protection);
            buffer.read(this->allocation_attributes);
            buffer.read(this->granted_access);
            buffer.read_vector(this->payload);
        }
    };

    struct process_control_response
    {
        uint64_t request_id{};
        int32_t status{};
        uint64_t bytes_written{};
        uint64_t base_address{};
        uint64_t region_size{};
        uint32_t old_protection{};
        uint32_t previous_suspend_count{};
        uint64_t minted_handle_bits{};
        uint32_t exported_object_type{};
        uint32_t allocation_type{};
        uint64_t size{};
        uint64_t maximum_size{};
        uint32_t page_protection{};
        uint32_t allocation_attributes{};
        uint32_t granted_access{};
        std::vector<std::byte> payload{};

        void serialize(utils::buffer_serializer& buffer) const
        {
            buffer.write(this->request_id);
            buffer.write(this->status);
            buffer.write(this->bytes_written);
            buffer.write(this->base_address);
            buffer.write(this->region_size);
            buffer.write(this->old_protection);
            buffer.write(this->previous_suspend_count);
            buffer.write(this->minted_handle_bits);
            buffer.write(this->exported_object_type);
            buffer.write(this->allocation_type);
            buffer.write(this->size);
            buffer.write(this->maximum_size);
            buffer.write(this->page_protection);
            buffer.write(this->allocation_attributes);
            buffer.write(this->granted_access);
            buffer.write_vector(this->payload);
        }

        void deserialize(utils::buffer_deserializer& buffer)
        {
            buffer.read(this->request_id);
            buffer.read(this->status);
            buffer.read(this->bytes_written);
            buffer.read(this->base_address);
            buffer.read(this->region_size);
            buffer.read(this->old_protection);
            buffer.read(this->previous_suspend_count);
            buffer.read(this->minted_handle_bits);
            buffer.read(this->exported_object_type);
            buffer.read(this->allocation_type);
            buffer.read(this->size);
            buffer.read(this->maximum_size);
            buffer.read(this->page_protection);
            buffer.read(this->allocation_attributes);
            buffer.read(this->granted_access);
            buffer.read_vector(this->payload);
        }
    };

    // Abstracts the transport for the synchronous request/response control protocol that lets a
    // parent process perform cross-process memory/process-control operations (ReadProcessMemory-
    // style) against a child it spawned via spawn_child_process (see child_process_spawn.hpp) -
    // deliberately a second, separate socketpair from pipe_ipc_channel's fire-and-forget named-pipe
    // broadcast, so a blocking control round-trip can never stall or be reordered against that
    // channel. The parent acts as client (request()); the child acts as server
    // (try_receive()/respond()) - both roles share this one interface, matching pipe_ipc_channel's
    // style. The concrete, socketpair-backed implementation lives in windows-analyzer
    // (child_process_spawn.cpp), next to the fork()/exec() logic that creates the transport.
    class process_control_channel
    {
      public:
        virtual ~process_control_channel() = default;

        // Blocks until a matching response arrives or timeout_ms elapses. A mismatched
        // request_id, a short read, EOF, or a timeout are all treated as failure and return
        // nullopt - and permanently poison the channel, so subsequent calls fail immediately
        // without waiting again.
        virtual std::optional<process_control_response> request(const process_control_request& request, int timeout_ms) = 0;

        // Non-blocking: returns nullopt immediately if no complete request is available yet.
        virtual std::optional<process_control_request> try_receive() = 0;

        virtual void respond(const process_control_response& response) = 0;

        // Fire-and-forget, child side: sent exactly once, over this same channel, right before the
        // child's own host process is about to end (see windows_emulator::notify_own_exit) - a
        // child that exits or crashes on its own is otherwise never observed by its parent (see
        // process_context::child_process_record). Never awaits a reply.
        virtual void notify_exit(int32_t exit_status) = 0;

        // Non-blocking, parent side: returns the exit status a child reported via notify_exit,
        // once it has arrived, or std::nullopt if none has arrived yet.
        virtual std::optional<int32_t> try_receive_exit_notification() = 0;

        // Parent side: forcibly ends the real host OS process behind this channel, for callers
        // that can no longer assume a poisoned (see request()) channel means the child already
        // exited - a child stuck in a long host-side operation poisons its channel via a timeout
        // exactly like a genuinely dead one does, so a caller that needs the process gone (rather
        // than merely unresponsive) must ask explicitly rather than infer it. A no-op default for
        // implementations with no real host process to kill (e.g. in tests).
        virtual void force_kill()
        {
        }
    };

} // namespace sogen
