add_executable(analyzer main.cpp)

if(WIN)
  target_sources(analyzer PRIVATE resource.rc)
endif()

sogen_assign_source_group(main.cpp)

target_link_libraries(analyzer PRIVATE windows-analyzer linux-analyzer)

set_property(GLOBAL PROPERTY VS_STARTUP_PROJECT analyzer)

sogen_strip_target(analyzer)

# The FEX backend's opt-in HVF path (EMULATOR_FEX_HVF=1) needs the hypervisor entitlement; ad-hoc
# re-signing is harmless for every other configuration.
if(APPLE AND SOGEN_ENABLE_FEX)
  add_custom_command(TARGET analyzer POST_BUILD
    COMMAND codesign --sign - --force --entitlements "${PROJECT_SOURCE_DIR}/src/backends/fex-emulator/hvf/hvf-entitlements.plist" "$<TARGET_FILE:analyzer>"
    COMMENT "Codesigning analyzer with the hypervisor entitlement")
endif()

add_test(NAME analyzer-test
         COMMAND "${PYTHON3_EXE}" "${CMAKE_CURRENT_LIST_DIR}/../windows-analyzer/test.py"
         WORKING_DIRECTORY "$<TARGET_FILE_DIR:analyzer>")
