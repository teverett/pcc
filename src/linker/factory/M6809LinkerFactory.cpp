#include "M6809LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m6809/M6809Assembler.h"

namespace rcc {

namespace {
// 0x7F00: leaves the entry sequence's own stack well clear of a stock 64K map's ROM window
// (conventionally $8000-$FFFF), mirroring kZ80BaseAddress's own "near the top of RAM" choice.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    lds 32512\n"
    "    jsr [fn_main]\n"
    "    std [ac]\n"
    "halt:\n"
    "    jmp [halt]\n"
    "ac: dw 0\n"
    "_bc: dw 0\n"
    "cx: dw 0\n"
    "dx: dw 0\n"
    "ptr: dw 0\n";
}  // namespace

std::unique_ptr<TargetLinker> makeM6809Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kM6809BaseAddress;
    config.machineType = kM6809CoffMachine;
    config.entryObject = assembleM6809Object(kEntryAsm);
    config.relocPatchers[kM6809RelocAbs16] = RelocKinds::patchAbs16BE;
    config.outputFormats = {"raw", "coff", "elf", "aout", "decb"};
    config.addressUnitBytes = 1;
    config.outputUnitBits = 8;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
