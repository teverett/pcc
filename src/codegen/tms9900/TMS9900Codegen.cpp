#include "TMS9900Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "TMS9900Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Code base; the simulator sets PC here directly, no reset vector. WP (the Workspace
// Pointer - a real, separate CPU register, not one of R0-R15 themselves) must point
// somewhere in RAM before R0-R15 mean anything at all; this backend always points it here,
// 32 bytes (R0-R15) comfortably inside ordinary RAM and, not coincidentally, the same address
// the TI-99/4A's own 256-byte scratchpad RAM starts at ($8300) - see docs/platforms.md's own
// `tms9900` entry.
constexpr uint32_t kBaseAddress = 0x2000;
constexpr uint32_t kWorkspaceBase = 0x8300;
// Initial r15 (software stack pointer - the TMS9900 has no hardware PUSH/POP of its own);
// grows down into ordinary RAM, well clear of both the workspace and any reasonably-sized
// program's own code.
constexpr uint32_t kInitialSP = 0xF000;
}  // namespace

std::string TMS9900Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int16_t>(static_cast<uint16_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void TMS9900Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                   Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    if (w > 2) failWide(0);
    const char *directive = w == 1 ? "db" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
    // Every instruction and every word-sized memory reference needs an even address; only a
    // `db` array can leave the running offset odd, so pad it back to even here.
    if (w == 1 && (values.size() % 2) != 0) trailer_.push_back("db 0");
}

void TMS9900Codegen::failWide(int line) const {
    err(line, "internal: 'long'/'long long' isn't implemented for --target=tms9900 yet - see "
               "TMS9900Codegen.h's own header comment");
}

std::string TMS9900Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lwpi " << kWorkspaceBase << "\n";
        code_ << "    li r15," << kInitialSP << "\n";
        code_ << "    bl [" << label(mainFn) << "]\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else if (slot->type.base == BaseType::Struct) {
            declareArray(lbl, std::vector<ir::ArrayInitElem>(slot->type.structSize), Type{BaseType::Char, false});
        } else {
            int w = widthOfType(slot->type);
            if (w > 2) failWide(0);
            long v = slot->hasInit ? slot->initValue : 0;
            const char *directive = w == 1 ? "db" : "dw";
            trailer_.push_back(lbl + ": " + directive + " " + std::to_string(static_cast<int16_t>(v)) + "  ; " +
                                slot->sourceName);
            if (w == 1) trailer_.push_back("db 0");
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> TMS9900Codegen::perFileAsm() const { return perFileAsm_; }

// Prologue saves the real return-address register (r11, set by `bl` - see this class's own
// header comment) and the caller's frame pointer, establishes this call's own frame pointer,
// then reserves `frameSize` bytes of locals below it - all via the software stack `r15` bump-
// allocates. The epilogue (shared by every `return` and the fall-through exit) undoes exactly
// that, then returns via "b [r11]" - the real, documented TMS9900 subroutine-return idiom.
void TMS9900Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunction_ = fn.func;
    code_ << label(fn.func) << ":\n";
    code_ << "    dect r15\n    mov r11,[r15]\n";
    code_ << "    dect r15\n    mov r10,[r15]\n";
    code_ << "    mov r15,r10\n";
    if (fn.func->frameSize > 0) code_ << "    ai r15,-" << fn.func->frameSize << "\n";

    genStmt(*fn.body);

    code_ << "    mov r10,r15\n";
    code_ << "    mov [ir15],r10\n";
    code_ << "    mov [ir15],r11\n";
    code_ << "    b [r11]\n\n";
}

