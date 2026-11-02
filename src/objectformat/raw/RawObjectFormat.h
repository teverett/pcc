#pragma once

#include "objectformat/ObjectFormat.h"

namespace pcc {

// `--format=objraw` - the degenerate object format: writes exactly
// `obj.textBytes`, nothing else at all - no symbol table, no
// relocation table, no header/magic of any kind. This is a real
// limitation, not an oversight: an object file with no symbol table
// can define nothing by name for another object to call/reference,
// and one with no relocation table can contain no unresolved
// external reference either. In practice that means an `objraw`
// object can't actually be `--link`ed through this project's own
// linker at all (confirmed directly, not just reasoned about): every
// target here synthesizes an entry sequence that calls `main` *by
// name* (see e.g. ../../linker/GenericLinker.h's own header comment
// on the entry object), and an `objraw` object - even as the only
// user object given - has thrown its own `fn_main` symbol away, so
// that call can never resolve. This format exists for the cases
// `--link` isn't involved at all: inspecting/diffing the exact bytes
// an assembler produced, or handing them to something outside this
// project that loads a flat blob at a known address itself. Any
// symbols/relocations a backend's assembler built for this
// ObjectFile are silently dropped on write (not an error - the same
// "lossy by design" stance ../../linker/format/RawWriter.h's own
// LinkedImage-side counterpart takes).
//
// `read()` has no header to parse at all - it just wraps the whole
// input back into `textBytes` with an empty symbol/relocation table
// and `machineType=0` (unknowable from the bytes themselves; a linker
// that cares must track which --target it asked for separately).
// This is also why detectObjectFormat() (see ObjectFormat.h) only
// ever returns this as the last-resort fallback: a raw object has no
// magic number to recognize it by, so anything that *could* be
// mistaken for one of the other four real formats always is first.
class RawObjectFormatWriter : public ObjectFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class RawObjectFormatReader : public ObjectFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
