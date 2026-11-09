#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sogen::host_ipc
{
    // Host-OS layer under the analyzer's multi-process machinery (windows-analyzer/child_process_spawn.cpp): one
    // sogen host process per guest process, joined by three byte-stream channels per parent/child pair (bootstrap +
    // pipe relay, process control, routing) that carry length-prefixed frames, with shared-memory section backings
    // passed out of band. The frame bytes are the same on every host; only the transport differs.
    //
    // POSIX: socketpair(AF_UNIX) channels, descriptors passed as SCM_RIGHTS on the length prefix, fork+execv,
    // SIGCHLD reaping, kill(). host_ipc_posix.cpp is the code that used to live in child_process_spawn.cpp.
    //
    // Windows: each channel is a duplex named pipe (unique name, PIPE_REJECT_REMOTE_CLIENTS, one instance) whose
    // client end the child inherits through PROC_THREAD_ATTRIBUTE_HANDLE_LIST; a section handle is passed by
    // DuplicateHandle into the peer process, its value travelling right after the length prefix (flag bit 63 of the
    // prefix, which no real frame reaches: frames are capped at 64 MiB). Children are created with CREATE_NO_WINDOW,
    // inherit the parent's desktop, and are put in a kill-on-close job object so they die with the parent.
    //
    // A channel or section "fd" is an int on both: a file descriptor on POSIX, a kernel handle value on Windows
    // (handle values fit in 32 bits, see HandleToLong). -1 is never a valid one.

    constexpr size_t max_fds_per_frame = 128;

    // POSIX hosts and Windows hosts; not Emscripten.
    bool supported();

    bool write_all(int fd, const void* data, size_t size);
    bool read_all(int fd, void* data, size_t size);

    // Writes the 8-byte frame length with `fds` attached (non-empty `fds`).
    bool write_length_with_fds(int fd, uint64_t length, std::span<const int> fds);
    // Reads an 8-byte frame length and any descriptors attached to it (appended to received_fds, not inheritable).
    bool read_length_with_fds(int fd, uint64_t& length, std::vector<int>& received_fds);

    void close_fd(int fd);

    // Waits up to timeout_ms (-1: forever) until `fd` has data or its peer has gone (a read then reports EOF).
    // False on timeout or error.
    bool wait_readable(int fd, int timeout_ms);

    // Non-blocking mode for read_some/write_some.
    void set_nonblocking(int fd);

    enum class io_status
    {
        done,        // bytes > 0 transferred
        would_block, // nothing right now
        eof,         // peer closed (reads only)
        error,
    };

    struct io_result
    {
        io_status status{io_status::error};
        size_t bytes{};
        // Raw return value of the underlying call and the host error code, for diagnostics only.
        ptrdiff_t raw{};
        int error_code{};
    };

    io_result write_some(int fd, const void* data, size_t size);
    io_result read_some(int fd, void* data, size_t size);

    std::string describe_error(int error_code);

    struct channel_pair
    {
        int parent_end{-1};
        int child_end{-1};
    };

    // Called once per spawn before any channel is created (POSIX: installs the SIGCHLD reaper).
    void prepare_child_spawning();

    // A connected duplex channel. On failure returns nullopt and sets `error`.
    std::optional<channel_pair> create_channel_pair(std::string& error);

    struct spawn_result
    {
        int pid{-1};
        std::string error{};
    };

    // Starts `executable` with `argv` (argv[0] included). `child_fds` are made available to the child under the same
    // values; `parent_only_fds` must not reach it. Does not wait for the child.
    spawn_result spawn_process(const std::filesystem::path& executable, const std::vector<std::string>& argv,
                               std::span<const int> child_fds, std::span<const int> parent_only_fds);

    // Records that `fd` is a channel to host process `pid` (needed on Windows to pass handles into it; no-op on POSIX).
    // A channel never bound this way is taken to lead to the parent process.
    void bind_channel_peer(int fd, int pid);

    bool process_alive(int pid);
    void kill_process(int pid);
    int current_process_id();
}
