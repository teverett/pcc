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

// `@addr` placement: pads forward to a byte offset from ORG (0x7C00) via `times`.
// NASM's `$` only increases, so placement can only insert forward padding, never reorder.
void I8086Codegen::emitPlacement(long address) { code_ << "times (" << address << "-($-$$)) db 0\n"; }

std::string I8086Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In objectMode, prog may have no `main`; otherwise IRBuilder guarantees one exists.

    // Tracks (sourceFile, start, end) ranges into code_/trailer_ for --dump-asm's
    // per-file output, without affecting the combined asmText returned below.
    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    // Fallback file attribution for --dump-asm when nothing owns a Slot/Function.
    std::string homeFile = mainFn ? mainFn->sourceFile
                            : !prog.functionBodies.empty() ? prog.functionBodies.front().func->sourceFile
                            : !prog.slots.empty() ? prog.slots.front()->sourceFile
                                                   : std::string();

    size_t rangeStart = static_cast<size_t>(code_.tellp());
    code_ << "BITS 16\n";
    // objectMode: no boot-sector wrapper/entry sequence here - the linker synthesizes
    // one after linking. An object's own base is `ORG 0`; placement is the linker's job.
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
        code_ << "    mov sp, 0x7C00\n";  // stack grows down from just below the boot sector itself
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

    // Fixed scratch for a wide Return value (its address may point into the frame
    // the epilogue is about to tear down); 4 words covers even a `long long`.
    trailer_.push_back("__wret: dw 0, 0, 0, 0");
    trailer_.push_back("__wargsp: dw 0");

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;  // not defined in this file
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
                // A wide element's literal splits into N low-to-high words; a symbol
                // element (dispatch-table idiom) is always a plain native-width address.
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

// Standard frame: push bp/mov bp,sp establishes the frame pointer Local/Param
// addressing is relative to; sub sp,N reserves locals. Epilogue tears it down in two
// instructions regardless of local count, giving each activation its own frame.
void I8086Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ":\n";
    // The body is generated first so the prologue can reserve one word below the Locals
    // for each nesting level of wideScope().
    curFrameSize_ = fn.func->frameSize;
    wideScopeDepth_ = 0;
    maxWideScopeDepth_ = 0;
    std::ostringstream body;
    std::swap(body, code_);
    genStmt(*fn.body);
    std::swap(body, code_);
    int frame = curFrameSize_ + 2 * maxWideScopeDepth_;
    code_ << "    push bp\n    mov bp, sp\n";
    if (frame > 0) code_ << "    sub sp, " << frame << "\n";
    std::string bodyText = body.str();
    const std::string placeholder = kFrameBottomPlaceholder;
    for (size_t at = bodyText.find(placeholder); at != std::string::npos; at = bodyText.find(placeholder, at)) {
        bodyText.replace(at, placeholder.size(), std::to_string(frame));
    }
    code_ << bodyText;

    code_ << "    mov sp, bp\n    pop bp\n";
    code_ << "    ret\n\n";
}

bool I8086Codegen::involvesWide(const ir::Expr &e) const {
    if (isWide(e.type) || (e.slot && isWide(e.slot->type))) return true;
    for (const ir::Expr *child : {e.lhs.get(), e.rhs.get(), e.elseExpr.get()}) {
        if (child && involvesWide(*child)) return true;
    }
    for (const auto &arg : e.args) {
        if (involvesWide(*arg)) return true;
    }
    return false;
}

void I8086Codegen::wideScope(const std::function<void()> &gen) {
    int cell = curFrameSize_ + 2 * (wideScopeDepth_ + 1);
    wideScopeDepth_++;
    maxWideScopeDepth_ = std::max(maxWideScopeDepth_, wideScopeDepth_);
    code_ << "    mov [bp-" << cell << "], sp\n";
    gen();
    code_ << "    mov sp, [bp-" << cell << "]\n";
    wideScopeDepth_--;
}

void I8086Codegen::resetStackToFrame() {
    code_ << "    lea sp, [bp-" << kFrameBottomPlaceholder << "]\n";
}

