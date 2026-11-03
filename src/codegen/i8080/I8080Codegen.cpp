#include "I8080Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "I8080Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - low
// enough to leave plenty of RAM above it in a bare 64KB 8080 system,
// high enough to stay clear of 0x0000-0x00FF (conventionally the
// RST-vector/boot area on real 8080 hardware, even though nothing
// here ever uses interrupts or RST - see tests/run_tests.sh's own
// emulator setup, which just loads this binary here and sets PC to
// it directly, no boot ROM involved).
constexpr uint32_t kBaseAddress = 0x0100;
// The hardware stack (SP/PUSH/POP/CALL/RET) needs an explicit initial
// value - unlike the 6502 (whose stack page is architecturally fixed
// at $0100) the 8080's SP is simply undefined at reset. Comfortably
// above every label this backend could ever generate for an ordinary
// test-sized program, inside the 64KB RAM tests/run_tests.sh gives
// the emulator.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void I8080Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) own label - the referenced Slot's or Function's own
// label() text, exactly the shape `lxi`'s own 16-bit operand already
// takes (see genAddrOf()/genIndexAddr()). declareArray() below hands
// this straight to a `dw` directive rather than computing a numeric
// low/high split itself - I8080Assembler.cpp's own `emit16` already
// resolves a symbol's full 16-bit address once every label is bound,
// the same single evaluator call `lxi`'s own operand goes through.
std::string I8080Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void I8080Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
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
    // Assumes HL already holds the target address (see this
    // function's own declaration comment) - `lda`/`sta` address `ac`
    // directly, by name, never touching HL, so the caller's address
    // in HL survives across both byte stores.
    code_ << "    lda ac\n    mov m,a\n";
    code_ << "    inx h\n";
    code_ << "    lda ac+1\n    mov m,a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl`, used by
// genShift()'s own loop and inlined directly by genMul()/
// genDivMod() (see I8080Codegen.h's own header comment on why there's
// no variable-count shift instruction to use instead). `ana a` is
// this target's own idiom for "clear carry without otherwise
// disturbing A" (every logical instruction clears CY as a side
// effect, and ANA A's "AND A with itself" leaves A's value alone) -
// needed because the 8080, unlike the 6502, has no explicit "clear
// carry" instruction, and a left/right rotate always needs a known
// carry-in to shift a 0 into the vacated bit.
void I8080Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    lhld " << lbl << "\n";
    code_ << "    ana a\n";
    if (isLeft) {
        code_ << "    mov a,l\n    ral\n    mov l,a\n";
        code_ << "    mov a,h\n    ral\n    mov h,a\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    mov a,h\n    rar\n    mov h,a\n";
        code_ << "    mov a,l\n    rar\n    mov l,a\n";
    }
    code_ << "    shld " << lbl << "\n";
}

