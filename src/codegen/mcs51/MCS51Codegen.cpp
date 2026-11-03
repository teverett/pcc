#include "MCS51Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "MCS51Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}
}  // namespace

void MCS51Codegen::allocXdata(const std::string &lbl, int count) {
    // `lbl` and the `xdata` statement must share one physical line -
    // the shared ../../../grammar/PccAsm.g4 only ever attaches a
    // label to a statement appearing on that exact same line (`line :
    // label* statement? NEWLINE`), same as every other label this
    // backend ever emits.
    code_ << lbl << ": xdata " << count << "\n";
}

void MCS51Codegen::declareTemp(const std::string &lbl) { allocXdata(lbl, 2); }

void MCS51Codegen::declareGlobalTemp(const std::string &lbl, long initValue) {
    allocXdata(lbl, 2);
    uint16_t v16 = static_cast<uint16_t>(initValue);
    initCode_ << "    movdptr " << lbl << "\n";
    initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
    initCode_ << "    incdptr\n";
    initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
}

// Local arrays are never given an `arrayInit` (see IR.h's own
// Slot::arrayInit comment: "locals are zero-filled... assigned at
// runtime by ordinary statements") - this just reserves `arraySize`
// words of XDATA, which real hardware RAM doesn't actually guarantee
// starts zeroed, but this backend's own test simulator (see
// tests/run_tests.sh's own check_8051) does, matching every other
// backend's own "freshly declared local reads as 0 until assigned"
// assumption.
void MCS51Codegen::declareArray(const std::string &lbl, int arraySize) { allocXdata(lbl, arraySize * 2); }

// Writes array element `e`'s own value into the already-allocated
// XDATA cell at `elemExpr`. A literal element is written byte-by-byte
// through a plain immediate (`movai`), same as declareGlobalTemp()'s
// own scalar case. A symbol element (a dispatch-table address-of-
// function/global, see IR.h's ArrayInitElem) has no compile-time byte
// value at all - its own address is loaded into `bc` first, through
// the same `dpl`/`dph` split loadCodeAddress()/loadXdataAddress()
// already use (a Function's address against the CODE label table via
// `movdptrc`, a Slot's against the XDATA one via `movdptr` - the two
// address spaces this target keeps genuinely separate), then written
// out through `bc` exactly like storeXdata() writes any other
// runtime-computed value.
void MCS51Codegen::genArrayElementInit(const std::string &elemExpr, const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        if (e.func) {
            initCode_ << "    movdptrc " << label(e.func) << "\n";
        } else {
            initCode_ << "    movdptr " << label(e.slot) << "\n";
        }
        initCode_ << "    mova dpl\n    movd bc\n";
        initCode_ << "    mova dph\n    movd bc+1\n";
        initCode_ << "    movdptr " << elemExpr << "\n";
        initCode_ << "    mova bc\n    movxw\n";
        initCode_ << "    incdptr\n";
        initCode_ << "    mova bc+1\n    movxw\n";
        return;
    }
    uint16_t v16 = static_cast<uint16_t>(e.value);
    initCode_ << "    movdptr " << elemExpr << "\n";
    initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
    initCode_ << "    incdptr\n";
    initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
}

void MCS51Codegen::declareGlobalArray(const std::string &lbl, int arraySize,
                                       const std::vector<ir::ArrayInitElem> &values) {
    allocXdata(lbl, arraySize * 2);
    for (int i = 0; i < arraySize; i++) {
        // Element `i`'s own `movdptr` operand - `lbl` itself for the
        // first element, `lbl+<byte offset>` otherwise: this
        // assembler's own expression grammar resolves the symbol part
        // against its XDATA label table and folds the constant offset
        // in alongside it, so the whole element address stays one
        // relocatable expression (see MCS51Assembler.h's own header
        // comment) rather than something this function has to resolve
        // to a number itself.
        std::string elemExpr = (i == 0) ? lbl : lbl + "+" + std::to_string(i * 2);
        static const ir::ArrayInitElem kZero{};
        const ir::ArrayInitElem &e = (static_cast<size_t>(i) < values.size()) ? values[i] : kZero;
        genArrayElementInit(elemExpr, e);
    }
}

void MCS51Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    movdd " << dst << ", " << src << "\n";
    code_ << "    movdd " << dst << "+1, " << src << "+1\n";
}

