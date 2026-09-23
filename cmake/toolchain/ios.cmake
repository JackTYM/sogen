set(CMAKE_SYSTEM_NAME "iOS")
# CMake leaves CMAKE_SYSTEM_PROCESSOR empty for an iOS CMAKE_SYSTEM_NAME; several
# CMakeLists here and in deps/ branch on it and would take the wrong arch path.
set(CMAKE_SYSTEM_PROCESSOR "arm64")
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
set(CMAKE_OSX_SYSROOT "iphoneos")
# CMake defaults executables to .app bundles on iOS; this plan wants a plain
# Mach-O to inspect, and bundling belongs to the later app-shell plan.
set(CMAKE_MACOSX_BUNDLE OFF)
