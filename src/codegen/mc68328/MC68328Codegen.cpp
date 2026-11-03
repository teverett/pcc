#include "MC68328Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "MC68328Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where A7 (the real hardware stack pointer) starts out, loaded by
// the CPU's own reset exception from vector 0 (see MC68328Codegen.h's
// own header comment) - comfortably above the loaded program and its
// data, growing down into otherwise-unused RAM (tests/run_tests.sh
// gives the simulator 16MB, the same convention as ../m68k's own
// `kInitialSP`).
constexpr uint32_t kInitialSP = 0x100000;

// Where `_start` lives - right after the two-vector reset table this
// backend lays out at address 0 (see MC68328Codegen.h's own header
// comment), with no gap: MC68328Assembler.cpp's own two-pass `org`
// only ever affects how a *label* resolves, never where a byte
// physically lands in its own output buffer (bytes are always
// emitted back-to-back, in program order), so this backend never
// calls `org` a second time to "jump" forward - doing that here would
// silently desynchronize the labels this code relies on (`_start`,
// every `g_`/`fn_` label) from where their own bytes actually end up
// once `--format=raw`'s flat image is loaded at address 0.
constexpr uint32_t kStartAddress = 8;
}  // namespace

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dl`'s own expression evaluator
// (MC68328Assembler.cpp's Line::Dl) already resolves a bare symbol
// the same way `leal label,a0` already does for an address held in a
// register (see genAddrOf).
std::string MC68328Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void MC68328Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dl ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

std::string MC68328Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever lays out the real reset
    // vector table or entry sequence itself, even the one that
    // happens to define `main` - MC68328LinkerFactory.cpp is solely
    // responsible for synthesizing exactly one (vector table
    // included), once every object is merged and `main`'s linked
    // address is resolvable (see Linker.h) - same pattern
    // ../m68k/M68kCodegen.cpp's own `objectMode` comment documents.
    // An object file's own base is `org 0` - it's the linker's job
    // to place it, not this backend's (see MC68328Assembler.h's own
    // header comment).
    code_ << "org 0\n";
    if (!objectMode) {
        // The real MC68328 reset vector table (see MC68328Codegen.h's
        // own header comment): vector 0 (initial SSP), vector 1
        // (initial PC). The CPU's own reset exception loads A7 and
        // the program counter from these two long words directly -
        // no `movel ...,a7` instruction is needed at `_start` the way
        // ../m68k's own simulator-PC-forced entry needs one.
        code_ << "    dl " << kInitialSP << "\n";
        code_ << "    dl " << kStartAddress << "\n";
        code_ << "_start:\n";
        // Belt-and-suspenders: real hardware already loads A7 from
        // vector 0 as part of taking the reset exception itself,
        // before fetching a single instruction - but this explicit
        // reload costs nothing and makes the image correct even
        // under a reset-vector-table implementation that (like
        // qemu-system-m68k's own `-M none` board, confirmed
        // directly: it fetches PC from vector 1 but leaves A7 at its
        // own arbitrary pre-reset value rather than loading it from
        // vector 0 too) only honors the PC half of the real reset
        // dance.
        code_ << "    movel " << kInitialSP << ",a7\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The result sits in D0 the moment execution reaches `halt` -
        // see M68kCodegen.h's own header comment on why that's the
        // authoritative place to read it from (a real, directly
        // inspectable 32-bit register).
        code_ << "halt:\n    bra halt\n\n";
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
            declareArray(lbl, slot->arrayInit);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            trailer_.push_back(lbl + ": dl " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) +
                                "  ; " + slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> MC68328Codegen::perFileAsm() const { return perFileAsm_; }

void MC68328Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    // Real per-call frame (see MC68328Codegen.h's own header comment):
    // `linkl a6,-N` pushes the caller's `a6`, points this call's own
    // `a6` at that saved value, and reserves N bytes below it for
    // this function's own Locals (frameSize is already word count -
    // see IR.h's own Function::frameSize - so bytes is *4).
    code_ << "    linkl a6," << (-4 * fn.func->frameSize) << "\n";

    genStmt(*fn.body);

    code_ << "    unlkl a6\n";
    code_ << "    rts\n\n";
}

void MC68328Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmpl 0,d0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    bra " << Ldone << "\n";
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
            code_ << "    cmpl 0,d0\n    beq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    bra " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "    unlkl a6\n";
            code_ << "    rts\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                code_ << "    movel d0," << addr(s.declSlot) << "\n";
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

void MC68328Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",d0\n";
            return;
        }
        case ir::ExprKind::Var: {
            code_ << "    movel " << operandOf(e.slot) << ",d0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                code_ << "    pushl a0\n";     // a0 is really pushed via d0's own slot width - see pushl's own note
                genExpr(*e.rhs);
                code_ << "    popl a0\n";
                code_ << "    stind d0,a0\n";
                return;
            }
            genExpr(*e.rhs);
            code_ << "    movel d0," << operandOf(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // d0 = pointer value
            code_ << "    movel d0,a0\n";
            code_ << "    pushl a0\n";
            genExpr(*e.rhs);
            code_ << "    popl a0\n";
            code_ << "    stind d0,a0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    negl d0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // d0 = pointer value
                code_ << "    movel d0,a0\n";
                code_ << "    ldind a0,d0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    notl d0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Real stack-passed arguments, pushed right-to-left so
            // param 0 ends up closest to the return address JSR is
            // about to push (see MC68328Codegen.h's own `addr()`
            // comment) - each call's own arguments land in that
            // call's own frame this way, never in a shared per-
            // function cell. Caller cleans up its own pushed
            // arguments afterwards (`adda <N>,a7`), same cdecl-style
            // convention as ../i386/I386Codegen.cpp's own identical
            // `Call` case.
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    pushl d0\n";
            }
            code_ << "    jsr " << label(e.func) << "\n";
            if (!e.args.empty()) code_ << "    adda " << (4 * e.args.size()) << ",a7\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "jsrind a0" is MC68328Assembler.cpp's own (An)-indirect
            // JSR encoding (mode 0b010) - a real near call through
            // whatever address a0 holds.
            genExpr(*e.lhs);  // d0 = the callee's own entry address
            code_ << "    movel d0,a0\n";
            code_ << "    jsrind a0\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable
            // here either (see M68kCodegen.cpp's own version of this
            // comment) - the MC68328 does have real maskable
            // interrupts (its own interrupt controller SFRs), but
            // this bare, no-vector-table compile target never sets
            // any of that up, so there's nothing for `ion`/`iof` to
            // meaningfully toggle. Treated as a no-op, same as the
            // generic 68000 backend.
            (void)e;
            return;
        }
    }
}

// See MC68328Codegen.h's own header comment: `A0` holds the computed
// address. `slot->arraySize > 0` means the base is a compile-time-
// known address (a Global's own label, loaded via `leal`, or a
// Local's own frame-relative storage - see addr()); a Global array's
// elements sit at increasing addresses as the index increases, but a
// Local array's own elements sit at *decreasing* addresses (addr(slot)
// is the array's first word, closest to `a6`; later words are further
// below it - see ../m68k/M68kCodegen.cpp's own identical comment), so
// a Local array indexes base-minus-scaled-index, not base-plus
// (`suba` instead of `adda`). Otherwise `slot` is a plain pointer
// variable, so the base instead comes from *reading* that variable's
// own stored value first.
void MC68328Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // d0 = index
    // d0 *= 4 (words are 4 bytes on this target); done as two
    // doublings rather than a shift, needing no extra register at
    // all.
    code_ << "    addl d0,d0\n    addl d0,d0\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    leal " << label(slot) << ",a0\n";
            code_ << "    adda d0,a0\n";
        } else {
            code_ << "    leal " << addr(slot) << ",a0\n";
            code_ << "    suba d0,a0\n";
        }
    } else {
        code_ << "    movel " << operandOf(slot) << ",a0\n";
        code_ << "    adda d0,a0\n";
    }
}

// Stores each of a Local array's own initializer values directly into
// its own frame slots (no static trailer storage - unlike a Global
// array's `declareArray()`, a Local's storage is this call's own
// frame, freshly reserved by `linkl` every time this function runs).
void MC68328Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        code_ << "    movel " << arrayElemText(slot->arrayInit[i]) << ",[a6-"
              << (4 * (slot->frameOffset + static_cast<int>(i))) << "]\n";
    }
}

void MC68328Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind a0,d0\n";
}

void MC68328Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "    leal " << operandOf(operand.slot) << ",a0\n";
        code_ << "    movel a0,d0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    movel a0,d0\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    leal " << label(operand.func) << ",a0\n";
        code_ << "    movel a0,d0\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MC68328Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
        if (op == "+") {
            code_ << "    addl d1,d0\n";  // d0 = d0 + d1 = rhs + lhs
        } else {
            code_ << "    subl d0,d1\n";  // d1 = d1 - d0 = lhs - rhs
            code_ << "    movel d1,d0\n";
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
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
        if (op == "&") {
            code_ << "    andl d1,d0\n";
        } else if (op == "|") {
            code_ << "    orl d1,d0\n";
        } else {
            code_ << "    eorl d1,d0\n";  // d0 = d0 xor d1 (EOR's own source/dest convention - see
                                            // MC68328Assembler.cpp's own comment)
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    movel d0,d1\n";  // d1 = shift count
        code_ << "    popl d0\n";      // d0 = value to shift
        code_ << "    " << (op == "<<" ? "lsll" : "lsrl") << " d1,d0\n";
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

// Real hardware multiply (`MULS.W`) - see M68kCodegen.h's own header
// comment on its own 16-bit-operand limitation (shared by this core
// unchanged - see MC68328Codegen.h's own header comment).
void MC68328Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";       // d1 = lhs, d0 = rhs
    code_ << "    mulsw d1,d0\n";  // d0 = d0 * d1 = rhs * lhs
}

// Real hardware divide (`DIVS.W`) - see M68kCodegen.cpp's own version
// of this comment.
void MC68328Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs (dividend), d0 = rhs (divisor)
    code_ << "    divsw d0,d1\n";  // d1 = quotient (low word) : remainder (high word)
    if (wantRemainder) {
        code_ << "    swapl d1\n";  // remainder now in the low word
    }
    code_ << "    movel d1,d0\n";
    code_ << "    extl d0\n";  // sign-extend whichever half is now in the low word
}

void MC68328Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, "blt");
    if (op == "<=") return emitCompare(lhs, rhs, "ble");
    if (op == ">") return emitCompare(lhs, rhs, "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.L <ea>,Dn` sets the real signed condition codes for `Dn - ea`
// directly in hardware - `branchIfTrue` is simply applied, no sign/
// overflow fixup needed.
void MC68328Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
    code_ << "    cmpl d0,d1\n";  // flags = d1 - d0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

void MC68328Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    code_ << "    bra " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

void MC68328Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    code_ << "    bra " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n    bra " << Ldone << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n";
    code_ << Ldone << ":\n";
}

void MC68328Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmpl 0,d0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

std::string MC68328Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                   const std::string &) const {
    throw std::runtime_error("the mc68328 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MC68328Codegen::assembleInProcess(const std::string &asmText,
                                                               const std::string &format) const {
    if (format == "raw") return assembleMC68328(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see MC68328Assembler.h's own `assembleMC68328Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMC68328Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
