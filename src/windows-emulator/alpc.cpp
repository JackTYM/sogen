#include "std_include.hpp"
#include "alpc.hpp"

#include "kernel_state.hpp"
#include "pipe_ipc_channel.hpp"
#include "port.hpp"
#include "syscall_utils.hpp"
#include "windows_emulator.hpp"

#include <serialization.hpp>
#include <utils/time.hpp>

namespace sogen::alpc
{
    namespace
    {
        constexpr uint32_t basic_information = 0;
        constexpr uint32_t associate_completion_port_information = 2;

        constexpr ULONG security_attribute = 0x80000000;
        constexpr ULONG view_attribute = 0x40000000;
        constexpr ULONG context_attribute = 0x20000000;
        constexpr ULONG handle_attribute = 0x10000000;
        constexpr ULONG token_attribute = 0x08000000;
        constexpr ULONG direct_attribute = 0x04000000;
        constexpr ULONG work_on_behalf_attribute = 0x02000000;

        constexpr uint64_t default_max_message_length = 0xFFEF;

        enum class transport_kind : uint8_t
        {
            connect_request,
            connect_reply,
            message,
            reply,
            closed,
        };

        struct transport
        {
            transport_kind kind{};
            uint32_t sender_message_id{};
            uint32_t sender_process_id{};
            uint32_t sender_thread_id{};
            uint16_t type{};
            uint32_t callback_id{};
            NTSTATUS status{};
            bool expects_reply{};
            std::vector<uint8_t> data{};
            std::optional<std::vector<uint8_t>> view{};

            std::string encode() const
            {
                utils::buffer_serializer buffer{};
                buffer.write(static_cast<uint8_t>(this->kind));
                buffer.write(this->sender_message_id);
                buffer.write(this->sender_process_id);
                buffer.write(this->sender_thread_id);
                buffer.write(this->type);
                buffer.write(this->callback_id);
                buffer.write(this->status);
                buffer.write(this->expects_reply);
                buffer.write_vector(this->data);
                buffer.write(this->view.has_value());
                if (this->view)
                {
                    buffer.write_vector(*this->view);
                }

                const auto& bytes = buffer.get_buffer();
                return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            }

            static transport decode(const std::string& encoded)
            {
                const std::span bytes{reinterpret_cast<const std::byte*>(encoded.data()), encoded.size()};
                utils::buffer_deserializer buffer{bytes};

                transport result{};
                result.kind = static_cast<transport_kind>(buffer.read<uint8_t>());
                buffer.read(result.sender_message_id);
                buffer.read(result.sender_process_id);
                buffer.read(result.sender_thread_id);
                buffer.read(result.type);
                buffer.read(result.callback_id);
                buffer.read(result.status);
                buffer.read(result.expects_reply);
                buffer.read_vector(result.data);
                if (buffer.read<bool>())
                {
                    result.view.emplace();
                    buffer.read_vector(*result.view);
                }

                return result;
            }
        };

        struct addressed_transport
        {
            std::u16string port_name{};
            uint64_t connection_id{};
            bool to_server{};
            transport payload{};
        };

        port* find_port(state& s, const uint64_t id)
        {
            const auto it = s.ports.find(id);
            return it == s.ports.end() ? nullptr : &it->second;
        }

        uint32_t new_message_id(state& s)
        {
            if (s.next_message_id == 0)
            {
                s.next_message_id = 1;
            }

            return s.next_message_id++;
        }

        uint64_t new_port_id(state& s)
        {
            return s.next_port_id++;
        }

        std::optional<uint64_t> find_connection_port(const state& s, const std::u16string_view name)
        {
            for (const auto& [id, p] : s.ports)
            {
                if (p.kind == port_kind::connection && !p.disconnected && p.name == name)
                {
                    return id;
                }
            }

            return std::nullopt;
        }

        void notify_completion_port(windows_emulator& win_emu, const port& p)
        {
            if (!p.completion_port)
            {
                return;
            }

            if (auto* completion = win_emu.process.io_completions.get(*p.completion_port))
            {
                io_completion_message packet{};
                packet.key_context = p.completion_key;
                completion->enqueue(packet);
            }
        }

        void enqueue_message(windows_emulator& win_emu, port& p, message m)
        {
            p.queue.push_back(std::move(m));
            notify_completion_port(win_emu, p);
        }

        bool trace_enabled()
        {
            static const bool enabled = std::getenv("SOGEN_TRACE_ALPC") != nullptr;
            return enabled;
        }

