#include "M6809Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "M6809Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Load/entry address for a whole-program raw binary; must equal
// M6809Assembler.h's kM6809BaseAddress.
constexpr uint32_t kBaseAddress = 0x3000;
// 0x7F00: matches M6809LinkerFactory.cpp's own entry sequence - leaves the
// stack clear of a stock 64K map's ROM window (conventionally $8000-$FFFF).
constexpr uint32_t kStackTop = 0x7F00;

// True iff `e` is a promoted `unsigned char` operand - an IR
// ExprKind::Convert node (every arithmetic operand narrower than `int` is
// already wrapped in one by IRBuilder) whose own operand's pre-promotion
// type is `unsigned char`. Used by genMul() to pick the real hardware MUL
// fast path (see its own comment).
bool isUnsignedByteOperand(const ir::Expr &e) {
    return e.kind == ir::ExprKind::Convert && e.lhs && e.lhs->type.base == BaseType::Char &&
           e.lhs->type.isUnsigned && e.lhs->type.ptrDepth == 0;
}
}  // namespace

void M6809Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

std::string M6809Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void M6809Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                 Type elemType) {
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

void M6809Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ldd [" << src << "]\n    std [" << dst << "]\n";
}

void M6809Codegen::loadAddrX(const std::string &cell) { code_ << "    ldx [" << cell << "]\n"; }

// `leax <off>,u` - real indexed addressing reaches a Local/Param directly
// off the fixed frame pointer `u`, unlike Z80's push/pop-through-ix dance
// or the 8080's software frame pointer. A Local lives at
// u-(frameOffset+extraBytes) (frame grows down from u); a Param at
// u+frameOffset+extraBytes. A Local array's address is its element 0, its
// lowest, with `extraBytes` counting upward from there.
void M6809Codegen::loadFrameAddr(const ir::Slot *slot, long extraBytes) {
    long off = slot->kind == ir::SlotKind::Param
                   ? (static_cast<long>(slot->frameOffset) + extraBytes)
                   : slot->arraySize > 0
                         ? (-static_cast<long>(ir::localLowOffset(*slot, typeWidths())) + extraBytes)
                         : (-(static_cast<long>(ir::localOffsetDescendingWide(*slot, typeWidths())) + extraBytes));
    code_ << "    leax " << off << ",u\n";
}

// One bit of a logical shift on the 16-bit cell `lbl`, via D: ASLB sets
// carry from B's own top bit (no need to clear it first - ASL always
// shifts in a 0), then ROLA chains that carry up into A for a left shift;
// the mirror image (LSRA/RORB) does the same chaining down for a right
// shift, carry-in to A's top bit also always 0.
void M6809Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ldd [" << lbl << "]\n";
    if (isLeft) {
        code_ << "    aslb\n    rola\n";
    } else {
        code_ << "    lsra\n    rorb\n";
    }
    code_ << "    std [" << lbl << "]\n";
}

int M6809Codegen::widthOfType(Type t) const { return widthOf(t, typeWidths()); }
bool M6809Codegen::isWide(Type t) const { return widthOfType(t) > 2; }
int M6809Codegen::wordCountOf(Type t) const { return widthOfType(t) / 2; }

// X holds the operand's address. SEX sign-extends B (the loaded byte) into
// A in one instruction for a signed char; an unsigned char just zeroes A.
void M6809Codegen::loadOperand(Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    ldb 0,x\n";
        if (type.isUnsigned) {
            code_ << "    clra\n";
        } else {
            code_ << "    sex\n";
        }
        code_ << "    std [ac]\n";
        return;
    }
    code_ << "    ldd 0,x\n    std [ac]\n";
}

void M6809Codegen::storeOperand(Type type) {
    if (widthOfType(type) == 1) {
        // `ac`'s own cell is big-endian (see M6809Assembler.h), so the
        // actual value byte - the one a `char` store writes - is at ac+1,
        // not ac itself.
        code_ << "    ldb [ac+1]\n    stb 0,x\n";
        return;
    }
    code_ << "    ldd [ac]\n    std 0,x\n";
}

void M6809Codegen::popWideInto(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    puls a,b\n    std [" << wideScratch(role, i) << "]\n";
    }
}

void M6809Codegen::pushWideFrom(const char *role, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ldd [" << wideScratch(role, i) << "]\n    pshs a,b\n";
    }
}

void M6809Codegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ldd [" << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "]\n";
        } else {
            loadFrameAddr(slot, 2 * i);
            code_ << "    ldd 0,x\n";
        }
        code_ << "    pshs a,b\n";
    }
}

void M6809Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        uint16_t w = static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    ldd " << w << "\n    pshs a,b\n";
    }
}

void M6809Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    puls a,b\n";
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    std [" << label(slot) << (i ? ("+" + std::to_string(2 * i)) : "") << "]\n";
        } else {
            loadFrameAddr(slot, 2 * i);
            code_ << "    std 0,x\n";
        }
    }
}

