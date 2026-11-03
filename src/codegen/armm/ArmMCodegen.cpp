#include "ArmMCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "ArmMAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly, same convention
// (and the same reason) as M68kCodegen.cpp's/MipsCodegen.cpp's own
// kBaseAddress.
constexpr uint32_t kBaseAddress = 0x1000u;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

void ArmMCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dw`'s own expression evaluator
// (ArmMAssembler.cpp's Line::Dw) already resolves a bare symbol the
// same way `loadAddress`'s own label operand already does for an
// address loaded into a register.
std::string ArmMCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void ArmMCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void ArmMCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movw " << reg << "," << lbl << "\n";
    code_ << "    movt " << reg << "," << lbl << "\n";
}

void ArmMCodegen::loadImm32(const std::string &reg, uint32_t value) {
    if (value <= 0xFF) {
        code_ << "    movs " << reg << "," << static_cast<int32_t>(value) << "\n";
        return;
    }
    code_ << "    movw " << reg << "," << static_cast<int32_t>(value & 0xFFFFu) << "\n";
    code_ << "    movt " << reg << "," << static_cast<int32_t>((value >> 16) & 0xFFFFu) << "\n";
}

void ArmMCodegen::loadFrameWordAddr(const std::string &reg, int wordsFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (wordsFromFp == 0) return;
    loadImm32("r3", static_cast<uint32_t>((wordsFromFp > 0 ? wordsFromFp : -wordsFromFp) * 4));
    code_ << "    " << (wordsFromFp > 0 ? "adds" : "subs") << " " << reg << "," << reg << ",r3\n";
}

void ArmMCodegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int wordsFromFp = slot->kind == ir::SlotKind::Param ? (2 + slot->paramIndex) : -slot->frameOffset;
    loadFrameWordAddr(reg, wordsFromFp);
}

void ArmMCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
    } else {
        loadFrameAddr(reg, slot);
    }
}

void ArmMCodegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        loadImm32(reg, static_cast<uint32_t>(e.value));
        return;
    }
    loadAddress(reg, e.slot ? label(e.slot) : label(e.func));
}

std::string ArmMCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - ArmMLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see ArmMAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        loadImm32("sp", kInitialSP);
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt` -
        // see ArmMCodegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    b halt\n\n";
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
            trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) +
                                "  ; " + slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M68kCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> ArmMCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `lr` around its own body -
// same reasoning (and the same risk: exactly one link register) as
// MipsCodegen.cpp's own comment, just one real instruction on either
// side here (`pushlr`/`poppc`) instead of MIPS's two - already
// reentrant on its own, since `pushlr`/`poppc` are ordinary real
// `sp`-relative push/pop. `push r7`/`mov r7,sp` on top of that gives
// this call its own frame pointer (see this file's own header
// comment), and one dummy `push r0` per word of `frameSize` reserves
// this call's own fresh Local storage - the pushed value is never
// read, only the resulting `sp` displacement matters, so there's no
// need for this dialect's assembler to grow a `sub sp,#imm`
// instruction of its own just for this. emitEpilogue() undoes exactly
// this, in reverse, at every `return` and once more here as the
// fall-off-the-end default.
void ArmMCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    code_ << "    push r7\n";
    code_ << "    mov r7,sp\n";
    frameSize_ = fn.func->frameSize;
    for (int i = 0; i < frameSize_; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

void ArmMCodegen::emitEpilogue() {
    for (int i = 0; i < frameSize_; i++) code_ << "    pop r1\n";
    code_ << "    pop r7\n";
    code_ << "    poppc\n";
}

void ArmMCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmp r0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    b " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    cmp r0,0\n    beq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit - genFunction()'s own fall-off-the-end
            // epilogue never runs for this path, so it needs this
            // same frame teardown inline.
            emitEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddr("r2", s.declSlot);
                code_ << "    stind r2,r0\n";
            }
            // No "else" case for a bare, uninitialized scalar: its
            // own frame word already exists (genFunction()'s own
            // reserve-pushes already made room for it) - same as a
            // real, ordinary C automatic variable, its value is
            // simply whatever was already in that stack slot until
            // something assigns it.
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

void ArmMCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            loadImm32("r0", static_cast<uint32_t>(e.intValue));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("r2", e.slot);
            code_ << "    ldind r2,r0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    push r2\n";
                genExpr(*e.rhs);
                code_ << "    pop r2\n";
                code_ << "    stind r2,r0\n";
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            code_ << "    stind r2,r0\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    push r0\n";
            genExpr(*e.rhs);
            code_ << "    pop r2\n";
            code_ << "    stind r2,r0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg r0,r0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r2,r0\n";
                code_ << "    ldind r2,r0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn r0,r0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed onto the real stack, last-declared
            // one first, so the first argument ends up closest to the
            // return address `bl` is about to set `lr` for - matching
            // the address every Param already addresses itself by
            // (see this file's own header comment: paramIndex 0 is
            // `r7+8`, the lowest positive offset). Each callee lays
            // them into ITS OWN frame itself; nothing here needs to
            // know anything about the callee's own parameter count or
            // names beyond `e.args.size()`. The caller cleans the
            // pushed words back off afterward with one dummy `pop r1`
            // per argument (no `add sp,#imm` instruction exists in
            // this dialect - see genFunction()'s own identical
            // reasoning for the Local-frame reserve/release).
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    push r0\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            for (size_t i = 0; i < e.args.size(); i++) code_ << "    pop r1\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // `blx r2` is this dialect's own real Thumb-1 "BLX
            // (register)" encoding (see ArmMAssembler.cpp's own
            // encodeBlx()) - a real near call through whatever address
            // r2 holds, setting `lr` to the return address exactly
            // like `bl` does. r2's own bit 0 must be 1 (the real
            // ARMv7-M interworking rule `BLX (register)` enforces - a
            // UsageFault otherwise), which it already is here: `e.lhs`
            // only ever evaluates to a value that itself traces back
            // to genAddrOf()'s own `ExprKind::Func` case (there's
            // nothing else in this subset that's callable at all),
            // which already set that bit - see its own comment.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r2,r0\n";
            code_ << "    blx r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal Cortex-M with no NVIC/exception setup done
            // at all here - treated as a no-op, same reasoning (and
            // the same choice) as M68kCodegen.cpp's/MipsCodegen.cpp's
            // own Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `r2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is the array's own first word - a compile-time-
// known label for a Global, or (see loadFrameAddr()) a runtime
// r7-relative address for a Local/Param; otherwise `slot` is a plain
// pointer variable, so the base instead comes from *reading* that
// variable's own stored value first (again from a label or an r7-
// relative address, depending on `slot->kind`). A Local/Param array's
// own element i sits *below* element 0 (addresses decrease as the
// index increases - see this file's own header comment), the mirror
// image of a Global array's increasing-address layout, so this is
// base-minus-scaled-index there, not base-plus.
void ArmMCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    code_ << "    lsli r0,r0,2\n";  // r0 *= 4 (words are 4 bytes on this target)
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("r2", label(slot));
            code_ << "    adds r2,r2,r0\n";
        } else {
            loadFrameAddr("r2", slot);
            code_ << "    subs r2,r2,r0\n";
        }
        return;
    }
    loadSlotAddr("r1", slot);
    code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's own stored value
    code_ << "    adds r2,r1,r0\n";
}

void ArmMCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind r2,r0\n";
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a
// Local's storage is this activation's own fresh frame region, not
// pre-set to anything; arrayInit is always fully populated up to
// arraySize, so this always leaves the whole array in a known state,
// same as a Global array's static trailer always did. An element's
// own address is slot->frameOffset+i words below r7, the same
// arithmetic loadFrameAddr() uses for the array's own first word,
// just advanced by i.
void ArmMCodegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        loadArrayInitElem("r0", slot->arrayInit[i]);
        loadFrameWordAddr("r2", -(slot->frameOffset + static_cast<int>(i)));
        code_ << "    stind r2,r0\n";
    }
}

void ArmMCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("r2", operand.slot);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        // The real ARMv7-M function-pointer value, not a bare code
        // address: `fn_<name>` itself is always even (every Thumb
        // instruction, hence every function entry point, is 2-byte
        // aligned), but `BX`/`BLX (register)` - the only way this
        // address is ever actually branched to (see this file's own
        // `ExprKind::CallPtr` case) - loads the target register's own
        // bit 0 straight into `EPSR.T` and raises a UsageFault if it
        // isn't 1; M-profile has no ARM (A32) state to switch to at
        // all, Thumb is the only state, so this bit must always be
        // set on a value headed for one of those instructions. Adding
        // 1 is exactly OR-ing in that bit, since the address itself is
        // always even.
        loadAddress("r2", label(operand.func));
        code_ << "    adds r2,r2,1\n";
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void ArmMCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    adds r0,r1,r0\n";  // r0 = lhs + rhs
        } else {
            code_ << "    subs r0,r1,r0\n";  // r0 = lhs - rhs
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
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "&") {
            code_ << "    and r0,r1\n";
        } else if (op == "|") {
            code_ << "    orr r0,r1\n";
        } else {
            code_ << "    eor r0,r1\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);               // r0 = shift count
        code_ << "    mov r1,r0\n";    // r1 = shift count
        code_ << "    pop r0\n";       // r0 = value to shift
        code_ << "    " << (op == "<<" ? "lsl" : "lsr") << " r0,r1\n";
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

// Real hardware multiply (`MULS`) - a full 32x32->32 product, no
// operand-size limitation the way the 68000's own `MULS.W` has.
void ArmMCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    muls r0,r1\n";
}

