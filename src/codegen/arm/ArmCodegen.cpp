#include "ArmCodegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "ArmAssembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly.
constexpr uint32_t kBaseAddress = 0x1000u;

// Where the entry sequence points `sp` before calling main() -
// comfortably above the loaded program and its data.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global) is the referenced Slot's or
// Function's label, written out bare - ArmAssembler.cpp's `dw` evaluator
// resolves it the same way a label operand resolves for MOVW/MOVT.
std::string ArmCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void ArmCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long` array's
    // literal elements are emitted as two "dw"s (low word, then high) per
    // element; a symbol element is always a native-int-width address, so
    // it still gets one "dw".
    const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<uint32_t>(v) << ", " << static_cast<uint32_t>(v >> 32);
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
}

void ArmCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movw " << reg << "," << lbl << "\n";
    code_ << "    movt " << reg << "," << lbl << "\n";
}

void ArmCodegen::emitFrameOffset(const std::string &reg, int offsetBytes, bool isAdd) {
    if (offsetBytes == 0) return;
    if (offsetBytes <= 255) {
        code_ << "    " << (isAdd ? "add" : "sub") << " " << reg << "," << reg << "," << offsetBytes << "\n";
        return;
    }
    std::string scratch = (reg == "r1") ? "r3" : "r1";
    uint32_t uv = static_cast<uint32_t>(offsetBytes);
    code_ << "    movw " << scratch << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
    if ((uv >> 16) != 0) code_ << "    movt " << scratch << "," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
    code_ << "    " << (isAdd ? "add" : "sub") << " " << reg << "," << reg << "," << scratch << "\n";
}

void ArmCodegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot, int elemIndex) {
    code_ << "    mov " << reg << ",fp\n";
    // `frameOffset` is already a byte offset (from the slot's declared
    // type's width); `elemIndex` is scaled by the array's element width.
    if (slot->kind == ir::SlotKind::Param) {
        emitFrameOffset(reg, slot->frameOffset, /*isAdd=*/true);
    } else {
        emitFrameOffset(reg, ir::localLowOffset(*slot, typeWidths()) - elemIndex * widthOfType(slot->type),
                        /*isAdd=*/false);
    }
}

void ArmCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
    } else {
        loadFrameAddr(reg, slot, 0);
    }
}

void ArmCodegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        if (e.slot) {
            loadSlotAddr(reg, e.slot);
        } else {
            loadAddress(reg, label(e.func));
        }
        return;
    }
    uint32_t uv = static_cast<uint32_t>(e.value);
    code_ << "    movw " << reg << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
    if ((uv >> 16) != 0) code_ << "    movt " << reg << "," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
}

void ArmCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &e = slot->arrayInit[i];
        loadArrayInitElem("r0", e);
        if (w == 8 && !e.isSymbol) {
            // loadArrayInitElem() only fills r0; fill r1 (the high word)
            // here too for a long long literal element. A symbol element
            // is always a native-int-width address, so stays single-word.
            auto v = static_cast<uint64_t>(e.value);
            uint32_t hi = static_cast<uint32_t>(v >> 32);
            code_ << "    movw r1," << static_cast<int32_t>(hi & 0xFFFFu) << "\n";
            if ((hi >> 16) != 0) code_ << "    movt r1," << static_cast<int32_t>((hi >> 16) & 0xFFFFu) << "\n";
        }
        loadFrameAddr("r2", slot, static_cast<int>(i));
        storeOperand("r2", slot->type);
    }
}

void ArmCodegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    // Re-truncate+re-extend by shifting the unwanted high bits out and
    // back in - zero-fill (`lsr`) for unsigned, sign-fill (`asr`) for
    // signed. ARM has no dedicated sign/zero-extend instruction here.
    if (w == 1) {
        code_ << "    lsl r0,r0,24\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,24\n";
    } else if (w == 2) {
        code_ << "    lsl r0,r0,16\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,16\n";
    }
}

// A byte-at-a-time word load, little-endian, tolerating any byte
// boundary - LDRB/STRB never fault on alignment, unlike LDR/STR. `r3`
// holds the running byte address (`addrReg` is read, never written), `r4`
// the byte just transferred; `destReg` is assembled one OR per byte.
void ArmCodegen::loadWordUnaligned(const std::string &addrReg, const std::string &destReg) {
    code_ << "    ldindb " << addrReg << ",r4\n";
    code_ << "    mov " << destReg << ",r4\n";
    for (int byteIdx = 1; byteIdx < 4; byteIdx++) {
        code_ << "    mov r3," << addrReg << "\n    add r3,r3," << byteIdx << "\n    ldindb r3,r4\n";
        code_ << "    lsl r4,r4," << (byteIdx * 8) << "\n    orr " << destReg << "," << destReg << ",r4\n";
    }
}
void ArmCodegen::storeWordUnaligned(const std::string &addrReg, const std::string &srcReg) {
    code_ << "    stindb " << addrReg << "," << srcReg << "\n";
    for (int byteIdx = 1; byteIdx < 4; byteIdx++) {
        code_ << "    mov r4," << srcReg << "\n    lsr r4,r4," << (byteIdx * 8) << "\n";
        code_ << "    mov r3," << addrReg << "\n    add r3,r3," << byteIdx << "\n    stindb r3,r4\n";
    }
}
void ArmCodegen::loadHalfUnaligned(const std::string &addrReg, const std::string &destReg, bool isUnsigned) {
    code_ << "    ldindb " << addrReg << "," << destReg << "\n";
    code_ << "    mov r3," << addrReg << "\n    add r3,r3,1\n    ldindb r3,r4\n";
    code_ << "    lsl r4,r4,8\n    orr " << destReg << "," << destReg << ",r4\n";
    if (!isUnsigned) {
        code_ << "    lsl " << destReg << "," << destReg << ",16\n    asr " << destReg << "," << destReg << ",16\n";
    }
}
void ArmCodegen::storeHalfUnaligned(const std::string &addrReg, const std::string &srcReg) {
    code_ << "    stindb " << addrReg << "," << srcReg << "\n";
    code_ << "    mov r4," << srcReg << "\n    lsr r4,r4,8\n";
    code_ << "    mov r3," << addrReg << "\n    add r3,r3,1\n    stindb r3,r4\n";
}

// `addrReg` holds the already-computed effective address (r2, by
// convention); loads into r0 (or r0:r1, for a `long long`), sign- or
// zero-extending a narrower-than-native-int value per its signedness.
void ArmCodegen::loadOperand(const std::string &addrReg, Type type, bool mayBeUnaligned) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb " : "ldindsb ") << addrReg << ",r0\n";
    } else if (w == 2) {
        if (mayBeUnaligned) {
            loadHalfUnaligned(addrReg, "r0", type.isUnsigned);
        } else {
            code_ << "    " << (type.isUnsigned ? "ldindh " : "ldindsh ") << addrReg << ",r0\n";
        }
    } else if (w == 4) {
        if (mayBeUnaligned) {
            loadWordUnaligned(addrReg, "r0");
        } else {
            code_ << "    ldind " << addrReg << ",r0\n";
        }
    } else if (mayBeUnaligned) {
        loadWordUnaligned(addrReg, "r0");
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        loadWordUnaligned(addrReg, "r1");
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    } else {
        code_ << "    ldind " << addrReg << ",r0\n";
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        code_ << "    ldind " << addrReg << ",r1\n";
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    }
}

void ArmCodegen::storeOperand(const std::string &addrReg, Type type, bool mayBeUnaligned) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",r0\n";
    } else if (w == 2) {
        if (mayBeUnaligned) {
            storeHalfUnaligned(addrReg, "r0");
        } else {
            code_ << "    stindh " << addrReg << ",r0\n";
        }
    } else if (w == 4) {
        if (mayBeUnaligned) {
            storeWordUnaligned(addrReg, "r0");
        } else {
            code_ << "    stind " << addrReg << ",r0\n";
        }
    } else if (mayBeUnaligned) {
        storeWordUnaligned(addrReg, "r0");
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        storeWordUnaligned(addrReg, "r1");
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    } else {
        code_ << "    stind " << addrReg << ",r0\n";
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        code_ << "    stind " << addrReg << ",r1\n";
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    }
}

// An implicit or explicit type conversion. Arithmetic/comparison operands
// narrower than `int` are already promoted by IRBuilder, so the only
// real work here is crossing the native-int/`long long` boundary, or
// re-truncating+re-extending r0 for a narrower target - see narrowR0To().
void ArmCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        code_ << "    " << (from.isUnsigned ? "mov r1,0\n" : "asr r1,r0,31\n");
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

void ArmCodegen::genNegate64() {
    // Two's complement negate of r0:r1 in place - `adc`'s immediate-0
    // operand2 reads the carry out of the preceding `adds`.
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    adds r0,r0,1\n    adc r1,r1,0\n";
}

// `long long` add/sub (lhs OP rhs) - evaluate both into r0:r1, stashing
// lhs's two words on the stack while rhs evaluates, then ripple the
// carry/borrow across the word pair with adc/sbc.
void ArmCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4=rhs_lo, r5=rhs_hi
    code_ << "    ldind sp,r0\n    add sp,sp,4\n";
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r0:r1 = lhs restored
    if (isAdd) {
        code_ << "    adds r0,r0,r4\n    adc r1,r1,r5\n";
    } else {
        code_ << "    subs r0,r0,r4\n    sbc r1,r1,r5\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit
// product: result_lo = lo(lhs)*lo(rhs) mod 2^32; result_hi =
// hi32(lo(lhs)*lo(rhs)) + lo(lhs)*hi(rhs) + hi(lhs)*lo(rhs), all mod 2^32.
// Only the low*low term needs a widening multiply (`umull`) - the other
// two only ever contribute to result_hi, so `mul`'s truncating 32x32->32
// result is exactly right for them.
void ArmCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    mov r6,r0\n    mov r7,r1\n";      // r6=rhs_lo, r7=rhs_hi
    code_ << "    ldind sp,r4\n    add sp,sp,4\n";  // r4 = lhs_lo
    code_ << "    ldind sp,r5\n    add sp,sp,4\n";  // r5 = lhs_hi
    code_ << "    mul r8,r4,r7\n";                   // r8 = lhs_lo*rhs_hi
    code_ << "    mul r9,r5,r6\n";                   // r9 = lhs_hi*rhs_lo
    code_ << "    umull r0,r1,r4,r6\n";               // r0:r1 = lhs_lo*rhs_lo (full 64-bit)
    code_ << "    add r1,r1,r8\n    add r1,r1,r9\n";
}

// Bit-serial shift/compare/subtract long division (no A32 instruction is
// wide enough for a 64/64 divide): r0:r1 = dividend (consumed,
// shifted left MSB-first) -> final quotient, r2:r3 = divisor, r4:r5 =
// remainder, r6:r7 = quotient (being built), r8 = iteration countdown,
// r9/r10/r12 = scratch. `isUnsigned` false divides magnitudes: each
// operand's sign bit is stashed on the stack across the loop and
// restored on the result afterward per C's truncating-division rule.
void ArmCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    mov r2,r0\n    mov r3,r1\n";      // r2:r3 = divisor
    code_ << "    ldind sp,r0\n    add sp,sp,4\n";
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r0:r1 = dividend

    std::string LskipNegN, LskipNegD;
    if (!isUnsigned) {
        code_ << "    lsr r9,r1,31\n";   // r9 = dividend's own sign
        code_ << "    lsr r10,r3,31\n";  // r10 = divisor's own sign
        code_ << "    sub sp,sp,4\n    stind sp,r9\n";
        code_ << "    sub sp,sp,4\n    stind sp,r10\n";  // stack (top->bottom): divisorSign, dividendSign
        LskipNegN = newLabel();
        code_ << "    cmp r9,0\n    beq " << LskipNegN << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    adds r0,r0,1\n    adc r1,r1,0\n";
        code_ << LskipNegN << ":\n";
        LskipNegD = newLabel();
        code_ << "    cmp r10,0\n    beq " << LskipNegD << "\n";
        code_ << "    mvn r2,r2\n    mvn r3,r3\n    adds r2,r2,1\n    adc r3,r3,0\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    mov r4,0\n    mov r5,0\n    mov r6,0\n    mov r7,0\n    mov r8,64\n";

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n    cmp r8,0\n    beq " << Ldone << "\n    sub r8,r8,1\n";

    // dividend <<= 1 (0-filled), capturing its own old top bit (r9).
    code_ << "    lsr r9,r1,31\n";
    code_ << "    lsr r10,r0,31\n";
    code_ << "    lsl r0,r0,1\n";
    code_ << "    lsl r1,r1,1\n    orr r1,r1,r10\n";

    // remainder = (remainder << 1) | (dividend's old top bit, r9).
    code_ << "    lsr r10,r4,31\n";
    code_ << "    lsl r4,r4,1\n    orr r4,r4,r9\n";
    code_ << "    lsl r5,r5,1\n    orr r5,r5,r10\n";

    // if (remainder >= divisor) { remainder -= divisor; quotient bit = 1 } else quotient bit = 0
    // (always an unsigned magnitude compare - both operands were
    // already negated to non-negative above for the signed case).
    code_ << "    cmp r5,r3\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r4,r2\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    subs r4,r4,r2\n    sbc r5,r5,r3\n";
    code_ << "    mov r12,1\n    b " << LshiftQ << "\n";
    code_ << LsubNo << ":\n    mov r12,0\n";
    code_ << LshiftQ << ":\n";

    // quotient = (quotient << 1) | quotient bit (r12).
    code_ << "    lsr r10,r6,31\n";
    code_ << "    lsl r6,r6,1\n    orr r6,r6,r12\n";
    code_ << "    lsl r7,r7,1\n    orr r7,r7,r10\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    ldind sp,r10\n    add sp,sp,4\n";  // r10 = divisorSign
        code_ << "    ldind sp,r9\n    add sp,sp,4\n";   // r9 = dividendSign
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    eor r12,r9,r10\n    cmp r12,0\n    beq " << LskipNegQ << "\n";
        code_ << "    mvn r6,r6\n    mvn r7,r7\n    adds r6,r6,1\n    adc r7,r7,0\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    cmp r9,0\n    beq " << LskipNegR << "\n";
        code_ << "    mvn r4,r4\n    mvn r5,r5\n    adds r4,r4,1\n    adc r5,r5,0\n";
        code_ << LskipNegR << ":\n";
    }
    if (wantRemainder) {
        code_ << "    mov r0,r4\n    mov r1,r5\n";
    } else {
        code_ << "    mov r0,r6\n    mov r1,r7\n";
    }
}

// A `long long` shifted by a runtime count - no A32 instruction shifts
// more than 32 bits at once, so this is a counted loop of 1-bit shifts,
// each carrying the bit that crosses the word boundary by hand.
// `isUnsigned` only matters for `>>`: each step's high-word shift is
// `asr` (sign-preserving) or `lsr` (zero-filling) accordingly.
void ArmCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);  // r0 = shift count (already promoted to a plain int)
    code_ << "    mov r4,r0\n";
    code_ << "    ldind sp,r0\n    add sp,sp,4\n";
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r0:r1 restored = the value being shifted
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmp r4,0\n    beq " << Ldone << "\n    sub r4,r4,1\n";
    if (isLeft) {
        code_ << "    lsr r5,r0,31\n";  // r5 = r0's old top bit, carries into r1
        code_ << "    lsl r0,r0,1\n    lsl r1,r1,1\n    orr r1,r1,r5\n";
    } else {
        code_ << "    lsl r5,r1,31\n";  // r5 = (r1&1)<<31, carries into r0's top
        code_ << "    lsr r0,r0,1\n    orr r0,r0,r5\n";
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " r1,r1,1\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison - compare the high words first (signed or
// unsigned condition per `isUnsigned`, since that's where the sign bit
// lives); only if equal does the low word matter, and that comparison is
// always unsigned regardless of the overall type's signedness.
void ArmCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";      // r4:r5 = rhs
    code_ << "    ldind sp,r0\n    add sp,sp,4\n";
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r0:r1 = lhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmp r0,r4\n    bne " << Lfalse << "\n";
        code_ << "    cmp r1,r5\n    bne " << Lfalse << "\n";
        code_ << "    mov r0," << onEqual << "\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    mov r0," << onDiffer << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string signedCC = op == "<" ? "lt" : op == "<=" ? "le" : op == ">" ? "gt" : "ge";
    std::string unsignedCC = op == "<" ? "lo" : op == "<=" ? "ls" : op == ">" ? "hi" : "hs";
    std::string hiCC = isUnsigned ? unsignedCC : signedCC;
    std::string LhiDiff = newLabel();
    code_ << "    cmp r1,r5\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r0,r4\n    b" << unsignedCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    b" << hiCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << Lfalse << ":\n    mov r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov r0,1\n";
    code_ << Ldone << ":\n";
}

std::string ArmCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Separate compilation means `prog` may not define `main` at all.
    // `objectMode` means no entry sequence is emitted here -
    // ArmLinkerFactory.cpp synthesizes one once every object is merged
    // and `main`'s linked address is resolvable. An object file's base is
    // `org 0`; placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movw sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    movt sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt`.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Running byte offset within the data segment (4-byte aligned from
    // the start), used to insert alignment padding ahead of a global
    // wider than 1 byte: an unaligned LDRH/STRH/LDR/STR access is
    // UNPREDICTABLE on this core and would silently round down to the
    // previous aligned address, clobbering whatever global sits there. A
    // `long long` only needs word alignment - it's always accessed as two
    // separate word-sized ldind/stind.
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file - it must never
        // get a local storage word, only be referenced by name, letting
        // the assembler's undefined-symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        int align = std::min(widthOfType(slot->type), 4);
        size_t padding = (align - (dataOffset % align)) % align;
        if (padding > 0) {
            std::string fill = "0";
            for (size_t i = 1; i < padding; i++) fill += ", 0";
            trailer_.push_back("db " + fill);
            dataOffset += padding;
        }
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
            dataOffset += static_cast<size_t>(widthOfType(slot->type)) * static_cast<size_t>(slot->arraySize);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + std::string(directive) + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
            dataOffset += static_cast<size_t>(w);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> ArmCodegen::perFileAsm() const { return perFileAsm_; }

// Every function saves/restores `lr` around its body, whether or not it
// calls anything else: ARM has exactly one link register, and a non-leaf
// function's own `BL` overwrites it, so without this save/restore the
// eventual `bx lr` would return to the wrong place. Always saving/
// restoring is simpler than tracking which functions are leaf.
//
// Every function also gets a per-call stack frame: after saving `lr`, it
// saves the caller's `fp`, points `fp` at that saved pair, and reserves
// `frameSize` bytes below it for its Locals (see loadFrameAddr() for the
// resulting address layout). This makes direct or indirect recursion
// safe: every activation gets its own `fp` and reserved Local words.
void ArmCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,sp,4\n    stind sp,lr\n";
    code_ << "    sub sp,sp,4\n    stind sp,fp\n";
    code_ << "    mov fp,sp\n";
    if (fn.func->frameSize > 0) emitFrameOffset("sp", fn.func->frameSize, /*isAdd=*/false);

    genStmt(*fn.body);

    code_ << "    mov sp,fp\n";
    code_ << "    ldind sp,fp\n    add sp,sp,4\n";
    code_ << "    ldind sp,lr\n    add sp,sp,4\n";
    code_ << "    bx lr\n\n";
}

void ArmCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r0,r1\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp r0,0\n    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    b " << Ldone << "\n";
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
            if (isWide(s.cond->type)) code_ << "    orr r0,r0,r1\n";
            code_ << "    cmp r0,0\n    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    b " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    b " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // An early return bypasses genFunction()'s fall-through
            // epilogue, so it needs this same frame teardown inline.
            code_ << "    mov sp,fp\n";
            code_ << "    ldind sp,fp\n    add sp,sp,4\n";
            code_ << "    ldind sp,lr\n    add sp,sp,4\n";
            code_ << "    bx lr\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadSlotAddr("r2", s.declSlot);
                code_ << "    stind r2,r0\n";
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
            code_ << "    b " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void ArmCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                uint32_t lo = static_cast<uint32_t>(v), hi = static_cast<uint32_t>(v >> 32);
                code_ << "    movw r0," << static_cast<int32_t>(lo & 0xFFFFu) << "\n";
                if ((lo >> 16) != 0) code_ << "    movt r0," << static_cast<int32_t>((lo >> 16) & 0xFFFFu) << "\n";
                code_ << "    movw r1," << static_cast<int32_t>(hi & 0xFFFFu) << "\n";
                if ((hi >> 16) != 0) code_ << "    movt r1," << static_cast<int32_t>((hi >> 16) & 0xFFFFu) << "\n";
                return;
            }
            uint32_t uv = static_cast<uint32_t>(e.intValue);
            code_ << "    movw r0," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
            if ((uv >> 16) != 0) {
                code_ << "    movt r0," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("r2", e.slot);
            loadOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    sub sp,sp,4\n    stind sp,r2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,r2\n    add sp,sp,4\n";
                storeOperand("r2", e.slot->type, /*mayBeUnaligned=*/e.slot->arraySize == 0);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            storeOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                genCompoundThroughR2(e, /*mayBeUnaligned=*/e.slot->arraySize == 0);
                return;
            }
            // A plain scalar target still needs its address computed
            // (ARM has no absolute-addressing memory operand), so it
            // goes through the same held-address tail as the indexed form.
            loadSlotAddr("r2", e.slot);
            genCompoundThroughR2(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r2,r0\n";  // r2 = address
            genCompoundThroughR2(e, /*mayBeUnaligned=*/true);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            // The condition's type can still be wide, so this reuses
            // If/While's wide-OR-then-compare idiom, not a bare r0 test.
            if (isWide(e.lhs->type)) code_ << "    orr r0,r0,r1\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp r0,0\n    beq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    b " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness - the pointee type of a
            // typed pointer, or native-int-width for untyped-int-as-address.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    sub sp,sp,4\n    stind sp,r0\n";
            genExpr(*e.rhs);  // r0, or r0:r1 if wide
            code_ << "    ldind sp,r2\n    add sp,sp,4\n";
            storeOperand("r2", e.type, /*mayBeUnaligned=*/true);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    rsb r0,r0,0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r2,r0\n";
                loadOperand("r2", e.type, /*mayBeUnaligned=*/true);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn r0,r0\n";
                if (isWide(e.rhs->type)) code_ << "    mvn r1,r1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed right-to-left on the hardware stack,
            // so arg 0 ends up closest to the saved fp/lr pair, at `fp+8`
            // inside the callee. The callee never cleans these up itself,
            // so the caller restores `sp` here once the call returns.
            // Every argument reserves at least one native word, and a wide
            // (`long long`) argument pushes its high word first so its low
            // word ends up closer to `fp`, matching a wide Param's
            // frameOffset/frameOffset+4 layout.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    sub sp,sp,4\n    stind sp,r1\n";
                    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
                    totalBytes += 8;
                } else {
                    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
                    totalBytes += 4;
                }
            }
            code_ << "    bl " << label(e.func) << "\n";
            if (totalBytes > 0) emitFrameOffset("sp", totalBytes, /*isAdd=*/true);
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind, since the callee isn't known at
            // compile time. `blx r2` is a near call through whatever
            // address r2 holds, setting `lr` like `bl` does.
            genExpr(*e.lhs);  // r0 = the callee's entry address
            code_ << "    mov r2,r0\n";
            code_ << "    blx r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No portable interrupt enable/disable on this bare-metal
            // core with no exception/GIC setup - treated as a no-op.
            (void)e;
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