void MCS51Codegen::loadXdata(const std::string &dstReg, const std::string &xdataExpr) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    movxr\n    movd " << dstReg << "\n";
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::storeXdata(const std::string &xdataExpr, const std::string &srcReg) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    mova " << srcReg << "\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova " << srcReg << "+1\n    movxw\n";
}

void MCS51Codegen::loadXdataAddress(const std::string &dstReg, const std::string &xdataExpr) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    mova dpl\n    movd " << dstReg << "\n";
    code_ << "    mova dph\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::loadCodeAddress(const std::string &dstReg, const std::string &codeExpr) {
    code_ << "    movdptrc " << codeExpr << "\n";
    code_ << "    mova dpl\n    movd " << dstReg << "\n";
    code_ << "    mova dph\n    movd " << dstReg << "+1\n";
}

// Loads DPTR from `ptr` (the only pseudo-register this ever needs to
// happen for - every other XDATA access already knows its own
// address at compile time, via loadXdata()/storeXdata()'s own plain
// numeric operand) and writes `ac`'s two bytes there, one MOVX at a
// time - doesn't disturb `ptr` itself.
void MCS51Codegen::storeAcIndirect() {
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    mova ac\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova ac+1\n    movxw\n";
}

void MCS51Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    ljmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfTaken << "\n";
}

void MCS51Codegen::safeDjnz(const std::string &counterReg, const std::string &targetIfNonzero,
                             const std::string &targetIfZero) {
    std::string Lnear = newLabel();
    code_ << "    djnz " << counterReg << ", " << Lnear << "\n";
    code_ << "    ljmp " << targetIfZero << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfNonzero << "\n";
}

