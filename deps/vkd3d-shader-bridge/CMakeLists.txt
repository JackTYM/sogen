include(ExternalProject)

set(VKD3D_SHADER_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../vkd3d)
set(VKD3D_SHADER_LIB ${VKD3D_SHADER_SOURCE_DIR}/.libs/libvkd3d-shader.a)
set(VKD3D_SHADER_BUILD_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/build-vkd3d-shader.sh)

# vkd3d builds with autotools, not CMake. Its configure.ac unconditionally requires
# a widl IDL compiler (only used to generate a handful of headers, incl. one that
# libvkd3d-shader itself needs) plus Vulkan/SPIR-V headers, and its Makefile.am
# needs GNU libtool/bison newer than what macOS ships. Locate all of it explicitly
# so failures are actionable instead of a cryptic autotools/bison/widl error.

find_program(VKD3D_SHADER_WIDL
    NAMES widl x86_64-w64-mingw32-widl i686-w64-mingw32-widl
)
if(NOT VKD3D_SHADER_WIDL)
    message(FATAL_ERROR "vkd3d-shader requires a widl IDL compiler. Install via 'brew install mingw-w64'.")
endif()

find_program(VKD3D_SHADER_GLIBTOOLIZE
    NAMES glibtoolize libtoolize
    HINTS /opt/homebrew/opt/libtool/libexec/gnubin /usr/local/opt/libtool/libexec/gnubin
)
if(NOT VKD3D_SHADER_GLIBTOOLIZE)
    message(FATAL_ERROR "vkd3d-shader requires GNU libtool. Install via 'brew install libtool'.")
endif()
get_filename_component(VKD3D_SHADER_LIBTOOL_BINDIR ${VKD3D_SHADER_GLIBTOOLIZE} DIRECTORY)

find_program(VKD3D_SHADER_BISON
    NAMES bison
    HINTS /opt/homebrew/opt/bison/bin /usr/local/opt/bison/bin
)
if(NOT VKD3D_SHADER_BISON)
    message(FATAL_ERROR "vkd3d-shader requires GNU Bison >= 2.5 (macOS ships bison 2.3). Install via 'brew install bison'.")
endif()
execute_process(
    COMMAND ${VKD3D_SHADER_BISON} --version
    OUTPUT_VARIABLE VKD3D_SHADER_BISON_VERSION_OUTPUT
)
if(NOT VKD3D_SHADER_BISON_VERSION_OUTPUT MATCHES "\\(GNU Bison\\) ([0-9]+)\\.([0-9]+)")
    message(FATAL_ERROR "Could not determine version of bison at ${VKD3D_SHADER_BISON}.")
endif()
if(CMAKE_MATCH_1 LESS 2 OR (CMAKE_MATCH_1 EQUAL 2 AND CMAKE_MATCH_2 LESS 5))
    message(FATAL_ERROR "vkd3d-shader requires GNU Bison >= 2.5, found ${CMAKE_MATCH_1}.${CMAKE_MATCH_2} at "
                         "${VKD3D_SHADER_BISON}. Install a newer bison via 'brew install bison'.")
endif()
get_filename_component(VKD3D_SHADER_BISON_BINDIR ${VKD3D_SHADER_BISON} DIRECTORY)

find_path(VKD3D_SHADER_VULKAN_INCLUDE_DIR vulkan/vulkan.h
    HINTS /opt/homebrew/opt/vulkan-headers/include /usr/local/opt/vulkan-headers/include
)
find_path(VKD3D_SHADER_SPIRV_INCLUDE_DIR spirv/unified1/spirv.h
    HINTS /opt/homebrew/opt/spirv-headers/include /usr/local/opt/spirv-headers/include
)
if(NOT VKD3D_SHADER_VULKAN_INCLUDE_DIR OR NOT VKD3D_SHADER_SPIRV_INCLUDE_DIR)
    message(FATAL_ERROR "vkd3d-shader requires Vulkan and SPIR-V headers. "
                         "Install via 'brew install vulkan-headers spirv-headers'.")
endif()

# MSYS2/mingw-w64 ships the Vulkan loader import lib as libvulkan.dll.a, which an MSVC-generator
# CMake (CMAKE_FIND_LIBRARY_PREFIXES "", CMAKE_FIND_LIBRARY_SUFFIXES .lib only) won't match
# without both the "lib" prefix and the .dll.a/.a suffixes added.
set(_vkd3d_shader_saved_lib_prefixes ${CMAKE_FIND_LIBRARY_PREFIXES})
set(_vkd3d_shader_saved_lib_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
list(APPEND CMAKE_FIND_LIBRARY_PREFIXES "lib")
list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a" ".a")
find_library(VKD3D_SHADER_LIBVULKAN NAMES vulkan-1 vulkan HINTS /opt/homebrew/lib /usr/local/lib)
if(NOT VKD3D_SHADER_LIBVULKAN)
    message(STATUS "vkd3d-shader: searched CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH} "
                    "prefixes=${CMAKE_FIND_LIBRARY_PREFIXES} suffixes=${CMAKE_FIND_LIBRARY_SUFFIXES}")
endif()
set(CMAKE_FIND_LIBRARY_PREFIXES ${_vkd3d_shader_saved_lib_prefixes})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_vkd3d_shader_saved_lib_suffixes})
if(NOT VKD3D_SHADER_LIBVULKAN)
    message(FATAL_ERROR "vkd3d-shader's configure needs to link a Vulkan loader. "
                         "Install via 'brew install vulkan-loader'.")
endif()
get_filename_component(VKD3D_SHADER_LIBVULKAN_DIR ${VKD3D_SHADER_LIBVULKAN} DIRECTORY)

ExternalProject_Add(
    vkd3d-shader-project
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    SOURCE_DIR ${VKD3D_SHADER_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${VKD3D_SHADER_LIBTOOL_BINDIR}:${VKD3D_SHADER_BISON_BINDIR}:$ENV{PATH}"
        "WIDL=${VKD3D_SHADER_WIDL}"
        "CPPFLAGS=-I${VKD3D_SHADER_VULKAN_INCLUDE_DIR} -I${VKD3D_SHADER_SPIRV_INCLUDE_DIR}"
        "LDFLAGS=-L${VKD3D_SHADER_LIBVULKAN_DIR}"
        sh ${VKD3D_SHADER_BUILD_SCRIPT} ${VKD3D_SHADER_SOURCE_DIR}
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_ALWAYS 1
    USES_TERMINAL_CONFIGURE 1
    USES_TERMINAL_BUILD 1
    BUILD_BYPRODUCTS ${VKD3D_SHADER_LIB}
)

add_library(vkd3d-shader-bridge INTERFACE)
add_dependencies(vkd3d-shader-bridge vkd3d-shader-project)
target_link_libraries(vkd3d-shader-bridge INTERFACE ${VKD3D_SHADER_LIB})
target_include_directories(vkd3d-shader-bridge INTERFACE ${VKD3D_SHADER_SOURCE_DIR}/include)
