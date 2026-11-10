#include "AVRCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "AVRAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Fixed scratch register names - see AVRCodegen.h's own header comment for each one's role.
const std::string kT0 = "r20";
const std::string kT1 = "r21";
const std::string kT2 = "r22";
const std::string kZero = "r23";
const std::string kTmp = "r24";
const std::string kTmp2 = "r25";

constexpr uint32_t kRamEnd = kAVRSramBaseAddress + 2048 - 1;
}  // namespace

AVRCodegen::Place AVRCodegen::canonicalPlace(int width) const {
    if (width <= 2) return Place{false, 16, ""};
    return Place{true, 0, "__wa"};
}
AVRCodegen::Place AVRCodegen::secondaryPlace(int width) const {
    if (width <= 2) return Place{false, 18, ""};
    return Place{true, 0, "__wb"};
}

void AVRCodegen::loadByte(const Place &p, int i, const std::string &reg) {
    if (p.isSram) {
        code_ << "    lds " << reg << ", " << p.label << (i ? "+" + std::to_string(i) : "") << "\n";
        return;
    }
    std::string src = regName(p.baseReg + i);
    if (src != reg) code_ << "    mov " << reg << ", " << src << "\n";
}
void AVRCodegen::storeByte(const Place &p, int i, const std::string &reg) {
    if (p.isSram) {
        code_ << "    sts " << p.label << (i ? "+" + std::to_string(i) : "") << ", " << reg << "\n";
        return;
    }
    std::string dst = regName(p.baseReg + i);
    if (dst != reg) code_ << "    mov " << dst << ", " << reg << "\n";
}

// Byte 0 (least significant) is pushed *last*, so it ends up at the lowest address (the real
// stack top) - this gives a Place's bytes the same ascending, LSB-first memory order a
// Local/Param/Global's own storage uses, which matters for genExpr(Call)'s argument pushes
// (popPlace()'s own order doesn't otherwise care, since it only has to invert whatever pushPlace()
// just did).
void AVRCodegen::pushPlace(const Place &p, int width) {
    for (int i = width - 1; i >= 0; i--) {
        loadByte(p, i, kT0);
        code_ << "    push " << kT0 << "\n";
    }
}
void AVRCodegen::popPlace(const Place &p, int width) {
    for (int i = 0; i < width; i++) {
        code_ << "    pop " << kT0 << "\n";
        storeByte(p, i, kT0);
    }
}

void AVRCodegen::copyPlace(const Place &dst, const Place &src, int width) {
    for (int i = 0; i < width; i++) {
        loadByte(src, i, kT0);
        storeByte(dst, i, kT0);
    }
}

void AVRCodegen::extendPlace(const Place &p, int fromWidth, int toWidth, bool isUnsigned) {
    if (isUnsigned) {
        for (int i = fromWidth; i < toWidth; i++) storeByte(p, i, kZero);
        return;
    }
    loadByte(p, fromWidth - 1, kT0);
    code_ << "    add " << kT0 << ", " << kT0 << "\n";  // lsl (no dedicated opcode - see MUL/ADD family)
    code_ << "    ldi " << kT1 << ", 0\n";
    std::string Lskip = newLabel();
    // Carry now holds the sign bit (see above): skip filling with 0xFF unless it's set.
    safeBranchIfFalse("brlo", Lskip);
    code_ << "    ldi " << kT1 << ", 255\n";
    code_ << Lskip << ":\n";
    for (int i = fromWidth; i < toWidth; i++) storeByte(p, i, kT1);
}

void AVRCodegen::safeBranchIfTrue(const std::string &branchMnemonic, const std::string &target) {
    std::string Lnear = newLabel();
    std::string Lskip = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << Lskip << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << target << "\n";
    code_ << Lskip << ":\n";
}
void AVRCodegen::safeBranchIfFalse(const std::string &branchMnemonic, const std::string &target) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << target << "\n";
    code_ << Lnear << ":\n";
}

void AVRCodegen::testCanonicalNonzero(int width) {
    Place c = canonicalPlace(width);
    loadByte(c, 0, kT0);
    for (int i = 1; i < width; i++) {
        loadByte(c, i, kT1);
        code_ << "    or " << kT0 << ", " << kT1 << "\n";
    }
    code_ << "    and " << kT0 << ", " << kT0 << "\n";
}

void AVRCodegen::addConst16(int loReg, int32_t value) {
    if (value == 0) return;
    uint16_t v = static_cast<uint16_t>(value);
    code_ << "    ldi " << kTmp << ", " << (v & 0xFF) << "\n";
    code_ << "    ldi " << kTmp2 << ", " << ((v >> 8) & 0xFF) << "\n";
    code_ << "    add " << regName(loReg) << ", " << kTmp << "\n";
    code_ << "    adc " << regName(loReg + 1) << ", " << kTmp2 << "\n";
}

