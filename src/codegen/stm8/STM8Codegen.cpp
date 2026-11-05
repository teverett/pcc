#include "STM8Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "STM8Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing, and the
// hardware stack's own initial top - same "comfortably clear of
// everything else this backend could generate, inside a flat 64KB
// address space" convention as every other bare-metal 8-bit backend
// here (see I8080Codegen.cpp's own comment on both constants).
constexpr uint32_t kBaseAddress = 0x0100;
constexpr uint32_t kStackTop = 0xFF00;

// Where this backend's own software frame stack starts (see
// STM8Codegen.h's own `genPrologue`/`genSlotAddress` comments) -
// comfortably clear of both the code/data region above
// `kBaseAddress` and the real hardware stack at `kStackTop`, with
// plenty of room between the two for either to grow without
// colliding, in the same "bare flat 64KB address space" spirit as
// `kBaseAddress`/`kStackTop` themselves.
constexpr uint32_t kFrameStackTop = 0xE000;
}  // namespace

void STM8Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's own
// label text (the referenced Slot's or Function's own address) - the
// shared assembler's own "dw" directive (STM8Assembler.cpp's emit16)
// resolves either into a real, full 16-bit value and writes it out
// big-endian, the same order a `ldwi`/`call` operand already gets.
std::string STM8Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType`'s own width picks `db` (a `char` array, one byte per
// element) vs `dw` (every other array - a `long`/`long long` array's
// own elements still only ever occupy one native word each, a known
// gap - see genIndex()'s own comment).
void STM8Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    const char *directive = widthOfType(elemType) == 1 ? "db" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void STM8Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ldw x, " << src << "\n    ldw " << dst << ", x\n";
}

void STM8Codegen::storeIndirect(const std::string &src) {
    // Assumes `x` already holds the target address - `ld`/`stxind`
    // address `src` directly, by name, never touching `x` itself
    // until the `incw` between the two bytes, so the caller's address
    // in `x` only ever advances forward, same convention as
    // I8080Codegen::storeAcIndirect.
    code_ << "    ld a, " << src << "\n    stxind\n";
    code_ << "    incw x\n";
    code_ << "    ld a, " << src << "+1\n    stxind\n";
}

void STM8Codegen::loadIndirect(const std::string &dst) {
    code_ << "    ldxind\n    ld " << dst << ", a\n";
    code_ << "    incw x\n";
    code_ << "    ldxind\n    ld " << dst << "+1, a\n";
}

// One in-place 1-bit shift of the 16-bit memory cell `lbl`, used by
// genShift()'s own loop and inlined directly by genMul()/
// genDivMod() - via the real word-wide `SLAW`/`SRLW` (see
// STM8Codegen.h's own header comment on why there's still no
// variable-count shift to use instead, even though these exist).
void STM8Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ldw x, " << lbl << "\n";
    code_ << "    " << (isLeft ? "slaw" : "srlw") << " x\n";
    code_ << "    ldw " << lbl << ", x\n";
}

// STM8 has no dedicated sign-extend instruction (the real hardware
// never needs to produce more than 16 live bits at once) - every
// sign-extending conversion here instead tests the byte's own top bit
// (`andi a, 0x80` isolates it, `tnza` turns that into a plain zero/
// nonzero test) and branches once; `a` must already hold the byte to
// extend, and ends up holding the fill byte itself (0xFF or 0x00).
void STM8Codegen::genSignFillByte() {
    code_ << "    andi a, 0x80\n    tnza\n";
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    jrne " << Lneg << "\n";
    code_ << "    ldi a, 0\n    jp " << Ldone << "\n";
    code_ << Lneg << ":\n    ldi a, 0xFF\n";
    code_ << Ldone << ":\n";
}

// Assumes `x` already holds the effective storage address (same
// precondition as loadAcIndirect()/storeAcIndirect()).
void STM8Codegen::loadOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ldxind\n";      // a = the single stored byte
        code_ << "    ld ac+1, a\n";  // low byte = the value itself
        if (type.isUnsigned) {
            code_ << "    ldi a, 0\n";
        } else {
            genSignFillByte();  // a is unchanged by "ld ac+1,a" above, so it's still the original byte here
        }
        code_ << "    ld ac, a\n";  // high byte = 0 or the sign fill
        return;
    }
    loadAcIndirect();
}

void STM8Codegen::storeOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ld a, ac+1\n    stxind\n";  // only the low byte is ever stored
        return;
    }
    storeAcIndirect();
}

// Pops `words` words off the real hardware stack (LSW on top - see
// genWideAdd()'s own header comment) into `role`'s own wideScratch()
// cells.
void STM8Codegen::popWideInto(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop x\n    ldw " << wideScratch(role, i) << ", x\n";
    }
}

void STM8Codegen::pushWideFrom(const char *role, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ldw x, " << wideScratch(role, i) << "\n    push x\n";
    }
}

void STM8Codegen::genWideWordAddr(const ir::Slot *slot, int words, int wordIndex) {
    int byteOff = 2 * (words - 1 - wordIndex);
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldwi x, " << label(slot) << (byteOff ? ("+" + std::to_string(byteOff)) : "") << "\n";
        return;
    }
    genSlotAddress(slot);
    if (byteOff > 0) code_ << "    ldwi y, " << byteOff << "\n    addw x, y\n";
}

// Pushes `slot`'s own `words`-word value onto the real hardware
// stack, word 0 (least significant) ending up on top - see this
// file's own wide-value stack convention (established by
// genWideAdd()'s own header comment).
void STM8Codegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        genWideWordAddr(slot, words, i);
        loadIndirect("bc");
        code_ << "    ldw x, bc\n    push x\n";
    }
}

void STM8Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        uint16_t w = static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    ldwi x, " << w << "\n    push x\n";
    }
}

// Pops `words` words off the real hardware stack (least significant
// on top) into `slot`'s own storage.
void STM8Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop x\n    ldw bc, x\n";
        genWideWordAddr(slot, words, i);
        storeIndirect("bc");
    }
}

// Every byte-at-a-time wide helper below first pops both operands off
// the real hardware stack into the fixed "wa"/"wb" scratch cells (see
// wideScratch()'s own comment), then walks every byte of every word
// from least to most significant - the same shape the i8080 backend's
// own identical helpers use (see I8080Codegen.cpp's own header
// comment), just chaining the real carry/borrow flag through this
// target's own `adc`/`sbc` instead.
namespace {
// `role`'s own `words`-word value, as individual byte labels, least
// significant byte first. Each `wideScratch()` cell stores its own
// high byte first (this target's own big-endian convention - see
// STM8Codegen.h's own header comment), so within one word the *low*
// byte (`+1`) comes first in this LSB-first ordering, followed by
// that same word's own high byte, then the next (more significant)
// word's low byte, and so on.
std::vector<std::string> wideByteLabelsLsbFirst(const char *role, int words) {
    std::vector<std::string> out;
    for (int i = 0; i < words; i++) {
        out.push_back(STM8Codegen::wideScratch(role, i) + "+1");
        out.push_back(STM8Codegen::wideScratch(role, i));
    }
    return out;
}
}  // namespace

void STM8Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    ld a, " << aBytes[k] << "\n";
        if (isAdd) {
            code_ << "    " << (k == 0 ? "add a, " : "adc a, ") << bBytes[k] << "\n";
        } else {
            code_ << "    " << (k == 0 ? "sub a, " : "sbc a, ") << bBytes[k] << "\n";
        }
        code_ << "    ld " << aBytes[k] << ", a\n";
    }
    pushWideFrom("a", words);
}

// Two's-complement negation in place: `cpl` every byte first, then
// `addi a, 1` into the least significant byte and `adc a, zero` (fold
// the resulting carry in, add nothing else) up through the rest - the
// same chained-carry shape as genWideAdd().
void STM8Codegen::genWideNegate(int words) {
    popWideInto("a", words);
    auto bytes = wideByteLabelsLsbFirst("a", words);
    for (const auto &b : bytes) {
        code_ << "    ld a, " << b << "\n    cpl a\n    ld " << b << ", a\n";
    }
    for (size_t k = 0; k < bytes.size(); k++) {
        code_ << "    ld a, " << bytes[k] << "\n";
        code_ << "    " << (k == 0 ? "addi a, 1\n" : "adc a, zero\n");
        code_ << "    ld " << bytes[k] << ", a\n";
    }
    pushWideFrom("a", words);
}

void STM8Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    const char *insn = op == "&" ? "and" : op == "|" ? "or" : "xor";
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    ld a, " << aBytes[k] << "\n    " << insn << " a, " << bBytes[k] << "\n";
        code_ << "    ld " << aBytes[k] << ", a\n";
    }
    pushWideFrom("a", words);
}

void STM8Codegen::genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    code_ << "    ldi a, 0\n    ld lc, a\n";
    for (size_t k = 0; k < aBytes.size(); k++) {
        code_ << "    ld a, " << aBytes[k] << "\n    xor a, " << bBytes[k] << "\n";
        code_ << "    or a, lc\n    ld lc, a\n";
    }
    code_ << "    ld a, lc\n    tnza\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "jreq" : "jrne") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// Compares most-significant byte first - only that byte's own
// comparison ever needs the signed/unsigned bias (same trick as
// emitLessCompare()'s own 16-bit version); once a byte differs, every
// byte after it is comparing a pure, already-unsigned magnitude.
void STM8Codegen::genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words) {
    genExpr(a);
    genExpr(b);
    popWideInto("b", words);
    popWideInto("a", words);
    auto aBytes = wideByteLabelsLsbFirst("a", words);
    auto bBytes = wideByteLabelsLsbFirst("b", words);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    for (size_t idx = aBytes.size(); idx-- > 0;) {
        bool top = idx == aBytes.size() - 1;
        code_ << "    ld a, " << bBytes[idx] << "\n";
        if (top && !isUnsigned) code_ << "    xori a, 0x80\n";
        code_ << "    ld lc, a\n";
        code_ << "    ld a, " << aBytes[idx] << "\n";
        if (top && !isUnsigned) code_ << "    xori a, 0x80\n";
        code_ << "    sub a, lc\n";  // flags = a - b
        if (idx == 0) {
            code_ << "    jrult " << (invert ? Lfalse : Ltrue) << "\n";
            code_ << "    jp " << (invert ? Ltrue : Lfalse) << "\n";
        } else {
            std::string Lcontinue = newLabel();
            code_ << "    jreq " << Lcontinue << "\n";
            code_ << "    jrult " << (invert ? Lfalse : Ltrue) << "\n";
            code_ << "    jp " << (invert ? Ltrue : Lfalse) << "\n";
            code_ << Lcontinue << ":\n";
        }
    }
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
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
// this one bit at a time) - `rlc`/`rrc` always rotate `a` through the
// real carry flag, chaining it from one byte into the next exactly
// the way shiftOneBit() already chains it (via `slaw`/`srlw`'s own
// internal carry) across the two bytes of a plain `int`. A signed
// right shift's very first byte needs its own sign bit preserved
// (arithmetic), not always 0 (logical) - `rlc a` (reloaded) then
// `rrc a` (on the original byte again) computes that top byte's own
// software SRA (there's no hardware one on this target either): the
// first `rlc` copies the byte's own sign bit into the carry, then
// `rrc` on the *original* byte rotates that same bit back in as the
// new top bit while shifting right - the sign survives, and the bit
// `rrc` shifts *out* (the original bit 0) is exactly what the next,
// less significant byte's own plain `rrc` needs.
void STM8Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);
    popWideInto("a", words);
    genExpr(rhs);
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count (always small; the low byte is enough)
    auto bytesMsbFirst = wideByteLabelsLsbFirst("a", words);
    std::reverse(bytesMsbFirst.begin(), bytesMsbFirst.end());
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    if (isLeft) {
        code_ << "    rcf\n";  // '<<' always shifts in a 0 bit
        for (size_t idx = bytesMsbFirst.size(); idx-- > 0;) {
            code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rlc a\n    ld " << bytesMsbFirst[idx] << ", a\n";
        }
    } else {
        for (size_t idx = 0; idx < bytesMsbFirst.size(); idx++) {
            if (idx == 0 && !isUnsigned) {
                code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rlc a\n";
                code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rrc a\n";
            } else {
                if (idx == 0) code_ << "    rcf\n";  // unsigned top byte: logical, zero-fill
                code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rrc a\n";
            }
            code_ << "    ld " << bytesMsbFirst[idx] << ", a\n";
        }
    }
    code_ << "    ld a, lc\n    dec a\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    pushWideFrom("a", words);
}

// See ../pdp11/Pdp11Codegen.cpp's own genWideMul()/genWideDivMod()
// comment (mirrored on ../i8080/I8080Codegen.cpp too) for the
// reasoning this repeats exactly: no simulator in this project's own
// test harness to verify a hand-written multi-byte multiply/divide
// routine against, so failing loudly is safer than shipping an
// unverifiable, possibly wrong one.
void STM8Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                  "see STM8Codegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void STM8Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                  "see STM8Codegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the real work here is crossing the native-word/`long`-or-
// `long long` boundary (pushing more words of sign/zero extension, or
// popping down to just the low word) and re-narrowing a `char` result
// (zero or sign-fill its own high byte back off).
void STM8Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldi a, 0\n    ld ac, a\n";
            } else {
                code_ << "    ld a, ac+1\n";
                genSignFillByte();
                code_ << "    ld ac, a\n";
            }
        }
        return;
    }
    if (fromWide && !toWide) {
        int words = wordCountOf(from);
        genExpr(*e.lhs);
        popWideInto("a", words);
        code_ << "    ldw x, " << wideScratch("a", 0) << "\n    ldw ac, x\n";  // word 0 (LSW) is this type's own
                                                                                 // truncated value
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldi a, 0\n    ld ac, a\n";
            } else {
                code_ << "    ld a, ac+1\n";
                genSignFillByte();
                code_ << "    ld ac, a\n";
            }
        }
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac already holds the native-width value, correctly sign/zero-extended to 16 bits
        code_ << "    ldw x, ac\n    ldw " << wideScratch("a", 0) << ", x\n";
        for (int i = 1; i < toWords; i++) {
            if (from.isUnsigned) {
                code_ << "    ldwi x, 0\n    ldw " << wideScratch("a", i) << ", x\n";
            } else {
                code_ << "    ld a, ac\n";  // a = the int's own high byte (bit 7 is the int's own sign bit)
                genSignFillByte();
                code_ << "    ld " << wideScratch("a", i) << ", a\n";
                code_ << "    ld " << wideScratch("a", i) << "+1, a\n";
            }
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
        code_ << "    ld a, " << wideScratch("a", fromWords - 1) << "\n";  // the narrower value's own top byte
        if (from.isUnsigned) {
            code_ << "    ldi a, 0\n";
        } else {
            genSignFillByte();
        }
        for (int i = fromWords; i < toWords; i++) {
            code_ << "    ld " << wideScratch("a", i) << ", a\n";
            code_ << "    ld " << wideScratch("a", i) << "+1, a\n";
        }
    }
    pushWideFrom("a", toWords);
}

std::string STM8Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all - an ordinary object
    // file never does. `objectMode` (see Codegen.h) means no object
    // file ever emits this entry sequence itself, even the one that
    // happens to define `main` - STM8LinkerFactory.cpp is solely
    // responsible for synthesizing exactly one, once every object is
    // merged and `main`'s linked address is resolvable (see
    // Linker.h). An object file's own base is `org 0` - it's the
    // linker's job to place it, not this backend's.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ldwi sp, " << kStackTop << "\n";
        // The software frame stack's own initial top, and `main`'s
        // own caller-side `fp` link - nothing ever reads this `fp`
        // value again once the whole program halts, so its initial
        // content doesn't matter beyond being a well-defined word.
        code_ << "    ldwi x, " << kFrameStackTop << "\n    ldw fsp, x\n";
        code_ << "    ldwi x, 0\n    ldw fp, x\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra `ldw` leaves it in the real `x`
        // register too, for a quick look with nothing more than a
        // register dump.
        code_ << "    ldw x, ac\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // The fixed memory pseudo-registers - see STM8Codegen.h's own
    // header comment for what each one is for. `lc` is this
    // backend's own loop counter (genMul()/genShift() need one but
    // can't use `a` - see their own comments), a plain byte, not one
    // of the five 16-bit cells. Every genExpr() call anywhere in the
    // *whole linked program* shares these same cells (a callee's own
    // return value and a caller's own next operand both pass through
    // `ac`), so unlike every other symbol this backend emits, they
    // can't each be redeclared per-object: in whole-program mode
    // they're declared once, right here; in object mode, this file
    // never declares them at all - every reference is then
    // automatically deferred as an external relocation, resolved
    // against the one real definition STM8LinkerFactory.cpp's own
    // synthesized entry object provides.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // `fp`/`fsp` are this backend's own software frame pointer
        // and frame-stack pointer (see STM8Codegen.h's own
        // `genPrologue`/`genSlotAddress` comments) - persistent
        // state across a whole call tree, so they're named memory
        // cells like every other pseudo-register here, never the
        // real `x`/`y` (both already fully committed as ephemeral
        // expression-evaluation transit registers everywhere else in
        // this file). They're entirely independent of the real
        // hardware `sp` (still used only for `call`/`ret` and
        // genBinary()'s own balanced `push`/`pop` scratch).
        declareTemp("fp");
        declareTemp("fsp");
        trailer_.push_back("lc: db 0");
        // A permanent zero byte - `adc a, zero` folds a pending carry
        // into a byte with nothing else added to it (see
        // genWideNegate()'s own comment), the same role `aci 0` plays
        // on the i8080 backend (which has a dedicated add-immediate-
        // with-carry instruction this dialect doesn't bother giving
        // a byte-immediate form of).
        trailer_.push_back("zero: db 0");
        // `wa0`-`wa3`/`wb0`-`wb3` - the `long`/`long long` byte-serial
        // scratch cells every genWide*() helper pops both operands
        // into (see wideScratch()'s own comment) before walking every
        // byte from least to most significant - no SP-relative
        // addressing exists on this target either (same reasoning as
        // the i8080 backend's own identical cells - see
        // I8080Codegen.h's own header comment), and "a" doubles as
        // this backend's own wide call-return-value transport (see
        // genStmt()'s own Return case / genExpr()'s own Call case).
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name in
        // code, letting the assembler's own undefined-symbol
        // handling defer it to a relocation.
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
                // High byte first, low byte second - see declareArray()'s
                // own comment.
                trailer_.push_back(lbl + ": db " + std::to_string((v16 >> 8) & 0xFF) + "  ; " + slot->sourceName);
                trailer_.push_back("db " + std::to_string(v16 & 0xFF));
            } else {
                // `long`/`long long` global - each word emitted most
                // significant first (matching every other cell's own
                // big-endian layout), least significant word last.
                auto uv = static_cast<unsigned long>(v);
                std::ostringstream line;
                line << lbl << ": db ";
                bool first = true;
                for (int i = w / 2 - 1; i >= 0; i--) {
                    uint16_t word = static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
                    if (!first) line << ", ";
                    line << ((word >> 8) & 0xFF) << ", " << (word & 0xFF);
                    first = false;
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

// Single-file-only (see STM8Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed to
// whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> STM8Codegen::perFileAsm() const { return perFileAsm_; }

void STM8Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    currentFunction_ = fn.func;
    genPrologue(*fn.func);
    genStmt(*fn.body);
    genEpilogue(*fn.func);
    currentFunction_ = nullptr;
    code_ << "\n";
}

// --- Per-call frame management ------------------------------------------
//
// Layout, growing down from `fp` (the address of this call's own
// saved caller's `fp`, i.e. the first word pushed onto the frame
// stack for this call):
//
//   fp+2*numParams .. fp+2   incoming arguments (param 0 closest to
//                            `fp` - the caller pushes them in reverse
//                            order, last argument first, so param 0
//                            ends up pushed last - see pushFrameWord()'s
//                            own call sites in the Call case below)
//   fp                       this call's own saved caller's `fp`
//   fp-2 .. fp-2*frameSize   this function's own Local storage, in
//                            declaration order (frameOffset 1 first)
//
// A Local's growing-downward layout (and its array elements'
// matching base-minus-index addressing - see genSlotAddress()) is
// the same shape I386Codegen.cpp's own `addr()`/`computeElementAddr`
// use for `ebp`-relative locals; the one real difference is that
// there's no spare STM8 register to hold a live frame-pointer
// snapshot the way `ebp` does on i386 (`x`/`y` are both already
// fully committed as this file's own ephemeral expression-evaluation
// transit registers - see STM8Codegen.h's own header comment), so
// `fp`/`fsp` are ordinary named memory cells instead, entirely
// separate from the real hardware `sp` (which keeps doing exactly
// what it already did - `call`/`ret` and genBinary()'s own balanced
// `push`/`pop` scratch - untouched by any of this).
void STM8Codegen::pushFrameWord(const std::string &cell) {
    code_ << "    ldw x, fsp\n    ldwi y, 2\n    subw x, y\n    ldw fsp, x\n";
    storeIndirect(cell);  // x still holds the just-decremented fsp
}

void STM8Codegen::genPrologue(const ir::Function &fn) {
    pushFrameWord("fp");           // save the caller's own fp
    code_ << "    ldw x, fsp\n    ldw fp, x\n";  // fp = address of that saved word
    if (fn.frameSize > 0) {
        code_ << "    ldw x, fp\n    ldwi y, " << fn.frameSize << "\n    subw x, y\n    ldw fsp, x\n";
    }
}

// Mirrors genPrologue(): recovers the caller's own `fp` (read from
// the word at the current `fp` address, *before* `fp` itself is
// overwritten) and resets `fsp` back to its value just before this
// call's own frame was pushed, i.e. past this call's own incoming
// arguments too (`numParams` words) - the callee cleans up its own
// argument area, so the caller's own post-`call` code needs no
// matching adjustment. Uses `bc` as scratch (dead at every statement
// boundary - see genBinary()'s own header comment), never `ac`,
// since `ac` already holds this call's own live return value by the
// time this runs (see the Return case below) - `ret` itself is
// always the last instruction, strictly after `fp`/`fsp` are both
// restored, same "duplicated at every return site" shape
// I386Codegen.cpp's own genFunction()/Return case uses.
void STM8Codegen::genEpilogue(const ir::Function &fn) {
    code_ << "    ldw x, fp\n";
    loadIndirect("bc");  // bc = the caller's own saved fp
    int numParams = static_cast<int>(fn.params.size());
    code_ << "    ldw x, fp\n    ldwi y, " << ((numParams + 1) * 2) << "\n    addw x, y\n    ldw fsp, x\n";
    code_ << "    ldw x, bc\n    ldw fp, x\n";
    code_ << "    ret\n";
}

// Leaves a Slot's own effective storage address in `x` - see
// STM8Codegen.h's own header comment.
void STM8Codegen::genSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldwi x, " << label(slot) << "\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    ldw x, fp\n    ldwi y, " << slot->frameOffset << "\n    addw x, y\n";
        return;
    }
    code_ << "    ldw x, fp\n    ldwi y, " << slot->frameOffset << "\n    subw x, y\n";
}

void STM8Codegen::genStmt(const ir::Stmt &s) {
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
            code_ << "    jreq " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            code_ << "    jreq " << Lend << "\n";
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
                // A wide value sits on the real hardware stack (LSW
                // on top) after genExpr() - pop it into the "a"
                // wideScratch() cells first, since genEpilogue() never
                // touches that stack at all (the real `call`/`ret`
                // pair, and this call's own balanced `push`/`pop`
                // scratch within one expression, are the only things
                // that ever do) - see genExpr()'s own Call case for
                // the matching "push it back from those same cells"
                // half of this convention.
                if (isWide(s.initOrReturnOrExpr->type)) popWideInto("a", wordCountOf(s.initOrReturnOrExpr->type));
            }
            genEpilogue(*currentFunction_);
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // A Local's words are already reserved by genPrologue()
            // (`frameSize` accounts for every one of them) - nothing
            // is separately declared here the way a Global is. An
            // array initializer still has to run its own per-element
            // stores every time this declaration executes (not once,
            // statically): a fresh call gets a fresh copy of the
            // frame, so a shared one-time initializer the way
            // declareArray() gives a Global would leak the *previous*
            // call's own last values into every new call instead.
            if (s.declSlot->arraySize > 0) {
                int stride = widthOfType(s.declSlot->type) == 1 ? 1 : 2;
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    const ir::ArrayInitElem &elem = s.declSlot->arrayInit[i];
                    if (elem.isSymbol && elem.slot && elem.slot->kind != ir::SlotKind::Global) {
                        // "&otherLocal"/"&someParam" - unlike a
                        // Global's or a Function's own address (both
                        // real compile-time constants arrayElemText()
                        // can hand straight to `ldwi`), a Local's or
                        // a Param's own address only exists at
                        // runtime, relative to this call's own `fp` -
                        // same genSlotAddress() every other Local/
                        // Param access goes through.
                        genSlotAddress(elem.slot);
                        code_ << "    ldw ac, x\n";
                    } else {
                        code_ << "    ldwi x, " << arrayElemText(elem) << "\n    ldw ac, x\n";
                    }
                    genSlotAddress(s.declSlot);  // x = address of element 0
                    if (i > 0) {
                        // Element i sits *below* element 0 - see
                        // genSlotAddress()'s own header comment on why
                        // a Local array's addresses decrease as its
                        // own index increases, the opposite of a
                        // Global array.
                        code_ << "    ldwi y, " << (i * stride) << "\n    subw x, y\n";
                    }
                    storeOperand(s.declSlot->type);
                }
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                if (isWide(s.declSlot->type)) {
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                } else {
                    genSlotAddress(s.declSlot);
                    storeOperand(s.declSlot->type);
                }
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

void STM8Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldwi x, " << v16 << "\n    ldw ac, x\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16("ac", label(e.slot));
            } else {
                genSlotAddress(e.slot);
                loadOperand(e.slot->type);
            }
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs - never wide (element width isn't threaded through genIndexAddr's own
                           // store side - see genIndex()'s own comment on this known, documented limitation).
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                code_ << "    ldw x, ptr\n    push x\n";
                genExpr(*e.rhs);
                code_ << "    pop x\n";
                storeOperand(e.slot->type);
                return;
            }
            if (isWide(e.slot->type)) {
                genExpr(*e.rhs);
                genWideStore(e.slot, wordCountOf(e.slot->type));
                return;
            }
            genExpr(*e.rhs);
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16(label(e.slot), "ac");
            } else {
                genSlotAddress(e.slot);
                storeOperand(e.slot->type);
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Deref is untyped (plain ints with '*'/'&' applied, see
            // Type.h's own header comment) - always a native word.
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    ldw x, ac\n    push x\n";
            genExpr(*e.rhs);
            code_ << "    pop x\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genWideNegate(wordCountOf(e.rhs->type));
                } else {
                    code_ << "    ldw x, ac\n    negw x\n    ldw ac, x\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    ldw x, ac\n";
                loadAcIndirect();
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    int words = wordCountOf(e.rhs->type);
                    popWideInto("a", words);
                    for (int i = 0; i < words; i++) {
                        std::string cell = wideScratch("a", i);
                        code_ << "    ldw x, " << cell << "\n    cplw x\n    ldw " << cell << ", x\n";
                    }
                    pushWideFrom("a", words);
                } else {
                    code_ << "    ldw x, ac\n    cplw x\n    ldw ac, x\n";
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Pushed onto the frame stack in reverse (last argument
            // first), so param 0 ends up pushed *last* - right next
            // to the callee's own saved `fp` - matching
            // genSlotAddress()'s own `fp + (paramIndex+1)*2` formula
            // (param 0 at `fp+2`, param 1 at `fp+4`, ...). Each
            // argument lands directly in the callee's own fresh
            // frame - there's no shared per-function cell for this to
            // ever alias between a call and a nested recursive call
            // of the same function. A wide argument's own genExpr()
            // already left its `words` words on the real hardware
            // stack (LSW on top) - popping and frame-pushing them one
            // at a time, least significant first, lands the most
            // significant word last, at the lowest address within
            // this argument's own region - exactly the address
            // genWideWordAddr() expects to find it at (byte offset 0
            // from this param's own base).
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    int words = wordCountOf(e.args[i]->type);
                    for (int w = 0; w < words; w++) {
                        code_ << "    pop x\n    ldw ac, x\n";
                        pushFrameWord("ac");
                    }
                } else {
                    pushFrameWord("ac");
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            if (isWide(e.type)) {
                // The callee left its own wide return value in the
                // "wa" scratch cells (see genStmt()'s own Return
                // case) - push it back onto the real hardware stack
                // as this expression's own result, in the usual LSW-
                // on-top order every wide helper expects.
                pushWideFrom("a", wordCountOf(e.type));
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no callee to
            // push a frame for yet, since the callee's own identity
            // isn't known until this is evaluated at runtime.
            //
            // The real STM8 `CALL [mem]` Indirect addressing mode
            // doesn't treat its own operand's address as the callee
            // directly - it fetches the actual entry address from the
            // word stored there (see STM8Assembler.cpp's own
            // `callind` comment), so the callee's own address (just
            // computed into `ac`) is spilled to a fresh temp cell
            // first, then `callind` is pointed at that cell - a real
            // hardware addressing mode, used exactly the way it calls
            // through an ordinary function-pointer variable in memory.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string T = allocTemp();
            declareTemp(T);
            copy16(T, "ac");
            code_ << "    callind " << T << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real STM8 interrupt enable/disable - RIM/SIM, the same
            // real hardware feature as the 8080/6502/8086 backends'
            // own ei/di-style intrinsics.
            code_ << "    " << (e.op == "ion" ? "rim" : "sim") << "\n";
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell - same role, and the same "idxExpr evaluated first and alone"
// caution, as I8080Codegen::genIndexAddr (see that file's own header
// comment - nothing here is safe to hold in `x`/`y`/`ptr`/`bc`/etc.
// across a general recursive genExpr() call).
void STM8Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    code_ << "    ldw x, ac\n";
    for (int i = 0; i < elemShift(slot->type); i++) code_ << "    slaw x\n";  // x *= widthOf(slot->type)
    code_ << "    ldw ptr, x\n";
    genSlotAddress(slot);  // x = slot's own storage address (Global/Local/Param alike)
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ldw y, ptr\n    addw x, y\n    ldw ptr, x\n";  // ptr = base + index*2
        } else {
            // A Local/Param array's own words sit at *decreasing*
            // addresses as its own element index increases
            // (genSlotAddress()'s own storage address is the array's
            // first word, closest to `fp`; later words are further
            // below it - see genSlotAddress()'s own header comment) -
            // the mirror image of a Global array's increasing-address
            // layout above, so this is base-minus-scaled-index, not
            // base-plus.
            code_ << "    ldw y, ptr\n    subw x, y\n    ldw ptr, x\n";  // ptr = base - index*2
        }
        return;
    }
    // y = base (a pointer variable's own stored value, read from its
    // just-computed storage address - a Global's, a Local's, or a
    // Param's, all alike) - ordinary pointer arithmetic, always
    // base-plus-scaled-index regardless of where the pointer
    // variable itself happens to live.
    loadIndirect("bc");
    code_ << "    ldw y, bc\n";
    code_ << "    ldw x, ptr\n    addw x, y\n    ldw ptr, x\n";
}

// A `char` array's own element is one byte (loadOperand() already
// knows how to sign/zero-extend one); every other array's own
// element still only ever reads one native word regardless of its
// own real width - a known, documented gap for `long`/`long long`
// array elements specifically (genIndexAddr() already scales the
// index correctly via elemShift(), but a multi-word element's own
// remaining words are never read/written), the same scope limitation
// this project's own pdp11/i8080 backends accept - scalars of every
// width work fully.
void STM8Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldw x, ptr\n";
    loadOperand(e.slot->type);
}

void STM8Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        genSlotAddress(operand.slot);
        code_ << "    ldw ac, x\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ldwi x, " << label(operand.func) << "\n    ldw ac, x\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* STM8 hardware stack (`push x`/`pop x`),
// never in a named memory cell - same reasoning as
// I8080Codegen::genBinary's own header comment (a nested binary op
// might reuse the same named cell).
void STM8Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already shares
    // one common type (IRBuilder's own usual-arithmetic-conversion
    // pass - see Type.h) via `e.lhs->type`, except shift (only the
    // LHS's own type matters) and &&/|| (width-independent).
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
        code_ << "    ldw x, ac\n    push x\n";
        genExpr(*e.rhs);                        // ac = rhs
        code_ << "    ldw y, ac\n    pop x\n";   // y = rhs, x = lhs (popped only now)
        if (op == "+") {
            code_ << "    addw x, y\n    ldw ac, x\n";
        } else {
            code_ << "    subw x, y\n    ldw ac, x\n";
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
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        if (wide) {
            genWideBitwise(*e.lhs, *e.rhs, op, words);
            return;
        }
        genBitwise(op, *e.lhs, *e.rhs);
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

// No register-register bitwise op on `x`/`y` (the real STM8's own
// `AND`/`OR`/`XOR` only ever touch `a` - see STM8Codegen.h's own
// header comment), so this goes byte-at-a-time through `a` instead,
// directly against each cell's own two named byte addresses
// (`bc`/`bc+1`, `ac`/`ac+1`) via this target's real direct-addressed
// ALU instructions - no transit register needed for the arithmetic
// itself, but `lhs` still has to survive evaluating `rhs` on the real
// hardware stack, not in `bc` directly (same reasoning as
// genBinary()'s own header comment: a nested `&`/`|`/`^` reuses `bc`
// too, so storing straight into it here, before `rhs` is evaluated,
// would get clobbered exactly the way a named cell always can).
void STM8Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    pop x\n    ldw bc, x\n";  // bc = lhs (popped only now, after rhs is fully evaluated)
    const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
    code_ << "    ld a, bc\n    " << insn << " a, ac\n    ld ac, a\n";
    code_ << "    ld a, bc+1\n    " << insn << " a, ac+1\n    ld ac+1, a\n";
}

// No variable-count shift instruction - loop `rhs` times instead, one
// bit at a time, via shiftOneBit(). The loop count lives in `lc`, a
// plain memory byte, not `a` - `a` is still needed as scratch to move
// `lc` in and out of memory each iteration (the STM8 has no `dec`/
// `tnz` on an arbitrary memory byte directly the way `incw`/`tnzw`
// exist for a whole word), but it's never live *across* a genExpr()
// call the way a result-carrying cell would need to be.
void STM8Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    ldw x, ac\n    push x\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                           // ac = shift count
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count (always small; low byte is enough)
    code_ << "    pop x\n    ldw bc, x\n";       // bc = value (restored only now, after the count is safely in lc)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    if (isLeft) {
        shiftOneBit("bc", /*isLeft=*/true);
    } else if (isUnsigned) {
        shiftOneBit("bc", /*isLeft=*/false);
    } else {
        // Real STM8 `SRAW` - the arithmetic (sign-preserving)
        // counterpart to `shiftOneBit()`'s own logical `SRLW`.
        code_ << "    ldw x, bc\n    sraw x\n    ldw bc, x\n";
    }
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply wide enough
// for a 16x16 - see STM8Codegen.h's own header comment): `bc` is the
// multiplicand (doubled each iteration), `dx` the multiplier (halved
// each iteration, consumed one bit at a time), `ac` the accumulating
// product, `lc` the loop counter (see genShift()'s own comment on why
// not `a`). Only verified against non-negative operands. `lhs` first
// survives evaluating `rhs` on the real hardware stack, not by an
// early, direct `copy16("bc", ...)` - `rhs` may itself be, or
// contain, a call (e.g. `n * fact(n - 1)`), and that callee's own
// body is just as free to reuse `bc` for its own unrelated purposes
// as any other function - same reasoning as genBitwise()'s own
// header comment.
void STM8Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop x\n    ldw bc, x\n";  // bc = multiplicand (popped only now, after rhs is fully evaluated)
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = product = 0
    code_ << "    ldi a, 16\n    ld lc, a\n";    // loop counter
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    code_ << "    ld a, dx+1\n    andi a, 1\n";
    code_ << "    jreq " << Lskipadd << "\n";
    code_ << "    ldw x, ac\n    ldw y, bc\n    addw x, y\n    ldw ac, x\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide wide enough for a 16/16 -
// see STM8Codegen.h's own header comment): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative - via the real `CPW x,y` +
// `jrult`, no separate loop counter needed at all here. Only verified
// against non-negative operands. `lhs` survives evaluating `rhs` on
// the real hardware stack - see genMul()'s own identical comment on
// why an early `copy16("dx", ...)` isn't safe here either.
// Repeated subtraction only ever divides non-negative magnitudes - a
// signed division/modulo instead records each operand's own sign as
// a 0/1 flag (testing the stored value's own high byte's own top
// bit directly, no bias trick needed for *this*), negates it to that
// same non-negative magnitude, divides via the existing loop, then
// restores the quotient's sign (negative iff the operands' signs
// disagreed) and the remainder's sign (always the dividend's own,
// matching C's truncating-toward-zero convention) - the same "widen/
// fix up around a narrower real primitive" shape every signed-
// division backend here uses, just fixing up the *sign* instead of
// the *width*.
void STM8Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop x\n    ldw dx, x\n";  // dx = dividend (popped only now, after rhs is fully evaluated)
    std::string divisorNeg = allocTemp();
    declareTemp(divisorNeg);
    if (!isUnsigned) {
        std::string Ld = newLabel(), Lr = newLabel();
        code_ << "    ldi a, 0\n    ld lc, a\n";  // lc = dividend-negative flag
        code_ << "    ld a, dx\n    andi a, 0x80\n    tnza\n    jreq " << Ld << "\n";
        code_ << "    ldw x, dx\n    negw x\n    ldw dx, x\n";
        code_ << "    ldi a, 1\n    ld lc, a\n";
        code_ << Ld << ":\n";
        code_ << "    ldi a, 0\n    ld " << divisorNeg << "+1, a\n";
        code_ << "    ld a, bc\n    andi a, 0x80\n    tnza\n    jreq " << Lr << "\n";
        code_ << "    ldw x, bc\n    negw x\n    ldw bc, x\n";
        code_ << "    ldi a, 1\n    ld " << divisorNeg << "+1, a\n";
        code_ << Lr << ":\n";
    }
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = quotient = 0 (dx/bc are now both non-negative magnitudes)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldw x, dx\n    ldw y, bc\n    cpw x, y\n";
    code_ << "    jrult " << Ldone << "\n";  // dx < bc - this round's subtraction can't happen, stop
    code_ << "    subw x, y\n    ldw dx, x\n";
    code_ << "    ldw x, ac\n    incw x\n    ldw ac, x\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) {
        copy16("ac", "dx");
        if (!isUnsigned) {
            std::string Lskip = newLabel();
            code_ << "    ld a, lc\n    tnza\n    jreq " << Lskip << "\n";
            code_ << "    ldw x, ac\n    negw x\n    ldw ac, x\n";
            code_ << Lskip << ":\n";
        }
        return;
    }
    if (!isUnsigned) {
        std::string Lskip = newLabel();
        code_ << "    ld a, lc\n    xor a, " << divisorNeg << "+1\n    tnza\n    jreq " << Lskip << "\n";
        code_ << "    ldw x, ac\n    negw x\n    ldw ac, x\n";
        code_ << Lskip << ":\n";
    }
}

void STM8Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false, isUnsigned);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false, isUnsigned);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true, isUnsigned);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true, isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: `CPW x,y` after loading
// both operands sets Z iff they're equal, full 16 bits at once.
// `lhs` survives evaluating `rhs` on the real hardware stack, not an
// early `copy16("bc", ...)` - see genMul()'s own identical comment
// on why (`rhs` may itself be, or contain, a call that reuses `bc`
// for its own unrelated purposes).
void STM8Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    ldw y, ac\n    pop x\n";
    code_ << "    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "jreq" : "jrne") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// I8080Codegen.h's own header comment on why: `CPW`'s own flags, like
// the 8080's status word, aren't trusted here for a signed compare):
// flipping just the top bit of each operand's high byte maps the
// signed range onto the unsigned one while preserving order, so a
// plain unsigned `CPW`+`jrult`/`jruge` is all that's left to do.
// `a` survives evaluating `b` on the real hardware stack, not an
// early `copy16("bc", ...)` - see genMul()'s own identical comment
// on why (`b` may itself be, or contain, a call that reuses `bc` for
// its own unrelated purposes).
void STM8Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned) {
    genExpr(a);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(b);  // ac = b
    code_ << "    pop x\n    ldw bc, x\n";  // bc = a (popped only now, after b is fully evaluated)
    if (!isUnsigned) {
        code_ << "    ld a, bc\n    xori a, 0x80\n    ld bc, a\n";  // bc's high byte, sign flipped
        code_ << "    ld a, ac\n    xori a, 0x80\n    ld ac, a\n";  // ac's high byte, sign flipped
    }
    code_ << "    ldw x, bc\n    ldw y, ac\n    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (invert ? "jruge" : "jrult") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// After genExpr(e) just left `e`'s own value - `ac`, if narrow, or
// `wordCountOf(e.type)` words on the real hardware stack (LSW on top)
// if wide - this leaves `x` holding a plain zero/nonzero value
// (`tnzw x`-tested), with any wide stack space already released.
void STM8Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    ldw x, ac\n    tnzw x\n";
        return;
    }
    int words = wordCountOf(e.type);
    popWideInto("a", words);
    auto bytes = wideByteLabelsLsbFirst("a", words);
    code_ << "    ldi a, 0\n    ld lc, a\n";
    for (const auto &b : bytes) code_ << "    ld a, " << b << "\n    or a, lc\n    ld lc, a\n";
    code_ << "    ld a, lc\n    tnza\n";  // Z set iff every word was zero - every caller only ever tests the flag
}

void STM8Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jreq " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jreq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jrne " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jrne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jreq " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

std::string STM8Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the stm8 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> STM8Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleSTM8(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see STM8Assembler.h's own `assembleSTM8Object`), and whichever
    // of these names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it - this file never touches a real COFF/ELF/Mach-O/
    // a.out byte layout itself.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSTM8Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
