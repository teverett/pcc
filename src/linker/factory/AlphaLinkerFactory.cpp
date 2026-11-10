#include "AlphaLinkerFactory.h"

#include <cstdint>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/alpha/AlphaAssembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {

uint32_t get32LE(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint32_t>(image[idx]) | (static_cast<uint32_t>(image[idx + 1]) << 8) |
           (static_cast<uint32_t>(image[idx + 2]) << 16) | (static_cast<uint32_t>(image[idx + 3]) << 24);
}
void put32LE(std::vector<uint8_t> &image, size_t idx, uint32_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    image[idx + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    image[idx + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

// Lower half of an `ldah`/`lda` pair: a plain additive 16-bit patch (the field holds raw bits,
// not yet sign-interpreted - see patchAlphaHigh()'s own comment for why that's enough here).
void patchAlphaLow(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t word = get32LE(image, siteOffset);
    uint32_t loPost = ((word & 0xFFFFu) + (value & 0xFFFFu)) & 0xFFFFu;
    put32LE(image, siteOffset, (word & 0xFFFF0000u) | loPost);
}

// Upper half of the pair. `lda`'s 16-bit field sign-extends (unlike a zero-extending `ori`),
// so simply splitting `value` at bit 16 and adding each half independently is wrong whenever
// the combined low half ends up negative: patched before the paired Lo site (always at
// siteOffset+4), this reads that site's own pre-patch bits to work out both the plain
// unsigned carry out of the low 16 bits (`k`) and whether the low half's sign bit flips as a
// result of this patch (`sPre`/`sPost`) - a flip either way shifts how much the high half's
// own sign-extended-low-half interpretation is still owed by one extra 0x10000, the same
// "high adjust" AlphaAssembler.cpp's own %hi/%lo split applies when building this pair from a
// value fully known at assembly time.
void patchAlphaHigh(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loWord = get32LE(image, siteOffset + 4);
    uint32_t loPre = loWord & 0xFFFFu;
    uint64_t sum = static_cast<uint64_t>(loPre) + static_cast<uint64_t>(value);
    uint32_t loPost = static_cast<uint32_t>(sum & 0xFFFFu);
    uint32_t k = static_cast<uint32_t>(sum >> 16);
    int64_t sPre = (loPre & 0x8000u) ? 1 : 0;
    int64_t sPost = (loPost & 0x8000u) ? 1 : 0;
    int64_t hiAdd = static_cast<int64_t>(k) + sPost - sPre;

    uint32_t word = get32LE(image, siteOffset);
    uint32_t hiPost = static_cast<uint32_t>(static_cast<int64_t>(word & 0xFFFFu) + hiAdd) & 0xFFFFu;
    put32LE(image, siteOffset, (word & 0xFFFF0000u) | hiPost);
}

void patchAlphaAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32LE(image, siteOffset, get32LE(image, siteOffset) + value);
}

// Splits `v` the same way AlphaAssembler.cpp's own `ldah`/`lda` %hi/%lo handling does, for a
// value fully known here (the initial stack pointer, below) rather than deferred to a
// relocation: `lo` is `v`'s low 16 bits, sign-extended back out to a plain (decimal-text)
// int32 so it round-trips through the shared grammar's signed-literal syntax; `hi` is
// whatever's left once `lo` is subtracted back out, which is exactly divisible by 0x10000.
std::pair<int32_t, int32_t> splitHiLo(uint32_t v) {
    int32_t lo = static_cast<int32_t>(static_cast<int16_t>(v & 0xFFFFu));
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(v) - lo) >> 16);
    return {hi, lo};
}

// kInitialSP (0x00200000) split into its ldah/lda halves; kInitialSP comfortably above
// AlphaCodegen.cpp's own kBaseAddress and this program's own code+data.
constexpr uint32_t kInitialSP = 0x00200000u;

std::string buildEntryAsm() {
    auto [spHi, spLo] = splitHiLo(kInitialSP);
    std::ostringstream out;
    out << "org 0\n";
    out << "_start:\n";
    out << "    ldah sp,zero," << spHi << "\n";
    out << "    lda sp,sp," << spLo << "\n";
    out << "    ldah t12,zero,fn_main\n";
    out << "    lda t12,t12,fn_main\n";
    out << "    jsr ra,(t12)\n";
    out << "halt:\n";
    out << "    br halt\n";
    return out.str();
}

}  // namespace

std::unique_ptr<TargetLinker> makeAlphaLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kAlphaBaseAddress;
    config.machineType = kAlphaCoffMachine;
    config.entryObject = assembleAlphaObject(buildEntryAsm());
    config.relocPatchers[kAlphaRelocHigh] = patchAlphaHigh;
    config.relocPatchers[kAlphaRelocLow] = patchAlphaLow;
    config.relocPatchers[kAlphaRelocAbs32] = patchAlphaAbs32;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
