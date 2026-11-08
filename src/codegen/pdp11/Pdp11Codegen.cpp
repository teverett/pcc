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

// Where the whole program loads and starts executing; the test harness sets the simulated
// CPU's PC here directly. Must stay even - the PDP-11 can only fetch an instruction word
// from an even address. Chosen above the real reset/trap vector range (0-0376).
constexpr uint32_t kBaseAddress = 0x1000;

// Where SP is set before calling main(), comfortably above the loaded program and its data.
constexpr uint32_t kInitialSp = 0xE000;
}  // namespace

// A literal element's decimal text, truncated to 16 bits; a symbol element is the
// referenced Slot's or Function's label, written out bare.
std::string Pdp11Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int>(static_cast<uint16_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void Pdp11Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 32-/64-bit data directive: a `long`/`long long` literal element is emitted as 2/4
    // "dw"s (least significant word first); a symbol element is always a plain native-word
    // address, so it still gets exactly one "dw".
    const char *directive = w == 1 ? "db" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        int words = w <= 2 ? 1 : w / 2;
        if (words > 1 && !e.isSymbol) {
            auto uv = static_cast<unsigned long>(e.value);
            for (int i = 0; i < words; i++) {
                if (!first) line << ", ";
                line << static_cast<uint16_t>((uv >> (16 * i)) & 0xFFFF);
                first = false;
            }
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
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
    // Real separate compilation means `prog` may not define `main` at all; `objectMode`
    // skips the entry sequence below, since Pdp11LinkerFactory.cpp synthesizes it once
    // instead, after linking. An object file's own base is `org 0`; placing it is the
    // linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    mov " << kInitialSp << ",sp\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The result sits in R0 once execution reaches `halt`. `halt` should stop the
        // simulated CPU; the `br halt` after it is a safety net in case a simulator's HALT
        // just traps to a console prompt and execution resumes at the next word.
        code_ << "halt:\n    halt\n    br halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Globals are laid out back-to-back in declaration order. PDP-11 hardware traps on an
    // odd-address word access, so a `dw`-or-wider global landing after an odd number of
    // accumulated `db` bytes needs one byte of padding first.
    size_t globalByteOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file: it must never get a storage word
        // here, only be referenced by name, letting the assembler defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        int elemW = widthOfType(slot->type);
        size_t totalBytes = slot->arraySize > 0 ? static_cast<size_t>(slot->arraySize) * elemW
                                                  : static_cast<size_t>(elemW);
        if (elemW >= 2 && (globalByteOffset % 2) != 0) {
            trailer_.push_back("db 0  ; alignment padding");
            globalByteOffset += 1;
        }
        globalByteOffset += totalBytes;
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

std::map<std::string, std::string> Pdp11Codegen::perFileAsm() const { return perFileAsm_; }

// Prologue: `push r5` saves the caller's frame pointer, `mov sp,r5` makes `r5` this call's
// frame pointer (pointing at that saved word, with the return address above it and the
// caller's pushed arguments above that - see addr()), then `sub N,sp` reserves Locals below
// `r5`. Epilogue (here and every `Return`) undoes it: `mov r5,sp` drops the Locals, `pop r5`
// restores the caller's frame pointer, `rts` pops the return address into `pc`.
void Pdp11Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    push r5\n";
    code_ << "    mov sp,r5\n";
    if (fn.func->frameSize > 0) code_ << "    sub " << fn.func->frameSize << ",sp\n";
    genStmt(*fn.body);
    code_ << "    mov r5,sp\n";
    code_ << "    pop r5\n";
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
            testTruthy(*s.cond);
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testTruthy(*s.cond);
            safeBranch("beq", Lend, Lbody);
            code_ << Lbody << ":\n";
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
                // A wide value sits on the stack (LSW on top) after genExpr(); pop it into
                // r0(LSW)..r(words-1)(MSW) first, since the epilogue's `mov r5,sp` is about
                // to discard this call's whole frame before `rts` runs.
                if (isWide(s.initOrReturnOrExpr->type)) {
                    int words = wordCountOf(s.initOrReturnOrExpr->type);
                    for (int i = 0; i < words; i++) code_ << "    pop r" << i << "\n";
                }
            }
            code_ << "    mov r5,sp\n";
            code_ << "    pop r5\n";
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
                    storeOperand(addr(s.declSlot), s.declSlot->type);
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
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            code_ << "    mov " << static_cast<int>(static_cast<uint16_t>(e.intValue)) << ",r0\n";
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
            if (e.lhs) {  // slot[idx] = rhs (never wide).
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    push r2\n";
                genExpr(*e.rhs);
                code_ << "    pop r2\n";
                if (widthOfType(e.slot->type) == 1) {
                    code_ << "    stindb r0,r2\n";
                } else {
                    code_ << "    stind r0,r2\n";
                }
                return;
            }
            if (isWide(e.slot->type)) {
                genExpr(*e.rhs);  // pushes `words` words
                int words = wordCountOf(e.slot->type);
                for (int i = 0; i < words; i++) code_ << "    mov [sp+" << (2 * i) << "],r0\n    mov r0,"
                                                        << wideWordOperand(e.slot, i) << "\n";
                code_ << "    add " << (2 * words) << ",sp\n";
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores through at e.type's width/signedness. The pointer value (always one
            // word) has to ride the data stack, not a register, across evaluating rhs, since
            // rhs may clobber any register as scratch (e.g. genDivMod() uses r2); storeIndirect()
            // recovers it, underneath rhs's own pushed value if that's wide.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r0,r2\n";
            code_ << "    push r2\n";
            genExpr(*e.rhs);  // r0, or pushes `words` words (LSW on top) if wide
            storeIndirect(e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--) - never wide.
                genIndexAddr(e.slot, *e.lhs);  // r2 = address
                genCompoundThroughR2(e);
                return;
            }
            // Plain scalar target - operandOf(slot) is a stable memory operand, so no
            // address needs to be held across rhs's own evaluation.
            std::string mem = operandOf(e.slot);
            genExpr(*e.rhs);  // r0 = rhs
            code_ << "    mov r0,r1\n";
            loadOperand(mem, e.type);  // r0 = old
            if (e.yieldOld) code_ << "    push r0\n";
            genCompoundOp(e.op, e.type);
            narrowR0To(e.type);
            storeOperand(mem, e.type);
            if (e.yieldOld) code_ << "    pop r0\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r0,r2\n";  // r2 = address
            genCompoundThroughR2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lthen = newLabel();
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            safeBranch("beq", Lelse, Lthen);
            code_ << Lthen << ":\n";
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
                    code_ << "    neg r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness. Unlike DerefAssign, no competing stack
                // use happens between computing the address and dereferencing it, so r2 can
                // safely hold the address directly.
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r0,r2\n";
                loadIndirect("r2", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    com r0\n";
                if (isWide(e.rhs->type)) {
                    for (int i = 1; i < wordCountOf(e.rhs->type); i++) code_ << "    com [sp+" << (2 * i) << "]\n";
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Push arguments right-to-left so param 0 ends up closest to the return address;
            // caller cleans up with `add N,sp` after the call returns. A wide argument's
            // genExpr() already pushes its own `words` words; a narrow one leaves its value
            // in r0, which this loop pushes itself.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    totalWords += wordCountOf(e.args[i]->type);
                } else {
                    code_ << "    push r0\n";
                    totalWords += 1;
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            if (totalWords > 0) code_ << "    add " << (2 * totalWords) << ",sp\n";
            if (isWide(e.type)) {
                // The callee left its wide return value in r0(LSW)..r(words-1)(MSW); push it
                // back as this expression's result in LSW-on-top order. Pushes each register
                // directly rather than relaying through r0, since r0 is itself one of the
                // values being pushed.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    push r" << i << "\n";
                }
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any argument on this kind,
            // since the callee is unknown at compile time. "jsrind r2" is the real `JSR
            // PC,@Rn` register-deferred call through whatever address r2 holds. No callee
            // signature is known, so the result stays plain int.
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
            // No trap vector is set up (see generate()), so there's nothing for `ion`/`iof`
            // to meaningfully enable or disable; treated as a no-op.
            (void)e;
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

std::string Pdp11Codegen::wideWordOperand(const ir::Slot *slot, int wordIndex) const {
    int off = 2 * wordIndex;
    if (slot->kind == ir::SlotKind::Global) {
        return off == 0 ? label(slot) : (label(slot) + "+" + std::to_string(off));
    }
    if (slot->kind == ir::SlotKind::Param) {
        return "[r5+" + std::to_string(slot->frameOffset + off) + "]";
    }
    return "[r5-" + std::to_string(slot->frameOffset + off) + "]";
}

// `movb` into a register destination always sign-extends to 16 bits, so an unsigned `char`
// has to mask the high byte back off afterward.
void Pdp11Codegen::loadOperand(const std::string &mem, Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    movb " << mem << ",r0\n";
        if (type.isUnsigned) code_ << "    bic 0xFF00,r0\n";
    } else {
        code_ << "    mov " << mem << ",r0\n";
    }
}

void Pdp11Codegen::storeOperand(const std::string &mem, Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    movb r0," << mem << "\n";
    } else {
        code_ << "    mov r0," << mem << "\n";
    }
}

void Pdp11Codegen::loadIndirect(const std::string &addrReg, Type type) {
    if (!isWide(type)) {
        loadOperand("[" + addrReg + "+0]", type);
        return;
    }
    int words = wordCountOf(type);
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    mov [" << addrReg << "+" << (2 * i) << "],r0\n";
        code_ << "    push r0\n";
    }
}

