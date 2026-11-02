#include "M65C02Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "M65C02Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - a bare,
// no-OS flat binary (see M65C02Codegen.h's own header comment), $0600
// being the conventional "put your test program here" address for a
// minimal/bare 6502 setup (below any BASIC/OS reservation, above
// zero page and the hardware stack page $0100-$01FF).
constexpr uint32_t kBaseAddress = 0x0600;
}  // namespace

void M65C02Codegen::declareTemp(const std::string &lbl) {
    trailer_.push_back(lbl + ": db 0");
    trailer_.push_back("db 0");
}

void M65C02Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": db ";
    for (size_t i = 0; i < values.size(); i++) {
        uint16_t v16 = static_cast<uint16_t>(values[i]);
        if (i > 0) line << ", ";
        line << static_cast<int>(v16 & 0xFF) << ", " << static_cast<int>((v16 >> 8) & 0xFF);
    }
    trailer_.push_back(line.str());
}

void M65C02Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lda " << src << "\n    sta " << dst << "\n";
    code_ << "    lda " << src << "+1\n    sta " << dst << "+1\n";
}

void M65C02Codegen::zeroZp(const std::string &lbl) {
    code_ << "    stzp " << lbl << "\n    stzp " << lbl << "+1\n";
}

void M65C02Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

void M65C02Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    inc " << lbl << "\n";
    code_ << "    lda " << lbl << "\n";
    code_ << "    bne " << Lskip << "\n";
    code_ << "    inc " << lbl << "+1\n";
    code_ << Lskip << ":\n";
}

std::string M65C02Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the 65C02 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see M65C02Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    code_ << "org " << kBaseAddress << "\n\n";
    // The fixed zero-page pseudo-registers - see M65C02Codegen.h's own
    // header comment for what each one is for and why `ptr`
    // specifically has to be bound this way (a pure compile-time
    // equate, not the usual sequential data-trailer layout) rather
    // than wherever declareTemp() would otherwise place it.
    code_ << "ptr = 0\nac = 2\nbc = 4\ncx = 6\ndx = 8\n\n";
    code_ << "_start:\n";
    code_ << "    jsr " << label(mainFn) << "\n";
    // The program's own final result sits in `ac`/`ac+1` (memory,
    // $0002/$0003) the moment execution reaches `halt` below - the
    // authoritative place to read it from (see M65C02Codegen.h's own
    // comment on the 6502 having no register wide enough to hold a
    // full 16-bit result the way pdp7's AC/8086's AX do). This extra
    // `lda` is a minor nicety only: it leaves the low byte sitting in
    // the real A register too, for a quick look with nothing more
    // than a register dump.
    code_ << "    lda ac\n";
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
            uint16_t v16 = static_cast<uint16_t>(v);
            trailer_.push_back(lbl + ": db " + std::to_string(v16 & 0xFF) + "  ; " + slot->sourceName);
            trailer_.push_back("db " + std::to_string((v16 >> 8) & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (see M65C02Codegen.h's own header comment: no
// `--format=coff`, so `--dump-asm` is never asked to split output
// across more than one input file for this target) - the whole
// generated text is simply attributed to whichever file defines
// `main`, skipping the fine-grained code/trailer-range tracking
// ../pdp7/Pdp7Codegen.cpp/../x86_8086/I8086Codegen.cpp's own
// perFileAsm() needs for their real multi-file case.
std::map<std::string, std::string> M65C02Codegen::perFileAsm() const { return perFileAsm_; }

void M65C02Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    rts\n\n";
}

void M65C02Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    lda ac\n    ora ac+1\n";
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
            code_ << "    lda ac\n    ora ac+1\n";
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
                    copy16(lbl, "ac");
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

void M65C02Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            // A zero byte stores via `stzp` (see zeroZp()'s own
            // comment) rather than ../m6502/M6502Codegen.cpp's own
            // "ldai 0 / sta" - one instruction instead of two, and
            // the only reason this case needs its own per-byte logic
            // instead of just calling zeroZp() outright (most literal
            // values here aren't 0 in both bytes at once).
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            if ((v16 & 0xFF) == 0) {
                code_ << "    stzp ac\n";
            } else {
                code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
            }
            if (((v16 >> 8) & 0xFF) == 0) {
                code_ << "    stzp ac+1\n";
            } else {
                code_ << "    ldai " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            copy16("ac", label(e.slot));
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                // Save `ptr` across evaluating rhs (which might
                // itself index through `ptr` again) on the real
                // hardware stack - the exact role
                // ../x86_8086/I8086Codegen.cpp's own "push bx"/"pop
                // bx" plays around the same spot.
                code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
                genExpr(*e.rhs);
                code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
                code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                return;
            }
            genExpr(*e.rhs);
            copy16(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
            genExpr(*e.rhs);
            code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
            code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ldai 0\n    sec\n    sbc ac\n    sta ac\n";
                code_ << "    ldai 0\n    sbc ac+1\n    sta ac+1\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
                code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    lda ac\n    eori 255\n    sta ac\n";
                code_ << "    lda ac+1\n    eori 255\n    sta ac+1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                copy16("p_" + e.func->sourceName + "_" + std::to_string(i), "ac");
            }
            code_ << "    jsr " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 6502 interrupt enable/disable - SEI sets the
            // interrupt-disable flag (iof), CLI clears it (ion); same
            // real hardware feature as the 8086 backend's sti/cli
            // (see I8086Codegen.cpp), just the opposite mnemonic-to-
            // meaning mapping.
            code_ << "    " << (e.op == "ion" ? "cli" : "sei") << "\n";
            return;
        }
    }
}

