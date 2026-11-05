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

void PowerPCCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                    Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<uint32_t>(v) << ", " << static_cast<uint32_t>(v >> 32);
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
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
        loadOperand("t2", reg, slot->type);
        return;
    }
    loadOperandDisp(slotDisp(slot), reg, slot->type);
}

void PowerPCCodegen::storeSlotValue(const ir::Slot *slot, const std::string &reg) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress("t2", label(slot));
        storeOperand("t2", reg, slot->type);
        return;
    }
    storeOperandDisp(slotDisp(slot), reg, slot->type);
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
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + directive + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
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
    if (fn.func->frameSize > 0) code_ << "    addi sp,sp,-" << fn.func->frameSize << "\n";

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
            if (isWide(s.cond->type)) code_ << "    or t0,t0,t1\n";
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
            if (isWide(s.cond->type)) code_ << "    or t0,t0,t1\n";
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
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
                loadImmediate("t1", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
                return;
            }
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
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
                storeOperand("t2", "t0", e.slot->type);
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
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    neg t0,t0\n";
                }
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
                if (isWide(e.rhs->type)) code_ << "    nor t1,t1,t1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
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
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    addi sp,sp,-8\n    stind sp,t1\n    stindd sp,t0,4\n";
                    totalBytes += 8;
                } else {
                    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
                    totalBytes += 4;
                }
            }
            code_ << "    bl " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    addi sp,sp," << totalBytes << "\n";
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
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    slwi t0,t0," << shift << "\n";
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
    loadOperand("t2", "t0", e.slot->type);
}

// Stores `slot->arrayInit` into this Local array's own frame slots -
// a Local array's own address isn't a compile-time-resolvable label
// the way a Global array's is, so (unlike declareArray()'s own `dw`
// trailer line) this has to run as real code, one element at a time,
// every time this declaration is reached.
void PowerPCCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int disp = slotDisp(slot) - w * static_cast<int>(i);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
            code_ << "    stindd fp,t0," << disp << "\n";
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
            code_ << "    stindd fp,t0," << (disp + 4) << "\n";
            continue;
        }
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
        } else {
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        if (w == 1) {
            code_ << "    stindb fp,t0," << disp << "\n";
        } else if (w == 2) {
            code_ << "    stindh fp,t0," << disp << "\n";
        } else {
            code_ << "    stindd fp,t0," << disp << "\n";
        }
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

void PowerPCCodegen::loadOperand(const std::string &addrReg, const std::string &reg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindb " << addrReg << "," << reg << "\n";
        if (!type.isUnsigned) code_ << "    slwi " << reg << "," << reg << ",24\n    srawi " << reg << "," << reg
                                     << ",24\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindhu" : "ldindh") << " " << addrReg << "," << reg << "\n";
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << "," << reg << "\n";
    } else {  // 8 - long long: t1:t0 (hi:lo), low word first (reg is always "t0" here)
        code_ << "    ldind " << addrReg << "," << reg << "\n";
        code_ << "    ldindd " << addrReg << ",t1,4\n";
    }
}

void PowerPCCodegen::storeOperand(const std::string &addrReg, const std::string &reg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << "," << reg << "\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << "," << reg << "\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << "," << reg << "\n";
    } else {
        code_ << "    stind " << addrReg << "," << reg << "\n";
        code_ << "    stindd " << addrReg << ",t1,4\n";
    }
}

void PowerPCCodegen::loadOperandDisp(int disp, const std::string &reg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindb fp," << reg << "," << disp << "\n";
        if (!type.isUnsigned) code_ << "    slwi " << reg << "," << reg << ",24\n    srawi " << reg << "," << reg
                                     << ",24\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindhu" : "ldindh") << " fp," << reg << "," << disp << "\n";
    } else if (w == 4) {
        code_ << "    ldindd fp," << reg << "," << disp << "\n";
    } else {
        code_ << "    ldindd fp," << reg << "," << disp << "\n";
        code_ << "    ldindd fp,t1," << (disp + 4) << "\n";
    }
}

