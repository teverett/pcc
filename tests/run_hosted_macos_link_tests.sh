#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PCC="${1:-$ROOT_DIR/build/bin/pcc}"
TMP_DIR="$(mktemp -d)"
trap 'rm -f "$TMP_DIR/hello.o" "$TMP_DIR/hello" "$TMP_DIR/abi.o" "$TMP_DIR/abi"; rmdir "$TMP_DIR"' EXIT

if [ "$(uname -s)" != "Darwin" ] || [ "$(uname -m)" != "arm64" ]; then
    echo "hosted macOS link tests require Apple Silicon macOS" >&2
    exit 1
fi

"$PCC" --target=armv8a --hosted=macos --format=macho \
    --out="$TMP_DIR/hello.o" "$ROOT_DIR/example/hello.c"
"$PCC" --link --target=armv8a --hosted=macos --format=macho \
    --out="$TMP_DIR/hello" "$TMP_DIR/hello.o"
[ "$("$TMP_DIR/hello")" = "hello pcc" ]

"$PCC" --target=armv8a --hosted=macos --format=macho \
    --out="$TMP_DIR/abi.o" "$SCRIPT_DIR/hosted_macos_abi.c"
"$PCC" --link --target=armv8a --hosted=macos --format=macho \
    --out="$TMP_DIR/abi" "$TMP_DIR/abi.o"
[ "$("$TMP_DIR/abi")" = "hosted macOS ABI ok" ]
