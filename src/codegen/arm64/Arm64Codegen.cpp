#include "Arm64Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Arm64Assembler.h"
#include "objectformat/ObjectFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing. qemu's own
// `-kernel` raw-image convention for the `virt` machine's AArch64
// boot (confirmed directly: no ELF header, no Linux `Image` magic -
// just a flat binary) loads it at RAM base + 0x80000 and sets the
// CPU's PC there directly, no vector table or `loader`-device PC-
// forcing trick needed at all (unlike M68kCodegen.cpp's own
// kBaseAddress) - see tests/run_tests.sh's own qemu-system-aarch64
// invocation. RAM base on `virt` is a fixed, documented 0x40000000.
constexpr uint64_t kBaseAddress = 0x40080000ull;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM (the test harness gives the
// guest 256M, so this is nowhere near the top).
constexpr uint64_t kInitialSP = 0x44000000ull;
}  // namespace

void Arm64Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

void Arm64Codegen::declareArray(const std::string &lbl, const std::vector<long> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << static_cast<int32_t>(static_cast<uint32_t>(values[i]));
    }
    trailer_.push_back(line.str());
}

void Arm64Codegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movz " << reg << "," << lbl << "\n";
    code_ << "    movk " << reg << "," << lbl << "\n";
}

std::string Arm64Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/`aout`
    // from a single file - see IRBuilder.h's `allowExternal`) means
    // `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - Arm64LinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see Arm64Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movz x9," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    movk x9," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    add sp,x9,0\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in w0 the moment execution reaches `halt` -
        // see Arm64Codegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    b halt\n\n";
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

// Single-file-only - see M68kCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> Arm64Codegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `x30` (the link register) around
// its own body, whether or not it happens to call anything else -
// `bl` sets `x30` to its own return address unconditionally, so a
// non-leaf function's own call to another function overwrites it; see
// MipsCodegen.cpp's own identical comment on `$ra` for the full
// reasoning (A64's `x30` plays exactly the same role MIPS's `$ra`
// does). The save/restore here only ever moves `x30`'s own low 32
// bits through `stind`/`ldind` (this backend's 32-bit-transfer-only
// load/store - see Arm64Codegen.h's own header comment) - safe only
// because every address in this backend's own universe (the program's
// own base/stack addresses, both fixed well under 4GB - see this
// file's own kBaseAddress/kInitialSP) fits in 32 bits, so the
// reload's automatic zero-extension (see Arm64Codegen.h's own header
// comment on why every 32-bit op here already guarantees this)
// reconstructs the exact original 64-bit return address.
void Arm64Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,sp,16\n    stind sp,w30\n";
    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);
    code_ << "    ldind sp,w30\n    add sp,sp,16\n";
    code_ << "    ret\n\n";
}

void Arm64Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cbz w0," << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    b " << Ldone << "\n";
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
            code_ << "    cbz w0," << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Restore `x30` (see genFunction()'s own comment) before
            // returning - a `return` statement is its own early exit
            // from the function, bypassing genFunction()'s own
            // fall-through epilogue entirely, so it needs this same
            // restore inline.
            code_ << "    ldind sp,w30\n    add sp,sp,16\n";
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
                    loadAddress("x2", lbl);
                    code_ << "    stind x2,w0\n";
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

void Arm64Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint32_t uv = static_cast<uint32_t>(e.intValue);
            if (uv <= 0xFFFFu) {
                code_ << "    movz w0," << static_cast<int32_t>(uv) << "\n";
            } else {
                code_ << "    movz w0," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
                code_ << "    movk w0," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadAddress("x2", label(e.slot));
            code_ << "    ldind x2,w0\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // x2 = target address
                code_ << "    sub sp,sp,16\n    stind sp,w2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,w2\n    add sp,sp,16\n";
                code_ << "    stind x2,w0\n";
                return;
            }
            genExpr(*e.rhs);
            loadAddress("x2", label(e.slot));
            code_ << "    stind x2,w0\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // w0 = pointer value
            code_ << "    sub sp,sp,16\n    stind sp,w0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,w2\n    add sp,sp,16\n";
            code_ << "    stind x2,w0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    sub w0,wzr,w0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // w0 = pointer value
                code_ << "    mov x2,x0\n";
                code_ << "    ldind x2,w0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn w0,w0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                loadAddress("x2", "p_" + e.func->sourceName + "_" + std::to_string(i));
                code_ << "    stind x2,w0\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal A64 core with no exception/interrupt setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as M68kCodegen.cpp's own/
            // MipsCodegen.cpp's own Intrinsic case.
            (void)e;
            return;
        }
    }
}