        bool deliver_transport(windows_emulator& win_emu, const addressed_transport& t)
        {
            auto& s = win_emu.process.alpc;
            const auto& payload = t.payload;

            if (trace_enabled())
            {
                win_emu.log.info("[alpc] pid=%u deliver kind=%d conn=0x%llx to_server=%d sender_mid=%u type=%u bytes=%zu\n",
                                 win_emu.process.process_id, static_cast<int>(payload.kind),
                                 static_cast<unsigned long long>(t.connection_id), t.to_server, payload.sender_message_id, payload.type,
                                 payload.data.size());
                std::string hex{};
                for (size_t i = 0; i < std::min<size_t>(payload.data.size(), 96); ++i)
                {
                    char b[4];
                    snprintf(b, sizeof(b), "%02x", payload.data[i]);
                    hex += b;
                }
                win_emu.log.info("[alpc]   data %s\n", hex.c_str());
            }

            const auto make_message = [&](const uint32_t message_id, const uint64_t via_port_id) {
                return message{
                    .type = payload.type,
                    .sender_process_id = payload.sender_process_id,
                    .sender_thread_id = payload.sender_thread_id,
                    .message_id = message_id,
                    .callback_id = payload.callback_id,
                    .via_port_id = via_port_id,
                    .data = payload.data,
                    .view = payload.view,
                };
            };

            const auto local_endpoint = [&](const bool server_side) -> std::pair<uint64_t, port*> {
                auto& endpoints = server_side ? s.server_ports : s.client_ports;
                const auto it = endpoints.find(t.connection_id);
                if (it == endpoints.end())
                {
                    return {0, nullptr};
                }

                return {it->second, find_port(s, it->second)};
            };

            switch (payload.kind)
            {
            case transport_kind::connect_request: {
                const auto connection_port_id = find_connection_port(s, t.port_name);
                if (!connection_port_id)
                {
                    return false;
                }

                const auto id = new_message_id(s);
                s.pending[id] = pending_message{
                    .connection_id = t.connection_id,
                    .sender_message_id = payload.sender_message_id,
                    .from_client = true,
                    .connection_request = true,
                    .connection_port_id = *connection_port_id,
                };

                auto m = make_message(id, *connection_port_id);
                m.type = LPC_CONNECTION_REQUEST;
                enqueue_message(win_emu, s.ports.at(*connection_port_id), std::move(m));
                return true;
            }

            case transport_kind::connect_reply: {
                const auto [client_id, client] = local_endpoint(false);
                if (!client || client->connection_status)
                {
                    return false;
                }

                client->connection_status = payload.status;
                client->connection_reply = make_message(payload.sender_message_id, client_id);
                client->connection_reply->type = LPC_REPLY;
                return true;
            }

            case transport_kind::message: {
                const auto [endpoint_id, endpoint] = local_endpoint(t.to_server);
                if (!endpoint)
                {
                    return false;
                }

                if (endpoint->disconnected)
                {
                    return true;
                }

                const auto id = new_message_id(s);
                if (payload.expects_reply)
                {
                    s.pending[id] = pending_message{
                        .connection_id = t.connection_id,
                        .sender_message_id = payload.sender_message_id,
                        .from_client = t.to_server,
                    };
                }

                auto* target = t.to_server ? find_port(s, endpoint->connection_port_id) : endpoint;
                if (!target)
                {
                    return true;
                }

                enqueue_message(win_emu, *target, make_message(id, endpoint_id));
                return true;
            }

            case transport_kind::reply: {
                const auto [endpoint_id, endpoint] = local_endpoint(t.to_server);
                if (!endpoint)
                {
                    return false;
                }

                auto m = make_message(payload.sender_message_id, endpoint_id);
                m.type = LPC_REPLY;

                if (endpoint->awaited_replies.erase(payload.sender_message_id) != 0)
                {
                    endpoint->replies[payload.sender_message_id] = std::move(m);
                    return true;
                }

                auto* target = t.to_server ? find_port(s, endpoint->connection_port_id) : endpoint;
                if (target)
                {
                    enqueue_message(win_emu, *target, std::move(m));
                }

                return true;
            }

            case transport_kind::closed: {
                const auto [endpoint_id, endpoint] = local_endpoint(t.to_server);
                if (!endpoint)
                {
                    return false;
                }

                endpoint->disconnected = true;

                if (t.to_server)
                {
                    if (auto* connection_port = find_port(s, endpoint->connection_port_id))
                    {
                        auto m = make_message(new_message_id(s), endpoint_id);
                        m.type = LPC_PORT_CLOSED;
                        enqueue_message(win_emu, *connection_port, std::move(m));
                    }
                }

                return true;
            }
            }

            return false;
        }

        void route(windows_emulator& win_emu, const addressed_transport& t)
        {
            if (deliver_transport(win_emu, t))
            {
                return;
            }

            win_emu.broadcast_alpc(t.port_name, t.connection_id, t.to_server, t.payload.encode());
        }

