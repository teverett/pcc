#include "ArmCodegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "ArmAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly via qemu's
// PC-forcing `loader` device (see tests/run_tests.sh's own
// qemu-system-arm invocation), same idea as M68kCodegen.cpp's own
// kBaseAddress (even the same value - both targets' own test harness
// entries load their program at this exact address).
constexpr uint32_t kBaseAddress = 0x1000u;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dw`'s own expression evaluator
// (ArmAssembler.cpp's Line::Dw) already resolves a bare symbol the
// same way `movw`/`movt`'s own label operand already does for an
// address loaded into a register (see loadAddress/genAddrOf).
std::string ArmCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void ArmCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long`
    // array's own literal elements are emitted as two "dw"s (low
    // word, then high) per element instead; a symbol element always
    // means a plain native-int-width address (the dispatch-table
    // idiom - see AST.h's ArrayInitElem), so it still gets one "dw".
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
    // `frameOffset` is already a real byte offset, computed by
    // IRBuilder from this slot's own declared type's width (see IR.h's
    // own comment on Slot::frameOffset) - no backend-side multiply by
    // a fixed word size any more. `elemIndex` (a Local array's own
    // compile-time element index) is scaled by that array's own
    // element width instead of the old fixed 4.
    if (slot->kind == ir::SlotKind::Param) {
        emitFrameOffset(reg, slot->frameOffset, /*isAdd=*/true);
    } else {
        emitFrameOffset(reg, slot->frameOffset + elemIndex * widthOfType(slot->type), /*isAdd=*/false);
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
            // A long long array literal's own element - loadArrayInitElem()
            // only ever fills r0 (every other element type fits in
            // one word); fill r1 (the high word) here too. A symbol
            // element always means a plain native-int-width address
            // (the dispatch-table idiom), so it stays single-word.
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
    // Re-truncate+re-extend by shifting the unwanted high bits out
    // and back in - zero-fill (`lsr`) for unsigned, sign-fill (`asr`)
    // for signed - the same idiom every narrower-than-native-int
    // store/Convert here needs, with no dedicated sign/zero-extend
    // instruction required.
    if (w == 1) {
        code_ << "    lsl r0,r0,24\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,24\n";
    } else if (w == 2) {
        code_ << "    lsl r0,r0,16\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,16\n";
    }
}

// `addrReg` holds the already-computed effective address (r2, by
// this backend's own convention - see loadSlotAddr()/genIndexAddr());
// loads into r0 (or r0:r1, for a `long long`), sign- or zero-
// extending a narrower-than-native-int value per its own signedness.
void ArmCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb " : "ldindsb ") << addrReg << ",r0\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindh " : "ldindsh ") << addrReg << ",r0\n";
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",r0\n";
    } else {
        code_ << "    ldind " << addrReg << ",r0\n";
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        code_ << "    ldind " << addrReg << ",r1\n";
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    }
}

void ArmCodegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",r0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",r0\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << ",r0\n";
    } else {
        code_ << "    stind " << addrReg << ",r0\n";
        code_ << "    add " << addrReg << "," << addrReg << ",4\n";
        code_ << "    stind " << addrReg << ",r1\n";
        code_ << "    sub " << addrReg << "," << addrReg << ",4\n";
    }
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is crossing the native-int/`long
// long` boundary, or re-truncating+re-extending r0 for a narrower
// target - see narrowR0To().
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
    // Two's complement negate of r0:r1 in place - `adc`'s own
    // immediate-0 operand2 reads the real carry out of the preceding
    // `adds`, no extra zero-register trick needed (unlike a target
    // whose ALU immediate forms are more restricted).
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    adds r0,r0,1\n    adc r1,r1,0\n";
}

// `long long` add/sub (lhs OP rhs) - evaluate both into r0:r1,
// stashing lhs's two words on the stack while rhs evaluates, then
// ripple the carry/borrow across the word pair with adc/sbc, one
// native word wider than a plain add/sub - see ArmAssembler.cpp's
// own adds/subs/adc/sbc comment.
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
// hi32(lo(lhs)*lo(rhs)) + lo(lhs)*hi(rhs) + hi(lhs)*lo(rhs), all mod
// 2^32. Only the low*low cross term needs a real widening multiply
// (`umull`) - the other two only ever contribute to result_hi, so
// any overflow of *their own* product past 32 bits would only affect
// bit 64 and beyond, which a 64-bit result doesn't keep anyway -
// `mul`'s own truncating 32x32->32 result is exactly right for them.
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

