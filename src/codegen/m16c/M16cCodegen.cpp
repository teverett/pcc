#include "M16cCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "M16cAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Load/execution start address for a whole-program raw binary; chosen clear
// of the M16C's fixed low-memory special-function-register block
// (0x0000-0x03FF on real silicon).
constexpr uint32_t kBaseAddress = 0x0400;
// Initial stack pointer, comfortably above the loaded program and its data.
constexpr uint32_t kInitialSp = 0xE000;
}  // namespace

void M16cCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element's decimal text, or a symbol element's label text.
std::string M16cCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int>(static_cast<uint16_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType`'s width picks `db` (`char` array, one byte per element) vs `dw`
// (every other array - a `long`/`long long` array element still only
// occupies one native word, see genIndex()).
void M16cCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    const char *directive = widthOfType(elemType) == 1 ? "db" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

// Stores arrayInit into `slot`'s frame slots; re-run on every call since a
// Local's storage (unlike a Global's static trailer) isn't pre-set. An
// element's address is `slot->frameOffset` plus `i` byte-widths below `fb`.
void M16cCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int stride = widthOfType(slot->type) == 1 ? 1 : 2;
    bool isChar = stride == 1;
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        code_ << "    mov.w " << arrayElemText(slot->arrayInit[i]) << ",r0\n";
        int off = slot->frameOffset + stride * static_cast<int>(i);
        code_ << "    " << (isChar ? "movb" : "mov.w") << " r0,[fb-" << off << "]\n";
    }
}

std::string M16cCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    mov.w " << kInitialSp << ",sp\n";
        code_ << "    jsr.w " << label(mainFn) << "\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w <= 2) {
                const char *directive = w == 1 ? "db" : "dw";
                trailer_.push_back(lbl + ": " + directive + " " +
                                    std::to_string(static_cast<int>(static_cast<uint16_t>(v))) + "  ; " +
                                    slot->sourceName);
            } else {
                // No 32-/64-bit data directive in this dialect, so a
                // `long`/`long long` global's words are emitted individually
                // (least significant first, matching wideWordOperand()).
                auto uv = static_cast<unsigned long>(v);
                std::ostringstream line;
                line << lbl << ": dw ";
                for (int i = 0; i < w / 2; i++) {
                    if (i > 0) line << ", ";
                    line << static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
                }
                line << "  ; " << slot->sourceName;
                trailer_.push_back(line.str());
            }
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> M16cCodegen::perFileAsm() const { return perFileAsm_; }

void M16cCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push.w fb\n";
    code_ << "    mov.w sp,fb\n";
    if (fn.func->frameSize > 0) code_ << "    sub.w " << fn.func->frameSize << ",sp\n";
    genStmt(*fn.body);
    code_ << "    mov.w fb,sp\n";
    code_ << "    pop.w fb\n";
    code_ << "    rts\n\n";
}

void M16cCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testTruthy(*s.cond);
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jeq " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testTruthy(*s.cond);
            code_ << "    jeq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                // A wide value sits on the stack (LSW on top); pop it into
                // r0(LSW)..r(words-1)(MSW) before the epilogue's `mov.w
                // fb,sp` discards this call's whole frame.
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) code_ << "    pop.w r" << i << "\n";
                }
            }
            // Inlined frame teardown, since genFunction()'s own epilogue
            // never runs for this early-exit path.
            code_ << "    mov.w fb,sp\n";
            code_ << "    pop.w fb\n";
            code_ << "    rts\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                if (isWide(s.declSlot->type)) {
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                } else {
                    storeOperand(frameOperand(s.declSlot), s.declSlot->type);
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

void M16cCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            code_ << "    mov.w " << static_cast<int>(static_cast<uint16_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            loadOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs - never wide (see genIndex()'s comment)
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                code_ << "    push.w a0\n";
                genExpr(*e.rhs);
                code_ << "    pop.w a0\n";
                code_ << "    " << (widthOfType(e.slot->type) == 1 ? "stb" : "st") << " r0,a0\n";
                return;
            }
            if (isWide(e.slot->type)) {
                genExpr(*e.rhs);  // pushes `words` words
                int words = wordCountOf(e.slot->type);
                for (int i = 0; i < words; i++) {
                    code_ << "    mov.w [sp+" << (2 * i) << "],r0\n";
                    code_ << "    mov.w r0," << wideWordOperand(e.slot, i) << "\n";
                }
                code_ << "    add.w " << (2 * words) << ",sp\n";
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores through at e.type's width. A wide pointee has no
            // `[a0+N]` offset addressing (this dialect's `ld`/`st` only
            // address through a0 with no displacement), so each word is
            // stored through a0 one at a time, advancing it 2 bytes between
            // words.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov.w r0,a0\n";
            code_ << "    push.w a0\n";  // save the address below rhs's own pushed words
            genExpr(*e.rhs);
            if (isWide(e.type)) {
                int words = wordCountOf(e.type);
                code_ << "    mov.w [sp+" << (2 * words) << "],a0\n";
                for (int i = 0; i < words; i++) {
                    code_ << "    pop.w r0\n";
                    code_ << "    st r0,a0\n";
                    if (i + 1 < words) code_ << "    add.w 2,a0\n";
                }
                code_ << "    add.w 2,sp\n";  // discard the now-read saved address word
                return;
            }
            code_ << "    pop.w a0\n";
            code_ << "    " << (widthOfType(e.type) == 1 ? "stb" : "st") << " r0,a0\n";
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs - never wide, same limitation as indexed Assign above
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                genCompoundThroughA0(e);
                return;
            }
            genExpr(*e.rhs);              // r0 = rhs
            code_ << "    mov.w r0,r1\n";  // r1 = rhs
            loadOperand(operandOf(e.slot), e.type);  // r0 = old
            if (e.yieldOld) code_ << "    push.w r0\n";
            genCompoundOp(e.op, e.type);
            narrowR0To(e.type);
            storeOperand(operandOf(e.slot), e.type);
            if (e.yieldOld) code_ << "    pop.w r0\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov.w r0,a0\n";
            genCompoundThroughA0(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jeq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genWideNegate(wordCountOf(e.rhs->type));
                } else {
                    code_ << "    neg.w r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov.w r0,a0\n";
                if (isWide(e.type)) {
                    // Pushes MSW first .. LSW last (ending on top); each word
                    // read through a0 one at a time (no `[a0+N]` form exists).
                    int words = wordCountOf(e.type);
                    if (words > 1) code_ << "    add.w " << (2 * (words - 1)) << ",a0\n";
                    for (int i = words - 1; i >= 0; i--) {
                        code_ << "    ld a0,r0\n";
                        code_ << "    push.w r0\n";
                        if (i > 0) code_ << "    sub.w 2,a0\n";
                    }
                    return;
                }
                code_ << "    " << (widthOfType(e.type) == 1 ? "ldb" : "ld") << " a0,r0\n";
                if (widthOfType(e.type) == 1 && e.type.isUnsigned) code_ << "    and.w 0x00FF,r0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not.w r0\n";
                if (isWide(e.rhs->type)) {
                    for (int i = 1; i < wordCountOf(e.rhs->type); i++) code_ << "    not.w [sp+" << (2 * i) << "]\n";
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed last-declared first, so argument 0 ends
            // up closest to the return address `jsr.w` is about to push
            // (matching frameOperand()'s `fb+4` for paramIndex 0). A wide
            // argument pushes its own `words` words directly; a narrow one
            // is pushed here from r0. The caller cleans the pushed words
            // back off afterward.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    totalWords += wordCountOf(e.args[i]->type);
                } else {
                    code_ << "    push.w r0\n";
                    totalWords += 1;
                }
            }
            code_ << "    jsr.w " << label(e.func) << "\n";
            if (totalWords > 0) code_ << "    add.w " << (2 * totalWords) << ",sp\n";
            if (isWide(e.type)) {
                // The callee left its wide return value in r0(LSW)..
                // r(words-1)(MSW) (see genStmt()'s Return case); push it
                // back as this expression's result, LSW on top.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    mov.w r" << i << ",r0\n    push.w r0\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // The real `JSRI.W` indirect-call instruction fetches the entry
            // address from the WORD stored at its operand (not the operand
            // address itself), so the callee's address (just computed into
            // r0) is spilled to a temp cell first, then `jsri.w` points at
            // that cell.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            std::string T = allocTemp();
            declareTemp(T);
            code_ << "    mov.w r0," << T << "\n";
            code_ << "    jsri.w " << T << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            code_ << "    " << (e.op == "ion" ? "fset i" : "fclr i") << "\n";
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// Leaves slot[idxExpr]'s effective address in `A0`. `slot->arraySize > 0`
// means a compile-time-known base (loaded via `lea`); otherwise `slot` is a
// plain pointer variable, so the base comes from reading its stored value.
//
// A Global array's elements sit at increasing addresses as the index
// increases, but a Local array's elements sit at DECREASING addresses
// (frameOperand()'s address is the array's first word, closest to `fb`;
// later words are further below it) - so indexing is base-minus-scaled-index
// for a Local, base-plus for a Global. Getting this backwards corrupts the
// saved `fb`/return address as soon as anything indexes past element 0.
void M16cCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    shl.w " << shift << ",r0\n";  // r0 *= widthOf(slot->type) (a power of two)
    if (slot->arraySize > 0) {
        code_ << "    lea " << operandOf(slot) << ",a0\n";
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    add.w r0,a0\n";
        } else {
            code_ << "    sub.w r0,a0\n";
        }
    } else {
        code_ << "    mov.w " << operandOf(slot) << ",a0\n";
        code_ << "    add.w r0,a0\n";
    }
}

// A `char` array element is one byte (`ldb`); every other array element
// still only reads one native word (`ld`) regardless of its real width - a
// known gap for `long`/`long long` array elements (genIndexAddr() scales the
// index correctly, but a multi-word element's remaining words are never
// read/written); scalars of every width work fully.
void M16cCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    " << (widthOfType(e.slot->type) == 1 ? "ldb" : "ld") << " a0,r0\n";
    if (widthOfType(e.slot->type) == 1 && e.slot->type.isUnsigned) code_ << "    and.w 0x00FF,r0\n";
}

