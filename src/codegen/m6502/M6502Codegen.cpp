#include "M6502Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "M6502Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// $0600: conventional bare-6502 load address, above zero page and the
// hardware stack page $0100-$01FF.
constexpr uint32_t kBaseAddress = 0x0600;

// Frame storage bump allocator region, between the hardware stack page and
// kBaseAddress so it never collides with either.
constexpr uint32_t kFrameStackBase = 0x0200;
}  // namespace

// A literal element's decimal text, or a symbol element's label (a
// dispatch-table address-of-function/global). A symbol has no resolved
// numeric value at codegen time, so it's left as a label for the
// assembler's `dblo`/`dbhi` directives to split into bytes later.
std::string M6502Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// A symbol element is always native-int width (2 bytes); only a plain
// literal element can be `char`-width (1 byte) or wide (4/8 bytes). A wide
// literal has no dblo/dbhi counterpart, so it's split into plain bytes
// directly, least significant first.
void M6502Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                 Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    bool first = true;
    auto emit = [&](const std::string &text) {
        trailer_.push_back((first ? lbl + ": " : "") + text);
        first = false;
    };
    for (const auto &e : values) {
        if (w == 1) {
            emit("db " + std::to_string(static_cast<int>(static_cast<uint8_t>(e.value))));
        } else if (w == 2) {
            std::string text = arrayElemText(e);
            emit("dblo " + text);
            trailer_.push_back("dbhi " + text);
        } else {
            auto uv = static_cast<unsigned long>(e.value);
            for (int b = 0; b < w; b++) {
                emit("db " + std::to_string(static_cast<int>((uv >> (8 * b)) & 0xFF)));
            }
        }
    }
}

void M6502Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lda " << src << "\n    sta " << dst << "\n";
    code_ << "    lda " << src << "+1\n    sta " << dst << "+1\n";
}

namespace {
// `base+i` operand text (just `base` for byte 0).
std::string byteAt(const char *base, int i) { return i == 0 ? base : (std::string(base) + "+" + std::to_string(i)); }
}  // namespace

// Pushes `base`'s `nbytes` bytes ascending (byte 0 first), so the most
// significant byte ends up on top.
void M6502Codegen::pushWide(const char *base, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(base, i) << "\n    pha\n";
}

// Mirror of pushWide(): pops descending (most significant, on top, first).
void M6502Codegen::popWide(const char *base, int nbytes) {
    for (int i = nbytes - 1; i >= 0; i--) code_ << "    pla\n    sta " << byteAt(base, i) << "\n";
}

void M6502Codegen::copyWide(const char *dst, const char *src, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(src, i) << "\n    sta " << byteAt(dst, i) << "\n";
}

void M6502Codegen::negateWideBuffer(const char *buf, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(buf, i) << "\n    eori 255\n    sta "
                                            << byteAt(buf, i) << "\n";
    code_ << "    clc\n    lda " << byteAt(buf, 0) << "\n    adci 1\n    sta " << byteAt(buf, 0) << "\n";
    for (int i = 1; i < nbytes; i++) {
        code_ << "    lda " << byteAt(buf, i) << "\n    adci 0\n    sta " << byteAt(buf, i) << "\n";
    }
}

void M6502Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

// A Global's wide-storage word label (`label+N`, N omitted for 0); a
// Local/Param's wide words go through `ptr`/Y instead.
std::string M6502Codegen::wideWordLabel(const ir::Slot *slot, int byteOffset) const {
    std::string lbl = label(slot);
    return byteOffset == 0 ? lbl : (lbl + "+" + std::to_string(byteOffset));
}

void M6502Codegen::genWideLoad(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << wideWordLabel(slot, i) << "\n    sta " << byteAt("wac", i) << "\n";
        }
        return;
    }
    setPtrToFrameSlot(slot, 0);
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    ldaind ptr\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldai " << ((uv >> (8 * i)) & 0xFF) << "\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideStore(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << byteAt("wac", i) << "\n    sta " << wideWordLabel(slot, i) << "\n";
        }
        return;
    }
    setPtrToFrameSlot(slot, 0);
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    lda " << byteAt("wac", i) << "\n    staind ptr\n";
    }
}

void M6502Codegen::genWideLoadAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    ldaind ptr\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideStoreAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    lda " << byteAt("wac", i) << "\n    staind ptr\n";
    }
}

void M6502Codegen::loadOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(type));
        return;
    }
    if (w == 1) {
        code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        if (type.isUnsigned) {
            code_ << "    ldai 0\n    sta ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::storeOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideStoreAtPtr(wordCountOf(type));
        return;
    }
    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
    if (w == 2) code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
}

void M6502Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;  // a native word: ac already holds the full value
    if (to.isUnsigned) {
        code_ << "    ldai 0\n    sta ac+1\n";
        return;
    }
    code_ << "    lda ac\n    sta cx\n    asl cx\n";
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    ldai 0\n    sta ac+1\n";
    safeBranch("bcs", Lneg, Ldone);
    code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// `op` applied to ac=old, bc=rhs, leaving the raw new value in ac.
