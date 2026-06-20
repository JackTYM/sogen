#!/usr/bin/env bash
# Cross-compile Windows sample binaries using mingw-w64.
# Run from the repo root. Outputs to build/release/artifacts/root/filesys/c/
set -euo pipefail

ARTIFACTS="$(dirname "$0")/../build/release/artifacts/root/filesys/c"
SAMPLES="$(dirname "$0")/../src/samples"
CXX="x86_64-w64-mingw32-g++"
FLAGS="-O2 -std=c++20"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "error: $CXX not found — install mingw-w64" >&2
    exit 1
fi

compile() {
    local name="$1"; shift
    echo "  $name"
    "$CXX" $FLAGS "$SAMPLES/$name"/*.cpp "$@" \
        -o "$ARTIFACTS/$name.exe"
}

mkdir -p "$ARTIFACTS"

compile registry-info-sample -ladvapi32
compile thread-info-sample -lntdll
compile process-info-sample -lpsapi
compile section-sample
compile security-sample
compile timer-sample
compile io-ops-sample
compile misc-ops-sample -ladvapi32

echo "Done. Samples written to $ARTIFACTS"