// `r2` holds the computed address. `arraySize > 0` means the base is the
// array's own address (loadSlotAddr() dispatches Global vs. Local);
// otherwise `slot` is a plain pointer variable, so the base comes from
// reading that variable's stored value first. Either array's element
// addresses increase with `idx`.
void ArmCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    lsl r0,r0," << shift << "\n";  // r0 *= the element type's own width
    if (slot->arraySize > 0) {
        loadSlotAddr("r2", slot);
        code_ << "    add r2,r2,r0\n";
    } else {
        loadSlotAddr("r1", slot);
        code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's own stored value
        code_ << "    add r2,r1,r0\n";
    }
}

void ArmCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("r2", e.slot->type, /*mayBeUnaligned=*/e.slot->arraySize == 0);
}

void ArmCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("r2", operand.slot);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("r2", label(operand.func));
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Entry: r0 = old, r1 = rhs (both already at `type`'s width/signedness).
void ArmCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add r0,r0,r1\n"; return; }
    if (op == "-") { code_ << "    sub r0,r0,r1\n"; return; }
    if (op == "*") { code_ << "    mul r0,r0,r1\n"; return; }
    if (op == "&") { code_ << "    and r0,r0,r1\n"; return; }
    if (op == "|") { code_ << "    orr r0,r0,r1\n"; return; }
    if (op == "^") { code_ << "    eor r0,r0,r1\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r0,r1\n";
        return;
    }
    // "/" or "%" - r3 (genDivMod()'s own quotient-holding scratch) is
    // free to reuse here: by this point e.rhs has already fully
    // evaluated, so nothing still needs whatever r3 held during that
    // evaluation.
    code_ << "    " << (isUnsigned ? "udiv" : "sdiv") << " r3,r0,r1\n";
    if (op == "%") {
        code_ << "    mls r0,r3,r1,r0\n";  // r0 = r0 - r3*r1 = old - quotient*rhs
    } else {
        code_ << "    mov r0,r3\n";
    }
}

