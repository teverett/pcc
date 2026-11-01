#include "A7outWriter.h"

#include <cstdio>
#include <sstream>
#include <stdexcept>

namespace pcc {

std::string A7outWriter::write(const LinkedImage &image) const {
    if (image.unitBits != 18) {
        throw std::runtime_error("A7outWriter: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(image.unitBits));
    }
    std::ostringstream out;
    char buf[32];
    for (size_t i = 0; i < image.words.size(); i++) {
        std::snprintf(buf, sizeof(buf), "%06o: %06o\n", static_cast<unsigned>(image.base + i), image.words[i]);
        out << buf;
    }
    return out.str();
}

}  // namespace pcc
