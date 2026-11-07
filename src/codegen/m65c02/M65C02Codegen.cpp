#include "M65C02Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "M65C02Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// $0600: the conventional "put your test program here" address for a bare
// 6502 setup, above zero page and the hardware stack page $0100-$01FF.
constexpr uint32_t kBaseAddress = 0x0600;

// Initial value of the software call-frame stack pointer `sp`; high enough
// to stay clear of code+data growing up from kBaseAddress.
constexpr uint32_t kFrameStackTop = 0xF000;
}  // namespace

// A literal element's decimal text, or a symbol element's label - the
// address isn't known at codegen time, so it's left for the assembler to
// resolve via dblo/dbhi.
std::string M65C02Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void M65C02Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
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
            for (int b = 0; b < w; b++) emit("db " + std::to_string(static_cast<int>((uv >> (8 * b)) & 0xFF)));
        }
    }
}

void M65C02Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lda " << src << "\n    sta " << dst << "\n";
    code_ << "    lda " << src << "+1\n    sta " << dst << "+1\n";
}

void M65C02Codegen::zeroZp(const std::string &lbl) {
    code_ << "    stzp " << lbl << "\n    stzp " << lbl << "+1\n";
}

namespace {
std::string byteAt(const char *base, int i) { return i == 0 ? base : (std::string(base) + "+" + std::to_string(i)); }
}  // namespace

void M65C02Codegen::pushWide(const char *base, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(base, i) << "\n    pha\n";
}

void M65C02Codegen::popWide(const char *base, int nbytes) {
    for (int i = nbytes - 1; i >= 0; i--) code_ << "    pla\n    sta " << byteAt(base, i) << "\n";
}

void M65C02Codegen::copyWide(const char *dst, const char *src, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(src, i) << "\n    sta " << byteAt(dst, i) << "\n";
}

void M65C02Codegen::negateWideBuffer(const char *buf, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(buf, i) << "\n    eori 255\n    sta "
                                            << byteAt(buf, i) << "\n";
    code_ << "    clc\n    lda " << byteAt(buf, 0) << "\n    adci 1\n    sta " << byteAt(buf, 0) << "\n";
    for (int i = 1; i < nbytes; i++) {
        code_ << "    lda " << byteAt(buf, i) << "\n    adci 0\n    sta " << byteAt(buf, i) << "\n";
    }
}

void M65C02Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

void M65C02Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    inc " << lbl << "\n";
    code_ << "    lda " << lbl << "\n";
    code_ << "    bne " << Lskip << "\n";
    code_ << "    inc " << lbl << "+1\n";
    code_ << Lskip << ":\n";
}

void M65C02Codegen::addConst16(const std::string &dstReg, const std::string &srcReg, int disp) {
    if (disp == 0) {
        copy16(dstReg, srcReg);
        return;
    }
    uint16_t k = static_cast<uint16_t>(disp);
    code_ << "    lda " << srcReg << "\n    clc\n    adci " << k << "\n    sta " << dstReg << "\n";
    code_ << "    lda " << srcReg << "+1\n    adci_hi " << k << "\n    sta " << dstReg << "+1\n";
}

void M65C02Codegen::add16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    clc\n    lda " << a << "\n    adc " << b << "\n    sta " << dst << "\n";
    code_ << "    lda " << a << "+1\n    adc " << b << "+1\n    sta " << dst << "+1\n";
}

void M65C02Codegen::sub16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    sec\n    lda " << a << "\n    sbc " << b << "\n    sta " << dst << "\n";
    code_ << "    lda " << a << "+1\n    sbc " << b << "+1\n    sta " << dst << "+1\n";
}

void M65C02Codegen::loadIndirect(const std::string &zpPtr, const std::string &dstReg) {
    code_ << "    ldyi 0\n    ldaind " << zpPtr << "\n    sta " << dstReg << "\n";
    code_ << "    ldyi 1\n    ldaind " << zpPtr << "\n    sta " << dstReg << "+1\n";
}

void M65C02Codegen::storeIndirect(const std::string &zpPtr, const std::string &srcReg) {
    code_ << "    ldyi 0\n    lda " << srcReg << "\n    staind " << zpPtr << "\n";
    code_ << "    ldyi 1\n    lda " << srcReg << "+1\n    staind " << zpPtr << "\n";
}

void M65C02Codegen::slotAddrToPtr(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Param) {
        // Slot::frameOffset assumes a two-int-width reservation above fp,
        // but this frame reserves only one word there (the saved fp), so
        // the extra reservation is subtracted back off here.
        addConst16("ptr", "fp", slot->frameOffset - 2 * widthOfType(Type{BaseType::Int, false}) + 2);
    } else {
        addConst16("ptr", "fp", -slot->frameOffset);
    }
}

