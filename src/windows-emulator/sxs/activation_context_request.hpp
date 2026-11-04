#pragma once
#include "../port.hpp"

namespace sogen::sxs
{
    lpc_request_result handle_create_activation_context(windows_emulator& win_emu, const lpc_request_context& request);

    // csrss's basesrv!BaseSrvCreateProcess: reports the compatibility declared by the new process's manifest back to
    // the creating kernelbase!CreateProcessInternalW, which derives the child's shim switch context from it.
    lpc_request_result handle_create_process(windows_emulator& win_emu, const lpc_request_context& request);
}
