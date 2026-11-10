add_executable(analyzer main.cpp)

if(WIN)
  target_sources(analyzer PRIVATE resource.rc)
endif()

# Long host paths (see analyzer.manifest). CMake hands .manifest sources to the MSVC-style linker.
if(MSVC)
  target_sources(analyzer PRIVATE analyzer.manifest)
endif()

sogen_assign_source_group(main.cpp)

target_link_libraries(analyzer PRIVATE windows-analyzer linux-analyzer macos-analyzer)

set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT analyzer)

sogen_strip_target(analyzer)
sogen_enable_hypervisor_entitlement(analyzer)

add_test(NAME analyzer-test
         COMMAND "${PYTHON3_EXE}" "${CMAKE_CURRENT_LIST_DIR}/../windows-analyzer/test.py"
         WORKING_DIRECTORY "$<TARGET_FILE_DIR:analyzer>")