// r2 must already hold the resolved address on entry. The address is
// stashed on the stack across evaluating e.rhs (which can reach
// arbitrary nested codegen using r0 through r5) and never recomputed, so
// an indexed/pointer target's address sub-expression is evaluated exactly once.
void ArmCodegen::genCompoundThroughR2(const ir::Expr &e, bool mayBeUnaligned) {
    code_ << "    sub sp,sp,4\n    stind sp,r2\n";  // stash address
    genExpr(*e.rhs);                                  // r0 = rhs
    code_ << "    mov r1,r0\n";                        // r1 = rhs
    code_ << "    ldind sp,r2\n    add sp,sp,4\n";      // r2 = address (restored)
    loadOperand("r2", e.type, mayBeUnaligned);            // r0 = old
    if (e.yieldOld) code_ << "    sub sp,sp,4\n    stind sp,r0\n";  // stash old
    genCompoundOp(e.op, e.type);                           // r0 = new (raw)
    narrowR0To(e.type);
    if (e.yieldOld) {
        code_ << "    mov r3,r0\n";                          // r3 = new
        code_ << "    ldind sp,r0\n    add sp,sp,4\n";         // r0 = old
        code_ << "    mov r1,r0\n";                             // r1 = old, preserved across the store below
        code_ << "    mov r0,r3\n";                              // r0 = new - storeOperand always stores from r0
        storeOperand("r2", e.type, mayBeUnaligned);
        code_ << "    mov r0,r1\n";                               // r0 = old (final result)
    } else {
        storeOperand("r2", e.type, mayBeUnaligned);  // r0 already = new
    }
}

void ArmCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here shares one common
    // type (IRBuilder's usual-arithmetic-conversion pass), except shift
    // (only the LHS's type matters) and &&/|| (width-independent).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r0,r1,r0\n";  // r0 = lhs + rhs
        } else {
            code_ << "    sub r0,r1,r0\n";  // r0 = lhs - rhs
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "orr" : "eor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    sub sp,sp,4\n    stind sp,r1\n";
            code_ << "    sub sp,sp,4\n    stind sp,r0\n";
            genExpr(*e.rhs);
            code_ << "    mov r4,r0\n    mov r5,r1\n";
            code_ << "    ldind sp,r0\n    add sp,sp,4\n";
            code_ << "    ldind sp,r1\n    add sp,sp,4\n";
            code_ << "    " << insn << " r0,r0,r4\n    " << insn << " r1,r1,r5\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
        code_ << "    " << insn << " r0,r1,r0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);                                  // r0 = shift count
        code_ << "    mov r1,r0\n";                        // r1 = shift count
        code_ << "    ldind sp,r0\n    add sp,sp,4\n";     // r0 = value to shift
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r0,r1\n";
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
    genCompare(op, *e.lhs, *e.rhs);
}

// Real hardware multiply (`MUL`) - a full 32x32->32 product in one
// instruction, no HI/LO-register-pair unpacking the way MIPS's own
// `MULT`+`MFLO` needs.
void ArmCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
    code_ << "    mul r0,r1,r0\n";
}