void M65C02Codegen::loadSlotToAc(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    if (w == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda " << label(slot) << "\n    sta ac\n";
        } else {
            slotAddrToPtr(slot);
            code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        }
        if (slot->type.isUnsigned) {
            code_ << "    stzp ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    stzp ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        copy16("ac", label(slot));
        return;
    }
    slotAddrToPtr(slot);
    loadIndirect("ptr", "ac");
}

void M65C02Codegen::storeAcToSlot(const ir::Slot *slot) {
    if (widthOfType(slot->type) == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda ac\n    sta " << label(slot) << "\n";
        } else {
            slotAddrToPtr(slot);
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
        }
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        copy16(label(slot), "ac");
        return;
    }
    slotAddrToPtr(slot);
    storeIndirect("ptr", "ac");
}

void M65C02Codegen::genWideLoad(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << lbl << (i ? "+" + std::to_string(i) : "") << "\n    sta " << byteAt("wac", i)
                  << "\n";
        }
        return;
    }
    slotAddrToPtr(slot);
    genWideLoadAtPtr(words);
}

void M65C02Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    int n = 2 * words;
    for (int i = 0; i < n; i++) code_ << "    ldai " << ((uv >> (8 * i)) & 0xFF) << "\n    sta " << byteAt("wac", i)
                                       << "\n";
}

void M65C02Codegen::genWideStore(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << byteAt("wac", i) << "\n    sta " << lbl << (i ? "+" + std::to_string(i) : "")
                  << "\n";
        }
        return;
    }
    slotAddrToPtr(slot);
    genWideStoreAtPtr(words);
}

void M65C02Codegen::genWideLoadAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) code_ << "    ldyi " << i << "\n    ldaind ptr\n    sta " << byteAt("wac", i)
                                       << "\n";
}

void M65C02Codegen::genWideStoreAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) code_ << "    ldyi " << i << "\n    lda " << byteAt("wac", i)
                                       << "\n    staind ptr\n";
}

void M65C02Codegen::loadOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(type));
        return;
    }
    if (w == 1) {
        code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        if (type.isUnsigned) {
            code_ << "    stzp ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    stzp ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M65C02Codegen::storeOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideStoreAtPtr(wordCountOf(type));
        return;
    }
    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
    if (w == 2) code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
}

void M65C02Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;  // a native word: ac already holds the full value
    if (to.isUnsigned) {
        code_ << "    stzp ac+1\n";
        return;
    }
    code_ << "    lda ac\n    sta cx\n    asl cx\n";
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    stzp ac+1\n";
    safeBranch("bcs", Lneg, Ldone);
    code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// `op` applied to ac=old, bc=rhs, leaving the raw new value in ac.
void M65C02Codegen::genCompoundOp(const std::string &op, Type type) {
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
        copy16("dx", "ac");  // dx = multiplier = old
        genMulCore();
        return;
    }
    if (op == "/" || op == "%") {
        copy16("dx", "ac");  // dx = dividend = old
        genDivModCore(op == "%", isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
        code_ << "    lda ac\n    " << insn << " bc\n    sta ac\n";
        code_ << "    lda ac+1\n    " << insn << " bc+1\n    sta ac+1\n";
        return;
    }
    code_ << "    lda bc\n    sta cx\n";  // bc's low byte is the shift count; bc is dead afterward
    genShiftCore(op == "<<", isUnsigned);
}

// `ptr` must already hold the resolved address on entry. `e.rhs`'s
// evaluation can clobber ptr/ac/bc/cx/dx, so the address is stashed on the
// hardware stack across it, and again around genCompoundOp() since its
// "/"/"%" case also uses ptr as scratch.
void M65C02Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
    genExpr(*e.rhs);      // ac = rhs
    copy16("bc", "ac");   // bc = rhs
    code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
    loadOperandAtPtr(e.type);  // ac = old
    if (e.yieldOld) code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
    genCompoundOp(e.op, e.type);  // ac = new (raw)
    narrowAcTo(e.type);
    code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
    storeOperandAtPtr(e.type);
    if (e.yieldOld) code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";  // ac = old (final result)
}

void M65C02Codegen::loadArrayElemToAc(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        uint16_t v16 = static_cast<uint16_t>(e.value);
        code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
        code_ << "    ldai_hi " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
        return;
    }
    std::string lbl = e.slot ? label(e.slot) : label(e.func);
    code_ << "    ldai " << lbl << "\n    sta ac\n";
    code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
}

