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

void I8086Codegen::declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    std::ostringstream line;
    line << label << ": " << (widthOfType(elemType) == 1 ? "db " : "dw ");
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

    // Fixed scratch for a wide (`long`/`long long`) Return - see
    // genStmt()'s own Return case for why this needs to exist at all
    // (a wide result's own address might point into the very frame
    // the epilogue is about to tear down) - 4 words covers even a
    // `long long`, unconditionally, whether or not this program ever
    // returns one.
    trailer_.push_back("__wret: dw 0, 0, 0, 0");
    trailer_.push_back("__wargsp: dw 0");

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file at all - see
        // Pdp7Codegen.cpp's own version of this same fix.
        if (slot->isExternal) continue;
        size_t gCodeStart = static_cast<size_t>(code_.tellp());
        size_t gTrailerStart = trailer_.size();
        if (slot->hasAddress) emitPlacement(slot->address);
        const std::string lbl = label(slot.get());
        int w = widthOfType(slot->type);
        if (slot->arraySize > 0) {
            std::ostringstream line;
            line << lbl << ": " << (w == 1 ? "db " : "dw ");
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                if (i > 0) line << ", ";
                // A wide (`long`/`long long`) array element's own
                // literal is split into its N low-to-high words here
                // too (a symbol element always means a plain native-
                // int-width address instead - the dispatch-table
                // idiom, see AST.h's ArrayInitElem - so it only ever
                // needs the one word every other backend's own
                // "always dd two words" edge case already accepts).
                const ir::ArrayInitElem &elem = slot->arrayInit[i];
                if (w > 2 && !elem.isSymbol) {
                    auto v = static_cast<uint64_t>(elem.value);
                    for (int wi = 0; wi < w / 2; wi++) {
                        if (wi > 0) line << ", ";
                        line << static_cast<uint16_t>(v >> (16 * wi));
                    }
                } else {
                    line << arrayElemText(elem);
                }
            }
            line << "  ; " << slot->sourceName;
            trailer_.push_back(line.str());
        } else if (w > 2) {
            auto v = static_cast<uint64_t>(slot->hasInit ? slot->initValue : 0);
            std::ostringstream line;
            line << lbl << ": dw ";
            for (int wi = 0; wi < w / 2; wi++) {
                if (wi > 0) line << ", ";
                line << static_cast<uint16_t>(v >> (16 * wi));
            }
            line << "  ; " << slot->sourceName;
            trailer_.push_back(line.str());
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            trailer_.push_back(lbl + ": " + (w == 1 ? "db " : "dw ") + std::to_string(v) + "  ; " + slot->sourceName);
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
    if (fn.func->frameSize > 0) code_ << "    sub sp, " << fn.func->frameSize << "\n";

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
            if (isWide(s.cond->type)) genWideTruthify(s.cond->type);
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
            // `continue` jumps here if this loop has its own post-
            // statement (a desugared for-loop's - see IR.h's
            // Stmt::postStmt), so post still runs before the condition
            // is retested; otherwise it's just Lstart itself, since
            // there's nothing else to run first.
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            if (isWide(s.cond->type)) genWideTruthify(s.cond->type);
            code_ << "    cmp ax, 0\n    je " << Lend << "\n";
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
                if (isWide(s.initOrReturnOrExpr->type)) {
                    // A wide result's own address (the convention
                    // every other wide expression uses) might point
                    // into THIS call's own frame (a wide local, or a
                    // literal just pushed on this call's own stack) -
                    // about to be torn down by the epilogue right
                    // below. Copying it into this one fixed, program-
                    // wide scratch block first (see generate()'s own
                    // "__wret" declaration) keeps it alive across the
                    // return - safe because a caller always consumes
                    // it (e.g. via an Assign's own copyWideBlock())
                    // immediately after the call returns, before any
                    // other call that could itself return a wide
                    // value and overwrite this same scratch.
                    int n = widthOfType(s.initOrReturnOrExpr->type) / 2;
                    code_ << "    mov si, ax\n";  // si = source address value
                    for (int i = 0; i < n; i++) {
                        // Only bx/bp can address memory in this
                        // dialect - bx is repointed at each base in
                        // turn via a plain `mov` (see genWideAdd()'s
                        // own identical comment).
                        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
                        code_ << "    mov bx, __wret\n    mov [bx+" << (2 * i) << "], ax\n";
                    }
                    code_ << "    mov ax, __wret\n";
                }
            }
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
                if (isWide(s.declSlot->type)) {
                    int n = widthOfType(s.declSlot->type) / 2;
                    genExpr(*s.initOrReturnOrExpr);  // ax = source address
                    code_ << "    mov di, ax\n";
                    genWideSlotAddr(s.declSlot);  // ax = destination address
                    code_ << "    mov si, ax\n";
                    for (int i = 0; i < n; i++) {
                        code_ << "    mov bx, di\n    mov ax, [bx+" << (2 * i) << "]\n";
                        code_ << "    mov bx, si\n    mov [bx+" << (2 * i) << "], ax\n";
                    }
                } else {
                    genExpr(*s.initOrReturnOrExpr);
                    storeOperand(addr(s.declSlot), s.declSlot->type);
                }
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
            if (isWide(e.type)) {
                // Real 8086 has no PUSH imm16 (that's an 80186+
                // addition) - load each word into AX first.
                int n = widthOfType(e.type) / 2;
                auto v = static_cast<uint64_t>(e.intValue);
                for (int i = n - 1; i >= 0; i--) {
                    code_ << "    mov ax, " << static_cast<uint16_t>(v >> (16 * i)) << "\n";
                    code_ << "    push ax\n";
                }
                code_ << "    mov ax, sp\n";
                return;
            }
            code_ << "    mov ax, " << e.intValue << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            // IRBuilder already rejects a bare array Var outside of
            // `&` (which bypasses this path), so e.slot is always a
            // scalar here.
            if (isWide(e.slot->type)) {
                genWideSlotAddr(e.slot);
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
            if (isWide(e.slot->type)) {
                int n = widthOfType(e.slot->type) / 2;
                // rhs first, THEN the destination address - never the
                // other way around: rhs may itself be a wide binary
                // op (genWideAdd/Mul/Compare), and those use si/di as
                // their OWN scratch internally, which would clobber a
                // destination address stashed in si/di before rhs
                // runs. computeElementAddr/genWideSlotAddr never touch
                // si/di, so computing the destination after is safe.
                genExpr(*e.rhs);       // ax = source address
                code_ << "    mov bx, ax\n";
                copyWideBlock(n, n, /*isUnsignedExt=*/false);  // ax = fresh copy's address
                code_ << "    mov di, ax\n";  // di = fresh copy's own address
                // slot[idx] = rhs (a wide element) needs the element's
                // own address, not the whole slot's - computeElementAddr
                // leaves it in bx (see its own comment); a plain
                // (unindexed) wide slot uses genWideSlotAddr instead.
                if (e.lhs) {
                    computeElementAddr(e.slot, *e.lhs);
                    code_ << "    mov si, bx\n";
                } else {
                    genWideSlotAddr(e.slot);
                    code_ << "    mov si, ax\n";
                }
                // Copy the fresh stack copy (di) into the real
                // destination (si) - only bx/bp can address memory in
                // this dialect (see genWideAdd()'s own comment), so
                // bx is repointed at each in turn - then free the
                // stack copy's space back afterward.
                for (int i = 0; i < n; i++) {
                    code_ << "    mov bx, di\n    mov ax, [bx+" << (2 * i) << "]\n";
                    code_ << "    mov bx, si\n    mov [bx+" << (2 * i) << "], ax\n";
                }
                code_ << "    add sp, " << (2 * n) << "\n";
                code_ << "    mov ax, si\n";
                return;
            }
            if (e.lhs) {  // slot[idx] = rhs
                std::string memOperand = computeElementAddr(e.slot, *e.lhs);
                code_ << "    push bx\n";
                genExpr(*e.rhs);
                code_ << "    pop bx\n";
                storeOperand(memOperand, e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores through at e.type's own width - the pointee type
            // of a real typed pointer, or a plain native int word for
            // the still-supported untyped-int-as-address style - see
            // IRBuilder.cpp's own DerefAssign comment. A wide pointee
            // reuses the same "fresh protected copy, then copy into
            // the real destination" shape as a wide Assign's own
            // unindexed branch above, except the destination address
            // comes directly from evaluating e.lhs (the pointer,
            // always narrow - see Unary "*"'s own identical comment
            // below) instead of genWideSlotAddr()/computeElementAddr().
            if (isWide(e.type)) {
                int n = widthOfType(e.type) / 2;
                genExpr(*e.rhs);  // ax = source address
                code_ << "    mov bx, ax\n";
                copyWideBlock(n, n, /*isUnsignedExt=*/false);  // ax = fresh protected copy's address
                code_ << "    mov di, ax\n";
                genExpr(*e.lhs);  // ax = pointer value = destination address
                code_ << "    mov si, ax\n";
                for (int i = 0; i < n; i++) {
                    code_ << "    mov bx, di\n    mov ax, [bx+" << (2 * i) << "]\n";
                    code_ << "    mov bx, si\n    mov [bx+" << (2 * i) << "], ax\n";
                }
                code_ << "    add sp, " << (2 * n) << "\n";
                code_ << "    mov ax, si\n";
                return;
            }
            genExpr(*e.lhs);  // ax = pointer value
            code_ << "    mov bx, ax\n    push bx\n";
            genExpr(*e.rhs);  // ax = value to store
            code_ << "    pop bx\n";
            storeOperand("[bx]", e.type);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    code_ << "    mov bx, ax\n";
                    genWideNegate(widthOfType(e.rhs->type) / 2);
                } else {
                    code_ << "    neg ax\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's own width - see DerefAssign's
                // identical comment above. A wide pointee's own
                // evaluation leaves ax = the pointee storage's address
                // (the convention every wide expression here uses -
                // see genWideSlotAddr()'s own comment) - which is
                // already exactly the pointer value itself, so there
                // is nothing further to load.
                genExpr(*e.rhs);  // ax = pointer value
                if (isWide(e.type)) return;
                code_ << "    mov bx, ax\n";
                loadOperand("[bx]", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    code_ << "    mov bx, ax\n";
                    for (int i = 0; i < widthOfType(e.rhs->type) / 2; i++) {
                        code_ << "    mov ax, [bx+" << (2 * i) << "]\n    not ax\n";
                        code_ << "    mov [bx+" << (2 * i) << "], ax\n";
                    }
                    code_ << "    mov ax, bx\n";
                } else {
                    code_ << "    not ax\n";
                }
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
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    // A wide argument's own evaluation (a literal, a
                    // convert, an add/mul/... result) generally leaves
                    // its own temp block sitting permanently on the
                    // stack below wherever sp was before it ran (see
                    // genWideAdd()'s own comment on why nothing here
                    // ever reclaims its own scratch - that's normally
                    // left to the enclosing statement/epilogue). That
                    // would be harmless for a single top-level wide
                    // statement, but here it is NOT: this loop pushes
                    // more than one argument's own real value within
                    // the SAME statement, so an earlier (farther, i.e.
                    // already-pushed) argument's position would be
                    // thrown off by however much a LATER argument's
                    // own temp leaks underneath it. So: remember sp
                    // from right before evaluating this argument, then
                    // write the real copy directly at (that saved sp -
                    // n words) and reset sp there - reclaiming whatever
                    // the argument's own evaluation left behind, same
                    // as if it had never grown the stack at all.
                    int n = widthOfType(e.args[i]->type) / 2;
                    code_ << "    mov [__wargsp], sp\n";
                    genExpr(*e.args[i]);  // ax = this argument's own address
                    code_ << "    mov si, ax\n";
                    code_ << "    mov ax, [__wargsp]\n    sub ax, " << (2 * n) << "\n    mov di, ax\n";
                    for (int w = 0; w < n; w++) {
                        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * w) << "]\n";
                        code_ << "    mov bx, di\n    mov [bx+" << (2 * w) << "], ax\n";
                    }
                    code_ << "    mov sp, di\n";
                    totalBytes += 2 * n;
                } else {
                    genExpr(*e.args[i]);
                    code_ << "    push ax\n";
                    totalBytes += 2;
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    add sp, " << totalBytes << "\n";
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
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
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
    // This dialect's own `shl` only takes a literal count of exactly
    // 1 (or `cl`, for a runtime count - see I8086Assembler.cpp's own
    // shl/shr/sar dispatch) - a wider compile-time-constant shift is
    // just that many `shl ax,1`s in a row.
    for (int i = 0; i < elemShift(slot->type); i++) code_ << "    shl ax, 1\n";
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
    std::string memOperand = computeElementAddr(e.slot, *e.lhs);  // bx = element address
    if (isWide(e.slot->type)) {
        // Every wide expression leaves its own ADDRESS in ax (see
        // genWideSlotAddr()'s own comment) - bx already holds it.
        code_ << "    mov ax, bx\n";
        return;
    }
    loadOperand(memOperand, e.slot->type);
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
        for (int i = 0; i < elemShift(operand.slot->type); i++) code_ << "    shl ax, 1\n";
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
void I8086Codegen::narrowAxTo(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        // No movzx/movsx on real 8086 (386+ only), and this dialect's
        // assembler has no "ah" register operand at all - zero-extend
        // by masking out every bit above AL instead of touching AH
        // directly; sign-extend with the real CBW instruction (sign-
        // extends AL into AH:AX).
        if (to.isUnsigned) {
            code_ << "    and ax, 0x00FF\n";
        } else {
            code_ << "    cbw\n";
        }
    }
    // 2 bytes (short/int on this target): ax already holds the full
    // value, nothing to do. A wide type never reaches here - see
    // genConvert()'s own isWide branches.
}

void I8086Codegen::loadOperand(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        if (type.isUnsigned) {
            code_ << "    xor ax, ax\n";  // clear ax first - a byte load into al alone never touches ah
            code_ << "    movb ax, " << mem << "\n";
        } else {
            code_ << "    movb ax, " << mem << "\n";
            code_ << "    cbw\n";
        }
    } else {
        code_ << "    mov ax, " << mem << "\n";
    }
}

