file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
  *.rc
)

list(SORT SRC_FILES)

# Mirrors windows-emulator/CMakeLists.txt's SOGEN_HAS_VKD3D_SHADER exclusion: d3d9_host.cpp (and
# the parse_vertex_decl() it defines) isn't compiled in on Android/iOS/Emscripten, so this test
# for it can't be either.
if(ANDROID OR IOS OR EMSCRIPTEN)
  list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/d3d9_vertex_decl_test.cpp")
endif()

add_executable(windows-emulator-test ${SRC_FILES})

sogen_assign_source_group(${SRC_FILES})

target_link_libraries(windows-emulator-test PRIVATE
  gtest
  gtest_main
  windows-emulator
  backend-selection
  vulkan-bridge-marshal
)

# A compile-time absolute path to the source tree doesn't survive this project's split
# build/test CI jobs (the smoke-test job downloads the built artifact rather than rebuilding
# from source, so the source tree isn't present there) - copy the fixtures next to the test
# binary instead, so they travel with it, and resolve the path at runtime relative to argv0.
add_custom_command(TARGET windows-emulator-test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/fixtures" "$<TARGET_FILE_DIR:windows-emulator-test>/fixtures"
  COMMENT "Copying test fixtures next to windows-emulator-test")

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_dependencies(windows-emulator-test test-sample)
endif()

add_test(NAME windows-emulator-test
         COMMAND "${PYTHON3_EXE}" "${PROJECT_SOURCE_DIR}/deps/gtest-parallel/gtest_parallel.py" ./windows-emulator-test
         WORKING_DIRECTORY "$<TARGET_FILE_DIR:windows-emulator-test>")

sogen_targets_set_folder("tests" windows-emulator-test)

sogen_strip_target(windows-emulator-test)

# See src/analyzer/CMakeLists.txt: entitlement for the FEX backend's opt-in HVF path.
if(APPLE AND SOGEN_ENABLE_FEX)
  add_custom_command(TARGET windows-emulator-test POST_BUILD
    COMMAND codesign --sign - --force --entitlements "${PROJECT_SOURCE_DIR}/src/backends/fex-emulator/hvf/hvf-entitlements.plist" "$<TARGET_FILE:windows-emulator-test>"
    COMMENT "Codesigning windows-emulator-test with the hypervisor entitlement")
endif()
