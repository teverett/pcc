#include "I386Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "I386Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where this backend's own image is loaded and runs - the
// conventional Multiboot load address (comfortably above the BIOS/
// real-mode data area, with no need to avoid stepping on anything
// below 1MB the way a real-mode target has to).
constexpr uint32_t kLoadAddr = 0x100000;

// A real Multiboot header (see the Multiboot Specification's "OS
// image format") - the standard, real-bootloader-agnostic contract a
// 32-bit protected-mode kernel image uses to tell whatever loaded it
// (GRUB, or here, `qemu-system-i386 -kernel` acting as one - see
// tests/run_tests.sh's own check_i386()) where to load it and where
// to start running, *without* this backend ever having to write any
// code of its own to get from real mode into protected mode: the
// loader does that part entirely, before this image's first
// instruction ever runs - confirmed directly by booting exactly this
// header (reading `CS`/`EAX` over qemu's own monitor) rather than
// assumed from the spec alone. `flags` has only the "load this
// image's raw bytes at a fixed physical address" bit (0x10000, the
// "a.out kludge" - no symbol table to omit, no alignment/memory-map
// request needed for a program this simple); `header_addr`/
// `load_addr` are both `kLoadAddr` (the header is the very first
// thing in the image); `load_end_addr`/`bss_end_addr` of 0 both mean
// "load the whole file, no separate zero-filled bss"; `entry_addr`
// is `_start`, right after this 32-byte header.
constexpr uint32_t kMbMagic = 0x1BADB002;
constexpr uint32_t kMbFlags = 0x10000;
constexpr uint32_t kMbChecksum = static_cast<uint32_t>(-(static_cast<int64_t>(kMbMagic) + kMbFlags));
}  // namespace

void I386Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dd 0"); }

void I386Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    std::ostringstream line;
    line << lbl << ": dd ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << values[i];
    }
    trailer_.push_back(line.str());
}

std::string I386Codegen::generate(const ir::Program &prog, bool objectMode) {
    if (objectMode) {
        err(0,
            "the i386 backend doesn't support real separate compilation yet - only a single self-contained "
            "--format=bin compile (see I386Codegen.h's own header comment)");
    }
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless `main` exists whenever
    // allowExternal is false (always true here - see above), so
    // mainFn is never null.

    code_ << "org " << kLoadAddr << "\n\n";
    code_ << "dd " << kMbMagic << "," << kMbFlags << "," << kMbChecksum << "\n";
    code_ << "dd " << kLoadAddr << "," << kLoadAddr << ",0,0\n";
    code_ << "dd _start\n\n";
    code_ << "_start:\n";
    // Masks both real 8259 PICs - confirmed directly that without
    // this, a program calling `_ion()` (this file's own `sti`, in
    // genExpr() below) triple-faults the moment a real hardware timer
    // tick arrives, since this backend never builds a real IDT for
    // one to vector through (same "real but nothing to usefully do"
    // stance as M68kCodegen.cpp's own `_ion()`/`_iof()` comment -
    // masking the PICs is what makes that stance actually safe here,
    // not just notionally true).
    code_ << "    mov al,0xFF\n    out 0x21,al\n";
    code_ << "    mov al,0xFF\n    out 0xA1,al\n";
    code_ << "    call " << label(mainFn) << "\n";
    // The result sits in eax the moment execution reaches `halt` -
    // see I386Codegen.h's own header comment on why that's the
    // authoritative place to read it from (a real, directly
    // inspectable 32-bit register).
    code_ << "    hlt\n";
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
            trailer_.push_back(lbl + ": dd " + std::to_string(v) + "  ; " + slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> I386Codegen::perFileAsm() const { return perFileAsm_; }

void I386Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ret\n\n";
}

void I386Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp eax,0\n    je " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            code_ << "    cmp eax,0\n    je " << Lend << "\n";
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
                    code_ << "    mov [" << lbl << "],eax\n";
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

void I386Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    mov eax," << e.intValue << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    mov eax,[" << label(e.slot) << "]\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                std::string addr = computeElementAddr(e.slot, *e.lhs);
                code_ << "    push ebx\n";
                genExpr(*e.rhs);
                code_ << "    pop ebx\n";
                code_ << "    mov " << addr << ",eax\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    mov [" << label(e.slot) << "],eax\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // eax = pointer value
            code_ << "    mov ebx,eax\n    push ebx\n";
            genExpr(*e.rhs);  // eax = value to store
            code_ << "    pop ebx\n";
            code_ << "    mov [ebx],eax\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg eax\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // eax = pointer value
                code_ << "    mov ebx,eax\n    mov eax,[ebx]\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not eax\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "    mov [p_" << e.func->sourceName << "_" << i << "],eax\n";
            }
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real i386 interrupt enable/disable - same role as
            // I8086Codegen.cpp's own `_ion()`/`_iof()` comment (that
            // file's own entry sequence runs with interrupts off from
            // `cli` until its own `sti`; this backend's stage-1 boot
            // never re-enables them at all before handing off to
            // stage-2, so `_ion()`/`_iof()` are the first real `sti`/
            // `cli` either way).
            code_ << "    " << (e.op == "ion" ? "sti" : "cli") << "\n";
            return;
        }
    }
}

