add_library(d3d9-command-protocol INTERFACE)

target_sources(d3d9-command-protocol INTERFACE
  d3d9_command_protocol.hpp
)

target_include_directories(d3d9-command-protocol INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

target_link_libraries(d3d9-command-protocol INTERFACE
  gpu-bridge-protocol
)
