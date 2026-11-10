#include "ArmMCodegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "ArmMAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded; the simulator sets the PC here directly.
constexpr uint32_t kBaseAddress = 0x1000u;

// Initial sp, set before calling main(); comfortably above the loaded program.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

void ArmMCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element is its decimal text; a symbol element is the referenced
// Slot's/Function's label, resolved later by the dw expression evaluator.
std::string ArmMCodegen::arrayElemText(const ir::ArrayInitElem &e, Type elemType) {
    if (!e.isSymbol) {
        // A long long literal needs two dw's (low/high word); handled by the caller.
        (void)elemType;
        return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    }
    // A function's address gets its low bit set (+1) for Thumb interworking:
    // BLX (register) raises a UsageFault if bit 0 isn't 1, since M-profile has
    // no ARM state. A plain data Slot is never called through, so it needs no bit.
    if (e.slot) return label(e.slot);
    return label(e.func) + "+1";
}

void ArmMCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
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
        line << arrayElemText(e, elemType);
        first = false;
    }
    trailer_.push_back(line.str());
}

void ArmMCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movw " << reg << "," << lbl << "\n";
    code_ << "    movt " << reg << "," << lbl << "\n";
}

void ArmMCodegen::loadImm32(const std::string &reg, uint32_t value) {
    if (value <= 0xFF) {
        code_ << "    movs " << reg << "," << static_cast<int32_t>(value) << "\n";
        return;
    }
    code_ << "    movw " << reg << "," << static_cast<int32_t>(value & 0xFFFFu) << "\n";
    code_ << "    movt " << reg << "," << static_cast<int32_t>((value >> 16) & 0xFFFFu) << "\n";
}

void ArmMCodegen::loadFrameByteAddr(const std::string &reg, int bytesFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (bytesFromFp == 0) return;
    loadImm32("r3", static_cast<uint32_t>(bytesFromFp > 0 ? bytesFromFp : -bytesFromFp));
    code_ << "    " << (bytesFromFp > 0 ? "adds" : "subs") << " " << reg << "," << reg << ",r3\n";
}

void ArmMCodegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int bytesFromFp =
        slot->kind == ir::SlotKind::Param ? slot->frameOffset : -ir::localLowOffset(*slot, typeWidths());
    loadFrameByteAddr(reg, bytesFromFp);
}

void ArmMCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
    } else {
        loadFrameAddr(reg, slot);
    }
}

void ArmMCodegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        loadImm32(reg, static_cast<uint32_t>(e.value));
        return;
    }
    if (e.slot) {
        loadAddress(reg, label(e.slot));
        return;
    }
    // Same Thumb-interworking bit requirement as arrayElemText().
    loadAddress(reg, label(e.func));
    code_ << "    adds " << reg << "," << reg << ",1\n";
}

std::string ArmMCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In object mode, prog may not define main at all; ArmMLinkerFactory.cpp
    // synthesizes this entry sequence once instead, after linking.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // movw/movt can't target sp directly (Rd==13 is UNPREDICTABLE), so the
        // initial sp is built in a scratch register and moved into sp instead.
        loadImm32("r0", kInitialSP);
        code_ << "    mov sp,r0\n";
        code_ << "    bl " << label(mainFn) << "\n";
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Pads ahead of a global wider than 1 byte, so an unaligned ldind/stind
    // doesn't round down and clobber a neighboring global.
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global is never defined here; only referenced by name,
        // letting the assembler defer it to a relocation.
        if (slot->isExternal) continue;
        int align = std::min(widthOfType(slot->type), 4);
        size_t padding = (align - (dataOffset % align)) % align;
        if (padding > 0) {
            std::string fill = "0";
            for (size_t i = 1; i < padding; i++) fill += ", 0";
            trailer_.push_back("db " + fill);
            dataOffset += padding;
        }
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
            dataOffset += static_cast<size_t>(widthOfType(slot->type)) * static_cast<size_t>(slot->arraySize);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + std::string(directive) + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
            dataOffset += static_cast<size_t>(w);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> ArmMCodegen::perFileAsm() const { return perFileAsm_; }

void ArmMCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    code_ << "    push r7\n";
    code_ << "    mov r7,sp\n";
    // frameSize is a byte count; round up to whole words since each push below
    // reserves one word, with no sub sp,#imm available to reserve exactly.
    frameSize_ = (fn.func->frameSize + 3) / 4;
    for (int i = 0; i < frameSize_; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

void ArmMCodegen::emitEpilogue() {
    for (int i = 0; i < frameSize_; i++) code_ << "    pop r1\n";
    code_ << "    pop r7\n";
    code_ << "    poppc\n";
}

void ArmMCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmp r0,0\n";
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
            code_ << "    cmp r0,0\n    beq " << Lend << "\n";
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
            // Early exit: the fall-off-the-end epilogue never runs for this path.
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
            // An uninitialized scalar's frame word already exists (reserved
            // by genFunction()); nothing to emit.
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

void ArmMCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImm32("r0", static_cast<uint32_t>(v));
                loadImm32("r1", static_cast<uint32_t>(v >> 32));
                return;
            }
            loadImm32("r0", static_cast<uint32_t>(e.intValue));
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
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                genCompoundThroughR2(e);
                return;
            }
            // A plain scalar target still needs its address computed (ARM has
            // no absolute-addressing operand), so it shares the indexed tail.
            loadSlotAddr("r2", e.slot);
            genCompoundThroughR2(e);
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
            // The condition's type can still be wide even though the result
            // type is capped to native width, so this reuses the If/While
            // wide-OR-then-compare idiom.
            if (isWide(e.lhs->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmp r0,0\n";
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
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness: the pointee type, or plain
            // native-int-width for the untyped-int-as-address style.
            genExpr(*e.lhs);  // r0 = pointer value
            code_ << "    push r0\n";
            genExpr(*e.rhs);  // r0, or r0:r1 if wide
            code_ << "    pop r2\n";
            storeOperand("r2", e.type);
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
            // Arguments push last-declared first, so the first argument ends
            // up closest to the return address (matching paramIndex 0 = r7+8).
            // The caller cleans up afterward with one dummy pop per pushed
            // word (no add sp,#imm here). A wide argument pushes its high word
            // first so its low word ends up closer to r7.
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
            // e.args is always empty: the callee is unknown at compile time, so
            // there's no p_<fn>_<i> cell to write arguments into.
            // blx r2 calls through whatever address r2 holds; r2's bit 0 must
            // already be 1 (ARMv7-M interworking), which genAddrOf()'s Func
            // case guarantees since nothing else here is callable.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r2,r0\n";
            code_ << "    blx r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No NVIC/exception setup here, so interrupt enable/disable is a no-op.
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

// arraySize > 0 means slot is the array itself (base = its first word);
// otherwise slot is a pointer variable, so the base is read from its stored
// value. Either array's element addresses increase with the index.
void ArmMCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    lsli r0,r0," << shift << "\n";  // r0 *= element width
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("r2", label(slot));
            code_ << "    adds r2,r2,r0\n";
        } else {
            loadFrameAddr("r2", slot);
            code_ << "    adds r2,r2,r0\n";
        }
        return;
    }
    loadSlotAddr("r1", slot);
    code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's stored value
    code_ << "    adds r2,r1,r0\n";
}

void ArmMCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("r2", e.slot->type);
}

// Unlike a Global's static trailer, a Local's frame storage is fresh per
// activation, so arrayInit must be re-stored on every call. Element i's
// address is i*widthOfType(slot->type) bytes above element 0's.
void ArmMCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        loadArrayInitElem("r0", elem);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            loadImm32("r1", static_cast<uint32_t>(v >> 32));
        }
        loadFrameByteAddr("r2", -ir::localLowOffset(*slot, typeWidths()) + w * static_cast<int>(i));
        storeOperand("r2", slot->type);
    }
}

void ArmMCodegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    // Shifts the unwanted high bits out and back in: lsr (zero-fill) for
    // unsigned, asr (sign-fill) for signed - no dedicated extend instruction here.
    if (w == 1) {
        code_ << "    lsli r0,r0,24\n";
        code_ << "    movs r1,24\n    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r1\n";
    } else if (w == 2) {
        code_ << "    lsli r0,r0,16\n";
        code_ << "    movs r1,16\n    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r1\n";
    }
}

// ldindb/ldindh always zero-extend; a signed source needs re-sign-extending
// via narrowR0To() afterward.
void ArmMCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindb " << addrReg << ",r0\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 2) {
        code_ << "    ldindh " << addrReg << ",r0\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",r0\n";
    } else {
        // subs has no immediate form here, so the high word's address is
        // computed into scratch r6 instead of mutating addrReg itself.
        code_ << "    ldind " << addrReg << ",r0\n";
        code_ << "    adds r6," << addrReg << ",4\n";
        code_ << "    ldind r6,r1\n";
    }
}

void ArmMCodegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",r0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",r0\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << ",r0\n";
    } else {
        code_ << "    stind " << addrReg << ",r0\n";
        code_ << "    adds r6," << addrReg << ",4\n";
        code_ << "    stind r6,r1\n";
    }
}

void ArmMCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    movs r1,0\n";
        } else {
            code_ << "    mov r1,r0\n    movs r4,31\n    asr r1,r4\n";
        }
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

// Two's complement negate of r0:r1 in place.
void ArmMCodegen::genNegate64() {
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    movs r4,1\n    adds r0,r0,r4\n    movs r4,0\n    adc r1,r4\n";
}

void ArmMCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4=rhs_lo, r5=rhs_hi
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 = lhs restored
    if (isAdd) {
        code_ << "    adds r0,r0,r4\n    adc r1,r5\n";
    } else {
        code_ << "    subs r0,r0,r4\n    sbc r1,r5\n";
    }
}

// 32x32->64 unsigned widening multiply: {r1:r0} = r6(a) * r4(b). No real UMULL
// here, so this is bit-serial shift-and-add (32 iterations): a (r6) is
// consumed one bit at a time via lsr, testing the carry bit it leaves behind
// (bhs/blo) directly; b's doubling value (r4:r5, grown via the lsl+adc
// carry-chain trick) is added into the accumulator (r0:r1) whenever that bit
// is 1. Clobbers r0-r6.
void ArmMCodegen::genMul64() {
    code_ << "    movs r5,0\n";  // r5 = b's high word
    code_ << "    movs r0,0\n    movs r1,0\n";
    code_ << "    movs r2,0\n";  // r2 = iteration count-up (0..32)
    code_ << "    movs r3,1\n";
    std::string Lloop = newLabel(), LdoAdd = newLabel(), LskipAdd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    cmp r2,32\n    beq " << Ldone << "\n";
    code_ << "    adds r2,r2,r3\n";
    code_ << "    lsr r6,r3\n    bhs " << LdoAdd << "\n    b " << LskipAdd << "\n";
    code_ << LdoAdd << ":\n";
    code_ << "    adds r0,r0,r4\n    adc r1,r5\n";
    code_ << LskipAdd << ":\n";
    code_ << "    adds r4,r4,r4\n    adc r5,r5\n";
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Compares high words first (sign lives there); the low word's comparison,
// reached only when the high words are equal, is always unsigned.
void ArmMCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
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
        code_ << "    movs r0," << onEqual << "\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    movs r0," << onDiffer << "\n";
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
    code_ << Lfalse << ":\n    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

// result_lo:result_hi = lo(lhs)*lo(rhs) (full 64-bit, via genMul64()) plus the
// two cross terms lhs_hi*rhs_lo/lhs_lo*rhs_hi folded into the high word only
// (truncating muls is correct there since anything past bit 64 is discarded).
void ArmMCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stash lhs: top->bottom = lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r6,r0\n    mov r5,r1\n";  // r6=rhs_lo, r5=rhs_hi
    code_ << "    pop r4\n    pop r2\n";        // r4=lhs_lo, r2=lhs_hi
    // Cross terms computed before genMul64() clobbers r4/r6.
    code_ << "    mov r3,r2\n    muls r3,r6\n";  // r3 = lhs_hi*rhs_lo
    code_ << "    mov r2,r5\n    muls r2,r4\n";  // r2 = rhs_hi*lhs_lo
    code_ << "    adds r2,r2,r3\n";
    code_ << "    push r2\n";  // stash the cross-term sum across genMul64()'s clobber
    genMul64();                 // {r1:r0} = r6(rhs_lo) * r4(lhs_lo)
    code_ << "    pop r2\n";
    code_ << "    adds r1,r1,r2\n";
}

