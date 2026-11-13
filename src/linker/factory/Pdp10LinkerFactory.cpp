#include "Pdp10LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/pdp10/Pdp10Assembler.h"
#include "codegen/pdp10/Pdp10Codegen.h"

namespace rcc {

namespace {

constexpr uint64_t kMask18 = 0777777;
constexpr uint64_t kMask36 = 0777777777777ULL;

// Reads/writes one 36-bit word packed as 5 big-endian bytes (top 4 bits of the first byte
// unused/zero) - the same layout Pdp10Assembler.cpp's own emit36() produces, confirmed against
// real `pdp10` behavior while developing this target (see Pdp10Codegen.h's header comment).
uint64_t read36(const std::vector<uint8_t> &image, uint32_t siteOffset) {
    return (static_cast<uint64_t>(image[siteOffset] & 0xF) << 32) | (static_cast<uint64_t>(image[siteOffset + 1]) << 24) |
           (static_cast<uint64_t>(image[siteOffset + 2]) << 16) | (static_cast<uint64_t>(image[siteOffset + 3]) << 8) |
           static_cast<uint64_t>(image[siteOffset + 4]);
}
void write36(std::vector<uint8_t> &image, uint32_t siteOffset, uint64_t word) {
    word &= kMask36;
    image[siteOffset] = static_cast<uint8_t>((word >> 32) & 0xF);
    image[siteOffset + 1] = static_cast<uint8_t>((word >> 24) & 0xFF);
    image[siteOffset + 2] = static_cast<uint8_t>((word >> 16) & 0xFF);
    image[siteOffset + 3] = static_cast<uint8_t>((word >> 8) & 0xFF);
    image[siteOffset + 4] = static_cast<uint8_t>(word & 0xFF);
}

// Every reference this target ever emits is a plain absolute address occupying a word's low
// 18 bits (see Pdp10Assembler.h's own header comment). Those 18 bits aren't byte-aligned
// within the word's 5-byte packing (they straddle a fractional boundary inside the word's
// third byte), so `siteOffset` here is the *word's own start* (matching
// Pdp10Assembler.cpp's own relocation records) - this clears the low 18 bits of the whole
// word via read36()/write36(), then ORs in the resolved value.
void patchAbs18(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint64_t word = read36(image, siteOffset);
    word = (word & ~kMask18) | (static_cast<uint64_t>(value) & kMask18);
    write36(image, siteOffset, word);
}

// 32768 decimal (0100000 octal) must match Pdp10Codegen.cpp's own private kInitialSp. `15` is
// AC17's decimal register number (P) - see Pdp10Codegen.h's own header comment.
constexpr const char *kEntryAsm =
    "org 0\n"
    "_start:\n"
    "    movei 15,32768\n"
    "    pushj 15,fn_main\n"
    "halt:\n"
    "    halt\n"
    "    jrst 0,halt\n";
}  // namespace

std::unique_ptr<TargetLinker> makePdp10Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPdp10BaseAddress;
    config.machineType = kPdp10CoffMachine;
    config.entryObject = assemblePdp10Object(kEntryAsm);
    config.relocPatchers[kPdp10RelocAbs18] = patchAbs18;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    config.addressUnitBytes = 5;
    config.outputUnitBits = 36;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
