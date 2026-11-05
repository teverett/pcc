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

// Where the whole program is loaded and starts executing - same
// "bare flat binary, test harness sets PC here directly" convention
// as every other target's own `kBaseAddress` (see
// Pdp11Codegen.cpp/M68kCodegen.cpp), chosen well clear of the M16C's
// own fixed low-memory special-function-register block (0x0000-
// 0x03FF on real silicon) even though nothing here ever touches it.
constexpr uint32_t kBaseAddress = 0x0400;
// Where this backend's own entry sequence points SP (the real
// hardware stack pointer, used by `push.w`/`pop.w` and by
// `jsr.w`/`rts`) before calling main() - comfortably above the loaded
// program and its data, growing down into otherwise-unused memory.
constexpr uint32_t kInitialSp = 0xE000;
}  // namespace

void M16cCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element's own decimal text, or a symbol element's own
// label text (the referenced Slot's or Function's own address) -
// M16cAssembler.cpp's own `dw`/`db` directives already evaluate any
// expr operand, symbol included, the same way `lea`/`jsr`'s own label
// operand already does.
std::string M16cCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int>(static_cast<uint16_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

// `elemType`'s own width picks `db` (a `char` array, one byte per
// element) vs `dw` (every other array - a `long`/`long long` array's
// own elements still only ever occupy one native word each, a known
// gap - see genIndex()'s own comment).
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

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a
// Local's storage is this activation's own fresh frame region, not
// pre-set to anything; arrayInit is always fully populated up to
// arraySize (zero-padded even with no source-level initializer), so
// this always leaves the whole array in a known state, same as a
// Global array's static trailer always did. An element's own address
// is `slot->frameOffset` plus `i` byte-widths below `fb` (see
// genIndexAddr()'s own comment on this same `elemShift()`-scaled
// arithmetic) - a `char` array's own elements are one byte apart
// (`movb`), every other array's own elements one word apart (`mov.w`,
// same known one-native-word-per-element gap as declareArray()).
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
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true in whole-program mode), so
    // mainFn is never null there.

    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    mov.w " << kInitialSp << ",sp\n";
        code_ << "    jsr.w " << label(mainFn) << "\n";
        // The result sits in R0 the moment execution reaches `halt` - the
        // same "a real, directly inspectable register" rationale as the
        // pdp11/68000 backends' own R0/D0 (see M16cCodegen.h's own header
        // comment).
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // never possible here (no real separate compilation - see above), but
                                          // matches every other backend's own defensive check at this exact spot
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
                // `long`/`long long` global - no 32-/64-bit data
                // directive in this dialect, so its own words are
                // emitted individually (least significant first,
                // matching `wideWordOperand()`'s own layout).
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

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
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
                // A wide value sits on the stack (LSW on top - see
                // M16cCodegen.h's own header comment) after genExpr()
                // - pop it into r0(LSW)..r(words-1)(MSW) first, since
                // the epilogue's own `mov.w fb,sp` is about to discard
                // this call's whole frame (and anything still sitting
                // above it on the stack) before `rts` ever runs - see
                // genExpr()'s own Call case for the matching read-it-
                // back-out-of-registers half of this convention. Every
                // M16C register (`r0`-`r3`) is available for this - the
                // widest type (`long long`, 4 words) exactly fills them.
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) code_ << "    pop.w r" << i << "\n";
                }
            }
            // Early exit from the function - genFunction()'s own
            // fall-through epilogue never runs for this path, so it
            // needs this same frame teardown inline (see its own
            // comment for what each instruction does).
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
            // No "else" case for a bare, uninitialized scalar: its
            // own frame word already exists (genFunction()'s own
            // `sub.w N,sp` reserved it) - same as a real, ordinary C
            // automatic variable, its value is simply whatever was
            // already in that stack slot until something assigns it.
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
            if (e.lhs) {  // slot[idx] = rhs - never wide (element width isn't threaded through genIndexAddr's own
                           // store side - see genIndex()'s own comment on this known, documented limitation).
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
            // Deref is untyped (plain ints with '*'/'&' applied, see
            // Type.h's own header comment) - always a native word.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov.w r0,a0\n";
            code_ << "    push.w a0\n";
            genExpr(*e.rhs);
            code_ << "    pop.w a0\n";
            code_ << "    st r0,a0\n";
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
                code_ << "    ld a0,r0\n";
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
            // Arguments are pushed onto the real stack, last-declared
            // one first, so the first argument ends up closest to the
            // return address `jsr.w` is about to push - matching the
            // address every Param already addresses itself by (see
            // M16cCodegen.h's `frameOperand()`: paramIndex 0 is
            // `fb+4`, the lowest positive offset). Each callee lays
            // them into ITS OWN frame itself; nothing here needs to
            // know anything about the callee's parameter count or
            // names beyond `e.args.size()`. A wide argument's own
            // genExpr() already pushes its own `words` words directly
            // (see this file's own wide-value stack convention); a
            // narrow one leaves its value in r0, which this loop
            // pushes itself. The caller cleans the pushed words back
            // off afterward (`add.w N,sp`).
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
                // The callee left its own wide return value in
                // r0(LSW)..r(words-1)(MSW) - see genStmt()'s own
                // Return case - push it back onto the stack as this
                // expression's own result, in the usual LSW-on-top
                // order every wide helper expects.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    mov.w r" << i << ",r0\n    push.w r0\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no way to push
            // an argument into the callee's own frame (see
            // ExprKind::Call's own comment) until the callee is known,
            // which it never is at compile time for this kind.
            //
            // The real M16C `JSRI.W` indirect-call instruction doesn't
            // treat its own operand's address as the callee directly -
            // it fetches the actual entry address from the WORD stored
            // there (see M16cAssembler.cpp's own comment), so the
            // callee's own address (just computed into r0) is spilled
            // to a fresh temp word cell first, then `jsri.w` is pointed
            // at that cell - a real hardware instruction, used exactly
            // the way it calls through an ordinary function-pointer
            // variable in memory.
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
            // Real M16C flag-register interrupt enable/disable -
            // `FSET I`/`FCLR I`, the same real hardware feature as the
            // 8080/6502/8086 backends' own `ei`/`di`/`cli`/`sti` (see
            // I8080Codegen.cpp).
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

// Leaves slot[idxExpr]'s own effective address in `A0` - see
// M16cCodegen.h's own header comment on why that's a dedicated
// address register, never aliasing `R0`/`R1`. `slot->arraySize > 0`
// means the base is a compile-time-known address (a Global's own
// label, loaded via `lea`, or a Local's own `fb`-relative address,
// also loaded via `lea` - see M16cCodegen.h's `frameOperand()`);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first (via
// `operandOf()`, the same for every Slot kind, since a runtime
// pointer VALUE is being offset here, not the slot's own storage
// address).
//
// A Global array's elements sit at increasing addresses as the index
// increases (an ordinary label-plus-offset layout), but a Local
// array's elements sit at DECREASING addresses as the index increases
// - `frameOperand()`'s own address is the array's *first* word,
// closest to `fb`, and later words are further below it (each Local's
// own frameOffset only ever grows downward from `fb` - see
// M16cCodegen.h's own comment) - so this is base-minus-scaled-index
// for a Local, not base-plus, the mirror image of a Global's layout.
// Getting this backwards corrupts the saved `fb`/return address the
// moment anything indexes a Local array past element 0.
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

// A `char` array's own element is one byte (`ldb`, which sign/zero-
// extends into `r0` the same way `loadOperand()`'s own `movb` does -
// see M16cAssembler.cpp's own comment); every other array's own
// element still only ever reads one native word (`ld`) regardless of
// its own real width - a known, documented gap for `long`/`long long`
// array elements specifically (genIndexAddr() already scales the
// index correctly via elemShift(), but a multi-word element's own
// remaining words are never read/written), the same scope limitation
// this project's own pdp11 backend accepts (see Pdp11Codegen.h's own
// header comment) - scalars of every width work fully.
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
    // Every arithmetic/bitwise/comparison operand here already shares
    // one common type (IRBuilder's own usual-arithmetic-conversion
    // pass - see Type.h) via `e.lhs->type`, except shift (only the
    // LHS's own type matters) and &&/|| (width-independent).
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
        code_ << "    " << insn << " r1,r0\n";  // r0 = r0 OP r1 = rhs OP lhs (every one of these is commutative)
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

// Real variable-count hardware shift - `SHL.W` for a left shift or an
// unsigned (logical) right shift, `SHA.W` (arithmetic - sign-
// preserving) for a signed right shift (see M16cCodegen.h's own
// header comment on its own signed-count convention: negative =
// right), which is why `isLeft` only ever decides whether to `NEG.W`
// the count first, not which instruction to use.
void M16cCodegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";  // r1 = shift count
    code_ << "    pop.w r0\n";     // r0 = value to shift
    if (!isLeft) code_ << "    neg.w r1\n";
    code_ << "    " << ((isLeft || isUnsigned) ? "shl.w" : "sha.w") << " r1,r0\n";
}

