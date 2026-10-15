#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "handles.hpp"

namespace sogen
{
    class windows_emulator;
    class emulator_thread;
    class kernel_slot;
    struct process_context;
    struct syscall_context;
    struct pipe_ipc_message;

    namespace alpc
    {
        constexpr uint32_t msgflg_reply_message = 0x1;
        constexpr uint32_t msgflg_release_message = 0x10000;
        constexpr uint32_t msgflg_sync_request = 0x20000;
        constexpr uint32_t msgflg_wait_alertable = 0x200000;
        constexpr uint32_t msgflg_wow64_call = 0x80000000;

        enum class port_kind : uint8_t
        {
            connection,
            server_communication,
            client_communication,
        };

        struct message
        {
            uint16_t type{};
            uint32_t sender_process_id{};
            uint32_t sender_thread_id{};
            uint32_t message_id{};
            uint32_t callback_id{};
            uint64_t client_view_size{};
            uint64_t via_port_id{};
            std::vector<uint8_t> data{};
            std::optional<std::vector<uint8_t>> view{};
        };

        struct port
        {
            port_kind kind{};
            std::u16string name{};
            uint64_t connection_id{};
            uint64_t connection_port_id{};
            uint64_t context{};
            uint32_t flags{};
            uint64_t max_message_length{};
            uint64_t connection_max_message_length{};
            bool disconnected{};

            std::deque<message> queue{};

            std::set<uint32_t> awaited_replies{};
            std::map<uint32_t, message> replies{};

            std::optional<NTSTATUS> connection_status{};
            std::optional<message> connection_reply{};

            std::optional<handle> completion_port{};
            uint64_t completion_key{};

            std::shared_ptr<kernel_slot> name_slot{};
            std::optional<std::u16string> kernel_peer{};
        };

        struct pending_message
        {
            uint64_t connection_id{};
            uint32_t sender_message_id{};
            bool from_client{};
            bool connection_request{};
            uint64_t connection_port_id{};
        };

        struct state
        {
            uint64_t next_port_id{1};
            uint32_t next_message_id{1};
            uint32_t next_connection_serial{};
            uint32_t sequence{};
            std::map<uint64_t, port> ports{};
            std::map<uint32_t, pending_message> pending{};
            std::map<uint64_t, uint64_t> server_ports{};
            std::map<uint64_t, uint64_t> client_ports{};
            uint64_t next_section_id{1};
            std::map<uint64_t, uint64_t> sections{};
            std::map<uint64_t, uint64_t> views{};
        };

        enum class wait_kind : uint8_t
        {
            receive,
            reply,
            connect,
            lpc_connect,
        };

        struct pending_wait
        {
            wait_kind kind{};
            uint64_t port_id{};
            uint32_t message_id{};
            uint64_t receive_message{};
            uint64_t buffer_length{};
            uint64_t receive_attributes{};
            uint64_t port_handle_out{};
            uint64_t connection_info{};
            uint64_t connection_info_length{};
            uint64_t maximum_message_length{};
            bool wow64_layout{};
            std::optional<std::chrono::steady_clock::time_point> timeout{};
        };

        bool is_guest_port_name(process_context& proc, std::u16string_view name);
        void create_kernel_listener(process_context& proc, std::u16string_view name, uint64_t max_message_length,
                                    std::u16string_view peer_name);

        NTSTATUS create_port(const syscall_context& c, std::u16string name, uint64_t port_attributes, handle& out);
        NTSTATUS create_port(const syscall_context& c, std::u16string name, uint32_t flags, uint64_t max_message_length, handle& out);
        NTSTATUS connect_port(const syscall_context& c, std::u16string_view name, uint64_t port_attributes, uint32_t flags,
                              uint64_t connection_message, uint64_t buffer_length, uint64_t out_message_attributes,
                              uint64_t in_message_attributes, uint64_t timeout, uint64_t port_handle_out);
        NTSTATUS connect_lpc_port(const syscall_context& c, std::u16string_view name, uint64_t connection_info,
                                  uint64_t connection_info_length, uint64_t maximum_message_length, uint64_t port_handle_out);
        NTSTATUS listen_port(const syscall_context& c, uint64_t port_id, uint64_t connection_request);
        NTSTATUS accept_connect_port(const syscall_context& c, uint64_t port_handle_out, handle connection_port, uint32_t flags,
                                     uint64_t port_attributes, uint64_t port_context, uint64_t connection_request, bool accept);
        NTSTATUS complete_connect_port(const syscall_context& c, uint64_t port_id);
        NTSTATUS send_wait_receive(const syscall_context& c, uint64_t port_id, uint32_t flags, uint64_t send_message,
                                   uint64_t send_attributes, uint64_t receive_message, uint64_t buffer_length, uint64_t receive_attributes,
                                   uint64_t timeout);
        NTSTATUS create_port_section(const syscall_context& c, uint64_t size, uint64_t section_handle_out, uint64_t actual_size_out);
        NTSTATUS delete_port_section(const syscall_context& c, uint64_t section);
        NTSTATUS create_section_view(const syscall_context& c, uint64_t view_attributes);
        NTSTATUS delete_section_view(const syscall_context& c, uint64_t view_base);
        NTSTATUS query_emulated_port_information(const syscall_context& c, uint32_t info_class, uint64_t information, uint32_t length,
                                                 uint64_t return_length);
        NTSTATUS query_information(const syscall_context& c, uint64_t port_id, uint32_t info_class, uint64_t information, uint32_t length,
                                   uint64_t return_length);
        NTSTATUS set_information(const syscall_context& c, uint64_t port_id, uint32_t info_class, uint64_t information, uint32_t length);
        void close_port(windows_emulator& win_emu, uint64_t port_id);

        bool has_queued_message(const process_context& proc, uint64_t port_id);
        std::optional<NTSTATUS> try_complete_wait(windows_emulator& win_emu, emulator_thread& thread, const pending_wait& wait);

        bool deliver(windows_emulator& win_emu, const pipe_ipc_message& message);
    }
}
