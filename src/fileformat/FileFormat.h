#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ObjectFile.h"

namespace pcc {

// Serializes a format-neutral ObjectFile (see ObjectFile.h) to one
// real on-disk byte layout - the write side of the "codegen backends
// and linkers never touch a real file format directly" boundary. One
// implementation per format, each in its own subdirectory under
// fileformat/: coff/, elf/, macho/, aout/, flat/, raw/, rim/, ptr/,
// a7out/, list/ (see FileFormatKind below). A backend's own assembler
// builds one ObjectFile per compile and a TargetLinker's own merge()
// (see ../linker/Linker.h) builds one per link; both hand it to
// whichever of these makeFileFormatWriter() picked for the requested
// `--format`/`--link --format` - neither ever constructs a byte
// layout directly. The same Writer serves both stages: a not-yet-
// linked ObjectFile still has relocations/undefined symbols, a linked
// one has none (see ObjectFile.h's own header comment) - every writer
// here just serializes whatever it's actually given.
class FileFormatWriter {
   public:
    virtual ~FileFormatWriter() = default;

    // Throws std::runtime_error on anything that makes `obj`
    // unwritable in this format (wrong `unitBits`, a program too big
    // for a fixed-size container, an unresolvable entry symbol - that
    // last one's actually caught earlier, by TargetLinker::merge()
    // itself, since every format needs it).
    virtual std::string write(const ObjectFile &obj) const = 0;
};

// The read side - parses this format's own real byte layout back
// into the same format-neutral ObjectFile, for a TargetLinker's own
// merge() (see ../linker/Linker.h) to consume regardless of which
// format any given input file actually is, or simply to inspect/
// round-trip a file this project itself wrote. Not every format has
// a reader that means anything: `list` is a human-readable
// disassembly/symbol-table dump with no defined way to parse back
// into structured data, so its own `read()` always throws (see
// list/ListFormat.h's own header comment) - every other format's own
// `read()` is the real, meaningful inverse of its own `write()`.
class FileFormatReader {
   public:
    virtual ~FileFormatReader() = default;
    virtual ObjectFile read(const std::string &bytes) const = 0;
};

// Every file format this project can read and/or write. "name" (see
// fileFormatKindFromName()) is exactly the `--format`/`--link
// --format` value a compile or link invocation selects - except that
// `Flat` answers to two different names depending on which direction
// it's used from: `objraw` when compiling a single file (an
// ObjectFile with no symbol table at all - see flat/FlatFormat.h's
// own header comment on why dropping it is this format's whole
// point) and `flat` when linking (the same drop-everything-but-
// `.text` idea, applied to an already-fully-resolved ObjectFile
// instead) - both names share the one FlatFormat implementation.
// `Raw` (pdp7/8086's own boot-sector convention) and `Rim`/`Ptr`/
// `A7out`/`List` (pdp7's own paper-tape/listing formats) only ever
// make sense as a *link* output, never as a per-file `--format` to
// compile with (a single, not-yet-linked object has no sensible
// "boot sector" or "paper tape" of its own - those conventions are
// about a final, placed, resolved program).
enum class FileFormatKind { Coff, Elf, MachO, Aout, Flat, Raw, Rim, Ptr, A7out, List };

std::unique_ptr<FileFormatWriter> makeFileFormatWriter(FileFormatKind kind);
std::unique_ptr<FileFormatReader> makeFileFormatReader(FileFormatKind kind);

// Maps a `--format`/`--link --format` string to its FileFormatKind,
// or nullopt if it isn't a name any fileformat/ implementation
// answers to (e.g. "asm", or a backend's own in-process whole-program
// "raw" compile format - see Codegen.h's own header comment: neither
// of those ever touches an ObjectFile or a file-format Reader/Writer
// at all, so neither is a FileFormatKind).
std::optional<FileFormatKind> fileFormatKindFromName(const std::string &name);

// The reverse of the above, for an error message/format-list that
// wants this format's own canonical spelling - `Flat`'s own two names
// (see FileFormatKind's own comment) canonicalize to "flat".
std::string fileFormatName(FileFormatKind kind);

// Sniffs a file's own leading bytes to tell which format it actually
// is - used by `--link` (see main.cpp) so a linker can merge object
// files that were each compiled with a different `--format`, without
// the user having to say which is which. Only ever returns one of
// the five formats an input object could actually have been compiled
// as (Coff/Elf/MachO/Aout/Flat) - Raw/Rim/Ptr/A7out/List are pure link
// *output* conventions, never a `--link` input. Real COFF/ELF/Mach-O/
// a.out magic numbers make the first four unambiguous; `Flat` (no
// magic at all, by definition) is only ever the fallback once none of
// the other four match.
FileFormatKind detectFileFormat(const std::string &bytes);

}  // namespace pcc
