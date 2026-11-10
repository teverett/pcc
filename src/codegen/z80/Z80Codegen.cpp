#include "Z80Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Z80Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Load/entry address for a whole-program raw binary; must equal
// Z80Assembler.h's kZ80BaseAddress.
constexpr uint32_t kBaseAddress = 0x8000;
// SP is undefined at reset; CALL/RET/PUSH/POP need a real initial value.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void Z80Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's decimal text, or a symbol element's (dispatch-table
// address-of-function/global) label text.
std::string Z80Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType` picks the directive: `db` for char, `dw` for short/int, or
// 2/4 `dw`s (LSW first) for long/long long. A symbol element is always a
// single native-word address, regardless of `elemType`.
void Z80Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    const char *directive = w == 1 ? "db" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        int words = w <= 2 ? 1 : w / 2;
        if (words > 1 && !e.isSymbol) {
            auto uv = static_cast<unsigned long>(e.value);
            for (int i = 0; i < words; i++) {
                if (!first) line << ", ";
                line << static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
                first = false;
            }
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
}

void Z80Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ld hl, [" << src << "]\n    ld [" << dst << "], hl\n";
}

void Z80Codegen::loadPair(const std::string &pairReg, const std::string &src) {
    code_ << "    ld " << pairReg << ", [" << src << "]\n";
}

void Z80Codegen::storePair(const std::string &dst, const std::string &pairReg) {
    code_ << "    ld [" << dst << "], " << pairReg << "\n";
}

// No direct register move exists between HL and IX, only via SP; `ix`
// itself is left untouched, so this is safe to call mid-expression.
void Z80Codegen::loadFrameAddr(const ir::Slot *slot, long extraBytes) {
    code_ << "    push ix\n    pop hl\n";
    // Param bytes sit at increasing addresses above `ix`; Local bytes grow
    // down from `ix`, at decreasing addresses as `extraBytes` increases,
    // except a Local array's, which ascend from element 0, its lowest
    // address (see ir::localLowOffset()).
    long off = slot->kind == ir::SlotKind::Param ? (static_cast<long>(slot->frameOffset) + extraBytes)
               : slot->arraySize > 0 ? (-static_cast<long>(ir::localLowOffset(*slot, typeWidths())) + extraBytes)
                                     : (-(static_cast<long>(ir::localOffsetDescendingWide(*slot, typeWidths())) + extraBytes));
    uint16_t off16 = static_cast<uint16_t>(off);
    code_ << "    ld de, " << off16 << "\n    add hl, de\n";
}

void Z80Codegen::readWordAt(const std::string &loReg, const std::string &hiReg) {
    code_ << "    ld " << loReg << ", m\n    inc hl\n    ld " << hiReg << ", m\n";
}

// A Local array's initializer values are stored into this call's own frame
// at the point of declaration, rather than static trailer data like a
// Global array's.
void Z80Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        long base = static_cast<long>(i) * w;
        if (w == 1 && !elem.isSymbol) {
            loadFrameAddr(slot, base);
            code_ << "    ld a, " << static_cast<int>(static_cast<uint8_t>(elem.value)) << "\n    ld m, a\n";
            continue;
        }
        if (elem.isSymbol || w <= 2) {
            loadFrameAddr(slot, base);
            code_ << "    ld de, " << arrayElemText(elem) << "\n";
            code_ << "    ld m, e\n    inc hl\n    ld m, d\n";
            continue;
        }
        // A wide element: 2 or 4 words, least significant first.
        auto uv = static_cast<unsigned long>(elem.value);
        int words = w / 2;
        for (int j = 0; j < words; j++) {
            loadFrameAddr(slot, base + 2 * j);
            uint16_t wv = static_cast<uint16_t>((uv >> (16 * j)) & 0xFFFF);
            code_ << "    ld de, " << wv << "\n";
            code_ << "    ld m, e\n    inc hl\n    ld m, d\n";
        }
    }
}

void Z80Codegen::storeAcIndirect() {
    // Loads ac's two bytes by name (not through HL) and stores each via (HL), so HL survives.
    code_ << "    ld a, [ac]\n    ld m, a\n";
    code_ << "    inc hl\n";
    code_ << "    ld a, [ac+1]\n    ld m, a\n";
}

// RL/RR target L/H directly, so this needs only two instructions vs. the
// 8080's six (RAL/RAR only touch the accumulator).
void Z80Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ld hl, [" << lbl << "]\n";
    code_ << "    or a\n";
    if (isLeft) {
        code_ << "    rl l\n    rl h\n";
    } else {
        code_ << "    rr h\n    rr l\n";
    }
    code_ << "    ld [" << lbl << "], hl\n";
}

int Z80Codegen::widthOfType(Type t) const { return widthOf(t, typeWidths()); }
bool Z80Codegen::isWide(Type t) const { return widthOfType(t) > 2; }
int Z80Codegen::wordCountOf(Type t) const { return widthOfType(t) / 2; }
int Z80Codegen::elemShift(Type t) const {
    switch (widthOfType(t)) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        default: return 3;  // 8
    }
}

// HL holds the operand's address. `rla` rotates the loaded byte (in L)
// left through carry, leaving carry = the byte's sign bit; `sbc a,a`
// (a - a - carry) then yields exactly 0x00 or 0xFF - the sign-extension
// byte in one branch-free move. An unsigned char just zero-fills H.
void Z80Codegen::loadOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ld a, m\n    ld l, a\n";
        if (type.isUnsigned) {
            code_ << "    ld h, 0\n";
        } else {
            code_ << "    rla\n    sbc a, a\n    ld h, a\n";
        }
        code_ << "    ld [ac], hl\n";
        return;
    }
    readWordAt("e", "d");
    storePair("ac", "de");
}

void Z80Codegen::storeOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ld a, [ac]\n    ld m, a\n";
        return;
    }
    storeAcIndirect();
}

void Z80Codegen::popWideInto(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop hl\n    ld [" << wideScratch(role, i) << "], hl\n";
    }
}

void Z80Codegen::pushWideFrom(const char *role, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ld hl, [" << wideScratch(role, i) << "]\n    push hl\n";
    }
}

void Z80Codegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ld hl, " << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "\n";
        } else {
            loadFrameAddr(slot, 2 * i);
        }
        code_ << "    ld c, m\n    inc hl\n    ld b, m\n";
        code_ << "    push bc\n";
    }
}

void Z80Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        uint16_t w = static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    ld hl, " << w << "\n    push hl\n";
    }
}

void Z80Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop bc\n";
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ld hl, " << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "\n";
        } else {
            loadFrameAddr(slot, 2 * i);
        }
        code_ << "    ld m, c\n    inc hl\n    ld m, b\n";
    }
}

// Word 0 is a plain ADD/SBC HL,DE; later words use ADC/SBC HL,DE, which
// already read the real carry flag, so no separate carry-propagation step
// is needed. Operands are popped into "wa"/"wb" scratch cells first since
// there's no SP-relative addressing to operate on the stack in place.
void Z80Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld de, [" << wideScratch("b", i) << "]\n";
        if (i == 0) {
            code_ << "    " << (isAdd ? "add hl, de\n" : "or a\n    sbc hl, de\n");
        } else {
            code_ << "    " << (isAdd ? "adc hl, de\n" : "sbc hl, de\n");
        }
        code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
    }
    pushWideFrom("a", words);
}

// Two's-complement negate: complement every word, then add 1 into the
// least significant word and let ADC HL,DE (DE=0 above word 0) carry it
// up through the rest.
void Z80Codegen::genWideNegate(int words) {
    popWideInto("a", words);
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld a, l\n    cpl\n    ld l, a\n";
        code_ << "    ld a, h\n    cpl\n    ld h, a\n";
        code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
    }
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld de, " << (i == 0 ? 1 : 0) << "\n";
        code_ << "    " << (i == 0 ? "add hl, de\n" : "adc hl, de\n");
        code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
    }
    pushWideFrom("a", words);
}

void Z80Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    const char *insn = op == "&" ? "and" : op == "|" ? "or" : "xor";
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld de, [" << wideScratch("b", i) << "]\n";
        code_ << "    ld a, l\n    " << insn << " e\n    ld l, a\n";
        code_ << "    ld a, h\n    " << insn << " d\n    ld h, a\n";
        code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
    }
    pushWideFrom("a", words);
}

void Z80Codegen::genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    code_ << "    ld hl, 0\n    ld [wacc], hl\n";
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld de, [" << wideScratch("b", i) << "]\n";
        code_ << "    ld a, l\n    xor e\n    ld l, a\n";
        code_ << "    ld a, h\n    xor d\n    ld h, a\n";
        code_ << "    ld de, [wacc]\n";
        code_ << "    ld a, l\n    or e\n    ld l, a\n";
        code_ << "    ld a, h\n    or d\n    ld h, a\n";
        code_ << "    ld [wacc], hl\n";
    }
    code_ << "    ld hl, [wacc]\n    ld a, l\n    or h\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (wantEqual ? "z" : "nz") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

// Same sign-bias trick as emitLessCompare(), compared most-significant word
// first - only that word needs the signed/unsigned distinction, since once
// two words differ every lower word is a pure unsigned magnitude.
void Z80Codegen::genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words) {
    genExpr(a);
    genExpr(b);
    popWideInto("b", words);
    popWideInto("a", words);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    ld de, [" << wideScratch("b", i) << "]\n";
        if (i == words - 1 && !isUnsigned) {
            code_ << "    ld a, h\n    xor 0x80\n    ld h, a\n";
            code_ << "    ld a, d\n    xor 0x80\n    ld d, a\n";
        }
        code_ << "    or a\n    sbc hl, de\n";
        if (i == 0) {
            code_ << "    jp " << (invert ? "nc" : "c") << ", " << Ltrue << "\n";
            code_ << "    jp " << Lfalse << "\n";
        } else {
            std::string Lcontinue = newLabel();
            code_ << "    jp z, " << Lcontinue << "\n";
            code_ << "    jp " << (invert ? "nc" : "c") << ", " << Ltrue << "\n";
            code_ << "    jp " << Lfalse << "\n";
            code_ << Lcontinue << ":\n";
        }
    }
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                  int words) {
    if (op == "==") return genWideEqualityCompare(lhs, rhs, /*wantEqual=*/true, words);
    if (op == "!=") return genWideEqualityCompare(lhs, rhs, /*wantEqual=*/false, words);
    if (op == "<") return genWideLessCompare(lhs, rhs, /*invert=*/false, isUnsigned, words);
    if (op == ">") return genWideLessCompare(rhs, lhs, /*invert=*/false, isUnsigned, words);
    if (op == "<=") return genWideLessCompare(rhs, lhs, /*invert=*/true, isUnsigned, words);
    if (op == ">=") return genWideLessCompare(lhs, rhs, /*invert=*/true, isUnsigned, words);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// A long/long long shifted by a runtime count, looped one bit at a time
// across every byte, most-significant byte first. No variable-count shift
// exists even for a single word on this chip (see genShift()).
void Z80Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);
    popWideInto("a", words);
    genExpr(rhs);
    code_ << "    ld a, [ac]\n    ld b, a\n";
    std::vector<std::string> bytesMsbFirst;  // high byte of the top word .. low byte of word 0
    for (int i = words - 1; i >= 0; i--) {
        bytesMsbFirst.push_back(wideScratch("a", i) + "+1");
        bytesMsbFirst.push_back(wideScratch("a", i));
    }
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    if (isLeft) {
        code_ << "    or a\n";  // clear carry - '<<' always shifts in a 0 bit, regardless of signedness
        for (size_t idx = bytesMsbFirst.size(); idx-- > 0;) {
            code_ << "    ld a, [" << bytesMsbFirst[idx] << "]\n    rl a\n    ld [" << bytesMsbFirst[idx] << "], a\n";
        }
    } else {
        for (size_t idx = 0; idx < bytesMsbFirst.size(); idx++) {
            // The first byte is the value's top byte: sra/srl read its own
            // top bit directly (signed keeps it, unsigned zero-fills), no
            // "clear carry" step needed like '<<'. Every byte after rotates
            // in the carry that just set.
            const char *instr = idx == 0 ? (isUnsigned ? "srl" : "sra") : "rr";
            code_ << "    ld a, [" << bytesMsbFirst[idx] << "]\n    " << instr << " a\n    ld ["
                  << bytesMsbFirst[idx] << "], a\n";
        }
    }
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    pushWideFrom("a", words);
}

// A long/long long multiply/divide needs a bit-serial multi-word routine,
// and this target has no simulator to verify a hand-written one against -
// failing loudly here is safer than shipping an unverifiable one.
void Z80Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                   "see Z80Codegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void Z80Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                   "see Z80Codegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

// Every arithmetic operand narrower than int is already promoted by
// IRBuilder, so the real work is crossing the native-word/wide boundary
// and re-narrowing a char result.
void Z80Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // ac
        if (widthOfType(to) == 1) {
            code_ << "    ld hl, [ac]\n";
            if (to.isUnsigned) {
                code_ << "    ld h, 0\n";
            } else {
                code_ << "    ld a, l\n    rla\n    sbc a, a\n    ld h, a\n";
            }
            code_ << "    ld [ac], hl\n";
        }
        return;
    }
    if (fromWide && !toWide) {
        // No SP-relative addressing, so every popped word must land
        // somewhere real first; only the least significant one is kept.
        int words = wordCountOf(from);
        genExpr(*e.lhs);
        popWideInto("a", words);
        code_ << "    ld hl, [" << wideScratch("a", 0) << "]\n    ld [ac], hl\n";
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ld h, 0\n";
            } else {
                code_ << "    ld a, l\n    rla\n    sbc a, a\n    ld h, a\n";
            }
            code_ << "    ld [ac], hl\n";
        }
        return;
    }
    if (!fromWide && toWide) {
        // Every extension word must land below word 0 (LSW stays on top),
        // so all words are written into "wa" first and pushed via
        // pushWideFrom(), rather than pushing word 0 directly.
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac already holds the native-width value, correctly sign/zero-extended to 16 bits
        code_ << "    ld hl, [ac]\n    ld [" << wideScratch("a", 0) << "], hl\n";
        for (int i = 1; i < toWords; i++) {
            if (from.isUnsigned) {
                code_ << "    ld hl, 0\n";
            } else {
                code_ << "    ld a, h\n    rla\n    sbc a, a\n    ld h, a\n    ld l, a\n";
            }
            code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
        }
        pushWideFrom("a", toWords);
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign. Words
    // are popped into "wa"; any new sign-extension words are computed from
    // the old most-significant word, then exactly toWords are pushed back.
    int fromWords = wordCountOf(from);
    int toWords = wordCountOf(to);
    genExpr(*e.lhs);
    popWideInto("a", fromWords);
    if (toWords > fromWords) {
        code_ << "    ld hl, [" << wideScratch("a", fromWords - 1) << "]\n";
        if (from.isUnsigned) {
            code_ << "    ld hl, 0\n";
        } else {
            code_ << "    ld a, h\n    rla\n    sbc a, a\n    ld h, a\n    ld l, a\n";
        }
        for (int i = fromWords; i < toWords; i++) code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
    }
    pushWideFrom("a", toWords);
}

std::string Z80Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation means `prog` may not define `main` at all.
    // In `objectMode`, no object file emits this entry sequence, even one
    // that defines `main` - Z80Linker.cpp synthesizes exactly one once
    // every object is merged. An object file's own base is `org 0`;
    // placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ld sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's result sits in `ac` once execution reaches `halt` -
        // this extra load also leaves it in HL for a quick register-dump look.
        code_ << "    ld hl, [ac]\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // The fixed pseudo-registers are shared across the whole linked
    // program, not redeclared per-object: in whole-program mode they're
    // declared once, here; in object mode this file never declares them,
    // so every reference becomes a relocation resolved against
    // Z80Linker.cpp's synthesized entry object.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("_bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // Wide (long/long long) scratch cells.
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("ret", i));
        declareTemp("wacc");
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; it must never get
        // local storage here, only be referenced by name so the assembler
        // defers it as a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 1) {
                trailer_.push_back(lbl + ": db " + std::to_string(static_cast<int>(static_cast<uint8_t>(v))) +
                                    "  ; " + slot->sourceName);
            } else if (w == 2) {
                uint16_t v16 = static_cast<uint16_t>(v);
                trailer_.push_back(lbl + ": db " + std::to_string(v16 & 0xFF) + "  ; " + slot->sourceName);
                trailer_.push_back("db " + std::to_string((v16 >> 8) & 0xFF));
            } else {
                auto uv = static_cast<unsigned long>(v);
                std::ostringstream line;
                line << lbl << ": dw ";
                for (int i = 0; i < w / 2; i++) {
                    if (i > 0) line << ", ";
                    line << static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
                }
                line << "  ; " << slot->sourceName;
                trailer_.push_back(line.str());
            }
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - the whole generated text is attributed to whichever
// file defines `main`.
std::map<std::string, std::string> Z80Codegen::perFileAsm() const { return perFileAsm_; }

// Every call gets its own real frame, built around `ix`. On entry SP
// points at the return address CALL just pushed; `push ix` saves the
// caller's frame pointer, then `ix` is set to that SP (no direct `ld
// ix,sp` exists, so this zeroes ix and adds SP in). From `ix`: `ix+0` is
// the saved caller ix, `ix+2` the return address, `ix+4+2*i` argument i
// (pushed by the caller in reverse order, so argument 0 is closest to the
// return address). `frameReserveBytes_` single-byte `dec sp`s then carve
// out this function's Locals below `ix` (no instruction moves a computed
// value into SP directly). Every Return undoes this with matching `inc
// sp`s before restoring `ix` and returning.
void Z80Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push ix\n";
    code_ << "    ld ix, 0\n";
    code_ << "    add ix, sp\n";
    // `frameSize` is rounded up to the native int width as a whole, which
    // can fall up to `intUnits-1` bytes short of the deepest byte an
    // individual Local actually needs (e.g. an int then two trailing chars
    // needs frameOffset 5 but frameSize 4) - padding by `intUnits-1` always
    // covers that worst case, at the cost of some extra stack space.
    frameReserveBytes_ = fn.func->frameSize + (typeWidths().intUnits - 1);
    for (int i = 0; i < frameReserveBytes_; i++) code_ << "    dec sp\n";

    genStmt(*fn.body);

    for (int i = 0; i < frameReserveBytes_; i++) code_ << "    inc sp\n";
    code_ << "    pop ix\n";
    code_ << "    ret\n\n";
}

