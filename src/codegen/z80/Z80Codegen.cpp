#include "Z80Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Z80Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - an
// arbitrary address distinct from the 8080 backend's own 0x0100 (see
// ../i8080/I8080Codegen.cpp), chosen only to leave plenty of RAM
// above it in a bare 64KB Z80 system; tests/run_tests.sh's own
// emulator setup loads this binary here and sets PC to it directly,
// no boot ROM involved.
constexpr uint32_t kBaseAddress = 0x8000;
// Same reasoning as the 8080 backend's own `kStackTop` - the Z80's
// SP is likewise undefined at reset and this backend's real CALL/
// RET/PUSH/POP all need a real initial value.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void Z80Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) own label - the referenced Slot's or Function's own
// label() text, exactly the shape `ld hl, nn`'s own 16-bit operand
// already takes (see genAddrOf()). declareArray() below hands this
// straight to a `dw` directive rather than computing a numeric
// low/high split itself - Z80Assembler.cpp's own `emit16` already
// resolves a symbol's full 16-bit address once every label is bound,
// the same single evaluator call `ld hl, nn`'s own operand goes
// through.
std::string Z80Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType` picks the data directive: `db` for a `char` array,
// `dw` for `short`/`int`, or (no 32-/64-bit directive in this
// dialect) 2 or 4 `dw`s per element - least significant word first,
// matching every wide value's own in-memory layout elsewhere in this
// file - for `long`/`long long`. A symbol element (the dispatch-
// table idiom - see IR.h's own ArrayInitElem comment) is always a
// single plain native-word address, regardless of `elemType`.
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

// See Z80Codegen.h's own header comment on why this goes through the
// real hardware stack: there's no `ld hl,ix` (or any other direct
// register-to-register move between HL and IX) on this chip, only
// `ld sp,ix`/`ld ix,sp`-shaped forms, neither of which is HL. `ix`
// itself is left untouched - only the real SP moves, transiently,
// across the push/pop pair - so this is safe to call in the middle
// of evaluating anything else.
void Z80Codegen::loadFrameAddr(const ir::Slot *slot, long extraBytes) {
    code_ << "    push ix\n    pop hl\n";
    // A Param's own bytes sit at increasing addresses above `ix`
    // (its first byte closest to the return address); a Local's grow
    // down from `ix`, so its bytes sit at decreasing addresses as
    // `extraBytes` increases - the mirror image, same as every other
    // byte-addressed backend's own array layout. `extraBytes` lets a
    // Local/Param array element, or one word of a wide (`long`/`long
    // long`) scalar, reuse this same formula - see this function's
    // own callers for what unit each one passes.
    long off = slot->kind == ir::SlotKind::Param ? (static_cast<long>(slot->frameOffset) + extraBytes)
                                                   : (-(static_cast<long>(slot->frameOffset) + extraBytes));
    uint16_t off16 = static_cast<uint16_t>(off);
    code_ << "    ld de, " << off16 << "\n    add hl, de\n";
}

void Z80Codegen::readWordAt(const std::string &loReg, const std::string &hiReg) {
    code_ << "    ld " << loReg << ", m\n    inc hl\n    ld " << hiReg << ", m\n";
}

// A Local array's own initializer values (if any - see
// IRBuilder.h's `ArrayInitElem`) are stored into this call's own
// frame once, right here at the point of declaration, rather than
// ever living in static trailer data the way a Global array's own
// `declareArray()` does - every call needs its own independent copy.
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
        // A wide (`long`/`long long`) element - 2 or 4 words, least
        // significant first, same layout as every other wide value's
        // own in-memory representation elsewhere in this file.
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
    // Assumes HL already holds the target address - plain
    // accumulator loads from `ac`'s own two bytes (by name, never
    // touching HL) interleaved with `(HL)` stores, so the caller's
    // address in HL survives across both.
    code_ << "    ld a, [ac]\n    ld m, a\n";
    code_ << "    inc hl\n";
    code_ << "    ld a, [ac+1]\n    ld m, a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl` - see
