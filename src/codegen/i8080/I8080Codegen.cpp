#include "I8080Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "I8080Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - low
// enough to leave plenty of RAM above it in a bare 64KB 8080 system,
// high enough to stay clear of 0x0000-0x00FF (conventionally the
// RST-vector/boot area on real 8080 hardware, even though nothing
// here ever uses interrupts or RST - see tests/run_tests.sh's own
// emulator setup, which just loads this binary here and sets PC to
// it directly, no boot ROM involved).
constexpr uint32_t kBaseAddress = 0x0100;
// The hardware stack (SP/PUSH/POP/CALL/RET) needs an explicit initial
// value - unlike the 6502 (whose stack page is architecturally fixed
// at $0100) the 8080's SP is simply undefined at reset. Comfortably
// above every label this backend could ever generate for an ordinary
// test-sized program, inside the 64KB RAM tests/run_tests.sh gives
// the emulator.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void I8080Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) own label - the referenced Slot's or Function's own
// label() text, exactly the shape `lxi`'s own 16-bit operand already
// takes (see genAddrOf()/genIndexAddr()). declareArray() below hands
// this straight to a `dw` directive rather than computing a numeric
// low/high split itself - I8080Assembler.cpp's own `emit16` already
// resolves a symbol's full 16-bit address once every label is bound,
// the same single evaluator call `lxi`'s own operand goes through.
std::string I8080Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType` picks the data directive - see ../z80/Z80Codegen.cpp's
// own declareArray() comment, which this mirrors exactly.
void I8080Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
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

void I8080Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lhld " << src << "\n    shld " << dst << "\n";
}

void I8080Codegen::loadPair(const std::string &pairReg, const std::string &src) {
    code_ << "    lhld " << src << "\n    push h\n    pop " << pairReg << "\n";
}

void I8080Codegen::storePair(const std::string &dst, const std::string &pairReg) {
    code_ << "    push " << pairReg << "\n    pop h\n    shld " << dst << "\n";
}

void I8080Codegen::storeAcIndirect() {
    // Assumes HL already holds the target address (see this
    // function's own declaration comment) - `lda`/`sta` address `ac`
    // directly, by name, never touching HL, so the caller's address
    // in HL survives across both byte stores.
    code_ << "    lda ac\n    mov m,a\n";
    code_ << "    inx h\n";
    code_ << "    lda ac+1\n    mov m,a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl`, used by
// genShift()'s own loop and inlined directly by genMul()/
// genDivMod() (see I8080Codegen.h's own header comment on why there's
// no variable-count shift instruction to use instead). `ana a` is
// this target's own idiom for "clear carry without otherwise
// disturbing A" (every logical instruction clears CY as a side
// effect, and ANA A's "AND A with itself" leaves A's value alone) -
// needed because the 8080, unlike the 6502, has no explicit "clear
// carry" instruction, and a left/right rotate always needs a known
// carry-in to shift a 0 into the vacated bit.
void I8080Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    lhld " << lbl << "\n";
    code_ << "    ana a\n";
    if (isLeft) {
        code_ << "    mov a,l\n    ral\n    mov l,a\n";
        code_ << "    mov a,h\n    ral\n    mov h,a\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    mov a,h\n    rar\n    mov h,a\n";
        code_ << "    mov a,l\n    rar\n    mov l,a\n";
    }
    code_ << "    shld " << lbl << "\n";
}

int I8080Codegen::widthOfType(Type t) const { return widthOf(t, typeWidths()); }
bool I8080Codegen::isWide(Type t) const { return widthOfType(t) > 2; }
int I8080Codegen::wordCountOf(Type t) const { return widthOfType(t) / 2; }
int I8080Codegen::elemShift(Type t) const {
    switch (widthOfType(t)) {
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        default: return 3;  // 8
    }
}

// HL already holds the operand's own address. The sign-extension
// idiom - `mov a,l` / `ral` / `sbb a` - leaves the carry flag holding
// exactly L's own original bit 7 after `ral` (RAL always rotates bit
// 7 into the carry), and `SBB A` is always `A - A - CY`, which is
// exactly 0x00 or 0xFF (CY clear or set) regardless of what `ral`
// left in A - the sign-extension byte, in three real instructions
// with no branch. An unsigned `char` just zero-fills H directly
// instead.
void I8080Codegen::loadOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    mov a,m\n    mov l,a\n";
        if (type.isUnsigned) {
            code_ << "    mvi h, 0\n";
        } else {
            code_ << "    ral\n    sbb a\n    mov h,a\n";
        }
        code_ << "    shld ac\n";
        return;
    }
    code_ << "    mov e,m\n    inx h\n    mov d,m\n";
    storePair("ac", "d");
}

void I8080Codegen::storeOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    lda ac\n    mov m,a\n";
        return;
    }
    storeAcIndirect();
}

void I8080Codegen::popWideInto(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop h\n    shld " << wideScratch(role, i) << "\n";
    }
}

void I8080Codegen::pushWideFrom(const char *role, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    lhld " << wideScratch(role, i) << "\n    push h\n";
    }
}

void I8080Codegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lxi h, " << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "\n";
        } else {
            genFrameAddr(slot, 2 * i);
        }
        code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        code_ << "    push d\n";
    }
}

void I8080Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        uint16_t w = static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    lxi h, " << w << "\n    push h\n";
    }
}

void I8080Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        // Popped into BC, not DE - genFrameAddr() below uses DE as its
        // own scratch for a Local/Param's offset (lxi d/dad d), so
        // popping the value-to-store into DE would have it clobbered
        // before the mov m,*/mov m,* below ever reads it back out.
        code_ << "    pop b\n";
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lxi h, " << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "\n";
        } else {
            genFrameAddr(slot, 2 * i);
        }
        code_ << "    mov m,c\n    inx h\n    mov m,b\n";
    }
}

