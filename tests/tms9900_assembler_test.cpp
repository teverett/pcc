// Byte-exact coverage of every one of the TMS9900's real 69 instructions, taken from its own
// opcode table (cross-checked against MAME's own tms9900.cpp decode table) - see
// src/codegen/tms9900/TMS9900Assembler.h's own header comment for the addressing-mode syntax
// every operand below uses.
#include "codegen/tms9900/TMS9900Assembler.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void expectBytes(const std::string &asmText, const std::vector<uint8_t> &want, const char *why) {
    std::string got;
    try {
        got = rcc::assembleTMS9900(asmText);
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << why << ": threw " << e.what() << '\n';
        failures++;
        return;
    }
    std::vector<uint8_t> gotBytes(got.begin(), got.end());
    if (gotBytes != want) {
        std::cerr << "FAIL " << why << ": got";
        for (uint8_t b : gotBytes) std::cerr << ' ' << std::hex << static_cast<int>(b);
        std::cerr << ", want";
        for (uint8_t b : want) std::cerr << ' ' << std::hex << static_cast<int>(b);
        std::cerr << '\n';
        failures++;
        return;
    }
    std::cout << "PASS " << why << '\n';
}

void expectThrows(const std::string &asmText, const char *why) {
    try {
        rcc::assembleTMS9900(asmText);
    } catch (const std::runtime_error &) {
        std::cout << "PASS " << why << '\n';
        return;
    }
    std::cerr << "FAIL " << why << ": expected a throw\n";
    failures++;
}

