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

void Pdp11Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 32-/64-bit data directive in this dialect - a `long`/`long
    // long` array's own literal elements are emitted as 2/4 "dw"s
    // (least significant word first) per element instead; a symbol
    // element always means a plain native-word address (the
    // dispatch-table idiom), so it still gets exactly one "dw".
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

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> Pdp11Codegen::perFileAsm() const { return perFileAsm_; }

// Prologue: `push r5` saves the caller's own frame pointer, then
// `mov sp,r5` makes `r5` this call's own frame pointer - pointing at
// that just-pushed word, with the return address `jsr` already
// pushed sitting right above it, and the caller's own pushed
// arguments above that (see Pdp11Codegen.h's own `addr()` comment).
// `sub N,sp` then reserves this function's own Locals, below `r5`.
// Epilogue (here and every `Return` - see genStmt()) undoes exactly
// that: `mov r5,sp` drops the Locals, `pop r5` restores the caller's
// own frame pointer (and leaves `sp` pointing at the return address),
// `rts` pops that address into `pc`.
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
                // A wide value sits on the stack (LSW on top - see
                // Pdp11Codegen.h's own header comment) after
                // genExpr() - pop it into r0(LSW)..r(words-1)(MSW)
                // first, since the epilogue's own `mov r5,sp` is
                // about to discard this call's whole frame (and
                // anything still sitting above it on the stack)
                // before `rts` ever runs - see genExpr()'s own Call
                // case for the matching read-it-back-out-of-registers
                // half of this convention.
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
            if (e.lhs) {  // slot[idx] = rhs (never wide - see Pdp11Codegen.h's own "Scope restriction"-style note:
                           // element width isn't threaded through genIndexAddr here, same gap i386 doesn't have -
                           // flagged as a known limitation).
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    push r2\n";
                genExpr(*e.rhs);
                code_ << "    pop r2\n";
                code_ << "    stind r0,r2\n";
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
            // Deref is untyped (plain ints with '*'/'&' applied, see
            // Type.h's own header comment) - always a native word.
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
                if (isWide(e.rhs->type)) {
                    genWideNegate(wordCountOf(e.rhs->type));
                } else {
                    code_ << "    neg r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r0,r2\n";
                code_ << "    ldind r2,r0\n";
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
            // Real stack-based argument passing - push right-to-left
            // so param 0 ends up closest to the return address (see
            // Pdp11Codegen.h's own `addr()` comment), caller cleans
            // up with `add N,sp` after the call returns. A wide
            // argument's own genExpr() already pushes its own `words`
            // words directly (see this file's own wide-value stack
            // convention); a narrow one leaves its value in r0, which
            // this loop pushes itself.
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
                // The callee left its own wide return value in
                // r0(LSW)..r(words-1)(MSW) - see genStmt()'s own
                // Return case - push it back onto the stack as this
                // expression's own result, in the usual LSW-on-top
                // order every wide helper expects.
                int words = wordCountOf(e.type);
                for (int i = words - 1; i >= 0; i--) {
                    code_ << "    mov r" << i << ",r0\n    push r0\n";
                }
            }
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
            // pointer. There's likewise no callee signature to learn
            // a real return type from here; the result stays plain
            // int, same as every other backend's own CallPtr.
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
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// See Pdp11Codegen.h's own header comment: `R2` holds the computed
// address, via the real register-deferred `(Rn)` mode - no zero-
// page-style placement restriction the way the 6502 backend's own
// `ptr` has. `slot->arraySize > 0` means `slot` is an array, so its
// base is a compile-time-known address: a Global's own label
// (loaded via `lea`), or a Local's own frame-relative address
// (computed from `r5` - see below); otherwise `slot` is a plain
// pointer variable (Global, Local, or Param), so the base instead
// comes from *reading* that variable's own stored value first
// (operandOf()).
//
// A Local array's element `i` lives at frameOffset+i (see IR.h's own
// Slot::frameOffset comment), so its address is `r5 -
// 2*(frameOffset+i)` - addresses *decrease* as the element index
// increases, the mirror image of a Global array's own increasing-
// address layout, since a Local's storage grows down from `r5` (see
// Pdp11Codegen.h's own `addr()` comment). `r0` already holds
// `idx*2`, so that's `r5 - 2*frameOffset - idx*2` - computed here as
// `sub` twice, not folded into one constant, since `idx` is only
// known at runtime.
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

// A `char` load sign-extends for free (real PDP-11 hardware: `movb`
// into a register destination always sign-extends to 16 bits), so an
// unsigned `char` has to mask the high byte back off afterward - the
// one extra instruction a signed load doesn't need.
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

// Pushes `slot`'s own `words`-word value, least-significant word on
// top (pushed last) - see Pdp11Codegen.h's own header comment on this
// convention, shared by every wide (`long`/`long long`) helper below.
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

// Pops `words` words off the stack (least-significant on top) into
// `slot`'s own storage.
void Pdp11Codegen::genWideStore(const ir::Slot *slot, int words) {
    for (int i = 0; i < words; i++) {
        code_ << "    pop r0\n";
        code_ << "    mov r0," << wideWordOperand(slot, i) << "\n";
    }
}

// `long`/`long long` add/sub - the standard PDP-11 multi-word idiom:
// word 0 is a plain ADD/SUB (setting C = carry/borrow for the next
// word); each later word first folds in the previous word's own C via
// ADC/SBC (which only ever adds/subtracts the carry bit itself, into
// one operand - there's no single two-operand "add with carry-in" on
// this hardware the way x86's ADC is), *then* does its own ADD/SUB
// (setting C for the word after it). Both operands can be ordinary
// memory operands directly (no register needed in between) - they
// already sit on the stack after evaluating lhs then rhs (lhs pushed
// first, so it's the deeper block; rhs is on top - see
// Pdp11Codegen.h's own header comment on this convention). Writing
// every result word over lhs's own slot, then discarding exactly
// rhs's now-stale block with one `add`, leaves lhs's block - now
// holding the result - as the new top, matching every wide helper's
// shared "result replaces both operands" convention.
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

// Two's-complement negation in place, in the same chained-carry shape
// as genWideAdd() - COM every word first, then ADD 1 into the least
// significant word and ADC the resulting carry up through the rest.
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
            // No real AND - BIC is "dst &= ~src" (see the narrow `&`
            // case's own comment) - COM the rhs word in place first
            // (its own stack slot is dead afterward anyway, since the
            // whole rhs block is discarded below).
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

// Compares two `words`-word values most-significant word first (only
// that one compare needs the signed/unsigned distinction - see
// Pdp11Codegen.h's own header comment on `isUnsigned`); once a word
// differs, that comparison alone decides the whole result, same as
// every other backend's own multi-word compare. `CMP src,dst` sets
// the real flags for `src - dst` (this file's own established
// convention - see emitCompare()'s comment), so `cmp lhsWord,rhsWord`
// is directly `lhs - rhs` with no operand-order inversion needed.
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
            // Only the most significant word (i == words-1) uses the
            // signed branch when the overall type is signed - every
            // lower word, once reached, is comparing a pure magnitude
            // (the higher words already matched).
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

// A `long`/`long long` shifted by a runtime count - no hardware
// shift here is wider than one word (see Pdp11Codegen.h's own header
// comment on why even the single-word case already loops one bit at
// a time) - so this loops over every word, one bit at a time, folding
// the bit that crosses a word boundary by hand the same way the
// narrower genShift() does, just chained across `words` words instead
// of held in a single register.
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
            // Logical (unsigned) shift fills with a cleared carry; a
            // signed shift's own top word instead keeps sign-
            // extending - CLC/no-CLC right before the top word's own
            // `ror` is the only difference between the two, since
            // `ror` always rotates whatever's currently in the carry
            // into the new top bit.
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

// `long`/`long long` multiply/divide need a genuine bit-serial
// software routine (the real EIS MUL/DIV only ever widen one native
// word to two, not `words` words to `2*words`) - deliberately not
// implemented here: this backend has no simulator in this project's
// own test harness to verify a hand-written multi-word multiply/
// divide algorithm against at all (every other backend that
// implements one either has real hardware wide enough, like SPARC's
// `smul`, or a working simulator to catch a mistake in a software
// one, like i386's own qemu-verified bit-serial divide) - failing
// loudly here is safer than shipping an unverifiable, possibly wrong
// one. `+`/`-`/comparisons/shifts/bitwise ops on `long`/`long long`
// are all implemented and don't depend on this.
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

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the real work here is crossing the native-word/`long`-or-
// `long long` boundary (pushing `words` more words of sign/zero
// extension, or popping down to just the low word) and re-narrowing a
// `char` result (mask or sign-extend its own top byte back off).
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
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // r0 = the native-width value, already correctly sign/zero-extended to 16 bits
        code_ << "    push r0\n";
        for (int i = 1; i < toWords; i++) {
            code_ << "    " << (from.isUnsigned ? "mov 0,r0\n" : "sxt r0\n") << "    push r0\n";
        }
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromWords = wordCountOf(from);
    int toWords = wordCountOf(to);
    genExpr(*e.lhs);
    if (toWords > fromWords) {
        code_ << "    mov [sp+" << (2 * (fromWords - 1)) << "],r0\n    sxt r0\n";
        for (int i = 0; i < toWords - fromWords; i++) code_ << "    push r0\n";
    } else if (toWords < fromWords) {
        code_ << "    add " << (2 * (fromWords - toWords)) << ",sp\n";
    }
}

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
    code_ << "    ldind r2,r0\n";
}

