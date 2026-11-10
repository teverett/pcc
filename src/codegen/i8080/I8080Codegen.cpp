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

// Where the whole program is loaded and starts executing - above the conventional
// RST-vector/boot area (0x0000-0x00FF), though nothing here uses interrupts or RST.
constexpr uint32_t kBaseAddress = 0x0100;
// Initial SP: the 8080's SP is undefined at reset (unlike the 6502's fixed $0100 stack page).
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void I8080Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's decimal text, or a symbol element's (a dispatch-table
// address-of-function/global) label text.
std::string I8080Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType` picks the data directive.
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
    // `lda`/`sta` address `ac` directly by name, never touching HL, so the caller's address in
    // HL survives across both byte stores.
    code_ << "    lda ac\n    mov m,a\n";
    code_ << "    inx h\n";
    code_ << "    lda ac+1\n    mov m,a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl`. `ana a` clears carry without
// disturbing A (the 8080 has no explicit clear-carry instruction), giving a known 0 carry-in to
// shift into the vacated bit.
void I8080Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    lhld " << lbl << "\n";
    code_ << "    ana a\n";
    if (isLeft) {
        code_ << "    mov a,l\n    ral\n    mov l,a\n";
        code_ << "    mov a,h\n    ral\n    mov h,a\n";
    } else {
        // Logical (unsigned) shift.
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

// HL must hold the operand's address. Sign-extension idiom: `ral` rotates L's bit 7 into carry,
// then `sbb a` (A-A-CY) yields 0x00 or 0xFF - the sign-extension byte, in three instructions
// with no branch. An unsigned `char` just zero-fills H instead.
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
        // Popped into BC, not DE - genFrameAddr() below uses DE as scratch for a Local/Param's
        // offset, which would clobber a value popped into DE before it's stored.
        code_ << "    pop b\n";
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lxi h, " << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "\n";
        } else {
            genFrameAddr(slot, 2 * i);
        }
        code_ << "    mov m,c\n    inx h\n    mov m,b\n";
    }
}

// Multi-byte add/sub/negate/bitwise/compare all work byte-at-a-time, LSB first, chaining the
// carry/borrow flag through ADC/SBB (no 16-bit ADC/SBB on this chip). Every wide helper below
// first pops both operands into the fixed "wa"/"wb" scratch cells, then walks every byte of
// every word from least to most significant.
namespace {
// `role`'s `words`-word value, as individual byte labels, least significant byte first.
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

// Compares most-significant byte first - only that byte needs the signed/unsigned bias (see
// emitLessCompare()); once a byte differs, every byte after it compares a plain unsigned
// magnitude.
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

// One bit at a time, most significant byte first, chaining the carry flag from one byte to the
// next via `ral`/`rar`. A signed right shift needs the top byte's sign bit preserved
// (arithmetic, not logical): `mov b,a`/`ral`/`mov a,b`/`rar` computes a software SRA by first
// copying the sign bit into carry, then rotating it back in from the *original* byte while
// shifting right.
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

// Unimplemented: no simulator in this project's test harness to verify a hand-written
// multi-word multiply/divide routine against, so failing loudly is safer than shipping an
// unverifiable one.
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

// An implicit or explicit conversion.
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
        // Every word is written into the "wa" scratch cells and pushed via pushWideFrom() (LSW
        // on top) rather than pushing word 0 directly followed by the extension word(s), which
        // would leave an extension word on top instead of the least significant one.
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac already holds the native-width value, sign/zero-extended to 16 bits
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
    // Real separate compilation means `prog` may not define `main` at all. `objectMode` means
    // no object file emits this entry sequence itself; I8080LinkerFactory.cpp synthesizes
    // exactly one once every object is merged and `main`'s linked address is resolvable. An
    // object file's base is `org 0` - placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lxi sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's final result sits in `ac` once execution reaches `halt` below; this
        // extra `lhld` also leaves it in HL for a quick look via register dump.
        code_ << "    lhld ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    // The fixed memory pseudo-registers. Every genExpr() call in the whole linked program
    // shares the same `ac`, so unlike every other symbol this backend emits, these five can't
    // be redeclared per-object: in whole-program mode they're declared once here as ordinary
    // temps; in object mode this file never declares them, so every reference is deferred as an
    // external relocation against the definition I8080LinkerFactory.cpp's synthesized entry
    // object provides.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // The current function's software frame pointer, shared the same way as the cells
        // above since a callee's prologue and its caller must agree on where it lives.
        declareTemp("fp");
        // Wide (`long`/`long long`) scratch cells.
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("ret", i));
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file - it must never get a local storage
        // word here, only be referenced by name, letting the assembler defer it to a relocation.
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

