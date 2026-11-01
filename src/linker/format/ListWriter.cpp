#include "ListWriter.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pcc {

std::string ListWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 18) {
        throw std::runtime_error("ListWriter: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    std::ostringstream out;
    out << "\nLabels:\n";
    std::vector<std::string> names;
    for (const auto &kv : image.symbols) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    char buf[64];
    for (const auto &name : names) {
        std::snprintf(buf, sizeof(buf), "%-8.8s %#06o", name.c_str(), image.symbols.at(name) & 0777777);
        out << buf << "\n";
    }
    return out.str();
}

}  // namespace pcc
