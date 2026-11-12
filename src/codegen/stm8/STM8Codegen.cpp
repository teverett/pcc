#include "STM8Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "STM8Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Program load/entry address, and the real hardware stack's initial top.
constexpr uint32_t kBaseAddress = 0x0100;
constexpr uint32_t kStackTop = 0xFF00;

// Initial top of the software frame stack; sits between the code/data
// region and the real hardware stack, clear of both.
constexpr uint32_t kFrameStackTop = 0xE000;
}  // namespace

void STM8Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

std::string STM8Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType`'s width picks db (char array) vs dw (every other array - a
// long/long long array's elements still only occupy one native word each,
// a known gap, see genIndex()).
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

// Assumes `x` already holds the target address.
void STM8Codegen::storeIndirect(const std::string &src) {
    code_ << "    ld a, " << src << "\n    stxind\n";
    code_ << "    incw x\n";
    code_ << "    ld a, " << src << "+1\n    stxind\n";
}

void STM8Codegen::loadIndirect(const std::string &dst) {
    code_ << "    ldxind\n    ld " << dst << ", a\n";
    code_ << "    incw x\n";
    code_ << "    ldxind\n    ld " << dst << "+1, a\n";
}

void STM8Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ldw x, " << lbl << "\n";
    code_ << "    " << (isLeft ? "slaw" : "srlw") << " x\n";
    code_ << "    ldw " << lbl << ", x\n";
}

// STM8 has no sign-extend instruction; tests the byte's top bit instead
// (andi a, 0x80 isolates it, tnza turns that into a zero/nonzero test) and
// branches once. `a` must already hold the byte to extend, and ends up
// holding the fill byte (0xFF or 0x00).
void STM8Codegen::genSignFillByte() {
    code_ << "    andi a, 0x80\n    tnza\n";
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    jrne " << Lneg << "\n";
    code_ << "    ldi a, 0\n    jp " << Ldone << "\n";
    code_ << Lneg << ":\n    ldi a, 0xFF\n";
    code_ << Ldone << ":\n";
}

// Assumes `x` already holds the effective storage address.
void STM8Codegen::loadOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ldxind\n";
        code_ << "    ld ac+1, a\n";
        if (type.isUnsigned) {
            code_ << "    ldi a, 0\n";
        } else {
            genSignFillByte();  // a is unchanged by "ld ac+1,a" above
        }
        code_ << "    ld ac, a\n";
        return;
    }
    loadAcIndirect();
}

void STM8Codegen::storeOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ld a, ac+1\n    stxind\n";
        return;
    }
    storeAcIndirect();
}

// Pops `words` words off the real hardware stack (LSW on top) into
// `role`'s wideScratch() cells.
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

// Pushes `slot`'s `words`-word value onto the real hardware stack, word 0
// (least significant) ending up on top.
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

// Pops `words` words off the real hardware stack (least significant on
// top) into `slot`'s storage.
void STM8Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop x\n    ldw bc, x\n";
        genWideWordAddr(slot, words, i);
        storeIndirect("bc");
    }
}

// Runtime-address counterpart of genWideWordAddr(): same
// most-significant-word-first layout, base read from `addrCell`.
void STM8Codegen::pushWideFromAddress(const std::string &addrCell, int words) {
    for (int i = words - 1; i >= 0; i--) {
        int byteOff = 2 * (words - 1 - i);
        code_ << "    ldw x, " << addrCell << "\n";
        if (byteOff > 0) code_ << "    ldwi y, " << byteOff << "\n    addw x, y\n";
        loadIndirect("bc");
        code_ << "    ldw x, bc\n    push x\n";
    }
}

void STM8Codegen::popWideToAddress(const std::string &addrCell, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop x\n    ldw bc, x\n";
        int byteOff = 2 * (words - 1 - i);
        code_ << "    ldw x, " << addrCell << "\n";
        if (byteOff > 0) code_ << "    ldwi y, " << byteOff << "\n    addw x, y\n";
        storeIndirect("bc");
    }
}

// Every byte-at-a-time wide helper below first pops both operands into the
// fixed "wa"/"wb" scratch cells, then walks every byte from least to most
// significant, chaining the carry/borrow flag through adc/sbc.
namespace {
// `role`'s `words`-word value as individual byte labels, least significant
// byte first; since each cell stores its high byte first, within one word
// the low byte (+1) comes before its own high byte in this ordering.
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

// Two's-complement negation in place: complement every byte, then add 1
// into the least significant byte and fold the carry through the rest via
// adc a, zero.
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

// Compares most-significant byte first; only that byte needs the
// signed/unsigned sign-bit bias, since once a byte differs every
// byte after it is comparing a pure unsigned magnitude.
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
        code_ << "    sub a, lc\n";
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

// One bit at a time, most significant byte first - rlc/rrc rotate `a`
// through the carry flag, chaining it byte to byte. A signed right shift's
// top byte needs its sign bit preserved: rlc (copies the sign bit into
// carry) then rrc on the same original byte rotates that bit back in as
// the new top bit while still shifting right and producing the correct
// carry-out for the next byte's plain rrc.
void STM8Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);
    popWideInto("a", words);
    genExpr(rhs);
    code_ << "    ld a, ac+1\n    ld lc, a\n";
    auto bytesMsbFirst = wideByteLabelsLsbFirst("a", words);
    std::reverse(bytesMsbFirst.begin(), bytesMsbFirst.end());
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    if (isLeft) {
        code_ << "    rcf\n";
        for (size_t idx = bytesMsbFirst.size(); idx-- > 0;) {
            code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rlc a\n    ld " << bytesMsbFirst[idx] << ", a\n";
        }
    } else {
        for (size_t idx = 0; idx < bytesMsbFirst.size(); idx++) {
            if (idx == 0 && !isUnsigned) {
                code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rlc a\n";
                code_ << "    ld a, " << bytesMsbFirst[idx] << "\n    rrc a\n";
            } else {
                if (idx == 0) code_ << "    rcf\n";
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

// No simulator exists here to verify a hand-written multi-byte multiply/
// divide routine against, so failing loudly is safer than an unverifiable one.
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

// The real work here is crossing the native-word/long-or-long-long
// boundary (adding words of sign/zero extension, or dropping down to the
// low word) and re-narrowing a char result.
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
        code_ << "    ldw x, " << wideScratch("a", 0) << "\n    ldw ac, x\n";
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
        genExpr(*e.lhs);
        code_ << "    ldw x, ac\n    ldw " << wideScratch("a", 0) << ", x\n";
        // A pointer value is always zero-extended: from.isUnsigned would
        // otherwise describe the pointee's signedness, not the pointer's,
        // and a pointer has no sign to extend regardless.
        bool extendUnsigned = isPointer(from) || from.isUnsigned;
        for (int i = 1; i < toWords; i++) {
            if (extendUnsigned) {
                code_ << "    ldwi x, 0\n    ldw " << wideScratch("a", i) << ", x\n";
            } else {
                code_ << "    ld a, ac\n";
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
        code_ << "    ld a, " << wideScratch("a", fromWords - 1) << "\n";
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
    // Real separate compilation means `prog` may not define `main` at all;
    // `objectMode` means no object file emits this entry sequence itself -
    // STM8LinkerFactory.cpp synthesizes exactly one once every object is
    // merged and `main`'s linked address is resolvable. An object file's
    // own base is `org 0`; placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ldwi sp, " << kStackTop << "\n";
        code_ << "    ldwi x, " << kFrameStackTop << "\n    ldw fsp, x\n";
        code_ << "    ldwi x, 0\n    ldw fp, x\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The result sits in `ac` once execution reaches `halt`; also left
        // in `x` for a quick look via a register dump.
        code_ << "    ldw x, ac\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // Every genExpr() call in the whole linked program shares these cells,
    // so unlike other symbols they can't be redeclared per-object: declared
    // once here in whole-program mode; in object mode every reference is
    // deferred as an external relocation against STM8LinkerFactory.cpp's
    // synthesized entry object.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        declareTemp("fp");
        declareTemp("fsp");
        trailer_.push_back("lc: db 0");
        // Permanent zero byte: adc a, zero folds a pending carry into a
        // byte with nothing else added (see genWideNegate()).
        trailer_.push_back("zero: db 0");
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global must never get storage here; only referenced
        // by name, so the assembler's undefined-symbol handling defers it
        // to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else if (slot->type.base == BaseType::Struct) {
            // No struct initializer exists in this subset, so this is
            // always zero-filled; reuses declareArray's own element-list
            // emission (one zero "db" per byte) rather than the width-
            // bucketed logic below, which only handles even widths.
            declareArray(lbl, std::vector<ir::ArrayInitElem>(slot->type.structSize), Type{BaseType::Char, false});
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 1) {
                trailer_.push_back(lbl + ": db " + std::to_string(static_cast<int>(static_cast<uint8_t>(v))) +
                                    "  ; " + slot->sourceName);
            } else if (w == 2) {
                uint16_t v16 = static_cast<uint16_t>(v);
                // High byte first, low byte second (big-endian).
                trailer_.push_back(lbl + ": db " + std::to_string((v16 >> 8) & 0xFF) + "  ; " + slot->sourceName);
                trailer_.push_back("db " + std::to_string(v16 & 0xFF));
            } else {
                // Each word emitted most significant first, least
                // significant word last.
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
// Layout, growing down from `fp` (the saved caller's `fp`, the first word
// pushed for this call):
//
//   fp+2*numParams .. fp+2   incoming arguments (param 0 closest to `fp`;
//                            pushed in reverse order, so param 0 is pushed last)
//   fp                       this call's saved caller's `fp`
//   fp-2 .. fp-2*frameSize   this function's Local storage, declaration order
//
// `fp`/`fsp` are ordinary named memory cells, not real registers: `x`/`y`
// are already committed as ephemeral transit registers, so there's no
// spare register to hold a live frame pointer. They're independent of the
// real hardware `sp` (used only for call/ret and genBinary()'s push/pop scratch).
void STM8Codegen::pushFrameWord(const std::string &cell) {
    code_ << "    ldw x, fsp\n    ldwi y, 2\n    subw x, y\n    ldw fsp, x\n";
    storeIndirect(cell);  // x still holds the just-decremented fsp
}

void STM8Codegen::genPrologue(const ir::Function &fn) {
    pushFrameWord("fp");
    code_ << "    ldw x, fsp\n    ldw fp, x\n";
    if (fn.frameSize > 0) {
        code_ << "    ldw x, fp\n    ldwi y, " << fn.frameSize << "\n    subw x, y\n    ldw fsp, x\n";
    }
}

// Resets `fsp` past this call's incoming arguments too, so the callee
// cleans up its own argument area and the caller needs no adjustment after
// `call`. Uses `bc` as scratch, never `ac`, since `ac` already holds the
// live return value by the time this runs.
void STM8Codegen::genEpilogue(const ir::Function &fn) {
    code_ << "    ldw x, fp\n";
    loadIndirect("bc");  // bc = the caller's saved fp
    int numParams = static_cast<int>(fn.params.size());
    code_ << "    ldw x, fp\n    ldwi y, " << ((numParams + 1) * 2) << "\n    addw x, y\n    ldw fsp, x\n";
    code_ << "    ldw x, bc\n    ldw fp, x\n";
    code_ << "    ret\n";
}

void STM8Codegen::genSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldwi x, " << label(slot) << "\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    ldw x, fp\n    ldwi y, " << slot->frameOffset << "\n    addw x, y\n";
        return;
    }
    int below = ir::localOffsetDescendingWide(*slot, typeWidths());
    code_ << "    ldw x, fp\n    ldwi y, " << below << "\n    subw x, y\n";
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
                // A wide value sits on the hardware stack (LSW on top) after
                // genExpr(); pop it into the "a" wideScratch() cells since
                // genEpilogue() never touches that stack (see the matching
                // push-back in genExpr()'s Call case).
                if (isWide(s.initOrReturnOrExpr->type)) popWideInto("a", wordCountOf(s.initOrReturnOrExpr->type));
            }
            genEpilogue(*currentFunction_);
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // A Local's words are already reserved by genPrologue(); an
            // array initializer still runs its per-element stores every
            // time this declaration executes (not once, statically) since
            // a fresh call gets a fresh frame.
            if (s.declSlot->arraySize > 0) {
                int stride = widthOfType(s.declSlot->type) == 1 ? 1 : 2;
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    const ir::ArrayInitElem &elem = s.declSlot->arrayInit[i];
                    if (elem.isSymbol && elem.slot && elem.slot->kind != ir::SlotKind::Global) {
                        // A Local's/Param's address only exists at runtime
                        // (relative to this call's `fp`), unlike a Global's
                        // or Function's compile-time constant address.
                        genSlotAddress(elem.slot);
                        code_ << "    ldw ac, x\n";
                    } else {
                        code_ << "    ldwi x, " << arrayElemText(elem) << "\n    ldw ac, x\n";
                    }
                    genSlotAddress(s.declSlot);  // x = address of element 0
                    if (i > 0) {
                        // Element i sits above element 0, the array's
                        // lowest address.
                        code_ << "    ldwi y, " << (i * stride) << "\n    addw x, y\n";
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
            if (e.lhs) {  // slot[idx] = rhs - never wide (see genIndex()'s known limitation)
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
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                genCompoundThroughPtr(e);
                return;
            }
            // Plain scalar target: a Global's label or a Local/Param's
            // fp-relative address has no side effects, so it's simply
            // recomputed after evaluating rhs rather than held across it.
            genExpr(*e.rhs);
            copy16("dx", "ac");  // dx = rhs, held across loading old
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16("ac", label(e.slot));
            } else {
                genSlotAddress(e.slot);
                loadOperand(e.slot->type);
            }
            copy16("bc", "ac");  // bc = old, ac = rhs - genCompoundOp()'s preconditions
            copy16("ac", "dx");
            if (e.yieldOld) copy16("cx", "bc");  // cx = old, held for the final result
            genCompoundOp(e.op, e.type);
            narrowAcTo(e.type);
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16(label(e.slot), "ac");
            } else {
                genSlotAddress(e.slot);
                storeOperand(e.slot->type);
            }
            if (e.yieldOld) copy16("ac", "cx");
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jreq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);              // ac = pointer value
            code_ << "    ldw ptr, ac\n";
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness: the pointee type of a
            // typed pointer, or a plain native word for untyped-int-as-address.
            genExpr(*e.lhs);  // ac = pointer value
            if (isWide(e.type)) {
                code_ << "    ldw ptr, ac\n";
                genExpr(*e.rhs);
                popWideToAddress("ptr", wordCountOf(e.type));
                return;
            }
            code_ << "    ldw x, ac\n    push x\n";
            genExpr(*e.rhs);
            code_ << "    pop x\n";
            storeOperand(e.type);
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
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // ac = pointer value
                if (isWide(e.type)) {
                    code_ << "    ldw ptr, ac\n";
                    pushWideFromAddress("ptr", wordCountOf(e.type));
                } else {
                    code_ << "    ldw x, ac\n";
                    loadOperand(e.type);
                }
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
            // Pushed in reverse (last argument first), so param 0 ends up
            // pushed last, at fp+2 - matching genSlotAddress()'s
            // fp+(paramIndex+1)*2 formula. A wide argument's words
            // (LSW on top) are popped and frame-pushed one at a time, least
            // significant first, landing the most significant word at the
            // lowest address - where genWideWordAddr() expects it.
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
                // Callee left its wide return value in the "wa" scratch
                // cells; push it back as this expression's result.
                pushWideFrom("a", wordCountOf(e.type));
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // `CALL [mem]` fetches the entry address from the word stored
            // at mem rather than treating mem itself as the callee, so the
            // computed callee address is spilled to a fresh temp cell first.
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
            // Real STM8 interrupt enable/disable (RIM/SIM).
            code_ << "    " << (e.op == "ion" ? "rim" : "sim") << "\n";
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

// Leaves slot[idxExpr]'s effective address in the `ptr` memory cell;
// idxExpr is evaluated first and alone, since no named cell survives a
// general recursive genExpr() call.
void STM8Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    code_ << "    ldw x, ac\n";
    for (int i = 0; i < elemShift(slot->type); i++) code_ << "    slaw x\n";  // x *= widthOf(slot->type)
    code_ << "    ldw ptr, x\n";
    genSlotAddress(slot);  // x = slot's own storage address (Global/Local/Param alike)
    if (slot->arraySize > 0) {
        code_ << "    ldw y, ptr\n    addw x, y\n    ldw ptr, x\n";  // ptr = base + index*2
        return;
    }
    // Ordinary pointer arithmetic: base-plus-scaled-index regardless of
    // where the pointer variable itself lives.
    loadIndirect("bc");
    code_ << "    ldw y, bc\n";
    code_ << "    ldw x, ptr\n    addw x, y\n    ldw ptr, x\n";
}

// Every array element reads only one native word regardless of its real
// width - a known gap for long/long long array elements (the index is
// scaled correctly by elemShift(), but a multi-word element's remaining
// words are never read/written). Scalars of every width work fully.
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

// Every binary op saves its left operand across evaluating the right one
// on the real hardware stack (push x/pop x), never in a named memory cell,
// since a nested binary op might reuse that cell.
void STM8Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Shares one common type via e.lhs->type, except shift (only LHS's
    // type matters) and &&/|| (width-independent).
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

// No register-register bitwise op on `x`/`y` (STM8 AND/OR/XOR only touch
// `a`), so this goes byte-at-a-time through `a` against each cell's named
// byte addresses. `lhs` still survives evaluating `rhs` on the hardware
// stack rather than in `bc` directly, since a nested `&`/`|`/`^` also reuses `bc`.
void STM8Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    pop x\n    ldw bc, x\n";  // bc = lhs
    const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
    code_ << "    ld a, bc\n    " << insn << " a, ac\n    ld ac, a\n";
    code_ << "    ld a, bc+1\n    " << insn << " a, ac+1\n    ld ac+1, a\n";
}

// No variable-count shift instruction - loop `rhs` times instead, one bit
// at a time, via shiftOneBit(). The loop count lives in `lc` (a plain
// memory byte, not `a`): STM8 has no dec/tnz on an arbitrary memory byte
// directly, so `a` is still needed as scratch to move it in and out.
void STM8Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);                           // ac = shift count
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count (low byte is enough)
    code_ << "    pop x\n    ldw bc, x\n";       // bc = value
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    if (isLeft) {
        shiftOneBit("bc", /*isLeft=*/true);
    } else if (isUnsigned) {
        shiftOneBit("bc", /*isLeft=*/false);
    } else {
        // SRAW: arithmetic (sign-preserving) counterpart to shiftOneBit()'s logical SRLW.
        code_ << "    ldw x, bc\n    sraw x\n    ldw bc, x\n";
    }
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply wide enough for a
// 16x16): `bc` is the multiplicand (doubled each iteration), `dx` the
// multiplier (halved, consumed one bit at a time), `ac` the accumulating
// product. Only verified against non-negative operands. `lhs` survives
// evaluating `rhs` on the hardware stack rather than an early
// copy16("bc", ...), since `rhs` may itself be or contain a call that reuses `bc`.
void STM8Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop x\n    ldw bc, x\n";  // bc = multiplicand
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

// Repeated subtraction (no hardware 16/16 divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac` counts
// how many times `bc` was subtracted via CPW+jrult, with no separate loop
// counter needed. Only divides non-negative magnitudes, so a signed
// division/modulo first records each operand's sign as a 0/1 flag, negates
// to a non-negative magnitude, divides via the loop, then restores the
// quotient's sign (negative iff the signs disagreed) and the remainder's
// sign (always the dividend's, matching C's truncating-toward-zero convention).
void STM8Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop x\n    ldw dx, x\n";  // dx = dividend
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
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldw x, dx\n    ldw y, bc\n    cpw x, y\n";
    code_ << "    jrult " << Ldone << "\n";  // dx < bc: stop
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