// No 16-bit add/sub-with-carry exists on real 6809 hardware (ADDD/SUBD
// alone can't chain a carry into the next word): word 0 uses ADDD/SUBD
// directly (its own carry-out already reflects the bit-15/16 boundary),
// every later word's low byte then high byte continues that same chain via
// ADCB/SBCB then ADCA/SBCA (see M6809Assembler.cpp's own comment on why
// these were added).
void M6809Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    for (int i = 0; i < words; i++) {
        if (i == 0) {
            code_ << "    ldd [" << wideScratch("a", i) << "]\n";
            code_ << "    " << (isAdd ? "addd" : "subd") << " [" << wideScratch("b", i) << "]\n";
            code_ << "    std [" << wideScratch("a", i) << "]\n";
        } else {
            code_ << "    ldb [" << wideScratch("a", i) << "+1]\n";
            code_ << "    " << (isAdd ? "adcb" : "sbcb") << " [" << wideScratch("b", i) << "+1]\n";
            code_ << "    stb [" << wideScratch("a", i) << "+1]\n";
            code_ << "    lda [" << wideScratch("a", i) << "]\n";
            code_ << "    " << (isAdd ? "adca" : "sbca") << " [" << wideScratch("b", i) << "]\n";
            code_ << "    sta [" << wideScratch("a", i) << "]\n";
        }
    }
    pushWideFrom("a", words);
}

// Two's-complement negate: complement every byte (COMA/COMB), then add 1
// into the least significant word and let the same ADCB/ADCA carry chain
// genWideAdd() uses propagate it up through the rest.
void M6809Codegen::genWideNegate(int words) {
    popWideInto("a", words);
    for (int i = 0; i < words; i++) {
        code_ << "    lda [" << wideScratch("a", i) << "]\n    coma\n    sta [" << wideScratch("a", i) << "]\n";
        code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    comb\n    stb [" << wideScratch("a", i) << "+1]\n";
    }
    for (int i = 0; i < words; i++) {
        if (i == 0) {
            code_ << "    ldd [" << wideScratch("a", i) << "]\n    addd 1\n    std [" << wideScratch("a", i)
                  << "]\n";
        } else {
            code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    adcb 0\n    stb [" << wideScratch("a", i)
                  << "+1]\n";
            code_ << "    lda [" << wideScratch("a", i) << "]\n    adca 0\n    sta [" << wideScratch("a", i)
                  << "]\n";
        }
    }
    pushWideFrom("a", words);
}

// No 16-bit AND/OR/XOR exists either - every word is combined byte by byte
// via the A/B-only forms.
void M6809Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    const char *ainsn = op == "&" ? "anda" : op == "|" ? "ora" : "eora";
    const char *binsn = op == "&" ? "andb" : op == "|" ? "orb" : "eorb";
    for (int i = 0; i < words; i++) {
        code_ << "    lda [" << wideScratch("a", i) << "]\n    " << ainsn << " [" << wideScratch("b", i)
              << "]\n    sta [" << wideScratch("a", i) << "]\n";
        code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    " << binsn << " [" << wideScratch("b", i)
              << "+1]\n    stb [" << wideScratch("a", i) << "+1]\n";
    }
    pushWideFrom("a", words);
}

void M6809Codegen::genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words) {
    genExpr(lhs);
    genExpr(rhs);
    popWideInto("b", words);
    popWideInto("a", words);
    code_ << "    ldd 0\n    std [wacc]\n";
    for (int i = 0; i < words; i++) {
        code_ << "    lda [" << wideScratch("a", i) << "]\n    eora [" << wideScratch("b", i) << "]\n";
        code_ << "    ora [wacc]\n    sta [wacc]\n";
        code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    eorb [" << wideScratch("b", i) << "+1]\n";
        code_ << "    orb [wacc+1]\n    stb [wacc+1]\n";
    }
    code_ << "    ldd [wacc]\n";  // LDD sets Z from the whole 16-bit result: Z=1 iff every word matched
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "lbeq" : "lbne") << " " << Ltrue << "\n";
    code_ << "    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

// Compared most-significant word first, using real N/V/C flags directly -
// no sign-bias trick needed, unlike Z80/8080 (see M6809Codegen.h). Only the
// top word's compare needs the signed/unsigned distinction; once two words
// differ every lower word is a pure unsigned magnitude.
void M6809Codegen::genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned,
                                       int words) {
    genExpr(a);
    genExpr(b);
    popWideInto("b", words);
    popWideInto("a", words);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ldd [" << wideScratch("a", i) << "]\n";
        code_ << "    subd [" << wideScratch("b", i) << "]\n";
        bool topWord = (i == words - 1);
        const char *ltMnem = (topWord && !isUnsigned) ? (invert ? "lbge" : "lblt") : (invert ? "lbhs" : "lblo");
        if (i == 0) {
            code_ << "    " << ltMnem << " " << Ltrue << "\n";
            code_ << "    lbra " << Lfalse << "\n";
        } else {
            std::string Lcontinue = newLabel();
            code_ << "    lbeq " << Lcontinue << "\n";
            code_ << "    " << ltMnem << " " << Ltrue << "\n";
            code_ << "    lbra " << Lfalse << "\n";
            code_ << Lcontinue << ":\n";
        }
    }
    code_ << Lfalse << ":\n    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