void Z80Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testTruthy(*s.cond);
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jp z, " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jp " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testTruthy(*s.cond);
            code_ << "    jp z, " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                // A wide value sits on the real stack (LSW on top) after
                // genExpr() - pop it into "wret" first since the stack
                // unwind below is about to move SP past it. genExpr()'s own
                // Call case reads it back out of "wret".
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) {
                        code_ << "    pop hl\n    ld [" << wideScratch("ret", i) << "], hl\n";
                    }
                }
            }
            for (int i = 0; i < frameReserveBytes_; i++) code_ << "    inc sp\n";
            code_ << "    pop ix\n";
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                if (isWide(s.declSlot->type)) {
                    genExpr(*s.initOrReturnOrExpr);  // pushes `wordCountOf()` words
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                    return;
                }
                // The target address must survive evaluating the
                // initializer, which might itself index an array and reuse
                // `ptr` - so it's saved on the real stack instead.
                loadFrameAddr(s.declSlot);
                code_ << "    push hl\n";
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    pop hl\n";
                storeOperand(s.declSlot->type);
            }
            return;
        }
        case ir::StmtKind::ExprStmt: {
            genExpr(*s.initOrReturnOrExpr);
            return;
        }
        case ir::StmtKind::Empty:
            return;
        case ir::StmtKind::Asm: {
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "    jp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void Z80Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ld hl, " << v16 << "\n    ld [ac], hl\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    ld hl, " << label(e.slot) << "\n";
            } else {
                loadFrameAddr(e.slot);
            }
            loadOperand(e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                if (isWide(e.slot->type)) {
                    int words = wordCountOf(e.slot->type);
                    // `ptr` is shared scratch - evaluating rhs might itself
                    // index another array, clobbering it - so the address is
                    // saved on the real stack across that call; rhs's pushed
                    // words land on top, so they're popped into "wb" before
                    // the address underneath is restored.
                    code_ << "    ld hl, [ptr]\n    push hl\n";
                    genExpr(*e.rhs);
                    popWideInto("b", words);
                    code_ << "    pop hl\n    ld [ptr], hl\n";
                    genWideStoreAtPtr("b", words);
                    return;
                }
                code_ << "    ld hl, [ptr]\n    push hl\n";
                genExpr(*e.rhs);
                code_ << "    pop hl\n";
                storeOperand(e.slot->type);
                return;
            }
            if (isWide(e.slot->type)) {
                genExpr(*e.rhs);  // pushes `wordCountOf()` words
                genWideStore(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                genExpr(*e.rhs);
                code_ << "    ld hl, " << label(e.slot) << "\n";
                storeOperand(e.slot->type);
                return;
            }
            // `ptr` isn't safe to park the address in across evaluating
            // `rhs` (rhs might reuse `ptr` itself indexing an array) - the
            // real stack is, same as the indexed-Assign case above.
            loadFrameAddr(e.slot);
            code_ << "    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeOperand(e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = address
            } else if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    ld hl, " << label(e.slot) << "\n";
                code_ << "    ld [ptr], hl\n";
            } else {
                loadFrameAddr(e.slot);
                code_ << "    ld [ptr], hl\n";
            }
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jp z, " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores through e.type's own width - the pointee type, or
            // plain native-word width for untyped-int-as-address. A wide
            // pointee mirrors the indexed Assign's wide branch above.
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            if (isWide(e.type)) {
                int words = wordCountOf(e.type);
                code_ << "    ld hl, [ptr]\n    push hl\n";
                genExpr(*e.rhs);
                popWideInto("b", words);
                code_ << "    pop hl\n    ld [ptr], hl\n";
                genWideStoreAtPtr("b", words);
                return;
            }
            code_ << "    ld hl, [ptr]\n    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeOperand(e.type);
            return;
        }
        case ir::ExprKind::Unary: {
            bool wide = isWide(e.rhs->type);
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (wide) {
                    genWideNegate(wordCountOf(e.rhs->type));
                    return;
                }
                code_ << "    ld hl, [ac]\n";
                code_ << "    xor a\n    sub l\n    ld l, a\n";
                code_ << "    ld a, 0\n    sbc a, h\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's own width - see DerefAssign's
                // identical comment above.
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                if (isWide(e.type)) {
                    genWideLoadAtPtr(wordCountOf(e.type));
                    return;
                }
                code_ << "    ld hl, [ptr]\n";
                loadOperand(e.type);
            } else if (e.op == "~") {
                if (wide) {
                    genExpr(*e.rhs);
                    int words = wordCountOf(e.rhs->type);
                    popWideInto("a", words);
                    for (int i = 0; i < words; i++) {
                        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
                        code_ << "    ld a, l\n    cpl\n    ld l, a\n";
                        code_ << "    ld a, h\n    cpl\n    ld h, a\n";
                        code_ << "    ld [" << wideScratch("a", i) << "], hl\n";
                    }
                    pushWideFrom("a", words);
                    return;
                }
                genExpr(*e.rhs);
                code_ << "    ld hl, [ac]\n";
                code_ << "    ld a, l\n    cpl\n    ld l, a\n";
                code_ << "    ld a, h\n    cpl\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed right-to-left so argument 0 ends up
            // closest to the return address (see genFunction()). The callee
            // never cleans these up (no `ret n` on this chip), so the caller
            // pops them back off after the call returns.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    totalWords += wordCountOf(e.args[i]->type);
                } else {
                    code_ << "    ld hl, [ac]\n    push hl\n";
                    totalWords += 1;
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            for (int i = 0; i < totalWords; i++) code_ << "    pop hl\n";
            if (isWide(e.type)) {
                // The callee left its wide return value in "wret" - push it
                // back as this expression's result, MSW first so LSW ends up on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    ld hl, [" << wideScratch("ret", i) << "]\n    push hl\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // No indirect-CALL instruction exists, only `JP (HL)` ("jp
            // [hl]"), identical to the 8080's PCHL. A real CALL is emulated
            // by pushing the return address, then jumping through HL.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            code_ << "    ld hl, [ac]\n";  // HL = callee's address - jp [hl]'s own implicit target
            code_ << "    ld de, " << Lret << "\n";
            code_ << "    push de\n";
            code_ << "    jp [hl]\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real interrupt enable/disable, same mnemonics as the 8080's EI/DI.
            code_ << "    " << (e.op == "ion" ? "ei" : "di") << "\n";
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// Leaves slot[idxExpr]'s effective address in `ptr`. `idxExpr` is always
// evaluated first, alone. `slot->arraySize > 0` means the base is a
// compile-time label; otherwise `slot` is a pointer variable and the base
// is read from its stored value via a single real instruction, unlike the
// 8080's stack round-trip.
void Z80Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    // elemShift() already scales by the element's full width, so this is
    // valid for a wide element too; only callers reading/writing through
    // `ptr` need their own wide-aware path.
    genExpr(idxExpr);  // ac = index
    // HL = index * widthOfType(slot->type): ADD HL,HL doubles HL in one
    // instruction, repeated elemShift() times. Stashed in `ptr` since
    // loadFrameAddr() below needs HL/DE itself.
    code_ << "    ld hl, [ac]\n";
    int shift = elemShift(slot->type);
    for (int i = 0; i < shift; i++) code_ << "    add hl, hl\n";
    code_ << "    ld [ptr], hl\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ld de, " << label(slot) << "\n";
            code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
        } else {
            loadFrameAddr(slot);               // hl = array's element 0, its lowest address
            code_ << "    ld de, [ptr]\n";     // de = scaled index, saved above
            code_ << "    add hl, de\n";
            code_ << "    ld [ptr], hl\n";
        }
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        loadPair("de", label(slot));
    } else {
        loadFrameAddr(slot);
        readWordAt("e", "d");
    }
    code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
}