void Pdp11Codegen::storeIndirect(Type type) {
    if (!isWide(type)) {
        code_ << "    pop r2\n";
        storeOperand("[r2+0]", type);
        return;
    }
    int words = wordCountOf(type);
    // r0/r1/r3/r4 are the only four registers free (r5 is the frame pointer, r2 holds the
    // address) - exactly enough to stash a `long long`'s 4 words while the address stays
    // buried underneath them on the stack.
    static const char *const tempRegs[] = {"r0", "r1", "r3", "r4"};
    for (int i = 0; i < words; i++) code_ << "    pop " << tempRegs[i] << "\n";
    code_ << "    pop r2\n";
    for (int i = 0; i < words; i++) code_ << "    mov " << tempRegs[i] << ",[r2+" << (2 * i) << "]\n";
}

// Pushes `slot`'s `words`-word value, least-significant word on top (pushed last).
void Pdp11Codegen::genWideLoad(const ir::Slot *slot, int words) {
    for (int i = words - 1; i >= 0; i--) {
        code_ << "    mov " << wideWordOperand(slot, i) << ",r0\n";
        code_ << "    push r0\n";
    }
}

void Pdp11Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    for (int i = words - 1; i >= 0; i--) {
        int shiftedWord = static_cast<int>((uv >> (16 * i)) & 0xFFFF);
        code_ << "    mov " << shiftedWord << ",r0\n";
        code_ << "    push r0\n";
    }
}

