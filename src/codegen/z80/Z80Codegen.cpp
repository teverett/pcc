#include "Z80Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Z80Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - an
// arbitrary address distinct from the 8080 backend's own 0x0100 (see
// ../i8080/I8080Codegen.cpp), chosen only to leave plenty of RAM
// above it in a bare 64KB Z80 system; tests/run_tests.sh's own
// emulator setup loads this binary here and sets PC to it directly,
// no boot ROM involved.
constexpr uint32_t kBaseAddress = 0x8000;
// Same reasoning as the 8080 backend's own `kStackTop` - the Z80's
// SP is likewise undefined at reset and this backend's real CALL/
// RET/PUSH/POP all need a real initial value.
constexpr uint32_t kStackTop = 0xFF00;
}  // namespace

void Z80Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

void Z80Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
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

void Z80Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ld hl, [" << src << "]\n    ld [" << dst << "], hl\n";
}

void Z80Codegen::loadPair(const std::string &pairReg, const std::string &src) {
    code_ << "    ld " << pairReg << ", [" << src << "]\n";
}

void Z80Codegen::storePair(const std::string &dst, const std::string &pairReg) {
    code_ << "    ld [" << dst << "], " << pairReg << "\n";
}

void Z80Codegen::storeAcIndirect() {
    // Assumes HL already holds the target address - plain
    // accumulator loads from `ac`'s own two bytes (by name, never
    // touching HL) interleaved with `(HL)` stores, so the caller's
    // address in HL survives across both.
    code_ << "    ld a, [ac]\n    ld m, a\n";
    code_ << "    inc hl\n";
    code_ << "    ld a, [ac+1]\n    ld m, a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl` - see
// Z80Codegen.h's own header comment on why this is only two
// instructions here (`RL`/`RR` target L/H directly) versus the
// 8080's six (RAL/RAR only ever touch the accumulator, so getting a
// value in and back out of it costs two extra MOVs per byte).
void Z80Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ld hl, [" << lbl << "]\n";
    code_ << "    or a\n";
    if (isLeft) {
        code_ << "    rl l\n    rl h\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`.
        code_ << "    rr h\n    rr l\n";
    }
    code_ << "    ld [" << lbl << "], hl\n";
}

std::string Z80Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the Z80 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see Z80Codegen.h's own header comment)");
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
    code_ << "    ld sp, " << kStackTop << "\n";
    code_ << "    call " << label(mainFn) << "\n";
    // The program's own final result sits in `ac` the moment
    // execution reaches `halt` below - the authoritative place to
    // read it from. This extra load leaves it in the real HL pair
    // too, for a quick look with nothing more than a register dump.
    code_ << "    ld hl, [ac]\n";
    code_ << "halt:\n    jp halt\n\n";

    // The fixed memory pseudo-registers - see Z80Codegen.h's own
    // header comment for what each one is for. Declared once, up
    // front, as ordinary temps (no addressing-mode restriction on
    // where any of these can live, same as the 8080 backend).
    declareTemp("ac");
    declareTemp("_bc");
    declareTemp("cx");
    declareTemp("dx");
    declareTemp("ptr");

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

// Single-file-only (see Z80Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> Z80Codegen::perFileAsm() const { return perFileAsm_; }

void Z80Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void Z80Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jp z, " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
            code_ << "    jp z, " << Lend << "\n";
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

void Z80Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ld hl, " << v16 << "\n    ld [ac], hl\n";
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
                code_ << "    ld hl, [ptr]\n    push hl\n";
                genExpr(*e.rhs);
                code_ << "    pop hl\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            copy16(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    ld hl, [ptr]\n    push hl\n";
            genExpr(*e.rhs);
            code_ << "    pop hl\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ld hl, [ac]\n";
                code_ << "    xor a\n    sub l\n    ld l, a\n";
                code_ << "    ld a, 0\n    sbc a, h\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    ld hl, [ac]\n";
                code_ << "    ld e, m\n    inc hl\n    ld d, m\n";
                code_ << "    ld [ac], de\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    ld hl, [ac]\n";
                code_ << "    ld a, l\n    cpl\n    ld l, a\n";
                code_ << "    ld a, h\n    cpl\n    ld h, a\n";
                code_ << "    ld [ac], hl\n";
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
            // Real Z80 interrupt enable/disable - same real hardware
            // feature, and same mnemonics, as the 8080 backend's own
            // EI/DI (see I8080Codegen.cpp).
            code_ << "    " << (e.op == "ion" ? "ei" : "di") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated first, and alone - see
// ../i8080/I8080Codegen.cpp's own genIndexAddr() comment on why.
// `slot->arraySize > 0` means the base is a compile-time-known label
// (an address - `ld de, <label>` loads a symbol's own address,
// exactly like genAddrOf()'s own use of the same trick), otherwise
// `slot` is a plain pointer variable and the base instead comes from
// *reading* that variable's own stored value into DE - a single real
// instruction here (see Z80Codegen.h's own header comment on
// loadPair()), unlike the 8080's own stack round-trip.
void Z80Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // HL = index*2 (words are 2 bytes on this byte-addressed target) -
    // `ADD HL,HL` doubles HL in one instruction, same trick as the
    // 8080 backend's own `DAD H`.
    code_ << "    ld hl, [ac]\n    add hl, hl\n";
    code_ << "    ld [ptr], hl\n";
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    ld de, " << lbl << "\n";
    } else {
        loadPair("de", lbl);
    }
    code_ << "    ld hl, [ptr]\n    add hl, de\n    ld [ptr], hl\n";
}

void Z80Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ld hl, [ptr]\n";
    code_ << "    ld e, m\n    inc hl\n    ld d, m\n";
    code_ << "    ld [ac], de\n";
}

