#include "PIC16Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "PIC16Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

void emitInitByte(std::ostream &out, const std::string &lbl, uint8_t v) {
    if (v == 0) {
        out << "    clrf " << lbl << "\n";
    } else {
        out << "    movlw " << static_cast<int>(v) << "\n    movwf " << lbl << "\n";
    }
}

// `lbl`'s fixed address if it names one of the shared scratch
// pseudo-registers/software-stack cells, else std::nullopt.
std::optional<uint32_t> fixedRegAddr(const std::string &lbl) {
    if (lbl == "ac") return kPIC16RegAc;
    if (lbl == "bc") return kPIC16RegBc;
    if (lbl == "cx") return kPIC16RegCx;
    if (lbl == "dx") return kPIC16RegDx;
    if (lbl == "ptr") return kPIC16RegPtr;
    if (lbl == "SP") return kPIC16RegSP;
    if (lbl == "FP") return kPIC16RegFP;
    if (lbl == "SSTACK") return kPIC16RegSStack;
    return std::nullopt;
}
}  // namespace

void PIC16Codegen::declareRam(const std::string &lbl, int count) {
    if (objectMode_) {
        if (auto addr = fixedRegAddr(lbl)) {
            ramDecls_.push_back(lbl + " = " + std::to_string(*addr));
            return;
        }
    }
    ramDecls_.push_back(lbl + ": res " + std::to_string(count));
}

void PIC16Codegen::copy2(const std::string &dst, const std::string &src) {
    if (dst == src) return;
    code_ << "    movf " << src << ",w\n    movwf " << dst << "\n";
    code_ << "    movf " << src << "+1,w\n    movwf " << dst << "+1\n";
}

void PIC16Codegen::clear2(const std::string &lbl) { code_ << "    clrf " << lbl << "\n    clrf " << lbl << "+1\n"; }

void PIC16Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    bcf STATUS,0\n";
    if (isLeft) {
        code_ << "    rlf " << lbl << ",f\n";
        code_ << "    rlf " << lbl << "+1,f\n";
    } else {
        code_ << "    rrf " << lbl << "+1,f\n";
        code_ << "    rrf " << lbl << ",f\n";
    }
}

// dst = a + b: low byte's ADDWF leaves its carry in STATUS<C>, folded into
// the high byte's operand (BTFSC + ADDLW 1) before the second ADDWF (no
// ADDWFC on this core). Safe when `dst` aliases `a`/`b`: the low-byte store
// happens before the high-byte read.
void PIC16Codegen::add16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    addwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    btfsc STATUS,0\n";
    code_ << "    addlw 1\n";
    code_ << "    addwf " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

// dst = a - b via SUBWF (f - W -> dest, subtrahend in W first).
// STATUS<C> = 1 means no borrow (like 6502 SBC); when the low byte borrows
// (C=0), BTFSS + ADDLW 1 folds that borrow into the high byte before the
// second SUBWF.
void PIC16Codegen::sub16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    subwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    btfss STATUS,0\n";
    code_ << "    addlw 1\n";
    code_ << "    subwf " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

// 16-bit increment via INCFSZ's "increment, skip next if result is zero":
// when the low byte wraps 0xFF->0x00, the following `goto` is skipped,
// falling into the high byte's INCF.
void PIC16Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    incfsz " << lbl << ",f\n";
    code_ << "    goto " << Lskip << "\n";
    code_ << "    incf " << lbl << "+1,f\n";
    code_ << Lskip << ":\n";
}

void PIC16Codegen::testAcZero() { code_ << "    movf ac,w\n    iorwf ac+1,w\n"; }

