#include "STM32Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "STM32Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Flash: holds the reset vector table and every instruction, nothing else - read-only on
// this board. `-kernel` loads the `--format=raw` output here.
constexpr uint32_t kFlashBase = 0x08000000;

// SRAM on this board (8KB, 0x20000000-0x20001FFF): every global/local/temp/array slot lives
// here, reserved via `ds`, never given image bytes.
constexpr uint32_t kRamBase = 0x20000000;

// Top of SRAM - where sp starts, loaded by the Cortex-M reset exception from vector 0.
constexpr uint32_t kInitialSP = 0x20002000;
}  // namespace

void STM32Codegen::declareTemp(const std::string &lbl, Type type) {
    ramTrailer_.push_back(lbl + ": dsb " + std::to_string(widthOfType(type)));
}

void STM32Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    ramTrailer_.push_back(lbl + ": dsb " + std::to_string(widthOfType(elemType) * values.size()));
}

// A literal element's value via `movl`; a symbol element's address via the same `lea`
// genAddrOf() uses, with the thumb-bit adjustment (+1) when the element names a Function. The
// loaded value is copied into the RAM cell `lbl+offset` resolves to, truncated to `elemType`'s
// width; a `long long` literal also needs its high word written separately since `str` here is
// always exactly 4 bytes.
void STM32Codegen::genArrayElemInit(const std::string &lbl, size_t offset, Type elemType, const ir::ArrayInitElem &e) {
    if (!e.isSymbol && e.value == 0) return;  // RAM already starts zeroed - nothing to copy
    int w = widthOfType(elemType);
    if (e.isSymbol) {
        code_ << "    lea " << (e.slot ? label(e.slot) : label(e.func)) << ",r0\n";
        if (!e.slot) {
            code_ << "    movl 1,r1\n";
            code_ << "    add r0,r0,r1\n";
        }
    } else if (w == 8) {
        auto v = static_cast<uint64_t>(e.value);
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v)) << ",r0\n";
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
    } else {
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.value)) << ",r0\n";
    }
    code_ << "    lea " << lbl << "+" << offset << ",r2\n";
    const char *store = w == 1 ? "strb" : w == 2 ? "strh" : "str";
    code_ << "    " << store << " r0,[r2]\n";
    if (w == 8) {
        code_ << "    movl 4,r3\n    add r2,r2,r3\n";
        code_ << "    str r1,[r2]\n";
    }
}

// Flash-resident code that writes `value` into the RAM cell `lbl` resolves to, truncated to
// `type`'s width. RAM starts out undefined, so a nonzero initializer needs this explicit
// one-time copy, emitted into `_start` for a global or at the `LocalDecl` site for a local.
void STM32Codegen::genGlobalInit(const std::string &ramLabel, Type type, long value) {
    if (value == 0) return;  // RAM already starts zeroed - nothing to copy
    int w = widthOfType(type);
    code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(value)) << ",r0\n";
    if (w == 8) code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32)) << ",r1\n";
    code_ << "    lea " << ramLabel << ",r2\n";
    const char *store = w == 1 ? "strb" : w == 2 ? "strh" : "str";
    code_ << "    " << store << " r0,[r2]\n";
    if (w == 8) {
        code_ << "    movl 4,r3\n    add r2,r2,r3\n";
        code_ << "    str r1,[r2]\n";
    }
}

// `byteOffsetFromFp` is already a byte displacement (IRBuilder.cpp accounts for the slot's
// width/alignment and, for a Param, the reservation past the saved r7/return address), so this
// just adds or subtracts it from r7 directly.
void STM32Codegen::loadFrameWordAddr(const std::string &reg, int byteOffsetFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (byteOffsetFromFp == 0) return;
    code_ << "    movl " << (byteOffsetFromFp > 0 ? byteOffsetFromFp : -byteOffsetFromFp) << ",r3\n";
    code_ << "    " << (byteOffsetFromFp > 0 ? "add" : "sub") << " " << reg << "," << reg << ",r3\n";
}

void STM32Codegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int byteOffsetFromFp =
        slot->kind == ir::SlotKind::Param ? slot->frameOffset : -ir::localLowOffset(*slot, typeWidths());
    loadFrameWordAddr(reg, byteOffsetFromFp);
}