// Multi-byte add/sub/negate/bitwise/compare all work byte-at-a-time
// here, LSB first, chaining the real carry/borrow flag through ADC/
// SBB (there's no 16-bit ADC/SBB on this chip the way the Z80's
// `ADC HL,rr`/`SBC HL,rr` give ../z80/Z80Codegen.cpp's own version a
// per-*word* chain instead) - every wide helper below first pops both
// operands into the fixed "wa"/"wb" scratch cells (see
// I8080Codegen.h's own header comment on wideScratch() - no SP-
// relative addressing exists on this chip either), then walks every
// byte of every word from least to most significant.
namespace {
// `role`'s own `words`-word value, as individual byte labels, least
// significant byte first - `wideScratch(role,0)` is that byte,
// `wideScratch(role,0)+"+1"` the next, `wideScratch(role,1)` the one
// after that, and so on (see I8080Codegen.h's own wideScratch()
// comment: each word is a real 2-byte "db lo, hi" cell).
std::vector<std::string> wideByteLabelsLsbFirst(const char *role, int words) {
    std::vector<std::string> out;
    for (int i = 0; i < words; i++) {
        out.push_back(I8080Codegen::wideScratch(role, i));
        out.push_back(I8080Codegen::wideScratch(role, i) + "+1");
    }
    return out;
}
}  // namespace

void I8080Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    lda " << bBytes[k] << "\n    mov b,a\n";
        code_ << "    lda " << aBytes[k] << "\n";
        if (isAdd) {
            code_ << "    " << (k == 0 ? "add b\n" : "adc b\n");
        } else {
            code_ << "    " << (k == 0 ? "sub b\n" : "sbb b\n");
        }
        code_ << "    sta " << aBytes[k] << "\n";
    }
    pushWideFrom("a", words);
}

void I8080Codegen::genWideNegate(int words) {
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    for (const auto &b : aBytes) {
        code_ << "    lda " << b << "\n    cma\n    sta " << b << "\n";
    }
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    lda " << aBytes[k] << "\n";
        code_ << "    " << (k == 0 ? "adi 1\n" : "aci 0\n");
        code_ << "    sta " << aBytes[k] << "\n";
    }
    pushWideFrom("a", words);
}

void I8080Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    const char *insn = op == "&" ? "ana" : op == "|" ? "ora" : "xra";
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    lda " << bBytes[k] << "\n    mov b,a\n";
        code_ << "    lda " << aBytes[k] << "\n    " << insn << " b\n";
        code_ << "    sta " << aBytes[k] << "\n";
    }
    pushWideFrom("a", words);
}

void I8080Codegen::genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    code_ << "    mvi c, 0\n";
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    lda " << bBytes[k] << "\n    mov d,a\n";
        code_ << "    lda " << aBytes[k] << "\n    xra d\n";
        code_ << "    ora c\n    mov c,a\n";
    }
    code_ << "    mov a,c\n    ora a\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (wantEqual ? "z" : "nz") << " " << Ltrue << "\n";
    code_ << "    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

// Compares most-significant byte first - only that byte's own
// comparison ever needs the signed/unsigned bias (see
// emitLessCompare()'s own comment); once a byte differs, every byte
// after it is comparing a pure, already-unsigned magnitude.
void I8080Codegen::genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words) {
    genExpr(a);
    genExpr(b);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    for (size_t idx = aBytes.size(); idx-- > 0;) {
        bool top = idx == aBytes.size() - 1;
        code_ << "    lda " << aBytes[idx] << "\n";
        if (top && !isUnsigned) code_ << "    xri 0x80\n";
        code_ << "    mov d,a\n";
        code_ << "    lda " << bBytes[idx] << "\n";
        if (top && !isUnsigned) code_ << "    xri 0x80\n";
        code_ << "    mov e,a\n";
        code_ << "    mov a,d\n    sub e\n";
        if (idx == 0) {
            code_ << "    jc " << (invert ? Lfalse : Ltrue) << "\n";
            code_ << "    jmp " << (invert ? Ltrue : Lfalse) << "\n";
        } else {
            std::string Lcontinue = newLabel();
            code_ << "    jz " << Lcontinue << "\n";
            code_ << "    jc " << (invert ? Lfalse : Ltrue) << "\n";
            code_ << "    jmp " << (invert ? Ltrue : Lfalse) << "\n";
            code_ << Lcontinue << ":\n";
        }
    }
    code_ << Lfalse << ":\n    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                     int words) {
    if (op == "==") return genWideEqualityCompare(lhs, rhs, /*wantEqual=*/true, words);
    if (op == "!=") return genWideEqualityCompare(lhs, rhs, /*wantEqual=*/false, words);
    if (op == "<") return genWideLessCompare(lhs, rhs, /*invert=*/false, isUnsigned, words);
    if (op == ">") return genWideLessCompare(rhs, lhs, /*invert=*/false, isUnsigned, words);
    if (op == "<=") return genWideLessCompare(rhs, lhs, /*invert=*/true, isUnsigned, words);
    if (op == ">=") return genWideLessCompare(lhs, rhs, /*invert=*/true, isUnsigned, words);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// One bit at a time, most significant byte first (see genShift()'s
// own header comment on why even the single-word case already does
// this one bit at a time) - `ral`/`rar` always rotate the
// accumulator, chaining the real carry flag from one byte into the
// next exactly the way shiftOneBit() already chains it across the
// two bytes of a plain `int`. The very first byte shifted right is
// the whole value's own top byte; a signed shift needs that one bit
// preserved (arithmetic), not always 0 (logical) - `mov b,a`/`ral`/
// `mov a,b`/`rar` computes that top byte's own software SRA (there's
// no hardware one on this chip): `ral` first copies the byte's own
// sign bit into the carry (via its own top-bit rotation), then `rar`
// on the *original* byte (restored from `b`) rotates that same bit
// back in as the new top bit while shifting right - the sign
// survives, and the carry `rar` shifts *out* (the original bit 0) is
// exactly what the next, lower byte's own plain `rar` needs.
void I8080Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);
    popWideInto("a", words);
    genExpr(rhs);
    code_ << "    lhld ac\n    mov c,l\n";
    auto bytesMsbFirst = wideByteLabelsLsbFirst("a", words);
    std::reverse(bytesMsbFirst.begin(), bytesMsbFirst.end());
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a,c\n    ora a\n";
    code_ << "    jz " << Ldone << "\n";
    if (isLeft) {
        code_ << "    ana a\n";  // clear carry - '<<' always shifts in a 0 bit
        for (size_t idx = bytesMsbFirst.size(); idx-- > 0;) {
            code_ << "    lda " << bytesMsbFirst[idx] << "\n    ral\n    sta " << bytesMsbFirst[idx] << "\n";
        }
    } else {
        for (size_t idx = 0; idx < bytesMsbFirst.size(); idx++) {
            if (idx == 0 && !isUnsigned) {
                code_ << "    lda " << bytesMsbFirst[idx] << "\n    mov b,a\n";
                code_ << "    ral\n    mov a,b\n    rar\n";
            } else {
                if (idx == 0) code_ << "    ana a\n";  // unsigned top byte: logical, zero-fill, clear carry first
                code_ << "    lda " << bytesMsbFirst[idx] << "\n    rar\n";
            }
            code_ << "    sta " << bytesMsbFirst[idx] << "\n";
        }
    }
    code_ << "    dcr c\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    pushWideFrom("a", words);
}

