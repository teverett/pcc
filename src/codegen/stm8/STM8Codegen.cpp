#include "STM8Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "STM8Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing, and the
// hardware stack's own initial top - same "comfortably clear of
// everything else this backend could generate, inside a flat 64KB
// address space" convention as every other bare-metal 8-bit backend
// here (see I8080Codegen.cpp's own comment on both constants).
constexpr uint32_t kBaseAddress = 0x0100;
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void STM8Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

void STM8Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": db ";
    for (size_t i = 0; i < values.size(); i++) {
        uint16_t v16 = static_cast<uint16_t>(values[i]);
        if (i > 0) line << ", ";
        // High byte first, low byte second - the STM8 is big-endian
        // (see this file's own header comment), the opposite order
        // from the little-endian i8080/6502 backends' own
        // declareArray().
        line << static_cast<int>((v16 >> 8) & 0xFF) << ", " << static_cast<int>(v16 & 0xFF);
    }
    trailer_.push_back(line.str());
}

void STM8Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ldw x, " << src << "\n    ldw " << dst << ", x\n";
}

void STM8Codegen::storeAcIndirect() {
    // Assumes `x` already holds the target address - `ld`/`stxind`
    // address `ac` directly, by name, never touching `x` itself until
    // the `incw` between the two bytes, so the caller's address in
    // `x` only ever advances forward, same convention as
    // I8080Codegen::storeAcIndirect.
    code_ << "    ld a, ac\n    stxind\n";
    code_ << "    incw x\n";
    code_ << "    ld a, ac+1\n    stxind\n";
}

void STM8Codegen::loadAcIndirect() {
    code_ << "    ldxind\n    ld ac, a\n";
    code_ << "    incw x\n";
    code_ << "    ldxind\n    ld ac+1, a\n";
}

// One in-place 1-bit shift of the 16-bit memory cell `lbl`, used by
// genShift()'s own loop and inlined directly by genMul()/
// genDivMod() - via the real word-wide `SLAW`/`SRLW` (see
// STM8Codegen.h's own header comment on why there's still no
// variable-count shift to use instead, even though these exist).
void STM8Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ldw x, " << lbl << "\n";
    code_ << "    " << (isLeft ? "slaw" : "srlw") << " x\n";
    code_ << "    ldw " << lbl << ", x\n";
}

std::string STM8Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the stm8 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see STM8Codegen.h's own header comment)");
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
    code_ << "    ldwi sp, " << kStackTop << "\n";
    code_ << "    call " << label(mainFn) << "\n";
    // The program's own final result sits in `ac` the moment
    // execution reaches `halt` below - the authoritative place to
    // read it from. This extra `ldw` leaves it in the real `x`
    // register too, for a quick look with nothing more than a
    // register dump.
    code_ << "    ldw x, ac\n";
    code_ << "halt:\n    jp halt\n\n";

    // The fixed memory pseudo-registers - see STM8Codegen.h's own
    // header comment for what each one is for. `lc` is this
    // backend's own loop counter (genMul()/genShift() need one but
    // can't use `a` - see their own comments), a plain byte, not one
    // of the five 16-bit cells.
    declareTemp("ac");
    declareTemp("bc");
    declareTemp("cx");
    declareTemp("dx");
    declareTemp("ptr");
    trailer_.push_back("lc: db 0");

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
            // High byte first, low byte second - see declareArray()'s
            // own comment.
            trailer_.push_back(lbl + ": db " + std::to_string((v16 >> 8) & 0xFF) + "  ; " + slot->sourceName);
            trailer_.push_back("db " + std::to_string(v16 & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (see STM8Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed to
// whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> STM8Codegen::perFileAsm() const { return perFileAsm_; }

void STM8Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void STM8Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    ldw x, ac\n    tnzw x\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jreq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jp " << Ldone << "\n";
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
            code_ << "    ldw x, ac\n    tnzw x\n";
            code_ << "    jreq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jp " << Lstart << "\n";
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

void STM8Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldwi x, " << v16 << "\n    ldw ac, x\n";
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
                code_ << "    ldw x, ptr\n    push x\n";
                genExpr(*e.rhs);
                code_ << "    pop x\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            copy16(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    ldw x, ac\n    push x\n";
            genExpr(*e.rhs);
            code_ << "    pop x\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ldw x, ac\n    negw x\n    ldw ac, x\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    ldw x, ac\n";
                loadAcIndirect();
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    ldw x, ac\n    cplw x\n    ldw ac, x\n";
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
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real STM8 interrupt enable/disable - RIM/SIM, the same
            // real hardware feature as the 8080/6502/8086 backends'
            // own ei/di-style intrinsics.
            code_ << "    " << (e.op == "ion" ? "rim" : "sim") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell - same role, and the same "idxExpr evaluated first and alone"
// caution, as I8080Codegen::genIndexAddr (see that file's own header
// comment - nothing here is safe to hold in `x`/`y`/`ptr`/`bc`/etc.
// across a general recursive genExpr() call).
void STM8Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    code_ << "    ldw x, ac\n    slaw x\n";  // x = index*2 (words are 2 bytes here too)
    code_ << "    ldw ptr, x\n";
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    ldwi y, " << lbl << "\n";  // y = base (a compile-time-known address)
    } else {
        code_ << "    ldw y, " << lbl << "\n";  // y = base (a pointer variable's own stored value)
    }
    code_ << "    ldw x, ptr\n    addw x, y\n    ldw ptr, x\n";  // ptr = index*2 + base
}

void STM8Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldw x, ptr\n";
    loadAcIndirect();
}

