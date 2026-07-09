file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  *.cpp
  *.hpp
  *.rc
)

list(SORT SRC_FILES)

if(SOGEN_BUILD_STATIC)
	add_library(fex-emulator STATIC ${SRC_FILES})
else()
    add_library(fex-emulator SHARED ${SRC_FILES})
endif()

target_include_directories(fex-emulator INTERFACE "${CMAKE_CURRENT_LIST_DIR}")

target_link_libraries(fex-emulator PUBLIC emulator)
target_link_libraries(fex-emulator PRIVATE emulator-common)

# FEXCore is built standalone via ExternalProject (see deps/CMakeLists.txt) and consumed as the
# imported FEXCore shared library. fex_external builds it; fexcore_artifact copies the result into
# the sogen artifact directory, which is where the fexcore IMPORTED_LOCATION actually points.
target_link_libraries(fex-emulator PRIVATE fexcore)
add_dependencies(fex-emulator fex_external fexcore_artifact)

sogen_strip_target(fex-emulator)