void M6502Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") {
        code_ << "    clc\n    lda ac\n    adc bc\n    sta ac\n";
        code_ << "    lda ac+1\n    adc bc+1\n    sta ac+1\n";
        return;
    }
    if (op == "-") {
        code_ << "    sec\n    lda ac\n    sbc bc\n    sta ac\n";
        code_ << "    lda ac+1\n    sbc bc+1\n    sta ac+1\n";
        return;
    }
    if (op == "*") {
        copy16("dx", "ac");  // dx = multiplier = old (bc already = rhs = multiplicand)
        genMulCore();
        return;
    }
    if (op == "/" || op == "%") {
        copy16("dx", "ac");  // dx = dividend = old (bc already = rhs = divisor)
        genDivModCore(op == "%", isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
        code_ << "    lda ac\n    " << insn << " bc\n    sta ac\n";
        code_ << "    lda ac+1\n    " << insn << " bc+1\n    sta ac+1\n";
        return;
    }
    // "<<" or ">>": bc's low byte is the shift count; bc is dead afterward.
    code_ << "    lda bc\n    sta cx\n";
    genShiftCore(op == "<<", isUnsigned);
}

// `ptr` must already hold the resolved address on entry. e.rhs's own
// evaluation may clobber ptr/ac/bc/cx/dx (nested calls, etc.), so `ptr` is
// stashed on the hardware stack across it.
void M6502Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";  // stash address
    genExpr(*e.rhs);                                            // ac = rhs
    copy16("bc", "ac");  // bc = rhs - untouched by the address restore/old-load below
    code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";  // ptr restored
    loadOperandAtPtr(e.type);                                   // ac = old
    if (e.yieldOld) code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";  // stash old - this expr's own result
    genCompoundOp(e.op, e.type);                                // ac = new (raw), using ac=old, bc=rhs
    narrowAcTo(e.type);
    storeOperandAtPtr(e.type);  // ptr is still valid - genCompoundOp()/narrowAcTo() never touch it
    if (e.yieldOld) code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";  // ac = old (final result)
}

void M6502Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    inc " << lbl << "\n";
    code_ << "    lda " << lbl << "\n";
    code_ << "    bne " << Lskip << "\n";
    code_ << "    inc " << lbl << "+1\n";
    code_ << Lskip << ":\n";
}