// See ../pdp11/Pdp11Codegen.cpp's own genWideMul()/genWideDivMod()
// comment (mirrored on ../z80/Z80Codegen.cpp too) for the reasoning
// this repeats exactly: no simulator in this project's own test
// harness to verify a hand-written multi-word multiply/divide
// routine against, so failing loudly is safer than shipping an
// unverifiable, possibly wrong one.
void I8080Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                   "see I8080Codegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void I8080Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                   "see I8080Codegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

// An implicit or explicit conversion - see ../z80/Z80Codegen.cpp's
// own genConvert(), which this mirrors exactly (just through this
// backend's own mnemonics/sign-extension idiom).
void I8080Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);
        if (widthOfType(to) == 1) {
            code_ << "    lhld ac\n";
            if (to.isUnsigned) {
                code_ << "    mvi h, 0\n";
            } else {
                code_ << "    mov a,l\n    ral\n    sbb a\n    mov h,a\n";
            }
            code_ << "    shld ac\n";
        }
        return;
    }
    if (fromWide && !toWide) {
        int words = wordCountOf(from);
        genExpr(*e.lhs);
        popWideInto("a", words);
        code_ << "    lhld " << wideScratch("a", 0) << "\n    shld ac\n";
        if (widthOfType(to) == 1) {
            code_ << "    lhld ac\n";
            if (to.isUnsigned) {
                code_ << "    mvi h, 0\n";
            } else {
                code_ << "    mov a,l\n    ral\n    sbb a\n    mov h,a\n";
            }
            code_ << "    shld ac\n";
        }
        return;
    }
    if (!fromWide && toWide) {
        // See ../z80/Z80Codegen.cpp's own version of this branch for
        // why every word is written into the "wa" scratch cells and
        // pushed via pushWideFrom() (LSW-on-top order, centrally)
        // rather than pushing word 0 directly followed by the
        // extension word(s) - the latter would leave an extension
        // word on top instead of the native (least significant) one.
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac already holds the native-width value, correctly sign/zero-extended to 16 bits
        code_ << "    lhld ac\n    shld " << wideScratch("a", 0) << "\n";
        for (int i = 1; i < toWords; i++) {
            if (from.isUnsigned) {
                code_ << "    lxi h, 0\n";
            } else {
                code_ << "    mov a,h\n    ral\n    sbb a\n    mov h,a\n    mov l,a\n";
            }
            code_ << "    shld " << wideScratch("a", i) << "\n";
        }
        pushWideFrom("a", toWords);
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromWords = wordCountOf(from);
    int toWords = wordCountOf(to);
    genExpr(*e.lhs);
    popWideInto("a", fromWords);
    if (toWords > fromWords) {
        code_ << "    lhld " << wideScratch("a", fromWords - 1) << "\n";
        if (from.isUnsigned) {
            code_ << "    lxi h, 0\n";
        } else {
            code_ << "    mov a,h\n    ral\n    sbb a\n    mov h,a\n    mov l,a\n";
        }
        for (int i = fromWords; i < toWords; i++) code_ << "    shld " << wideScratch("a", i) << "\n";
    }
    pushWideFrom("a", toWords);
}

std::string I8080Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself, even the one that happens to define `main` -
    // I8080LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `main`'s linked
    // address is resolvable (see Linker.h) - same pattern
    // Z80Codegen.cpp's own `objectMode` comment documents. An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lxi sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra `lhld` leaves it in the real HL
        // pair too, for a quick look with nothing more than a
        // register dump.
        code_ << "    lhld ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    // The fixed memory pseudo-registers - see I8080Codegen.h's own
    // header comment for what each one is for. Every genExpr() call
    // anywhere in the *whole linked program* shares the same `ac`
    // (it's the one place a callee's own return value and a caller's
    // own next operand both pass through), so unlike every other
    // symbol this backend emits, these five can't each be redeclared
    // per-object: in whole-program mode they're declared once, right
    // here, as ordinary temps; in object mode, this file never
    // declares them at all - every reference anywhere in a compiled
    // object is then automatically deferred as an external
    // relocation, resolved against the one real definition
    // I8080LinkerFactory.cpp's own synthesized entry object provides
    // - see Z80Codegen.cpp's own `generate()` comment for the
    // original version of this same reasoning.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // The current function's own software frame pointer (see
        // I8080Codegen.h's own header comment) - shared the same way
        // as the five cells above, since a callee's own prologue and
        // its caller both need to agree on the one real place it
        // lives, exactly like `ac` does for a return value.
        declareTemp("fp");
        // Wide (`long`/`long long`) scratch cells - see
        // I8080Codegen.h's own header comment on wideScratch().
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("ret", i));
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

// Single-file-only (see I8080Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> I8080Codegen::perFileAsm() const { return perFileAsm_; }

// Every activation gets its own real stack frame (see I8080Codegen.h's
// own header comment for the full layout): `lhld fp`/`push h` saves
// the caller's own frame pointer on the real hardware stack, `lxi
// h,0`/`dad sp` reads the current SP into HL (the standard 8080 idiom
// for that - there's no direct "HL=SP" instruction) right after that
// push, so it's exactly the address of the just-pushed word, and
// `shld fp` makes that this call's own new frame base. `dad d` (with
// the locals' own *negative* total byte size already in DE) then
// `sphl` moves SP down below them in one step, reserving that memory
// so nothing later in this function (a nested `push`, or a nested
// `call`'s own argument pushes) can ever land on top of it - the same
// role I8086Codegen.cpp's own `sub sp, N` plays, just computed by hand
// since the 8080 has no instruction that subtracts an immediate from
// SP directly.
void I8080Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    lhld fp\n    push h\n";
    code_ << "    lxi h, 0\n    dad sp\n    shld fp\n";
    // See ../z80/Z80Codegen.cpp's own genFunction() comment for why
    // `fn.func->frameSize` alone can fall up to `intUnits-1` bytes
    // short of the deepest byte any individual Local slot actually
    // needs below `fp` (same shared-IRBuilder quirk, same fix) -
    // padding by that much here always covers the worst case.
    long reserveBytes = static_cast<long>(fn.func->frameSize) + (typeWidths().intUnits - 1);
    if (reserveBytes > 0) {
        code_ << "    lxi d, " << (-reserveBytes) << "\n    dad d\n    sphl\n";
    }
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

