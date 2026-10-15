#pragma once

#include <memory>

namespace sogen
{
    class windows_emulator;

    // Hosts the RPCSS endpoint (\RPC Control\epmapper) that combase uses for its local OXID resolver, class
    // registration, running object table and activation requests. It is served by the root process of the tree;
    // every other process reaches it through ALPC.
    void start_rpcss(windows_emulator& win_emu);
}