// A Local array is no longer a static `dw`-declared data block (see
// generate()'s own trailer_, still used for Global arrays) - it's
// reserved frame storage (see genFunction()'s own `sub N,sp`), freshly
// allocated on every call, so its own initializer has to be stored
// into it with real instructions each time this declaration runs,
// not baked into the program image once. A literal element loads
// with a plain `mov`; a symbol element (a Slot's or Function's own
// address - see IR.h's own ArrayInitElem comment) needs `lea` instead
// - this dialect's bare-label operand means "read the value stored
// there", not "take its address" (see Pdp11Assembler.h's own header
// comment), so only `lea` gives the address itself.
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

// A Local/Param's own address is only ever a *runtime* value (`r5`
// plus/minus a displacement) - there's no real PDP-11 instruction
// that loads a register-relative effective address directly (`lea`
// here is a synthesized pseudo-op that only ever works from a
// compile-time-absolute label, via PC-immediate addressing - see
// Pdp11Assembler.cpp's own `encodeLea`), so this computes it with
// real arithmetic instead, the same `mov r5,rX` + `add`/`sub` shape
// genIndexAddr() above uses for a Local array's own base.
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

void Pdp11Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h) via `e.lhs->type`, except shift
    // (only the LHS's own type matters) and &&/|| (width-independent).
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
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%");
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

// No variable-count *logical* shift (see Pdp11Codegen.h's own header
// comment on why the EIS `ASH` instruction doesn't fit here) - loop
// one bit at a time instead, counting down in `r1`. `isUnsigned`
// picks between `ROR` (logical, zero-filling) and `ASR` (arithmetic,
// sign-preserving) for `>>`; `<<` never cares about signedness.
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

void Pdp11Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "blo" : "blt");
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "blos" : "ble");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "bhi" : "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "bhis" : "bge");
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

// After genExpr(e) just left `e`'s own value - r0, if narrow, or
// `wordCountOf(e.type)` words on the stack if wide (see
// Pdp11Codegen.h's own header comment on this convention) - this
// leaves r0 holding a plain 0/nonzero truth value, with any wide
// stack space already released; a bare `TST r0` is never wrong to
// run again afterward (idempotent), so every call site here still
// does its own, matching the narrow case's existing shape exactly.
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
