#include "../std_include.hpp"
#include "named_pipe.hpp"
#include "../windows_emulator.hpp"
#include "../io_completion_wait.hpp"
#include <algorithm>
#include <limits>
#include <utils/finally.hpp>

namespace sogen
{
    namespace
    {
        NTSTATUS translate_pipe_status(const utils::pipe_io_status status)
        {
            switch (status)
            {
            case utils::pipe_io_status::success:
                return STATUS_SUCCESS;
            case utils::pipe_io_status::pending:
                return STATUS_PENDING;
            case utils::pipe_io_status::broken:
                return STATUS_PIPE_BROKEN;
            case utils::pipe_io_status::cancelled:
                return STATUS_CANCELLED;
            case utils::pipe_io_status::disconnected:
                return STATUS_PIPE_DISCONNECTED;
            case utils::pipe_io_status::more_data:
                return STATUS_BUFFER_OVERFLOW;
            default:
                return STATUS_UNSUCCESSFUL;
            }
        }

        constexpr auto wait_name_offset = static_cast<ULONG>(offsetof(file_pipe_wait_for_buffer, name));

        std::optional<file_pipe_wait_for_buffer> read_wait_header(const windows_emulator& win_emu, const io_device_context& context)
        {
            file_pipe_wait_for_buffer header{};
            if (context.input_buffer_length < wait_name_offset ||
                !win_emu.memory.try_read_memory(context.input_buffer, &header, wait_name_offset))
            {
                return std::nullopt;
            }
            return header;
        }

        std::optional<std::u16string> read_awaited_pipe_name(const windows_emulator& win_emu, const io_device_context& context,
                                                             const file_pipe_wait_for_buffer& header)
        {
            if (header.name_length > context.input_buffer_length - wait_name_offset)
            {
                return std::nullopt;
            }
            std::u16string name(header.name_length / sizeof(char16_t), u'\0');
            if (!win_emu.memory.try_read_memory(context.input_buffer + wait_name_offset, name.data(), name.size() * sizeof(char16_t)))
            {
                return std::nullopt;
            }
            constexpr std::u16string_view directory = u"\\Device\\NamedPipe\\";
            auto pipe_name = std::u16string(directory) + name;
            if (windows_path(pipe_name) == windows_path(directory))
            {
                return std::nullopt;
            }
            return pipe_name;
        }

        std::optional<std::chrono::steady_clock::time_point> wait_deadline(windows_emulator& win_emu,
                                                                           const file_pipe_wait_for_buffer& header,
                                                                           const LARGE_INTEGER default_timeout)
        {
            constexpr auto infinite = std::chrono::steady_clock::time_point::min();
            const auto timeout = header.timeout_specified ? header.timeout : default_timeout;
            const auto deadline = utils::convert_delay_interval_to_time_point(win_emu.clock(), timeout);
            if (deadline == infinite)
            {
                return std::nullopt;
            }
            return deadline;
        }

        constexpr uint32_t relay_read_size = 0x10000;

        bool is_same_pipe(const named_pipe& pipe, const windows_path& path)
        {
            return windows_path(pipe.name) == path;
        }

        // GetNamedPipeClientProcessId and its siblings pass the attribute name as a plain, not necessarily
        // NUL-terminated string, e.g. "ClientProcessId".
        std::optional<std::string> read_attribute_name(windows_emulator& win_emu, const io_device_context& context)
        {
            if (!context.input_buffer || context.input_buffer_length == 0)
            {
                return std::nullopt;
            }
            std::string attribute_name(context.input_buffer_length, '\0');
            if (!win_emu.memory.try_read_memory(context.input_buffer, attribute_name.data(), attribute_name.size()))
            {
                return std::nullopt;
            }
            while (!attribute_name.empty() && attribute_name.back() == '\0')
            {
                attribute_name.pop_back();
            }
            return attribute_name;
        }

        NTSTATUS write_process_id(windows_emulator& win_emu, const io_device_context& context, const ULONG process_id)
        {
            if (!context.output_buffer || context.output_buffer_length < sizeof(process_id) ||
                !win_emu.memory.try_write_memory(context.output_buffer, &process_id, sizeof(process_id)))
            {
                return STATUS_INVALID_PARAMETER;
            }
            if (context.io_status_block)
            {
                context.io_status_block.access(
                    [](IO_STATUS_BLOCK<EmulatorTraits<Emu64>>& status_block) { status_block.Information = sizeof(ULONG); });
            }
            return STATUS_SUCCESS;
        }

        void claim_listening_instance(windows_emulator& win_emu, named_pipe& server, const uint64_t connection_id,
                                      const std::optional<uint32_t> client_process_id)
        {
            server.relay = std::move(server.unopened_peer);
            server.connection_id = connection_id;
            server.client_process_id = client_process_id;
            win_emu.wake_idle_vcpu();
        }

