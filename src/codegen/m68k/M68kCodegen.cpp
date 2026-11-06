#include "M68kCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "M68kAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly (see
// tests/run_tests.sh's own qemu-system-m68k invocation), so unlike
// real 68000 hardware (which reads its initial SSP/PC from the reset
// vector at address 0) this backend never needs to lay one out.
constexpr uint32_t kBaseAddress = 0x1000;

// Where this backend's own entry sequence points A7 (the real
// hardware stack pointer, used by PUSH/POP-equivalent moves and by
// JSR/RTS) before calling main() - comfortably above the loaded
// program and its data, growing down into otherwise-unused RAM
// (tests/run_tests.sh gives the simulator 16MB).
constexpr uint32_t kInitialSP = 0x100000;
}  // namespace

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dl`'s own expression evaluator
// (M68kAssembler.cpp's Line::Dl) already resolves a bare symbol the
// same way `leal label,a0` already does for an address held in a
// register (see genAddrOf).
std::string M68kCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void M68kCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long`
    // array's own literal elements are emitted as two "dl"s (low
    // dword, then high) per element instead; a symbol element always
    // means a plain native-int-width address (the dispatch-table
    // idiom - see AST.h's ArrayInitElem), so it still gets one "dl".
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
    // 68000 memory accesses need an even address (an odd one takes a
    // real hardware address-error exception) - every `dl`/`dw`
    // elsewhere in this dialect is already an even number of bytes,
    // so a `db` array is the only shape that can leave the running
    // offset odd; pad it back to even here, once, rather than at
    // every later data declaration's own call site.
    if (w == 1 && (values.size() % 2) != 0) trailer_.push_back("db 0");
}

std::string M68kCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - M68kLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see M68kAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movel " << kInitialSP << ",a7\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The result sits in D0 the moment execution reaches `halt` -
        // see M68kCodegen.h's own header comment on why that's the
        // authoritative place to read it from (a real, directly
        // inspectable 32-bit register, unlike the 6502 backend's own
        // memory-resident "ac" - see that file's own comment).
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
                if (w == 1) trailer_.push_back("db 0");  // keep the running offset even - see declareArray()
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
std::map<std::string, std::string> M68kCodegen::perFileAsm() const { return perFileAsm_; }

void M68kCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    // Real per-call frame (see M68kCodegen.h's own header comment):
    // `linkl a6,-N` pushes the caller's `a6`, points this call's own
    // `a6` at that saved value, and reserves N bytes below it for
    // this function's own Locals (frameSize is already a real byte
    // count - see IR.h's own Function::frameSize).
    code_ << "    linkl a6," << (-fn.func->frameSize) << "\n";

    genStmt(*fn.body);

    code_ << "    unlkl a6\n";
    code_ << "    rts\n\n";
}