std::string M6502Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation means `prog` may not define `main` at all.
    // `objectMode` means no object file emits this entry sequence; the
    // linker synthesizes exactly one once every object is merged. An
    // object file's base is `org 0` - placement is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    // Fixed zero-page pseudo-registers, bound via compile-time equate (not
    // the usual data-trailer layout) so every object declares them
    // identically regardless of where it's linked, with no relocation
    // needed for the equate itself. `fp`/`sp`: the software frame pointer
    // and the frame-stack bump allocator. `wac`/`wbc`/`wtmp`/`wtmp2`: the
    // 8-byte wide-value buffers.
    code_ << "ptr = 0\nac = 2\nbc = 4\ncx = 6\ndx = 8\nfp = 10\nsp = 12\naBase = 14\n";
    code_ << "wac = 16\nwbc = 24\nwtmp = 32\nwtmp2 = 40\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // One-time init of the frame-stack bump allocator before the first
        // call; genFunction()'s prologue only reads/advances `sp` after this.
        code_ << "    ldai " << (kFrameStackBase & 0xFF) << "\n    sta sp\n";
        code_ << "    ldai " << ((kFrameStackBase >> 8) & 0xFF) << "\n    sta sp+1\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The program's final result sits in ac/ac+1 at `halt` below. This
        // extra `lda` also leaves the low byte in the real A register.
        code_ << "    lda ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; it's only
        // referenced by name, letting the assembler defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            auto uv = static_cast<unsigned long>(v);
            int w = widthOfType(slot->type);
            trailer_.push_back(lbl + ": db " + std::to_string(uv & 0xFF) + "  ; " + slot->sourceName);
            for (int i = 1; i < w; i++) trailer_.push_back("db " + std::to_string((uv >> (8 * i)) & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (no `--format=coff` for this target), so the whole
// generated text is attributed to whichever file defines `main`.
std::map<std::string, std::string> M6502Codegen::perFileAsm() const { return perFileAsm_; }

// `paramBytes` is the caller's already-reserved argument-area size;
// subtracting it off `sp` makes `fp` land on argument 0's cell. There's no
// immediate SBC on this target, so the subtraction is addition of the
// two's-complement, exact under mod-65536 wraparound.
void M6502Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunction_ = fn.func;
    code_ << label(fn.func) << ":\n";
    // Every param reserves at least a native int's width (2 bytes); a wider
    // type reserves its own width - the sum is the caller's argument-area size.
    int paramBytes = 0;
    for (const ir::Slot *p : fn.func->params) paramBytes += std::max(widthOfType(p->type), 2);
    int localBytes = fn.func->frameSize;
    // Save the caller's `fp` on the hardware stack; restored by genEpilogue().
    code_ << "    lda fp\n    pha\n    lda fp+1\n    pha\n";
    if (paramBytes > 0) {
        int negParamBytes = (0x10000 - paramBytes) & 0xFFFF;
        code_ << "    clc\n    lda sp\n    adci " << (negParamBytes & 0xFF) << "\n    sta fp\n";
        code_ << "    lda sp+1\n    adci " << ((negParamBytes >> 8) & 0xFF) << "\n    sta fp+1\n";
    } else {
        code_ << "    lda sp\n    sta fp\n    lda sp+1\n    sta fp+1\n";
    }
    if (localBytes > 0) {
        code_ << "    clc\n    lda sp\n    adci " << (localBytes & 0xFF) << "\n    sta sp\n";
        code_ << "    lda sp+1\n    adci " << ((localBytes >> 8) & 0xFF) << "\n    sta sp+1\n";
    }
    genStmt(*fn.body);
    genEpilogue();
    code_ << "\n";
}

void M6502Codegen::genEpilogue() {
    // sp = fp: Params and Locals collapse back to this call's frame base.
    code_ << "    lda fp\n    sta sp\n    lda fp+1\n    sta sp+1\n";
    code_ << "    pla\n    sta fp+1\n    pla\n    sta fp\n";  // restore caller's fp
    code_ << "    rts\n";
}

int M6502Codegen::frameByteOffset(const ir::Slot *slot) const {
    if (slot->kind == ir::SlotKind::Param) {
        // IRBuilder's frameOffset for a Param is counted past a fixed
        // two-native-int reservation; `fp` here lands on the first
        // argument instead, so that reservation is subtracted back off.
        return slot->frameOffset - 2 * widthOfType(Type{BaseType::Int, false});
    }
    // Locals start right after params, so the first local's frameOffset
    // (one native int's width) must land there, not one int further on.
    int paramBytes = 0;
    for (const ir::Slot *p : curFunction_->params) paramBytes += std::max(widthOfType(p->type), 2);
    return paramBytes + slot->frameOffset - widthOfType(Type{BaseType::Int, false});
}

void M6502Codegen::setPtrToFrameSlot(const ir::Slot *slot, int extraByteOffset) {
    int off = frameByteOffset(slot) + extraByteOffset;
    code_ << "    clc\n    lda fp\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
    code_ << "    lda fp+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
}

void M6502Codegen::loadSlotToAc(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    if (w == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda " << label(slot) << "\n    sta ac\n";
        } else {
            setPtrToFrameSlot(slot, 0);
            code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        }
        if (slot->type.isUnsigned) {
            code_ << "    ldai 0\n    sta ac+1\n";
            return;
        }
        // No "test bit 7" instruction in this dialect, so the sign bit is
        // extracted into the carry via ASL on a throwaway copy (cx).
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        copy16("ac", label(slot));
        return;
    }
    setPtrToFrameSlot(slot, 0);
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::storeAcToSlot(const ir::Slot *slot) {
    if (widthOfType(slot->type) == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda ac\n    sta " << label(slot) << "\n";
        } else {
            setPtrToFrameSlot(slot, 0);
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
        }
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        copy16(label(slot), "ac");
        return;
    }
    setPtrToFrameSlot(slot, 0);
    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
    code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
}

// Every call re-initializes the array, since there's no static trailer for
// Local storage; element i's cell is the array's frame cell plus i*2 bytes.
void M6502Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        setPtrToFrameSlot(slot, w * static_cast<int>(i));
        if (w <= 2) {
            std::string text = arrayElemText(elem);
            code_ << "    ldai " << text << "\n    sta ac\n";
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
            if (w == 2) {
                code_ << "    ldai_hi " << text << "\n    sta ac+1\n";
                code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
            }
        } else {
            // A wide element's literal is never a symbol - split directly
            // into `w` plain bytes, least significant first.
            auto uv = static_cast<unsigned long>(elem.value);
            for (int b = 0; b < w; b++) {
                code_ << "    ldai " << ((uv >> (8 * b)) & 0xFF) << "\n    sta ac\n";
                code_ << "    ldyi " << b << "\n    lda ac\n    staind ptr\n";
            }
        }
    }
}