void M6809Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
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
// across every byte, most-significant byte first - no variable-count
// shift exists even for a single word on this chip (see genShift()). The
// byte list is built most-significant-byte-first (word i's high byte, then
// its low byte, for i descending) to match this cell layout's big-endian
// byte order within each word.
void M6809Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);
    popWideInto("a", words);
    genExpr(rhs);
    code_ << "    ldb [ac+1]\n    stb [cx+1]\n";
    std::vector<std::string> bytesMsbFirst;
    for (int i = words - 1; i >= 0; i--) {
        bytesMsbFirst.push_back(wideScratch("a", i));
        bytesMsbFirst.push_back(wideScratch("a", i) + "+1");
    }
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldb [cx+1]\n";
    code_ << "    lbeq " << Ldone << "\n";
    if (isLeft) {
        for (size_t idx = bytesMsbFirst.size(); idx-- > 0;) {
            bool first = idx == bytesMsbFirst.size() - 1;
            code_ << "    ldb [" << bytesMsbFirst[idx] << "]\n    " << (first ? "aslb" : "rolb") << "\n    stb ["
                  << bytesMsbFirst[idx] << "]\n";
        }
    } else {
        for (size_t idx = 0; idx < bytesMsbFirst.size(); idx++) {
            const char *insn = idx == 0 ? (isUnsigned ? "lsrb" : "asrb") : "rorb";
            code_ << "    ldb [" << bytesMsbFirst[idx] << "]\n    " << insn << "\n    stb [" << bytesMsbFirst[idx]
                  << "]\n";
        }
    }
    code_ << "    ldb [cx+1]\n    decb\n    stb [cx+1]\n";
    code_ << "    lbra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    pushWideFrom("a", words);
}

// A long/long long multiply/divide needs a bit-serial multi-word routine,
// and this target has no simulator to verify a hand-written one against -
// failing loudly here is safer than shipping an unverifiable one (same
// rationale as Z80Codegen.cpp's/I8080Codegen.h's own genWideMul()).
void M6809Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                  "see M6809Codegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void M6809Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                  "see M6809Codegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

// Every arithmetic operand narrower than int is already promoted by
// IRBuilder, so the real work is crossing the native-word/wide boundary
// and re-narrowing a char result.
void M6809Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);

    // D = 0xFFFF or 0x0000 depending on `highByteAddr`'s own bit 7 (the
    // overall sign bit of the word it's the high byte of) - used to
    // materialize a signed extension word.
    auto signExtendWord = [&](const std::string &highByteAddr) {
        std::string Lskip = newLabel();
        code_ << "    ldb [" << highByteAddr << "]\n    andb 128\n";
        code_ << "    ldd 0\n";
        code_ << "    lbeq " << Lskip << "\n";
        code_ << "    ldd 65535\n";
        code_ << Lskip << ":\n";
    };

    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // ac
        if (widthOfType(to) == 1) {
            code_ << "    ldb [ac+1]\n";
            code_ << (to.isUnsigned ? "    clra\n" : "    sex\n");
            code_ << "    std [ac]\n";
        }
        return;
    }
    if (fromWide && !toWide) {
        int words = wordCountOf(from);
        genExpr(*e.lhs);
        popWideInto("a", words);
        code_ << "    ldd [" << wideScratch("a", 0) << "]\n    std [ac]\n";
        if (widthOfType(to) == 1) {
            code_ << "    ldb [ac+1]\n";
            code_ << (to.isUnsigned ? "    clra\n" : "    sex\n");
            code_ << "    std [ac]\n";
        }
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac already holds the native-width value, correctly sign/zero-extended to 16 bits
        code_ << "    ldd [ac]\n    std [" << wideScratch("a", 0) << "]\n";
        for (int i = 1; i < toWords; i++) {
            if (from.isUnsigned) {
                code_ << "    ldd 0\n";
            } else {
                signExtendWord("ac");
            }
            code_ << "    std [" << wideScratch("a", i) << "]\n";
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
        for (int i = fromWords; i < toWords; i++) {
            if (from.isUnsigned) {
                code_ << "    ldd 0\n";
            } else {
                signExtendWord(wideScratch("a", fromWords - 1));
            }
            code_ << "    std [" << wideScratch("a", i) << "]\n";
        }
    }
    pushWideFrom("a", toWords);
}

