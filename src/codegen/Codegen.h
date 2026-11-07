#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "../Type.h"
#include "ir/IR.h"

namespace pcc {

// Turns target-independent IR (names already resolved, semantics already checked by
// IRBuilder) into a target's assembly text, and drives that target's assembler to produce
// a binary. A backend only ever does instruction selection, never symbol-table work.
class Backend {
   public:
    virtual ~Backend() = default;

    // Compiles the whole program to this target's assembly text. When `objectMode` is true
    // (real separate compilation), no entry sequence is emitted; the linker synthesizes one
    // combined entry sequence once every object is merged and `main` is resolvable.
    virtual std::string generate(const ir::Program &prog, bool objectMode = false) = 0;

    // Backend name, selected via --target=<name>.
    virtual std::string name() const = 0;

    // This target's storage width for char/short/int/long/long long; usually bytes, but a
    // target's native word on a non-byte-addressable backend (e.g. pdp7).
    virtual TypeWidths typeWidths() const = 0;

    // Extension for the temp file holding generated text before assembling; some external
    // assemblers key off it.
    virtual std::string sourceExtension() const = 0;

    // Valid --format values other than "asm" (handled uniformly: print/save the text as-is).
    virtual std::vector<std::string> assembledFormats() const = 0;

    // Whether `format` means real separate compilation (object mode) for this backend; the
    // same format name can mean different things per backend (e.g. elf is a complete
    // program for 8086 but a relocatable object for z80). Default: only "coff".
    virtual bool isObjectFormat(const std::string &format) const { return format == "coff"; }

    // Where to find the real assembler if --assembler wasn't given: a relative path or a
    // PATH-resolved command name.
    virtual std::string defaultAssemblerPath() const = 0;

    // Shell command to assemble `srcPath` into `outPath` using `format` and `assemblerPath`.
    virtual std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                              const std::string &srcPath, const std::string &outPath) const = 0;

    // Valid only after generate() was called on this instance: splits the combined generated
    // text per input source file (for --dump-asm). Text not tied to one input file (entry
    // sequence, internal helper routines) is attributed to the file defining `main`.
    virtual std::map<std::string, std::string> perFileAsm() const = 0;

    // Assembles `asmText` for `format` entirely in-process, or returns nullopt if this
    // backend doesn't support that; the caller then falls back to buildAssembleCommand()
    // plus an external assembler subprocess.
    virtual std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
        (void)asmText;
        (void)format;
        return std::nullopt;
    }
};

}  // namespace pcc
