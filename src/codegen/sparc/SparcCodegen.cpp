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

// Where the whole program is loaded and starts executing; the simulator sets the CPU's
// PC here directly. Must match SparcAssembler.h's kSparcBaseAddress.
constexpr uint32_t kBaseAddress = 0x00004000u;

// Where the entry sequence points `sp` before calling main() - above the loaded program
// and its data, growing down into unused RAM.
constexpr uint32_t kInitialSP = 0x00100000u;
}  // namespace

// A Local's or Param's effective address, relative to `fp` - see genFunction() for the
// frame layout. Never called for a Global, which uses a fixed label instead.
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

// A literal element's decimal text; a symbol element is the referenced Slot's or
// Function's label, written out bare - `dw` evaluates and relocates it as kSparcRelocAbs32.
std::string SparcCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

// Only called for a Global array - its storage is one fixed trailer blob, resolved once
// by the assembler/linker. A Local array gets a fresh frame slot per call instead, so its
// initializer is stored at runtime (see genLocalArrayInit()). `elemType`'s width picks
// `db`/`dh`/`dw` (a `long long` element, `w==8`, splits into two `dw`s: low word, high).
void SparcCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                   Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<uint32_t>(v) << ", " << static_cast<uint32_t>(v >> 32);
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
}

// Stores a Local array's initializer into its freshly reserved frame slots, one element
// per call. Element i's address is the array's base, its lowest address, plus `w*i`,
// matching genIndexAddr()'s base-plus-scaled-index addressing.
void SparcCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int disp = slotFrameByteOffset(slot) + w * static_cast<int>(i);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
            code_ << "    stind fp,t0," << disp << "\n";
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
            code_ << "    stind fp,t0," << (disp + 4) << "\n";
            continue;
        }
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
        } else {
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        if (w == 1) {
            code_ << "    stindb fp,t0," << disp << "\n";
        } else if (w == 2) {
            code_ << "    stindh fp,t0," << disp << "\n";
        } else {
            code_ << "    stind fp,t0," << disp << "\n";
        }
    }
}

void SparcCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    sethi " << lbl << "," << reg << "\n";
    code_ << "    or " << reg << "," << lbl << "," << reg << "\n";
}

void SparcCodegen::movReg(const std::string &dst, const std::string &src) {
    code_ << "    or g0," << src << "," << dst << "\n";
}

// Loads/stores `slot`'s scalar value - a Global goes through its fixed label address
// plus loadOperand()/storeOperand(); a Local or Param goes directly through
// loadOperandDisp()/storeOperandDisp() at `slotFrameByteOffset()`.
void SparcCodegen::loadSlotValue(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress("t2", label(slot));
        loadOperand("t2", slot->type);
        return;
    }
    loadOperandDisp(slotFrameByteOffset(slot), slot->type);
}

void SparcCodegen::storeSlotValue(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress("t2", label(slot));
        storeOperand("t2", slot->type);
        return;
    }
    storeOperandDisp(slotFrameByteOffset(slot), slot->type);
}

std::string SparcCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation means `prog` may not define `main` at all. In
    // `objectMode`, no object file emits this entry sequence itself - the linker
    // synthesizes exactly one once every object is merged and `main`'s address is
    // resolvable. An object file's base is `org 0`; placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        uint32_t hi = (kInitialSP >> 10) & 0x3FFFFFu;
        uint32_t lo = kInitialSP & 0x3FFu;
        code_ << "    sethi " << hi << ",sp\n";
        code_ << "    or sp," << lo << ",sp\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The result sits in t0 the moment execution reaches `halt`.
        code_ << "halt:\n    ba halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file - it must never get a storage
        // word here, only be referenced by name, letting the assembler defer it to a
        // relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + directive + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> SparcCodegen::perFileAsm() const { return perFileAsm_; }

