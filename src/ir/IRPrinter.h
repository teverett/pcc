#pragma once

#include <string>

#include "IR.h"

namespace pcc {

// A human-readable dump of the IR (see IR.h) - slots, functions, and
// each function's body as indented pseudo-code with every name
// shown as "sourceName#id" (the id disambiguates locals that shadow
// across scopes, or share a name with a global). For --dump-ir; not
// meant to be parsed back in anywhere.
//
// `sourceFile`, if non-empty, restricts the dump to just the globals
// and functions whose own `Slot::sourceFile`/`Function::sourceFile`
// matches it - used to give each input `.c` file its own `--dump-ir`
// output instead of one dump combining every file given on the
// command line (see main.cpp). A referenced name from another file
// (e.g. a cross-file call) still prints normally - only *which
// top-level declarations appear at all* is filtered, not how a
// reference to one is rendered.
std::string printIR(const ir::Program &prog, const std::string &sourceFile = "");

}  // namespace pcc