void STM32Codegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    lea " << label(slot) << "," << reg << "\n";
    } else {
        loadFrameAddr(reg, slot);
    }
}

void STM32Codegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.value)) << "," << reg << "\n";
        return;
    }
    code_ << "    lea " << (e.slot ? label(e.slot) : label(e.func)) << "," << reg << "\n";
    if (!e.slot) {
        code_ << "    movl 1,r1\n";
        code_ << "    add " << reg << "," << reg << ",r1\n";
    }
}

std::string STM32Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In object mode, `prog` may not define `main`; no object file emits the vector
    // table/reset-handler/`_start` - STM32LinkerFactory.cpp synthesizes exactly one once every
    // object is merged. An object's flash section is `org 0`; placing it is the linker's job.
    //
    // RAM keeps its real fixed hardware base (kRamBase) even in object mode, since its
    // placement is independent of where this object's flash ends up linked - so a RAM address
    // resolves to its final value at assembly time, no base-shift relocation needed.
    //
    // Running byte offset within RAM, used to pad ahead of a global wider than 1 byte (an
    // unaligned ldr/str/ldrh/strh is UNPREDICTABLE on this core). kRamBase is word-aligned, so
    // tracking this from 0 is exact.
    size_t ramOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global is defined and reserves its RAM storage in another file; this
        // file only references it by name, deferred to a relocation.
        if (slot->isExternal) continue;
        int align = std::min(widthOfType(slot->type), 4);
        size_t padding = (align - (ramOffset % align)) % align;
        if (padding > 0) {
            ramTrailer_.push_back("dsb " + std::to_string(padding));
            ramOffset += padding;
        }
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
            ramOffset += static_cast<size_t>(widthOfType(slot->type)) * static_cast<size_t>(slot->arraySize);
            // The startup copy writes each nonzero element into RAM before `main` runs, spliced
            // into `_start`; skipped in object mode since no `_start` is emitted there.
            if (!objectMode) {
                int w = widthOfType(slot->type);
                for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                    genArrayElemInit(lbl, i * static_cast<size_t>(w), slot->type, slot->arrayInit[i]);
                }
            }
        } else {
            declareTemp(lbl, slot->type);
            ramOffset += static_cast<size_t>(widthOfType(slot->type));
            if (!objectMode) genGlobalInit(lbl, slot->type, slot->hasInit ? slot->initValue : 0);
        }
    }
    std::string initCode = code_.str();
    code_.str("");

    code_ << "org " << (objectMode ? 0 : kFlashBase) << "\n";
    if (!objectMode) {
        code_ << "dl " << kInitialSP << "\n";  // vector[0]: initial sp
        code_ << "dlt _start\n";               // vector[1]: reset handler, thumb bit forced on
        code_ << "_start:\n";
        code_ << initCode;
        code_ << "    bl " << label(mainFn) << "\n";
        // main's result sits in r0 when execution reaches `halt`.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    code_ << "\nramorg " << kRamBase << "\n";
    for (const auto &line : ramTrailer_) code_ << line << "\n";

    return code_.str();
}

std::map<std::string, std::string> STM32Codegen::perFileAsm() const { return perFileAsm_; }

void STM32Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    code_ << "    push r7\n";
    code_ << "    mov r7,sp\n";
    // frameSize is a byte count; each push/pop reserves/releases a whole 4-byte word (there's
    // no `sub sp,#imm` here), so this rounds up to whole words.
    frameSize_ = fn.func->frameSize;
    int frameWords = (frameSize_ + 3) / 4;
    for (int i = 0; i < frameWords; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();  // only reached if control falls off the end with no `return`
    code_ << "\n";
}

void STM32Codegen::emitEpilogue() {
    int frameWords = (frameSize_ + 3) / 4;
    for (int i = 0; i < frameWords; i++) code_ << "    pop r1\n";
    code_ << "    pop r7\n";
    code_ << "    poplr\n";
}