// Real hardware multiply (`MUL.W`) - see M16cCodegen.h's own header
// comment on which half of its 32-bit product (`R0`, the low word) is
// this project's own 16-bit truncated answer; the low word of a
// multiply's own full product is identical whether the operands are
// signed or unsigned, so this one routine already serves both.
void M16cCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    pop.w r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    mul.w r1,r0\n";   // r2:r0 = r0 * r1 = rhs * lhs (high word:low word)
}

// Real hardware divide (`DIVU.W`) only ever divides unsigned
// magnitudes - a signed division/modulo instead records each
// operand's own sign as a 0/1 flag, negates it to that same
// non-negative magnitude, divides, and restores the quotient's sign
// (negative iff the operands' signs disagreed) and the remainder's
// sign (always the dividend's own, matching C's truncating-toward-
// zero convention) - the same "widen/fix up around a narrower real
// hardware op" shape every signed-division backend here uses, just
// fixing up the *sign* instead of the *width* (there's no dedicated
// sign-extending widen instruction on this target to begin with - see
// genConvert()'s own comment).
void M16cCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push.w r0\n";
    genExpr(rhs);
    code_ << "    mov.w r0,r1\n";  // r1 = divisor
    code_ << "    pop.w r0\n";     // r0 = dividend
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
        code_ << "    pop.w r3\n";  // r3 = dividend-negative flag (the divisor's own sign doesn't affect the
                                     // remainder's own sign at all)
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