// Precondition: `bc` holds `old`, `ac` holds `rhs`. Each operator's body
// below mirrors its genBinary() helper's post-setup logic on the same
// named cells. Leaves the raw new value in `ac`.
void STM8Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+" || op == "-") {
        code_ << "    ldw x, bc\n    ldw y, ac\n";
        code_ << "    " << (op == "+" ? "addw" : "subw") << " x, y\n    ldw ac, x\n";
        return;
    }
    if (op == "*") {
        copy16("dx", "ac");  // dx = multiplier (rhs) - bc already holds the multiplicand (old)
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
        return;
    }
    if (op == "/" || op == "%") {
        code_ << "    ldw x, bc\n    ldw dx, x\n";  // dx = dividend (old)
        copy16("bc", "ac");                           // bc = divisor (rhs)
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
        code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = quotient = 0
        std::string Lloop = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    ldw x, dx\n    ldw y, bc\n    cpw x, y\n";
        code_ << "    jrult " << Ldone << "\n";
        code_ << "    subw x, y\n    ldw dx, x\n";
        code_ << "    ldw x, ac\n    incw x\n    ldw ac, x\n";
        code_ << "    jp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        if (op == "%") {
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
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    ld a, bc\n    " << insn << " a, ac\n    ld ac, a\n";
        code_ << "    ld a, bc+1\n    " << insn << " a, ac+1\n    ld ac+1, a\n";
        return;
    }
    // "<<" or ">>"
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count; bc already holds the value to shift (old)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    if (op == "<<") {
        shiftOneBit("bc", /*isLeft=*/true);
    } else if (isUnsigned) {
        shiftOneBit("bc", /*isLeft=*/false);
    } else {
        code_ << "    ldw x, bc\n    sraw x\n    ldw bc, x\n";
    }
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// `ptr` holds the resolved address on entry; stashed on the hardware stack
// while `rhs` is evaluated, since `rhs` may itself be or contain an
// arbitrarily deep expression free to reuse every named cell here.
void STM8Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    ldw x, ptr\n    push x\n";  // stash address
    genExpr(*e.rhs);                           // ac = rhs
    copy16("dx", "ac");
    code_ << "    pop x\n    ldw ptr, x\n";       // ptr = address, restored
    code_ << "    ldw x, ptr\n";
    loadOperand(e.type);  // ac = old
    copy16("bc", "ac");    // bc = old, ac = rhs - genCompoundOp()'s preconditions
    copy16("ac", "dx");
    if (e.yieldOld) copy16("cx", "bc");  // cx = old, held for the final result
    genCompoundOp(e.op, e.type);
    narrowAcTo(e.type);
    code_ << "    ldw x, ptr\n";
    storeOperand(e.type);
    if (e.yieldOld) copy16("ac", "cx");  // ac = old (final result)
}