// `x2` holds the computed address, no zero-page-style placement
// restriction. `arraySize > 0` means the base is a compile-time-known
// label (an *address*); otherwise `slot` is a plain pointer variable,
// so the base instead comes from *reading* that variable's own stored
// value first.
//
// Multiplying the index by 4 (this target's own word size) uses a
// real `mul` against a loaded constant rather than a shift, since
// this dialect has no dedicated shift-by-immediate mnemonic at all
// (unlike MIPS's own `sll`) - `lslv`/`lsrv` (variable, register-form
// shift) cover every *language*-level `<<`/`>>` this backend ever
// needs, and a fixed `mul` by 4 is no less "real hardware" for this
// one compile-time-known case.
void Arm64Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // w0 = index
    code_ << "    movz w1,4\n    mul w0,w0,w1\n";  // w0 *= 4
    std::string lbl = label(slot);
    if (slot->arraySize > 0) {
        loadAddress("x2", lbl);
        // `x0`'s own upper 32 bits are already zero (the `mul` above
        // is a 32-bit "W-form" op - see Arm64Codegen.h's own header
        // comment on why that's an architectural guarantee, not an
        // assumption), so `x0` is already a valid 64-bit operand here
        // with no explicit zero-extension needed.
        code_ << "    add x2,x2,x0\n";
    } else {
        loadAddress("x1", lbl);
        code_ << "    ldind x1,w1\n";  // w1 = the pointer variable's own stored value (x1 zero-extends too)
        code_ << "    add x2,x1,x0\n";
    }
}

void Arm64Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind x2,w0\n";
}

void Arm64Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadAddress("x2", label(operand.slot));
        code_ << "    mov x0,x2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov x0,x2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Arm64Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,16\n    stind sp,w0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,w1\n    add sp,sp,16\n";  // w1 = lhs, w0 = rhs
        if (op == "+") {
            code_ << "    add w0,w1,w0\n";
        } else {
            code_ << "    sub w0,w1,w0\n";
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
        code_ << "    sub sp,sp,16\n    stind sp,w0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,w1\n    add sp,sp,16\n";  // w1 = lhs, w0 = rhs
        if (op == "&") {
            code_ << "    and w0,w1,w0\n";
        } else if (op == "|") {
            code_ << "    orr w0,w1,w0\n";
        } else {
            code_ << "    eor w0,w1,w0\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,16\n    stind sp,w0\n";
        genExpr(*e.rhs);  // w0 = shift count
        code_ << "    mov w1,w0\n";                        // w1 = shift count
        code_ << "    ldind sp,w0\n    add sp,sp,16\n";  // w0 = value to shift
        code_ << "    " << (op == "<<" ? "lslv" : "lsrv") << " w0,w0,w1\n";
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

// Real hardware multiply (`mul`, a full 32x32->32 product) - no
// operand-size limitation the way 68000's own `MULS.W` has.
void Arm64Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp,w0\n";
    genExpr(rhs);
    code_ << "    ldind sp,w1\n    add sp,sp,16\n";  // w1 = lhs, w0 = rhs
    code_ << "    mul w0,w1,w0\n";
}

// Real hardware divide (`sdiv`) plus `msub` (`rd = ra - rn*rm`, one
// instruction) for the remainder - no packed HI/LO register pair to
// unpack the way MIPS's own `mflo`/`mfhi` need.
void Arm64Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp,w0\n";
    genExpr(rhs);
    code_ << "    ldind sp,w1\n    add sp,sp,16\n";  // w1 = lhs (dividend), w0 = rhs (divisor)
    if (wantRemainder) {
        code_ << "    sdiv w2,w1,w0\n";    // w2 = quotient
        code_ << "    msub w0,w2,w0,w1\n";  // w0 = w1 - w2*w0 = dividend - quotient*divisor
    } else {
        code_ << "    sdiv w0,w1,w0\n";
    }
}

// `cmp` (real flags-setting `subs`) plus a dedicated `cset<cc>` per
// comparison operator - each one directly produces the boolean this
// language needs (1 or 0), no branch at all needed - see
// Arm64Codegen.h's own header comment on why this dialect spells
// `cset` as six fixed mnemonics instead of one taking a condition-code
// operand.
void Arm64Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp,w0\n";
    genExpr(rhs);
    code_ << "    ldind sp,w1\n    add sp,sp,16\n";  // w1 = lhs, w0 = rhs
    code_ << "    cmp w1,w0\n";                        // flags = lhs - rhs
    if (op == "<") {
        code_ << "    csetlt w0\n";
        return;
    }
    if (op == ">") {
        code_ << "    csetgt w0\n";
        return;
    }
    if (op == "<=") {
        code_ << "    csetle w0\n";
        return;
    }
    if (op == ">=") {
        code_ << "    csetge w0\n";
        return;
    }
    if (op == "==") {
        code_ << "    cseteq w0\n";
        return;
    }
    if (op == "!=") {
        code_ << "    csetne w0\n";
        return;
    }
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void Arm64Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cbz w0," << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cbz w0," << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movz w0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movz w0,1\n";
    code_ << Ldone << ":\n";
}

void Arm64Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cbnz w0," << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cbnz w0," << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movz w0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movz w0,0\n";
    code_ << Ldone << ":\n";
}

// `cmp w0,wzr` + `cseteq` is exactly "w0 = (w0 == 0)" - logical not,
// no branch needed, same idea as MIPS's own `sltiu t0,t0,1`.
void Arm64Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmp w0,wzr\n    cseteq w0\n";
}

std::string Arm64Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the armv8a backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Arm64Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleArm64(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see Arm64Assembler.h's own `assembleArm64Object`), and
    // whichever of these five names was asked for just picks that
    // format's own ObjectFormatWriter (see objectformat/ObjectFormat.h)
    // to serialize it.
    if (auto kind = objectFormatKindFromName(format)) {
        ObjectFile obj = assembleArm64Object(asmText);
        return makeObjectFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
