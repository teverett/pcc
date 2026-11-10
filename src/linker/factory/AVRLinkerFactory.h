#pragma once

#include <memory>

#include "linker/Linker.h"

namespace pcc {

// Builds a GenericLinker for the avr backend: flash (program memory) objects laid out from
// address 0 (the real ATmega328P reset vector), SRAM (data memory) objects laid out from 0x0100
// (see AVRAssembler.h's kAVRSramBaseAddress), and a synthesized `_start` entry object (set
// SPL/SPH to RAMEND, `call fn_main`, spin forever) assembled and linked like any other object.
std::unique_ptr<TargetLinker> makeAVRLinker();

}  // namespace pcc
