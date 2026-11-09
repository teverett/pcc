#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PCC="${1:-$ROOT_DIR/build/bin/pcc}"
BUILD_DIR="$ROOT_DIR/build/hosted-macos-tests"

if [ "$(uname -s)" != "Darwin" ] || [ "$(uname -m)" != "arm64" ]; then
    echo "hosted macOS runtime tests require Apple Silicon macOS" >&2
    exit 1
fi

mkdir -p "$BUILD_DIR"
python3 "$SCRIPT_DIR/hosted_internal_linker_test.py" "$PCC"

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/hello.o" "$ROOT_DIR/example/hello.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/hello" "$BUILD_DIR/hello.o" -lSystem
codesign --verify "$BUILD_DIR/hello"
dyld_info -validate_only "$BUILD_DIR/hello"
otool -L "$BUILD_DIR/hello" | grep -q '/usr/lib/libSystem.B.dylib'
nm -u "$BUILD_DIR/hello" | grep -q '_puts'
python3 - "$BUILD_DIR/hello" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"hello pcc\n", result.stdout
PY

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/abi.o" "$SCRIPT_DIR/hosted_macos_abi.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/abi" "$BUILD_DIR/abi.o" -lSystem
python3 - "$BUILD_DIR/abi" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"hosted macOS ABI ok\n", result.stdout
PY

for case in global zero_global; do
    "$PCC" --target=armv8a-macos --format=macho \
        --out="$BUILD_DIR/$case.o" "$SCRIPT_DIR/hosted_macos_$case.c"
    "$PCC" --link --target=armv8a-macos --format=macho \
        --out="$BUILD_DIR/$case" "$BUILD_DIR/$case.o" -lSystem
    codesign --verify "$BUILD_DIR/$case"
    dyld_info -validate_only "$BUILD_DIR/$case"
done
python3 - "$BUILD_DIR/global" "$BUILD_DIR/zero_global" <<'PY'
import subprocess
import sys

for path, expected in zip(sys.argv[1:], (37, 42)):
    result = subprocess.run([path], capture_output=True)
    assert result.returncode == expected, (path, result)
    assert result.stdout == b"", (path, result.stdout)
PY

"${CC:-clang}" -arch arm64 -dynamiclib -fno-stack-protector \
    -Wl,-install_name,@rpath/libpccfixture.dylib \
    "$SCRIPT_DIR/hosted_macos_dylib_fixture.c" -o "$BUILD_DIR/libpccfixture.dylib"
"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/dylib-output.o" "$SCRIPT_DIR/hosted_macos_dylib.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/dylib-output" "$BUILD_DIR/dylib-output.o" \
    -L "$BUILD_DIR" -lpccfixture -lSystem
codesign --verify "$BUILD_DIR/dylib-output"
dyld_info -validate_only "$BUILD_DIR/dylib-output"
python3 - "$BUILD_DIR/dylib-output" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"custom dylib import ok\n", result.stdout
PY

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/dylib-exit.o" "$SCRIPT_DIR/hosted_macos_dylib_exit.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/dylib-exit" "$BUILD_DIR/dylib-exit.o" \
    -L "$BUILD_DIR" -lpccfixture -lSystem
python3 - "$BUILD_DIR/dylib-exit" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"", result.stdout
PY

"${CC:-clang}" -arch arm64 -c "$SCRIPT_DIR/hosted_macos_archive_root.s" -o "$BUILD_DIR/archive-root.o"
"${CC:-clang}" -arch arm64 -c "$SCRIPT_DIR/hosted_macos_archive_leaf.s" -o "$BUILD_DIR/archive-leaf.o"
"${CC:-clang}" -arch arm64 -c "$SCRIPT_DIR/hosted_macos_archive_unused.s" -o "$BUILD_DIR/archive-unused.o"
ar rcs "$BUILD_DIR/libhostedarchive.a" "$BUILD_DIR/archive-root.o" \
    "$BUILD_DIR/archive-leaf.o" "$BUILD_DIR/archive-unused.o"
"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/archive.o" "$SCRIPT_DIR/hosted_macos_archive.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/archive" "$BUILD_DIR/archive.o" -L "$BUILD_DIR" \
    -lhostedarchive -lSystem
python3 - "$BUILD_DIR/archive" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"", result.stdout
PY

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/archive-leaf-main.o" "$SCRIPT_DIR/hosted_macos_archive_leaf.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/archive-leaf-main" "$BUILD_DIR/archive-leaf-main.o" \
    -L "$BUILD_DIR" -lhostedarchive -lSystem
python3 - "$BUILD_DIR/archive-leaf-main" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
assert result.stdout == b"", result.stdout
PY

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/unresolved.o" "$SCRIPT_DIR/hosted_macos_unresolved.c"
printf 'preserve destination on failed link\n' > "$BUILD_DIR/atomic-output"
if "$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/atomic-output" "$BUILD_DIR/unresolved.o" -lSystem \
    2>"$BUILD_DIR/unresolved-link.log"; then
    echo "expected unresolved macOS symbol failure" >&2
    exit 1
fi
grep -q "intentionally_missing_symbol" "$BUILD_DIR/unresolved-link.log"
grep -q "preserve destination on failed link" "$BUILD_DIR/atomic-output"

"${CC:-clang}" -arch x86_64 -c "$SCRIPT_DIR/hosted_macos_wrong_arch.c" \
    -o "$BUILD_DIR/wrong-arch.o"
printf 'preserve destination on invalid input\n' > "$BUILD_DIR/atomic-output"
if "$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/atomic-output" "$BUILD_DIR/wrong-arch.o" \
    2>"$BUILD_DIR/wrong-arch-link.log"; then
    echo "expected non-arm64 object failure" >&2
    exit 1
fi
grep -q "not arm64" "$BUILD_DIR/wrong-arch-link.log"
grep -q "preserve destination on invalid input" "$BUILD_DIR/atomic-output"

"${CC:-clang}" -arch arm64 -c "$SCRIPT_DIR/hosted_macos_bss.s" -o "$BUILD_DIR/bss.o"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/bss" "$BUILD_DIR/bss.o"
otool -l "$BUILD_DIR/bss" | grep -q 'segname __DATA'
python3 - "$BUILD_DIR/bss" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 0, result
PY

"$PCC" --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/constant.o" "$SCRIPT_DIR/hosted_macos_constant.c"
"$PCC" --link --target=armv8a-macos --format=macho \
    --out="$BUILD_DIR/constant" "$BUILD_DIR/constant.o"
codesign --verify "$BUILD_DIR/constant"
dyld_info -validate_only "$BUILD_DIR/constant"
if otool -L "$BUILD_DIR/constant" | grep -q '/usr/lib/libSystem.B.dylib'; then
    echo "libc-free macOS link unexpectedly loaded libSystem" >&2
    exit 1
fi
python3 - "$BUILD_DIR/constant" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 7, result
assert result.stdout == b"", result.stdout
PY