void M16cCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    lea " << operandOf(operand.slot) << ",r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov.w a0,r0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    lea " << label(operand.func) << ",r0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void M16cCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every operand here shares one common type via `e.lhs->type`, except
    // shift (only the LHS's type matters) and &&/|| (width-independent).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    int words = wide ? wordCountOf(operandType) : 1;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+", words);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push.w r0\n";
        genExpr(*e.rhs);
        code_ << "    pop.w r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add.w r1,r0\n";  // r0 = r0 + r1 = rhs + lhs
        } else {
            code_ << "    sub.w r0,r1\n";  // r1 = r1 - r0 = lhs - rhs
            code_ << "    mov.w r1,r0\n";
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs, words);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned, words);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        if (wide) {
            genWideBitwise(*e.lhs, *e.rhs, op, words);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push.w r0\n";
        genExpr(*e.rhs);
        code_ << "    pop.w r1\n";  // r1 = lhs, r0 = rhs
        const char *insn = (op == "&") ? "and.w" : (op == "|") ? "or.w" : "xor.w";
        code_ << "    " << insn << " r1,r0\n";  // r0 = r0 OP r1 (commutative)
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned, words);
            return;
        }
        genShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
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
    if (wide) {
        genWideCompare(op, *e.lhs, *e.rhs, isUnsigned, words);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs, isUnsigned);
}

