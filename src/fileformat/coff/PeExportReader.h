#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

struct PeExport {
    std::string name;
    uint16_t ordinal;
    bool isData;
};

std::vector<PeExport> readPeExports(const std::string &bytes);

}