void STM32Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmpz r0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmpz r0\n    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    b " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    b " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit, so genFunction()'s fall-off-the-end epilogue never runs for this path.
            emitEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddr("r2", s.declSlot);
                storeOperand("r2", s.declSlot->type);
            }
            // No "else" for a bare, uninitialized scalar: its frame word already exists
            // (genFunction()'s reserve-pushes made room for it), like an ordinary C automatic.
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
            code_ << "    b " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void STM32Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v)) << ",r0\n";
                code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
                return;
            }
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("r2", e.slot);
            loadOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    push r2\n";
                genExpr(*e.rhs);
                code_ << "    pop r2\n";
                storeOperand("r2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            storeOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness: the pointee type of a typed pointer, or a
            // plain native int word for the untyped-int-as-address style.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r2,r0\n";
            code_ << "    push r2\n";
            genExpr(*e.rhs);  // r0, or r1:r0 if wide
            code_ << "    pop r2\n";
            storeOperand("r2", e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // r2 = address
                genCompoundThroughR2(e);
                return;
            }
            // Plain scalar target - loadSlotAddr() is side-effect-free, so it's computed after
            // e.rhs, same order as the Assign case above.
            genExpr(*e.rhs);  // r0 = rhs
            code_ << "    mov r1,r0\n";
            loadSlotAddr("r2", e.slot);
            loadOperand("r2", e.type);  // r0 = old
            if (e.yieldOld) code_ << "    push r0\n";
            genCompoundOp(e.op, e.type);
            narrowR0To(e.type);
            storeOperand("r2", e.type);
            if (e.yieldOld) code_ << "    pop r0\n";
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    mov r2,r0\n";  // r2 = address
            genCompoundThroughR2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmpz r0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    b " << Ldone << "\n";
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
                    code_ << "    neg r0,r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness, as DerefAssign above.
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r2,r0\n";
                loadOperand("r2", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn r0,r0\n";
                if (isWide(e.rhs->type)) code_ << "    mvn r1,r1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed last-declared one first, matching Param addressing. The
            // caller cleans them back off with one dummy `pop r1` per argument (no
            // `add sp,#imm` here). A wide argument pushes its high word first so its low word
            // ends up closer to r7, matching a wide Param's frameOffset/frameOffset+4 layout.
            int pushedWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    push r1\n    push r0\n";
                    pushedWords += 2;
                } else {
                    code_ << "    push r0\n";
                    pushedWords += 1;
                }
            }
            code_ << "    bl " << label(e.func) << "\n";
            for (int i = 0; i < pushedWords; i++) code_ << "    pop r1\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty: no per-callee p_<fn>_<i> cell exists until the callee is
            // known. `blx r0` is an indirect call through whatever address r0 holds.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    blx r0\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Cortex-M has real PRIMASK/cpsie/cpsid, but nothing here sets up an
            // exception/interrupt table for it to matter against on a bare-metal no-OS compile.
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

// `r2` holds the computed address. `arraySize > 0` means the base is the array's first word
// (a label for a Global, or an r7-relative address for a Local/Param); otherwise `slot` is a
// pointer variable, so the base comes from reading that variable's stored value first. Either
// array's element addresses increase with the index.
void STM32Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    // r0 *= the element type's width via a variable-count `lsl`.
    int shift = elemShift(slot->type);
    if (shift > 0) {
        code_ << "    movl " << shift << ",r3\n";
        code_ << "    lsl r0,r0,r3\n";
    }
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lea " << label(slot) << ",r2\n";
            code_ << "    add r2,r2,r0\n";
        } else {
            loadFrameAddr("r2", slot);
            code_ << "    add r2,r2,r0\n";
        }
        return;
    }
    loadSlotAddr("r2", slot);
    code_ << "    ldr r2,[r2]\n";  // r2 = the pointer variable's own stored value
    code_ << "    add r2,r2,r0\n";
}

void STM32Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("r2", e.slot->type);
}

void STM32Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add r0,r0,r1\n"; return; }
    if (op == "-") { code_ << "    sub r0,r0,r1\n"; return; }
    if (op == "*") { code_ << "    mul r1,r0\n"; return; }
    if (op == "&") { code_ << "    and r0,r1\n"; return; }
    if (op == "|") { code_ << "    orr r0,r1\n"; return; }
    if (op == "^") { code_ << "    eor r0,r1\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r0,r1\n";
        return;
    }
    // "/" or "%": r4 = old / rhs (never r2, which must stay intact as the address register for
    // the store right after this call returns); "%" recovers the remainder via multiply-subtract.
    code_ << "    " << (isUnsigned ? "udiv" : "sdiv") << " r4,r0,r1\n";
    if (op == "%") {
        code_ << "    mul r1,r4\n";     // r4 = r4 * r1 = quotient * rhs
        code_ << "    sub r0,r0,r4\n";  // r0 = old - quotient*rhs = remainder
    } else {
        code_ << "    mov r0,r4\n";
    }
}