void PIC16Codegen::gotoIfZero(const std::string &target) {
    code_ << "    btfsc STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

void PIC16Codegen::gotoIfNonZero(const std::string &target) {
    code_ << "    btfss STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

void PIC16Codegen::storeAcIndirect(int width) {
    code_ << "    movf ptr,w\n    movwf FSR\n";
    code_ << "    movf ac,w\n    movwf INDF\n";
    if (width == 1) return;
    code_ << "    incf FSR,f\n";
    code_ << "    movf ac+1,w\n    movwf INDF\n";
}

void PIC16Codegen::loadAcIndirect(int width, bool isUnsigned) {
    code_ << "    movf ptr,w\n    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf ac\n";
    if (width == 1) {
        signExtendAcHighByte(isUnsigned);
        return;
    }
    code_ << "    incf FSR,f\n";
    code_ << "    movf INDF,w\n    movwf ac+1\n";
}

void PIC16Codegen::signExtendAcHighByte(bool isUnsigned) {
    if (isUnsigned) {
        code_ << "    clrf ac+1\n";
        return;
    }
    // The branch on andlw's Z flag must come immediately after it.
    code_ << "    movf ac,w\n    andlw 0x80\n";
    std::string Lneg = newLabel(), Ldone = newLabel();
    gotoIfNonZero(Lneg);
    code_ << "    clrf ac+1\n    goto " << Ldone << "\n";
    code_ << Lneg << ":\n    movlw 0xFF\n    movwf ac+1\n";
    code_ << Ldone << ":\n";
}

void PIC16Codegen::loadAcFromLabel(const std::string &lbl, int width, bool isUnsigned) {
    code_ << "    movf " << lbl << ",w\n    movwf ac\n";
    if (width == 1) {
        signExtendAcHighByte(isUnsigned);
        return;
    }
    code_ << "    movf " << lbl << "+1,w\n    movwf ac+1\n";
}

void PIC16Codegen::storeAcToLabel(const std::string &lbl, int width) {
    code_ << "    movf ac,w\n    movwf " << lbl << "\n";
    if (width == 1) return;
    code_ << "    movf ac+1,w\n    movwf " << lbl << "+1\n";
}

// Runtime push/pop on the shared `SSTACK` region via `SP`/FSR/INDF.
// Pushes low byte then high; pops high then low (LIFO). `SP` always
// points one past the current top of stack.
void PIC16Codegen::pushCell2(const std::string &src) {
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf " << src << ",w\n    movwf INDF\n";
    code_ << "    incf FSR,f\n    incf SP,f\n";
    code_ << "    movf " << src << "+1,w\n    movwf INDF\n";
    code_ << "    incf SP,f\n";
}

void PIC16Codegen::popCell2(const std::string &dst) {
    code_ << "    decf SP,f\n";
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf " << dst << "+1\n";
    code_ << "    decf SP,f\n";
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf " << dst << "\n";
}

void PIC16Codegen::pushPtrCell() {
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf ptr,w\n    movwf INDF\n";
    code_ << "    incf SP,f\n";
}

void PIC16Codegen::popPtrCell() {
    code_ << "    decf SP,f\n";
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf ptr\n";
}

// A literal element's value is split into `width` compile-time-known bytes
// and loaded via emitInitByte(). A symbol element (dispatch-table
// address-of-function/global) is always native-int-width (2 bytes): a
// Function's program-memory word address splits low/high byte like
// genAddrOf()'s Func case (movlwcl/movlwch); a Slot's one-byte RAM address
// loads with a zero high byte, like genAddrOf()'s Var case.
void PIC16Codegen::emitInitElem(std::ostream &out, const std::string &base, int width, const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        const std::string hi = base + "+1";
        if (e.func) {
            out << "    movlwcl " << label(e.func) << "\n    movwf " << base << "\n";
            out << "    movlwch " << label(e.func) << "\n    movwf " << hi << "\n";
        } else {
            out << "    movlw " << label(e.slot) << "\n    movwf " << base << "\n";
            out << "    clrf " << hi << "\n";
        }
        for (int i = 2; i < width; i++) out << "    clrf " << base << "+" << i << "\n";
        return;
    }
    for (int i = 0; i < width; i++) {
        uint8_t b = static_cast<uint8_t>((static_cast<unsigned long>(e.value) >> (8 * i)) & 0xFF);
        emitInitByte(out, i == 0 ? base : base + "+" + std::to_string(i), b);
    }
}

std::string PIC16Codegen::generate(const ir::Program &prog, bool objectMode) {
    objectMode_ = objectMode;
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // mainFn is only ever null in object mode, for a file that doesn't
    // define `main` itself.

    declareRam("ac", 2);
    declareRam("bc", 2);
    declareRam("cx", 2);
    declareRam("dx", 2);
    declareRam("ptr", 1);
    declareRam("SP", 1);
    // Holds the current call's frame base within `SSTACK`.
    declareRam("FP", 1);
    // A deeper program trips PIC16Assembler.cpp's "file-register space
    // exhausted" check rather than silently corrupting memory.
    declareRam("SSTACK", kPIC16SStackSize);

    // Globals need real startup code, not a loaded data image: data
    // memory has no power-on value.
    std::ostringstream globalInit;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; it's only ever
        // referenced by name, deferred to a data-space relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        int elemWidth = widthOfType(slot->type);
        if (slot->arraySize > 0) {
            declareRam(lbl, slot->arraySize * elemWidth);
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                std::string base = lbl + "+" + std::to_string(i * elemWidth);
                emitInitElem(globalInit, base, elemWidth, slot->arrayInit[i]);
            }
        } else {
            declareRam(lbl, elemWidth);
            long v = slot->hasInit ? slot->initValue : 0;
            for (int i = 0; i < elemWidth; i++) {
                uint8_t b = static_cast<uint8_t>((static_cast<unsigned long>(v) >> (8 * i)) & 0xFF);
                emitInitByte(globalInit, i == 0 ? lbl : lbl + "+" + std::to_string(i), b);
            }
        }
    }

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // globalInit has nowhere else to run in object mode, so it
            // runs in fn_main's own prologue instead, which still needs
            // its own frame (no params, but may have Locals).
            curNumParams_ = 0;
            code_ << label(fb.func) << ":\n";
            emitPrologue(/*numParams=*/0, fb.func->frameSize);
            code_ << globalInit.str();
            genStmt(*fb.body);
            emitEpilogue(/*numParams=*/0);
            code_ << "    return\n\n";
        } else {
            genFunction(fb);
        }
    }

    std::ostringstream header;
    header << "INDF = 0\nSTATUS = 3\nFSR = 4\nINTCON = 0x0B\n\n";
    // In object mode no object emits the entry sequence; the linker
    // synthesizes exactly one once every object is merged.
    header << "org 0\n";
    if (!objectMode) {
        header << "    goto START\n\n";
        header << "START:\n";
        header << "    movlw SSTACK\n    movwf SP\n";
        header << globalInit.str();
        header << "    call " << label(mainFn) << "\n";
        header << "HALT:\n    goto HALT\n\n";
    }

    std::ostringstream out;
    out << header.str();
    out << code_.str();
    // In whole-program mode every pseudo-register/global shares one `res`
    // counter starting at `kPIC16RamBase`. In object mode the fixed
    // pseudo-registers are already equates, so `ramDecls_` holds only
    // globals' `res` lines at the default `orgram` cursor (0).
    if (!objectMode) out << "\norgram " << kPIC16RamBase << "\n";
    for (const auto &l : ramDecls_) out << l << "\n";
    return out.str();
}