// Real variable-count hardware shift: `SHL.W` for a left shift or unsigned
// right shift, `SHA.W` (sign-preserving) for a signed right shift - its
// signed count convention (negative = right) is why `isLeft` only decides
// whether to `NEG.W` the count, not which instruction to use.
void M16cCodegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";  // r1 = shift count
    code_ << "    pop.w r0\n";     // r0 = value to shift
    if (!isLeft) code_ << "    neg.w r1\n";
    code_ << "    " << ((isLeft || isUnsigned) ? "shl.w" : "sha.w") << " r1,r0\n";
}

// `MUL.W`'s low result word is identical whether the operands are signed or
// unsigned, so this one routine serves both.
void M16cCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    pop.w r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    mul.w r1,r0\n";   // r2:r0 = r0 * r1 = rhs * lhs (high word:low word)
}

// `DIVU.W` only divides unsigned magnitudes, so a signed division/modulo
// records each operand's sign, negates to a non-negative magnitude, divides,
// then restores the quotient's sign (negative iff the signs disagreed) and
// the remainder's sign (always the dividend's, matching C's
// truncate-toward-zero convention).
void M16cCodegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    if (isUnsigned) {
        code_ << "    mov.w 0,r2\n";
        code_ << "    divu.w r1,r0\n";  // r0 = quotient, r2 = remainder
        if (wantRemainder) code_ << "    mov.w r2,r0\n";
        return;
    }
    std::string Ld = newLabel(), Lr = newLabel();
    code_ << "    mov.w 0,r3\n";  // r3 = dividend-negative flag
    code_ << "    cmp.w 0,r0\n    jge " << Ld << "\n";
    code_ << "    neg.w r0\n    mov.w 1,r3\n";
    code_ << Ld << ":\n";
    code_ << "    push.w r3\n";   // stash the dividend-negative flag across the divisor's own sign check
    code_ << "    mov.w 0,r3\n";  // r3 = divisor-negative flag
    code_ << "    cmp.w 0,r1\n    jge " << Lr << "\n";
    code_ << "    neg.w r1\n    mov.w 1,r3\n";
    code_ << Lr << ":\n";
    code_ << "    mov.w 0,r2\n";
    code_ << "    divu.w r1,r0\n";  // r0 = |quotient|, r2 = |remainder|
    std::string Lskip = newLabel();
    if (wantRemainder) {
        code_ << "    mov.w r2,r0\n";
        code_ << "    pop.w r3\n";  // r3 = dividend-negative flag (divisor's sign doesn't affect the remainder)
        code_ << "    cmp.w 0,r3\n    jeq " << Lskip << "\n";
        code_ << "    neg.w r0\n";
        code_ << Lskip << ":\n";
        return;
    }
    code_ << "    xor.w r3,[sp]\n";  // [sp] (dividend-negative) ^= r3 (divisor-negative) -> quotient-negative
    code_ << "    pop.w r3\n";
    code_ << "    cmp.w 0,r3\n    jeq " << Lskip << "\n";
    code_ << "    neg.w r0\n";
    code_ << Lskip << ":\n";
}