        named_pipe* find_connected_peer(process_context& process, const named_pipe& pipe)
        {
            const windows_path path{pipe.name};
            for (auto& device : process.devices | std::views::values)
            {
                auto* peer = device.get_internal_device<named_pipe>();
                if (peer && peer != &pipe && peer->server_end != pipe.server_end && peer->connection_id == pipe.connection_id &&
                    is_same_pipe(*peer, path))
                {
                    return peer;
                }
            }
            return nullptr;
        }
    }

    void named_pipe_end_transfer::serialize(utils::buffer_serializer& buffer) const
    {
        buffer.write(this->name);
        buffer.write(this->server_end);
        buffer.write(this->connection_id);
        buffer.write_optional(this->client_process_id);
        buffer.write(this->pipe_type);
        buffer.write(this->read_mode);
        buffer.write(this->completion_mode);
        buffer.write(this->max_instances);
        buffer.write(this->inbound_quota);
        buffer.write(this->outbound_quota);
        buffer.write(this->create_options);
        buffer.write(this->default_timeout);
        buffer.write(this->unread_data);
    }

    void named_pipe_end_transfer::deserialize(utils::buffer_deserializer& buffer)
    {
        buffer.read(this->name);
        buffer.read(this->server_end);
        buffer.read(this->connection_id);
        buffer.read_optional(this->client_process_id);
        buffer.read(this->pipe_type);
        buffer.read(this->read_mode);
        buffer.read(this->completion_mode);
        buffer.read(this->max_instances);
        buffer.read(this->inbound_quota);
        buffer.read(this->outbound_quota);
        buffer.read(this->create_options);
        buffer.read(this->default_timeout);
        buffer.read(this->unread_data);
    }

    void named_pipe::copy_configuration(const named_pipe& source)
    {
        this->name = source.name;
        this->pipe_type = source.pipe_type;
        this->read_mode = source.read_mode;
        this->completion_mode = source.completion_mode;
        this->max_instances = source.max_instances;
        this->inbound_quota = source.inbound_quota;
        this->outbound_quota = source.outbound_quota;
        this->create_options = source.create_options;
        this->default_timeout = source.default_timeout;
    }

    named_pipe_instances find_named_pipe_instances(process_context& process, const std::u16string_view name, const named_pipe* ignored)
    {
        named_pipe_instances instances{};
        const windows_path path{name};
        for (auto& device : process.devices | std::views::values)
        {
            auto* pipe = device.get_internal_device<named_pipe>();
            if (!pipe || pipe == ignored || !pipe->server_end || windows_path(pipe->name) != path)
            {
                continue;
            }
            instances.any = pipe;
            if (pipe->unopened_peer)
            {
                instances.listening = pipe;
                break;
            }
        }
        return instances;
    }

    void complete_named_pipe_waits(windows_emulator& win_emu, const std::u16string_view name, const NTSTATUS status)
    {
        const windows_path path{name};
        for (auto& device : win_emu.process.devices | std::views::values)
        {
            if (auto* pipe = device.get_internal_device<named_pipe>())
            {
                pipe->complete_waits(win_emu, path, status);
            }
        }
    }

    void named_pipe::complete_waits(windows_emulator& win_emu, const windows_path& pipe_path, const NTSTATUS status)
    {
        for (auto it = this->pending_.begin(); it != this->pending_.end();)
        {
            if (it->kind != operation_kind::wait || windows_path(it->awaited.name) != pipe_path)
            {
                ++it;
                continue;
            }
            this->complete(win_emu, *it, {.status = status}, true);
            release_operation(win_emu.process, *it);
            it = this->pending_.erase(it);
        }
    }

    void named_pipe::complete_waits_if_last_server_end(windows_emulator& win_emu) const
    {
        if (this->server_end && !find_named_pipe_instances(win_emu.process, this->name, this).any)
        {
            win_emu.unregister_named_pipe_server(pipe_short_name(this->name));
            complete_named_pipe_waits(win_emu, this->name, STATUS_OBJECT_NAME_NOT_FOUND);
        }
    }

    void named_pipe::create_endpoints()
    {
        auto endpoints =
            utils::host_pipe::create_pair(this->pipe_type != 0, std::max<ULONG>({this->inbound_quota, this->outbound_quota, 4096}));
        this->endpoint = std::move(endpoints.first);
        this->unopened_peer = std::move(endpoints.second);
    }

    // The relay keeps the boundaries of the writes forwarded through it, as the peer's pipe type is not known here.
    void named_pipe::create_relay()
    {
        auto endpoints = utils::host_pipe::create_pair(true, std::max<ULONG>({this->inbound_quota, this->outbound_quota, 4096}));
        this->endpoint = std::move(endpoints.first);
        this->relay = std::move(endpoints.second);
    }

    void named_pipe::queue_relay_bytes(const std::string_view data)
    {
        this->relay_outbox_.emplace_back(data);
        this->flush_relay_outbox();
    }

    void named_pipe::flush_relay_outbox()
    {
        while (this->relay)
        {
            if (!this->relay_write_)
            {
                if (this->relay_outbox_.empty())
                {
                    return;
                }
                this->relay_write_ = this->relay->write(this->relay_outbox_.front());
            }
            if (this->relay_write_->check_completion().status == utils::pipe_io_status::pending)
            {
                return;
            }
            this->relay_write_.reset();
            this->relay_outbox_.pop_front();
        }
    }