// Pops `words` words off the stack (least-significant on top) into `slot`'s storage.
void Pdp11Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop r0\n";
        code_ << "    mov r0," << wideWordOperand(slot, i) << "\n";
    }
}

// Standard PDP-11 multi-word idiom: word 0 is a plain ADD/SUB (setting C for the next word);
// each later word first folds in the previous C via ADC/SBC (which only adds/subtracts the
// carry bit into one operand - there's no two-operand "add with carry-in" here), then does
// its own ADD/SUB. Result is written over lhs's stack block; rhs's now-stale block is
// discarded with one `add`, leaving lhs's block as the new top.
void Pdp11Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    genExpr(lhs);
    genExpr(rhs);
    for (int i = 0; i < words; i++) {
        std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
        std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
        if (i > 0) code_ << "    " << (isAdd ? "adc" : "sbc") << " " << lhsOperand << "\n";
        code_ << "    " << (isAdd ? "add" : "sub") << " " << rhsOperand << "," << lhsOperand << "\n";
    }
    code_ << "    add " << (2 * words) << ",sp\n";
}

// Two's-complement negation in place: COM every word first, then ADD 1 into the least
// significant word and ADC the carry up through the rest.
void Pdp11Codegen::genWideNegate(int words) {
    for (int i = 0; i < words; i++) code_ << "    com [sp+" << (2 * i) << "]\n";
    code_ << "    add 1,[sp+0]\n";
    for (int i = 1; i < words; i++) code_ << "    adc [sp+" << (2 * i) << "]\n";
}

