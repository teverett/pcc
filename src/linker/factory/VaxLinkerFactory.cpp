#include "VaxLinkerFactory.h"

#include "codegen/vax/VaxAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

// 1048576 is kInitialSP (0x00100000), the same stack top VaxCodegen.cpp's whole-program entry uses.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    movl imm 1048576,sp\n"
    "    calls imm 0,abs fn_main\n"
    "done:\n"
    "    halt\n"
    "    brb done\n";

}  // namespace

std::unique_ptr<TargetLinker> makeVaxLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kVaxBaseAddress;
    config.machineType = kVaxCoffMachine;
    config.entryObject = assembleVaxObject(kEntryAsm);
    config.relocPatchers[kVaxRelocAbs32] = RelocKinds::patchAbs32;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