// slot[idxExpr]'s effective-address operand text, e.g.
// "[g_arr3+ebx]", with ebx already loaded with idx*4 (dwords are 4
// bytes on this target). See I8086Codegen.cpp's own
// `computeElementAddr` - identical role and reasoning, just 32 bits
// throughout (`shl eax,2` instead of `shl ax,1`).
std::string I386Codegen::computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    std::string lbl = label(slot);
    genExpr(idxExpr);
    code_ << "    shl eax,2\n";
    if (slot->arraySize > 0) {
        code_ << "    mov ebx,eax\n";
        return "[" + lbl + "+ebx]";
    }
    code_ << "    add eax,[" << lbl << "]\n";
    code_ << "    mov ebx,eax\n";
    return "[ebx]";
}

void I386Codegen::genIndex(const ir::Expr &e) {
    std::string addr = computeElementAddr(e.slot, *e.lhs);
    code_ << "    mov eax," << addr << "\n";
}

void I386Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    mov eax," << label(operand.slot) << "\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        std::string lbl = label(operand.slot);
        genExpr(*operand.lhs);
        code_ << "    shl eax,2\n";
        if (operand.slot->arraySize > 0) {
            code_ << "    add eax," << lbl << "\n";
        } else {
            code_ << "    add eax,[" << lbl << "]\n";
        }
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void I386Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    pop ebx\n";
        if (op == "+") {
            code_ << "    add eax,ebx\n";
        } else {
            code_ << "    sub ebx,eax\n    mov eax,ebx\n";
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
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    pop ebx\n";
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        code_ << "    " << insn << " eax,ebx\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    push eax\n";
        genExpr(*e.rhs);
        code_ << "    mov ecx,eax\n";
        code_ << "    pop eax\n";
        code_ << "    " << (op == "<<" ? "shl" : "shr") << " eax,cl\n";
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

void I386Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "je");
    if (op == "!=") return emitCompare(lhs, rhs, "jne");
    if (op == "<") return emitCompare(lhs, rhs, "jl");
    if (op == "<=") return emitCompare(lhs, rhs, "jle");
    if (op == ">") return emitCompare(lhs, rhs, "jg");
    if (op == ">=") return emitCompare(lhs, rhs, "jge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// cmp ebx,eax compares lhs-rhs (ebx=lhs, eax=rhs after the pop below)
// - see I8086Codegen.cpp's own `emitCompare` comment, unchanged here.
void I386Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue) {
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    pop ebx\n";
    code_ << "    cmp ebx,eax\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jumpIfTrue << " " << Ltrue << "\n";
    code_ << "    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    je " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp eax,0\n    je " << Lfalse << "\n";
    code_ << "    jmp " << Ltrue << "\n";
    code_ << Lfalse << ":\n    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    jne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp eax,0\n    jne " << Ltrue << "\n";
    code_ << "    jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    mov eax,0\n";
    code_ << Ldone << ":\n";
}

void I386Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp eax,0\n    je " << Ltrue << "\n";
    code_ << "    mov eax,0\n    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n    mov eax,1\n";
    code_ << Ldone << ":\n";
}

// Real hardware multiply/divide (imul/idiv) - a genuine 32x32->32
// product/quotient, same real-hardware story as
// I8086Codegen.cpp's own `imul`/`idiv` comment, just 32 bits wide
// (`cdq` sign-extends eax into edx:eax, same role as that file's own
// `cwd`).
void I386Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    pop ebx\n";
    code_ << "    imul ebx\n";  // edx:eax = eax*ebx; keep eax
}

void I386Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push eax\n";
    genExpr(rhs);
    code_ << "    mov ebx,eax\n";  // ebx = divisor
    code_ << "    pop eax\n";      // eax = dividend
    code_ << "    cdq\n";          // sign-extend eax into edx:eax
    code_ << "    idiv ebx\n";     // eax = quotient, edx = remainder
    if (wantRemainder) code_ << "    mov eax,edx\n";
}

std::string I386Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the i386 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I386Codegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "bin") return assembleI386(asmText);
    return std::nullopt;
}

}  // namespace pcc
