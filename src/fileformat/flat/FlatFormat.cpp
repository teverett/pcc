#include "FlatFormat.h"

namespace rcc {

std::string FlatFormatWriter::write(const ObjectFile &obj) const {
    return std::string(obj.textBytes.begin(), obj.textBytes.end());
}

ObjectFile FlatFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    obj.textBytes.assign(bytes.begin(), bytes.end());
    return obj;
}

}  // namespace rcc