void STM8Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    ldwi x, " << label(operand.slot) << "\n    ldw ac, x\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* STM8 hardware stack (`push x`/`pop x`),
// never in a named memory cell - same reasoning as
// I8080Codegen::genBinary's own header comment (a nested binary op
// might reuse the same named cell).
void STM8Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        genExpr(*e.lhs);
        code_ << "    ldw x, ac\n    push x\n";
        genExpr(*e.rhs);                     // ac = rhs
        code_ << "    ldw y, ac\n    pop x\n";  // y = rhs, x = lhs (popped only now)
        code_ << "    addw x, y\n    ldw ac, x\n";
        return;
    }
    if (op == "-") {
        genExpr(*e.lhs);
        code_ << "    ldw x, ac\n    push x\n";
        genExpr(*e.rhs);
        code_ << "    ldw y, ac\n    pop x\n";  // y = rhs, x = lhs
        code_ << "    subw x, y\n    ldw ac, x\n";
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
        genBitwise(op, *e.lhs, *e.rhs);
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

// No register-register bitwise op on `x`/`y` (the real STM8's own
// `AND`/`OR`/`XOR` only ever touch `a` - see STM8Codegen.h's own
// header comment), so this goes byte-at-a-time through `a` instead,
// directly against each cell's own two named byte addresses
// (`bc`/`bc+1`, `ac`/`ac+1`) via this target's real direct-addressed
// ALU instructions - no transit register needed for the arithmetic
// itself, but `lhs` still has to survive evaluating `rhs` on the real
// hardware stack, not in `bc` directly (same reasoning as
// genBinary()'s own header comment: a nested `&`/`|`/`^` reuses `bc`
// too, so storing straight into it here, before `rhs` is evaluated,
// would get clobbered exactly the way a named cell always can).
void STM8Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    pop x\n    ldw bc, x\n";  // bc = lhs (popped only now, after rhs is fully evaluated)
    const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
    code_ << "    ld a, bc\n    " << insn << " a, ac\n    ld ac, a\n";
    code_ << "    ld a, bc+1\n    " << insn << " a, ac+1\n    ld ac+1, a\n";
}

// No variable-count shift instruction - loop `rhs` times instead, one
// bit at a time, via shiftOneBit(). The loop count lives in `lc`, a
// plain memory byte, not `a` - `a` is still needed as scratch to move
// `lc` in and out of memory each iteration (the STM8 has no `dec`/
// `tnz` on an arbitrary memory byte directly the way `incw`/`tnzw`
// exist for a whole word), but it's never live *across* a genExpr()
// call the way a result-carrying cell would need to be.
void STM8Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    ldw x, ac\n    push x\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                           // ac = shift count
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count (always small; low byte is enough)
    code_ << "    pop x\n    ldw bc, x\n";       // bc = value (restored only now, after the count is safely in lc)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    shiftOneBit("bc", isLeft);
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply wide enough
// for a 16x16 - see STM8Codegen.h's own header comment): `bc` is the
// multiplicand (doubled each iteration), `dx` the multiplier (halved
// each iteration, consumed one bit at a time), `ac` the accumulating
// product, `lc` the loop counter (see genShift()'s own comment on why
// not `a`). Only verified against non-negative operands.
void STM8Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    copy16("bc", "ac");  // bc = multiplicand
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = product = 0
    code_ << "    ldi a, 16\n    ld lc, a\n";    // loop counter
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    code_ << "    ld a, dx+1\n    andi a, 1\n";
    code_ << "    jreq " << Lskipadd << "\n";
    code_ << "    ldw x, ac\n    ldw y, bc\n    addw x, y\n    ldw ac, x\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide wide enough for a 16/16 -
// see STM8Codegen.h's own header comment): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative - via the real `CPW x,y` +
// `jrult`, no separate loop counter needed at all here. Only verified
// against non-negative operands.
void STM8Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    copy16("dx", "ac");  // dx = dividend (running remainder)
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldw x, dx\n    ldw y, bc\n    cpw x, y\n";
    code_ << "    jrult " << Ldone << "\n";  // dx < bc - this round's subtraction can't happen, stop
    code_ << "    subw x, y\n    ldw dx, x\n";
    code_ << "    ldw x, ac\n    incw x\n    ldw ac, x\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void STM8Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: `CPW x,y` after loading
// both operands sets Z iff they're equal, full 16 bits at once.
void STM8Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    copy16("bc", "ac");  // bc = lhs
    genExpr(rhs);         // ac = rhs
    code_ << "    ldw x, bc\n    ldw y, ac\n    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "jreq" : "jrne") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// I8080Codegen.h's own header comment on why: `CPW`'s own flags, like
// the 8080's status word, aren't trusted here for a signed compare):
// flipping just the top bit of each operand's high byte maps the
// signed range onto the unsigned one while preserving order, so a
// plain unsigned `CPW`+`jrult`/`jruge` is all that's left to do.
void STM8Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    copy16("bc", "ac");  // bc = a
    genExpr(b);           // ac = b
    code_ << "    ld a, bc\n    xori a, 0x80\n    ld bc, a\n";  // bc's high byte, sign flipped
    code_ << "    ld a, ac\n    xori a, 0x80\n    ld ac, a\n";  // ac's high byte, sign flipped
    code_ << "    ldw x, bc\n    ldw y, ac\n    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (invert ? "jruge" : "jrult") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jreq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jreq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jrne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jrne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jreq " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

std::string STM8Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the stm8 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> STM8Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "bin") return assembleSTM8(asmText);
    return std::nullopt;
}

}  // namespace pcc
