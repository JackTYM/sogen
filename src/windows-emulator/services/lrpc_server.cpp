#include "../std_include.hpp"
#include "lrpc_server.hpp"

#include "../windows_emulator.hpp"

namespace sogen::lrpc
{
    namespace
    {
        constexpr uint32_t message_request = 0;
        constexpr uint32_t message_bind = 1;
        constexpr uint32_t message_fault = 2;
        constexpr uint32_t message_response = 3;

        constexpr uint32_t request_flag_object_uuid = 0x1;
        constexpr uint32_t request_flag_view = 0x4;

        constexpr size_t request_header_size = 0x40;
        constexpr size_t response_header_size = 0x18;
        constexpr size_t fault_size = 0x14;

        constexpr uint32_t syntax_ndr = 1;

        template <typename T>
        T read_at(const std::vector<uint8_t>& data, const size_t offset)
        {
            T value{};
            if (offset + sizeof(T) <= data.size())
            {
                std::memcpy(&value, data.data() + offset, sizeof(T));
            }

            return value;
        }

        template <typename T>
        void write_at(std::vector<uint8_t>& data, const size_t offset, const T& value)
        {
            if (data.size() < offset + sizeof(T))
            {
                data.resize(offset + sizeof(T));
            }

            std::memcpy(data.data() + offset, &value, sizeof(T));
        }

        bool trace_enabled()
        {
            static const bool enabled = std::getenv("SOGEN_TRACE_LRPC") != nullptr;
            return enabled;
        }
    }

    void server::add_interface(std::shared_ptr<rpc_interface> rpc_interface)
    {
        const auto uuid = rpc_interface->uuid();
        this->interfaces_[uuid] = std::move(rpc_interface);
    }

    void server::on_connect(windows_emulator& /*win_emu*/, const uint64_t connection_id, const uint32_t /*client_process_id*/)
    {
        this->bindings_[connection_id] = {};
    }

    void server::on_disconnect(windows_emulator& win_emu, const uint64_t connection_id)
    {
        this->bindings_.erase(connection_id);
        for (const auto& entry : this->interfaces_)
        {
            entry.second->on_disconnect(win_emu, connection_id);
        }
    }

    void server::on_message(windows_emulator& win_emu, const alpc::host_call& message)
    {
        const auto type = read_at<uint32_t>(message.data, 0);
        if (type == message_bind)
        {
            this->handle_bind(win_emu, message);
        }
        else if (type == message_request)
        {
            this->handle_request(win_emu, message);
        }
        else if (message.expects_reply)
        {
            win_emu.log.warn("LRPC: unexpected message type %u\n", type);
            std::vector<uint8_t> fault(fault_size);
            write_at<uint64_t>(fault, 0, message_fault);
            write_at<uint32_t>(fault, 8, rpc_x_bad_stub_data);
            alpc::host_reply(win_emu, message, std::move(fault));
        }
    }

    void server::handle_bind(windows_emulator& win_emu, const alpc::host_call& message)
    {
        auto reply = message.data;

        ndr::guid uuid{};
        if (reply.size() >= 0x1C)
        {
            std::memcpy(uuid.data(), reply.data() + 0x0C, uuid.size());
        }

        const auto offered = read_at<uint32_t>(reply, 0x20);
        const auto context_id = read_at<uint16_t>(reply, 0x24);
        const auto entry = this->interfaces_.find(uuid);

        uint32_t status = 0;
        if (entry == this->interfaces_.end())
        {
            status = rpc_s_unknown_if;
        }
        else if (!(offered & syntax_ndr))
        {
            status = 1730;
        }
        else
        {
            this->bindings_[message.connection_id][context_id] = entry->second;
            write_at<uint32_t>(reply, 0x20, syntax_ndr);
        }

        if (trace_enabled() || status != 0)
        {
            win_emu.log.info("LRPC bind pid=%u context=%u status=%u uuid=%02x%02x%02x%02x-...\n", message.sender_process_id, context_id,
                             status, uuid[3], uuid[2], uuid[1], uuid[0]);
        }

        write_at<uint32_t>(reply, 0x08, status);
        alpc::host_reply(win_emu, message, std::move(reply));
    }

    void server::handle_request(windows_emulator& win_emu, const alpc::host_call& message)
    {
        call c{};
        c.message = message;
        c.call_id = read_at<uint32_t>(message.data, 0x0C);
        c.opnum = read_at<uint32_t>(message.data, 0x14);

        const auto flags = read_at<uint32_t>(message.data, 0x08);
        const auto context_id = read_at<uint16_t>(message.data, 0x10);

        if (flags & request_flag_object_uuid)
        {
            ndr::guid object{};
            if (message.data.size() >= 0x28)
            {
                std::memcpy(object.data(), message.data.data() + 0x18, object.size());
            }

            c.object = object;
        }

        if ((flags & request_flag_view) && message.view)
        {
            const auto size = std::min<size_t>(read_at<uint64_t>(message.data, request_header_size), message.view->size());
            c.stub.assign(message.view->begin(), message.view->begin() + static_cast<ptrdiff_t>(size));
        }
        else if (message.data.size() > request_header_size)
        {
            c.stub.assign(message.data.begin() + request_header_size, message.data.end());
        }

        std::shared_ptr<rpc_interface> target{};
        const auto connection = this->bindings_.find(message.connection_id);
        if (connection != this->bindings_.end())
        {
            if (const auto binding = connection->second.find(context_id); binding != connection->second.end())
            {
                target = binding->second;
            }
        }

        if (trace_enabled())
        {
            win_emu.log.info("LRPC call pid=%u context=%u opnum=%u flags=0x%x stub=%zu\n", message.sender_process_id, context_id, c.opnum,
                             flags, c.stub.size());
            std::string hex{};
            for (size_t i = 0; i < std::min<size_t>(message.data.size(), 512); ++i)
            {
                char b[4];
                snprintf(b, sizeof(b), "%02x", message.data[i]);
                hex += b;
            }

            win_emu.log.info("LRPC   data %s\n", hex.c_str());
        }

        if (!target)
        {
            respond(win_emu, c, result::failure(rpc_s_unknown_if));
            return;
        }

        const auto r = target->handle(win_emu, c);
        if (!r.deferred)
        {
            respond(win_emu, c, r);
        }
    }

    void server::respond(windows_emulator& win_emu, const call& c, const result& r)
    {
        if (!c.message.expects_reply)
        {
            return;
        }

        if (r.fault != 0)
        {
            win_emu.log.warn("LRPC call opnum %u failed with %u\n", c.opnum, r.fault);
            std::vector<uint8_t> fault(fault_size);
            write_at<uint64_t>(fault, 0, message_fault);
            write_at<uint32_t>(fault, 8, r.fault);
            alpc::host_reply(win_emu, c.message, std::move(fault));
            return;
        }

        std::vector<uint8_t> reply(response_header_size);
        write_at<uint32_t>(reply, 0, message_response);
        write_at<uint32_t>(reply, 0x0C, c.call_id);
        reply.insert(reply.end(), r.stub.begin(), r.stub.end());
        alpc::host_reply(win_emu, c.message, std::move(reply));
    }
}