void I8086Codegen::storeOperand(const std::string &mem, Type type) {
    if (widthOfType(type) == 1) {
        code_ << "    movb " << mem << ", ax\n";
    } else {
        code_ << "    mov " << mem << ", ax\n";
    }
}

// A Local or Param/Global's own N-word (N=widthOfType(type)/2)
// storage, word 0 (least significant) through word N-1 (most),
// always in LOW-TO-HIGH ADDRESS order from the address this leaves in
// AX - a Global's or Param's own storage already grows that way (see
// I8086Codegen.h's own `addr()` comment: a Param's frameOffset grows
// up from `bp`, same direction as a Global's label), so AX is simply
// that slot's own base address. A LOCAL's own storage instead grows
// DOWN from `bp` as its declared size increases (the mirror image,
// same convention every Local array here already uses - see
// computeElementAddr()'s own comment) - its own word 0 (least
// significant, "lowest address" in THIS method's own convention) is
// therefore the FARTHEST one from `bp`, not the closest, so AX here
// is `bp - frameOffset - (N-1)*2`, not `bp - frameOffset`.
void I8086Codegen::genWideSlotAddr(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    mov ax, " << label(slot) << "\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    lea ax, " << addr(slot) << "\n";
        return;
    }
    int n = widthOfType(slot->type) / 2;
    code_ << "    lea ax, " << addr(slot, 2 * (n - 1)) << "\n";
}

