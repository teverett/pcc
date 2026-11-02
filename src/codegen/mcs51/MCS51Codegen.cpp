#include "MCS51Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "MCS51Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}
}  // namespace

uint32_t MCS51Codegen::allocXdata(const std::string &lbl, int count) {
    uint32_t addr = nextXdata_;
    xdataAddr_[lbl] = addr;
    nextXdata_ += static_cast<uint32_t>(count);
    return addr;
}

uint32_t MCS51Codegen::xdataAddr(const std::string &lbl) const {
    auto it = xdataAddr_.find(lbl);
    if (it == xdataAddr_.end()) {
        throw std::runtime_error("internal: '" + lbl + "' has no XDATA address - generate()'s own pre-pass should "
                                                         "have allocated one before any code referencing it");
    }
    return it->second;
}

void MCS51Codegen::declareTemp(const std::string &lbl) { allocXdata(lbl, 2); }

void MCS51Codegen::declareGlobalTemp(const std::string &lbl, long initValue) {
    uint32_t addr = allocXdata(lbl, 2);
    uint16_t v16 = static_cast<uint16_t>(initValue);
    initCode_ << "    movdptr " << addr << "\n";
    initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
    initCode_ << "    incdptr\n";
    initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
}

// Local arrays are never given an `arrayInit` (see IR.h's own
// Slot::arrayInit comment: "locals are zero-filled... assigned at
// runtime by ordinary statements") - this just reserves `arraySize`
// words of XDATA, which real hardware RAM doesn't actually guarantee
// starts zeroed, but this backend's own test simulator (see
// tests/run_tests.sh's own check_8051) does, matching every other
// backend's own "freshly declared local reads as 0 until assigned"
// assumption.
void MCS51Codegen::declareArray(const std::string &lbl, int arraySize) { allocXdata(lbl, arraySize * 2); }

void MCS51Codegen::declareGlobalArray(const std::string &lbl, int arraySize, const std::vector<long> &values) {
    uint32_t base = allocXdata(lbl, arraySize * 2);
    for (int i = 0; i < arraySize; i++) {
        long v = (static_cast<size_t>(i) < values.size()) ? values[i] : 0;
        uint16_t v16 = static_cast<uint16_t>(v);
        uint32_t addr = base + static_cast<uint32_t>(i) * 2;
        initCode_ << "    movdptr " << addr << "\n";
        initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
        initCode_ << "    incdptr\n";
        initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
    }
}

void MCS51Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    movdd " << dst << ", " << src << "\n";
    code_ << "    movdd " << dst << "+1, " << src << "+1\n";
}

void MCS51Codegen::loadXdata(const std::string &dstReg, uint32_t addr) {
    code_ << "    movdptr " << addr << "\n";
    code_ << "    movxr\n    movd " << dstReg << "\n";
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::storeXdata(uint32_t addr, const std::string &srcReg) {
    code_ << "    movdptr " << addr << "\n";
    code_ << "    mova " << srcReg << "\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova " << srcReg << "+1\n    movxw\n";
}

// Loads DPTR from `ptr` (the only pseudo-register this ever needs to
// happen for - every other XDATA access already knows its own
// address at compile time, via loadXdata()/storeXdata()'s own plain
// numeric operand) and writes `ac`'s two bytes there, one MOVX at a
// time - doesn't disturb `ptr` itself.
void MCS51Codegen::storeAcIndirect() {
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    mova ac\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova ac+1\n    movxw\n";
}

void MCS51Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    ljmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfTaken << "\n";
}

void MCS51Codegen::safeDjnz(const std::string &counterReg, const std::string &targetIfNonzero,
                             const std::string &targetIfZero) {
    std::string Lnear = newLabel();
    code_ << "    djnz " << counterReg << ", " << Lnear << "\n";
    code_ << "    ljmp " << targetIfZero << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfNonzero << "\n";
}