void AVRCodegen::adjustSP(int delta) {
    if (delta == 0) return;
    code_ << "    in r30, 0x3d\n    in r31, 0x3e\n";
    addConst16(30, delta);
    code_ << "    out 0x3e, r31\n    out 0x3d, r30\n";
}

void AVRCodegen::materializeAddr(const ir::Slot *slot, int byteOffset) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldiw r30, " << label(slot) << (byteOffset ? "+" + std::to_string(byteOffset) : "") << "\n";
        return;
    }
    code_ << "    movw r30, r28\n";
    addConst16(30, frameDisplacement(slot) + byteOffset);
}

void AVRCodegen::negatePlace(const Place &place, int width) {
    for (int i = 0; i < width; i++) {
        loadByte(place, i, kT1);
        code_ << "    com " << kT1 << "\n";
        storeByte(place, i, kT1);
    }
    for (int i = 0; i < width; i++) {
        loadByte(place, i, kT1);
        code_ << "    " << (i == 0 ? "subi" : "sbci") << " " << kT1 << ", 255\n";
        storeByte(place, i, kT1);
    }
}

// +1 throughout: right after the prologue, Y equals the current SP, which PUSH always writes to
// *before* decrementing - so Y+0 is never safe storage (a later, unrelated push - e.g. saving a
// value across a nested call - would overwrite it before anything reads it back). Every real byte
// this frame owns (Locals, the saved caller-Y, the return address, Params) therefore starts at
// Y+1, not Y+0.
int AVRCodegen::frameDisplacement(const ir::Slot *slot) const {
    if (slot->kind == ir::SlotKind::Param) return curFrameSize_ + slot->frameOffset + 1;
    return curFrameSize_ - ir::localLowOffset(*slot, typeWidths()) + 1;
}

void AVRCodegen::loadSlotInto(const ir::Slot *slot, const Place &place, int width) {
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < width; i++) {
            code_ << "    lds " << kT0 << ", " << label(slot) << (i ? "+" + std::to_string(i) : "") << "\n";
            storeByte(place, i, kT0);
        }
        return;
    }
    int disp = frameDisplacement(slot);
    if (disp >= 0 && disp + width - 1 <= 63) {
        for (int i = 0; i < width; i++) {
            code_ << "    ldd " << kT0 << ", " << (disp + i) << "(r28)\n";
            storeByte(place, i, kT0);
        }
        return;
    }
    materializeAddr(slot, 0);
    for (int i = 0; i < width; i++) {
        code_ << "    ld " << kT0 << ", ai (r30)\n";
        storeByte(place, i, kT0);
    }
}

void AVRCodegen::storeSlotFrom(const ir::Slot *slot, const Place &place, int width) {
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < width; i++) {
            loadByte(place, i, kT0);
            code_ << "    sts " << label(slot) << (i ? "+" + std::to_string(i) : "") << ", " << kT0 << "\n";
        }
        return;
    }
    int disp = frameDisplacement(slot);
    if (disp >= 0 && disp + width - 1 <= 63) {
        for (int i = 0; i < width; i++) {
            loadByte(place, i, kT0);
            code_ << "    std " << (disp + i) << "(r28), " << kT0 << "\n";
        }
        return;
    }
    materializeAddr(slot, 0);
    for (int i = 0; i < width; i++) {
        loadByte(place, i, kT0);
        code_ << "    st ai (r30), " << kT0 << "\n";
    }
}

void AVRCodegen::loadImmediateInto(const Place &place, int width, uint64_t value) {
    for (int i = 0; i < width; i++) {
        int byteVal = static_cast<int>((value >> (8 * i)) & 0xFF);
        if (place.isSram) {
            code_ << "    ldi " << kT0 << ", " << byteVal << "\n";
            storeByte(place, i, kT0);
        } else {
            code_ << "    ldi " << regName(place.baseReg + i) << ", " << byteVal << "\n";
        }
    }
}

void AVRCodegen::loadFromX(const Place &place, int width) {
    for (int i = 0; i < width; i++) {
        code_ << "    ld " << kT0 << ", ai (r26)\n";
        storeByte(place, i, kT0);
    }
}
void AVRCodegen::storeToX(const Place &place, int width) {
    for (int i = 0; i < width; i++) {
        loadByte(place, i, kT0);
        code_ << "    st ai (r26), " << kT0 << "\n";
    }
}

