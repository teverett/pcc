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

// `lbl`'s own fixed address, if it names one of the seven shared
// scratch pseudo-registers/software-stack cells (see
// PIC16Assembler.h's own `kPIC16Reg*` comment) - `std::nullopt` for
// every other name (a real global/local/param, which declareRam()
// still reserves via `res` even in object mode).
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

// dst = a + b, via the standard PIC16 textbook 16-bit-add idiom (no
// ADDWFC on this core - see PIC16Codegen.h's own header comment):
// the low byte's own ADDWF leaves its carry-out in STATUS<C>, folded
// into the high byte's own operand (via BTFSC + ADDLW 1) before that
// second ADDWF. Safe when `dst` aliases `a` or `b`: the low-byte
// store only ever happens after both low bytes are already consumed
// into W, and the high-byte read of `a`/`b` never happens until after
// that store, by which point only the low byte (not the high byte)
// could have been overwritten.
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

// dst = a - b, the same shape, via SUBWF (f - W -> dest, so the
// divisor/subtrahend has to be in W first) and the no-borrow-carry
// convention (STATUS<C> = 1 means no borrow happened, same as 6502's
// SBC, opposite of the 8080's CY - see I8080Codegen.h's own header
// comment on that difference): when the low byte borrowed (C=0),
// BTFSS + ADDLW 1 bumps what gets subtracted from the high byte by
// one, exactly folding that borrow in before the second SUBWF.
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

// 16-bit increment via INCFSZ's own "increment f, skip the next
// instruction if the result is zero" shape - the standard PIC16 way
// to chain a carry into the high byte with no ADDWFC available: when
// the low byte wraps 0xFF->0x00, the `goto` immediately after is
// skipped, falling straight into the high byte's own INCF.
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

void PIC16Codegen::storeAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR\n";
    code_ << "    movf ac,w\n    movwf INDF\n";
    code_ << "    incf FSR,f\n";
    code_ << "    movf ac+1,w\n    movwf INDF\n";
}

void PIC16Codegen::loadAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf ac\n";
    code_ << "    incf FSR,f\n";
    code_ << "    movf INDF,w\n    movwf ac+1\n";
}

// Real runtime push/pop on the shared `SSTACK` region via `SP`/FSR/
// INDF - see PIC16Codegen.h's own header comment on why this has to
// be a genuine stack, not a compile-time-indexed fixed cell. Pushes
// low byte then high; pops high then low (LIFO) - `SP` always points
// one past the current top of stack, matching every push leaving it
// there for the next one.
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

// A literal element's own value, split into two compile-time-known
// bytes and loaded via emitInitByte()'s usual clrf/movlw+movwf
// choice; a symbol element (a dispatch-table address-of-function/
// global, see IR.h's ArrayInitElem) has no compile-time byte value at
// all - a Function's own real program-memory *word* address is split
// low byte/high byte the same way genAddrOf()'s own Func case does
// (`movlwcl`/`movlwch`), and a Slot's own one-byte RAM address (data
// space, always 0-255 on this baseline/mid-range target) is loaded
// with a zero high byte, the same way genAddrOf()'s own Var case
// does.
void PIC16Codegen::emitInitElem(std::ostream &out, const std::string &loLbl, const std::string &hiLbl,
                                 const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        if (e.func) {
            out << "    movlwcl " << label(e.func) << "\n    movwf " << loLbl << "\n";
            out << "    movlwch " << label(e.func) << "\n    movwf " << hiLbl << "\n";
        } else {
            out << "    movlw " << label(e.slot) << "\n    movwf " << loLbl << "\n";
            out << "    clrf " << hiLbl << "\n";
        }
        return;
    }
    uint16_t v16 = static_cast<uint16_t>(e.value);
    emitInitByte(out, loLbl, static_cast<uint8_t>(v16 & 0xFF));
    emitInitByte(out, hiLbl, static_cast<uint8_t>((v16 >> 8) & 0xFF));
}

