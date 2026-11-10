#include "SaturnLinkerFactory.h"

#include <string>

#include "codegen/saturn/SaturnAssembler.h"
#include "codegen/saturn/SaturnCodegen.h"
#include "linker/GenericLinker.h"

namespace pcc {

std::unique_ptr<TargetLinker> makeSaturnLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kSaturnBaseAddress;
    config.machineType = kSaturnCoffMachine;
    // Beyond the entry sequence, this is the single object that defines the shared software
    // multiply/divide runtime (see saturnRuntimeAsm()), since every user object leaves it as
    // external references rather than defining its own copy - same reasoning as pdp7's own
    // entry object (Pdp7LinkerFactory.cpp).
    config.entryObject = assembleSaturnObject("mov d0, " + std::to_string(kSaturnStackTop) +
                                               "\ngosbvl FN_main\nhalt\n" + saturnRuntimeAsm());
    config.relocPatchers[kSaturnRelocAbs32] = RelocKinds::patchAbs32;
    // "raw" is reserved by the shared FileFormat writer for an x86 512-byte boot sector (see
    // RawFormat.cpp) - not what this backend means by a whole-program flat binary - so the
    // linked-output format name here is "flat" (same as z80/m6502/m68k's own LinkerFactory),
    // even though Backend::assembledFormats() still calls the single-file equivalent "raw"
    // (that path never goes through FileFormat at all - see SaturnCodegen::assembleInProcess()).
    config.outputFormats = {"flat", "coff"};
    // Byte-addressed, little-endian - the default GenericLinkerConfig already assumes this
    // (addressUnitBytes = 1, outputUnitBits = 8), unlike pdp7/vax's own word-addressed targets.
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
