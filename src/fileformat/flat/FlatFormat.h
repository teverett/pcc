#pragma once

#include "fileformat/FileFormat.h"

namespace pcc {

// `--format=objraw` (compile) / `--link --format=flat` (link) - the
// degenerate file format: writes exactly `obj.textBytes` as-is,
// nothing else at all - no symbol table, no relocation table, no
// header/magic of any kind. This is a real limitation, not an
// oversight: a file with no symbol table can define nothing by name
// for another object to call/reference, and one with no relocation
// table can contain no unresolved external reference either. In
// practice that means an `objraw` object can't actually be `--link`ed
// through this project's own linker at all (confirmed directly, not
// just reasoned about): every target here synthesizes an entry
// sequence that calls `main` *by name* (see e.g.
// ../../linker/GenericLinker.h's own header comment on the entry
// object), and an `objraw` object - even as the only user object
// given - has thrown its own `fn_main` symbol away, so that call can
// never resolve. This format exists for the cases `--link` isn't
// involved at all: inspecting/diffing the exact bytes an assembler
// produced, or handing them to something outside this project that
// loads a flat blob at a known address itself; and, as a *link*
// output (`flat`), for a target whose own simulator/hardware just
// loads bytes at a fixed address directly (e.g. z80's own
// check_z80()), with no fixed container size or signature the way
// `raw/RawFormat.h`'s own 8086 boot-sector convention needs.
//
// Both roles are exactly the same operation - `obj.textBytes` is
// already this target's own real, final packed-byte image either way
// (see ObjectFile.h's own `unitBits`/`base` comment: a word-addressed
// target's own `.textBytes` is already packed multiple bytes per
// word, pre-link or post-link alike, so there's no unpacking left for
// this format to do) - so one implementation answers to both
// `--format` names (see FileFormatKind's own comment).
//
// `read()` has no header to parse at all - it just wraps the whole
// input back into `textBytes` with an empty symbol/relocation table.
// This is also why detectFileFormat() (see ../FileFormat.h) only
// ever returns this as the last-resort fallback: a flat file has no
// magic number to recognize it by, so anything that *could* be
// mistaken for one of the other four real formats always is first.
class FlatFormatWriter : public FileFormatWriter {
   public:
    std::string write(const ObjectFile &obj) const override;
};

class FlatFormatReader : public FileFormatReader {
   public:
    ObjectFile read(const std::string &bytes) const override;
};

}  // namespace pcc
