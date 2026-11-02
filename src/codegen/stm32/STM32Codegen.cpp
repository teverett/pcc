#include "STM32Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "STM32Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Real STM32 flash: this is where the reset vector table, every
// instruction, and nothing else lives - confirmed read-only against
// qemu's own `stm32vldiscovery` board model (see STM32Codegen.h's own
// header comment). `-kernel` loads this backend's own `--format=bin`
// output straight here.
constexpr uint32_t kFlashBase = 0x08000000;

// Real STM32 SRAM on this board (8KB, `0x20000000`-`0x20001FFF` - see
// tests/run_tests.sh's own check_stm32() comment on how this was
// confirmed): every global/local/temp/array slot lives here instead
// (see STM32Codegen.h's own header comment on why), reserved via
// `ds`, never given image bytes.
constexpr uint32_t kRamBase = 0x20000000;

// Top of that same SRAM - where `sp` starts out, loaded by the real
// Cortex-M reset exception from the vector table's own first word
// (confirmed directly: booting a hand-assembled image with this
// exact value at vector 0 leaves `sp` reading it back unchanged - no
// redundant explicit reload needed here the way MC68328Codegen.cpp's
// own comment says its own simulator board needs).
constexpr uint32_t kInitialSP = 0x20002000;
}  // namespace

void STM32Codegen::declareTemp(const std::string &lbl) { ramTrailer_.push_back(lbl + ": ds 1"); }

void STM32Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    ramTrailer_.push_back(lbl + ": ds " + std::to_string(values.size()));
}

// Flash-resident code that writes `value` into the RAM word `lbl`
// resolves to - see STM32Codegen.h's own header comment on why a
// slot's own real initial value can't just sit there in the image
// the way every other backend's flat-blob data does: RAM starts out
// with whatever qemu/real silicon happens to leave it as, so a
// nonzero initializer needs an explicit one-time copy, emitted as
// ordinary code into whichever stream the caller is building (this
// backend's own `_start`, for a global; right where a `LocalDecl`
// executes, for a local - see genStmt()'s own `LocalDecl` case).
void STM32Codegen::genGlobalInit(const std::string &ramLabel, long value) {
    if (value == 0) return;  // RAM already starts zeroed - nothing to copy
    code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(value)) << ",r0\n";
    code_ << "    lea " << ramLabel << ",r2\n";
    code_ << "    str r0,[r2]\n";
}

std::string STM32Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the stm32 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see STM32Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    // Globals first (see STM32Codegen.h's own header comment: nothing
    // about processing order matters here, since every RAM
    // reservation/init-copy line below is independently resolved by
    // STM32Assembler.cpp's own two-pass assembly - this just means
    // `initCode_` is ready before `_start` gets written out).
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // never possible here (no real separate compilation - see above), but
                                          // matches every other backend's own defensive check at this exact spot
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                if (slot->arrayInit[i] == 0) continue;
                code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(slot->arrayInit[i])) << ",r0\n";
                code_ << "    lea " << lbl << "+" << (i * 4) << ",r2\n";
                code_ << "    str r0,[r2]\n";
            }
        } else {
            declareTemp(lbl);
            genGlobalInit(lbl, slot->hasInit ? slot->initValue : 0);
        }
    }
    std::string initCode = code_.str();
    code_.str("");

    code_ << "org " << kFlashBase << "\n";
    code_ << "dl " << kInitialSP << "\n";  // vector[0]: initial sp
    code_ << "dlt _start\n";               // vector[1]: reset handler, thumb bit forced on
    code_ << "_start:\n";
    code_ << initCode;
    code_ << "    bl " << label(mainFn) << "\n";
    // The result sits in r0 the moment execution reaches `halt` - see
    // STM32Codegen.h's own header comment on why that's the
    // authoritative place to read it from (a real, directly
    // inspectable 32-bit register).
    code_ << "halt:\n    b halt\n\n";

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    code_ << "\nramorg " << kRamBase << "\n";
    for (const auto &line : ramTrailer_) code_ << line << "\n";

    return code_.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> STM32Codegen::perFileAsm() const { return perFileAsm_; }

void STM32Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    poplr\n\n";  // reached only if control falls off the end with no explicit `return`
}

