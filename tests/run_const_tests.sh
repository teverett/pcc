#!/usr/bin/env bash
# Unit tests for the 'const' qualifier (grammar/CParser.g4's 'cst' token on
# param/globalDecl/simpleStmt, src/ast/BuildAst.cpp's Param::isConst/
# GlobalVar::isConst/Stmt::isConst, src/ir/IRBuilder.cpp's rejectConstWrite())
# - see docs/language.md's "Const declarations" section for exactly what
# this does and doesn't protect.
#
# Tests (i386, --format=asm, no simulator needed):
#   test_const_global_write_rejected       - assigning to a 'const' global
#       must be a compile error.
#   test_const_local_write_rejected        - assigning to a 'const' local
#       must be a compile error.
#   test_const_param_write_rejected        - assigning to a 'const'
#       parameter must be a compile error.
#   test_const_array_element_write_rejected - writing to an element of a
#       'const' array must be a compile error.
#   test_const_incdec_rejected             - '++'/'--' on a 'const' local
#       must be a compile error (it desugars into the same compound-assign
#       path a '+=' would).
#   test_const_global_without_initializer_rejected - a non-'extern' 'const'
#       global with no initializer must be a compile error (there would
#       otherwise be no way to ever give it a value).
#   test_const_write_through_pointer_accepted - writing through a 'const'
#       pointer variable's own pointee (`*p = x`/`p[i] = x`) is NOT
#       rejected - this subset tracks 'const' on the declared name, not
#       per pointer-indirection level (see docs/language.md).
#   test_const_compiles_and_links           - a program that only reads
#       'const' globals/locals/params compiles and links cleanly.
#
# Usage: tests/run_const_tests.sh [path/to/rcc]

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RCC="$SCRIPT_DIR/../build/bin/rcc"
[ $# -ge 1 ] && RCC="$1"

if [ ! -x "$RCC" ]; then
    echo "rcc binary not found at $RCC - build it first ('make')" >&2
    exit 1
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
fail=0

test_const_global_write_rejected() {
    local dir="$tmpdir/global"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
const int limit = 10;
int main() {
    limit = 20;
    return limit;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_global_write_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_global_write_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_global_write_rejected"
    return 0
}

test_const_local_write_rejected() {
    local dir="$tmpdir/local"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    const int x = 5;
    x = 6;
    return x;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_local_write_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_local_write_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_local_write_rejected"
    return 0
}

test_const_param_write_rejected() {
    local dir="$tmpdir/param"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int bump(const int x) {
    x = x + 1;
    return x;
}
int main() {
    return bump(1);
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_param_write_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_param_write_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_param_write_rejected"
    return 0
}

test_const_array_element_write_rejected() {
    local dir="$tmpdir/array"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
const int table[3] = {1, 2, 3};
int main() {
    table[0] = 9;
    return table[0];
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_array_element_write_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_array_element_write_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_array_element_write_rejected"
    return 0
}

test_const_incdec_rejected() {
    local dir="$tmpdir/incdec"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    const int x = 5;
    x++;
    return x;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_incdec_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_incdec_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_incdec_rejected"
    return 0
}

test_const_global_without_initializer_rejected() {
    local dir="$tmpdir/noinit"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
const int limit;
int main() {
    return limit;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_global_without_initializer_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "const" "$dir/log"; then
        echo "FAIL test_const_global_without_initializer_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_global_without_initializer_rejected"
    return 0
}

test_const_write_through_pointer_accepted() {
    local dir="$tmpdir/throughptr"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
int main() {
    int v = 1;
    const int *p = &v;
    *p = 2;
    p[0] = 3;
    return v;
}
EOF
    if ! "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_write_through_pointer_accepted: expected this to compile (const is tracked" \
             "per declared name, not per pointer-indirection level - see docs/language.md), but it failed:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_write_through_pointer_accepted"
    return 0
}

test_const_compiles_and_links() {
    local dir="$tmpdir/link"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
const int limit = 10;
static const int bias = 1;

int clamp(const int x) {
    const int half = limit / 2;
    return x > half ? half : x;
}

int main() {
    return clamp(3) + clamp(9) + bias;
}
EOF
    if ! "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_const_compiles_and_links: expected this to compile, but it failed:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_const_compiles_and_links"
    return 0
}

test_const_global_write_rejected || fail=1
test_const_local_write_rejected || fail=1
test_const_param_write_rejected || fail=1
test_const_array_element_write_rejected || fail=1
test_const_incdec_rejected || fail=1
test_const_global_without_initializer_rejected || fail=1
test_const_write_through_pointer_accepted || fail=1
test_const_compiles_and_links || fail=1

exit $fail
