#include "ArmCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "ArmAssembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly via qemu's
// PC-forcing `loader` device (see tests/run_tests.sh's own
// qemu-system-arm invocation), same idea as M68kCodegen.cpp's own
// kBaseAddress (even the same value - both targets' own test harness
// entries load their program at this exact address).
constexpr uint32_t kBaseAddress = 0x1000u;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

void ArmCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

void ArmCodegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << static_cast<int32_t>(static_cast<uint32_t>(values[i]));
    }
    trailer_.push_back(line.str());
}

void ArmCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movw " << reg << "," << lbl << "\n";
    code_ << "    movt " << reg << "," << lbl << "\n";
}

std::string ArmCodegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the armv7a backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=raw compile (see ArmCodegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here), so mainFn is never
    // null.

    code_ << "org " << kBaseAddress << "\n\n";
    code_ << "_start:\n";
    code_ << "    movw sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
    code_ << "    movt sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
    code_ << "    bl " << label(mainFn) << "\n";
    // The result sits in r0 the moment execution reaches `halt` - see
    // ArmCodegen.h's own header comment on why that's the
    // authoritative place to read it from.
    code_ << "halt:\n    b halt\n\n";

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // never possible here (no real separate compilation), but matches every
                                          // other backend's own defensive check at this exact spot
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
std::map<std::string, std::string> ArmCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `lr` around its own body,
// whether or not it happens to call anything else - unlike the
// 68000's own JSR/RTS (which push/pop the real hardware stack
// automatically, so a nested call just works - see M68kCodegen.cpp's
// own comment), ARM has exactly one link register: a non-leaf
// function's own call to another function (`BL`) overwrites `lr`
// outright, so without this save/restore a caller's own eventual `bx
// lr` would return into whatever its *own* last call last returned
// to, not back to its own caller. Always saving/restoring (leaf or
// not) is simpler than tracking which functions are actually
// non-leaf, and costs only two real instructions on either side -
// same reasoning (and the same choice) as MipsCodegen.cpp's own `$ra`
// save/restore.
void ArmCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,sp,4\n    stind sp,lr\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ldind sp,lr\n    add sp,sp,4\n";
    code_ << "    bx lr\n\n";
}

void ArmCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp r0,0\n    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            // Restore `lr` (see genFunction()'s own comment) before
            // returning - a `return` statement is its own early exit
            // from the function, bypassing genFunction()'s own
            // fall-through epilogue entirely, so it needs this same
            // restore inline.
            code_ << "    ldind sp,lr\n    add sp,sp,4\n";
            code_ << "    bx lr\n";
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
                    loadAddress("r2", lbl);
                    code_ << "    stind r2,r0\n";
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

void ArmCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint32_t uv = static_cast<uint32_t>(e.intValue);
            code_ << "    movw r0," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
            if ((uv >> 16) != 0) {
                code_ << "    movt r0," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadAddress("r2", label(e.slot));
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
                code_ << "    sub sp,sp,4\n    stind sp,r2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,r2\n    add sp,sp,4\n";
                code_ << "    stind r2,r0\n";
                return;
            }
            genExpr(*e.rhs);
            loadAddress("r2", label(e.slot));
            code_ << "    stind r2,r0\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    sub sp,sp,4\n    stind sp,r0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,r2\n    add sp,sp,4\n";
            code_ << "    stind r2,r0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    rsb r0,r0,0\n";
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
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                loadAddress("r2", "p_" + e.func->sourceName + "_" + std::to_string(i));
                code_ << "    stind r2,r0\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal A-profile core with no exception/GIC setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as M68kCodegen.cpp/MipsCodegen.cpp's
            // own Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `r2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is a compile-time-known label (an *address*);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first.
void ArmCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    code_ << "    lsl r0,r0,2\n";  // r0 *= 4 (words are 4 bytes on this target)
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        loadAddress("r2", lbl);
        code_ << "    add r2,r2,r0\n";
    } else {
        loadAddress("r1", lbl);
        code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's own stored value
        code_ << "    add r2,r1,r0\n";
    }
}

void ArmCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind r2,r0\n";
}

void ArmCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadAddress("r2", label(operand.slot));
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void ArmCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r0,r1,r0\n";  // r0 = lhs + rhs
        } else {
            code_ << "    sub r0,r1,r0\n";  // r0 = lhs - rhs
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
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
        if (op == "&") {
            code_ << "    and r0,r1,r0\n";
        } else if (op == "|") {
            code_ << "    orr r0,r1,r0\n";
        } else {
            code_ << "    eor r0,r1,r0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,4\n    stind sp,r0\n";
        genExpr(*e.rhs);                                  // r0 = shift count
        code_ << "    mov r1,r0\n";                        // r1 = shift count
        code_ << "    ldind sp,r0\n    add sp,sp,4\n";     // r0 = value to shift
        code_ << "    " << (op == "<<" ? "lsl" : "lsr") << " r0,r0,r1\n";
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

// Real hardware multiply (`MUL`) - a full 32x32->32 product in one
// instruction, no HI/LO-register-pair unpacking the way MIPS's own
// `MULT`+`MFLO` needs.
void ArmCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
    code_ << "    mul r0,r1,r0\n";
}

// Real hardware divide (`SDIV`) - a full 32-bit signed quotient
// directly, no HI/LO-register-pair unpacking the way MIPS's own `DIV`
// needs. The remainder (`%`) then uses `MLS` (multiply-subtract) to
// compute `dividend - quotient*divisor` in one more instruction -
// `r3`'s own copy of the divisor (rather than reusing `r0` directly)
// exists only so `MLS`'s own `Rd` and `Rm` operands are never the
// same register, which several real ARM cores treat as
// UNPREDICTABLE for multiply-family instructions.
void ArmCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    sdiv r3,r1,r0\n";                 // r3 = quotient
    if (!wantRemainder) {
        code_ << "    mov r0,r3\n";
        return;
    }
    code_ << "    mov r2,r0\n";          // r2 = divisor (own copy, see this function's own comment)
    code_ << "    mls r0,r3,r2,r1\n";    // r0 = r1 - r3*r2 = dividend - quotient*divisor = remainder
}

// Every ARM data-processing instruction (including `MOV`) is
// conditionally executed per its own 4-bit condition field, so
// materializing a 0/1 boolean needs no branch at all - just `MOV
// r0,#0` followed by one conditionally executed `MOV r0,#1` (see
// ArmCodegen.h's own header comment).
void ArmCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,4\n    stind sp,r0\n";
    genExpr(rhs);
    code_ << "    ldind sp,r1\n    add sp,sp,4\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";                      // flags = lhs - rhs
    code_ << "    mov r0,0\n";
    if (op == "<") {
        code_ << "    movlt r0,1\n";
    } else if (op == ">") {
        code_ << "    movgt r0,1\n";
    } else if (op == "<=") {
        code_ << "    movle r0,1\n";
    } else if (op == ">=") {
        code_ << "    movge r0,1\n";
    } else if (op == "==") {
        code_ << "    moveq r0,1\n";
    } else if (op == "!=") {
        code_ << "    movne r0,1\n";
    } else {
        err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    }
}

void ArmCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov r0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov r0,0\n";
    code_ << Ldone << ":\n";
}

// `!x` is exactly "x == 0" - one `CMP`+conditional-`MOV` pair, same
// idea as genCompare()'s own "==" case.
void ArmCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmp r0,0\n";
    code_ << "    mov r0,0\n";
    code_ << "    moveq r0,1\n";
}

std::string ArmCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                              const std::string &) const {
    throw std::runtime_error("the armv7a backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> ArmCodegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleArm(asmText);
    return std::nullopt;
}

}  // namespace pcc
