#pragma once
#include "../io_device.hpp"
#include "../process_context.hpp"

namespace sogen
{
    // FSCTL_PIPE_PEEK = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 3, METHOD_BUFFERED, FILE_READ_DATA)
    constexpr ULONG FSCTL_PIPE_PEEK = 0x11400C;
    // FSCTL_PIPE_DISCONNECT = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_DISCONNECT = 0x110004;
    // FSCTL_PIPE_LISTEN = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_LISTEN = 0x110008;
    constexpr ULONG FILE_PIPE_CONNECTED_STATE = 3;

    // Header of FILE_PIPE_PEEK_BUFFER; the peeked data follows immediately after.
    struct file_pipe_peek_buffer
    {
        ULONG named_pipe_state;
        ULONG read_data_available;
        ULONG number_of_messages;
        ULONG message_length;
    };

    class named_pipe : public io_device
    {
      public:
        std::u16string name;
        std::deque<std::string> write_queue;
        ACCESS_MASK access = 0;
        ULONG pipe_type;
        ULONG read_mode;
        ULONG completion_mode;
        ULONG max_instances;
        ULONG inbound_quota;
        ULONG outbound_quota;
        LARGE_INTEGER default_timeout;

        // Set by a client's NtCreateFile on this same pipe name (see handle_named_pipe_create) when that
        // open happens before this server instance calls FSCTL_PIPE_LISTEN -- the common case for
        // same-process client/server pairs (e.g. mojo::PlatformChannel's CreateNamedPipe+CreateFile
        // self-connect idiom). Real ConnectNamedPipe returns FALSE with ERROR_PIPE_CONNECTED immediately
        // in that case rather than waiting, since there is nothing left to wait for.
        bool client_connected{false};

        // Backs FSCTL_PIPE_LISTEN: parks the listening thread on an event that is signaled once a
        // client actually connects (see mark_client_connected), whether that connect happened in this
        // same process or was forwarded from a sibling OS process over a pipe_ipc_channel.
        handle listen_event{};

        // Backs a pended NtReadFile: parks the reading thread on an event signaled once data lands in
        // write_queue, whether pushed by a same-process peer instance (deliver_bytes_to_named_pipe) or
        // forwarded from a sibling OS process over a pipe_ipc_channel.
        handle read_ready_event{};
        std::optional<io_device_context> pending_read{};

        void create(windows_emulator&, const io_device_creation_data&) override
        {
        }

        void work(windows_emulator& win_emu) override
        {
            if (this->pending_read && !this->write_queue.empty())
            {
                const auto ctx = *this->pending_read;
                this->complete_read(win_emu, ctx);
                this->pending_read.reset();

                if (auto* e = win_emu.process.events.get(this->read_ready_event))
                {
                    e->signaled = true;
                }
            }
        }

        // Called once a client is known to have connected to this pipe instance, whether observed
        // locally (handle_named_pipe_create's same-process device-table scan) or forwarded from a
        // sibling OS process (mark_named_pipe_connected). Wakes a thread already parked in
        // FSCTL_PIPE_LISTEN, in addition to the pre-existing synchronous client_connected check listen()
        // itself performs for the case where the client connects before the server ever listens.
        void mark_client_connected(windows_emulator& win_emu)
        {
            this->client_connected = true;

            if (this->listen_event.bits)
            {
                if (auto* e = win_emu.process.events.get(this->listen_event))
                {
                    e->signaled = true;
                }
            }
        }