std::map<std::string, std::string> PIC16Codegen::perFileAsm() const { return perFileAsm_; }

void PIC16Codegen::loadFrameBaseW(const ir::Slot *slot) {
    code_ << "    movlw " << frameSlotOffset(slot) << "\n";
    code_ << "    addwf FP,w\n";
}

void PIC16Codegen::loadFrameAddr(const ir::Slot *slot) {
    loadFrameBaseW(slot);
    code_ << "    movwf ptr\n";
}

// Pushes the caller's `FP` onto `SSTACK` right after the argument words
// the caller already pushed, sets `FP` to this call's frame base
// (SP - 2*numParams), then reserves `frameSize` more words for Locals.
void PIC16Codegen::emitPrologue(int numParams, int frameSize) {
    code_ << "    movlw " << (2 * numParams) << "\n";
    code_ << "    subwf SP,w\n";
    code_ << "    movwf ptr\n";  // ptr = this call's frame base (FP-to-be)
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf FP,w\n    movwf INDF\n";  // save the caller's FP
    code_ << "    incf SP,f\n";
    code_ << "    movf ptr,w\n    movwf FP\n";
    if (frameSize > 0) {
        code_ << "    movlw " << frameSize << "\n";
        code_ << "    addwf SP,f\n";
    }
}

// Reverse of emitPrologue(): `SP = FP` deallocates the whole frame (Locals,
// saved-FP byte, argument words) at once, then restores the caller's FP
// (read before FP itself is overwritten).
void PIC16Codegen::emitEpilogue(int numParams) {
    code_ << "    movlw " << (2 * numParams) << "\n";
    code_ << "    addwf FP,w\n";
    code_ << "    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf ptr\n";  // ptr = the caller's FP
    code_ << "    movf FP,w\n    movwf SP\n";
    code_ << "    movf ptr,w\n    movwf FP\n";
}