std::string MCS51Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (i.e. whenever `objectMode` is false
    // here too - see Codegen.h's own header comment), so mainFn is
    // only ever null in object mode, for a file that doesn't happen
    // to define `main` itself.

    // Pre-pass: every global and every function's own parameters get
    // their XDATA address assigned up front, before any code is
    // generated at all - see allocXdata()'s own header comment on
    // why a Call site or Var read needs that address to already
    // exist no matter which function is textually processed first
    // below (unlike 6502's own trailer, which is just ordinary
    // assembler-resolved CODE-space text and so never has this
    // problem - see MCS51Assembler.h's own header comment).
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - see generate()'s own
        // `objectMode` comment in MCS51Codegen.h for why this
        // backend's Harvard CODE/XDATA split leaves no way to make
        // one visible across files the way a function already is;
        // skipping storage here at least keeps this file's own
        // XDATA layout from wasting space on it.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareGlobalArray(lbl, slot->arraySize, slot->arrayInit);
        } else {
            declareGlobalTemp(lbl, slot->hasInit ? slot->initValue : 0);
        }
    }
    for (const auto &fn : prog.functions) {
        for (ir::Slot *p : fn->params) declareTemp(label(p));
    }

    code_ << "org " << kMCS51BaseAddress << "\n";
    // The fixed internal-RAM pseudo-registers - see MCS51Codegen.h's
    // own header comment for what each one is for. 0x30 sits safely
    // above the 4 register banks (0x00-0x1F) and the bit-addressable
    // area (0x20-0x2F), and comfortably below the 128-byte floor
    // every one of 8031/8032/8051/8052 guarantees. Every object
    // declares these identically and harmlessly (a plain compile-time
    // equate, the same in every object regardless of where it's
    // linked - see MCS51LinkerFactory.cpp's own entry object, which
    // redeclares them too), so no `objectMode` special-casing is
    // needed here at all - same reasoning as
    // ../i8008/I8008Codegen.cpp's own equivalent constants. `dpl`/
    // `dph` are the real DPL/DPH SFRs (0x82/0x83), not invented
    // scratch cells - see this file's own header comment on why
    // loadXdataAddress() needs plain `direct`-addressable names for
    // them.
    code_ << "ac = 0x30\nbc = 0x32\ncx = 0x34\ndx = 0x36\nptr = 0x38\ndpl = 0x82\ndph = 0x83\n\n";

    // `objectMode` means no object file ever emits the entry sequence
    // itself, even the one that happens to define `main` -
    // MCS51LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `main`'s linked
    // address is resolvable (see MCS51Codegen.h's own `objectMode`
    // comment above).
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << initCode_.str();
        code_ << "    lcall " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` (XDATA's own 16-
        // bit result doesn't fit any real register, same reason every
        // other memory-cell-based backend's own header comment gives) the
        // moment execution reaches `halt` below - the authoritative place
        // to read it from. This extra `mova` is a minor nicety only: it
        // leaves the low byte sitting in the real accumulator too, for a
        // quick look with nothing more than a register dump.
        code_ << "    mova ac\n";
        code_ << "halt:\n    ljmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // This file's own globals' init code (see
            // declareGlobalTemp()'s own comment) has nowhere else to
            // run in object mode: MCS51LinkerFactory.cpp's own
            // synthesized `_start` is built once, before it has ever
            // seen this (or any) user object, so it can't know this
            // file's own `initCode_` exists at all. `fn_main`'s own
            // prologue is the one place every linked program is
            // guaranteed to reach exactly once, before any other
            // code, that this file *does* control - see
            // MCS51Codegen.h's own `objectMode` comment for why only
            // the file defining `main` gets this treatment.
            code_ << label(fb.func) << ":\n";
            code_ << initCode_.str();
            genStmt(*fb.body);
            code_ << "    ret\n\n";
        } else {
            genFunction(fb);
        }
    }

    return code_.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no multi-input-file splitting of one combined dump).
std::map<std::string, std::string> MCS51Codegen::perFileAsm() const { return perFileAsm_; }

void MCS51Codegen::genFunction(const ir::FunctionBody &fn) {
    // Params' own XDATA storage is already allocated by generate()'s
    // own pre-pass - nothing left to do here but emit the body.
    code_ << label(fn.func) << ":\n";
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void MCS51Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    mova ac\n    orl ac+1\n";
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("jz", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    ljmp " << Ldone << "\n";
            if (s.elseStmt) {
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    mova ac\n    orl ac+1\n";
            safeBranch("jz", Lend, Lbody);
            code_ << Lbody << ":\n";
            genStmt(*s.loopBody);
            code_ << "    ljmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareArray(lbl, s.declSlot->arraySize);
            } else {
                declareTemp(lbl);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    storeXdata(lbl, "ac");
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

void MCS51Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movai " << (v16 & 0xFF) << "\n    movd ac\n";
            code_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movd ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadXdata("ac", label(e.slot));
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target XDATA address
                // Save `ptr` across evaluating rhs (which might
                // itself index through `ptr` again) on the real
                // hardware stack - a real 8051 `PUSH`/`POP direct`
                // moves one memory byte at a time, no accumulator
                // detour needed (unlike every memory-cell-based
                // backend that only has an implied-accumulator push).
                code_ << "    push ptr\n    push ptr+1\n";
                genExpr(*e.rhs);
                code_ << "    pop ptr+1\n    pop ptr\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            storeXdata(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    push ptr\n    push ptr+1\n";
            genExpr(*e.rhs);
            code_ << "    pop ptr+1\n    pop ptr\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    movai 0\n    clrc\n    subb ac\n    movd ac\n";
                code_ << "    movai 0\n    subb ac+1\n    movd ac+1\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    movdpl ptr\n    movdph ptr+1\n";
                code_ << "    movxr\n    movd ac\n";
                code_ << "    incdptr\n";
                code_ << "    movxr\n    movd ac+1\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mova ac\n    xrli 255\n    movd ac\n";
                code_ << "    mova ac+1\n    xrli 255\n    movd ac+1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // An argument is passed by writing it straight into the
            // callee's own `p_<name>_<i>` XDATA slot (allocated by
            // generate()'s own pre-pass) - fine as long as this file
            // is the one that actually defines the callee, since only
            // then does that pre-pass know the callee's own real
            // param list at all. An external callee (real separate
            // compilation - see IR.h's own `isExternal`) is a bare
            // function *name* with no param list this file ever sees
            // (IRBuilder never populates one for it), so there's no
            // slot to even compute an address for - and unlike a
            // function's own CODE-space address (a real relocatable
            // symbol, see MCS51Assembler.h's own `kMCS51RelocAbs16`),
            // an XDATA slot address is always a plain per-compile
            // numeric constant this backend's Harvard CODE/XDATA split
            // gives no relocation for at all (see MCS51Codegen.h's own
            // `objectMode` comment) - passing an argument across that
            // boundary genuinely can't be made to work here. A
            // parameterless external call has no such slot to write
            // at all, so it's unaffected.
            if (e.func->isExternal && !e.args.empty()) {
                err(e.line, "call to '" + e.func->sourceName + "' needs " + std::to_string(e.args.size()) +
                                " argument(s), but '" + e.func->sourceName + "' isn't defined in this file - the "
                                "8051 backend can only pass arguments to a function defined in the same file (see "
                                "MCS51Codegen.h's own header comment on this target's Harvard CODE/XDATA split)");
            }
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                storeXdata("p_" + e.func->sourceName + "_" + std::to_string(i), "ac");
            }
            code_ << "    lcall " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> XDATA slot to write one into until the callee
            // is known, which it never is at compile time for this
            // kind.
            //
            // The real 8051 has no indirect CALL at all - only `JMP
            // @A+DPTR`, a real indirect *jump* (address = DPTR + A).
            // A call through a computed address is faked by manually
            // reproducing LCALL's own real push order onto the
            // hardware stack - low return-address byte pushed first,
            // high byte second, via two 8-bit PUSH-direct instructions
            // (the real SP/PUSH/POP only ever move one internal-RAM
            // byte at a time - see MCS51Codegen.h's own header
            // comment) - before jumping, so that the callee's own
            // `ret` (RET pops high then low, the exact mirror order)
            // returns here exactly as if a real LCALL had reached it.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string ret = newLabel();
            loadCodeAddress("bc", ret);  // bc = this call's own return address
            code_ << "    push bc\n    push bc+1\n";
            code_ << "    movdpl ac\n    movdph ac+1\n";  // dptr = callee address
            code_ << "    clra\n";                        // a = 0, so @A+DPTR == DPTR exactly
            code_ << "    jmpadptr\n";
            code_ << ret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 8051 global interrupt enable/disable - `SETB EA`
            // (IE.7) turns every interrupt source on, `CLR EA` turns
            // them all off - same real hardware feature, and the
            // same "ion"/"iof" IR naming, as the 6502/8080 backends'
            // own `cli`/`sei` and `ei`/`di` (see M6502Codegen.cpp's
            // own comment) - just the opposite mnemonic-to-meaning
            // mapping again (SETB *enables* here, the way CLI does on
            // 6502 but not on 8080).
            code_ << "    " << (e.op == "ion" ? "setea" : "clrea") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective XDATA address in `ptr`.