// Every function gives its own call activation a per-call frame on the hardware stack,
// addressed through `fp`. The prologue:
//
//   sub sp,4,sp ; stind sp,lr  - push the return address left in `lr`, freeing it for
//                                this function's own calls
//   sub sp,4,sp ; stind sp,fp  - push the caller's `fp`
//   or g0,sp,fp                - fp now addresses this call's saved-fp word
//   sub sp,4*frameSize,sp      - reserve this call's Locals (only when frameSize > 0)
//
// leaves this layout relative to `fp`: `[fp+0]` the caller's `fp`, `[fp+4]` the return
// address, `[fp+8+4*i]` the caller's i'th pushed argument - loadSlotAddr()/
// loadOperandDisp()/storeOperandDisp() compute both the Param and Local case from this
// `fp`. Every word below the reserved Locals is free for temp spills, always balanced so
// `sp` is back at `fp-frameSize` by the time emitEpilogue() runs.
//
// Because every call gets its own pushed frame, direct and mutual recursion each get
// independent storage per call, limited only by the hardware stack (kInitialSP).
void SparcCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,4,sp\n    stind sp,lr\n";
    code_ << "    sub sp,4,sp\n    stind sp,fp\n";
    code_ << "    or g0,sp,fp\n";
    if (fn.func->frameSize > 0) code_ << "    sub sp," << fn.func->frameSize << ",sp\n";
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "    retl\n\n";
}

// Undoes genFunction()'s prologue, right before a `retl`. `fp` must still hold this
// call's frame address when `[fp+0]`/`[fp+4]` are read and the new `sp` computed, before
// `fp` itself is overwritten. Never touches `t0`/`t1`, which already hold the return
// value by now.
void SparcCodegen::emitEpilogue() {
    code_ << "    add fp,4,t2\n    ldind t2,t3\n";  // t3 = saved lr, at [fp+4]
    code_ << "    ldind fp,t2\n";                   // t2 = saved fp, at [fp+0]
    code_ << "    add fp,8,sp\n";                   // sp = fp+8 - the caller's own post-push sp
    code_ << "    or g0,t3,lr\n";
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
            if (isWide(s.cond->type)) code_ << "    or t0,t0,t1\n";
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
            if (isWide(s.cond->type)) code_ << "    or t0,t0,t1\n";
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
            // An early exit bypasses genFunction()'s fall-through epilogue, so the same
            // frame teardown is inlined here too.
            emitEpilogue();
            code_ << "    retl\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeSlotValue(s.declSlot);
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
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "    ba " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void SparcCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
                loadImmediate("t1", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
                return;
            }
            loadImmediate("t0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotValue(e.slot);
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
                storeOperand("t2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            storeSlotValue(e.slot);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness - the pointee type of a typed pointer,
            // or a plain int word for untyped-int-as-address. storeOperand() handles a
            // wide (8-byte) pointee too, storing t0/t1 at addrReg/addrReg+4.
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    sub sp,4,sp\n    stind sp,t0\n";
            genExpr(*e.rhs);  // t0, or t1:t0 if wide
            code_ << "    ldind sp,t2\n    add sp,4,sp\n";
            storeOperand("t2", e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // t2 = address
                genCompoundThroughT2(e);
                return;
            }
            // Plain scalar target - loadSlotValue()/storeSlotValue() address it afresh
            // with no side effect, so there's no address to hold across `e.rhs`'s
            // evaluation.
            genExpr(*e.rhs);  // t0 = rhs
            movReg("t1", "t0");  // t1 = rhs
            loadSlotValue(e.slot);  // t0 = old
            if (e.yieldOld) code_ << "    sub sp,4,sp\n    stind sp,t0\n";
            genCompoundOp(e.op, e.type);
            narrowT0To(e.type);
            storeSlotValue(e.slot);
            if (e.yieldOld) code_ << "    ldind sp,t0\n    add sp,4,sp\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            movReg("t2", "t0");  // t2 = address
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    or t0,t0,t1\n";
            code_ << "    cmp t0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    ba " << Ldone << "\n";
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
                    code_ << "    sub g0,t0,t0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // t0 = pointer value
                movReg("t2", "t0");
                loadOperand("t2", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    xor t0,-1,t0\n";
                if (isWide(e.rhs->type)) code_ << "    xor t1,-1,t1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Call: {
            // Pushed right-to-left, so argument 0 ends up closest to the stack top when
            // `call` runs. Once the callee's prologue has pushed `lr` and `fp`, argument
            // 0 sits at `fp+8`, argument i at `fp+8+4*i`. The caller cleans the arguments
            // off afterwards (`call` never touches `sp`). A wide (`long long`) argument
            // pushes both words, high first, so its low word ends up at the lower
            // address, matching a wide Param's frameOffset/frameOffset+4 layout.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    sub sp,8,sp\n    stind sp,t1\n    stind sp,t0,4\n";
                    totalBytes += 8;
                } else {
                    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
                    totalBytes += 4;
                }
            }
            code_ << "    call " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    add sp," << totalBytes << ",sp\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - the callee isn't known until runtime, so
            // there's no declared parameter list to match pushed arguments against.
            // "jmpl t0,lr" transfers control to whatever address t0 holds and leaves the
            // address of this instruction in lr, the same return-address convention a
            // direct `call` leaves, so the callee's `retl` epilogue returns correctly
            // regardless of how it was called.
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    jmpl t0,lr\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No trap table is set up on this bare-metal target, so interrupt
            // enable/disable is a no-op.
            (void)e;
            return;
        }
    }
}

