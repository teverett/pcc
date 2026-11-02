#include "A7outFormat.h"

#include <cstdio>
#include <sstream>
#include <stdexcept>

#include "fileformat/Pdp7TapePacking.h"

namespace pcc {

std::string A7outFormatWriter::write(const ObjectFile &obj) const {
    if (obj.unitBits != 18) {
        throw std::runtime_error("A7outFormat: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(obj.unitBits));
    }
    std::vector<uint32_t> words = wordsFromPdp7TextBytes(obj.textBytes);
    std::ostringstream out;
    char buf[32];
    for (size_t i = 0; i < words.size(); i++) {
        std::snprintf(buf, sizeof(buf), "%06o: %06o\n", static_cast<unsigned>(obj.base + i), words[i]);
        out << buf;
    }
    return out.str();
}

ObjectFile A7outFormatReader::read(const std::string &bytes) const {
    std::istringstream in(bytes);
    std::string line;
    std::vector<uint32_t> words;
    uint32_t base = 0;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        unsigned addr = 0, word = 0;
        if (std::sscanf(line.c_str(), "%o: %o", &addr, &word) != 2) {
            throw std::runtime_error("A7outFormat: malformed line '" + line + "'");
        }
        if (first) {
            base = addr;
            first = false;
        } else if (addr != base + words.size()) {
            throw std::runtime_error("A7outFormat: non-sequential address at '" + line + "'");
        }
        words.push_back(word);
    }
    ObjectFile obj;
    obj.textBytes = pdp7TextBytesFromWords(words);
    obj.unitBits = 18;
    obj.base = base;
    return obj;
}

}  // namespace pcc