// Z80Codegen.h's own header comment on why this is only two
// instructions here (`RL`/`RR` target L/H directly) versus the
// 8080's six (RAL/RAR only ever touch the accumulator, so getting a
// value in and back out of it costs two extra MOVs per byte).
void Z80Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ld hl, [" << lbl << "]\n";
    code_ << "    or a\n";
    if (isLeft) {
        code_ << "    rl l\n    rl h\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
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

// HL already holds the operand's own address. A 1-byte operand's
// own sign extension is the classic Z80 idiom: `rla` rotates the
// byte (already saved in L) left through the carry, leaving the
// carry holding exactly the byte's own original sign bit; `sbc a,a`
// is then `a - a - carry`, which is always exactly 0x00 (carry
// clear) or 0xFF (carry set) - the sign-extension byte, in one
// branch-free register move. An unsigned `char` just zero-fills H
// directly instead.
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

// Standard multi-word add/sub: word 0 is a plain `ADD HL,DE`/`SBC
// HL,DE` (setting CY for the word after it); every later word folds
// in the previous word's own carry/borrow for free, since `ADC
// HL,DE`/`SBC HL,DE` already read the real carry flag - no separate
// "add the carry in, then add the words" step needed the way a
// plain 8-bit chain would (compare ../pdp11/Pdp11Codegen.cpp's own
// version, which needs that extra step since PDP-11 only has an ADC
// that adds the carry *alone*, not fused into the same instruction).
// Both operands are popped into the fixed "wa"/"wb" scratch cells
// first (see Z80Codegen.h's own header comment on why - no SP-
// relative addressing exists here to operate on the real stack in
// place the way the PDP-11 backend does), and the `words`-word
// result is written back over "wa" and pushed back onto the real
// stack before returning.
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

// Two's-complement negate in place: complement every word first,
// then add 1 into the least significant word and let `ADC HL,DE`
// (with DE=0 on every word but the first) carry that +1 up through
// the rest - the same chained-carry shape genWideAdd() already uses.
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

// Same sign-bias trick as emitLessCompare() (see that function's own
// comment), generalized across `words` words: compares most-
// significant word first, since only that one word's own comparison
// ever needs the signed/unsigned distinction - once a word differs,
// every lower word left is comparing a pure, already-unsigned
// magnitude (the higher words already matched).
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

// A `long`/`long long` shifted by a runtime count - looped one bit
// at a time across every byte of every word, most-significant byte
// first (see the per-iteration comment below for why). No variable-
// count shift exists even for a single native word on this chip
// (see genShift()'s own header comment), so the wide case is just
// that same one-bit-at-a-time loop stretched across more bytes.
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
            // The very first byte shifted is the whole value's own
            // top byte - `sra`/`srl` read that byte's own top bit
            // directly (not the carry flag), so no separate "clear
            // the carry first" step is needed the way '<<' needs one:
            // signed keeps that bit (arithmetic shift), unsigned
            // zero-fills it (logical shift). Every byte after that
            // just rotates in the carry `sra`/`srl` (or the previous
            // `rr`) just set, one byte at a time down to the least
            // significant.
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

// See ../pdp11/Pdp11Codegen.cpp's own genWideMul()/genWideDivMod()
// comment for the reasoning this mirrors exactly: a `long`/`long
// long` multiply/divide needs a genuine bit-serial multi-word
// software routine, and this target has no simulator wired into
// tests/run_tests.sh to verify a hand-written one against - failing
// loudly here is safer than shipping an unverifiable, possibly wrong
// one. `+`/`-`/comparisons/shifts/bitwise ops on `long`/`long long`
// are all implemented above and don't depend on this.
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

// An implicit or explicit type conversion (`ir::ExprKind::Convert` -
// see IR.h's own comment). Every arithmetic/comparison operand
// narrower than `int` is already promoted by IRBuilder before any
// Binary/Unary ever sees it, so the real work here is crossing the
// native-word/wide boundary (pushing more sign/zero-extension words,
// or discarding down to just the low word) and re-narrowing a
// `char` result (mask or sign-extend its own top byte back off).
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
        // No SP-relative addressing on this chip (unlike the PDP-11
        // backend's own `[sp+N]`) - every popped word has to land
        // somewhere real first, so the whole block is popped into the
        // "wa" scratch cells and only the least significant one kept.
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
        // Every extension word (word 1 and up) has to land BELOW
        // word 0 (the native value, always the least significant) in
        // the pushed representation - LSW stays on top (see
        // Z80Codegen.h's own header comment) - so this writes every
        // word into the "wa" scratch cells first and pushes them all
        // via pushWideFrom(), the same centrally-ordered path every
        // other wide helper uses, rather than pushing word 0 directly
        // and the extension word(s) after it (which would leave an
        // extension word on top instead).
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
    // fromWide && toWide: pad/truncate word count, preserving sign -
    // same "no SP-relative addressing" reasoning as the fromWide-only
    // case above: every word is popped into "wa" first, the sign-
    // extension word(s) (if growing) are computed from the old
    // most-significant word and written into "wa"'s own extra slots,
    // then exactly `toWords` of them are pushed back.
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
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all - an ordinary object
    // file never does. `objectMode` (see Codegen.h) means no object
    // file ever emits this entry sequence itself, even the one that
    // happens to define `main` - Z80Linker.cpp is solely responsible
    // for synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h) - same
    // pattern Pdp7Codegen.cpp/I8086Codegen.cpp's own `objectMode`
    // comment documents. An object file's own base is `org 0` - it's
    // the linker's job to place it, not this backend's (see
    // Z80Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ld sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra load leaves it in the real HL pair
        // too, for a quick look with nothing more than a register dump.
        code_ << "    ld hl, [ac]\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // The fixed memory pseudo-registers - see Z80Codegen.h's own
    // header comment for what each one is for. Every genExpr() call
    // anywhere in the *whole linked program* shares the same `ac`
    // (it's the one place a callee's own return value and a caller's
    // own next operand both pass through - see the `Call`/`Return`
    // cases below), so unlike every other symbol this backend emits,
    // these five can't each be redeclared per-object: in whole-
    // program mode (`!objectMode`) they're declared once, right here,
    // as ordinary temps; in object mode, this file never declares
    // them at all - every reference to `ac`/`_bc`/`cx`/`dx`/`ptr`
    // anywhere in a compiled object is then automatically deferred as
    // an external relocation (the same "undefined symbol" path any
    // other symbol would take), resolved against the one real
    // definition Z80Linker.cpp's own synthesized entry object
    // provides - see that file's own header comment.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("_bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // Wide (`long`/`long long`) scratch cells - see Z80Codegen.h's
        // own header comment on wideScratch()/"wacc".
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("ret", i));
        declareTemp("wacc");
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
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