        uint64_t attribute_offset(const ULONG allocated, const ULONG attribute, const bool wow64)
        {
            struct attribute_size
            {
                ULONG flag;
                uint64_t size64;
                uint64_t size32;
            };

            constexpr std::array<attribute_size, 7> sizes{{
                {security_attribute, 0x18, 0x0C},
                {view_attribute, 0x20, 0x10},
                {context_attribute, 0x20, 0x14},
                {handle_attribute, 0x18, 0x10},
                {token_attribute, 0x18, 0x18},
                {direct_attribute, 0x08, 0x04},
                {work_on_behalf_attribute, 0x08, 0x08},
            }};

            uint64_t offset = sizeof(ALPC_MESSAGE_ATTRIBUTES);
            for (const auto& entry : sizes)
            {
                if (entry.flag == attribute)
                {
                    break;
                }

                if (allocated & entry.flag)
                {
                    offset += wow64 ? entry.size32 : entry.size64;
                }
            }

            return offset;
        }

        void write_receive_attributes(windows_emulator& win_emu, const uint64_t attributes_address, const message& m, const bool wow64)
        {
            if (!attributes_address)
            {
                return;
            }

            auto& emu = win_emu.emu();
            auto header = emu.read_memory<ALPC_MESSAGE_ATTRIBUTES>(attributes_address);
            header.ValidAttributes = 0;

            if (header.AllocatedAttributes & context_attribute)
            {
                const auto* via = find_port(win_emu.process.alpc, m.via_port_id);
                const uint64_t port_context = via ? via->context : 0;
                const auto address = attributes_address + attribute_offset(header.AllocatedAttributes, context_attribute, wow64);
                const auto sequence = ++win_emu.process.alpc.sequence;

                if (wow64)
                {
                    const std::array<uint32_t, 5> context{static_cast<uint32_t>(port_context), 0, sequence, m.message_id, m.callback_id};
                    emu.write_memory(address, context.data(), sizeof(context));
                }
                else
                {
                    const std::array<uint64_t, 2> pointers{port_context, 0};
                    const std::array<uint32_t, 3> ids{sequence, m.message_id, m.callback_id};
                    emu.write_memory(address, pointers.data(), sizeof(pointers));
                    emu.write_memory(address + sizeof(pointers), ids.data(), sizeof(ids));
                }

                header.ValidAttributes |= context_attribute;
            }

            if ((header.AllocatedAttributes & view_attribute) && m.view)
            {
                auto& s = win_emu.process.alpc;
                const auto size = page_align_up(m.view->size());
                const auto base = win_emu.memory.allocate_memory(size, memory_permission::read_write);
                if (base)
                {
                    emu.write_memory(base, m.view->data(), m.view->size());
                    const auto section = s.next_section_id++;
                    s.sections[section] = size;
                    s.views[base] = size;

                    const auto address = attributes_address + attribute_offset(header.AllocatedAttributes, view_attribute, wow64);
                    if (wow64)
                    {
                        const std::array<uint32_t, 4> view{0, static_cast<uint32_t>(section), static_cast<uint32_t>(base),
                                                           static_cast<uint32_t>(size)};
                        emu.write_memory(address, view.data(), sizeof(view));
                    }
                    else
                    {
                        const std::array<uint64_t, 4> view{0, section, base, size};
                        emu.write_memory(address, view.data(), sizeof(view));
                    }

                    header.ValidAttributes |= view_attribute;
                }
            }

            emu.write_memory(attributes_address, &header, sizeof(header));
        }

        std::optional<std::vector<uint8_t>> read_send_view(const syscall_context& c, const uint64_t attributes_address, const bool wow64)
        {
            if (!attributes_address)
            {
                return std::nullopt;
            }

            const auto header = c.emu.read_memory<ALPC_MESSAGE_ATTRIBUTES>(attributes_address);
            if (!(header.ValidAttributes & view_attribute))
            {
                return std::nullopt;
            }

            const auto address = attributes_address + attribute_offset(header.AllocatedAttributes, view_attribute, wow64);
            const auto base = wow64 ? uint64_t{c.emu.read_memory<uint32_t>(address + 8)} : c.emu.read_memory<uint64_t>(address + 0x10);
            const auto size = wow64 ? uint64_t{c.emu.read_memory<uint32_t>(address + 0xC)} : c.emu.read_memory<uint64_t>(address + 0x18);
            if (!base)
            {
                return std::nullopt;
            }

            std::vector<uint8_t> bytes(size);
            c.emu.read_memory(base, bytes.data(), bytes.size());
            return bytes;
        }