// `t2` holds the computed address. `arraySize > 0` means the base is a compile-time-known
// address (a Global's label, or a Local's `fp`-relative address - the array's first
// byte); otherwise `slot` is a plain pointer variable, so the base comes from reading
// that variable's own stored value first.
//
// `elemShift(slot->type)` scales the index by the element width (1/2/4/8 bytes). A
// Global or Local array's elements sit at increasing addresses as the index grows
// (`base+i*w`), a Local array's base being its lowest address, element 0.
void SparcCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    sll t0," << shift << ",t0\n";
    bool isGlobal = slot->kind == ir::SlotKind::Global;
    if (slot->arraySize > 0) {
        if (isGlobal) {
            loadAddress("t2", label(slot));
        } else {
            loadSlotAddr("t2", slot);
        }
        code_ << "    add t2,t0,t2\n";
    } else {
        if (isGlobal) {
            loadAddress("t1", label(slot));
            code_ << "    ldind t1,t1\n";  // t1 = the pointer variable's own stored value
        } else {
            loadOperandDisp(slotFrameByteOffset(slot), Type{BaseType::Int, false});
            movReg("t1", "t0");
        }
        code_ << "    add t1,t0,t2\n";
    }
}

void SparcCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("t2", e.slot->type);
}

void SparcCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add t0,t1,t0\n"; return; }
    if (op == "-") { code_ << "    sub t0,t1,t0\n"; return; }
    if (op == "*") { code_ << "    smul t0,t1,t0\n"; return; }
    if (op == "&") { code_ << "    and t0,t1,t0\n"; return; }
    if (op == "|") { code_ << "    or t0,t1,t0\n"; return; }
    if (op == "^") { code_ << "    xor t0,t1,t0\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "sll" : (isUnsigned ? "srl" : "sra");
        code_ << "    " << insn << " t0,t1,t0\n";
        return;
    }
    // "/" or "%" - t5 is free scratch here, used to swap old/rhs into t1/t0 so the same
    // divide sequence as genDivMod() applies unchanged. Clobbers t2; callers that still
    // need t2 afterward must preserve it around this call.
    movReg("t5", "t0");  // t5 = old
    movReg("t0", "t1");  // t0 = rhs (divisor)
    movReg("t1", "t5");  // t1 = old (dividend)
    if (isUnsigned) {
        code_ << "    wry g0,g0\n";
    } else {
        code_ << "    sra t1,31,t2\n    wry t2,g0\n";
    }
    const char *divInsn = isUnsigned ? "udiv" : "sdiv";
    const char *mulInsn = isUnsigned ? "umul" : "smul";
    if (op == "%") {
        code_ << "    " << divInsn << " t1,t0,t2\n";  // t2 = quotient (old/rhs)
        code_ << "    " << mulInsn << " t2,t0,t2\n";   // t2 = quotient * rhs
        code_ << "    sub t1,t2,t0\n";                  // t0 = old - quotient*rhs = remainder
    } else {
        code_ << "    " << divInsn << " t1,t0,t0\n";
    }
}

