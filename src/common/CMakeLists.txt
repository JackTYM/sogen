file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

if(WIN)
  list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/utils/ios_device_jit_mmap_shim.cpp")
endif()

add_library(emulator-common ${SRC_FILES})

sogen_assign_source_group(${SRC_FILES})

target_include_directories(emulator-common INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(emulator-common PUBLIC
  emulator-platform
  Threads::Threads
  libzstd_static
  minidump::minidump
)

if(WIN)
  target_link_libraries(emulator-common PUBLIC
    ws2_32
  )
endif()