void Z80Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    ld hl, " << label(operand.slot) << "\n    ld [ac], hl\n";
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
// right one on the *real* Z80 hardware stack (`push hl`/`pop de`),
// never in a named memory cell - see ../i8080/I8080Codegen.cpp's own
// genBinary() header comment for why (the same reasoning applies
// unchanged: the right operand's own genExpr() might recurse into
// genMul()/genDivMod()/genShift(), all of which share `bc`/`cx`/`dx`).
void Z80Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        code_ << "    add hl, de\n    ld [ac], hl\n";
        return;
    }
    if (op == "-") {
        genExpr(*e.lhs);
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        code_ << "    or a\n    sbc hl, de\n";       // HL = lhs - rhs (real 16-bit subtract-with-carry)
        code_ << "    ld [ac], hl\n";
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
        code_ << "    ld hl, [ac]\n    push hl\n";
        genExpr(*e.rhs);
        code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    ld a, l\n    " << insn << " e\n    ld l, a\n";
        code_ << "    ld a, h\n    " << insn << " d\n    ld h, a\n";
        code_ << "    ld [ac], hl\n";
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

// No variable-count shift instruction (see Z80Codegen.h's own header
// comment) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit().
void Z80Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);                                // ac = value to shift
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                                 // ac = shift count
    code_ << "    ld a, [ac]\n    ld b, a\n";     // b = shift count (always small; low byte is enough)
    code_ << "    pop hl\n    ld [_bc], hl\n";      // bc = value (restored only now)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    shiftOneBit("_bc", isLeft);
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "_bc");
}

// Textbook shift-and-add multiply (no hardware multiply): `bc` is
// the multiplicand (doubled each iteration), `dx` the multiplier
// (halved each iteration), `ac` the accumulating product. Only
// verified against non-negative operands.
void Z80Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);                                // ac = multiplicand
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the multiplier
    genExpr(rhs);
    code_ << "    pop hl\n    ld [_bc], hl\n";  // bc = multiplicand (restored only now)
    copy16("dx", "ac");                          // dx = multiplier
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    code_ << "    ld b, 16\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, b\n    or a\n";
    code_ << "    jp z, " << Ldone << "\n";
    code_ << "    ld a, [dx]\n    and 1\n";
    code_ << "    jp z, " << Lskipadd << "\n";
    code_ << "    ld hl, [ac]\n    ld de, [_bc]\n    add hl, de\n    ld [ac], hl\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("_bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dec b\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient. A single `SBC HL,DE` both tests "has dx gone below
// bc?" (the carry flag) and computes the committed subtraction
// result in one instruction - no separate tentative-then-commit byte
// reconstruction needed the way the 8080 backend's own version
// needs (see I8080Codegen.cpp). Only verified against non-negative
// operands.
void Z80Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);                                // ac = dividend
    code_ << "    ld hl, [ac]\n    push hl\n";  // saved across evaluating the divisor
    genExpr(rhs);
    code_ << "    pop hl\n    ld [dx], hl\n";  // dx = dividend (restored only now)
    copy16("_bc", "ac");                          // bc = divisor
    code_ << "    ld hl, 0\n    ld [ac], hl\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld hl, [dx]\n    ld de, [_bc]\n";
    code_ << "    or a\n    sbc hl, de\n";  // HL = dx - bc; CY = borrow (dx < bc)
    code_ << "    jp c, " << Ldone << "\n";
    code_ << "    ld [dx], hl\n";  // commit: dx -= bc (HL already holds the subtracted result)
    // Quotient += 1 - a real 16-bit increment.
    code_ << "    ld hl, [ac]\n    inc hl\n    ld [ac], hl\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void Z80Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` reduce to a single `SBC HL,DE`: the Z flag it sets
// directly means "lhs == rhs" (the full 16-bit result is zero iff
// the two operands were equal) - no manual byte-by-byte OR-check
// needed the way the 8080 backend's own version needs.
void Z80Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(rhs);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = lhs, DE = rhs
    code_ << "    or a\n    sbc hl, de\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (wantEqual ? "z" : "nz") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// ../i8080/I8080Codegen.h's own header comment: the Z80, like the
// 8080, has no single flag or condition code for signed less-than) -
// flipping the sign bit of each operand's high byte maps the signed
// range onto the unsigned one while preserving order, and a single
// `SBC HL,DE` (instead of the 8080's two byte-wise `SUB`/`SBB`) does
// the rest.
void Z80Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    ld hl, [ac]\n    push hl\n";
    genExpr(b);
    code_ << "    ld de, [ac]\n    pop hl\n";  // HL = a, DE = b
    code_ << "    ld a, h\n    xor 0x80\n    ld h, a\n";  // H = a's high byte, sign flipped
    code_ << "    ld a, d\n    xor 0x80\n    ld d, a\n";  // D = b's high byte, sign flipped
    code_ << "    or a\n    sbc hl, de\n";                // CY = (a < b) unbiased
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp " << (invert ? "nc" : "c") << ", " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp z, " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp z, " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp nz, " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    code_ << "    jp nz, " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

void Z80Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    ld hl, [ac]\n    ld a, l\n    or h\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jp z, " << Ltrue << "\n";
    code_ << "    ld hl, 0\n    ld [ac], hl\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ld hl, 1\n    ld [ac], hl\n";
    code_ << Ldone << ":\n";
}

std::string Z80Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                              const std::string &) const {
    throw std::runtime_error("the Z80 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Z80Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "bin") return assembleZ80(asmText);
    return std::nullopt;
}

}  // namespace pcc
