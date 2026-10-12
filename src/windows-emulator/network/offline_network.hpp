#pragma once

#include "dns_lookup.hpp"
#include "socket_factory.hpp"

#include <string>
#include <vector>

namespace sogen
{

    namespace network
    {
        struct offline_network
        {
            std::unique_ptr<dns_lookup> dns{};
            std::unique_ptr<socket_factory> sockets{};
        };

        // A host whose network adapter is disconnected: only loopback names and addresses are reachable, except
        // the given hosts and their subdomains, which resolve and connect as if a firewall let only them through.
        offline_network create_offline_network(std::vector<std::string> reachable_hosts = {});
    }

} // namespace sogen