// r2 must already hold the resolved address on entry; it's stashed on the stack across e.rhs's
// evaluation, since that can recursively reach arbitrary codegen (calls included) that may
// clobber every other register.
void STM32Codegen::genCompoundThroughR2(const ir::Expr &e) {
    code_ << "    push r2\n";
    genExpr(*e.rhs);              // r0 = rhs
    code_ << "    mov r1,r0\n";    // r1 = rhs, held across the r2 reload below
    code_ << "    pop r2\n";        // r2 = address
    loadOperand("r2", e.type);         // r0 = old
    if (e.yieldOld) code_ << "    push r0\n";
    genCompoundOp(e.op, e.type);          // r0 = new (raw)
    narrowR0To(e.type);
    if (e.yieldOld) {
        code_ << "    mov r4,r0\n";   // r4 = new
        code_ << "    pop r0\n";       // r0 = old
        code_ << "    mov r5,r0\n";     // r5 = old, preserved across the store below
        code_ << "    mov r0,r4\n";      // r0 = new (storeOperand always stores from r0)
        storeOperand("r2", e.type);         // r2 still addresses the target (loadOperand never moves it - e.type is never wide)
        code_ << "    mov r0,r5\n";          // r0 = old (final result)
    } else {
        storeOperand("r2", e.type);  // r0 already = new; r2 still addresses the target
    }
}

// Stores arrayInit into `slot`'s frame slots. Unlike a Global's static trailer, a Local's
// storage is a fresh frame region each call, so this must re-run every time. An element's
// address is i*widthOfType(slot->type) bytes above element 0's.
void STM32Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        loadArrayInitElem("r0", elem);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
        }
        loadFrameWordAddr("r2", -ir::localLowOffset(*slot, typeWidths()) + w * static_cast<int>(i));
        storeOperand("r2", slot->type);
    }
}

void STM32Codegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    // Re-truncate+re-extend via `lsl` then `asr`/`lsr` (zero-fill for unsigned, sign-fill for
    // signed).
    if (w == 1) {
        code_ << "    movl 24,r3\n";
        code_ << "    lsl r0,r0,r3\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,r3\n";
    } else if (w == 2) {
        code_ << "    movl 16,r3\n";
        code_ << "    lsl r0,r0,r3\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,r3\n";
    }
}

// `addrReg` holds the already-computed effective address (r2 by convention); loads into r0
// (or r0:r1 for `long long`). `ldrb`/`ldrh` always zero-extend, so a signed source needs an
// additional re-sign-extend via narrowR0To().
void STM32Codegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldrb r0,[" << addrReg << "]\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 2) {
        code_ << "    ldrh r0,[" << addrReg << "]\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 4) {
        code_ << "    ldr r0,[" << addrReg << "]\n";
    } else {
        code_ << "    ldr r0,[" << addrReg << "]\n";
        code_ << "    movl 4,r3\n    add r6," << addrReg << ",r3\n";
        code_ << "    ldr r1,[r6]\n";
    }
}

void STM32Codegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    strb r0,[" << addrReg << "]\n";
    } else if (w == 2) {
        code_ << "    strh r0,[" << addrReg << "]\n";
    } else if (w == 4) {
        code_ << "    str r0,[" << addrReg << "]\n";
    } else {
        code_ << "    str r0,[" << addrReg << "]\n";
        code_ << "    movl 4,r3\n    add r6," << addrReg << ",r3\n";
        code_ << "    str r1,[r6]\n";
    }
}

// An implicit or explicit type conversion. IRBuilder already promotes narrower operands before
// Binary/Unary sees them, so the only work here is crossing the int/`long long` boundary or
// renarrowing via narrowR0To().
void STM32Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    movl 0,r1\n";
        } else {
            code_ << "    mov r1,r0\n    movl 31,r3\n    asr r1,r1,r3\n";
        }
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

