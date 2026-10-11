#pragma once

#include "dns_lookup.hpp"
#include "socket_factory.hpp"

namespace sogen
{

    namespace network
    {
        // A host whose network adapter is disconnected: only loopback names and addresses are reachable.
        std::unique_ptr<dns_lookup> create_offline_dns_lookup();
        std::unique_ptr<socket_factory> create_offline_socket_factory();
    }

} // namespace sogen