std::string M6809Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation means `prog` may not define `main` at all.
    // In `objectMode`, no object file emits this entry sequence, even one
    // that defines `main` - M6809LinkerFactory.cpp synthesizes exactly one
    // once every object is merged. An object file's own base is `org 0`;
    // placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lds " << kStackTop << "\n";
        code_ << "    jsr [" << label(mainFn) << "]\n";
        // The program's result sits in `ac` once execution reaches `halt` -
        // this extra load also leaves it in D for a quick register-dump look.
        code_ << "    ldd [ac]\n";
        code_ << "halt:\n    jmp [halt]\n\n";
    }

    // The fixed pseudo-registers are shared across the whole linked
    // program, not redeclared per-object: in whole-program mode they're
    // declared once, here; in object mode this file never declares them,
    // so every reference becomes a relocation resolved against
    // M6809LinkerFactory.cpp's synthesized entry object.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("_bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("a", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("b", i));
        for (int i = 0; i < 4; i++) declareTemp(wideScratch("ret", i));
        declareTemp("wacc");
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else if (slot->type.base == BaseType::Struct) {
            declareArray(lbl, std::vector<ir::ArrayInitElem>(slot->type.structSize), Type{BaseType::Char, false});
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 1) {
                trailer_.push_back(lbl + ": db " + std::to_string(static_cast<int>(static_cast<uint8_t>(v))) +
                                    "  ; " + slot->sourceName);
            } else if (w == 2) {
                uint16_t v16 = static_cast<uint16_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(v16) + "  ; " + slot->sourceName);
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

// Every call gets its own real frame, built around `u`. On entry S points
// at the return address JSR just pushed; `pshs u` saves the caller's frame
// pointer, `tfr s,u` sets `u` to that S - unlike Z80's `ix` (no direct
// move to/from HL, so it goes through the real stack) this is one plain
// register transfer. From `u`: `u+0` the saved caller u, `u+2` the return
// address, `u+4+2*i` argument i (pushed by the caller in reverse order, so
// argument 0 is closest to the return address). `frameReserveBytes_` bytes
// of locals are then carved out from `s` in one `leas -n,s` - no dec-sp
// loop needed, unlike Z80. Every Return undoes this with a matching
// `leas n,s` before restoring `u` and returning.
void M6809Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pshs u\n";
    code_ << "    tfr s,u\n";
    // See Z80Codegen.cpp's identical comment: `frameSize` is rounded up to
    // the native int width as a whole, padding by `intUnits-1` to always
    // cover a Local whose own frameOffset falls short of its deepest byte.
    frameReserveBytes_ = fn.func->frameSize + (typeWidths().intUnits - 1);
    if (frameReserveBytes_ > 0) code_ << "    leas -" << frameReserveBytes_ << ",s\n";

    genStmt(*fn.body);

    if (frameReserveBytes_ > 0) code_ << "    leas " << frameReserveBytes_ << ",s\n";
    code_ << "    puls u\n";
    code_ << "    rts\n\n";
}