void M16cCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";  // r1 = divisor
    code_ << "    pop.w r0\n";     // r0 = dividend
    genDivModCore(wantRemainder, isUnsigned);
}

void M16cCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitCompare(lhs, rhs, "jeq");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "jltu" : "jlt");
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "jleu" : "jle");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "jgtu" : "jgt");
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "jgeu" : "jge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.W src,dst` sets flags for `dst - src`, so `cmp.w r0,r1` (r0=rhs,
// r1=lhs) computes `lhs - rhs` with no further sign/overflow fixup needed.
void M16cCodegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    pop.w r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    cmp.w r0,r1\n";   // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpIfTrue << " " << Ltrue << "\n";
    code_ << "    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    cmp.w 0,r0\n";
        return;
    }
    int words = wordCountOf(e.type);
    code_ << "    mov.w [sp],r0\n";
    for (int i = 1; i < words; i++) code_ << "    or.w [sp+" << (2 * i) << "],r0\n";
    code_ << "    add.w " << (2 * words) << ",sp\n";
    code_ << "    cmp.w 0,r0\n";
}

void M16cCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jeq " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jeq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov.w 0,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jne " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

void M16cCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jeq " << Ltrue << "\n";
    code_ << "    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
}

// `movb` into a register destination always sign-extends to 16 bits, so an
// unsigned `char` has to mask the high byte back off afterward.
void M16cCodegen::loadOperand(const std::string &mem, Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    movb " << mem << ",r0\n";
        if (type.isUnsigned) code_ << "    and.w 0x00FF,r0\n";
    } else {
        code_ << "    mov.w " << mem << ",r0\n";
    }
}

