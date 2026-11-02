#include "M68kCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "M68kAssembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly (see
// tests/run_tests.sh's own qemu-system-m68k invocation), so unlike
// real 68000 hardware (which reads its initial SSP/PC from the reset
// vector at address 0) this backend never needs to lay one out.
constexpr uint32_t kBaseAddress = 0x1000;

// Where this backend's own entry sequence points A7 (the real
// hardware stack pointer, used by PUSH/POP-equivalent moves and by
// JSR/RTS) before calling main() - comfortably above the loaded
// program and its data, growing down into otherwise-unused RAM
// (tests/run_tests.sh gives the simulator 16MB).
constexpr uint32_t kInitialSP = 0x100000;
}  // namespace

void M68kCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dl 0"); }

void M68kCodegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dl ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << static_cast<int32_t>(static_cast<uint32_t>(values[i]));
    }
    trailer_.push_back(line.str());
}

std::string M68kCodegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the 68000 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=raw compile (see M68kCodegen.h's own header comment)");
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
    code_ << "    movel " << kInitialSP << ",a7\n";
    code_ << "    jsr " << label(mainFn) << "\n";
    // The result sits in D0 the moment execution reaches `halt` -
    // see M68kCodegen.h's own header comment on why that's the
    // authoritative place to read it from (a real, directly
    // inspectable 32-bit register, unlike the 6502 backend's own
    // memory-resident "ac" - see that file's own comment).
    code_ << "halt:\n    bra halt\n\n";

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
            trailer_.push_back(lbl + ": dl " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) +
                                "  ; " + slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> M68kCodegen::perFileAsm() const { return perFileAsm_; }

void M68kCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    rts\n\n";
}

void M68kCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmpl 0,d0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    bra " << Ldone << "\n";
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
            code_ << "    cmpl 0,d0\n    beq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    bra " << Lstart << "\n";
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
                    code_ << "    movel d0," << lbl << "\n";
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

void M68kCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",d0\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    movel " << label(e.slot) << ",d0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                code_ << "    pushl a0\n";     // a0 is really pushed via d0's own slot width - see pushl's own note
                genExpr(*e.rhs);
                code_ << "    popl a0\n";
                code_ << "    stind d0,a0\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    movel d0," << label(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // d0 = pointer value
            code_ << "    movel d0,a0\n";
            code_ << "    pushl a0\n";
            genExpr(*e.rhs);
            code_ << "    popl a0\n";
            code_ << "    stind d0,a0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    negl d0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // d0 = pointer value
                code_ << "    movel d0,a0\n";
                code_ << "    ldind a0,d0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    notl d0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "    movel d0,p_" << e.func->sourceName << "_" << i << "\n";
            }
            code_ << "    jsr " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal 68000 with no vector table set up at all
            // (see generate()'s own comment) - RTE/the SR interrupt-
            // mask bits exist, but there's nothing here for `ion`/
            // `iof` to meaningfully enable or disable. Treated as a
            // no-op rather than a hard error (matching this
            // intrinsic's own "do whatever this target's version of
            // enable/disable interrupts is" spirit, for a target
            // where that's simply nothing).
            (void)e;
            return;
        }
    }
}

// See M68kCodegen.h's own header comment: `A0` holds the computed
// address, no zero-page-style placement restriction the way the
// 6502 backend's own `ptr` has (see that file's own
// genIndexAddr()). `slot->arraySize > 0` means the base is a
// compile-time-known label (an *address*, loaded via `leal`);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first.
void M68kCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // d0 = index
    // d0 *= 4 (words are 4 bytes on this target - see M68kCodegen.h's
    // own header comment on word size); done as two doublings rather
    // than a shift, needing no extra register at all.
    code_ << "    addl d0,d0\n    addl d0,d0\n";
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    leal " << lbl << ",a0\n";
        code_ << "    adda d0,a0\n";
    } else {
        code_ << "    movel " << lbl << ",a0\n";
        code_ << "    adda d0,a0\n";
    }
}

void M68kCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind a0,d0\n";
}

void M68kCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    leal " << label(operand.slot) << ",a0\n";
        code_ << "    movel a0,d0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movel a0,d0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void M68kCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
        if (op == "+") {
            code_ << "    addl d1,d0\n";  // d0 = d0 + d1 = rhs + lhs
        } else {
            code_ << "    subl d0,d1\n";  // d1 = d1 - d0 = lhs - rhs
            code_ << "    movel d1,d0\n";
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
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
        if (op == "&") {
            code_ << "    andl d1,d0\n";
        } else if (op == "|") {
            code_ << "    orl d1,d0\n";
        } else {
            code_ << "    eorl d1,d0\n";  // d0 = d0 xor d1 (EOR's own source/dest convention - see
                                            // M68kAssembler.cpp's own comment)
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    movel d0,d1\n";  // d1 = shift count
        code_ << "    popl d0\n";      // d0 = value to shift
        code_ << "    " << (op == "<<" ? "lsll" : "lsrl") << " d1,d0\n";
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

// Real hardware multiply (`MULS.W`) - see M68kCodegen.h's own header
// comment on its own 16-bit-operand limitation.
void M68kCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";       // d1 = lhs, d0 = rhs
    code_ << "    mulsw d1,d0\n";  // d0 = d0 * d1 = rhs * lhs
}

// Real hardware divide (`DIVS.W`) - packs a 16-bit quotient and a
// 16-bit remainder into one 32-bit register (see M68kCodegen.h's own
// header comment on `DIVS.W`'s own operand-size limitation); `SWAP`+
// `EXT.L` pulls each half back out as a real sign-extended long.
void M68kCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs (dividend), d0 = rhs (divisor)
    code_ << "    divsw d0,d1\n";  // d1 = quotient (low word) : remainder (high word)
    if (wantRemainder) {
        code_ << "    swapl d1\n";  // remainder now in the low word
    }
    code_ << "    movel d1,d0\n";
    code_ << "    extl d0\n";  // sign-extend whichever half is now in the low word
}

void M68kCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, "blt");
    if (op == "<=") return emitCompare(lhs, rhs, "ble");
    if (op == ">") return emitCompare(lhs, rhs, "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.L <ea>,Dn` sets the real signed condition codes for `Dn - ea`
// directly in hardware (see M68kCodegen.h's own header comment) -
// `branchIfTrue` is simply applied, no sign/overflow fixup needed the
// way the 6502 backend's own comparisons do.
void M68kCodegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
    code_ << "    cmpl d0,d1\n";  // flags = d1 - d0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

void M68kCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    code_ << "    bra " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

void M68kCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    code_ << "    bra " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n    bra " << Ldone << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n";
    code_ << Ldone << ":\n";
}

void M68kCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

std::string M68kCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 68000 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M68kCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleM68k(asmText);
    return std::nullopt;
}

}  // namespace pcc