// See this file's own header comment above genFunction() for the
// prologue this undoes: `lhld fp`/`sphl` snaps SP back up to this
// call's own frame base (dropping the locals' reserved region in one
// instruction, regardless of how many words that was), `pop h` lifts
// the caller's own frame pointer back off the real hardware stack,
// `shld fp` restores it as the current `fp`, and `ret` returns to the
// caller exactly as it always did - the real hardware CALL/RET pair
// (and the real SP they already use) never needed any change of their
// own to be reentrant.
void I8080Codegen::emitEpilogue() {
    code_ << "    lhld fp\n    sphl\n";
    code_ << "    pop h\n    shld fp\n";
    code_ << "    ret\n";
}

// A Local or Param slot's own effective address, computed fresh every
// time rather than ever cached - see I8080Codegen.h's own header
// comment for why this target has no addressing mode that could do
// this as part of a single `mov`/`mvi`/etc. the way Z80's `(IX+d)` or
// 8086's `[bp+d]` can.
void I8080Codegen::genFrameAddr(const ir::Slot *slot, long extraBytes) {
    code_ << "    lhld fp\n";
    code_ << "    lxi d, " << frameByteOffset(slot, extraBytes) << "\n";
    code_ << "    dad d\n";
}

void I8080Codegen::genStmt(const ir::Stmt &s) {
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
            code_ << "    jz " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jmp " << Ldone << "\n";
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
            code_ << "    jz " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                // A wide value sits on the real stack (LSW on top -
                // see I8080Codegen.h's own header comment) after
                // genExpr() - pop it into the fixed "wret" scratch
                // cells first (plain memory, untouched by
                // emitEpilogue()'s own `sphl`) rather than leaving it
                // on the stack, since `sphl` is about to reset SP
                // straight back to `fp`, abandoning (not reading)
                // whatever still sits above it. genExpr()'s own Call
                // case reads it back out of "wret" the same way.
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) {
                        code_ << "    pop h\n    shld " << wideScratch("ret", i) << "\n";
                    }
                }
            }
            // An early return bypasses genFunction()'s own fall-
            // through epilogue entirely, so it needs this same frame
            // teardown inline - see emitEpilogue()'s own comment.
            emitEpilogue();
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
                genExpr(*s.initOrReturnOrExpr);
                genFrameAddr(s.declSlot);
                storeOperand(s.declSlot->type);
            }
            // No "else" case for a bare, uninitialized scalar: its own
            // frame word already exists (genFunction()'s own prologue
            // already reserved it) - same as a real, ordinary C
            // automatic variable, its value is simply whatever was
            // already in that stack slot until something assigns it.
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

