#!/usr/bin/env bash
# Unit tests for struct layout/alignment (src/StructLayout.cpp,
# IRBuilder.cpp's registerStructs()/resolveStructType()) and for the
# features this subset deliberately doesn't support for a struct (see
# docs/language.md's own "Structs" section) - each of those must be a
# loud compile error, not a silent miscompile.
#
# Tests (i386, --format=asm, no simulator needed):
#   test_padded_global_reserves_full_size - a global struct whose
#       members need padding (char then int) must reserve its real,
#       padded byte count in the generated assembly, not just the
#       unpadded sum of its members' own widths - the one place a wrong
#       alignment rule could otherwise look right by accident (see
#       tests/struct_alignment_size.c's own header comment for why a
#       sizeof()-based runtime check alone wouldn't catch this).
#   test_struct_by_value_param_rejected   - a function parameter typed
#       as a plain struct (not a pointer to one) must be a compile
#       error.
#   test_struct_by_value_return_rejected  - ditto for a return type.
#   test_whole_struct_assignment_rejected - `a = b` where both are
#       struct-typed must be a compile error.
#   test_packed_global_reserves_exact_size - a '@packed' global struct
#       must reserve exactly the unpadded sum of its members' widths,
#       not the naturally-aligned (padded) size - the packed mirror of
#       test_padded_global_reserves_full_size above.
#   test_packed_misaligned_member_rejected - a '@packed' struct whose
#       layout would put a multi-byte member at a misaligned byte
#       offset must be a compile error, not a silent miscompile - some
#       backends can't actually perform that access (see
#       IRBuilder.cpp's checkPackedAlignmentSafety()).
#
# Usage: tests/run_struct_layout_tests.sh [path/to/rcc]

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

test_padded_global_reserves_full_size() {
    local dir="$tmpdir/padded"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Mixed { char a; int b; };
struct Mixed g;
int main() {
    g.a = 1;
    g.b = 2;
    return g.a + g.b;
}
EOF
    if ! "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_padded_global_reserves_full_size: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local line reserved
    line="$(grep -E '^g_g:' "$dir/t.s")"
    # "db 0, 0, ..." - one zero per reserved byte; count the commas plus one.
    reserved="$(echo "$line" | grep -o ',' | wc -l | tr -d ' ')"
    reserved=$((reserved + 1))
    if [ "$reserved" -ne 8 ]; then
        echo "FAIL test_padded_global_reserves_full_size: expected 8 reserved bytes (1 for 'a', 3" \
             "padding, 4 for 'b'), got $reserved - line was: $line"
        return 1
    fi
    echo "PASS test_padded_global_reserves_full_size (reserved $reserved bytes)"
    return 0
}

test_packed_global_reserves_exact_size() {
    local dir="$tmpdir/packed"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Mixed @packed { int b; char a; char c; };
struct Mixed g;
int main() {
    g.b = 1;
    g.a = 2;
    g.c = 3;
    return g.a + g.b + g.c;
}
EOF
    if ! "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_packed_global_reserves_exact_size: fixture failed to compile"
        cat "$dir/log"
        return 1
    fi
    local line reserved
    line="$(grep -E '^g_g:' "$dir/t.s")"
    reserved="$(echo "$line" | grep -o ',' | wc -l | tr -d ' ')"
    reserved=$((reserved + 1))
    if [ "$reserved" -ne 6 ]; then
        echo "FAIL test_packed_global_reserves_exact_size: expected 6 reserved bytes (4 for 'b', 1" \
             "for 'a', 1 for 'c', no padding), got $reserved - line was: $line"
        return 1
    fi
    echo "PASS test_packed_global_reserves_exact_size (reserved $reserved bytes)"
    return 0
}

test_packed_misaligned_member_rejected() {
    local dir="$tmpdir/packedmisaligned"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Mixed @packed { char a; int b; };
int main() {
    struct Mixed m;
    m.a = 1;
    m.b = 2;
    return m.a + m.b;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_packed_misaligned_member_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "misaligned\|packed" "$dir/log"; then
        echo "FAIL test_packed_misaligned_member_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_packed_misaligned_member_rejected"
    return 0
}

test_struct_by_value_param_rejected() {
    local dir="$tmpdir/param"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Point { int x; int y; };
int sum(struct Point p) {
    return p.x + p.y;
}
int main() {
    struct Point a;
    a.x = 1;
    a.y = 2;
    return sum(a);
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_struct_by_value_param_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "struct by value" "$dir/log"; then
        echo "FAIL test_struct_by_value_param_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_struct_by_value_param_rejected"
    return 0
}

test_struct_by_value_return_rejected() {
    local dir="$tmpdir/return"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Point { int x; int y; };
struct Point make() {
    struct Point p;
    p.x = 1;
    p.y = 2;
    return p;
}
int main() {
    struct Point a;
    a = make();
    return a.x;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_struct_by_value_return_rejected: expected a compile error, but it compiled"
        return 1
    fi
    if ! grep -qi "struct by value" "$dir/log"; then
        echo "FAIL test_struct_by_value_return_rejected: compile failed, but not with the expected message:"
        cat "$dir/log"
        return 1
    fi
    echo "PASS test_struct_by_value_return_rejected"
    return 0
}

test_whole_struct_assignment_rejected() {
    local dir="$tmpdir/assign"
    mkdir -p "$dir"
    cat >"$dir/t.c" <<'EOF'
struct Point { int x; int y; };
int main() {
    struct Point a;
    struct Point b;
    a.x = 1;
    a.y = 2;
    b = a;
    return b.x + b.y;
}
EOF
    if "$RCC" --target=i386 --format=asm --out="$dir/t.s" "$dir/t.c" >"$dir/log" 2>&1; then
        echo "FAIL test_whole_struct_assignment_rejected: expected a compile error, but it compiled"
        return 1
    fi
    echo "PASS test_whole_struct_assignment_rejected"
    return 0
}

test_padded_global_reserves_full_size || fail=1
test_packed_global_reserves_exact_size || fail=1
test_packed_misaligned_member_rejected || fail=1
test_struct_by_value_param_rejected || fail=1
test_struct_by_value_return_rejected || fail=1
test_whole_struct_assignment_rejected || fail=1

exit $fail