void PIC16Codegen::genFunction(const ir::FunctionBody &fn) {
    curNumParams_ = static_cast<int>(fn.func->params.size());
    code_ << label(fn.func) << ":\n";
    emitPrologue(curNumParams_, fn.func->frameSize);
    genStmt(*fn.body);
    emitEpilogue(curNumParams_);
    code_ << "    return\n\n";
}

void PIC16Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testAcZero();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            gotoIfZero(s.elseStmt ? Lelse : Ldone);
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    goto " << Ldone << "\n";
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
            testAcZero();
            gotoIfZero(Lend);
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    goto " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    goto " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    goto " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            // Evaluated into `ac` before the epilogue runs; the epilogue's
            // scratch use of `ptr` never touches `ac`, so the return value
            // survives it intact.
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            emitEpilogue(curNumParams_);
            code_ << "    return\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // The Local's frame words already exist (emitPrologue()
            // reserved them); this just fills them in.
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddr(s.declSlot);
                storeAcIndirect(widthOfType(s.declSlot->type));
            }
            // An uninitialized scalar keeps whatever its frame slot held,
            // like an ordinary C automatic variable.
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
            code_ << "    goto " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void PIC16Codegen::requireNarrow(const ir::Expr &e) const {
    if (widthOfType(e.type) > 2) {
        err(e.line, "'long'/'long long' arithmetic isn't supported on this target - "
                    "see PIC16Codegen.h's own header comment on requireNarrow()");
    }
}

// Narrowing to `char` masks off the high byte and, for a signed target,
// sign-extends bit 7 back across it; every other conversion is already a
// pure reinterpretation at this cell's fixed 2-byte width.
void PIC16Codegen::genConvert(const ir::Expr &e) {
    genExpr(*e.lhs);
    if (widthOfType(e.type) != 1) return;
    signExtendAcHighByte(e.type.isUnsigned);
}

void PIC16Codegen::genExpr(const ir::Expr &e) {
    requireNarrow(e);
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movlw " << (v16 & 0xFF) << "\n    movwf ac\n";
            code_ << "    movlw " << ((v16 >> 8) & 0xFF) << "\n    movwf ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            // e.slot is always a scalar here; a bare array Var is rejected
            // outside of `&`.
            if (e.slot->kind == ir::SlotKind::Global) {
                loadAcFromLabel(label(e.slot), widthOfType(e.slot->type), e.slot->type.isUnsigned);
            } else {
                loadFrameAddr(e.slot);
                loadAcIndirect(widthOfType(e.slot->type), e.slot->type.isUnsigned);
            }
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                pushPtrCell();
                genExpr(*e.rhs);
                popPtrCell();
                storeAcIndirect(widthOfType(e.slot->type));
                return;
            }
            genExpr(*e.rhs);
            // Computing the target address never touches `ac`, so it's
            // safe to do after rhs is already evaluated into it.
            if (e.slot->kind == ir::SlotKind::Global) {
                storeAcToLabel(label(e.slot), widthOfType(e.slot->type));
            } else {
                loadFrameAddr(e.slot);
                storeAcIndirect(widthOfType(e.slot->type));
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    movf ac,w\n    movwf ptr\n";
            pushPtrCell();
            genExpr(*e.rhs);
            popPtrCell();
            storeAcIndirect(widthOfType(e.type));
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                genCompoundThroughPtr(e);
                return;
            }
            // Computing the target address never touches `ac`, so it's
            // simply recomputed (or named again) after rhs.
            genExpr(*e.rhs);  // ac = rhs
            copy2("dx", "ac");  // dx = rhs, held until consumed
            if (e.slot->kind == ir::SlotKind::Global) {
                loadAcFromLabel(label(e.slot), widthOfType(e.slot->type), e.slot->type.isUnsigned);  // ac = old
            } else {
                loadFrameAddr(e.slot);
                loadAcIndirect(widthOfType(e.slot->type), e.slot->type.isUnsigned);  // ac = old
            }
            copy2("bc", "ac");  // bc = old - genCompoundOp()'s precondition
            if (e.yieldOld) pushCell2("bc");  // old, saved across genCompoundOp()
            copy2("ac", "dx");  // ac = rhs - genCompoundOp()'s other precondition
            genCompoundOp(e.op, e.type);
            narrowAcTo(e.type);
            if (e.slot->kind == ir::SlotKind::Global) {
                storeAcToLabel(label(e.slot), widthOfType(e.slot->type));
            } else {
                loadFrameAddr(e.slot);
                storeAcIndirect(widthOfType(e.slot->type));
            }
            if (e.yieldOld) popCell2("ac");  // ac = old (final result)
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    movf ac,w\n    movwf ptr\n";
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testAcZero();
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            gotoIfZero(Lelse);
            genExpr(*e.rhs);
            code_ << "    goto " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    comf ac,f\n    comf ac+1,f\n";
                inc16("ac");
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    movf ac,w\n    movwf ptr\n";
                loadAcIndirect(widthOfType(e.type), e.type.isUnsigned);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    comf ac,f\n    comf ac+1,f\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Each argument is pushed onto `SSTACK` in paramIndex order
            // (SP only increments, so the first pushed argument ends up
            // at the lowest address), the layout emitPrologue() expects.
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                pushCell2("ac");
            }
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // No indirect CALL/GOTO exists on this core (see genAddrOf's
            // ir::ExprKind::Func case) - a function pointer can be stored,
            // compared, or passed around, but never called through.
            err(e.line, "calling through a function pointer isn't supported on this target - "
                        "no indirect CALL/GOTO exists on this core");
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Global-interrupt enable/disable: INTCON<7> (GIE). No
            // peripheral interrupt source is ever enabled, so this alone
            // is inert but real.
            code_ << "    " << (e.op == "ion" ? "bsf" : "bcf") << " INTCON,7\n";
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// Leaves slot[idxExpr]'s effective file address in `ptr` (one byte).
// `idxExpr` is evaluated first and alone, since it may itself clobber
// every pseudo-register this function uses.
void PIC16Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ac *= element width (elemShift()); a char element needs no shift.
    for (int i = 0, n = elemShift(slot->type); i < n; i++) shiftOneBit("ac", /*isLeft=*/true);
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        if (slot->arraySize > 0) {
            code_ << "    movlw " << lbl << "\n";
        } else {
            code_ << "    movf " << lbl << ",w\n";
        }
    } else if (slot->arraySize > 0) {
        // A Local/Param array's base is FP-relative.
        loadFrameBaseW(slot);
    } else {
        // A Local/Param pointer variable's value is the base to index
        // from, read out of its frame slot first.
        loadFrameAddr(slot);
        code_ << "    movf ptr,w\n    movwf FSR\n";
        code_ << "    movf INDF,w\n";
    }
    code_ << "    addwf ac,w\n";
    code_ << "    movwf ptr\n";
}