void STM32Codegen::genNegate64() {
    // Two's complement negate of r0:r1 in place.
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    movl 1,r3\n    add r0,r0,r3\n";
    code_ << "    movl 0,r3\n    adc r1,r3\n";
}

void STM32Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("r2", operand.slot);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        // Cortex-M Thumb interworking: a function address must carry the Thumb-select bit
        // (LSB=1) for `blx` to branch into it correctly. Every function label is always even
        // (2-byte aligned), so this is exactly "address + 1".
        code_ << "    lea " << label(operand.func) << ",r0\n";
        code_ << "    movl 1,r1\n";
        code_ << "    add r0,r0,r1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// `long long` add/sub: evaluate lhs into r0:r1, stash it on the stack while rhs evaluates into
// r4:r5, then ripple the carry/borrow across the word pair with `adc`/`sbc`.
void STM32Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4=rhs_lo, r5=rhs_hi
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = lhs restored
    if (isAdd) {
        code_ << "    add r0,r0,r4\n    adc r1,r5\n";
    } else {
        code_ << "    sub r0,r0,r4\n    sbc r1,r5\n";
    }
}

// 32x32->64 unsigned widening multiply: {r1:r0} = r6(a) * r4(b), both consumed. No widening
// multiply instruction exists, so this is a bit-serial shift-and-add loop: `a` consumed one
// bit at a time (shifted right, carry tested via `bhs`/`blo`); `b`'s doubled value (r4:r5,
// growing to 64 bits) added into the accumulator (r0:r1) whenever that bit is 1. Clobbers r0-r6.
void STM32Codegen::genMul64() {
    code_ << "    movl 0,r5\n";  // r5 = b's high word (b, r4, is only ever 32 bits wide)
    code_ << "    movl 0,r0\n    movl 0,r1\n";
    code_ << "    movl 0,r2\n";  // r2 = iteration count-up (0..32)
    std::string Lloop = newLabel(), LdoAdd = newLabel(), LskipAdd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movl 32,r3\n    cmp r2,r3\n    beq " << Ldone << "\n";
    code_ << "    movl 1,r3\n    add r2,r2,r3\n";
    // The carry flag left by shifting out a's low bit IS that bit, tested via `bhs`/`blo`.
    code_ << "    lsr r6,r6,r3\n    bhs " << LdoAdd << "\n    b " << LskipAdd << "\n";
    code_ << LdoAdd << ":\n";
    code_ << "    add r0,r0,r4\n    adc r1,r5\n";
    code_ << LskipAdd << ":\n";
    // Double b (r4:r5) via self-add, carrying its old top bit into r5 through `adc`.
    code_ << "    add r4,r4,r4\n    adc r5,r5\n";
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit product: result =
// lo(lhs)*lo(rhs) (via genMul64()) plus the cross terms lhs_hi*rhs_lo/lhs_lo*rhs_hi folded into
// the high word only (truncating `mul` is exactly right for them).
void STM32Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stash lhs: top->bottom = lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r6,r0\n    mov r5,r1\n";  // r6=rhs_lo, r5=rhs_hi
    code_ << "    pop r4\n    pop r2\n";        // r4=lhs_lo, r2=lhs_hi
    // Cross terms computed now, before genMul64() clobbers r4/r6.
    code_ << "    mov r3,r2\n    mul r6,r3\n";  // r3 = lhs_hi*rhs_lo
    code_ << "    mov r2,r5\n    mul r4,r2\n";  // r2 = rhs_hi*lhs_lo
    code_ << "    add r2,r2,r3\n";
    code_ << "    push r2\n";  // stash the cross-term sum across genMul64()'s own full clobber
    genMul64();                 // {r1:r0} = r6(rhs_lo) * r4(lhs_lo), the full 64-bit low*low product
    code_ << "    pop r2\n";
    code_ << "    add r1,r1,r2\n";
}