// The whole generated text is attributed to whichever file defines `main`.
std::map<std::string, std::string> I8080Codegen::perFileAsm() const { return perFileAsm_; }

// Every activation gets its own real stack frame: `lhld fp`/`push h` saves the caller's frame
// pointer, `lxi h,0`/`dad sp` reads SP into HL (there's no direct "HL=SP" instruction) right
// after that push - exactly the address of the just-pushed word - and `shld fp` makes that this
// call's new frame base. `dad d` (with the locals' negative total byte size in DE) then `sphl`
// moves SP down below them in one step, reserving that memory before anything later in this
// function can land on top of it.
void I8080Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    lhld fp\n    push h\n";
    code_ << "    lxi h, 0\n    dad sp\n    shld fp\n";
    // `fn.func->frameSize` alone can fall up to `intUnits-1` bytes short of the deepest byte a
    // Local slot needs below `fp`; padding by that much covers the worst case.
    long reserveBytes = static_cast<long>(fn.func->frameSize) + (typeWidths().intUnits - 1);
    if (reserveBytes > 0) {
        code_ << "    lxi d, " << (-reserveBytes) << "\n    dad d\n    sphl\n";
    }
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

// Undoes genFunction()'s prologue: `lhld fp`/`sphl` snaps SP back to this call's frame base,
// dropping the locals' reserved region; `pop h`/`shld fp` restores the caller's `fp`; `ret`
// returns.
void I8080Codegen::emitEpilogue() {
    code_ << "    lhld fp\n    sphl\n";
    code_ << "    pop h\n    shld fp\n";
    code_ << "    ret\n";
}

// A Local/Param slot's effective address, computed fresh every time rather than cached - this
// target has no addressing mode that could do it as part of a single instruction.
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
                // A wide value sits on the real stack (LSW on top) after genExpr(); pop it into
                // the "wret" scratch cells since emitEpilogue()'s `sphl` is about to reset SP
                // back to `fp`, abandoning whatever's above it. genExpr()'s Call case reads it
                // back out of "wret" the same way.
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) {
                        code_ << "    pop h\n    shld " << wideScratch("ret", i) << "\n";
                    }
                }
            }
            // An early return bypasses genFunction()'s fall-through epilogue, so it needs this
            // same frame teardown inline.
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
            // No "else" case for a bare, uninitialized scalar: its frame word already exists
            // (genFunction()'s prologue reserved it), so its value is whatever was already in
            // that stack slot until something assigns it.
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
                    // `ptr` is a single shared scratch cell - evaluating `rhs` might itself
                    // index another array and clobber it, so the element address is saved on
                    // the real stack first; rhs's pushed words land on top of it and are popped
                    // into "wb" before the address underneath is restored.
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
            // Stores at e.type's width - the pointee type of a typed pointer, or native-word
            // width for the untyped-int-as-address style. A wide pointee mirrors the indexed
            // Assign's wide branch above, except `ptr` comes from evaluating e.lhs (the
            // pointer, always narrow) instead of genIndexAddr().
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
                // Loads at e.type's width - see DerefAssign above.
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
            // Each argument is pushed last-declared first, so argument 0 ends up closest to the
            // return address `call` is about to push (paramIndex 0 is `fp+4`). Each value is
            // computed into `ac` and transited through HL, since `PUSH` only takes a register
            // pair, never a memory operand.
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
            // The caller cleans its own pushed arguments off afterward (cdecl convention),
            // needing no cooperation from the callee.
            if (totalWords > 0) {
                code_ << "    lxi h, " << (totalWords * 2) << "\n    dad sp\n    sphl\n";
            }
            if (isWide(e.type)) {
                // The callee left its wide return value in the "wret" scratch cells; push it
                // back as this expression's result, most significant word first so the least
                // significant ends up on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    lhld " << wideScratch("ret", i) << "\n    push h\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // The 8080 has no indirect-CALL instruction, only `PCHL` (jump to the address in
            // HL), so a real CALL is emulated: push the return address, then `PCHL`. The
            // callee's `ret` pops that address back into PC, landing right after `pchl` below.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            code_ << "    lhld ac\n";  // HL = callee's address - pchl's implicit target
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
            // Real 8080 interrupt enable/disable: EI/DI.
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

// Leaves slot[idxExpr]'s effective address in the `ptr` memory cell. `idxExpr` is evaluated
// first and alone, since genExpr() may itself need `ptr`/`bc`/`cx`/`dx` for nested
// subexpressions.
//
// `slot->arraySize > 0` means `slot` is itself the array; otherwise `slot` is a pointer
// variable and the base comes from reading its stored value. A Global's base is a compile-
// time-known label, or for a pointer variable a direct read of it (loadPair()); a Local/Param's
// base is runtime-relative to `fp` (genFrameAddr()), and for a pointer variable means
// dereferencing that frame address.
//
// Both a Global and a Local array's element i sits at base+2*i; genFrameAddr() gives a Local
// array's element 0, its lowest address.
void I8080Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // HL = index * widthOfType(slot->type), via `dad h` (doubling) repeated elemShift() times.
    // Stashed in `ptr` immediately since loading the base below goes through HL too.
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
            // slot is a pointer variable living in this frame - DE needs its stored runtime
            // value, not its own address.
            code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        }
    }
    code_ << "    lhld ptr\n    dad d\n    shld ptr\n";  // ptr = index*2 + base
}

