#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

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
        std::vector<std::byte> payload{};
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
    };

} // namespace sogen
