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

// Initial A7, loaded by the reset exception from vector 0; well above the loaded program,
// growing down into unused RAM (tests/run_tests.sh gives the simulator 16MB).
constexpr uint32_t kInitialSP = 0x100000;

// `_start` sits right after the two-vector reset table at address 0, with no gap: `org` in
// MC68328Assembler.cpp only affects label resolution, never where bytes land in the output
// buffer, so this backend never calls `org` again to "jump" forward.
constexpr uint32_t kStartAddress = 8;
}  // namespace

// A literal element is its decimal text; a symbol element (a dispatch-table
// address-of-function/global) is the referenced Slot's or Function's label, written out bare.
std::string MC68328Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void MC68328Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive: a `long long` literal element emits as two "dl"s (low dword
    // then high); a symbol element is always native-int-width, so it still gets one "dl".
    const char *directive = w == 1 ? "db" : w == 2 ? "dw" : "dl";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<int32_t>(static_cast<uint32_t>(v)) << ", "
                 << static_cast<int32_t>(static_cast<uint32_t>(v >> 32));
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
    // 68000 memory accesses need an even address; a `db` array is the only shape that can leave
    // the running offset odd, so pad it back to even here.
    if (w == 1 && (values.size() % 2) != 0) trailer_.push_back("db 0");
}

