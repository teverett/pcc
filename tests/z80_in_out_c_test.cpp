// Checks Z80Assembler.cpp's ED-prefixed IN r,(C)/OUT (C),r encodings against the real Z80
// opcode table (every documented register, b/c/d/e/h/l/a) - added for src/rt/cpc/'s PSG/
// keyboard access, which needs the port-on-BC form the immediate-port IN A,[n]/OUT [n],A
// instructions can't express.
#include "codegen/z80/Z80Assembler.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}

void testInRC() {
    // IN r,(C): ED 40 | (r << 3), r = b=0,c=1,d=2,e=3,h=4,l=5,a=7 (6 is an undocumented,
    // flags-only placeholder this backend deliberately doesn't encode).
    struct {
        const char *reg;
        uint8_t code;
    } cases[] = {
        {"b", 0x40}, {"c", 0x48}, {"d", 0x50}, {"e", 0x58}, {"h", 0x60}, {"l", 0x68}, {"a", 0x78},
    };
    for (const auto &c : cases) {
        std::string bytes = rcc::assembleZ80(std::string("in ") + c.reg + ", [c]\n");
        require(bytes.size() == 2, "IN r,(C) should assemble to exactly 2 bytes");
        require(static_cast<uint8_t>(bytes[0]) == 0xED, "IN r,(C) should start with the ED prefix");
        require(static_cast<uint8_t>(bytes[1]) == c.code,
                (std::string("IN ") + c.reg + ",(C) encoded the wrong opcode byte").c_str());
    }
}

void testOutCR() {
    // OUT (C),r: ED 41 | (r << 3), same register numbering as IN r,(C) above.
    struct {
        const char *reg;
        uint8_t code;
    } cases[] = {
        {"b", 0x41}, {"c", 0x49}, {"d", 0x51}, {"e", 0x59}, {"h", 0x61}, {"l", 0x69}, {"a", 0x79},
    };
    for (const auto &c : cases) {
        std::string bytes = rcc::assembleZ80(std::string("out [c], ") + c.reg + "\n");
        require(bytes.size() == 2, "OUT (C),r should assemble to exactly 2 bytes");
        require(static_cast<uint8_t>(bytes[0]) == 0xED, "OUT (C),r should start with the ED prefix");
        require(static_cast<uint8_t>(bytes[1]) == c.code,
                (std::string("OUT (C),") + c.reg + " encoded the wrong opcode byte").c_str());
    }
}

// The pre-existing immediate-port forms must still work unchanged once the register-indirect
// forms share the same "in"/"out" mnemonics.
void testImmediatePortFormsStillWork() {
    std::string inBytes = rcc::assembleZ80("in a, [254]\n");
    require(inBytes.size() == 2 && static_cast<uint8_t>(inBytes[0]) == 0xDB &&
                static_cast<uint8_t>(inBytes[1]) == 254,
            "IN A,(n) regressed");
    std::string outBytes = rcc::assembleZ80("out [254], a\n");
    require(outBytes.size() == 2 && static_cast<uint8_t>(outBytes[0]) == 0xD3 &&
                static_cast<uint8_t>(outBytes[1]) == 254,
            "OUT (n),A regressed");
}

void rejects(const std::string &asmText, const char *why) {
    try {
        rcc::assembleZ80(asmText);
    } catch (const std::runtime_error &) {
        return;
    }
    throw std::runtime_error(std::string("should have rejected: ") + why);
}

void testRejections() {
    rejects("in m, [c]\n", "the undocumented r=6 placeholder register");
    rejects("in a, [bc]\n", "a bracketed register other than c for the port");
}

}  // namespace

int main() {
    try {
        testInRC();
        testOutCR();
        testImmediatePortFormsStillWork();
        testRejections();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