// `ptr` already holds the element's address. Reads `words` 16-bit words,
// most significant first, pushing each (LSW ends up on top).
void Z80Codegen::genWideLoadAtPtr(int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ld hl, [ptr]\n";
        if (i > 0) code_ << "    ld bc, " << (2 * i) << "\n    add hl, bc\n";
        code_ << "    ld c, m\n    inc hl\n    ld b, m\n";
        code_ << "    push bc\n";
    }
}

// Mirror image for a write. DE (not BC) carries the address offset here,
// since BC holds the value being stored.
void Z80Codegen::genWideStoreAtPtr(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [ptr]\n";
        if (i > 0) code_ << "    ld de, " << (2 * i) << "\n    add hl, de\n";
        code_ << "    ld bc, [" << wideScratch(role, i) << "]\n";
        code_ << "    ld m, c\n    inc hl\n    ld m, b\n";
    }
}

void Z80Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);  // ptr = element address
    if (isWide(e.slot->type)) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    code_ << "    ld hl, [ptr]\n";
    loadOperand(e.slot->type);
}

// Entry: HL = old value, DE = rhs value (both already at `type`'s
// width/signedness). `+`/`-`/`&`/`|`/`^` reuse genBinary()'s own HL/DE
// tail; `*`/`/`/`%`/`<<`/`>>` seed `_bc`/`dx` (or `_bc`/`b`) and reuse
// genMul()'s/genDivMod()'s/genShift()'s own loop bodies.
void Z80Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add hl, de\n    ld [ac], hl\n"; return; }
    if (op == "-") { code_ << "    or a\n    sbc hl, de\n    ld [ac], hl\n"; return; }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    ld a, l\n    " << insn << " e\n    ld l, a\n";
        code_ << "    ld a, h\n    " << insn << " d\n    ld h, a\n";
        code_ << "    ld [ac], hl\n";
        return;
    }
    if (op == "*") {
        code_ << "    ld [_bc], hl\n";  // bc = old (multiplicand)
        code_ << "    ld [dx], de\n";    // dx = rhs (multiplier)
        code_ << "    ld hl, 0\n    ld [ac], hl\n";
        code_ << "    ld b, 16\n";
        std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    ld a, b\n    or a\n";
        code_ << "    jp z, " << Ldone << "\n";
        code_ << "    ld a, [dx]\n    and 1\n";
        code_ << "    jp z, " << Lskipadd << "\n";
        code_ << "    ld hl, [ac]\n    ld de, [_bc]\n    add hl, de\n    ld [ac], hl\n";
        code_ << Lskipadd << ":\n";
        shiftOneBit("_bc", /*isLeft=*/true);
        shiftOneBit("dx", /*isLeft=*/false);
        code_ << "    dec b\n";
        code_ << "    jp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        code_ << "    ld [_bc], hl\n";  // bc = old (value)
        code_ << "    ld a, e\n    ld b, a\n";  // b = shift count (low byte of rhs)
        std::string Lloop = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    ld a, b\n    or a\n";
        code_ << "    jp z, " << Ldone << "\n";
        if (op == "<<" || isUnsigned) {
            shiftOneBit("_bc", /*isLeft=*/op == "<<");
        } else {
            code_ << "    ld hl, [_bc]\n";
            code_ << "    ld a, h\n    sra a\n    ld h, a\n";
            code_ << "    rr l\n";
            code_ << "    ld [_bc], hl\n";
        }
        code_ << "    dec b\n";
        code_ << "    jp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        copy16("ac", "_bc");
        return;
    }
    // "/" or "%"
    code_ << "    ld [dx], hl\n";  // dx = old (dividend)
    code_ << "    ld [_bc], de\n";  // bc = rhs (divisor)
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld hl, [dx]\n    ld de, [_bc]\n";
    code_ << "    or a\n    sbc hl, de\n";
    code_ << "    jp c, " << Ldone << "\n";
    code_ << "    ld [dx], hl\n";
    code_ << "    ld hl, [ac]\n    inc hl\n    ld [ac], hl\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (op == "%") copy16("ac", "dx");
}

