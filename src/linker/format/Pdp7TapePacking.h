#pragma once

#include <string>

#include "linker/LinkedImage.h"

namespace pcc {

// Shared by RimWriter.cpp/PtrWriter.cpp - not itself a FormatWriter,
// just the one packing scheme they both use: pdp7's own paper-tape
// byte packing, 3 bytes per 18-bit word, 6 bits each with the high
// bit always set (not a custom convention - see
// ../../../pdp-assembler/src/Assembler.cpp's own writeOutput, which
// this reproduces). `rimSignature`, if true, appends the one extra
// "jmp base" signature word real `rim` tapes end with so the
// hardware bootstrap loader knows where to start execution; a plain
// `ptr` tape has none (loading it still needs a separate "go" command
// telling the simulator/hardware where to start).
std::string packPdp7Tape(const LinkedImage &image, bool rimSignature);

}  // namespace pcc
