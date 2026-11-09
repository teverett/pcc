#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PCC="${1:-$ROOT_DIR/build/bin/pcc}"
BUILD_DIR="$ROOT_DIR/build/hosted-linux-tests"
PREPARE_ONLY="${PCC_HOSTED_LINUX_PREPARE_ONLY:-0}"
TEST_LD="${PCC_HOSTED_TEST_LD:-ld}"
TEST_AS="${PCC_HOSTED_TEST_AS:-as}"
TEST_AR="${PCC_HOSTED_TEST_AR:-ar}"

if [ "$(uname -s)" != "Linux" ] && [ "$PREPARE_ONLY" != "1" ]; then
    echo "hosted ELF/i386 runtime tests require Linux" >&2
    exit 1
fi
command -v "$TEST_AR" >/dev/null
command -v "$TEST_LD" >/dev/null
command -v "$TEST_AS" >/dev/null
mkdir -p "$BUILD_DIR"

python3 "$SCRIPT_DIR/hosted_internal_linker_test.py" "$PCC"
python3 "$SCRIPT_DIR/hosted_linux_linker_test.py" "$PCC"

"$PCC" --target=i386-linux --format=elf --out="$BUILD_DIR/library.o" \
    "$SCRIPT_DIR/hosted_library_impl.c"
"$PCC" --target=i386-linux --format=elf --out="$BUILD_DIR/main.o" \
    "$SCRIPT_DIR/hosted_library_main.c"
rm -f "$BUILD_DIR/libhosted_test.a"
"$TEST_AR" rcs "$BUILD_DIR/libhosted_test.a" "$BUILD_DIR/library.o"
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/static-app" \
    "$BUILD_DIR/main.o" "$BUILD_DIR/libhosted_test.a"
if [ "$PREPARE_ONLY" != "1" ]; then
python3 - "$BUILD_DIR/static-app" <<'PY'
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True)
assert result.returncode == 42, result
assert result.stdout == b"", result.stdout
PY
fi
echo "PASS hosted internal static archive"

"$TEST_LD" -m elf_i386 -shared -soname libhosted_test.so -o "$BUILD_DIR/libhosted_test.so" \
    "$BUILD_DIR/library.o"
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/dynamic-app" \
    "$BUILD_DIR/main.o" -L"$BUILD_DIR" -lhosted_test
cp "$BUILD_DIR/libhosted_test.so" "$BUILD_DIR/libhosted_test.so.1"
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/versioned-library-app" \
    "$BUILD_DIR/main.o" -L"$BUILD_DIR" -l:libhosted_test.so.1
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/versioned-library-path-app" \
    "$BUILD_DIR/main.o" "$BUILD_DIR/libhosted_test.so.1"
python3 - "$BUILD_DIR/dynamic-app" "$BUILD_DIR/libhosted_test.so" <<'PY'
import os
from pathlib import Path
import struct
import subprocess
import sys

executable = Path(sys.argv[1])
shared = Path(sys.argv[2])
data = executable.read_bytes()
phoff = struct.unpack_from("<I", data, 28)[0]
phentsize, phnum = struct.unpack_from("<HH", data, 42)
headers = [struct.unpack_from("<IIIIIIII", data, phoff + i * phentsize) for i in range(phnum)]
interpreter = next(header for header in headers if header[0] == 3)
assert data[interpreter[1]:interpreter[1] + interpreter[4]] == b"/lib/ld-linux.so.2\0"
dynamic = next(header for header in headers if header[0] == 2)
entries = {}
for offset in range(dynamic[1], dynamic[1] + dynamic[4], 8):
    tag, value = struct.unpack_from("<II", data, offset)
    if tag == 0:
        break
    entries.setdefault(tag, []).append(value)
base = 0x08048000
strings = data[entries[5][0] - base:]
needed = [strings[offset:].split(b"\0", 1)[0].decode() for offset in entries.get(1, [])]
assert shared.name in needed, needed
assert entries[20] == [17]
assert entries[2] == [8]
relocation = entries[23][0] - base
_, info = struct.unpack_from("<II", data, relocation)
assert info & 0xff == 7
symbol_index = info >> 8
symbol_table = entries[6][0] - base + symbol_index * 16
symbol_name = struct.unpack_from("<I", data, symbol_table)[0]
assert strings[symbol_name:].split(b"\0", 1)[0] == b"library_value"
env = dict(os.environ, LD_LIBRARY_PATH=str(shared.parent))
if os.environ.get("PCC_HOSTED_LINUX_PREPARE_ONLY") != "1":
    result = subprocess.run([str(executable)], capture_output=True, env=env)
    assert result.returncode == 42, result
PY
echo "PASS hosted internal shared-library imports and PLT"
python3 - "$BUILD_DIR/versioned-library-app" "$BUILD_DIR/versioned-library-path-app" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

env = dict(os.environ, LD_LIBRARY_PATH=str(Path(sys.argv[1]).parent))
for filename in sys.argv[1:]:
    if os.environ.get("PCC_HOSTED_LINUX_PREPARE_ONLY") != "1":
        result = subprocess.run([filename], capture_output=True, env=env)
        assert result.returncode == 42, result
PY
echo "PASS hosted versioned shared-library inputs"

for name in output status; do
    "$PCC" --target=i386-linux --format=elf --out="$BUILD_DIR/$name.o" \
        "$SCRIPT_DIR/hosted_libc_$name.c"
