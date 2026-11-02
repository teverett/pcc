#include "Pdp7TapePacking.h"

#include <stdexcept>

namespace pcc {

namespace {

constexpr uint32_t kMask18 = 0777777;
constexpr uint32_t kSignatureTag = 0600000;   // the two high bits packWord's own "final" word always sets
constexpr uint32_t kSignatureMask = 0177777;  // everything packWord's own "final" word reserves for `base`

void packWord(std::string &out, uint32_t w, bool final) {
    out.push_back(static_cast<char>(((w >> 12) & 077) | 0200));
    out.push_back(static_cast<char>(((w >> 6) & 077) | 0200));
    out.push_back(static_cast<char>((w & 077) | 0200 | (final ? 0100 : 0)));
}

}  // namespace

std::string packPdp7Tape(const std::vector<uint32_t> &words, uint32_t base, bool rimSignature) {
    std::string out;
    for (uint32_t w : words) packWord(out, w & kMask18, false);
    if (rimSignature) packWord(out, (kSignatureTag | base) & kMask18, true);
    return out;
}

std::vector<uint32_t> unpackPdp7Tape(const std::string &bytes, bool rimSignature, uint32_t *outBase) {
    if (bytes.size() % 3 != 0) throw std::runtime_error("Pdp7TapePacking: tape length isn't a multiple of 3 bytes");
    std::vector<uint32_t> words;
    size_t numGroups = bytes.size() / 3;
    for (size_t g = 0; g < numGroups; g++) {
        uint8_t b0 = static_cast<uint8_t>(bytes[g * 3]);
        uint8_t b1 = static_cast<uint8_t>(bytes[g * 3 + 1]);
        uint8_t b2 = static_cast<uint8_t>(bytes[g * 3 + 2]);
        if ((b0 & 0200) == 0 || (b1 & 0200) == 0 || (b2 & 0200) == 0) {
            throw std::runtime_error("Pdp7TapePacking: a tape byte is missing its own high bit");
        }
        uint32_t word = ((static_cast<uint32_t>(b0) & 077) << 12) | ((static_cast<uint32_t>(b1) & 077) << 6) |
                         (static_cast<uint32_t>(b2) & 077);
        bool isFinal = (b2 & 0100) != 0;
        if (isFinal) {
            if (!rimSignature || g + 1 != numGroups) {
                throw std::runtime_error(
                    "Pdp7TapePacking: unexpected bootstrap-signature word (not the last, or not expected at all)");
            }
            if (outBase) *outBase = word & kSignatureMask;
            break;  // the signature word is never itself a program word
        }
        words.push_back(word);
    }
    if (rimSignature && (bytes.empty() || (static_cast<uint8_t>(bytes[bytes.size() - 1]) & 0100) == 0)) {
        throw std::runtime_error("Pdp7TapePacking: expected a trailing bootstrap-signature word, found none");
    }
    return words;
}

std::vector<uint32_t> wordsFromPdp7TextBytes(const std::vector<uint8_t> &textBytes) {
    if (textBytes.size() % 3 != 0) {
        throw std::runtime_error("Pdp7TapePacking: .text size isn't a multiple of 3 bytes - not word-aligned");
    }
    std::vector<uint32_t> words;
    words.reserve(textBytes.size() / 3);
    for (size_t i = 0; i < textBytes.size(); i += 3) {
        words.push_back((static_cast<uint32_t>(textBytes[i]) << 16) | (static_cast<uint32_t>(textBytes[i + 1]) << 8) |
                         static_cast<uint32_t>(textBytes[i + 2]));
    }
    return words;
}

std::vector<uint8_t> pdp7TextBytesFromWords(const std::vector<uint32_t> &words) {
    std::vector<uint8_t> textBytes;
    textBytes.reserve(words.size() * 3);
    for (uint32_t w : words) {
        textBytes.push_back(static_cast<uint8_t>((w >> 16) & 0xFF));
        textBytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        textBytes.push_back(static_cast<uint8_t>(w & 0xFF));
    }
    return textBytes;
}

}  // namespace pcc