void I8080Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    lxi h, " << v16 << "\n    shld ac\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    lxi h, " << label(e.slot) << "\n";
            } else {
                genFrameAddr(e.slot);
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
                    code_ << "    lhld ptr\n    push h\n";
                    genExpr(*e.rhs);
                    popWideInto("b", words);
                    code_ << "    pop h\n    shld ptr\n";
                    genWideStoreAtPtr("b", words);
                    return;
                }
                code_ << "    lhld ptr\n    push h\n";
                genExpr(*e.rhs);
                code_ << "    pop h\n";
                storeOperand(e.slot->type);
                return;
            }
            if (isWide(e.slot->type)) {
                genExpr(*e.rhs);  // pushes `wordCountOf()` words
                genWideStore(e.slot, wordCountOf(e.slot->type));
                return;
            }
            genExpr(*e.rhs);
            if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    lxi h, " << label(e.slot) << "\n";
            } else {
                genFrameAddr(e.slot);
            }
            storeOperand(e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = address
                genCompoundThroughPtr(e);
                return;
            }
            genCompoundScalar(e);
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
            code_ << "    jz " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
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
                code_ << "    lhld ptr\n    push h\n";
                genExpr(*e.rhs);
                popWideInto("b", words);
                code_ << "    pop h\n    shld ptr\n";
                genWideStoreAtPtr("b", words);
                return;
            }
            code_ << "    lhld ptr\n    push h\n";
            genExpr(*e.rhs);
            code_ << "    pop h\n";
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
                code_ << "    lhld ac\n";
                code_ << "    xra a\n    sub l\n    mov l,a\n";
                code_ << "    mvi a, 0\n    sbb h\n    mov h,a\n";
                code_ << "    shld ac\n";
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
                code_ << "    lhld ptr\n";
                loadOperand(e.type);
            } else if (e.op == "~") {
                if (wide) {
                    genExpr(*e.rhs);
                    int words = wordCountOf(e.rhs->type);
                    popWideInto("a", words);
                    for (int i = 0; i < words; i++) {
                        code_ << "    lhld " << wideScratch("a", i) << "\n";
                        code_ << "    mov a,l\n    cma\n    mov l,a\n";
                        code_ << "    mov a,h\n    cma\n    mov h,a\n";
                        code_ << "    shld " << wideScratch("a", i) << "\n";
                    }
                    pushWideFrom("a", words);
                    return;
                }
                genExpr(*e.rhs);
                code_ << "    lhld ac\n";
                code_ << "    mov a,l\n    cma\n    mov l,a\n";
                code_ << "    mov a,h\n    cma\n    mov h,a\n";
                code_ << "    shld ac\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Each argument is pushed onto the real hardware stack,
            // last-declared one first, so argument 0 ends up closest
            // to the return address `call` is about to push - the
            // address every Param already addresses itself by (see
            // I8080Codegen.h's own `frameByteOffset()`: paramIndex 0 is
            // `fp+4`, the lowest positive offset). The callee's own
            // prologue lays them into ITS OWN frame itself; nothing
            // here needs to know anything about the callee's
            // parameter count beyond `e.args.size()`. Each value is
            // computed into `ac` and then transited through HL (the
            // only way to get a memory value onto the real stack -
            // `PUSH` only ever takes a register pair, never a memory
            // operand).
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    totalWords += wordCountOf(e.args[i]->type);
                } else {
                    code_ << "    lhld ac\n    push h\n";
                    totalWords += 1;
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            // The caller cleans its own pushed arguments back off
            // afterward - cdecl's own convention, chosen here (same
            // as I8086Codegen.cpp's own `add sp, N`) only because it
            // needs no cooperation from the callee at all. `lxi
            // h,N`/`dad sp`/`sphl` moves SP by a compile-time-constant
            // amount in one step, the same idiom genFunction()'s own
            // prologue uses to reserve locals.
            if (totalWords > 0) {
                code_ << "    lxi h, " << (totalWords * 2) << "\n    dad sp\n    sphl\n";
            }
            if (isWide(e.type)) {
                // The callee left its own wide return value in the
                // "wret" scratch cells (see genStmt()'s own Return
                // case) - push it back onto the stack as this
                // expression's own result, most significant word
                // first so the least significant ends up on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    lhld " << wideScratch("ret", i) << "\n    push h\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr). The 8080 has no
            // indirect-CALL instruction, only `PCHL` (jump to the
            // address in HL) - so a real CALL is emulated by hand:
            // push the return address (the label right after `pchl`)
            // onto the real hardware stack, exactly as `CALL` itself
            // would, then `PCHL` to the callee. The callee's own
            // `ret` pops that same address back into PC, landing
            // right after `pchl` below - indistinguishable from an
            // ordinary call/return pair.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            code_ << "    lhld ac\n";  // HL = callee's address - pchl's own implicit target
            code_ << "    lxi d, " << Lret << "\n";
            code_ << "    push d\n";
            code_ << "    pchl\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 8080 interrupt enable/disable - EI/DI, the same
            // real hardware feature as the 6502/8086 backends' own
            // cli/sti (see M6502Codegen.cpp/I8086Codegen.cpp).
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
// cell. `idxExpr` is always evaluated *first*, and alone - genExpr()
// is a general recursive-descent call that might itself need `ptr`/
// `bc`/`cx`/`dx` (or DE/HL transiently) for its own nested
// subexpressions, so nothing of this function's own can safely be
// sitting in any of those across that call; only after it returns
// (and its result is safely resting in `ac`) does this function
// start using its own scratch.
//
// `slot->arraySize > 0` means `slot` is itself the array, and its own
// base address is the thing to index from; otherwise `slot` is a
// plain pointer variable and the base instead comes from *reading*
// that variable's own stored value. A Global's own base is a compile-
// time-known label (a bare `lxi d, <label>` loads a symbol's own
// address, exactly like genAddrOf()'s own use of the same trick) or,
// for a pointer variable, a direct read of its known address
// (loadPair()); a Local/Param's base is only known at runtime,
// relative to `fp` (genFrameAddr()), and for a pointer variable means
// dereferencing that frame address rather than just computing it.
//
// A Global array's element i sits at base+2*i, same as any ordinary
// increasing-address memory - but a *Local* array's element i sits at
// base-2*i (`frameByteOffset()` is already negative, and
// genFrameAddr()'s own `addr()` is the array's first/lowest-index
// word - see I8080Codegen.h's own header comment on why): getting
// this backwards silently corrupts whatever's above the array in the
// frame (another Local, the saved `fp`, or the return address
// itself), so the combine step below subtracts `ptr` (idx*2) from the
// base instead of adding it whenever the base came from a Local/Param
// array.
void I8080Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    // `elemShift()` already scales by the element's own full width
    // (0/1/2/3 for a 1/2/4/8-byte element - see I8080Codegen.h's own
    // header comment), so this address computation is exactly as
    // valid for a wide (`long`/`long long`) element as for a plain
    // one; only the callers that read/write through the resulting
    // `ptr` need their own wide-aware path (see genIndex()/genExpr()'s
    // own Assign case).
    genExpr(idxExpr);  // ac = index
    // HL = index * widthOfType(slot->type) - `dad h` doubles HL in a
    // single instruction (adding a value to itself is exactly
    // doubling it), repeated `elemShift()` times (0 for a `char`
    // element, 1 for `short`/`int`, 2/3 for `long`/`long long` - see
    // I8080Codegen.h's own header comment). Stashed in `ptr`
    // immediately - loading the base below (particularly
    // loadPair()/genFrameAddr(), which go through HL) would otherwise
    // clobber it.
    code_ << "    lhld ac\n";
    int shift = elemShift(slot->type);
    for (int i = 0; i < shift; i++) code_ << "    dad h\n";
    code_ << "    shld ptr\n";
    const bool isGlobal = slot->kind == ir::SlotKind::Global;
    if (isGlobal) {
        if (slot->arraySize > 0) {
            code_ << "    lxi d, " << label(slot) << "\n";
        } else {
            loadPair("d", label(slot));
        }
    } else {
        genFrameAddr(slot);  // HL = slot's own frame address
        if (slot->arraySize > 0) {
            code_ << "    push h\n    pop d\n";  // DE = that address (the array's own base)
        } else {
            // slot is a pointer VARIABLE living in this frame - DE
            // needs its stored runtime VALUE, not its own address.
            code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        }
    }
    const bool subtractIdx = !isGlobal && slot->arraySize > 0;
    if (subtractIdx) {
        // ptr = base(DE) - index*2(currently in `ptr`) - see this
        // function's own header comment on why a Local array needs
        // this instead of the add below. Same DE-minus-HL byte-by-
        // byte borrow chain as genBinary()'s own "-" operator.
        code_ << "    lhld ptr\n";
        code_ << "    mov a,e\n    sub l\n    mov e,a\n";
        code_ << "    mov a,d\n    sbb h\n    mov d,a\n";
        storePair("ptr", "d");
    } else {
        code_ << "    lhld ptr\n    dad d\n    shld ptr\n";  // ptr = index*2 + base
    }
}

// `ptr` already holds the element's own address (genIndexAddr()).
// Reads its `words` 16-bit words, most significant first, pushing
// each one onto the real stack as it goes - same least-significant-
// word-on-top convention as every other wide load here (see
// I8080Codegen.h's own header comment) - by recomputing `ptr+2*i`
// fresh for each word via the otherwise-unused BC pair (DE already
// held the popped value in the Assign-side equivalent of this; here
// there's no popped value to protect, so this could use DE too, but
// BC keeps this and genWideStoreAtPtr() symmetric).
void I8080Codegen::genWideLoadAtPtr(int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    lhld ptr\n";
        if (i > 0) code_ << "    lxi b, " << (2 * i) << "\n    dad b\n";
        code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        code_ << "    push d\n";
    }
}

