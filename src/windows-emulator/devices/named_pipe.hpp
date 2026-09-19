#pragma once
#include "../io_device.hpp"
#include "../process_context.hpp"
#include <utils/string.hpp>

namespace sogen
{
    // FSCTL_PIPE_PEEK = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 3, METHOD_BUFFERED, FILE_READ_DATA)
    constexpr ULONG FSCTL_PIPE_PEEK = 0x11400C;
    // FSCTL_PIPE_DISCONNECT = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_DISCONNECT = 0x110004;
    // FSCTL_PIPE_LISTEN = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_LISTEN = 0x110008;
    // FSCTL_PIPE_WAIT = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 6, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_WAIT = 0x110018;
    // FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE = 0x110030;
    // FSCTL_PIPE_GET_PIPE_ATTRIBUTE = CTL_CODE(FILE_DEVICE_NAMED_PIPE, 10, METHOD_BUFFERED, FILE_ANY_ACCESS)
    constexpr ULONG FSCTL_PIPE_GET_PIPE_ATTRIBUTE = 0x110028;
    constexpr ULONG FILE_PIPE_CONNECTED_STATE = 3;

    // Header of FILE_PIPE_PEEK_BUFFER; the peeked data follows immediately after.
    struct file_pipe_peek_buffer
    {
        ULONG named_pipe_state;
        ULONG read_data_available;
        ULONG number_of_messages;
        ULONG message_length;
    };

    // Strips the \Device\NamedPipe\, \??\Pipe\, or \DosDevices\Pipe\ prefix a pipe's full NT
    // path is stored under (see is_named_pipe_path, syscall_utils.hpp), leaving the bare name a
    // client passes to WaitNamedPipeW/CreateFile via \\.\pipe\<name>. Returns the input unchanged
    // if none of the prefixes match, so it is safe to call on an already-short name too.
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
        std::deque<std::string> write_queue;
        ACCESS_MASK access = 0;
        ULONG pipe_type;
        ULONG read_mode;
        ULONG completion_mode;
        ULONG max_instances;
        ULONG inbound_quota;
        ULONG outbound_quota;
        LARGE_INTEGER default_timeout;

        // True only for an instance created by the server side (NtCreateNamedPipeFile). A client's own
        // NtCreateFile connect (handle_named_pipe_create) creates a named_pipe object too, but it isn't a
        // server instance -- see windows_emulator::register_named_pipe_server, which this flag gates.
        bool is_server_instance{false};

        // Mirrors the handle's real synchronous-vs-overlapped mode: false iff the creating
        // NtCreateFile/NtCreateNamedPipeFile's CreateOptions carried neither FILE_SYNCHRONOUS_IO_ALERT nor
        // FILE_SYNCHRONOUS_IO_NONALERT, the NT-level signature of a Win32 FILE_FLAG_OVERLAPPED handle. Gates
        // try_deliver_read's, listen()'s, and wait()'s park-the-calling-thread behavior -- see there.
        bool is_synchronous_handle{true};

        // Backs a pended FSCTL_PIPE_WAIT (see wait()): parks the calling thread on an event that is
        // signaled once a server instance with the awaited name is registered (see
        // windows_emulator::register_named_pipe_server), whether that happened in this same process or
        // was forwarded from a sibling OS process over a pipe_ipc_channel.
        handle wait_event{};

        // Backs an overlapped FSCTL_PIPE_WAIT (see wait()): mirrors pending_listen's own replay-on-wake
        // pattern -- work() completes this once wait_event is signaled, delivering through the caller's
        // own event/APC/IOCP instead of parking the calling thread.
        std::optional<io_device_context> pending_wait{};

        // Set by a client's NtCreateFile on this same pipe name (see handle_named_pipe_create) when that
        // open happens before this server instance calls FSCTL_PIPE_LISTEN -- the common case for
        // same-process client/server pairs (e.g. mojo::PlatformChannel's CreateNamedPipe+CreateFile
        // self-connect idiom). Real ConnectNamedPipe returns FALSE with ERROR_PIPE_CONNECTED immediately
        // in that case rather than waiting, since there is nothing left to wait for.
        bool client_connected{false};