// Software restoring-by-repeated-subtraction divide/remainder - see
// ArmMCodegen.h's own header comment on why there's no hardware fast
// path. Works on absolute values (`r0`/`r1`), then reapplies the
// sign C's own truncating-toward-zero division needs: the quotient
// is negative iff the operands' signs differ, and the remainder
// always takes the dividend's own sign.
void ArmMCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs (dividend), r0 = rhs (divisor)

    std::string LlhsAbsDone = newLabel();
    std::string LrhsAbsDone = newLabel();
    std::string Lloop = newLabel();
    std::string LloopDone = newLabel();
    std::string LnegateDone = newLabel();

    // r3 = 1 if the result (whichever one wantRemainder picks) needs
    // negating, else 0 - computed from the *original* signs before
    // either operand is overwritten by its own absolute value.
    if (wantRemainder) {
        code_ << "    movs r3,0\n";
        code_ << "    cmp r1,0\n    bge " << LlhsAbsDone << "\n";
        code_ << "    movs r3,1\n";
        code_ << LlhsAbsDone << ":\n";
    } else {
        // r3 = (lhs<0) XOR (rhs<0), built from two independent sign
        // tests (no real XOR-of-booleans instruction needed: the
        // second test just flips r3 via a second small branch).
        code_ << "    movs r3,0\n";
        code_ << "    cmp r1,0\n    bge " << LlhsAbsDone << "\n";
        code_ << "    movs r3,1\n";
        code_ << LlhsAbsDone << ":\n";
        code_ << "    cmp r0,0\n    bge " << LrhsAbsDone << "\n";
        code_ << "    movs r2,1\n";
        code_ << "    eor r3,r2\n";
        code_ << LrhsAbsDone << ":\n";
    }

    // Take absolute values of the dividend/divisor themselves - sign
    // already captured above.
    std::string LabsLhsDone = newLabel();
    code_ << "    cmp r1,0\n    bge " << LabsLhsDone << "\n";
    code_ << "    neg r1,r1\n";
    code_ << LabsLhsDone << ":\n";

    std::string LabsRhsDone = newLabel();
    code_ << "    cmp r0,0\n    bge " << LabsRhsDone << "\n";
    code_ << "    neg r0,r0\n";
    code_ << LabsRhsDone << ":\n";

    // r1 = |remainder| (starts as |lhs|), r2 = quotient (starts 0),
    // r0 = |divisor|. Repeated subtraction: while remainder>=divisor,
    // remainder-=divisor, quotient+=1.
    code_ << "    movs r2,0\n";
    code_ << Lloop << ":\n";
    code_ << "    cmp r1,r0\n    blt " << LloopDone << "\n";
    code_ << "    subs r1,r1,r0\n";
    code_ << "    adds r2,r2,1\n";
    code_ << "    b " << Lloop << "\n";
    code_ << LloopDone << ":\n";

    code_ << "    mov r0," << (wantRemainder ? "r1" : "r2") << "\n";
    code_ << "    cmp r3,0\n    beq " << LnegateDone << "\n";
    code_ << "    neg r0,r0\n";
    code_ << LnegateDone << ":\n";
}

// `CMP` sets the real signed condition codes for `Rn - Rm` directly
// in hardware (same as 68000's own `CMP.L` - see M68kCodegen.h's own
// header comment) - `branchIfTrue` is simply applied, no sign/
// overflow fixup needed the way the 6502 backend's own comparisons
// do.
void ArmMCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";   // flags = r1 - r0 = lhs - rhs
    std::string branchIfTrue;
    if (op == "==") branchIfTrue = "beq";
    else if (op == "!=") branchIfTrue = "bne";
    else if (op == "<") branchIfTrue = "blt";
    else if (op == "<=") branchIfTrue = "ble";
    else if (op == ">") branchIfTrue = "bgt";
    else if (op == ">=") branchIfTrue = "bge";
    else err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

std::string ArmMCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the cortex-m3 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> ArmMCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleArmM(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see ArmMAssembler.h's own `assembleArmMObject`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArmMObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
