#include "STM8LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/stm8/STM8Assembler.h"

namespace pcc {

namespace {
// 65280 (0xFF00) is the stack top; 57344 (0xE000) is the software frame-stack's initial top.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    ldwi sp, 65280\n"
    "    ldwi x, 57344\n"
    "    ldw fsp, x\n"
    "    ldwi x, 0\n"
    "    ldw fp, x\n"
    "    call fn_main\n"
    "    ldw x, ac\n"
    "halt:\n"
    "    jp halt\n"
    "ac: db 0, 0\n"
    "bc: db 0, 0\n"
    "cx: db 0, 0\n"
    "dx: db 0, 0\n"
    "ptr: db 0, 0\n"
    "fp: db 0, 0\n"
    "fsp: db 0, 0\n"
    "lc: db 0\n";
}  // namespace

std::unique_ptr<TargetLinker> makeSTM8Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kSTM8BaseAddress;
    config.machineType = kSTM8CoffMachine;
    config.entryObject = assembleSTM8Object(kEntryAsm);
    config.relocPatchers[kSTM8RelocAbs16] = RelocKinds::patchAbs16BE;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