// See M65C02Codegen.h's own header comment on this being the one
// thing that ever needs a real zero-page address. `slot->arraySize >
// 0` means the base is a compile-time-known label (an *address*, so
// `ldai`/`ldai_hi` applied to that label's own name split it into
// its low/high byte at assemble time - see
// M65C02Assembler.cpp's emitImmLow/High's own comment); otherwise
// `slot` is a plain pointer variable, so the base instead comes from
// *reading* that variable's own stored value (`adc <label>`/`adc
// <label>+1`, an ordinary absolute-memory operand, not an immediate).
void M65C02Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ac *= 2 (words are 2 bytes on this byte-addressed target, same
    // reason as ../x86_8086/I8086Codegen.cpp's own `shl ax,1`).
    code_ << "    asl ac\n    rol ac+1\n";
    std::string lbl = label(slot);
    code_ << "    clc\n";
    if (slot->arraySize > 0) {
        code_ << "    lda ac\n    adci " << lbl << "\n    sta ptr\n";
        code_ << "    lda ac+1\n    adci_hi " << lbl << "\n    sta ptr+1\n";
    } else {
        code_ << "    lda ac\n    adc " << lbl << "\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc " << lbl << "+1\n    sta ptr+1\n";
    }
}

void M65C02Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M65C02Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        std::string lbl = label(operand.slot);
        code_ << "    ldai " << lbl << "\n    sta ac\n";
        code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void M65C02Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    clc\n    lda bc\n    adc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    adc ac+1\n    sta ac+1\n";
        } else {
            code_ << "    sec\n    lda bc\n    sbc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    sbc ac+1\n    sta ac+1\n";
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
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
        const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
        code_ << "    lda bc\n    " << insn << " ac\n    sta ac\n";
        code_ << "    lda bc+1\n    " << insn << " ac+1\n    sta ac+1\n";
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

// No variable-count shift instruction (ASL/LSR always shift exactly
// one bit) - loop `rhs` times instead, the same fix
// ../pdp7/Pdp7Codegen.cpp's own `<<`/`>>` already needed for the
// same reason (see M65C02Codegen.h's own header comment).
void M65C02Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    lda ac\n    sta cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
    copy16("ac", "bc");  // ac = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl ac\n    rol ac+1\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with in
        // the first place), as every other backend's own `>>`.
        code_ << "    lsr ac+1\n    ror ac\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (no hardware multiply - see
// M65C02Codegen.h's own header comment): `bc` is the multiplicand
// (shifted left each iteration), `dx` the multiplier (shifted right,
// consumed one bit at a time into `cx`'s own loop), `ac` the
// accumulating product. Only verified against non-negative operands.
void M65C02Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = multiplicand
    zeroZp("ac");  // ac = product = 0
    code_ << "    ldai 16\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Ladd = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    code_ << "    lda dx\n    andi 1\n";
    safeBranch("bne", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    clc\n    lda ac\n    adc bc\n    sta ac\n";
    code_ << "    lda ac+1\n    adc bc+1\n    sta ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    asl bc\n    rol bc+1\n";
    code_ << "    lsr dx+1\n    ror dx\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide - see M65C02Codegen.h's own
// header comment) - the same algorithm, for the same reason,
// ../pdp7/Pdp7Codegen.cpp's own ensureDivModRoutine() already uses:
// `dx` is the running remainder (starts as the dividend, `bc` the
// fixed divisor), `ac` the quotient, counting how many times `bc`
// was subtracted out of `dx` before it would've gone negative. Only
// verified against non-negative operands.
void M65C02Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pla\n    sta dx+1\n    pla\n    sta dx\n";  // dx = dividend (running remainder)
    zeroZp("ac");  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentatively compute dx - bc; carry set afterward means no
    // borrow occurred, i.e. dx >= bc. The low byte's tentative result
    // is stashed on the stack (A is needed for the high byte's own
    // subtraction next) until it's known whether to actually commit.
    code_ << "    sec\n    lda dx\n    sbc bc\n    pha\n";
    code_ << "    lda dx+1\n    sbc bc+1\n";
    safeBranch("bcs", Lcommit, Llt);
    code_ << Lcommit << ":\n";
    code_ << "    sta dx+1\n    pla\n    sta dx\n";
    inc16("ac");
    code_ << "    jmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    pla\n";  // discard the stashed tentative low byte - this iteration's subtraction is rejected
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void M65C02Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
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
// chained from low into high byte) is zero iff a==b, checked the same
// "OR the two result bytes together" way genLogicalNot() checks `ac`
// for zero - `dx` holds the low byte's tentative result while the
// high byte's own subtraction needs A.
void M65C02Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
    code_ << "    sec\n    lda bc\n    sbc ac\n    sta dx\n";
    code_ << "    lda bc+1\n    sbc ac+1\n    ora dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction, the standard 6502 multi-byte
// signed-compare idiom: after `a - b`'s final (high) byte SBC, the
// sign bit (N) only reflects the *true* signed result once corrected
// for overflow (V) - if V is set, the apparent sign is backwards, so
// `EOR #$80` (flipping just the sign bit, which also recomputes N/Z
// for the corrected value) fixes it up. This is exactly the "N xor V"
// rule x86's SF/OF flags encode as a single combined test (see
// ../x86_8086/I8086Codegen.cpp's own signed `cmp`+`jl`/`jle`/etc.) -
// the 6502 has both flags too, just no one instruction to test their
// XOR directly, hence the explicit fixup.
void M65C02Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(b);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = a
    code_ << "    sec\n    lda bc\n    sbc ac\n    lda bc+1\n    sbc ac+1\n";
    std::string Lfixskip = newLabel();
    code_ << "    bvc " << Lfixskip << "\n";
    code_ << "    eori 128\n";
    code_ << Lfixskip << ":\n";
    // N now correctly reflects "a < b" (BMI) / "a >= b" (BPL).
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("beq", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("bne", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

void M65C02Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("beq", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    stzp ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    zeroZp("ac");
    code_ << Ldone << ":\n";
}

std::string M65C02Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 65C02 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M65C02Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "bin") return assembleM65C02(asmText);
    return std::nullopt;
}

}  // namespace pcc
