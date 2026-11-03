#include "MipsCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "MipsAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly (see
// tests/run_tests.sh's own qemu-system-mips64 invocation), same idea
// as M68kCodegen.cpp's own kBaseAddress.
constexpr uint32_t kBaseAddress = 0x80010000u;

// Where this backend's own entry sequence points $sp before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM.
constexpr uint32_t kInitialSP = 0x80100000u;
}  // namespace

void MipsCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element's own decimal text; a symbol element is the
// referenced Slot's or Function's own label, written out bare - "dw"
// (MipsAssembler.cpp's Line::Dw) already evaluates any expr operand,
// symbol included, and now also records the same external-name/base-
// shift relocation (kMipsRelocAbs32) a label used as an instruction's
// own absolute address already gets.
std::string MipsCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void MipsCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void MipsCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    lui " << reg << "," << lbl << "\n";
    code_ << "    ori " << reg << "," << reg << "," << lbl << "\n";
}

std::string MipsCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - MipsLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see MipsAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lui sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    ori sp,sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    jal " << label(mainFn) << "\n    nop\n";
        // The result sits in $t0 the moment execution reaches `halt` -
        // see MipsCodegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    beq zero,zero,halt\n    nop\n\n";
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
            trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) +
                                "  ; " + slot->sourceName);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> MipsCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `$ra` around its own body,
// whether or not it happens to call anything else - unlike the
// 68000's own JSR/RTS (which push/pop the real hardware stack
// automatically, so a nested call just works - see M68kCodegen.cpp's
// own comment), MIPS has exactly one link register: a non-leaf
// function's own call to another function overwrites `$ra` outright,
// so without this save/restore a caller's own eventual `jr ra` would
// return into whatever its *own* last call last returned to, not
// back to its own caller. Always saving/restoring (leaf or not) is
// simpler than tracking which functions are actually non-leaf, and
// costs only two real instructions on either side.
void MipsCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    addiu sp,sp,-4\n    stind sp,ra\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ldind sp,ra\n    addiu sp,sp,4\n";
    code_ << "    jr ra\n    nop\n\n";
}

void MipsCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq t0,zero," << (s.elseStmt ? Lelse : Ldone) << "\n    nop\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    j " << Ldone << "\n    nop\n";
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
            code_ << "    beq t0,zero," << Lend << "\n    nop\n";
            genStmt(*s.loopBody);
            code_ << "    j " << Lstart << "\n    nop\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Restore `$ra` (see genFunction()'s own comment) before
            // returning - a `return` statement is its own early exit
            // from the function, bypassing genFunction()'s own
            // fall-through epilogue entirely, so it needs this same
            // restore inline.
            code_ << "    ldind sp,ra\n    addiu sp,sp,4\n";
            code_ << "    jr ra\n    nop\n";
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
                    loadAddress("t2", lbl);
                    code_ << "    stind t2,t0\n";
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

void MipsCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            int32_t v = static_cast<int32_t>(static_cast<uint32_t>(e.intValue));
            if (v >= -32768 && v <= 32767) {
                code_ << "    addiu t0,zero," << v << "\n";
            } else {
                uint32_t uv = static_cast<uint32_t>(v);
                code_ << "    lui t0," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
                code_ << "    ori t0,t0," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadAddress("t2", label(e.slot));
            code_ << "    ldind t2,t0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // t2 = target address
                code_ << "    addiu sp,sp,-4\n    stind sp,t2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,t2\n    addiu sp,sp,4\n";
                code_ << "    stind t2,t0\n";
                return;
            }
            genExpr(*e.rhs);
            loadAddress("t2", label(e.slot));
            code_ << "    stind t2,t0\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,t2\n    addiu sp,sp,4\n";
            code_ << "    stind t2,t0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    subu t0,zero,t0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // t0 = pointer value
                code_ << "    addu t2,t0,zero\n";
                code_ << "    ldind t2,t0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    nor t0,t0,zero\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                loadAddress("t2", "p_" + e.func->sourceName + "_" + std::to_string(i));
                code_ << "    stind t2,t0\n";
            }
            code_ << "    jal " << label(e.func) << "\n    nop\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "jalr ra,t2" is the real MIPS Jump-And-Link-Register
            // instruction (funct 0x09): it jumps to whatever address
            // t2 holds and leaves the return address in ra, exactly
            // like a direct "jal label" does for a compile-time-known
            // target - same real hardware branch-delay slot as every
            // other control-transfer here (see this file's own header
            // comment), so the nop right after it is required, not
            // decorative.
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    addu t2,t0,zero\n";
            code_ << "    jalr ra,t2\n    nop\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal R10000 with no exception/interrupt setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as M68kCodegen.cpp's own
            // Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `$t2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is a compile-time-known label (an *address*);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first.
void MipsCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    code_ << "    sll t0,t0,2\n";  // t0 *= 4 (words are 4 bytes on this target)
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        loadAddress("t2", lbl);
        code_ << "    addu t2,t2,t0\n";
    } else {
        loadAddress("t1", lbl);
        code_ << "    ldind t1,t1\n";  // t1 = the pointer variable's own stored value
        code_ << "    addu t2,t1,t0\n";
    }
}

void MipsCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind t2,t0\n";
}

void MipsCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadAddress("t2", label(operand.slot));
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MipsCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "+") {
            code_ << "    addu t0,t1,t0\n";  // t0 = lhs + rhs
        } else {
            code_ << "    subu t0,t1,t0\n";  // t0 = lhs - rhs
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
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "&") {
            code_ << "    and t0,t1,t0\n";
        } else if (op == "|") {
            code_ << "    or t0,t1,t0\n";
        } else {
            code_ << "    xor t0,t1,t0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        code_ << "    addu t1,t0,zero\n";      // t1 = shift count
        code_ << "    ldind sp,t0\n    addiu sp,sp,4\n";  // t0 = value to shift
        code_ << "    " << (op == "<<" ? "sllv" : "srlv") << " t0,t0,t1\n";
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

// Real hardware multiply (`MULT`+`MFLO`) - a full 32x32->32 product,
// no operand-size limitation the way the 68000's own `MULS.W` has.
void MipsCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    mult t1,t0\n";
    code_ << "    mflo t0\n";
}

// Real hardware divide (`DIV`) - `MFLO`/`MFHI` pull the quotient and
// remainder straight back out, no packed-register unpacking needed
// the way the 68000's own `DIVS.W` needs (see M68kCodegen.cpp's own
// comment).
void MipsCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    code_ << "    div t1,t0\n";
    code_ << "    " << (wantRemainder ? "mfhi" : "mflo") << " t0\n";
}

// `SLT`/`SLTU`/`SLTIU` each directly produce the 0/1 boolean this
// language needs - no branch at all needed for any comparison
// (simpler than both the 68000's own CMP+Bcc, which needs a real
// branch to materialize the boolean, and the 6502's N-xor-V fixup -
// see their own comments).
void MipsCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
    if (op == "<") {
        code_ << "    slt t0,t1,t0\n";  // t0 = (lhs < rhs)
        return;
    }
    if (op == ">") {
        code_ << "    slt t0,t0,t1\n";  // t0 = (rhs < lhs) = (lhs > rhs)
        return;
    }
    if (op == "<=") {
        code_ << "    slt at,t0,t1\n";                        // at = (rhs < lhs)
        code_ << "    addiu t0,zero,1\n    xor t0,t0,at\n";  // t0 = 1 ^ (rhs<lhs) = !(rhs<lhs) = (lhs<=rhs)
        return;
    }
    if (op == ">=") {
        code_ << "    slt at,t1,t0\n";   // at = (lhs < rhs)
        code_ << "    addiu t0,zero,1\n    xor t0,t0,at\n";  // t0 = !(lhs<rhs) = (lhs>=rhs)
        return;
    }
    if (op == "==") {
        code_ << "    subu at,t1,t0\n";    // at = lhs - rhs
        code_ << "    sltiu t0,at,1\n";    // t0 = (unsigned(at) < 1) = (at == 0)
        return;
    }
    if (op == "!=") {
        code_ << "    subu at,t1,t0\n";    // at = lhs - rhs
        code_ << "    sltu t0,zero,at\n";  // t0 = (0 < unsigned(at)) = (at != 0)
        return;
    }
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void MipsCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq t0,zero," << Lfalse << "\n    nop\n";
    genExpr(rhs);
    code_ << "    beq t0,zero," << Lfalse << "\n    nop\n";
    code_ << "    j " << Ltrue << "\n    nop\n";
    code_ << Lfalse << ":\n    addiu t0,zero,0\n    j " << Ldone << "\n    nop\n";
    code_ << Ltrue << ":\n    addiu t0,zero,1\n";
    code_ << Ldone << ":\n";
}

void MipsCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    bne t0,zero," << Ltrue << "\n    nop\n";
    genExpr(rhs);
    code_ << "    bne t0,zero," << Ltrue << "\n    nop\n";
    code_ << "    j " << Lfalse << "\n    nop\n";
    code_ << Ltrue << ":\n    addiu t0,zero,1\n    j " << Ldone << "\n    nop\n";
    code_ << Lfalse << ":\n    addiu t0,zero,0\n";
    code_ << Ldone << ":\n";
}

// `SLTIU t0,t0,1` is exactly "t0 = (unsigned(t0) < 1)" = "t0 == 0" -
// logical not, in one real instruction, no branch needed.
void MipsCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    sltiu t0,t0,1\n";
}

std::string MipsCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the r10000 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MipsCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleMips(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see MipsAssembler.h's own `assembleMipsObject`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMipsObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