void I8086Codegen::genTruthValue(const ir::Expr &e, bool atStatement) {
    if (!isWide(e.type)) {
        genExpr(e);
        return;
    }
    if (atStatement) {
        genExpr(e);
        genWideTruthify(e.type);
        resetStackToFrame();
        return;
    }
    wideScope([&] {
        genExpr(e);
        genWideTruthify(e.type);
    });
}

void I8086Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genTruthValue(*s.cond, /*atStatement=*/true);
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
            // continue runs the post-statement (desugared for-loop) before retesting,
            // if there is one; otherwise it jumps straight back to the condition.
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genTruthValue(*s.cond, /*atStatement=*/true);
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
                    // The result's address may point into this frame, about to be torn
                    // down below, so copy it into the fixed __wret scratch first; safe
                    // since the caller consumes __wret immediately after the call returns.
                    int n = widthOfType(s.initOrReturnOrExpr->type) / 2;
                    code_ << "    mov si, ax\n";  // si = source address value
                    for (int i = 0; i < n; i++) {
                        // Only bx/bp can address memory here, so bx is repointed per base.
                        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
                        code_ << "    mov bx, __wret\n    mov [bx+" << (2 * i) << "], ax\n";
                    }
                    code_ << "    mov ax, __wret\n";
                }
            }
            // Early exit: inline the same frame teardown genFunction()'s epilogue does.
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
                    resetStackToFrame();
                } else if (involvesWide(*s.initOrReturnOrExpr)) {
                    genExpr(*s.initOrReturnOrExpr);
                    resetStackToFrame();
                    storeOperand(addr(s.declSlot), s.declSlot->type);
                } else {
                    genExpr(*s.initOrReturnOrExpr);
                    storeOperand(addr(s.declSlot), s.declSlot->type);
                }
            }
            // An uninitialized scalar needs no code: its frame word already exists.
            return;
        }
        case ir::StmtKind::ExprStmt: {
            // A wide value's temporaries stay on the stack until its scope ends.
            genExpr(*s.initOrReturnOrExpr);
            if (involvesWide(*s.initOrReturnOrExpr)) resetStackToFrame();
            return;
        }
        case ir::StmtKind::Empty:
            return;
        case ir::StmtKind::Asm: {
            // Verbatim passthrough: emitted exactly as written, no reformatting.
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "    jmp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void I8086Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                // Real 8086 has no PUSH imm16 - load each word into AX first.
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
            // IRBuilder rejects a bare array Var outside of `&`, so e.slot is scalar here.
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
                // rhs must be evaluated before the destination address: a wide rhs
                // (genWideAdd/Mul/Compare) uses si/di as scratch, which would clobber
                // a destination address stashed there first.
                genExpr(*e.rhs);       // ax = source address
                code_ << "    mov bx, ax\n";
                copyWideBlock(n, n, /*isUnsignedExt=*/false);  // ax = fresh copy's address
                code_ << "    mov di, ax\n";  // di = fresh copy's own address
                // An indexed destination needs the element's own address (bx, from
                // computeElementAddr); an unindexed one uses genWideSlotAddr instead.
                if (e.lhs) {
                    computeElementAddr(e.slot, *e.lhs);
                    code_ << "    mov si, bx\n";
                } else {
                    genWideSlotAddr(e.slot);
                    code_ << "    mov si, ax\n";
                }
                // Copy the fresh stack copy (di) into the real destination (si), then
                // free the stack copy's space.
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
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                computeElementAddr(e.slot, *e.lhs);  // bx = address
                genCompoundThroughBx(e);
                return;
            }
            // operandOf(slot) is a stable memory operand, not a clobberable register,
            // so no address needs to be held across rhs's own evaluation.
            std::string mem = operandOf(e.slot);
            genExpr(*e.rhs);  // ax = rhs
            code_ << "    mov bx, ax\n";
            loadOperand(mem, e.type);  // ax = old
            if (e.yieldOld) code_ << "    push ax\n";
            genCompoundOp(e.op, e.type);
            narrowAxTo(e.type);
            storeOperand(mem, e.type);
            if (e.yieldOld) code_ << "    pop ax\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // ax = pointer value
            code_ << "    mov bx, ax\n";  // bx = address
            genCompoundThroughBx(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genTruthValue(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp ax, 0\n    je " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's own width (the pointee type). A wide pointee reuses
            // the "fresh protected copy, then copy to real destination" shape of a
            // wide Assign, except the destination address comes from evaluating e.lhs
            // (the pointer, always narrow) rather than genWideSlotAddr/computeElementAddr.
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
                // Loads at e.type's own width. A wide pointee's evaluation already
                // leaves its storage address in ax, which is exactly the pointer value,
                // so there's nothing further to load.
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
            // cdecl: arguments pushed right-to-left so the first ends up closest to the
            // return address, matching Param addressing (paramIndex 0 at bp+4); the
            // caller cleans up afterward (add sp,N).
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    // A wide argument's evaluation leaves its own temp block on the
                    // stack. Since several arguments are pushed within this same
                    // statement, an earlier temp would be displaced by a later one's -
                    // so each argument's real copy is written at (sp before it ran) -
                    // n words, and sp is reset there, reclaiming the temp.
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
            // e.args is always empty (IRBuilder rejects arguments on this kind).
            // "call ax" is a real near-indirect call through a register (FF /2).
            genExpr(*e.lhs);  // ax = the callee's own entry address
            code_ << "    call ax\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 8086 interrupt enable/disable (sti/cli).
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

// slot[idxExpr]'s effective-address operand text ("[bx]"), with bx loaded with the
// final address (idx*2 words, plus the array's/pointer's own base).
//
// slot is the base address itself only for a declared array; slot[idx] on a plain
// variable indexes through that variable's runtime pointer VALUE instead, via
// operandOf(). A Global array's base is a compile-time immediate; a Local array's
// needs `lea` (only known relative to bp at runtime).
std::string I8086Codegen::computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);
    // shl only takes a literal count of 1 (or cl) - a wider shift is that many in a row.
    for (int i = 0; i < elemShift(slot->type); i++) code_ << "    shl ax, 1\n";
    code_ << "    mov bx, ax\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    add bx, " << label(slot) << "\n";
        } else {
            code_ << "    lea ax, " << addr(slot) << "\n    add bx, ax\n";
        }
    } else {
        code_ << "    add bx, " << operandOf(slot) << "\n";
    }
    return "[bx]";
}

