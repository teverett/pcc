#include "RawFormat.h"

namespace rcc {

std::string RawFormatWriter::write(const ObjectFile &obj) const {
    return std::string(obj.textBytes.begin(), obj.textBytes.end());
}

ObjectFile RawFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    obj.textBytes.assign(bytes.begin(), bytes.end());
    return obj;
}

}  // namespace rcc
