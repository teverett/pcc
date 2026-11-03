#include "Z80Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Z80Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - an
// arbitrary address distinct from the 8080 backend's own 0x0100 (see
// ../i8080/I8080Codegen.cpp), chosen only to leave plenty of RAM
// above it in a bare 64KB Z80 system; tests/run_tests.sh's own
// emulator setup loads this binary here and sets PC to it directly,
// no boot ROM involved.
constexpr uint32_t kBaseAddress = 0x8000;
// Same reasoning as the 8080 backend's own `kStackTop` - the Z80's
// SP is likewise undefined at reset and this backend's real CALL/
// RET/PUSH/POP all need a real initial value.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void Z80Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) own label - the referenced Slot's or Function's own
// label() text, exactly the shape `ld hl, nn`'s own 16-bit operand
// already takes (see genAddrOf()). declareArray() below hands this
// straight to a `dw` directive rather than computing a numeric
// low/high split itself - Z80Assembler.cpp's own `emit16` already
// resolves a symbol's full 16-bit address once every label is bound,
// the same single evaluator call `ld hl, nn`'s own operand goes
// through.
std::string Z80Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void Z80Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void Z80Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ld hl, [" << src << "]\n    ld [" << dst << "], hl\n";
}

void Z80Codegen::loadPair(const std::string &pairReg, const std::string &src) {
    code_ << "    ld " << pairReg << ", [" << src << "]\n";
}

void Z80Codegen::storePair(const std::string &dst, const std::string &pairReg) {
    code_ << "    ld [" << dst << "], " << pairReg << "\n";
}

// See Z80Codegen.h's own header comment on why this goes through the
// real hardware stack: there's no `ld hl,ix` (or any other direct
// register-to-register move between HL and IX) on this chip, only
// `ld sp,ix`/`ld ix,sp`-shaped forms, neither of which is HL. `ix`
// itself is left untouched - only the real SP moves, transiently,
// across the push/pop pair - so this is safe to call in the middle
// of evaluating anything else.
void Z80Codegen::loadFrameAddr(const ir::Slot *slot, long extraWords) {
    code_ << "    push ix\n    pop hl\n";
    long off = slot->kind == ir::SlotKind::Param ? (4 + 2 * static_cast<long>(slot->paramIndex))
                                                   : (-2L * (slot->frameOffset + extraWords));
    uint16_t off16 = static_cast<uint16_t>(off);
    code_ << "    ld de, " << off16 << "\n    add hl, de\n";
}

void Z80Codegen::readWordAt(const std::string &loReg, const std::string &hiReg) {
    code_ << "    ld " << loReg << ", m\n    inc hl\n    ld " << hiReg << ", m\n";
}

// A Local array's own initializer values (if any - see
// IRBuilder.h's `ArrayInitElem`) are stored into this call's own
// frame once, right here at the point of declaration, rather than
// ever living in static trailer data the way a Global array's own
// `declareArray()` does - every call needs its own independent copy.
void Z80Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        loadFrameAddr(slot, static_cast<long>(i));
        code_ << "    ld de, " << arrayElemText(slot->arrayInit[i]) << "\n";
        code_ << "    ld m, e\n    inc hl\n    ld m, d\n";
    }
}

void Z80Codegen::storeAcIndirect() {
    // Assumes HL already holds the target address - plain
    // accumulator loads from `ac`'s own two bytes (by name, never
    // touching HL) interleaved with `(HL)` stores, so the caller's
    // address in HL survives across both.
    code_ << "    ld a, [ac]\n    ld m, a\n";
    code_ << "    inc hl\n";
    code_ << "    ld a, [ac+1]\n    ld m, a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl` - see
// Z80Codegen.h's own header comment on why this is only two
// instructions here (`RL`/`RR` target L/H directly) versus the
// 8080's six (RAL/RAR only ever touch the accumulator, so getting a
// value in and back out of it costs two extra MOVs per byte).
void Z80Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ld hl, [" << lbl << "]\n";
    code_ << "    or a\n";
    if (isLeft) {
        code_ << "    rl l\n    rl h\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    rr h\n    rr l\n";
    }
    code_ << "    ld [" << lbl << "], hl\n";
}