void STM32Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmpz r0\n";
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
            code_ << "    cmpz r0\n    beq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    poplr\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareArray(lbl, s.declSlot->arrayInit);
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    if (s.declSlot->arrayInit[i] == 0) continue;
                    code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(s.declSlot->arrayInit[i]))
                          << ",r0\n";
                    code_ << "    lea " << lbl << "+" << (i * 4) << ",r2\n";
                    code_ << "    str r0,[r2]\n";
                }
            } else {
                declareTemp(lbl);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    code_ << "    lea " << lbl << ",r2\n    str r0,[r2]\n";
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

void STM32Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    lea " << label(e.slot) << ",r2\n    ldr r0,[r2]\n";
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
                code_ << "    str r0,[r2]\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    lea " << label(e.slot) << ",r2\n    str r0,[r2]\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);              // r0 = pointer value
            code_ << "    mov r2,r0\n";
            code_ << "    push r2\n";
            genExpr(*e.rhs);
            code_ << "    pop r2\n";
            code_ << "    str r0,[r2]\n";
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
                code_ << "    ldr r0,[r2]\n";
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
                code_ << "    lea p_" << e.func->sourceName << "_" << i << ",r2\n    str r0,[r2]\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Same "real but nothing for a bare-metal no-OS compile
            // to meaningfully do" stance as M68kCodegen.cpp's own
            // `_ion()`/`_iof()` comment - a real Cortex-M has a real
            // PRIMASK/`cpsie`/`cpsid`, but nothing here ever sets up
            // an exception/interrupt table for it to matter against.
            (void)e;
            return;
        }
    }
}

// See STM32Codegen.h's own header comment: `r2` holds the computed
// address, the same role m68k's `A0` plays (no zero-page-style
// placement restriction the 6502 backend's own `ptr` has).
// `slot->arraySize > 0` means the base is a compile-time-known label
// (an *address*, loaded via `lea`); otherwise `slot` is a plain
// pointer variable, so the base instead comes from *reading* that
// variable's own stored value first.
void STM32Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    // r0 *= 4 (words are 4 bytes on this target); done as two
    // doublings rather than a shift, needing no extra register at
    // all - same trick as M68kCodegen.cpp's own genIndexAddr().
    code_ << "    add r0,r0,r0\n    add r0,r0,r0\n";
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    lea " << lbl << ",r2\n";
        code_ << "    add r2,r2,r0\n";
    } else {
        code_ << "    lea " << lbl << ",r2\n    ldr r2,[r2]\n";
        code_ << "    add r2,r2,r0\n";
    }
}

void STM32Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldr r0,[r2]\n";
}

void STM32Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    lea " << label(operand.slot) << ",r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void STM32Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r0,r0,r1\n";  // r0 = rhs + lhs
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
        genExpr(*e.rhs);
        code_ << "    mov r1,r0\n";  // r1 = shift count
        code_ << "    pop r0\n";     // r0 = value to shift
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

// Real hardware multiply (`muls`) - a genuine 32x32->32 product, no
// operand-width caveat the way m68k's word-sized `MULS.W` needs (see
// STM32Codegen.h's own header comment).
void STM32Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    mul r1,r0\n";  // r0 = r0 * r1 = rhs * lhs
}

// Real hardware divide (`sdiv`) - this CPU, unlike every other target
// here (even the original 68000), has a genuine signed integer divide
// with no operand-width limitation at all, so unlike every other
// backend's own genDivMod() there's no software loop or quotient/
// remainder-packing trick needed: `%` is computed directly from a
// real division plus one real multiply-subtract (see STM32Codegen.h's
// own header comment).
void STM32Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";          // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    sdiv r2,r1,r0\n";  // r2 = lhs / rhs
    if (wantRemainder) {
        code_ << "    mul r0,r2\n";       // r2 = r2 * r0 = quotient * rhs
        code_ << "    sub r0,r1,r2\n";    // r0 = lhs - quotient*rhs = remainder
    } else {
        code_ << "    mov r0,r2\n";
    }
}

void STM32Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, "blt");
    if (op == "<=") return emitCompare(lhs, rhs, "ble");
    if (op == ">") return emitCompare(lhs, rhs, "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Real signed `cmp` + condition-code branch - this CPU's own flags
// already encode signed less-than/greater-than correctly in hardware
// (same as m68k's `CMP.L`+`Bcc` - see STM32Codegen.h's own header
// comment), no fixup needed the way the 6502 backend's own
// comparisons do.
void STM32Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";  // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmpz r0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

std::string STM32Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the stm32 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> STM32Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "bin") return assembleSTM32(asmText);
    return std::nullopt;
}

}  // namespace pcc
