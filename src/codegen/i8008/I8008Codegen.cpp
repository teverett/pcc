#include "I8008Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "I8008Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where this backend's own fixed memory cells live - `ac`/`bc`/`cx`/
// `dx`/`ptr`/`sp` plus the real runtime stack `sp` points into (see
// I8008Codegen.h's own header comment) - pinned to known, fixed
// addresses (via the assembler's own `name = expr` equate support,
// unlike every other backend's own `ac` etc., which are ordinary
// sequentially-laid-out temps) purely so tests/run_tests.sh's own
// from-scratch Python 8008 emulator can read the final result
// straight out of memory at a known address, without needing to
// parse this assembler's own symbol table at all. Comfortably above
// where this backend's own generated code and globals could ever
// reach for an ordinary test program, and comfortably inside the
// 8008's real 14-bit (16KB) address space.
constexpr uint32_t kPseudoBase = 0x3000;
// How many 16-bit values this backend's own software stack can hold
// at once across the whole program - generous for ordinary test
// programs (every binary op, `*`, `/`, comparison, and indexed/deref
// assignment pushes at most one value for the lifetime of evaluating
// one subexpression), but a real, fixed limit with no overflow
// check - see pushAc()'s own comment.
[[maybe_unused]] constexpr int kStackSize = 32;  // documents the reserved region's size; nothing needs to read it
                                                  // back since the stack's own content is never initialized
constexpr uint32_t kStackBase = kPseudoBase + 12;
}  // namespace

// Pushes `ac`'s current value onto this backend's own real runtime
// stack: read `ac`'s two bytes into B/C first (computing `sp`'s own
// value needs HL, which would otherwise clobber `ac` itself mid-read
// - same reasoning as storeAcIndirect()), then write them at the
// current top of stack and advance `sp` by one word.
void I8008Codegen::pushAc() {
    pointAt("ac");
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt("ac+1");
    code_ << "    mov a, m\n    mov c, a\n";
    loadHLFromCell("sp");
    code_ << "    mov a, b\n    mov m, a\n";
    incHL();
    code_ << "    mov a, c\n    mov m, a\n";
    incHL();
    storeHLToCell("sp");
}

