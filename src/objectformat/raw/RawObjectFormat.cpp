#include "RawObjectFormat.h"

namespace pcc {

std::string RawObjectFormatWriter::write(const ObjectFile &obj) const {
    return std::string(obj.textBytes.begin(), obj.textBytes.end());
}

ObjectFile RawObjectFormatReader::read(const std::string &bytes) const {
    ObjectFile obj;
    obj.textBytes.assign(bytes.begin(), bytes.end());
    return obj;
}

}  // namespace pcc