void PowerPCCodegen::storeOperandDisp(int disp, const std::string &reg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb fp," << reg << "," << disp << "\n";
    } else if (w == 2) {
        code_ << "    stindh fp," << reg << "," << disp << "\n";
    } else if (w == 4) {
        code_ << "    stindd fp," << reg << "," << disp << "\n";
    } else {
        code_ << "    stindd fp," << reg << "," << disp << "\n";
        code_ << "    stindd fp,t1," << (disp + 4) << "\n";
    }
}

void PowerPCCodegen::narrowT0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    slwi t0,t0,24\n";
        code_ << "    " << (to.isUnsigned ? "srwi" : "srawi") << " t0,t0,24\n";
    } else if (w == 2) {
        code_ << "    slwi t0,t0,16\n";
        code_ << "    " << (to.isUnsigned ? "srwi" : "srawi") << " t0,t0,16\n";
    }
}

// An implicit or explicit type conversion. Every arithmetic/
// comparison operand narrower than `int` is already promoted by
// IRBuilder before any Binary/Unary ever sees it, so the only real
// work here is crossing the native-int/`long long` boundary (sign/
// zero-extending t0 into t1, or just dropping t1), or re-truncating+
// re-extending t0 for a narrower target - see narrowT0To().
void PowerPCCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowT0To(to);
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    li t1,0\n";
        } else {
            code_ << "    srawi t1,t0,31\n";
        }
    } else if (!fromWide && !toWide) {
        narrowT0To(to);
    }
}

// `long long` add/sub (lhs OP rhs) - PowerPC has no single carry flag
// GPR instruction this dialect exposes, so the carry/borrow out of
// the low-word op is recovered the same way a comparison already
// materializes a boolean here: `cmplw`+a conditional branch.
void PowerPCCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);  // t1:t0 = lhs
    code_ << "    addi sp,sp,-8\n    stind sp,t0\n    stindd sp,t1,4\n";
    genExpr(rhs);  // t1:t0 = rhs
    code_ << "    or t3,t0,t0\n    or t4,t1,t1\n";  // t3:t4 = rhs (lo:hi)
    code_ << "    ldind sp,t0\n    ldindd sp,t1,4\n    addi sp,sp,8\n";  // t1:t0 = lhs
    std::string Lcarry = newLabel(), Ldone = newLabel();
    if (isAdd) {
        code_ << "    add t5,t0,t3\n";           // t5 = lo sum
        code_ << "    cmplw t5,t0\n    bge " << Lcarry << "\n";  // unsigned: sum >= lhs_lo means no carry
        code_ << "    li t6,1\n    b " << Ldone << "\n";
        code_ << Lcarry << ":\n    li t6,0\n";
        code_ << Ldone << ":\n";
        code_ << "    add t1,t1,t4\n    add t1,t1,t6\n";
        code_ << "    or t0,t5,t5\n";
    } else {
        code_ << "    cmplw t0,t3\n    bge " << Lcarry << "\n";  // unsigned: lhs_lo >= rhs_lo means no borrow
        code_ << "    li t6,1\n    b " << Ldone << "\n";
        code_ << Lcarry << ":\n    li t6,0\n";
        code_ << Ldone << ":\n";
        code_ << "    subf t5,t3,t0\n";          // t5 = t0 - t3
        code_ << "    subf t1,t4,t1\n";          // t1 = t1 - t4
        code_ << "    subf t1,t6,t1\n";          // t1 -= borrow
        code_ << "    or t0,t5,t5\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit
// product - the standard 32-on-32 cross-multiply decomposition, using
// `mullw` (low 32) and `mulhwu` (high 32) three times.
void PowerPCCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-8\n    stind sp,t0\n    stindd sp,t1,4\n";
    genExpr(rhs);
    code_ << "    or t5,t0,t0\n    or t6,t1,t1\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldindd sp,t4,4\n    addi sp,sp,8\n";  // t3:t4 = lhs (lo:hi)
    code_ << "    mullw t0,t3,t6\n";    // lhs_lo*rhs_hi
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    code_ << "    mullw t0,t4,t5\n";    // lhs_hi*rhs_lo
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    code_ << "    mullw t0,t3,t5\n    mulhwu t1,t3,t5\n";  // lhs_lo*rhs_lo (final result_lo/carry)
    code_ << "    ldind sp,t6\n    addi sp,sp,4\n";  // crossB_lo
    code_ << "    ldind sp,t5\n    addi sp,sp,4\n";  // crossA_lo
    code_ << "    add t1,t1,t6\n    add t1,t1,t5\n";
}