std::string PIC16Codegen::generate(const ir::Program &prog, bool objectMode) {
    objectMode_ = objectMode;
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (i.e. whenever `objectMode` is false
    // here too - see Codegen.h's own header comment), so mainFn is
    // only ever null in object mode, for a file that doesn't happen
    // to define `main` itself.

    declareRam("ac", 2);
    declareRam("bc", 2);
    declareRam("cx", 2);
    declareRam("dx", 2);
    declareRam("ptr", 1);
    declareRam("SP", 1);
    // Holds the current call's own frame base within `SSTACK` - see
    // PIC16Codegen.h's own header comment on the frame layout
    // emitPrologue()/emitEpilogue() build there.
    declareRam("FP", 1);
    // `kPIC16SStackSize` bytes - comfortably more than any expression
    // nesting (plus call-site saves) this project's own test programs
    // ever reach, *and* enough call frames deep for every test this
    // backend's own correctness was hand-verified against (see
    // PIC16Codegen.h's own header comment: every Local/Param's own
    // real per-call storage lives here too now, not just binary-op
    // operand saves). A deeper program trips PIC16Assembler.cpp's own
    // "file-register space exhausted" check rather than silently
    // corrupting memory.
    declareRam("SSTACK", kPIC16SStackSize);

    // Globals need real startup code, not a loaded data image - see
    // PIC16Codegen.h's own header comment on why (Harvard
    // architecture: data memory has no "load this value at boot"
    // concept the way program memory does).
    std::ostringstream globalInit;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // `res` reservation here, only ever be referenced by name
        // (see label()) in code, letting PIC16Assembler.cpp's own
        // object-mode resolution defer it to a data-space relocation
        // against whichever other object actually defines it.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareRam(lbl, slot->arraySize * 2);
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                std::string base = lbl + "+" + std::to_string(i * 2);
                std::string hi = lbl + "+" + std::to_string(i * 2 + 1);
                emitInitElem(globalInit, base, hi, slot->arrayInit[i]);
            }
        } else {
            declareRam(lbl, 2);
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            emitInitByte(globalInit, lbl, static_cast<uint8_t>(v16 & 0xFF));
            emitInitByte(globalInit, lbl + "+1", static_cast<uint8_t>((v16 >> 8) & 0xFF));
        }
    }

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // This file's own globals' init code (`globalInit` above)
            // has nowhere else to run in object mode -
            // PIC16LinkerFactory.cpp's own synthesized `START` entry
            // sequence is built once, before it has ever seen this
            // (or any) user object, so it can't know this file's own
            // globals exist at all. `fn_main`'s own prologue is the
            // one place every linked program is guaranteed to reach
            // exactly once, before any other code, that this file
            // does control - see PIC16Codegen.h's own `objectMode`
            // comment. `main` still needs its own real per-call frame
            // too (it has no params - IRBuilder guarantees that - but
            // may still have Locals), so this inlines the same
            // emitPrologue()/emitEpilogue() genFunction() would
            // otherwise provide, around the global init code and the
            // body alike.
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
    // `objectMode` means no object file ever emits the entry sequence
    // itself, even the one that happens to define `main` -
    // PIC16LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `main`'s linked
    // address is resolvable (see this file's own header comment on
    // `objectMode`). An object file's own base is always `org 0`
    // either way (see PIC16Assembler.h's own `kPIC16BaseAddress`
    // comment) - it's the linker's job to place it, not this
    // backend's.
    header << "org 0\n";
    if (!objectMode) {
        header << "    goto START\n\n";
        header << "START:\n";
        header << "    movlw SSTACK\n    movwf SP\n";
        // Every real Global (never a Local/Param - see this file's
        // own header comment on why those have no startup state of
        // their own left to initialize at all now - nor this
        // backend's own pseudo-registers) gets explicitly written
        // here first, since this target's own data memory powers up
        // to an unknown state - see PIC16Codegen.h's own header
        // comment.
        header << globalInit.str();
        header << "    call " << label(mainFn) << "\n";
        header << "HALT:\n    goto HALT\n\n";
    }

    std::ostringstream out;
    out << header.str();
    out << code_.str();
    // In whole-program mode, every pseudo-register/global shares one
    // unified `res` counter starting at the real GPR-bank base
    // (`kPIC16RamBase`) - a Local/Param is never part of it, having
    // no `res`-declared cell of its own at all any more (see this
    // file's own header comment). In object mode, declareRam()
    // already turned the eight fixed pseudo-registers into equates
    // above (no `res` at all for them - see that function's own
    // comment), so `ramDecls_` here holds only real globals' own
    // `res` lines, meant to be this *object's own* local, pre-link
    // data-space offsets - leaving the `orgram` cursor at its default
    // (0, reset at the start of every pass - see
    // PIC16Assembler.cpp's own `Assembler::pass()`) is exactly what
    // that needs, so no `orgram` directive is emitted here at all.
    if (!objectMode) out << "\norgram " << kPIC16RamBase << "\n";
    for (const auto &l : ramDecls_) out << l << "\n";
    return out.str();
}