// `ptr` must already hold the resolved address. The address is shuttled
// through the real stack across evaluating `rhs` (ptr is a single shared
// cell, and rhs's own evaluation may recurse into something that reuses
// it). `cx` stashes `yieldOld`'s value afterward.
void Z80Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    ld hl, [ptr]\n";
    loadOperand(e.type);  // ac = old
    code_ << "    ld hl, [ptr]\n";
    code_ << "    push hl\n";  // stash address
    code_ << "    ld hl, [ac]\n";
    code_ << "    push hl\n";  // stash old (ac is about to be overwritten by rhs)
    genExpr(*e.rhs);            // ac = rhs
    code_ << "    ld de, [ac]\n";
    code_ << "    pop hl\n";  // hl = old
    if (e.yieldOld) code_ << "    push hl\n";  // re-stash old for the final result
    genCompoundOp(e.op, e.type);                // ac = new (raw)
    narrowAcTo(e.type);
    if (e.yieldOld) {
        code_ << "    pop hl\n";  // hl = old
        code_ << "    ld [cx], hl\n";
        code_ << "    pop hl\n";  // hl = address
        storeOperand(e.type);      // stores ac(=new) through hl
        code_ << "    ld hl, [cx]\n";
        code_ << "    ld [ac], hl\n";  // ac = old (final result)
    } else {
        code_ << "    pop hl\n";  // hl = address
        storeOperand(e.type);      // ac already = new
    }
}

