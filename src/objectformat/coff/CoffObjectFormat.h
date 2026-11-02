#pragma once

#include "objectformat/ObjectFormat.h"

namespace pcc {

// Wraps ../coff/CoffWriter.h/CoffReader.h as an ObjectFormatWriter/
// ObjectFormatReader pair - see those files' own header comments for
// the real COFF byte layout and this project's own documented
// deviations from it (a custom, non-registered Machine value; no
// `.data` section).
class CoffObjectFormatWriter : public ObjectFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class CoffObjectFormatReader : public ObjectFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
