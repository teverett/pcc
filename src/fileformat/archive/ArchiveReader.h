#pragma once

#include <string>
#include <vector>

namespace rcc {

struct ArchiveMember {
    std::string name;
    std::string bytes;
};

std::vector<ArchiveMember> readArchive(const std::string &bytes);

}  // namespace rcc