// The reverse: retreats `sp` by one word first, then reads the two
// bytes now at the top of stack into `dst` - never touches `ac`
// itself unless `dst` happens to be "ac". Doesn't call
// storeHLToCell() - writing the decremented address back to `sp`
// needs pointAt("sp"), which would repoint the real HL away from the
// very address this function still needs to read the popped word
// from, so the decremented address is stashed in D/E first instead
// and `sp` is written from there once the read is already done.
void I8008Codegen::popInto(const std::string &dst) {
    loadHLFromCell("sp");  // HL = current sp (one past the last pushed word)
    decHL();
    decHL();  // HL = the address of the word being popped
    code_ << "    mov a, l\n    mov d, a\n";  // stash the decremented address in D/E
    code_ << "    mov a, h\n    mov e, a\n";
    code_ << "    mov a, m\n    mov b, a\n";  // read the popped word while HL still points at it
    incHL();
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt("sp");
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt("sp+1");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt(dst);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::storeHLToCell(const std::string &cell) {
    code_ << "    mov a, l\n    mov d, a\n";
    code_ << "    mov a, h\n    mov e, a\n";
    pointAt(cell);
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt(cell + "+1");
    code_ << "    mov a, e\n    mov m, a\n";
}

void I8008Codegen::decHL() {
    std::string Lskip = newLabel();
    code_ << "    mov a, l\n    and a\n";  // Z set iff l==0 (about to borrow into h)
    code_ << "    jfz " << Lskip << "\n";  // l != 0 - no borrow needed
    code_ << "    dcr h\n";
    code_ << Lskip << ":\n";
    code_ << "    dcr l\n";
}

void I8008Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

void I8008Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
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

void I8008Codegen::pointAt(const std::string &lbl) {
    code_ << "    mvihi h, " << lbl << "\n";
    code_ << "    mvi l, " << lbl << "\n";
}

void I8008Codegen::loadHLFromCell(const std::string &cell) {
    pointAt(cell);
    code_ << "    mov a, m\n    mov e, a\n";  // e = low byte of the stored value
    pointAt(cell + "+1");
    code_ << "    mov a, m\n    mov d, a\n";  // d = high byte of the stored value
    code_ << "    mov l, e\n    mov h, d\n";  // HL = (d,e) - the value itself, not cell's own address
}

void I8008Codegen::storeImmediate16(const std::string &dst, const std::string &exprText) {
    pointAt(dst);
    code_ << "    mvi a, " << exprText << "\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mvihi a, " << exprText << "\n    mov m, a\n";
}

void I8008Codegen::storeAcIndirect(const std::string &addrCell) {
    pointAt("ac");
    code_ << "    mov a, m\n    mov b, a\n";  // b = ac's low byte
    pointAt("ac+1");
    code_ << "    mov a, m\n    mov c, a\n";  // c = ac's high byte
    loadHLFromCell(addrCell);                  // HL = the real target address
    code_ << "    mov a, b\n    mov m, a\n";
    incHL();
    code_ << "    mov a, c\n    mov m, a\n";
}

// A real 16-bit increment of the HL *register pair* itself (not a
// memory cell - see copy16()/pushAc() for that) - the 8008 has no
// 16-bit increment instruction at all, so this chains two 8-bit
// `INR`s through a carry test, the same workaround the 6502 backend's
// own inc16() needs for the same reason (see M6502Codegen.cpp).
void I8008Codegen::incHL() {
    std::string Lskip = newLabel();
    code_ << "    inr l\n";
    code_ << "    mov a, l\n    and a\n";  // Z set iff l wrapped to 0
    code_ << "    jfz " << Lskip << "\n";  // didn't wrap - skip the high-byte carry
    code_ << "    inr h\n";
    code_ << Lskip << ":\n";
}

// Copies one 16-bit memory cell to another - four real memory
// accesses (through B/C, since there's no way to read src's two
// bytes and write dst's two bytes without repointing HL in between -
// see this file's own header comment), not the one or two
// instructions every other backend's own copy16() needs.
void I8008Codegen::copy16(const std::string &dst, const std::string &src) {
    pointAt(src);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt(src + "+1");
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt(dst);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl`, through
// the accumulator (the 8008's only rotates - RLC/RRC/RAL/RAR - work
// on A alone, unlike the Z80 backend's own register-targeted `RL r`/
// `RR r` - see I8008Codegen.h's own header comment). `and a` is this
// target's own idiom for "clear carry without disturbing A" (every
// logical instruction clears CY as a side effect - confirmed against
// NDA/XRA/ORA's own documented "C=0" flag behavior).
void I8008Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    if (isLeft) {
        pointAt(lbl);
        code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`. High byte first, so its
        // own vacated-bit carry-out feeds the low byte's rotate.
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    and a\n    rar\n    mov m, a\n";
        pointAt(lbl);
        code_ << "    mov a, m\n    rar\n    mov m, a\n";
    }
}

