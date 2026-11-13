#include "PowerPCCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "PowerPCAssembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing; the test harness
// sets the simulated CPU's PC here directly.
constexpr uint32_t kBaseAddress = 0x00100000u;

// Where this backend's entry sequence points r1 before calling main(),
// above the loaded program, growing down.
constexpr uint32_t kInitialSP = 0x00200000u;
}  // namespace

// A literal element's decimal text; a symbol element is the referenced
// Slot's or Function's label. "dw" also records the same external-name/
// base-shift relocation a lis/ori address immediate gets.
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
    // `objectMode` means no object file emits this entry sequence itself;
    // PowerPCLinkerFactory.cpp synthesizes it once every object is merged.
    // An object file's base is `org 0` - placement is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lis sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    ori sp,sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in t0 the moment execution reaches `halt`.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; it must never get
        // a storage word here, only be referenced by name, letting the
        // assembler's undefined-symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else if (slot->type.base == BaseType::Struct) {
            // No struct initializer exists in this subset, so this is
            // always zero-filled; reuses declareArray's own element-list
            // emission (one zero "db" per byte) rather than a new
            // directive, since this assembler dialect has no "reserve N
            // zero bytes" form of its own.
            declareArray(lbl, std::vector<ir::ArrayInitElem>(slot->type.structSize), Type{BaseType::Char, false});
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

// Single-file-only, as in MipsCodegen.cpp (no `--format=coff` here).
std::map<std::string, std::string> PowerPCCodegen::perFileAsm() const { return perFileAsm_; }

// Saves/restores the link register (via `mflr`/`mtlr` through `r0` -
// PowerPC's LR isn't directly memory-addressable) around every function,
// leaf or not, since any call overwrites LR.
//
// The prologue pushes the saved return address then the caller's `fp`,
// then points `fp` at the second pushed word: `fp+0` is the caller's `fp`,
// `fp+4` is the saved return address, `fp+8+4*i` is argument i (matching
// slotDisp()'s Param displacement). `addi sp,sp,-4*frameSize` then reserves
// this call's Locals below `fp` (matching slotDisp()'s Local displacement)
// - each call gets its own `fp`, chained through the stack, making
// recursion safe.
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

// Restores `fp` and the link register, leaving `sp` at argument 0 again.
// The caller, not the callee, pops the arguments it pushed (caller-cleans-
// up convention, as in I386Codegen.cpp).
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
            // An early exit needs the same frame teardown inline before
            // `blr`; `t0` (the return value) is never touched by it.
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
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "    b " << irLabel(s.labelId) << "\n";
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
            // Stores at e.type's width/signedness - the pointee type, or
            // native-int width for untyped-int-as-address.
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
            genExpr(*e.rhs);  // t0, or t1:t0 if wide
            code_ << "    ldind sp,t2\n    addi sp,sp,4\n";
            storeOperand("t2", "t0", e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // t2 = address
                genCompoundThroughT2(e);
                return;
            }
            // loadSlotValue/storeSlotValue re-address a plain scalar target
            // fresh each time (no side effect to preserve), so rhs
            // evaluates first, same order as plain Assign above.
            genExpr(*e.rhs);  // t0 = rhs
            code_ << "    or t1,t0,t0\n";  // t1 = t0 ("or rX,rY,rY" is PowerPC's register-move idiom)
            loadSlotValue("t0", e.slot);  // t0 = old
            if (e.yieldOld) code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
            genCompoundOp(e.op, e.type);
            narrowT0To(e.type);
            storeSlotValue(e.slot, "t0");
            if (e.yieldOld) code_ << "    ldind sp,t0\n    addi sp,sp,4\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    or t2,t0,t0\n";  // t2 = t0
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    or t0,t0,t1\n";
            code_ << "    cmpwi t0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    b " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
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
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // t0 = pointer value
                code_ << "    or t2,t0,t0\n";  // t2 = t0
                loadOperand("t2", "t0", e.type);
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
            // Pushes arguments right-to-left so argument 0 ends up closest
            // to the saved return address once the callee's prologue runs
            // (`bl` never touches `sp`). The caller cleans the arguments
            // back off afterward, since the callee's epilogue only
            // restores `sp` to where it was on entry.
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
            // Real PowerPC has no single-instruction indirect call - `bl`'s
            // target is always a compile-time-relative displacement, never
            // a register - so this uses the standard two-instruction idiom:
            // `mtctr` moves the callee's address into the Count Register,
            // then `bctrl` branches to it and sets the link register.
            genExpr(*e.lhs);  // t0 = the callee's entry address
            code_ << "    mtctr t0\n";
            code_ << "    bctrl\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No interrupt setup exists on this bare-metal target, so
            // interrupt enable/disable is a no-op, as in MipsCodegen.cpp.
            (void)e;
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// `arraySize > 0` means `slot` itself is the array, so the base address is
// either a compile-time-known label (a Global) or `fp+slotDisp(slot)` (a
// Local); either way the scaled index is added. `arraySize == 0` means `slot` is a plain pointer
// variable, so the base comes from reading its stored value instead.
void PowerPCCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) {
        code_ << "    slwi t0,t0," << shift << "\n";
    } else if (shift < 0) {
        // Non-power-of-two element width (a struct's): scale by a real multiply instead.
        loadImmediate("t1", widthOfType(slot->type));
        code_ << "    mullw t0,t0,t1\n";
    }
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("t2", label(slot));
            code_ << "    add t2,t2,t0\n";
        } else {
            code_ << "    addi t2,fp," << slotDisp(slot) << "\n";
            code_ << "    add t2,t2,t0\n";
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

void PowerPCCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add t0,t1,t0\n"; return; }
    // subf t0,t1,t0 = t0 - t1 = old - rhs.
    if (op == "-") { code_ << "    subf t0,t1,t0\n"; return; }
    if (op == "*") { code_ << "    mullw t0,t1,t0\n"; return; }
    if (op == "&") { code_ << "    and t0,t1,t0\n"; return; }
    if (op == "|") { code_ << "    or t0,t1,t0\n"; return; }
    if (op == "^") { code_ << "    xor t0,t1,t0\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "slw" : (isUnsigned ? "srw" : "sraw");
        code_ << "    " << insn << " t0,t0,t1\n";
        return;
    }
    // "/" or "%": t0 = old, t1 = rhs (divisor). "%" recovers the remainder
    // via multiply-subtract, since PowerPC has no remainder instruction.
    // t5 (never t2) holds the quotient scratch, since t2 is this backend's
    // dedicated address register, still needed intact for the store after.
    const char *divInsn = isUnsigned ? "divwu" : "divw";
    if (op == "%") {
        code_ << "    " << divInsn << " t5,t0,t1\n";  // t5 = old / rhs
        code_ << "    mullw t5,t5,t1\n";               // t5 = quotient * rhs
        code_ << "    subf t0,t5,t0\n";                // t0 = old - quotient*rhs = remainder
    } else {
        code_ << "    " << divInsn << " t0,t0,t1\n";
    }
}

