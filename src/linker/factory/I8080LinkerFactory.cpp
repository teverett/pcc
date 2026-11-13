#include "I8080LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/i8080/I8080Assembler.h"

namespace rcc {

namespace {
// 65280 (0xFF00): the 8080's SP is undefined at reset and CALL/RET/PUSH/POP need a real value.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    lxi sp, 65280\n"
    "    call fn_main\n"
    "    lhld ac\n"
    "halt:\n"
    "    jmp halt\n"
    "ac: db 0, 0\n"
    "bc: db 0, 0\n"
    "cx: db 0, 0\n"
    "dx: db 0, 0\n"
    "ptr: db 0, 0\n";
}  // namespace

std::unique_ptr<TargetLinker> makeI8080Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kI8080BaseAddress;
    config.machineType = kI8080CoffMachine;
    config.entryObject = assembleI8080Object(kEntryAsm);
    config.relocPatchers[kI8080RelocAbs16] = RelocKinds::patchAbs16;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