    void named_pipe::forward_relay_bytes(windows_emulator& win_emu)
    {
        while (this->relay)
        {
            const auto operation = this->relay->read(relay_read_size, true);
            const auto result = operation->check_completion();
            if (result.status != utils::pipe_io_status::success && result.status != utils::pipe_io_status::more_data)
            {
                return;
            }
            this->relay_message_.append(operation->data());
            if (result.status == utils::pipe_io_status::more_data)
            {
                continue;
            }
            win_emu.broadcast_named_pipe_write(this->name, this->connection_id, !this->server_end, this->relay_message_);
            this->relay_message_.clear();
        }
    }

    void named_pipe::publish_server_name(process_context& process)
    {
        if (this->server_end && !this->published_server_name_)
        {
            this->published_server_name_ = named_objects::publish_pipe_server(process, pipe_short_name(this->name));
        }
    }

    bool named_pipe::reads_messages() const
    {
        return this->read_mode != 0;
    }

    bool named_pipe::is_disconnected() const
    {
        return !this->endpoint || this->endpoint->is_disconnected();
    }

    void named_pipe::complete(windows_emulator& win_emu, const pending_operation& operation, const operation_result result,
                              const bool was_pending)
    {
        const auto& context = operation.context;
        const auto status = result.status;
        this->signaled = true;
        if (operation.kind == operation_kind::read && result.transferred)
        {
            win_emu.memory.write_memory(context.output_buffer, operation.operation->data().data(), result.transferred);
        }
        if (was_pending && context.wow64_io_status_block && !this->is_synchronous())
        {
            context.wow64_io_status_block.write(
                {.Status = status, .Information = static_cast<EmulatorTraits<Emu32>::ULONG_PTR>(result.transferred)});
        }
        else
        {
            context.io_status_block.write({.Status = status, .Information = result.transferred});
            write_io_status(context, status);
        }

        if (auto* event = win_emu.process.events.get(context.event))
        {
            event->set_signaled(true);
        }
        // Windows queues no APC and posts no completion packet for an overlapped request that fails immediately; only its
        // caller learns of the failure.
        if (!was_pending && NT_ERROR(status))
        {
            win_emu.wake_idle_vcpu();
            return;
        }

        if (context.apc_routine)
        {
            if (auto* thread = win_emu.process.threads.get(operation.issuing_thread))
            {
                thread->pending_apcs.push_back({.apc_routine = context.apc_routine,
                                                .apc_argument1 = context.apc_context,
                                                .apc_argument2 = context.wow64_io_status_block ? context.wow64_io_status_block.value()
                                                                                               : context.io_status_block.value(),
                                                .apc_argument3 = 0,
                                                .restamp_io_status_block = static_cast<bool>(context.wow64_io_status_block),
                                                .io_status = static_cast<int32_t>(status),
                                                .io_information = result.transferred});
            }
        }
        else if (was_pending || !NT_SUCCESS(status) || !(context.completion_notification_flags & 1))
        {
            io_device::queue_io_completion(win_emu, context, {.Status = status, .Information = result.transferred});
        }

        win_emu.wake_idle_vcpu();
    }

    NTSTATUS named_pipe::start_io(windows_emulator& win_emu, io_device_context context, const bool reading)
    {
        if (!this->endpoint)
        {
            return this->server_end ? STATUS_PIPE_DISCONNECTED : STATUS_INVALID_HANDLE;
        }
        if (!context.io_status_block)
        {
            return STATUS_INVALID_HANDLE;
        }
        const auto required_access =
            reading ? (FILE_READ_DATA | GENERIC_READ | GENERIC_ALL) : (FILE_WRITE_DATA | GENERIC_WRITE | GENERIC_ALL);
        if (!(context.granted_access & required_access))
        {
            return STATUS_ACCESS_DENIED;
        }
        if ((context.event.bits && !win_emu.process.events.get(context.event)) ||
            (context.completion_port.bits && !win_emu.process.io_completions.get(context.completion_port)))
        {
            return STATUS_INVALID_HANDLE;
        }
        if (auto* event = win_emu.process.events.get(context.event))
        {
            event->set_signaled(false);
        }
        std::unique_ptr<utils::host_pipe_operation> operation;
        if (reading)
        {
            operation = this->endpoint->read(context.output_buffer_length, this->reads_messages());
        }
        else
        {
            if (!is_memory_readable(win_emu.memory, context.input_buffer, context.input_buffer_length))
            {
                return STATUS_ACCESS_VIOLATION;
            }
            std::string data(context.input_buffer_length, '\0');
            win_emu.memory.read_memory(context.input_buffer, data.data(), data.size());
            operation = this->endpoint->write(data);
        }
        return this->enqueue_operation(win_emu, context, std::move(operation), reading ? operation_kind::read : operation_kind::write);
    }

