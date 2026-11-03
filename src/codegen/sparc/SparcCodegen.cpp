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

// A Local's or Param's own effective address, relative to `fp` - see
// SparcCodegen.h's own header comment and genFunction()'s own comment
// for the exact frame layout this implements. Never called for a
// Global (which keeps its own fixed, name-derived label instead -
// see loadAddress()/label()).
void SparcCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    int off = slotFrameByteOffset(slot);
    if (off == 0) {
        movReg(reg, "fp");
        return;
    }
    code_ << "    add fp," << off << "," << reg << "\n";
}

void SparcCodegen::loadImmediate(const std::string &reg, int32_t v) {
    if (v >= -4096 && v <= 4095) {
        code_ << "    or g0," << v << "," << reg << "\n";
        return;
    }
    uint32_t uv = static_cast<uint32_t>(v);
    uint32_t hi = (uv >> 10) & 0x3FFFFFu;
    uint32_t lo = uv & 0x3FFu;
    code_ << "    sethi " << hi << "," << reg << "\n";
    code_ << "    or " << reg << "," << lo << "," << reg << "\n";
}

// A literal element's own decimal text; a symbol element is the
// referenced Slot's or Function's own label, written out bare - "dw"
// (SparcAssembler.cpp's Line::Dw) already evaluates any expr operand,
// symbol included, and now also records the same external-name/base-
// shift relocation (kSparcRelocAbs32) a label used as `sethi`/`or`'s
// own split address immediate already gets.
std::string SparcCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

// Only ever called for a Global array now - its storage is one fixed
// trailer blob for the whole run, so its own initializer is just data,
// resolved once by the assembler/linker. A Local array instead gets a
// fresh frame slot on every call (see genFunction()'s own comment), so
// its own initializer has to be stored at runtime instead - see
// genLocalArrayInit() below.
void SparcCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

// Stores a Local array's own initializer into its freshly reserved
// frame slots, one element per call - see declareArray()'s own
// comment on why a Local can't just get a static trailer blob the way
// a Global does. Element i's own address is `slot`'s own base address
// (its first word, at `fp-4*frameOffset`) *minus* `4*i`, not plus -
// see genIndexAddr()'s own comment on exactly this same base-minus-
// scaled-index shape for a Local array's own indexed read/write.
void SparcCodegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
        } else {
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        int off = slotFrameByteOffset(slot) - 4 * static_cast<int>(i);
        if (off == 0) {
            movReg("t2", "fp");
        } else {
            code_ << "    add fp," << off << ",t2\n";
        }
        code_ << "    stind t2,t0\n";
    }
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

// Every function here gives its own call activation a real per-call
// frame on the hardware stack, addressed through `fp` - see
// SparcCodegen.h's own header comment on why a real register-window
// `save`/`restore` isn't used for this instead. The prologue:
//
//   sub sp,4,sp ; stind sp,lr  - push the return address `call` (or
//                                CallPtr's own `jmpl`) just left in
//                                `lr`, freeing `lr` for this
//                                function's own calls
//   sub sp,4,sp ; stind sp,fp  - push the caller's own `fp`
//   or g0,sp,fp                - fp now addresses this call's own
//                                saved-fp word
//   sub sp,4*frameSize,sp      - reserve this call's own Locals
//                                (only emitted when frameSize > 0)
//
// leaves exactly this layout relative to `fp`: `[fp+0]` the caller's
// own `fp`, `[fp+4]` the return address, `[fp+8+4*i]` the caller's
// own i'th pushed argument (see the Call case's own comment on why
// argument 0 ends up closest to `fp+8`) - loadSlotAddr() computes
// both the Param and the Local case from this same `fp`. Every word
// below this call's own reserved Locals is free for this call's own
// further `sub sp,4,sp`/`add sp,4,sp` temp spills (see genBinary()
// and friends) - always balanced, so `sp` is back at `fp-4*frameSize`
// by the time emitEpilogue() below runs.
//
// Because every call gets its own freshly pushed frame, a Local or
// Param is no longer one cell shared by every activation of a
// function - direct recursion (a function calling itself) and mutual
// recursion (two or more functions calling each other in a cycle)
// both get their own independent storage per call, limited only by
// how much of the real hardware stack is left (see kInitialSP), not
// by any fixed, call-depth-independent cell count.
void SparcCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,4,sp\n    stind sp,lr\n";
    code_ << "    sub sp,4,sp\n    stind sp,fp\n";
    code_ << "    or g0,sp,fp\n";
    if (fn.func->frameSize > 0) code_ << "    sub sp," << (4 * fn.func->frameSize) << ",sp\n";
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "    retl\n\n";
}