// `elemShift(slot->type)` scales a runtime index by the element width (1/2/4/8 bytes). A Global or
// Local array's elements sit at increasing addresses as the index grows (base+i*w), matching every
// other target here; a plain pointer variable's own stored value is the runtime base instead.
void AVRCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    int elemW = widthOfType(slot->type);
    int shift = elemW == 1 ? 0 : elemW == 2 ? 1 : elemW == 4 ? 2 : 3;
    if (slot->arraySize > 0) {
        materializeAddr(slot, 0);
        code_ << "    movw r26, r30\n";
    } else {
        loadSlotInto(slot, Place{false, 26, ""}, 2);
    }
    genExpr(idxExpr);  // -> canonicalPlace(2) = r16:r17
    for (int k = 0; k < shift; k++) code_ << "    add r16, r16\n    adc r17, r17\n";
    code_ << "    add r26, r16\n    adc r27, r17\n";
}

void AVRCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    int w = widthOfType(e.slot->type);
    loadFromX(canonicalPlace(w), w);
}

void AVRCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    ldiw r16, " << label(operand.slot) << "\n";
        } else {
            materializeAddr(operand.slot, 0);
            code_ << "    movw r16, r30\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movw r16, r26\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ldiwc r16, " << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void AVRCodegen::combineBytes(const std::string &op, int width, const Place &acc, const Place &operand) {
    for (int i = 0; i < width; i++) {
        loadByte(acc, i, kT0);
        loadByte(operand, i, kT1);
        std::string insn = op == "+" ? (i == 0 ? "add" : "adc")
                          : op == "-" ? (i == 0 ? "sub" : "sbc")
                          : op == "&" ? "and"
                          : op == "|" ? "or"
                                      : "eor";
        code_ << "    " << insn << " " << kT0 << ", " << kT1 << "\n";
        storeByte(acc, i, kT0);
    }
}

void AVRCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    int w = widthOfType(lhs.type);
    bool isUnsigned = lhs.type.isUnsigned;
    Place canon = canonicalPlace(w), sec = secondaryPlace(w);
    genExpr(lhs);
    pushPlace(canon, w);
    genExpr(rhs);
    popPlace(sec, w);
    // canon = rhs, sec = lhs.
    std::string mnemonic;
    bool swap = false;
    if (op == "==") {
        mnemonic = "breq";
    } else if (op == "!=") {
        mnemonic = "brne";
    } else if (op == "<") {
        mnemonic = isUnsigned ? "brlo" : "brlt";
    } else if (op == ">=") {
        mnemonic = isUnsigned ? "brsh" : "brge";
    } else if (op == ">") {
        mnemonic = isUnsigned ? "brlo" : "brlt";
        swap = true;
    } else {  // "<="
        mnemonic = isUnsigned ? "brsh" : "brge";
        swap = true;
    }
    const Place &a = swap ? canon : sec;
    const Place &b = swap ? sec : canon;
    for (int i = 0; i < w; i++) {
        loadByte(a, i, kT0);
        loadByte(b, i, kT1);
        code_ << "    " << (i == 0 ? "cp" : "cpc") << " " << kT0 << ", " << kT1 << "\n";
    }
    std::string Ltrue = newLabel(), Ldone = newLabel();
    safeBranchIfTrue(mnemonic, Ltrue);
    loadImmediateInto(canonicalPlace(2), 2, 0);
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    loadImmediateInto(canonicalPlace(2), 2, 1);
    code_ << Ldone << ":\n";
}

void AVRCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testCanonicalNonzero(widthOfType(lhs.type));
    safeBranchIfTrue("breq", Lfalse);
    genExpr(rhs);
    testCanonicalNonzero(widthOfType(rhs.type));
    safeBranchIfTrue("breq", Lfalse);
    loadImmediateInto(canonicalPlace(2), 2, 1);
    code_ << "    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    loadImmediateInto(canonicalPlace(2), 2, 0);
    code_ << Ldone << ":\n";
}

void AVRCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testCanonicalNonzero(widthOfType(lhs.type));
    safeBranchIfTrue("brne", Ltrue);
    genExpr(rhs);
    testCanonicalNonzero(widthOfType(rhs.type));
    safeBranchIfTrue("brne", Ltrue);
    loadImmediateInto(canonicalPlace(2), 2, 0);
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    loadImmediateInto(canonicalPlace(2), 2, 1);
    code_ << Ldone << ":\n";
}

void AVRCodegen::genLogicalNot(const ir::Expr &rhs) {
    std::string Lzero = newLabel(), Ldone = newLabel();
    genExpr(rhs);
    testCanonicalNonzero(widthOfType(rhs.type));
    safeBranchIfTrue("breq", Lzero);
    loadImmediateInto(canonicalPlace(2), 2, 0);
    code_ << "    jmp " << Ldone << "\n";
    code_ << Lzero << ":\n";
    loadImmediateInto(canonicalPlace(2), 2, 1);
    code_ << Ldone << ":\n";
}