std::string MCS51Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the 8051 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see MCS51Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    // Pre-pass: every global and every function's own parameters get
    // their XDATA address assigned up front, before any code is
    // generated at all - see allocXdata()'s own header comment on
    // why a Call site or Var read needs that address to already
    // exist no matter which function is textually processed first
    // below (unlike 6502's own trailer, which is just ordinary
    // assembler-resolved CODE-space text and so never has this
    // problem - see MCS51Assembler.h's own header comment).
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // never possible here (no real separate compilation - see above), but
                                          // matches every other backend's own defensive check at this exact spot
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareGlobalArray(lbl, slot->arraySize, slot->arrayInit);
        } else {
            declareGlobalTemp(lbl, slot->hasInit ? slot->initValue : 0);
        }
    }
    for (const auto &fn : prog.functions) {
        for (ir::Slot *p : fn->params) declareTemp(label(p));
    }

    code_ << "org 0\n";
    // The fixed internal-RAM pseudo-registers - see MCS51Codegen.h's
    // own header comment for what each one is for. 0x30 sits safely
    // above the 4 register banks (0x00-0x1F) and the bit-addressable
    // area (0x20-0x2F), and comfortably below the 128-byte floor
    // every one of 8031/8032/8051/8052 guarantees.
    code_ << "ac = 0x30\nbc = 0x32\ncx = 0x34\ndx = 0x36\nptr = 0x38\n\n";
    code_ << "_start:\n";
    code_ << initCode_.str();
    code_ << "    lcall " << label(mainFn) << "\n";
    // The program's own final result sits in `ac` (XDATA's own 16-
    // bit result doesn't fit any real register, same reason every
    // other memory-cell-based backend's own header comment gives) the
    // moment execution reaches `halt` below - the authoritative place
    // to read it from. This extra `mova` is a minor nicety only: it
    // leaves the low byte sitting in the real accumulator too, for a
    // quick look with nothing more than a register dump.
    code_ << "    mova ac\n";
    code_ << "halt:\n    ljmp halt\n\n";

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    return code_.str();
}

// Single-file-only (see MCS51Codegen.h's own header comment: no
// `--format=coff`) - see M6502Codegen.cpp's own version of this
// comment for why that makes per-input-file splitting unnecessary.
std::map<std::string, std::string> MCS51Codegen::perFileAsm() const { return perFileAsm_; }