        NTSTATUS write_received_message(windows_emulator& win_emu, const message& m, const uint64_t receive_message,
                                        const uint64_t buffer_length, const uint64_t receive_attributes, const bool wow64)
        {
            // WOW64 ALPC messages keep the 64-bit header footprint: the 32-bit PORT_MESSAGE occupies its first
            // bytes and the payload still starts at sizeof(PORT_MESSAGE64), so only the header fields differ.
            auto& emu = win_emu.emu();
            constexpr auto header_size = static_cast<ULONG>(sizeof(PORT_MESSAGE64));
            const auto required = header_size + static_cast<ULONG>(m.data.size());

            if (buffer_length)
            {
                const auto available = emu.read_memory<uint64_t>(buffer_length);
                if (available < required)
                {
                    emu.write_memory<uint64_t>(buffer_length, required);
                    return STATUS_BUFFER_TOO_SMALL;
                }
            }

            lpc_port_message header{};
            header.is_wow64 = wow64;
            header.native.u1.s1.DataLength = static_cast<CSHORT>(m.data.size());
            header.native.u1.s1.TotalLength = static_cast<CSHORT>(required);
            header.native.u2.s2.Type = static_cast<CSHORT>(m.type);
            header.native.ClientId.UniqueProcess = m.sender_process_id;
            header.native.ClientId.UniqueThread = m.sender_thread_id;
            header.native.MessageId = m.message_id;
            if (m.type == LPC_CONNECTION_REQUEST)
            {
                header.native.ClientViewSize = m.client_view_size;
            }
            else
            {
                header.native.CallbackId = m.callback_id;
            }

            header.write(emulator_object<PORT_MESSAGE64>{emu, receive_message});
            if (!m.data.empty())
            {
                emu.write_memory(receive_message + header_size, m.data.data(), m.data.size());
            }

            if (buffer_length)
            {
                emu.write_memory<uint64_t>(buffer_length, required);
            }

            write_receive_attributes(win_emu, receive_attributes, m, wow64);
            return STATUS_SUCCESS;
        }

        std::deque<message>* receive_queue(state& s, port& p)
        {
            if (p.kind == port_kind::server_communication)
            {
                auto* connection_port = find_port(s, p.connection_port_id);
                return connection_port ? &connection_port->queue : nullptr;
            }

            return &p.queue;
        }

        struct read_message
        {
            lpc_port_message header{};
            std::vector<uint8_t> data{};
        };

        read_message read_port_message(const syscall_context& c, const uint64_t address, const bool wow64)
        {
            read_message result{};
            result.header.is_wow64 = wow64;
            if (wow64)
            {
                const auto header = c.emu.read_memory<PORT_MESSAGE32>(address);
                result.header.native.u1.s1.DataLength = header.u1.s1.DataLength;
                result.header.native.u1.s1.TotalLength = header.u1.s1.TotalLength;
                result.header.native.u2.s2.Type = header.u2.s2.Type;
                result.header.native.u2.s2.DataInfoOffset = header.u2.s2.DataInfoOffset;
                result.header.native.ClientId.UniqueProcess = header.ClientId.UniqueProcess;
                result.header.native.ClientId.UniqueThread = header.ClientId.UniqueThread;
                result.header.native.MessageId = header.MessageId;
                result.header.native.CallbackId = header.CallbackId;
            }
            else
            {
                result.header.native = c.emu.read_memory<PORT_MESSAGE64>(address);
            }

            result.data.resize(result.header.data_length());
            if (!result.data.empty())
            {
                c.emu.read_memory(address + sizeof(PORT_MESSAGE64), result.data.data(), result.data.size());
            }

            return result;
        }

        std::optional<std::chrono::steady_clock::time_point> wait_deadline(const syscall_context& c, const uint64_t timeout)
        {
            if (!timeout)
            {
                return std::nullopt;
            }

            return utils::convert_delay_interval_to_time_point(c.win_emu.clock(), c.emu.read_memory<LARGE_INTEGER>(timeout));
        }

        uint64_t read_max_message_length(const syscall_context& c, const uint64_t port_attributes)
        {
            if (!port_attributes)
            {
                return default_max_message_length;
            }

            constexpr uint64_t max_message_length_offset = 0x10;
            const auto value = c.emu.read_memory<uint64_t>(port_attributes + max_message_length_offset);
            return value == 0 ? default_max_message_length : value;
        }

        NTSTATUS park(const syscall_context& c, pending_wait wait)
        {
            auto& thread = c.thread();
            if (const auto status = try_complete_wait(c.win_emu, thread, wait))
            {
                return *status;
            }

            thread.await_alpc = wait;
            c.win_emu.yield_thread(c.vcpu, false);
            return STATUS_SUCCESS;
        }
    }

    bool is_guest_port_name(process_context& proc, const std::u16string_view name)
    {
        if (name.empty())
        {
            return false;
        }

        if (find_connection_port(proc.alpc, name))
        {
            return true;
        }

        if (!proc.shared_arena)
        {
            return false;
        }

        const auto found = proc.shared_arena->find_named(name);
        if (!found)
        {
            return false;
        }

        proc.shared_arena->release(found->slot);
        return found->kind == kernel_arena::named_object_kind::alpc_port;
    }