void Pdp11Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    genExpr(lhs);
    genExpr(rhs);
    for (int i = 0; i < words; i++) {
        std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
        std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
        if (op == "&") {
            // No real AND - BIC is "dst &= ~src", so COM the rhs word first (its stack slot
            // is dead afterward anyway).
            code_ << "    com " << rhsOperand << "\n";
            code_ << "    bic " << rhsOperand << "," << lhsOperand << "\n";
        } else if (op == "|") {
            code_ << "    bis " << rhsOperand << "," << lhsOperand << "\n";
        } else {
            code_ << "    mov " << rhsOperand << ",r0\n";
            code_ << "    xor r0," << lhsOperand << "\n";
        }
    }
    code_ << "    add " << (2 * words) << ",sp\n";
}

// Compares two `words`-word values most-significant word first (only that compare needs the
// signed/unsigned distinction); once a word differs, that comparison alone decides the
// result. `CMP src,dst` sets flags for `src - dst`, so `cmp lhsWord,rhsWord` is directly
// `lhs - rhs`.
void Pdp11Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                    int words) {
    genExpr(lhs);
    genExpr(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        for (int i = words - 1; i >= 0; i--) {
            std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
            std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
            code_ << "    cmp " << lhsOperand << "," << rhsOperand << "\n";
            std::string Lcontinue = newLabel();
            safeBranch("beq", Lcontinue, (op == "==" ? Lfalse : Ltrue));
            code_ << Lcontinue << ":\n";
        }
        code_ << "    jmp " << (op == "==" ? Ltrue : Lfalse) << "\n";
    } else {
        std::string signedBranch = op == "<" ? "blt" : op == "<=" ? "ble" : op == ">" ? "bgt" : "bge";
        std::string unsignedBranch = op == "<" ? "blo" : op == "<=" ? "blos" : op == ">" ? "bhi" : "bhis";
        for (int i = words - 1; i >= 0; i--) {
            std::string lhsOperand = "[sp+" + std::to_string(2 * words + 2 * i) + "]";
            std::string rhsOperand = "[sp+" + std::to_string(2 * i) + "]";
            code_ << "    cmp " << lhsOperand << "," << rhsOperand << "\n";
            std::string Lcontinue = newLabel(), Ldiffer = newLabel();
            safeBranch("beq", Lcontinue, Ldiffer);
            code_ << Ldiffer << ":\n";
            // Only the most significant word uses the signed branch; lower words, once
            // reached, compare a pure magnitude (higher words already matched).
            bool useSigned = (i == words - 1) && !isUnsigned;
            safeBranch(useSigned ? signedBranch : unsignedBranch, Ltrue, Lfalse);
            code_ << Lcontinue << ":\n";
        }
        code_ << "    jmp " << Lfalse << "\n";  // every word equal
    }
    code_ << Lfalse << ":\n    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
    code_ << "    add " << (4 * words) << ",sp\n";  // release both operand blocks - the result is a plain int in r0
}