// No-op unless `to` is a `char`.
void Z80Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    code_ << "    ld hl, [ac]\n";
    if (to.isUnsigned) {
        code_ << "    ld h, 0\n";
    } else {
        code_ << "    ld a, l\n    rla\n    sbc a, a\n    ld h, a\n";
    }
    code_ << "    ld [ac], hl\n";
}

void Z80Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    ld hl, " << label(operand.slot) << "\n    ld [ac], hl\n";
        } else {
            loadFrameAddr(operand.slot);
            code_ << "    ld [ac], hl\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ld hl, " << label(operand.func) << "\n    ld [ac], hl\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Saves the left operand across evaluating the right one on the real
// stack (push hl/pop de), never in a named cell: the right operand's own
// genExpr() might recurse into genMul()/genDivMod()/genShift(), which
// share bc/cx/dx.
void Z80Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Operands share one common type (usual arithmetic conversions), except
    // shift (only lhs's type matters) and &&/|| (width-independent).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    int words = wide ? wordCountOf(operandType) : 1;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+", words);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        if (op == "+") {
            code_ << "    add hl, de\n";
        } else {
            code_ << "    or a\n    sbc hl, de\n";  // HL = lhs - rhs (real 16-bit subtract-with-carry)
        }
        code_ << "    ld [ac], hl\n";
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs, words);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", words);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        if (wide) {
            genWideBitwise(*e.lhs, *e.rhs, op, words);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    ld a, l\n    " << insn << " e\n    ld l, a\n";
        code_ << "    ld a, h\n    " << insn << " d\n    ld h, a\n";
        code_ << "    ld [ac], hl\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned, words);
            return;
        }
        genShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
        return;
    }
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
    if (wide) {
        genWideCompare(op, *e.lhs, *e.rhs, isUnsigned, words);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs, isUnsigned);
}

