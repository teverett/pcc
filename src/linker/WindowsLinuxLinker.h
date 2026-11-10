#pragma once

#include <string>
#include <vector>

namespace rcc {

enum class HostedTarget { LinuxI386, WindowsI386, MacOSArm64 };

void linkHosted(const std::vector<std::string> &inputs, HostedTarget target, const std::string &outputPath);

}  // namespace rcc