// Real hardware divide (`SDIV`/`UDIV`) - a full 32-bit quotient directly.
// The remainder (`%`) then uses `MLS` to compute `dividend -
// quotient*divisor`; `r3` holds a copy of the divisor so `MLS`'s `Rd` and
// `Rm` are never the same register, which several ARM cores treat as
// UNPREDICTABLE for multiply-family instructions.
void ArmCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    " << (isUnsigned ? "udiv" : "sdiv") << " r3,r1,r0\n";  // r3 = quotient
    if (!wantRemainder) {
        code_ << "    mov r0,r3\n";
        return;
    }
    code_ << "    mov r2,r0\n";          // r2 = divisor copy
    code_ << "    mls r0,r3,r2,r1\n";    // r0 = r1 - r3*r2 = remainder
}

// Every ARM data-processing instruction is conditionally executed per its
// 4-bit condition field, so materializing a 0/1 boolean needs no branch:
// `MOV r0,#0` followed by one conditionally executed `MOV r0,#1`.
// `isUnsigned` picks the HS/LO/HI/LS condition family instead of
// LT/GE/LE/GT for the ordered operators.
void ArmCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";                      // flags = lhs - rhs
    code_ << "    mov r0,0\n";
    if (op == "<") {
        code_ << "    mov" << (isUnsigned ? "lo" : "lt") << " r0,1\n";
    } else if (op == ">") {
        code_ << "    mov" << (isUnsigned ? "hi" : "gt") << " r0,1\n";
    } else if (op == "<=") {
        code_ << "    mov" << (isUnsigned ? "ls" : "le") << " r0,1\n";
    } else if (op == ">=") {
        code_ << "    mov" << (isUnsigned ? "hs" : "ge") << " r0,1\n";
    } else if (op == "==") {
        code_ << "    moveq r0,1\n";
    } else if (op == "!=") {
        code_ << "    movne r0,1\n";
    } else {
        err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    }
}

// A wide (`long long`) operand's truthiness needs both words - `orr
// r0,r0,r1` folds them together before the usual "cmp r0,0".
void ArmCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r0,r1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r0,r1\n";
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r0,r1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r0,r1\n";
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov r0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov r0,0\n";
    code_ << Ldone << ":\n";
}

// `!x` is exactly "x == 0" - one `CMP`+conditional-`MOV` pair.
void ArmCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r0,r1\n";
    code_ << "    cmp r0,0\n";
    code_ << "    mov r0,0\n";
    code_ << "    moveq r0,1\n";
}

std::optional<std::string> ArmCodegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleArm(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: assembleArmObject() builds one
    // format-neutral ObjectFile, and the requested format just picks that
    // format's own FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArmObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