void M6502Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testTruthy(*s.cond);
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("beq", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    jmp " << Ldone << "\n";
            if (s.elseStmt) {
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testTruthy(*s.cond);
            safeBranch("beq", Lend, Lbody);
            code_ << Lbody << ":\n";
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
            genEpilogue();  // early exit: genFunction()'s fall-through epilogue doesn't run here
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                if (isWide(s.declSlot->type)) {
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                } else {
                    storeAcToSlot(s.declSlot);
                }
            }
            // No "else" case: an uninitialized scalar's frame cell already
            // exists (reserved by genFunction()'s `sp` bump).
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

void M6502Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
            code_ << "    ldai " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            loadSlotToAc(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                int w = widthOfType(e.slot->type);
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                // Save `ptr` across evaluating rhs, which might index
                // through `ptr` again.
                code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
                genExpr(*e.rhs);
                code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
                if (w > 2) {
                    genWideStoreAtPtr(wordCountOf(e.slot->type));
                } else {
                    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                    if (w == 2) code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                }
                return;
            }
            genExpr(*e.rhs);
            if (isWide(e.slot->type)) {
                genWideStore(e.slot, wordCountOf(e.slot->type));
            } else {
                storeAcToSlot(e.slot);
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness: the pointee type of a
            // typed pointer, or a plain native word for untyped-int-as-address.
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
            genExpr(*e.rhs);
            code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
            storeOperandAtPtr(e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                genCompoundThroughPtr(e);
                return;
            }
            // Plain scalar target: its storage (fixed label or
            // fp+frameByteOffset()) is reconstructible after rhs evaluates,
            // since `fp` is restored by any nested call's epilogue.
            genExpr(*e.rhs);     // ac = rhs
            copy16("bc", "ac");  // bc = rhs
            loadSlotToAc(e.slot);  // ac = old
            if (e.yieldOld) code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
            genCompoundOp(e.op, e.type);
            narrowAcTo(e.type);
            storeAcToSlot(e.slot);
            if (e.yieldOld) code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);     // ac = pointer value
            copy16("ptr", "ac");  // ptr = address
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lthen = newLabel();
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            safeBranch("beq", Lelse, Lthen);
            code_ << Lthen << ":\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            bool wide = isWide(e.rhs->type);
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (wide) {
                    negateWideBuffer("wac", 2 * wordCountOf(e.rhs->type));
                } else {
                    code_ << "    ldai 0\n    sec\n    sbc ac\n    sta ac\n";
                    code_ << "    ldai 0\n    sbc ac+1\n    sta ac+1\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness (see DerefAssign above).
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                loadOperandAtPtr(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (wide) {
                    int n = 2 * wordCountOf(e.rhs->type);
                    for (int i = 0; i < n; i++) {
                        std::string b = i == 0 ? "wac" : ("wac+" + std::to_string(i));
                        code_ << "    lda " << b << "\n    eori 255\n    sta " << b << "\n";
                    }
                } else {
                    code_ << "    lda ac\n    eori 255\n    sta ac\n";
                    code_ << "    lda ac+1\n    eori 255\n    sta ac+1\n";
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Every argument is evaluated and parked on the hardware stack
            // first, before any `sp`/`aBase` use, so a nested call in a
            // later argument can't invalidate an earlier one's computed
            // address. Each reserves at least a native int's width (2
            // bytes); a wider type reserves its own width - mirroring
            // genFunction()'s `paramBytes` on the callee side.
            std::vector<int> argWidths(e.args.size());
            int totalBytes = 0;
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                int w = std::max(widthOfType(e.args[i]->type), 2);
                argWidths[i] = w;
                totalBytes += w;
                if (w > 2) {
                    pushWide("wac", w);
                } else {
                    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
                }
            }
            if (!e.args.empty()) {
                // aBase = sp (this call's argument area, becoming the
                // callee's `fp` once its prologue subtracts this size off).
                code_ << "    lda sp\n    sta aBase\n    lda sp+1\n    sta aBase+1\n";
                code_ << "    clc\n    lda sp\n    adci " << (totalBytes & 0xFF) << "\n    sta sp\n";
                code_ << "    lda sp+1\n    adci " << ((totalBytes >> 8) & 0xFF) << "\n    sta sp+1\n";
                std::vector<int> offsets(e.args.size());
                int running = 0;
                for (size_t i = 0; i < e.args.size(); i++) {
                    offsets[i] = running;
                    running += argWidths[i];
                }
                // Unpark LIFO (last-pushed argument comes off first) into
                // its own cell, aBase+offsets[i].
                for (size_t i = e.args.size(); i-- > 0;) {
                    int w = argWidths[i];
                    if (w > 2) {
                        popWide("wac", w);
                    } else {
                        code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";
                    }
                    int off = offsets[i];
                    code_ << "    clc\n    lda aBase\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
                    code_ << "    lda aBase+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
                    if (w > 2) {
                        genWideStoreAtPtr(w / 2);
                    } else {
                        code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                        code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                    }
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            // A wide return value is wherever the callee's `return`
            // expression left it (`wac`, untouched by genEpilogue()).
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty: there's no per-callee p_<fn>_<i> cell
            // to write into until the callee is known.
            //
            // The 6502 has no indirect-call instruction, only indirect JMP
            // ("jmpi", opcode $6C). A call through a computed address is
            // built by hand from JSR's return-address convention: JSR
            // pushes (return address - 1), RTS adds 1 back after popping.
            // Pushing (Lret-1) the same way, then jumping indirectly
            // through `ptr`, leaves the stack exactly as a real `jsr`
            // would, so `rts` returns to right after "jmpi ptr".
            genExpr(*e.lhs);  // ac = the callee's own entry address
            copy16("ptr", "ac");
            std::string Lret = newLabel();
            code_ << "    ldai_hi " << Lret << "-1\n    pha\n";
            code_ << "    ldai " << Lret << "-1\n    pha\n";
            code_ << "    jmpi ptr\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // SEI sets the interrupt-disable flag, CLI clears it.
            code_ << "    " << (e.op == "ion" ? "cli" : "sei") << "\n";
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

// `slot->arraySize > 0` means the base is a compile-time-known label
// (an address, split into low/high bytes via `ldai`/`ldai_hi`);
// otherwise `slot` is a pointer variable, so the base comes from
// reading its stored value (an absolute-memory operand, not an immediate).
void M6502Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    int shift = elemShift(slot->type);  // ac *= widthOf(slot->type), a power of two
    for (int i = 0; i < shift; i++) code_ << "    asl ac\n    rol ac+1\n";
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        code_ << "    clc\n";
        if (slot->arraySize > 0) {
            code_ << "    lda ac\n    adci " << lbl << "\n    sta ptr\n";
            code_ << "    lda ac+1\n    adci_hi " << lbl << "\n    sta ptr+1\n";
        } else {
            code_ << "    lda ac\n    adc " << lbl << "\n    sta ptr\n";
            code_ << "    lda ac+1\n    adc " << lbl << "+1\n    sta ptr+1\n";
        }
        return;
    }
    // Local/Param: set `ptr` to fp+frameByteOffset() first; `ac` already
    // holds the scaled index.
    setPtrToFrameSlot(slot, 0);
    if (slot->arraySize > 0) {
        // `ptr` IS the array's first word - base-plus-scaled-index.
        code_ << "    clc\n    lda ac\n    adc ptr\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc ptr+1\n    sta ptr+1\n";
    } else {
        // slot is a pointer variable stored at that cell - read its value
        // (through `ptr`, still the cell's address) and add the index to that.
        code_ << "    ldyi 0\n    ldaind ptr\n    sta bc\n";
        code_ << "    ldyi 1\n    ldaind ptr\n    sta bc+1\n";
        code_ << "    clc\n    lda ac\n    adc bc\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc bc+1\n    sta ptr+1\n";
    }
}

void M6502Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    int w = widthOfType(e.slot->type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    if (w == 1) {
        code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        if (e.slot->type.isUnsigned) {
            code_ << "    ldai 0\n    sta ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            std::string lbl = label(operand.slot);
            code_ << "    ldai " << lbl << "\n    sta ac\n";
            code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        } else {
            setPtrToFrameSlot(operand.slot, 0);
            copy16("ac", "ptr");
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        std::string lbl = label(operand.func);
        code_ << "    ldai " << lbl << "\n    sta ac\n";
        code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// An implicit or explicit type conversion (see IR.h's own ExprKind::
// Every arithmetic/comparison operand narrower than `int` is already
// promoted by IRBuilder, so the real work here is narrowing/widening a
// `char` and crossing the native-word/wide boundary (padding `wac` with
// sign/zero-extension bytes, or leaving the low bytes alone when shrinking).
void M6502Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // ac
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldai 0\n    sta ac+1\n";
            } else {
                code_ << "    lda ac\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta ac+1\n";
                safeBranch("bcs", Lneg, Ldone);
                code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
                code_ << Ldone << ":\n";
            }
        }
        return;
    }
    if (fromWide && !toWide) {
        genExpr(*e.lhs);  // wac holds `wordCountOf(from)` words
        code_ << "    lda " << byteAt("wac", 0) << "\n    sta ac\n";
        code_ << "    lda " << byteAt("wac", 1) << "\n    sta ac+1\n";
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldai 0\n    sta ac+1\n";
            } else {
                code_ << "    lda ac\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta ac+1\n";
                safeBranch("bcs", Lneg, Ldone);
                code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
                code_ << Ldone << ":\n";
            }
        }
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac, already correctly sign/zero-extended to 16 bits
        code_ << "    lda ac\n    sta " << byteAt("wac", 0) << "\n";
        code_ << "    lda ac+1\n    sta " << byteAt("wac", 1) << "\n";
        if (toWords > 1) {
            // Sign/zero-extend per the source's signedness. A pointer value
            // is always zero-extended: `from.isUnsigned` would otherwise
            // describe the pointee's signedness, not the pointer's own.
            if (isPointer(from) || from.isUnsigned) {
                code_ << "    ldai 0\n    sta dx\n";
            } else {
                code_ << "    lda ac+1\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta dx\n";
                safeBranch("bcc", Ldone, Lneg);
                code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
                code_ << Ldone << ":\n";
            }
            for (int i = 2; i < 2 * toWords; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
        }
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromN = 2 * wordCountOf(from);
    int toN = 2 * wordCountOf(to);
    genExpr(*e.lhs);  // wac holds `fromN` bytes
    if (toN > fromN) {
        if (from.isUnsigned) {
            code_ << "    ldai 0\n    sta dx\n";
        } else {
            code_ << "    lda " << byteAt("wac", fromN - 1) << "\n    sta cx\n    asl cx\n";
            std::string Lneg = newLabel(), Ldone = newLabel();
            code_ << "    ldai 0\n    sta dx\n";
            safeBranch("bcc", Ldone, Lneg);
            code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
            code_ << Ldone << ":\n";
        }
        for (int i = fromN; i < toN; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
    }
    // toN < fromN: the low `toN` bytes already in `wac` are the truncated value.
}

// Pushes lhs across evaluating rhs, leaving wbc=lhs, wac=rhs, then combines
// byte by byte with a single carry chain (CLC/SEC once; ADC/SBC fold in the
// previous byte's carry automatically) into `wac`.
void M6502Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    code_ << "    " << (isAdd ? "clc" : "sec") << "\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    " << (isAdd ? "adc" : "sbc") << " " << byteAt("wac", i)
              << "\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    " << insn << " " << byteAt("wac", i) << "\n    sta "
              << byteAt("wac", i) << "\n";
    }
}

// XORs every byte pair together (into `dx`, ORed across bytes), zero iff equal.
void M6502Codegen::emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    code_ << "    lda " << byteAt("wbc", 0) << "\n    eor " << byteAt("wac", 0) << "\n    sta dx\n";
    for (int i = 1; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    eor " << byteAt("wac", i) << "\n    ora dx\n    sta dx\n";
    }
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    lda dx\n";
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// One chained SBC from least to most significant byte computes the full
// subtraction in one pass; the final byte's flags alone decide the
// comparison, with the same N-xor-V overflow fixup as emitLessCompare().
void M6502Codegen::emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words,
                                        bool invert) {
    int n = 2 * words;
    genExpr(a);
    pushWide("wac", n);
    genExpr(b);
    popWide("wbc", n);  // wbc = a, wac = b
    code_ << "    sec\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    sbc " << byteAt("wac", i) << "\n";
    }
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);  // carry alone already means unsigned "<"
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n    eori 128\n" << Lfixskip << ":\n";
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                   int words) {
    if (op == "==") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/true);
    if (op == "!=") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/false);
    if (op == "<") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/false);
    if (op == ">") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/false);
    if (op == "<=") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/true);
    if (op == ">=") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Loops one bit at a time, chaining carry across all `n` bytes via ASL/ROL
