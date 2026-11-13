#include "PIC16LinkerFactory.h"

#include <cstdint>
#include <vector>

#include "linker/GenericLinker.h"
#include "codegen/pic16/PIC16Assembler.h"

namespace rcc {

namespace {

uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}
void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// Masked add into GOTO/CALL's 11-bit word-address field (bits [10:0]),
// preserving the opcode in bits [15:11]. `value` is a byte address, so
// it is halved to a word address first.
void patchPIC16GotoCall(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t addr = static_cast<uint16_t>(((word & 0x07FF) + (value / 2)) & 0x07FF);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0x07FFu) | addr));
}

// Masked add into the 7-bit file-register field (bits [6:0]); the GPR
// bank fits entirely in 7 bits. File-register space is byte-addressed,
// so no word/byte scaling is needed here.
void patchPIC16Data(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t addr = static_cast<uint16_t>(((word & 0x7F) + value) & 0x7F);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0x7Fu) | addr));
}

// Masked add into MOVLW's 8-bit literal field (bits [7:0]) with an extern
// function's resolved word address (`value` is a byte address, halved
// here, matching patchPIC16GotoCall). The field is always 0 pre-link for
// this relocation kind (only an extern symbol uses it - see
// PIC16Assembler.cpp's movlwcl/movlwch case), so there's no carry to lose
// by patching the lo/hi byte independently.
void patchPIC16CodeLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t lo = static_cast<uint16_t>(((word & 0xFF) + (value / 2)) & 0xFF);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0xFFu) | lo));
}
void patchPIC16CodeHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t hi = static_cast<uint16_t>(((word & 0xFF) + ((value / 2) >> 8)) & 0xFF);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0xFFu) | hi));
}

// Must mirror PIC16Codegen.cpp's own declareRam() pseudo-register
// equates exactly, then points SP at SSTACK before calling fn_main.
const std::string kEntryAsm = "org 0\n"
                               "ac = " + std::to_string(kPIC16RegAc) + "\n" +
                               "bc = " + std::to_string(kPIC16RegBc) + "\n" +
                               "cx = " + std::to_string(kPIC16RegCx) + "\n" +
                               "dx = " + std::to_string(kPIC16RegDx) + "\n" +
                               "ptr = " + std::to_string(kPIC16RegPtr) + "\n" +
                               "SP = " + std::to_string(kPIC16RegSP) + "\n" +
                               "FP = " + std::to_string(kPIC16RegFP) + "\n" +
                               "SSTACK = " + std::to_string(kPIC16RegSStack) + "\n" +
                               "_start:\n"
                               "    movlw SSTACK\n"
                               "    movwf SP\n"
                               "    call fn_main\n"
                               "HALT:\n"
                               "    goto HALT\n";

}  // namespace

std::unique_ptr<TargetLinker> makePIC16Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPIC16BaseAddress;
    config.machineType = kPIC16CoffMachine;
    config.dataBaseAddress = kPIC16DataBaseAddress;
    config.entryObject = assemblePIC16Object(kEntryAsm);
    config.relocPatchers[kPIC16RelocGotoCall] = patchPIC16GotoCall;
    config.relocPatchers[kPIC16RelocData] = patchPIC16Data;
    config.relocPatchers[kPIC16RelocCodeLo] = patchPIC16CodeLo;
    config.relocPatchers[kPIC16RelocCodeHi] = patchPIC16CodeHi;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