// A `long long` shifted by a runtime count: no instruction here shifts more than 32 bits at
// once, so this is a counted loop of 1-bit shifts (r6 = count). `<<` doubles r0:r1 via the
// self-add carry-chained-doubling trick. `>>` has no `adc` analogue for the downward direction,
// so the bit crossing the r1/r0 boundary is captured into r4 before shifting, then OR'd into
// r0's new top bit via a precomputed 0x80000000 mask (r3); `isUnsigned` picks `asr` vs `lsr`
// for r1's shift.
void STM32Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);  // r0 = shift count (already promoted to a plain int)
    code_ << "    mov r6,r0\n";
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 restored = the value being shifted
    code_ << "    movl 1,r5\n";            // r5 = the fixed amount (1) every shift/decrement below uses
    std::string Lloop = newLabel(), Ldone = newLabel();
    if (!isLeft) {
        code_ << "    movl 1,r3\n    movl 31,r2\n    lsl r3,r3,r2\n";  // r3 = 0x80000000
    }
    code_ << Lloop << ":\n";
    code_ << "    cmpz r6\n    beq " << Ldone << "\n    sub r6,r6,r5\n";
    if (isLeft) {
        code_ << "    add r0,r0,r0\n    adc r1,r1\n";
    } else {
        std::string LskipOr = newLabel();
        code_ << "    mov r4,r1\n    and r4,r5\n";  // r4 = r1's own old bit 0 (the bit crossing into r0)
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " r1,r1,r5\n";
        code_ << "    lsr r0,r0,r5\n";
        code_ << "    cmpz r4\n    beq " << LskipOr << "\n";
        code_ << "    orr r0,r3\n";
        code_ << LskipOr << ":\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Bit-serial shift/compare/subtract long division (no instruction here is wide enough for a
// genuine 64/64 divide): r0:r1 start as the dividend and finish as the quotient (restoring
// division, building the quotient into bits vacated as the dividend shifts left); r4:r5 is the
// remainder (starts 0); r2:r3 is the divisor. `isUnsigned` false divides magnitudes (each
// operand's sign bit, read before negating, is stashed and restored afterward per C's
// truncating-division rule).
void STM32Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r2,r0\n    mov r3,r1\n";  // r2:r3 = divisor
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = dividend

    if (!isUnsigned) {
        code_ << "    movl 31,r4\n";
        code_ << "    mov r5,r1\n    lsr r5,r5,r4\n";  // r5 = dividend's own sign (0/1)
        code_ << "    mov r6,r3\n    lsr r6,r6,r4\n";  // r6 = divisor's own sign (0/1)
        code_ << "    push r5\n    push r6\n";          // stack (top->bottom): dividendSign, divisorSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    cmpz r5\n    beq " << LskipNegN << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    movl 1,r4\n    add r0,r0,r4\n    movl 0,r4\n    adc r1,r4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    cmpz r6\n    beq " << LskipNegD << "\n";
        code_ << "    mvn r2,r2\n    mvn r3,r3\n    movl 1,r4\n    add r2,r2,r4\n    movl 0,r4\n    adc r3,r4\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    movl 0,r4\n    movl 0,r5\n";  // remainder = 0
    code_ << "    movl 64,r6\n";                 // iteration count-down (64..0), using `cmpz`
                                                   // instead of a second compare register

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel();
    code_ << Lloop << ":\n    cmpz r6\n    beq " << Ldone << "\n";
    // No register is spare for a "1" constant - `push r3`/`pop r3` borrows the divisor's high
    // word as a scratch slot, safe since nothing reads it again until this restores it.
    code_ << "    push r3\n    movl 1,r3\n    sub r6,r6,r3\n    pop r3\n";

    // Shift {remainder:dividend} left by 1 as one combined value via carry-chained doubling;
    // the bit overflowing out of the dividend (r1's old top bit) lands in the remainder's new
    // bit 0 through the carry chain.
    code_ << "    add r0,r0,r0\n    adc r1,r1\n    adc r4,r4\n    adc r5,r5\n";

    // if (remainder >= divisor) { remainder -= divisor; quotient bit = 1 } - an unsigned
    // magnitude compare, since both operands were already negated non-negative for the signed case.
    code_ << "    cmp r5,r3\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r4,r2\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bhi " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    sub r4,r4,r2\n    sbc r5,r3\n";
    code_ << "    push r3\n    movl 1,r3\n    add r0,r0,r3\n    pop r3\n";  // set the vacated quotient bit
    code_ << LsubNo << ":\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    pop r6\n";  // r6 = divisorSign
        code_ << "    pop r2\n";  // r2 = dividendSign
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov r3,r6\n    eor r3,r2\n";  // r3 = divisorSign ^ dividendSign (quotient-negate flag)
        code_ << "    cmpz r3\n    beq " << LskipNegQ << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    movl 1,r6\n    add r0,r0,r6\n    movl 0,r6\n    adc r1,r6\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    cmpz r2\n    beq " << LskipNegR << "\n";
        code_ << "    mvn r4,r4\n    mvn r5,r5\n    movl 1,r6\n    add r4,r4,r6\n    movl 0,r6\n    adc r5,r6\n";
        code_ << LskipNegR << ":\n";
    }
    if (wantRemainder) {
        code_ << "    mov r0,r4\n    mov r1,r5\n";
    }
}