void M16cCodegen::storeOperand(const std::string &mem, Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    movb r0," << mem << "\n";
    } else {
        code_ << "    mov.w r0," << mem << "\n";
    }
}

void M16cCodegen::narrowR0To(Type to) {
    if (widthOfType(to) != 1) return;  // a native word: r0 already holds the full value
    if (to.isUnsigned) {
        code_ << "    and.w 0x00FF,r0\n";
        return;
    }
    code_ << "    movb r0,r0\n";  // narrow to the low byte, then (register dest) sign-extend back up
}

void M16cCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") {
        code_ << "    add.w r1,r0\n";
        return;
    }
    if (op == "-") {
        code_ << "    sub.w r1,r0\n";
        return;
    }
    if (op == "*") {
        code_ << "    mul.w r1,r0\n";  // r2:r0 = r0*r1 = old*rhs (high word:low word)
        return;
    }
    if (op == "/" || op == "%") {
        genDivModCore(op == "%", isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and.w" : (op == "|") ? "or.w" : "xor.w";
        code_ << "    " << insn << " r1,r0\n";
        return;
    }
    // "<<" or ">>" - same signed-count convention as genShift().
    if (op != "<<") code_ << "    neg.w r1\n";
    code_ << "    " << ((op == "<<" || isUnsigned) ? "shl.w" : "sha.w") << " r1,r0\n";
}

// `e.rhs`'s evaluation can recursively reach codegen that clobbers r0-r3, so
// `a0` is stashed on the stack across it; genCompoundOp() itself never
// touches `a0`.
void M16cCodegen::genCompoundThroughA0(const ir::Expr &e) {
    code_ << "    push.w a0\n";    // stash address across rhs's own evaluation
    genExpr(*e.rhs);               // r0 = rhs
    code_ << "    mov.w r0,r1\n";  // r1 = rhs
    code_ << "    pop.w a0\n";     // a0 restored
    code_ << "    " << (widthOfType(e.type) == 1 ? "ldb" : "ld") << " a0,r0\n";  // r0 = old
    if (widthOfType(e.type) == 1 && e.type.isUnsigned) code_ << "    and.w 0x00FF,r0\n";
    if (e.yieldOld) code_ << "    push.w r0\n";  // stash old - this expr's own result
    genCompoundOp(e.op, e.type);  // r0 = new (raw), using r0=old, r1=rhs
    narrowR0To(e.type);
    code_ << "    " << (widthOfType(e.type) == 1 ? "stb" : "st") << " r0,a0\n";
    if (e.yieldOld) code_ << "    pop.w r0\n";  // r0 = old (final result)
}

std::string M16cCodegen::wideWordOperand(const ir::Slot *slot, int wordIndex) const {
    int off = 2 * wordIndex;
    if (slot->kind == ir::SlotKind::Global) {
        return off == 0 ? label(slot) : (label(slot) + "+" + std::to_string(off));
    }
    if (slot->kind == ir::SlotKind::Param) {
        return "[fb+" + std::to_string(slot->frameOffset + off) + "]";
    }
    return "[fb-" + std::to_string(slot->frameOffset + off) + "]";
}

void M16cCodegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    mov.w " << wideWordOperand(slot, i) << ",r0\n";
        code_ << "    push.w r0\n";
    }
}

