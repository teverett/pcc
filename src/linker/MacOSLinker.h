#pragma once

#include <string>
#include <vector>

namespace rcc {

void nativeMacOSLink(const std::vector<std::string> &inputs, const std::string &outputPath);

}  // namespace rcc
