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

// `Flat` answers to two `--format` names: `objraw` when compiling (drops the symbol table) and
// `flat` when linking (same, on an already-resolved ObjectFile) — one implementation, two names.
// `Raw`/`Rim`/`Ptr`/`A7out`/`List` are link-output-only conventions, never a per-file compile format.
enum class FileFormatKind { Coff, Elf, MachO, Aout, Flat, Raw, Rim, Ptr, A7out, List };

std::unique_ptr<FileFormatWriter> makeFileFormatWriter(FileFormatKind kind);
std::unique_ptr<FileFormatReader> makeFileFormatReader(FileFormatKind kind);

// Maps a `--format`/`--link --format` string to its FileFormatKind, or nullopt if none matches.
std::optional<FileFormatKind> fileFormatKindFromName(const std::string &name);

// Canonical spelling of a format name; `Flat`'s two names both canonicalize to "flat".
std::string fileFormatName(FileFormatKind kind);

// Sniffs a file's leading bytes to identify its format, for `--link` merging inputs compiled with
// different `--format`s. Only returns Coff/Elf/MachO/Aout/Flat (link-output-only formats are never
// `--link` input); `Flat` has no magic number, so it's the last-resort fallback.
FileFormatKind detectFileFormat(const std::string &bytes);

}  // namespace rcc