    NTSTATUS create_port(const syscall_context& c, std::u16string name, const uint64_t port_attributes, handle& out)
    {
        const auto flags = port_attributes ? c.emu.read_memory<uint32_t>(port_attributes) : 0;
        return create_port(c, std::move(name), flags, read_max_message_length(c, port_attributes), out);
    }

    NTSTATUS create_port(const syscall_context& c, std::u16string name, const uint32_t flags, const uint64_t max_message_length,
                         handle& out)
    {
        auto& s = c.proc.alpc;

        std::shared_ptr<kernel_slot> name_slot{};
        if (!name.empty())
        {
            if (find_connection_port(s, name))
            {
                return STATUS_OBJECT_NAME_COLLISION;
            }

            if (auto* arena = c.proc.ensure_shared_arena())
            {
                const auto slot = arena->allocate();
                if (!slot)
                {
                    return STATUS_INSUFFICIENT_RESOURCES;
                }

                const auto existing = arena->register_named(name, kernel_arena::named_object_kind::alpc_port, *slot);
                if (existing)
                {
                    arena->release(*slot);
                    arena->release(existing->slot);
                    return STATUS_OBJECT_NAME_COLLISION;
                }

                arena->words(*slot)[0].store(c.proc.process_id);
                name_slot = std::make_shared<kernel_slot>(c.proc.shared_arena, *slot);
            }
        }

        port p{};
        p.kind = port_kind::connection;
        p.name = name;
        p.flags = flags;
        p.max_message_length = max_message_length;
        p.name_slot = std::move(name_slot);

        const auto id = new_port_id(s);
        s.ports.emplace(id, std::move(p));
        out = c.proc.ports.store(port_container::guest_alpc(std::move(name), id));
        return STATUS_SUCCESS;
    }

    NTSTATUS connect_port(const syscall_context& c, const std::u16string_view name, const uint64_t port_attributes, const uint32_t flags,
                          const uint64_t connection_message, const uint64_t buffer_length, const uint64_t /*out_message_attributes*/,
                          const uint64_t in_message_attributes, const uint64_t timeout, const uint64_t port_handle_out)
    {
        auto& s = c.proc.alpc;

        port p{};
        p.kind = port_kind::client_communication;
        p.name = name;
        p.flags = port_attributes ? c.emu.read_memory<uint32_t>(port_attributes) : 0;
        p.max_message_length = read_max_message_length(c, port_attributes);
        p.connection_id = (static_cast<uint64_t>(c.proc.process_id) << 32) | ++s.next_connection_serial;

        const auto id = new_port_id(s);
        const auto connection_id = p.connection_id;
        s.ports.emplace(id, std::move(p));
        s.client_ports[connection_id] = id;

        transport payload{};
        payload.kind = transport_kind::connect_request;
        payload.sender_message_id = new_message_id(s);
        payload.sender_process_id = c.proc.process_id;
        payload.sender_thread_id = c.thread().id;
        if (connection_message)
        {
            payload.data = read_port_message(c, connection_message, (flags & msgflg_wow64_call) != 0).data;
        }

        (void)in_message_attributes;

        route(c.win_emu, {.port_name = std::u16string(name), .connection_id = connection_id, .to_server = true, .payload = payload});

        return park(c, pending_wait{
                           .kind = wait_kind::connect,
                           .port_id = id,
                           .receive_message = connection_message,
                           .buffer_length = buffer_length,
                           .port_handle_out = port_handle_out,
                           .wow64_layout = (flags & msgflg_wow64_call) != 0,
                           .timeout = wait_deadline(c, timeout),
                       });
    }

    NTSTATUS accept_connect_port(const syscall_context& c, const uint64_t port_handle_out, const handle connection_port,
                                 const uint32_t flags, const uint64_t port_attributes, const uint64_t port_context,
                                 const uint64_t connection_request, const bool accept)
    {
        auto& s = c.proc.alpc;

        const auto* container = c.proc.ports.get(connection_port);
        auto* listener = container ? find_port(s, container->alpc_port_id()) : nullptr;
        if (!listener || listener->kind != port_kind::connection || !connection_request)
        {
            return STATUS_INVALID_PARAMETER;
        }

        const auto request = read_port_message(c, connection_request, (flags & msgflg_wow64_call) != 0);
        const auto pending = s.pending.find(request.header.native.MessageId);
        if (pending == s.pending.end() || !pending->second.connection_request)
        {
            return STATUS_INVALID_PARAMETER;
        }

        const auto record = pending->second;
        s.pending.erase(pending);

        transport payload{};
        payload.kind = transport_kind::connect_reply;
        payload.sender_message_id = record.sender_message_id;
        payload.sender_process_id = c.proc.process_id;
        payload.sender_thread_id = c.thread().id;
        payload.status = accept ? STATUS_SUCCESS : STATUS_PORT_CONNECTION_REFUSED;
        payload.data = request.data;

        if (accept)
        {
            port p{};
            p.kind = port_kind::server_communication;
            p.name = listener->name;
            p.connection_id = record.connection_id;
            p.connection_port_id = container->alpc_port_id();
            p.context = port_context;
            p.max_message_length = port_attributes ? read_max_message_length(c, port_attributes) : listener->max_message_length;

            const auto id = new_port_id(s);
            s.ports.emplace(id, std::move(p));
            s.server_ports[record.connection_id] = id;

            const auto h = c.proc.ports.store(port_container::guest_alpc(listener->name, id));
            if (port_handle_out)
            {
                c.emu.write_memory(port_handle_out, &h, sizeof(h));
            }
        }

        route(c.win_emu, {.port_name = listener->name, .connection_id = record.connection_id, .to_server = false, .payload = payload});
        return STATUS_SUCCESS;
    }