void M65C02Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        addConst16("ptr", "fp", -(slot->frameOffset + w * static_cast<int>(i)));
        if (w <= 2) {
            loadArrayElemToAc(elem);
            if (w == 1) {
                code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
            } else {
                storeIndirect("ptr", "ac");
            }
        } else {
            // A wide element's literal is never a symbol; split directly
            // into w plain bytes, least significant first.
            auto uv = static_cast<unsigned long>(elem.value);
            for (int b = 0; b < w; b++) {
                code_ << "    ldai " << ((uv >> (8 * b)) & 0xFF) << "\n    sta ac\n";
                code_ << "    ldyi " << b << "\n    lda ac\n    staind ptr\n";
            }
        }
    }
}

void M65C02Codegen::emitEpilogue() {
    loadIndirect("fp", "ptr");  // ptr = caller's saved fp
    addConst16("sp", "fp", 2);  // sp = just above this frame's saved-fp word
    copy16("fp", "ptr");        // fp = caller's frame, restored
}

std::string M65C02Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation means `prog` may not define `main` at all;
    // objectMode means the entry sequence isn't emitted here at all -
    // M65C02Linker.cpp synthesizes it once every object is merged and
    // main's linked address is known. An object file's own base is org 0;
    // placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    // The fixed zero-page pseudo-registers, bound as equates rather than a
    // sequential data-trailer layout. Safe to declare identically in every
    // separately-compiled object since an equate's value is a fixed
    // constant, not a linker-visible address.
    code_ << "ptr = 0\nac = 2\nbc = 4\ncx = 6\ndx = 8\nsp = 10\nfp = 12\n";
    code_ << "wac = 14\nwbc = 22\nwtmp = 30\nwtmp2 = 38\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // Seeds the software call-frame stack before main's prologue runs.
        // fp only needs zeroing for tidiness: main never returns to a
        // caller that reads its saved value back.
        code_ << "    ldai " << kFrameStackTop << "\n    sta sp\n";
        code_ << "    ldai_hi " << kFrameStackTop << "\n    sta sp+1\n";
        zeroZp("fp");
        code_ << "    jsr " << label(mainFn) << "\n";
        // The program's result sits in ac/ac+1; this lda also leaves the
        // low byte in the real A register for a quick register-dump look.
        code_ << "    lda ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file: skip it here and
        // let the assembler defer the reference as a relocation.
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

std::map<std::string, std::string> M65C02Codegen::perFileAsm() const { return perFileAsm_; }

// Prologue: pushes the caller's fp onto the software stack right where the
// caller's pushed arguments end, sets fp to that saved word, then reserves
// frameSize*2 more bytes below fp for locals.
void M65C02Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    addConst16("ptr", "sp", -2);     // ptr = saved-fp word's address
    storeIndirect("ptr", "fp");      // [ptr] = caller's fp
    copy16("fp", "ptr");             // fp = this call's frame base
    addConst16("sp", "fp", -fn.func->frameSize);  // reserve the locals below fp
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "    rts\n\n";
}