void MCS51Codegen::genFunction(const ir::FunctionBody &fn) {
    // Params' own XDATA storage is already allocated by generate()'s
    // own pre-pass - nothing left to do here but emit the body.
    code_ << label(fn.func) << ":\n";
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void MCS51Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    mova ac\n    orl ac+1\n";
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("jz", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    ljmp " << Ldone << "\n";
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
            code_ << "    mova ac\n    orl ac+1\n";
            safeBranch("jz", Lend, Lbody);
            code_ << Lbody << ":\n";
            genStmt(*s.loopBody);
            code_ << "    ljmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareArray(lbl, s.declSlot->arraySize);
            } else {
                declareTemp(lbl);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    storeXdata(xdataAddr(lbl), "ac");
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

void MCS51Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movai " << (v16 & 0xFF) << "\n    movd ac\n";
            code_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movd ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadXdata("ac", xdataAddr(label(e.slot)));
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target XDATA address
                // Save `ptr` across evaluating rhs (which might
                // itself index through `ptr` again) on the real
                // hardware stack - a real 8051 `PUSH`/`POP direct`
                // moves one memory byte at a time, no accumulator
                // detour needed (unlike every memory-cell-based
                // backend that only has an implied-accumulator push).
                code_ << "    push ptr\n    push ptr+1\n";
                genExpr(*e.rhs);
                code_ << "    pop ptr+1\n    pop ptr\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            storeXdata(xdataAddr(label(e.slot)), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    push ptr\n    push ptr+1\n";
            genExpr(*e.rhs);
            code_ << "    pop ptr+1\n    pop ptr\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    movai 0\n    clrc\n    subb ac\n    movd ac\n";
                code_ << "    movai 0\n    subb ac+1\n    movd ac+1\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    movdpl ptr\n    movdph ptr+1\n";
                code_ << "    movxr\n    movd ac\n";
                code_ << "    incdptr\n";
                code_ << "    movxr\n    movd ac+1\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mova ac\n    xrli 255\n    movd ac\n";
                code_ << "    mova ac+1\n    xrli 255\n    movd ac+1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                storeXdata(xdataAddr("p_" + e.func->sourceName + "_" + std::to_string(i)), "ac");
            }
            code_ << "    lcall " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 8051 global interrupt enable/disable - `SETB EA`
            // (IE.7) turns every interrupt source on, `CLR EA` turns
            // them all off - same real hardware feature, and the
            // same "ion"/"iof" IR naming, as the 6502/8080 backends'
            // own `cli`/`sei` and `ei`/`di` (see M6502Codegen.cpp's
            // own comment) - just the opposite mnemonic-to-meaning
            // mapping again (SETB *enables* here, the way CLI does on
            // 6502 but not on 8080).
            code_ << "    " << (e.op == "ion" ? "setea" : "clrea") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective XDATA address in `ptr`.
// `slot->arraySize > 0` means the base is a compile-time-known
// constant (this backend's own allocXdata() result, embedded
// directly as a plain immediate - there's no assembler symbol for an
// XDATA address at all, see MCS51Assembler.h's own header comment);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value via
// loadXdata().
void MCS51Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ac *= 2 (words are 2 bytes on this byte-addressed target).
    code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
    code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    if (slot->arraySize > 0) {
        uint32_t base = xdataAddr(label(slot));
        code_ << "    clrc\n    mova ac\n    addi " << (base & 0xFF) << "\n    movd ptr\n";
        code_ << "    mova ac+1\n    addci " << ((base >> 8) & 0xFF) << "\n    movd ptr+1\n";
    } else {
        loadXdata("bc", xdataAddr(label(slot)));
        code_ << "    clrc\n    mova bc\n    add ac\n    movd ptr\n";
        code_ << "    mova bc+1\n    addc ac+1\n    movd ptr+1\n";
    }
}

void MCS51Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    movxr\n    movd ac\n";
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd ac+1\n";
}

