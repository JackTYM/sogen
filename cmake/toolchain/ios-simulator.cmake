set(CMAKE_SYSTEM_NAME "iOS")
# CMake leaves CMAKE_SYSTEM_PROCESSOR empty for an iOS CMAKE_SYSTEM_NAME; several
# CMakeLists here and in deps/ branch on it and would take the wrong arch path.
set(CMAKE_SYSTEM_PROCESSOR "arm64")
# An Apple Silicon Mac runs the iOS Simulator natively as arm64; an x86_64 slice
# would only ever be needed on an Intel host, which this project does not target.
set(CMAKE_OSX_ARCHITECTURES "arm64")
# Xcode 27's iPhoneOS SDK libc++ headers hard-error (unsupported-platform #warning as -Werror)
# below iOS 15.0 - this was 14.0 until that toolchain update.
set(CMAKE_OSX_DEPLOYMENT_TARGET 15.0)
set(CMAKE_OSX_SYSROOT "iphonesimulator")
# CMake defaults executables to .app bundles on iOS; the embeddable static-library
# configuration produces no executables, and app bundling belongs to the Xcode project.
set(CMAKE_MACOSX_BUNDLE OFF)