void PIC16Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadAcIndirect(widthOfType(e.slot->type), e.slot->type.isUnsigned);
}

void PIC16Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    movlw " << label(operand.slot) << "\n    movwf ac\n";
        } else {
            loadFrameAddr(operand.slot);
            code_ << "    movf ptr,w\n    movwf ac\n";
        }
        code_ << "    clrf ac+1\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movf ptr,w\n    movwf ac\n    clrf ac+1\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        // `ac` holds fn's program-memory word address, split low/high
        // byte. No indirect CALL/GOTO exists on this core (CALL/GOTO only
        // take an inline literal target), so this value can be stored,
        // compared, or passed around, but never called through.
        code_ << "    movlwcl " << label(operand.func) << "\n    movwf ac\n";
        code_ << "    movlwch " << label(operand.func) << "\n    movwf ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Stores arrayInit into `slot`'s frame words; re-run on every call since
// a Local's storage is a fresh frame region each activation, unlike a
// Global's static trailer. arrayInit is always zero-padded to arraySize.
// A symbol element is a Function's program-memory address (movlwcl/
// movlwch) or another Slot's address - compile-time for a Global, FP-
// relative for a Local/Param. Element `i`'s address is the slot's base
// plus `i` element-widths (increasing, since SSTACK grows upward). A
// literal element is written byte by byte directly, bypassing `ac`, so a
// local array wider than `ac` (long/long long) can still be initialized.
void PIC16Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int elemWidth = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int addrOffset = frameSlotOffset(slot) + elemWidth * static_cast<int>(i);
        if (elem.isSymbol) {
            if (elem.func) {
                code_ << "    movlwcl " << label(elem.func) << "\n    movwf ac\n";
                code_ << "    movlwch " << label(elem.func) << "\n    movwf ac+1\n";
            } else if (elem.slot->kind == ir::SlotKind::Global) {
                code_ << "    movlw " << label(elem.slot) << "\n    movwf ac\n";
                code_ << "    clrf ac+1\n";
            } else {
                loadFrameAddr(elem.slot);
                code_ << "    movf ptr,w\n    movwf ac\n";
                code_ << "    clrf ac+1\n";
            }
            code_ << "    movlw " << addrOffset << "\n";
            code_ << "    addwf FP,w\n    movwf ptr\n";
            storeAcIndirect(2);
            continue;
        }
        for (int b = 0; b < elemWidth; b++) {
            uint8_t byteVal = static_cast<uint8_t>((static_cast<unsigned long>(elem.value) >> (8 * b)) & 0xFF);
            code_ << "    movlw " << (addrOffset + b) << "\n";
            code_ << "    addwf FP,w\n    movwf ptr\n";
            code_ << "    movf ptr,w\n    movwf FSR\n";
            if (byteVal == 0) {
                code_ << "    clrf INDF\n";
            } else {
                code_ << "    movlw " << static_cast<int>(byteVal) << "\n    movwf INDF\n";
            }
        }
    }
}

