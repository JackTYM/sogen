#include "offline_network.hpp"
#include "socket_wrapper.hpp"

#include <utils/string.hpp>

#include <algorithm>
#include <mutex>
#include <set>

namespace sogen
{

    namespace network
    {
        namespace
        {
            struct reachable_set
            {
                std::vector<std::string> hosts{};
                std::mutex mutex{};
                std::set<std::string> addresses{};

                static std::string host_of(address addr)
                {
                    addr.set_port(0);
                    return addr.to_string();
                }

                bool is_reachable_host(const std::string_view hostname) const
                {
                    return std::ranges::any_of(this->hosts, [&](const std::string& host) {
                        if (hostname.size() < host.size() ||
                            !utils::string::equals_ignore_case(hostname.substr(hostname.size() - host.size()), std::string_view{host}))
                        {
                            return false;
                        }

                        return hostname.size() == host.size() || hostname[hostname.size() - host.size() - 1] == '.';
                    });
                }

                void add(const address& addr)
                {
                    const std::scoped_lock lock{this->mutex};
                    this->addresses.insert(host_of(addr));
                }

                bool contains(const address& addr)
                {
                    const std::scoped_lock lock{this->mutex};
                    return this->addresses.contains(host_of(addr));
                }
            };

            struct offline_dns_lookup : dns_lookup
            {
                std::shared_ptr<reachable_set> reachable{};

                explicit offline_dns_lookup(std::shared_ptr<reachable_set> reachable_hosts)
                    : reachable(std::move(reachable_hosts))
                {
                }

                std::vector<address> resolve_host(const std::string_view hostname, const std::optional<int> family) override
                {
                    auto results = dns_lookup::resolve_host(hostname, family);
                    if (this->reachable->is_reachable_host(hostname))
                    {
                        for (const auto& result : results)
                        {
                            this->reachable->add(result);
                        }

                        return results;
                    }

                    std::erase_if(results, [](const address& a) { return !a.is_loopback(); });
                    return results;
                }
            };

            class offline_socket : public socket_wrapper
            {
              public:
                offline_socket(const int af, const int type, const int protocol, std::shared_ptr<reachable_set> reachable)
                    : socket_wrapper(af, type, protocol),
                      reachable_(std::move(reachable))
                {
                }

                bool connect(const address& addr) override
                {
                    if (!this->is_reachable(addr))
                    {
                        SET_SOCKET_ERROR(SERR(ENETUNREACH));
                        return false;
                    }

                    return socket_wrapper::connect(addr);
                }

                sent_size sendto(const address& destination, const std::span<const std::byte> data) override
                {
                    if (!this->is_reachable(destination))
                    {
                        SET_SOCKET_ERROR(SERR(ENETUNREACH));
                        return -1;
                    }

                    return socket_wrapper::sendto(destination, data);
                }

              private:
                std::shared_ptr<reachable_set> reachable_{};

                bool is_reachable(const address& addr) const
                {
                    return addr.is_loopback() || this->reachable_->contains(addr);
                }
            };

            struct offline_socket_factory : socket_factory
            {
                std::shared_ptr<reachable_set> reachable{};

                explicit offline_socket_factory(std::shared_ptr<reachable_set> reachable_hosts)
                    : reachable(std::move(reachable_hosts))
                {
                }

                std::unique_ptr<i_socket> create_socket(const int af, const int type, const int protocol) override
                {
                    return std::make_unique<offline_socket>(af, type, protocol, this->reachable);
                }
            };
        }

        offline_network create_offline_network(std::vector<std::string> reachable_hosts)
        {
            auto reachable = std::make_shared<reachable_set>();
            reachable->hosts = std::move(reachable_hosts);

            return {
                .dns = std::make_unique<offline_dns_lookup>(reachable),
                .sockets = std::make_unique<offline_socket_factory>(reachable),
            };
        }
    }

} // namespace sogen