// Classic bit-serial shift/compare/subtract long division (no A32
// instruction is wide enough for a genuine 64/64 divide) - unlike
// i386's own version of this (which has to spill to a stack-resident
// scratch block, see I386Codegen.cpp's own comment), every one of
// this routine's values fits in a register here: r0:r1 = dividend
// (being consumed, shifted left MSB-first) -> final quotient, r2:r3 =
// divisor, r4:r5 = remainder, r6:r7 = quotient (being built), r8 =
// iteration countdown, r9/r10/r12 = scratch. `isUnsigned` false
// divides *magnitudes* (each operand's own sign bit, read before
// negating, is stashed on the stack across the whole loop and
// restored on the result afterward per C's truncating-division rule -
// same two-step shape as I386Codegen.cpp's own signed path).
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

// A `long long` shifted by a runtime count - no A32 instruction
// shifts more than 32 bits at once, so this is a counted loop of
// 1-bit shifts, each carrying the bit that crosses the word boundary
// by hand - same spirit as the divide routine's own bit-at-a-time
// loop above. `isUnsigned` only matters for `>>`: each step's own
// high-word shift is `asr` (sign-preserving) or `lsr` (zero-filling)
// accordingly; `<<` never cares about signedness.
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
// unsigned condition per `isUnsigned`, since that's where the sign
// bit lives); only if those are equal does the low word's own
// comparison matter, and at that point it's always unsigned
// regardless of the overall type's signedness (once the high halves
// match, the remaining 32 bits are a plain magnitude).
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
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - ArmLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see ArmAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movw sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    movt sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt` -
        // see ArmCodegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Running byte offset within the data segment (which always
    // starts on a 4-byte boundary - every instruction above is
    // exactly 4 bytes, and the whole image's own load address is
    // itself 4-byte-aligned - see kBaseAddress) - used only to insert
    // alignment padding ahead of a global whose own type is wider
    // than 1 byte: a `short`/`int`/`long`/`long long` global landing
    // on an odd (or, beyond halfword, non-word) address would
    // otherwise make `ldindh`/`stindh`/`ldind`/`stind`'s real ARM
    // LDRH/STRH/LDR/STR instructions silently round the address down
    // to the next aligned one (unaligned access is UNPREDICTABLE/
    // implementation-defined on this core), clobbering whatever
    // *previous* global happens to sit there instead. A `long long`
    // only ever needs word (4-byte) alignment here, never 8 - it's
    // always accessed as two separate word-sized ldind/stind, never
    // one wider instruction.
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
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

// Single-file-only - see M68kCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> ArmCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `lr` around its own body,
// whether or not it happens to call anything else - unlike the
// 68000's own JSR/RTS (which push/pop the real hardware stack
// automatically, so a nested call just works - see M68kCodegen.cpp's
// own comment), ARM has exactly one link register: a non-leaf
// function's own call to another function (`BL`) overwrites `lr`
// outright, so without this save/restore a caller's own eventual `bx
// lr` would return into whatever its *own* last call last returned
// to, not back to its own caller. Always saving/restoring (leaf or
// not) is simpler than tracking which functions are actually
// non-leaf, and costs only two real instructions on either side -
// same reasoning (and the same choice) as MipsCodegen.cpp's own `$ra`
// save/restore.
//
// Every function also gets a real per-call stack frame: after saving
// `lr`, it saves the caller's own `fp` the same way, then points `fp`
// at that saved pair and reserves `frameSize` bytes below it for its
// own Locals (`sub sp,sp,frameSize`, already real bytes - see IR.h's
// own comment on Function::frameSize) - see loadFrameAddr()'s own
// comment for the resulting Local/Param address layout. This is what
// makes direct or indirect recursion safe: every activation of this
// function gets its own `fp` and its own reserved Local words, rather
// than this backend's own old model of one statically allocated cell
// per Local/Param shared by every call.
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
            // Tear down this call's own frame (see genFunction()'s
            // own comment) before returning - a `return` statement is
            // its own early exit from the function, bypassing
            // genFunction()'s own fall-through epilogue entirely, so
            // it needs this same teardown inline.
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
                storeOperand("r2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            storeOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's own width/signedness - the pointee
            // type of a real typed pointer, or plain native-int-width
            // for the still-supported untyped-int-as-address style -
            // see IRBuilder.cpp's own DerefAssign comment.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    sub sp,sp,4\n    stind sp,r0\n";
            genExpr(*e.rhs);  // r0, or r0:r1 if wide
            code_ << "    ldind sp,r2\n    add sp,sp,4\n";
            storeOperand("r2", e.type);
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
                // Loads at e.type's own width/signedness - see
                // DerefAssign's identical comment above.
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r2,r0\n";
                loadOperand("r2", e.type);
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
            // Real argument passing on the hardware stack - pushed
            // right-to-left (so arg 0 ends up closest to this call's
            // own saved fp/lr pair, at `fp+8` inside the callee - see
            // loadFrameAddr()'s own comment), same convention (and
            // same reasoning) as ../i386/I386Codegen.cpp's own
            // identical `Call` case. The callee never cleans these up
            // itself (see genFunction()'s own `mov sp,fp` epilogue),
            // so the caller restores `sp` right here once the call
            // returns.
            // Every argument reserves at least one native word (the
            // same "no narrower than a native word" rule IRBuilder.cpp's
            // own param-offset layout already applies), and a wide
            // (`long long`) argument pushes its high word first so
            // its low word ends up closer to `fp`, matching a wide
            // Param's own frameOffset/frameOffset+4 layout.
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
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no way to push
            // this call's own arguments in the callee's own expected
            // order until the callee is known, which it never is at
            // compile time for this kind.
            // `blx r2` is this dialect's own ARMv7-A `BLX (register)`
            // encoding (see ArmAssembler.cpp's own encodeBlx()) - a
            // real near call through whatever address r2 holds,
            // setting `lr` to the return address exactly like `bl`
            // does.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r2,r0\n";
            code_ << "    blx r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal A-profile core with no exception/GIC setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as M68kCodegen.cpp/MipsCodegen.cpp's
            // own Intrinsic case.
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

// `r2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is the array's own address (a Global's own fixed
// label, or a Local's own frame-relative address - loadSlotAddr()
// dispatches on which); otherwise `slot` is a plain pointer variable,
// so the base instead comes from *reading* that variable's own stored
// value first. A Global array's element addresses increase with
// `idx` (`add`); a Local array's own decrease (`sub` - see
// loadFrameAddr()'s own comment on why), since its later elements sit
// further *below* `fp`.
void ArmCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    lsl r0,r0," << shift << "\n";  // r0 *= the element type's own width
    if (slot->arraySize > 0) {
        loadSlotAddr("r2", slot);
        code_ << "    " << (slot->kind == ir::SlotKind::Global ? "add" : "sub") << " r2,r2,r0\n";
    } else {
        loadSlotAddr("r1", slot);
        code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's own stored value
        code_ << "    add r2,r1,r0\n";
    }
}

void ArmCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("r2", e.slot->type);
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

void ArmCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h), except shift (only the LHS's own
    // type matters) and &&/|| (width-independent, handled below).
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

// Real hardware divide (`SDIV`/`UDIV`) - a full 32-bit quotient
// directly, no HI/LO-register-pair unpacking the way MIPS's own `DIV`
// needs. The remainder (`%`) then uses `MLS` (multiply-subtract) to
// compute `dividend - quotient*divisor` in one more instruction -
// `r3`'s own copy of the divisor (rather than reusing `r0` directly)
// exists only so `MLS`'s own `Rd` and `Rm` operands are never the
// same register, which several real ARM cores treat as
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
    code_ << "    mov r2,r0\n";          // r2 = divisor (own copy, see this function's own comment)
    code_ << "    mls r0,r3,r2,r1\n";    // r0 = r1 - r3*r2 = dividend - quotient*divisor = remainder
}

// Every ARM data-processing instruction (including `MOV`) is
// conditionally executed per its own 4-bit condition field, so
// materializing a 0/1 boolean needs no branch at all - just `MOV
// r0,#0` followed by one conditionally executed `MOV r0,#1` (see
// ArmCodegen.h's own header comment). `isUnsigned` picks the HS/LO/
// HI/LS condition family instead of LT/GE/LE/GT for the ordered
// operators - see ArmAssembler.cpp's own condOf().
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
// r0,r0,r1` folds them together before the usual "cmp r0,0" (the
// combined value is discarded immediately after, so clobbering r0
// this way is safe).
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

// `!x` is exactly "x == 0" - one `CMP`+conditional-`MOV` pair, same
// idea as genCompare()'s own "==" case.
void ArmCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r0,r1\n";
    code_ << "    cmp r0,0\n";
    code_ << "    mov r0,0\n";
    code_ << "    moveq r0,1\n";
}

std::string ArmCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                              const std::string &) const {
    throw std::runtime_error("the armv7a backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> ArmCodegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleArm(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see ArmAssembler.h's own `assembleArmObject`), and whichever of
    // these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArmObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
