#include "Pdp7TapePacking.h"

namespace pcc {

namespace {

constexpr uint32_t kMask18 = 0777777;

void packWord(std::string &out, uint32_t w, bool final) {
    out.push_back(static_cast<char>(((w >> 12) & 077) | 0200));
    out.push_back(static_cast<char>(((w >> 6) & 077) | 0200));
    out.push_back(static_cast<char>((w & 077) | 0200 | (final ? 0100 : 0)));
}

}  // namespace

std::string packPdp7Tape(const LinkedImage &image, bool rimSignature) {
    std::string out;
    for (uint32_t w : image.words) packWord(out, w & kMask18, false);
    if (rimSignature) packWord(out, (0600000 | image.base) & kMask18, true);
    return out;
}

}  // namespace pcc
