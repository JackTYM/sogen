#!/usr/bin/env bash
# Stages the minimal emulation root the iOS app needs, plus the 32-bit guest PE it runs.
#
# The full captured root is ~44 GB, almost all of it sample games/apps under filesys/c that
# native-gpu-clear-sample never touches. windows_emulator only needs the Windows system tree,
# the registry hives and api-set.bin; everything else under filesys/c is dropped.
#
# Usage: tools/stage-ios-emulation-root.sh [/path/to/full/root] [/path/to/output/root]
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_ROOT="${1:-/Users/jack/Documents/Coding/C++/sogen/build/release/artifacts/root}"
OUT_ROOT="${2:-$REPO_ROOT/build/ios-root}"

if [[ ! -d "$SRC_ROOT/filesys/c/windows" ]]; then
  echo "error: '$SRC_ROOT' does not look like a sogen emulation root (no filesys/c/windows)" >&2
  exit 1
fi

echo "==> staging emulation root: $SRC_ROOT -> $OUT_ROOT"
mkdir -p "$OUT_ROOT/filesys/c"
rsync -a --delete "$SRC_ROOT/filesys/c/windows/" "$OUT_ROOT/filesys/c/windows/"
rsync -a --delete "$SRC_ROOT/registry/"          "$OUT_ROOT/registry/"
cp -f "$SRC_ROOT/api-set.bin" "$OUT_ROOT/api-set.bin"

echo "==> building the 32-bit guest PE"
# The sample is a WoW64-only guest: its d3dkmt structs are the packed all-32-bit wire layout
# that sogen's gdi.cpp handlers apply their `is_wow64_process` fixups to. A 64-bit build would
# hand the host handlers the wrong struct shape. src/CMakeLists.txt only adds samples/ under
# WIN32, so this is built directly with mingw-w64 instead of through a CMake preset.
i686-w64-mingw32-g++ \
  -O2 -std=c++20 \
  -static -static-libgcc -static-libstdc++ \
  -I "$REPO_ROOT/src/dxgk-command-protocol" \
  "$REPO_ROOT/src/samples/native-gpu-clear-sample/native-gpu-clear-sample.cpp" \
  -o "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" \
  -luser32 -lgdi32

cp -f "$OUT_ROOT/filesys/c/native-gpu-clear-sample.exe" "$OUT_ROOT/native-gpu-clear-sample.exe"

echo "==> done"
du -sh "$OUT_ROOT"
file "$OUT_ROOT/native-gpu-clear-sample.exe"