void MCS51Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        uint32_t addr = xdataAddr(label(operand.slot));
        code_ << "    movai " << (addr & 0xFF) << "\n    movd ac\n";
        code_ << "    movai " << ((addr >> 8) & 0xFF) << "\n    movd ac+1\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MCS51Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    mova bc\n    add ac\n    movd ac\n";
            code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
        } else {
            code_ << "    mova bc\n    clrc\n    subb ac\n    movd ac\n";
            code_ << "    mova bc+1\n    subb ac+1\n    movd ac+1\n";
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
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";
        const char *insn = (op == "&") ? "anl" : (op == "|") ? "orl" : "xrl";
        code_ << "    mova bc\n    " << insn << " ac\n    movd ac\n";
        code_ << "    mova bc+1\n    " << insn << " ac+1\n    movd ac+1\n";
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

// No variable-count shift instruction (`RLC A`/`RRC A` always rotate
// exactly one bit) - loop `rhs` times instead, via a real `DJNZ`
// (see safeDjnz()'s own comment) guarded by an upfront zero check
// (unlike 6502's own plain decrement-then-test loop, `DJNZ`
// decrements *before* testing, so a shift-by-zero has to be caught
// before ever entering the loop body, not after).
void MCS51Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    mova ac\n    movd cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pop bc+1\n    pop bc\n";
    copy16("ac", "bc");  // ac = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    mova cx\n";
    safeBranch("jz", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
        code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    clrc\n    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    }
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (the real `MUL AB` is only 8x8 ->
// 16, too narrow for this language's 16-bit `int` - see
// MCS51Codegen.h's own header comment): `bc` is the multiplicand
// (shifted left each iteration), `dx` the multiplier (shifted right,
// consumed one bit at a time), `ac` the accumulating product, `cx`
// the fixed 16-iteration counter. Only verified against non-negative
// operands.
void MCS51Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop bc+1\n    pop bc\n";                  // bc = multiplicand
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = product = 0
    code_ << "    movai 16\n    movd cx\n";
    std::string Lloop = newLabel(), Ladd = newLabel(), Lskipadd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mova dx\n    anli 1\n";
    safeBranch("jnz", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    mova bc\n    add ac\n    movd ac\n";
    code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    clrc\n    mova bc\n    rlca\n    movd bc\n";
    code_ << "    mova bc+1\n    rlca\n    movd bc+1\n";
    code_ << "    clrc\n    mova dx+1\n    rrca\n    movd dx+1\n";
    code_ << "    mova dx\n    rrca\n    movd dx\n";
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// Repeated subtraction (the real `DIV AB` is only 8/8 -> 8r8, same
// width problem as `MUL AB` above) - the same algorithm every other
// software-divmod backend here uses: `dx` is the running remainder
// (starts as the dividend, `bc` the fixed divisor), `ac` the
// quotient, counting how many times `bc` was subtracted out of `dx`
// before it would've gone negative - the tentative subtraction's low
// byte is stashed in `cx` (free for the duration of this routine)
// until it's known whether to actually commit it. Only verified
// against non-negative operands.
void MCS51Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop dx+1\n    pop dx\n";                  // dx = dividend (running remainder)
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    clrc\n    mova dx\n    subb bc\n    movd cx\n";
    code_ << "    mova dx+1\n    subb bc+1\n";
    safeBranch("jc", Llt, Lcommit);
    code_ << Lcommit << ":\n";
    code_ << "    movd dx+1\n    movdd dx, cx\n";
    code_ << "    mova ac\n    addi 1\n    movd ac\n";
    code_ << "    mova ac+1\n    addci 0\n    movd ac+1\n";
    code_ << "    ljmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    ljmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void MCS51Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to one "is a < b?" primitive
    // (emitLessCompare) by choosing which operand goes first and
    // whether to invert the final sense: a>b is b<a; a<=b is !(b<a);
    // a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b - `JZ`/`JNZ` test
// the accumulator directly on this target (no separate zero flag to
// consult), and the final `ORL A,direct` above it already leaves
// exactly "low result OR high result" sitting in A, zero iff both
// halves were zero.
void MCS51Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
    code_ << "    mova bc\n    clrc\n    subb ac\n    movd dx\n";
    code_ << "    mova bc+1\n    subb ac+1\n    orl dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "jz" : "jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction, the same "N xor V" idiom the
// 6502/8080 backends use (see M6502Codegen.cpp's own comment): after
// `a - b`'s final (high) byte SUBB, the accumulator's own sign bit
// (ACC.7) only reflects the *true* signed result once corrected for
// overflow (the real PSW.OV, which SUBB does set for signed
// over/underflow, exactly like 6502's V) - jumping over an `XRL
// A,#0x80` (flipping just the sign bit) when OV is clear, applying it
// when OV is set, fixes that up.
void MCS51Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(b);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = a
    code_ << "    mova bc\n    clrc\n    subb ac\n";
    code_ << "    mova bc+1\n    subb ac+1\n";
    std::string Lfixskip = newLabel();
    code_ << "    jnbov " << Lfixskip << "\n";
    code_ << "    xrli 128\n";
    code_ << Lfixskip << ":\n";
    // ACC.7 now correctly reflects "a < b" (set) / "a >= b" (clear).
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(invert ? "jnbacc7" : "jbacc7", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("jz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

std::string MCS51Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 8051 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MCS51Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "bin") return assembleMCS51(asmText);
    return std::nullopt;
}

}  // namespace pcc