void TMS9900Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) failWide(s.line);
            code_ << "    ci r0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jeq " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            if (isWide(s.cond->type)) failWide(s.line);
            code_ << "    ci r0,0\n    jeq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    mov r10,r15\n";
            code_ << "    mov [ir15],r10\n";
            code_ << "    mov [ir15],r11\n";
            code_ << "    b [r11]\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeOperand(addr(s.declSlot), s.declSlot->type);
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
            code_ << "    jmp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void TMS9900Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) failWide(e.line);
            code_ << "    li r0," << static_cast<int16_t>(e.intValue) << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r4 = target address
                pushR4();
                genExpr(*e.rhs);
                popR4();
                storeIndirect(e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r0,r4\n";
            pushR4();
            genExpr(*e.rhs);
            popR4();
            storeIndirect(e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);
                genCompoundThroughPtr(e);
                return;
            }
            std::string mem = operandOf(e.slot);
            genExpr(*e.rhs);  // r0 = rhs
            code_ << "    mov r0,r1\n";
            loadOperand(mem, e.type);  // r0 = old
            if (e.yieldOld) pushR0();
            genCompoundOp(e.op, e.type);
            narrowR0To(e.type);
            storeOperand(mem, e.type);
            if (e.yieldOld) popR0();
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r0,r4\n";
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) failWide(e.line);
            code_ << "    ci r0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jeq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) failWide(e.line);
                code_ << "    neg r0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r0,r4\n";
                loadIndirect(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) failWide(e.line);
                code_ << "    inv r0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed right-to-left so param 0 ends up closest to the saved
            // return address/frame pointer, matching every Param's own ascending
            // frameOffset (see addr()'s own comment); the caller deallocates them afterward.
            // Every argument reserves exactly one native word - there's no wider-than-int
            // argument this backend supports yet (see failWide()).
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) failWide(e.line);
                pushR0();
                totalBytes += 2;
            }
            code_ << "    bl [" << label(e.func) << "]\n";
            if (totalBytes > 0) code_ << "    ai r15," << totalBytes << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r0,r4\n";
            // Saves/restores r11 around the call exactly like a real `bl` would via another
            // register, since the callee's own prologue/epilogue already expects `bl`'s own
            // return-address convention - `bl` itself needs a Symbolic/label operand, so an
            // indirect call instead uses `b` to transfer control and plants the return address
            // (the next instruction after this sequence) into r11 itself first.
            std::string Lret = newLabel();
            code_ << "    li r11," << Lret << "\n";
            code_ << "    b [r4]\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            (void)e;  // no vector table set up - "ion"/"iof" are no-ops here, same as other backends
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// r4 holds the computed address on return. For an array (slot->arraySize > 0), the base is a
// compile-time-known address; otherwise `slot` is a plain pointer variable, so the base comes
// from reading its stored value.
void TMS9900Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift >= 0) {
        if (shift > 0) code_ << "    sla r0," << shift << "\n";
    } else {
        // A struct element's non-power-of-two width - scale via the real hardware multiply
        // (always exact: both operands fit in 16 bits).
        code_ << "    li r1," << widthOfType(slot->type) << "\n";
        code_ << "    mov r0,r2\n    mpy r1,r2\n    mov r3,r0\n";
    }
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    li r4," << label(slot) << "\n";
        } else {
            int base = slot->kind == ir::SlotKind::Param ? slot->frameOffset
                                                           : -ir::localLowOffset(*slot, typeWidths());
            code_ << "    li r4," << base << "\n    a r10,r4\n";
        }
    } else {
        code_ << "    mov " << operandOf(slot) << ",r4\n";
    }
    code_ << "    a r0,r4\n";
}

// r0=old, r1=rhs on entry (CompoundAssign's own contract); leaves the un-narrowed new value in
// r0. `type` is never wide here (IRBuilder's rejectIfWide() guarantees it).
void TMS9900Codegen::genCompoundOp(const std::string &op, Type type) {
    if (op == "+") {
        code_ << "    a r1,r0\n";
        return;
    }
    if (op == "-") {
        code_ << "    s r1,r0\n";
        return;
    }
    if (op == "*") {
        code_ << "    mov r1,r2\n    mpy r0,r2\n    mov r3,r0\n";
        return;
    }
    if (op == "/" || op == "%") {
        genDivMod(type.isUnsigned, op == "%");
        return;
    }
    // TMS9900 has no plain AND instruction - only SZC ("set zeros corresponding", i.e.
    // dst &= ~src) and SOC ("set ones corresponding", dst |= src). AND is synthesized as
    // "dst &= ~(~src)", i.e. invert a copy of src first, then SZC it in.
    if (op == "&") {
        code_ << "    mov r1,r2\n    inv r2\n    szc r2,r0\n";
        return;
    }
    if (op == "|") {
        code_ << "    soc r1,r0\n";
        return;
    }
    if (op == "^") {
        code_ << "    xor r1,r0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        genShiftCore(op == "<<", type.isUnsigned);
        return;
    }
    err(0, "internal: unhandled compound operator '" + op + "'");
}