        // Delivers a read from write_queue if data is already available, or pends the request (parking
        // the calling thread on read_ready_event, mirroring listen()'s own park/wake pattern) until
        // work() finds data to deliver. Mirrors afd_endpoint's delayed_ioctl_ pattern: the whole request
        // context is captured and re-executed later from the device's own work() pump rather than the
        // originating syscall handler.
        NTSTATUS try_deliver_read(windows_emulator& win_emu, const io_device_context& ctx)
        {
            if (!this->write_queue.empty())
            {
                return this->complete_read(win_emu, ctx);
            }

            this->pending_read = ctx;

            if (!this->read_ready_event.bits)
            {
                event e{};
                e.type = NotificationEvent;
                e.signaled = false;
                this->read_ready_event = win_emu.process.events.store(std::move(e));
            }
            else if (auto* e = win_emu.process.events.get(this->read_ready_event))
            {
                e->signaled = false;
            }

            auto& t = ctx.thread();
            t.await_objects = {this->read_ready_event};
            t.await_any = false;
            t.await_time = {};
            win_emu.yield_thread(*ctx.vcpu, false);

            return STATUS_PENDING;
        }

        NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override
        {
            if (c.io_control_code == FSCTL_PIPE_PEEK)
            {
                return this->peek(win_emu, c);
            }

            if (c.io_control_code == FSCTL_PIPE_LISTEN)
            {
                return this->listen(win_emu, c);
            }

            if (c.io_control_code == FSCTL_PIPE_DISCONNECT)
            {
                // DisconnectNamedPipe is synchronous on real Windows -- it never waits for a peer, it just
                // tears down whatever connection state exists (none, here) immediately.
                return STATUS_SUCCESS;
            }

            win_emu.log.warn("Unsupported named pipe FSCTL: 0x%X\n", static_cast<uint32_t>(c.io_control_code));
            return STATUS_NOT_SUPPORTED;
        }

        void serialize_object(utils::buffer_serializer&) const override
        {
        }

        void deserialize_object(utils::buffer_deserializer&) override
        {
        }

      private:
        NTSTATUS listen(windows_emulator& win_emu, const io_device_context& c)
        {
            if (this->client_connected)
            {
                this->client_connected = false;
                return STATUS_PIPE_CONNECTED;
            }

            if (!this->listen_event.bits)
            {
                event e{};
                e.type = NotificationEvent;
                e.signaled = false;
                this->listen_event = win_emu.process.events.store(std::move(e));
            }

            auto& t = c.thread();
            t.await_objects = {this->listen_event};
            t.await_any = false;
            t.await_time = {};
            win_emu.yield_thread(*c.vcpu, false);

            return STATUS_PENDING;
        }