// The mirror image for a write: `wideScratch(role,i)` already holds
// word `i` of the value to store (see this function's own caller -
// genExpr()'s Assign case pops the evaluated rhs into "wb" first,
// before the element's own address can be clobbered by evaluating
// it), and `lda`/direct-`sta`-by-label addressing reaches that value
// one byte at a time without ever needing it in a register pair that
// `ptr+2*i`'s own address computation is also using.
void I8080Codegen::genWideStoreAtPtr(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    lhld ptr\n";
        if (i > 0) code_ << "    lxi b, " << (2 * i) << "\n    dad b\n";
        code_ << "    lda " << wideScratch(role, i) << "\n    mov m,a\n";
        code_ << "    inx h\n";
        code_ << "    lda " << wideScratch(role, i) << "+1\n    mov m,a\n";
    }
}

void I8080Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);  // ptr = element address
    if (isWide(e.slot->type)) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    code_ << "    lhld ptr\n";
    loadOperand(e.slot->type);
}

// Entry convention: HL = old value, DE = rhs value (both already at
// `type`'s own width/signedness - IRBuilder.cpp's own
// buildCompoundRhs() already converts `rhs` to match). `+` is a single
// `dad d` (HL already holds both operands' "accumulator" role, unlike
// genBinary()'s own tail, which has to shuttle lhs/rhs out of `ac`
// first since genExpr() always deposits there). `-`/`&`/`|`/`^` reuse
// genBinary()'s own byte-wise instruction shapes with HL/DE's roles
// swapped from that function's own lhs/rhs convention - a register-
// generic mnemonic (MOV/SUB/SBB/ANA/ORA/XRA all encode any 8-bit
// register operand the same way - see I8080Assembler.cpp's own
// reg8Code()), so this is the same instruction shape, not a new one.
// `*`/`/`/`%`/`<<`/`>>` seed `bc`/`dx` directly from HL/DE (HL can go
// straight to its target cell via `shld`; DE needs a real stack round-
// trip through HL first, same as storePair()) and then run genMul()'s/
// genDivMod()'s/genShift()'s own loop body unchanged.
void I8080Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    dad d\n    shld ac\n"; return; }
    if (op == "-") {
        code_ << "    mov a,l\n    sub e\n    mov l,a\n";
        code_ << "    mov a,h\n    sbb d\n    mov h,a\n";
        code_ << "    shld ac\n";
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "ana" : (op == "|") ? "ora" : "xra";
        code_ << "    mov a,l\n    " << insn << " e\n    mov l,a\n";
        code_ << "    mov a,h\n    " << insn << " d\n    mov h,a\n";
        code_ << "    shld ac\n";
        return;
    }
    if (op == "*") {
        code_ << "    shld bc\n";  // bc = old (multiplicand) - HL already holds it
        storePair("dx", "d");       // dx = rhs (multiplier)
        code_ << "    lxi h, 0\n    shld ac\n";
        code_ << "    mvi c, 16\n";
        std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    mov a,c\n    ora a\n";
        code_ << "    jz " << Ldone << "\n";
        code_ << "    lda dx\n    ani 1\n";
        code_ << "    jz " << Lskipadd << "\n";
        loadPair("d", "bc");
        code_ << "    lhld ac\n    dad d\n    shld ac\n";
        code_ << Lskipadd << ":\n";
        shiftOneBit("bc", /*isLeft=*/true);
        shiftOneBit("dx", /*isLeft=*/false);
        code_ << "    dcr c\n";
        code_ << "    jmp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        code_ << "    shld bc\n";  // bc = old (value) - HL already holds it
        code_ << "    mov c, e\n";  // c = shift count (low byte of rhs)
        std::string Lloop = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    mov a,c\n    ora a\n";
        code_ << "    jz " << Ldone << "\n";
        if (op == "<<" || isUnsigned) {
            shiftOneBit("bc", /*isLeft=*/op == "<<");
        } else {
            code_ << "    lhld bc\n";
            code_ << "    mov a,h\n    mov b,a\n    ral\n    mov a,b\n    rar\n    mov h,a\n";
            code_ << "    mov a,l\n    rar\n    mov l,a\n";
            code_ << "    shld bc\n";
        }
        code_ << "    dcr c\n";
        code_ << "    jmp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        copy16("ac", "bc");
        return;
    }
    // "/" or "%"
    code_ << "    shld dx\n";  // dx = old (dividend) - HL already holds it
    storePair("bc", "d");       // bc = rhs (divisor)
    code_ << "    lxi h, 0\n    shld ac\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    loadPair("d", "dx");
    code_ << "    lhld bc\n";
    code_ << "    mov a,e\n    sub l\n    mov c,a\n";
    code_ << "    mov a,d\n    sbb h\n";
    code_ << "    jc " << Ldone << "\n";
    code_ << "    mov d,a\n    mov e,c\n";
    storePair("dx", "d");
    code_ << "    lhld ac\n    inx h\n    shld ac\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (op == "%") copy16("ac", "dx");
}

// The target's own address is a pure function of `e.slot` (see this
// method's own declaration comment), so it's computed once before
// loading `old`, and again - fresh, no state carried across `rhs`'s
// own evaluation - right before the final store; only `old` itself
// (and, for a postfix `++`/`--`, a second copy of it for the final
// result) needs the real stack to survive evaluating `rhs`.
void I8080Codegen::genCompoundScalar(const ir::Expr &e) {
    auto genAddr = [&] {
        if (e.slot->kind == ir::SlotKind::Global) {
            code_ << "    lxi h, " << label(e.slot) << "\n";
        } else {
            genFrameAddr(e.slot);
        }
    };
    genAddr();
    loadOperand(e.type);  // ac = old
    code_ << "    lhld ac\n";
    code_ << "    push h\n";  // stash old across rhs's own evaluation
    genExpr(*e.rhs);            // ac = rhs
    loadPair("d", "ac");          // de = rhs
    code_ << "    pop h\n";        // hl = old
    if (e.yieldOld) code_ << "    push h\n";  // re-stash old for the final result
    genCompoundOp(e.op, e.type);                // ac = new (raw)
    narrowAcTo(e.type);
    if (e.yieldOld) {
        code_ << "    pop h\n";  // hl = old
        code_ << "    shld cx\n";
    }
    genAddr();
    storeOperand(e.type);  // stores ac(=new); ac already = new if !yieldOld
    if (e.yieldOld) {
        code_ << "    lhld cx\n";
        code_ << "    shld ac\n";  // ac = old (final result)
    }
}

