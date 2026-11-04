#include "PIC18Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "PIC18Assembler.h"
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
}  // namespace

void PIC18Codegen::declareRam(const std::string &lbl, int count, bool zeroAtStart) {
    ramDecls_.push_back(lbl + ": res " + std::to_string(count));
    if (zeroAtStart) zeroAtStart_.push_back({lbl, count});
}

void PIC18Codegen::copy2(const std::string &dst, const std::string &src) {
    if (dst == src) return;
    code_ << "    movff " << src << "," << dst << "\n";
    code_ << "    movff " << src << "+1," << dst << "+1\n";
}

void PIC18Codegen::clear2(const std::string &lbl) { code_ << "    clrf " << lbl << "\n    clrf " << lbl << "+1\n"; }

void PIC18Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    bcf STATUS,0\n";
    if (isLeft) {
        code_ << "    rlcf " << lbl << ",f\n";
        code_ << "    rlcf " << lbl << "+1,f\n";
    } else {
        code_ << "    rrcf " << lbl << "+1,f\n";
        code_ << "    rrcf " << lbl << ",f\n";
    }
}

// dst = a + b via ADDWFC's own real carry chain - see PIC18Codegen.h's
// own header comment on why this needs no PIC16-style manual fixup.
void PIC18Codegen::add16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    addwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    addwfc " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

// dst = a - b via SUBWFB's own real borrow chain.
void PIC18Codegen::sub16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    subwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    subwfb " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

// Size, in bytes, of `FSTACK` - see PIC18Codegen.h's own header
// comment on the real trade-off this constant is: more headroom for
// simultaneous call frames (deeper recursion before this backend's
// own frame region runs out, rather than the real 31-level hardware
// call stack) against fewer bytes left over, in this chip's own 96-
// byte Access Bank window, for every linked program's own globals.
constexpr int kFrameStackBytes = 48;

// Saves the caller's own `FP` (untouched by the real hardware `call`
// that just landed here) at `NFP-1`, then adopts `NFP` - the address
// the caller already computed for this call's own frame, immediately
// before `call` - as this call's own `FP`. See PIC18Codegen.h's own
// header comment.
void PIC18Codegen::emitPrologue() {
    code_ << "    movf NFP,w\n    addlw 255\n    movwf ptr\n";  // ptr = NFP-1 (255 == -1 mod 256)
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf FP,w\n    movwf INDF0\n";  // [NFP-1] = caller's own FP
    code_ << "    movf NFP,w\n    movwf FP\n";
}

// The reverse: restores the caller's own `FP` from `FP-1`, right
// before falling through to the real hardware `return`.
void PIC18Codegen::emitEpilogue() {
    code_ << "    movf FP,w\n    addlw 255\n    movwf ptr\n";  // ptr = FP-1
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf INDF0,w\n    movwf FP\n";
}

void PIC18Codegen::slotAddrToPtr(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    movlw " << label(slot) << "\n    movwf ptr\n";
        return;
    }
    int off = slotFrameOffsetBytes(slot);
    code_ << "    movf FP,w\n";
    if (off != 0) code_ << "    addlw " << off << "\n";
    code_ << "    movwf ptr\n";
}

void PIC18Codegen::loadSlotToAc(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        copy2("ac", label(slot));
        return;
    }
    slotAddrToPtr(slot);
    loadAcIndirect();
}

void PIC18Codegen::storeAcToSlot(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        copy2(label(slot), "ac");
        return;
    }
    slotAddrToPtr(slot);
    storeAcIndirect();
}

void PIC18Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    incfsz " << lbl << ",f\n";
    code_ << "    goto " << Lskip << "\n";
    code_ << "    incf " << lbl << "+1,f\n";
    code_ << Lskip << ":\n";
}

void PIC18Codegen::testAcZero() { code_ << "    movf ac,w\n    iorwf ac+1,w\n"; }