std::string I8080Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself, even the one that happens to define `main` -
    // I8080LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `main`'s linked
    // address is resolvable (see Linker.h) - same pattern
    // Z80Codegen.cpp's own `objectMode` comment documents. An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lxi sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra `lhld` leaves it in the real HL
        // pair too, for a quick look with nothing more than a
        // register dump.
        code_ << "    lhld ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    // The fixed memory pseudo-registers - see I8080Codegen.h's own
    // header comment for what each one is for. Every genExpr() call
    // anywhere in the *whole linked program* shares the same `ac`
    // (it's the one place a callee's own return value and a caller's
    // own next operand both pass through), so unlike every other
    // symbol this backend emits, these five can't each be redeclared
    // per-object: in whole-program mode they're declared once, right
    // here, as ordinary temps; in object mode, this file never
    // declares them at all - every reference anywhere in a compiled
    // object is then automatically deferred as an external
    // relocation, resolved against the one real definition
    // I8080LinkerFactory.cpp's own synthesized entry object provides
    // - see Z80Codegen.cpp's own `generate()` comment for the
    // original version of this same reasoning.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // The current function's own software frame pointer (see
        // I8080Codegen.h's own header comment) - shared the same way
        // as the five cells above, since a callee's own prologue and
        // its caller both need to agree on the one real place it
        // lives, exactly like `ac` does for a return value.
        declareTemp("fp");
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            trailer_.push_back(lbl + ": db " + std::to_string(v16 & 0xFF) + "  ; " + slot->sourceName);
            trailer_.push_back("db " + std::to_string((v16 >> 8) & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (see I8080Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> I8080Codegen::perFileAsm() const { return perFileAsm_; }

// Every activation gets its own real stack frame (see I8080Codegen.h's
// own header comment for the full layout): `lhld fp`/`push h` saves
// the caller's own frame pointer on the real hardware stack, `lxi
// h,0`/`dad sp` reads the current SP into HL (the standard 8080 idiom
// for that - there's no direct "HL=SP" instruction) right after that
// push, so it's exactly the address of the just-pushed word, and
// `shld fp` makes that this call's own new frame base. `dad d` (with
// the locals' own *negative* total byte size already in DE) then
// `sphl` moves SP down below them in one step, reserving that memory
// so nothing later in this function (a nested `push`, or a nested
// `call`'s own argument pushes) can ever land on top of it - the same
// role I8086Codegen.cpp's own `sub sp, N` plays, just computed by hand
// since the 8080 has no instruction that subtracts an immediate from
// SP directly.
void I8080Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    lhld fp\n    push h\n";
    code_ << "    lxi h, 0\n    dad sp\n    shld fp\n";
    if (fn.func->frameSize > 0) {
        code_ << "    lxi d, " << (-2L * fn.func->frameSize) << "\n    dad d\n    sphl\n";
    }
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

// See this file's own header comment above genFunction() for the
// prologue this undoes: `lhld fp`/`sphl` snaps SP back up to this
// call's own frame base (dropping the locals' reserved region in one
// instruction, regardless of how many words that was), `pop h` lifts
// the caller's own frame pointer back off the real hardware stack,
// `shld fp` restores it as the current `fp`, and `ret` returns to the
// caller exactly as it always did - the real hardware CALL/RET pair
// (and the real SP they already use) never needed any change of their
// own to be reentrant.
void I8080Codegen::emitEpilogue() {
    code_ << "    lhld fp\n    sphl\n";
    code_ << "    pop h\n    shld fp\n";
    code_ << "    ret\n";
}

// A Local or Param slot's own effective address, computed fresh every
// time rather than ever cached - see I8080Codegen.h's own header
// comment for why this target has no addressing mode that could do
// this as part of a single `mov`/`mvi`/etc. the way Z80's `(IX+d)` or
// 8086's `[bp+d]` can.
void I8080Codegen::genFrameAddr(const ir::Slot *slot) {
    code_ << "    lhld fp\n";
    code_ << "    lxi d, " << frameByteOffset(slot) << "\n";
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
            code_ << "    lhld ac\n    mov a,l\n    ora h\n";
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
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    lhld ac\n    mov a,l\n    ora h\n";
            code_ << "    jz " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // An early return bypasses genFunction()'s own fall-
            // through epilogue entirely, so it needs this same frame
            // teardown inline - see emitEpilogue()'s own comment.
            emitEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                genFrameAddr(s.declSlot);
                storeAcIndirect();
            }
            // No "else" case for a bare, uninitialized scalar: its own
            // frame word already exists (genFunction()'s own prologue
            // already reserved it) - same as a real, ordinary C
            // automatic variable, its value is simply whatever was
            // already in that stack slot until something assigns it.
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

void I8080Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    lxi h, " << v16 << "\n    shld ac\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16("ac", label(e.slot));
            } else {
                genFrameAddr(e.slot);
                code_ << "    mov e,m\n    inx h\n    mov d,m\n";
                storePair("ac", "d");
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
                code_ << "    lhld ptr\n    push h\n";
                genExpr(*e.rhs);
                code_ << "    pop h\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16(label(e.slot), "ac");
            } else {
                genFrameAddr(e.slot);
                storeAcIndirect();
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    lhld ptr\n    push h\n";
            genExpr(*e.rhs);
            code_ << "    pop h\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    lhld ac\n";
                code_ << "    xra a\n    sub l\n    mov l,a\n";
                code_ << "    mvi a, 0\n    sbb h\n    mov h,a\n";
                code_ << "    shld ac\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    lhld ptr\n";
                code_ << "    mov e,m\n    inx h\n    mov d,m\n";
                storePair("ac", "d");
            } else if (e.op == "~") {
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
            // Each argument is pushed onto the real hardware stack,
            // last-declared one first, so argument 0 ends up closest
            // to the return address `call` is about to push - the
            // address every Param already addresses itself by (see
            // I8080Codegen.h's own `frameByteOffset()`: paramIndex 0 is
            // `fp+4`, the lowest positive offset). The callee's own
            // prologue lays them into ITS OWN frame itself; nothing
            // here needs to know anything about the callee's
            // parameter count beyond `e.args.size()`. Each value is
            // computed into `ac` and then transited through HL (the
            // only way to get a memory value onto the real stack -
            // `PUSH` only ever takes a register pair, never a memory
            // operand).
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    lhld ac\n    push h\n";
            }
            code_ << "    call " << label(e.func) << "\n";
            // The caller cleans its own pushed arguments back off
            // afterward - cdecl's own convention, chosen here (same
            // as I8086Codegen.cpp's own `add sp, N`) only because it
            // needs no cooperation from the callee at all. `lxi
            // h,N`/`dad sp`/`sphl` moves SP by a compile-time-constant
            // amount in one step, the same idiom genFunction()'s own
            // prologue uses to reserve locals.
            if (!e.args.empty()) {
                code_ << "    lxi h, " << (e.args.size() * 2) << "\n    dad sp\n    sphl\n";
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr). The 8080 has no
            // indirect-CALL instruction, only `PCHL` (jump to the
            // address in HL) - so a real CALL is emulated by hand:
            // push the return address (the label right after `pchl`)
            // onto the real hardware stack, exactly as `CALL` itself
            // would, then `PCHL` to the callee. The callee's own
            // `ret` pops that same address back into PC, landing
            // right after `pchl` below - indistinguishable from an
            // ordinary call/return pair.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            code_ << "    lhld ac\n";  // HL = callee's address - pchl's own implicit target
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
            // Real 8080 interrupt enable/disable - EI/DI, the same
            // real hardware feature as the 6502/8086 backends' own
            // cli/sti (see M6502Codegen.cpp/I8086Codegen.cpp).
            code_ << "    " << (e.op == "ion" ? "ei" : "di") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated *first*, and alone - genExpr()
// is a general recursive-descent call that might itself need `ptr`/
// `bc`/`cx`/`dx` (or DE/HL transiently) for its own nested
// subexpressions, so nothing of this function's own can safely be
// sitting in any of those across that call; only after it returns
// (and its result is safely resting in `ac`) does this function
// start using its own scratch.
//
// `slot->arraySize > 0` means `slot` is itself the array, and its own
// base address is the thing to index from; otherwise `slot` is a
// plain pointer variable and the base instead comes from *reading*
// that variable's own stored value. A Global's own base is a compile-
// time-known label (a bare `lxi d, <label>` loads a symbol's own
// address, exactly like genAddrOf()'s own use of the same trick) or,
// for a pointer variable, a direct read of its known address
// (loadPair()); a Local/Param's base is only known at runtime,
// relative to `fp` (genFrameAddr()), and for a pointer variable means
// dereferencing that frame address rather than just computing it.
//
// A Global array's element i sits at base+2*i, same as any ordinary
// increasing-address memory - but a *Local* array's element i sits at
// base-2*i (`frameByteOffset()` is already negative, and
// genFrameAddr()'s own `addr()` is the array's first/lowest-index
// word - see I8080Codegen.h's own header comment on why): getting
// this backwards silently corrupts whatever's above the array in the
// frame (another Local, the saved `fp`, or the return address
// itself), so the combine step below subtracts `ptr` (idx*2) from the
// base instead of adding it whenever the base came from a Local/Param
// array.
void I8080Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // HL = index*2 (words are 2 bytes on this byte-addressed target,
    // same reason every other backend's own indexing doubles the
    // index first - `dad h` doubles HL in a single instruction, since
    // adding a value to itself is exactly doubling it). Stashed in
    // `ptr` immediately - loading the base below (particularly
    // loadPair()/genFrameAddr(), which go through HL) would otherwise
    // clobber it.
    code_ << "    lhld ac\n    dad h\n";
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
            // slot is a pointer VARIABLE living in this frame - DE
            // needs its stored runtime VALUE, not its own address.
            code_ << "    mov e,m\n    inx h\n    mov d,m\n";
        }
    }
    const bool subtractIdx = !isGlobal && slot->arraySize > 0;
    if (subtractIdx) {
        // ptr = base(DE) - index*2(currently in `ptr`) - see this
        // function's own header comment on why a Local array needs
        // this instead of the add below. Same DE-minus-HL byte-by-
        // byte borrow chain as genBinary()'s own "-" operator.
        code_ << "    lhld ptr\n";
        code_ << "    mov a,e\n    sub l\n    mov e,a\n";
        code_ << "    mov a,d\n    sbb h\n    mov d,a\n";
        storePair("ptr", "d");
    } else {
        code_ << "    lhld ptr\n    dad d\n    shld ptr\n";  // ptr = index*2 + base
    }
}

