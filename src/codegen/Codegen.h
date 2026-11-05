#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../Type.h"
#include "ir/IR.h"

namespace pcc {

// A codegen backend: turns the IR (shared by every backend - see
// IR.h/IRBuilder.h, neither of which knows or cares which target is
// in play) into that target's assembly text, and knows how to drive
// that target's own real assembler to turn the text into a binary.
// Each backend lives in its own subdirectory under codegen/ and is
// otherwise independent: its own instruction selection, its own
// register/temp conventions, its own notion of how a program starts
// and returns a result, its own assembler tool and command line. By
// the time a backend sees the IR, every name is already resolved to
// a Slot/Function and every semantic check has already run (see
// IR.h's header comment) - a backend only ever does instruction
// selection, never symbol-table work.
//
// The *language* (see ../README.md) is deliberately the same no
// matter the target - no recursion, one scalar type, arrays,
// pointers, the same control-flow and operators - so a given .c file
// means the same thing regardless of --target. What differs is only
// how that meaning gets expressed in each target's instruction set.
class Backend {
   public:
    virtual ~Backend() = default;

    // Compiles the whole program to this target's assembly text.
    // `objectMode` is real separate compilation's own switch
    // (`--format=coff` from a single `.c` file - see IRBuilder.h's
    // `allowExternal`, which is what makes `prog` possible at all
    // here): when true, no entry sequence is emitted at all - not
    // even by the one file that happens to define `main` - since an
    // object file is never directly runnable; the linker is what
    // eventually synthesizes exactly one combined entry sequence
    // itself, once every object is merged and `main`'s own linked
    // address is resolvable (see Linker.h). When false (the default,
    // every other format), behavior is unchanged: the file defining
    // `main` (guaranteed to exist - see IRBuilder.h) gets the usual
    // inline entry sequence, same as always.
    virtual std::string generate(const ir::Program &prog, bool objectMode = false) = 0;

    // The short name this backend is selected with (--target=<name>).
    virtual std::string name() const = 0;

    // This target's own storage unit for each of char/short/int/long/
    // long long (see Type.h) - bytes on every byte-addressable
    // backend, this target's own native word on one that isn't
    // (pdp7). Every backend documents its own int width in its own
    // *Codegen.h header comment already; this is that same fact, plus
    // the other four widths, in one place IRBuilder/IRPrinter can read
    // without knowing which backend is selected.
    virtual TypeWidths typeWidths() const = 0;

    // File extension to give a temp file holding this backend's
    // generated text, before handing it to the real assembler (e.g.
    // ".s" for pdp7as, ".asm" for nasm) - some assemblers key off it.
    virtual std::string sourceExtension() const = 0;

    // Valid --format values other than "asm" (which every backend
    // supports uniformly: just print/save the generated text, no
    // real assembler involved) - e.g. pdp7's {"a7out","list","ptr","rim"}.
    virtual std::vector<std::string> assembledFormats() const = 0;

    // Whether `format` means real separate compilation for this
    // backend specifically (`objectMode`/IRBuilder.h's
    // `allowExternal` - see generate()'s own comment) - main.cpp asks
    // this instead of hardcoding a format name, since the same name
    // can mean genuinely different things to different backends:
    // `--format=elf` is a *complete* self-contained whole program for
    // `8086` (I8086Codegen.h's own peer of `raw`/`bin`) but a real
    // relocatable object file (see
    // ../fileformat/FileFormat.h) for `z80` (Z80Codegen.h's own
    // peer of `coff`). The default - only "coff" - matches every
    // backend that predates the format-neutral object model (pdp7,
    // 8086); a backend that uses more than one object-file format
    // (z80's coff/elf/macho/aout/objraw) overrides this.
    virtual bool isObjectFormat(const std::string &format) const { return format == "coff"; }

    // Where to look for the real assembler if --assembler wasn't
    // given - a relative path (resolved from the current directory)
    // or a bare command name (resolved via PATH).
    virtual std::string defaultAssemblerPath() const = 0;

    // The full shell command to assemble `srcPath` (this backend's
    // generated text, already on disk) into `outPath` using `format`
    // (one of assembledFormats()) and `assemblerPath`. Every
    // argument is already shell-quoted by the caller except
    // `assemblerPath` and `format`, which the implementation quotes
    // itself if needed.
    virtual std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                              const std::string &srcPath, const std::string &outPath) const = 0;

    // Only meaningful after generate() has been called on this same
    // instance: returns that program's generated text split one
    // entry per *input* source file (keyed the same way Slot::
    // sourceFile/Function::sourceFile are - see IR.h) instead of the
    // single combined text generate() itself returns. Used by
    // --dump-asm so each input `.c` file gets its own dump, instead
    // of one dump combining every file given on pcc's command line.
    // Shared, not-tied-to-any-one-function text (the entry sequence,
    // this backend's own internal constants like a divide routine or
    // literal cache) is attributed to whichever file defines `main`,
    // the same "where the program starts" home used for the entry
    // sequence itself - these dumps are purely for a human to read,
    // never reassembled, so that's a reasonable home for it even
    // though it isn't really "owned" by any one input file.
    virtual std::map<std::string, std::string> perFileAsm() const = 0;

    // Assembles `asmText` (this same backend's own generate() output)
    // into `format` (one of assembledFormats()) entirely in-process,
    // with no external assembler involved - returns the real output
    // bytes/text, or `std::nullopt` if this backend doesn't (yet)
    // support in-process assembly for `format` at all, in which case
    // the caller falls back to `buildAssembleCommand()` + a real
    // external-assembler subprocess exactly as before. The default
    // implementation always returns `nullopt` (i.e. "no in-process
    // assembler, use the external one") - a backend overrides this
    // once it has its own from-scratch assembler; until then nothing
    // about its existing external-assembler behavior changes. See
    // pdp7/Pdp7Assembler.h for the first one.
    virtual std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
        (void)asmText;
        (void)format;
        return std::nullopt;
    }
};

}  // namespace pcc