// Single-file-only (see PIC16Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> PIC16Codegen::perFileAsm() const { return perFileAsm_; }

// Leaves FP + frameSlotOffset(slot) in `W` - see PIC16Codegen.h's own
// comment on this and the two callers built from it.
void PIC16Codegen::loadFrameBaseW(const ir::Slot *slot) {
    code_ << "    movlw " << frameSlotOffset(slot) << "\n";
    code_ << "    addwf FP,w\n";
}

void PIC16Codegen::loadFrameAddr(const ir::Slot *slot) {
    loadFrameBaseW(slot);
    code_ << "    movwf ptr\n";
}

// Pushes the caller's own `FP` onto `SSTACK` right after the
// `numParams` argument words the caller already pushed via
// pushCell2() (see genExpr()'s own Call case) - at this point `SP` is
// exactly `(this call's own argument base) + 2*numParams`, so
// subtracting that back out gives this call's own frame base, stored
// into `FP` once the caller's old value is safely saved. `frameSize`
// more words (this function's own Locals, 0 if it declares none) are
// then reserved by simply advancing `SP` past them - see
// PIC16Codegen.h's own header comment for the full frame layout.
void PIC16Codegen::emitPrologue(int numParams, int frameSize) {
    code_ << "    movlw " << (2 * numParams) << "\n";
    code_ << "    subwf SP,w\n";
    code_ << "    movwf ptr\n";  // ptr = this call's own frame base (FP-to-be)
    code_ << "    movf SP,w\n    movwf FSR\n";
    code_ << "    movf FP,w\n    movwf INDF\n";  // save the caller's own FP
    code_ << "    incf SP,f\n";
    code_ << "    movf ptr,w\n    movwf FP\n";
    if (frameSize > 0) {
        code_ << "    movlw " << (2 * frameSize) << "\n";
        code_ << "    addwf SP,f\n";
    }
}

// The exact reverse of emitPrologue(), read right to left: deallocate
// this whole frame at once (`SP = FP`, freeing its Locals, its own
// saved-FP byte, and the caller's pushed argument words together -
// cheaper than, and equivalent to, popping each in turn, and a
// callee-cleans-up convention, so genExpr()'s own Call case never
// needs to clean anything up after `call` returns either), then
// restore the caller's own FP from where emitPrologue() saved it
// (`FP+2*numParams`, read *before* `FP` itself is overwritten, or
// there'd be nothing left pointing at it).
void PIC16Codegen::emitEpilogue(int numParams) {
    code_ << "    movlw " << (2 * numParams) << "\n";
    code_ << "    addwf FP,w\n";
    code_ << "    movwf FSR\n";
    code_ << "    movf INDF,w\n    movwf ptr\n";  // ptr = the caller's own FP
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
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testAcZero();
            gotoIfZero(Lend);
            genStmt(*s.loopBody);
            code_ << "    goto " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            // Evaluated into `ac` before the epilogue runs - the
            // epilogue's own scratch use of `ptr` never touches `ac`
            // (see emitEpilogue()'s own comment), so the return value
            // survives it intact, the same way the caller already
            // expects to find a call's result in `ac` right after
            // `call` returns (see genExpr()'s own header comment).
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            emitEpilogue(curNumParams_);
            code_ << "    return\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // No `declareRam()` here any more - a Local's own two (or
            // `arraySize*2`) bytes already exist, reserved by
            // genFunction()'s own emitPrologue() advancing `SP` past
            // every Local in this function up front (see IR.h's own
            // `Function::frameSize`); this just has to fill them in.
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddr(s.declSlot);
                storeAcIndirect();
            }
            // No "else" case for a bare, uninitialized scalar: its
            // own frame word already exists - same as a real,
            // ordinary C automatic variable, its value is simply
            // whatever this call's own frame slot already held until
            // something assigns it (see PIC16Codegen.h's own header
            // comment: a Local is never zeroed at program start, only
            // ever written by codegen before any read of it).
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