void M16cCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitCompare(lhs, rhs, "jeq");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "jltu" : "jlt");
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "jleu" : "jle");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "jgtu" : "jgt");
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "jgeu" : "jge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.W src,dst` sets the real signed condition codes for `dst - src`
// directly in hardware (matching every other two-operand mnemonic's
// own `dst = dst OP src` convention - see M16cCodegen.h's own header
// comment), so `r0`(rhs)/`r1`(lhs) is exactly `lhs - rhs`, with no
// further sign/overflow fixup needed, the same as the pdp11/68000/
// 8086 backends' own comparisons.
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

// After genExpr(e) just left `e`'s own value - r0, if narrow, or
// `wordCountOf(e.type)` words on the stack if wide (see
// M16cCodegen.h's own header comment on this convention) - this
// leaves r0 holding a plain 0/nonzero truth value, with any wide
// stack space already released; a bare `cmp.w 0,r0` is never wrong to
// run again afterward (idempotent), so every call site here still
// does its own, matching the narrow case's existing shape exactly.
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

// A `char` load sign-extends for free (real M16C hardware: `MOV.B`
// into a register destination always sign-extends to 16 bits, same as
// the pdp11 backend's own `MOVB` - see Pdp11Codegen.cpp's own
// `loadOperand()`), so an unsigned `char` has to mask the high byte
// back off afterward - the one extra instruction a signed load
// doesn't need.
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

// Word `wordIndex` (0 = least significant) of `slot`'s own multi-word
// storage - word 0 sits at `slot`'s own ordinary (scalar) address,
// each later word 2 bytes further from `fb`/the label (see IR.h's own
// comment on how a type wider than one word occupies consecutive
// storage units).
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

// Pushes `slot`'s own `words`-word value, least-significant word on
// top (pushed last) - see M16cCodegen.h's own header comment on this
// convention, shared by every wide (`long`/`long long`) helper below.
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

// Pops `words` words off the stack (least-significant on top) into
// `slot`'s own storage.
void M16cCodegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop.w r0\n";
        code_ << "    mov.w r0," << wideWordOperand(slot, i) << "\n";
    }
}