    NTSTATUS send_wait_receive(const syscall_context& c, const uint64_t port_id, const uint32_t flags, const uint64_t send_message,
                               const uint64_t send_attributes, const uint64_t receive_message, const uint64_t buffer_length,
                               const uint64_t receive_attributes, const uint64_t timeout)
    {
        auto& s = c.proc.alpc;
        auto* p = find_port(s, port_id);
        if (!p)
        {
            return STATUS_INVALID_HANDLE;
        }

        const bool wow64 = (flags & msgflg_wow64_call) != 0;

        if (trace_enabled())
        {
            c.win_emu.log.info("[alpc] pid=%u tid=%u send_wait_receive port=%llu kind=%d flags=0x%x send=%d receive=%d\n",
                               c.proc.process_id, c.thread().id, static_cast<unsigned long long>(port_id), static_cast<int>(p->kind), flags,
                               send_message != 0, receive_message != 0);
        }

        if (send_message)
        {
            const auto sent = read_port_message(c, send_message, wow64);
            const auto message_id = sent.header.native.MessageId;

            if (trace_enabled())
            {
                c.win_emu.log.info("[alpc]   send mid=%u cb=%u type=0x%x bytes=%zu pending=%d\n", message_id, sent.header.native.CallbackId,
                                   static_cast<uint32_t>(sent.header.native.u2.s2.Type), sent.data.size(),
                                   static_cast<int>(s.pending.contains(message_id)));
            }

            if (const auto pending = s.pending.find(message_id); message_id != 0 && pending != s.pending.end())
            {
                const auto record = pending->second;
                s.pending.erase(pending);

                transport payload{};
                payload.kind = transport_kind::reply;
                payload.sender_message_id = record.sender_message_id;
                payload.sender_process_id = c.proc.process_id;
                payload.sender_thread_id = c.thread().id;
                payload.callback_id = sent.header.native.CallbackId;
                payload.data = sent.data;
                payload.view = read_send_view(c, send_attributes, wow64);

                route(c.win_emu,
                      {.port_name = p->name, .connection_id = record.connection_id, .to_server = !record.from_client, .payload = payload});
            }
            else if (!(flags & msgflg_release_message))
            {
                if (p->kind == port_kind::connection)
                {
                    return STATUS_INVALID_PORT_HANDLE;
                }

                if (p->disconnected)
                {
                    return STATUS_PORT_DISCONNECTED;
                }

                if (sizeof(PORT_MESSAGE64) + sent.data.size() > p->max_message_length)
                {
                    return STATUS_INVALID_PARAMETER;
                }

                const bool sync = (flags & msgflg_sync_request) != 0;

                transport payload{};
                payload.kind = transport_kind::message;
                payload.sender_message_id = new_message_id(s);
                payload.sender_process_id = c.proc.process_id;
                payload.sender_thread_id = c.thread().id;
                payload.type = sync ? LPC_REQUEST : LPC_DATAGRAM;
                payload.callback_id = sent.header.native.CallbackId;
                payload.expects_reply = sync;
                payload.data = sent.data;
                payload.view = read_send_view(c, send_attributes, wow64);

                if (sync)
                {
                    p->awaited_replies.insert(payload.sender_message_id);
                }

                route(c.win_emu, {.port_name = p->name,
                                  .connection_id = p->connection_id,
                                  .to_server = p->kind == port_kind::client_communication,
                                  .payload = payload});

                if (sync)
                {
                    return park(c, pending_wait{
                                       .kind = wait_kind::reply,
                                       .port_id = port_id,
                                       .message_id = payload.sender_message_id,
                                       .receive_message = receive_message,
                                       .buffer_length = buffer_length,
                                       .receive_attributes = receive_attributes,
                                       .wow64_layout = wow64,
                                       .timeout = wait_deadline(c, timeout),
                                   });
                }
            }
        }

        if (!receive_message)
        {
            return STATUS_SUCCESS;
        }

        return park(c, pending_wait{
                           .kind = wait_kind::receive,
                           .port_id = port_id,
                           .receive_message = receive_message,
                           .buffer_length = buffer_length,
                           .receive_attributes = receive_attributes,
                           .wow64_layout = wow64,
                           .timeout = wait_deadline(c, timeout),
                       });
    }

