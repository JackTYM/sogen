# Makes Unicorn's own TCG-buffer mmap() call go through the proven-working JIT26 create+bless
# path (jit26_prepare_region(nil, size)) on real iOS hardware, where TXM/SPTM blocks any
# writable->executable memory transition unless the specific pages have been blessed by an
# attached debugger first. See cmake/unicorn-ios-device-jit-shim.h for the full explanation,
# including why this replaced an earlier bless-after-the-fact attempt that live device testing
# disproved, and cmake/unicorn-ios-simulator-jit.cmake for the sibling Simulator fix this mirrors
# the structure of (force-included header, compile-time macro substitution -- not a runtime hook).
#
# Scope: only the iphoneos device SDK (set by cmake/toolchain/ios.cmake). Native macOS and the
# iphonesimulator target are untouched and keep relying on their own existing logic.
#
# By the time this file runs (after add_subdirectory(unicorn) has triggered the platform/compiler
# checks), CMAKE_OSX_SYSROOT has been resolved from the short SDK name set by the toolchain file
# into the SDK's full absolute path (e.g. ".../Platforms/iPhoneOS.platform/.../SDKs/
# iPhoneOSNN.N.sdk"), so a plain STREQUAL "iphoneos" match never fires -- match on the resolved
# path instead, exactly as unicorn-ios-simulator-jit.cmake does for "iPhoneSimulator".
if(CMAKE_OSX_SYSROOT MATCHES "iPhoneOS")
    set(_SOGEN_UNICORN_IOS_DEVICE_JIT_SHIM "${CMAKE_CURRENT_LIST_DIR}/unicorn-ios-device-jit-shim.h")

    foreach(_sogen_unicorn_device_jit_target unicorn unicorn-common x86_64-softmmu)
        if(TARGET ${_sogen_unicorn_device_jit_target})
            # "SHELL:" (CMake >= 3.12) is required here for the same reason
            # unicorn-ios-simulator-jit.cmake needs it: deps/unicorn's own CMakeLists.txt already
            # applies its own "-include x86_64.h" to this same target, and a second, separately
            # added "-include" option would otherwise get silently deduplicated down to a bare
            # stray filename argument.
            target_compile_options(${_sogen_unicorn_device_jit_target} PRIVATE
                "SHELL:-include \"${_SOGEN_UNICORN_IOS_DEVICE_JIT_SHIM}\"")
        endif()
    endforeach()
endif()