// No instruction shifts more than 32 bits, so this is a counted loop of 1-bit
// shifts (r6 = count). << doubles r0:r1 via self-add carry-chaining; >> has no
// such chain for the downward direction, so the bit crossing the r1/r0
// boundary is captured by hand and OR'd into r0's top bit via a 0x80000000
// mask. isUnsigned picks asr (sign-preserving) or lsr (zero-filling) for r1.
void ArmMCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);  // r0 = shift count
    code_ << "    mov r6,r0\n";
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 restored = the value being shifted
    code_ << "    movs r5,1\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    if (!isLeft) code_ << "    movs r3,1\n    lsli r3,r3,31\n";  // r3 = 0x80000000, for the >> path
    code_ << Lloop << ":\n";
    code_ << "    cmp r6,0\n    beq " << Ldone << "\n    subs r6,r6,r5\n";
    if (isLeft) {
        code_ << "    adds r0,r0,r0\n    adc r1,r1\n";
    } else {
        std::string LskipOr = newLabel();
        code_ << "    mov r4,r1\n    and r4,r5\n";  // r4 = r1's old bit 0, crossing into r0
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " r1,r5\n";
        code_ << "    lsr r0,r5\n";
        code_ << "    cmp r4,0\n    beq " << LskipOr << "\n";
        code_ << "    orr r0,r3\n";
        code_ << LskipOr << ":\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Bit-serial restoring long division: r0:r1 start as the dividend and finish
// as the quotient (built into the bits vacated as it shifts left); r4:r5 is
// the remainder (starts 0); r2:r3 is the divisor. Signed division works on
// magnitudes, with each operand's original sign stashed on the stack and
// reapplied to the result afterward per C's truncating-division rule.
void ArmMCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r2,r0\n    mov r3,r1\n";  // r2:r3 = divisor
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = dividend

    if (!isUnsigned) {
        code_ << "    movs r4,31\n";
        code_ << "    mov r5,r1\n    lsr r5,r4\n";  // r5 = dividend's sign (0/1)
        code_ << "    mov r6,r3\n    lsr r6,r4\n";  // r6 = divisor's sign (0/1)
        code_ << "    push r5\n    push r6\n";       // stack (top->bottom): divisorSign, dividendSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    cmp r5,0\n    beq " << LskipNegN << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    adds r0,r0,1\n    movs r4,0\n    adc r1,r4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    cmp r6,0\n    beq " << LskipNegD << "\n";
        code_ << "    mvn r2,r2\n    mvn r3,r3\n    adds r2,r2,1\n    movs r4,0\n    adc r3,r4\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    movs r4,0\n    movs r5,0\n";  // remainder = 0
    code_ << "    movs r6,0\n";                  // iteration count-up (0..64)

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel();
    code_ << Lloop << ":\n    cmp r6,64\n    beq " << Ldone << "\n    adds r6,r6,1\n";

    // Shift {remainder:dividend} left by 1 as one 128-bit value via the
    // carry-chain; the dividend's old top bit lands in the remainder's bit 0.
    code_ << "    adds r0,r0,r0\n    adc r1,r1\n    adc r4,r4\n    adc r5,r5\n";

    // if (remainder >= divisor) { remainder -= divisor; quotient bit = 1 }
    // (always an unsigned magnitude compare, since signed operands were
    // already negated to non-negative above).
    code_ << "    cmp r5,r3\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r4,r2\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bhi " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    subs r4,r4,r2\n    sbc r5,r3\n";
    code_ << "    adds r0,r0,1\n";  // set the quotient bit the shift above vacated at bit 0
    code_ << LsubNo << ":\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    pop r6\n";  // r6 = divisorSign
        code_ << "    pop r2\n";  // r2 = dividendSign
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov r3,r6\n    eor r3,r2\n    cmp r3,0\n    beq " << LskipNegQ << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    adds r0,r0,1\n    movs r3,0\n    adc r1,r3\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    cmp r2,0\n    beq " << LskipNegR << "\n";
        code_ << "    mvn r4,r4\n    mvn r5,r5\n    adds r4,r4,1\n    movs r3,0\n    adc r5,r3\n";
        code_ << LskipNegR << ":\n";
    }
    if (wantRemainder) {
        code_ << "    mov r0,r4\n    mov r1,r5\n";
    }
}