    std::optional<NTSTATUS> try_complete_wait(windows_emulator& win_emu, emulator_thread& thread, const pending_wait& wait)
    {
        (void)thread;
        auto& s = win_emu.process.alpc;
        auto* p = find_port(s, wait.port_id);
        if (!p)
        {
            return STATUS_PORT_DISCONNECTED;
        }

        switch (wait.kind)
        {
        case wait_kind::connect:
            if (p->connection_status)
            {
                const auto status = *p->connection_status;
                if (!NT_SUCCESS(status))
                {
                    s.client_ports.erase(p->connection_id);
                    s.ports.erase(wait.port_id);
                    return status;
                }

                if (wait.receive_message && p->connection_reply)
                {
                    const auto write_status = write_received_message(win_emu, *p->connection_reply, wait.receive_message,
                                                                     wait.buffer_length, 0, wait.wow64_layout);
                    if (!NT_SUCCESS(write_status))
                    {
                        return write_status;
                    }
                }

                const auto h = win_emu.process.ports.store(port_container::guest_alpc(p->name, wait.port_id));
                win_emu.emu().write_memory(wait.port_handle_out, &h, sizeof(h));
                p->connection_reply.reset();
                return STATUS_SUCCESS;
            }
            break;

        case wait_kind::reply:
            if (const auto reply = p->replies.find(wait.message_id); reply != p->replies.end())
            {
                const auto status = wait.receive_message
                                        ? write_received_message(win_emu, reply->second, wait.receive_message, wait.buffer_length,
                                                                 wait.receive_attributes, wait.wow64_layout)
                                        : STATUS_SUCCESS;
                if (status != STATUS_BUFFER_TOO_SMALL)
                {
                    p->replies.erase(reply);
                }

                return status;
            }

            if (p->disconnected)
            {
                p->awaited_replies.erase(wait.message_id);
                return STATUS_PORT_DISCONNECTED;
            }
            break;

        case wait_kind::receive:
            if (auto* queue = receive_queue(s, *p))
            {
                const auto entry = std::ranges::find_if(
                    *queue, [&](const message& m) { return p->kind != port_kind::server_communication || m.via_port_id == wait.port_id; });

                if (entry != queue->end())
                {
                    const auto status = write_received_message(win_emu, *entry, wait.receive_message, wait.buffer_length,
                                                               wait.receive_attributes, wait.wow64_layout);
                    if (status != STATUS_BUFFER_TOO_SMALL)
                    {
                        queue->erase(entry);
                    }

                    return status;
                }
            }

            if (p->kind == port_kind::client_communication && p->disconnected)
            {
                return STATUS_PORT_DISCONNECTED;
            }
            break;
        }

        if (wait.timeout && *wait.timeout <= win_emu.clock().steady_now())
        {
            return STATUS_TIMEOUT;
        }

        return std::nullopt;
    }

    NTSTATUS query_emulated_port_information(const syscall_context& c, const uint32_t info_class, const uint64_t information,
                                             const uint32_t length, const uint64_t return_length)
    {
        if (info_class != basic_information)
        {
            return STATUS_INVALID_INFO_CLASS;
        }

        constexpr uint32_t required = 16;
        if (return_length)
        {
            c.emu.write_memory<uint32_t>(return_length, required);
        }

        if (length < required || !information)
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        const std::array<uint8_t, required> zeroed{};
        c.emu.write_memory(information, zeroed.data(), zeroed.size());

        return STATUS_SUCCESS;
    }

    NTSTATUS query_information(const syscall_context& c, const uint64_t port_id, const uint32_t info_class, const uint64_t information,
                               const uint32_t length, const uint64_t return_length)
    {
        auto* p = find_port(c.proc.alpc, port_id);
        if (!p)
        {
            return STATUS_INVALID_HANDLE;
        }

        if (info_class != basic_information)
        {
            return STATUS_INVALID_INFO_CLASS;
        }

        constexpr uint32_t required = 16;
        if (return_length)
        {
            c.emu.write_memory<uint32_t>(return_length, required);
        }

        if (length < required || !information)
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        c.emu.write_memory<uint32_t>(information, p->flags);
        c.emu.write_memory<uint32_t>(information + 4, 0);
        c.emu.write_memory<uint64_t>(information + 8, p->context);

        return STATUS_SUCCESS;
    }

