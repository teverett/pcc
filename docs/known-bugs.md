# Known bugs

Defects that are understood but not yet fixed. Each entry gives the
affected code, the trigger, and the wrong behavior.

## Integer literals at or above 2^63 fail to parse

Affects every target (`src/ast/BuildAst.cpp`).

Literal text is converted with `std::stol`, so a value such as
`18446744065119617025ULL` aborts compilation with a `stol` error
instead of wrapping to its unsigned value. Values built from smaller
constants (`4611686018427387904ULL + 4611686018427387904ULL`) work.

## sparc: wide truthiness test never combines the two words

Affects the `sparc` target (`src/codegen/sparc/SparcCodegen.cpp`).

For a `long long` condition in `if`, `while`, `?:` and the logical
operators, the backend emits `or t0,t0,t1`. SPARC assembly puts the
destination last, so this writes `t1` and leaves `t0` holding only the
low word. A wide value whose low word is zero but whose high word is
not (for example `1LL << 32`) tests false.