// `ptr` must already hold the resolved address on entry. The address
// is always shuttled back through the real hardware stack across
// evaluating `rhs` (`ptr` is a single shared scratch cell, not a
// stack - see I8080Codegen.h's own header comment - and `rhs`'s own
// evaluation may recurse into arbitrarily complex codegen that reuses
// `ptr`, e.g. a nested compound assignment's own identical addressing
// problem). Once `rhs` has been evaluated, nothing further here
// recurses into another expression, so the shared `cx` cell (declared
// once in generate(), never written by any other existing codegen in
// this file) is safe to use as a transient `yieldOld` stash for the
// remainder of this one call.
void I8080Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    lhld ptr\n";
    loadOperand(e.type);  // ac = old
    code_ << "    lhld ptr\n";
    code_ << "    push h\n";  // stash address
    code_ << "    lhld ac\n";
    code_ << "    push h\n";  // stash old (ac is about to be overwritten by rhs)
    genExpr(*e.rhs);            // ac = rhs
    loadPair("d", "ac");          // de = rhs
    code_ << "    pop h\n";        // hl = old
    if (e.yieldOld) code_ << "    push h\n";  // re-stash old for the final result
    genCompoundOp(e.op, e.type);                // ac = new (raw)
    narrowAcTo(e.type);
    if (e.yieldOld) {
        code_ << "    pop h\n";  // hl = old
        code_ << "    shld cx\n";
        code_ << "    pop h\n";   // hl = address
        storeOperand(e.type);      // stores ac(=new) through hl
        code_ << "    lhld cx\n";
        code_ << "    shld ac\n";    // ac = old (final result)
    } else {
        code_ << "    pop h\n";  // hl = address
        storeOperand(e.type);      // ac already = new
    }
}

// Same narrow-to-narrow re-extension genConvert()'s own `!fromWide &&
// !toWide` case already does - a no-op unless `to` is a `char`.
void I8080Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    code_ << "    lhld ac\n";
    if (to.isUnsigned) {
        code_ << "    mvi h, 0\n";
    } else {
        code_ << "    mov a,l\n    ral\n    sbb a\n    mov h,a\n";
    }
    code_ << "    shld ac\n";
}

void I8080Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    lxi h, " << label(operand.slot) << "\n    shld ac\n";
        } else {
            genFrameAddr(operand.slot);
            code_ << "    shld ac\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    lxi h, " << label(operand.func) << "\n    shld ac\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a Local's
// storage is this activation's own fresh frame region, not pre-set to
// anything; arrayInit is always fully populated up to arraySize
// (zero-padded even with no source-level initializer), so this always
// leaves the whole array in a known state, same as a Global array's
// static trailer always did. Element i's own address is
// `frameByteOffset(slot) - 2*i` (see I8080Codegen.h's own header
// comment on why a Local array's elements sit at decreasing addresses
// as i increases) - computed directly here rather than through
// genFrameAddr(), which only ever knows `slot`'s own single offset.
void I8080Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        long base = static_cast<long>(i) * w;
        if (w == 1 && !elem.isSymbol) {
            genFrameAddr(slot, base);
            code_ << "    mvi a, " << static_cast<int>(static_cast<uint8_t>(elem.value)) << "\n    mov m,a\n";
            continue;
        }
        if (elem.isSymbol || w <= 2) {
            code_ << "    lxi h, " << arrayElemText(elem) << "\n    shld ac\n";
            genFrameAddr(slot, base);
            storeAcIndirect();
            continue;
        }
        // A wide (`long`/`long long`) element - 2 or 4 words, least
        // significant first, same layout as every other wide value's
        // own in-memory representation elsewhere in this file.
        auto uv = static_cast<unsigned long>(elem.value);
        int words = w / 2;
        for (int j = 0; j < words; j++) {
            uint16_t wv = static_cast<uint16_t>((uv >> (16 * j)) & 0xFFFF);
            code_ << "    lxi h, " << wv << "\n    shld ac\n";
            genFrameAddr(slot, base + 2 * j);
            storeAcIndirect();
        }
    }
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* 8080 hardware stack (`push h`/`pop d`),
// never in a named memory cell like `bc` - the right operand's own
// genExpr() is a general recursive call that might itself be another
// binary op (or genMul()/genDivMod()/genShift(), which all share the
// same `bc`/`cx`/`dx` cells), so a named cell holding "the other
// operand" would get clobbered the moment a nested one reuses it.
// The real stack has no such aliasing problem - it's genuinely
// unique call-depth storage, same reason the 6502 backend's own
// PHA/PLA sequences serve this exact role (see M6502Codegen.cpp).
void I8080Codegen::genBinary(const ir::Expr &e) {
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
        code_ << "    lhld ac\n    push h\n";  // save lhs on the real stack across evaluating rhs - see this
                                                // function's own header comment on why a named memory cell can't
                                                // be used for that instead
        genExpr(*e.rhs);                        // ac = rhs
        code_ << "    pop d\n";                  // DE = lhs (popped only now, after rhs is fully evaluated)
        if (op == "+") {
            code_ << "    lhld ac\n    dad d\n    shld ac\n";
        } else {
            code_ << "    lhld ac\n";  // HL = rhs
            code_ << "    mov a,e\n    sub l\n    mov e,a\n";
            code_ << "    mov a,d\n    sbb h\n    mov d,a\n";
            storePair("ac", "d");
        }
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
        code_ << "    lhld ac\n    push h\n";
        genExpr(*e.rhs);
        code_ << "    pop d\n";    // DE = lhs
        code_ << "    lhld ac\n";  // HL = rhs
        const char *insn = (op == "&") ? "ana" : (op == "|") ? "ora" : "xra";
        code_ << "    mov a,l\n    " << insn << " e\n    mov l,a\n";
        code_ << "    mov a,h\n    " << insn << " d\n    mov h,a\n";
        code_ << "    shld ac\n";
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

// No variable-count shift instruction - loop `rhs` times instead, one
// bit at a time, via shiftOneBit() (see I8080Codegen.h's own header
// comment, and the same fix every other software-shift backend here
// already needs for the same reason). `isUnsigned` only matters for
// `>>` - see genWideShift()'s own comment on the same signed/
// unsigned distinction, done here with the same `mov b,a`/`ral`/
// `mov a,b`/`rar` software-SRA idiom for just the top byte.
void I8080Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the count - see
                                            // genBinary()'s own header comment on why not a named cell
    genExpr(rhs);                           // ac = shift count
    code_ << "    lhld ac\n    mov c,l\n";  // c = shift count (always small; low byte is enough)
    code_ << "    pop h\n    shld bc\n";     // bc = value (restored only now, after the count is safely in c)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a,c\n    ora a\n";
    code_ << "    jz " << Ldone << "\n";
    if (isLeft || isUnsigned) {
        shiftOneBit("bc", isLeft);
    } else {
        code_ << "    lhld bc\n";
        code_ << "    mov a,h\n    mov b,a\n    ral\n    mov a,b\n    rar\n    mov h,a\n";
        code_ << "    mov a,l\n    rar\n    mov l,a\n";
        code_ << "    shld bc\n";
    }
    code_ << "    dcr c\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply - see
// I8080Codegen.h's own header comment): `bc` is the multiplicand
// (doubled each iteration), `dx` the multiplier (halved each
// iteration, consumed one bit at a time), `ac` the accumulating
// product. Only verified against non-negative operands.
void I8080Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);                          // ac = multiplicand
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the multiplier
    genExpr(rhs);
    code_ << "    pop h\n    shld bc\n";  // bc = multiplicand (restored only now)
    copy16("dx", "ac");                     // dx = multiplier
    code_ << "    lxi h, 0\n    shld ac\n";  // ac = product = 0
    code_ << "    mvi c, 16\n";               // loop counter, in the real register C
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a,c\n    ora a\n";
    code_ << "    jz " << Ldone << "\n";
    code_ << "    lda dx\n    ani 1\n";
    code_ << "    jz " << Lskipadd << "\n";
    loadPair("d", "bc");
    code_ << "    lhld ac\n    dad d\n    shld ac\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dcr c\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide - see I8080Codegen.h's own
