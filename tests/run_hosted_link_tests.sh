#!/usr/bin/env bash
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PCC="${1:-$SCRIPT_DIR/../build/bin/pcc}"

if [ "$(uname -s)" != "Linux" ]; then
    echo "hosted ELF/i386 link tests require Linux binutils" >&2
    exit 1
fi

for tool in ld ar readelf; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool not found on PATH" >&2
        exit 1
    fi
done

python3 "$SCRIPT_DIR/hosted_elf_startup_test.py" "$PCC"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

"$PCC" --target=i386 --hosted --format=elf --out="$tmpdir/library.o" \
    "$SCRIPT_DIR/hosted_library_impl.c"
"$PCC" --target=i386 --hosted --format=elf --out="$tmpdir/main.o" \
    "$SCRIPT_DIR/hosted_library_main.c"

ar rcs "$tmpdir/libhosted_test.a" "$tmpdir/library.o"
"$PCC" --link --target=i386 --hosted --format=elf --out="$tmpdir/static-app" \
    "$tmpdir/main.o" "$tmpdir/libhosted_test.a"
readelf -h "$tmpdir/static-app" | grep -Eq 'Type:.*EXEC'
readelf -s "$tmpdir/static-app" | grep -Eq 'GLOBAL.*library_value$'
python3 - "$tmpdir/static-app" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 42, result
assert result.stdout == b"", result.stdout
PY
echo "PASS hosted static archive"

ld -m elf_i386 -shared -soname libhosted_test.so -o "$tmpdir/libhosted_test.so" "$tmpdir/library.o"
"$PCC" --link --target=i386 --hosted --format=elf --out="$tmpdir/dynamic-app" \
    "$tmpdir/main.o" -L"$tmpdir" -lhosted_test
readelf -h "$tmpdir/dynamic-app" | grep -Eq 'Type:.*EXEC'
readelf -d "$tmpdir/dynamic-app" | grep -q 'Shared library: \[libhosted_test.so\]'
echo "PASS hosted shared library"

for name in output status; do
    "$PCC" --target=i386 --hosted --format=elf --out="$tmpdir/$name.o" \
        "$SCRIPT_DIR/hosted_libc_$name.c"
done

"$PCC" --link --target=i386 --hosted --format=elf --out="$tmpdir/output-app" \
    "$tmpdir/output.o" -lc
"$PCC" --link --target=i386 --hosted --format=elf --out="$tmpdir/status-app" \
    "$tmpdir/status.o" -l c

python3 - "$tmpdir/output-app" "$tmpdir/status-app" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"hello pcc\n", result.stdout

result = subprocess.run([sys.argv[2]], capture_output=True)
assert result.returncode == 37, result
assert result.stdout == b"returning status\n", result.stdout
PY
echo "PASS hosted libc startup"
