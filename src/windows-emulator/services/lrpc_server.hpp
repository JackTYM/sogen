#pragma once

#include "../alpc.hpp"
#include "ndr.hpp"

#include <functional>
#include <map>
#include <memory>

namespace sogen
{
    // The server side of rpcrt4's LRPC protocol over ALPC, for RPC interfaces the emulator implements itself.
    //
    // Messages (offsets into the ALPC payload):
    //   bind     +0x00 type 1, +0x08 status (out), +0x0C RPC_SYNTAX_IDENTIFIER of the interface,
    //            +0x20 offered transfer syntaxes (1 NDR, 2 NDR64; the selected one on return),
    //            +0x24/+0x26 presentation context ids the client assigned to NDR / NDR64
    //   request  +0x00 type 0, +0x08 flags (4: stub data in a view whose size is at +0x40), +0x0C call id,
    //            +0x10 presentation context id, +0x14 opnum, +0x18 object uuid, stub data at +0x40
    //   response +0x00 type 3, +0x0C call id, stub data at +0x18
    namespace lrpc
    {
        constexpr uint32_t rpc_s_unknown_if = 1717;
        constexpr uint32_t rpc_s_procnum_out_of_range = 1745;
        constexpr uint32_t rpc_x_bad_stub_data = 1783;

        struct call
        {
            alpc::host_call message{};
            uint32_t call_id{};
            uint32_t opnum{};
            std::optional<ndr::guid> object{};
            std::vector<uint8_t> stub{};
        };

        struct result
        {
            uint32_t fault{};
            std::vector<uint8_t> stub{};
            bool deferred{};

            static result reply(std::vector<uint8_t> stub)
            {
                return {.stub = std::move(stub)};
            }

            static result failure(const uint32_t status)
            {
                return {.fault = status};
            }

            static result later()
            {
                return {.deferred = true};
            }
        };

        class rpc_interface
        {
          public:
            virtual ~rpc_interface() = default;

            virtual ndr::guid uuid() const = 0;
            virtual result handle(windows_emulator& win_emu, const call& c) = 0;

            virtual void on_disconnect(windows_emulator& win_emu, uint64_t connection_id)
            {
                (void)win_emu;
                (void)connection_id;
            }
        };

        class server : public alpc::host_server
        {
          public:
            void add_interface(std::shared_ptr<rpc_interface> rpc_interface);

            void on_connect(windows_emulator& win_emu, uint64_t connection_id, uint32_t client_process_id) override;
            void on_message(windows_emulator& win_emu, const alpc::host_call& message) override;
            void on_disconnect(windows_emulator& win_emu, uint64_t connection_id) override;

            static void respond(windows_emulator& win_emu, const call& c, const result& r);

          private:
            std::map<ndr::guid, std::shared_ptr<rpc_interface>> interfaces_{};
            std::map<uint64_t, std::map<uint16_t, std::shared_ptr<rpc_interface>>> bindings_{};

            void handle_bind(windows_emulator& win_emu, const alpc::host_call& message);
            void handle_request(windows_emulator& win_emu, const alpc::host_call& message);
        };
    }
}