void M68kCodegen::genStmt(const ir::Stmt &s) {
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

void M68kCodegen::genExpr(const ir::Expr &e) {
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
                code_ << "    pushl a0\n";     // a0 is really pushed via d0's own slot width - see pushl's own note
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
            // Stores through at e.type's own width/signedness - the
            // pointee type of a real typed pointer, or a plain native
            // int word for the still-supported untyped-int-as-address
            // style - see IRBuilder.cpp's own DerefAssign comment.
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
            // Plain scalar target - operandOf(slot) is a stable frame/
            // absolute operand, never a clobberable register, so no
            // address needs to be held across rhs's own evaluation at
            // all.
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
                // Loads at e.type's own width/signedness - see
                // DerefAssign's identical comment above.
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
            // Real stack-passed arguments, pushed right-to-left so
            // param 0 ends up closest to the return address JSR is
            // about to push (see M68kCodegen.h's own `addr()` comment)
            // - each call's own arguments land in that call's own
            // frame this way, never in a shared per-function cell.
            // Caller cleans up its own pushed arguments afterwards
            // (`adda <N>,a7`) - same cdecl-style convention as
            // I386Codegen.cpp's own identical `Call` case. Every
            // argument reserves at least one native int's worth (the
            // same "no narrower than a native word" rule IRBuilder.cpp's
            // own param-offset layout already applies), and a wide
            // (`long long`) argument pushes its high word first so its
            // low word ends up at the lower address, matching a wide
            // Param's own frameOffset/frameOffset+4 layout.
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
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "jsrind a0" is M68kAssembler.cpp's new (An)-indirect
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
            // No real portable notion of interrupt enable/disable on
            // a bare-metal 68000 with no vector table set up at all
            // (see generate()'s own comment) - RTE/the SR interrupt-
            // mask bits exist, but there's nothing here for `ion`/
            // `iof` to meaningfully enable or disable. Treated as a
            // no-op rather than a hard error (matching this
            // intrinsic's own "do whatever this target's version of
            // enable/disable interrupts is" spirit, for a target
            // where that's simply nothing).
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

// See M68kCodegen.h's own header comment: `A0` holds the computed
// address, no zero-page-style placement restriction the way the
// 6502 backend's own `ptr` has (see that file's own
// genIndexAddr()). `slot->arraySize > 0` means the base is a
// compile-time-known address (a Global's own label, loaded via
// `leal`, or a Local's own frame-relative storage - see addr()); a
// Global array's elements sit at increasing addresses as the index
// increases, but a Local array's own elements sit at *decreasing*
// addresses (addr(slot) is the array's first word, closest to `a6`;
// later words are further below it - see M68kCodegen.h's own
// `addr()` comment), so a Local array indexes base-minus-scaled-
// index, not base-plus (`suba` instead of `adda`). Otherwise `slot`
// is a plain pointer variable, so the base instead comes from
// *reading* that variable's own stored value first.
void M68kCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // d0 = index
    // d0 *= widthOf(slot->type) via repeated doubling - this array's
    // own element width, not a fixed "always 4 bytes" - needing no
    // extra register at all, same shape as the old fixed ×4.
    for (int i = elemDoublings(slot->type); i > 0; i--) code_ << "    addl d0,d0\n";
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

void M68kCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    addl d1,d0\n"; return; }
    if (op == "-") { code_ << "    subl d1,d0\n"; return; }
    // MULS.W/MULU.W Dn,Dn (see genMul()'s own identical form) - this
    // target's only hardware multiply, 16-bit operands in, correct
    // 32-bit product out (see M68kCodegen.h's own header comment on
    // this limitation).
    if (op == "*") { code_ << "    " << (isUnsigned ? "muluw" : "mulsw") << " d1,d0\n"; return; }
    if (op == "&") { code_ << "    andl d1,d0\n"; return; }
    if (op == "|") { code_ << "    orl d1,d0\n"; return; }
    // EOR.L Dn,<ea> - d1 is the source register field, d0 the
    // destination, same role EOR always plays in genBinary()'s own
    // "^" case.
    if (op == "^") { code_ << "    eorl d1,d0\n"; return; }
    if (op == "<<" || op == ">>") {
        code_ << "    " << (op == "<<" ? "lsll" : (isUnsigned ? "lsrl" : "asrl")) << " d1,d0\n";
        return;
    }
    // "/" or "%" - DIVS.W/DIVU.W <ea>,Dn: d1 is the word-sized
    // divisor, d0 the long dividend and destination (quotient in the
    // low word, remainder in the high word afterward - see
    // genDivMod()'s own identical form).
    code_ << "    " << (isUnsigned ? "divuw" : "divsw") << " d1,d0\n";
    if (op == "%") code_ << "    swapl d0\n";
    code_ << "    " << (isUnsigned ? "andl 65535,d0\n" : "extl d0\n");
}

// A0 must already hold the resolved address on entry, and nothing
// else live - mirrors genExpr()'s own indexed-Assign case, which
// already stashes A0 on the real hardware stack (A7) across `e.rhs`'s
// own evaluation for the identical reason: that evaluation can
// recursively reach arbitrary codegen (calls included), which may
// freely use every data register and A0 itself.
void M68kCodegen::genCompoundThroughA0(const ir::Expr &e) {
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

// Stores each of a Local array's own initializer values directly into
// its own frame slots (no static trailer storage - unlike a Global
// array's `declareArray()`, a Local's storage is this call's own
// frame, freshly reserved by `linkl` every time this function runs).
// `MOVE.L` accepts an immediate or absolute-label source going
// straight into a frame-relative destination, so each element needs
// only one instruction.
void M68kCodegen::genLocalArrayInit(const ir::Slot *slot) {
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

void M68kCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadIndirect(e.slot->type);
}

void M68kCodegen::narrowD0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    extw d0\n    extl d0\n";
        if (to.isUnsigned) code_ << "    andl 255,d0\n";
    } else if (w == 2) {
        code_ << "    extl d0\n";
        if (to.isUnsigned) code_ << "    andl 65535,d0\n";
    }
    // 4 bytes (Int/Long): D0 already holds the full value, nothing to
    // do. 8 bytes never reaches here - see genConvert().
}