void I8086Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add ax, bx\n"; return; }
    if (op == "-") { code_ << "    sub ax, bx\n"; return; }
    if (op == "*") { code_ << "    " << (isUnsigned ? "mul" : "imul") << " bx\n"; return; }  // dx:ax = ax*bx
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    " << insn << " ax, bx\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        code_ << "    mov cx, bx\n";
        code_ << "    " << (op == "<<" ? "shl" : (isUnsigned ? "shr" : "sar")) << " ax, cl\n";
        return;
    }
    // "/" or "%": div/idiv only forbid ax/dx (the dividend) as the divisor.
    code_ << "    " << (isUnsigned ? "xor dx, dx\n" : "cwd\n");
    code_ << "    " << (isUnsigned ? "div" : "idiv") << " bx\n";
    if (op == "%") code_ << "    mov ax, dx\n";
}

// bx must already hold the resolved address. Only `[bx]` is a valid memory base here,
// so the address is stashed on the stack (not held in a register) across rhs's own
// evaluation, which may use any register.
void I8086Codegen::genCompoundThroughBx(const ir::Expr &e) {
    code_ << "    push bx\n";  // stash address
    genExpr(*e.rhs);            // ax = rhs
    code_ << "    push ax\n";    // stash rhs
    code_ << "    pop cx\n";      // cx = rhs
    code_ << "    pop bx\n";       // bx = address
    loadOperand("[bx]", e.type);    // ax = old
    code_ << "    push bx\n";        // re-stash address for the store below
    if (e.yieldOld) code_ << "    push ax\n";  // stash old too
    code_ << "    mov bx, cx\n";                // bx = rhs
    genCompoundOp(e.op, e.type);                  // ax = new (raw)
    narrowAxTo(e.type);
    if (e.yieldOld) {
        code_ << "    mov cx, ax\n";  // cx = new
        code_ << "    pop ax\n";       // ax = old
        code_ << "    pop bx\n";        // bx = address
        code_ << "    mov dx, ax\n";     // dx = old (preserved across the store below)
        code_ << "    mov ax, cx\n";      // ax = new (storeOperand always stores from ax)
        storeOperand("[bx]", e.type);
        code_ << "    mov ax, dx\n";        // ax = old (final result)
    } else {
        code_ << "    pop bx\n";  // bx = address
        storeOperand("[bx]", e.type);  // ax already = new
    }
}

