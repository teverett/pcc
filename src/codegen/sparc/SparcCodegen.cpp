#include "SparcCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "SparcAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly, same idea as
// PowerPCCodegen.cpp's own kBaseAddress. Must match
// SparcAssembler.h's own `kSparcBaseAddress` exactly (duplicated
// there, same convention as every other migrated backend - see that
// file's own comment).
constexpr uint32_t kBaseAddress = 0x00004000u;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM.
constexpr uint32_t kInitialSP = 0x00100000u;
}  // namespace

void SparcCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

void SparcCodegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << static_cast<int32_t>(static_cast<uint32_t>(values[i]));
    }
    trailer_.push_back(line.str());
}

void SparcCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    sethi " << lbl << "," << reg << "\n";
    code_ << "    or " << reg << "," << lbl << "," << reg << "\n";
}

void SparcCodegen::movReg(const std::string &dst, const std::string &src) {
    code_ << "    or g0," << src << "," << dst << "\n";
}

std::string SparcCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - SparcLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see SparcAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        uint32_t hi = (kInitialSP >> 10) & 0x3FFFFFu;
        uint32_t lo = kInitialSP & 0x3FFu;
        code_ << "    sethi " << hi << ",sp\n";
        code_ << "    or sp," << lo << ",sp\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The result sits in t0 the moment execution reaches `halt` -
        // see SparcCodegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    ba halt\n\n";
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

// Single-file-only - see PowerPCCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> SparcCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores the real return-address register
// `lr` (`%o7`, the register SPARC's own `call` instruction itself
// writes into - directly readable/writable, unlike PowerPC's own link
// register - see SparcCodegen.h's own header comment) around its own
// body, whether or not it happens to call anything else - a non-leaf
// function's own call to another function overwrites `lr` outright,
// so without this save/restore a caller's own eventual `retl` would
// return into whatever its *own* last call last returned to, not back
// to its own caller. Always saving/restoring (leaf or not) is simpler
// than tracking which functions are actually non-leaf - same
// reasoning as PowerPCCodegen.cpp's own version of this same comment.
void SparcCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,4,sp\n    stind sp,lr\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ldind sp,lr\n    add sp,4,sp\n";
    code_ << "    retl\n\n";
}

void SparcCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cmp t0,0\n";
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    ba " << Ldone << "\n";
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
            code_ << "    cmp t0,0\n";
            code_ << "    beq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    ba " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Restore `lr` (see genFunction()'s own comment) before
            // returning - a `return` statement is its own early exit
            // from the function, bypassing genFunction()'s own
            // fall-through epilogue entirely, so it needs this same
            // restore inline.
            code_ << "    ldind sp,lr\n    add sp,4,sp\n";
            code_ << "    retl\n";
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

void SparcCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            int32_t v = static_cast<int32_t>(static_cast<uint32_t>(e.intValue));
            if (v >= -4096 && v <= 4095) {
                code_ << "    or g0," << v << ",t0\n";
            } else {
                uint32_t uv = static_cast<uint32_t>(v);
                uint32_t hi = (uv >> 10) & 0x3FFFFFu;
                uint32_t lo = uv & 0x3FFu;
                code_ << "    sethi " << hi << ",t0\n";
                code_ << "    or t0," << lo << ",t0\n";
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
                code_ << "    sub sp,4,sp\n    stind sp,t2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,t2\n    add sp,4,sp\n";
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
            code_ << "    sub sp,4,sp\n    stind sp,t0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,t2\n    add sp,4,sp\n";
            code_ << "    stind t2,t0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    sub g0,t0,t0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // t0 = pointer value
                movReg("t2", "t0");
                code_ << "    ldind t2,t0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    xor t0,-1,t0\n";
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
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal target with no trap table set up at all
            // here - treated as a no-op, same reasoning (and the same
            // choice) as PowerPCCodegen.cpp's own Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `t2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is a compile-time-known label (an *address*);
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first.
void SparcCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);              // t0 = index
    code_ << "    sll t0,2,t0\n";  // t0 *= 4 (words are 4 bytes on this target)
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        loadAddress("t2", lbl);
        code_ << "    add t2,t0,t2\n";
    } else {
        loadAddress("t1", lbl);
        code_ << "    ldind t1,t1\n";       // t1 = the pointer variable's own stored value
        code_ << "    add t1,t0,t2\n";
    }
}

void SparcCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind t2,t0\n";
}

void SparcCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadAddress("t2", label(operand.slot));
        movReg("t0", "t2");
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        movReg("t0", "t2");
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void SparcCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    sub sp,4,sp\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
        if (op == "+") {
            code_ << "    add t1,t0,t0\n";  // t0 = lhs + rhs
        } else {
            code_ << "    sub t1,t0,t0\n";  // t0 = lhs - rhs
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
        code_ << "    sub sp,4,sp\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
        if (op == "&") {
            code_ << "    and t1,t0,t0\n";
        } else if (op == "|") {
            code_ << "    or t1,t0,t0\n";
        } else {
            code_ << "    xor t1,t0,t0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    sub sp,4,sp\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        movReg("t1", "t0");
        code_ << "    ldind sp,t0\n    add sp,4,sp\n";  // t0 = value to shift
        code_ << "    " << (op == "<<" ? "sll" : "srl") << " t0,t1,t0\n";
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

// Real hardware multiply (`smul`) - a full 32x32->32 product, no
// operand-size limitation the way the 68000's own `MULS.W` has.
void SparcCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
    code_ << "    smul t1,t0,t0\n";
}

// Real hardware divide (`sdiv`) gives only the quotient, and (like
// every SPARC V8 32-bit divide) treats the dividend as a 64-bit
// `Y:rs1` pair under the hood - `sra t1,31,t2` then `wry t2,g0` seeds
// Y with the dividend's own sign bits first (real SPARC's own
// standard idiom for a 32-bit signed divide; skipping this gives a
// silently wrong result whenever the dividend is negative). There is
// no remainder instruction at all (same situation as PowerPC's own
// classic `divw` - see PowerPCCodegen.h's own header comment), so `%`
// is synthesized the standard way: `sdiv` for the quotient, `smul` to
// multiply the quotient back out by the divisor, then `sub` to
// recover the remainder as `dividend - quotient * divisor`.
void SparcCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    code_ << "    sra t1,31,t2\n    wry t2,g0\n";
    if (!wantRemainder) {
        code_ << "    sdiv t1,t0,t0\n";
        return;
    }
    code_ << "    sdiv t1,t0,t2\n";   // t2 = quotient (t1/t0)
    code_ << "    smul t2,t0,t2\n";   // t2 = quotient * divisor
    code_ << "    sub t1,t2,t0\n";    // t0 = t1 - t2 = dividend - quotient*divisor
}

// SPARC has no single compare-and-set instruction - a comparison here
// always needs `cmp` (real `subcc rs1,rs2,%g0`) plus a real
// conditional branch to decide which boolean value to materialize,
// the same "compare, then branch to decide" shape as PowerPC's own
// `cmpw`+`bc` (see PowerPCCodegen.h's own header comment).
void SparcCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
    code_ << "    cmp t1,t0\n";
    std::string branch;
    if (op == "<") branch = "blt";
    else if (op == ">") branch = "bgt";
    else if (op == "<=") branch = "ble";
    else if (op == ">=") branch = "bge";
    else if (op == "==") branch = "beq";
    else if (op == "!=") branch = "bne";
    else err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branch << " " << Ltrue << "\n";
    code_ << "    or g0,0,t0\n";
    code_ << "    ba " << Ldone << "\n";
    code_ << Ltrue << ":\n    or g0,1,t0\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmp t0,0\n    beq " << Lfalse << "\n";
    code_ << "    ba " << Ltrue << "\n";
    code_ << Lfalse << ":\n    or g0,0,t0\n    ba " << Ldone << "\n";
    code_ << Ltrue << ":\n    or g0,1,t0\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmp t0,0\n    bne " << Ltrue << "\n";
    code_ << "    ba " << Lfalse << "\n";
    code_ << Ltrue << ":\n    or g0,1,t0\n    ba " << Ldone << "\n";
    code_ << Lfalse << ":\n    or g0,0,t0\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    bne " << Lfalse << "\n";
    code_ << "    or g0,1,t0\n    ba " << Ldone << "\n";
    code_ << Lfalse << ":\n    or g0,0,t0\n";
    code_ << Ldone << ":\n";
}

std::string SparcCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                  const std::string &) const {
    throw std::runtime_error("the sparc backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports");
}

std::optional<std::string> SparcCodegen::assembleInProcess(const std::string &asmText,
                                                              const std::string &format) const {
    if (format == "raw") return assembleSparc(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see SparcAssembler.h's own `assembleSparcObject`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSparcObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
