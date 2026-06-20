file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

list(SORT SRC_FILES)

add_executable(registry-info-sample ${SRC_FILES})

if(WIN)
  target_link_libraries(registry-info-sample PRIVATE advapi32)
endif()

sogen_assign_source_group(${SRC_FILES})