// t2 must already hold the resolved address on entry, with nothing else live; it is
// stashed on the stack across `e.rhs`'s evaluation and again around genCompoundOp(),
// since either can clobber it.
void SparcCodegen::genCompoundThroughT2(const ir::Expr &e) {
    code_ << "    sub sp,4,sp\n    stind sp,t2\n";
    genExpr(*e.rhs);                 // t0 = rhs
    movReg("t1", "t0");                // t1 = rhs, held across the t2 reload below
    code_ << "    ldind sp,t2\n    add sp,4,sp\n";  // t2 = address
    loadOperand("t2", e.type);            // t0 = old
    if (e.yieldOld) code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    code_ << "    sub sp,4,sp\n    stind sp,t2\n";    // re-stash the address around genCompoundOp()
    genCompoundOp(e.op, e.type);              // t0 = new (raw)
    narrowT0To(e.type);
    code_ << "    ldind sp,t2\n    add sp,4,sp\n";       // t2 = address restored
    if (e.yieldOld) {
        movReg("t3", "t0");                       // t3 = new
        code_ << "    ldind sp,t0\n    add sp,4,sp\n";  // t0 = old
        movReg("t4", "t0");                           // t4 = old, preserved across the store below
        movReg("t0", "t3");                            // t0 = new (storeOperand always stores from t0)
        storeOperand("t2", e.type);                       // t2 still addresses the target
        movReg("t0", "t4");                                // t0 = old (final result)
    } else {
        storeOperand("t2", e.type);  // t0 already = new; t2 still addresses the target
    }
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

// Loads a value of `type` from the address already in `addrReg` into `t0` (or `t1:t0`
// for a `long long`), using this dialect's sign-/zero-extending byte/halfword load
// mnemonics for a narrower-than-int source.
void SparcCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb" : "ldindsb") << " " << addrReg << ",t0\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindh" : "ldindsh") << " " << addrReg << ",t0\n";
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",t0\n";
    } else {  // 8 - long long: t1:t0 (hi:lo), low word first
        code_ << "    ldind " << addrReg << ",t0\n";
        code_ << "    ldind " << addrReg << ",t1,4\n";
    }
}

void SparcCodegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",t0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",t0\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << ",t0\n";
    } else {
        code_ << "    stind " << addrReg << ",t0\n";
        code_ << "    stind " << addrReg << ",t1,4\n";
    }
}

// The same loads/stores as loadOperand()/storeOperand(), but directly at `fp+disp`
// (no separate address computation) - the Local/Param fast path, folding `disp` into
// the load/store instruction's own displacement field.
void SparcCodegen::loadOperandDisp(int disp, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb" : "ldindsb") << " fp,t0," << disp << "\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindh" : "ldindsh") << " fp,t0," << disp << "\n";
    } else if (w == 4) {
        code_ << "    ldind fp,t0," << disp << "\n";
    } else {
        code_ << "    ldind fp,t0," << disp << "\n";
        code_ << "    ldind fp,t1," << (disp + 4) << "\n";
    }
}

void SparcCodegen::storeOperandDisp(int disp, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb fp,t0," << disp << "\n";
    } else if (w == 2) {
        code_ << "    stindh fp,t0," << disp << "\n";
    } else if (w == 4) {
        code_ << "    stind fp,t0," << disp << "\n";
    } else {
        code_ << "    stind fp,t0," << disp << "\n";
        code_ << "    stind fp,t1," << (disp + 4) << "\n";
    }
}

void SparcCodegen::narrowT0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    sll t0,24,t0\n";
        code_ << "    " << (to.isUnsigned ? "srl" : "sra") << " t0,24,t0\n";
    } else if (w == 2) {
        code_ << "    sll t0,16,t0\n";
        code_ << "    " << (to.isUnsigned ? "srl" : "sra") << " t0,16,t0\n";
    }
    // 4 bytes (Int/Long): t0 already holds the full value. 8 never reaches here.
}

// An implicit or explicit type conversion. Every arithmetic/comparison operand narrower
// than `int` is already promoted by IRBuilder, so the only work here is crossing the
// int/`long long` boundary (sign/zero-extending t0 into t1, or dropping t1), or
// re-truncating+re-extending t0 for a narrower target.
void SparcCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // t0, or t1:t0 if fromWide
    if (fromWide && !toWide) {
        narrowT0To(to);  // t1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    or g0,0,t1\n";
        } else {
            code_ << "    sra t0,31,t1\n";  // sign-extend t0 into t1
        }
    } else if (!fromWide && !toWide) {
        narrowT0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct.
}

