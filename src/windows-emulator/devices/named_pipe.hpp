#pragma once
#include "../io_device.hpp"
#include "../windows_emulator.hpp"
#include "../named_objects.hpp"
#include "named_pipe_end_transfer.hpp"
#include <utils/host_pipe.hpp>
#include <utils/string.hpp>

namespace sogen
{
    // FSCTL_PIPE_PEEK = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 3, METHOD_BUFFERED, FILE_READ_DATA)
    constexpr ULONG FSCTL_PIPE_PEEK = 0x11400C;
    constexpr ULONG FSCTL_PIPE_DISCONNECT = 0x110004;
    constexpr ULONG FSCTL_PIPE_LISTEN = 0x110008;
    constexpr ULONG FSCTL_PIPE_WAIT = 0x110018;
    // FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE = 0x110030;
    // FSCTL_PIPE_GET_PIPE_ATTRIBUTE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 10, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_GET_PIPE_ATTRIBUTE = 0x110028;
    // FSCTL_PIPE_TRANSCEIVE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 5, METHOD_NEITHER, FILE_READ_DATA | FILE_WRITE_DATA)
    constexpr ULONG FSCTL_PIPE_TRANSCEIVE = 0x11C017;
    // FSCTL_PIPE_IMPERSONATE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_IMPERSONATE = 0x11001C;
    constexpr ULONG FILE_PIPE_CONNECTED_STATE = 3;

    struct file_pipe_wait_for_buffer
    {
        LARGE_INTEGER timeout;
        ULONG name_length;
        BOOLEAN timeout_specified;
        std::array<char16_t, 1> name;
    };

    // Header of FILE_PIPE_PEEK_BUFFER; the peeked data follows immediately after.
    struct file_pipe_peek_buffer
    {
        ULONG named_pipe_state;
        ULONG read_data_available;
        ULONG number_of_messages;
        ULONG message_length;
    };

    // The bare name a client passes to WaitNamedPipeW/CreateFile via \\.\pipe\<name>, as published in the
    // tree-wide pipe server registry (named_objects::publish_pipe_server).
    inline std::u16string_view pipe_short_name(const std::u16string_view full_name)
    {
        for (const std::u16string_view prefix : {std::u16string_view(u"\\Device\\NamedPipe\\"), std::u16string_view(u"\\??\\Pipe\\"),
                                                 std::u16string_view(u"\\DosDevices\\Pipe\\")})
        {
            if (utils::string::starts_with_ignore_case(full_name, prefix))
            {
                return full_name.substr(prefix.size());
            }
        }

        return full_name;
    }

    class named_pipe : public io_device
    {
      public:
        std::u16string name;
        std::shared_ptr<utils::host_pipe> endpoint;
        std::shared_ptr<utils::host_pipe> unopened_peer;
        ULONG pipe_type{};
        ULONG read_mode{};
        ULONG completion_mode{};
        ULONG max_instances{};
        ULONG inbound_quota{};
        ULONG outbound_quota{};
        ULONG create_options{};
        bool server_end{};
        bool signaled{true};
        LARGE_INTEGER default_timeout{};

        // Identifies the connection across host processes: one client end is paired with exactly one server
        // instance, and bytes travelling between host processes are routed by name and this id.
        uint64_t connection_id{};
        std::optional<uint32_t> client_process_id{};

        // Stands in for the peer end when that peer lives in another host process: bytes the peer sends are
        // written into it, and whatever this end sends to the peer is drained from it and forwarded.
        std::shared_ptr<utils::host_pipe> relay;

        void create(windows_emulator&, const io_device_creation_data&) override
        {
        }

        void work(windows_emulator& win_emu) override;
        bool cancel_io(windows_emulator& win_emu, uint64_t io_status_block, std::optional<uint32_t> issuing_thread_id = {}) override;
        bool has_pending_io(uint32_t issuing_thread_id) const override;
        void release_references(process_context& process) override;
        NTSTATUS start_io(windows_emulator& win_emu, io_device_context context, bool reading);
        void copy_configuration(const named_pipe& source);
        void complete_waits(windows_emulator& win_emu, const windows_path& pipe_path, NTSTATUS status);
        void complete_waits_if_last_server_end(windows_emulator& win_emu) const;
        void create_endpoints();
        void create_relay();
        void queue_relay_bytes(std::string_view data);
        void publish_server_name(process_context& process);

        NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override;