void PIC18Codegen::gotoIfZero(const std::string &target) {
    code_ << "    btfsc STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

void PIC18Codegen::gotoIfNonZero(const std::string &target) {
    code_ << "    btfss STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

// Via FSR0L/INDF0/POSTINC0 - one real instruction fewer than PIC16's
// own FSR/INDF-plus-explicit-INCF pair (see PIC18Codegen.h's own
// header comment): POSTINC0 reads/writes through FSR0 and then
// auto-increments it in the same instruction.
void PIC18Codegen::storeAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf ac,w\n    movwf POSTINC0\n";
    code_ << "    movf ac+1,w\n    movwf INDF0\n";
}

void PIC18Codegen::loadAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf POSTINC0,w\n    movwf ac\n";
    code_ << "    movf INDF0,w\n    movwf ac+1\n";
}

// Real runtime push/pop on the shared `SSTACK` region via `SP`/FSR0 -
// see PIC18Codegen.h's own header comment on why this has to be a
// genuine stack, not a compile-time-indexed fixed cell. POSTINC0
// (access, then increment) is a natural push. There's no hardware
// pre-decrement mode on this core to mirror it for pop (PIC18 only
// has INDFn/POSTINCn/POSTDECn/PREINCn/PLUSWn - no PREDECn), so pop
// instead decrements FSR0L explicitly (DECF) before each plain INDF0
// access - two real instructions instead of one, but still correct.
// `SP` is kept in sync with FSR0L itself by simply copying it back
// out afterward.
void PIC18Codegen::pushCell2(const std::string &src) {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    movf " << src << ",w\n    movwf POSTINC0\n";
    code_ << "    movf " << src << "+1,w\n    movwf POSTINC0\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::popCell2(const std::string &dst) {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf " << dst << "+1\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf " << dst << "\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::pushPtrCell() {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    movf ptr,w\n    movwf POSTINC0\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::popPtrCell() {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf ptr\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

std::string PIC18Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // `objectMode` is false (see Codegen.h's own header comment), so
    // `mainFn` is only ever null in object mode, for a file that
    // doesn't happen to define `main` itself.

    if (objectMode) {
        // Plain compile-time equates, identical in every object and in
        // PIC18LinkerFactory.cpp's own entry object - see
        // PIC18Codegen.h's own `objectMode` comment and
        // PIC18Assembler.h's own `kPIC18DataBaseAddress` comment for
        // why these specific addresses (and why no relocation is
        // needed for them at all).
        code_ << "ac = 0\nbc = 2\ncx = 4\ndx = 6\nptr = 8\nSP = 9\nSSTACK = 10\nFP = 34\nNFP = 35\nFSTACK = 36\n";
    } else {
        declareRam("ac", 2, false);
        declareRam("bc", 2, false);
        declareRam("cx", 2, false);
        declareRam("dx", 2, false);
        declareRam("ptr", 1, false);
        declareRam("SP", 1, false);
        declareRam("SSTACK", 24, false);
        declareRam("FP", 1, false);
        declareRam("NFP", 1, false);
        declareRam("FSTACK", kFrameStackBytes, false);
    }

    std::ostringstream globalInit;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareRam(lbl, slot->arraySize * 2, false);
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                std::string base = lbl + "+" + std::to_string(i * 2);
                std::string hi = lbl + "+" + std::to_string(i * 2 + 1);
                emitInitElem(globalInit, base, hi, slot->arrayInit[i]);
            }
        } else {
            declareRam(lbl, 2, false);
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            emitInitByte(globalInit, lbl, static_cast<uint8_t>(v16 & 0xFF));
            emitInitByte(globalInit, lbl + "+1", static_cast<uint8_t>((v16 >> 8) & 0xFF));
        }
    }

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // This file's own globals' init code has nowhere else to
            // run in object mode - see PIC18Codegen.h's own
            // `objectMode` comment for why `fn_main`'s own prologue is
            // the one place every linked program is guaranteed to
            // reach exactly once, before any other code, that this
            // file *does* control.
            genFunction(fb, globalInit.str());
        } else {
            genFunction(fb);
        }
    }

    std::ostringstream header;
    // Access Bank SFR short addresses (see PIC18Codegen.h's own
    // header comment - this is the real PIC18F2420 SFR map's own
    // bottom byte, the exact form every instruction here other than
    // MOVFF resolves via the Access Bank's own 0x60-0xFF->0xF60-0xFFF
    // hardware aliasing).
    header << "STATUS = 0xD8\nFSR0L = 0xE9\nFSR0H = 0xEA\nPOSTINC0 = 0xEE\nINDF0 = 0xEF\nINTCON = 0xF2\n";
    // PCL/PCLATH/PCLATU/STKPTR/TOSL/TOSH/TOSU - the real PIC18 core's
    // own program-counter-latch and top-of-return-stack registers
    // (0xFF9-0xFFF, standardized identically across the whole PIC18
    // family, not a per-chip peripheral address) - see genExpr's own
    // CallPtr case for what these are for.
    header << "PCL = 0xF9\nPCLATH = 0xFA\nPCLATU = 0xFB\nSTKPTR = 0xFC\nTOSL = 0xFD\nTOSH = 0xFE\nTOSU = 0xFF\n\n";
    if (objectMode) {
        // No entry sequence here at all - see PIC18Codegen.h's own
        // `objectMode` comment: PIC18LinkerFactory.cpp's own
        // synthesized entry object is solely responsible for the real
        // hardware reset vector, `FSR0H`/`SP` setup, and `call
        // fn_main`/halt, once every object is merged.
        header << "org 0\n\n";
    } else {
        header << "org 0\n    goto START\n\n";
        header << "START:\n";
        header << "    clrf FSR0H\n";  // FSR0 only ever needs its low byte set - see PIC18Codegen.h's own header
                                        // comment (this target's own data is restricted to the Access Bank's own
                                        // low byte, so the high byte of a real pointer never has to change)
        header << "    movlw SSTACK\n    movwf SP\n";
        for (const auto &entry : zeroAtStart_) {
            for (int i = 0; i < entry.second; i++) {
                header << "    clrf " << entry.first;
                if (i > 0) header << "+" << i;
                header << "\n";
            }
        }
        header << globalInit.str();
        // `fn_main`'s own prologue expects `NFP` to already hold its
        // new frame's own address (see this file's own header comment
        // on `FP`/`NFP`) - `FSTACK+1` (not `FSTACK` itself) leaves one
        // byte at `FSTACK+0` for that same prologue's own "caller's
        // FP" save, never read back meaningfully since `main` has no
        // real caller to return `FP` to in whole-program mode.
        header << "    movlw FSTACK+1\n    movwf NFP\n";
        header << "    call " << label(mainFn) << "\n";
        header << "HALT:\n    goto HALT\n\n";
    }

    std::ostringstream out;
    out << header.str();
    out << code_.str();
    out << "\norgram 0x00\n";
    for (const auto &l : ramDecls_) out << l << "\n";
    return out.str();
}

