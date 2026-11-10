#pragma once

#include <map>
#include <string>

namespace rcc {

// Runs the text-level preprocessor over `path` and everything it recursively
// `#include`s, before ANTLR sees it. `predefined` seeds the macro table
// (main.cpp passes target macros like __PDP7__/__8086__); each top-level
// file gets a fresh copy, but macros persist across its own include tree.
std::string preprocess(const std::string &path, const std::map<std::string, std::string> &predefined);

}  // namespace rcc
