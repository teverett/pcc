#include "ListFormat.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pcc {

std::string ListFormatWriter::write(const ObjectFile &obj) const {
    if (obj.unitBits != 18) {
        throw std::runtime_error("ListFormat: expected an 18-bit-word image, got unitBits=" +
                                  std::to_string(obj.unitBits));
    }
    std::ostringstream out;
    out << "\nLabels:\n";
    std::vector<const ObjSymbol *> syms;
    for (const auto &sym : obj.symbols) syms.push_back(&sym);
    std::sort(syms.begin(), syms.end(), [](const ObjSymbol *a, const ObjSymbol *b) { return a->name < b->name; });
    char buf[64];
    for (const ObjSymbol *sym : syms) {
        // Code symbol values are relative to .text start; data symbol values are already absolute.
        uint32_t addr = sym->isData ? sym->value : obj.base + sym->value;
        std::snprintf(buf, sizeof(buf), "%-8.8s %#06o", sym->name.c_str(), addr & 0777777);
        out << buf << "\n";
    }
    return out.str();
}

ObjectFile ListFormatReader::read(const std::string &) const {
    throw std::runtime_error(
        "ListFormat: a symbol-table listing has no program text to recover - this format never round-trips (see "
        "ListFormat.h's own header comment)");
}

}  // namespace pcc
