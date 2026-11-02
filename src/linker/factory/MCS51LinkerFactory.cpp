#include "MCS51LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/mcs51/MCS51Assembler.h"

namespace pcc {

namespace {
// Mirrors MCS51Codegen.cpp's own `generate()` entry sequence exactly
// (the fixed internal-RAM pseudo-register equates, `lcall fn_main`,
// a final `mova ac` so the result also sits in the real accumulator,
// then an infinite-loop halt) - see that file's own header comment
// for why 0x30 is where these five pseudo-registers live, and for
// what `dpl`/`dph` (the real DPL/DPH SFRs) are for; this entry
// sequence itself never touches XDATA, but declares them anyway to
// keep this text an exact mirror.
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
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    // Real XDATA address 0 - this target's own second, Harvard data
    // address space (see MCS51Codegen.h's own header comment) starts
    // at the same address CODE does on this target, but the two never
    // alias each other (reached over entirely separate buses - MOVX
    // vs. PC-fetch), so this is a coincidence of this one target's
    // own real hardware, not a shared constant with `baseAddress`
    // above (see GenericLinkerConfig::dataBaseAddress's own comment).
    config.dataBaseAddress = 0;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