std::string MC68328Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In object mode, `prog` may not define `main` at all: MC68328LinkerFactory.cpp alone
    // synthesizes the reset vector table and entry sequence once objects are merged and
    // `main`'s linked address is resolvable. An object file's own base is `org 0`; the linker
    // places it.
    code_ << "org 0\n";
    if (!objectMode) {
        // Reset vector table: vector 0 (initial SSP), vector 1 (initial PC); the CPU's reset
        // exception loads A7 and PC from these directly.
        code_ << "    dl " << kInitialSP << "\n";
        code_ << "    dl " << kStartAddress << "\n";
        code_ << "_start:\n";
        // Belt-and-suspenders reload of A7: some simulators (confirmed on qemu-system-m68k's
        // `-M none` board) honor the PC half of reset from vector 1 but not the A7 half.
        code_ << "    movel " << kInitialSP << ",a7\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The result sits in D0 the moment execution reaches `halt`.
        code_ << "halt:\n    bra halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file: it must never get storage here, only
        // be referenced by name, letting the assembler defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dl " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(uv))) +
                                    ", " + std::to_string(static_cast<int32_t>(static_cast<uint32_t>(uv >> 32))) +
                                    "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dw" : "dl";
                trailer_.push_back(lbl + ": " + directive + " " + std::to_string(v) + "  ; " + slot->sourceName);
                if (w == 1) trailer_.push_back("db 0");  // keep the running offset even
            }
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
    // `linkl a6,-N` pushes the caller's a6, points this call's a6 at it, and reserves N bytes
    // below it for this function's Locals.
    code_ << "    linkl a6," << (-fn.func->frameSize) << "\n";

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
            if (isWide(s.cond->type)) code_ << "    orl d1,d0\n";
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orl d1,d0\n";
            code_ << "    cmpl 0,d0\n    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    bra " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    bra " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    bra " << loopLabels_.back().first << "\n";
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
                storeOperand(addr(s.declSlot), addr(s.declSlot, 4), s.declSlot->type);
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
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(v)) << ",d0\n";
                code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",d1\n";
                return;
            }
            code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",d0\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadOperand(operandOf(e.slot), operandOf(e.slot, 4), e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // a0 = target address
                code_ << "    pushl a0\n";
                genExpr(*e.rhs);
                code_ << "    popl a0\n";
                storeIndirect(e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeOperand(operandOf(e.slot), operandOf(e.slot, 4), e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's own width/signedness: the pointee type, or a native int word
            // for the untyped-int-as-address style.
            genExpr(*e.lhs);  // d0 = pointer value
            code_ << "    movel d0,a0\n";
            code_ << "    pushl a0\n";
            genExpr(*e.rhs);
            code_ << "    popl a0\n";
            storeIndirect(e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // a0 = address
                genCompoundThroughA0(e);
                return;
            }
            // Plain scalar target: operandOf(slot) is a stable frame/absolute operand, so no
            // address needs to be held across rhs's evaluation.
            std::string mem = operandOf(e.slot);
            genExpr(*e.rhs);  // d0 = rhs
            code_ << "    movel d0,d1\n";
            loadOperand(mem, "", e.type);  // d0 = old
            if (e.yieldOld) code_ << "    pushl d0\n";
            genCompoundOp(e.op, e.type);
            narrowD0To(e.type);
            storeOperand(mem, "", e.type);
            if (e.yieldOld) code_ << "    popl d0\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // d0 = pointer value
            code_ << "    movel d0,a0\n";  // a0 = address
            genCompoundThroughA0(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    orl d1,d0\n";
            code_ << "    cmpl 0,d0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    bra " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    negl d0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's own width/signedness, same as DerefAssign above.
                genExpr(*e.rhs);  // d0 = pointer value
                code_ << "    movel d0,a0\n";
                loadIndirect(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    notl d0\n";
                if (isWide(e.rhs->type)) code_ << "    notl d1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Stack-passed arguments, pushed right-to-left so param 0 ends up closest to the
            // return address JSR pushes; caller cleans up with `adda <N>,a7` afterward (cdecl
            // style). A wide (`long long`) argument pushes its high word first so its low word
            // ends up at the lower address, matching a wide Param's frameOffset/frameOffset+4.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    pushl d1\n    pushl d0\n";
                    totalBytes += 8;
                } else {
                    code_ << "    pushl d0\n";
                    totalBytes += 4;
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    adda " << totalBytes << ",a7\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here: the callee isn't known at compile time, so there's
            // no per-callee cell to pass arguments through. "jsrind a0" is a near call through
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
            // The MC68328 has real maskable interrupts, but this bare compile target never sets
            // up an interrupt vector table, so `ion`/`iof` have nothing to toggle: no-op.
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

// Leaves the computed address in A0. `slot->arraySize > 0` means a compile-time-known base
// (a Global's label via `leal`, or a Local's frame-relative addr()); either array's elements
// sit at increasing addresses, so both index base-plus (`adda`). Otherwise `slot` is a
// pointer variable, so the base is read from its stored value first.
void MC68328Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // d0 = index
    // d0 *= widthOf(slot->type) via repeated doubling.
    for (int i = elemDoublings(slot->type); i > 0; i--) code_ << "    addl d0,d0\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    leal " << label(slot) << ",a0\n";
            code_ << "    adda d0,a0\n";
        } else {
            code_ << "    leal " << addr(slot) << ",a0\n";
            code_ << "    adda d0,a0\n";
        }
    } else {
        code_ << "    movel " << operandOf(slot) << ",a0\n";
        code_ << "    adda d0,a0\n";
    }
}

void MC68328Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    addl d1,d0\n"; return; }
    if (op == "-") { code_ << "    subl d1,d0\n"; return; }
    // MULS.W/MULU.W Dn,Dn: the only hardware multiply, 16-bit operands in, 32-bit product out.
    if (op == "*") { code_ << "    " << (isUnsigned ? "muluw" : "mulsw") << " d1,d0\n"; return; }
    if (op == "&") { code_ << "    andl d1,d0\n"; return; }
    if (op == "|") { code_ << "    orl d1,d0\n"; return; }
    // EOR.L Dn,<ea>: d1 is the source register field, d0 the destination.
    if (op == "^") { code_ << "    eorl d1,d0\n"; return; }
    if (op == "<<" || op == ">>") {
        code_ << "    " << (op == "<<" ? "lsll" : (isUnsigned ? "lsrl" : "asrl")) << " d1,d0\n";
        return;
    }
    // "/" or "%": d0 = old value (dividend), d1 = divisor.
    code_ << "    movel d1,d5\n    movel d0,d1\n    movel d5,d0\n";
    emitDivMod32(isUnsigned);
    code_ << "    movel " << (op == "%" ? "d2" : "d3") << ",d0\n";
}

