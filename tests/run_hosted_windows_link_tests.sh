#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
RCC="${1:-$ROOT_DIR/build/bin/rcc}"
AR="${MINGW_AR:-}"
BUILD_DIR="$ROOT_DIR/build/hosted-windows-tests"

if [ -z "$AR" ]; then
    if command -v i686-w64-mingw32-ar >/dev/null 2>&1; then
        AR=i686-w64-mingw32-ar
    elif command -v i686-elf-ar >/dev/null 2>&1; then
        AR=i686-elf-ar
    else
        AR=ar
    fi
fi
if ! command -v "$AR" >/dev/null 2>&1; then
    echo "$AR not found on PATH" >&2
    exit 1
fi
mkdir -p "$BUILD_DIR"
if [ -d /opt/bin ]; then
    export PATH="/opt/bin:$PATH"
fi

python3 "$SCRIPT_DIR/hosted_internal_linker_test.py" "$RCC"

"$RCC" --target=i386-windows --format=coff --out="$BUILD_DIR/library.obj" \
    "$SCRIPT_DIR/hosted_library_impl.c"
"$RCC" --target=i386-windows --format=coff --out="$BUILD_DIR/main.obj" \
    "$SCRIPT_DIR/hosted_library_main.c"
"$AR" rcs "$BUILD_DIR/hosted_static.lib" "$BUILD_DIR/library.obj"
"$RCC" --link --target=i386-windows --format=coff --out="$BUILD_DIR/static-app.exe" \
    "$BUILD_DIR/main.obj" -L"$BUILD_DIR" -lhosted_static
python3 - "$BUILD_DIR/static-app.exe" <<'PY'
import os
from pathlib import Path
import struct
import subprocess
import sys

path = Path(sys.argv[1])
data = path.read_bytes()
pe = struct.unpack_from("<I", data, 0x3c)[0]
assert data[:2] == b"MZ" and data[pe:pe + 4] == b"PE\0\0"
assert struct.unpack_from("<H", data, pe + 4)[0] == 0x14c
if os.name == "nt" or os.environ.get("OS") == "Windows_NT":
    result = subprocess.run([str(path)])
    assert result.returncode == 42, result.returncode
PY
echo "PASS hosted internal static .lib and PE output"

if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    COFF_CC=i686-w64-mingw32-gcc
elif command -v clang >/dev/null 2>&1; then
    COFF_CC=clang
else
    echo "No COFF fixture compiler available; skipping external COFF section interoperability"
    COFF_CC=""
fi
if [ -n "$COFF_CC" ]; then
    if [ "$COFF_CC" = clang ]; then
        "$COFF_CC" --target=i686-pc-windows-msvc -fno-addrsig -c \
            "$SCRIPT_DIR/hosted_windows_sections.c" -o "$BUILD_DIR/sections.obj"
    else
        "$COFF_CC" -fno-asynchronous-unwind-tables -c \
            "$SCRIPT_DIR/hosted_windows_sections.c" -o "$BUILD_DIR/sections.obj"
    fi
    "$RCC" --target=i386-windows --format=coff --out="$BUILD_DIR/sections-main.obj" \
        "$SCRIPT_DIR/hosted_windows_sections_main.c"
    "$RCC" --link --target=i386-windows --format=coff --out="$BUILD_DIR/sections.exe" \
        "$BUILD_DIR/sections-main.obj" "$BUILD_DIR/sections.obj"
    python3 - "$BUILD_DIR/sections.exe" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

path = Path(sys.argv[1])
assert path.read_bytes()[:2] == b"MZ"
if os.name == "nt" or os.environ.get("OS") == "Windows_NT":
    result = subprocess.run([str(path)], capture_output=True)
    assert result.returncode == 17, (result.returncode, result.stderr)
PY
    echo "PASS hosted Windows external COFF initialized data and BSS"
    if [ "$COFF_CC" = i686-w64-mingw32-gcc ]; then
        "$COFF_CC" -shared -o "$BUILD_DIR/sample.dll" "$BUILD_DIR/sections.obj" \
            -Wl,--out-implib,"$BUILD_DIR/libsample.dll.a"
        "$RCC" --link --target=i386-windows --format=coff --out="$BUILD_DIR/import-library.exe" \
            "$BUILD_DIR/sections-main.obj" "$BUILD_DIR/libsample.dll.a"
        "$RCC" --link --target=i386-windows --format=coff --out="$BUILD_DIR/import-dll.exe" \
            "$BUILD_DIR/sections-main.obj" -L"$BUILD_DIR" -lsample
        python3 - "$BUILD_DIR" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

directory = Path(sys.argv[1])
for name in ("import-library.exe", "import-dll.exe"):
    path = directory / name
    assert b"sample.dll\0" in path.read_bytes()
    if os.name == "nt" or os.environ.get("OS") == "Windows_NT":
        result = subprocess.run([str(path)], capture_output=True)
        assert result.returncode == 17, (name, result.returncode, result.stderr)
print("PASS hosted Windows GNU import library and direct DLL export resolution")
PY
    fi
fi

if [ "${OS:-}" = "Windows_NT" ]; then
    for fixture in hosted_libc_output hosted_libc_status; do
        "$RCC" --target=i386-windows --format=coff --out="$BUILD_DIR/$fixture.obj" \
            "$SCRIPT_DIR/$fixture.c"
        "$RCC" --link --target=i386-windows --format=coff --out="$BUILD_DIR/$fixture.exe" \
            "$BUILD_DIR/$fixture.obj" -lmsvcrt
    done
    python3 - "$BUILD_DIR" <<'PY'
from pathlib import Path
import subprocess
import sys

directory = Path(sys.argv[1])
for name, expected, status in [
    ("hosted_libc_output", b"hello rcc\r\n", 0),
    ("hosted_libc_status", b"returning status\r\n", 37),
]:
    result = subprocess.run([str(directory / (name + ".exe"))], capture_output=True)
    assert result.stdout == expected, (name, result.stdout, result.stderr)
    assert result.returncode == status, (name, result.returncode, result.stderr)
print("PASS hosted Windows real CRT captured stdout and main return status")
PY
fi