void PowerPCCodegen::genNegate64() {
    code_ << "    nor t0,t0,t0\n    nor t1,t1,t1\n";
    code_ << "    addi t0,t0,1\n";
    std::string Lcarry = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    bne " << Lcarry << "\n";
    code_ << "    addi t1,t1,1\n    b " << Ldone << "\n";
    code_ << Lcarry << ":\n";
    code_ << Ldone << ":\n";
}

// Classic bit-serial shift/compare/subtract long division - see
// I386Codegen.cpp's/MipsCodegen.cpp's own genWideDivMod() for the
// algorithm, ported onto the stack with $sp playing the role `ebx`/
// `$sp`+ldind-with-displacement plays there (this backend's own
// `ldindd`/`stindd` already take a displacement, same as MIPS's).
void PowerPCCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-8\n    stind sp,t1\n    stindd sp,t0,4\n";
    genExpr(rhs);
    code_ << "    addi sp,sp,-8\n    stind sp,t1\n    stindd sp,t0,4\n";
    code_ << "    addi sp,sp,-36\n";
    code_ << "    ldindd sp,t0,44\n    stindd sp,t0,0\n";
    code_ << "    ldindd sp,t0,40\n    stindd sp,t0,4\n";
    code_ << "    ldindd sp,t0,36\n    stindd sp,t0,8\n";
    code_ << "    ldindd sp,t0,32\n    stindd sp,t0,12\n";
    code_ << "    li t0,0\n    stindd sp,t0,16\n    stindd sp,t0,20\n";
    code_ << "    stindd sp,t0,24\n    stindd sp,t0,28\n";
    code_ << "    li t0,32\n    stindd sp,t0,32\n";

    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        code_ << "    ldindd sp,t0,4\n    srwi t0,t0,31\n    stindd sp,t0,40\n";
        code_ << "    ldindd sp,t0,12\n    srwi t0,t0,31\n    stindd sp,t0,44\n";
        code_ << "    ldindd sp,t0,40\n    cmpwi t0,0\n    beq " << LskipNegN << "\n";
        code_ << "    ldindd sp,t0,0\n    ldindd sp,t1,4\n";
        genNegate64();
        code_ << "    stindd sp,t0,0\n    stindd sp,t1,4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    ldindd sp,t0,44\n    cmpwi t0,0\n    beq " << LskipNegD << "\n";
        code_ << "    ldindd sp,t0,8\n    ldindd sp,t1,12\n";
        genNegate64();
        code_ << "    stindd sp,t0,8\n    stindd sp,t1,12\n";
        code_ << LskipNegD << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldindd sp,t0,32\n    cmpwi t0,0\n    beq " << Ldone << "\n";
    code_ << "    addi t0,t0,-1\n    stindd sp,t0,32\n";

    code_ << "    ldindd sp,t0,4\n    srwi t0,t0,31\n    or t3,t0,t0\n";
    code_ << "    ldindd sp,t0,0\n    srwi t0,t0,31\n    or t4,t0,t0\n";
    code_ << "    ldindd sp,t0,0\n    slwi t0,t0,1\n    stindd sp,t0,0\n";
    code_ << "    ldindd sp,t0,4\n    slwi t0,t0,1\n    or t0,t0,t4\n    stindd sp,t0,4\n";

    code_ << "    ldindd sp,t0,16\n    srwi t0,t0,31\n    or t5,t0,t0\n";
    code_ << "    ldindd sp,t0,16\n    slwi t0,t0,1\n    or t0,t0,t3\n    stindd sp,t0,16\n";
    code_ << "    ldindd sp,t0,20\n    slwi t0,t0,1\n    or t0,t0,t5\n    stindd sp,t0,20\n";

    code_ << "    ldindd sp,t0,20\n    ldindd sp,t1,12\n    cmpw t0,t1\n    bne " << LhiDiff << "\n";
    code_ << "    ldindd sp,t0,16\n    ldindd sp,t1,8\n    cmplw t0,t1\n    blt " << LsubNo << "\n";
    code_ << "    b " << LsubYes << "\n";
    code_ << LhiDiff << ":\n";
    code_ << "    cmplw t0,t1\n    blt " << LsubNo << "\n";
    code_ << "    b " << LsubYes << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    ldindd sp,t0,16\n    ldindd sp,t1,8\n    cmplw t0,t1\n    bge " << LshiftQ << "_nb\n";
    code_ << "    li t6,1\n    b " << LshiftQ << "_bdone\n";
    code_ << LshiftQ << "_nb:\n    li t6,0\n";
    code_ << LshiftQ << "_bdone:\n";
    code_ << "    subf t0,t1,t0\n    stindd sp,t0,16\n";
    code_ << "    ldindd sp,t0,20\n    ldindd sp,t1,12\n    subf t0,t1,t0\n    subf t0,t6,t0\n    stindd sp,t0,20\n";
    code_ << "    li t3,1\n    b " << LshiftQ << "\n";
    code_ << LsubNo << ":\n    li t3,0\n";
    code_ << LshiftQ << ":\n";

    code_ << "    ldindd sp,t0,24\n    srwi t0,t0,31\n    or t5,t0,t0\n";
    code_ << "    ldindd sp,t0,24\n    slwi t0,t0,1\n    or t0,t0,t3\n    stindd sp,t0,24\n";
    code_ << "    ldindd sp,t0,28\n    slwi t0,t0,1\n    or t0,t0,t5\n    stindd sp,t0,28\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    ldindd sp,t3,40\n    ldindd sp,t0,44\n    xor t3,t3,t0\n";
        code_ << "    cmpwi t3,0\n    beq " << LskipNegQ << "\n";
        code_ << "    ldindd sp,t0,24\n    ldindd sp,t1,28\n";
        genNegate64();
        code_ << "    stindd sp,t0,24\n    stindd sp,t1,28\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    ldindd sp,t3,40\n    cmpwi t3,0\n    beq " << LskipNegR << "\n";
        code_ << "    ldindd sp,t0,16\n    ldindd sp,t1,20\n";
        genNegate64();
        code_ << "    stindd sp,t0,16\n    stindd sp,t1,20\n";
        code_ << LskipNegR << ":\n";
    }
    code_ << "    ldindd sp,t0,24\n    ldindd sp,t1,28\n";
    code_ << "    ldindd sp,t4,16\n    ldindd sp,t5,20\n";
    code_ << "    addi sp,sp,52\n";
    if (wantRemainder) {
        code_ << "    or t0,t4,t4\n    or t1,t5,t5\n";
    }
}