done
LIBRARY_ARGS=()
if [ -n "${PCC_LINUX_LIBRARY_PATHS:-}" ]; then
    IFS=: read -r -a LIBRARY_DIRS <<< "$PCC_LINUX_LIBRARY_PATHS"
    for directory in "${LIBRARY_DIRS[@]}"; do
        LIBRARY_ARGS+=("-L$directory")
    done
fi
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/output-app" \
    "$BUILD_DIR/output.o" "${LIBRARY_ARGS[@]}" -lc
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/status-app" \
    "$BUILD_DIR/status.o" "${LIBRARY_ARGS[@]}" -l c
python3 - "$BUILD_DIR/output-app" "$BUILD_DIR/status-app" <<'PY'
import os
from pathlib import Path
import struct
import subprocess
import sys

for filename in sys.argv[1:]:
    data = Path(filename).read_bytes()
    phoff = struct.unpack_from("<I", data, 28)[0]
    size, count = struct.unpack_from("<HH", data, 42)
    headers = [struct.unpack_from("<IIIIIIII", data, phoff + index * size) for index in range(count)]
    dynamic = next(header for header in headers if header[0] == 2)
    tags = {}
    for at in range(dynamic[1], dynamic[1] + dynamic[4], 8):
        tag, value = struct.unpack_from("<II", data, at)
        if not tag:
            break
        tags.setdefault(tag, []).append(value)
    strings = data[tags[5][0] - 0x08048000:][:tags[10][0]]
    needed = [strings[at:].split(b"\0", 1)[0] for at in tags[1]]
    assert b"libc.so.6" in needed, needed
    assert b"__libc_start_main\0" in strings
    assert b"puts\0" in strings
    assert 0x6ffffff0 in tags and 0x6ffffffe in tags
if os.environ.get("PCC_HOSTED_LINUX_PREPARE_ONLY") != "1":
    result = subprocess.run([sys.argv[1]], capture_output=True)
    assert result.returncode == 0, result
    assert result.stdout == b"hello pcc\n", result.stdout
    result = subprocess.run([sys.argv[2]], capture_output=True)
    assert result.returncode == 37, result
    assert result.stdout == b"returning status\n", result.stdout
PY
echo "PASS hosted real libc import/version metadata"
if [ "$PREPARE_ONLY" != "1" ]; then
    echo "PASS hosted libc initialization, puts and exit status"
fi

cat > "$BUILD_DIR/lifecycle.s" <<'ASM'
.text
.globl lifecycle_value
.type lifecycle_value,@function
lifecycle_value:
    mov value,%eax
    ret
.type initialize,@function
initialize:
    movl $42,value
    ret
.type cleanup,@function
cleanup:
    push %ebx
    mov $4,%eax
    mov $1,%ebx
    mov $message,%ecx
    mov $10,%edx
    int $0x80
    pop %ebx
    ret
.section .rodata
message:
    .ascii "finalized\n"
.data
value:
    .long 0
.section .init_array,"aw"
    .long initialize
.section .fini_array,"aw"
    .long cleanup
.section .note.GNU-stack,"",@progbits
ASM
"$TEST_AS" --32 -o "$BUILD_DIR/lifecycle.o" "$BUILD_DIR/lifecycle.s"
"$TEST_LD" -m elf_i386 -shared -soname liblifecycle.so \
    -o "$BUILD_DIR/liblifecycle.so" "$BUILD_DIR/lifecycle.o"
cat > "$BUILD_DIR/lifecycle-main.c" <<'C'
int lifecycle_value();
int main() { return lifecycle_value(); }
C
"$PCC" --target=i386-linux --format=elf --out="$BUILD_DIR/lifecycle-main.o" \
    "$BUILD_DIR/lifecycle-main.c"
cat > "$BUILD_DIR/lifecycle-status.c" <<'C'
int lifecycle_value();
int main() { return lifecycle_value() - 5; }
C
"$PCC" --target=i386-linux --format=elf --out="$BUILD_DIR/lifecycle-status.o" \
    "$BUILD_DIR/lifecycle-status.c"
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/finalizer-app" \
    "$BUILD_DIR/lifecycle-main.o" -L"$BUILD_DIR" -llifecycle "${LIBRARY_ARGS[@]}" -lc
"$PCC" --link --target=i386-linux --format=elf --out="$BUILD_DIR/finalizer-status-app" \
    "$BUILD_DIR/lifecycle-status.o" -L"$BUILD_DIR" -llifecycle "${LIBRARY_ARGS[@]}" -lc
if [ "$PREPARE_ONLY" != "1" ]; then
python3 - "$BUILD_DIR/finalizer-app" "$BUILD_DIR/finalizer-status-app" <<'PY'
import os
from pathlib import Path
import subprocess
import sys

env = dict(os.environ, LD_LIBRARY_PATH=str(Path(sys.argv[1]).parent))
for filename, status in zip(sys.argv[1:], (42, 37)):
    result = subprocess.run([filename], capture_output=True, env=env)
    assert result.returncode == status, result
    assert result.stdout == b"finalized\n", result.stdout
PY
echo "PASS hosted libc loader constructors and finalizer"
fi
if [ "$PREPARE_ONLY" = "1" ]; then
    echo "Prepared ELF/i386 executables; runtime verification must run on Linux."
fi
