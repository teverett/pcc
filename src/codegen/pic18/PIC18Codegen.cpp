#include "PIC18Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "PIC18Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

void emitInitByte(std::ostream &out, const std::string &lbl, uint8_t v) {
    if (v == 0) {
        out << "    clrf " << lbl << "\n";
    } else {
        out << "    movlw " << static_cast<int>(v) << "\n    movwf " << lbl << "\n";
    }
}
}  // namespace

void PIC18Codegen::declareRam(const std::string &lbl, int count, bool zeroAtStart) {
    ramDecls_.push_back(lbl + ": res " + std::to_string(count));
    if (zeroAtStart) zeroAtStart_.push_back({lbl, count});
}

void PIC18Codegen::copy2(const std::string &dst, const std::string &src) {
    if (dst == src) return;
    code_ << "    movff " << src << "," << dst << "\n";
    code_ << "    movff " << src << "+1," << dst << "+1\n";
}

void PIC18Codegen::clear2(const std::string &lbl) { code_ << "    clrf " << lbl << "\n    clrf " << lbl << "+1\n"; }

void PIC18Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    bcf STATUS,0\n";
    if (isLeft) {
        code_ << "    rlcf " << lbl << ",f\n";
        code_ << "    rlcf " << lbl << "+1,f\n";
    } else {
        code_ << "    rrcf " << lbl << "+1,f\n";
        code_ << "    rrcf " << lbl << ",f\n";
    }
}

// dst = a + b via ADDWFC's own real carry chain - see PIC18Codegen.h's
// own header comment on why this needs no PIC16-style manual fixup.
void PIC18Codegen::add16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    addwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    addwfc " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

// dst = a - b via SUBWFB's own real borrow chain.
void PIC18Codegen::sub16(const std::string &dst, const std::string &a, const std::string &b) {
    code_ << "    movf " << b << ",w\n";
    code_ << "    subwf " << a << ",w\n";
    code_ << "    movwf " << dst << "\n";
    code_ << "    movf " << b << "+1,w\n";
    code_ << "    subwfb " << a << "+1,w\n";
    code_ << "    movwf " << dst << "+1\n";
}

void PIC18Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    incfsz " << lbl << ",f\n";
    code_ << "    goto " << Lskip << "\n";
    code_ << "    incf " << lbl << "+1,f\n";
    code_ << Lskip << ":\n";
}

void PIC18Codegen::testAcZero() { code_ << "    movf ac,w\n    iorwf ac+1,w\n"; }

void PIC18Codegen::gotoIfZero(const std::string &target) {
    code_ << "    btfsc STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

void PIC18Codegen::gotoIfNonZero(const std::string &target) {
    code_ << "    btfss STATUS,2\n";
    code_ << "    goto " << target << "\n";
}

// Via FSR0L/INDF0/POSTINC0 - one real instruction fewer than PIC16's
// own FSR/INDF-plus-explicit-INCF pair (see PIC18Codegen.h's own
// header comment): POSTINC0 reads/writes through FSR0 and then
// auto-increments it in the same instruction.
void PIC18Codegen::storeAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf ac,w\n    movwf POSTINC0\n";
    code_ << "    movf ac+1,w\n    movwf INDF0\n";
}

void PIC18Codegen::loadAcIndirect() {
    code_ << "    movf ptr,w\n    movwf FSR0L\n";
    code_ << "    movf POSTINC0,w\n    movwf ac\n";
    code_ << "    movf INDF0,w\n    movwf ac+1\n";
}