// Every binary op saves its left operand across evaluating the right one
// on `SSTACK` (a fixed cell would be clobbered by a nested call).
void PIC16Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        pushCell2("ac");
        genExpr(*e.rhs);
        popCell2("bc");
        if (op == "+") {
            add16("ac", "bc", "ac");
        } else {
            sub16("ac", "bc", "ac");
        }
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        return;
    }
    if (op == "/") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/false);
        return;
    }
    if (op == "%") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/true);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        genExpr(*e.lhs);
        pushCell2("ac");
        genExpr(*e.rhs);
        popCell2("bc");
        const char *insn = (op == "&") ? "andwf" : (op == "|") ? "iorwf" : "xorwf";
        code_ << "    movf bc,w\n    " << insn << " ac,w\n    movwf ac\n";
        code_ << "    movf bc+1,w\n    " << insn << " ac+1,w\n    movwf ac+1\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(*e.lhs, *e.rhs, op == "<<");
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

// No variable-count shift instruction (RLF/RRF rotate exactly one bit),
// so loop `rhs` times via shiftOneBit().
void PIC16Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    code_ << "    movf ac,w\n    movwf cx\n";  // cx = shift count (low byte only)
    popCell2("ac");  // ac = value to shift
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";  // MOVF also sets Z
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    shiftOneBit("ac", isLeft);
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (no hardware multiply): `bc` is the
// multiplicand (doubled each iteration), `dx` the multiplier (halved
// each iteration, consumed one bit at a time), `ac` the accumulating
// product. Only verified against non-negative operands.
void PIC16Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("dx", "ac");  // dx = multiplier
    popCell2("bc");     // bc = multiplicand
    clear2("ac");
    code_ << "    movlw 16\n    movwf cx\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    code_ << "    movf dx,w\n    andlw 1\n";
    code_ << "    btfsc STATUS,2\n    goto " << Lskipadd << "\n";
    add16("ac", "ac", "bc");
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative. Only verified against non-
// negative operands.
void PIC16Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("bc", "ac");  // bc = divisor
    popCell2("dx");     // dx = dividend (running remainder)
    clear2("ac");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    sub16("cx", "dx", "bc");  // cx = dx - bc (tentative); STATUS<C> = 1 iff no borrow (dx >= bc)
    code_ << "    btfss STATUS,0\n    goto " << Ldone << "\n";
    copy2("dx", "cx");
    inc16("ac");
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy2("ac", "dx");
}