void M16cCodegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        int shiftedWord = static_cast<int>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    mov.w " << shiftedWord << ",r0\n";
        code_ << "    push.w r0\n";
    }
}

void M16cCodegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop.w r0\n";
        code_ << "    mov.w r0," << wideWordOperand(slot, i) << "\n";
    }
}

// Word 0 is a plain `add.w`/`sub.w` (sets the carry/borrow flag), each later
// word folds that flag in via `adc.w`/`sbb.w`. Writing every result word
// over lhs's slot, then discarding rhs's now-stale block, leaves lhs's
// block - holding the result - as the new stack top.
void M16cCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    for (int i = 0; i < words; i++) {
        std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
        std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
        const char *mnemonic = i == 0 ? (isAdd ? "add.w" : "sub.w") : (isAdd ? "adc.w" : "sbb.w");
        code_ << "    " << mnemonic << " " << rhsOperand << "," << lhsOperand << "\n";
    }
    code_ << "    add.w " << (2 * words) << ",sp\n";
}

void M16cCodegen::genWideNegate(int words) {
    for (int i = 0; i < words; i++) code_ << "    not.w [sp+" << (2 * i) << "]\n";
    code_ << "    add.w 1,[sp+0]\n";
    for (int i = 1; i < words; i++) code_ << "    adc.w 0,[sp+" << (2 * i) << "]\n";
}

void M16cCodegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    const char *mnemonic = op == "&" ? "and.w" : op == "|" ? "or.w" : "xor.w";
    for (int i = 0; i < words; i++) {
        std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
        std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
        code_ << "    " << mnemonic << " " << rhsOperand << "," << lhsOperand << "\n";
    }
    code_ << "    add.w " << (2 * words) << ",sp\n";
}

// Compares most-significant word first; once a word differs, that
// comparison alone decides the result. Only the most significant word's
// comparison uses the signed jump when the type is signed - lower words,
// once reached, compare a pure magnitude.
void M16cCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                   int words) {
    genExpr(lhs);
    genExpr(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        for (int i = words - 1; i >= 0; i--) {
            std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
            std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
            code_ << "    cmp.w " << rhsOperand << "," << lhsOperand << "\n";
            code_ << "    jne " << (op == "==" ? Lfalse : Ltrue) << "\n";
        }
        code_ << "    jmp " << (op == "==" ? Ltrue : Lfalse) << "\n";
    } else {
        std::string signedJ = op == "<" ? "jlt" : op == "<=" ? "jle" : op == ">" ? "jgt" : "jge";
        std::string unsignedJ = op == "<" ? "jltu" : op == "<=" ? "jleu" : op == ">" ? "jgtu" : "jgeu";
        for (int i = words - 1; i >= 0; i--) {
            std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
            std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
            std::string Lcontinue = newLabel();
            code_ << "    cmp.w " << rhsOperand << "," << lhsOperand << "\n";
            code_ << "    jeq " << Lcontinue << "\n";
            bool useSigned = (i == words - 1) && !isUnsigned;
            code_ << "    " << (useSigned ? signedJ : unsignedJ) << " " << Ltrue << "\n";
            code_ << "    jmp " << Lfalse << "\n";
            code_ << Lcontinue << ":\n";
        }
        code_ << "    jmp " << Lfalse << "\n";  // every word equal
    }
    code_ << Lfalse << ":\n    mov.w 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov.w 1,r0\n";
    code_ << Ldone << ":\n";
    code_ << "    add.w " << (4 * words) << ",sp\n";  // release both operand blocks
}

