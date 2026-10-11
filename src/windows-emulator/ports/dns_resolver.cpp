#include "../std_include.hpp"
#include "dns_resolver.hpp"

#include "binary_writer.hpp"
#include "../windows_emulator.hpp"

#include <cstring>

#define DNS_TYPE_A     0x01
#define DNS_TYPE_CNAME 0x05
#define DNS_TYPE_AAAA  0x1C

#ifndef OS_WINDOWS
#define ERROR_SUCCESS              0x0
#define DNS_ERROR_RCODE_NAME_ERROR 0x232B
#define DNS_INFO_NO_RECORDS        0x251D
#endif

namespace sogen
{

    namespace
    {
        constexpr DWORD k_dns_record_flags = 0x2009;
        constexpr DWORD k_dns_record_ttl = 0x708;
        constexpr size_t k_dns_record_data_size = 16;

        struct resolved_dns_record
        {
            std::u16string name;
            WORD type{};
            WORD data_length{};
            DWORD flags{k_dns_record_flags};
            DWORD ttl{k_dns_record_ttl};
            DWORD reserved{};
            std::array<std::byte, k_dns_record_data_size> data{};
        };

        struct dns_query_request
        {
            std::u16string hostname;
            WORD type{};
            DWORD flags{};
            uint64_t options{};
        };

        constexpr uint64_t k_dns_query_addrconfig = 0x2000;
        constexpr uint64_t k_dns_query_dual_addr = 0x4000;

        size_t union_arm_alignment(const utils::aligned_binary_writer& writer, const WORD type)
        {
            // NDR (32-bit) aligns each DNS_RECORD union arm to its own alignment, and the IP6_ADDRESS arm is
            // 8-aligned; NDR64 aligns every arm to the union's 8-byte alignment.
            if (writer.pointer_size() == utils::aligned_binary_writer::pointer_size_64 || type == DNS_TYPE_AAAA)
            {
                return 8;
            }

            return 4;
        }

        void write_record_list(utils::aligned_binary_writer& writer, const std::vector<resolved_dns_record>& records, const size_t index)
        {
            const auto& record = records[index];
            const auto has_next = index + 1 < records.size();

            writer.align_to(8);
            writer.write_ndr_pointer(has_next);
            writer.write_ndr_pointer(true);
            writer.write(record.type);
            writer.write(record.data_length);
            writer.write(record.flags);
            writer.write(record.ttl);
            writer.write(record.reserved);

            writer.write(record.type);
            writer.write(record.data.data(), record.data_length, union_arm_alignment(writer, record.type));

            if (has_next)
            {
                write_record_list(writer, records, index + 1);
            }

            writer.write_ndr_u16string(record.name);
        }

        struct dns_query_response
        {
            std::vector<resolved_dns_record> records;
            uint64_t error_code{};

            void write(utils::aligned_binary_writer& writer) const
            {
                writer.write_ndr_pointer(!records.empty());
                if (!records.empty())
                {
                    write_record_list(writer, records, 0);
                }

                writer.align_to(writer.pointer_size());
                writer.pad(24);
                writer.write(error_code);
                writer.pad(64);
            }
        };

        bool parse_dns_query_request(windows_emulator& win_emu, const lpc_request_context& c, dns_query_request& request)
        {
            // The hostname length/name follow a run of pointer-sized fields, so a 32-bit (WoW64) caller packs
            // them at half the 64-bit offsets; parse relative to the caller's pointer width.
            const size_t ptr = win_emu.process.is_wow64_process ? sizeof(uint32_t) : sizeof(uint64_t);
            const auto length_offset = static_cast<ULONG>(1 * ptr); // name character count (incl. NUL)
            const auto actual_length_offset = static_cast<ULONG>(3 * ptr);
            const auto hostname_offset = static_cast<ULONG>(4 * ptr); // UTF-16 hostname
            constexpr ULONG fixed_trailer_size = 8;
            auto& emu = win_emu.emu();

            if (c.send_buffer_length < hostname_offset + sizeof(char16_t) + fixed_trailer_size)
            {
                return false;
            }

            const auto read_count = [&](const emulator_pointer at) -> size_t {
                return win_emu.process.is_wow64_process ? static_cast<size_t>(emu.read_memory<uint32_t>(at))
                                                        : static_cast<size_t>(emu.read_memory<uint64_t>(at));
            };
            const auto hostname_length = read_count(c.send_buffer + length_offset);
            const auto hostname_actual_length = read_count(c.send_buffer + actual_length_offset);
            if (hostname_length == 0 || hostname_actual_length != hostname_length)
            {
                return false;
            }

            const auto hostname_bytes = hostname_length * sizeof(char16_t);
            if (hostname_offset + hostname_bytes + fixed_trailer_size > c.send_buffer_length)
            {
                return false;
            }

            std::u16string encoded_hostname(hostname_length, u'\0');
            emu.read_memory(c.send_buffer + hostname_offset, encoded_hostname.data(), hostname_bytes);
            if (encoded_hostname.back() != u'\0')
            {
                return false;
            }

            request.hostname.assign(encoded_hostname.begin(), encoded_hostname.end() - 1);
            const auto query_type_offset = c.send_buffer + hostname_offset + hostname_bytes;
            request.type = emu.read_memory<uint16_t>(query_type_offset);
            request.flags = emu.read_memory<uint32_t>(query_type_offset + sizeof(uint32_t));
            request.options = emu.read_memory<uint64_t>(c.send_buffer + c.send_buffer_length - sizeof(request.options));
            return true;
        }

