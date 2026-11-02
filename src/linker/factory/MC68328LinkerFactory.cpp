#include "MC68328LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/mc68328/MC68328Assembler.h"

namespace pcc {

namespace {
// `1048576` (0x100000) is the same initial-SSP value as
// MC68328Codegen.cpp's own (private) `kInitialSP` - comfortably
// above the loaded program and its data, growing down into
// otherwise-unused RAM, same convention as ../m68k's own entry.
// `dl _start` (not the literal `8`) lets the assembler resolve
// vector 1 itself - this *is* a local address reference needing the
// same base-shift relocation as any other (see
// MC68328Assembler.h's own header comment), handled automatically,
// no special-casing needed.
constexpr const char *kEntryAsm =
    "org 0\n"
    "dl 1048576\n"
    "dl _start\n"
    "_start:\n"
    "    movel 1048576,a7\n"
    "    jsr fn_main\n"
    "halt:\n"
    "    bra halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makeMC68328Linker() {
    GenericLinkerConfig config;
    config.baseAddress = 0;
    config.machineType = kMC68328CoffMachine;
    config.entryObject = assembleMC68328Object(kEntryAsm);
    config.relocPatchers[kMC68328RelocAbs32] = RelocKinds::patchAbs32BE;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