// Single-file-only (see Z80Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> Z80Codegen::perFileAsm() const { return perFileAsm_; }

// Every call gets its own real frame, built around `ix`: the real
// hardware stack is what makes this reentrant (direct or indirect
// recursion both just mean more real CALLs, each pushing its own
// return address and getting its own `ix` - see loadFrameAddr()'s
// own header comment). On entry, SP points at the real return
// address CALL just pushed; `push ix` saves the caller's own frame
// pointer right below it, then `ix` is set to that same SP (no
// direct `ld ix,sp` exists on this chip, so this goes through `ix`
// itself: zero it, then add the real SP value in) - so from `ix`:
// `ix+0` is the saved caller `ix`, `ix+2` the return address,
// `ix+4+2*i` the i'th argument (pushed by the caller in reverse
// order - see the `Call` case below - so argument 0 ends up closest
// to the return address). `frameReserveBytes_` single-byte `dec sp`s
// then carve out this function's own Locals below `ix`, one byte at
// a time (no instruction moves an arbitrary computed value into SP
// directly - see loadFrameAddr()'s own comment on the same
// restriction for HL/ix). Every `Return` (and this implicit
// fallthrough one) undoes exactly that with the matching `inc sp`s
// before restoring the caller's own `ix` and returning.
void Z80Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push ix\n";
    code_ << "    ld ix, 0\n";
    code_ << "    add ix, sp\n";
    // `fn.func->frameSize` (IRBuilder's own total-bytes-used figure,
    // rounded up to the native int width as a WHOLE) can fall up to
    // `intUnits-1` bytes short of the deepest byte any individual
    // Local slot actually needs below `ix`, whenever the LAST Local
    // IRBuilder happened to lay out is itself narrower than an int
    // (its own `Slot::frameOffset` - the deepest byte it needs - can
    // then exceed `frameSize`, since `frameSize` only guarantees
    // *total* bytes, aligned as a whole, not that every individual
    // offset fits inside it): e.g. an `int` then two trailing `char`
    // Locals needs frameOffset 5 reserved but a bare `frameSize`
    // of 4 - the 5th byte would sit one below the real stack pointer,
    // in territory the very next `push` anywhere in this function
    // would silently clobber. Padding by `intUnits-1` here always
    // covers that worst case (proven by construction: `alignUp(x,
    // intUnits) - intUnits` can fall short of `x - 1` by at most
    // `intUnits-1`), at the cost of reserving up to one int-width's
    // worth of otherwise-harmless extra stack space.
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
                // A wide value sits on the real stack (LSW on top -
                // see Z80Codegen.h's own header comment) after
                // genExpr() - pop it into the fixed "wret" scratch
                // cells first (plain memory, untouched by the
                // `inc sp` unwind/`pop ix` below) rather than leaving
                // it on the stack, since that unwind is about to move
                // SP back up past wherever it currently sits. The
                // matching read-it-back-out-of-"wret" half of this
                // convention is genExpr()'s own Call case.
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
                // The target address has to survive evaluating the
                // initializer intact - which might itself index into
                // an array and so reuse `ptr` as scratch (see
                // genIndexAddr()) - so it's saved on the real
                // hardware stack instead, exactly like the indexed-
                // Assign case below does.
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
                    // `ptr` is a single shared scratch cell - evaluating
                    // `rhs` might itself index another array (clobbering
                    // it), so the element address is saved on the real
                    // stack across that call, the same way the narrow
                    // case below saves it in HL via push/pop; the rhs's
                    // own `words` pushed words land on top of that saved
                    // address, so they're popped into "wb" (safely out of
                    // the way) before the address underneath is restored.
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
            // `ptr` isn't safe to park the target address in across
            // evaluating `rhs`, since `rhs` might itself index into
            // an array and reuse `ptr` as scratch there - the real
            // hardware stack is, so the address is saved there
            // instead, same as the indexed-Assign case above (which
            // gets this for free: `rhs` is evaluated after `ptr` is
            // already filled in, and nothing here touches `ptr`
            // again until storeOperand() reads it back out).
            loadFrameAddr(e.slot);
            code_ << "    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeOperand(e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores through at e.type's own width - the pointee type
            // of a real typed pointer, or plain native-word width for
            // the still-supported untyped-int-as-address style - see
            // IRBuilder.cpp's own DerefAssign comment. A wide pointee
            // mirrors the indexed Assign's own wide branch above,
            // except `ptr` comes from evaluating e.lhs (the pointer,
            // always narrow) instead of genIndexAddr().
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
            // Real argument passing on the real hardware stack -
            // pushed right-to-left so argument 0 ends up closest to
            // the return address (see genFunction()'s own header
            // comment on the resulting `ix+4+2*i` layout); the callee
            // never cleans these up itself (no `ret n` on this chip),
            // so this caller pops them back off, one throwaway `pop
            // hl` per native word pushed, right after the call
            // returns. A wide argument's own genExpr() already pushes
            // its own `wordCountOf()` words directly, in the usual
            // LSW-on-top order every wide helper expects.
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
                // The callee left its own wide return value in the
                // "wret" scratch cells (see genStmt()'s own Return
                // case) - push it back onto the stack as this
                // expression's own result, most significant word
                // first so the least significant ends up on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    ld hl, [" << wideScratch("ret", i) << "]\n    push hl\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr). Like the 8080 (see
            // ../i8080/I8080Codegen.cpp's own CallPtr comment), the
            // Z80 has no indirect-CALL instruction, only `JP (HL)`
            // (spelled "jp [hl]" in this dialect - see
            // Z80Assembler.h's own header comment on bracket syntax),
            // object-code-identical to the 8080's `PCHL`. A real CALL
            // is emulated the same way: push the return address (the
            // label right after `jp [hl]`) onto the real hardware
            // stack, then jump to the callee through HL. The callee's
            // own `ret` pops that address back into PC.
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
            // Real Z80 interrupt enable/disable - same real hardware
            // feature, and same mnemonics, as the 8080 backend's own
            // EI/DI (see I8080Codegen.cpp).
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

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated first, and alone - see
// ../i8080/I8080Codegen.cpp's own genIndexAddr() comment on why.
// `slot->arraySize > 0` means the base is a compile-time-known label
// (an address - `ld de, <label>` loads a symbol's own address,
// exactly like genAddrOf()'s own use of the same trick), otherwise
// `slot` is a plain pointer variable and the base instead comes from
// *reading* that variable's own stored value into DE - a single real
// instruction here (see Z80Codegen.h's own header comment on
// loadPair()), unlike the 8080's own stack round-trip.
void Z80Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    // `elemShift()` already scales by the element's own full width
    // (0/1/2/3 for a 1/2/4/8-byte element - see Z80Codegen.h's own
    // header comment), so this address computation is exactly as
    // valid for a wide (`long`/`long long`) element as for a plain
    // one; only the callers that read/write through the resulting
    // `ptr` need their own wide-aware path (see genIndex()/genExpr()'s
    // own Assign case).
    genExpr(idxExpr);  // ac = index
    // HL = index * widthOfType(slot->type) - `ADD HL,HL` doubles HL
    // in one instruction (same trick as the 8080 backend's own `DAD
    // H`), repeated `elemShift()` times (0 for a `char` element, 1
    // for `short`/`int`, 2/3 for `long`/`long long` - see
    // Z80Codegen.h's own header comment). Stashed in `ptr` (not a
    // register) since loadFrameAddr() below needs HL/DE itself.
    code_ << "    ld hl, [ac]\n";
    int shift = elemShift(slot->type);
    for (int i = 0; i < shift; i++) code_ << "    add hl, hl\n";
    code_ << "    ld [ptr], hl\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ld de, " << label(slot) << "\n";
            code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
        } else {
            // A Local array's own words sit at decreasing addresses
            // as its element index increases (loadFrameAddr() gives
            // the array's own first word, closest to `ix` - later
            // words are further below it) - the mirror image of a
            // Global array's increasing-address layout above, so
            // this is base-minus-scaled-index via a real subtract-
            // with-carry, not base-plus.
            loadFrameAddr(slot);               // hl = array's own base address
            code_ << "    ld de, [ptr]\n";     // de = scaled index, saved above
            code_ << "    or a\n    sbc hl, de\n";
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

// `ptr` already holds the element's own address (genIndexAddr()).
// Reads its `words` 16-bit words, most significant first, pushing
// each one onto the real stack as it goes - same least-significant-
// word-on-top convention as every other wide load here (see
// Z80Codegen.h's own header comment) - via the extended `LD BC,(nn)`-
// style direct load this dialect spells `ld bc, [ptr]` (see
// Z80Assembler.cpp's own comment on that form).
void Z80Codegen::genWideLoadAtPtr(int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ld hl, [ptr]\n";
        if (i > 0) code_ << "    ld bc, " << (2 * i) << "\n    add hl, bc\n";
        code_ << "    ld c, m\n    inc hl\n    ld b, m\n";
        code_ << "    push bc\n";
    }
}

