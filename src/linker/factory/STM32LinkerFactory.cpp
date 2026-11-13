#include "STM32LinkerFactory.h"

#include "codegen/stm32/STM32Assembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {

// Thumb-2 instructions are stored as two little-endian 16-bit halfwords in instruction order, not
// one plain 32-bit word.
uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint16_t>(image[idx]) | (static_cast<uint16_t>(image[idx + 1]) << 8));
}
void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// `movw`/`movt Rd,#imm16` (T3) imm4:i:imm3:imm8 split: bits [3:0],[10] of the first halfword,
// bits [14:12],[7:0] of the second.
uint16_t readImm16(uint16_t hw1, uint16_t hw2) {
    uint16_t imm4 = hw1 & 0xFu;
    uint16_t i = static_cast<uint16_t>((hw1 >> 10) & 1u);
    uint16_t imm3 = static_cast<uint16_t>((hw2 >> 12) & 0x7u);
    uint16_t imm8 = hw2 & 0xFFu;
    return static_cast<uint16_t>((imm4 << 12) | (i << 11) | (imm3 << 8) | imm8);
}
void writeImm16(uint16_t &hw1, uint16_t &hw2, uint16_t imm16) {
    uint16_t imm4 = static_cast<uint16_t>((imm16 >> 12) & 0xFu);
    uint16_t i = static_cast<uint16_t>((imm16 >> 11) & 1u);
    uint16_t imm3 = static_cast<uint16_t>((imm16 >> 8) & 0x7u);
    uint16_t imm8 = imm16 & 0xFFu;
    hw1 = static_cast<uint16_t>((hw1 & ~0x040Fu) | imm4 | (i << 10));
    hw2 = static_cast<uint16_t>((hw2 & ~0x70FFu) | (imm3 << 12) | imm8);
}

void patchStm32MovWLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t hw1 = get16(image, siteOffset);
    uint16_t hw2 = get16(image, siteOffset + 2);
    uint16_t imm16 = static_cast<uint16_t>((readImm16(hw1, hw2) + (value & 0xFFFFu)) & 0xFFFFu);
    writeImm16(hw1, hw2, imm16);
    put16(image, siteOffset, hw1);
    put16(image, siteOffset + 2, hw2);
}

// High half: needs the carry out of the low half's addition, read from the already-patched
// sibling site 4 bytes earlier (`movw` is always immediately followed by `movt` for the same
// label, patched in that order).
void patchStm32MovTHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t lowHw1 = get16(image, siteOffset - 4);
    uint16_t lowHw2 = get16(image, siteOffset - 2);
    uint32_t lowImm16 = readImm16(lowHw1, lowHw2);
    uint32_t carry = (lowImm16 + (value & 0xFFFFu)) > 0xFFFFu ? 1u : 0u;

    uint16_t hw1 = get16(image, siteOffset);
    uint16_t hw2 = get16(image, siteOffset + 2);
    uint16_t imm16 =
        static_cast<uint16_t>((readImm16(hw1, hw2) + ((value >> 16) & 0xFFFFu) + carry) & 0xFFFFu);
    writeImm16(hw1, hw2, imm16);
    put16(image, siteOffset, hw1);
    put16(image, siteOffset + 2, hw2);
}

// `bl`/`b` (T1 BL / T4 B) wide 25-bit PC-relative field, non-contiguous: S/imm10 in the first
// halfword, J1/J2/imm11 in the second, with J1/J2 each XORed against S and the matching I1/I2
// bit. Field starts zeroed, so this only writes, never adds. +4 offset (no extra pipeline offset
// beyond PC = this instruction's address + size).
void patchStm32Branch25(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t offset = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr + 4);
    uint32_t val25 = static_cast<uint32_t>(offset) & 0x1FFFFFFu;
    uint32_t S = (val25 >> 24) & 1u, I1 = (val25 >> 23) & 1u, I2 = (val25 >> 22) & 1u;
    uint32_t imm10 = (val25 >> 12) & 0x3FFu, imm11 = (val25 >> 1) & 0x7FFu;
    uint32_t J1 = S ^ I1 ^ 1u, J2 = S ^ I2 ^ 1u;

    uint16_t hw1 = get16(image, siteOffset);
    uint16_t hw2 = get16(image, siteOffset + 2);
    hw1 = static_cast<uint16_t>((hw1 & ~0x07FFu) | (S << 10) | imm10);
    hw2 = static_cast<uint16_t>((hw2 & ~0x2FFFu) | (J1 << 13) | (J2 << 11) | imm11);
    put16(image, siteOffset, hw1);
    put16(image, siteOffset + 2, hw2);
}

// 536879104 (0x20002000) is the initial stack pointer, top of SRAM. Unlike other targets, `sp` is
// never explicitly loaded: a Cortex-M reset exception auto-loads it from the vector table's first
// word, so this object only supplies the table and the reset handler it points to.
constexpr const char *kEntryAsm =
    "org 0\n"
    "dl 536879104\n"
    "dlt _start\n"
    "_start:\n"
    "    bl fn_main\n"
    "halt:\n"
    "    b halt\n";

}  // namespace

std::unique_ptr<TargetLinker> makeSTM32Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kSTM32BaseAddress;
    config.machineType = kSTM32CoffMachine;
    config.entryObject = assembleSTM32Object(kEntryAsm);
    config.relocPatchers[kSTM32RelocAbs32] = RelocKinds::patchAbs32;
    config.relocPatchers[kSTM32RelocMovWLo] = patchStm32MovWLo;
    config.relocPatchers[kSTM32RelocMovTHi] = patchStm32MovTHi;
    config.relocPatchers[kSTM32RelocBranch25] = patchStm32Branch25;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
