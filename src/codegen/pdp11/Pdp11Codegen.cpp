#include "Pdp11Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Pdp11Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness would set the simulated CPU's PC here directly (same
// convention as every other "bare flat binary" backend - see
// M68kCodegen.cpp/M6502Codegen.cpp's own `kBaseAddress`), so this
// backend never has to lay out a real PDP-11 reset/trap vector
// (those live at addresses 0-0376, below this). Must stay even - the
// PDP-11 can only fetch an instruction word from an even address.
constexpr uint32_t kBaseAddress = 0x1000;

// Where this backend's own entry sequence points SP (the real
// hardware stack pointer, used by `push`/`pop` and by `jsr`/`rts`)
// before calling main() - comfortably above the loaded program and
// its data, growing down into otherwise-unused memory.
constexpr uint32_t kInitialSp = 0xE000;
}  // namespace

void Pdp11Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element's own decimal text, truncated to this target's
// own 16-bit word width; a symbol element is the referenced Slot's or
// Function's own label, written out bare - "dw" (Pdp11Assembler.cpp's
// Line::Dw) already evaluates any expr operand, symbol included, and
// now also records the same external-name/base-shift relocation
// (kPdp11RelocAbs16) a label used as `lea`'s own absolute extension
// word already gets.
std::string Pdp11Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int>(static_cast<uint16_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void Pdp11Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void Pdp11Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

std::string Pdp11Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - Pdp11LinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see Pdp11Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    mov " << kInitialSp << ",sp\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The result sits in R0 the moment execution reaches `halt` -
        // same "a real, directly inspectable register" rationale as
        // the 68000 backend's own D0 (see Pdp11Codegen.h's own header
        // comment). The real `halt` instruction should actually stop
        // the simulated CPU right here; the `br halt` right after it
        // is only a safety net in case a given simulator's own `HALT`
        // just traps to a console/ODT prompt and execution somehow
        // resumes at the next word instead.
        code_ << "halt:\n    halt\n    br halt\n\n";
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
std::map<std::string, std::string> Pdp11Codegen::perFileAsm() const { return perFileAsm_; }

void Pdp11Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    rts\n\n";
}

void Pdp11Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    tst r0\n";
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("beq", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    jmp " << Ldone << "\n";
            if (s.elseStmt) {
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    tst r0\n";
            safeBranch("beq", Lend, Lbody);
            code_ << Lbody << ":\n";
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
                    code_ << "    mov r0," << lbl << "\n";
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

void Pdp11Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    mov " << static_cast<int>(static_cast<uint16_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    mov " << label(e.slot) << ",r0\n";
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
                code_ << "    stind r0,r2\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    mov r0," << label(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r0,r2\n";
            code_ << "    push r2\n";
            genExpr(*e.rhs);
            code_ << "    pop r2\n";
            code_ << "    stind r0,r2\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg r0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r0,r2\n";
                code_ << "    ldind r2,r0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    com r0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "    mov r0,p_" << e.func->sourceName << "_" << i << "\n";
            }
            code_ << "    jsr " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "jsrind r2" is Pdp11Assembler.cpp's real `JSR PC,@Rn`
            // encoding (register-DEFERRED mode, not register-direct -
            // see that file's own comment) - a genuine, standard PDP-
            // 11 call through whatever address r2 holds, the same way
            // real PDP-11 software routinely calls through a function
            // pointer.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r0,r2\n";
            code_ << "    jsrind r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal PDP-11 with no trap vector set up at all
            // (see generate()'s own comment) - the real `RTI`/PSW
            // priority bits exist, but there's nothing here for
            // `ion`/`iof` to meaningfully enable or disable. Treated
            // as a no-op, same as the 68000 backend's own version of
            // this same intrinsic.
            (void)e;
            return;
        }
    }
}

// See Pdp11Codegen.h's own header comment: `R2` holds the computed
// address, via the real register-deferred `(Rn)` mode - no zero-
// page-style placement restriction the way the 6502 backend's own
// `ptr` has. `slot->arraySize > 0` means the base is a compile-time-
// known label (an *address*, loaded via `lea`); otherwise `slot` is a
// plain pointer variable, so the base instead comes from *reading*
// that variable's own stored value first.
void Pdp11Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    code_ << "    asl r0\n";  // r0 *= 2 (words are 2 bytes on this byte-addressed target)
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    lea " << lbl << ",r2\n";
    } else {
        code_ << "    mov " << lbl << ",r2\n";
    }
    code_ << "    add r0,r2\n";
}

void Pdp11Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind r2,r0\n";
}

