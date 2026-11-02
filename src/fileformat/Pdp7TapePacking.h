#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// Shared by rim/RimFormat.cpp and ptr/PtrFormat.cpp - not itself a
// file format, just the one byte-packing scheme they both use: pdp7's
// own paper-tape byte packing, 3 bytes per 18-bit word, 6 bits each
// with the high bit always set (not a custom convention - see
// ../../pdp-assembler/src/Assembler.cpp's own writeOutput, which this
// reproduces). `rimSignature`, if true, appends the one extra "jmp
// base" signature word real `rim` tapes end with so the hardware
// bootstrap loader knows where to start execution; a plain `ptr` tape
// has none (loading it still needs a separate "go" command telling
// the simulator/hardware where to start).
std::string packPdp7Tape(const std::vector<uint32_t> &words, uint32_t base, bool rimSignature);

// The exact inverse of packPdp7Tape() above: unpacks a real paper-
// tape byte stream back into the 18-bit words it encodes. If
// `rimSignature` is true, the trailing "jmp base" signature word (see
// above) is recognized and stripped rather than decoded as ordinary
// program data, and `*outBase` (if non-null) receives the base
// address it encodes.
std::vector<uint32_t> unpackPdp7Tape(const std::string &bytes, bool rimSignature, uint32_t *outBase = nullptr);

// `ObjectFile::textBytes` (see ../ObjectFile.h) is pdp7's own real,
// final 18-bit words, 3 raw bytes each, big-endian (see
// ../../codegen/pdp7/Pdp7Assembler.cpp's own `buildObjectFile()`) -
// not the distinct 6-bit-per-byte paper-tape packing above. Every
// pdp7-specific file format (rim/ptr/a7out/list) that needs real word
// values rather than raw bytes converts between the two with these.
std::vector<uint32_t> wordsFromPdp7TextBytes(const std::vector<uint8_t> &textBytes);
std::vector<uint8_t> pdp7TextBytesFromWords(const std::vector<uint32_t> &words);

}  // namespace pcc