    NTSTATUS named_pipe::start_listen(windows_emulator& win_emu, io_device_context context)
    {
        if ((!this->endpoint && !this->server_end) || !context.io_status_block)
        {
            return STATUS_INVALID_HANDLE;
        }
        if (!this->endpoint)
        {
            this->create_endpoints();
            complete_named_pipe_waits(win_emu, this->name, STATUS_SUCCESS);
            accept_pending_pipe_connection(win_emu, *this);
        }
        if (!this->unopened_peer)
        {
            return STATUS_PIPE_CONNECTED;
        }
        if (this->completion_mode != 0)
        {
            return STATUS_PIPE_LISTENING;
        }
        return this->enqueue_operation(win_emu, context, nullptr, operation_kind::listen);
    }

    bool named_pipe::is_directory() const
    {
        return !this->endpoint && !this->server_end && windows_path(this->name) == windows_path(u"\\Device\\NamedPipe\\");
    }

    bool named_pipe::is_synchronous() const
    {
        return (this->create_options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) != 0;
    }

    NTSTATUS named_pipe::start_wait(windows_emulator& win_emu, io_device_context context)
    {
        awaited_pipe awaited{};
        const auto status = this->prepare_wait(win_emu, context, awaited);
        if (status == STATUS_PENDING)
        {
            return this->enqueue_operation(win_emu, context, nullptr, operation_kind::wait, std::move(awaited));
        }
        if (this->is_synchronous())
        {
            return status;
        }
        return this->enqueue_operation(win_emu, context, nullptr, operation_kind::decided, {}, status);
    }

    NTSTATUS named_pipe::prepare_wait(windows_emulator& win_emu, const io_device_context& context, awaited_pipe& awaited) const
    {
        if (!this->is_directory())
        {
            return STATUS_ILLEGAL_FUNCTION;
        }
        const auto header = read_wait_header(win_emu, context);
        if (!header)
        {
            return STATUS_INVALID_PARAMETER;
        }
        auto awaited_name = read_awaited_pipe_name(win_emu, context, *header);
        if (!awaited_name)
        {
            return STATUS_INVALID_PARAMETER;
        }
        const auto instances = find_named_pipe_instances(win_emu.process, *awaited_name);
        if (instances.listening)
        {
            return STATUS_SUCCESS;
        }
        if (!instances.any)
        {
            // Whether an instance served by another host process is busy is not known here; the connect waits
            // there until one listens.
            return win_emu.is_named_pipe_server_known(pipe_short_name(*awaited_name)) ? STATUS_SUCCESS : STATUS_OBJECT_NAME_NOT_FOUND;
        }
        awaited = {.name = std::move(*awaited_name), .deadline = wait_deadline(win_emu, *header, instances.any->default_timeout)};
        return STATUS_PENDING;
    }

    NTSTATUS named_pipe::disconnect(windows_emulator& win_emu)
    {
        if (this->is_disconnected())
        {
            return STATUS_PIPE_DISCONNECTED;
        }
        if (!this->server_end)
        {
            return STATUS_ILLEGAL_FUNCTION;
        }
        this->endpoint->disconnect();
        this->endpoint.reset();
        this->unopened_peer.reset();
        this->relay.reset();
        this->relay_outbox_.clear();
        this->relay_write_.reset();
        this->relay_message_.clear();
        this->connection_id = 0;
        this->client_process_id.reset();
        this->complete_pending_operations(win_emu, STATUS_PIPE_DISCONNECTED);
        const windows_path instance_path{this->name};
        for (auto& device : win_emu.process.devices | std::views::values)
        {
            if (auto* pipe = device.get_internal_device<named_pipe>(); pipe && pipe != this && windows_path(pipe->name) == instance_path)
            {
                pipe->work(win_emu);
            }
        }
        return STATUS_SUCCESS;
    }

    void named_pipe::complete_pending_operations(windows_emulator& win_emu, const NTSTATUS status)
    {
        for (auto& operation : this->pending_)
        {
            if (operation.operation)
            {
                operation.operation->cancel();
            }
            const auto result = operation.kind == operation_kind::decided ? operation.decided_status : status;
            this->complete(win_emu, operation, {.status = result}, true);
            release_operation(win_emu.process, operation);
        }
        this->pending_.clear();
    }