std::string I8008Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the 8008 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=raw compile (see I8008Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    code_ << "org 0\n\n";
    // The fixed memory pseudo-registers and the runtime software
    // stack's own pointer cell - see this file's own anonymous-
    // namespace comment on `kPseudoBase`/`kStackBase`.
    code_ << "ac = " << kPseudoBase << "\n";
    code_ << "bc = " << (kPseudoBase + 2) << "\n";
    code_ << "cx = " << (kPseudoBase + 4) << "\n";
    code_ << "dx = " << (kPseudoBase + 6) << "\n";
    code_ << "ptr = " << (kPseudoBase + 8) << "\n";
    code_ << "sp = " << (kPseudoBase + 10) << "\n";
    code_ << "\n_start:\n";
    // The software stack grows up from `kStackBase`; `sp` always
    // holds the address *just past* the last pushed word (see
    // pushAc()/popInto()).
    storeImmediate16("sp", std::to_string(kStackBase));
    code_ << "    cal " << label(mainFn) << "\n";
    // The program's own final result sits in `ac` the moment
    // execution reaches `halt` below - the authoritative (and, on
    // this backend, only practical - there's no register wide enough
    // to dump the way every other backend's own halt sequence
    // leaves one) place to read it from.
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

// Single-file-only (see I8008Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> I8008Codegen::perFileAsm() const { return perFileAsm_; }

void I8008Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void I8008Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            pointAt("ac");
            code_ << "    mov a, m\n";
            pointAt("ac+1");
            code_ << "    or m\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jtz " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            pointAt("ac");
            code_ << "    mov a, m\n";
            pointAt("ac+1");
            code_ << "    or m\n";
            code_ << "    jtz " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jmp " << Lstart << "\n";
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

void I8008Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            storeImmediate16("ac", std::to_string(static_cast<uint16_t>(e.intValue)));
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
                copy16("ac", "ptr");
                pushAc();  // push ptr's own value, carried via ac
                genExpr(*e.rhs);
                popInto("ptr");  // ac is untouched by popInto() - it still holds rhs's value
                storeAcIndirect("ptr");
                return;
            }
            genExpr(*e.rhs);
            copy16(label(e.slot), "ac");
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            pushAc();  // ac already equals the pointer value - push it directly
            genExpr(*e.rhs);
            popInto("ptr");
            storeAcIndirect("ptr");
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                pointAt("ac");
                code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                pointAt("ac+1");
                code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                pointAt("ac");
                code_ << "    mov a, m\n    adi 1\n    mov m, a\n";
                pointAt("ac+1");
                code_ << "    mov a, m\n    aci 0\n    mov m, a\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                loadHLFromCell("ptr");  // HL = the pointer's own value (the target address)
                code_ << "    mov a, m\n    mov b, a\n";
                incHL();
                code_ << "    mov a, m\n    mov c, a\n";
                pointAt("ac");
                code_ << "    mov a, b\n    mov m, a\n";
                pointAt("ac+1");
                code_ << "    mov a, c\n    mov m, a\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                pointAt("ac");
                code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                pointAt("ac+1");
                code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
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
            code_ << "    cal " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            err(e.line,
                "the 8008 has no interrupt enable/disable instruction at all (interrupts work by an external "
                "signal forcing an instruction onto the bus, not a software EI/DI the way every later chip in "
                "this family has one) - _ion()/_iof() aren't supported on this target, see I8008Codegen.h's own "
                "header comment");
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated first, and alone - see
// ../i8080/I8080Codegen.cpp's own genIndexAddr() comment on why.
// `slot->arraySize > 0` means the base is a compile-time-known
// address (loaded via `mvi`/`mvihi` directly, same split-byte trick
// as everywhere else in this backend); otherwise `slot` is a plain
// pointer variable and the base instead comes from *reading* that
// variable's own stored value. Either way there's no hardware 16-bit
// add here (no `DAD` - see I8008Codegen.h's own header comment), so
// the doubled index and the base are added byte-wise through the
// accumulator with the carry chained from low into high.
void I8008Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ptr = index*2 (doubling, via self-add - words are 2 bytes on
    // this byte-addressed target, same reason every other backend's
    // own indexing doubles the index first).
    pointAt("ac");
    code_ << "    mov a, m\n    mov b, a\n";  // b = index_lo
    pointAt("ac+1");
    code_ << "    mov a, m\n    mov c, a\n";  // c = index_hi
    code_ << "    mov a, b\n    add b\n    mov d, a\n";  // d = index_lo + index_lo
    code_ << "    mov a, c\n    adc c\n    mov e, a\n";  // e = index_hi + index_hi + carry
    pointAt("ptr");
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt("ptr+1");
    code_ << "    mov a, e\n    mov m, a\n";

    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        code_ << "    mvi b, " << lbl << "\n";     // b = low byte of the array's own address
        code_ << "    mvihi c, " << lbl << "\n";    // c = high byte
    } else {
        pointAt(lbl);
        code_ << "    mov a, m\n    mov b, a\n";
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    mov c, a\n";
    }
    pointAt("ptr");
    code_ << "    mov a, m\n    add b\n    mov d, a\n";
    pointAt("ptr+1");
    code_ << "    mov a, m\n    adc c\n    mov e, a\n";
    pointAt("ptr");
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt("ptr+1");
    code_ << "    mov a, e\n    mov m, a\n";
}