void I8080Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    lhld ptr\n";
    code_ << "    mov e,m\n    inx h\n    mov d,m\n";
    storePair("ac", "d");
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

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a Local's
// storage is this activation's own fresh frame region, not pre-set to
// anything; arrayInit is always fully populated up to arraySize
// (zero-padded even with no source-level initializer), so this always
// leaves the whole array in a known state, same as a Global array's
// static trailer always did. Element i's own address is
// `frameByteOffset(slot) - 2*i` (see I8080Codegen.h's own header
// comment on why a Local array's elements sit at decreasing addresses
// as i increases) - computed directly here rather than through
// genFrameAddr(), which only ever knows `slot`'s own single offset.
void I8080Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        code_ << "    lxi h, " << arrayElemText(slot->arrayInit[i]) << "\n    shld ac\n";
        long elemOffset = frameByteOffset(slot) - 2L * static_cast<long>(i);
        code_ << "    lhld fp\n    lxi d, " << elemOffset << "\n    dad d\n";
        storeAcIndirect();
    }
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* 8080 hardware stack (`push h`/`pop d`),
// never in a named memory cell like `bc` - the right operand's own
// genExpr() is a general recursive call that might itself be another
// binary op (or genMul()/genDivMod()/genShift(), which all share the
// same `bc`/`cx`/`dx` cells), so a named cell holding "the other
// operand" would get clobbered the moment a nested one reuses it.
// The real stack has no such aliasing problem - it's genuinely
// unique call-depth storage, same reason the 6502 backend's own
// PHA/PLA sequences serve this exact role (see M6502Codegen.cpp).
void I8080Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        genExpr(*e.lhs);
        code_ << "    lhld ac\n    push h\n";  // save lhs on the real stack across evaluating rhs - see this
                                                // function's own header comment on why a named memory cell can't
                                                // be used for that instead
        genExpr(*e.rhs);                        // ac = rhs
        code_ << "    pop d\n";                  // DE = lhs (popped only now, after rhs is fully evaluated)
        code_ << "    lhld ac\n    dad d\n    shld ac\n";
        return;
    }
    if (op == "-") {
        genExpr(*e.lhs);
        code_ << "    lhld ac\n    push h\n";
        genExpr(*e.rhs);
        code_ << "    pop d\n";    // DE = lhs
        code_ << "    lhld ac\n";  // HL = rhs
        code_ << "    mov a,e\n    sub l\n    mov e,a\n";
        code_ << "    mov a,d\n    sbb h\n    mov d,a\n";
        storePair("ac", "d");
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

// No variable-count shift instruction - loop `rhs` times instead, one
// bit at a time, via shiftOneBit() (see I8080Codegen.h's own header
// comment, and the same fix every other software-shift backend here
// already needs for the same reason).
void I8080Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the count - see
                                            // genBinary()'s own header comment on why not a named cell
    genExpr(rhs);                           // ac = shift count
    code_ << "    lhld ac\n    mov c,l\n";  // c = shift count (always small; low byte is enough)
    code_ << "    pop h\n    shld bc\n";     // bc = value (restored only now, after the count is safely in c)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a,c\n    ora a\n";
    code_ << "    jz " << Ldone << "\n";
    shiftOneBit("bc", isLeft);
    code_ << "    dcr c\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply - see
// I8080Codegen.h's own header comment): `bc` is the multiplicand
// (doubled each iteration), `dx` the multiplier (halved each
// iteration, consumed one bit at a time), `ac` the accumulating
// product. Only verified against non-negative operands.
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

// Repeated subtraction (no hardware divide - see I8080Codegen.h's own
// header comment): `dx` is the running remainder (starts as the
// dividend), `bc` the fixed divisor, `ac` the quotient, counting how
// many times `bc` was subtracted out of `dx` before it would've gone
// negative. Only verified against non-negative operands.
void I8080Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);                          // ac = dividend
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop h\n    shld dx\n";  // dx = dividend (restored only now)
    copy16("bc", "ac");                     // bc = divisor
    code_ << "    lxi h, 0\n    shld ac\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    loadPair("d", "dx");
    code_ << "    lhld bc\n";
    // Tentative dx - bc; carry (borrow) set afterward means dx < bc,
    // so this round's subtraction can't happen - stop.
    code_ << "    mov a,e\n    sub l\n    mov c,a\n";  // c = tentative low result (c is scratch here, not "the
                                                        // divisor" - bc's own value was already consumed into HL)
    code_ << "    mov a,d\n    sbb h\n";
    code_ << "    jc " << Ldone << "\n";
    // Commit: dx -= bc.
    code_ << "    mov d,a\n    mov e,c\n";
    storePair("dx", "d");
    // Quotient += 1 - a real 16-bit increment (`inx h`), unlike the
    // 6502's own carry-chained inc16() workaround.
    code_ << "    lhld ac\n    inx h\n    shld ac\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void I8080Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b, checked the same