        std::vector<resolved_dns_record> resolve_host_addresses(windows_emulator& win_emu, const std::u16string& host, const WORD dns_type)
        {
            const auto family = dns_type == DNS_TYPE_A ? AF_INET : AF_INET6;
            const auto results = win_emu.dns_lookup().resolve_host(u16_to_u8(host), family);

            std::vector<resolved_dns_record> records;
            for (const auto& current : results)
            {
                resolved_dns_record record{};
                record.name = host;

                if (current.is_ipv4() && dns_type == DNS_TYPE_A)
                {
                    const auto& ipv4 = current.get_in_addr();
                    record.type = DNS_TYPE_A;
                    record.data_length = sizeof(ipv4.sin_addr);
                    memcpy(record.data.data(), &ipv4.sin_addr, sizeof(ipv4.sin_addr));
                }
                else if (current.is_ipv6() && dns_type == DNS_TYPE_AAAA)
                {
                    const auto& ipv6 = current.get_in6_addr();
                    record.type = DNS_TYPE_AAAA;
                    record.data_length = sizeof(ipv6.sin6_addr);
                    memcpy(record.data.data(), &ipv6.sin6_addr, sizeof(ipv6.sin6_addr));
                }
                else
                {
                    continue;
                }

                const auto is_duplicate = std::ranges::any_of(records, [&record](const resolved_dns_record& existing) {
                    return existing.type == record.type && existing.data == record.data;
                });
                if (!is_duplicate)
                {
                    records.push_back(std::move(record));
                }
            }

            return records;
        }

        // The emulated host is configured with IPv4 only, so address-config queries (getaddrinfo's AF_UNSPEC
        // path) get no AAAA records; dual-address AAAA queries carry the A records as IPv4-mapped AAAA records,
        // which ws2_32 turns back into AF_INET results.
        dns_query_response resolve_query(windows_emulator& win_emu, const std::u16string& host, const WORD dns_type, const uint64_t options)
        {
            dns_query_response response{};
            if (dns_type == DNS_TYPE_AAAA && !(options & k_dns_query_addrconfig))
            {
                response.records = resolve_host_addresses(win_emu, host, DNS_TYPE_AAAA);
            }

            if (dns_type == DNS_TYPE_A)
            {
                response.records = resolve_host_addresses(win_emu, host, DNS_TYPE_A);
            }
            else if (dns_type == DNS_TYPE_AAAA && (options & k_dns_query_dual_addr))
            {
                for (auto record : resolve_host_addresses(win_emu, host, DNS_TYPE_A))
                {
                    std::array<std::byte, k_dns_record_data_size> mapped{};
                    mapped[10] = std::byte{0xFF};
                    mapped[11] = std::byte{0xFF};
                    memcpy(mapped.data() + 12, record.data.data(), 4);
                    record.type = DNS_TYPE_AAAA;
                    record.data_length = k_dns_record_data_size;
                    record.data = mapped;
                    response.records.push_back(std::move(record));
                }
            }

            if (!response.records.empty())
            {
                response.error_code = ERROR_SUCCESS;
            }
            else if (win_emu.dns_lookup().resolve_host(u16_to_u8(host)).empty())
            {
                response.error_code = DNS_ERROR_RCODE_NAME_ERROR;
            }
            else
            {
                response.error_code = DNS_INFO_NO_RECORDS;
            }

            return response;
        }

        struct dns_resolver : rpc_port
        {
            NTSTATUS handle_rpc(windows_emulator& win_emu, const uint32_t procedure_id, const lpc_request_context& c,
                                utils::aligned_binary_writer& writer, std::vector<alpc_reply_handle>& /*reply_handles*/) override
            {
                if (procedure_id == 4)
                {
                    return handle_dns_query(win_emu, c, writer);
                }

                win_emu.log.print(color::gray, "Unexpected DNSResolver procedure: %u\n", procedure_id);
                return STATUS_NOT_SUPPORTED;
            }

            static NTSTATUS handle_dns_query(windows_emulator& win_emu, const lpc_request_context& c, utils::aligned_binary_writer& writer)
            {
                dns_query_request request{};
                if (!parse_dns_query_request(win_emu, c, request))
                {
                    return STATUS_INVALID_PARAMETER;
                }

                if (request.type != DNS_TYPE_A && request.type != DNS_TYPE_AAAA)
                {
                    return STATUS_NOT_SUPPORTED;
                }

                win_emu.callbacks.on_generic_activity("DNS query: " + u16_to_u8(request.hostname));
                writer.write(request.options);
                writer.write(resolve_query(win_emu, request.hostname, request.type, request.options));

                return STATUS_SUCCESS;
            }
        };
    }

    std::unique_ptr<port> create_dns_resolver()
    {
        return std::make_unique<dns_resolver>();
    }

} // namespace sogen