// Builds a fresh `toN`-word low-to-high block on the real stack from
// the `fromN`-word low-to-high block whose address is already in BX -
// see I8086Codegen.h's own comment on this method and why it covers
// both a same-width copy (Assign/LocalDecl-init) and a widening/
// narrowing Convert. Pushes the extension words (if widening) first
// (ending up at the highest addresses), then the real/truncated words
// high-to-low (so the least significant ends up on top of the stack,
// the lowest address) - leaves the new block's own address in AX.
void I8086Codegen::copyWideBlock(int fromN, int toN, bool isUnsignedExt) {
    int copyN = fromN < toN ? fromN : toN;
    if (toN > fromN) {
        code_ << "    mov cx, [bx+" << (2 * (fromN - 1)) << "]\n";  // cx = old MSW
        if (isUnsignedExt) {
            code_ << "    xor dx, dx\n";
        } else {
            code_ << "    mov ax, cx\n    cwd\n";  // dx = sign-extension of the old MSW (0 or 0xFFFF)
        }
        for (int i = 0; i < toN - fromN; i++) code_ << "    push dx\n";
    }
    for (int i = copyN - 1; i >= 0; i--) {
        code_ << "    mov cx, [bx+" << (2 * i) << "]\n";
        code_ << "    push cx\n";
    }
    code_ << "    mov ax, sp\n";
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is: crossing the native-int/wide
// boundary (building or discarding extra words), or re-truncating+
// re-extending AX for a narrower target (narrowAxTo()).
void I8086Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // ax, or an address if fromWide
    if (fromWide && toWide) {
        int fromN = widthOfType(from) / 2, toN = widthOfType(to) / 2;
        if (fromN == toN) return;  // same width, different signedness only - bit pattern unchanged
        code_ << "    mov bx, ax\n";
        copyWideBlock(fromN, toN, from.isUnsigned);
    } else if (fromWide && !toWide) {
        code_ << "    mov bx, ax\n    mov ax, [bx]\n";  // least-significant word only
        narrowAxTo(to);
    } else if (!fromWide && toWide) {
        int toN = widthOfType(to) / 2;
        if (from.isUnsigned) {
            code_ << "    xor dx, dx\n";
        } else {
            code_ << "    cwd\n";
        }
        for (int i = 0; i < toN - 2; i++) code_ << "    push dx\n";
        code_ << "    push dx\n    push ax\n";
        code_ << "    mov ax, sp\n";
    } else {
        narrowAxTo(to);
    }
}