        // The connecting client's real Windows PID at the moment client_connected was set. Real NPFS
        // records this once, at connect time, and never updates it even if the connected handle is
        // later duplicated/inherited into a different process -- see get_connection_attribute. Known
        // for both a same-process connect (handle_named_pipe_create's own c.proc.process_id) and a
        // connect forwarded from a sibling OS process (the sender's own process_id, carried through
        // pipe_ipc_message::client_process_id -- e.g. the real, live-observed case of mojo's Windows
        // named-pipe bootstrap channel: server in this process, client connecting from a separately
        // spawned msedgewebview2.exe process).
        std::optional<uint32_t> client_process_id{};

        // Backs FSCTL_PIPE_LISTEN: parks the listening thread on an event that is signaled once a
        // client actually connects (see mark_client_connected), whether that connect happened in this
        // same process or was forwarded from a sibling OS process over a pipe_ipc_channel.
        handle listen_event{};

        // Backs a pended NtReadFile: parks the reading thread on an event signaled once data lands in
        // write_queue, whether pushed by a same-process peer instance (deliver_bytes_to_named_pipe) or
        // forwarded from a sibling OS process over a pipe_ipc_channel.
        handle read_ready_event{};
        std::optional<io_device_context> pending_read{};

        // Backs an overlapped FSCTL_PIPE_LISTEN (see listen()): unlike a synchronous handle, whose
        // listening thread parks on listen_event until a client connects, an overlapped handle must
        // return STATUS_PENDING to the caller immediately and deliver the eventual connect through the
        // caller's own event/APC/IOCP instead -- the same completion path complete_read() already uses
        // for a pended read. Captured here so work() can complete it once client_connected is set,
        // mirroring pending_read's own replay-on-wake pattern.
        std::optional<io_device_context> pending_listen{};

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

            if (this->pending_listen && this->client_connected)
            {
                if (std::getenv("SOGEN_TRACE_PIPE_IO"))
                {
                    win_emu.log.info("[pipe-io-trace] work() completing pending FSCTL_PIPE_LISTEN for pipe='%s', "
                                     "completion_port=%d event=0x%llx apc_routine=0x%llx\n",
                                     u16_to_u8(this->name).c_str(), this->get_completion_port().has_value(),
                                     static_cast<unsigned long long>(this->pending_listen->event.bits),
                                     static_cast<unsigned long long>(this->pending_listen->apc_routine));
                }

                this->client_connected = false;
                const auto ctx = *this->pending_listen;
                this->pending_listen.reset();
                this->complete_listen(win_emu, ctx);
            }

