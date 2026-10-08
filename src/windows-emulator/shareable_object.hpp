#pragma once

#include "std_include.hpp"
#include "process_context.hpp"
#include "process_control_channel.hpp"

namespace sogen
{
    class windows_emulator;

    // A kernel object being handed to another sogen host process (NtDuplicateObject across the
    // process boundary). Objects whose state lives in shared memory are described by where that state
    // is, so the receiver ends up referring to the very same state; only when the state cannot be shared
    // (no shared memory on the host, or a different arena) does it degrade to a snapshot of the values.
    struct shared_object_description
    {
        handle_types::type type{};
        ACCESS_MASK granted_access{};
        // ref_counted_object::serialize_object output.
        std::vector<std::byte> object_bytes{};
        // Pagefile section content, only when the section's backing is not a shareable mapping.
        std::vector<std::byte> fallback_content{};
        // A section's backing, or the kernel arena's backing for synchronization objects.
        std::shared_ptr<shared_backing> backing{};
        // Slot inside the arena holding the object's state, or -1 for none.
        int64_t arena_slot{-1};
    };

    bool is_shareable_object_type(handle_types::type type);

    // Describes the object behind `resolved_handle`, moving synchronization state into the shared arena
    // if it is not there yet. Image- and file-backed sections are not shareable.
    NTSTATUS describe_shareable_object(process_context& process, handle resolved_handle, shared_object_description& description);

    // Creates a local object from a description and stores it in the process's handle table.
    NTSTATUS adopt_shareable_object(windows_emulator& win_emu, const shared_object_description& description, handle& adopted_handle);

    void write_description_to_request(const shared_object_description& description, process_control_request& request);
    shared_object_description read_description_from_request(const process_control_request& request);
    void write_description_to_response(const shared_object_description& description, process_control_response& response);
    shared_object_description read_description_from_response(const process_control_response& response);
}
