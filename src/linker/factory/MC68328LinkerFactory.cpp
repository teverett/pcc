#include "MC68328LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/mc68328/MC68328Assembler.h"

namespace rcc {

namespace {
// 0x100000 must match MC68328Codegen.cpp's private kInitialSP.
// `dl _start` lets the assembler relocate reset vector 1 itself.
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

}  // namespace rcc