// `long long` add/sub (lhs OP rhs) - `cmp` only feeds a conditional branch, never a
// carry-consuming add, so the carry/borrow out of the low-word operation is recovered
// via an unsigned `cmp`+branch test deciding whether the high-word add needs +1.
void SparcCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);  // t1:t0 = lhs
    code_ << "    sub sp,8,sp\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t1:t0 = rhs
    code_ << "    or g0,t0,t3\n    or g0,t1,t4\n";  // t3:t4 = rhs (lo:hi)
    code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    add sp,8,sp\n";  // t1:t0 = lhs
    std::string Lcarry = newLabel(), Ldone = newLabel();
    if (isAdd) {
        code_ << "    add t0,t3,t5\n";              // t5 = lo sum
        code_ << "    cmp t5,t0\n    bgeu " << Lcarry << "\n";  // unsigned: sum >= lhs_lo means no carry
        code_ << "    or g0,1,t6\n    ba " << Ldone << "\n";
        code_ << Lcarry << ":\n    or g0,0,t6\n";
        code_ << Ldone << ":\n";
        code_ << "    add t1,t4,t1\n    add t1,t6,t1\n";
        code_ << "    or g0,t5,t0\n";
    } else {
        code_ << "    cmp t0,t3\n    bgeu " << Lcarry << "\n";  // unsigned: lhs_lo >= rhs_lo means no borrow
        code_ << "    or g0,1,t6\n    ba " << Ldone << "\n";
        code_ << Lcarry << ":\n    or g0,0,t6\n";
        code_ << Ldone << ":\n";
        code_ << "    sub t0,t3,t5\n";
        code_ << "    sub t1,t4,t1\n";
        code_ << "    sub t1,t6,t1\n";
        code_ << "    or g0,t5,t0\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit product - the
// standard 32-on-32 cross-multiply decomposition, using `smul` three times (each a
// 32x32->64 product, high half read back with `rdy`) and discarding each cross term's
// high half. The low-order 32 bits of a product don't depend on signedness, so plain
// `smul` (not `umul`) is used throughout.
void SparcCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);  // t1:t0 = lhs
    code_ << "    sub sp,8,sp\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t1:t0 = rhs
    code_ << "    or g0,t0,t5\n    or g0,t1,t6\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldind sp,t4,4\n    add sp,8,sp\n";  // t3:t4 = lhs (lo:hi)
    code_ << "    smul t3,t6,t0\n";  // lhs_lo*rhs_hi
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    code_ << "    smul t4,t5,t0\n";  // lhs_hi*rhs_lo
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    code_ << "    smul t3,t5,t0\n    rdy t1\n";  // lhs_lo*rhs_lo (final result_lo/carry)
    code_ << "    ldind sp,t6\n    add sp,4,sp\n";   // crossB_lo (lhs_hi*rhs_lo)
    code_ << "    ldind sp,t5\n    add sp,4,sp\n";   // crossA_lo (lhs_lo*rhs_hi)
    code_ << "    add t1,t6,t1\n    add t1,t5,t1\n";
}

void SparcCodegen::genNegate64() {
    code_ << "    xor t0,-1,t0\n    xor t1,-1,t1\n";
    code_ << "    add t0,1,t0\n";
    std::string Lcarry = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    bne " << Lcarry << "\n";
    code_ << "    add t1,1,t1\n    ba " << Ldone << "\n";
    code_ << Lcarry << ":\n";
    code_ << Ldone << ":\n";
}

