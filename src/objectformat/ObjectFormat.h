#pragma once

#include <memory>
#include <optional>
#include <string>

#include "ObjectFile.h"

namespace pcc {

// Serializes a format-neutral ObjectFile (see ObjectFile.h) to one
// real on-disk object-file byte layout - the write side of the
// "codegen backends never touch a real object-file format directly"
// boundary. One implementation per format, each in its own file
// under this same directory: CoffObjectFormat.h (wraps
// ../coff/CoffWriter.h), ElfObjectFormat.h, MachOObjectFormat.h,
// AoutObjectFormat.h, RawObjectFormat.h. A backend's own assembler
// builds one ObjectFile and hands it to whichever of these
// makeObjectFormatWriter() below picked for the requested
// `--format`; it never constructs a byte layout itself.
class ObjectFormatWriter {
   public:
    virtual ~ObjectFormatWriter() = default;
    virtual std::string write(const ObjectFile &obj) const = 0;
};

// The read side - parses this format's own real byte layout back
// into the same format-neutral ObjectFile, for a TargetLinker's own
// merge() (see ../linker/Linker.h) to consume regardless of which
// format any given input .o file actually is.
class ObjectFormatReader {
   public:
    virtual ~ObjectFormatReader() = default;
    virtual ObjectFile read(const std::string &bytes) const = 0;
};

// The five object-file formats this project can read and write -
// "name" (see objectFormatKindFromName()) is exactly the --format
// value a compile or --link invocation selects.
enum class ObjectFormatKind { Coff, Elf, MachO, Aout, Raw };

std::unique_ptr<ObjectFormatWriter> makeObjectFormatWriter(ObjectFormatKind kind);
std::unique_ptr<ObjectFormatReader> makeObjectFormatReader(ObjectFormatKind kind);

// Maps a --format string ("coff"/"elf"/"macho"/"aout"/"objraw") to
// its ObjectFormatKind, or nullopt if it isn't one of these five.
// ("objraw", not "raw": a backend's existing whole-program
// `--format=raw` - see Codegen.h's own header comment - is a
// completely different, self-contained-program concept from an
// `ObjectFile`-shaped relocatable object with no symbol table, even
// though both happen to be "just bytes" - see RawObjectFormat.h's
// own header comment for why they're kept namespace-distinct rather
// than overloading one --format name with two meanings.)
std::optional<ObjectFormatKind> objectFormatKindFromName(const std::string &name);

// The reverse of the above, for an error message/`assembledFormats()`
// list that wants this format's own canonical --format spelling.
std::string objectFormatName(ObjectFormatKind kind);

// Sniffs a .o file's own leading bytes to tell which of these five
// formats it actually is - used by `--link` (see main.cpp) so a
// linker can merge object files that were each compiled with a
// different `--format`, without the user having to say which is
// which. Real COFF/ELF/Mach-O/a.out magic numbers make the first
// four unambiguous; "objraw" (no magic at all, by definition) is
// only ever the fallback once none of the other four match.
ObjectFormatKind detectObjectFormat(const std::string &bytes);

}  // namespace pcc