// Negates the N-word value whose address is in BX, in place - two's
// complement across the whole block: NOT every word, then add 1 with
// the carry rippling through every word above it (the same "not every
// word, then +1 with carry" shape any software bignum negate uses).
void I8086Codegen::genWideNegate(int n) {
    for (int i = 0; i < n; i++) {
        code_ << "    mov ax, [bx+" << (2 * i) << "]\n    not ax\n    mov [bx+" << (2 * i) << "], ax\n";
    }
    code_ << "    mov ax, [bx]\n    add ax, 1\n    mov [bx], ax\n";
    for (int i = 1; i < n; i++) {
        code_ << "    mov ax, [bx+" << (2 * i) << "]\n    adc ax, 0\n    mov [bx+" << (2 * i) << "], ax\n";
    }
}

// `long`/`long long` add/sub: evaluate both operand addresses, then
// ripple the carry/borrow across N words low-to-high via real ADC/SBB
// - the generalization of a native add/sub one word wider, same idiom
// any multi-word codegen uses, just N words instead of 2.
void I8086Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    // si = lhs address, via a plain register rather than the stack -
    // rhs's own evaluation below may itself push a fresh scratch
    // block (a wide literal/convert), shifting sp further down, which
    // would make a later `pop` here retrieve the wrong word.
    code_ << "    mov si, ax\n";
    genExpr(rhs);
    code_ << "    mov di, ax\n";  // di = rhs address
    code_ << "    sub sp, " << (2 * n) << "\n";
    // Only bx/bp can address memory in this dialect (si/di are
    // address *values* here, never bracket bases - see
    // I8086Assembler.cpp's own buildOperand()), so bx is repointed at
    // whichever of the three bases (lhs/rhs/result) each step needs
    // via a plain `mov` (never `add` - `mov` alone never touches
    // FLAGS on this target, unlike `add`, so repointing bx this way
    // never disturbs the carry the next word's `adc`/`sbb` needs to
    // pick back up from this one; the `+2*i` byte offset itself is
    // just the bracket's own displacement, not a separate instruction
    // - see I8086Assembler.cpp's own `[bx+N]` addressing).
    for (int i = 0; i < n; i++) {
        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
        code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * i) << "]\n";
        code_ << "    " << (i == 0 ? (isAdd ? "add" : "sub") : (isAdd ? "adc" : "sbb")) << " ax, cx\n";
        code_ << "    mov bx, sp\n    mov [bx+" << (2 * i) << "], ax\n";
    }
    code_ << "    mov ax, sp\n";
}

// `long`/`long long` multiply, keeping the low N words of the true
// product - the schoolbook O(n^2) cross-multiply, using the real
// hardware MUL for every partial 16x16->32 product (same idea as
// I386Codegen.cpp's genWideMul, generalized from 2 words to N) and
// discarding whatever overflow a given partial product's own carry
// pushes past word N-1.
void I8086Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    // si = lhs address - see genWideAdd()'s own comment on why this
    // has to be a plain register move, not a push/pop pair, across
    // rhs's own evaluation.
    code_ << "    mov si, ax\n";
    genExpr(rhs);
    code_ << "    mov di, ax\n";  // di = rhs address
    code_ << "    sub sp, " << (2 * n) << "\n";
    code_ << "    mov bx, sp\n";
    code_ << "    mov ax, 0\n";
    for (int i = 0; i < n; i++) code_ << "    mov [bx+" << (2 * i) << "], ax\n";
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n - i; j++) {
            // Only bx/bp can address memory in this dialect, so it's
            // repointed at whichever base (lhs/rhs/result) each step
            // needs via a plain `mov` (never `add` - see genWideAdd()'s
            // own identical comment on why that matters here: the
            // carry chain below depends on it).
            code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
            code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * j) << "]\n";
            code_ << "    mul cx\n";  // dx:ax = word_i(lhs) * word_j(rhs)
            // Add this partial product (dx:ax) into the result
            // starting at word i+j, carry rippling further up through
            // the rest of the N-word result - `mov` never touches
            // FLAGS on this target, so the carry each `add`/`adc`
            // below leaves behind survives the intervening loads/
            // stores undisturbed, letting a single `adc` pick it back
            // up exactly where the previous word's arithmetic left it.
            code_ << "    mov bx, sp\n";
            code_ << "    mov cx, [bx+" << (2 * (i + j)) << "]\n";
            code_ << "    add cx, ax\n";
            code_ << "    mov [bx+" << (2 * (i + j)) << "], cx\n";
            if (i + j + 1 < n) {
                code_ << "    mov cx, [bx+" << (2 * (i + j + 1)) << "]\n";
                code_ << "    adc cx, dx\n";
                code_ << "    mov [bx+" << (2 * (i + j + 1)) << "], cx\n";
                for (int k = i + j + 2; k < n; k++) {
                    code_ << "    mov cx, [bx+" << (2 * k) << "]\n    adc cx, 0\n    mov [bx+" << (2 * k)
                          << "], cx\n";
                }
            }
        }
    }
    code_ << "    mov ax, sp\n";
}

