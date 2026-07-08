# zlib 1.1.4 (vendored, frozen historical drop) -- see deps/zlib114/PROVENANCE.md.
# Built as a small static library and linked only into the windows-emulator, which
# uses it to natively service MW2's guest zlib inflate calls (HLE redirect).

set(ZLIB114_DIR "${CMAKE_CURRENT_LIST_DIR}/zlib114")

add_library(zlib114 STATIC
    "${ZLIB114_DIR}/adler32.c"
    "${ZLIB114_DIR}/compress.c"
    "${ZLIB114_DIR}/crc32.c"
    "${ZLIB114_DIR}/deflate.c"
    "${ZLIB114_DIR}/gzio.c"
    "${ZLIB114_DIR}/infblock.c"
    "${ZLIB114_DIR}/infcodes.c"
    "${ZLIB114_DIR}/inffast.c"
    "${ZLIB114_DIR}/inflate.c"
    "${ZLIB114_DIR}/inftrees.c"
    "${ZLIB114_DIR}/infutil.c"
    "${ZLIB114_DIR}/trees.c"
    "${ZLIB114_DIR}/uncompr.c"
    "${ZLIB114_DIR}/zutil.c"
)

target_include_directories(zlib114 PUBLIC "${ZLIB114_DIR}")

# Modern Apple clang predefines TARGET_OS_MAC, which makes 1.1.4's zutil.h try to
# #include <unix.h> (a Carbon-era header absent on current macOS). Undefine it so
# the vendored source builds unmodified. See deps/zlib114/PROVENANCE.md.
if(APPLE)
  target_compile_options(zlib114 PRIVATE -U TARGET_OS_MAC)
endif()

# Vendored 2002-era C: silence its warnings so it does not trip -Werror builds.
if(NOT MSVC)
  target_compile_options(zlib114 PRIVATE -w)
else()
  target_compile_options(zlib114 PRIVATE /w)
endif()