// `long`/`long long` add/sub - word 0 is a plain `add.w`/`sub.w`
// (setting the real carry/borrow flag for the next word), each later
// word folds that flag in via `adc.w`/`sbb.w` (real M16C add-with-
// carry/subtract-with-borrow - see M16cAssembler.cpp's own comment).
// Both operands are ordinary `[sp+N]` memory operands directly (no
// register needed in between - this dialect's own two-operand
// instructions accept any addressing mode on either side) - they
// already sit on the stack after evaluating lhs then rhs (lhs pushed
// first, so it's the deeper block; rhs is on top). Writing every
// result word over lhs's own slot, then discarding exactly rhs's now-
// stale block with one `add.w`, leaves lhs's block - now holding the
// result - as the new top, matching every wide helper's shared
// "result replaces both operands" convention.
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

// Two's-complement negation in place: `not.w` every word first, then
// `add.w 1` into the least significant word and `adc.w 0` (fold the
// resulting carry in, add nothing else) up through the rest - the
// same chained-carry shape as genWideAdd().
void M16cCodegen::genWideNegate(int words) {
    for (int i = 0; i < words; i++) code_ << "    not.w [sp+" << (2 * i) << "]\n";
    code_ << "    add.w 1,[sp+0]\n";
    for (int i = 1; i < words; i++) code_ << "    adc.w 0,[sp+" << (2 * i) << "]\n";
}

// Every bitwise operator here has a real, direct M16C instruction
// (`and.w`/`or.w`/`xor.w`, each already working against any operand
// kind, memory included - unlike this project's own pdp11 backend,
// which has no real hardware AND and has to synthesize one via
// De Morgan's law - see Pdp11Codegen.cpp's own comment), so this is
// simply that one instruction applied word by word.
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

// Compares two `words`-word values most-significant word first (only
// that one comparison needs the signed/unsigned distinction - see
// M16cCodegen.h's own header comment on `isUnsigned`); once a word
// differs, that comparison alone decides the whole result, same as
// every other backend's own multi-word compare. `CMP.W src,dst` sets
// the real flags for `dst - src` (this file's own established
// convention), so `cmp.w rhsWord,lhsWord` is directly `lhs - rhs`,
// with no operand-order inversion needed.
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
            // Only the most significant word (i == words-1) uses the
            // signed jump when the overall type is signed - every
            // lower word, once reached, is comparing a pure
            // magnitude (the higher words already matched).
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
    code_ << "    add.w " << (4 * words) << ",sp\n";  // release both operand blocks - the result is a plain int
                                                        // in r0
}

// A `long`/`long long` shifted by a runtime count - `SHL.W`/`SHA.W`
// only ever shift one native word, not `words` words at once, so this
// loops over every word, one bit at a time, chaining the bit that
// crosses a word boundary through the real carry flag via `ROL.W`/
// `ROR.W` (real M16C single-bit rotate-through-carry - see
// M16cAssembler.cpp's own comment).
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
            // Arithmetic (sign-preserving) right shift: seed the
            // carry from the top word's own current sign bit instead
            // of clearing it, so the first `ror.w` below rotates that
            // same sign bit back into the vacated top bit - the
            // bit-serial equivalent of the narrow case's own `SHA.W`.
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

// `long`/`long long` multiply/divide need a genuine bit-serial
// software routine (the real `MUL.W`/`DIVU.W` only ever widen one
// native word to two, not `words` words to `2*words`) - deliberately
// not implemented here: this backend has no simulator in this
// project's own test harness to verify a hand-written multi-word
// multiply/divide algorithm against at all (the same situation this
// project's own pdp11 backend is in - see Pdp11Codegen.cpp's own
// genWideMul() comment) - failing loudly here is safer than shipping
// an unverifiable, possibly wrong one. `+`/`-`/comparisons/shifts/
// bitwise ops/negation on `long`/`long long` are all implemented and
// don't depend on this.
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

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the real work here is crossing the native-word/`long`-or-
// `long long` boundary (pushing `words` more words of sign/zero
// extension, or popping down to just the low word) and re-narrowing a
// `char` result (mask or sign-extend its own top byte back off).
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

// Leaves `r1` holding `0xFFFF` if `r0` (already loaded) is negative,
// else `0` - the "extra high word" of a sign-extending widen, used
// wherever more than one such word is needed - no dedicated widen-
// with-sign instruction exists on this target (unlike the pdp11
// backend's own `SXT` - see Pdp11Codegen.cpp's own `genConvert()`), so
// this branches on the sign once and reuses that one result.
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
