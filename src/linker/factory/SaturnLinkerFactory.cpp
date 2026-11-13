#include "SaturnLinkerFactory.h"

#include <string>

#include "codegen/saturn/SaturnAssembler.h"
#include "codegen/saturn/SaturnCodegen.h"
#include "linker/GenericLinker.h"

namespace rcc {

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
    // The linked-output format name here is "raw" (same as every other LinkerFactory,
    // including x86_8086's own), same name Backend::assembledFormats() uses for the
    // single-file equivalent (that path never goes through FileFormat at all - see
    // SaturnCodegen::assembleInProcess()).
    config.outputFormats = {"raw", "coff"};
    // Byte-addressed, little-endian - the default GenericLinkerConfig already assumes this
    // (addressUnitBytes = 1, outputUnitBits = 8), unlike pdp7/vax's own word-addressed targets.
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