        void serialize_object(utils::buffer_serializer& buffer) const override;
        void deserialize_object(utils::buffer_deserializer& buffer) override;

      private:
        enum class operation_kind
        {
            read,
            write,
            listen,
            wait,
            decided,
        };

        struct awaited_pipe
        {
            std::u16string name;
            std::optional<std::chrono::steady_clock::time_point> deadline;

            bool has_expired(utils::clock& clock) const
            {
                return this->deadline && *this->deadline <= clock.steady_now();
            }
        };

        struct pending_operation
        {
            io_device_context context;
            std::unique_ptr<utils::host_pipe_operation> operation{};
            handle issuing_thread{};
            operation_kind kind{};
            awaited_pipe awaited{};
            NTSTATUS decided_status{};
        };

        struct operation_result
        {
            NTSTATUS status{};
            uint32_t transferred{};
        };

        std::vector<pending_operation> pending_;
        std::deque<std::string> relay_outbox_;
        std::unique_ptr<utils::host_pipe_operation> relay_write_;
        std::string relay_message_;
        std::unique_ptr<kernel_slot> published_server_name_;

        NTSTATUS start_listen(windows_emulator& win_emu, io_device_context context);
        NTSTATUS start_wait(windows_emulator& win_emu, io_device_context context);
        NTSTATUS prepare_wait(windows_emulator& win_emu, const io_device_context& context, awaited_pipe& awaited) const;
        NTSTATUS transceive(windows_emulator& win_emu, const io_device_context& context);
        NTSTATUS get_connection_attribute(windows_emulator& win_emu, const io_device_context& context) const;
        NTSTATUS get_pipe_attribute(windows_emulator& win_emu, const io_device_context& context) const;
        NTSTATUS peek(windows_emulator& win_emu, const io_device_context& c) const;
        bool is_directory() const;
        bool is_synchronous() const;
        bool reads_messages() const;
        NTSTATUS disconnect(windows_emulator& win_emu);
        bool is_disconnected() const;
        void complete_pending_operations(windows_emulator& win_emu, NTSTATUS status);
        NTSTATUS enqueue_operation(windows_emulator& win_emu, io_device_context context,
                                   std::unique_ptr<utils::host_pipe_operation> operation, operation_kind kind, awaited_pipe awaited = {},
                                   NTSTATUS decided_status = STATUS_PENDING, bool complete_inline = true);
        void flush_relay_outbox();
        void forward_relay_bytes(windows_emulator& win_emu);

        operation_result poll(windows_emulator& win_emu, const pending_operation& operation) const;
        void complete(windows_emulator& win_emu, const pending_operation& operation, operation_result result, bool was_pending);
        static void release_operation(process_context& process, pending_operation& operation);
    };

    struct named_pipe_instances
    {
        named_pipe* any{};
        named_pipe* listening{};
    };

    named_pipe_instances find_named_pipe_instances(process_context& process, std::u16string_view name, const named_pipe* ignored = nullptr);
    void complete_named_pipe_waits(windows_emulator& win_emu, std::u16string_view name, NTSTATUS status);

    uint64_t allocate_pipe_connection_id(process_context& process);

    // Pairs a client end created in this process with a listening server instance of the same process.
    void connect_named_pipe_ends(process_context& process, named_pipe& server, named_pipe& client);

    // Connects a client end created in this process to the pipe named by client.name: a listening local
    // instance, or a server instance published by another host process of the tree.
    NTSTATUS connect_named_pipe_client_end(windows_emulator& win_emu, named_pipe& client);

    // Applies a connect forwarded from another host process to a listening local server instance, queueing it
    // while all instances are busy. Returns false if this process serves no instance of the name.
    bool connect_named_pipe_client(windows_emulator& win_emu, process_context& process, std::u16string_view name, uint64_t connection_id,
                                   std::optional<uint32_t> client_process_id);

    // Hands a connect queued while all instances were busy to a server instance that listens again.
    void accept_pending_pipe_connection(windows_emulator& win_emu, named_pipe& server);

    // Applies bytes forwarded from another host process to the end of the connection they are headed to.
    void deliver_bytes_to_named_pipe(process_context& process, std::u16string_view name, uint64_t connection_id, bool to_server,
                                     std::string_view data);

    named_pipe_end_transfer export_named_pipe_end(process_context& process, named_pipe& pipe);
    void import_named_pipe_end(process_context& process, named_pipe& pipe, const named_pipe_end_transfer& transfer);

} // namespace sogen
