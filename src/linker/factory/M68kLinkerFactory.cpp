#include "M68kLinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m68k/M68kAssembler.h"

namespace pcc {

namespace {
// `0x100000` is the same initial-stack-pointer value as
// M68kCodegen.cpp's own (private) `kInitialSP` - comfortably above
// the loaded program and its data, same reasoning as that file's own
// comment.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    movel 1048576, a7\n"
    "    jsr fn_main\n"
    "halt:\n"
    "    bra halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makeM68kLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kM68kBaseAddress;
    config.machineType = kM68kCoffMachine;
    config.entryObject = assembleM68kObject(kEntryAsm);
    config.relocPatchers[kM68kRelocAbs32] = RelocKinds::patchAbs32BE;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