void ArmMCodegen::genAddrOf(const ir::Expr &operand) {
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
        // BLX (register) loads the target's bit 0 into EPSR.T and faults if
        // it isn't 1 (M-profile has no ARM state); the label is always even,
        // so +1 sets that bit.
        loadAddress("r2", label(operand.func));
        code_ << "    adds r2,r2,1\n";
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Entry: r0 = old, r1 = rhs (both at type's width/signedness).
void ArmMCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    adds r0,r0,r1\n"; return; }
    if (op == "-") { code_ << "    subs r0,r0,r1\n"; return; }
    if (op == "*") { code_ << "    muls r0,r1\n"; return; }
    if (op == "&") { code_ << "    and r0,r1\n"; return; }
    if (op == "|") { code_ << "    orr r0,r1\n"; return; }
    if (op == "^") { code_ << "    eor r0,r1\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r1\n";
        return;
    }
    // "/" or "%": genDivModCore() expects r1=dividend,r0=divisor, so swap
    // from this function's r0=old,r1=rhs entry via scratch r3.
    code_ << "    mov r3,r0\n    mov r0,r1\n    mov r1,r3\n";
    genDivModCore(/*wantRemainder=*/op == "%", isUnsigned);
}

// r2 must already hold the resolved address on entry; it's stashed on the
// stack (never recomputed) across evaluating e.rhs, since genExpr() may
// clobber r0-r7, and again across genCompoundOp() since its "/"/"%" path
// uses r2 as scratch.
void ArmMCodegen::genCompoundThroughR2(const ir::Expr &e) {
    code_ << "    push r2\n";   // stash address
    genExpr(*e.rhs);              // r0 = rhs
    code_ << "    push r0\n";      // stash rhs
    code_ << "    pop r3\n";        // r3 = rhs (loadOperand() below never touches r3)
    code_ << "    pop r2\n";         // r2 = address (restored)
    loadOperand("r2", e.type);          // r0 = old
    code_ << "    mov r1,r3\n";           // r1 = rhs
    code_ << "    push r2\n";              // stash address across genCompoundOp()
    if (e.yieldOld) code_ << "    push r0\n";  // stash old
    genCompoundOp(e.op, e.type);                // r0 = new (raw)
    narrowR0To(e.type);
    if (e.yieldOld) {
        code_ << "    mov r1,r0\n";               // r1 = new
        code_ << "    pop r0\n";                    // r0 = old
        code_ << "    pop r2\n";                      // r2 = address (restored)
        code_ << "    mov r3,r0\n";                     // r3 = old, preserved across the store below
        code_ << "    mov r0,r1\n";                       // r0 = new - storeOperand always stores from r0
        storeOperand("r2", e.type);
        code_ << "    mov r0,r3\n";                         // r0 = old (final result)
    } else {
        code_ << "    pop r2\n";  // r2 = address (restored)
        storeOperand("r2", e.type);  // r0 already = new
    }
}

void ArmMCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // lhs/rhs share one common type (usual arithmetic conversions), except
    // shift (only lhs's type matters) and &&/|| (width-independent, below).
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
            code_ << "    adds r0,r1,r0\n";  // r0 = lhs + rhs
        } else {
            code_ << "    subs r0,r1,r0\n";  // r0 = lhs - rhs
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
        genExpr(*e.rhs);               // r0 = shift count
        code_ << "    mov r1,r0\n";    // r1 = shift count
        code_ << "    pop r0\n";       // r0 = value to shift
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r1\n";
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

void ArmMCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    muls r0,r1\n";
}