// header comment): `dx` is the running remainder (starts as the
// dividend), `bc` the fixed divisor, `ac` the quotient, counting how
// many times `bc` was subtracted out of `dx` before it would've gone
// negative. Only verified against non-negative operands.
void I8080Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);                          // ac = dividend
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop h\n    shld dx\n";  // dx = dividend (restored only now)
    copy16("bc", "ac");                     // bc = divisor
    code_ << "    lxi h, 0\n    shld ac\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    loadPair("d", "dx");
    code_ << "    lhld bc\n";
    // Tentative dx - bc; carry (borrow) set afterward means dx < bc,
    // so this round's subtraction can't happen - stop.
    code_ << "    mov a,e\n    sub l\n    mov c,a\n";  // c = tentative low result (c is scratch here, not "the
                                                        // divisor" - bc's own value was already consumed into HL)
    code_ << "    mov a,d\n    sbb h\n";
    code_ << "    jc " << Ldone << "\n";
    // Commit: dx -= bc.
    code_ << "    mov d,a\n    mov e,c\n";
    storePair("dx", "d");
    // Quotient += 1 - a real 16-bit increment (`inx h`), unlike the
    // 6502's own carry-chained inc16() workaround.
    code_ << "    lhld ac\n    inx h\n    shld ac\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void I8080Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false, isUnsigned);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false, isUnsigned);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true, isUnsigned);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true, isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b, checked the same
// "OR the two result bytes together" way genLogicalNot() checks a
// value for zero.
void I8080Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating rhs
    genExpr(rhs);
    code_ << "    pop d\n";    // DE = lhs
    code_ << "    lhld ac\n";  // HL = rhs
    code_ << "    mov a,e\n    sub l\n    mov c,a\n";
    code_ << "    mov a,d\n    sbb h\n";
    code_ << "    ora c\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (wantEqual ? "z" : "nz") << " " << Ltrue << "\n";
    code_ << "    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// I8080Codegen.h's own header comment on why the 8080, unlike every
// other backend here, has no hardware overflow flag to combine with
// the result's sign bit instead): flipping just the top bit of each
// operand's high byte maps the signed range onto the unsigned one
// while preserving order, so a plain unsigned compare (the final
// borrow out of the high-byte subtraction) is all that's left to do.
void I8080Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned) {
    genExpr(a);
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating b
    genExpr(b);
    code_ << "    pop d\n";    // DE = a
    code_ << "    lhld ac\n";  // HL = b
    if (!isUnsigned) {
        code_ << "    mov a,d\n    xri 0x80\n    mov d,a\n";  // D = a's high byte, sign flipped
        code_ << "    mov a,h\n    xri 0x80\n    mov h,a\n";  // H = b's high byte, sign flipped
    }
    code_ << "    mov a,e\n    sub l\n";                  // a'_lo - b'_lo, CY = borrow
    code_ << "    mov a,d\n    sbb h\n";                  // a'_hi - b'_hi - borrow; CY now = (a < b) unbiased
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (invert ? "nc" : "c") << " " << Ltrue << "\n";
    code_ << "    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jz " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jz " << Lfalse << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    lxi h, 0\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jnz " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jnz " << Ltrue << "\n";
    code_ << Lfalse << ":\n    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jz " << Ltrue << "\n";
    code_ << "    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

// After genExpr(e) just left `e`'s own value - `ac`, if narrow, or
// `wordCountOf(e.type)` words on the real stack (LSW on top) if wide
// (see ../z80/Z80Codegen.h's own header comment on this convention) -
// this leaves the real Z flag set exactly as a plain `ora h` would,
// with any wide stack space already released, ready for an
// immediate `jz`/`jnz`.
void I8080Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    lhld ac\n    mov a,l\n    ora h\n";
        return;
    }
    int words = wordCountOf(e.type);
    popWideInto("a", words);
    auto bytes = wideByteLabelsLsbFirst("a", words);
    code_ << "    mvi c, 0\n";
    for (const auto &b : bytes) {
        code_ << "    lda " << b << "\n    ora c\n    mov c,a\n";
    }
    code_ << "    mov a,c\n    ora a\n";
}

std::string I8080Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 8080 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I8080Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleI8080(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly
    // once (see I8080Assembler.h's own `assembleI8080Object`), and
    // whichever of these names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI8080Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