void AVRCodegen::genDivMod(int width, bool wantRemainder, bool isUnsigned) {
    Place canon = canonicalPlace(width), sec = secondaryPlace(width);
    // sec = dividend (lhs), canon = divisor (rhs).
    const std::string kFlagDividendNeg = kTmp2;
    const std::string kFlagDivisorNeg = "r30";
    if (!isUnsigned) {
        code_ << "    eor " << kFlagDividendNeg << ", " << kFlagDividendNeg << "\n";
        code_ << "    eor " << kFlagDivisorNeg << ", " << kFlagDivisorNeg << "\n";
        std::string Lskip1 = newLabel();
        loadByte(sec, width - 1, kT0);
        code_ << "    add " << kT0 << ", " << kT0 << "\n";
        safeBranchIfFalse("brlo", Lskip1);
        negatePlace(sec, width);
        code_ << "    ldi " << kFlagDividendNeg << ", 1\n";
        code_ << Lskip1 << ":\n";
        std::string Lskip2 = newLabel();
        loadByte(canon, width - 1, kT0);
        code_ << "    add " << kT0 << ", " << kT0 << "\n";
        safeBranchIfFalse("brlo", Lskip2);
        negatePlace(canon, width);
        code_ << "    ldi " << kFlagDivisorNeg << ", 1\n";
        code_ << Lskip2 << ":\n";
    }
    Place wc{true, 0, "__wc"}, wd{true, 0, "__wd"};
    copyPlace(wc, sec, width);
    for (int i = 0; i < width; i++) storeByte(wd, i, kZero);
    code_ << "    ldi " << kTmp << ", " << (8 * width) << "\n";
    std::string Lbody = newLabel(), Lskip = newLabel();
    code_ << Lbody << ":\n";
    for (int i = 0; i < width; i++) {
        loadByte(wc, i, kT0);
        code_ << "    " << (i == 0 ? "add" : "adc") << " " << kT0 << ", " << kT0 << "\n";
        storeByte(wc, i, kT0);
    }
    for (int i = 0; i < width; i++) {
        loadByte(wd, i, kT0);
        code_ << "    adc " << kT0 << ", " << kT0 << "\n";
        storeByte(wd, i, kT0);
    }
    for (int i = 0; i < width; i++) {
        loadByte(wd, i, kT0);
        loadByte(canon, i, kT1);
        code_ << "    " << (i == 0 ? "cp" : "cpc") << " " << kT0 << ", " << kT1 << "\n";
    }
    safeBranchIfTrue("brlo", Lskip);
    for (int i = 0; i < width; i++) {
        loadByte(wd, i, kT0);
        loadByte(canon, i, kT1);
        code_ << "    " << (i == 0 ? "sub" : "sbc") << " " << kT0 << ", " << kT1 << "\n";
        storeByte(wd, i, kT0);
    }
    loadByte(wc, 0, kT0);
    code_ << "    ldi " << kT1 << ", 1\n";
    code_ << "    or " << kT0 << ", " << kT1 << "\n";
    storeByte(wc, 0, kT0);
    code_ << Lskip << ":\n";
    code_ << "    dec " << kTmp << "\n";
    safeBranchIfTrue("brne", Lbody);

    if (!isUnsigned) {
        std::string Lskip3 = newLabel(), Lskip4 = newLabel();
        code_ << "    mov " << kT0 << ", " << kFlagDividendNeg << "\n";
        code_ << "    eor " << kT0 << ", " << kFlagDivisorNeg << "\n";
        code_ << "    and " << kT0 << ", " << kT0 << "\n";
        safeBranchIfFalse("brne", Lskip3);
        negatePlace(wc, width);
        code_ << Lskip3 << ":\n";
        code_ << "    mov " << kT0 << ", " << kFlagDividendNeg << "\n";
        code_ << "    and " << kT0 << ", " << kT0 << "\n";
        safeBranchIfFalse("brne", Lskip4);
        negatePlace(wd, width);
        code_ << Lskip4 << ":\n";
    }
    copyPlace(canon, wantRemainder ? wd : wc, width);
}

// The well-known 3-`mul` 16x16->16 sequence: the cross terms' own high bytes only affect a third
// result byte, which a 16-bit result doesn't keep (C's `*` wraps silently) - so only their low
// bytes are added in.
void AVRCodegen::genNarrowMul() {
    // canonical (r16:r17) = rhs, secondary (r18:r19) = lhs.
    code_ << "    mul r18, r16\n";
    code_ << "    mov " << kT2 << ", r0\n";
    code_ << "    mov " << kTmp << ", r1\n";
    code_ << "    mul r19, r16\n";
    code_ << "    add " << kTmp << ", r0\n";
    code_ << "    mul r18, r17\n";
    code_ << "    add " << kTmp << ", r0\n";
    code_ << "    mov r16, " << kT2 << "\n";
    code_ << "    mov r17, " << kTmp << "\n";
}

