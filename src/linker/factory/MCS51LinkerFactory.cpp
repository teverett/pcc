#include "MCS51LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/mcs51/MCS51Assembler.h"

namespace rcc {

namespace {
// Must mirror MCS51Codegen.cpp's own entry sequence exactly, including
// the pseudo-register equates, even though this sequence never
// touches XDATA itself.
constexpr const char *kEntryAsm =
    "org 0\n"
    "ac = 0x30\nbc = 0x32\ncx = 0x34\ndx = 0x36\nptr = 0x38\ndpl = 0x82\ndph = 0x83\n\n"
    "_start:\n"
    "    lcall fn_main\n"
    "    mova ac\n"
    "halt:\n"
    "    ljmp halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makeMCS51Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kMCS51BaseAddress;
    config.machineType = kMCS51CoffMachine;
    config.entryObject = assembleMCS51Object(kEntryAsm);
    config.relocPatchers[kMCS51RelocAbs16] = RelocKinds::patchAbs16BE;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    // XDATA and CODE both start at address 0 but are separate, non-aliasing
    // address spaces reached over separate buses (MOVX vs. PC-fetch).
    config.dataBaseAddress = 0;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