            if (this->pending_wait)
            {
                if (const auto* e = win_emu.process.events.get(this->wait_event); e && e->signaled)
                {
                    const auto ctx = *this->pending_wait;
                    this->pending_wait.reset();
                    this->complete_listen(win_emu, ctx);
                }
            }
        }

        // Called once a client is known to have connected to this pipe instance, whether observed
        // locally (handle_named_pipe_create's same-process device-table scan) or forwarded from a
        // sibling OS process (mark_named_pipe_connected). Wakes a thread already parked in
        // FSCTL_PIPE_LISTEN, in addition to the pre-existing synchronous client_connected check listen()
        // itself performs for the case where the client connects before the server ever listens.
        void mark_client_connected(windows_emulator& win_emu, std::optional<uint32_t> client_pid = std::nullopt)
        {
            this->client_connected = true;

            if (client_pid)
            {
                this->client_process_id = client_pid;
            }

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

            // A synchronous handle blocks here, exactly like real Windows: park the calling thread and
            // replay this call once read_ready_event is signaled (see work()). An overlapped handle must
            // return STATUS_PENDING to the caller immediately instead -- real completion still happens
            // later, out of work()/complete_read(), via the caller's event/APC/completion port.
            if (this->is_synchronous_handle)
            {
                auto& t = ctx.thread();
                t.await_objects = {this->read_ready_event};
                t.await_any = false;
                t.await_time = {};
                win_emu.yield_thread(*ctx.vcpu, false);
            }

            return STATUS_PENDING;
        }

        NTSTATUS io_control(windows_emulator& win_emu, const io_device_context& c) override
        {
            static const bool trace_pipe_io = std::getenv("SOGEN_TRACE_PIPE_IO") != nullptr;
            if (trace_pipe_io)
            {
                win_emu.log.info("[pipe-io-trace] FSCTL pipe='%s' code=0x%X tid=%u synchronous=%d\n", u16_to_u8(this->name).c_str(),
                                 static_cast<uint32_t>(c.io_control_code), c.thread().id, this->is_synchronous_handle);
            }

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

            if (c.io_control_code == FSCTL_PIPE_WAIT)
            {
                return this->wait(win_emu, c);
            }

            if (c.io_control_code == FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE)
            {
                return this->get_connection_attribute(win_emu, c);
            }

            if (c.io_control_code == FSCTL_PIPE_GET_PIPE_ATTRIBUTE)
            {
                return this->get_pipe_attribute(win_emu, c);
            }

            win_emu.log.warn("Unsupported named pipe FSCTL: 0x%X\n", static_cast<uint32_t>(c.io_control_code));
            return STATUS_NOT_SUPPORTED;
        }

        // GetNamedPipeClientProcessId/GetNamedPipeServerProcessId and their siblings (kernelbase.dll)
        // pass the requested attribute as a plain (non-Unicode), not necessarily NUL-terminated,
        // attribute-name string in the input buffer -- e.g. "ClientProcessId".
        static std::string read_attribute_name(windows_emulator& win_emu, const io_device_context& c)
        {
            std::string attribute_name(c.input_buffer_length, '\0');
            win_emu.emu().read_memory(c.input_buffer, attribute_name.data(), attribute_name.size());
            while (!attribute_name.empty() && attribute_name.back() == '\0')
            {
                attribute_name.pop_back();
            }

            return attribute_name;
        }

        // Backs GetNamedPipeClientProcessId/GetNamedPipeClientSessionId. Only ClientProcessId is
        // implemented -- the one attribute this investigation has observed queried live (mojo's
        // Windows named-pipe transport, right after a successful WebView2Environment creation); any
        // other attribute name still reports unsupported rather than fabricating a value never
        // verified against real behavior.
        NTSTATUS get_connection_attribute(windows_emulator& win_emu, const io_device_context& c)
        {
            if (!c.input_buffer || c.input_buffer_length == 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attribute_name = read_attribute_name(win_emu, c);

            if (attribute_name == "ClientProcessId" && this->client_process_id && c.output_buffer &&
                c.output_buffer_length >= sizeof(ULONG))
            {
                const ULONG pid = *this->client_process_id;
                win_emu.emu().write_memory(c.output_buffer, &pid, sizeof(pid));
                return STATUS_SUCCESS;
            }

            win_emu.log.warn("Unsupported named pipe connection attribute: '%s'\n", attribute_name.c_str());
            return STATUS_NOT_SUPPORTED;
        }

        // Backs GetNamedPipeServerProcessId/GetNamedPipeServerSessionId. Only ServerProcessId is
        // implemented, for the same reason get_connection_attribute only implements ClientProcessId --
        // it is always simply this process's own real Windows PID, since it is by construction the
        // process that owns the server end of its own named_pipe device instances.
        NTSTATUS get_pipe_attribute(windows_emulator& win_emu, const io_device_context& c)
        {
            if (!c.input_buffer || c.input_buffer_length == 0)
            {
                return STATUS_INVALID_PARAMETER;
            }

            const auto attribute_name = read_attribute_name(win_emu, c);

            if (attribute_name == "ServerProcessId" && c.output_buffer && c.output_buffer_length >= sizeof(ULONG))
            {
                const ULONG pid = win_emu.process.process_id;
                win_emu.emu().write_memory(c.output_buffer, &pid, sizeof(pid));
                return STATUS_SUCCESS;
            }

            win_emu.log.warn("Unsupported named pipe attribute: '%s'\n", attribute_name.c_str());
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

            if (!this->is_synchronous_handle)
            {
                if (std::getenv("SOGEN_TRACE_PIPE_IO"))
                {
                    win_emu.log.info("[pipe-io-trace] listen() pending (overlapped) for pipe='%s' tid=%u, completion_port=%d event=0x%llx "
                                     "apc_routine=0x%llx\n",
                                     u16_to_u8(this->name).c_str(), c.thread().id, this->get_completion_port().has_value(),
                                     static_cast<unsigned long long>(c.event.bits), static_cast<unsigned long long>(c.apc_routine));
                }

                this->pending_listen = c;
                return STATUS_PENDING;
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

        // Backs WaitNamedPipeW, issued against a handle to the NamedPipe filesystem's root
        // (\Device\NamedPipe\), not a specific pipe instance -- the input buffer (FILE_PIPE_WAIT_FOR_BUFFER:
        // LARGE_INTEGER Timeout; ULONG NameLength; BOOLEAN TimeoutSpecified; WCHAR Name[1];, Name at offset
        // 14) names the pipe to wait for. A server instance might not exist yet at this point -- e.g. the
        // client racing ahead of the server's own NtCreateNamedPipeFile call -- so this parks the same way
        // listen() does, rather than answering STATUS_SUCCESS unconditionally, matching real Windows
        // WaitNamedPipeW blocking until a server instance is actually created. An overlapped handle instead
        // pends (see pending_wait) and is completed later from work(), the same way listen()'s own
        // pending_listen path is.
        NTSTATUS wait(windows_emulator& win_emu, const io_device_context& c)
        {
            constexpr size_t name_offset = 14;
            if (!c.input_buffer || c.input_buffer_length < name_offset)
            {
                return STATUS_SUCCESS;
            }

            auto& emu = win_emu.emu();
            const auto name_length = emu.read_memory<uint32_t>(c.input_buffer + 8);
            const auto name_bytes = std::min<size_t>(name_length, c.input_buffer_length - name_offset);

            std::u16string target_name(name_bytes / sizeof(char16_t), u'\0');
            if (!target_name.empty())
            {
                emu.read_memory(c.input_buffer + name_offset, target_name.data(), target_name.size() * sizeof(char16_t));
            }

            if (target_name.empty() || win_emu.is_named_pipe_server_known(target_name))
            {
                return STATUS_SUCCESS;
            }

            if (!this->wait_event.bits)
            {
                event e{};
                e.type = NotificationEvent;
                e.signaled = false;
                this->wait_event = win_emu.process.events.store(std::move(e));
            }

            win_emu.register_pipe_wait(target_name, this->wait_event);

            if (!this->is_synchronous_handle)
            {
                this->pending_wait = c;
                return STATUS_PENDING;
            }

            auto& t = c.thread();
            t.await_objects = {this->wait_event};
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

            if (std::getenv("SOGEN_TRACE_PIPE_IO_BYTES"))
            {
                win_emu.log.info("[pipe-io-bytes-trace] NtReadFile pipe='%s' length=%zu tid=%u bytes=%s\n", u16_to_u8(this->name).c_str(),
                                 to_copy, ctx.thread().id, utils::string::to_hex_string(data.substr(0, to_copy)).c_str());
            }

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

            // Same WOW64 IoStatusBlock-widening gap complete_listen() restamps into ApcContext -- see
            // there for the full rationale. Unlike complete_listen()'s always-zero completion, a read
            // carries a real byte count, so information32 is derived from to_copy instead of hardcoded.
            if (win_emu.process.is_wow64_process && ctx.apc_context)
            {
                constexpr uint32_t status32 = STATUS_SUCCESS;
                const auto information32 = static_cast<uint32_t>(to_copy);
                win_emu.emu().write_memory(ctx.apc_context, &status32, sizeof(status32));
                win_emu.emu().write_memory(ctx.apc_context + sizeof(status32), &information32, sizeof(information32));
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

        // Completes a deferred overlapped FSCTL_PIPE_LISTEN (see listen()'s pending_listen path) once a
        // client has connected, exactly like complete_read() does for a pended read: caller-supplied
        // event, WoW64-aware APC, and an I/O completion port packet if one is associated with this
        // handle. A real ConnectNamedPipe completion carries no output data, so unlike complete_read()
        // there is nothing to copy -- only the completion itself (Information = 0). Reused as-is by
        // work()'s pending_wait completion (FSCTL_PIPE_WAIT), which needs the identical no-payload shape.
        NTSTATUS complete_listen(windows_emulator& win_emu, const io_device_context& ctx)
        {
            if (std::getenv("SOGEN_TRACE_PIPE_IO"))
            {
                win_emu.log.info(
                    "[pipe-io-trace] complete_listen pipe='%s' io_status_block=0x%llx is_wow64=%d apc_routine=0x%llx event=0x%llx\n",
                    u16_to_u8(this->name).c_str(), static_cast<unsigned long long>(ctx.io_status_block.value()),
                    win_emu.process.is_wow64_process ? 1 : 0, static_cast<unsigned long long>(ctx.apc_routine),
                    static_cast<unsigned long long>(ctx.event.bits));
            }

            if (ctx.io_status_block)
            {
                IO_STATUS_BLOCK<EmulatorTraits<Emu64>> block{};
                block.Information = 0;
                ctx.io_status_block.write(block);
            }

            // Real Windows' WOW64 syscall thunking widens IoStatusBlock into a transient, per-thread
            // scratch IO_STATUS_BLOCK for the duration of the NtFsControlFile call itself; the write
            // above lands there, not in memory the guest ever looks at again. ApcContext is left
            // untouched by that widening -- every Win32 overlapped-I/O wrapper (ConnectNamedPipe,
            // ReadFile, WriteFile, ...) sets it to the caller's own LPOVERLAPPED, which is also what a
            // caller polling via GetOverlappedResult (no APC, no I/O completion port) actually reads.
            // Restamp the real completion there too, since without it that caller never observes it.
            if (win_emu.process.is_wow64_process && ctx.apc_context)
            {
                constexpr uint32_t status32 = STATUS_SUCCESS;
                constexpr uint32_t information32 = 0;
                win_emu.emu().write_memory(ctx.apc_context, &status32, sizeof(status32));
                win_emu.emu().write_memory(ctx.apc_context + sizeof(status32), &information32, sizeof(information32));
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
                    constexpr uint32_t information32 = 0;
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
                    .io_information = 0,
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
                    message.io_status_block.Information = 0;
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
    // Used both for a local client's NtCreateFile (handle_named_pipe_create, which passes its own
    // c.proc.process_id as client_pid) and for a connect forwarded from a sibling OS process (which
    // passes the sender's own PID, carried over the wire in pipe_ipc_message::client_process_id -- see
    // named_pipe::client_process_id and broadcast_named_pipe_connect).
    inline void mark_named_pipe_connected(windows_emulator& win_emu, process_context& proc, const std::u16string_view name,
                                          std::optional<uint32_t> client_pid = std::nullopt)
    {
        static const bool trace_pipe_io = std::getenv("SOGEN_TRACE_PIPE_IO") != nullptr;

        bool matched = false;
        for (auto& entry : proc.devices)
        {
            if (auto* pipe = entry.second.get_internal_device<named_pipe>())
            {
                if (trace_pipe_io)
                {
                    win_emu.log.info("[pipe-io-trace] mark_named_pipe_connected candidate pipe='%s' incoming='%s' match=%d\n",
                                     u16_to_u8(pipe->name).c_str(), u16_to_u8(name).c_str(), pipe->name == name);
                }

                if (pipe->name == name)
                {
                    matched = true;
                    pipe->mark_client_connected(win_emu, client_pid);
                }
            }
        }

        if (trace_pipe_io && !matched)
        {
            win_emu.log.info("[pipe-io-trace] mark_named_pipe_connected NO MATCH for incoming='%s'\n", u16_to_u8(name).c_str());
        }
    }

} // namespace sogen
