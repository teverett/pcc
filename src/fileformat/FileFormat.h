#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ObjectFile.h"

namespace rcc {

// Serializes a format-neutral ObjectFile (ObjectFile.h) to one real on-disk byte layout. One
// implementation per format, in its own subdirectory under fileformat/ (see FileFormatKind). The
// same Writer serves both a not-yet-linked ObjectFile and a linked one.
class FileFormatWriter {
   public:
    virtual ~FileFormatWriter() = default;

    // Throws std::runtime_error if `obj` is unwritable in this format (wrong `unitBits`, program
    // too big for a fixed-size container).
    virtual std::string write(const ObjectFile &obj) const = 0;
};

// Parses this format's byte layout back into an ObjectFile. `list`'s read() always throws: a
// human-readable dump has no defined inverse (see list/ListFormat.h).
class FileFormatReader {
   public:
    virtual ~FileFormatReader() = default;
    virtual ObjectFile read(const std::string &bytes) const = 0;
};

// `Raw` answers to two `--format` names: `raw` when compiling a whole program or linking (same,
// on an already-resolved ObjectFile), and `objraw` when compiling (drops the symbol table,
// inspection-only) — one implementation, two names.
// `Rim`/`Ptr`/`A7out`/`List`/`Dos33`/`ProDos`/`Cmd`/`ColecoRom`/`Decb`/`Prg`/`AtariExe`/
// `Atari2600Rom` are link-output-only conventions, never a per-file compile format.
enum class FileFormatKind { Coff, Elf, MachO, Aout, Raw, Rim, Ptr, A7out, List, Dos33, ProDos, Cmd, ColecoRom, Decb, Prg, AtariExe, Atari2600Rom };

std::unique_ptr<FileFormatWriter> makeFileFormatWriter(FileFormatKind kind);
std::unique_ptr<FileFormatReader> makeFileFormatReader(FileFormatKind kind);

// Maps a `--format`/`--link --format` string to its FileFormatKind, or nullopt if none matches.
std::optional<FileFormatKind> fileFormatKindFromName(const std::string &name);

// Canonical spelling of a format name; `Raw`'s three names all canonicalize to "raw".
std::string fileFormatName(FileFormatKind kind);

// Sniffs a file's leading bytes to identify its format, for `--link` merging inputs compiled with
// different `--format`s. Only returns Coff/Elf/MachO/Aout/Raw (link-output-only formats,
// including Dos33/ProDos/Cmd/ColecoRom, are never `--link` input); `Raw` has no magic number, so
// it's the last-resort fallback.
FileFormatKind detectFileFormat(const std::string &bytes);

}  // namespace rcc