    NTSTATUS set_information(const syscall_context& c, const uint64_t port_id, const uint32_t info_class, const uint64_t information,
                             const uint32_t length)
    {
        auto* p = find_port(c.proc.alpc, port_id);
        if (!p)
        {
            return STATUS_INVALID_HANDLE;
        }

        if (info_class != associate_completion_port_information)
        {
            return STATUS_INVALID_INFO_CLASS;
        }

        if (length < 16 || !information)
        {
            return STATUS_INFO_LENGTH_MISMATCH;
        }

        const auto key = c.emu.read_memory<uint64_t>(information);
        const auto port_handle = c.emu.read_memory<uint64_t>(information + 8);

        handle completion{};
        completion.bits = port_handle;
        if (!c.proc.io_completions.get(completion))
        {
            return STATUS_INVALID_HANDLE;
        }

        p->completion_port = completion;
        p->completion_key = key;

        for (size_t i = 0; i < p->queue.size(); ++i)
        {
            notify_completion_port(c.win_emu, *p);
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS create_port_section(const syscall_context& c, const uint64_t size, const uint64_t section_handle_out,
                                 const uint64_t actual_size_out)
    {
        if (!section_handle_out || size == 0)
        {
            return STATUS_INVALID_PARAMETER;
        }

        auto& s = c.proc.alpc;
        const auto section = s.next_section_id++;
        const auto actual_size = page_align_up(size);
        s.sections[section] = actual_size;

        c.emu.write_memory<uint64_t>(section_handle_out, section);

        if (actual_size_out)
        {
            c.emu.write_memory<uint64_t>(actual_size_out, actual_size);
        }

        return STATUS_SUCCESS;
    }

    NTSTATUS delete_port_section(const syscall_context& c, const uint64_t section)
    {
        return c.proc.alpc.sections.erase(section) != 0 ? STATUS_SUCCESS : STATUS_INVALID_HANDLE;
    }

    NTSTATUS create_section_view(const syscall_context& c, const uint64_t view_attributes)
    {
        if (!view_attributes)
        {
            return STATUS_INVALID_PARAMETER;
        }

        auto& s = c.proc.alpc;
        const auto section = c.emu.read_memory<uint64_t>(view_attributes + 8);
        const auto requested = c.emu.read_memory<uint64_t>(view_attributes + 0x18);

        const auto entry = s.sections.find(section);
        if (entry == s.sections.end())
        {
            return STATUS_INVALID_HANDLE;
        }

        const auto size = requested == 0 ? entry->second : std::min(page_align_up(requested), entry->second);
        const auto base = c.win_emu.memory.allocate_memory(size, memory_permission::read_write);
        if (!base)
        {
            return STATUS_NO_MEMORY;
        }

        s.views[base] = size;

        c.emu.write_memory<uint64_t>(view_attributes + 0x10, base);
        c.emu.write_memory<uint64_t>(view_attributes + 0x18, size);

        return STATUS_SUCCESS;
    }

    NTSTATUS delete_section_view(const syscall_context& c, const uint64_t view_base)
    {
        auto& s = c.proc.alpc;
        const auto entry = s.views.find(view_base);
        if (entry == s.views.end())
        {
            return STATUS_INVALID_PARAMETER;
        }

        c.win_emu.memory.release_memory(view_base, 0);
        s.views.erase(entry);
        return STATUS_SUCCESS;
    }

    void close_port(windows_emulator& win_emu, const uint64_t port_id)
    {
        auto& s = win_emu.process.alpc;
        auto* p = find_port(s, port_id);
        if (!p)
        {
            return;
        }

        if (p->kind != port_kind::connection)
        {
            const bool server_side = p->kind == port_kind::server_communication;
            if (!p->disconnected)
            {
                transport payload{};
                payload.kind = transport_kind::closed;
                payload.sender_process_id = win_emu.process.process_id;
                route(win_emu, {.port_name = p->name, .connection_id = p->connection_id, .to_server = !server_side, .payload = payload});
            }

            auto& endpoints = server_side ? s.server_ports : s.client_ports;
            if (const auto it = endpoints.find(p->connection_id); it != endpoints.end() && it->second == port_id)
            {
                endpoints.erase(it);
            }
        }

        s.ports.erase(port_id);
    }

    bool has_queued_message(const process_context& proc, const uint64_t port_id)
    {
        const auto it = proc.alpc.ports.find(port_id);
        return it != proc.alpc.ports.end() && !it->second.queue.empty();
    }

    bool deliver(windows_emulator& win_emu, const pipe_ipc_message& message)
    {
        return deliver_transport(win_emu, addressed_transport{
                                              .port_name = message.pipe_name,
                                              .connection_id = message.connection_id,
                                              .to_server = message.to_server,
                                              .payload = transport::decode(message.data),
                                          });
    }
}
