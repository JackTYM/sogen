file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
  *.rc
)

list(SORT SRC_FILES)

if(SOGEN_USE_SYSTEM_SDL3 AND NOT EMSCRIPTEN AND NOT TARGET SDL3::SDL3)
  find_package(SDL3 CONFIG REQUIRED)
endif()

if(NOT TARGET SDL3::SDL3)
  list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/ui_backends/sdl_ui_backend.cpp")
  if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "The SDL3 submodule (deps/SDL) is required for the host UI backend on this platform")
  endif()
endif()

# vkd3d-shader (real D3D9 SM2/3 shader translation) is a desktop-only feature: it's cross-compiled
# from source via autotools and needs a widl IDL compiler plus GNU libtool/bison on the host, none of
# which are meaningful (or available) on Android/iOS/Emscripten.
if(NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
  set(SOGEN_HAS_VKD3D_SHADER ON)
endif()

if(NOT SOGEN_HAS_VKD3D_SHADER)
  list(REMOVE_ITEM SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/devices/d3d9_host.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/devices/d3d9_host.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/devices/d3d9_shader_translator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/devices/d3d9_shader_translator.hpp"
  )
endif()

add_library(windows-emulator ${SRC_FILES})

sogen_assign_source_group(${SRC_FILES})

if(NOT SOGEN_ENABLE_CLANG_TIDY)
  target_precompile_headers(windows-emulator PRIVATE std_include.hpp)
endif()

find_package(Freetype REQUIRED)

target_link_libraries(windows-emulator PUBLIC emulator minidump)
target_link_libraries(windows-emulator PRIVATE gpu-bridge-protocol dxgk-command-protocol d3d9-command-protocol vulkan-headers Freetype::Freetype zlib114)

if(SOGEN_HAS_VKD3D_SHADER)
  target_link_libraries(windows-emulator PRIVATE vkd3d-shader-bridge)
  target_compile_definitions(windows-emulator PRIVATE SOGEN_HAS_VKD3D_SHADER=1)
endif()

if(TARGET SDL3::SDL3)
  target_link_libraries(windows-emulator PRIVATE SDL3::SDL3)
  target_compile_definitions(windows-emulator PRIVATE SOGEN_HAS_SDL3=1)
endif()

target_include_directories(windows-emulator INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

sogen_strip_target(windows-emulator)