void M6809Codegen::genStmt(const ir::Stmt &s) {
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
            code_ << "    lbeq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    lbra " << Ldone << "\n";
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
            code_ << "    lbeq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    lbra " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    lbra " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    lbra " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) {
                        code_ << "    puls a,b\n    std [" << wideScratch("ret", i) << "]\n";
                    }
                }
            }
            if (frameReserveBytes_ > 0) code_ << "    leas " << frameReserveBytes_ << ",s\n";
            code_ << "    puls u\n";
            code_ << "    rts\n";
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
                code_ << "    pshs x\n";
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    puls x\n";
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
            code_ << "    lbra " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void M6809Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldd " << v16 << "\n    std [ac]\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    ldx " << label(e.slot) << "\n";  // X = the global's own address, as data
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
                    code_ << "    ldd [ptr]\n    pshs a,b\n";
                    genExpr(*e.rhs);
                    popWideInto("b", words);
                    code_ << "    puls a,b\n    std [ptr]\n";
                    genWideStoreAtPtr("b", words);
                    return;
                }
                code_ << "    ldd [ptr]\n    pshs a,b\n";
                genExpr(*e.rhs);
                code_ << "    puls a,b\n    std [ptr]\n";
                loadAddrX("ptr");
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
                code_ << "    ldx " << label(e.slot) << "\n";
                storeOperand(e.slot->type);
                return;
            }
            loadFrameAddr(e.slot);
            code_ << "    pshs x\n";
            genExpr(*e.rhs);
            code_ << "    puls x\n";
            storeOperand(e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = address
            } else if (e.slot->kind == ir::SlotKind::Global) {
                code_ << "    ldd " << label(e.slot) << "\n    std [ptr]\n";
            } else {
                loadFrameAddr(e.slot);
                code_ << "    stx [ptr]\n";
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
            code_ << "    lbeq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    lbra " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            if (isWide(e.type)) {
                int words = wordCountOf(e.type);
                code_ << "    ldd [ptr]\n    pshs a,b\n";
                genExpr(*e.rhs);
                popWideInto("b", words);
                code_ << "    puls a,b\n    std [ptr]\n";
                genWideStoreAtPtr("b", words);
                return;
            }
            code_ << "    ldd [ptr]\n    pshs a,b\n";
            genExpr(*e.rhs);
            code_ << "    puls a,b\n    std [ptr]\n";
            loadAddrX("ptr");
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
                code_ << "    ldd 0\n    subd [ac]\n    std [ac]\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                if (isWide(e.type)) {
                    genWideLoadAtPtr(wordCountOf(e.type));
                    return;
                }
                loadAddrX("ptr");
                loadOperand(e.type);
            } else if (e.op == "~") {
                if (wide) {
                    genExpr(*e.rhs);
                    int words = wordCountOf(e.rhs->type);
                    popWideInto("a", words);
                    for (int i = 0; i < words; i++) {
                        code_ << "    lda [" << wideScratch("a", i) << "]\n    coma\n    sta [" << wideScratch("a", i)
                              << "]\n";
                        code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    comb\n    stb ["
                              << wideScratch("a", i) << "+1]\n";
                    }
                    pushWideFrom("a", words);
                    return;
                }
                genExpr(*e.rhs);
                code_ << "    lda [ac]\n    coma\n    sta [ac]\n";
                code_ << "    ldb [ac+1]\n    comb\n    stb [ac+1]\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed right-to-left so argument 0 ends up
            // closest to the return address (see genFunction()). The callee
            // never cleans these up, so the caller pops them back off after
            // the call returns.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    totalWords += wordCountOf(e.args[i]->type);
                } else {
                    code_ << "    ldd [ac]\n    pshs a,b\n";
                    totalWords += 1;
                }
            }
            code_ << "    jsr [" << label(e.func) << "]\n";
            for (int i = 0; i < totalWords; i++) code_ << "    puls a,b\n";
            if (isWide(e.type)) {
                // The callee left its wide return value in "wret" - push it
                // back as this expression's result, MSW first so LSW ends up on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    ldd [" << wideScratch("ret", i) << "]\n    pshs a,b\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // JMP's indexed form transfers control directly to the computed
            // address (see M6809Assembler.h), so a real CALL is emulated the
            // same way Z80's "jp [hl]" trick does: push the return address,
            // then jump through the register holding the callee's address.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            loadAddrX("ac");  // X = callee's address
            code_ << "    ldd " << Lret << "\n    pshs a,b\n";
            code_ << "    jmp 0,x\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No dedicated EI/DI on this chip - ANDCC/ORCC mask/unmask IRQ
            // via the I flag directly (see M6809Assembler.h).
            code_ << "    " << (e.op == "ion" ? "andcc 0xEF" : "orcc 0x10") << "\n";
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

// Leaves slot[idxExpr]'s effective address in `ptr`. The scaling multiply
// (index * widthOfType(slot->type)) is a compile-time-constant-width
// shift-and-add, fully unrolled over the width's own set bits (see
// Z80Codegen.cpp's identical comment), so a non-power-of-two element width
// (e.g. 3, for a 3-byte struct) scales correctly, unlike a naive
// power-of-two doubling.
void M6809Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    code_ << "    ldd [ac]\n    std [_bc]\n";  // _bc = index (current doubling value)
    code_ << "    ldd 0\n    std [ac]\n";       // ac = running product, starts at 0
    for (int w = widthOfType(slot->type); w > 0; w >>= 1) {
        if (w & 1) {
            code_ << "    ldd [ac]\n    addd [_bc]\n    std [ac]\n";
        }
        if (w >> 1) {
            code_ << "    ldd [_bc]\n    addd [_bc]\n    std [_bc]\n";
        }
    }
    code_ << "    ldd [ac]\n    std [ptr]\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ldd [ptr]\n    addd " << label(slot) << "\n    std [ptr]\n";
        } else {
            loadFrameAddr(slot);  // X = array's element 0, its lowest address
            code_ << "    stx [cx]\n";
            code_ << "    ldd [ptr]\n    addd [cx]\n    std [ptr]\n";
        }
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldd [" << label(slot) << "]\n";  // D = the pointer variable's own stored value
    } else {
        loadFrameAddr(slot);  // X = address of the pointer-variable's own storage
        code_ << "    ldd 0,x\n";
    }
    code_ << "    addd [ptr]\n    std [ptr]\n";
}

// `ptr` already holds the element's address. Reads `words` 16-bit words,
// most significant first, pushing each (LSW ends up on top) - real indexed
// addressing lets every word be read with one `ldd <2*i>,x`, no per-word
// address arithmetic needed (unlike Z80's own version of this).
void M6809Codegen::genWideLoadAtPtr(int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    ldx [ptr]\n";
        code_ << "    ldd " << (2 * i) << ",x\n";
        code_ << "    pshs a,b\n";
    }
}

// Mirror image for a write; `role`'s scratch cells must already hold the
// words to store (see its Z80Codegen.cpp counterpart's identical contract).
void M6809Codegen::genWideStoreAtPtr(const char *role, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    ldx [ptr]\n";
        code_ << "    ldd [" << wideScratch(role, i) << "]\n";
        code_ << "    std " << (2 * i) << ",x\n";
    }
}