        // Pops as much of the front of write_queue as ctx's buffer holds and delivers it exactly like
        // deliver_file_io_completion (syscalls/file.cpp) does for a regular file: caller-supplied event,
        // WoW64-aware APC, and (independent of both) an I/O completion port packet if one is associated
        // with this handle. Used for both an immediate read (data already queued) and a pended one
        // completed later from work().
        NTSTATUS complete_read(windows_emulator& win_emu, const io_device_context& ctx)
        {
            const std::string_view data = this->write_queue.front();
            const size_t to_copy = std::min<size_t>(data.size(), ctx.output_buffer_length);

            win_emu.emu().write_memory(ctx.output_buffer, data.data(), to_copy);

            if (to_copy == data.size())
            {
                this->write_queue.pop_front();
            }
            else
            {
                this->write_queue.front().erase(0, to_copy);
            }

            if (ctx.io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = static_cast<uint32_t>(to_copy);
                ctx.io_status_block.write(block);
            }

            if (ctx.event.bits)
            {
                if (auto* e = win_emu.process.events.get(ctx.event))
                {
                    e->signaled = true;
                }
            }

            if (ctx.apc_routine)
            {
                if (win_emu.process.is_wow64_process && ctx.io_status_block)
                {
                    constexpr uint32_t status32 = STATUS_SUCCESS;
                    const auto information32 = static_cast<uint32_t>(to_copy);
                    win_emu.emu().write_memory(ctx.io_status_block.value(), &status32, sizeof(status32));
                    win_emu.emu().write_memory(ctx.io_status_block.value() + sizeof(status32), &information32, sizeof(information32));
                }

                ctx.thread().pending_apcs.push_back({
                    .flags = 0,
                    .apc_routine = ctx.apc_routine,
                    .apc_argument1 = ctx.apc_context,
                    .apc_argument2 = ctx.io_status_block.value(),
                    .apc_argument3 = 0,
                    .restamp_io_status_block = win_emu.process.is_wow64_process && static_cast<bool>(ctx.io_status_block),
                    .io_status = static_cast<int32_t>(STATUS_SUCCESS),
                    .io_information = static_cast<uint32_t>(to_copy),
                });
            }

            if (const auto association = this->get_completion_port())
            {
                if (auto* completion = win_emu.process.io_completions.get(association->port))
                {
                    io_completion_message message{};
                    message.key_context = association->key;
                    message.apc_context = ctx.apc_context;
                    message.io_status_block.Status = STATUS_SUCCESS;
                    message.io_status_block.Information = static_cast<uint32_t>(to_copy);
                    completion->enqueue(message);
                }
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS peek(windows_emulator& win_emu, const io_device_context& c)
        {
            constexpr auto header_size = static_cast<ULONG>(sizeof(file_pipe_peek_buffer));
            if (!c.output_buffer || c.output_buffer_length < header_size)
            {
                return STATUS_INVALID_PARAMETER;
            }

            size_t available = 0;
            for (const auto& chunk : this->write_queue)
            {
                available += chunk.size();
            }

            // Peeking is non-destructive: copy as much queued data as the caller's buffer holds.
            const auto data_capacity = static_cast<size_t>(c.output_buffer_length - header_size);
            std::string data;
            for (const auto& chunk : this->write_queue)
            {
                if (data.size() >= data_capacity)
                {
                    break;
                }

                data.append(chunk, 0, std::min(data_capacity - data.size(), chunk.size()));
            }

            file_pipe_peek_buffer header{};
            header.named_pipe_state = FILE_PIPE_CONNECTED_STATE;
            header.read_data_available = static_cast<ULONG>(available);
            header.number_of_messages = 0;
            header.message_length = this->write_queue.empty() ? 0 : static_cast<ULONG>(this->write_queue.front().size());

            auto& emu = win_emu.emu();
            emu.write_memory(c.output_buffer, &header, sizeof(header));
            if (!data.empty())
            {
                emu.write_memory(c.output_buffer + header_size, data.data(), data.size());
            }

            if (c.io_status_block)
            {
                c.io_status_block.access(
                    [&](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& sb) { sb.Information = header_size + static_cast<uint32_t>(data.size()); });
            }

            return available > data.size() ? STATUS_BUFFER_OVERFLOW : STATUS_SUCCESS;
        }
    };

    // Appends data to the write_queue of every named_pipe instance in proc.devices whose name matches,
    // other than exclude_self. Used both for same-process delivery (a write on one same-named handle
    // reaching another same-named handle in the same process's device table) and for applying a write
    // forwarded from a sibling OS process (exclude_self is null there, since the writer's own instance
    // isn't in this process's device table to begin with).
    inline void deliver_bytes_to_named_pipe(process_context& proc, const std::u16string_view name, const std::string_view data,
                                            const named_pipe* exclude_self)
    {
        for (auto& entry : proc.devices)
        {
            auto* pipe = entry.second.get_internal_device<named_pipe>();
            if (pipe && pipe != exclude_self && pipe->name == name)
            {
                pipe->write_queue.emplace_back(data);
            }
        }
    }

    // Marks every named_pipe instance in proc.devices whose name matches as having a connected client.
    // Used both for a local client's NtCreateFile (handle_named_pipe_create) and for a connect forwarded
    // from a sibling OS process.
    inline void mark_named_pipe_connected(windows_emulator& win_emu, process_context& proc, const std::u16string_view name)
    {
        for (auto& entry : proc.devices)
        {
            if (auto* pipe = entry.second.get_internal_device<named_pipe>(); pipe && pipe->name == name)
            {
                pipe->mark_client_connected(win_emu);
            }
        }
    }

} // namespace sogen
