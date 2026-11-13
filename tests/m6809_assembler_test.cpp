#include "codegen/m6809/M6809Assembler.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

// `ldd 1234` (immediate 16-bit load, real 6809 opcode 0xCC) followed by
// `std [ac]` (extended-addressing store to a resolved label), with `ac`
// itself a `dw 0` cell at the program's `org` address - exercises
// immediate loads, extended stores, label resolution, and this
// dialect's big-endian multi-byte field order all at once.
void testLddImmediateAndStdExtended() {
    std::string asmText =
        "org 0x100\n"
        "ac: dw 0\n"
        "ldd 1234\n"
        "std [ac]\n";
    std::string bytes = rcc::assembleM6809(asmText);

    std::string expected;
    expected.push_back(0x00);
    expected.push_back(0x00);  // ac's own `dw 0` cell
    expected.push_back(static_cast<char>(0xCC));  // LDD immediate
    expected.push_back(0x04);
    expected.push_back(static_cast<char>(0xD2));  // 1234 = 0x04D2, big-endian
    expected.push_back(static_cast<char>(0xFD));  // STD extended
    expected.push_back(0x01);
    expected.push_back(0x00);  // ac's address, 0x0100, big-endian
    require(bytes == expected, "ldd #1234 / std [ac] didn't produce the expected byte sequence");
}

// `leax 4,u` - the 8-bit-offset indexed addressing form this backend
// always uses: opcode 0x30, postbyte 0x88|0x40 (U register select) =
// 0xC8, then the signed offset byte.
void testLeaxIndexed() {
    std::string bytes = rcc::assembleM6809("leax 4,u\n");
    std::string expected;
    expected.push_back(0x30);
    expected.push_back(static_cast<char>(0xC8));
    expected.push_back(0x04);
    require(bytes == expected, "leax 4,u didn't produce the expected opcode+postbyte+offset bytes");
}

// A negative offset round-trips through the two's-complement byte
// correctly: `leax -12,s` -> postbyte 0x88|0x60=0xE8, offset byte 0xF4.
void testLeaxIndexedNegativeOffset() {
    std::string bytes = rcc::assembleM6809("leax -12,s\n");
    std::string expected;
    expected.push_back(0x30);
    expected.push_back(static_cast<char>(0xE8));
    expected.push_back(static_cast<char>(0xF4));
    require(bytes == expected, "leax -12,s didn't encode the negative offset correctly");
}

// `pshs a,b,x` - register-list postbyte: A=0x02 | B=0x04 | X=0x10 = 0x16.
void testPshsRegisterList() {
    std::string bytes = rcc::assembleM6809("pshs a,b,x\n");
    std::string expected;
    expected.push_back(0x34);
    expected.push_back(0x16);
    require(bytes == expected, "pshs a,b,x didn't produce the expected register-list postbyte");
}

// `mul` - single opcode byte, no operand.
void testMul() {
    std::string bytes = rcc::assembleM6809("mul\n");
    require(bytes.size() == 1, "mul should assemble to exactly one byte");
    require(static_cast<uint8_t>(bytes[0]) == 0x3D, "mul's opcode byte is wrong");
}

// `tfr s,u` - postbyte (S=0x4 << 4) | (U=0x3) = 0x43, used by the
// codegen's own function-prologue frame setup.
void testTfr() {
    std::string bytes = rcc::assembleM6809("tfr s,u\n");
    std::string expected;
    expected.push_back(0x1F);
    expected.push_back(0x43);
    require(bytes == expected, "tfr s,u didn't produce the expected postbyte");
}

// `lbeq somewhere` - prefixed long branch (0x10 0x27), with a correctly
// computed big-endian relative displacement to a backward label.
void testLbeqBackwardBranch() {
    std::string asmText =
        "top:\n"
        "mul\n"
        "lbeq top\n";
    std::string bytes = rcc::assembleM6809(asmText);
    require(bytes.size() == 1 + 4, "mul + lbeq should be 5 bytes total");
    require(static_cast<uint8_t>(bytes[1]) == 0x10 && static_cast<uint8_t>(bytes[2]) == 0x27,
            "lbeq's opcode bytes are wrong");
    // Displacement is relative to the address right after the 2-byte
    // field: site of the field is offset 3, so "next instruction" is at
    // offset 5; target (`top`) is at offset 0, so the displacement is
    // 0 - 5 = -5 (0xFFFB).
    require(static_cast<uint8_t>(bytes[3]) == 0xFF && static_cast<uint8_t>(bytes[4]) == 0xFB,
            "lbeq's relative displacement is wrong");
}