std::vector<uint8_t> w(uint16_t hi) { return {static_cast<uint8_t>(hi >> 8), static_cast<uint8_t>(hi & 0xFF)}; }
std::vector<uint8_t> concat(std::vector<uint8_t> a, const std::vector<uint8_t> &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// --- Format 1: every one of the 12 two-operand instructions, register-direct form ------
void testFormat1() {
    // SZC r1,r2: 0x4000 | (Td=00<<10)|(D=2<<6)|(Ts=00<<4)|S=1 = 0x4000|0x0080|0x0001 = 0x4081
    expectBytes("szc r1,r2\n", w(0x4081), "format1_szc_register");
    expectBytes("szcb r1,r2\n", w(0x5081), "format1_szcb_register");
    expectBytes("s r1,r2\n", w(0x6081), "format1_s_register");
    expectBytes("sb r1,r2\n", w(0x7081), "format1_sb_register");
    expectBytes("c r1,r2\n", w(0x8081), "format1_c_register");
    expectBytes("cb r1,r2\n", w(0x9081), "format1_cb_register");
    expectBytes("a r1,r2\n", w(0xA081), "format1_a_register");
    expectBytes("ab r1,r2\n", w(0xB081), "format1_ab_register");
    expectBytes("mov r1,r2\n", w(0xC081), "format1_mov_register");
    expectBytes("movb r1,r2\n", w(0xD081), "format1_movb_register");
    expectBytes("soc r1,r2\n", w(0xE081), "format1_soc_register");
    expectBytes("socb r1,r2\n", w(0xF081), "format1_socb_register");
}

// Every addressing mode, both as source and as destination, on a single representative
// instruction (MOV) - confirms classifyEA()'s own mode/register-field encoding and the
// source-before-destination extension-word order.
void testFormat1AddressingModes() {
    // MOV [r3],r0: source register-indirect (Ts=01,S=3), dest register (Td=00,D=0).
    // word = 0xC000 | (0<<10)|(0<<6)|(0b01<<4)|3 = 0xC013
    expectBytes("mov [r3],r0\n", w(0xC013), "format1_mov_src_regindirect");
    // MOV [ir5],r0: source register-indirect-autoincrement (Ts=11,S=5).
    // word = 0xC000 | (0b11<<4)|5 = 0xC035
    expectBytes("mov [ir5],r0\n", w(0xC035), "format1_mov_src_regindirect_autoinc");
    // MOV 1000,r0: source symbolic (Ts=10,S=0), one extension word (the address itself).
    // word = 0xC000 | (0b10<<4)|0 = 0xC020, extension = 1000 = 0x03E8
    expectBytes("mov [1000],r0\n", concat(w(0xC020), w(0x03E8)), "format1_mov_src_symbolic");
    // MOV [r3+1000],r0: source indexed (Ts=10,S=3), extension word = 1000.
    // word = 0xC000 | (0b10<<4)|3 = 0xC023
    expectBytes("mov [r3+1000],r0\n", concat(w(0xC023), w(0x03E8)), "format1_mov_src_indexed");
    // MOV r0,[1000]: dest symbolic (Td=10,D=0) - the extension word still follows the SOURCE's
    // own (none, here) and precedes nothing else, i.e. it's simply the one extension word.
    // word = 0xC000 | (0b10<<10)|(0<<6)|(0<<4)|0 = 0xC800, extension = 1000.
    expectBytes("mov r0,[1000]\n", concat(w(0xC800), w(0x03E8)), "format1_mov_dst_symbolic");
    // MOV [1000],[2000]: both operands need an extension word - source's word (1000) comes
    // before destination's (2000).
    // word = 0xC000 | (0b10<<10)|(0<<6)|(0b10<<4)|0 = 0xC820
    expectBytes("mov [1000],[2000]\n", concat(w(0xC820), concat(w(0x03E8), w(0x07D0))),
                "format1_mov_both_symbolic_src_then_dst_order");
}

// --- Format 2: jumps (PC-relative) and SBO/SBZ/TB (CRU-bit, plain signed displacement) ---
void testFormat2Jumps() {
    // "here: jmp here" - target is this very instruction's own address, so the branch must
    // step backward by one word (the PC has already advanced past this instruction):
    // disp = (here - (here+2))/2 = -1 -> low byte 0xFF.
    expectBytes("here: jmp here\n", w(0x10FF), "format2_jmp_self_backward");
    // A forward jump to the very next instruction (disp=0): "jmp there\nthere: mov r0,r0".
    expectBytes("jmp there\nthere: mov r0,r0\n", concat(w(0x1000), w(0xC000)), "format2_jmp_forward_zero_disp");
    static const std::pair<const char *, uint8_t> conditional[] = {
        {"jlt", 0x11}, {"jle", 0x12}, {"jeq", 0x13}, {"jhe", 0x14}, {"jgt", 0x15}, {"jne", 0x16},
        {"jnc", 0x17}, {"joc", 0x18}, {"jno", 0x19}, {"jl", 0x1A}, {"jh", 0x1B}, {"jop", 0x1C},
    };
    for (auto &[mnem, hi] : conditional) {
        std::string asmText = std::string("here: ") + mnem + " here\n";
        expectBytes(asmText, w(static_cast<uint16_t>((hi << 8) | 0xFF)), (std::string("format2_") + mnem).c_str());
    }
}
void testFormat2CruBit() {
    expectBytes("sbo 5\n", w(0x1D05), "format2_sbo_positive");
    expectBytes("sbz -1\n", w(0x1EFF), "format2_sbz_negative");
    expectBytes("tb 127\n", w(0x1F7F), "format2_tb_max");
}

// --- Format 3: COC/CZC/XOR, and XOP (its own distinct opcode, same field shape) ---------
void testFormat3() {
    // COC [r3],r2: word = 0x2000 | (2<<6)|(0b01<<4)|3 = 0x2093
    expectBytes("coc [r3],r2\n", w(0x2093), "format3_coc");
    expectBytes("czc [r3],r2\n", w(0x2493), "format3_czc");
    expectBytes("xor [r3],r2\n", w(0x2893), "format3_xor");
    // XOP [r3],5: word = 0x2C00 | (5<<6)|(0b01<<4)|3 = 0x2D53
    expectBytes("xop [r3],5\n", w(0x2D53), "format3_xop_immediate_vector");
    expectBytes("xop [r3],r5\n", w(0x2D53), "format3_xop_register_vector");
}

// --- Format 4: LDCR/STCR --------------------------------------------------------------
void testFormat4() {
    // LDCR [r3],8: word = 0x3000 | (8<<6)|(0b01<<4)|3 = 0x3213
    expectBytes("ldcr [r3],8\n", w(0x3213), "format4_ldcr");
    expectBytes("stcr [r3],8\n", w(0x3613), "format4_stcr");
    // Count 0 means 16 bits - a real, legal field value.
    expectBytes("ldcr [r3],0\n", w(0x3013), "format4_ldcr_count_zero_means_16");
}

// --- Format 9: MPY/DIV (same field shape as format 3) ---------------------------------
void testFormat9() {
    // MPY [r3],r2: word = 0x3800 | (2<<6)|(0b01<<4)|3 = 0x3893
    expectBytes("mpy [r3],r2\n", w(0x3893), "format9_mpy");
    expectBytes("div [r3],r2\n", w(0x3C93), "format9_div");
}

// --- Format 5: SRA/SRL/SLA/SRC - "register,count" --------------------------------------
void testFormat5() {
    // SRA r4,3: word = 0x0800 | (4<<4)|3 = 0x0843
    expectBytes("sra r4,3\n", w(0x0843), "format5_sra");
    expectBytes("srl r4,3\n", w(0x0943), "format5_srl");
    expectBytes("sla r4,3\n", w(0x0A43), "format5_sla");
    expectBytes("src r4,3\n", w(0x0B43), "format5_src");
    // Count 0 means "take the count from r0's own low nibble" - a real, legal field value.
    expectBytes("sra r4,0\n", w(0x0840), "format5_sra_count_zero_means_from_r0");
}

// --- Format 6: every single-operand instruction, register-direct and one other mode ----
void testFormat6() {
    static const std::pair<const char *, uint16_t> ops[] = {
        {"blwp", 0x0400}, {"b", 0x0440},   {"x", 0x0480},    {"clr", 0x04C0}, {"neg", 0x0500}, {"inv", 0x0540},
        {"inc", 0x0580},  {"inct", 0x05C0}, {"dec", 0x0600},  {"dect", 0x0640}, {"bl", 0x0680},  {"swpb", 0x06C0},
        {"seto", 0x0700}, {"abs", 0x0740},
    };
    for (auto &[mnem, base] : ops) {
        // Register-direct r5: word = base | 5.
        expectBytes(std::string(mnem) + " r5\n", w(static_cast<uint16_t>(base | 5)),
                    (std::string("format6_") + mnem + "_register").c_str());
    }
    // One representative addressing-mode variation beyond register-direct, confirmed with its
    // own extension word: B [label] (the real long-range unconditional branch).
    expectBytes("b [4096]\n", concat(w(0x0460), w(0x1000)), "format6_b_symbolic_with_extension_word");
    // BL [label],  the real subroutine-call instruction: R11 := return address, PC := label.
    expectBytes("bl [4096]\n", concat(w(0x06A0), w(0x1000)), "format6_bl_symbolic_with_extension_word");
}

// --- Format 7: control instructions, no operand ----------------------------------------
void testFormat7() {
    expectBytes("idle\n", w(0x0340), "format7_idle");
    expectBytes("rset\n", w(0x0360), "format7_rset");
    expectBytes("rtwp\n", w(0x0380), "format7_rtwp");
    expectBytes("ckon\n", w(0x03A0), "format7_ckon");
    expectBytes("ckof\n", w(0x03C0), "format7_ckof");
    expectBytes("lrex\n", w(0x03E0), "format7_lrex");
    expectThrows("idle r0\n", "format7_rejects_an_operand");
}

// --- Format 8: immediate and register-only instructions ---------------------------------
void testFormat8() {
    // LI r3,1234: word = 0x0200|3, extension = 1234 = 0x04D2.
    expectBytes("li r3,1234\n", concat(w(0x0203), w(0x04D2)), "format8_li");
    expectBytes("ai r3,1234\n", concat(w(0x0223), w(0x04D2)), "format8_ai");
    expectBytes("andi r3,1234\n", concat(w(0x0243), w(0x04D2)), "format8_andi");
    expectBytes("ori r3,1234\n", concat(w(0x0263), w(0x04D2)), "format8_ori");
    expectBytes("ci r3,1234\n", concat(w(0x0283), w(0x04D2)), "format8_ci");
    expectBytes("stwp r7\n", w(0x02A7), "format8_stwp");
    expectBytes("stst r7\n", w(0x02C7), "format8_stst");
    expectBytes("lwpi 32768\n", concat(w(0x02E0), w(0x8000)), "format8_lwpi");
    expectBytes("limi 2\n", concat(w(0x0300), w(0x0002)), "format8_limi");
}

// --- Rejections: addressing modes/operand shapes this dialect should refuse -------------
void testRejections() {
    expectThrows("mov 5,r0\n", "rejects_bare_immediate_as_two_operand_source");
    expectThrows("clr 5\n", "format6_rejects_bare_immediate");
    expectThrows("sra 5,3\n", "format5_rejects_non_register_first_operand");
    expectThrows("xor [r3],5\n", "format3_xor_rejects_non_register_destination");
    expectThrows("jmp 70000\n", "format2_rejects_out_of_range_displacement");
    expectThrows("foo r0,r1\n", "rejects_unknown_mnemonic");
}

}  // namespace

int main() {
    testFormat1();
    testFormat1AddressingModes();
    testFormat2Jumps();
    testFormat2CruBit();
    testFormat3();
    testFormat4();
    testFormat9();
    testFormat5();
    testFormat6();
    testFormat7();
    testFormat8();
    testRejections();
    if (failures > 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    return 0;
}