    NTSTATUS named_pipe::enqueue_operation(windows_emulator& win_emu, io_device_context context,
                                           std::unique_ptr<utils::host_pipe_operation> operation, const operation_kind kind,
                                           awaited_pipe awaited, const NTSTATUS decided_status, const bool complete_inline)
    {
        context.issuing_thread_id = context.thread().id;
        pending_operation pending{.context = context,
                                  .operation = std::move(operation),
                                  .issuing_thread = win_emu.process.threads.find_handle(&context.thread()),
                                  .kind = kind,
                                  .awaited = std::move(awaited),
                                  .decided_status = decided_status};
        std::array<handle, 3> retained_references{};
        const std::array<handle*, 3> references{&pending.context.event, &pending.context.completion_port, &pending.issuing_thread};
        for (size_t i = 0; i < references.size(); ++i)
        {
            if (!io_completion_wait::retain_handle_reference(win_emu.process, context.vcpu->active_thread, *references[i],
                                                             retained_references[i]))
            {
                for (auto& retained : retained_references)
                {
                    io_completion_wait::release_handle_reference(win_emu.process, retained);
                }
                return STATUS_INVALID_HANDLE;
            }
        }
        for (size_t i = 0; i < references.size(); ++i)
        {
            *references[i] = retained_references[i];
        }
        bool queued{};
        const auto cleanup = utils::finally([&] {
            if (!queued)
            {
                release_operation(win_emu.process, pending);
            }
        });
        this->signaled = false;
        const auto result = pending.operation && complete_inline ? pending.operation->check_completion()
                                                                 : utils::pipe_io_result{.status = utils::pipe_io_status::pending};
        if (result.status != utils::pipe_io_status::pending)
        {
            const auto status = translate_pipe_status(result.status);
            this->complete(win_emu, pending, {.status = status, .transferred = result.transferred}, false);
            return status;
        }
        pending.context.vcpu = nullptr;
        context.io_status_block.write({.Status = STATUS_PENDING, .Information = 0});
        write_io_status(context, STATUS_PENDING);
        if (auto* event = win_emu.process.events.get(context.event))
        {
            event->set_signaled(false);
        }
        this->pending_.push_back(std::move(pending));
        queued = true;
        if (this->create_options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT))
        {
            context.thread().await_file_io = context.io_status_block.value();
            win_emu.yield_thread(*context.vcpu);
        }
        return STATUS_PENDING;
    }

    void named_pipe::release_operation(process_context& process, pending_operation& operation)
    {
        io_completion_wait::release_handle_reference(process, operation.context.event);
        io_completion_wait::release_handle_reference(process, operation.context.completion_port);
        io_completion_wait::release_handle_reference(process, operation.issuing_thread);
    }

    named_pipe::operation_result named_pipe::poll(windows_emulator& win_emu, const pending_operation& operation) const
    {
        switch (operation.kind)
        {
        case operation_kind::listen:
            return {.status = this->unopened_peer ? STATUS_PENDING : STATUS_SUCCESS};
        case operation_kind::decided:
            return {.status = operation.decided_status};
        case operation_kind::wait: {
            const auto instances = find_named_pipe_instances(win_emu.process, operation.awaited.name);
            if (instances.listening)
            {
                return {.status = STATUS_SUCCESS};
            }
            if (!instances.any)
            {
                return {.status = STATUS_OBJECT_NAME_NOT_FOUND};
            }
            return {.status = operation.awaited.has_expired(win_emu.clock()) ? STATUS_IO_TIMEOUT : STATUS_PENDING};
        }
        default: {
            const auto result = operation.operation->check_completion();
            return {.status = translate_pipe_status(result.status), .transferred = result.transferred};
        }
        }
    }

    void named_pipe::work(windows_emulator& win_emu)
    {
        this->flush_relay_outbox();
        for (auto it = this->pending_.begin(); it != this->pending_.end();)
        {
            const auto result = this->poll(win_emu, *it);
            if (result.status == STATUS_PENDING)
            {
                ++it;
                continue;
            }
            this->complete(win_emu, *it, result, true);
            release_operation(win_emu.process, *it);
            it = this->pending_.erase(it);
        }
        this->forward_relay_bytes(win_emu);
    }

    bool named_pipe::has_pending_io(const uint32_t issuing_thread_id) const
    {
        return std::ranges::any_of(this->pending_, [issuing_thread_id](const auto& operation) {
            return operation.context.issuing_thread_id == issuing_thread_id;
        });
    }

    bool named_pipe::cancel_io(windows_emulator& win_emu, const uint64_t io_status_block, const std::optional<uint32_t> issuing_thread_id)
    {
        bool cancelled{};
        for (auto it = this->pending_.begin(); it != this->pending_.end();)
        {
            if (issuing_thread_id && it->context.issuing_thread_id != *issuing_thread_id)
            {
                ++it;
                continue;
            }
            if (io_status_block && it->context.io_status_block.value() != io_status_block &&
                it->context.wow64_io_status_block.value() != io_status_block)
            {
                ++it;
                continue;
            }
            if (it->operation)
            {
                it->operation->cancel();
            }
            const auto result =
                it->operation ? it->operation->check_completion() : utils::pipe_io_result{.status = utils::pipe_io_status::cancelled};
            if (result.status == utils::pipe_io_status::pending)
            {
                cancelled = true;
                ++it;
                continue;
            }
            this->complete(win_emu, *it, {.status = translate_pipe_status(result.status), .transferred = result.transferred}, true);
            release_operation(win_emu.process, *it);
            it = this->pending_.erase(it);
            cancelled = true;
        }
        return cancelled;
    }

    void named_pipe::release_references(process_context& process)
    {
        for (auto& operation : this->pending_)
        {
            if (operation.operation)
            {
                operation.operation->cancel();
            }
            release_operation(process, operation);
        }
        this->pending_.clear();
    }

    void named_pipe::serialize_object(utils::buffer_serializer& buffer) const
    {
        buffer.write(this->name);
        buffer.write(this->pipe_type);
        buffer.write(this->read_mode);
        buffer.write(this->completion_mode);
        buffer.write(this->max_instances);
        buffer.write(this->inbound_quota);
        buffer.write(this->outbound_quota);
        buffer.write(this->create_options);
        buffer.write(this->server_end);
        buffer.write(this->signaled);
        buffer.write(this->default_timeout);
        buffer.write(this->connection_id);
        buffer.write_optional(this->client_process_id);
        buffer.write_vector(std::vector<std::string>(this->relay_outbox_.begin(), this->relay_outbox_.end()));
        buffer.write(this->relay_message_);
        buffer.write(static_cast<bool>(this->relay_write_));
        if (this->relay_write_)
        {
            this->relay_write_->serialize(buffer);
        }
        for (const auto& pipe : {this->endpoint, this->unopened_peer, this->relay})
        {
            buffer.write(static_cast<bool>(pipe));
            if (pipe)
            {
                pipe->serialize(buffer);
            }
        }
        buffer.write(static_cast<uint64_t>(this->pending_.size()));
        for (const auto& pending : this->pending_)
        {
            buffer.write(pending.context);
            buffer.write(static_cast<bool>(pending.operation));
            if (pending.operation)
            {
                pending.operation->serialize(buffer);
            }
            buffer.write(pending.issuing_thread);
            buffer.write(pending.kind);
            buffer.write(pending.awaited.name);
            buffer.write_optional(pending.awaited.deadline);
            buffer.write(pending.decided_status);
        }
    }

    void named_pipe::deserialize_object(utils::buffer_deserializer& buffer)
    {
        buffer.read(this->name);
        buffer.read(this->pipe_type);
        buffer.read(this->read_mode);
        buffer.read(this->completion_mode);
        buffer.read(this->max_instances);
        buffer.read(this->inbound_quota);
        buffer.read(this->outbound_quota);
        buffer.read(this->create_options);
        buffer.read(this->server_end);
        buffer.read(this->signaled);
        buffer.read(this->default_timeout);
        buffer.read(this->connection_id);
        buffer.read_optional(this->client_process_id);
        std::vector<std::string> outbox;
        buffer.read_vector(outbox);
        this->relay_outbox_.assign(outbox.begin(), outbox.end());
        buffer.read(this->relay_message_);
        this->relay_write_ = buffer.read<bool>() ? std::make_unique<utils::host_pipe_operation>(buffer) : nullptr;
        for (auto* pipe : {&this->endpoint, &this->unopened_peer, &this->relay})
        {
            *pipe = buffer.read<bool>() ? utils::host_pipe::deserialize(buffer) : nullptr;
        }
        this->pending_.clear();
        const auto count = buffer.read<uint64_t>();
        for (uint64_t i = 0; i < count; ++i)
        {
            pending_operation pending{.context = buffer.read<io_device_context>()};
            if (buffer.read<bool>())
            {
                pending.operation = std::make_unique<utils::host_pipe_operation>(buffer);
            }
            buffer.read(pending.issuing_thread);
            buffer.read(pending.kind);
            buffer.read(pending.awaited.name);
            buffer.read_optional(pending.awaited.deadline);
            buffer.read(pending.decided_status);
            this->pending_.push_back(std::move(pending));
        }
    }

    NTSTATUS named_pipe::io_control(windows_emulator& win_emu, const io_device_context& c)
    {
        static const bool trace_pipe_io = std::getenv("SOGEN_TRACE_PIPE_IO") != nullptr;
        if (trace_pipe_io)
        {
            win_emu.log.info("[pipe-io-trace] FSCTL pipe='%s' code=0x%X synchronous=%d\n", u16_to_u8(this->name).c_str(),
                             static_cast<uint32_t>(c.io_control_code), this->is_synchronous());
        }

        switch (c.io_control_code)
        {
        case FSCTL_PIPE_LISTEN:
            return this->start_listen(win_emu, c);
        case FSCTL_PIPE_PEEK:
            return this->peek(win_emu, c);
        case FSCTL_PIPE_WAIT:
            return this->start_wait(win_emu, c);
        case FSCTL_PIPE_DISCONNECT:
            return this->disconnect(win_emu);
        case FSCTL_PIPE_TRANSCEIVE:
            return this->transceive(win_emu, c);
        case FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE:
            return this->get_connection_attribute(win_emu, c);
        case FSCTL_PIPE_GET_PIPE_ATTRIBUTE:
            return this->get_pipe_attribute(win_emu, c);
        case FSCTL_PIPE_IMPERSONATE:
            // There is no per-thread impersonation model to switch the server thread into; callers such as crashpad's
            // ExceptionHandlerServer only need ImpersonateNamedPipeClient to succeed.
            return STATUS_SUCCESS;
        default:
            win_emu.log.warn("Unsupported named pipe FSCTL: 0x%X\n", static_cast<uint32_t>(c.io_control_code));
            return STATUS_NOT_SUPPORTED;
        }
    }

    NTSTATUS named_pipe::transceive(windows_emulator& win_emu, const io_device_context& context)
    {
        if (!context.input_buffer || !context.input_buffer_length || !context.output_buffer)
        {
            return STATUS_INVALID_PARAMETER;
        }
        if (!this->endpoint)
        {
            return this->server_end ? STATUS_PIPE_DISCONNECTED : STATUS_INVALID_HANDLE;
        }
        if (!context.io_status_block)
        {
            return STATUS_INVALID_HANDLE;
        }
        if (!is_memory_readable(win_emu.memory, context.input_buffer, context.input_buffer_length))
        {
            return STATUS_ACCESS_VIOLATION;
        }

        std::string request(context.input_buffer_length, '\0');
        win_emu.memory.read_memory(context.input_buffer, request.data(), request.size());
        const auto write = this->endpoint->write(request);
        const auto written = write->check_completion();
        if (written.status == utils::pipe_io_status::pending)
        {
            write->cancel();
            return STATUS_PIPE_BUSY;
        }
        if (written.status != utils::pipe_io_status::success)
        {
            return translate_pipe_status(written.status);
        }

        // The reply is always delivered through the pending path: execute_ioctl completes a request that returns
        // anything but STATUS_PENDING on its own.
        return this->enqueue_operation(win_emu, context, this->endpoint->read(context.output_buffer_length, this->reads_messages()),
                                       operation_kind::read, {}, STATUS_PENDING, false);
    }

    // Only ClientProcessId and ServerProcessId are implemented; other attributes stay unsupported rather than
    // reporting values never checked against Windows.
    NTSTATUS named_pipe::get_connection_attribute(windows_emulator& win_emu, const io_device_context& context) const
    {
        const auto attribute_name = read_attribute_name(win_emu, context);
        if (!attribute_name)
        {
            return STATUS_INVALID_PARAMETER;
        }
        if (*attribute_name == "ClientProcessId" && this->client_process_id)
        {
            return write_process_id(win_emu, context, *this->client_process_id);
        }

        win_emu.log.warn("Unsupported named pipe connection attribute: '%s'\n", attribute_name->c_str());
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS named_pipe::get_pipe_attribute(windows_emulator& win_emu, const io_device_context& context) const
    {
        const auto attribute_name = read_attribute_name(win_emu, context);
        if (!attribute_name)
        {
            return STATUS_INVALID_PARAMETER;
        }
        if (*attribute_name == "ServerProcessId")
        {
            return write_process_id(win_emu, context, win_emu.process.process_id);
        }

        win_emu.log.warn("Unsupported named pipe attribute: '%s'\n", attribute_name->c_str());
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS named_pipe::peek(windows_emulator& win_emu, const io_device_context& c) const
    {
        constexpr auto header_size = static_cast<ULONG>(sizeof(file_pipe_peek_buffer));
        if (!c.output_buffer || c.output_buffer_length < header_size)
        {
            return STATUS_INVALID_PARAMETER;
        }

        if (!this->endpoint)
        {
            return this->server_end ? STATUS_INVALID_PIPE_STATE : STATUS_INVALID_HANDLE;
        }
        uint32_t available{};
        std::string data;
        const auto result = this->endpoint->peek(data, c.output_buffer_length - header_size, available);
        if (result.status == utils::pipe_io_status::disconnected)
        {
            return STATUS_PIPE_DISCONNECTED;
        }
        if (result.status != utils::pipe_io_status::success)
        {
            return result.status == utils::pipe_io_status::broken ? STATUS_PIPE_BROKEN : STATUS_UNSUCCESSFUL;
        }

        file_pipe_peek_buffer header{};
        header.named_pipe_state = FILE_PIPE_CONNECTED_STATE;
        header.read_data_available = static_cast<ULONG>(available);
        header.number_of_messages = 0;
        header.message_length = 0;

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

    uint64_t allocate_pipe_connection_id(process_context& process)
    {
        return (static_cast<uint64_t>(process.process_id) << 32) | ++process.next_pipe_connection_serial;
    }

    void connect_named_pipe_ends(process_context& process, named_pipe& server, named_pipe& client)
    {
        client.endpoint = std::move(server.unopened_peer);
        const auto connection_id = allocate_pipe_connection_id(process);
        server.connection_id = connection_id;
        client.connection_id = connection_id;
        server.client_process_id = process.process_id;
        client.client_process_id = process.process_id;
    }

    NTSTATUS connect_named_pipe_client_end(windows_emulator& win_emu, named_pipe& client)
    {
        auto& process = win_emu.process;
        const auto instances = find_named_pipe_instances(process, client.name);
        if (instances.listening)
        {
            const auto create_options = client.create_options;
            client.copy_configuration(*instances.listening);
            client.create_options = create_options;
            connect_named_pipe_ends(process, *instances.listening, client);
            return STATUS_SUCCESS;
        }
        if (instances.any)
        {
            return STATUS_PIPE_NOT_AVAILABLE;
        }
        if (!win_emu.is_named_pipe_server_known(pipe_short_name(client.name)))
        {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }

        client.create_relay();
        client.connection_id = allocate_pipe_connection_id(process);
        client.client_process_id = process.process_id;
        win_emu.broadcast_named_pipe_connect(client.name, client.connection_id, process.process_id);
        return STATUS_SUCCESS;
    }

    bool connect_named_pipe_client(windows_emulator& win_emu, process_context& process, const std::u16string_view name,
                                   const uint64_t connection_id, const std::optional<uint32_t> client_process_id)
    {
        const windows_path path{name};
        bool serves_name = false;
        for (auto& device : process.devices | std::views::values)
        {
            auto* pipe = device.get_internal_device<named_pipe>();
            if (!pipe || !pipe->server_end || !is_same_pipe(*pipe, path))
            {
                continue;
            }
            serves_name = true;
            if (pipe->unopened_peer)
            {
                claim_listening_instance(win_emu, *pipe, connection_id, client_process_id);
                return true;
            }
        }

        if (serves_name)
        {
            process.pending_pipe_connections.push_back(
                {.name = std::u16string(name), .connection_id = connection_id, .client_process_id = client_process_id});
        }
        return serves_name;
    }

    void accept_pending_pipe_connection(windows_emulator& win_emu, named_pipe& server)
    {
        if (!server.unopened_peer)
        {
            return;
        }
        auto& pending_connections = win_emu.process.pending_pipe_connections;
        const windows_path path{server.name};
        const auto pending = std::ranges::find_if(
            pending_connections, [&](const pending_pipe_connection& connection) { return windows_path(connection.name) == path; });
        if (pending == pending_connections.end())
        {
            return;
        }

        claim_listening_instance(win_emu, server, pending->connection_id, pending->client_process_id);
        for (const auto& data : pending->client_bytes)
        {
            server.queue_relay_bytes(data);
        }
        pending_connections.erase(pending);
    }

    void deliver_bytes_to_named_pipe(process_context& process, const std::u16string_view name, const uint64_t connection_id,
                                     const bool to_server, const std::string_view data)
    {
        const windows_path path{name};
        for (auto& device : process.devices | std::views::values)
        {
            auto* pipe = device.get_internal_device<named_pipe>();
            if (pipe && pipe->relay && pipe->server_end == to_server && pipe->connection_id == connection_id && is_same_pipe(*pipe, path))
            {
                pipe->queue_relay_bytes(data);
                return;
            }
        }

        if (!to_server)
        {
            return;
        }

        for (auto& pending : process.pending_pipe_connections)
        {
            if (pending.connection_id == connection_id && windows_path(pending.name) == path)
            {
                pending.client_bytes.emplace_back(data);
                return;
            }
        }
    }

    named_pipe_end_transfer export_named_pipe_end(process_context& process, named_pipe& pipe)
    {
        if (pipe.endpoint && !pipe.connection_id)
        {
            pipe.connection_id = allocate_pipe_connection_id(process);
        }

        named_pipe_end_transfer transfer{
            .name = pipe.name,
            .server_end = pipe.server_end,
            .connection_id = pipe.connection_id,
            .client_process_id = pipe.client_process_id,
            .pipe_type = pipe.pipe_type,
            .read_mode = pipe.read_mode,
            .completion_mode = pipe.completion_mode,
            .max_instances = pipe.max_instances,
            .inbound_quota = pipe.inbound_quota,
            .outbound_quota = pipe.outbound_quota,
            .create_options = pipe.create_options,
            .default_timeout = pipe.default_timeout,
        };

        if (!pipe.endpoint)
        {
            return transfer;
        }

        if (pipe.relay)
        {
            uint32_t available{};
            (void)pipe.endpoint->peek(transfer.unread_data, std::numeric_limits<uint32_t>::max(), available);
        }
        else if (auto* peer = find_connected_peer(process, pipe); peer && !peer->relay)
        {
            // The peer forwards everything it sends to this end from now on; both host processes then read from the
            // same queue, as two handles to one pipe end do on Windows.
            peer->relay = pipe.endpoint;
        }

        return transfer;
    }

    void import_named_pipe_end(process_context& process, named_pipe& pipe, const named_pipe_end_transfer& transfer)
    {
        pipe.name = transfer.name;
        pipe.server_end = transfer.server_end;
        pipe.connection_id = transfer.connection_id;
        pipe.client_process_id = transfer.client_process_id;
        pipe.pipe_type = transfer.pipe_type;
        pipe.read_mode = transfer.read_mode;
        pipe.completion_mode = transfer.completion_mode;
        pipe.max_instances = transfer.max_instances;
        pipe.inbound_quota = transfer.inbound_quota;
        pipe.outbound_quota = transfer.outbound_quota;
        pipe.create_options = transfer.create_options;
        pipe.default_timeout = transfer.default_timeout;

        if (transfer.connection_id)
        {
            pipe.create_relay();
            if (!transfer.unread_data.empty())
            {
                pipe.queue_relay_bytes(transfer.unread_data);
            }
        }
        else if (transfer.server_end)
        {
            pipe.create_endpoints();
        }

        pipe.publish_server_name(process);
    }
}
