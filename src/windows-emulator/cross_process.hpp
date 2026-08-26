#pragma once

#include "handles.hpp"

#include <variant>

namespace sogen
{
    struct syscall_context;
    class process_control_channel;

    struct child_target
    {
        uint32_t record_id{};
        process_control_channel* channel{};
    };

    // Resolves a guest-supplied handle to the child process it names, for a syscall handler about to
    // perform a cross-process operation against it (NtReadVirtualMemory and friends, see
    // process_control_channel.hpp) - or the NTSTATUS to return to the guest instead.
    //
    // h must be one of the process/thread pseudo handles NtCreateUserProcess mints for its child
    // (handle_NtCreateUserProcess); both share the same id, so either resolves the same record.
    // Returns STATUS_NOT_SUPPORTED if h isn't such a handle at all - callers use this to fall through
    // to their existing same-process-only logic, exactly the STATUS_NOT_SUPPORTED they already return
    // unconditionally today for any handle that isn't process_context::is_current_process_handle.
    // Returns STATUS_INVALID_HANDLE if h names a record process_context::child_processes doesn't know,
    // STATUS_NOT_SUPPORTED if the record exists but has no live control channel (no spawning support
    // on this platform, or a snapshot-restored process whose children's channels were never
    // serialized), and STATUS_ACCESS_DENIED if required_access isn't fully covered by the record's
    // granted_access.
    //
    // A pseudo handle only ever carries {id, type} - there is nowhere to stash a narrowed per-handle
    // access mask the way a real Windows handle can via NtDuplicateObject, so the check below is
    // necessarily against child_process_record::granted_access as a whole, the mask granted once at
    // NtCreateUserProcess time. This is a genuine simplification versus real Windows semantics, though
    // an exact match for the scenario this feature targets (a sandbox broker that only ever duplicates
    // its process handle with DUPLICATE_SAME_ACCESS, never narrowing it).
    std::variant<child_target, NTSTATUS> resolve_child_target(const syscall_context& c, handle h, ACCESS_MASK required_access);

} // namespace sogen