// A0 must already hold the resolved address on entry; it's stashed on the stack across
// `e.rhs`'s evaluation since that can recursively clobber every data register and A0 itself.
void MC68328Codegen::genCompoundThroughA0(const ir::Expr &e) {
    code_ << "    pushl a0\n";
    genExpr(*e.rhs);              // d0 = rhs
    code_ << "    movel d0,d1\n";  // d1 = rhs, held across the a0 reload below
    code_ << "    popl a0\n";       // a0 = address
    loadIndirect(e.type);             // d0 = old
    if (e.yieldOld) code_ << "    pushl d0\n";
    genCompoundOp(e.op, e.type);         // d0 = new (raw)
    narrowD0To(e.type);
    if (e.yieldOld) {
        code_ << "    movel d0,d2\n";  // d2 = new
        code_ << "    popl d0\n";       // d0 = old
        code_ << "    movel d0,d3\n";    // d3 = old, preserved across the store below
        code_ << "    movel d2,d0\n";     // d0 = new (storeIndirect always stores from d0)
        storeIndirect(e.type);               // a0 still addresses the target (loadIndirect never bumps it - e.type is never wide)
        code_ << "    movel d3,d0\n";          // d0 = old (final result)
    } else {
        storeIndirect(e.type);  // d0 already = new; a0 still addresses the target
    }
}

// Stores a Local array's initializer values directly into its frame slots (no static trailer
// storage, since a Local's storage is freshly reserved by `linkl` on each call).
void MC68328Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int byteOffset = static_cast<int>(i) * w;
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(v)) << "," << addr(slot, byteOffset)
                  << "\n";
            code_ << "    movel " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ","
                  << addr(slot, byteOffset + 4) << "\n";
            continue;
        }
        const char *mnemonic = w == 1 ? "moveb" : w == 2 ? "movew" : "movel";
        code_ << "    " << mnemonic << " " << arrayElemText(elem) << "," << addr(slot, byteOffset) << "\n";
    }
}

void MC68328Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadIndirect(e.slot->type);
}

void MC68328Codegen::narrowD0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    extw d0\n    extl d0\n";
        if (to.isUnsigned) code_ << "    andl 255,d0\n";
    } else if (w == 2) {
        code_ << "    extl d0\n";
        if (to.isUnsigned) code_ << "    andl 65535,d0\n";
    }
    // 4 bytes: nothing to do. 8 bytes never reaches here (see genConvert()).
}

void MC68328Codegen::loadOperand(const std::string &mem, const std::string &highMem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    moveb " << mem << ",d0\n";
        code_ << "    " << (type.isUnsigned ? "andl 255,d0\n" : "extw d0\n    extl d0\n");
    } else if (w == 2) {
        code_ << "    movew " << mem << ",d0\n";
        code_ << "    " << (type.isUnsigned ? "andl 65535,d0\n" : "extl d0\n");
    } else if (w == 4) {
        code_ << "    movel " << mem << ",d0\n";
    } else {  // 8 - long long: D0:D1 (lo:hi)
        code_ << "    movel " << mem << ",d0\n";
        code_ << "    movel " << highMem << ",d1\n";
    }
}

void MC68328Codegen::storeOperand(const std::string &mem, const std::string &highMem, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    moveb d0," << mem << "\n";
    } else if (w == 2) {
        code_ << "    movew d0," << mem << "\n";
    } else if (w == 4) {
        code_ << "    movel d0," << mem << "\n";
    } else {  // 8
        code_ << "    movel d0," << mem << "\n";
        code_ << "    movel d1," << highMem << "\n";
    }
}

// A0 already holds the effective address; indirect counterpart of loadOperand(). The wide case
// bumps A0 by 4 between the low and high word.
void MC68328Codegen::loadIndirect(Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindb a0,d0\n";
        code_ << "    " << (type.isUnsigned ? "andl 255,d0\n" : "extw d0\n    extl d0\n");
    } else if (w == 2) {
        code_ << "    ldindw a0,d0\n";
        code_ << "    " << (type.isUnsigned ? "andl 65535,d0\n" : "extl d0\n");
    } else if (w == 4) {
        code_ << "    ldind a0,d0\n";
    } else {  // 8
        code_ << "    ldind a0,d0\n";
        code_ << "    adda 4,a0\n    ldind a0,d1\n";
    }
}

void MC68328Codegen::storeIndirect(Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb d0,a0\n";
    } else if (w == 2) {
        code_ << "    stindw d0,a0\n";
    } else if (w == 4) {
        code_ << "    stind d0,a0\n";
    } else {  // 8
        code_ << "    stind d0,a0\n";
        code_ << "    adda 4,a0\n    stind d1,a0\n";
    }
}