// Undoes genFunction()'s own prologue exactly, right before a `retl` -
// see that function's own comment for the frame layout this reads
// back. Order matters only in that `fp` itself must still hold *this*
// call's own frame address when `[fp+0]`/`[fp+4]` are read and when
// the new `sp` is computed from it - both happen before `fp` itself is
// overwritten with the caller's own value. Never touches `t0`, which
// by now already holds this function's own return value (an
// IR-level `void` function simply leaves `t0` as whatever it last
// was, same as every other backend's own untyped "ac").
void SparcCodegen::emitEpilogue() {
    code_ << "    add fp,4,t2\n    ldind t2,t1\n";  // t1 = saved lr, at [fp+4]
    code_ << "    ldind fp,t2\n";                   // t2 = saved fp, at [fp+0]
    code_ << "    add fp,8,sp\n";                   // sp = fp+8 - the caller's own post-push sp
    code_ << "    or g0,t1,lr\n";
    code_ << "    or g0,t2,fp\n";
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    cmp t0,0\n";
            code_ << "    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    ba " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    ba " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    ba " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // A `return` statement is its own early exit from the
            // function, bypassing genFunction()'s own fall-through
            // epilogue entirely, so it needs that same frame teardown
            // inlined here too - see emitEpilogue()'s own comment.
            emitEpilogue();
            code_ << "    retl\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadSlotAddr("t2", s.declSlot);
                code_ << "    stind t2,t0\n";
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
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
            return;
        }
        case ir::ExprKind::Var: {
            if (e.slot->kind == ir::SlotKind::Global) {
                loadAddress("t2", label(e.slot));
            } else {
                loadSlotAddr("t2", e.slot);
            }
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
            if (e.slot->kind == ir::SlotKind::Global) {
                loadAddress("t2", label(e.slot));
            } else {
                loadSlotAddr("t2", e.slot);
            }
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
            // Pushed right-to-left, so argument 0 ends up closest to
            // the stack top at the moment `call` runs - exactly the
            // same cdecl-style shape as I386Codegen.cpp's own `Call`
            // case, just with "push" spelled out as this dialect's
            // own `sub sp,4,sp`/`stind sp,t0` pair. Once the callee's
            // own prologue has pushed `lr` and `fp` (see genFunction()'s
            // own comment), argument 0 sits at `fp+8`, argument i at
            // `fp+8+4*i` - see loadSlotAddr()'s own Param case. The
            // caller cleans the arguments back off afterwards (`call`
            // itself never touches `sp`).
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    sub sp,4,sp\n    stind sp,t0\n";
            }
            code_ << "    call " << label(e.func) << "\n";
            if (!e.args.empty()) code_ << "    add sp," << (4 * e.args.size()) << ",sp\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): the callee isn't known
            // until runtime for this kind, so there's no declared
            // parameter list a caller could even match its own pushed
            // arguments against.
            // "jmpl t0,lr" is the real SPARC JMPL instruction
            // (SparcAssembler.cpp's own new `jmpl rs1,rd` mnemonic):
            // it transfers control to whatever address t0 holds and
            // leaves the address of this jmpl instruction itself in
            // lr, exactly the same return-address convention a direct
            // `call` already leaves there (see
            // SparcAssembler.cpp's own `encodeRetl()` comment on
            // `retl`'s "lr+8" assumption) - so a function reached this
            // way returns correctly with the exact same `retl`
            // epilogue every function already uses regardless of how
            // it was called. The assembler synthesizes the
            // real delay-slot `nop` right after it automatically,
            // same as `call` already relies on.
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    jmpl t0,lr\n";
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
// 0` means the base is a compile-time-known address (a Global's own
// label, or a Local's own `fp`-relative address - either way, the
// array's first word); otherwise `slot` is a plain pointer variable,
// so the base instead comes from *reading* that variable's own stored
// value first (itself at a Global's label or a Local/Param's
// `fp`-relative address, same distinction as every other slot access -
// see loadSlotAddr()).
//
// A Global array's elements sit at *increasing* addresses as the
// index grows (`label+i*4`, the ordinary layout); a Local array's
// elements instead sit at *decreasing* addresses from its own first
// word (`base-i*4`), because `base` itself is already `fp-4*
// frameOffset` and later elements (index i>0) occupy the *larger*
// frameOffset values `frameOffset+i` (see IR.h's own comment on
// Slot::frameOffset), which this backend's own `fp-4*frameOffset`
// addressing turns into the *smaller* (more negative) address. Adding
// `i*4` here instead of subtracting it would silently read/write the
// wrong word for any index past 0 - the same bug this migration
// specifically guards against on every other frame-pointer-based
// backend (see e.g. I386Codegen.cpp's own `computeElementAddr()`).
void SparcCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);              // t0 = index
    code_ << "    sll t0,2,t0\n";  // t0 *= 4 (words are 4 bytes on this target)
    bool isGlobal = slot->kind == ir::SlotKind::Global;
    if (slot->arraySize > 0) {
        if (isGlobal) {
            loadAddress("t2", label(slot));
        } else {
            loadSlotAddr("t2", slot);
        }
        code_ << "    " << (isGlobal ? "add" : "sub") << " t2,t0,t2\n";
    } else {
        if (isGlobal) {
            loadAddress("t1", label(slot));
        } else {
            loadSlotAddr("t1", slot);
        }
        code_ << "    ldind t1,t1\n";  // t1 = the pointer variable's own stored value
        code_ << "    add t1,t0,t2\n";
    }
}

void SparcCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldind t2,t0\n";
}

void SparcCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            loadAddress("t2", label(operand.slot));
        } else {
            loadSlotAddr("t2", operand.slot);
        }
        movReg("t0", "t2");
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        movReg("t0", "t2");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
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