// r4 must hold the resolved address on entry; stashed across e.rhs's evaluation since that can
// recursively clobber every register.
void TMS9900Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    pushR4();
    genExpr(*e.rhs);       // r0 = rhs
    code_ << "    mov r0,r1\n";  // r1 = rhs, held across the r4 reload below
    popR4();                 // r4 = address
    loadIndirect(e.type);     // r0 = old
    if (e.yieldOld) pushR0();
    genCompoundOp(e.op, e.type);  // r0 = new (raw)
    narrowR0To(e.type);
    if (e.yieldOld) {
        code_ << "    mov r0,r2\n";  // r2 = new
        popR0();                       // r0 = old
        code_ << "    mov r0,r3\n";     // r3 = old, preserved across the store below
        code_ << "    mov r2,r0\n";      // r0 = new (storeIndirect always stores from r0)
        storeIndirect(e.type);               // r4 still addresses the target
        code_ << "    mov r3,r0\n";            // r0 = old (final result)
    } else {
        storeIndirect(e.type);
    }
}

void TMS9900Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    if (w > 2) failWide(0);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int byteOffset = static_cast<int>(i) * w;
        const char *mnemonic = w == 1 ? "movb" : "mov";
        code_ << "    li r0," << arrayElemText(elem) << "\n";
        code_ << "    " << mnemonic << " r0," << addr(slot, byteOffset) << "\n";
    }
}

void TMS9900Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadIndirect(e.slot->type);
}

void TMS9900Codegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    if (w > 2) failWide(0);
    if (w == 1) {
        // r0's real bits, on entry, sit in its own low byte (every arithmetic op here leaves
        // them there) - unsigned re-truncation is a plain mask; signed re-extension needs the
        // byte moved up into the high-byte position first (SLA 8) so its own bit 7 lands on
        // r0's bit 15, then arithmetic-shifted back down (SRA 8) to sign-extend from there.
        if (to.isUnsigned) {
            code_ << "    andi r0,255\n";
        } else {
            code_ << "    sla r0,8\n    sra r0,8\n";
        }
    }
    // 2 bytes: nothing to do (r0 is already a full native word).
}

// `type`'s real bits for a `char` sit in the high byte of the addressed word (MOVB always
// touches a word's own most-significant byte - see TMS9900Assembler.h's own header comment),
// so a byte load/store always goes through r0's high byte, then is moved down into the low-
// byte position (sign/zero-extended per `type`) every other value in this backend keeps.
void TMS9900Codegen::loadOperand(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w > 2) failWide(0);
    if (w == 1) {
        code_ << "    movb " << mem << ",r0\n";
        if (type.isUnsigned) {
            code_ << "    srl r0,8\n";
        } else {
            code_ << "    sra r0,8\n";
        }
    } else {
        code_ << "    mov " << mem << ",r0\n";
    }
}

void TMS9900Codegen::storeOperand(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w > 2) failWide(0);
    if (w == 1) {
        code_ << "    sla r0,8\n    movb r0," << mem << "\n    sra r0,8\n";
    } else {
        code_ << "    mov r0," << mem << "\n";
    }
}

void TMS9900Codegen::loadIndirect(Type type) {
    int w = widthOfType(type);
    if (w > 2) failWide(0);
    if (w == 1) {
        code_ << "    movb [r4],r0\n";
        code_ << "    " << (type.isUnsigned ? "srl r0,8\n" : "sra r0,8\n");
    } else {
        code_ << "    mov [r4],r0\n";
    }
}

void TMS9900Codegen::storeIndirect(Type type) {
    int w = widthOfType(type);
    if (w > 2) failWide(0);
    if (w == 1) {
        code_ << "    sla r0,8\n    movb r0,[r4]\n    sra r0,8\n";
    } else {
        code_ << "    mov r0,[r4]\n";
    }
}