// An implicit or explicit type conversion. Operands narrower than `int` are already promoted
// by IRBuilder, so the only work here is crossing the native-int/`long long` boundary, or
// re-truncating a narrower target.
void MC68328Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // d0, or d0:d1 if fromWide
    if (fromWide && !toWide) {
        narrowD0To(to);  // d1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    movel 0,d1\n";
        } else {
            // Sign-extend D0 into D1 (no "cdq"-equivalent instruction here): arithmetic-shift a
            // copy of D0 right by 31 to leave D1 all-0s or all-1s per D0's sign.
            code_ << "    movel d0,d1\n";
            code_ << "    movel 31,d2\n    asrl d2,d1\n";
        }
    } else if (!fromWide && !toWide) {
        narrowD0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct.
}

// `long long` add/sub: evaluate both into D0:D1 (lo:hi), stashing lhs's words on the stack
// while rhs evaluates, then ripple the carry/borrow across the word pair with ADDX/SUBX.
void MC68328Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);  // d0:d1 = lhs
    code_ << "    pushl d1\n    pushl d0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);                              // d0:d1 = rhs
    code_ << "    movel d0,d2\n    movel d1,d3\n";  // d2=rhs_lo, d3=rhs_hi
    code_ << "    popl d0\n    popl d1\n";            // d0=lhs_lo, d1=lhs_hi
    if (isAdd) {
        code_ << "    addl d2,d0\n    addxl d3,d1\n";
    } else {
        code_ << "    subl d2,d0\n    subxl d3,d1\n";
    }
}

// `long long` multiply: 68000 has no 32x32 hardware multiply (only 16x16->32 MULS.W/MULU.W,
// which a `long long`'s 32-bit halves don't fit), so this is a shift-and-add multiply over the
// full 64 bits, bit by bit. Two's-complement multiplication is sign-agnostic at the bit-pattern
// level, so no sign handling is needed, unlike genWideDivMod(). Addressed via `a0` over a stack
// scratch block: [a0+0/4] multiplicand (shifted left each iteration), [a0+8/12] multiplier
// (shifted right, consumed bit by bit), [a0+16/20] accumulating result. Result in D0(lo):D1(hi).
void MC68328Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    pushl d1\n    pushl d0\n";  // mcand_hi, mcand_lo (lo on top)
    genExpr(rhs);
    code_ << "    pushl d1\n    pushl d0\n";  // mplier_hi, mplier_lo (lo on top)
    code_ << "    suba 24,a7\n    movel a7,a0\n";
    code_ << "    movel [a0+32],d0\n    movel d0,[a0+0]\n";   // mcand_lo
    code_ << "    movel [a0+36],d0\n    movel d0,[a0+4]\n";   // mcand_hi
    code_ << "    movel [a0+24],d0\n    movel d0,[a0+8]\n";   // mplier_lo
    code_ << "    movel [a0+28],d0\n    movel d0,[a0+12]\n";  // mplier_hi
    code_ << "    movel 0,d0\n";
    code_ << "    movel d0,[a0+16]\n    movel d0,[a0+20]\n";  // result = 0
    code_ << "    movel 1,d3\n    movel 31,d6\n";             // fixed shift-count registers
    std::string Lloop = newLabel(), Ldone = newLabel(), LnoAdd = newLabel();
    code_ << "    movel 64,d1\n";  // loop counter
    code_ << Lloop << ":\n";
    code_ << "    cmpl 0,d1\n    beq " << Ldone << "\n    subl 1,d1\n";

    // if the multiplier's bit 0 is set, result += multiplicand (64-bit add via ADD/ADDX).
    code_ << "    movel [a0+8],d0\n    andl 1,d0\n    cmpl 0,d0\n    beq " << LnoAdd << "\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+0],d2\n    addl d2,d0\n    movel d0,[a0+16]\n";
    code_ << "    movel [a0+20],d0\n    movel [a0+4],d2\n    addxl d2,d0\n    movel d0,[a0+20]\n";
    code_ << LnoAdd << ":\n";

    // multiplicand <<= 1: ADD.L Dn,Dn doubles Dn and sets the carry from bit 31; ADDX.L Dn,Dn
    // doubles the high word while folding that carry in.
    code_ << "    movel [a0+0],d0\n    addl d0,d0\n    movel d0,[a0+0]\n";
    code_ << "    movel [a0+4],d0\n    addxl d0,d0\n    movel d0,[a0+4]\n";

    // multiplier >>= 1: the high word's outgoing bit 0 is carried into the low word's new bit 31
    // by hand.
    code_ << "    movel [a0+12],d4\n";                           // d4 = mplier_hi (before shifting)
    code_ << "    movel d4,d0\n    lsrl d3,d0\n    movel d0,[a0+12]\n";  // mplier_hi >>= 1
    code_ << "    andl 1,d4\n    lsll d6,d4\n";                   // d4 = (old mplier_hi & 1) << 31
    code_ << "    movel [a0+8],d0\n    lsrl d3,d0\n    orl d4,d0\n    movel d0,[a0+8]\n";

    code_ << "    bra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+20],d1\n";  // d0:d1 = result
    code_ << "    adda 40,a7\n";  // release the 6-word scratch block and the 4 earlier stashed operand words
}

