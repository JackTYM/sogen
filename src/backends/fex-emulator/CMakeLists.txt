file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
  *.rc
)

# hvf/ holds the standalone HVF spike target (sogen-hvf-spike below), not library sources.
list(FILTER SRC_FILES EXCLUDE REGEX "/hvf/")

list(SORT SRC_FILES)

if(SOGEN_BUILD_STATIC)
	add_library(fex-emulator STATIC ${SRC_FILES})
else()
    add_library(fex-emulator SHARED ${SRC_FILES})
endif()

target_include_directories(fex-emulator INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

target_link_libraries(fex-emulator PUBLIC emulator)
target_link_libraries(fex-emulator PRIVATE emulator-common)

# FEXCore is built standalone via ExternalProject (see deps/CMakeLists.txt) and consumed as the
# imported FEXCore shared library. The dependency forces the ExternalProject to build before this target.
target_link_libraries(fex-emulator PRIVATE fexcore)
add_dependencies(fex-emulator fex_external)

sogen_strip_target(fex-emulator)

# Phase-0 feasibility spike: run real FEXCore JIT output inside a Hypervisor.framework vCPU with
# hardware TSO (ACTLR_EL1.EnTSO). Standalone experiment binary, deliberately not linked into the
# backend. Requires the com.apple.security.hypervisor entitlement, applied via ad-hoc codesign.
if(APPLE)
  add_executable(sogen-hvf-spike hvf/hvf_spike.cpp)
  target_link_libraries(sogen-hvf-spike PRIVATE fexcore "-framework Hypervisor")
  add_dependencies(sogen-hvf-spike fex_external)
  set_target_properties(sogen-hvf-spike PROPERTIES BUILD_RPATH "@loader_path")
  add_custom_command(TARGET sogen-hvf-spike POST_BUILD
    COMMAND codesign --sign - --force --entitlements "${CMAKE_CURRENT_LIST_DIR}/hvf/hvf-entitlements.plist" "$<TARGET_FILE:sogen-hvf-spike>"
    COMMENT "Codesigning sogen-hvf-spike with the hypervisor entitlement")
endif()