std::string Z80Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all - an ordinary object
    // file never does. `objectMode` (see Codegen.h) means no object
    // file ever emits this entry sequence itself, even the one that
    // happens to define `main` - Z80Linker.cpp is solely responsible
    // for synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h) - same
    // pattern Pdp7Codegen.cpp/I8086Codegen.cpp's own `objectMode`
    // comment documents. An object file's own base is `org 0` - it's
    // the linker's job to place it, not this backend's (see
    // Z80Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ld sp, " << kStackTop << "\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra load leaves it in the real HL pair
        // too, for a quick look with nothing more than a register dump.
        code_ << "    ld hl, [ac]\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // The fixed memory pseudo-registers - see Z80Codegen.h's own
    // header comment for what each one is for. Every genExpr() call
    // anywhere in the *whole linked program* shares the same `ac`
    // (it's the one place a callee's own return value and a caller's
    // own next operand both pass through - see the `Call`/`Return`
    // cases below), so unlike every other symbol this backend emits,
    // these five can't each be redeclared per-object: in whole-
    // program mode (`!objectMode`) they're declared once, right here,
    // as ordinary temps; in object mode, this file never declares
    // them at all - every reference to `ac`/`_bc`/`cx`/`dx`/`ptr`
    // anywhere in a compiled object is then automatically deferred as
    // an external relocation (the same "undefined symbol" path any
    // other symbol would take), resolved against the one real
    // definition Z80Linker.cpp's own synthesized entry object
    // provides - see that file's own header comment.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("_bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
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

// Single-file-only (see Z80Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> Z80Codegen::perFileAsm() const { return perFileAsm_; }

// Every call gets its own real frame, built around `ix`: the real
// hardware stack is what makes this reentrant (direct or indirect
// recursion both just mean more real CALLs, each pushing its own
// return address and getting its own `ix` - see loadFrameAddr()'s
// own header comment). On entry, SP points at the real return
// address CALL just pushed; `push ix` saves the caller's own frame
// pointer right below it, then `ix` is set to that same SP (no
// direct `ld ix,sp` exists on this chip, so this goes through `ix`
// itself: zero it, then add the real SP value in) - so from `ix`:
// `ix+0` is the saved caller `ix`, `ix+2` the return address,
// `ix+4+2*i` the i'th argument (pushed by the caller in reverse
// order - see the `Call` case below - so argument 0 ends up closest
// to the return address). `frameReserveBytes_` single-byte `dec sp`s
// then carve out this function's own Locals below `ix`, one byte at
// a time (no instruction moves an arbitrary computed value into SP
// directly - see loadFrameAddr()'s own comment on the same
// restriction for HL/ix). Every `Return` (and this implicit
// fallthrough one) undoes exactly that with the matching `inc sp`s
// before restoring the caller's own `ix` and returning.
void Z80Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push ix\n";
    code_ << "    ld ix, 0\n";
    code_ << "    add ix, sp\n";
    frameReserveBytes_ = fn.func->frameSize * 2;
    for (int i = 0; i < frameReserveBytes_; i++) code_ << "    dec sp\n";

    genStmt(*fn.body);

    for (int i = 0; i < frameReserveBytes_; i++) code_ << "    inc sp\n";
    code_ << "    pop ix\n";
    code_ << "    ret\n\n";
}

void Z80Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jp z, " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
            code_ << "    jp z, " << Lend << "\n";
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
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            for (int i = 0; i < frameReserveBytes_; i++) code_ << "    inc sp\n";
            code_ << "    pop ix\n";
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                // The target address has to survive evaluating the
                // initializer intact - which might itself index into
                // an array and so reuse `ptr` as scratch (see
                // genIndexAddr()) - so it's saved on the real
                // hardware stack instead, exactly like the indexed-
                // Assign case below does.
                loadFrameAddr(s.declSlot);
                code_ << "    push hl\n";
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    pop hl\n";
                storeAcIndirect();
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

