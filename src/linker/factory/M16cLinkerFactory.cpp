#include "M16cLinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m16c/M16cAssembler.h"

namespace pcc {

namespace {
// 57344 (0xE000) must match M16cCodegen.cpp's private kInitialSp.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    mov.w 57344,sp\n"
    "    jsr.w fn_main\n"
    "halt:\n"
    "    jmp halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makeM16cLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kM16cBaseAddress;
    config.machineType = kM16cCoffMachine;
    config.entryObject = assembleM16cObject(kEntryAsm);
    config.relocPatchers[kM16cRelocAbs16] = RelocKinds::patchAbs16;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
