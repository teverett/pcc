#include "PowerPCCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "PowerPCAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly, same idea as
// MipsCodegen.cpp's own kBaseAddress.
constexpr uint32_t kBaseAddress = 0x00100000u;

// Where this backend's own entry sequence points r1 before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM.
constexpr uint32_t kInitialSP = 0x00200000u;
}  // namespace

// A literal element's own decimal text; a symbol element is the
// referenced Slot's or Function's own label, written out bare - "dw"
// (PowerPCAssembler.cpp's Line::Dw) already evaluates any expr
// operand, symbol included, and now also records the same external-
// name/base-shift relocation (kPowerPCRelocAbs32) a label used as
// `lis`/`ori`'s own split address immediate already gets.
std::string PowerPCCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void PowerPCCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void PowerPCCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    lis " << reg << "," << lbl << "\n";
    code_ << "    ori " << reg << "," << reg << "," << lbl << "\n";
}

void PowerPCCodegen::loadImmediate(const std::string &reg, int32_t v) {
    if (v >= -32768 && v <= 32767) {
        code_ << "    li " << reg << "," << v << "\n";
        return;
    }
    uint32_t uv = static_cast<uint32_t>(v);
    code_ << "    lis " << reg << "," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
    code_ << "    ori " << reg << "," << reg << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
}

void PowerPCCodegen::loadSlotValue(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress("t2", label(slot));
        code_ << "    ldind t2," << reg << "\n";
        return;
    }
    code_ << "    ldindd fp," << reg << "," << slotDisp(slot) << "\n";
}

void PowerPCCodegen::storeSlotValue(const ir::Slot *slot, const std::string &reg) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress("t2", label(slot));
        code_ << "    stind t2," << reg << "\n";
        return;
    }
    code_ << "    stindd fp," << reg << "," << slotDisp(slot) << "\n";
}

std::string PowerPCCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - PowerPCLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see PowerPCAssembler.h's own header
    // comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lis sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    ori sp,sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in t0 the moment execution reaches `halt` -
        // see PowerPCCodegen.h's own header comment on why that's the
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

// Single-file-only - see MipsCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> PowerPCCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores the real hardware link register
// (via `mflr`/`mtlr` through `r0`, the only way to get at it - unlike
// MIPS's own `$ra`, a plain GPR, PowerPC's link register isn't
// directly memory-addressable) around its own body, whether or not it
// happens to call anything else - a non-leaf function's own call to
// another function overwrites the link register outright, so without
// this save/restore a caller's own eventual `blr` would return into
// whatever its *own* last call last returned to, not back to its own
// caller. Always saving/restoring (leaf or not) is simpler than
// tracking which functions are actually non-leaf, and costs only two
// real instructions (plus the stack push/pop) on either side - same
// reasoning as MipsCodegen.cpp's own version of this same comment.
//
// This same prologue also gives every call its own real frame: after
// the caller has already pushed its arguments onto `sp` (see
// ExprKind::Call below) and `bl` has set the link register, `sp`
// still points at argument 0, untouched by `bl` itself (a real
// PowerPC call is only ever a branch-and-link, never a hardware stack
// push). The prologue pushes the saved return address and the
// caller's own `fp` below that, in that order, then points `fp` at
// the second of those two pushed words - so `fp+0` is the caller's
// `fp`, `fp+4` is the saved return address, and `fp+8+4*i` is
// argument i, exactly matching slotDisp()'s own Param displacement.
// `addi sp,sp,-4*frameSize` then reserves this call's own Locals
// below `fp`, matching slotDisp()'s own Local displacement. Every
// activation gets its own `fp` this way, chained back through the
// stack to its caller's, which is what makes direct and mutual
// recursion safe - unlike this backend's own previous model, where
// every Local/Param was one shared memory cell every call (recursive
// or not) stepped on identically.
void PowerPCCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    mflr r0\n";
    code_ << "    addi sp,sp,-4\n    stind sp,r0\n";
    code_ << "    addi sp,sp,-4\n    stind sp,fp\n";
    code_ << "    or fp,sp,sp\n";
    if (fn.func->frameSize > 0) code_ << "    addi sp,sp,-" << (fn.func->frameSize * 4) << "\n";

    genStmt(*fn.body);

    emitEpilogue();
    code_ << "    blr\n\n";
}

