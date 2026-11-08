#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCC="${1:-$SCRIPT_DIR/../build/bin/pcc}"
CC="${MINGW_CC:-i686-w64-mingw32-gcc}"
AR="${MINGW_AR:-i686-w64-mingw32-ar}"
OBJDUMP="${MINGW_OBJDUMP:-i686-w64-mingw32-objdump}"
MSVC_LINK="${MSVC_LINK:-lld-link}"

for tool in "$CC" "$AR" "$OBJDUMP" "$MSVC_LINK"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool not found on PATH" >&2
        exit 1
    fi
done

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

"$PCC" --target=i386 --hosted=windows --format=coff --out="$tmpdir/library.obj" \
    "$SCRIPT_DIR/hosted_library_impl.c"
"$PCC" --target=i386 --hosted=windows --format=coff --out="$tmpdir/main.obj" \
    "$SCRIPT_DIR/hosted_library_main.c"

python3 - "$tmpdir/main.obj" "$tmpdir/library.obj" <<'PY'
import struct
import sys

for path in sys.argv[1:]:
    data = open(path, "rb").read()
    text = struct.unpack_from("<8sIIIIIIHHI", data, 20)
    code = data[text[4]:text[4] + text[3]]
    assert b"\x53\x56\x57" in code, f"{path}: missing EBX/ESI/EDI saves"
    assert b"\x5f\x5e\x5b" in code, f"{path}: missing EBX/ESI/EDI restores"
    print(f"PASS hosted Windows callee-saved registers: {path}")
PY

"$AR" rcs "$tmpdir/hosted_static.lib" "$tmpdir/library.obj"
"$PCC" --link --target=i386 --hosted=windows --format=coff --windows-linker=mingw \
    --linker="$CC" --out="$tmpdir/static-app.exe" "$tmpdir/main.obj" -L"$tmpdir" -lhosted_static
"$OBJDUMP" -f "$tmpdir/static-app.exe" | grep -Eq 'file format pei-i386'
echo "PASS hosted Windows static .lib"

"$CC" -c -o "$tmpdir/kernel_stub.obj" "$SCRIPT_DIR/hosted_windows_kernel_stub.c"
"$AR" rcs "$tmpdir/kernel32.lib" "$tmpdir/kernel_stub.obj"
"$PCC" --link --target=i386 --hosted=windows --format=coff --windows-linker=msvc \
    --linker="$MSVC_LINK" --out="$tmpdir/msvc-style-app.exe" "$tmpdir/main.obj" \
    -L"$tmpdir" -lhosted_static
"$OBJDUMP" -f "$tmpdir/msvc-style-app.exe" | grep -Eq 'file format pei-i386'
echo "PASS hosted Windows MSVC-style .lib linking"

"$CC" -shared -Wl,--export-all-symbols -Wl,--out-implib,"$tmpdir/hosted_import.lib" \
    -o "$tmpdir/hosted_test.dll" "$tmpdir/library.obj"
"$PCC" --link --target=i386 --hosted=windows --format=coff --windows-linker=mingw \
    --linker="$CC" --out="$tmpdir/dynamic-app.exe" "$tmpdir/main.obj" "$tmpdir/hosted_import.lib"
"$OBJDUMP" -f "$tmpdir/dynamic-app.exe" | grep -Eq 'file format pei-i386'
"$OBJDUMP" -p "$tmpdir/dynamic-app.exe" | grep -q 'hosted_test.dll'
echo "PASS hosted Windows DLL import .lib"