void STM8Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    if (to.isUnsigned) {
        code_ << "    ldi a, 0\n    ld ac, a\n";
    } else {
        code_ << "    ld a, ac+1\n";
        genSignFillByte();
        code_ << "    ld ac, a\n";
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

// CPW x,y after loading both operands sets Z iff they're equal, full 16
// bits at once - no sign handling needed. `lhs` survives evaluating `rhs`
// on the hardware stack rather than an early copy16("bc", ...), since
// `rhs` may itself be or contain a call that reuses `bc`.
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

// Signed 16-bit "a < b" via the sign-bit bias trick: flipping just the top
// bit of each operand's high byte maps the signed range onto the unsigned
// one while preserving order, so a plain unsigned CPW+jrult/jruge suffices.
// `a` survives evaluating `b` on the hardware stack rather than an early
// copy16("bc", ...), since `b` may itself be or contain a call that reuses `bc`.
void STM8Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned) {
    genExpr(a);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(b);  // ac = b
    code_ << "    pop x\n    ldw bc, x\n";  // bc = a
    if (!isUnsigned) {
        code_ << "    ld a, bc\n    xori a, 0x80\n    ld bc, a\n";
        code_ << "    ld a, ac\n    xori a, 0x80\n    ld ac, a\n";
    }
    code_ << "    ldw x, bc\n    ldw y, ac\n    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (invert ? "jruge" : "jrult") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// A wide value (words on the hardware stack, LSW on top) is ORed together
// into `lc` first, since it's true iff any word is nonzero; a narrow value
// is just tnzw-tested directly, leaving the zero/nonzero result in the flags.
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
    code_ << "    ld a, lc\n    tnza\n";
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

std::optional<std::string> STM8Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleSTM8(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSTM8Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