void M65C02Codegen::genStmt(const ir::Stmt &s) {
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
            // Early exit: genFunction()'s fall-through epilogue never runs
            // for this path, so it's repeated inline here.
            emitEpilogue();
            code_ << "    rts\n";
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

void M65C02Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            // Each byte is handled independently since the two halves of a
            // 16-bit literal aren't usually zero together.
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            if ((v16 & 0xFF) == 0) {
                code_ << "    stzp ac\n";
            } else {
                code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
            }
            if (((v16 >> 8) & 0xFF) == 0) {
                code_ << "    stzp ac+1\n";
            } else {
                code_ << "    ldai " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
            }
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
                // Stash ptr on the hardware stack since rhs might index
                // through ptr again.
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
            // A plain scalar target's storage is reconstructible after rhs
            // evaluates (fp is restored by any nested call's epilogue
            // before it returns), so nothing needs stashing across rhs here.
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
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                loadOperandAtPtr(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (wide) {
                    int n = 2 * wordCountOf(e.rhs->type);
                    for (int i = 0; i < n; i++) {
                        std::string b = byteAt("wac", i);
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
            // Pushes each argument in reverse declaration order so
            // argument 0 ends up closest to the callee's frame (fp+2), then
            // cleans the pushed words back off itself (cdecl-style).
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                int w = std::max(widthOfType(e.args[i]->type), 2);
                totalBytes += w;
                addConst16("sp", "sp", -w);
                if (w > 2) {
                    for (int b = 0; b < w; b++) code_ << "    ldyi " << b << "\n    lda " << byteAt("wac", b)
                                                         << "\n    staind sp\n";
                } else {
                    storeIndirect("sp", "ac");
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            if (totalBytes > 0) addConst16("sp", "sp", totalBytes);
            return;
        }
        case ir::ExprKind::CallPtr: {
            // The 6502 has no indirect-call instruction, only jmp (addr)
            // (jmpi). This emulates JSR's return-address convention by hand
            // (JSR pushes return-address-1, high then low byte; RTS pulls
            // low then high and adds 1 back), then jumps indirectly through
            // ptr, leaving the hardware stack exactly as a real jsr would
            // have, so the callee's rts returns right after "jmpi ptr".
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

// slot->arraySize > 0 means the array's storage is indexed directly: a
// Global's is base+idx*2 off a compile-time label, a Local's is
// fp-relative at base-idx*2 (a Param array always decays to a pointer, so
// can't reach this branch). Otherwise slot is a plain pointer variable:
// the base comes from reading its stored value, and idx*2 is always ADDED
// to it regardless of where the pointer variable lives.
void M65C02Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    int shift = elemShift(slot->type);  // ac *= widthOf(slot->type), a power of two
    for (int i = 0; i < shift; i++) code_ << "    asl ac\n    rol ac+1\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            std::string lbl = label(slot);
            code_ << "    clc\n";
            code_ << "    lda ac\n    adci " << lbl << "\n    sta ptr\n";
            code_ << "    lda ac+1\n    adci_hi " << lbl << "\n    sta ptr+1\n";
        } else {
            addConst16("ptr", "fp", -slot->frameOffset);  // ptr = element 0's own address
            sub16("ptr", "ptr", "ac");                         // ptr -= idx*2
        }
    } else if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        code_ << "    clc\n";
        code_ << "    lda ac\n    adc " << lbl << "\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc " << lbl << "+1\n    sta ptr+1\n";
    } else {
        slotAddrToPtr(slot);        // ptr = slot's own frame address
        loadIndirect("ptr", "bc");  // bc = slot's own stored pointer value
        add16("ptr", "ac", "bc");   // ptr = idx*2 + bc
    }
}

void M65C02Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    int w = widthOfType(e.slot->type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    if (w == 1) {
        code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        if (e.slot->type.isUnsigned) {
            code_ << "    stzp ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    stzp ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M65C02Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            std::string lbl = label(operand.slot);
            code_ << "    ldai " << lbl << "\n    sta ac\n";
            code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        } else {
            slotAddrToPtr(operand.slot);
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

void M65C02Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    auto narrowCharFixup = [&]() {
        if (widthOfType(to) != 1) return;
        if (to.isUnsigned) {
            code_ << "    stzp ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    stzp ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
    };
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);
        narrowCharFixup();
        return;
    }
    if (fromWide && !toWide) {
        genExpr(*e.lhs);
        code_ << "    lda " << byteAt("wac", 0) << "\n    sta ac\n";
        code_ << "    lda " << byteAt("wac", 1) << "\n    sta ac+1\n";
        narrowCharFixup();
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);
        code_ << "    lda ac\n    sta " << byteAt("wac", 0) << "\n";
        code_ << "    lda ac+1\n    sta " << byteAt("wac", 1) << "\n";
        if (toWords > 1) {
            // from.isUnsigned describes the pointee's signedness for a
            // pointer type, not the pointer's own (which has no sign), so
            // isPointer(from) is checked separately to force zero-extension.
            if (isPointer(from) || from.isUnsigned) {
                code_ << "    stzp dx\n";
            } else {
                code_ << "    lda ac+1\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    stzp dx\n";
                safeBranch("bcc", Ldone, Lneg);
                code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
                code_ << Ldone << ":\n";
            }
            for (int i = 2; i < 2 * toWords; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
        }
        return;
    }
    int fromN = 2 * wordCountOf(from);
    int toN = 2 * wordCountOf(to);
    genExpr(*e.lhs);
    if (toN > fromN) {
        if (from.isUnsigned) {
            code_ << "    stzp dx\n";
        } else {
            code_ << "    lda " << byteAt("wac", fromN - 1) << "\n    sta cx\n    asl cx\n";
            std::string Lneg = newLabel(), Ldone = newLabel();
            code_ << "    stzp dx\n";
            safeBranch("bcc", Ldone, Lneg);
            code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
            code_ << Ldone << ":\n";
        }
        for (int i = fromN; i < toN; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M65C02Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
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

void M65C02Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
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

void M65C02Codegen::emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual) {
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
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words,
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
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n    eori 128\n" << Lfixskip << ":\n";
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                    int words) {
    if (op == "==") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/true);
    if (op == "!=") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/false);
    if (op == "<") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/false);
    if (op == ">") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/false);
    if (op == "<=") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/true);
    if (op == ">=") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void M65C02Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
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