void PIC16Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movlw " << (v16 & 0xFF) << "\n    movwf ac\n";
            code_ << "    movlw " << ((v16 >> 8) & 0xFF) << "\n    movwf ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            // IRBuilder already rejects a bare array Var outside of
            // `&` (which bypasses this path), so e.slot is always a
            // scalar here.
            if (e.slot->kind == ir::SlotKind::Global) {
                copy2("ac", label(e.slot));
            } else {
                loadFrameAddr(e.slot);
                loadAcIndirect();
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
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            // Computing the target address never touches `ac` (see
            // loadFrameAddr()'s own comment), so it's always safe to
            // do after the rhs is already evaluated into it, Global
            // or Local/Param alike.
            if (e.slot->kind == ir::SlotKind::Global) {
                copy2(label(e.slot), "ac");
            } else {
                loadFrameAddr(e.slot);
                storeAcIndirect();
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    movf ac,w\n    movwf ptr\n";
            pushPtrCell();
            genExpr(*e.rhs);
            popPtrCell();
            storeAcIndirect();
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
                loadAcIndirect();
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    comf ac,f\n    comf ac+1,f\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Each argument is pushed onto the real `SSTACK` software
            // stack, in paramIndex order (see pushCell2()'s own
            // comment: `SP` only ever increments, so the first-
            // pushed argument ends up at the lowest address) - the
            // exact shape emitPrologue() expects to find waiting for
            // it on entry (see PIC16Codegen.h's own header comment on
            // the frame layout). Evaluating argument i+1 can never
            // clobber argument i's own already-pushed value, the same
            // "a real stack has no aliasing problem" guarantee this
            // file's own header comment already relies on for
            // genBinary()'s operand saves.
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                pushCell2("ac");
            }
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real PIC16 global-interrupt enable/disable - INTCON<7>
            // (GIE), the same real hardware feature every other
            // backend's own _ion()/_iof() toggles (see
            // I8080Codegen.cpp's own ei/di comment) - nothing here
            // ever actually enables a peripheral interrupt source, so
            // setting GIE alone is a real but inert hardware action,
            // same spirit as those other backends.
            code_ << "    " << (e.op == "ion" ? "bsf" : "bcf") << " INTCON,7\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective file address in `ptr` (always
// one byte on this target - see PIC16Codegen.h's own header
// comment). `idxExpr` is evaluated first and alone, before anything
// here touches its own scratch (`ac`'s low byte, then `ptr`), for the
// same reason every other backend's own genIndexAddr() does: it's a
// general recursive call that might itself need every pseudo-
// register this function could otherwise clobber.
void PIC16Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    shiftOneBit("ac", /*isLeft=*/true);  // ac *= 2 (words are 2 bytes on this byte-addressed target)
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        if (slot->arraySize > 0) {
            code_ << "    movlw " << lbl << "\n";
        } else {
            code_ << "    movf " << lbl << ",w\n";
        }
    } else if (slot->arraySize > 0) {
        // A Local/Param array's own base is FP-relative, not a
        // compile-time address - loadFrameBaseW() leaves it in `w`
        // directly, ready for the same "add in ac's scaled index"
        // step below.
        loadFrameBaseW(slot);
    } else {
        // A Local/Param pointer variable's own VALUE is the base to
        // index from - read out of its own frame slot first (only
        // the low byte matters, same as a Global pointer variable's
        // own `movf lbl,w` above - see PIC16Codegen.h's own header
        // comment on why every address here already fits one byte).
        loadFrameAddr(slot);
        code_ << "    movf ptr,w\n    movwf FSR\n";
        code_ << "    movf INDF,w\n";
    }
    code_ << "    addwf ac,w\n";
    code_ << "    movwf ptr\n";
}