// t2 must already hold the resolved address on entry; it is stashed on the
// real hardware stack (PowerPC has no dedicated push/pop) across evaluating
// `e.rhs`, which may recursively clobber every other register.
void PowerPCCodegen::genCompoundThroughT2(const ir::Expr &e) {
    code_ << "    addi sp,sp,-4\n    stind sp,t2\n";
    genExpr(*e.rhs);               // t0 = rhs
    code_ << "    or t1,t0,t0\n";   // t1 = rhs, held across the t2 reload below
    code_ << "    ldind sp,t2\n    addi sp,sp,4\n";  // t2 = address
    loadOperand("t2", "t0", e.type);  // t0 = old
    if (e.yieldOld) code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genCompoundOp(e.op, e.type);          // t0 = new (raw)
    narrowT0To(e.type);
    if (e.yieldOld) {
        code_ << "    or t3,t0,t0\n";               // t3 = new
        code_ << "    ldind sp,t0\n    addi sp,sp,4\n";  // t0 = old
        code_ << "    or t4,t0,t0\n";                 // t4 = old, preserved across the store below
        code_ << "    or t0,t3,t3\n";                  // t0 = new (storeOperand always stores from t0)
        storeOperand("t2", "t0", e.type);
        code_ << "    or t0,t4,t4\n";                       // t0 = old (final result)
    } else {
        storeOperand("t2", "t0", e.type);  // t0 already = new; t2 still addresses the target
    }
}

// A Local array's address isn't a compile-time-resolvable label the way a
// Global array's is, so (unlike declareArray()'s `dw` trailer line) this
// runs as real code, one element at a time, each time this is reached.
void PowerPCCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int disp = slotDisp(slot) + w * static_cast<int>(i);
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
        code_ << "    or t0,t2,t2\n";  // t0 = t2
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
        code_ << "    or t0,t2,t2\n";  // t0 = t2
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

// IRBuilder already promotes any operand narrower than `int`, so the only
// real work here is crossing the native-int/`long long` boundary (sign/
// zero-extending t0 into t1, or dropping t1), or re-narrowing t0.
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

// `long long` add/sub. This dialect exposes no carry-flag instruction, so
// the carry/borrow out of the low-word op is recovered via `cmplw` plus a
// conditional branch, the same way a comparison materializes a boolean.
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

// `long long` multiply, keeping the low 64 bits of the true 128-bit product
// via the standard 32-on-32 cross-multiply decomposition.
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

// Classic bit-serial shift/compare/subtract long division, as in
// I386Codegen.cpp's/MipsCodegen.cpp's genWideDivMod(), using the stack for
// scratch state via `ldindd`/`stindd`'s displacement.
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

// mullw's low-order 32 bits are the same whether operands are signed or
// unsigned.
void PowerPCCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addi sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addi sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    mullw t0,t1,t0\n";
}

// `divw`/`divwu` give only the quotient - unlike MIPS's `div` (HI/LO) -
// so `%` is synthesized: divide, `mullw` the quotient back out, `subf` to
// recover the remainder as `dividend - quotient * divisor`.
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

// PowerPC has no compare-and-set instruction (unlike MIPS's `slt`), so this
// needs `cmpw`/`cmplw` plus a conditional branch to materialize the boolean.
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

std::optional<std::string> PowerPCCodegen::assembleInProcess(const std::string &asmText,
                                                               const std::string &format) const {
    if (format == "raw") return assemblePowerPC(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral
    // ObjectFile, then picks that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePowerPCObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
