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

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_dependencies(windows-emulator-test test-sample)
endif()

add_test(NAME windows-emulator-test
         COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=." -- "${PYTHON3_EXE}" "${PROJECT_SOURCE_DIR}/deps/gtest-parallel/gtest_parallel.py" ./windows-emulator-test
         WORKING_DIRECTORY "$<TARGET_FILE_DIR:windows-emulator-test>")

sogen_targets_set_folder("tests" windows-emulator-test)

sogen_strip_target(windows-emulator-test)