// Real runtime push/pop on the shared `SSTACK` region via `SP`/FSR0 -
// see PIC18Codegen.h's own header comment on why this has to be a
// genuine stack, not a compile-time-indexed fixed cell. POSTINC0
// (access, then increment) is a natural push. There's no hardware
// pre-decrement mode on this core to mirror it for pop (PIC18 only
// has INDFn/POSTINCn/POSTDECn/PREINCn/PLUSWn - no PREDECn), so pop
// instead decrements FSR0L explicitly (DECF) before each plain INDF0
// access - two real instructions instead of one, but still correct.
// `SP` is kept in sync with FSR0L itself by simply copying it back
// out afterward.
void PIC18Codegen::pushCell2(const std::string &src) {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    movf " << src << ",w\n    movwf POSTINC0\n";
    code_ << "    movf " << src << "+1,w\n    movwf POSTINC0\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::popCell2(const std::string &dst) {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf " << dst << "+1\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf " << dst << "\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::pushPtrCell() {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    movf ptr,w\n    movwf POSTINC0\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

void PIC18Codegen::popPtrCell() {
    code_ << "    movf SP,w\n    movwf FSR0L\n";
    code_ << "    decf FSR0L,f\n    movf INDF0,w\n    movwf ptr\n";
    code_ << "    movf FSR0L,w\n    movwf SP\n";
}

std::string PIC18Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the pic18 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=hex compile (see PIC18Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    declareRam("ac", 2, false);
    declareRam("bc", 2, false);
    declareRam("cx", 2, false);
    declareRam("dx", 2, false);
    declareRam("ptr", 1, false);
    declareRam("SP", 1, false);
    declareRam("SSTACK", 24, false);

    std::ostringstream globalInit;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareRam(lbl, slot->arraySize * 2, false);
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                uint16_t v16 = static_cast<uint16_t>(slot->arrayInit[i]);
                std::string base = lbl + "+" + std::to_string(i * 2);
                std::string hi = lbl + "+" + std::to_string(i * 2 + 1);
                emitInitByte(globalInit, base, static_cast<uint8_t>(v16 & 0xFF));
                emitInitByte(globalInit, hi, static_cast<uint8_t>((v16 >> 8) & 0xFF));
            }
        } else {
            declareRam(lbl, 2, false);
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            emitInitByte(globalInit, lbl, static_cast<uint8_t>(v16 & 0xFF));
            emitInitByte(globalInit, lbl + "+1", static_cast<uint8_t>((v16 >> 8) & 0xFF));
        }
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    std::ostringstream header;
    // Access Bank SFR short addresses (see PIC18Codegen.h's own
    // header comment - this is the real PIC18F2420 SFR map's own
    // bottom byte, the exact form every instruction here other than
    // MOVFF resolves via the Access Bank's own 0x60-0xFF->0xF60-0xFFF
    // hardware aliasing).
    header << "STATUS = 0xD8\nFSR0L = 0xE9\nFSR0H = 0xEA\nPOSTINC0 = 0xEE\nINDF0 = 0xEF\nINTCON = 0xF2\n\n";
    header << "org 0\n    goto START\n\n";
    header << "START:\n";
    header << "    clrf FSR0H\n";  // FSR0 only ever needs its low byte set - see PIC18Codegen.h's own header
                                    // comment (this target's own data is restricted to the Access Bank's own
                                    // low byte, so the high byte of a real pointer never has to change)
    header << "    movlw SSTACK\n    movwf SP\n";
    for (const auto &entry : zeroAtStart_) {
        for (int i = 0; i < entry.second; i++) {
            header << "    clrf " << entry.first;
            if (i > 0) header << "+" << i;
            header << "\n";
        }
    }
    header << globalInit.str();
    header << "    call " << label(mainFn) << "\n";
    header << "HALT:\n    goto HALT\n\n";

    std::ostringstream out;
    out << header.str();
    out << code_.str();
    out << "\norgram 0x00\n";
    for (const auto &l : ramDecls_) out << l << "\n";
    return out.str();
}

std::map<std::string, std::string> PIC18Codegen::perFileAsm() const { return perFileAsm_; }

void PIC18Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareRam(label(p), 2);
    genStmt(*fn.body);
    code_ << "    return\n\n";
}

void PIC18Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testAcZero();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            gotoIfZero(s.elseStmt ? Lelse : Ldone);
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    goto " << Ldone << "\n";
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
            testAcZero();
            gotoIfZero(Lend);
            genStmt(*s.loopBody);
            code_ << "    goto " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    return\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareRam(lbl, s.declSlot->arraySize * 2);
            } else {
                declareRam(lbl, 2);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    copy2(lbl, "ac");
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

void PIC18Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movlw " << (v16 & 0xFF) << "\n    movwf ac\n";
            code_ << "    movlw " << ((v16 >> 8) & 0xFF) << "\n    movwf ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            copy2("ac", label(e.slot));
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                pushPtrCell();
                genExpr(*e.rhs);
                popPtrCell();
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            copy2(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    movf ac,w\n    movwf ptr\n";
            pushPtrCell();
            genExpr(*e.rhs);
            popPtrCell();
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    comf ac,f\n    comf ac+1,f\n";
                inc16("ac");
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    movf ac,w\n    movwf ptr\n";
                loadAcIndirect();
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    comf ac,f\n    comf ac+1,f\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                copy2("p_" + e.func->sourceName + "_" + std::to_string(i), "ac");
            }
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real PIC18 global-interrupt enable/disable - INTCON<7>
            // (GIE), the exact same bit position and mechanism
            // PIC16Codegen.cpp's own _ion()/_iof() uses.
            code_ << "    " << (e.op == "ion" ? "bsf" : "bcf") << " INTCON,7\n";
            return;
        }
    }
}