// A `long`/`long long` shifted by a runtime count: no hardware shift is wider than one word,
// so this loops one bit at a time, folding the bit crossing a word boundary by hand, chained
// across `words` words.
void Pdp11Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    genExpr(lhs);     // pushes `words` words
    code_ << "    push r1\n";  // reserve a slot to stash the shift count
    genExpr(rhs);              // r0 = shift count (already promoted to a plain int)
    code_ << "    mov r0,[sp]\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    tst [sp]\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    clc\n";
        for (int i = 0; i < words; i++) {
            std::string w = "[sp+" + std::to_string(2 + 2 * i) + "]";
            code_ << "    rol " << w << "\n";
        }
    } else {
        code_ << "    clc\n";
        for (int i = words - 1; i >= 0; i--) {
            std::string w = "[sp+" + std::to_string(2 + 2 * i) + "]";
            // A signed shift's top word sign-extends instead of filling with a cleared
            // carry; CLC/no-CLC before that word's `ror` is the only difference.
            if (i == words - 1 && !isUnsigned) {
                code_ << "    asr " << w << "\n";
            } else {
                code_ << "    ror " << w << "\n";
            }
        }
    }
    code_ << "    sub 1,[sp]\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    code_ << "    pop r1\n";  // discard the shift-count slot; the `words`-word result stays on top
}

// `long`/`long long` multiply/divide need a bit-serial software routine (EIS MUL/DIV only
// widen one native word to two, not `words` to `2*words`); deliberately not implemented
// since this backend has no simulator to verify such an algorithm against.
void Pdp11Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int) {
    err(lhs.line, "multiplying two 'long'/'long long' values isn't supported on this target yet - "
                   "see Pdp11Codegen.cpp's own genWideMul() comment");
    (void)rhs;
}

void Pdp11Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool, bool, int) {
    err(lhs.line, "dividing two 'long'/'long long' values isn't supported on this target yet - "
                   "see Pdp11Codegen.cpp's own genWideDivMod() comment");
    (void)rhs;
}

// The real work here is crossing the native-word/`long`-or-`long long` boundary (pushing
// more words of sign/zero extension, or popping to just the low word) and re-narrowing a
// `char` result.
void Pdp11Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // r0
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    bic 0xFF00,r0\n";
            } else {
                code_ << "    movb r0,r0\n";  // narrow to the low byte, then (register dest) sign-extend back up
            }
        }
        return;
    }
    if (fromWide && !toWide) {
        int words = wordCountOf(from);
        genExpr(*e.lhs);               // pushes `words` words
        code_ << "    mov [sp],r0\n";  // the low word is this type's own truncated value
        code_ << "    add " << (2 * words) << ",sp\n";
        if (widthOfType(to) == 1) {
            code_ << "    " << (to.isUnsigned ? "bic 0xFF00,r0\n" : "movb r0,r0\n");
        }
        return;
    }
    if (!fromWide && toWide) {
        // The least significant word must end up on top of the stack, so the real value
        // (saved in r1 before SXT overwrites r0) is pushed LAST, after the extension words.
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // r0 = the native-width value, already correctly sign/zero-extended to 16 bits
        code_ << "    mov r0,r1\n";
        code_ << "    " << (from.isUnsigned ? "mov 0,r0\n" : "sxt r0\n");
        for (int i = 1; i < toWords; i++) code_ << "    push r0\n";
        code_ << "    push r1\n";
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromWords = wordCountOf(from);
    int toWords = wordCountOf(to);
    genExpr(*e.lhs);
    if (toWords > fromWords) {
        // Extension words must land underneath the existing fromWords words, so those are
        // popped into registers first and pushed back on top of the new extension words,
        // restoring word 0 to the top.
        code_ << "    mov [sp+" << (2 * (fromWords - 1)) << "],r0\n    sxt r0\n";
        static const char *const tempRegs[] = {"r1", "r3", "r4"};
        for (int i = 0; i < fromWords; i++) code_ << "    pop " << tempRegs[i] << "\n";
        for (int i = 0; i < toWords - fromWords; i++) code_ << "    push r0\n";
        for (int i = fromWords - 1; i >= 0; i--) code_ << "    push " << tempRegs[i] << "\n";
    } else if (toWords < fromWords) {
        code_ << "    add " << (2 * (fromWords - toWords)) << ",sp\n";
    }
}