void MC68328Codegen::genNegate64() {
    code_ << "    notl d0\n    notl d1\n";
    code_ << "    movel 0,d2\n";
    code_ << "    addl 1,d0\n";
    code_ << "    addxl d2,d1\n";
}

// Classic bit-serial shift/compare/subtract long division, addressed via a0 over a 9-word
// scratch block.
void MC68328Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl d1\n    pushl d0\n";  // lhs_hi, lhs_lo (lhs_lo on top)
    genExpr(rhs);
    code_ << "    pushl d1\n    pushl d0\n";  // rhs_hi, rhs_lo (rhs_lo on top)
    code_ << "    suba 36,a7\n    movel a7,a0\n";
    // Fetch the stashed operands from just above the scratch block.
    code_ << "    movel [a0+44],d0\n    movel d0,[a0+0]\n";   // n_lo = lhs_lo
    code_ << "    movel [a0+48],d0\n    movel d0,[a0+4]\n";   // n_hi = lhs_hi
    code_ << "    movel [a0+36],d0\n    movel d0,[a0+8]\n";   // d_lo = rhs_lo
    code_ << "    movel [a0+40],d0\n    movel d0,[a0+12]\n";  // d_hi = rhs_hi
    code_ << "    movel 0,d0\n";
    code_ << "    movel d0,[a0+16]\n    movel d0,[a0+20]\n";  // r = 0
    code_ << "    movel d0,[a0+24]\n    movel d0,[a0+28]\n";  // q = 0
    code_ << "    movel 64,d0\n    movel d0,[a0+32]\n";       // 64 iterations

    // d6/d7 are fixed shift counts for the whole routine, never rewritten below.
    code_ << "    movel 1,d6\n    movel 31,d7\n";

    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        // lhsSign/rhsSign reuse [a0+36]/[a0+40], the rhs staging words, never read again for
        // that purpose.
        code_ << "    movel [a0+4],d0\n    lsrl d7,d0\n    movel d0,[a0+36]\n";  // lhsSign
        code_ << "    movel [a0+12],d0\n    lsrl d7,d0\n    movel d0,[a0+40]\n";  // rhsSign
        code_ << "    movel [a0+36],d0\n    cmpl 0,d0\n    beq " << LskipNegN << "\n";
        code_ << "    movel [a0+0],d0\n    movel [a0+4],d1\n";
        genNegate64();
        code_ << "    movel d0,[a0+0]\n    movel d1,[a0+4]\n";
        code_ << LskipNegN << ":\n";
        code_ << "    movel [a0+40],d0\n    cmpl 0,d0\n    beq " << LskipNegD << "\n";
        code_ << "    movel [a0+8],d0\n    movel [a0+12],d1\n";
        genNegate64();
        code_ << "    movel d0,[a0+8]\n    movel d1,[a0+12]\n";
        code_ << LskipNegD << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movel [a0+32],d0\n    cmpl 0,d0\n    beq " << Ldone << "\n";
    code_ << "    subl 1,d0\n    movel d0,[a0+32]\n";

    // n <<= 1 (0-filled), capturing its own old top bit into d1.
    code_ << "    movel [a0+4],d0\n    lsrl d7,d0\n    movel d0,d1\n";  // d1 = n's old top bit
    code_ << "    movel [a0+0],d0\n    lsrl d7,d0\n    movel d0,d2\n";  // d2 = n_lo's old top bit
    code_ << "    movel [a0+0],d0\n    lsll d6,d0\n    movel d0,[a0+0]\n";
    code_ << "    movel [a0+4],d0\n    lsll d6,d0\n    orl d2,d0\n    movel d0,[a0+4]\n";

    // r = (r << 1) | (n's old top bit, already in d1).
    code_ << "    movel [a0+16],d0\n    lsrl d7,d0\n    movel d0,d3\n";  // d3 = r_lo's old top bit
    code_ << "    movel [a0+16],d0\n    lsll d6,d0\n    orl d1,d0\n    movel d0,[a0+16]\n";
    code_ << "    movel [a0+20],d0\n    lsll d6,d0\n    orl d3,d0\n    movel d0,[a0+20]\n";

    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 } - always an unsigned
    // compare, since n and d were already negated to a non-negative magnitude above.
    code_ << "    movel [a0+20],d0\n    movel [a0+12],d1\n    cmpl d1,d0\n    bne " << LhiDiff << "\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+8],d1\n    cmpl d1,d0\n    bcc " << LsubYes << "\n";
    code_ << "    bra " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bcc " << LsubYes << "\n    bra " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+8],d1\n    subl d1,d0\n    movel d0,[a0+16]\n";
    code_ << "    movel [a0+20],d0\n    movel [a0+12],d1\n    subxl d1,d0\n    movel d0,[a0+20]\n";
    code_ << "    movel 1,d5\n    bra " << LshiftQ << "\n";
    code_ << LsubNo << ":\n    movel 0,d5\n";
    code_ << LshiftQ << ":\n";

    // q = (q << 1) | quotient bit (in d5).
    code_ << "    movel [a0+24],d0\n    lsrl d7,d0\n    movel d0,d2\n";  // d2 = q_lo's old top bit
    code_ << "    movel [a0+24],d0\n    lsll d6,d0\n    orl d5,d0\n    movel d0,[a0+24]\n";
    code_ << "    movel [a0+28],d0\n    lsll d6,d0\n    orl d2,d0\n    movel d0,[a0+28]\n";
    code_ << "    bra " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the operands' signs differed; the
        // remainder always takes the dividend's (lhs) own sign.
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    movel [a0+36],d5\n    movel [a0+40],d0\n    eorl d5,d0\n";
        code_ << "    cmpl 0,d0\n    beq " << LskipNegQ << "\n";
        code_ << "    movel [a0+24],d0\n    movel [a0+28],d1\n";
        genNegate64();
        code_ << "    movel d0,[a0+24]\n    movel d1,[a0+28]\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    movel [a0+36],d0\n    cmpl 0,d0\n    beq " << LskipNegR << "\n";
        code_ << "    movel [a0+16],d0\n    movel [a0+20],d1\n";
        genNegate64();
        code_ << "    movel d0,[a0+16]\n    movel d1,[a0+20]\n";
        code_ << LskipNegR << ":\n";
    }
    code_ << "    movel [a0+24],d0\n    movel [a0+28],d1\n";  // d0:d1 = quotient
    code_ << "    movel [a0+16],d2\n    movel [a0+20],d3\n";  // d2:d3 = remainder
    code_ << "    adda 52,a7\n";  // release the 9-word scratch block and the 4 earlier stashed operand words
    if (wantRemainder) code_ << "    movel d2,d0\n    movel d3,d1\n";
}