void PIC18Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    shiftOneBit("ac", /*isLeft=*/true);  // ac *= 2
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    movlw " << lbl << "\n";
    } else {
        code_ << "    movf " << lbl << ",w\n";
    }
    code_ << "    addwf ac,w\n";
    code_ << "    movwf ptr\n";
}

void PIC18Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadAcIndirect();
}

void PIC18Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    movlw " << label(operand.slot) << "\n    movwf ac\n";
        code_ << "    clrf ac+1\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movf ptr,w\n    movwf ac\n    clrf ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void PIC18Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        pushCell2("ac");
        genExpr(*e.rhs);
        popCell2("bc");
        if (op == "+") {
            add16("ac", "bc", "ac");
        } else {
            sub16("ac", "bc", "ac");
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
        pushCell2("ac");
        genExpr(*e.rhs);
        popCell2("bc");
        const char *insn = (op == "&") ? "andwf" : (op == "|") ? "iorwf" : "xorwf";
        code_ << "    movf bc,w\n    " << insn << " ac,w\n    movwf ac\n";
        code_ << "    movf bc+1,w\n    " << insn << " ac+1,w\n    movwf ac+1\n";
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

void PIC18Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    code_ << "    movf ac,w\n    movwf cx\n";
    popCell2("ac");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    shiftOneBit("ac", isLeft);
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("dx", "ac");
    popCell2("bc");
    clear2("ac");
    code_ << "    movlw 16\n    movwf cx\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movf cx,w\n";
    code_ << "    btfsc STATUS,2\n    goto " << Ldone << "\n";
    code_ << "    movf dx,w\n    andlw 1\n";
    code_ << "    btfsc STATUS,2\n    goto " << Lskipadd << "\n";
    add16("ac", "ac", "bc");
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    decf cx,f\n";
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    copy2("bc", "ac");
    popCell2("dx");
    clear2("ac");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    sub16("cx", "dx", "bc");
    code_ << "    btfss STATUS,0\n    goto " << Ldone << "\n";
    copy2("dx", "cx");
    inc16("ac");
    code_ << "    goto " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy2("ac", "dx");
}

void PIC18Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void PIC18Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    pushCell2("ac");
    genExpr(rhs);
    popCell2("bc");  // bc = lhs, ac = rhs
    sub16("cx", "bc", "ac");
    code_ << "    movf cx,w\n    iorwf cx+1,w\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "btfsc" : "btfss") << " STATUS,2\n    goto " << Ltrue << "\n";
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the real N-xor-OV rule (see
// PIC18Codegen.h's own header comment on why this core, unlike
// PIC16's, has a real hardware overflow flag to use instead of a
// sign-bit-bias trick): after sub16()'s own final SUBWFB, "a < b"
// holds iff STATUS<N> and STATUS<OV> disagree.
void PIC18Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    pushCell2("ac");
    genExpr(b);
    popCell2("bc");  // bc = a, ac = b
    sub16("cx", "bc", "ac");  // cx = a - b; STATUS<N,OV> reflect the signed result
    std::string LovSet = newLabel(), Lless = newLabel(), Lnotless = newLabel(), Ldone = newLabel();
    code_ << "    btfsc STATUS,3\n    goto " << LovSet << "\n";
    code_ << "    btfsc STATUS,4\n    goto " << Lless << "\n";
    code_ << "    goto " << Lnotless << "\n";
    code_ << LovSet << ":\n";
    code_ << "    btfss STATUS,4\n    goto " << Lless << "\n";
    code_ << "    goto " << Lnotless << "\n";
    code_ << Lless << ":\n";
    if (!invert) {
        code_ << "    movlw 1\n    movwf ac\n    clrf ac+1\n";
    } else {
        clear2("ac");
    }
    code_ << "    goto " << Ldone << "\n";
    code_ << Lnotless << ":\n";
    if (!invert) {
        clear2("ac");
    } else {
        code_ << "    movlw 1\n    movwf ac\n    clrf ac+1\n";
    }
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testAcZero();
    gotoIfZero(Lfalse);
    genExpr(rhs);
    testAcZero();
    gotoIfZero(Lfalse);
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n    goto " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    clear2("ac");
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testAcZero();
    gotoIfNonZero(Ltrue);
    genExpr(rhs);
    testAcZero();
    gotoIfNonZero(Ltrue);
    code_ << Lfalse << ":\n";
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

void PIC18Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testAcZero();
    std::string Ltrue = newLabel(), Ldone = newLabel();
    gotoIfZero(Ltrue);
    clear2("ac");
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n    movlw 1\n    movwf ac\n    clrf ac+1\n";
    code_ << Ldone << ":\n";
}

std::string PIC18Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the pic18 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'hex' is)");
}

std::optional<std::string> PIC18Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "hex") return assemblePIC18(asmText);
    return std::nullopt;
}

}  // namespace pcc