// `long long` comparison: compare the high words first (per `isUnsigned`, since that's where
// the sign bit lives); only if equal does the low word matter, always compared unsigned.
void STM32Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4:r5 = rhs
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = lhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmp r0,r4\n    bne " << Lfalse << "\n";
        code_ << "    cmp r1,r5\n    bne " << Lfalse << "\n";
        code_ << "    movl " << onEqual << ",r0\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    movl " << onDiffer << ",r0\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string signedCC = op == "<" ? "blt" : op == "<=" ? "ble" : op == ">" ? "bgt" : "bge";
    std::string unsignedCC = op == "<" ? "blo" : op == "<=" ? "bls" : op == ">" ? "bhi" : "bhs";
    std::string hiCC = isUnsigned ? unsignedCC : signedCC;
    std::string LhiDiff = newLabel();
    code_ << "    cmp r1,r5\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r0,r4\n    " << unsignedCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    " << hiCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Arithmetic/bitwise/comparison operands already share one common type (IRBuilder's usual
    // arithmetic conversions), except shift (only the LHS's type matters) and &&/|| (handled
    // below, width-independent).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r0,r0,r1\n";  // r0 = rhs + lhs
        } else {
            code_ << "    sub r0,r1,r0\n";  // r0 = lhs - rhs
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
        const char *insn = (op == "&") ? "and" : (op == "|") ? "orr" : "eor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    push r1\n    push r0\n";
            genExpr(*e.rhs);
            code_ << "    mov r4,r0\n    mov r5,r1\n";
            code_ << "    pop r0\n    pop r1\n";  // r0:r1 = lhs
            code_ << "    " << insn << " r0,r4\n    " << insn << " r1,r5\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        code_ << "    " << insn << " r0,r1\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    mov r1,r0\n";  // r1 = shift count
        code_ << "    pop r0\n";     // r0 = value to shift
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r0,r1\n";
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

// Hardware multiply (`muls`): a genuine 32x32->32 product, no operand-width caveat.
void STM32Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    mul r1,r0\n";  // r0 = r0 * r1 = rhs * lhs
}

// Hardware divide (`sdiv`/`udiv`): a genuine signed/unsigned integer divide, so no software
// loop is needed; `%` is computed directly via one multiply-subtract.
void STM32Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";          // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    " << (isUnsigned ? "udiv" : "sdiv") << " r2,r1,r0\n";  // r2 = lhs / rhs
    if (wantRemainder) {
        code_ << "    mul r0,r2\n";       // r2 = r2 * r0 = quotient * rhs
        code_ << "    sub r0,r1,r2\n";    // r0 = lhs - quotient*rhs = remainder
    } else {
        code_ << "    mov r0,r2\n";
    }
}

void STM32Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    if (op == "==") return emitCompare(lhs, rhs, "beq", isUnsigned);
    if (op == "!=") return emitCompare(lhs, rhs, "bne", isUnsigned);
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "blo" : "blt", isUnsigned);
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "bls" : "ble", isUnsigned);
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "bhi" : "bgt", isUnsigned);
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "bhs" : "bge", isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `cmp` + condition-code branch: flags already encode signed/unsigned ordering correctly, no
// fixup needed. `isUnsigned` is unused here directly (baked into genCompare()'s choice of
// `branchIfTrue`) but documents why.
void STM32Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue,
                                 bool isUnsigned) {
    (void)isUnsigned;
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";  // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

// A wide operand's truthiness needs both words: `orr r0,r1` folds them before `cmpz r0`.
void STM32Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmpz r0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> STM32Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleSTM32(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: builds one format-neutral ObjectFile, then picks
    // that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSTM32Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
