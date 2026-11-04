#pragma once
#include "../std_include.hpp"

#include <utils/serialization.hpp>

namespace sogen
{
    // Everything another host process needs to stand in for a pipe end handed to it by handle
    // inheritance or cross-process duplication.
    struct named_pipe_end_transfer
    {
        std::u16string name{};
        bool server_end{};
        uint64_t connection_id{};
        std::optional<uint32_t> client_process_id{};
        ULONG pipe_type{};
        ULONG read_mode{};
        ULONG completion_mode{};
        ULONG max_instances{};
        ULONG inbound_quota{};
        ULONG outbound_quota{};
        ULONG create_options{};
        LARGE_INTEGER default_timeout{};
        std::string unread_data{};

        void serialize(utils::buffer_serializer& buffer) const;
        void deserialize(utils::buffer_deserializer& buffer);
    };
}