// A `long long` shifted by a runtime count - a counted loop of 1-bit
// shifts, each carrying the bit that crosses the word boundary by
// hand (no hardware 64-bit shift).
void PowerPCCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-8\n    stind sp,t0\n    stindd sp,t1,4\n";
    genExpr(rhs);  // t0 = shift count
    code_ << "    or t3,t0,t0\n";
    code_ << "    ldind sp,t0\n    ldindd sp,t1,4\n    addi sp,sp,8\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmpwi t3,0\n    beq " << Ldone << "\n    addi t3,t3,-1\n";
    if (isLeft) {
        code_ << "    srwi t4,t0,31\n";
        code_ << "    slwi t0,t0,1\n";
        code_ << "    slwi t1,t1,1\n    or t1,t1,t4\n";
    } else {
        code_ << "    slwi t4,t1,31\n";
        code_ << "    srwi t0,t0,1\n    or t0,t0,t4\n";
        code_ << "    " << (isUnsigned ? "srwi" : "srawi") << " t1,t1,1\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison - compare the high words first (signed or
// unsigned per `isUnsigned`), falling back to an always-unsigned
// compare of the low words only when the high words are equal.
void PowerPCCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs,
                                      bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-8\n    stind sp,t0\n    stindd sp,t1,4\n";
    genExpr(rhs);
    code_ << "    or t5,t0,t0\n    or t6,t1,t1\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldindd sp,t4,4\n    addi sp,sp,8\n";  // t3:t4 = lhs (lo:hi)
    if (op == "==" || op == "!=") {
        std::string Lfalse = newLabel(), Ldone = newLabel();
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmpw t3,t5\n    bne " << Lfalse << "\n";
        code_ << "    cmpw t4,t6\n    bne " << Lfalse << "\n";
        code_ << "    li t0," << onEqual << "\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    li t0," << onDiffer << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string LhiDiff = newLabel(), Ldone = newLabel(), Lt = newLabel(), Lf = newLabel();
    code_ << "    cmpw t4,t6\n    bne " << LhiDiff << "\n";
    const char *loBranch = op == "<" ? "blt" : op == ">" ? "bgt" : op == "<=" ? "ble" : "bge";
    code_ << "    cmplw t3,t5\n    " << loBranch << " " << Lt << "\n    b " << Lf << "\n";
    code_ << Lt << ":\n    li t0,1\n    b " << Ldone << "\n";
    code_ << Lf << ":\n    li t0,0\n    b " << Ldone << "\n";
    code_ << LhiDiff << ":\n";
    const char *hiCmp = isUnsigned ? "cmplw" : "cmpw";
    code_ << "    " << hiCmp << " t4,t6\n    " << loBranch << " " << Lt << "\n    b " << Lf << "\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "+") {
            code_ << "    add t0,t1,t0\n";
        } else {
            code_ << "    subf t0,t0,t1\n";  // t0 = t1 - t0 = lhs - rhs
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = op == "&" ? "and" : op == "|" ? "or" : "xor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    addi sp,sp,-8\n    stind sp,t0\n    stindd sp,t1,4\n";
            genExpr(*e.rhs);
            code_ << "    or t3,t0,t0\n    or t4,t1,t1\n";
            code_ << "    ldind sp,t0\n    ldindd sp,t1,4\n    addi sp,sp,8\n";
            code_ << "    " << insn << " t0,t0,t3\n    " << insn << " t1,t1,t4\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addi sp,sp,4\n";
        code_ << "    " << insn << " t0,t1,t0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        code_ << "    or t1,t0,t0\n";
        code_ << "    ldind sp,t0\n    addi sp,sp,4\n";  // t0 = value to shift
        const char *insn = op == "<<" ? "slw" : (isUnsigned ? "srw" : "sraw");
        code_ << "    " << insn << " t0,t0,t1\n";
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
// Signedness doesn't matter for the low-order 32 bits mullw keeps.
void PowerPCCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    mullw t0,t1,t0\n";
}

// Real hardware divide (`divw`/`divwu`) gives only the quotient -
// unlike MIPS's own `div` (which leaves both quotient and remainder
// ready to pull out of HI/LO), classic PowerPC has no remainder
// instruction at all, so `%` is synthesized the standard way: divide
// for the quotient, `mullw` to multiply the quotient back out by the
// divisor, then `subf` to recover the remainder as `dividend -
// quotient * divisor`.
void PowerPCCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    const char *divInsn = isUnsigned ? "divwu" : "divw";
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    if (!wantRemainder) {
        code_ << "    " << divInsn << " t0,t1,t0\n";
        return;
    }
    code_ << "    " << divInsn << " t2,t1,t0\n";  // t2 = quotient (t1/t0)
    code_ << "    mullw t2,t2,t0\n";               // t2 = quotient * divisor
    code_ << "    subf t0,t2,t1\n";                // t0 = t1 - t2 = dividend - quotient*divisor
}

// PowerPC has no single compare-and-set instruction the way MIPS's
// own `slt` is - a comparison here always needs `cmpw`/`cmplw` plus a
// real conditional branch to decide which boolean value to
// materialize, the same "compare, then branch to decide" shape as the
// 68000's own CMP+Bcc. `isUnsigned` (the shared operand type's own
// signedness) picks `cmpw` vs `cmplw`.
void PowerPCCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    " << (isUnsigned ? "cmplw" : "cmpw") << " t1,t0\n";
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
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    cmpwi t0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    li t0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    li t0,1\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpwi t0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    cmpwi t0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    li t0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    li t0,0\n";
    code_ << Ldone << ":\n";
}

void PowerPCCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
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
