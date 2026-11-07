#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pcc {

// pdp7 paper-tape packing: 3 bytes per 18-bit word, 6 bits each, high bit always set. If
// `rimSignature`, appends a trailing "jmp base" word so the bootstrap loader knows where to start
// execution; a plain `ptr` tape has none.
std::string packPdp7Tape(const std::vector<uint32_t> &words, uint32_t base, bool rimSignature);

// Inverse of packPdp7Tape(). If `rimSignature`, strips the trailing signature word instead of
// decoding it as data, and `*outBase` (if non-null) receives the base address it encodes.
std::vector<uint32_t> unpackPdp7Tape(const std::string &bytes, bool rimSignature, uint32_t *outBase = nullptr);

// Converts between `ObjectFile::textBytes` (18-bit words as 3 raw big-endian bytes each) and the
// distinct 6-bit-per-byte paper-tape packing above.
std::vector<uint32_t> wordsFromPdp7TextBytes(const std::vector<uint8_t> &textBytes);
std::vector<uint8_t> pdp7TextBytesFromWords(const std::vector<uint32_t> &words);

}  // namespace pcc