// No variable-count shift instruction - loops `rhs` times via
// shiftOneBit(). A signed right shift refills the vacated top bit with the
// sign bit (arithmetic) instead of 0 (logical), via a sign-copy into the
// top byte before an `rr`/`rr` chain.
void Z80Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);                                // ac = value to shift
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                                 // ac = shift count
    code_ << "    ld a, [ac]\n    ld b, a\n";     // b = shift count (always small; low byte is enough)
    code_ << "    pop hl\n    ld [_bc], hl\n";      // bc = value (restored only now)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    if (isLeft || isUnsigned) {
        shiftOneBit("_bc", isLeft);
    } else {
        code_ << "    ld hl, [_bc]\n";
        code_ << "    ld a, h\n    sra a\n    ld h, a\n";  // top byte: arithmetic (sign-preserving)
        code_ << "    rr l\n";                               // low byte: carry (the bit sra shifted out) rolls in
        code_ << "    ld [_bc], hl\n";
    }
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "_bc");
}

// Shift-and-add multiply (no hardware multiply): bc doubles each
// iteration, dx halves, ac accumulates. Only verified against
// non-negative operands.
void Z80Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);                                // ac = multiplicand
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the multiplier
    genExpr(rhs);
    code_ << "    pop hl\n    ld [_bc], hl\n";  // bc = multiplicand (restored only now)
    copy16("dx", "ac");                          // dx = multiplier
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    code_ << "    ld b, 16\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    code_ << "    ld a, [dx]\n    and 1\n";
    code_ << "    jp z, " << Lskipadd << "\n";
    code_ << "    ld hl, [ac]\n    ld de, [_bc]\n    add hl, de\n    ld [ac], hl\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("_bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): dx is the running remainder,