// "OR the two result bytes together" way genLogicalNot() checks a
// value for zero.
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

// Signed 16-bit "a < b" via the standard bias trick (see
// I8080Codegen.h's own header comment on why the 8080, unlike every
// other backend here, has no hardware overflow flag to combine with
// the result's sign bit instead): flipping just the top bit of each
// operand's high byte maps the signed range onto the unsigned one
// while preserving order, so a plain unsigned compare (the final
// borrow out of the high-byte subtraction) is all that's left to do.
void I8080Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    lhld ac\n    push h\n";  // saved on the real stack across evaluating b
    genExpr(b);
    code_ << "    pop d\n";    // DE = a
    code_ << "    lhld ac\n";  // HL = b
    code_ << "    mov a,d\n    xri 0x80\n    mov d,a\n";  // D = a's high byte, sign flipped
    code_ << "    mov a,h\n    xri 0x80\n    mov h,a\n";  // H = b's high byte, sign flipped
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
    code_ << "    lhld ac\n    mov a,l\n    ora h\n";
    code_ << "    jz " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    lhld ac\n    mov a,l\n    ora h\n";
    code_ << "    jz " << Lfalse << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    lxi h, 0\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    lhld ac\n    mov a,l\n    ora h\n";
    code_ << "    jnz " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    lhld ac\n    mov a,l\n    ora h\n";
    code_ << "    jnz " << Ltrue << "\n";
    code_ << Lfalse << ":\n    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

void I8080Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    lhld ac\n    mov a,l\n    ora h\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jz " << Ltrue << "\n";
    code_ << "    lxi h, 0\n    shld ac\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    lxi h, 1\n    shld ac\n";
    code_ << Ldone << ":\n";
}

std::string I8080Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 8080 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I8080Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleI8080(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly
    // once (see I8080Assembler.h's own `assembleI8080Object`), and
    // whichever of these names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI8080Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
