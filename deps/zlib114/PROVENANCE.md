# Vendored zlib 1.1.4 (frozen historical drop)

This directory is a verbatim source drop of **zlib 1.1.4** (released March 11th,
2002), included as a build dependency rather than a git submodule because 1.1.4
is a fixed, no-longer-maintained historical release with no live upstream to
track.

## Why this exact version

Call of Duty: Modern Warfare 2 (`iw4sp.exe`) statically links stock zlib 1.1.4
and decompresses its game assets through it. Sogen recognises the compiled
`inflate` / `inflateInit2_` / `inflateEnd` entry points in the guest and
substitutes native host calls into this byte-identical build (a standard
high-level-emulation redirect). Using the *same* zlib version the guest itself
uses is required for byte-identical output: zlib's inflate engine was fully
rewritten between 1.1.4 and 1.2.0, and the exact number of input bytes consumed
at an output-limited streaming stop is implementation-defined, so a different
version could silently desync a streaming decode. See
`src/windows-emulator/windows_emulator.cpp` (`install_iw4sp_zlib_hooks`) and
HANDOFF_MACBOOK.md §81-83.

## Provenance

- Upstream archive: `https://zlib.net/fossils/zlib-1.1.4.tar.gz`
- Archive SHA-256: `9e3e973174f9910fd51539ef9ce94c86a3943d4f897fab8e9adf4b19e6a8291e`

The version strings in this source (`"1.1.4"` / the `inflate 1.1.4 Copyright
1995-2002 Mark Adler` banner) match byte-for-byte the strings embedded in the
staged `iw4sp.exe` (SHA-256 `4dd53fabf9677980aaf081009584ade99d6bf440330046e5c8950e87ef0547d9`).

## What was included / omitted

The complete compression-library source (all `.c`/`.h` that make up `libz`) plus
`README` (which carries the zlib licence) and `ChangeLog`. The stand-alone
example/utility programs from the tarball (`example.c`, `minigzip.c`,
`maketree.c`) are omitted as they are not part of the library.

## Local modifications

None. The source is unmodified upstream. The single portability adjustment
needed on a modern Apple-clang host — undefining the compiler-predefined
`TARGET_OS_MAC` macro so 1.1.4's `zutil.h` does not try to `#include <unix.h>`
(a Carbon-era header) — is applied as a build flag in `deps/zlib114.cmake`, not
by editing the sources, keeping this drop verifiable against the upstream hash.

## Licence

zlib licence (permissive). Full text at the end of `README`.
