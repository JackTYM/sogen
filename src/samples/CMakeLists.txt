sogen_get_all_targets(EXISTING_TARGETS)

##########################################

add_subdirectory(bad-sample)
add_subdirectory(hook-sample)
add_subdirectory(messagebox-sample)
add_subdirectory(manual-messagebox-sample)
add_subdirectory(manual-controls-sample)
add_subdirectory(custom-paint-sample)
add_subdirectory(test-sample)
add_subdirectory(registry-info-sample)
add_subdirectory(thread-info-sample)
add_subdirectory(process-info-sample)
add_subdirectory(section-sample)
add_subdirectory(sync-sample)
add_subdirectory(file-io-sample)
add_subdirectory(pipe-sample)
add_subdirectory(security-sample)
add_subdirectory(timer-sample)
add_subdirectory(io-ops-sample)
add_subdirectory(misc-ops-sample)
add_subdirectory(socket-sample)
add_subdirectory(vulkan-shim)
add_subdirectory(vulkan-shim-test)

##########################################

sogen_get_all_targets(ALL_TARGETS)
sogen_list_difference("${ALL_TARGETS}" "${EXISTING_TARGETS}" SAMPLE_TARGETS)
sogen_targets_remove_compile_options("${SAMPLE_TARGETS}" /arch:AVX2 -mavx2)
