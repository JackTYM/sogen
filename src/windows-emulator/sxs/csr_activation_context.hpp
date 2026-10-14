#pragma once

#include "../port.hpp"

#include <optional>

namespace sogen
{
    // csrss's basesrv!BaseSrvSxsCreateActivationContext: builds the activation context for the manifest
    // a client's CreateActCtx (or the loader, for a DLL with a manifest resource) sends over the API port
    // and maps it into the client. Returns nullopt for any other CSR API message.
    std::optional<lpc_request_result> try_handle_sxs_create_activation_context(windows_emulator& win_emu, const lpc_request_context& c);
}