// Classic bit-serial shift/compare/subtract long division, generalized
// from I386Codegen.cpp's own 2-word version to N words (N=2 for
// `long`, N=4 for `long long` on this 16-bit-native target) - same
// algorithm, same scratch-block shape, just a runtime-constant N
// instead of a fixed 2: [0..N-1]=dividend-being-consumed, [N..2N-1]=
// divisor, [2N..3N-1]=remainder being built, [3N..4N-1]=quotient being
// built, [4N]=iteration countdown. `si` addresses the whole block for
// this method's entire duration (nothing else here ever needs `bx`
// for anything else, unlike every other wide helper in this file, so
// it's set once and never repointed).
//
// `isUnsigned` false divides *magnitudes*, negating each operand in
// place if its own sign word (MSW) is negative, remembering both
// signs in two more scratch words, then restoring them on the result
// afterward per C's truncating-division rule (quotient negative iff
// the signs differed; remainder always takes the dividend's own sign)
// - the exact same two-step shape I386Codegen.cpp's own signed
// genWideDivMod() uses.
// genWideDivMod() itself is just a thin call site: copy lhs/rhs into
// the shared N-word scratch block, `call` the one shared subroutine
// for this N (emitted once by emitWideDivModSubroutine(), regardless
// of how many division sites use it), then take a fresh stack copy
// of whichever result (quotient or remainder) was asked for - the
// same "fresh copy before the scratch might be reused" safety
// copyWideBlock() already provides for a plain Var/Convert result,
// needed here since a later division elsewhere in the very same
// expression (e.g. two division results both still live as call
// arguments) would otherwise overwrite this one's result first.
void I8086Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    int n = widthOfType(lhs.type) / 2;
    emitWideDivModSubroutine(n);
    std::string blk = "__wdm" + std::to_string(n) + "_blk";
    genExpr(lhs);
    code_ << "    mov si, ax\n";  // lhs address
    genExpr(rhs);
    code_ << "    mov di, ax\n";  // rhs address
    code_ << "    mov ax, " << (isUnsigned ? 1 : 0) << "\n";
    code_ << "    mov bx, " << blk << "\n    mov [bx+" << (2 * (4 * n + 3)) << "], ax\n";
    for (int w = 0; w < n; w++) {
        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * w) << "]\n";
        code_ << "    mov bx, " << blk << "\n    mov [bx+" << (2 * w) << "], ax\n";
    }
    for (int w = 0; w < n; w++) {
        code_ << "    mov bx, di\n    mov ax, [bx+" << (2 * w) << "]\n";
        code_ << "    mov bx, " << blk << "\n    mov [bx+" << (2 * (n + w)) << "], ax\n";
    }
    code_ << "    call __wdivmod" << n << "\n";
    int resultOff = wantRemainder ? (2 * n) : (3 * n);
    code_ << "    mov bx, " << blk << "\n    lea ax, [bx+" << (2 * resultOff) << "]\n";
    code_ << "    mov bx, ax\n";
    copyWideBlock(n, n, /*isUnsignedExt=*/false);  // fresh copy, since __wdmN_blk may be reused by a later division
}