// (left) or LSR/ROR (right, highest byte first so carry flows downward). A
// signed right shift re-derives the sign bit into carry before the top
// byte's ROR instead of LSR's fixed 0-fill (using `dx` as scratch, since
// `cx` holds the loop count and every byte of `wac` is live).
void M6502Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);  // ac = shift count (already promoted to a plain int)
    code_ << "    lda ac\n    sta cx\n";
    popWide("wac", n);
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl " << byteAt("wac", 0) << "\n";
        for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wac", i) << "\n";
    } else if (isUnsigned) {
        code_ << "    lsr " << byteAt("wac", n - 1) << "\n";
        for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    } else {
        code_ << "    lda " << byteAt("wac", n - 1) << "\n    sta dx\n    asl dx\n";
        code_ << "    ror " << byteAt("wac", n - 1) << "\n";
        for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Shift-and-add multiply, `n*8` bits serially: `wbc` is the multiplicand
// (shifted left each iteration, truncated mod 2^(8n)), `wac` the multiplier
// (shifted right, consumed one bit at a time), `wtmp` the product. Needs no
// signedness: a truncated product's bit pattern is the same either way.
void M6502Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = multiplicand, wac = multiplier
    for (int i = 0; i < n; i++) code_ << "    ldai 0\n    sta " << byteAt("wtmp", i) << "\n";
    code_ << "    ldai " << (8 * n) << "\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Ladd = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    code_ << "    lda " << byteAt("wac", 0) << "\n    andi 1\n";
    safeBranch("bne", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    clc\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    adc " << byteAt("wbc", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    code_ << Lskipadd << ":\n";
    code_ << "    asl " << byteAt("wbc", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wbc", i) << "\n";
    code_ << "    lsr " << byteAt("wac", n - 1) << "\n";
    for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copyWide("wac", "wtmp", n);
}

// Binary long division, `n*8` bits serially: `wbc` and `wtmp` (running
// remainder) act as one combined `2n`-byte shift register - each iteration
// shifts the pair left by one bit, then tries `wtmp -= wac` (the divisor):
// no borrow means quotient bit 1 and the subtraction stands; otherwise bit
// 0 and `wac` is added back. Repeated subtraction (the narrow genDivMod()
// algorithm) would need up to 2^(8n) iterations, infeasible at 4/8 bytes.
//
// Signed division wraps this unsigned core: negate either operand's
// magnitude if it started negative (flags in `dx`/`dx+1`), then fix the
// quotient's sign (negative iff signs differed) and the remainder's
// (always the dividend's original sign) afterward.
void M6502Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                                  int words) {
    int n = 2 * words;
    auto isNegByte = [&](const char *lblByte, const char *dst) {
        code_ << "    lda " << lblByte << "\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta " << dst << "\n";
        safeBranch("bcc", Ldone, Lneg);
        code_ << Lneg << ":\n    ldai 1\n    sta " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negateIfFlag = [&](const char *flag, const char *buf) {
        std::string Lskip = newLabel(), Ldo = newLabel();
        code_ << "    lda " << flag << "\n";
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negateWideBuffer(buf, n);
        code_ << Lskip << ":\n";
    };
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = dividend, wac = divisor
    if (!isUnsigned) {
        isNegByte(byteAt("wbc", n - 1).c_str(), "dx");
        isNegByte(byteAt("wac", n - 1).c_str(), "dx+1");
        negateIfFlag("dx", "wbc");
        negateIfFlag("dx+1", "wac");
    }
    for (int i = 0; i < n; i++) code_ << "    ldai 0\n    sta " << byteAt("wtmp", i) << "\n    sta "
                                       << byteAt("wtmp2", i) << "\n";
    code_ << "    ldai " << (8 * n) << "\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Lrestore = newLabel(), Lsetbit = newLabel(), Lshiftq = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    // Combined 1-bit left shift of (wtmp:wbc): wbc's outgoing top bit
    // becomes wtmp's new bottom bit via the carry chain.
    code_ << "    asl " << byteAt("wbc", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wbc", i) << "\n";
    for (int i = 0; i < n; i++) code_ << "    rol " << byteAt("wtmp", i) << "\n";
    // Trial subtraction: wtmp -= wac, always committed.
    code_ << "    sec\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    sbc " << byteAt("wac", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    safeBranch("bcs", Lsetbit, Lrestore);
    code_ << Lrestore << ":\n";
    code_ << "    clc\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    adc " << byteAt("wac", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    code_ << "    clc\n    jmp " << Lshiftq << "\n";
    code_ << Lsetbit << ":\n    sec\n";
    code_ << Lshiftq << ":\n";
    for (int i = 0; i < n; i++) code_ << "    rol " << byteAt("wtmp2", i) << "\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    lda dx\n    eor dx+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negateWideBuffer("wtmp2", n);
        code_ << Lskip << ":\n";
        negateIfFlag("dx", "wtmp");
    }
    copyWide("wac", wantRemainder ? "wtmp" : "wtmp2", n);
}

void M6502Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand shares one common type
    // (IRBuilder's usual-arithmetic-conversion pass), except shift (only
    // LHS's type matters) and &&/|| (width-independent).
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
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
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    clc\n    lda bc\n    adc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    adc ac+1\n    sta ac+1\n";
        } else {
            code_ << "    sec\n    lda bc\n    sbc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    sbc ac+1\n    sta ac+1\n";
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
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned, words);
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
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
        const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
        code_ << "    lda bc\n    " << insn << " ac\n    sta ac\n";
        code_ << "    lda bc+1\n    " << insn << " ac+1\n    sta ac+1\n";
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
    if (wide) {
        genWideCompare(op, *e.lhs, *e.rhs, isUnsigned, words);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs, isUnsigned);
}

// No variable-count shift instruction (ASL/LSR shift exactly one bit), so
// this loops `rhs` times instead.
void M6502Codegen::genShiftCore(bool isLeft, bool isUnsigned) {
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl ac\n    rol ac+1\n";
    } else if (isUnsigned) {
        // Logical (unsigned) shift: fills with 0 either way.
        code_ << "    lsr ac+1\n    ror ac\n";
    } else {
        // Arithmetic (signed) shift: re-derive the sign bit (ac+1 bit 7)
        // into carry via a throwaway copy (bc+1, since ASL needs a memory
        // operand and would destroy ac+1), then rotate it back in at the top.
        code_ << "    lda ac+1\n    sta bc+1\n    asl bc+1\n";
        code_ << "    ror ac+1\n    ror ac\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    lda ac\n    sta cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
    copy16("ac", "bc");  // ac = value to shift; bc is dead from here on (free as scratch)
    genShiftCore(isLeft, isUnsigned);
}

// Shift-and-add multiply: `bc` the multiplicand (shifted left each
// iteration), `dx` the multiplier (shifted right, consumed one bit at a
// time), `ac` the product. Only verified against non-negative operands.
void M6502Codegen::genMulCore() {
    code_ << "    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";  // ac = product = 0
    code_ << "    ldai 16\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Ladd = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    code_ << "    lda dx\n    andi 1\n";
    safeBranch("bne", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    clc\n    lda ac\n    adc bc\n    sta ac\n";
    code_ << "    lda ac+1\n    adc bc+1\n    sta ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    asl bc\n    rol bc+1\n";
    code_ << "    lsr dx+1\n    ror dx\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = multiplicand
    genMulCore();
}

// Repeated subtraction: `dx` is the running remainder (starts as the
// dividend), `bc` the divisor, `ac` the quotient, counting how many times
// `bc` was subtracted before going negative. Only verified against
// non-negative operands.
void M6502Codegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    auto negate16 = [&](const char *lbl) {
        code_ << "    ldai 0\n    sec\n    sbc " << lbl << "\n    sta " << lbl << "\n";
        code_ << "    ldai 0\n    sbc " << lbl << "+1\n    sta " << lbl << "+1\n";
    };
    // Leaves 0/1 in `dst` per `lbl`'s top (sign) bit.
    auto isNeg = [&](const char *lbl, const char *dst) {
        code_ << "    lda " << lbl << "+1\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta " << dst << "\n";
        safeBranch("bcc", Ldone, Lneg);
        code_ << Lneg << ":\n    ldai 1\n    sta " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negateIf = [&](const char *flag, const char *lbl) {
        std::string Lskip = newLabel(), Ldo = newLabel();
        code_ << "    lda " << flag << "\n";
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16(lbl);
        code_ << Lskip << ":\n";
    };
    // `cx`'s low/high byte double as the dividend/divisor sign flags, but
    // isNeg() also uses `cx` as scratch, so the dividend's flag is stashed
    // in `aBase` and restored before either flag is read below.
    if (!isUnsigned) {
        isNeg("dx", "cx");
        code_ << "    lda cx\n    sta aBase\n";  // stash the dividend's own sign flag (aBase: free here)
        isNeg("bc", "cx+1");
        code_ << "    lda aBase\n    sta cx\n";  // restore the dividend's own sign flag over cx's own low byte
        negateIf("cx", "dx");
        negateIf("cx+1", "bc");
    }
    code_ << "    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentatively compute dx - bc; carry set means no borrow (dx >= bc).
    // The low byte's result is stashed on the stack until it's known
    // whether to commit.
    code_ << "    sec\n    lda dx\n    sbc bc\n    pha\n";
    code_ << "    lda dx+1\n    sbc bc+1\n";
    safeBranch("bcs", Lcommit, Llt);
    code_ << Lcommit << ":\n";
    code_ << "    sta dx+1\n    pla\n    sta dx\n";
    inc16("ac");
    code_ << "    jmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    pla\n";  // discard the stashed low byte - subtraction rejected
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed; the
        // remainder always takes the dividend's original sign.
        code_ << "    lda cx\n    eor cx+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("cx", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void M6502Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pla\n    sta dx+1\n    pla\n    sta dx\n";  // dx = dividend (running remainder)
    genDivModCore(wantRemainder, isUnsigned);
}

void M6502Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to "is a < b?" by choosing operand
    // order and whether to invert: a>b is b<a; a<=b is !(b<a); a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling: a-b (16 bits, borrow chained low into
// high byte) is zero iff a==b; `dx` holds the low byte while A is needed
// for the high byte's subtraction.
void M6502Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
    code_ << "    sec\n    lda bc\n    sbc ac\n    sta dx\n";
    code_ << "    lda bc+1\n    sbc ac+1\n    ora dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction: after the final (high) byte SBC,
// N reflects the true signed result only once corrected for overflow (V) -
// if V is set, `EOR #$80` flips the sign bit to fix it (the 6502's N-xor-V
// rule, with no single instruction to test it directly).
void M6502Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert) {
    genExpr(a);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(b);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = a
    code_ << "    sec\n    lda bc\n    sbc ac\n    lda bc+1\n    sbc ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        // No fixup needed: carry after the subtraction directly means
        // "no borrow" (a>=b) or "borrow" (a<b).
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n";
        code_ << "    eori 128\n";
        code_ << Lfixskip << ":\n";
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);  // N now correctly reflects a<b (BMI) / a>=b (BPL)
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// Leaves `ac` holding 0 (false) or nonzero (true), with Z set correctly for
// an immediately-following beq/bne. For a wide value, ORs all `wac` bytes
// into `ac` first (true iff any byte is nonzero).
void M6502Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    lda ac\n    ora ac+1\n";
        return;
    }
    int n = 2 * wordCountOf(e.type);
    code_ << "    lda " << byteAt("wac", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    ora " << byteAt("wac", i) << "\n";
    code_ << "    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << "    lda ac\n";
}

void M6502Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("beq", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("bne", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("beq", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> M6502Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleM6502(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral
    // ObjectFile, then picks that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM6502Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