// Classic bit-serial shift/compare/subtract long division, using the stack for scratch
// and displacement-operand `ldind`/`stind` throughout. `isUnsigned` false divides
// magnitudes: lhs/rhs sign bits are read out of the pushed operand words, each negated
// in place if set, then restored on the result per C's truncating-division rule.
void SparcCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,8,sp\n    stind sp,t1\n    stind sp,t0,4\n";  // [sp]=lhs_hi,[sp+4]=lhs_lo
    genExpr(rhs);
    code_ << "    sub sp,8,sp\n    stind sp,t1\n    stind sp,t0,4\n";  // [sp]=rhs_hi,[sp+4]=rhs_lo
    code_ << "    sub sp,36,sp\n";
    // Scratch layout at sp+0: n_lo,n_hi,d_lo,d_hi,r_lo,r_hi,q_lo,q_hi,cnt (9 words, 36 bytes)
    code_ << "    ldind sp,t0,44\n    stind sp,t0,0\n";   // n_lo = lhs_lo
    code_ << "    ldind sp,t0,40\n    stind sp,t0,4\n";   // n_hi = lhs_hi
    code_ << "    ldind sp,t0,36\n    stind sp,t0,8\n";   // d_lo = rhs_lo
    code_ << "    ldind sp,t0,32\n    stind sp,t0,12\n";  // d_hi = rhs_hi
    code_ << "    stind sp,g0,16\n    stind sp,g0,20\n";  // r = 0
    code_ << "    stind sp,g0,24\n    stind sp,g0,28\n";  // q = 0
    code_ << "    or g0,32,t0\n    stind sp,t0,32\n";     // cnt = 32 (per-word, so 2*32=64 total)

    if (!isUnsigned) {
        code_ << "    ldind sp,t0,4\n    srl t0,31,t0\n    stind sp,t0,40\n";    // [sp+40] = lhsSign
        code_ << "    ldind sp,t0,12\n    srl t0,31,t0\n    stind sp,t0,44\n";   // [sp+44] = rhsSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    ldind sp,t0,40\n    cmp t0,0\n    beq " << LskipNegN << "\n";
        code_ << "    ldind sp,t0,0\n    ldind sp,t1,4\n";
        genNegate64();
        code_ << "    stind sp,t0,0\n    stind sp,t1,4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    ldind sp,t0,44\n    cmp t0,0\n    beq " << LskipNegD << "\n";
        code_ << "    ldind sp,t0,8\n    ldind sp,t1,12\n";
        genNegate64();
        code_ << "    stind sp,t0,8\n    stind sp,t1,12\n";
        code_ << LskipNegD << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldind sp,t0,32\n    cmp t0,0\n    beq " << Ldone << "\n";
    code_ << "    sub t0,1,t0\n    stind sp,t0,32\n";

    // n <<= 1 (0-filled), capturing its own old top bit into t3.
    code_ << "    ldind sp,t0,4\n    srl t0,31,t0\n    or g0,t0,t3\n";
    code_ << "    ldind sp,t0,0\n    srl t0,31,t0\n    or g0,t0,t4\n";
    code_ << "    ldind sp,t0,0\n    sll t0,1,t0\n    stind sp,t0,0\n";
    code_ << "    ldind sp,t0,4\n    sll t0,1,t0\n    or t0,t4,t0\n    stind sp,t0,4\n";

    // r = (r << 1) | (n's old top bit, in t3).
    code_ << "    ldind sp,t0,16\n    srl t0,31,t0\n    or g0,t0,t5\n";
    code_ << "    ldind sp,t0,16\n    sll t0,1,t0\n    or t0,t3,t0\n    stind sp,t0,16\n";
    code_ << "    ldind sp,t0,20\n    sll t0,1,t0\n    or t0,t5,t0\n    stind sp,t0,20\n";

    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 }
    code_ << "    ldind sp,t0,20\n    ldind sp,t1,12\n    cmp t0,t1\n    bne " << LhiDiff << "\n";
    code_ << "    ldind sp,t0,16\n    ldind sp,t1,8\n    cmp t0,t1\n    bltu " << LsubNo << "\n";
    code_ << "    ba " << LsubYes << "\n";
    code_ << LhiDiff << ":\n";
    code_ << "    cmp t0,t1\n    bltu " << LsubNo << "\n";
    code_ << "    ba " << LsubYes << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    ldind sp,t0,16\n    ldind sp,t1,8\n    cmp t0,t1\n    bgeu " << LshiftQ << "_nb\n";
    code_ << "    or g0,1,t6\n    ba " << LshiftQ << "_bdone\n";
    code_ << LshiftQ << "_nb:\n    or g0,0,t6\n";
    code_ << LshiftQ << "_bdone:\n";
    code_ << "    sub t0,t1,t0\n    stind sp,t0,16\n";
    code_ << "    ldind sp,t0,20\n    ldind sp,t1,12\n    sub t0,t1,t0\n    sub t0,t6,t0\n    stind sp,t0,20\n";
    code_ << "    or g0,1,t3\n    ba " << LshiftQ << "\n";
    code_ << LsubNo << ":\n    or g0,0,t3\n";
    code_ << LshiftQ << ":\n";

    // q = (q << 1) | quotient bit (in t3).
    code_ << "    ldind sp,t0,24\n    srl t0,31,t0\n    or g0,t0,t5\n";
    code_ << "    ldind sp,t0,24\n    sll t0,1,t0\n    or t0,t3,t0\n    stind sp,t0,24\n";
    code_ << "    ldind sp,t0,28\n    sll t0,1,t0\n    or t0,t5,t0\n    stind sp,t0,28\n";
    code_ << "    ba " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    ldind sp,t3,40\n    ldind sp,t0,44\n    xor t3,t0,t3\n";
        code_ << "    cmp t3,0\n    beq " << LskipNegQ << "\n";
        code_ << "    ldind sp,t0,24\n    ldind sp,t1,28\n";
        genNegate64();
        code_ << "    stind sp,t0,24\n    stind sp,t1,28\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    ldind sp,t3,40\n    cmp t3,0\n    beq " << LskipNegR << "\n";
        code_ << "    ldind sp,t0,16\n    ldind sp,t1,20\n";
        genNegate64();
        code_ << "    stind sp,t0,16\n    stind sp,t1,20\n";
        code_ << LskipNegR << ":\n";
    }
    code_ << "    ldind sp,t0,24\n    ldind sp,t1,28\n";  // quotient
    code_ << "    ldind sp,t4,16\n    ldind sp,t5,20\n";  // remainder (saved past the scratch release)
    code_ << "    add sp,52,sp\n";  // release the 9-word scratch + the 4 earlier stashed operand words
    if (wantRemainder) {
        code_ << "    or g0,t4,t0\n    or g0,t5,t1\n";
    }
}