// No-op unless `to` is a `char`.
void Pdp11Codegen::narrowR0To(Type to) {
    if (widthOfType(to) != 1) return;
    if (to.isUnsigned) {
        code_ << "    bic 0xFF00,r0\n";
    } else {
        code_ << "    movb r0,r0\n";  // narrow to the low byte, then (register dest) sign-extend back up
    }
}

// `R2` ends up holding the computed address via register-deferred `(Rn)` mode. An array's
// base is a compile-time-known address (a Global label, or a Local's frame-relative
// address); a plain pointer variable's base instead comes from reading its stored value.
// A Local array's address decreases as the element index increases (storage grows down from
// `r5`): element `i` is at `r5 - 2*(frameOffset+i)`, computed via two `sub`s since `idx` is
// only known at runtime.
void Pdp11Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    asl r0\n";  // r0 *= widthOf(slot->type) (a power of two - see elemShift())
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lea " << label(slot) << ",r2\n";
            code_ << "    add r0,r2\n";
        } else {
            code_ << "    mov r5,r2\n";
            code_ << "    sub " << slot->frameOffset << ",r2\n";
            code_ << "    sub r0,r2\n";
        }
        return;
    }
    code_ << "    mov " << operandOf(slot) << ",r2\n";
    code_ << "    add r0,r2\n";
}

void Pdp11Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    if (widthOfType(e.slot->type) == 1) {
        // MOVB into a register destination always sign-extends to 16 bits; an unsigned char
        // needs its high byte masked back off afterward.
        code_ << "    ldindb r2,r0\n";
        if (e.slot->type.isUnsigned) code_ << "    bic 0xFF00,r0\n";
    } else {
        code_ << "    ldind r2,r0\n";
    }
}

// A Local array is reserved frame storage, freshly allocated on every call, so its
// initializer must be stored with real instructions each time this runs. A symbol element
// (a Slot's or Function's address) needs `lea`, since a bare-label operand otherwise means
// "read the value stored there", not "take its address".
void Pdp11Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        if (elem.isSymbol) {
            code_ << "    lea " << (elem.slot ? label(elem.slot) : label(elem.func)) << ",r0\n";
        } else {
            code_ << "    mov " << static_cast<int>(static_cast<uint16_t>(elem.value)) << ",r0\n";
        }
        code_ << "    mov r0,[r5-" << (slot->frameOffset + 2 * static_cast<int>(i)) << "]\n";
    }
}

// A Local/Param's address is a runtime value (`r5` plus/minus a displacement); `lea` only
// works from a compile-time-absolute label (PC-immediate addressing), so this computes the
// address with real arithmetic instead.
void Pdp11Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    lea " << label(operand.slot) << ",r0\n";
            return;
        }
        code_ << "    mov r5,r0\n";
        if (operand.slot->kind == ir::SlotKind::Param) {
            code_ << "    add " << operand.slot->frameOffset << ",r0\n";
        } else {
            code_ << "    sub " << operand.slot->frameOffset << ",r0\n";
        }
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