// `ptr` already holds the element's address (genIndexAddr()). Reads its `words` 16-bit words,
// most significant first, pushing each onto the real stack (LSW on top), recomputing
// `ptr+2*i` fresh for each word via BC.
void I8080Codegen::genWideLoadAtPtr(int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    lhld ptr\n";
        if (i > 0) code_ << "    lxi b, " << (2 * i) << "\n    dad b\n";
        code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        code_ << "    push d\n";
    }
}

// The mirror image for a write: `wideScratch(role,i)` already holds word `i` of the value to
// store, reached one byte at a time via `lda` without needing a register pair that
// `ptr+2*i`'s address computation is also using.
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

// Entry convention: HL = old value, DE = rhs value, both already at `type`'s width/signedness.
// `+` is a single `dad d`. `-`/`&`/`|`/`^` reuse genBinary()'s byte-wise instruction shapes with
// HL/DE's roles swapped. `*`/`/`/`%`/`<<`/`>>` seed `bc`/`dx` from HL/DE and run genMul()'s/
// genDivMod()'s/genShift()'s loop bodies unchanged.
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

// The target address is computed once before loading `old`, then again fresh right before the
// final store; only `old` (and, for a postfix `++`/`--`, a second copy for the final result)
// needs the real stack to survive evaluating `rhs`.
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

// `ptr` must already hold the resolved address on entry. The address is shuttled through the
// real hardware stack across evaluating `rhs`, since `rhs` may recurse into codegen that reuses
// `ptr` (e.g. a nested compound assignment). After `rhs` is evaluated nothing here recurses
// further, so `cx` is safe as a transient `yieldOld` stash.
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

// A no-op unless `to` is a `char`.
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

// Stores arrayInit into `slot`'s frame slots; every call re-runs this since a Local's storage
// is a fresh frame region each activation, not pre-set like a Global's static trailer.
// arrayInit is always fully populated up to arraySize (zero-padded), leaving the whole array in
// a known state.
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
        // A wide (`long`/`long long`) element - 2 or 4 words, least significant first.
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

// Every binary op saves its left operand across evaluating the right one on the real 8080
// hardware stack (`push h`/`pop d`), never in a named cell like `bc` - the right operand's
// genExpr() might itself be another binary op (or genMul()/genDivMod()/genShift(), which share
// the same `bc`/`cx`/`dx` cells), which would clobber a named "other operand" cell.
void I8080Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every operand here shares one common type (usual arithmetic conversions), except shift
    // (only the LHS's type matters) and &&/|| (width-independent).
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
        code_ << "    lhld ac\n    push h\n";  // save lhs on the real stack across evaluating rhs
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