void M6809Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);  // ptr = element address
    if (isWide(e.slot->type)) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    loadAddrX("ptr");
    loadOperand(e.slot->type);
}

// Element stores go straight through X's indexed form - one `ldd`/`std` per
// element (or `ldb`/`stb` for a `char`), no inc-pointer dance like Z80's.
void M6809Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        long base = static_cast<long>(i) * w;
        if (w == 1 && !elem.isSymbol) {
            loadFrameAddr(slot, base);
            code_ << "    ldb " << static_cast<int>(static_cast<uint8_t>(elem.value)) << "\n    stb 0,x\n";
            continue;
        }
        if (elem.isSymbol || w <= 2) {
            loadFrameAddr(slot, base);
            code_ << "    ldd " << arrayElemText(elem) << "\n    std 0,x\n";
            continue;
        }
        auto uv = static_cast<unsigned long>(elem.value);
        int words = w / 2;
        for (int j = 0; j < words; j++) {
            loadFrameAddr(slot, base + 2 * j);
            uint16_t wv = static_cast<uint16_t>((uv >> (16 * j)) & 0xFFFF);
            code_ << "    ldd " << wv << "\n    std 0,x\n";
        }
    }
}

// Entry: `_bc` = old value, `ac` = rhs value (both already at `type`'s
// width/signedness). `+`/`-`/`&`/`|`/`^` end with the raw new value in
// `ac`; `*`/`/`/`%`/`<<`/`>>` reuse genMul()'s/genDivMod()'s/genShift()'s
// own loop shapes with `_bc`/`dx`/`cx+1` as the working cells.
void M6809Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") {
        code_ << "    ldd [_bc]\n    addd [ac]\n    std [ac]\n";
        return;
    }
    if (op == "-") {
        code_ << "    ldd [_bc]\n    subd [ac]\n    std [ac]\n";
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *ainsn = op == "&" ? "anda" : op == "|" ? "ora" : "eora";
        const char *binsn = op == "&" ? "andb" : op == "|" ? "orb" : "eorb";
        code_ << "    lda [_bc]\n    " << ainsn << " [ac]\n    sta [ac]\n";
        code_ << "    ldb [_bc+1]\n    " << binsn << " [ac+1]\n    stb [ac+1]\n";
        return;
    }
    if (op == "*") {
        code_ << "    ldd [ac]\n    std [dx]\n";  // dx = rhs (multiplier); _bc already = old (multiplicand)
        code_ << "    ldd 0\n    std [ac]\n";
        code_ << "    ldb 16\n    stb [cx+1]\n";
        std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    ldb [cx+1]\n    lbeq " << Ldone << "\n";
        code_ << "    ldb [dx+1]\n    andb 1\n    lbeq " << Lskipadd << "\n";
        code_ << "    ldd [ac]\n    addd [_bc]\n    std [ac]\n";
        code_ << Lskipadd << ":\n";
        shiftOneBit("_bc", /*isLeft=*/true);
        shiftOneBit("dx", /*isLeft=*/false);
        code_ << "    ldb [cx+1]\n    decb\n    stb [cx+1]\n";
        code_ << "    lbra " << Lloop << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        code_ << "    ldb [ac+1]\n    stb [cx+1]\n";  // cx+1 = shift count; _bc already = old (value)
        std::string Lloop = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    ldb [cx+1]\n    lbeq " << Ldone << "\n";
        if (op == "<<" || isUnsigned) {
            shiftOneBit("_bc", /*isLeft=*/op == "<<");
        } else {
            code_ << "    lda [_bc]\n    asra\n    sta [_bc]\n";
            code_ << "    ldb [_bc+1]\n    rorb\n    stb [_bc+1]\n";
        }
        code_ << "    ldb [cx+1]\n    decb\n    stb [cx+1]\n";
        code_ << "    lbra " << Lloop << "\n";
        code_ << Ldone << ":\n";
        copy16("ac", "_bc");
        return;
    }
    // "/" or "%"
    code_ << "    ldd [_bc]\n    std [dx]\n";  // dx = old (dividend)
    code_ << "    ldd [ac]\n    std [_bc]\n";  // _bc = rhs (divisor)
    code_ << "    ldd 0\n    std [ac]\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldd [dx]\n    subd [_bc]\n";
    code_ << "    lblo " << Ldone << "\n";
    code_ << "    std [dx]\n";
    code_ << "    ldd [ac]\n    addd 1\n    std [ac]\n";
    code_ << "    lbra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (op == "%") copy16("ac", "dx");
}

