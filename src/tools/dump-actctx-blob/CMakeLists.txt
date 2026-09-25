file(GLOB SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

list(SORT SRC_FILES)

add_executable(dump-actctx-blob ${SRC_FILES})

sogen_assign_source_group(${SRC_FILES})

target_link_libraries(dump-actctx-blob PRIVATE
  ntdll
)

sogen_strip_target(dump-actctx-blob)

add_subdirectory(test-exes)
