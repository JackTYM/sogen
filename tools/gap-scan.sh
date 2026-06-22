#!/usr/bin/env bash
# Run a binary under analyzer and rank emulation gaps by frequency.
# Usage: gap-scan.sh <exe> [analyzer-args...]
# Run from build/release/artifacts/ or set ARTIFACTS_DIR.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <exe> [analyzer-args...]" >&2
    exit 1
fi

EXE="$1"; shift
ARTIFACTS="${ARTIFACTS_DIR:-$(dirname "$0")/../build/release/artifacts}"
ANALYZER="$ARTIFACTS/analyzer"

if [[ ! -x "$ANALYZER" ]]; then
    echo "error: analyzer not found at $ANALYZER" >&2
    exit 1
fi

timeout 10s "$ANALYZER" -e root -v "$EXE" "$@" 2>&1 | \
    { grep -oP '(Unimplemented syscall: \S+|Unsupported \w[\w ]*class: 0x[0-9a-fA-F]+|Unsupported \w[\w ]*IOCTL: 0x[0-9a-fA-F]+|Unsupported \w[\w ]*FSCTL: 0x[0-9a-fA-F]+|Unknown syscall: 0x[0-9a-fA-F]+|\[gpu-bridge\] Unsupported \S+)' || true; } | \
    sort | uniq -c | sort -rn