void I8008Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadHLFromCell("ptr");  // HL = the computed address itself
    code_ << "    mov a, m\n    mov b, a\n";
    incHL();
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt("ac");
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt("ac+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        storeImmediate16("ac", label(operand.slot));
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
// right one on the real runtime stack (pushAc()/popInto() - see
// I8008Codegen.h's own header comment on why a compile-time-indexed
// save cell doesn't work once a real function call is involved).
void I8008Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        pushAc();
        genExpr(*e.rhs);
        popInto("bc");  // bc = lhs
        const char *lo = op == "+" ? "add" : "sub";
        const char *hi = op == "+" ? "adc" : "sbb";
        pointAt("bc");
        code_ << "    mov a, m\n";  // a = lhs_lo
        pointAt("ac");
        code_ << "    " << lo << " m\n";  // a = lhs_lo (+/-) rhs_lo, CY set accordingly
        code_ << "    mov b, a\n";
        pointAt("bc+1");
        code_ << "    mov a, m\n";  // a = lhs_hi
        pointAt("ac+1");
        code_ << "    " << hi << " m\n";  // a = lhs_hi (+/-) rhs_hi (+/-) carry
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
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
        pushAc();
        genExpr(*e.rhs);
        popInto("bc");  // bc = lhs
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        pointAt("bc");
        code_ << "    mov a, m\n";
        pointAt("ac");
        code_ << "    " << insn << " m\n";
        code_ << "    mov b, a\n";
        pointAt("bc+1");
        code_ << "    mov a, m\n";
        pointAt("ac+1");
        code_ << "    " << insn << " m\n";
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
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

// No variable-count shift instruction (see I8008Codegen.h's own
// header comment) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit().
void I8008Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);  // ac = value to shift
    pushAc();
    genExpr(rhs);         // ac = shift count
    copy16("cx", "ac");  // stash the count in a memory cell, not a register - popInto() below
                          // uses every one of A/B/C/D/E/H/L as its own scratch internally
    popInto("bc");         // bc = value (restored only now)
    pointAt("cx");
    code_ << "    mov a, m\n    mov b, a\n";  // b = shift count (always small; low byte is enough)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a, b\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    shiftOneBit("bc", isLeft);
    code_ << "    dcr b\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply): `bc` is
// the multiplicand (doubled each iteration), `dx` the multiplier
// (halved each iteration), `ac` the accumulating product. Only
// verified against non-negative operands.
void I8008Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);  // ac = multiplicand
    pushAc();
    genExpr(rhs);
    popInto("bc");       // bc = multiplicand (restored only now)
    copy16("dx", "ac");    // dx = multiplier
    storeImmediate16("ac", "0");
    code_ << "    mvi d, 16\n";  // loop counter, in the real register D
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a, d\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    pointAt("dx");
    code_ << "    mov a, m\n    ani 1\n";
    code_ << "    jtz " << Lskipadd << "\n";
    pointAt("bc");
    code_ << "    mov a, m\n    mov e, a\n";  // e = bc_lo
    pointAt("ac");
    code_ << "    mov a, m\n    add e\n    mov m, a\n";
    pointAt("bc+1");
    code_ << "    mov a, m\n    mov e, a\n";  // e = bc_hi
    pointAt("ac+1");
    code_ << "    mov a, m\n    adc e\n    mov m, a\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dcr d\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative. Only verified against
