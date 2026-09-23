# Fixes Unicorn's Apple-Silicon JIT write-protect handling for the iOS Simulator target, entirely
# from the CMake side — deps/unicorn is a shallow clone of the upstream, non-fork momo5502/unicorn
# repo (see .gitmodules), so a source edit inside it would not survive a fresh checkout. See
# cmake/unicorn-ios-simulator-jit-shim.h for the full explanation of the two underlying bugs and
# how force-including that header (via -include, below) fixes them without touching the submodule.
#
# Scope: only the iphonesimulator SDK (set by cmake/toolchain/ios-simulator.cmake). Native macOS
# and the iphoneos device target are untouched and keep relying on Unicorn's own upstream logic.
#
# By the time this file runs (after add_subdirectory(unicorn) has triggered the platform/compiler
# checks), CMAKE_OSX_SYSROOT has been resolved from the short SDK name set by the toolchain file
# into the SDK's full absolute path (e.g. ".../Platforms/iPhoneSimulator.platform/.../SDKs/
# iPhoneSimulatorNN.N.sdk"), so a plain STREQUAL "iphonesimulator" match never fires - match on the
# resolved path instead.
if(CMAKE_OSX_SYSROOT MATCHES "iPhoneSimulator")
    set(_SOGEN_UNICORN_IOS_SIM_JIT_SHIM "${CMAKE_CURRENT_LIST_DIR}/unicorn-ios-simulator-jit-shim.h")

    foreach(_sogen_unicorn_jit_target unicorn unicorn-common x86_64-softmmu)
        if(TARGET ${_sogen_unicorn_jit_target})
            # Activates the real (non-no-op) branch of every existing HAVE_PTHREAD_JIT_PROTECT
            # check in uc.c / translate-all.c / qemu/osdep.h (USE_MAP_JIT), exactly as if
            # qemu/configure had detected it — which it never can on this target, since the SDK
            # marks the probed symbol unavailable despite it being genuinely usable there.
            target_compile_definitions(${_sogen_unicorn_jit_target} PRIVATE HAVE_PTHREAD_JIT_PROTECT)

            # Pre-defines the TCG_APPLE_JIT_H include guard so the real
            # deps/unicorn/qemu/include/tcg/tcg-apple-jit.h becomes a no-op wherever it is
            # subsequently #include'd (its content is entirely wrapped in that guard) - this shim
            # header's own declarations are used instead for the whole translation unit.
            #
            # deps/unicorn's own CMakeLists.txt already applies its own "-include x86_64.h" to this
            # same target; a second, separately-added "-include" PRIVATE compile option (as two
            # plain list items) gets silently deduplicated by CMake down to just the bare path with
            # its "-include" flag dropped (verified: produces a bare stray filename argument on the
            # command line, which clang then rejects as an extra input --
            # "cannot specify -o when generating multiple output files"). The "SHELL:" prefix
            # (CMake >= 3.12) opts this option out of that de-duplication.
            target_compile_options(${_sogen_unicorn_jit_target} PRIVATE "SHELL:-include \"${_SOGEN_UNICORN_IOS_SIM_JIT_SHIM}\"")
        endif()
    endforeach()
endif()
