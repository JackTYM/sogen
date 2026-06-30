file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

list(SORT SRC_FILES)

add_executable(native-gpu-clear-sample ${SRC_FILES})

target_link_libraries(native-gpu-clear-sample PRIVATE dxgk-command-protocol)

if(WIN)
  target_link_libraries(native-gpu-clear-sample PRIVATE gdi32 user32)
endif()

sogen_assign_source_group(${SRC_FILES})