// Entry convention: r0 = old value, r1 = rhs value, both at `type`'s width/signedness. `-`
// and `&` aren't commutative, so each is reordered to compute old-op-rhs with `old` already
// in r0 (`sub r1,r0` for `-`; `com r1`/`bic r1,r0` directly for `&`, since the destination is
// already r0). `/`/`%` reshuffles into genDivMod()'s r1=dividend/r2=divisor layout.
void Pdp11Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add r1,r0\n"; return; }
    if (op == "-") { code_ << "    sub r1,r0\n"; return; }
    if (op == "*") { code_ << "    mul r1,r0\n"; code_ << "    mov r1,r0\n"; return; }
    if (op == "&") { code_ << "    com r1\n"; code_ << "    bic r1,r0\n"; return; }
    if (op == "|") { code_ << "    bis r1,r0\n"; return; }
    if (op == "^") { code_ << "    xor r1,r0\n"; return; }
    if (op == "<<" || op == ">>") {
        std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n    tst r1\n";
        safeBranch("beq", Ldone, Lbody);
        code_ << Lbody << ":\n";
        if (op == "<<") {
            code_ << "    asl r0\n";
        } else if (isUnsigned) {
            code_ << "    clc\n    ror r0\n";
        } else {
            code_ << "    asr r0\n";
        }
        code_ << "    sub 1,r1\n";
        code_ << "    jmp " << Lloop << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    // "/" or "%"
    code_ << "    mov r1,r2\n";  // r2 = divisor
    code_ << "    mov r0,r1\n";  // r1 = dividend
    if (isUnsigned) {
        emitUnsignedDiv();
    } else {
        code_ << "    tst r1\n    sxt r0\n";  // r0 = sign extension of the dividend (high word)
        code_ << "    div r2,r0\n";           // r0 = quotient, r1 = remainder
    }
    if (op == "%") code_ << "    mov r1,r0\n";
}

// r2 must already hold the resolved address on entry. The address rides the stack (not a
// register) across evaluating rhs, since that evaluation may use any register.
void Pdp11Codegen::genCompoundThroughR2(const ir::Expr &e) {
    code_ << "    push r2\n";  // stash address
    genExpr(*e.rhs);            // r0 = rhs
    code_ << "    push r0\n";    // stash rhs
    code_ << "    pop r1\n";      // r1 = rhs
    code_ << "    pop r2\n";       // r2 = address
    loadOperand("[r2+0]", e.type);  // r0 = old
    code_ << "    push r2\n";        // re-stash address for the store below
    if (e.yieldOld) code_ << "    push r0\n";  // stash old too
    genCompoundOp(e.op, e.type);                // r0 = new (raw)
    narrowR0To(e.type);
    if (e.yieldOld) {
        code_ << "    pop r1\n";  // r1 = old (pre-op), stashed above, storeOperand() never touches r1
        code_ << "    pop r2\n";   // r2 = address
        storeOperand("[r2+0]", e.type);  // stores the new value (still in r0)
        code_ << "    mov r1,r0\n";       // r0 = old (final result)
    } else {
        code_ << "    pop r2\n";  // r2 = address
        storeOperand("[r2+0]", e.type);  // r0 already = new
    }
}

void Pdp11Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every operand here shares one common type via `e.lhs->type` (IRBuilder's usual-
    // arithmetic-conversion pass), except shift (only LHS matters) and &&/|| (width-independent).
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
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "&") {
            // No real AND on the PDP-11 - only BIC (dst &= ~src). De Morgan: COM rhs first
            // (giving ~rhs), then BIC ~rhs,lhs leaves lhs & rhs.
            code_ << "    com r0\n";
            code_ << "    bic r0,r1\n";
            code_ << "    mov r1,r0\n";
        } else if (op == "|") {
            code_ << "    bis r1,r0\n";  // r0 |= r1
        } else {
            code_ << "    xor r1,r0\n";  // r0 ^= r1
        }
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

// No variable-count logical shift (EIS `ASH` shifts arithmetically), so loop one bit at a
// time, counting down in `r1`. `isUnsigned` picks `ROR` (zero-filling) vs `ASR`
// (sign-preserving) for `>>`; `<<` never cares about signedness.
void Pdp11Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
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
    } else if (isUnsigned) {
        code_ << "    clc\n    ror r0\n";
    } else {
        code_ << "    asr r0\n";  // arithmetic: sign bit stays, same value rotated back into bit 15
    }
    code_ << "    sub 1,r1\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// EIS `MUL`: the low word of its 32-bit product (`R1`) is the 16-bit truncated answer.