// A `long long` shifted by a runtime count: no native instruction shifts more than 32 bits at
// once, so this is a counted loop of 1-bit shifts, carrying the bit across the word boundary
// by hand.
void MC68328Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl d1\n    pushl d0\n";
    genExpr(rhs);                   // d0 = shift count (already promoted to a plain int)
    code_ << "    movel d0,d2\n";   // d2 = remaining count
    code_ << "    popl d0\n    popl d1\n";  // d0:d1 restored = the value being shifted (lo:hi)
    code_ << "    movel 1,d6\n    movel 31,d7\n";  // fixed shift-count registers (register-count-only dialect)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmpl 0,d2\n    beq " << Ldone << "\n    subl 1,d2\n";
    if (isLeft) {
        code_ << "    movel d0,d3\n    lsrl d7,d3\n";  // d3 = d0's old top bit
        code_ << "    lsll d6,d1\n    orl d3,d1\n";
        code_ << "    lsll d6,d0\n";
    } else {
        code_ << "    movel d1,d3\n    lsll d7,d3\n";  // d3 = (d1&1)<<31
        code_ << "    lsrl d6,d0\n    orl d3,d0\n";
        code_ << "    " << (isUnsigned ? "lsrl" : "asrl") << " d6,d1\n";
    }
    code_ << "    bra " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison: compare the high words first (signed or unsigned branch per
// `isUnsigned`, since that's where the sign bit lives); if equal, the low words compare as
// unsigned regardless of the overall type's signedness.
void MC68328Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl d1\n    pushl d0\n";
    genExpr(rhs);
    code_ << "    movel d0,d2\n    movel d1,d3\n";  // d2:d3 = rhs (lo:hi)
    code_ << "    popl d0\n    popl d1\n";          // d0:d1 = lhs (lo:hi)
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmpl d2,d0\n    bne " << Lfalse << "\n";
        code_ << "    cmpl d3,d1\n    bne " << Lfalse << "\n";
        code_ << "    movel " << onEqual << ",d0\n    bra " << Ldone << "\n";
        code_ << Lfalse << ":\n    movel " << onDiffer << ",d0\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string signedBcc = op == "<" ? "blt" : op == "<=" ? "ble" : op == ">" ? "bgt" : "bge";
    std::string unsignedBcc = op == "<" ? "bcs" : op == "<=" ? "bls" : op == ">" ? "bhi" : "bcc";
    std::string hiBcc = isUnsigned ? unsignedBcc : signedBcc;
    std::string LhiDiff = newLabel();
    code_ << "    cmpl d3,d1\n    bne " << LhiDiff << "\n";
    code_ << "    cmpl d2,d0\n    " << unsignedBcc << " " << Ltrue << "\n";
    code_ << "    bra " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    " << hiBcc << " " << Ltrue << "\n";
    code_ << "    bra " << Lfalse << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
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
    // Arithmetic/bitwise/comparison operands already share one common type (`e.lhs->type`),
    // except shift (only the LHS type matters) and &&/|| (handled by genLogicalAnd/Or below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
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
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs);
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
        const char *insn = op == "&" ? "andl" : op == "|" ? "orl" : "eorl";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    pushl d1\n    pushl d0\n";
            genExpr(*e.rhs);
            code_ << "    movel d0,d2\n    movel d1,d3\n";
            code_ << "    popl d0\n    popl d1\n";
            code_ << "    " << insn << " d2,d0\n    " << insn << " d3,d1\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
        // EOR's source/dest convention (Dn is always source) still gives the right result for
        // this commutative op regardless of operand order.
        code_ << "    " << insn << " d1,d0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    pushl d0\n";
        genExpr(*e.rhs);
        code_ << "    movel d0,d1\n";  // d1 = shift count
        code_ << "    popl d0\n";      // d0 = value to shift
        const char *insn = op == "<<" ? "lsll" : (isUnsigned ? "lsrl" : "asrl");
        code_ << "    " << insn << " d1,d0\n";
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