// The N-word bit-serial divide itself (see genWideDivMod()'s own
// comment on why it's a shared subroutine, not inlined per call
// site) - same algorithm/scratch layout the inlined version used to
// use, just addressed through a fixed global block instead of a
// stack-relative one (no `sub sp`/`add sp` needed around it at all),
// and with `isUnsigned` now a RUNTIME flag word in that same block
// (`uns`) rather than a compile-time branch, since this one routine
// body is shared by both a signed and an unsigned division of the
// same N from however many call sites each uses it.
void I8086Codegen::emitWideDivModSubroutine(int n) {
    if (!wideDivModEmitted_.insert(n).second) return;
    std::string blk = "__wdm" + std::to_string(n) + "_blk";
    trailer_.push_back(blk + ": dw " + [n] {
        std::string s;
        for (int i = 0; i < 4 * n + 4; i++) s += (i ? ", 0" : "0");
        return s;
    }());
    // The subroutine's own code has to land in `trailer_` (after
    // every function body, never reached except by the explicit
    // `call` at a division site - see __wret's own identical "data in
    // the trailer" precedent in generate(), here used for code
    // instead) rather than straight into `code_`, which is still mid-
    // way through building the CALLER's own function body at this
    // point - genWideNegateIf/genWideNegateAt below both write to
    // `code_` directly with no stream parameter of their own, so
    // `code_` is swapped out for a fresh scratch buffer for the
    // length of this method and swapped back before returning.
    std::ostringstream callerCode;
    callerCode << code_.str();
    code_.str("");
    code_.clear();
    code_ << "__wdivmod" << n << ":\n";
    code_ << "    mov bx, " << blk << "\n";
    int signLhsOff = 4 * n + 1, signRhsOff = 4 * n + 2, unsOff = 4 * n + 3;
    code_ << "    mov ax, 0\n";
    for (int w = 0; w < n; w++) code_ << "    mov [bx+" << (2 * (2 * n + w)) << "], ax\n";  // r = 0
    for (int w = 0; w < n; w++) code_ << "    mov [bx+" << (2 * (3 * n + w)) << "], ax\n";  // q = 0
    code_ << "    mov ax, " << (16 * n) << "\n    mov [bx+" << (2 * 4 * n) << "], ax\n";    // cnt
    std::string LskipSignEntry = newLabel();
    code_ << "    mov ax, [bx+" << (2 * unsOff) << "]\n    cmp ax, 0\n    jne " << LskipSignEntry << "\n";
    {
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    mov ax, [bx+" << (2 * (n - 1)) << "]\n    cmp ax, 0\n    mov cx, 0\n    jge " << LskipNegN
              << "\n    mov cx, 1\n" << LskipNegN << ":\n    mov [bx+" << (2 * signLhsOff) << "], cx\n";
        code_ << "    mov ax, [bx+" << (2 * (2 * n - 1)) << "]\n    cmp ax, 0\n    mov cx, 0\n    jge " << LskipNegD
              << "\n    mov cx, 1\n" << LskipNegD << ":\n    mov [bx+" << (2 * signRhsOff) << "], cx\n";
        genWideNegateIf(0, n, signLhsOff);
        genWideNegateIf(n, n, signRhsOff);
    }
    code_ << LskipSignEntry << ":\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov ax, [bx+" << (2 * 4 * n) << "]\n    cmp ax, 0\n";
    // Ldone sits just past this loop's own body, which for a 4-word
    // (`long long`) operand is well past a short (rel8) jump's +-127-
    // byte range - jne-around-a-near-jmp reaches it regardless of
    // distance, same trick real 8086 assembly uses for any
    // conditional branch wider than a short jump.
    {
        std::string Lcontinue = newLabel();
        code_ << "    jne " << Lcontinue << "\n    jmp " << Ldone << "\n" << Lcontinue << ":\n";
    }
    code_ << "    sub ax, 1\n    mov [bx+" << (2 * 4 * n) << "], ax\n";
    // n <<= 1 (0-filled), capturing its own old top bit via the real
    // carry flag (the same rcl-style low-to-high chain genWideShift()
    // uses - `mov` never touches FLAGS on this target, so the carry
    // the last word's own `shl`/`rcl` leaves behind survives the
    // intervening loads/stores undisturbed) - `jc` right after reads
    // that same still-pending carry back out as a plain 0/1 into cx,
    // avoiding a 15-wide shift just to isolate one bit.
    code_ << "    mov ax, [bx+0]\n    shl ax, 1\n    mov [bx+0], ax\n";
    for (int w = 1; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * w) << "]\n    rcl ax, 1\n    mov [bx+" << (2 * w) << "], ax\n";
    }
    std::string Lcarry0 = newLabel(), Lcarry1 = newLabel();
    code_ << "    jc " << Lcarry1 << "\n    mov cx, 0\n    jmp " << Lcarry0 << "\n";
    code_ << Lcarry1 << ":\n    mov cx, 1\n" << Lcarry0 << ":\n";  // cx = n's own old top bit (the one shifted out)
    // r = (r << 1) | cx
    code_ << "    mov ax, [bx+" << (2 * 2 * n) << "]\n    shl ax, 1\n    or ax, cx\n    mov [bx+" << (2 * 2 * n)
          << "], ax\n";
    for (int w = 1; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * (2 * n + w)) << "]\n    rcl ax, 1\n    mov [bx+" << (2 * (2 * n + w))
              << "], ax\n";
    }
    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 } - always an unsigned
    // magnitude compare, N words, most significant first.
    for (int k = n - 1; k >= 0; k--) {
        code_ << "    mov ax, [bx+" << (2 * (2 * n + k)) << "]\n";
        code_ << "    mov cx, [bx+" << (2 * (n + k)) << "]\n";
        code_ << "    cmp ax, cx\n";
        code_ << "    ja " << LsubYes << "\n    jb " << LsubNo << "\n";
        // equal at this word - fall through to compare the next, less
        // significant one
    }
    code_ << "    jmp " << LsubYes << "\n";  // every word equal -> r == d -> subtract
    code_ << LsubYes << ":\n";
    for (int w = 0; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * (2 * n + w)) << "]\n";
        code_ << "    mov cx, [bx+" << (2 * (n + w)) << "]\n";
        code_ << "    " << (w == 0 ? "sub" : "sbb") << " ax, cx\n";
        code_ << "    mov [bx+" << (2 * (2 * n + w)) << "], ax\n";
    }
    code_ << "    mov cx, 1\n    jmp " << LhiDiff << "\n";
    code_ << LsubNo << ":\n    mov cx, 0\n";
    code_ << LhiDiff << ":\n";
    // q = (q << 1) | cx
    code_ << "    mov ax, [bx+" << (2 * 3 * n) << "]\n    shl ax, 1\n    or ax, cx\n    mov [bx+" << (2 * 3 * n)
          << "], ax\n";
    for (int w = 1; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * (3 * n + w)) << "]\n    rcl ax, 1\n    mov [bx+" << (2 * (3 * n + w))
              << "], ax\n";
    }
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    std::string LskipSignExit = newLabel();
    code_ << "    mov ax, [bx+" << (2 * unsOff) << "]\n    cmp ax, 0\n    jne " << LskipSignExit << "\n";
    {
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov ax, [bx+" << (2 * signLhsOff) << "]\n    mov cx, [bx+" << (2 * signRhsOff) << "]\n";
        code_ << "    cmp ax, cx\n    je " << LskipNegQ << "\n";
        genWideNegateAt(3 * n, n);
        code_ << LskipNegQ << ":\n";
        code_ << "    mov ax, [bx+" << (2 * signLhsOff) << "]\n    cmp ax, 0\n    je " << LskipNegR << "\n";
        genWideNegateAt(2 * n, n);
        code_ << LskipNegR << ":\n";
    }
    code_ << LskipSignExit << ":\n";
    code_ << "    ret\n";
    trailer_.push_back(code_.str());
    code_.str("");
    code_.clear();
    code_ << callerCode.str();
}