void Z80Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ld hl, " << v16 << "\n    ld [ac], hl\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16("ac", label(e.slot));
            } else {
                loadFrameAddr(e.slot);
                readWordAt("e", "d");
                storePair("ac", "de");
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
                code_ << "    ld hl, [ptr]\n    push hl\n";
                genExpr(*e.rhs);
                code_ << "    pop hl\n";
                storeAcIndirect();
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                genExpr(*e.rhs);
                copy16(label(e.slot), "ac");
                return;
            }
            // Same reasoning as the indexed-Assign case just above -
            // `ptr` isn't safe to park the target address in across
            // evaluating `rhs`, since `rhs` might itself index into
            // an array and reuse `ptr` as scratch there.
            loadFrameAddr(e.slot);
            code_ << "    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    ld hl, [ptr]\n    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ld hl, [ac]\n";
                code_ << "    xor a\n    sub l\n    ld l, a\n";
                code_ << "    ld a, 0\n    sbc a, h\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    ld hl, [ac]\n";
                code_ << "    ld e, m\n    inc hl\n    ld d, m\n";
                code_ << "    ld [ac], de\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    ld hl, [ac]\n";
                code_ << "    ld a, l\n    cpl\n    ld l, a\n";
                code_ << "    ld a, h\n    cpl\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Real argument passing on the real hardware stack -
            // pushed right-to-left so argument 0 ends up closest to
            // the return address (see genFunction()'s own header
            // comment on the resulting `ix+4+2*i` layout); the callee
            // never cleans these up itself (no `ret n` on this chip),
            // so this caller pops them back off, one throwaway `pop
            // hl` per argument, right after the call returns.
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    ld hl, [ac]\n    push hl\n";
            }
            code_ << "    call " << label(e.func) << "\n";
            for (size_t i = 0; i < e.args.size(); i++) code_ << "    pop hl\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr). Like the 8080 (see
            // ../i8080/I8080Codegen.cpp's own CallPtr comment), the
            // Z80 has no indirect-CALL instruction, only `JP (HL)`
            // (spelled "jp [hl]" in this dialect - see
            // Z80Assembler.h's own header comment on bracket syntax),
            // object-code-identical to the 8080's `PCHL`. A real CALL
            // is emulated the same way: push the return address (the
            // label right after `jp [hl]`) onto the real hardware
            // stack, then jump to the callee through HL. The callee's
            // own `ret` pops that address back into PC.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lret = newLabel();
            code_ << "    ld hl, [ac]\n";  // HL = callee's address - jp [hl]'s own implicit target
            code_ << "    ld de, " << Lret << "\n";
            code_ << "    push de\n";
            code_ << "    jp [hl]\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real Z80 interrupt enable/disable - same real hardware
            // feature, and same mnemonics, as the 8080 backend's own
            // EI/DI (see I8080Codegen.cpp).
            code_ << "    " << (e.op == "ion" ? "ei" : "di") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated first, and alone - see
// ../i8080/I8080Codegen.cpp's own genIndexAddr() comment on why.
// `slot->arraySize > 0` means the base is a compile-time-known label
// (an address - `ld de, <label>` loads a symbol's own address,
// exactly like genAddrOf()'s own use of the same trick), otherwise
// `slot` is a plain pointer variable and the base instead comes from
// *reading* that variable's own stored value into DE - a single real
// instruction here (see Z80Codegen.h's own header comment on
// loadPair()), unlike the 8080's own stack round-trip.
void Z80Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // HL = index*2 (words are 2 bytes on this byte-addressed target) -
    // `ADD HL,HL` doubles HL in one instruction, same trick as the
    // 8080 backend's own `DAD H`. Stashed in `ptr` (not a register)
    // since loadFrameAddr() below needs HL/DE itself.
    code_ << "    ld hl, [ac]\n    add hl, hl\n";
    code_ << "    ld [ptr], hl\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ld de, " << label(slot) << "\n";
            code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
        } else {
            // A Local array's own words sit at decreasing addresses
            // as its element index increases (loadFrameAddr() gives
            // the array's own first word, closest to `ix` - later
            // words are further below it) - the mirror image of a
            // Global array's increasing-address layout above, so
            // this is base-minus-scaled-index via a real subtract-
            // with-carry, not base-plus.
            loadFrameAddr(slot);               // hl = array's own base address
            code_ << "    ld de, [ptr]\n";     // de = scaled index, saved above
            code_ << "    or a\n    sbc hl, de\n";
            code_ << "    ld [ptr], hl\n";
        }
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        loadPair("de", label(slot));
    } else {
        loadFrameAddr(slot);
        readWordAt("e", "d");
    }
    code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
}

void Z80Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ld hl, [ptr]\n";
    code_ << "    ld e, m\n    inc hl\n    ld d, m\n";
    code_ << "    ld [ac], de\n";
}

