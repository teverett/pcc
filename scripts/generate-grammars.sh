#!/usr/bin/env bash
# Regenerates src/generated/ (checked into git) from grammar/*.g4 with
# the real ANTLR4 tool. pcc's own build (the Makefile) no longer runs
# ANTLR at all - it just compiles whatever's already in src/generated/
# - so the only time this script needs to run is right after editing
# a .g4 file, by hand, as its own separate step: regenerate, review
# the diff under src/generated/, then commit both together.
#
# This also means the real ANTLR4 *tool* (Java + the jar) is a
# contributor-machine-only dependency now, not a build or CI one -
# only the ANTLR4 C++ *runtime* (headers/lib the generated code
# itself calls into) still needs to be available wherever pcc is
# actually built.
#
# Usage:
#   scripts/generate-grammars.sh                 # uses `antlr` on PATH
#   ANTLR="java -jar /path/to/antlr-4.13.2-complete.jar" \
#       scripts/generate-grammars.sh             # or an explicit jar

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

ANTLR="${ANTLR:-antlr}"
GRAMMAR_DIR="$ROOT_DIR/grammar"
GEN_DIR="$ROOT_DIR/src/generated"

rm -rf "$GEN_DIR"
mkdir -p "$GEN_DIR"

# Same three invocations, and the same lexer-before-parser-in-one-
# invocation reason, as the Makefile's own (now-removed) grammar-
# generation rules used to document: a split lexer/parser pair's
# parser needs its lexer's own .tokens file, which only exists once
# that lexer has been generated - one invocation naming both files
# (lexer first) generates it as a side effect before the parser half
# of the same invocation needs it.
$ANTLR -Dlanguage=Cpp -visitor -no-listener -o "$GEN_DIR" \
    "$GRAMMAR_DIR/CLexer.g4" "$GRAMMAR_DIR/CParser.g4"

$ANTLR -Dlanguage=Cpp -visitor -no-listener -o "$GEN_DIR" \
    "$GRAMMAR_DIR/PccAsm.g4"

$ANTLR -Dlanguage=Cpp -visitor -no-listener -o "$GEN_DIR" \
    "$GRAMMAR_DIR/PccIRLexer.g4" "$GRAMMAR_DIR/PccIRParser.g4"

echo "Regenerated $GEN_DIR - review the diff and commit it along with the .g4 change." >&2
