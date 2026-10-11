#pragma once

#include "std_include.hpp"
#include "process_context.hpp"

namespace sogen
{
    hmenu create_menu_from_template(process_context& process, memory_manager& memory, std::span<const uint8_t> menu_template);
}