void Z80Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    ld hl, " << label(operand.slot) << "\n    ld [ac], hl\n";
        } else {
            loadFrameAddr(operand.slot);
            code_ << "    ld [ac], hl\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ld hl, " << label(operand.func) << "\n    ld [ac], hl\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* Z80 hardware stack (`push hl`/`pop de`),
// never in a named memory cell - see ../i8080/I8080Codegen.cpp's own
// genBinary() header comment for why (the same reasoning applies
// unchanged: the right operand's own genExpr() might recurse into
// genMul()/genDivMod()/genShift(), all of which share `bc`/`cx`/`dx`).
void Z80Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        code_ << "    add hl, de\n    ld [ac], hl\n";
        return;
    }
    if (op == "-") {
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        code_ << "    or a\n    sbc hl, de\n";       // HL = lhs - rhs (real 16-bit subtract-with-carry)
        code_ << "    ld [ac], hl\n";
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
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    ld a, l\n    " << insn << " e\n    ld l, a\n";
        code_ << "    ld a, h\n    " << insn << " d\n    ld h, a\n";
        code_ << "    ld [ac], hl\n";
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

// No variable-count shift instruction (see Z80Codegen.h's own header
// comment) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit().
void Z80Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);                                // ac = value to shift
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                                 // ac = shift count
    code_ << "    ld a, [ac]\n    ld b, a\n";     // b = shift count (always small; low byte is enough)
    code_ << "    pop hl\n    ld [_bc], hl\n";      // bc = value (restored only now)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    shiftOneBit("_bc", isLeft);
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "_bc");
}

// Textbook shift-and-add multiply (no hardware multiply): `bc` is
// the multiplicand (doubled each iteration), `dx` the multiplier
// (halved each iteration), `ac` the accumulating product. Only
// verified against non-negative operands.
void Z80Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);                                // ac = multiplicand
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the multiplier
    genExpr(rhs);
    code_ << "    pop hl\n    ld [_bc], hl\n";  // bc = multiplicand (restored only now)
    copy16("dx", "ac");                          // dx = multiplier
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    code_ << "    ld b, 16\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    code_ << "    ld a, [dx]\n    and 1\n";
    code_ << "    jp z, " << Lskipadd << "\n";
    code_ << "    ld hl, [ac]\n    ld de, [_bc]\n    add hl, de\n    ld [ac], hl\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("_bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient. A single `SBC HL,DE` both tests "has dx gone below
// bc?" (the carry flag) and computes the committed subtraction
// result in one instruction - no separate tentative-then-commit byte
// reconstruction needed the way the 8080 backend's own version
// needs (see I8080Codegen.cpp). Only verified against non-negative
// operands.
void Z80Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);                                // ac = dividend
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop hl\n    ld [dx], hl\n";  // dx = dividend (restored only now)
    copy16("_bc", "ac");                          // bc = divisor
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld hl, [dx]\n    ld de, [_bc]\n";
    code_ << "    or a\n    sbc hl, de\n";  // HL = dx - bc; CY = borrow (dx < bc)
    code_ << "    jp c, " << Ldone << "\n";
    code_ << "    ld [dx], hl\n";  // commit: dx -= bc (HL already holds the subtracted result)
    // Quotient += 1 - a real 16-bit increment.
    code_ << "    ld hl, [ac]\n    inc hl\n    ld [ac], hl\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void Z80Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` reduce to a single `SBC HL,DE`: the Z flag it sets
// directly means "lhs == rhs" (the full 16-bit result is zero iff
// the two operands were equal) - no manual byte-by-byte OR-check
// needed the way the 8080 backend's own version needs.
void Z80Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(rhs);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
    code_ << "    or a\n    sbc hl, de\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (wantEqual ? "z" : "nz") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// ../i8080/I8080Codegen.h's own header comment: the Z80, like the
// 8080, has no single flag or condition code for signed less-than) -
// flipping the sign bit of each operand's high byte maps the signed
// range onto the unsigned one while preserving order, and a single
// `SBC HL,DE` (instead of the 8080's two byte-wise `SUB`/`SBB`) does
// the rest.
void Z80Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(b);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = a, DE = b
    code_ << "    ld a, h\n    xor 0x80\n    ld h, a\n";  // H = a's high byte, sign flipped
    code_ << "    ld a, d\n    xor 0x80\n    ld d, a\n";  // D = b's high byte, sign flipped
    code_ << "    or a\n    sbc hl, de\n";                // CY = (a < b) unbiased
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (invert ? "nc" : "c") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp z, " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp z, " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp nz, " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp nz, " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp z, " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

std::string Z80Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                              const std::string &) const {
    throw std::runtime_error("the Z80 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Z80Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleZ80(asmText);
    // `coff`/`elf`/`macho`/`aout`: real separate compilation (see
    // generate()'s own `objectMode` comment) - this backend's own
    // assembler builds one format-neutral ObjectFile exactly once
    // (see Z80Assembler.h's own `assembleZ80Object`), and whichever
    // of these four names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it - this file never touches a real COFF/ELF/Mach-O/
    // a.out byte layout itself.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleZ80Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
