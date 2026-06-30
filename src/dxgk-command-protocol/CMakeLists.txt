add_library(dxgk-command-protocol INTERFACE)

target_sources(dxgk-command-protocol INTERFACE
  dxgk_command_protocol.hpp
)

target_include_directories(dxgk-command-protocol INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