// bc the fixed divisor, ac the quotient. SBC HL,DE both tests and commits
// the subtraction in one instruction. The core loop only handles
// non-negative operands, so a signed operand is divided via its absolute
// value, with `ptr`'s low/high byte holding the original signs while it
// runs; the quotient's sign is restored (negative iff the signs differed),
// and the remainder always takes the dividend's own sign.
void Z80Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);                                // ac = dividend
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop hl\n    ld [dx], hl\n";  // dx = dividend (restored only now)
    copy16("_bc", "ac");                          // bc = divisor
    auto isNeg = [&](const char *cell, const char *dst) {
        code_ << "    ld a, [" << cell << "+1]\n    and 128\n";
        std::string Lnonneg = newLabel(), Ldone = newLabel();
        code_ << "    jp z, " << Lnonneg << "\n";
        code_ << "    ld a, 1\n    ld [" << dst << "], a\n    jp " << Ldone << "\n";
        code_ << Lnonneg << ":\n    ld a, 0\n    ld [" << dst << "], a\n";
        code_ << Ldone << ":\n";
    };
    auto negate16 = [&](const char *cell) {
        code_ << "    ld hl, [" << cell << "]\n";
        code_ << "    xor a\n    sub l\n    ld l, a\n";
        code_ << "    ld a, 0\n    sbc a, h\n    ld h, a\n";
        code_ << "    ld [" << cell << "], hl\n";
    };
    auto negateIf = [&](const char *flagCell, const char *cell) {
        code_ << "    ld a, [" << flagCell << "]\n";
        std::string Lskip = newLabel();
        code_ << "    or a\n    jp z, " << Lskip << "\n";
        negate16(cell);
        code_ << Lskip << ":\n";
    };
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("_bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "_bc");
    }
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld hl, [dx]\n    ld de, [_bc]\n";
    code_ << "    or a\n    sbc hl, de\n";  // HL = dx - bc; CY = borrow (dx < bc)
    code_ << "    jp c, " << Ldone << "\n";
    code_ << "    ld [dx], hl\n";  // commit: dx -= bc (HL already holds the subtracted result)
    code_ << "    ld hl, [ac]\n    inc hl\n    ld [ac], hl\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed; the
        // remainder keeps the dividend's sign.
        code_ << "    ld a, [ptr]\n    ld b, a\n    ld a, [ptr+1]\n    xor b\n";
        std::string Lskip = newLabel();
        code_ << "    jp z, " << Lskip << "\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void Z80Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false, isUnsigned);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false, isUnsigned);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true, isUnsigned);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true, isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` reduce to one SBC HL,DE: the Z flag directly means lhs == rhs.