void PIC16Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadAcIndirect();
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
        // `ac` ends up holding `fn`'s own real program-memory *word*
        // address (never a call target here, just a value - see
        // PIC16Assembler.cpp's own `movlwcl`/`movlwch` comment), split
        // low byte/high byte across two literal loads the way every
        // narrow-literal-field core here needs for a wider-than-8-bit
        // address. There's deliberately no `ir::ExprKind::CallPtr` case
        // in genExpr()'s own switch above - real baseline/mid-range
        // PIC16 has no indirect CALL/GOTO at all (its one hardware
        // call/return stack isn't software-addressable, and CALL/GOTO
        // only ever take an inline literal target - see
        // PIC16Codegen.h's own header comment), so a value produced
        // here can be stored, compared, or passed around, but never
        // actually called through on this target.
        code_ << "    movlwcl " << label(operand.func) << "\n    movwf ac\n";
        code_ << "    movlwch " << label(operand.func) << "\n    movwf ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Stores arrayInit into `slot`'s own frame words - every call needs
// this re-run, since (unlike a Global's one static trailer, written
// once by globalInit) a Local's storage is this activation's own
// fresh frame region, not pre-set to anything; arrayInit is always
// fully populated up to arraySize (zero-padded even with no source-
// level initializer - see BuildAst.cpp's padArrayInit), so this
// always leaves the whole array in a known state, same as a Global
// array's static trailer always did. A literal element's value is
// split into `ac`'s two bytes the same way an IntLit is; a symbol
// element is either a Function's own program-memory address
// (`movlwcl`/`movlwch`, same as genAddrOf()'s own Func case) or
// another Slot's own address - a compile-time constant for a Global
// (`label(elem.slot)`), but FP-relative, so only known at runtime,
// for a Local/Param (e.g. `int x; int *p[] = {&x};` - a valid,
// already-resolved reference to a sibling Local/Param - see
// IRBuilder.cpp's own `resolveArrayInit`). Element `i`'s own address
// is `slot`'s own first word plus `i` words - *increasing*, never the
// base-minus-`i` arithmetic a decreasing-address frame would need
// here (see PIC16Codegen.h's own header comment).
void PIC16Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
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
        } else {
            uint16_t v16 = static_cast<uint16_t>(elem.value);
            code_ << "    movlw " << (v16 & 0xFF) << "\n    movwf ac\n";
            code_ << "    movlw " << ((v16 >> 8) & 0xFF) << "\n    movwf ac+1\n";
        }
        code_ << "    movlw " << (frameSlotOffset(slot) + 2 * static_cast<int>(i)) << "\n";
        code_ << "    addwf FP,w\n    movwf ptr\n";
        storeAcIndirect();
    }
}

// Every binary op here saves its left operand across evaluating the
// right one on the real `SSTACK` software stack (see PIC16Codegen.h's
// own header comment on why a fixed cell can't safely play this role
// instead - a nested call would clobber it).
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

// No variable-count shift instruction (RLF/RRF always rotate exactly
// one bit) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit().
void PIC16Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    code_ << "    movf ac,w\n    movwf cx\n";  // cx = shift count (always small; low byte is enough)
    popCell2("ac");  // ac = value to shift
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";  // MOVF affects Z too - no separate zero-test needed for a single byte
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

void PIC16Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a 16-bit subtract (full
// borrow chain via sub16()) is zero iff the operands are equal,
// checked the same "OR the two result bytes together" way
// testAcZero() checks `ac`.
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

// Signed 16-bit "a < b" via the standard bias trick (see
// PIC16Codegen.h's own header comment on why this core, like the
// 8080, has no hardware overflow flag to use instead): flipping just
// the top bit of each operand's high byte maps the signed range onto
// the unsigned one while preserving order, so a plain unsigned
// subtract-and-check-the-final-borrow (sub16()'s own STATUS<C>) is
// all that's left to do.
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

std::string PIC16Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the pic16 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'hex' is)");
}

std::optional<std::string> PIC16Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "hex") return assemblePIC16(asmText);
    // `coff`/`elf`/`aout`/`objraw`: real separate compilation (see
    // generate()'s own `objectMode` comment) - this backend's own
    // assembler builds one format-neutral ObjectFile exactly once
    // (see PIC16Assembler.h's own `assemblePIC16Object`), and
    // whichever of these four names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePIC16Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
