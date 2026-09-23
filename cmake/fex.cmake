# FEX-Emu (x86 -> AArch64 JIT). Built only when the FEX backend is enabled (ARM64 Linux/Darwin/Android +
# Clang, resolved at the top level). FEX is not designed to be embedded via add_subdirectory: it
# assumes it is the top-level project (~60 uses of CMAKE_SOURCE_DIR) and configures its whole
# loader/tools tree. Instead build it standalone via ExternalProject and consume the resulting
# self-contained shared FEXCore library (the FEXCore_shared target). Only that target is built -
# not the FEX tools.
if(SOGEN_ENABLE_FEX)
  include(ExternalProject)

  set(_FEX_SRC "${CMAKE_CURRENT_SOURCE_DIR}/FEX")
  set(_FEXCORE_IOS_ARGS "")

  # iOS forbids dynamically-loaded libraries outside the app bundle's own frameworks, so FEXCore
  # is linked in statically there (the same reason MoltenVK is -force_load'd as a static archive
  # into the iOS app). The nested ExternalProject configure does not inherit the outer toolchain
  # file automatically, so it must be forwarded explicitly along with the sysroot/arch/processor
  # the outer iOS toolchain already resolved - otherwise it would silently configure for the host
  # Mac instead of cross-compiling.
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_FEXCORE_BUILD_TARGET "FEXCore")
    set(_FEXCORE_SHARED_LIB "libFEXCore.a")
    set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
    set(_FEXCORE_IOS_ARGS
      -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
      -DCMAKE_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}
      -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}
      -DCMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR})
    # FEX_IOS_POLL_INTERRUPT switches the JIT's cooperative-preemption check at block entry from
    # the InterruptFaultPage fault trick (dead on real iOS device - a permanently-attached
    # debugger claims every hardware exception before sogen's own signal handler ever sees it) to
    # a plain polled flag. Real-device only - the Simulator has no attached-debugger fault
    # problem, so it keeps using the normal InterruptFaultPage mechanism. By the time this file
    # runs, CMAKE_OSX_SYSROOT has already been resolved from the toolchain's short SDK name into
    # the full absolute SDK path, so this must match the resolved path's substring rather than
    # STREQUAL the short name.
    if(CMAKE_OSX_SYSROOT MATCHES "iPhoneOS")
      list(APPEND _FEXCORE_IOS_ARGS -DCMAKE_CXX_FLAGS=-DFEX_IOS_POLL_INTERRUPT)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_FEXCORE_BUILD_TARGET "FEXCore_shared")
    set(_FEXCORE_SHARED_LIB "libFEXCore.dylib")
    set(_FEXCORE_OSX_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
  else()
    set(_FEXCORE_BUILD_TARGET "FEXCore_shared")
    set(_FEXCORE_SHARED_LIB "libFEXCore.so")
    set(_FEXCORE_OSX_ARGS "")
  endif()

  if(CMAKE_TOOLCHAIN_FILE)
    set(_FEXCORE_TOOLCHAIN_ARGS -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})
  else()
    set(_FEXCORE_TOOLCHAIN_ARGS "")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    list(APPEND _FEXCORE_TOOLCHAIN_ARGS
      -DANDROID_ABI:STRING=${CMAKE_ANDROID_ARCH_ABI}
      -DANDROID_PLATFORM:STRING=${CMAKE_SYSTEM_VERSION}
    )
  endif()

  # Propagate AddressSanitizer into the FEXCore build so the whole chain is instrumented
  # consistently (mismatched ASan instrumentation across shared libraries causes false positives).
  if(SOGEN_ENABLE_SANITIZER)
    set(_FEXCORE_SANITIZER_ARGS -DENABLE_ASAN=ON)
  else()
    set(_FEXCORE_SANITIZER_ARGS "")
  endif()

  # Mirrors compiler-env.cmake's own LTO gate (SOGEN_ENABLE_CLANG_TIDY excluded there since IPO
  # conflicts with clang-tidy's per-TU analysis) so the tidy preset doesn't LTO this sub-build while
  # leaving the rest of the tree without it.
  if(SOGEN_ENABLE_LTO AND NOT SOGEN_ENABLE_CLANG_TIDY)
    set(_FEXCORE_LTO_ARGS -DENABLE_LTO=ON)
  else()
    set(_FEXCORE_LTO_ARGS -DENABLE_LTO=OFF)
  endif()

  # The sub-build is always single-config (Ninja) and needs a concrete build type at configure time.
  # A multi-config outer generator has none - it picks the configuration at build time - so there is
  # nothing to forward and FEXCore gets an optimized default instead.
  get_property(_FEXCORE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(_FEXCORE_MULTI_CONFIG OR NOT CMAKE_BUILD_TYPE)
    set(_FEXCORE_BUILD_TYPE "RelWithDebInfo")
  else()
    set(_FEXCORE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
  endif()

  ExternalProject_Add(fex_external
    SOURCE_DIR "${_FEX_SRC}"
    CMAKE_GENERATOR "Ninja"
    CMAKE_ARGS
      -DCMAKE_BUILD_TYPE=${_FEXCORE_BUILD_TYPE}
      -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
      ${_FEXCORE_OSX_ARGS}
      ${_FEXCORE_TOOLCHAIN_ARGS}
      ${_FEXCORE_IOS_ARGS}
      ${_FEXCORE_SANITIZER_ARGS}
      ${_FEXCORE_LTO_ARGS}
      -DENABLE_CCACHE=OFF
      -DBUILD_FEXCONFIG=OFF
      -DBUILD_THUNKS=OFF
      -DBUILD_FEX_LINUX_TESTS=OFF
      -DBUILD_TESTING=OFF
      -DENABLE_OFFLINE_TELEMETRY=OFF
      # Do not let FEX replace the process allocator; sogen owns it when FEXCore is embedded.
      -DENABLE_FEX_ALLOCATOR=OFF
      -DENABLE_JEMALLOC_GLIBC_ALLOC=OFF
    # Build only the self-contained core library, not FEX's loader/server tools: the static
    # FEXCore target on iOS (no dynamic loading outside the app bundle), FEXCore_shared elsewhere.
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target ${_FEXCORE_BUILD_TARGET}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS <BINARY_DIR>/FEXCore/Source/${_FEXCORE_SHARED_LIB}
  )

  ExternalProject_Get_property(fex_external BINARY_DIR)

  # CI builds and tests run in separate jobs/runners, exchanging only the artifacts output
  # directory (build/<preset>/artifacts/) as an uploaded/downloaded tarball - the ExternalProject's
  # own build tree (where fexcore's IMPORTED_LOCATION below points) never leaves the build job's
  # runner. Copy the library into the artifacts directory too, alongside fex-emulator's own
  # output, so the test job's @loader_path-relative rpath resolves it without needing that
  # build-tree path to exist.
  add_custom_command(
    TARGET fex_external POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_SHARED_LIB}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
    COMMENT "Copying libFEXCore shared library to the artifacts directory"
  )

  # CMake validates that every directory in an IMPORTED target's INTERFACE_INCLUDE_DIRECTORIES
  # exists at generate time. ${BINARY_DIR}/include (FEXCore's build-time-generated headers, see
  # below) doesn't exist yet on a fresh checkout - it's only created once the fex_external
  # ExternalProject actually builds - so create an empty placeholder now; the real generated
  # headers land in the same directory later, when FEXCore_shared builds.
  file(MAKE_DIRECTORY "${BINARY_DIR}/include")

  # Imported view of the FEXCore shared library for consumers (fex-emulator). The consuming target
  # must also add_dependencies(... fex_external) so the ExternalProject builds first.
  #
  # FEXCore's public headers transitively include FEX's vendored fmt (LogManager.h) and
  # unordered_dense (fextl/robin_map.h), so consumers need those include dirs too. FMT_HEADER_ONLY
  # keeps fmt fully inline in the consumer TU, avoiding a link dependency on fmt symbols that the
  # hidden-visibility FEXCore library does not re-export. FEXCore also generates some of its own
  # headers at build time (Config/ConfigValues.inl, IR/IRDefines.inc, ...) into its binary dir's
  # include/ - consumers need that include dir too.
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_library(fexcore STATIC IMPORTED GLOBAL)
  else()
    add_library(fexcore SHARED IMPORTED GLOBAL)
  endif()
  set_target_properties(fexcore PROPERTIES
    IMPORTED_LOCATION "${BINARY_DIR}/FEXCore/Source/${_FEXCORE_SHARED_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${_FEX_SRC}/FEXCore/include;${BINARY_DIR}/include;${_FEX_SRC}/External/fmt/include;${_FEX_SRC}/External/unordered_dense/include"
    INTERFACE_COMPILE_DEFINITIONS "FMT_HEADER_ONLY=1")
endif()
