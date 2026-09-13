#include "../std_include.hpp"
#include "lsa_sspirpc.hpp"

#include "binary_writer.hpp"
#include "../registry/registry_utils.hpp"
#include "../windows_emulator.hpp"

namespace sogen
{

    namespace
    {
        constexpr std::array<uint8_t, 16> k_sspirpc_context_uuid = {
            0x53, 0x53, 0x50, 0x49, 0x52, 0x50, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        };

        void write_sspir_security_string_header(utils::aligned_binary_writer& writer, const std::u16string_view value)
        {
            writer.write(static_cast<uint16_t>(value.size() * sizeof(char16_t)));
            writer.write(static_cast<uint16_t>((value.size() + 1) * sizeof(char16_t)));
            writer.write_ndr_pointer(true);
        }

        struct lsa_sspirpc_port : rpc_port
        {
            /*
             * The interface UUID captured live from the guest matches the reverse-engineered
             * "sspirpc" interface (4f32adc8-6052-4a04-8701-293ccf2096f0), hosted on
             * \RPC Control\lsasspirpc, documented at
             * https://github.com/EvanMcBroom/lsa-whisperer/wiki (source/ms-sspir.idl). Only the two
             * procedures GetUserNameW's internals actually invoke are implemented: procnum 0
             * (SspirConnectRpc, LsaConnectUntrusted's session bootstrap) and procnum 0xE
             * (SspirGetUserName, backing GetUserNameExW).
             */
            NTSTATUS handle_rpc(windows_emulator& win_emu, const uint32_t procedure_id, const lpc_request_context& c,
                                utils::aligned_binary_writer& writer, std::vector<alpc_reply_handle>& /*reply_handles*/) override
            {
                switch (procedure_id)
                {
                case 0:
                    return handle_connect_rpc(writer);
                case 1:
                case 2:
                    return handle_disconnect_rpc(writer);
                case 0xE:
                    return handle_get_user_name(win_emu, c, writer);
                default:
                    return STATUS_NOT_SUPPORTED;
                }
            }

          private:
            static NTSTATUS handle_connect_rpc(utils::aligned_binary_writer& writer)
            {
                constexpr uint32_t package_count = 6;
                constexpr uint32_t operational_mode = 0;

                writer.write(package_count);
                writer.write(operational_mode);
                writer.write<uint32_t>(0);
                writer.write(k_sspirpc_context_uuid.data(), k_sspirpc_context_uuid.size());
                writer.write<int32_t>(0);

                return STATUS_SUCCESS;
            }

            static NTSTATUS handle_disconnect_rpc(utils::aligned_binary_writer& writer)
            {
                writer.write<uint32_t>(0);
                writer.pad(k_sspirpc_context_uuid.size());
                writer.write<int32_t>(0);

                return STATUS_SUCCESS;
            }

            static NTSTATUS handle_get_user_name(windows_emulator& win_emu, const lpc_request_context& c,
                                                 utils::aligned_binary_writer& writer)
            {
                constexpr uint32_t name_user_principal = 8;

                uint32_t options = 0;
                if (c.send_buffer_length >= sizeof(options))
                {
                    options = win_emu.emu().read_memory<uint32_t>(c.send_buffer + c.send_buffer_length - sizeof(options));
                }

                const auto user = registry_utils::get_user_name(win_emu.registry);
                const auto domain = registry_utils::get_account_domain(win_emu.registry);

                const auto name = (options & 0xFFFF) == name_user_principal ? user + u"@" + domain : domain + u"\\" + user;

                write_sspir_security_string_header(writer, name);
                writer.write<int32_t>(static_cast<int32_t>(name.size()));
                writer.write_ndr_u16string(name, false);
                writer.write<int32_t>(0);

                return STATUS_SUCCESS;
            }
        };
    }

    std::unique_ptr<port> create_lsa_sspirpc_port()
    {
        return std::make_unique<lsa_sspirpc_port>();
    }

} // namespace sogen
