#include "I8086Codegen.h"

#include <stdexcept>

#include "I8086Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}
}  // namespace

std::string I8086Codegen::allocTemp() { return "t" + std::to_string(tempCounter_++); }

// A literal element is its own decimal text; a symbol element is the
// referenced Slot's or Function's own label, written out bare - NASM-
// style "dw" (I8086Assembler.cpp's Line::Dw) already evaluates any
// expr operand, symbol included, the same way "mov ax, fn_f" already
// does for an immediate (see genAddrOf).
std::string I8086Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void I8086Codegen::declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values) {
    std::ostringstream line;
    line << label << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

std::string I8086Codegen::newLabel() { return "L" + std::to_string(labelCounter_++); }

// Placement (`@addr` in source - see IR.h's Slot::hasAddress, and
// ../pdp7/Pdp7Codegen.cpp's own placement comment for the other
// backend's take on the same source syntax). This backend's whole
// output is one flat, position-fixed boot sector (`ORG 0x7C00`, see
// generate() below) - there's no notion of an absolute machine
// address here the way pdp7's `. = addr` has, and NASM's `-f bin`
// output flatly refuses a second `ORG` directive (verified directly:
// "program origin redefined"), so there's no way to jump the
// in-progress output's position backward and later resume where it
// left off the way the pdp7 backend's save/restore trick does.
// `addr` here means something narrower instead: a byte offset from
// this backend's own ORG (0x7C00), forward-only - `times addr-($-$$)
// db 0` (the same NASM idiom already used below for the final
// boot-signature padding) pads up to it, then the placed item is
// emitted, and whatever's next in program order just continues
// immediately after - there's no restore, because NASM's `$` can
// only ever increase. A placed item's own address must not already
// have been passed by the time it's reached in program order
// (NASM reports it plainly: "TIMES value N is negative" if so,
// verified directly) - unlike pdp7, placement here can't reorder
// things, only insert forward padding.
void I8086Codegen::emitPlacement(long address) { code_ << "times (" << address << "-($-$$)) db 0\n"; }

std::string I8086Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff` from a single file -
    // see IRBuilder.h's `allowExternal`) means `prog` may not define
    // `main` at all - an ordinary object file never does. Outside
    // `objectMode`, IRBuilder still guarantees a parameterless `main`
    // exists, so `mainFn` is never null there.

    // --dump-asm wants one file per *input*, not one combined dump -
    // see Codegen.h's perFileAsm() and Pdp7Codegen.cpp's own version
    // of this same tracking for the fuller rationale (same idea
    // here: track (sourceFile, start, end) ranges by write position
    // into code_/by index into trailer_, without ever touching
    // code_/trailer_ themselves, so the single combined asmText this
    // function returns - used for every real --format - is
    // completely unaffected).
    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    // Purely for --dump-asm's own per-file attribution of the handful
    // of things with no single owning Slot/Function - never a
    // correctness concern. See Pdp7Codegen.cpp's own `homeFile`.
    std::string homeFile = mainFn ? mainFn->sourceFile
                            : !prog.functionBodies.empty() ? prog.functionBodies.front().func->sourceFile
                            : !prog.slots.empty() ? prog.slots.front()->sourceFile
                                                   : std::string();

    size_t rangeStart = static_cast<size_t>(code_.tellp());
    code_ << "BITS 16\n";
    // `objectMode` (see Codegen.h) means no object file ever emits
    // the boot-sector wrapper or entry sequence itself, even the one
    // that happens to define `main` - the linker is solely
    // responsible for synthesizing exactly one, once every object is
    // merged and `main`'s linked address is resolvable (see
    // Linker.h). An object file's own base is `ORG 0` - it's the
    // linker's job to place it, not this backend's (see
    // I8086Assembler.h).
    if (objectMode) {
        code_ << "ORG 0\n\n";
    } else {
        code_ << "ORG 0x7C00\n\n";
        code_ << "_start:\n";
        code_ << "    cli\n";
        code_ << "    xor ax, ax\n";
        code_ << "    mov ds, ax\n";
        code_ << "    mov es, ax\n";
        code_ << "    mov ss, ax\n";
        code_ << "    mov sp, 0x7C00\n";  // stack grows down from just below the boot sector
        code_ << "    sti\n";
        code_ << "    call " << label(mainFn) << "\n";
        code_ << "    hlt\n";
        code_ << "_hang:\n";
        code_ << "    jmp _hang\n\n";
        codeRanges.push_back({mainFn->sourceFile, rangeStart, static_cast<size_t>(code_.tellp())});
    }

    for (const auto &fb : prog.functionBodies) {
        size_t fCodeStart = static_cast<size_t>(code_.tellp());
        size_t fTrailerStart = trailer_.size();
        if (fb.func->hasAddress) emitPlacement(fb.func->address);
        genFunction(fb);
        codeRanges.push_back({fb.func->sourceFile, fCodeStart, static_cast<size_t>(code_.tellp())});
        trailerRanges.push_back({fb.func->sourceFile, fTrailerStart, trailer_.size()});
    }

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file at all - see
        // Pdp7Codegen.cpp's own version of this same fix.
        if (slot->isExternal) continue;
        size_t gCodeStart = static_cast<size_t>(code_.tellp());
        size_t gTrailerStart = trailer_.size();
        if (slot->hasAddress) emitPlacement(slot->address);
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            std::ostringstream line;
            line << lbl << ": dw ";
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                if (i > 0) line << ", ";
                line << arrayElemText(slot->arrayInit[i]);
            }
            line << "  ; " << slot->sourceName;
            trailer_.push_back(line.str());
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            trailer_.push_back(lbl + ": dw " + std::to_string(v) + "  ; " + slot->sourceName);
        }
        codeRanges.push_back({slot->sourceFile, gCodeStart, static_cast<size_t>(code_.tellp())});
        trailerRanges.push_back({slot->sourceFile, gTrailerStart, trailer_.size()});
    }

    std::string codeStr = code_.str();
    perFileAsm_.clear();
    for (const auto &[file, s, e] : codeRanges) perFileAsm_[file] += codeStr.substr(s, e - s);
    for (const auto &[file, s, e] : trailerRanges) {
        for (size_t i = s; i < e; i++) perFileAsm_[file] += trailer_[i] + "\n";
    }
    std::ostringstream out;
    out << codeStr;
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> I8086Codegen::perFileAsm() const { return perFileAsm_; }

// Every activation gets its own real stack frame: `push bp` saves
// the caller's frame pointer, `mov bp,sp` makes `bp` the fixed point
// every Local/Param in this call addresses relative to (see
// I8086Codegen.h's `addr()`), and `sub sp,N` reserves this
// function's own locals below it. The epilogue's `mov sp,bp` tears
// the whole frame down in one instruction regardless of how many
// locals it had, then `pop bp` restores the caller's own frame -
// this is what lets two overlapping activations of the same
// function (direct or indirect recursion) never share storage: each
// `call` gets a fresh `bp`/frame of its own.
void I8086Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ":\n";
    code_ << "    push bp\n    mov bp, sp\n";
    if (fn.func->frameSize > 0) code_ << "    sub sp, " << (fn.func->frameSize * 2) << "\n";

    genStmt(*fn.body);

    code_ << "    mov sp, bp\n    pop bp\n";
    code_ << "    ret\n\n";
}

void I8086Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp ax, 0\n    je " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    cmp ax, 0\n    je " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit from the function - genFunction()'s own
            // fall-through epilogue never runs for this path, so it
            // needs this same frame teardown inline (see its own
            // comment for what each instruction does).
            code_ << "    mov sp, bp\n    pop bp\n";
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    mov " << addr(s.declSlot) << ", ax\n";
            }
            // No "else" case for a bare, uninitialized scalar: its
            // own frame word already exists (genFunction()'s own
            // `sub sp,N` reserved it) - same as a real, ordinary C
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
            // Verbatim passthrough - see grammar/C.g4's `statement`
            // comment on `asm { ... }`. Not reformatted/indented:
            // whatever's between the braces goes straight into the
            // generated assembly text exactly as written.
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
    }
}

void I8086Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    mov ax, " << e.intValue << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            // IRBuilder already rejects a bare array Var outside of
            // `&` (which bypasses this path), so e.slot is always a
            // scalar here.
            code_ << "    mov ax, " << operandOf(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                std::string memOperand = computeElementAddr(e.slot, *e.lhs);
                code_ << "    push bx\n";
                genExpr(*e.rhs);
                code_ << "    pop bx\n";
                code_ << "    mov " << memOperand << ", ax\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    mov " << operandOf(e.slot) << ", ax\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ax = pointer value
            code_ << "    mov bx, ax\n    push bx\n";
            genExpr(*e.rhs);  // ax = value to store
            code_ << "    pop bx\n";
            code_ << "    mov [bx], ax\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg ax\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ax = pointer value
                code_ << "    mov bx, ax\n    mov ax, [bx]\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not ax\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed onto the real stack, last-declared
            // one first, so the first argument ends up closest to the
            // return address `call` is about to push - matching the
            // address every Param already addresses itself by (see
            // I8086Codegen.h's `addr()`: paramIndex 0 is `bp+4`, the
            // lowest positive offset). Each callee lays them into ITS
            // OWN frame itself; nothing here needs to know anything
            // about the callee's parameter count or names beyond
            // `e.args.size()` (true of a real separate-compilation
            // external function too, same as before this convention
            // existed - see IR.h's `isExternal`). The caller cleans
            // the pushed words back off afterward (`add sp,N`) -
            // cdecl's own convention, chosen here only because it
            // needs no cooperation from the callee at all.
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    push ax\n";
            }
            code_ << "    call " << label(e.func) << "\n";
            if (!e.args.empty()) code_ << "    add sp, " << (e.args.size() * 2) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "call ax" (I8086Assembler.cpp's new FF /2 encoding) is a
            // real, direct near-indirect call through a register -
            // unlike pdp7's jms, no temp cell is needed at all.
            genExpr(*e.lhs);  // ax = the callee's own entry address
            code_ << "    call ax\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 8086 interrupt enable/disable - unlike pdp7's
            // ion/iof (see Pdp7Codegen.cpp), these are ordinary
            // opcodes NASM already knows by name, no literal value
            // needed. Note the boot-sector entry sequence already
            // runs with interrupts off from `cli` until its own
            // `sti` (see generate()) - calling _iof()/_ion() from
            // user code just nests inside that, same as real 8086
            // code doing the same thing would.
            code_ << "    " << (e.op == "ion" ? "sti" : "cli") << "\n";
            return;
        }
    }
}

// slot[idxExpr]'s effective-address operand text, e.g. "[bx]", with
// bx already loaded with the final effective address (idx*2 - words
// are 2 bytes on this byte-addressed target, unlike pdp7's word-
// addressed memory - plus the array's or pointer's own base).
//
// slot is only the base address itself when it's a declared array
// (slot->arraySize > 0) - slot[idx] on a plain variable means that
// variable holds a pointer, and the base to index from is its
// runtime VALUE, not its own address. A Global array's base folds
// into a compile-time immediate the same way its own address always
// does (see genAddrOf below); a Local array's base needs `lea`
// instead, since it's only known at runtime relative to `bp` (see
// I8086Codegen.h's `addr()`) - every activation gets its own. A
// pointer variable (Global or not) is indexed through whatever
// address its own VALUE holds either way, via operandOf().
std::string I8086Codegen::computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);
    code_ << "    shl ax, 1\n";
    code_ << "    mov bx, ax\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    add bx, " << label(slot) << "\n";
        } else {
            // A Local array's words sit at decreasing addresses as
            // its own element index increases (addr(slot) is the
            // array's *first* word, closest to `bp`; later words are
            // further below it - see addr()'s own comment) - the
            // mirror image of a Global array's increasing-address
            // layout above, so this is base-minus-scaled-index, not
            // base-plus.
            code_ << "    lea ax, " << addr(slot) << "\n    sub ax, bx\n    mov bx, ax\n";
        }
    } else {
        code_ << "    add bx, " << operandOf(slot) << "\n";
    }
    return "[bx]";
}

void I8086Codegen::genIndex(const ir::Expr &e) {
    std::string memOperand = computeElementAddr(e.slot, *e.lhs);
    code_ << "    mov ax, " << memOperand << "\n";
}

// &operand: a plain variable/array (&slot -> its own address), an
// array element (&slot[idx] -> that element's address), or a bare
// function name (&fn -> fn's own entry label - the same address a
// direct `call label(fn)` already targets; see label()'s own
// comment) - nothing else in this subset has an address worth taking.
// A Global's own address is a compile-time immediate (its label,
// used bare); a Local/Param's is only known at runtime relative to
// `bp` (see addr()), so it needs `lea` instead of a plain `mov`.
void I8086Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            code_ << "    mov ax, " << label(operand.slot) << "\n";
        } else {
            code_ << "    lea ax, " << addr(operand.slot) << "\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genExpr(*operand.lhs);
        code_ << "    shl ax, 1\n";
        if (operand.slot->arraySize > 0) {
            if (operand.slot->kind == ir::SlotKind::Global) {
                code_ << "    add ax, " << label(operand.slot) << "\n";
            } else {
                // See computeElementAddr()'s own identical comment -
                // a Local array's element address is base-minus-
                // scaled-index.
                code_ << "    mov bx, ax\n    lea ax, " << addr(operand.slot) << "\n    sub ax, bx\n";
            }
        } else {
            code_ << "    add ax, " << operandOf(operand.slot) << "\n";
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    mov ax, " << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a
// Local's storage is this activation's own fresh frame region, not
// pre-set to anything; arrayInit is always fully populated up to
// arraySize (zero-padded even with no source-level initializer - see
// BuildAst.cpp's padArrayInit), so this always leaves the whole array
// in a known state, same as a Global array's static trailer always
// did. An element's own address is slot->frameOffset+i words below
// `bp`, the same arithmetic addr() uses for the array's own first
// word, just advanced by i.
void I8086Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        code_ << "    mov ax, " << arrayElemText(slot->arrayInit[i]) << "\n";
        code_ << "    mov [bp-" << (2 * (slot->frameOffset + static_cast<int>(i))) << "], ax\n";
    }
}

void I8086Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push ax\n";
        genExpr(*e.rhs);
        code_ << "    pop bx\n";
        if (op == "+") {
            code_ << "    add ax, bx\n";
        } else {
            code_ << "    sub bx, ax\n    mov ax, bx\n";
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
        // Real hardware and/or/xor - unlike pdp7 (see
        // Pdp7Codegen.cpp's own comment), this target has all three
        // directly, so no De Morgan's trick is needed for `|`.
        genExpr(*e.lhs);
        code_ << "    push ax\n";
        genExpr(*e.rhs);
        code_ << "    pop bx\n";
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    " << insn << " ax, bx\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        // Real hardware variable-count shift (shl/shr take the count
        // from cl) - unlike pdp7, which has no such instruction and
        // has to loop (see Pdp7Codegen.cpp's own comment). `>>` is
        // `shr`, a *logical* shift (same convention as pdp7's, for
        // the same reason: this language has no unsigned type to
        // make the distinction with in the first place).
        genExpr(*e.lhs);
        code_ << "    push ax\n";
        genExpr(*e.rhs);
        code_ << "    mov cx, ax\n";
        code_ << "    pop ax\n";
        code_ << "    " << (op == "<<" ? "shl" : "shr") << " ax, cl\n";
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

void I8086Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "je");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, "jl");
    if (op == "<=") return emitCompare(lhs, rhs, "jle");
    if (op == ">") return emitCompare(lhs, rhs, "jg");
    if (op == ">=") return emitCompare(lhs, rhs, "jge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// cmp bx, ax compares lhs-rhs (bx=lhs, ax=rhs after the pop below),
// so `jumpIfTrue` is a signed conditional jump matching C's `int`
// comparison semantics directly - no sza/sma-style workaround needed.
void I8086Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue) {
    genExpr(lhs);
    code_ << "    push ax\n";
    genExpr(rhs);
    code_ << "    pop bx\n";
    code_ << "    cmp bx, ax\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpIfTrue << " " << Ltrue << "\n";
    code_ << "    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Ltrue << "\n";
    code_ << "    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}

// Real hardware multiply/divide (IMUL/IDIV) - no software routine
// needed the way pdp7's lack of a verified idiv required; only the
// low word of the product is kept, matching C int wraparound.
void I8086Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push ax\n";
    genExpr(rhs);
    code_ << "    pop bx\n";
    code_ << "    imul bx\n";  // dx:ax = ax*bx; keep ax
}

void I8086Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push ax\n";
    genExpr(rhs);
    code_ << "    mov bx, ax\n";  // bx = divisor
    code_ << "    pop ax\n";      // ax = dividend
    code_ << "    cwd\n";         // sign-extend ax into dx:ax
    code_ << "    idiv bx\n";     // ax = quotient, dx = remainder
    if (wantRemainder) code_ << "    mov ax, dx\n";
}

std::string I8086Codegen::buildAssembleCommand(const std::string &assemblerPath, const std::string & /*format*/,
                                                 const std::string &srcPath, const std::string &outPath) const {
    return "\"" + assemblerPath + "\" -f bin -o \"" + outPath + "\" \"" + srcPath + "\"";
}

// No external assembler for this target anymore either (see
// ../pdp7/Pdp7Codegen.cpp's own version of this override) - a real
// from-scratch x86 instruction encoder, not just text manipulation;
// see I8086Assembler.h for what it does and doesn't support.
//
// The boot-sector padding/signature (`times (510-($-$$)) db 0\ndw
// 0xAA55`) is added here, only for `--format=raw`, not in generate()
// itself - `generate()`'s own text is shared by every format, and
// that padding only means anything for a direct, bootable, exactly-
// one-sector image. Appending it unconditionally used to also apply
// it to `--format=coff` - harmless for a small program, but a real
// bug for a larger one: `times`'s count goes negative once the real
// code+data is already past 510 bytes, and this backend's own
// assembler doesn't reject that (confirmed directly: a plain NASM
// build of the same text errors with "TIMES value N is negative",
// but this one silently wraps the negative count to a huge unsigned
// one instead, producing a multi-gigabyte "object file"). An object
// file has no 510-byte limit at all - it's not meant to be booted
// directly - so it should never see this padding in the first place.
std::optional<std::string> I8086Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") {
        std::string bootable = asmText + "\ntimes (510-($-$$)) db 0\ndw 0xAA55\n";
        return assembleI8086(bootable, I8086OutputFormat::Bin);
    }
    // `--format=coff` always means real separate compilation (see
    // IRBuilder.h's `allowExternal`) - an external/undefined symbol
    // is always allowed here, whole-program single-file compile or
    // not, deferred as a relocation for a future `--link` step.
    if (format == "coff") return assembleI8086(asmText, I8086OutputFormat::Coff, /*allowExternal=*/true);
    // `--format=elf`, like `--format=raw`, is a *complete* self-
    // contained program - an external/undefined symbol is a hard
    // error (allowExternal defaults to false), same as "asm"/"raw",
    // not a relocation to defer to a future `--link` step the way
    // "coff" allows (see fileformat/elf/ElfFormat.h's own header comment for why
    // this is a peer of `bin`'s "whole program" role, not of `coff`'s
    // "relocatable object" one). Unlike "raw", there's no 510-byte
    // sector limit to pad for - a real ELF file has no such
    // constraint - so `asmText` is assembled exactly as generate()
    // wrote it, with no boot-sector padding appended.
    if (format == "elf") return assembleI8086(asmText, I8086OutputFormat::Elf);
    return std::nullopt;
}

}  // namespace pcc