// `adcb [x]` / `sbca [x]` - the carry/borrow-chain 8-bit ALU ops added
// specifically so M6809Codegen's genWideAdd() can propagate a carry from
// one 16-bit word into the next (no 16-bit ADCD exists on real 6809
// hardware). Opcodes: ADCB extended = 0xF9, SBCA extended = 0xB2.
void testAdcbAndSbcaExtended() {
    std::string asmText =
        "org 0x200\n"
        "cell: dw 0\n"
        "adcb [cell]\n"
        "sbca [cell]\n";
    std::string bytes = rcc::assembleM6809(asmText);
    std::string expected;
    expected.push_back(0x00);
    expected.push_back(0x00);  // cell's own `dw 0`
    expected.push_back(static_cast<char>(0xF9));
    expected.push_back(0x02);
    expected.push_back(0x00);  // ADCB [cell], cell at 0x0200
    expected.push_back(static_cast<char>(0xB2));
    expected.push_back(0x02);
    expected.push_back(0x00);  // SBCA [cell]
    require(bytes == expected, "adcb/sbca extended didn't produce the expected opcode+address bytes");
}

// `jmp 0,x` / `jsr 4,u` - the indexed forms added so M6809Codegen can call
// through a register-held address (C function pointers). Opcodes: JMP
// indexed = 0x6E, JSR indexed = 0xAD.
void testJmpJsrIndexed() {
    std::string bytes = rcc::assembleM6809("jmp 0,x\njsr 4,u\n");
    std::string expected;
    expected.push_back(0x6E);
    expected.push_back(static_cast<char>(0x88));
    expected.push_back(0x00);
    expected.push_back(static_cast<char>(0xAD));
    expected.push_back(static_cast<char>(0xC8));
    expected.push_back(0x04);
    require(bytes == expected, "jmp 0,x / jsr 4,u didn't produce the expected indexed-form bytes");
}

// `andcc 0xEF` / `orcc 0x10` - the IRQ mask/unmask primitives
// M6809Codegen's Intrinsic case uses for "ion"/"iof".
void testAndccOrcc() {
    std::string bytes = rcc::assembleM6809("andcc 0xEF\norcc 0x10\n");
    std::string expected;
    expected.push_back(0x1C);
    expected.push_back(static_cast<char>(0xEF));
    expected.push_back(0x1A);
    expected.push_back(0x10);
    require(bytes == expected, "andcc/orcc didn't produce the expected opcode+mask bytes");
}

// An undefined symbol becomes a relocation in object mode, rather than a
// hard assembly error the way plain assembleM6809() treats it.
void testObjectModeUndefinedSymbolBecomesRelocation() {
    rcc::ObjectFile obj = rcc::assembleM6809Object("ldx [fn_other]\n");
    require(obj.machineType == rcc::kM6809CoffMachine, "object's machineType should be kM6809CoffMachine");
    bool foundReloc = false;
    for (const auto &reloc : obj.relocations) {
        if (reloc.symbolName == "fn_other") {
            foundReloc = true;
            require(reloc.type == rcc::kM6809RelocAbs16, "relocation type should be kM6809RelocAbs16");
        }
    }
    require(foundReloc, "referencing an undefined symbol in object mode should produce a relocation for it");

    bool foundUndefinedSymbol = false;
    for (const auto &sym : obj.symbols) {
        if (sym.name == "fn_other") {
            foundUndefinedSymbol = true;
            require(!sym.isDefined, "the undefined symbol's own table entry should be marked isDefined=false");
        }
    }
    require(foundUndefinedSymbol, "an undefined relocation target should still get its own symbol-table entry");
}

// Plain (non-object-mode) assembly rejects an undefined symbol outright.
void testPlainModeUndefinedSymbolThrows() {
    bool threw = false;
    try {
        rcc::assembleM6809("ldx [fn_other]\n");
    } catch (const std::runtime_error &) {
        threw = true;
    }
    require(threw, "assembleM6809 should reject an undefined symbol, unlike assembleM6809Object");
}

}  // namespace

int main() {
    try {
        testLddImmediateAndStdExtended();
        testLeaxIndexed();
        testLeaxIndexedNegativeOffset();
        testPshsRegisterList();
        testMul();
        testTfr();
        testLbeqBackwardBranch();
        testAdcbAndSbcaExtended();
        testJmpJsrIndexed();
        testAndccOrcc();
        testObjectModeUndefinedSymbolBecomesRelocation();
        testPlainModeUndefinedSymbolThrows();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
