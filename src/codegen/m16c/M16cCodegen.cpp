#include "M16cCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "M16cAssembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - same
// "bare flat binary, test harness sets PC here directly" convention
// as every other target's own `kBaseAddress` (see
// Pdp11Codegen.cpp/M68kCodegen.cpp), chosen well clear of the M16C's
// own fixed low-memory special-function-register block (0x0000-
// 0x03FF on real silicon) even though nothing here ever touches it.
constexpr uint32_t kBaseAddress = 0x0400;
// Where this backend's own entry sequence points SP (the real
// hardware stack pointer, used by `push.w`/`pop.w` and by
// `jsr.w`/`rts`) before calling main() - comfortably above the loaded
// program and its data, growing down into otherwise-unused memory.
constexpr uint32_t kInitialSp = 0xE000;
}  // namespace

void M16cCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

void M16cCodegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << static_cast<int>(static_cast<uint16_t>(values[i]));
    }
    trailer_.push_back(line.str());
}

std::string M16cCodegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the m16c backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see M16cCodegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    code_ << "org " << kBaseAddress << "\n\n";
    code_ << "_start:\n";
    code_ << "    mov.w " << kInitialSp << ",sp\n";
    code_ << "    jsr.w " << label(mainFn) << "\n";
    // The result sits in R0 the moment execution reaches `halt` - the
    // same "a real, directly inspectable register" rationale as the
    // pdp11/68000 backends' own R0/D0 (see M16cCodegen.h's own header
    // comment).
    code_ << "halt:\n    jmp halt\n\n";

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // never possible here (no real separate compilation - see above), but
                                          // matches every other backend's own defensive check at this exact spot
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<int>(static_cast<uint16_t>(v))) + "  ; " +
                                slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> M16cCodegen::perFileAsm() const { return perFileAsm_; }

void M16cCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    rts\n\n";
}

void M16cCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmp.w 0,r0\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
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
            std::string Lstart = newLabel(), Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    cmp.w 0,r0\n";
            code_ << "    jeq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    rts\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareArray(lbl, s.declSlot->arrayInit);
            } else {
                declareTemp(lbl);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    code_ << "    mov.w r0," << lbl << "\n";
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

void M16cCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    mov.w " << static_cast<int>(static_cast<uint16_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    mov.w " << label(e.slot) << ",r0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                code_ << "    push.w a0\n";
                genExpr(*e.rhs);
                code_ << "    pop.w a0\n";
                code_ << "    st r0,a0\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    mov.w r0," << label(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov.w r0,a0\n";
            code_ << "    push.w a0\n";
            genExpr(*e.rhs);
            code_ << "    pop.w a0\n";
            code_ << "    st r0,a0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg.w r0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov.w r0,a0\n";
                code_ << "    ld a0,r0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not.w r0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "    mov.w r0,p_" << e.func->sourceName << "_" << i << "\n";
            }
            code_ << "    jsr.w " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real M16C flag-register interrupt enable/disable -
            // `FSET I`/`FCLR I`, the same real hardware feature as the
            // 8080/6502/8086 backends' own `ei`/`di`/`cli`/`sti` (see
            // I8080Codegen.cpp).
            code_ << "    " << (e.op == "ion" ? "fset i" : "fclr i") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in `A0` - see
// M16cCodegen.h's own header comment on why that's a dedicated
// address register, never aliasing `R0`/`R1`. `slot->arraySize > 0`
// means the base is a compile-time-known label (an address, loaded
// via `lea`); otherwise `slot` is a plain pointer variable, so the
// base instead comes from *reading* that variable's own stored value
// first.
void M16cCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);               // r0 = index
    code_ << "    shl.w 1,r0\n";   // r0 *= 2 (words are 2 bytes on this byte-addressed target)
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    lea " << lbl << ",a0\n";
    } else {
        code_ << "    mov.w " << lbl << ",a0\n";
    }
    code_ << "    add.w r0,a0\n";
}

void M16cCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ld a0,r0\n";
}

void M16cCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    lea " << label(operand.slot) << ",r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov.w a0,r0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void M16cCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push.w r0\n";
        genExpr(*e.rhs);
        code_ << "    pop.w r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add.w r1,r0\n";  // r0 = r0 + r1 = rhs + lhs
        } else {
            code_ << "    sub.w r0,r1\n";  // r1 = r1 - r0 = lhs - rhs
            code_ << "    mov.w r1,r0\n";
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
        code_ << "    push.w r0\n";
        genExpr(*e.rhs);
        code_ << "    pop.w r1\n";  // r1 = lhs, r0 = rhs
        const char *insn = (op == "&") ? "and.w" : (op == "|") ? "or.w" : "xor.w";
        code_ << "    " << insn << " r1,r0\n";  // r0 = r0 OP r1 = rhs OP lhs (every one of these is commutative)
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

// Real variable-count hardware shift (`SHL.W`) - see M16cCodegen.h's
// own header comment on its own signed-count convention (negative =
// right), which is why `isLeft` only ever decides whether to `NEG.W`
// the count first, never which instruction to use.
void M16cCodegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";  // r1 = shift count
    code_ << "    pop.w r0\n";     // r0 = value to shift
    if (!isLeft) code_ << "    neg.w r1\n";
    code_ << "    shl.w r1,r0\n";
}

// Real hardware multiply (`MUL.W`) - see M16cCodegen.h's own header
// comment on which half of its 32-bit product (`R0`, the low word) is
// this project's own 16-bit truncated answer.
void M16cCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    pop.w r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    mul.w r1,r0\n";   // r2:r0 = r0 * r1 = rhs * lhs (high word:low word)
}

// Real hardware divide (`DIVU.W`) - needs a genuine 32-bit dividend in
// `R2:R0`; `R2` is simply zeroed rather than sign-extended (see
// M16cCodegen.h's own header comment on the "non-negative operands
// only" scope limitation that follows from that).
void M16cCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";    // r1 = divisor
    code_ << "    pop.w r0\n";       // r0 = dividend (low word)
    code_ << "    mov.w 0,r2\n";     // r2 = dividend's high word = 0
    code_ << "    divu.w r1,r0\n";   // r0 = quotient, r2 = remainder
    if (wantRemainder) code_ << "    mov.w r2,r0\n";
}

void M16cCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "jeq");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, "jlt");
    if (op == "<=") return emitCompare(lhs, rhs, "jle");
    if (op == ">") return emitCompare(lhs, rhs, "jgt");
    if (op == ">=") return emitCompare(lhs, rhs, "jge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.W src,dst` sets the real signed condition codes for `dst - src`
// directly in hardware (matching every other two-operand mnemonic's
// own `dst = dst OP src` convention - see M16cCodegen.h's own header
// comment), so `r0`(rhs)/`r1`(lhs) is exactly `lhs - rhs`, with no
// further sign/overflow fixup needed, the same as the pdp11/68000/
// 8086 backends' own comparisons.
void M16cCodegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    pop.w r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    cmp.w r0,r1\n";   // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpIfTrue << " " << Ltrue << "\n";
    code_ << "    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    cmp.w 0,r0\n";
    code_ << "    jeq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp.w 0,r0\n";
    code_ << "    jeq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov.w 0,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    cmp.w 0,r0\n";
    code_ << "    jne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp.w 0,r0\n";
    code_ << "    jne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmp.w 0,r0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jeq " << Ltrue << "\n";
    code_ << "    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

std::string M16cCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the m16c backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M16cCodegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "bin") return assembleM16c(asmText);
    return std::nullopt;
}

}  // namespace pcc
