file(GLOB SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
)

list(SORT SRC_FILES)

add_library(fex-hvf STATIC ${SRC_FILES})

target_include_directories(fex-hvf INTERFACE "${CMAKE_CURRENT_LIST_DIR}")