void Pdp11Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    lea " << label(operand.slot) << ",r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r2,r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    lea " << label(operand.func) << ",r0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Pdp11Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r1,r0\n";  // r0 = r0 + r1 = rhs + lhs
        } else {
            code_ << "    sub r0,r1\n";  // r1 = r1 - r0 = lhs - rhs
            code_ << "    mov r1,r0\n";
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
            // No real AND instruction on the PDP-11 - only BIC ("bit
            // clear": dst &= ~src). De Morgan's law turns that into a
            // real AND: `COM` the rhs first (giving `~rhs`), then
            // `BIC ~rhs,lhs` leaves `lhs & ~~rhs` = `lhs & rhs`.
            code_ << "    com r0\n";
            code_ << "    bic r0,r1\n";
            code_ << "    mov r1,r0\n";
        } else if (op == "|") {
            code_ << "    bis r1,r0\n";  // r0 |= r1
        } else {
            code_ << "    xor r1,r0\n";  // r0 ^= r1 (XOR's own source-must-be-a-register convention - see
                                          // Pdp11Assembler.cpp's own comment)
        }
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

// No variable-count *logical* shift (see Pdp11Codegen.h's own header
// comment on why the EIS `ASH` instruction doesn't fit here) - loop
// one bit at a time instead, counting down in `r1`.
void Pdp11Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    mov r0,r1\n";  // r1 = shift count
    code_ << "    pop r0\n";     // r0 = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    tst r1\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl r0\n";
    } else {
        // Logical (unsigned) shift - same convention (and the same
        // reason: no unsigned type to make the distinction with) as
        // every other backend's own `>>` - `CLC` first so `ROR`
        // rotates a 0, not the sign bit, into bit 15.
        code_ << "    clc\n    ror r0\n";
    }
    code_ << "    sub 1,r1\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Real EIS hardware multiply (`MUL`) - see Pdp11Codegen.h's own
// header comment on which half of its 32-bit product (`R1`, the low
// word) is this project's own 16-bit truncated answer.
void Pdp11Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    mul r1,r0\n";  // r0:r1 = r0 * r1 = rhs * lhs (high word:low word)
    code_ << "    mov r1,r0\n";
}

// Real EIS hardware divide (`DIV`) - needs a genuine 32-bit dividend
// in `R0:R1`, so the 16-bit dividend is first sign-extended into that
// pair with `SXT` (`TST` first, to set the N flag `SXT` itself reads)
// - the same "widen with a dedicated instruction, then use the real
// hardware op" shape as the 8086 backend's own `CWD`+`IDIV` (see
// Pdp11Codegen.h's own header comment).
void Pdp11Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    mov r0,r2\n";  // r2 = divisor
    code_ << "    pop r1\n";     // r1 = dividend (low word)
    code_ << "    tst r1\n    sxt r0\n";  // r0 = sign extension of the dividend (high word)
    code_ << "    div r2,r0\n";           // r0 = quotient, r1 = remainder
    if (wantRemainder) code_ << "    mov r1,r0\n";
}

void Pdp11Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, "blt");
    if (op == "<=") return emitCompare(lhs, rhs, "ble");
    if (op == ">") return emitCompare(lhs, rhs, "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP src,dst` sets the real signed condition codes for `src - dst`
// directly in hardware (see Pdp11Codegen.h's own header comment on
// this operand order) - `r1`(lhs)/`r0`(rhs) is exactly `lhs - rhs`,
// so `branchIfTrue` applies with no further sign/overflow fixup, the
// same as the 68000/8086 backends' own comparisons.
void Pdp11Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";  // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(branchIfTrue, Ltrue, Lfalse);
    code_ << Lfalse << ":\n    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    tst r0\n";
    std::string Lfalse = newLabel(), Lcheck2 = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    safeBranch("beq", Lfalse, Lcheck2);
    code_ << Lcheck2 << ":\n";
    genExpr(rhs);
    code_ << "    tst r0\n";
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    mov 1,r0\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov 0,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    tst r0\n";
    std::string Ltrue = newLabel(), Lcheck2 = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("bne", Ltrue, Lcheck2);
    code_ << Lcheck2 << ":\n";
    genExpr(rhs);
    code_ << "    tst r0\n";
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Lfalse << ":\n    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    tst r0\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("beq", Ltrue, Lfalse);
    code_ << Lfalse << ":\n    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
}

std::string Pdp11Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the pdp11 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Pdp11Codegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assemblePdp11(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see Pdp11Assembler.h's own `assemblePdp11Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see fileformat/FileFormat.h)
    // to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePdp11Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
