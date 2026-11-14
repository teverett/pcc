#include "Tms9900LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/tms9900/TMS9900Assembler.h"

namespace rcc {

namespace {
// WP must be set (via LWPI) before R0-R15 mean anything at all - see TMS9900Codegen.h's own
// header comment for why $8300 - then r15 (the software stack pointer) is seeded, `bl`
// branches into `fn_main` (setting r11 to the address of the following `halt` label, so
// `fn_main`'s own ordinary epilogue - "b [r11]" - lands there correctly, the same convention
// every nested call/return pair in this backend uses), and the result sits in r0 once
// execution reaches `halt`, a directly inspectable register.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    lwpi 33536\n"
    "    li r15, 61440\n"
    "    bl [fn_main]\n"
    "halt:\n"
    "    jmp halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makeTMS9900Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kTMS9900BaseAddress;
    config.machineType = kTMS9900CoffMachine;
    config.entryObject = assembleTMS9900Object(kEntryAsm);
    config.relocPatchers[kTMS9900RelocAbs16] = RelocKinds::patchAbs16BE;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