// `ptr` must already hold the resolved address. The address and the old
// value are both shuttled through the real stack across evaluating `rhs`
// (ptr/ac/_bc are shared cells, and rhs's own evaluation may recurse into
// something that reuses them, and genCompoundOp() itself may clobber `_bc`
// for `*`/`<<`/`>>`).
void M6809Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    loadAddrX("ptr");
    loadOperand(e.type);  // ac = old
    code_ << "    ldx [ptr]\n    pshs x\n";  // stash address
    code_ << "    ldd [ac]\n    pshs a,b\n";  // stash old (ac is about to be overwritten by rhs)
    genExpr(*e.rhs);                           // ac = rhs
    code_ << "    puls a,b\n    std [_bc]\n";  // _bc = old
    if (e.yieldOld) code_ << "    pshs a,b\n";  // re-stash old (D still holds it) for the final result
    genCompoundOp(e.op, e.type);                // ac = new (raw)
    narrowAcTo(e.type);
    if (e.yieldOld) {
        code_ << "    puls a,b\n    std [cx]\n";  // cx = old
        code_ << "    puls x\n";                    // X = address
        storeOperand(e.type);                        // stores ac(=new) through x
        code_ << "    ldd [cx]\n    std [ac]\n";    // ac = old (final result)
    } else {
        code_ << "    puls x\n";  // X = address
        storeOperand(e.type);      // ac already = new
    }
}

// No-op unless `to` is a `char`.
void M6809Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    code_ << "    ldb [ac+1]\n";
    code_ << (to.isUnsigned ? "    clra\n" : "    sex\n");
    code_ << "    std [ac]\n";
}

void M6809Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    ldd " << label(operand.slot) << "\n    std [ac]\n";
        } else {
            loadFrameAddr(operand.slot);
            code_ << "    stx [ac]\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ldd " << label(operand.func) << "\n    std [ac]\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Saves the left operand across evaluating the right one on the real
// stack (pshs a,b/puls a,b), never in a named cell: the right operand's own
// genExpr() might recurse into genMul()/genDivMod()/genShift(), which
// share _bc/cx/dx.
void M6809Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
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
        code_ << "    ldd [ac]\n    std [_bc]\n";
        genExpr(*e.rhs);
        code_ << "    ldd [_bc]\n";
        code_ << "    " << (op == "+" ? "addd" : "subd") << " [ac]\n";
        code_ << "    std [ac]\n";
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
        code_ << "    ldd [ac]\n    std [_bc]\n";
        genExpr(*e.rhs);
        const char *ainsn = op == "&" ? "anda" : op == "|" ? "ora" : "eora";
        const char *binsn = op == "&" ? "andb" : op == "|" ? "orb" : "eorb";
        code_ << "    lda [_bc]\n    " << ainsn << " [ac]\n    sta [ac]\n";
        code_ << "    ldb [_bc+1]\n    " << binsn << " [ac+1]\n    stb [ac+1]\n";
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
// sign bit (arithmetic ASRA) instead of 0 (logical LSRA).
void M6809Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    ldd [ac]\n    std [_bc]\n";  // _bc = value
    genExpr(rhs);
    code_ << "    ldb [ac+1]\n    stb [cx+1]\n";  // cx+1 = shift count (always small; low byte is enough)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldb [cx+1]\n    lbeq " << Ldone << "\n";
    if (isLeft || isUnsigned) {
        shiftOneBit("_bc", isLeft);
    } else {
        code_ << "    lda [_bc]\n    asra\n    sta [_bc]\n";
        code_ << "    ldb [_bc+1]\n    rorb\n    stb [_bc+1]\n";
    }
    code_ << "    ldb [cx+1]\n    decb\n    stb [cx+1]\n";
    code_ << "    lbra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "_bc");
}

// Real hardware MUL (8x8->16 unsigned) is used directly when both operands
// are statically `unsigned char` - the one case that's correctness-free to
// fast-path, since the chip itself computes the product, not a hand-written
// algorithm. Every other `*` (both signed-byte and any int-width operand)
// falls back to the same shift-and-add loop Z80/8080 use: a correct
// 16x16->16 multiply built from three partial 8x8 MULs plus carry handling
// is new, unverified logic, not a safe template port, and a signed-byte MUL
// needs its own post-hoc sign correction this pass deliberately skips.
void M6809Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isUnsignedByteOperand(lhs) && isUnsignedByteOperand(rhs)) {
        genExpr(lhs);
        code_ << "    ldb [ac+1]\n    stb [_bc+1]\n";  // _bc's low byte = lhs's own byte value
        genExpr(rhs);
        code_ << "    lda [_bc+1]\n    ldb [ac+1]\n    mul\n    std [ac]\n";
        return;
    }
    genExpr(lhs);                                // ac = multiplicand
    code_ << "    ldd [ac]\n    std [_bc]\n";    // saved across evaluating the multiplier
    genExpr(rhs);
    code_ << "    ldd [ac]\n    std [dx]\n";      // dx = multiplier
    code_ << "    ldd 0\n    std [ac]\n";
    code_ << "    ldb 16\n    stb [cx+1]\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldb [cx+1]\n    lbeq " << Ldone << "\n";
    code_ << "    ldb [dx+1]\n    andb 1\n    lbeq " << Lskipadd << "\n";
    code_ << "    ldd [ac]\n    addd [_bc]\n    std [ac]\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("_bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    ldb [cx+1]\n    decb\n    stb [cx+1]\n";
    code_ << "    lbra " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): dx is the running remainder,