// Restores `fp` and the link register from the words genFunction()'s
// own prologue saved them to, and leaves `sp` pointing at argument 0
// again (the same place it pointed right on entry, before the
// prologue's own two pushes and `addi sp,sp,-4*frameSize`) - the
// caller, not this function, is responsible for popping the arguments
// it itself pushed (see ExprKind::Call below), the same caller-cleans-
// up convention I386Codegen.cpp's own `add esp,N` after `call` uses.
void PowerPCCodegen::emitEpilogue() {
    code_ << "    ldind fp,t2\n";
    code_ << "    addi sp,fp,4\n";
    code_ << "    ldind sp,r0\n";
    code_ << "    addi sp,sp,4\n";
    code_ << "    mtlr r0\n";
    code_ << "    or fp,t2,t2\n";
}

void PowerPCCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmpwi t0,0\n";
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    cmpwi t0,0\n";
            code_ << "    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    b " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    b " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // A `return` statement is its own early exit from the
            // function, bypassing genFunction()'s own fall-through
            // epilogue entirely, so it needs that same frame teardown
            // inline, before `blr` - see emitEpilogue()'s own comment.
            // `t0` (this return value) is never touched by it.
            emitEpilogue();
            code_ << "    blr\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeSlotValue(s.declSlot, "t0");
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

void PowerPCCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            int32_t v = static_cast<int32_t>(static_cast<uint32_t>(e.intValue));
            if (v >= -32768 && v <= 32767) {
                code_ << "    li t0," << v << "\n";
            } else {
                uint32_t uv = static_cast<uint32_t>(v);
                code_ << "    lis t0," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
                code_ << "    ori t0,t0," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotValue("t0", e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // t2 = target address
                code_ << "    addi sp,sp,-4\n    stind sp,t2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,t2\n    addi sp,sp,4\n";
                code_ << "    stind t2,t0\n";
                return;
            }
            genExpr(*e.rhs);
            storeSlotValue(e.slot, "t0");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,t2\n    addi sp,sp,4\n";
            code_ << "    stind t2,t0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg t0,t0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // t0 = pointer value
                code_ << "    or t2,t0,t0\n";  // t2 = t0 (real PowerPC "mr" idiom - see genBinary()'s own
                                                // header comment on why r0 can't be used as a zero register here)
                code_ << "    ldind t2,t0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    nor t0,t0,t0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Pushes arguments right-to-left so argument 0 ends up
            // closest to the saved return address once genFunction()'s
            // own prologue runs in the callee (see its own comment,
            // and slotDisp()'s Param displacement) - `bl` itself never
            // touches `sp`, so it's still pointing at argument 0 the
            // instant control reaches the callee. This function (the
            // caller) cleans the arguments back off afterward, since
            // the callee's own epilogue only ever restores `sp` to
            // where it was on entry - argument 0's own address, not
            // any address above it.
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            if (!e.args.empty()) code_ << "    addi sp,sp," << (e.args.size() * 4) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // Real PowerPC has no single-instruction indirect call -
            // `bl`'s own target is always a compile-time-relative
            // displacement, never a register. The standard two-
            // instruction idiom every real PowerPC compiler uses
            // instead: `mtctr` moves the callee's own address into
            // the Count Register, then `bctrl` branches to whatever
            // address CTR holds and sets the link register, exactly
            // like `bl` does for a compile-time-known target.
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    mtctr t0\n";
            code_ << "    bctrl\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal target with no exception/interrupt setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as MipsCodegen.cpp's own
            // Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `t2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means `slot` itself is the array, so the base address is either
// a compile-time-known label (a Global) or `fp+slotDisp(slot)` (a
// Local - see slotDisp()'s own comment on why a Local array's element
// i sits at a *decreasing* address as i increases, the opposite of a
// Global array's own increasing-address layout, hence `subf` below
// rather than `add`). `arraySize == 0` means `slot` is a plain
// pointer variable instead, so the base comes from *reading* that
// variable's own stored value first (loadSlotValue, not an address).
void PowerPCCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    code_ << "    slwi t0,t0,2\n";  // t0 *= 4 (words are 4 bytes on this target)
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("t2", label(slot));
            code_ << "    add t2,t2,t0\n";
        } else {
            code_ << "    addi t2,fp," << slotDisp(slot) << "\n";
            code_ << "    subf t2,t0,t2\n";  // t2 = t2 - t0 ("subf rD,rA,rB" = rB - rA)
        }
    } else {
        loadSlotValue("t1", slot);  // t1 = the pointer variable's own stored value
        code_ << "    add t2,t1,t0\n";
    }
}

void PowerPCCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind t2,t0\n";
}

// Stores `slot->arrayInit` into this Local array's own frame slots -
// a Local array's own address isn't a compile-time-resolvable label
// the way a Global array's is, so (unlike declareArray()'s own `dw`
// trailer line) this has to run as real code, one element at a time,
// every time this declaration is reached.
void PowerPCCodegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
        } else {
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        int disp = slotDisp(slot) - 4 * static_cast<int>(i);
        code_ << "    stindd fp,t0," << disp << "\n";
    }
}

void PowerPCCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            loadAddress("t0", label(operand.slot));
        } else {
            code_ << "    addi t0,fp," << slotDisp(operand.slot) << "\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    or t0,t2,t2\n";  // t0 = t2 (real PowerPC "mr" idiom)
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
        code_ << "    or t0,t2,t2\n";  // t0 = t2 (real PowerPC "mr" idiom)
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void PowerPCCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "+") {
            code_ << "    add t0,t1,t0\n";  // t0 = lhs + rhs
        } else {
            code_ << "    subf t0,t0,t1\n";  // t0 = t1 - t0 = lhs - rhs
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
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "&") {
            code_ << "    and t0,t1,t0\n";
        } else if (op == "|") {
            code_ << "    or t0,t1,t0\n";
        } else {
            code_ << "    xor t0,t1,t0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        code_ << "    or t1,t0,t0\n";                    // t1 = shift count (t0, real PowerPC "mr" idiom)
        code_ << "    ldind sp,t0\n    addi sp,sp,4\n";  // t0 = value to shift
        code_ << "    " << (op == "<<" ? "slw" : "srw") << " t0,t0,t1\n";
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

// Real hardware multiply (`mullw`) - a full 32x32->32 product, no
// operand-size limitation the way the 68000's own `MULS.W` has.
void PowerPCCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    mullw t0,t1,t0\n";
}

// Real hardware divide (`divw`) gives only the quotient - unlike
// MIPS's own `div` (which leaves both quotient and remainder ready to
// pull out of HI/LO), classic PowerPC has no remainder instruction at
// all, so `%` is synthesized the standard way: `divw` for the
// quotient, `mullw` to multiply the quotient back out by the divisor,
// then `subf` to recover the remainder as `dividend - quotient *
// divisor`.
void PowerPCCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    if (!wantRemainder) {
        code_ << "    divw t0,t1,t0\n";
        return;
    }
    code_ << "    divw t2,t1,t0\n";    // t2 = quotient (t1/t0)
    code_ << "    mullw t2,t2,t0\n";   // t2 = quotient * divisor
    code_ << "    subf t0,t2,t1\n";    // t0 = t1 - t2 = dividend - quotient*divisor
}

// PowerPC has no single compare-and-set instruction the way MIPS's
// own `slt` is - a comparison here always needs `cmpw` plus a real
// conditional branch to decide which boolean value to materialize,
// the same "compare, then branch to decide" shape as the 68000's own
// CMP+Bcc (see M68kCodegen.h's own header comment).
void PowerPCCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    cmpw t1,t0\n";
    std::string branch;
    if (op == "<") branch = "blt";
    else if (op == ">") branch = "bgt";
    else if (op == "<=") branch = "ble";
    else if (op == ">=") branch = "bge";
    else if (op == "==") branch = "beq";
    else if (op == "!=") branch = "bne";
    else err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branch << " " << Ltrue << "\n";
    code_ << "    li t0,0\n";
    code_ << "    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    li t0,1\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmpwi t0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    li t0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    li t0,1\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmpwi t0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    li t0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    li t0,0\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    bne " << Lfalse << "\n";
    code_ << "    li t0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    li t0,0\n";
    code_ << Ldone << ":\n";
}

std::string PowerPCCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                   const std::string &) const {
    throw std::runtime_error("the powerpc backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'raw' is)");
}

std::optional<std::string> PowerPCCodegen::assembleInProcess(const std::string &asmText,
                                                               const std::string &format) const {
    if (format == "raw") return assemblePowerPC(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see PowerPCAssembler.h's own `assemblePowerPCObject`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePowerPCObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