void Z80Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(rhs);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
    code_ << "    or a\n    sbc hl, de\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (wantEqual ? "z" : "nz") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

// Signed "a < b" via the sign-bias trick: flipping each operand's
// high-byte sign bit maps the signed range onto the unsigned one, then one
// SBC HL,DE does the rest.
void Z80Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned) {
    genExpr(a);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(b);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = a, DE = b
    if (!isUnsigned) {
        code_ << "    ld a, h\n    xor 0x80\n    ld h, a\n";  // H = a's high byte, sign flipped
        code_ << "    ld a, d\n    xor 0x80\n    ld d, a\n";  // D = b's high byte, sign flipped
    }
    code_ << "    or a\n    sbc hl, de\n";                // CY = (a < b)
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (invert ? "nc" : "c") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jp z, " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jp z, " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jp nz, " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jp nz, " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp z, " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

// Leaves the real Z flag set as `or a`/`or h` would, with any wide stack
// space already released, ready for an immediate `jp z`/`jp nz`.
void Z80Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
        return;
    }
    int words = wordCountOf(e.type);
    popWideInto("a", words);
    code_ << "    ld a, 0\n";
    for (int i = 0; i < words; i++) {
        code_ << "    ld hl, [" << wideScratch("a", i) << "]\n";
        code_ << "    or l\n    or h\n";
    }
}

std::optional<std::string> Z80Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleZ80(asmText);
    // coff/elf/macho/aout: builds one format-neutral ObjectFile, then the
    // requested format's own FileFormatWriter serializes it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleZ80Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