// Precondition: `bc` holds `old`, `ac` holds `rhs` - the same register
// roles genBinary()'s operator bodies settle into after evaluating their
// own lhs/rhs. Leaves the raw new value in `ac`.
void PIC16Codegen::genCompoundOp(const std::string &op, Type) {
    if (op == "+") {
        add16("ac", "bc", "ac");
        return;
    }
    if (op == "-") {
        sub16("ac", "bc", "ac");
        return;
    }
    if (op == "*") {
        copy2("dx", "ac");  // dx = multiplier (rhs) - bc already holds the multiplicand (old)
        clear2("ac");
        code_ << "    movlw 16\n    movwf cx\n";
        std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    movf cx,w\n";
        code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
        code_ << "    movf dx,w\n    andlw 1\n";
        code_ << "    btfsc STATUS,2\n    goto " << Lskipadd << "\n";
        add16("ac", "ac", "bc");
        code_ << Lskipadd << ":\n";
        shiftOneBit("bc", /*isLeft=*/true);
        shiftOneBit("dx", /*isLeft=*/false);
        code_ << "    decf cx,f\n";
        code_ << "    goto " << Lloop << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (op == "/" || op == "%") {
        copy2("dx", "bc");  // dx = dividend (old) - copied before bc is overwritten below
        copy2("bc", "ac");   // bc = divisor (rhs)
        clear2("ac");
        std::string Lloop = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        sub16("cx", "dx", "bc");
        code_ << "    btfss STATUS,0\n    goto " << Ldone << "\n";
        copy2("dx", "cx");
        inc16("ac");
        code_ << "    goto " << Lloop << "\n";
        code_ << Ldone << ":\n";
        if (op == "%") copy2("ac", "dx");
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "andwf" : (op == "|") ? "iorwf" : "xorwf";
        code_ << "    movf bc,w\n    " << insn << " ac,w\n    movwf ac\n";
        code_ << "    movf bc+1,w\n    " << insn << " ac+1,w\n    movwf ac+1\n";
        return;
    }
    // "<<" or ">>"
    code_ << "    movf ac,w\n    movwf cx\n";  // cx = shift count (rhs's own low byte)
    copy2("ac", "bc");                           // ac = value to shift (old)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    shiftOneBit("ac", op == "<<");
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// `ptr` holds the resolved address on entry, stashed via pushPtrCell()
// while `rhs` (which may itself clobber every pseudo-register) is
// evaluated. `yieldOld`'s value survives on `SSTACK` across
// genCompoundOp()'s use of bc/cx/dx.
void PIC16Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    pushPtrCell();        // stash address
    genExpr(*e.rhs);        // ac = rhs
    copy2("dx", "ac");         // dx = rhs, held until genCompoundOp() consumes it
    popPtrCell();                // ptr = address, restored only now
    loadAcIndirect(widthOfType(e.type), e.type.isUnsigned);  // ac = old
    copy2("bc", "ac");                                         // bc = old - genCompoundOp()'s precondition
    if (e.yieldOld) pushCell2("bc");  // old, saved across genCompoundOp()
    copy2("ac", "dx");                  // ac = rhs - genCompoundOp()'s other precondition
    genCompoundOp(e.op, e.type);          // ac = new (raw)
    narrowAcTo(e.type);
    storeAcIndirect(widthOfType(e.type));  // ptr still untouched since popPtrCell()
    if (e.yieldOld) popCell2("ac");          // ac = old (final result)
}

void PIC16Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    signExtendAcHighByte(to.isUnsigned);
}

void PIC16Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling: sub16() is zero iff the operands are
// equal, checked by ORing both result bytes.
void PIC16Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    popCell2("bc");  // bc = lhs, ac = rhs
    sub16("cx", "bc", "ac");
    code_ << "    movf cx,w\n    iorwf cx+1,w\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "btfsc" : "btfss") << " STATUS,2\n    goto " << Ltrue << "\n";
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the sign-bit-bias trick: flipping the top bit
// of each operand's high byte maps signed order onto unsigned order, then
// sub16()'s STATUS<C> gives the answer.
void PIC16Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    pushCell2("ac");
    genExpr(b);
    popCell2("bc");  // bc = a, ac = b
    code_ << "    movf bc+1,w\n    xorlw 0x80\n    movwf bc+1\n";
    code_ << "    movf ac+1,w\n    xorlw 0x80\n    movwf ac+1\n";
    sub16("cx", "bc", "ac");  // cx = a - b (biased); STATUS<C> = 0 iff a < b
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (invert ? "btfsc" : "btfss") << " STATUS,0\n    goto " << Ltrue << "\n";
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

void PIC16Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testAcZero();
    gotoIfZero(Lfalse);
    genExpr(rhs);
    testAcZero();
    gotoIfZero(Lfalse);
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n    goto " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    clear2("ac");
    code_ << Ldone << ":\n";
}

void PIC16Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testAcZero();
    gotoIfNonZero(Ltrue);
    genExpr(rhs);
    testAcZero();
    gotoIfNonZero(Ltrue);
    code_ << Lfalse << ":\n";
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

void PIC16Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testAcZero();
    std::string Ltrue = newLabel(), Ldone = newLabel();
    gotoIfZero(Ltrue);
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> PIC16Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "hex") return assemblePIC16(asmText);
    // coff/elf/aout/objraw: build one format-neutral ObjectFile, then pick
    // that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePIC16Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