// Negates the N words at scratch offset `wordOffset` (word units, bx
// already the scratch base) in place, only if the flag word at
// `flagOffset` is non-zero - the signed genWideDivMod()'s own
// "negate this operand's magnitude iff its sign flag is set" step.
void I8086Codegen::genWideNegateIf(int wordOffset, int n, int flagOffset) {
    std::string Lskip = newLabel();
    code_ << "    mov ax, [bx+" << (2 * flagOffset) << "]\n    cmp ax, 0\n    je " << Lskip << "\n";
    genWideNegateAt(wordOffset, n);
    code_ << Lskip << ":\n";
}

// Negates the N words at scratch offset `wordOffset` in place (bx
// already the scratch base) - two's complement across the whole
// block, same "not every word, then +1 with carry" shape as
// genWideNegate(), just addressed by a fixed displacement from `bx`
// instead of a separate address in a register.
void I8086Codegen::genWideNegateAt(int wordOffset, int n) {
    for (int w = 0; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * (wordOffset + w)) << "]\n    not ax\n    mov [bx+"
              << (2 * (wordOffset + w)) << "], ax\n";
    }
    code_ << "    mov ax, [bx+" << (2 * wordOffset) << "]\n    add ax, 1\n    mov [bx+" << (2 * wordOffset)
          << "], ax\n";
    for (int w = 1; w < n; w++) {
        code_ << "    mov ax, [bx+" << (2 * (wordOffset + w)) << "]\n    adc ax, 0\n    mov [bx+"
              << (2 * (wordOffset + w)) << "], ax\n";
    }
}

// `long`/`long long` shifted by a runtime count - no native
// instruction shifts more than one native word at once, so this is a
// counted loop of 1-bit shifts, each using the real carry flag to
// chain across the N words (rcl/rcr - see I8086Assembler.cpp's own
// comment on why `mov` never disturbing FLAGS is what makes that
// safe with memory loads/stores interleaved). `isUnsigned` only
// matters for `>>`: the first (most significant) word's own shift is
// `sar` (sign-preserving) or `shr` (zero-filling) accordingly; `<<`
// never cares about signedness.
void I8086Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    code_ << "    mov si, ax\n";  // si = value's own address (held as a value; bx repoints onto it each access)
    genExpr(rhs);                  // ax = shift count (already promoted to a plain int)
    code_ << "    mov cx, ax\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmp cx, 0\n    je " << Ldone << "\n    sub cx, 1\n";
    if (isLeft) {
        code_ << "    mov bx, si\n    mov ax, [bx+0]\n    shl ax, 1\n    mov [bx+0], ax\n";
        for (int w = 1; w < n; w++) {
            code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * w) << "]\n    rcl ax, 1\n    mov [bx+" << (2 * w)
                  << "], ax\n";
        }
    } else {
        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * (n - 1)) << "]\n    "
              << (isUnsigned ? "shr" : "sar") << " ax, 1\n    mov [bx+" << (2 * (n - 1)) << "], ax\n";
        for (int w = n - 2; w >= 0; w--) {
            code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * w) << "]\n    rcr ax, 1\n    mov [bx+" << (2 * w)
                  << "], ax\n";
        }
    }
    code_ << "    jmp " << Lloop << "\n" << Ldone << ":\n";
    code_ << "    mov ax, si\n";
}

// `long`/`long long` comparison - compare the most significant word
// first (using the signed or unsigned jcc per `isUnsigned`, since
// that's where the sign bit lives); only if it's equal does the next
// word down matter, and once any word differs the comparison is
// always an unsigned magnitude compare from there (the standard big-
// number ordered-comparison idiom, same as I386Codegen.cpp's own
// genWideCompare, generalized from 2 words to N).
void I8086Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    // si = lhs address - see genWideAdd()'s own comment on why this
    // has to be a plain register move, not a push/pop pair, across
    // rhs's own evaluation.
    code_ << "    mov si, ax\n";
    genExpr(rhs);
    code_ << "    mov di, ax\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        for (int k = 0; k < n; k++) {
            code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * k) << "]\n";
            code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * k) << "]\n";
            code_ << "    cmp ax, cx\n    jne " << Lfalse << "\n";
        }
        code_ << "    mov ax, " << (op == "==" ? "1" : "0") << "\n    jmp " << Ldone << "\n";
        code_ << Lfalse << ":\n    mov ax, " << (op == "==" ? "0" : "1") << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    bool resultIfLess = (op == "<" || op == "<=");
    bool resultIfGreater = (op == ">" || op == ">=");
    for (int k = n - 1; k >= 0; k--) {
        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * k) << "]\n";
        code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * k) << "]\n";
        code_ << "    cmp ax, cx\n";
        std::string lessJcc = (k == n - 1 && !isUnsigned) ? "jl" : "jb";
        std::string greaterJcc = (k == n - 1 && !isUnsigned) ? "jg" : "ja";
        code_ << "    " << lessJcc << " " << (resultIfLess ? Ltrue : Lfalse) << "\n";
        code_ << "    " << greaterJcc << " " << (resultIfGreater ? Ltrue : Lfalse) << "\n";
    }
    // Every word equal.
    code_ << "    mov ax, " << ((op == "<=" || op == ">=") ? "1" : "0") << "\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        code_ << "    mov ax, " << arrayElemText(slot->arrayInit[i]) << "\n";
        std::string dst = addr(slot, w * static_cast<int>(i));
        if (w == 1) {
            code_ << "    movb " << dst << ", ax\n";
        } else {
            code_ << "    mov " << dst << ", ax\n";
        }
    }
}