// A `long long` shifted by a runtime count - a counted loop of 1-bit shifts, each
// carrying the bit that crosses the word boundary by hand.
void SparcCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,8,sp\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t0 = shift count (already promoted to a plain int)
    code_ << "    or g0,t0,t3\n";
    code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    add sp,8,sp\n";  // t1:t0 restored = the value being shifted
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    cmp t3,0\n    beq " << Ldone << "\n    sub t3,1,t3\n";
    if (isLeft) {
        code_ << "    srl t0,31,t4\n";       // t4 = t0's old top bit, carries into t1
        code_ << "    sll t0,1,t0\n";
        code_ << "    sll t1,1,t1\n    or t1,t4,t1\n";
    } else {
        code_ << "    sll t1,31,t4\n";       // t4 = (t1&1)<<31, carries into t0's top
        code_ << "    srl t0,1,t0\n    or t0,t4,t0\n";
        code_ << "    " << (isUnsigned ? "srl" : "sra") << " t1,1,t1\n";
    }
    code_ << "    ba " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison - compare the high words first (signed or unsigned per
// `isUnsigned`), falling back to an unsigned compare of the low words only when the high
// words are equal.
void SparcCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs,
                                     bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,8,sp\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);
    code_ << "    or g0,t0,t5\n    or g0,t1,t6\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldind sp,t4,4\n    add sp,8,sp\n";  // t3:t4 = lhs (lo:hi)
    if (op == "==" || op == "!=") {
        std::string Lfalse = newLabel(), Ldone = newLabel();
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmp t3,t5\n    bne " << Lfalse << "\n";
        code_ << "    cmp t4,t6\n    bne " << Lfalse << "\n";
        code_ << "    or g0," << onEqual << ",t0\n    ba " << Ldone << "\n";
        code_ << Lfalse << ":\n    or g0," << onDiffer << ",t0\n";
        code_ << Ldone << ":\n";
        return;
    }
    // t0 = (hi differ) ? hiCompare(t4,t6) : loCompareUnsigned(t3,t5) - once the high
    // words are equal, the remaining 32 bits are a plain unsigned magnitude.
    std::string LhiDiff = newLabel(), Ldone = newLabel(), Lt = newLabel(), Lf = newLabel();
    code_ << "    cmp t4,t6\n    bne " << LhiDiff << "\n";
    std::string loBranch = op == "<" ? "bltu" : op == ">" ? "bgtu" : op == "<=" ? "bleu" : "bgeu";
    code_ << "    cmp t3,t5\n    " << loBranch << " " << Lt << "\n    ba " << Lf << "\n";
    code_ << Lt << ":\n    or g0,1,t0\n    ba " << Ldone << "\n";
    code_ << Lf << ":\n    or g0,0,t0\n    ba " << Ldone << "\n";
    code_ << LhiDiff << ":\n";
    std::string hiBranch;
    if (isUnsigned) {
        hiBranch = loBranch;
    } else {
        hiBranch = op == "<" ? "blt" : op == ">" ? "bgt" : op == "<=" ? "ble" : "bge";
    }
    code_ << "    cmp t4,t6\n    " << hiBranch << " " << Lt << "\n    ba " << Lf << "\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand already shares one common type;
    // `e.lhs->type` names it, except for shift (only the LHS's type matters) and &&/||
    // (width-independent, handled by genLogicalAnd/Or below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
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
        const char *insn = op == "&" ? "and" : op == "|" ? "or" : "xor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    sub sp,8,sp\n    stind sp,t0\n    stind sp,t1,4\n";
            genExpr(*e.rhs);
            code_ << "    or g0,t0,t3\n    or g0,t1,t4\n";
            code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    add sp,8,sp\n";
            code_ << "    " << insn << " t0,t3,t0\n    " << insn << " t1,t4,t1\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    sub sp,4,sp\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    add sp,4,sp\n";
        code_ << "    " << insn << " t1,t0,t0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    sub sp,4,sp\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        movReg("t1", "t0");
        code_ << "    ldind sp,t0\n    add sp,4,sp\n";  // t0 = value to shift
        const char *insn = op == "<<" ? "sll" : (isUnsigned ? "srl" : "sra");
        code_ << "    " << insn << " t0,t1,t0\n";
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

