#pragma once
#include "../io_device.hpp"

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

    class named_pipe : public io_device_container
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

        // Backs FSCTL_PIPE_LISTEN: parks the listening thread on an event that sogen never signals, since
        // it has no cross-instance/cross-process named-pipe connect modeling. This matches real Windows
        // for a server whose real client never connects (e.g. Crashpad's own handler, which only ever gets
        // a connection when a monitored process reports a crash) -- ConnectNamedPipe genuinely blocks the
        // calling thread forever in that case too.
        handle listen_event{};

        void create(windows_emulator&, const io_device_creation_data&) override
        {
        }

        void work(windows_emulator&) override
        {
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

} // namespace sogen
