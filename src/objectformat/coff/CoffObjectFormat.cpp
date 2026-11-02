#include "CoffObjectFormat.h"

#include "coff/CoffReader.h"
#include "coff/CoffWriter.h"

namespace pcc {

std::string CoffObjectFormatWriter::write(const ObjectFile &obj) const {
    return writeCoffObject(obj.machineType, obj.textBytes, obj.symbols, obj.relocations, obj.dataSize);
}

ObjectFile CoffObjectFormatReader::read(const std::string &bytes) const { return readCoffObject(bytes); }

}  // namespace pcc