void I8086Codegen::genIndex(const ir::Expr &e) {
    std::string memOperand = computeElementAddr(e.slot, *e.lhs);  // bx = element address
    if (isWide(e.slot->type)) {
        // Every wide expression leaves its address in ax - bx already holds it.
        code_ << "    mov ax, bx\n";
        return;
    }
    loadOperand(memOperand, e.slot->type);
}

// &operand: a variable/array, an array element, or a bare function name. A Global's
// address is a compile-time immediate; a Local/Param's needs `lea` since it's only
// known at runtime relative to bp.
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
                code_ << "    mov bx, ax\n    lea ax, " << addr(operand.slot) << "\n    add ax, bx\n";
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

void I8086Codegen::narrowAxTo(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        // No movzx/movsx on real 8086, and no "ah" operand in this dialect - zero-extend
        // by masking out bits above AL; sign-extend with CBW (AL into AH:AX).
        if (to.isUnsigned) {
            code_ << "    and ax, 0x00FF\n";
        } else {
            code_ << "    cbw\n";
        }
    }
    // 2 bytes: ax already holds the full value. A wide type never reaches here.
}

void I8086Codegen::loadOperand(const std::string &mem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        if (type.isUnsigned) {
            code_ << "    xor ax, ax\n";  // movb loads al only, never touches ah
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

// Leaves the address of slot's N-word storage in ax, words low-to-high as address
// increases. Global/Param storage already grows that way, so ax is just the base
// address. A Local's storage grows DOWN from bp as size increases, so its word 0 is
// the farthest word from bp: ax = bp - frameOffset - (N-1)*2, not bp - frameOffset.
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

// Builds a fresh toN-word low-to-high block on the stack from the fromN-word block
// at the address in bx (covers both a same-width copy and a widening/narrowing
// Convert). Pushes extension words first (highest addresses), then the real words
// high-to-low, so the least significant word ends up at the lowest address. Leaves
// the new block's address in ax.
void I8086Codegen::copyWideBlock(int fromN, int toN, bool isUnsignedExt) {
    int copyN = fromN < toN ? fromN : toN;
    if (toN > fromN) {
        code_ << "    mov cx, [bx+" << (2 * (fromN - 1)) << "]\n";  // cx = old MSW
        if (isUnsignedExt) {
            code_ << "    xor dx, dx\n";
        } else {
            code_ << "    mov ax, cx\n    cwd\n";  // dx = sign-extension of old MSW
        }
        for (int i = 0; i < toN - fromN; i++) code_ << "    push dx\n";
    }
    for (int i = copyN - 1; i >= 0; i--) {
        code_ << "    mov cx, [bx+" << (2 * i) << "]\n";
        code_ << "    push cx\n";
    }
    code_ << "    mov ax, sp\n";
}

// IRBuilder already promotes narrower operands before Binary/Unary sees them, so the
// only work here is crossing the native-int/wide boundary, or re-extending via narrowAxTo.
void I8086Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (fromWide && !toWide) {
        wideScope([&] {
            genExpr(*e.lhs);
            code_ << "    mov bx, ax\n    mov ax, [bx]\n";  // least-significant word only
        });
        narrowAxTo(to);
        return;
    }
    genExpr(*e.lhs);  // ax, or an address if fromWide
    if (fromWide && toWide) {
        int fromN = widthOfType(from) / 2, toN = widthOfType(to) / 2;
        if (fromN == toN) return;  // same width - bit pattern unchanged
        code_ << "    mov bx, ax\n";
        copyWideBlock(fromN, toN, from.isUnsigned);
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

// Two's complement across the N-word value at bx: NOT every word, then +1 with carry
// rippling through the rest.
void I8086Codegen::genWideNegate(int n) {
    for (int i = 0; i < n; i++) {
        code_ << "    mov ax, [bx+" << (2 * i) << "]\n    not ax\n    mov [bx+" << (2 * i) << "], ax\n";
    }
    code_ << "    mov ax, [bx]\n    add ax, 1\n    mov [bx], ax\n";
    for (int i = 1; i < n; i++) {
        code_ << "    mov ax, [bx+" << (2 * i) << "]\n    adc ax, 0\n    mov [bx+" << (2 * i) << "], ax\n";
    }
}

// long/long long add/sub: ripple carry/borrow across N words low-to-high via ADC/SBB.
void I8086Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    // si holds the address as a value (not a bracket base), so rhs's own evaluation
    // (which may push its own scratch, moving sp) can't disturb it.
    code_ << "    mov si, ax\n";
    genExpr(rhs);
    code_ << "    mov di, ax\n";  // di = rhs address
    code_ << "    sub sp, " << (2 * n) << "\n";
    // Only bx/bp can address memory here, so bx is repointed per base via `mov`
    // (never `add`, since `mov` doesn't touch FLAGS - preserving the carry chain).
    for (int i = 0; i < n; i++) {
        code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
        code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * i) << "]\n";
        code_ << "    " << (i == 0 ? (isAdd ? "add" : "sub") : (isAdd ? "adc" : "sbb")) << " ax, cx\n";
        code_ << "    mov bx, sp\n    mov [bx+" << (2 * i) << "], ax\n";
    }
    code_ << "    mov ax, sp\n";
}

// long/long long multiply: schoolbook O(n^2) cross-multiply with hardware MUL for
// each 16x16->32 partial product, keeping only the low N words (discarding overflow).
void I8086Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    code_ << "    mov si, ax\n";  // si = lhs address, held across rhs's own evaluation
    genExpr(rhs);
    code_ << "    mov di, ax\n";  // di = rhs address
    code_ << "    sub sp, " << (2 * n) << "\n";
    code_ << "    mov bx, sp\n";
    code_ << "    mov ax, 0\n";
    for (int i = 0; i < n; i++) code_ << "    mov [bx+" << (2 * i) << "], ax\n";
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n - i; j++) {
            // bx is repointed per base via `mov` (not `add`, which would disturb FLAGS).
            code_ << "    mov bx, si\n    mov ax, [bx+" << (2 * i) << "]\n";
            code_ << "    mov bx, di\n    mov cx, [bx+" << (2 * j) << "]\n";
            code_ << "    mul cx\n";  // dx:ax = word_i(lhs) * word_j(rhs)
            // Add dx:ax into the result at word i+j, carry rippling up through the rest.
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

// Classic bit-serial shift/compare/subtract long division, generalized to N words
// (N=2 for `long`, N=4 for `long long`). Scratch block layout: [0..N-1]=dividend being
// consumed, [N..2N-1]=divisor, [2N..3N-1]=remainder being built, [3N..4N-1]=quotient
// being built, [4N]=iteration countdown.
//
// Signed division operates on magnitudes: negates each operand in place if its sign
// word is negative, remembers both signs, then restores them on the result per C's
// truncating-division rule (quotient negative iff signs differed; remainder takes the
// dividend's sign).
//
// This function is a thin call site: copy lhs/rhs into the shared N-word scratch block,
// call the one shared subroutine for this N, then take a fresh stack copy of whichever
// result was asked for (needed since a later division could overwrite the same scratch).
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

// The N-word bit-serial divide itself, emitted once per N as a shared subroutine
// (addressed through a fixed global block, not the stack) - `isUnsigned` is a runtime
// flag word in that block, since one routine body serves both signed and unsigned calls.
void I8086Codegen::emitWideDivModSubroutine(int n) {
    if (!wideDivModEmitted_.insert(n).second) return;
    std::string blk = "__wdm" + std::to_string(n) + "_blk";
    trailer_.push_back(blk + ": dw " + [n] {
        std::string s;
        for (int i = 0; i < 4 * n + 4; i++) s += (i ? ", 0" : "0");
        return s;
    }());
    // The subroutine's code must land in trailer_, not mid-way through the caller's own
    // code_ - so code_ is swapped for a fresh buffer here and swapped back at the end.
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
    // jne-around-a-near-jmp reaches Ldone regardless of distance, since a 4-word
    // loop body can exceed a short jump's +-127-byte range.
    {
        std::string Lcontinue = newLabel();
        code_ << "    jne " << Lcontinue << "\n    jmp " << Ldone << "\n" << Lcontinue << ":\n";
    }
    code_ << "    sub ax, 1\n    mov [bx+" << (2 * 4 * n) << "], ax\n";
    // n <<= 1, capturing its old top bit via the carry flag (shl/rcl chain, same as
    // genWideShift); `jc` reads that pending carry into cx as a plain 0/1.
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

// Negates the N words at scratch offset wordOffset (bx = scratch base) only if the
// flag word at flagOffset is non-zero.
void I8086Codegen::genWideNegateIf(int wordOffset, int n, int flagOffset) {
    std::string Lskip = newLabel();
    code_ << "    mov ax, [bx+" << (2 * flagOffset) << "]\n    cmp ax, 0\n    je " << Lskip << "\n";
    genWideNegateAt(wordOffset, n);
    code_ << Lskip << ":\n";
}

// Negates the N words at scratch offset wordOffset (bx = scratch base) in place.
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

// long/long long shift by a runtime count: a counted loop of 1-bit shifts, chaining
// across the N words via rcl/rcr and the carry flag. `>>` uses sar (signed) or shr
// (unsigned) on the most significant word; `<<` doesn't care about signedness.
void I8086Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    code_ << "    mov si, ax\n";  // si = value's address, held across rhs's own evaluation
    genExpr(rhs);                  // ax = shift count
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

// long/long long comparison: compares the most significant word first with the
// signed/unsigned jcc (the sign bit lives there); once a word differs, every word
// below is an unsigned magnitude compare.
void I8086Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    int n = widthOfType(lhs.type) / 2;
    genExpr(lhs);
    code_ << "    mov si, ax\n";  // si = lhs address, held across rhs's own evaluation
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
    // e.lhs->type names the common operand type (IRBuilder's usual-arithmetic-conversion
    // pass), except for shift (only LHS matters) and &&/|| (handled separately below).
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
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        if (wide) {
            int n = widthOfType(operandType) / 2;
            genExpr(*e.lhs);
            code_ << "    mov si, ax\n";  // si = lhs address, held across rhs's own evaluation
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
        // Real hardware variable-count shift: shl/shr/sar take the count from cl.
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
        wideScope([&] { genWideCompare(op, lhs, rhs, isUnsigned); });
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

// cmp bx, ax compares lhs-rhs (bx=lhs, ax=rhs), so jumpIfTrue matches C's comparison
// semantics directly. isUnsigned is already baked into jumpIfTrue by the caller.
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

void I8086Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genTruthValue(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    genTruthValue(rhs);
    code_ << "    cmp ax, 0\n    je " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genTruthValue(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    genTruthValue(rhs);
    code_ << "    cmp ax, 0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov ax, 0\n";
    code_ << Ldone << ":\n";
}

void I8086Codegen::genLogicalNot(const ir::Expr &rhs) {
    genTruthValue(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp ax, 0\n    je " << Ltrue << "\n";
    code_ << "    mov ax, 0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov ax, 1\n";
    code_ << Ldone << ":\n";
}


// Reduces the N-word value at the address in ax to a single word in ax that's zero
// iff every word was zero (every wide helper leaves an address in ax, not a value).
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

// A from-scratch x86 instruction encoder, no external assembler needed.
//
// The boot-sector padding/signature is only added for `--format=raw`, never in
// generate() itself: an object file has no 510-byte limit (it's never booted
// directly), so it must never see this padding.
std::optional<std::string> I8086Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") {
        std::string bootable = asmText + "\ntimes (510-($-$$)) db 0\ndw 0xAA55\n";
        return assembleI8086(bootable, I8086OutputFormat::Bin);
    }
    // `--format=coff` is real separate compilation: an undefined symbol is deferred
    // as a relocation for a future `--link` step.
    if (format == "coff") return assembleI8086(asmText, I8086OutputFormat::Coff, /*allowExternal=*/true);
    // `--format=elf`, like `--format=raw`, is a complete self-contained program: an
    // undefined symbol is a hard error, and no boot-sector padding is appended.
    if (format == "elf") return assembleI8086(asmText, I8086OutputFormat::Elf);
    return std::nullopt;
}

}  // namespace pcc