// The mirror image for a write: `wideScratch(role,i)` already holds
// word `i` of the value to store (see this function's own caller -
// genExpr()'s Assign case pops the evaluated rhs into "wb" first,
// before the element's own address can be clobbered by evaluating
// it). DE (not BC) carries the element address's own `+2*i` offset
// here, since BC is what the value itself is loaded into.
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

// Every binary op here saves its left operand across evaluating the
// right one on the *real* Z80 hardware stack (`push hl`/`pop de`),
// never in a named memory cell - see ../i8080/I8080Codegen.cpp's own
// genBinary() header comment for why (the same reasoning applies
// unchanged: the right operand's own genExpr() might recurse into
// genMul()/genDivMod()/genShift(), all of which share `bc`/`cx`/`dx`).
void Z80Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already shares
    // one common type (IRBuilder's own usual-arithmetic-conversion
    // pass - see Type.h), except shift (only the LHS's own type
    // matters) and &&/|| (width-independent).
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
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%");
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

// No variable-count shift instruction (see Z80Codegen.h's own header
// comment) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit(). `isUnsigned` only matters for `>>`: a signed right
// shift has to keep re-filling the vacated top bit with the sign bit
// (arithmetic shift) rather than always 0 (logical shift) - a plain
// `rr`/`rr` chain is the logical form; the signed form shifts the top
// byte in with a copy of its own sign bit first, same idea as
// genWideShift()'s own per-byte `sra`.
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

