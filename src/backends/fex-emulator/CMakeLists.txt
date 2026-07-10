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
# imported FEXCore shared library. The dependency forces the ExternalProject to build before this target.
target_link_libraries(fex-emulator PRIVATE fexcore)
add_dependencies(fex-emulator fex_external)

# FEXCore's own build sets its shared library's install name to "@rpath/libFEXCore.dylib" (macOS) /
# gives it a soname (Linux), but nothing else points an rpath at the ExternalProject's own build
# directory where that file actually lives - every prior local build "worked" only because of a
# manual `cp .../libFEXCore.dylib build/<preset>/artifacts/` step, which a fresh checkout (or any
# consumer that doesn't happen to run from that specific artifacts directory, e.g. the Python
# extension module's build-time stub-generation step, which needs to dlopen the freshly-built
# module immediately) never gets. Point fex-emulator's own rpath directly at the real build
# location instead, so @rpath/libFEXCore.dylib resolves unconditionally.
get_target_property(_fexcore_lib_path fexcore IMPORTED_LOCATION)
get_filename_component(_fexcore_lib_dir "${_fexcore_lib_path}" DIRECTORY)
set_target_properties(fex-emulator PROPERTIES
  BUILD_RPATH "${_fexcore_lib_dir}"
  INSTALL_RPATH "${_fexcore_lib_dir}")
unset(_fexcore_lib_path)
unset(_fexcore_lib_dir)

sogen_strip_target(fex-emulator)