// Division is restoring shift-subtract, one quotient bit per
// iteration (genDivModCore()), so its cost is fixed at 32 steps.
void ArmMCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs (dividend), r0 = rhs (divisor)
    genDivModCore(wantRemainder, isUnsigned);
}

// Entry: r1=dividend, r0=divisor; result in r0. Clobbers r1-r5.
// Signed operands are divided as magnitudes; the quotient is negated
// when the signs differ and the remainder takes the dividend's sign.
// Each step doubles the remainder with the dividend's outgoing bit
// carried in (`adds`/`adc`); a carry out of the remainder means it
// already exceeds any 32-bit divisor.
void ArmMCodegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    if (!isUnsigned) {
        std::string LnPos = newLabel(), LdPos = newLabel();
        code_ << "    movs r5,0\n";  // 1 if the result needs negating
        code_ << "    cmp r1,0\n    bge " << LnPos << "\n";
        code_ << "    movs r5,1\n    neg r1,r1\n";
        code_ << LnPos << ":\n";
        code_ << "    cmp r0,0\n    bge " << LdPos << "\n";
        code_ << "    neg r0,r0\n";
        if (!wantRemainder) code_ << "    movs r4,1\n    eor r5,r4\n";
        code_ << LdPos << ":\n";
    }

    std::string Lloop = newLabel(), Lsub = newLabel(), Lnext = newLabel();
    code_ << "    movs r2,0\n    movs r3,0\n    movs r4,0\n";  // r2 = quotient, r3 = remainder, r4 = step
    code_ << Lloop << ":\n";
    code_ << "    adds r2,r2,r2\n";
    code_ << "    adds r1,r1,r1\n";  // carry = dividend's outgoing bit
    code_ << "    adc r3,r3\n";       // carry = remainder's outgoing bit
    code_ << "    bhs " << Lsub << "\n";
    code_ << "    cmp r3,r0\n    blo " << Lnext << "\n";
    code_ << Lsub << ":\n";
    code_ << "    subs r3,r3,r0\n    adds r2,r2,1\n";
    code_ << Lnext << ":\n";
    code_ << "    adds r4,r4,1\n    cmp r4,32\n    bne " << Lloop << "\n";
    code_ << "    mov r0," << (wantRemainder ? "r3" : "r2") << "\n";

    if (!isUnsigned) {
        std::string Ldone = newLabel();
        code_ << "    cmp r5,0\n    beq " << Ldone << "\n";
        code_ << "    neg r0,r0\n";
        code_ << Ldone << ":\n";
    }
}

// CMP sets condition codes for Rn-Rm in hardware; isUnsigned picks the
// unsigned branch mnemonic (blo/bls/bhi/bhs) over the signed one.
void ArmMCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";   // flags = r1 - r0 = lhs - rhs
    std::string branchIfTrue;
    if (op == "==") branchIfTrue = "beq";
    else if (op == "!=") branchIfTrue = "bne";
    else if (op == "<") branchIfTrue = isUnsigned ? "blo" : "blt";
    else if (op == "<=") branchIfTrue = isUnsigned ? "bls" : "ble";
    else if (op == ">") branchIfTrue = isUnsigned ? "bhi" : "bgt";
    else if (op == ">=") branchIfTrue = isUnsigned ? "bhs" : "bge";
    else err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

// A wide operand's truthiness needs both words: orr r0,r1 folds them
// together before the usual cmp r0,0.
void ArmMCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

std::optional<std::string> ArmMCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleArmM(asmText);
    // coff/elf/macho/aout/objraw: build one format-neutral ObjectFile, then
    // serialize it with that format's FileFormatWriter.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArmMObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