// Real hardware multiply (`smul`) - a full 32x32->32 product. Signedness doesn't matter
// for the low-order 32 bits `smul` keeps.
void SparcCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
    code_ << "    smul t1,t0,t0\n";
}

// Real hardware divide (`sdiv`/`udiv`) gives only the quotient, treating the dividend as
// a 64-bit `Y:rs1` pair: `sra t1,31,t2` then `wry t2,g0` seeds Y with the dividend's sign
// bits for the signed form (skipping this gives a wrong result for a negative dividend),
// or `wry g0,g0` seeds an all-zero high half for unsigned. There is no remainder
// instruction, so `%` is synthesized as `sdiv`/`udiv` for the quotient, `smul`/`umul` to
// multiply back out, then `sub` to recover `dividend - quotient * divisor`.
void SparcCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    if (isUnsigned) {
        code_ << "    wry g0,g0\n";
    } else {
        code_ << "    sra t1,31,t2\n    wry t2,g0\n";
    }
    const char *divInsn = isUnsigned ? "udiv" : "sdiv";
    const char *mulInsn = isUnsigned ? "umul" : "smul";
    if (!wantRemainder) {
        code_ << "    " << divInsn << " t1,t0,t0\n";
        return;
    }
    code_ << "    " << divInsn << " t1,t0,t2\n";   // t2 = quotient (t1/t0)
    code_ << "    " << mulInsn << " t2,t0,t2\n";   // t2 = quotient * divisor
    code_ << "    sub t1,t2,t0\n";    // t0 = t1 - t2 = dividend - quotient*divisor
}

// SPARC has no compare-and-set instruction - a comparison needs `cmp` (`subcc
// rs1,rs2,%g0`) plus a conditional branch to decide which boolean value to materialize.
// `isUnsigned` picks the plain or unsigned branch family for ordered comparisons;
// `==`/`!=` need no such choice.
void SparcCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    sub sp,4,sp\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    add sp,4,sp\n";  // t1 = lhs, t0 = rhs
    code_ << "    cmp t1,t0\n";
    std::string branch;
    if (op == "<") branch = isUnsigned ? "bltu" : "blt";
    else if (op == ">") branch = isUnsigned ? "bgtu" : "bgt";
    else if (op == "<=") branch = isUnsigned ? "bleu" : "ble";
    else if (op == ">=") branch = isUnsigned ? "bgeu" : "bge";
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
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    cmp t0,0\n    beq " << Lfalse << "\n";
    code_ << "    ba " << Ltrue << "\n";
    code_ << Lfalse << ":\n    or g0,0,t0\n    ba " << Ldone << "\n";
    code_ << Ltrue << ":\n    or g0,1,t0\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp t0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    cmp t0,0\n    bne " << Ltrue << "\n";
    code_ << "    ba " << Lfalse << "\n";
    code_ << Ltrue << ":\n    or g0,1,t0\n    ba " << Ldone << "\n";
    code_ << Lfalse << ":\n    or g0,0,t0\n";
    code_ << Ldone << ":\n";
}

void SparcCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
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
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral ObjectFile, then
    // picks that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSparcObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
