#include "offline_network.hpp"
#include "socket_wrapper.hpp"

namespace sogen
{

    namespace network
    {
        namespace
        {
            struct offline_dns_lookup : dns_lookup
            {
                std::vector<address> resolve_host(const std::string_view hostname, const std::optional<int> family) override
                {
                    auto results = dns_lookup::resolve_host(hostname, family);
                    std::erase_if(results, [](const address& a) { return !a.is_loopback(); });
                    return results;
                }
            };

            class offline_socket : public socket_wrapper
            {
              public:
                using socket_wrapper::socket_wrapper;

                bool connect(const address& addr) override
                {
                    if (!addr.is_loopback())
                    {
                        SET_SOCKET_ERROR(SERR(ENETUNREACH));
                        return false;
                    }

                    return socket_wrapper::connect(addr);
                }

                sent_size sendto(const address& destination, const std::span<const std::byte> data) override
                {
                    if (!destination.is_loopback())
                    {
                        SET_SOCKET_ERROR(SERR(ENETUNREACH));
                        return -1;
                    }

                    return socket_wrapper::sendto(destination, data);
                }
            };

            struct offline_socket_factory : socket_factory
            {
                std::unique_ptr<i_socket> create_socket(const int af, const int type, const int protocol) override
                {
                    return std::make_unique<offline_socket>(af, type, protocol);
                }
            };
        }

        std::unique_ptr<dns_lookup> create_offline_dns_lookup()
        {
            return std::make_unique<offline_dns_lookup>();
        }

        std::unique_ptr<socket_factory> create_offline_socket_factory()
        {
            return std::make_unique<offline_socket_factory>();
        }
    }

} // namespace sogen
