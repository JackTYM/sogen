#!/bin/sh
# Builds libvkd3d-shader.a from the deps/vkd3d autotools source tree.
#
# Only libvkd3d-shader.la is built (not libvkd3d/libvkd3d-utils, the D3D12-on-Vulkan
# runtime). configure still probes for Vulkan/SPIR-V headers because the top-level
# configure.ac checks them unconditionally, and widl is required because
# libvkd3d-shader's fx.c pulls in an IDL-generated header (vkd3d_d3dx9shader.h).
#
# Expects PATH/WIDL/CPPFLAGS/LDFLAGS to already be set in the environment by the
# CMake caller (see CMakeLists.txt in this directory).
#
# Usage: build-vkd3d-shader.sh <source dir> [<build dir> [extra configure args...]]
# Without a build dir the source tree is configured and built in place. With one, the build
# runs in a pristine copy of the committed sources, so several targets can be built side by side.
set -e

SRC_DIR=$1
BUILD_DIR=${2:-${SRC_DIR}}
[ $# -gt 2 ] && shift 2 || shift $#

if [ "${BUILD_DIR}" != "${SRC_DIR}" ] && [ ! -f "${BUILD_DIR}/configure.ac" ]; then
    mkdir -p "${BUILD_DIR}"
    git -C "${SRC_DIR}" archive HEAD | tar -x -C "${BUILD_DIR}"
fi
cd "${BUILD_DIR}"

if [ ! -x ./configure ]; then
    ./autogen.sh
fi

if [ ! -f Makefile ]; then
    ./configure --disable-tests --disable-demos "$@"
fi

# These headers are BUILT_SOURCES in Makefile.am, so a plain `make all` picks them
# up automatically, but targeting libvkd3d-shader.la directly does not.
make \
    include/vkd3d_d3d10shader.h include/vkd3d_d3d10_1shader.h include/vkd3d_d3d11shader.h \
    include/vkd3d_d3d12.h include/vkd3d_d3d12sdklayers.h include/vkd3d_d3d12shader.h \
    include/vkd3d_d3dcommon.h include/vkd3d_d3dx9shader.h include/vkd3d_dxgi.h \
    include/vkd3d_dxgi1_2.h include/vkd3d_dxgi1_3.h include/vkd3d_dxgi1_4.h \
    include/vkd3d_dxgibase.h include/vkd3d_dxgiformat.h include/vkd3d_dxgitype.h \
    include/private/spirv_grammar.h include/private/vkd3d_version.h

make libvkd3d-shader.la
