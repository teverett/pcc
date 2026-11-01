#pragma once

#include <map>
#include <string>

namespace pcc {

// Runs pcc's own small text-level preprocessor over one top-level
// source file - and everything it `#include`s, recursively - before
// ANTLR ever sees it; see Preprocessor.cpp's own header comment for
// what's supported and the deliberate limitations. `predefined`
// seeds this file's own macro table (main.cpp uses it for the
// target macros, `__PDP7__`/`__8086__`); each top-level file given
// to `pcc` gets its own fresh copy, same as a real C translation
// unit, but macros persist across that file's own `#include` tree.
std::string preprocess(const std::string &path, const std::map<std::string, std::string> &predefined);

}  // namespace pcc