void M68kCodegen::loadOperand(const std::string &mem, const std::string &highMem, Type type) {
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

void M68kCodegen::storeOperand(const std::string &mem, const std::string &highMem, Type type) {
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

// `A0` already holds the effective address (see genIndexAddr()) - the
// indirect-addressing counterpart of loadOperand()/storeOperand()
// above. The wide case bumps A0 by 4 between the low and high word -
// never needed again afterward by either call site (genIndex()/the
// indexed-Assign case in genExpr()).
void M68kCodegen::loadIndirect(Type type) {
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

void M68kCodegen::storeIndirect(Type type) {
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

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert) - see I386Codegen.cpp's own genConvert() for the full
// reasoning (every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder, so the only real work here is
// crossing the native-int/`long long` boundary, or re-truncating a
// narrower target).
void M68kCodegen::genConvert(const ir::Expr &e) {
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
            // Sign-extend D0's own sign into all of D1 - 68000 has no
            // single "extend D0 into D1" instruction the way `cdq`
            // is on i386, so this is built from what's already here:
            // shift a copy of D0 right by 31 arithmetic bit positions
            // (one bit at a time is enough since only the sign matters
            // - the real value is discarded), leaving D1 all-0s or
            // all-1s per D0's own sign.
            code_ << "    movel d0,d1\n";
            code_ << "    movel 31,d2\n    asrl d2,d1\n";
        }
    } else if (!fromWide && !toWide) {
        narrowD0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

// `long long` add/sub (lhs OP rhs): evaluate both into D0:D1 (lo:hi),
// stashing lhs's two words on the stack while rhs evaluates, then
// ripple the carry/borrow across the word pair with ADDX/SUBX - the
// real-hardware generalization of a native add/sub one word wider,
// same idiom I386Codegen.cpp's genWideAdd() uses with adc/sbb.
void M68kCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
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

// `long long` multiply: 68000 has no 32x32 hardware multiply at all
// (only MULS.W/MULU.W, 16x16->32 - see genMul()/M68kCodegen.h's own
// header comment on that instruction's own operand-size limitation,
// which a `long long`'s own 32-bit halves essentially never satisfy).
// Instead of relying on that narrow primitive, this is a genuine
// shift-and-add multiply over the full 64 bits, bit by bit - exactly
// the same algorithm a hardware multiplier circuit itself implements,
// just unrolled into a software loop; correct for any operand
// magnitude. Two's-complement multiplication is sign-agnostic at the
// bit-pattern level (the same circuit/algorithm produces the correct
// low 64 result bits whether the operands are "meant" to be signed or
// unsigned - only the *interpretation* of the result differs), so no
// sign handling is needed here at all, unlike genWideDivMod().
// Addressed via `a0` over a small stack-resident scratch block, same
// idiom as genWideDivMod(): [a0+0/4] the multiplicand (shifted left
// each iteration), [a0+8/12] the multiplier (shifted right, consumed
// bit by bit from the low end), [a0+16/20] the result being
// accumulated. Leaves the result in D0(lo):D1(hi).
void M68kCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
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

    // if the multiplier's own bit 0 is set, result += multiplicand
    // (64-bit add via ADD/ADDX, same ripple-carry idiom as
    // genWideAdd()).
    code_ << "    movel [a0+8],d0\n    andl 1,d0\n    cmpl 0,d0\n    beq " << LnoAdd << "\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+0],d2\n    addl d2,d0\n    movel d0,[a0+16]\n";
    code_ << "    movel [a0+20],d0\n    movel [a0+4],d2\n    addxl d2,d0\n    movel d0,[a0+20]\n";
    code_ << LnoAdd << ":\n";

    // multiplicand <<= 1 - ADD.L Dn,Dn doubles Dn and sets X/C from
    // the bit that overflows out of bit 31; ADDX.L Dn,Dn then doubles
    // the high word while folding that same carry in, giving a
    // correct 64-bit left shift by 1 in two instructions, no separate
    // bit-extraction needed (contrast genWideShift()'s own `<<`,
    // which has to extract the carry by hand because it shifts by a
    // runtime count, not always exactly 1).
    code_ << "    movel [a0+0],d0\n    addl d0,d0\n    movel d0,[a0+0]\n";
    code_ << "    movel [a0+4],d0\n    addxl d0,d0\n    movel d0,[a0+4]\n";

    // multiplier >>= 1 (plain bit-pattern shift, no sign to preserve -
    // see this method's own header comment) - the high word's own
    // outgoing bit 0 has to be carried into the low word's new bit 31
    // by hand, the same extract-then-OR idiom genWideShift() uses.
    code_ << "    movel [a0+12],d4\n";                           // d4 = mplier_hi (before shifting)
    code_ << "    movel d4,d0\n    lsrl d3,d0\n    movel d0,[a0+12]\n";  // mplier_hi >>= 1
    code_ << "    andl 1,d4\n    lsll d6,d4\n";                   // d4 = (old mplier_hi & 1) << 31
    code_ << "    movel [a0+8],d0\n    lsrl d3,d0\n    orl d4,d0\n    movel d0,[a0+8]\n";

    code_ << "    bra " << Lloop << "\n";
    code_ << Ldone << ":\n";
    code_ << "    movel [a0+16],d0\n    movel [a0+20],d1\n";  // d0:d1 = result
    code_ << "    adda 40,a7\n";  // release the 6-word scratch block and the 4 earlier stashed operand words
}

void M68kCodegen::genNegate64() {
    code_ << "    notl d0\n    notl d1\n";
    code_ << "    movel 0,d2\n";
    code_ << "    addl 1,d0\n";
    code_ << "    addxl d2,d1\n";
}

// Classic bit-serial shift/compare/subtract long division - see
// I386Codegen.cpp's own genWideDivMod() for the full algorithm this
// is a direct translation of (same 9-word scratch layout, same
// signed-magnitude wrapper), addressed via `a0` instead of `ebx`.
void M68kCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
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

    // d6/d7 are fixed shift counts for the whole routine (register-
    // count is the only shift form this dialect implements - see
    // M68kAssembler.cpp's own encodeShift) - neither is ever written
    // again below, so setting them once up front is enough.
    code_ << "    movel 1,d6\n    movel 31,d7\n";

    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        // lhsSign/rhsSign reuse [a0+36]/[a0+40] - the rhs_lo/rhs_hi
        // staging words, already copied into d_lo/d_hi above and
        // never read again for that purpose.
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

    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 }
    // (always an unsigned 64-bit magnitude compare, even for a signed
    // division - both n and d have already been negated to a
    // non-negative magnitude above.)
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

// A `long long` shifted by a runtime count - no native instruction
// shifts more than 32 bits at once, so this is a counted loop of
// 1-bit shifts, each carrying the bit that crosses the word boundary
// by hand - see I386Codegen.cpp's own genWideShift() for the
// identical idiom this is translated from.
void M68kCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
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

// A `long long` comparison - compare the high words first (using the
// signed or unsigned branch per `isUnsigned`, since that's where the
// sign bit lives); only if those are equal does the low word's own
// comparison matter, and at that point it's always an unsigned
// compare regardless of the overall type's signedness - see
// I386Codegen.cpp's own genWideCompare() for the identical idiom.
void M68kCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
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

void M68kCodegen::genAddrOf(const ir::Expr &operand) {
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

void M68kCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already shares
    // one common type (IRBuilder's own usual-arithmetic-conversion
    // pass - see Type.h) - `e.lhs->type` names it, except for shift
    // (only the LHS's own type matters) and &&/|| (width-independent
    // truth-testing, handled by genLogicalAnd/Or below).
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
        // EOR's own source/dest convention (Dn is always the source
        // here) still gives the right result for this commutative op
        // regardless of operand order - see M68kAssembler.cpp's own
        // comment.
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

// Real hardware multiply (`MULS.W`/`MULU.W`) - see M68kCodegen.h's own
// header comment on its own 16-bit-operand limitation.
void M68kCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs, d0 = rhs
    code_ << "    " << (isUnsigned ? "muluw" : "mulsw") << " d1,d0\n";  // d0 = d0 * d1 = rhs * lhs
}

// Real hardware divide (`DIVS.W`/`DIVU.W`) - packs a 16-bit quotient
// and a 16-bit remainder into one 32-bit register (see
// M68kCodegen.h's own header comment on this instruction family's own
// operand-size limitation); `SWAP` pulls the needed half into the low
// word, then either `EXT.L` (signed) or a plain mask (unsigned, since
// an unsigned 16-bit half's own bit 15 is real magnitude, not a sign
// to extend) widens it to a real long.
void M68kCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    pushl d0\n";
    genExpr(rhs);
    code_ << "    popl d1\n";  // d1 = lhs (dividend), d0 = rhs (divisor)
    code_ << "    " << (isUnsigned ? "divuw" : "divsw") << " d0,d1\n";  // d1 = quotient (low word) : remainder (high)
    if (wantRemainder) {
        code_ << "    swapl d1\n";  // remainder now in the low word
    }
    code_ << "    movel d1,d0\n";
    code_ << "    " << (isUnsigned ? "andl 65535,d0\n" : "extl d0\n");
}

void M68kCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
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

// `CMP.L <ea>,Dn` sets the real condition codes for `Dn - ea` in
// hardware (see M68kCodegen.h's own header comment) - `branchIfTrue`
// (already picked per signedness by genCompare() above - BHI/BLS/
// BCC/BCS test the carry flag for an unsigned compare, BGT/BLE/BGE/
// BLT the N^V combination for a signed one) is simply applied, no
// further fixup needed the way the 6502 backend's own comparisons do.
void M68kCodegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
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

// A wide (`long long`) operand's truthiness needs both words - `orl
// d1,d0` folds them together before the usual "cmpl 0,d0" (the
// combined value is discarded immediately after, so clobbering d0
// this way is safe).
void M68kCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void M68kCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void M68kCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orl d1,d0\n";
    code_ << "    cmpl 0,d0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movel 0,d0\n    bra " << Ldone << "\n";
    code_ << Ltrue << ":\n    movel 1,d0\n";
    code_ << Ldone << ":\n";
}

std::string M68kCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 68000 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M68kCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleM68k(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see M68kAssembler.h's own `assembleM68kObject`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM68kObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