// Textbook shift-and-add multiply (no hardware multiply): `bc` is
// the multiplicand (doubled each iteration), `dx` the multiplier
// (halved each iteration), `ac` the accumulating product. Only
// verified against non-negative operands.
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

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient. A single `SBC HL,DE` both tests "has dx gone below
// bc?" (the carry flag) and computes the committed subtraction
// result in one instruction - no separate tentative-then-commit byte
// reconstruction needed the way the 8080 backend's own version
// needs (see I8080Codegen.cpp). Only verified against non-negative
// operands.
void Z80Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);                                // ac = dividend
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop hl\n    ld [dx], hl\n";  // dx = dividend (restored only now)
    copy16("_bc", "ac");                          // bc = divisor
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld hl, [dx]\n    ld de, [_bc]\n";
    code_ << "    or a\n    sbc hl, de\n";  // HL = dx - bc; CY = borrow (dx < bc)
    code_ << "    jp c, " << Ldone << "\n";
    code_ << "    ld [dx], hl\n";  // commit: dx -= bc (HL already holds the subtracted result)
    // Quotient += 1 - a real 16-bit increment.
    code_ << "    ld hl, [ac]\n    inc hl\n    ld [ac], hl\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
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

// `==`/`!=` reduce to a single `SBC HL,DE`: the Z flag it sets
// directly means "lhs == rhs" (the full 16-bit result is zero iff
// the two operands were equal) - no manual byte-by-byte OR-check
// needed the way the 8080 backend's own version needs.
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