void Pdp11Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    mul r1,r0\n";  // r0:r1 = r0 * r1 = rhs * lhs (high word:low word)
    code_ << "    mov r1,r0\n";
}

// EIS `DIV` needs a 32-bit dividend in `R0:R1`, so the 16-bit dividend is first
// sign-extended into that pair with `SXT` (`TST` first, to set the N flag `SXT` reads).
void Pdp11Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    mov r0,r2\n";  // r2 = divisor
    code_ << "    pop r1\n";     // r1 = dividend (low word)
    if (isUnsigned) {
        emitUnsignedDiv();
    } else {
        code_ << "    tst r1\n    sxt r0\n";  // r0 = sign extension of the dividend (high word)
        code_ << "    div r2,r0\n";           // r0 = quotient, r1 = remainder
    }
    if (wantRemainder) code_ << "    mov r1,r0\n";
}

// EIS `DIV` is signed: the divisor is a signed 16-bit value and the
// quotient must fit in 15 bits. With the dividend zero-extended into
// R0:R1 it is exact for divisors 2..0x7FFF. A divisor of 1 would give
// a quotient up to 0xFFFF, so it is handled directly; a divisor with
// its top bit set divides an unsigned 16-bit dividend at most once.
void Pdp11Codegen::emitUnsignedDiv() {
    std::string Lbig = newLabel(), Lsmall = newLabel(), Lone = newLabel(), Lnorm = newLabel(),
                Lge = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << "    tst r2\n";  // clears V, so blt branches exactly when N is set
    safeBranch("blt", Lbig, Lsmall);
    code_ << Lsmall << ":\n";
    code_ << "    mov r2,r0\n    sub 1,r0\n";
    safeBranch("beq", Lone, Lnorm);
    code_ << Lnorm << ":\n";
    code_ << "    mov 0,r0\n    div r2,r0\n    jmp " << Ldone << "\n";
    code_ << Lone << ":\n";
    code_ << "    mov r1,r0\n    mov 0,r1\n    jmp " << Ldone << "\n";
    code_ << Lbig << ":\n";
    code_ << "    cmp r1,r2\n";  // flags = dividend - divisor
    safeBranch("bhis", Lge, Llt);
    code_ << Llt << ":\n";
    code_ << "    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Lge << ":\n";
    code_ << "    sub r2,r1\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "blo" : "blt");
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "blos" : "ble");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "bhi" : "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "bhis" : "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP src,dst` sets condition codes for `src - dst`, so `cmp r1,r0` (lhs, rhs) is directly
// `lhs - rhs`, with `branchIfTrue` applying with no further sign/overflow fixup.
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

// Leaves r0 holding a plain 0/nonzero truth value, with any wide stack space released.
void Pdp11Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    tst r0\n";
        return;
    }
    int words = wordCountOf(e.type);
    code_ << "    mov [sp],r0\n";
    for (int i = 1; i < words; i++) code_ << "    bis [sp+" << (2 * i) << "],r0\n";
    code_ << "    add " << (2 * words) << ",sp\n";
    code_ << "    tst r0\n";
}

void Pdp11Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    testTruthy(lhs);
    std::string Lfalse = newLabel(), Lcheck2 = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    safeBranch("beq", Lfalse, Lcheck2);
    code_ << Lcheck2 << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    mov 1,r0\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov 0,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    testTruthy(lhs);
    std::string Ltrue = newLabel(), Lcheck2 = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("bne", Ltrue, Lcheck2);
    code_ << Lcheck2 << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Lfalse << ":\n    mov 0,r0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov 1,r0\n";
    code_ << Ldone << ":\n";
}

void Pdp11Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
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
    // `coff`/`elf`/`macho`/`aout`/`objraw`: build one format-neutral ObjectFile, then pick
    // that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assemblePdp11Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