void M65C02Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words) {
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

void M65C02Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                                   int words) {
    int n = 2 * words;
    auto isNegByte = [&](const std::string &lblByte, const char *dst) {
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
        isNegByte(byteAt("wbc", n - 1), "dx");
        isNegByte(byteAt("wac", n - 1), "dx+1");
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
    code_ << "    asl " << byteAt("wbc", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wbc", i) << "\n";
    for (int i = 0; i < n; i++) code_ << "    rol " << byteAt("wtmp", i) << "\n";
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

void M65C02Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    lda ac\n    ora ac+1\n";
        return;
    }
    int n = 2 * wordCountOf(e.type);
    code_ << "    lda " << byteAt("wac", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    ora " << byteAt("wac", i) << "\n";
    code_ << "    sta ac\n    stzp ac+1\n";
    code_ << "    lda ac\n";
}

void M65C02Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
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

// ASL/LSR shift exactly one bit, so this loops `rhs` times.
void M65C02Codegen::genShiftCore(bool isLeft, bool isUnsigned) {
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl ac\n    rol ac+1\n";
    } else if (isUnsigned) {
        code_ << "    lsr ac+1\n    ror ac\n";
    } else {
        // Re-derives the current sign bit into carry each iteration (via a
        // throwaway copy in bc+1) and rotates it back in at the top,
        // instead of LSR's fixed 0-fill.
        code_ << "    lda ac+1\n    sta bc+1\n    asl bc+1\n";
        code_ << "    ror ac+1\n    ror ac\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    lda ac\n    sta cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
    copy16("ac", "bc");  // ac = value to shift; bc is dead from here on (free as scratch)
    genShiftCore(isLeft, isUnsigned);
}

// Shift-and-add multiply: bc is the multiplicand (shifted left each
// iteration), dx the multiplier (shifted right, consumed one bit at a
// time), ac the accumulating product.
void M65C02Codegen::genMulCore() {
    zeroZp("ac");  // ac = product = 0
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

void M65C02Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = multiplicand
    genMulCore();
}

// Repeated subtraction: dx is the running remainder (starts as the
// dividend), bc the fixed divisor, ac the quotient, counting how many
// times bc was subtracted out of dx before it would've gone negative.
void M65C02Codegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    auto negate16 = [&](const char *lbl) {
        code_ << "    ldai 0\n    sec\n    sbc " << lbl << "\n    sta " << lbl << "\n";
        code_ << "    ldai 0\n    sbc " << lbl << "+1\n    sta " << lbl << "+1\n";
    };
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
    // ptr's low/high byte double as this operation's two sign flags.
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "bc");
    }
    zeroZp("ac");  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentatively computes dx - bc; carry set means dx >= bc. The low
    // byte's result is stashed on the stack until it's known whether to
    // commit it.
    code_ << "    sec\n    lda dx\n    sbc bc\n    pha\n";
    code_ << "    lda dx+1\n    sbc bc+1\n";
    safeBranch("bcs", Lcommit, Llt);
    code_ << Lcommit << ":\n";
    code_ << "    sta dx+1\n    pla\n    sta dx\n";
    inc16("ac");
    code_ << "    jmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    pla\n";  // discard the rejected tentative low byte
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    lda ptr\n    eor ptr+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void M65C02Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pla\n    sta dx+1\n    pla\n    sta dx\n";  // dx = dividend (running remainder)
    genDivModCore(wantRemainder, isUnsigned);
}

void M65C02Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to "is a < b?": a>b is b<a, a<=b
    // is !(b<a), a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// a-b (full 16 bits, borrow chained from low into high byte) is zero iff
// a==b; needs no sign handling.
void M65C02Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
    code_ << "    sec\n    lda bc\n    sbc ac\n    sta dx\n";
    code_ << "    lda bc+1\n    sbc ac+1\n    ora dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction: after the final SBC, the sign bit
// (N) reflects the true signed result only once corrected for overflow
// (V) - if V is set the apparent sign is backwards, so EOR #$80 fixes it.
void M65C02Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert) {
    genExpr(a);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(b);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = a
    code_ << "    sec\n    lda bc\n    sbc ac\n    lda bc+1\n    sbc ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n";
        code_ << "    eori 128\n";
        code_ << Lfixskip << ":\n";
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("beq", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("bne", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("beq", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

std::string M65C02Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 65C02 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M65C02Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleM65C02(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM65C02Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
