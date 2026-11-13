#include "AVRLinkerFactory.h"

#include <sstream>

#include "codegen/avr/AVRAssembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                   (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}

// `call`/`jmp`'s second word is a plain flash *word* address - see AVRAssembler.h's
// kAVRRelocCallWord. Every symbol here is word-aligned, so halving `value` first and adding that
// is exact (no rounding).
void patchCallWord(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t cur = get16(image, siteOffset);
    put16(image, siteOffset, static_cast<uint16_t>(cur + value / 2));
}

// `ldiw`/`ldiwc`'s two `ldi` words - see AVRAssembler.h's kAVRRelocLdiw16. `ldi`'s own immediate
// field is split non-contiguously inside the word (bits 11:8 and 3:0, around the fixed opcode and
// destination-register bits), so the two bytes can't be patched independently without losing a
// carry between them (see AVRAssembler.cpp's `encodeLdiw()`): this decodes both words' scattered
// bits back into one plain 16-bit value, adds `value` to that, and re-splits the sum.
void patchLdiw16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t w1 = get16(image, siteOffset);
    uint16_t w2 = get16(image, siteOffset + 2);
    uint32_t kLo = ((static_cast<uint32_t>(w1) >> 8) & 0xF) << 4 | (w1 & 0xF);
    uint32_t kHi = ((static_cast<uint32_t>(w2) >> 8) & 0xF) << 4 | (w2 & 0xF);
    uint32_t cur = kLo | (kHi << 8);
    uint32_t newv = (cur + value) & 0xFFFF;
    uint32_t nLo = newv & 0xFF, nHi = (newv >> 8) & 0xFF;
    w1 = static_cast<uint16_t>((w1 & 0xF0F0) | (((nLo >> 4) & 0xF) << 8) | (nLo & 0xF));
    w2 = static_cast<uint16_t>((w2 & 0xF0F0) | (((nHi >> 4) & 0xF) << 8) | (nHi & 0xF));
    put16(image, siteOffset, w1);
    put16(image, siteOffset + 2, w2);
}

// SPH/SPL are I/O addresses 0x3E/0x3D on every classic-core ATmega; RAMEND is this target's last
// usable SRAM byte (kAVRSramBaseAddress + 2048 - 1 - see AVRCodegen.h's own header comment on the
// ATmega328P's 2 KB of SRAM).
//
// `__wa`/`__wb`/`__wc`/`__wd` (AVRCodegen.h's own wide-value scratch buffers) are declared here,
// in the one object every link always includes, rather than in each compiled file's own output
// (AVRCodegen.cpp's generate() only does that in non-object/single-file mode): a `long`/
// `long long` call argument or return value staged through one of them has to resolve to the
// *same* four buffers regardless of which object does the staging and which reads it back, and
// only a single, shared definition - resolved by every other object's own `lds`/`sts` as a plain
// extern, the same as a user `extern` global - guarantees that.
std::string entryAsm() {
    constexpr uint32_t kRamEnd = kAVRSramBaseAddress + 2048 - 1;
    std::ostringstream out;
    out << "org 0\n";
    out << "__wa: ram 8\n__wb: ram 8\n__wc: ram 8\n__wd: ram 8\n";
    out << "    ldi r16, " << (kRamEnd & 0xFF) << "\n";
    out << "    out 0x3d, r16\n";
    out << "    ldi r16, " << ((kRamEnd >> 8) & 0xFF) << "\n";
    out << "    out 0x3e, r16\n";
    out << "    call fn_main\n";
    out << "done:\n";
    out << "    rjmp done\n";
    return out.str();
}

}  // namespace

std::unique_ptr<TargetLinker> makeAVRLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kAVRFlashBaseAddress;
    config.machineType = kAVRCoffMachine;
    config.entryObject = assembleAVRObject(entryAsm());
    config.relocPatchers[kAVRRelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kAVRRelocCallWord] = patchCallWord;
    config.relocPatchers[kAVRRelocLdiw16] = patchLdiw16;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    config.dataBaseAddress = kAVRSramBaseAddress;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