std::map<std::string, std::string> PIC18Codegen::perFileAsm() const { return perFileAsm_; }

void PIC18Codegen::genFunction(const ir::FunctionBody &fn, const std::string &extraPrologue) {
    paramCount_ = static_cast<int>(fn.func->params.size());
    frameSizeBytes_ = (paramCount_ + fn.func->frameSize) * 2;
    code_ << label(fn.func) << ":\n";
    emitPrologue();
    code_ << extraPrologue;
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "    return\n\n";
}

void PIC18Codegen::genStmt(const ir::Stmt &s) {
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
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            emitEpilogue();
            code_ << "    return\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // Space for every Local is already accounted for by
            // `Slot::frameOffset`/`Function::frameSize` (IR.h) - this
            // only ever has an initializer left to emit, for a
            // scalar; an array local has none.
            if (s.declSlot->arraySize == 0 && s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeAcToSlot(s.declSlot);
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

void PIC18Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movlw " << (v16 & 0xFF) << "\n    movwf ac\n";
            code_ << "    movlw " << ((v16 >> 8) & 0xFF) << "\n    movwf ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotToAc(e.slot);
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
            storeAcToSlot(e.slot);
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
            // The callee's own new frame starts right after this
            // function's own ("`FP + frameSizeBytes_`"), with one more
            // byte set aside first for the callee's own saved-FP slot
            // - see this file's own header comment on `FP`/`NFP`.
            // `frameSizeBytes_` is this function's own compile-time
            // constant throughout its whole codegen, so this offset
            // from the *current* `FP` is correct at whatever call
            // depth this call site actually runs at, recursive or
            // not.
            int newFrameOff = frameSizeBytes_ + 1;
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);  // ac = this argument's value
                // ptr = FP + newFrameOff + i*2 - recomputed fresh for
                // every argument, after evaluating it (which may
                // itself call through its own complete frame, but
                // always restores `FP` first) rather than carried
                // across more than one argument's own evaluation.
                code_ << "    movf FP,w\n    addlw " << (newFrameOff + static_cast<int>(i) * 2) << "\n";
                code_ << "    movwf ptr\n";
                storeAcIndirect();
            }
            // NFP = FP + newFrameOff - handed to the callee's own
            // prologue, recomputed fresh (not reused from any earlier
            // value) immediately before `call` so no argument's own
            // evaluation above can have clobbered it.
            code_ << "    movf FP,w\n    addlw " << newFrameOff << "\n    movwf NFP\n";
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // Real PIC18F2420 has no indirect-CALL instruction at all
            // (GOTO/CALL always encode their own 20-bit target
            // directly in the instruction word) and doesn't enable
            // the optional Extended Instruction Set's CALLW either
            // (see this file's own header comment on the classic,
            // non-enhanced core this backend targets) - so this is
            // the real mechanism actual PIC18 C compilers use on a
            // core without CALLW: PUSH reserves a fresh slot on the
            // real hardware return stack, then that slot's own three
            // bytes - TOSU/TOSH/TOSL, directly addressable SFRs, the
            // same as any other - are overwritten with the REAL
            // desired return point, a symbolic label the assembler
            // resolves on its own, never a hand-counted offset (that
            // was the one real risk here - getting "how many
            // instructions until the jump" wrong by hand would
            // corrupt the return-address stack silently rather than
            // failing loudly; a label removes the question entirely).
            // PCLATU/PCLATH/PCL are then loaded from the callee's own
            // computed address, PCL last - writing it is what
            // actually latches PCLATH/PCLATU into the real PC and
            // jumps.
            genExpr(*e.lhs);  // ac/ac+1 = the callee's own address
            // Same NFP handoff the direct Call case above gives every
            // callee - IRBuilder.cpp already enforces 0 arguments for
            // this kind (see its own comment on ir::ExprKind::CallPtr),
            // so there's no argument-copying loop to mirror, but the
            // callee still needs its own frame to start in the right
            // place. Recomputed fresh here, after evaluating the
            // callee (which may itself call through a normal frame),
            // same as the direct Call case's own reasoning for why.
            int newFrameOff = frameSizeBytes_ + 1;
            code_ << "    movf FP,w\n    addlw " << newFrameOff << "\n    movwf NFP\n";
            std::string Lret = newLabel();
            code_ << "    push\n";
            code_ << "    movlwcl " << Lret << "\n    movwf TOSL\n";
            code_ << "    movlwch " << Lret << "\n    movwf TOSH\n";
            code_ << "    clrf TOSU\n";
            code_ << "    clrf PCLATU\n";
            code_ << "    movf ac+1,w\n    movwf PCLATH\n";
            code_ << "    movf ac,w\n    movwf PCL\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real PIC18 global-interrupt enable/disable - INTCON<7>
            // (GIE), the exact same bit position and mechanism
            // PIC16Codegen.cpp's own _ion()/_iof() uses.
            code_ << "    " << (e.op == "ion" ? "bsf" : "bcf") << " INTCON,7\n";
            return;
        }
    }
}