// No variable-count shift instruction - loop `rhs` times, one bit at a time, via shiftOneBit().
// `isUnsigned` only matters for `>>`, using the same `mov b,a`/`ral`/`mov a,b`/`rar` software-
// SRA idiom for the top byte as genWideShift().
void I8080Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the count
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

// Textbook shift-and-add multiply (no hardware multiply): `bc` is the multiplicand (doubled
// each iteration), `dx` the multiplier (halved each iteration), `ac` the accumulating product.
// Only verified against non-negative operands.
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

// Repeated subtraction (no hardware divide): `dx` is the running remainder (starts as the
// dividend), `bc` the fixed divisor, `ac` the quotient, counting how many times `bc` was
// subtracted out of `dx` before it would go negative. The core loop only handles non-negative
// operands, so a signed operand is divided via its absolute value: `ptr`'s low/high byte holds
// the dividend's/divisor's original sign while the unsigned core runs, then the quotient's sign
// (1 iff the two signs differed) and the remainder's sign (the dividend's) are restored after.
void I8080Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);                          // ac = dividend
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop h\n    shld dx\n";  // dx = dividend (restored only now)
    copy16("bc", "ac");                     // bc = divisor
    auto isNeg = [&](const char *cell, const char *dst) {
        code_ << "    lda " << cell << "+1\n    ani 128\n";
        std::string Lnonneg = newLabel(), Ldone = newLabel();
        code_ << "    jz " << Lnonneg << "\n";
        code_ << "    mvi a, 1\n    sta " << dst << "\n    jmp " << Ldone << "\n";
        code_ << Lnonneg << ":\n    mvi a, 0\n    sta " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negate16 = [&](const char *cell) {
        code_ << "    lhld " << cell << "\n";
        code_ << "    xra a\n    sub l\n    mov l,a\n";
        code_ << "    mvi a, 0\n    sbb h\n    mov h,a\n";
        code_ << "    shld " << cell << "\n";
    };
    auto negateIf = [&](const char *flagCell, const char *cell) {
        code_ << "    lda " << flagCell << "\n";
        code_ << "    ora a\n";  // LDA alone leaves flags unaffected on the 8080 - this sets Z from A's own value
        std::string Lskip = newLabel();
        code_ << "    jz " << Lskip << "\n";
        negate16(cell);
        code_ << Lskip << ":\n";
    };
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "bc");
    }
    code_ << "    lxi h, 0\n    shld ac\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    loadPair("d", "dx");
    code_ << "    lhld bc\n";
    // Tentative dx - bc; carry (borrow) set afterward means dx < bc, so this round's
    // subtraction can't happen - stop.
    code_ << "    mov a,e\n    sub l\n    mov c,a\n";  // c = tentative low result
    code_ << "    mov a,d\n    sbb h\n";
    code_ << "    jc " << Ldone << "\n";
    // Commit: dx -= bc.
    code_ << "    mov d,a\n    mov e,c\n";
    storePair("dx", "d");
    code_ << "    lhld ac\n    inx h\n    shld ac\n";  // quotient += 1
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed; the remainder always takes
        // the dividend's original sign.
        code_ << "    lda ptr\n    mov b,a\n    lda ptr+1\n    xra b\n";
        std::string Lskip = newLabel();
        code_ << "    jz " << Lskip << "\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
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

// `==`/`!=` need no sign handling: a-b (16 bits, borrow chained low into high byte) is zero
// iff a==b.
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

// Signed 16-bit "a < b" via the bias trick: flipping the top bit of each operand's high byte
// maps the signed range onto the unsigned one while preserving order, leaving a plain unsigned
// compare (the final borrow) to do.
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

// After genExpr(e) leaves `e`'s value - `ac` if narrow, or `wordCountOf(e.type)` words on the
// real stack (LSW on top) if wide - this sets Z exactly as a plain `ora h` would, with any wide
// stack space released, ready for an immediate `jz`/`jnz`.
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

std::optional<std::string> I8080Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleI8080(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral ObjectFile, then picks
    // that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI8080Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