void TMS9900Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    if (isWide(from) || isWide(to)) failWide(e.line);
    genExpr(*e.lhs);
    narrowR0To(to);
}

void TMS9900Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    li r0," << label(operand.slot) << "\n";
        } else {
            int off = operand.slot->kind == ir::SlotKind::Param
                          ? operand.slot->frameOffset + paramByteFixup(operand.slot)
                          : -ir::localOffsetDescendingWide(*operand.slot, typeWidths());
            code_ << "    li r0," << off << "\n    a r10,r0\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r4,r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    li r0," << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void TMS9900Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    Type operandType = e.lhs->type;
    bool isUnsigned = operandType.isUnsigned;
    if (isWide(operandType) && op != "&&" && op != "||") failWide(e.line);
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        pushR0();
        genExpr(*e.rhs);
        popR1();  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    a r1,r0\n";
        } else {
            code_ << "    s r0,r1\n";  // r1 = r1 - r0 = lhs - rhs
            code_ << "    mov r1,r0\n";
        }
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        return;
    }
    if (op == "/" || op == "%") {
        genExpr(*e.lhs);
        pushR0();
        genExpr(*e.rhs);
        code_ << "    mov r0,r1\n";
        popR0();  // r0 = lhs (dividend), r1 = rhs (divisor)
        genDivMod(isUnsigned, op == "%");
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        genExpr(*e.lhs);
        pushR0();
        genExpr(*e.rhs);
        popR1();  // r1 = lhs, r0 = rhs
        if (op == "&") {
            code_ << "    mov r0,r2\n    inv r2\n    szc r2,r1\n    mov r1,r0\n";
        } else if (op == "|") {
            code_ << "    soc r0,r1\n    mov r1,r0\n";
        } else {
            code_ << "    xor r0,r1\n    mov r1,r0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        pushR0();
        genExpr(*e.rhs);  // r0 = count
        code_ << "    mov r0,r1\n";
        popR0();  // r0 = value, r1 = count
        genShiftCore(op == "<<", isUnsigned);
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

// r0=value, r1=count on entry (either real order genBinary/genCompoundOp naturally produces);
// the hardware shift instructions can only take their count from an immediate 1-15 or, when
// the instruction's own count field is 0, from r0's own low nibble - so the value is moved out
// of r0's way into r2 first, then the count into r0, before shifting.
void TMS9900Codegen::genShiftCore(bool isLeft, bool isUnsigned) {
    code_ << "    mov r0,r2\n";
    code_ << "    mov r1,r0\n";
    const char *insn = isLeft ? "sla" : (isUnsigned ? "srl" : "sra");
    code_ << "    " << insn << " r2,0\n";
    code_ << "    mov r2,r0\n";
}

// r0=rhs, r1=lhs preset (genBinary's own arrangement before calling this) - multiplication
// commutes, so which operand ends up as MPY's "source" vs "destination" register doesn't
// matter. The real hardware MPY always produces a correct 32-bit product; only the low word
// (the truncated 16x16->16 result C's `int*int` actually wants) is kept - truncated
// multiplication is identical whether the inputs are signed or unsigned (modular arithmetic),
// so no separate signed path is needed.
void TMS9900Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushR0();
    genExpr(rhs);
    popR1();
    code_ << "    mov r1,r2\n    mpy r0,r2\n    mov r3,r0\n";
}

// r0=dividend, r1=divisor preset. The real hardware DIV is unsigned-only (a 32-bit dividend in
// a register pair divided by a 16-bit divisor, giving a 16-bit quotient/remainder - the
// dividend's own high word must be strictly less than the divisor, trivially true here since
// it's always seeded to 0). Signed division negates both operands to magnitudes first, divides
// unsigned, then restores the quotient's sign (negative iff the original signs differed) and
// the remainder's sign (always the dividend's own, matching C's truncating-division
// convention) - the TMS9900 has no signed divide instruction at all.
void TMS9900Codegen::genDivMod(bool isUnsigned, bool wantRemainder) {
    if (isUnsigned) {
        code_ << "    li r2,0\n    mov r0,r3\n    div r1,r2\n";
        code_ << "    mov " << (wantRemainder ? "r3" : "r2") << ",r0\n";
        return;
    }
    std::string Lskip1 = newLabel(), Lskip2 = newLabel();
    code_ << "    mov r0,r4\n";  // r4 = original dividend (sign + remainder-sign source)
    code_ << "    mov r1,r5\n";  // r5 = original divisor (sign source)
    code_ << "    ci r0,0\n    jhe " << Lskip1 << "\n    neg r0\n";
    code_ << Lskip1 << ":\n";
    code_ << "    ci r1,0\n    jhe " << Lskip2 << "\n    neg r1\n";
    code_ << Lskip2 << ":\n";
    code_ << "    li r2,0\n    mov r0,r3\n    div r1,r2\n";  // r2 = |quotient|, r3 = |remainder|
    code_ << "    mov r4,r6\n    xor r5,r6\n";  // r6's sign bit: original signs differed?
    std::string LnoQNeg = newLabel();
    code_ << "    ci r6,0\n    jhe " << LnoQNeg << "\n    neg r2\n";
    code_ << LnoQNeg << ":\n";
    std::string LnoRNeg = newLabel();
    code_ << "    ci r4,0\n    jhe " << LnoRNeg << "\n    neg r3\n";
    code_ << LnoRNeg << ":\n";
    code_ << "    mov " << (wantRemainder ? "r3" : "r2") << ",r0\n";
}

void TMS9900Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) failWide(lhs.line);
    bool isUnsigned = lhs.type.isUnsigned;
    if (op == "==") return emitCompare(lhs, rhs, "jeq");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "jl" : "jlt");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "jh" : "jgt");
    // The TMS9900 has no direct signed ">=" or unsigned "<=" jump - computed instead by
    // swapping which operand is the destination of `c` and testing the opposite-direction
    // condition that *is* available (see emitCompare()'s own header comment): "lhs<=rhs"
    // unsigned has no direct jump, but "rhs>=lhs" (jhe) does, and they're equivalent; signed
    // "<=" already has a direct jle, so only the unsigned case needs the swap, and
    // symmetrically only the signed case needs it for ">=".
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "jhe" : "jle", /*swapOperands=*/isUnsigned);
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "jhe" : "jle", /*swapOperands=*/!isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void TMS9900Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue,
                                  bool swapOperands) {
    genExpr(lhs);
    pushR0();
    genExpr(rhs);
    popR1();  // r1 = lhs, r0 = rhs
    // `c src,dst` sets condition codes for "dst compared to src". Ordinarily src=rhs(r0),
    // dst=lhs(r1), so the codes directly read as "lhs <cond> rhs"; `swapOperands` computes
    // "rhs <cond> lhs" instead (used only for the two conditions with no direct jump - see
    // genCompare()'s own comment for why that's exactly equivalent).
    if (swapOperands) {
        code_ << "    c r1,r0\n";
    } else {
        code_ << "    c r0,r1\n";
    }
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    li r0,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    li r0,1\n";
    code_ << Ldone << ":\n";
}

void TMS9900Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type) || isWide(rhs.type)) failWide(lhs.line);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ci r0,0\n    jeq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    ci r0,0\n    jeq " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    li r0,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    li r0,1\n";
    code_ << Ldone << ":\n";
}

void TMS9900Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type) || isWide(rhs.type)) failWide(lhs.line);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ci r0,0\n    jne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    ci r0,0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    li r0,1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    li r0,0\n";
    code_ << Ldone << ":\n";
}

void TMS9900Codegen::genLogicalNot(const ir::Expr &rhs) {
    if (isWide(rhs.type)) failWide(rhs.line);
    genExpr(rhs);
    code_ << "    ci r0,0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jeq " << Ltrue << "\n";
    code_ << "    li r0,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    li r0,1\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> TMS9900Codegen::assembleInProcess(const std::string &asmText,
                                                               const std::string &format) const {
    if (format == "raw") return assembleTMS9900(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleTMS9900Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