// Real hardware multiply (`MULS.W`/`MULU.W`): 16-bit operands only.
void MC68328Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
    code_ << "    " << (isUnsigned ? "muluw" : "mulsw") << " d1,d0\n";  // d0 = d0 * d1 = rhs * lhs
}

// `int`/`long` are 32 bits here, wider than DIVS.W/DIVU.W's 32/16
// divide with a 16-bit quotient, so division is a bit-serial loop
// (emitDivMod32()).
void MC68328Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs (dividend), d0 = rhs (divisor)
    emitDivMod32(isUnsigned);
    code_ << "    movel " << (wantRemainder ? "d2" : "d3") << ",d0\n";
}

// Restoring shift-subtract division, one quotient bit per iteration.
// The remainder's own top bit is checked before each shift: if set,
// the shifted value exceeds 32 bits and so exceeds the divisor, and
// the subtraction's modular result is still exact. Signed operands
// are divided as magnitudes; the quotient is negated when the signs
// differ and the remainder takes the dividend's sign.
void MC68328Codegen::emitDivMod32(bool isUnsigned) {
    code_ << "    pushl a0\n";  // a compound assignment's target address
    code_ << "    movel 1,d6\n    movel 31,d7\n";
    std::string LskipN = newLabel(), LskipD = newLabel();
    if (!isUnsigned) {
        code_ << "    movel d1,d5\n    lsrl d7,d5\n    pushl d5\n";  // dividend sign
        code_ << "    movel d0,d5\n    lsrl d7,d5\n    pushl d5\n";  // divisor sign
        code_ << "    movel a7,a0\n";                                  // [a0+0] divisor sign, [a0+4] dividend sign
        code_ << "    movel [a0+4],d5\n    cmpl 0,d5\n    beq " << LskipN << "\n";
        code_ << "    notl d1\n    addl 1,d1\n";
        code_ << LskipN << ":\n";
        code_ << "    movel [a0+0],d5\n    cmpl 0,d5\n    beq " << LskipD << "\n";
        code_ << "    notl d0\n    addl 1,d0\n";
        code_ << LskipD << ":\n";
    }
    std::string Lloop = newLabel(), Ldone = newLabel(), Lsub = newLabel(), Lnext = newLabel();
    code_ << "    movel 0,d2\n    movel 0,d3\n    movel 32,d4\n";
    code_ << Lloop << ":\n";
    code_ << "    cmpl 0,d4\n    beq " << Ldone << "\n";
    code_ << "    subl 1,d4\n";
    code_ << "    movel d1,d5\n    lsrl d7,d5\n    lsll d6,d1\n";     // d5 = dividend's outgoing bit
    code_ << "    movel d2,a0\n";                                      // a0 = remainder before shifting
    code_ << "    lsll d6,d2\n    orl d5,d2\n    lsll d6,d3\n";
    code_ << "    movel a0,d5\n    lsrl d7,d5\n    cmpl 0,d5\n    bne " << Lsub << "\n";
    code_ << "    cmpl d0,d2\n    bcc " << Lsub << "\n";
    code_ << "    bra " << Lnext << "\n";
    code_ << Lsub << ":\n";
    code_ << "    subl d0,d2\n    orl d6,d3\n";
    code_ << Lnext << ":\n";
    code_ << "    bra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        std::string LskipQ = newLabel(), LskipR = newLabel();
        code_ << "    movel a7,a0\n";
        code_ << "    movel [a0+0],d5\n    movel [a0+4],d4\n    eorl d4,d5\n";
        code_ << "    cmpl 0,d5\n    beq " << LskipQ << "\n";
        code_ << "    notl d3\n    addl 1,d3\n";
        code_ << LskipQ << ":\n";
        code_ << "    cmpl 0,d4\n    beq " << LskipR << "\n";
        code_ << "    notl d2\n    addl 1,d2\n";
        code_ << LskipR << ":\n";
        code_ << "    adda 8,a7\n";
    }
    code_ << "    popl a0\n";
}