// `slot->arraySize > 0` means the base is this array's own relocatable
// label (loadXdataAddress() resolves and loads it); otherwise `slot`
// is a plain pointer variable, so the base instead comes from
// *reading* that variable's own stored value via loadXdata(). Either
// way the base ends up in `bc`, and the same 16-bit add then folds
// the (already-doubled) index into it.
void MCS51Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ac *= 2 (words are 2 bytes on this byte-addressed target).
    code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
    code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    if (slot->arraySize > 0) {
        loadXdataAddress("bc", label(slot));
    } else {
        loadXdata("bc", label(slot));
    }
    code_ << "    clrc\n    mova bc\n    add ac\n    movd ptr\n";
    code_ << "    mova bc+1\n    addc ac+1\n    movd ptr+1\n";
}

void MCS51Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    movxr\n    movd ac\n";
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd ac+1\n";
}

void MCS51Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadXdataAddress("ac", label(operand.slot));
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadCodeAddress("ac", label(operand.func));
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MCS51Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    mova bc\n    add ac\n    movd ac\n";
            code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
        } else {
            code_ << "    mova bc\n    clrc\n    subb ac\n    movd ac\n";
            code_ << "    mova bc+1\n    subb ac+1\n    movd ac+1\n";
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
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";
        const char *insn = (op == "&") ? "anl" : (op == "|") ? "orl" : "xrl";
        code_ << "    mova bc\n    " << insn << " ac\n    movd ac\n";
        code_ << "    mova bc+1\n    " << insn << " ac+1\n    movd ac+1\n";
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

// No variable-count shift instruction (`RLC A`/`RRC A` always rotate
// exactly one bit) - loop `rhs` times instead, via a real `DJNZ`
// (see safeDjnz()'s own comment) guarded by an upfront zero check
// (unlike 6502's own plain decrement-then-test loop, `DJNZ`
// decrements *before* testing, so a shift-by-zero has to be caught
// before ever entering the loop body, not after).
void MCS51Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    mova ac\n    movd cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pop bc+1\n    pop bc\n";
    copy16("ac", "bc");  // ac = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    mova cx\n";
    safeBranch("jz", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
        code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    clrc\n    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    }
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (the real `MUL AB` is only 8x8 ->
// 16, too narrow for this language's 16-bit `int` - see
// MCS51Codegen.h's own header comment): `bc` is the multiplicand
// (shifted left each iteration), `dx` the multiplier (shifted right,
// consumed one bit at a time), `ac` the accumulating product, `cx`
// the fixed 16-iteration counter. Only verified against non-negative
// operands.
void MCS51Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop bc+1\n    pop bc\n";                  // bc = multiplicand
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = product = 0
    code_ << "    movai 16\n    movd cx\n";
    std::string Lloop = newLabel(), Ladd = newLabel(), Lskipadd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mova dx\n    anli 1\n";
    safeBranch("jnz", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    mova bc\n    add ac\n    movd ac\n";
    code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    clrc\n    mova bc\n    rlca\n    movd bc\n";
    code_ << "    mova bc+1\n    rlca\n    movd bc+1\n";
    code_ << "    clrc\n    mova dx+1\n    rrca\n    movd dx+1\n";
    code_ << "    mova dx\n    rrca\n    movd dx\n";
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// Repeated subtraction (the real `DIV AB` is only 8/8 -> 8r8, same
// width problem as `MUL AB` above) - the same algorithm every other
// software-divmod backend here uses: `dx` is the running remainder
// (starts as the dividend, `bc` the fixed divisor), `ac` the
// quotient, counting how many times `bc` was subtracted out of `dx`
// before it would've gone negative - the tentative subtraction's low
// byte is stashed in `cx` (free for the duration of this routine)
// until it's known whether to actually commit it. Only verified
// against non-negative operands.
void MCS51Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop dx+1\n    pop dx\n";                  // dx = dividend (running remainder)
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    clrc\n    mova dx\n    subb bc\n    movd cx\n";
    code_ << "    mova dx+1\n    subb bc+1\n";
    safeBranch("jc", Llt, Lcommit);
    code_ << Lcommit << ":\n";
    code_ << "    movd dx+1\n    movdd dx, cx\n";
    code_ << "    mova ac\n    addi 1\n    movd ac\n";
    code_ << "    mova ac+1\n    addci 0\n    movd ac+1\n";
    code_ << "    ljmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    ljmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void MCS51Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to one "is a < b?" primitive
    // (emitLessCompare) by choosing which operand goes first and
    // whether to invert the final sense: a>b is b<a; a<=b is !(b<a);
    // a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b - `JZ`/`JNZ` test
// the accumulator directly on this target (no separate zero flag to
// consult), and the final `ORL A,direct` above it already leaves
// exactly "low result OR high result" sitting in A, zero iff both
// halves were zero.
void MCS51Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
    code_ << "    mova bc\n    clrc\n    subb ac\n    movd dx\n";
    code_ << "    mova bc+1\n    subb ac+1\n    orl dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "jz" : "jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction, the same "N xor V" idiom the
// 6502/8080 backends use (see M6502Codegen.cpp's own comment): after
// `a - b`'s final (high) byte SUBB, the accumulator's own sign bit
// (ACC.7) only reflects the *true* signed result once corrected for
// overflow (the real PSW.OV, which SUBB does set for signed
// over/underflow, exactly like 6502's V) - jumping over an `XRL
// A,#0x80` (flipping just the sign bit) when OV is clear, applying it
// when OV is set, fixes that up.
void MCS51Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(b);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = a
    code_ << "    mova bc\n    clrc\n    subb ac\n";
    code_ << "    mova bc+1\n    subb ac+1\n";
    std::string Lfixskip = newLabel();
    code_ << "    jnbov " << Lfixskip << "\n";
    code_ << "    xrli 128\n";
    code_ << Lfixskip << ":\n";
    // ACC.7 now correctly reflects "a < b" (set) / "a >= b" (clear).
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(invert ? "jnbacc7" : "jbacc7", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("jz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

std::string MCS51Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 8051 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MCS51Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleMCS51(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see MCS51Assembler.h's own `assembleMCS51Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see fileformat/FileFormat.h)
    // to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMCS51Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