void PIC18Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    shiftOneBit("ac", /*isLeft=*/true);  // ac *= 2
    if (slot->arraySize > 0) {
        // `slot` is itself the array - its own base address is a
        // fixed, compile-time label for a Global, or `FP` plus this
        // function's own compile-time offset for it for a Local -
        // computed into `bc` (not `ac`, which still holds `index*2`
        // here) either way.
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    movlw " << label(slot) << "\n    movwf bc\n";
        } else {
            code_ << "    movf FP,w\n";
            int off = slotFrameOffsetBytes(slot);
            if (off != 0) code_ << "    addlw " << off << "\n";
            code_ << "    movwf bc\n";
        }
    } else {
        // `slot` holds a pointer *value* - save `index*2` (still in
        // `ac`) in `bc` before loadSlotToAc() overwrites `ac` with
        // that value.
        code_ << "    movf ac,w\n    movwf bc\n";
        loadSlotToAc(slot);  // ac = the pointer's own current value
    }
    code_ << "    movf bc,w\n    addwf ac,w\n    movwf ptr\n";
}

void PIC18Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadAcIndirect();
}

// A literal element's own value, split into two compile-time-known
// bytes and loaded via emitInitByte()'s usual clrf/movlw+movwf
// choice; a symbol element (a dispatch-table address-of-function/
// global, see IR.h's ArrayInitElem) has no compile-time byte value at
// all - a Function's own real program-memory address is split low
// byte/high byte the same way genAddrOf()'s own Func case does
// (`movlwcl`/`movlwch`), and a Slot's own one-byte RAM address is
// loaded with a zero high byte, the same way genAddrOf()'s own Var
// case does.
void PIC18Codegen::emitInitElem(std::ostream &out, const std::string &loLbl, const std::string &hiLbl,
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

void PIC18Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    movlw " << label(operand.slot) << "\n    movwf ac\n";
        } else {
            // A Local/Param's own address is `FP` plus this
            // function's own compile-time offset for it - a real
            // runtime value (`FP` itself varies call to call), unlike
            // a Global's own fixed label above.
            code_ << "    movf FP,w\n";
            int off = slotFrameOffsetBytes(operand.slot);
            if (off != 0) code_ << "    addlw " << off << "\n";
            code_ << "    movwf ac\n";
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
        // `ac` ends up holding `fn`'s own real program-memory *byte*
        // address, split low byte/high byte across two literal loads
        // (see PIC18Assembler.cpp's own `movlwcl`/`movlwch` comment) -
        // GOTO/CALL always encode their own target directly in the
        // instruction word, so there's no single real instruction for
        // taking a function's address as plain data either. See
        // genExpr's own CallPtr case for calling through a value in
        // exactly this form.
        code_ << "    movlwcl " << label(operand.func) << "\n    movwf ac\n";
        code_ << "    movlwch " << label(operand.func) << "\n    movwf ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void PIC18Codegen::genBinary(const ir::Expr &e) {
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

void PIC18Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    code_ << "    movf ac,w\n    movwf cx\n";
    popCell2("ac");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    shiftOneBit("ac", isLeft);
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("dx", "ac");
    popCell2("bc");
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

void PIC18Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("bc", "ac");
    popCell2("dx");
    clear2("ac");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    sub16("cx", "dx", "bc");
    code_ << "    btfss STATUS,0\n    goto " << Ldone << "\n";
    copy2("dx", "cx");
    inc16("ac");
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy2("ac", "dx");
}

