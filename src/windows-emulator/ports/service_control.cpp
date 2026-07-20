#include "../std_include.hpp"
#include "service_control.hpp"

#include "binary_writer.hpp"
#include "../windows_emulator.hpp"

namespace sogen
{

    namespace
    {
        // The \RPC Control\ntsvcs ALPC port hosts several RPC interfaces. The one implemented here is svcctl
        // (the Service Control Manager), {367ABB81-9844-35F1-AD32-98F038001003}: while creating a render audio
        // client, mmdevapi opens the "AudioSrv" service through it (OpenSCManagerW -> OpenServiceW ->
        // SubscribeServiceChangeNotifications) so it can react to the audio service restarting. Opnums this
        // file does not recognize (e.g. calls belonging to the PnP CM_* config-manager interface, which shares
        // the same port) fall back to an empty success reply.

        // svcctl opnums observed on the wire. Opnum 64 is ROpenSCManager2 (MS-SCMR, added in Windows 10 1703+), a
        // distinct method from the classic ROpenSCManagerW: its [in] is just the "ServicesActive" database name
        // string + desired access, with no machine name or input handle. The other opnums here are the classic
        // MS-SCMR numbers.
        constexpr uint32_t k_svcctl_close_handle = 0;       // RCloseServiceHandle ([in,out] handle)
        constexpr uint32_t k_svcctl_open_service_w = 16;    // ROpenServiceW (SCM handle + name + access)
        constexpr uint32_t k_svcctl_subscribe = 55;         // SubscribeServiceChangeNotifications (handle + mask)
        constexpr uint32_t k_svcctl_open_sc_manager_w = 64; // ROpenSCManager2 (database string + access)

        constexpr uint32_t k_error_success = 0;

        // Fabricated SC_RPC_HANDLEs. The SCM is fully stubbed, so the bytes only need to round-trip: the client
        // stores the handle from Open* and hands it back on the follow-on Open/Close/Notify calls.
        constexpr std::array<uint8_t, 16> k_scm_handle_uuid = {0x53, 0x6f, 0x67, 0x65, 0x6e, 0x53, 0x43, 0x4d,
                                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        constexpr std::array<uint8_t, 16> k_service_handle_uuid = {0x53, 0x6f, 0x67, 0x65, 0x6e, 0x53, 0x76, 0x63,
                                                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
        constexpr std::array<uint8_t, 16> k_null_handle_uuid = {};

        // An NDR [out] context handle: a 4-byte attributes field followed by the 16-byte handle UUID.
        void write_context_handle(utils::aligned_binary_writer& writer, const std::array<uint8_t, 16>& uuid)
        {
            writer.write<uint32_t>(0); // context handle attributes
            writer.write(uuid.data(), uuid.size(), 1);
        }

        void write_return(utils::aligned_binary_writer& writer, const uint32_t win32_error)
        {
            writer.align_to(sizeof(uint32_t));
            writer.write<uint32_t>(win32_error);
        }

        struct service_control_port : rpc_port
        {
            NTSTATUS handle_rpc(windows_emulator& win_emu, const uint32_t procedure_id, const lpc_request_context& c,
                                utils::aligned_binary_writer& writer) override
            {
                switch (procedure_id)
                {
                case k_svcctl_open_sc_manager_w:
                    write_context_handle(writer, k_scm_handle_uuid);
                    write_return(writer, k_error_success);
                    return STATUS_SUCCESS;

                case k_svcctl_open_service_w:
                    write_context_handle(writer, k_service_handle_uuid);
                    write_return(writer, k_error_success);
                    return STATUS_SUCCESS;

                case k_svcctl_subscribe:
                    // SubscribeServiceChangeNotifications's [out] (classic NDR) is a non-encapsulated union
                    // discriminated by the change-mask; for the masks used here the arm is a unique pointer to
                    // an 8-byte WNF_STATE_NAME the client would subscribe to. NDR order is: the pointer referent,
                    // then the return value, then the deferred pointee.
                    {
                        // The change-mask is an [in] parameter so it is NOT echoed in the output NDR.
                        writer.write<uint32_t>(0x00020000);      // unique pointer referent (non-null)
                        writer.write<uint32_t>(k_error_success); // return value
                        constexpr uint64_t k_wnf_audiosrv_running = 0x41C200A1700AC3C5ULL;
                        // Written as two uint32_t so binary_writer does not insert 8-byte alignment padding
                        // before them, which a single write<uint64_t> call would.
                        writer.write<uint32_t>(static_cast<uint32_t>(k_wnf_audiosrv_running));
                        writer.write<uint32_t>(static_cast<uint32_t>(k_wnf_audiosrv_running >> 32));
                    }
                    return STATUS_SUCCESS;

                case k_svcctl_close_handle:
                    // [in, out] SC_RPC_HANDLE: the client expects the handle nulled out on a successful close.
                    write_context_handle(writer, k_null_handle_uuid);
                    write_return(writer, k_error_success);
                    return STATUS_SUCCESS;

                default: {
                    std::string hex;
                    const auto count = std::min<ULONG>(c.send_buffer_length, 64);
                    std::vector<uint8_t> bytes(count, 0);
                    if (count)
                    {
                        win_emu.emu().read_memory(c.send_buffer, bytes.data(), bytes.size());
                    }
                    std::array<char, 4> tmp{};
                    for (const auto b : bytes)
                    {
                        (void)snprintf(tmp.data(), tmp.size(), "%02x ", b);
                        hex += tmp.data();
                    }
                    win_emu.log.info("[ntsvcs] unrecognized opnum=%u send_len=%u in: %s\n", procedure_id,
                                     static_cast<uint32_t>(c.send_buffer_length), hex.c_str());
                    return STATUS_SUCCESS;
                }
                }
            }
        };
    }

    std::unique_ptr<port> create_service_control_port()
    {
        return std::make_unique<service_control_port>();
    }

} // namespace sogen
