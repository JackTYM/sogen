#pragma once

#include "../port.hpp"

namespace sogen
{

    std::unique_ptr<port> create_lsa_sspirpc_port();

} // namespace sogen