void PIC18Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void PIC18Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
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

// Signed 16-bit "a < b" via the real N-xor-OV rule (see
// PIC18Codegen.h's own header comment on why this core, unlike
// PIC16's, has a real hardware overflow flag to use instead of a
// sign-bit-bias trick): after sub16()'s own final SUBWFB, "a < b"
// holds iff STATUS<N> and STATUS<OV> disagree.
void PIC18Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    pushCell2("ac");
    genExpr(b);
    popCell2("bc");  // bc = a, ac = b
    sub16("cx", "bc", "ac");  // cx = a - b; STATUS<N,OV> reflect the signed result
    std::string LovSet = newLabel(), Lless = newLabel(), Lnotless = newLabel(), Ldone = newLabel();
    code_ << "    btfsc STATUS,3\n    goto " << LovSet << "\n";
    code_ << "    btfsc STATUS,4\n    goto " << Lless << "\n";
    code_ << "    goto " << Lnotless << "\n";
    code_ << LovSet << ":\n";
    code_ << "    btfss STATUS,4\n    goto " << Lless << "\n";
    code_ << "    goto " << Lnotless << "\n";
    code_ << Lless << ":\n";
    if (!invert) {
        code_ << "    movlw 1\n    movwf ac\n    clrf ac+1\n";
    } else {
        clear2("ac");
    }
    code_ << "    goto " << Ldone << "\n";
    code_ << Lnotless << ":\n";
    if (!invert) {
        clear2("ac");
    } else {
        code_ << "    movlw 1\n    movwf ac\n    clrf ac+1\n";
    }
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void PIC18Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void PIC18Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testAcZero();
    std::string Ltrue = newLabel(), Ldone = newLabel();
    gotoIfZero(Ltrue);
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

std::string PIC18Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the pic18 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (see that function's own comment)");
}

std::optional<std::string> PIC18Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "hex") return assemblePIC18(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see PIC18Assembler.h's own `assemblePIC18Object`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePIC18Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