void I8086Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h) - `e.lhs->type` names it, except
    // for shift (only the LHS's own type matters) and &&/|| (width-
    // independent truth-testing, handled by genLogicalAnd/Or below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
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
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs, isUnsigned);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        // Real hardware and/or/xor - unlike pdp7 (see
        // Pdp7Codegen.cpp's own comment), this target has all three
        // directly, so no De Morgan's trick is needed for `|`.
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        if (wide) {
            int n = widthOfType(operandType) / 2;
            genExpr(*e.lhs);
            // si = lhs address - see genWideAdd()'s own comment on
            // why this has to be a plain register move, not a
            // push/pop pair, across rhs's own evaluation.
            code_ << "    mov si, ax\n";
            genExpr(*e.rhs);
            code_ << "    mov di, ax\n";
            code_ << "    sub sp, " << (2 * n) << "\n";
            for (int i = 0; i < n; i++) {
                code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
                code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * i) << "]\n";
                code_ << "    " << insn << " ax, cx\n";
                code_ << "    mov bx, sp\n    mov [bx+" << (2 * i) << "], ax\n";
            }
            code_ << "    mov ax, sp\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push ax\n";
        genExpr(*e.rhs);
        code_ << "    pop bx\n";
        code_ << "    " << insn << " ax, bx\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        // Real hardware variable-count shift (shl/shr/sar take the
        // count from cl) - unlike pdp7, which has no such instruction
        // and has to loop (see Pdp7Codegen.cpp's own comment). `>>`
        // is `shr` (logical/unsigned) or `sar` (arithmetic/signed)
        // per the shifted value's own signedness.
        genExpr(*e.lhs);
        code_ << "    push ax\n";
        genExpr(*e.rhs);
        code_ << "    mov cx, ax\n";
        code_ << "    pop ax\n";
        const char *insn = op == "<<" ? "shl" : (isUnsigned ? "shr" : "sar");
        code_ << "    " << insn << " ax, cl\n";
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
    bool isUnsigned = lhs.type.isUnsigned;
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, isUnsigned);
        return;
    }
    if (op == "==") return emitCompare(lhs, rhs, "je", isUnsigned);
    if (op == "!=") return emitCompare(lhs, rhs, "jne", isUnsigned);
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "jb" : "jl", isUnsigned);
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "jbe" : "jle", isUnsigned);
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "ja" : "jg", isUnsigned);
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "jae" : "jge", isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// cmp bx, ax compares lhs-rhs (bx=lhs, ax=rhs after the pop below),
// so `jumpIfTrue` is a conditional jump matching C's comparison
// semantics directly - no sza/sma-style workaround needed;
// `isUnsigned` is unused here (the caller already baked it into
// `jumpIfTrue`) but kept for symmetry with the wide path/call sites.
void I8086Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue, bool) {
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

// A wide (`long`/`long long`) operand's truthiness needs every word -
// `mov bx,ax` + OR-ing each further word into `ax` folds them
// together before the usual "cmp ax,0" (the combined value is
// discarded immediately after, so clobbering ax this way is safe).
void I8086Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) genWideTruthify(lhs.type);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) genWideTruthify(rhs.type);
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) genWideTruthify(lhs.type);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) genWideTruthify(rhs.type);
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) genWideTruthify(rhs.type);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Ltrue << "\n";
    code_ << "    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}


// Real hardware multiply/divide (IMUL/IDIV, or MUL/DIV for an
// unsigned operand) - no software routine needed the way pdp7's lack
// of a verified idiv required; only the low word of the product is
// kept, matching C int wraparound.
// Reduces the N-word value whose address is in AX down to a single
// word in AX that's zero iff every word was zero - every other wide
// helper here leaves an ADDRESS in ax, not a value, so a plain
// "cmp ax,0" would only ever test whether that address happens to be
// 0 (never true in practice), not the value it points to.
void I8086Codegen::genWideTruthify(Type t) {
    int n = widthOfType(t) / 2;
    code_ << "    mov bx, ax\n    mov ax, [bx]\n";
    for (int i = 1; i < n; i++) {
        code_ << "    mov cx, [bx+" << (2 * i) << "]\n    or ax, cx\n";
    }
}

void I8086Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push ax\n";
    genExpr(rhs);
    code_ << "    pop bx\n";
    code_ << "    " << (isUnsigned ? "mul" : "imul") << " bx\n";  // dx:ax = ax*bx; keep ax
}

void I8086Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push ax\n";
    genExpr(rhs);
    code_ << "    mov bx, ax\n";  // bx = divisor
    code_ << "    pop ax\n";      // ax = dividend
    code_ << "    " << (isUnsigned ? "xor dx, dx\n" : "cwd\n");
    code_ << "    " << (isUnsigned ? "div" : "idiv") << " bx\n";  // ax = quotient, dx = remainder
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
