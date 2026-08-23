#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sogen
{
    enum class pipe_ipc_message_type : uint8_t
    {
        write = 0,
        connect = 1,
        server_created = 2,
    };

    // A single forwarded named-pipe event: bytes written on the sender's end (delivered verbatim
    // to any same-named pipe instance on the receiving end), a client-connect notification
    // (mirrors the same-process client_connected/listen_event handshake across the process
    // boundary), or a server instance being created (mirrors the same-process
    // known-server/wait_event handshake -- see windows_emulator::register_named_pipe_server).
    struct pipe_ipc_message
    {
        pipe_ipc_message_type type{};
        std::u16string pipe_name{};
        std::string data{};
    };

    // Abstracts the transport that forwards named-pipe traffic to a sibling OS process spawned via
    // spawn_child_process (see child_process_spawn.hpp). windows_emulator only ever sends/receives
    // framed messages through this interface - it never touches the underlying fd, matching how
    // socket_factory/dns_lookup/clock are injected rather than implemented in-tree. The concrete,
    // socketpair-backed implementation lives in windows-analyzer (child_process_spawn.cpp), next to
    // the fork()/exec() logic that creates the transport in the first place.
    class pipe_ipc_channel
    {
      public:
        virtual ~pipe_ipc_channel() = default;

        virtual void send(const pipe_ipc_message& message) = 0;

        // Non-blocking: returns nullopt immediately if no complete message is available yet.
        virtual std::optional<pipe_ipc_message> try_receive() = 0;
    };

} // namespace sogen