// Signed 16-bit "a < b" via the standard bias trick (see
// ../i8080/I8080Codegen.h's own header comment: the Z80, like the
// 8080, has no single flag or condition code for signed less-than) -
// flipping the sign bit of each operand's high byte maps the signed
// range onto the unsigned one while preserving order, and a single
// `SBC HL,DE` (instead of the 8080's two byte-wise `SUB`/`SBB`) does
// the rest.
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

// After genExpr(e) just left `e`'s own value - `ac`, if narrow, or
// `wordCountOf(e.type)` words on the real stack (LSW on top) if wide
// (see Z80Codegen.h's own header comment on this convention) - this
// leaves the real Z flag set exactly as a plain `or a`/`or h` would,
// with any wide stack space already released, ready for an
// immediate `jp z`/`jp nz`.
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

std::string Z80Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                              const std::string &) const {
    throw std::runtime_error("the Z80 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Z80Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleZ80(asmText);
    // `coff`/`elf`/`macho`/`aout`: real separate compilation (see
    // generate()'s own `objectMode` comment) - this backend's own
    // assembler builds one format-neutral ObjectFile exactly once
    // (see Z80Assembler.h's own `assembleZ80Object`), and whichever
    // of these four names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it - this file never touches a real COFF/ELF/Mach-O/
    // a.out byte layout itself.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleZ80Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