// _bc the fixed divisor, ac the quotient. SUBD both tests and computes the
// subtracted result in one instruction (its carry = unsigned borrow,
// checked directly via LBLO - no sign-bias trick needed). The core loop
// only handles non-negative operands, so a signed operand is divided via
// its absolute value, with `ptr`'s high/low bytes holding the original
// signs while it runs; the quotient's sign is restored (negative iff the
// signs differed), and the remainder always takes the dividend's own sign.
void M6809Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);                                // ac = dividend
    code_ << "    ldd [ac]\n    std [dx]\n";    // saved across evaluating the divisor
    genExpr(rhs);
    code_ << "    ldd [ac]\n    std [_bc]\n";    // bc = divisor
    auto isNeg = [&](const char *cell, const char *dstByteAddr) {
        code_ << "    ldb [" << cell << "]\n    andb 128\n";  // ANDB sets Z from the masked result
        std::string Lnonneg = newLabel(), Ldone2 = newLabel();
        code_ << "    lbeq " << Lnonneg << "\n";
        code_ << "    ldb 1\n    stb [" << dstByteAddr << "]\n    lbra " << Ldone2 << "\n";
        code_ << Lnonneg << ":\n    ldb 0\n    stb [" << dstByteAddr << "]\n";
        code_ << Ldone2 << ":\n";
    };
    auto negate16 = [&](const char *cell) { code_ << "    ldd 0\n    subd [" << cell << "]\n    std [" << cell << "]\n"; };
    auto negateIf = [&](const char *flagByteAddr, const char *cell) {
        code_ << "    ldb [" << flagByteAddr << "]\n";  // LDB sets Z from the loaded value directly
        std::string Lskip = newLabel();
        code_ << "    lbeq " << Lskip << "\n";
        negate16(cell);
        code_ << Lskip << ":\n";
    };
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("_bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "_bc");
    }
    code_ << "    ldd 0\n    std [ac]\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldd [dx]\n    subd [_bc]\n";  // D = dx - _bc; C = borrow (dx < _bc)
    code_ << "    lblo " << Ldone << "\n";
    code_ << "    std [dx]\n";  // commit: dx -= _bc (D already holds the subtracted result)
    code_ << "    ldd [ac]\n    addd 1\n    std [ac]\n";
    code_ << "    lbra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed; the
        // remainder keeps the dividend's sign.
        code_ << "    ldb [ptr]\n    stb [cx]\n    ldb [ptr+1]\n    eorb [cx]\n";
        std::string Lskip = newLabel();
        code_ << "    lbeq " << Lskip << "\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void M6809Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false, isUnsigned);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false, isUnsigned);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true, isUnsigned);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true, isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` reduce to one CMPD: the Z flag directly means lhs == rhs.
void M6809Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ldd [ac]\n    std [_bc]\n";
    genExpr(rhs);
    code_ << "    ldd [_bc]\n    cmpd [ac]\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "lbeq" : "lbne") << " " << Ltrue << "\n";
    code_ << "    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

// Real N/V/C flags from one SUBD give the answer directly - no sign-bias
// trick needed (unlike Z80/8080; see M6809Codegen.h): LBLT/LBGE for signed,
// LBLO/LBHS for unsigned.
void M6809Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned) {
    genExpr(a);
    code_ << "    ldd [ac]\n    std [_bc]\n";
    genExpr(b);
    code_ << "    ldd [_bc]\n    subd [ac]\n";  // D = a - b
    const char *mnem = isUnsigned ? (invert ? "lbhs" : "lblo") : (invert ? "lbge" : "lblt");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << mnem << " " << Ltrue << "\n";
    code_ << "    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

void M6809Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    lbeq " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    lbeq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldd 0\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

void M6809Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    lbne " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    lbne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

void M6809Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    lbeq " << Ltrue << "\n";
    code_ << "    ldd 0\n    std [ac]\n    lbra " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldd 1\n    std [ac]\n";
    code_ << Ldone << ":\n";
}

// Leaves the real Z flag set from the value genExpr(e) just left behind,
// ready for an immediate `lbeq`/`lbne`. A non-wide value's own LDD already
// sets it; a wide value needs its words OR-ed together first.
void M6809Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    ldd [ac]\n";
        return;
    }
    int words = wordCountOf(e.type);
    popWideInto("a", words);
    code_ << "    ldd 0\n    std [wacc]\n";
    for (int i = 0; i < words; i++) {
        code_ << "    lda [" << wideScratch("a", i) << "]\n    ora [wacc]\n    sta [wacc]\n";
        code_ << "    ldb [" << wideScratch("a", i) << "+1]\n    orb [wacc+1]\n    stb [wacc+1]\n";
    }
    code_ << "    ldd [wacc]\n";
}

std::optional<std::string> M6809Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleM6809(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM6809Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
