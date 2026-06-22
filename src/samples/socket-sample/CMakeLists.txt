file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS *.cpp *.hpp)
list(SORT SRC_FILES)
add_executable(socket-sample ${SRC_FILES})

if(WIN)
  target_link_libraries(socket-sample PRIVATE ws2_32)
endif()

sogen_assign_source_group(${SRC_FILES})