// `SHL.W`/`SHA.W` only shift one native word at a time, so this loops one
// bit at a time, chaining the bit that crosses a word boundary through the
// carry flag via `ROL.W`/`ROR.W`.
void M16cCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);                // pushes `words` words
    code_ << "    push.w r1\n";  // reserve a slot to stash the shift count
    genExpr(rhs);                // r0 = shift count (already promoted to a plain int)
    code_ << "    mov.w r0,[sp]\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    cmp.w 0,[sp]\n    jeq " << Ldone << "\n";
    if (isLeft) {
        code_ << "    clc\n";
        for (int i = 0; i < words; i++) code_ << "    rol.w [sp+" << (2 + 2 * i) << "]\n";
    } else {
        if (isUnsigned) {
            code_ << "    clc\n";
        } else {
            // Seed the carry from the top word's current sign bit instead of
            // clearing it, so the first `ror.w` rotates that sign bit back
            // into the vacated top bit.
            std::string topWord = "[sp+" + std::to_string(2 + 2 * (words - 1)) + "]";
            std::string Lneg = newLabel(), Lcdone = newLabel();
            code_ << "    cmp.w 0," << topWord << "\n    jlt " << Lneg << "\n";
            code_ << "    clc\n    jmp " << Lcdone << "\n";
            code_ << Lneg << ":\n    stc\n";
            code_ << Lcdone << ":\n";
        }
        for (int i = words - 1; i >= 0; i--) code_ << "    ror.w [sp+" << (2 + 2 * i) << "]\n";
    }
    code_ << "    sub.w 1,[sp]\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    code_ << "    pop.w r1\n";  // discard the shift-count slot; the `words`-word result stays on top
}

// Not implemented: `MUL.W`/`DIVU.W` only widen one native word to two, and
// this project has no simulator to verify a hand-written multi-word
// multiply/divide against.
void M16cCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                  "see M16cCodegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void M16cCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                  "see M16cCodegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

void M16cCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // r0
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    and.w 0x00FF,r0\n";
            } else {
                code_ << "    movb r0,r0\n";  // narrow to the low byte, then (register dest) sign-extend back up
            }
        }
        return;
    }
    if (fromWide && !toWide) {
        int words = wordCountOf(from);
        genExpr(*e.lhs);                 // pushes `words` words
        code_ << "    mov.w [sp],r0\n";  // the low word is this type's own truncated value
        code_ << "    add.w " << (2 * words) << ",sp\n";
        if (widthOfType(to) == 1) {
            code_ << "    " << (to.isUnsigned ? "and.w 0x00FF,r0\n" : "movb r0,r0\n");
        }
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // r0 = the native-width value, already correctly sign/zero-extended to 16 bits
        code_ << "    push.w r0\n";
        if (from.isUnsigned) {
            for (int i = 1; i < toWords; i++) code_ << "    mov.w 0,r0\n    push.w r0\n";
        } else {
            genSignFillWord();  // r1 = 0xFFFF if r0 is negative, else 0
            for (int i = 1; i < toWords; i++) code_ << "    push.w r1\n";
        }
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromWords = wordCountOf(from);
    int toWords = wordCountOf(to);
    genExpr(*e.lhs);
    if (toWords > fromWords) {
        code_ << "    mov.w [sp+" << (2 * (fromWords - 1)) << "],r0\n";
        if (from.isUnsigned) {
            code_ << "    mov.w 0,r1\n";
        } else {
            genSignFillWord();
        }
        for (int i = 0; i < toWords - fromWords; i++) code_ << "    push.w r1\n";
    } else if (toWords < fromWords) {
        code_ << "    add.w " << (2 * (fromWords - toWords)) << ",sp\n";
    }
}

// No dedicated widen-with-sign instruction exists on this target, so this
// branches on the sign once and reuses the result for every extra word.
void M16cCodegen::genSignFillWord() {
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    cmp.w 0,r0\n    jlt " << Lneg << "\n";
    code_ << "    mov.w 0,r1\n    jmp " << Ldone << "\n";
    code_ << Lneg << ":\n    mov.w 0xFFFF,r1\n";
    code_ << Ldone << ":\n";
}

std::string M16cCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the m16c backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports");
}

std::optional<std::string> M16cCodegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleM16c(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM16cObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
