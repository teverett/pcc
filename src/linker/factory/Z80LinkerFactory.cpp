#include "Z80LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/z80/Z80Assembler.h"

namespace pcc {

namespace {
// `65280` (0xFF00) is the same stack-top value as Z80Codegen.cpp's
// own (private) `kStackTop` - the Z80's SP is undefined at reset and
// this backend's real CALL/RET/PUSH/POP all need a real initial
// value, same reasoning as that file's own comment.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    ld sp, 65280\n"
    "    call fn_main\n"
    "    ld hl, [ac]\n"
    "halt:\n"
    "    jp halt\n"
    "ac: db 0, 0\n"
    "_bc: db 0, 0\n"
    "cx: db 0, 0\n"
    "dx: db 0, 0\n"
    "ptr: db 0, 0\n";
}  // namespace

std::unique_ptr<TargetLinker> makeZ80Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kZ80BaseAddress;
    config.machineType = kZ80CoffMachine;
    config.entryObject = assembleZ80Object(kEntryAsm);
    config.relocPatchers[kZ80RelocAbs16] = RelocKinds::patchAbs16;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