void AVRCodegen::genWideMul(int width) {
    Place canon = canonicalPlace(width), sec = secondaryPlace(width);  // canon=rhs, sec=lhs
    Place wc{true, 0, "__wc"};
    for (int i = 0; i < width; i++) storeByte(wc, i, kZero);
    auto propagateCarry = [&](int from) {
        for (int i = from; i < width; i++) {
            loadByte(wc, i, kT2);
            code_ << "    adc " << kT2 << ", " << kZero << "\n";
            storeByte(wc, i, kT2);
        }
    };
    for (int s = 0; s < width; s++) {
        for (int i = 0; i <= s; i++) {
            int j = s - i;
            if (i >= width || j >= width) continue;
            loadByte(sec, i, kT0);
            loadByte(canon, j, kT1);
            code_ << "    mul " << kT0 << ", " << kT1 << "\n";
            loadByte(wc, s, kT2);
            code_ << "    add " << kT2 << ", r0\n";
            storeByte(wc, s, kT2);
            propagateCarry(s + 1);
            if (s + 1 < width) {
                loadByte(wc, s + 1, kT2);
                code_ << "    add " << kT2 << ", r1\n";
                storeByte(wc, s + 1, kT2);
                propagateCarry(s + 2);
            }
        }
    }
    copyPlace(canon, wc, width);
}

void AVRCodegen::genShift(int width, bool isLeft, bool isUnsigned) {
    Place c = canonicalPlace(width);
    std::string Lbody = newLabel(), Ldone = newLabel();
    code_ << "    and " << kTmp << ", " << kTmp << "\n";
    safeBranchIfFalse("brne", Ldone);
    code_ << Lbody << ":\n";
    if (isLeft) {
        for (int i = 0; i < width; i++) {
            loadByte(c, i, kT0);
            code_ << "    " << (i == 0 ? "add" : "adc") << " " << kT0 << ", " << kT0 << "\n";
            storeByte(c, i, kT0);
        }
    } else {
        loadByte(c, width - 1, kT0);
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " " << kT0 << "\n";
        storeByte(c, width - 1, kT0);
        for (int i = width - 2; i >= 0; i--) {
            loadByte(c, i, kT0);
            code_ << "    ror " << kT0 << "\n";
            storeByte(c, i, kT0);
        }
    }
    code_ << "    dec " << kTmp << "\n";
    safeBranchIfTrue("brne", Lbody);
    code_ << Ldone << ":\n";
}

void AVRCodegen::genCompoundOp(const std::string &op, Type type) {
    int w = widthOfType(type);
    if (op == "+" || op == "-" || op == "&" || op == "|" || op == "^") {
        combineBytes(op, w, canonicalPlace(w), secondaryPlace(w));
        return;
    }
    if (op == "*") {
        if (w <= 2) {
            genNarrowMul();
        } else {
            genWideMul(w);
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(w, op == "<<", type.isUnsigned);
        return;
    }
    if (op == "/" || op == "%") {
        // genDivMod() expects the dividend in secondaryPlace and the divisor in canonicalPlace
        // (genBinary()'s own lhs/rhs evaluation order); here canonical holds `old` (the dividend)
        // and secondary holds `rhs` (the divisor) - the reverse role - so their contents are
        // swapped first.
        Place canon = canonicalPlace(w), sec = secondaryPlace(w);
        for (int i = 0; i < w; i++) {
            loadByte(canon, i, kT0);
            loadByte(sec, i, kT1);
            storeByte(canon, i, kT1);
            storeByte(sec, i, kT0);
        }
        genDivMod(w, op == "%", type.isUnsigned);
        return;
    }
    err(0, "internal: unhandled compound operator '" + op + "'");
}

void AVRCodegen::genCompoundThroughX(const ir::Expr &e) {
    int w = widthOfType(e.type);
    bool isShift = e.op == "<<" || e.op == ">>";
    Place sec = secondaryPlace(w);
    // Two independent snapshots of X survive genExpr(e.rhs) (which may itself use X) - one for
    // the old-value load, one for the final store, since loadFromX()/storeToX() each advance X.
    // They're pushed store-snapshot first so a yieldOld "old" value, pushed later in between, can
    // still be popped out from on top of it before the store snapshot is needed - genCompoundOp()
    // leaves `sec` (rhs, no longer needed once it's run) free to hold "old" across that pop.
    code_ << "    push r27\n    push r26\n";  // for the store
    code_ << "    push r27\n    push r26\n";  // for the old-value load
    genExpr(*e.rhs);
    if (isShift) {
        loadByte(canonicalPlace(widthOfType(e.rhs->type)), 0, kTmp);
    } else {
        copyPlace(sec, canonicalPlace(w), w);
    }
    code_ << "    pop r26\n    pop r27\n";
    loadFromX(canonicalPlace(w), w);  // canonical = old
    if (e.yieldOld) pushPlace(canonicalPlace(w), w);
    genCompoundOp(e.op, e.type);  // canonical = new; `sec` is free again
    if (e.yieldOld) popPlace(sec, w);
    code_ << "    pop r26\n    pop r27\n";
    storeToX(canonicalPlace(w), w);
    if (e.yieldOld) copyPlace(canonicalPlace(w), sec, w);
}

// Every arithmetic/comparison operand narrower than `int` is already promoted by IRBuilder, so
// the only real work here is crossing the narrow(<=2)/wide(>=4) place boundary, or re-extending a
// narrower target's unused upper byte.
void AVRCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    int fw = widthOfType(from), tw = widthOfType(to);
    genExpr(*e.lhs);  // -> canonicalPlace(fw)
    if (fw == tw) return;
    if (fw > tw) {
        if (fw >= 4 && tw <= 2) copyPlace(canonicalPlace(tw), canonicalPlace(fw), tw);
        return;
    }
    if (fw <= 2 && tw <= 2) {
        extendPlace(canonicalPlace(2), fw, tw, from.isUnsigned);
        return;
    }
    copyPlace(canonicalPlace(tw), canonicalPlace(fw), fw);
    extendPlace(canonicalPlace(tw), fw, tw, from.isUnsigned);
}

void AVRCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t idx = 0; idx < slot->arrayInit.size(); idx++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[idx];
        int at = w * static_cast<int>(idx);
        materializeAddr(slot, at);  // Z = &slot[idx]
        if (elem.isSymbol) {
            const std::string target = elem.slot ? label(elem.slot) : label(elem.func);
            if (elem.func) {
                code_ << "    ldiwc r16, " << target << "\n";
            } else {
                code_ << "    ldiw r16, " << target << "\n";
            }
            code_ << "    st ai (r30), r16\n";
            if (w > 1) code_ << "    st ai (r30), r17\n";
            for (int i = 2; i < w; i++) code_ << "    st ai (r30), " << kZero << "\n";
            continue;
        }
        auto v = static_cast<uint64_t>(elem.value);
        for (int i = 0; i < w; i++) {
            code_ << "    ldi " << kT0 << ", " << static_cast<int>((v >> (8 * i)) & 0xFF) << "\n";
            code_ << "    st ai (r30), " << kT0 << "\n";
        }
    }
}

void AVRCodegen::declareGlobal(const ir::Slot *slot) {
    std::string lbl = label(slot);
    int w = widthOfType(slot->type);
    ramDecls_.push_back(lbl + ": ram " + std::to_string(w));
    auto v = static_cast<uint64_t>(slot->hasInit ? slot->initValue : 0);
    for (int i = 0; i < w; i++) {
        initCode_ << "    ldi " << kT0 << ", " << static_cast<int>((v >> (8 * i)) & 0xFF) << "\n";
        initCode_ << "    sts " << lbl << (i ? "+" + std::to_string(i) : "") << ", " << kT0 << "\n";
    }
}

void AVRCodegen::genArrayElementInit(const std::string &elemLabel, Type elemType, const ir::ArrayInitElem &e) {
    int w = widthOfType(elemType);
    if (e.isSymbol) {
        if (e.func) {
            initCode_ << "    ldiwc r16, " << label(e.func) << "\n";
        } else {
            initCode_ << "    ldiw r16, " << label(e.slot) << "\n";
        }
        initCode_ << "    sts " << elemLabel << ", r16\n";
        if (w > 1) initCode_ << "    sts " << elemLabel << "+1, r17\n";
        for (int i = 2; i < w; i++) initCode_ << "    sts " << elemLabel << "+" << i << ", " << kZero << "\n";
        return;
    }
    auto v = static_cast<uint64_t>(e.value);
    for (int i = 0; i < w; i++) {
        initCode_ << "    ldi " << kT0 << ", " << static_cast<int>((v >> (8 * i)) & 0xFF) << "\n";
        initCode_ << "    sts " << elemLabel << (i ? "+" + std::to_string(i) : "") << ", " << kT0 << "\n";
    }
}

void AVRCodegen::declareGlobalArray(const ir::Slot *slot) {
    std::string lbl = label(slot);
    int ew = widthOfType(slot->type);
    ramDecls_.push_back(lbl + ": ram " + std::to_string(ew * slot->arraySize));
    static const ir::ArrayInitElem kZeroElem{};
    for (int idx = 0; idx < slot->arraySize; idx++) {
        std::string elemLbl = lbl + (idx ? "+" + std::to_string(idx * ew) : "");
        const ir::ArrayInitElem &e =
            (static_cast<size_t>(idx) < slot->arrayInit.size()) ? slot->arrayInit[idx] : kZeroElem;
        genArrayElementInit(elemLbl, slot->type, e);
    }
}

void AVRCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        genCompare(op, *e.lhs, *e.rhs);
        return;
    }
    Type operandType = e.lhs->type;
    int w = widthOfType(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "<<" || op == ">>") {
        Place c = canonicalPlace(w);
        genExpr(*e.lhs);
        pushPlace(c, w);
        genExpr(*e.rhs);  // -> canonicalPlace(2), the shift count
        code_ << "    mov " << kTmp << ", r16\n";
        popPlace(c, w);
        genShift(w, op == "<<", isUnsigned);
        return;
    }
    Place canon = canonicalPlace(w), sec = secondaryPlace(w);
    genExpr(*e.lhs);
    pushPlace(canon, w);
    genExpr(*e.rhs);
    popPlace(sec, w);
    // sec = lhs, canon = rhs.
    if (op == "+" || op == "-" || op == "&" || op == "|" || op == "^") {
        combineBytes(op, w, sec, canon);
        copyPlace(canon, sec, w);
        return;
    }
    if (op == "*") {
        if (w <= 2) {
            genNarrowMul();
        } else {
            genWideMul(w);
        }
        return;
    }
    if (op == "/" || op == "%") {
        genDivMod(w, op == "%", isUnsigned);
        return;
    }
    err(e.line, "internal: unhandled binary operator '" + op + "'");
}

void AVRCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            int w = widthOfType(e.type);
            loadImmediateInto(canonicalPlace(w), w, static_cast<uint64_t>(e.intValue));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotValue(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {
                genIndexAddr(e.slot, *e.lhs);
                code_ << "    push r27\n    push r26\n";
                genExpr(*e.rhs);
                code_ << "    pop r26\n    pop r27\n";
                storeToX(canonicalPlace(widthOfType(e.slot->type)), widthOfType(e.slot->type));
                return;
            }
            genExpr(*e.rhs);
            storeSlotValue(e.slot);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            int w = widthOfType(e.type);
            genExpr(*e.lhs);  // pointer value -> canonicalPlace(2)
            code_ << "    movw r26, r16\n";
            code_ << "    push r27\n    push r26\n";
            genExpr(*e.rhs);
            code_ << "    pop r26\n    pop r27\n";
            storeToX(canonicalPlace(w), w);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {
                genIndexAddr(e.slot, *e.lhs);
                genCompoundThroughX(e);
                return;
            }
            int w = widthOfType(e.type);
            bool isShift = e.op == "<<" || e.op == ">>";
            genExpr(*e.rhs);
            if (isShift) {
                loadByte(canonicalPlace(widthOfType(e.rhs->type)), 0, kTmp);
            } else {
                copyPlace(secondaryPlace(w), canonicalPlace(w), w);
            }
            loadSlotInto(e.slot, canonicalPlace(w), w);  // canonical = old
            if (e.yieldOld) pushPlace(canonicalPlace(w), w);
            genCompoundOp(e.op, e.type);
            storeSlotFrom(e.slot, canonicalPlace(w), w);
            if (e.yieldOld) popPlace(canonicalPlace(w), w);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // pointer value -> canonicalPlace(2)
            code_ << "    movw r26, r16\n";
            genCompoundThroughX(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            int w = widthOfType(e.type);
            std::string Lelse = newLabel(), Ldone = newLabel();
            genExpr(*e.lhs);
            testCanonicalNonzero(widthOfType(e.lhs->type));
            safeBranchIfFalse("brne", Lelse);
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            (void)w;
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                int w = widthOfType(e.type);
                genExpr(*e.rhs);
                negatePlace(canonicalPlace(w), w);
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                int w = widthOfType(e.type);
                genExpr(*e.rhs);  // pointer value -> canonicalPlace(2)
                code_ << "    movw r26, r16\n";
                loadFromX(canonicalPlace(w), w);
            } else if (e.op == "~") {
                int w = widthOfType(e.rhs->type);
                genExpr(*e.rhs);
                Place c = canonicalPlace(w);
                for (int i = 0; i < w; i++) {
                    loadByte(c, i, kT0);
                    code_ << "    com " << kT0 << "\n";
                    storeByte(c, i, kT0);
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Call: {
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                int aw = widthOfType(e.args[i]->type);
                int pushWidth = aw > 2 ? aw : 2;
                genExpr(*e.args[i]);
                if (pushWidth > aw) extendPlace(canonicalPlace(aw), aw, pushWidth, /*isUnsigned=*/true);
                pushPlace(canonicalPlace(pushWidth), pushWidth);
                totalBytes += pushWidth;
            }
            code_ << "    call " << label(e.func) << "\n";
            adjustSP(totalBytes);
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);  // a flash *byte* address (see ldiwc's own comment) -> canonicalPlace(2)
            code_ << "    movw r30, r16\n";
            code_ << "    lsr r31\n    ror r30\n";  // halve into a word address for `icall`
            code_ << "    icall\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Func: {
            err(e.line, "internal: a bare function name has no value outside '&'");
        }
        case ir::ExprKind::Intrinsic: {
            // No interrupt vectors are set up on this bare-metal target, so interrupt
            // enable/disable is a no-op.
            (void)e;
            return;
        }
    }
}

void AVRCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            genExpr(*s.cond);
            testCanonicalNonzero(widthOfType(s.cond->type));
            safeBranchIfFalse("brne", s.elseStmt ? Lelse : Ldone);
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
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testCanonicalNonzero(widthOfType(s.cond->type));
            safeBranchIfFalse("brne", Lend);
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
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            genEpilogue();
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeSlotValue(s.declSlot);
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
            code_ << "    jmp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

// Real hardware stack frame: pushes the caller's Y, then lowers Y (and, through it, SP) by
// `fn->frameSize` so Y addresses this call's own Locals at ascending displacements - see this
// file's own header comment for the full layout.
void AVRCodegen::genPrologue(const ir::Function *fn) {
    curFrameSize_ = fn->frameSize;
    code_ << "    push r28\n    push r29\n";
    code_ << "    in r28, 0x3d\n    in r29, 0x3e\n";
    addConst16(28, -curFrameSize_);
    code_ << "    out 0x3e, r29\n    out 0x3d, r28\n";
}

void AVRCodegen::genEpilogue() {
    addConst16(28, curFrameSize_);
    code_ << "    out 0x3e, r29\n    out 0x3d, r28\n";
    code_ << "    pop r29\n    pop r28\n";
}

void AVRCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    genPrologue(fn.func);
    genStmt(*fn.body);
    genEpilogue();
    code_ << "    ret\n\n";
}

std::string AVRCodegen::generate(const ir::Program &prog, bool objectMode) {
    perFileAsm_.clear();
    ramDecls_.clear();
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    // Every Global's SRAM reservation/initializer is assigned up front, before any code is
    // generated, so a Var read has it regardless of textual order.
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        if (slot->arraySize > 0) {
            declareGlobalArray(slot.get());
        } else {
            declareGlobal(slot.get());
        }
    }

    std::string full;
    auto flush = [&](const std::string &file) {
        std::string chunk = code_.str();
        code_.str("");
        code_.clear();
        full += chunk;
        perFileAsm_[file] += chunk;
    };

    code_ << "org 0\n\n";
    if (!objectMode) {
        if (!mainFn) err(0, "no 'main' function defined");
        code_ << "_start:\n";
        code_ << "    eor " << kZero << ", " << kZero << "\n";
        code_ << "    ldi r16, " << (kRamEnd & 0xFF) << "\n";
        code_ << "    out 0x3d, r16\n";
        code_ << "    ldi r16, " << ((kRamEnd >> 8) & 0xFF) << "\n";
        code_ << "    out 0x3e, r16\n";
        code_ << initCode_.str();
        code_ << "    call " << label(mainFn) << "\n";
        code_ << "done:\n    jmp done\n\n";
    }
    flush(mainFn ? mainFn->sourceFile : std::string());

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // This file's globals' init code has nowhere else to run in object mode - the
            // synthesized `_start` is built before seeing any user object, so `fn_main`'s
            // prologue (the one point this file's code is guaranteed to run first) takes over,
            // the zero register included (see MCS51Codegen.cpp's own `ssp` init for the same
            // split).
            code_ << label(fb.func) << ":\n";
            genPrologue(fb.func);
            code_ << "    eor " << kZero << ", " << kZero << "\n";
            code_ << initCode_.str();
            genStmt(*fb.body);
            genEpilogue();
            code_ << "    ret\n\n";
        } else {
            genFunction(fb);
        }
        flush(fb.func->sourceFile);
    }

    for (const auto &decl : ramDecls_) code_ << decl << "\n";
    // In object mode, AVRLinkerFactory.cpp's own entry object declares these once for the whole
    // linked program instead (see its own comment) - a `long`/`long long` value staged through
    // one of them has to resolve to the same four buffers regardless of which object does the
    // staging and which reads it back.
    if (!objectMode) code_ << "__wa: ram 8\n__wb: ram 8\n__wc: ram 8\n__wd: ram 8\n";
    flush(mainFn ? mainFn->sourceFile : std::string());

    return full;
}

std::map<std::string, std::string> AVRCodegen::perFileAsm() const { return perFileAsm_; }

std::string AVRCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the avr backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports");
}

std::optional<std::string> AVRCodegen::assembleInProcess(const std::string &asmText, const std::string &format) const {
    if (format == "raw") return assembleAVR(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleAVRObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