// non-negative operands.
void I8008Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);  // ac = dividend
    pushAc();
    genExpr(rhs);
    popInto("dx");        // dx = dividend (running remainder, restored only now)
    copy16("bc", "ac");     // bc = divisor
    storeImmediate16("ac", "0");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentative dx - bc; carry (borrow) set afterward means dx < bc,
    // so this round's subtraction can't happen - stop.
    pointAt("dx");
    code_ << "    mov a, m\n";
    pointAt("bc");
    code_ << "    sub m\n";
    code_ << "    mov e, a\n";  // e = tentative low result
    pointAt("dx+1");
    code_ << "    mov a, m\n";
    pointAt("bc+1");
    code_ << "    sbb m\n";  // a = tentative high result; CY = final borrow
    code_ << "    jtc " << Ldone << "\n";
    // Commit: dx -= bc (the tentative result just computed).
    code_ << "    mov d, a\n";
    pointAt("dx");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt("dx+1");
    code_ << "    mov a, d\n    mov m, a\n";
    // Quotient += 1 - a 16-bit increment of the `ac` memory cell via
    // a carry-chained ADI/ACI (not incHL() - that's only for the
    // real HL register pair, see this file's own header comment).
    pointAt("ac");
    code_ << "    mov a, m\n    adi 1\n    mov m, a\n";
    pointAt("ac+1");
    code_ << "    mov a, m\n    aci 0\n    mov m, a\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void I8008Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b, checked the
// same "OR the two result bytes together" way genLogicalNot() checks
// a value for zero.
void I8008Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    pushAc();
    genExpr(rhs);
    popInto("bc");  // bc = lhs
    pointAt("bc");
    code_ << "    mov a, m\n";
    pointAt("ac");
    code_ << "    sub m\n";
    code_ << "    mov b, a\n";  // b = tentative low diff
    pointAt("bc+1");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    sbb m\n";
    code_ << "    or b\n";  // Z set iff both diff bytes zero
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (wantEqual ? "tz" : "fz") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// ../i8080/I8080Codegen.h's own header comment: the 8008, like the
// 8080, has no hardware overflow flag) - flipping the sign bit of
// each operand's high byte, in place, maps the signed range onto the
// unsigned one while preserving order, and the final borrow out of a
// plain byte-wise subtract is all that's left to check.
void I8008Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    pushAc();
    genExpr(b);
    popInto("bc");  // bc = a
    pointAt("bc+1");
    code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";  // a's high byte, sign flipped (in place)
    pointAt("ac+1");
    code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";  // b's high byte, sign flipped (in place)
    pointAt("bc");
    code_ << "    mov a, m\n";
    pointAt("ac");
    code_ << "    sub m\n";  // a'_lo - b'_lo, CY = borrow
    pointAt("bc+1");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    sbb m\n";  // a'_hi - b'_hi - borrow; CY now = (a < b) unbiased
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (invert ? "fc" : "tc") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    pointAt("ac");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    or m\n";
    code_ << "    jtz " << Lfalse << "\n";
    genExpr(rhs);
    pointAt("ac");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    or m\n";
    code_ << "    jtz " << Lfalse << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    storeImmediate16("ac", "0");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    pointAt("ac");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    or m\n";
    code_ << "    jfz " << Ltrue << "\n";
    genExpr(rhs);
    pointAt("ac");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    or m\n";
    code_ << "    jfz " << Ltrue << "\n";
    code_ << Lfalse << ":\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    pointAt("ac");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    or m\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jtz " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

std::string I8008Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 8008 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I8008Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleI8008(asmText);
    return std::nullopt;
}

}  // namespace pcc