void MC68328Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, isUnsigned);
        return;
    }
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "bcs" : "blt");
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "bls" : "ble");
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "bhi" : "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "bcc" : "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `CMP.L <ea>,Dn` sets condition codes for `Dn - ea`; `branchIfTrue` is already picked per
// signedness by genCompare() (BHI/BLS/BCC/BCS test carry for unsigned, BGT/BLE/BGE/BLT test
// N^V for signed), so it's applied directly here.
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

// A wide (`long long`) operand's truthiness needs both words: `orl d1,d0` folds them together
// before the usual "cmpl 0,d0".
void MC68328Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orl d1,d0\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orl d1,d0\n";
    code_ << "    cmpl 0,d0\n    beq " << Lfalse << "\n";
    code_ << "    bra " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

void MC68328Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orl d1,d0\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orl d1,d0\n";
    code_ << "    cmpl 0,d0\n    bne " << Ltrue << "\n";
    code_ << "    bra " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n    bra " << Ldone << "\n";
    code_ << Lfalse << ":\n    movel 0,d0\n";
    code_ << Ldone << ":\n";
}

void MC68328Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orl d1,d0\n";
    code_ << "    cmpl 0,d0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

std::string MC68328Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 68000 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MC68328Codegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleMC68328(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: build one format-neutral ObjectFile, then pick that
    // format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMC68328Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
