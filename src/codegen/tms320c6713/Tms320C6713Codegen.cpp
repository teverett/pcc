#include "Tms320C6713Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "Tms320C6713Assembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Register names - see Tms320C6713Codegen.h's own header comment for the convention these
// follow.
constexpr const char *AC = "B0";
constexpr const char *T1 = "B1";
constexpr const char *T2 = "B2";
constexpr const char *RA = "B3";
constexpr const char *T3 = "B4";
constexpr const char *T4 = "B5";
constexpr const char *T5 = "B6";
constexpr const char *T6 = "B7";
constexpr const char *T7 = "B8";
constexpr const char *T8 = "B9";
constexpr const char *T9 = "B10";
constexpr const char *T10 = "B11";
constexpr const char *T11 = "B12";
constexpr const char *ZERO = "B13";
constexpr const char *FP = "B14";
constexpr const char *SP = "B15";

// Where the whole program loads; the test harness sets the simulated CPU's PC here directly.
constexpr uint32_t kBaseAddress = 0x00020000u;
// Initial sp, comfortably above the loaded program and its data.
constexpr uint32_t kInitialSP = 0x00100000u;
}  // namespace

std::string Tms320C6713Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void Tms320C6713Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
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

// --- low-level emission helpers -------------------------------------------

void Tms320C6713Codegen::loadImm32(const std::string &reg, int32_t v) {
    int32_t lo16 = static_cast<int32_t>(static_cast<int16_t>(v & 0xFFFF));
    if (v == lo16) {
        code_ << "    MVK " << reg << "," << lo16 << "\n";
        return;
    }
    code_ << "    MVK " << reg << "," << lo16 << "\n";
    uint32_t hi16 = (static_cast<uint32_t>(v) >> 16) & 0xFFFFu;
    code_ << "    MVKH " << reg << "," << hi16 << "\n";
}

void Tms320C6713Codegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    MVK " << reg << "," << lbl << "\n";
    code_ << "    MVKH " << reg << "," << lbl << "\n";
}

void Tms320C6713Codegen::adjustReg(const std::string &reg, int32_t delta) {
    if (delta == 0) return;
    loadImm32(T10, delta);
    code_ << "    ADD " << reg << "," << reg << "," << T10 << "\n";
}

void Tms320C6713Codegen::computeAddr(const std::string &dst, const std::string &baseReg, int32_t byteOffset) {
    loadImm32(dst, byteOffset);
    code_ << "    ADD " << dst << "," << dst << "," << baseReg << "\n";
}

void Tms320C6713Codegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
        return;
    }
    int offset = slot->kind == ir::SlotKind::Param ? slot->frameOffset : -ir::localLowOffset(*slot, typeWidths());
    computeAddr(reg, FP, offset);
}

void Tms320C6713Codegen::loadStack(const std::string &reg, int32_t byteOffset) {
    computeAddr(T2, SP, byteOffset);
    emitLoad("LDW", T2, reg);
}

void Tms320C6713Codegen::storeStack(const std::string &reg, int32_t byteOffset) {
    computeAddr(T2, SP, byteOffset);
    emitStore("STW", T2, reg);
}

void Tms320C6713Codegen::genPush(const std::string &reg) {
    adjustReg(SP, -4);
    emitStore("STW", SP, reg);
}

void Tms320C6713Codegen::genPop(const std::string &reg) {
    emitLoad("LDW", SP, reg);
    adjustReg(SP, 4);
}

void Tms320C6713Codegen::genPushWide(const std::string &lo, const std::string &hi) {
    adjustReg(SP, -8);
    emitStore("STW", SP, lo);
    computeAddr(T2, SP, 4);
    emitStore("STW", T2, hi);
}

void Tms320C6713Codegen::genPopWide(const std::string &lo, const std::string &hi) {
    emitLoad("LDW", SP, lo);
    computeAddr(T2, SP, 4);
    emitLoad("LDW", T2, hi);
    adjustReg(SP, 8);
}

void Tms320C6713Codegen::emitLoad(const std::string &mnemonic, const std::string &base, const std::string &reg) {
    code_ << "    " << mnemonic << " " << base << "," << reg << "\n";
    for (int i = 0; i < 4; i++) code_ << "    NOP\n";
}

void Tms320C6713Codegen::emitStore(const std::string &mnemonic, const std::string &base, const std::string &reg) {
    code_ << "    " << mnemonic << " " << base << "," << reg << "\n";
}

void Tms320C6713Codegen::emitMul(const std::string &mnemonic, const std::string &dst, const std::string &src1,
                                  const std::string &src2) {
    code_ << "    " << mnemonic << " " << dst << "," << src1 << "," << src2 << "\n";
    code_ << "    NOP\n";
}

void Tms320C6713Codegen::emitBranch(const std::string &label) {
    code_ << "    B " << label << "\n";
    for (int i = 0; i < 5; i++) code_ << "    NOP\n";
}

void Tms320C6713Codegen::emitBranchZ(bool branchIfZero, const std::string &label) {
    code_ << "    " << (branchIfZero ? "BZ " : "BNZ ") << label << "\n";
    for (int i = 0; i < 5; i++) code_ << "    NOP\n";
}

void Tms320C6713Codegen::emitBranchReg(const std::string &reg) {
    code_ << "    BR (" << reg << ")\n";
    for (int i = 0; i < 5; i++) code_ << "    NOP\n";
}

// --- program/function structure --------------------------------------------

std::string Tms320C6713Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // Establishes the permanent zero register (see this backend's own header comment);
        // never written again anywhere else in generated code.
        code_ << "    MVK " << ZERO << ",0\n";
        loadImm32(SP, static_cast<int32_t>(kInitialSP));
        loadAddress(RA, "halt");
        emitBranch(label(mainFn));
        code_ << "halt:\n";
        emitBranch("halt");
        code_ << "\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        int elemW = widthOfType(slot->type);
        size_t totalBytes = slot->arraySize > 0 ? static_cast<size_t>(slot->arraySize) * elemW
                                                  : static_cast<size_t>(elemW);
        // LDW/STW need 4-byte alignment; a `long long` is always accessed as two separate
        // 4-byte words (never one 8-byte transfer), so 4 bytes is this target's own widest
        // alignment requirement, matching MipsCodegen.cpp's own 32-bit-native data layout.
        size_t align = elemW >= 4 ? 4 : static_cast<size_t>(elemW);
        while (align > 1 && (dataOffset % align) != 0) {
            trailer_.push_back("db 0  ; alignment padding");
            dataOffset += 1;
        }
        dataOffset += totalBytes;
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
        } else if (slot->type.base == BaseType::Struct) {
            // No struct initializer exists in this subset, so this is
            // always zero-filled; reuses declareArray's own element-list
            // emission rather than the width-bucketed logic below, which
            // only handles 1/2/4/8-byte widths.
            declareArray(lbl, std::vector<ir::ArrayInitElem>(slot->type.structSize), Type{BaseType::Char, false});
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            if (elemW == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = elemW == 1 ? "db" : elemW == 2 ? "dh" : "dw";
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

std::map<std::string, std::string> Tms320C6713Codegen::perFileAsm() const { return perFileAsm_; }

// B14/B3 are saved before B14/B15 move, the same chicken-and-egg care AlphaCodegen.cpp's own
// genFunction() comment describes: the caller's fp/ra must land in the NEW frame (computed off
// the already-adjusted sp) before fp itself is repointed there.
void Tms320C6713Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    adjustReg(SP, -8);
    emitStore("STW", SP, FP);        // caller's fp at new sp+0
    computeAddr(T2, SP, 4);
    emitStore("STW", T2, RA);         // this call's own return address at new sp+4
    code_ << "    ADD " << FP << "," << SP << "," << ZERO << "\n";  // fp = sp
    if (fn.func->frameSize > 0) adjustReg(SP, -fn.func->frameSize);

    genStmt(*fn.body);

    genEpilogue();
    code_ << "\n";
}

void Tms320C6713Codegen::genEpilogue() {
    code_ << "    ADD " << SP << "," << FP << "," << ZERO << "\n";  // sp = fp (drop locals)
    emitLoad("LDW", SP, T9);                                        // T9 = saved caller's fp
    computeAddr(T2, SP, 4);
    emitLoad("LDW", T2, RA);                                        // ra = this call's own saved ra
    adjustReg(SP, 8);
    code_ << "    ADD " << FP << "," << T9 << "," << ZERO << "\n";  // fp = caller's fp
    emitBranchReg(RA);
}

void Tms320C6713Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            emitBranchZ(true, s.elseStmt ? Lelse : Ldone);
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                emitBranch(Ldone);
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
            if (isWide(s.cond->type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
            emitBranchZ(true, Lend);
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            emitBranch(Lstart);
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            emitBranch(loopLabels_.back().second);
            return;
        }
        case ir::StmtKind::Continue: {
            emitBranch(loopLabels_.back().first);
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            genEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadSlotAddr(T2, s.declSlot);
                storeOperand(T2, s.declSlot->type);
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
            emitBranch(irLabel(s.labelId));
            return;
        }
    }
}

void Tms320C6713Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImm32(AC, static_cast<int32_t>(static_cast<uint32_t>(v)));
                loadImm32(T1, static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
                return;
            }
            loadImm32(AC, static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
            narrowB0To(e.type);
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr(T2, e.slot);
            loadOperand(T2, e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {
                genIndexAddr(e.slot, *e.lhs);  // T2 = target address
                genPush(T2);
                genExpr(*e.rhs);
                genPop(T2);
                storeOperand(T2, e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr(T2, e.slot);
            storeOperand(T2, e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {
                genIndexAddr(e.slot, *e.lhs);
                genCompoundThroughT2(e);
                return;
            }
            loadSlotAddr(T2, e.slot);
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // AC = pointer value
            code_ << "    ADD " << T2 << "," << AC << "," << ZERO << "\n";
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            if (isWide(e.lhs->type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            emitBranchZ(true, Lelse);
            genExpr(*e.rhs);
            emitBranch(Ldone);
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // AC = pointer value
            genPush(AC);
            genExpr(*e.rhs);
            genPop(T2);
            storeOperand(T2, e.type);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    SUB " << AC << "," << ZERO << "," << AC << "\n";
                    narrowB0To(e.rhs->type);
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // AC = pointer value
                code_ << "    ADD " << T2 << "," << AC << "," << ZERO << "\n";
                loadOperand(T2, e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                loadImm32(T10, -1);
                code_ << "    XOR " << AC << "," << AC << "," << T10 << "\n";
                if (isWide(e.rhs->type)) {
                    code_ << "    XOR " << T1 << "," << T1 << "," << T10 << "\n";
                } else {
                    narrowB0To(e.rhs->type);
                }
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
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    genPushWide(AC, T1);
                    totalBytes += 8;
                } else {
                    genPush(AC);
                    totalBytes += 4;
                }
            }
            std::string retLbl = newLabel();
            loadAddress(RA, retLbl);
            emitBranch(label(e.func));
            code_ << retLbl << ":\n";
            if (totalBytes > 0) adjustReg(SP, totalBytes);
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);  // AC = the callee's own entry address
            code_ << "    ADD " << T2 << "," << AC << "," << ZERO << "\n";
            std::string retLbl = newLabel();
            loadAddress(RA, retLbl);
            emitBranchReg(T2);
            code_ << retLbl << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            (void)e;  // no exception/interrupt setup on this bare-metal target; no-op.
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

void Tms320C6713Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // AC = index
    int shift = elemShift(slot->type);
    if (shift > 0) {
        loadImm32(T10, shift);
        code_ << "    SHL " << AC << "," << AC << "," << T10 << "\n";
    }
    if (slot->arraySize > 0) {
        loadSlotAddr(T2, slot);
        code_ << "    ADD " << T2 << "," << T2 << "," << AC << "\n";
    } else {
        loadSlotAddr(T1, slot);
        emitLoad("LDW", T1, T1);  // T1 = the pointer variable's own stored value
        code_ << "    ADD " << T2 << "," << T1 << "," << AC << "\n";
    }
}

void Tms320C6713Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand(T2, e.slot->type);
}

void Tms320C6713Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int offset = -ir::localLowOffset(*slot, typeWidths()) + w * static_cast<int>(i);
        if (elem.isSymbol) {
            loadAddress(AC, elem.slot ? label(elem.slot) : label(elem.func));
            computeAddr(T2, FP, offset);
            emitStore("STW", T2, AC);
            continue;
        }
        if (w == 8) {
            auto v = static_cast<uint64_t>(elem.value);
            loadImm32(AC, static_cast<int32_t>(static_cast<uint32_t>(v)));
            loadImm32(T1, static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
            computeAddr(T2, FP, offset);
            emitStore("STW", T2, AC);
            computeAddr(T2, FP, offset + 4);
            emitStore("STW", T2, T1);
            continue;
        }
        loadImm32(AC, static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        computeAddr(T2, FP, offset);
        emitStore(w == 1 ? "STB" : w == 2 ? "STH" : "STW", T2, AC);
    }
}

void Tms320C6713Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr(T2, operand.slot);
        code_ << "    ADD " << AC << "," << T2 << "," << ZERO << "\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    ADD " << AC << "," << T2 << "," << ZERO << "\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress(T2, label(operand.func));
        code_ << "    ADD " << AC << "," << T2 << "," << ZERO << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Tms320C6713Codegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        emitLoad("LDB", addrReg, AC);
        narrowB0To(type);
        return;
    }
    if (w == 2) {
        emitLoad("LDH", addrReg, AC);
        narrowB0To(type);
        return;
    }
    if (w == 4) {
        emitLoad("LDW", addrReg, AC);
        return;
    }
    // w == 8
    emitLoad("LDW", addrReg, AC);     // lo
    computeAddr(T9, addrReg, 4);       // T9 = addrReg+4 (never disturbs addrReg itself)
    emitLoad("LDW", T9, T1);            // hi
}

void Tms320C6713Codegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        emitStore("STB", addrReg, AC);
        return;
    }
    if (w == 2) {
        emitStore("STH", addrReg, AC);
        return;
    }
    if (w == 4) {
        emitStore("STW", addrReg, AC);
        return;
    }
    emitStore("STW", addrReg, AC);
    computeAddr(T9, addrReg, 4);
    emitStore("STW", T9, T1);
}

void Tms320C6713Codegen::narrowB0To(Type to) {
    int w = widthOfType(to);
    if (w >= 4) return;
    int shiftAmt = 32 - 8 * w;
    loadImm32(T10, shiftAmt);
    code_ << "    SHL " << AC << "," << AC << "," << T10 << "\n";
    code_ << "    " << (to.isUnsigned ? "SHRU" : "SHR") << " " << AC << "," << AC << "," << T10 << "\n";
}

void Tms320C6713Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowB0To(to);  // T1 simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            loadImm32(T1, 0);
        } else {
            loadImm32(T10, 31);
            code_ << "    SHR " << T1 << "," << AC << "," << T10 << "\n";  // sign-fill T1
        }
    } else if (!fromWide && !toWide) {
        narrowB0To(to);
    }
    // fromWide && toWide: nothing to do.
}

// --- multiply ---------------------------------------------------------------

// Standard TI 16x16-decomposition sequence (see this backend's header comment): the low 32
// bits of src1*src2, correct regardless of either operand's sign interpretation since
// multiplication mod 2^32 only depends on each operand's own bit pattern mod 2^32. `dst` is
// only ever written by the final instruction, so it may safely alias `src1`/`src2`.
// The plain MPY+MPYHL+MPYLH sum (as above, shifted and added) is only *almost* low32(A*B):
// MPY itself sign-extends each operand's low halfword before multiplying, so whenever A's (or
// B's) low 16 bits have their own bit 15 set, MPY's result is short by exactly 0x10000*B (resp.
// 0x10000*A) relative to the unsigned-low16*unsigned-low16 product the textbook decomposition
// actually needs - an error that does NOT cancel out the way the equivalent error in
// MPYHL/MPYLH's own cross terms does once shifted left 16 (that one lands exactly on a multiple
// of 2^32 and vanishes mod 2^32; this one doesn't). Correct for it explicitly below via a
// branch-free mask (0 or 0xFFFFFFFF from each operand's own low-halfword sign bit) rather than
// trust the sequence to be exact as a bare sum.
void Tms320C6713Codegen::genMulLow32(const std::string &dst, const std::string &src1, const std::string &src2) {
    code_ << "    ADD " << T6 << "," << src1 << "," << ZERO << "\n";  // T6 = A (safe copy)
    code_ << "    ADD " << T7 << "," << src2 << "," << ZERO << "\n";   // T7 = B (safe copy)

    emitMul("MPY", T3, T6, T7);     // T3 = lo*lo partial product
    emitMul("MPYHL", T4, T6, T7);    // T4 = A_hi * B_lo
    emitMul("MPYLH", T5, T6, T7);     // T5 = A_lo * B_hi
    code_ << "    ADD " << T4 << "," << T4 << "," << T5 << "\n";
    loadImm32(T8, 16);
    code_ << "    SHL " << T4 << "," << T4 << "," << T8 << "\n";
    code_ << "    ADD " << T3 << "," << T3 << "," << T4 << "\n";

    // If A's low halfword's own sign bit is set, add B's raw low halfword (unshifted sign
    // error) back in, shifted into position.
    loadImm32(T8, 0x8000);
    code_ << "    AND " << T9 << "," << T6 << "," << T8 << "\n";
    loadImm32(T8, 15);
    code_ << "    SHRU " << T9 << "," << T9 << "," << T8 << "\n";  // clean 0/1
    code_ << "    SUB " << T9 << "," << ZERO << "," << T9 << "\n";  // mask: 0 or all-ones
    loadImm32(T8, 0xFFFF);
    code_ << "    AND " << T10 << "," << T7 << "," << T8 << "\n";
    code_ << "    AND " << T10 << "," << T10 << "," << T9 << "\n";
    loadImm32(T8, 16);
    code_ << "    SHL " << T10 << "," << T10 << "," << T8 << "\n";
    code_ << "    ADD " << T3 << "," << T3 << "," << T10 << "\n";

    // Symmetric correction if B's low halfword's own sign bit is set.
    loadImm32(T8, 0x8000);
    code_ << "    AND " << T9 << "," << T7 << "," << T8 << "\n";
    loadImm32(T8, 15);
    code_ << "    SHRU " << T9 << "," << T9 << "," << T8 << "\n";
    code_ << "    SUB " << T9 << "," << ZERO << "," << T9 << "\n";
    loadImm32(T8, 0xFFFF);
    code_ << "    AND " << T10 << "," << T6 << "," << T8 << "\n";
    code_ << "    AND " << T10 << "," << T10 << "," << T9 << "\n";
    loadImm32(T8, 16);
    code_ << "    SHL " << T10 << "," << T10 << "," << T8 << "\n";
    code_ << "    ADD " << T3 << "," << T3 << "," << T10 << "\n";

    code_ << "    ADD " << dst << "," << T3 << "," << ZERO << "\n";
}

// Full unsigned 64-bit product of two 32-bit registers via a 32-iteration shift-and-add loop
// (see this backend's own header comment on why the sign-extending MPY family can't give this
// directly). `aReg`/`bReg` are copied out before anything else is touched, so `outLo`/`outHi`
// may safely alias either of them.
void Tms320C6713Codegen::genMulFull32(const std::string &aReg, const std::string &bReg, const std::string &outLo,
                                       const std::string &outHi) {
    code_ << "    ADD " << T6 << "," << aReg << "," << ZERO << "\n";  // addend lo = a
    loadImm32(T7, 0);                                                  // addend hi = 0
    code_ << "    ADD " << T8 << "," << bReg << "," << ZERO << "\n";    // multiplier = b
    loadImm32(T3, 0);                                                    // result lo = 0
    loadImm32(T4, 0);                                                     // result hi = 0
    loadImm32(T9, 32);                                                      // counter = 32

    std::string Lloop = newLabel(), Ldone = newLabel(), Lskip = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    CMPEQ " << AC << "," << T9 << "," << ZERO << "\n";
    emitBranchZ(false, Ldone);  // counter == 0 -> done
    loadImm32(T10, -1);
    code_ << "    ADD " << T9 << "," << T9 << "," << T10 << "\n";

    loadImm32(T10, 1);
    code_ << "    AND " << AC << "," << T8 << "," << T10 << "\n";  // bit = multiplier & 1
    emitBranchZ(true, Lskip);
    // result += addend (64-bit add with carry)
    code_ << "    ADD " << T11 << "," << T3 << "," << T6 << "\n";       // sum = resultLo+addendLo
    code_ << "    CMPLTU " << AC << "," << T11 << "," << T3 << "\n";     // carry
    code_ << "    ADD " << T4 << "," << T4 << "," << T7 << "\n";
    code_ << "    ADD " << T4 << "," << T4 << "," << AC << "\n";
    code_ << "    ADD " << T3 << "," << T11 << "," << ZERO << "\n";
    code_ << Lskip << ":\n";
    // addend <<= 1 (64-bit)
    loadImm32(T10, 31);
    code_ << "    SHRU " << AC << "," << T6 << "," << T10 << "\n";  // top bit of addend lo
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << "    SHL " << T7 << "," << T7 << "," << T10 << "\n";
    code_ << "    OR " << T7 << "," << T7 << "," << AC << "\n";
    // multiplier >>>= 1
    loadImm32(T10, 1);
    code_ << "    SHRU " << T8 << "," << T8 << "," << T10 << "\n";
    emitBranch(Lloop);
    code_ << Ldone << ":\n";

    code_ << "    ADD " << outLo << "," << T3 << "," << ZERO << "\n";
    code_ << "    ADD " << outHi << "," << T4 << "," << ZERO << "\n";
}

void Tms320C6713Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    genPush(AC);
    genExpr(rhs);
    code_ << "    ADD " << T1 << "," << AC << "," << ZERO << "\n";  // T1 = rhs
    genPop(AC);                                                      // AC = lhs
    genMulLow32(AC, AC, T1);
}

// `long long` multiply, low 64 bits of the 128-bit product: result = lo64(lhsLo*rhsLo) +
// (low32(lhsLo*rhsHi + lhsHi*rhsLo) << 32), the standard cross-term decomposition - see this
// backend's own header comment.
void Tms320C6713Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    genPushWide(AC, T1);
    genExpr(rhs);
    genPushWide(AC, T1);
    // stack layout (relative to current sp): +0 rhs_lo, +4 rhs_hi, +8 lhs_lo, +12 lhs_hi
    loadStack(AC, 8);
    loadStack(T1, 4);
    genMulLow32(T5, AC, T1);  // T5 = crossA = lhsLo*rhsHi (low32)
    loadStack(AC, 12);
    loadStack(T1, 0);
    genMulLow32(T2, AC, T1);  // T2 = crossB = lhsHi*rhsLo (low32)
    code_ << "    ADD " << T5 << "," << T5 << "," << T2 << "\n";  // T5 = crossSum

    loadStack(AC, 8);
    loadStack(T1, 0);
    genMulFull32(AC, T1, AC, T1);  // AC:T1 = full lhsLo*rhsLo (lo:hi)
    code_ << "    ADD " << T1 << "," << T1 << "," << T5 << "\n";

    adjustReg(SP, 16);
}

// `long long` add/sub: stash lhs on the stack while rhs evaluates, then ripple carry/borrow
// across the word pair via CMPLTU (there's no carry flag on this ISA either).
void Tms320C6713Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    genPushWide(AC, T1);
    genExpr(rhs);  // AC:T1 = rhs
    code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";
    code_ << "    ADD " << T4 << "," << T1 << "," << ZERO << "\n";
    genPopWide(AC, T1);  // AC:T1 = lhs
    if (isAdd) {
        code_ << "    ADD " << T5 << "," << AC << "," << T3 << "\n";
        code_ << "    CMPLTU " << T6 << "," << T5 << "," << AC << "\n";
        code_ << "    ADD " << T1 << "," << T1 << "," << T4 << "\n";
        code_ << "    ADD " << T1 << "," << T1 << "," << T6 << "\n";
        code_ << "    ADD " << AC << "," << T5 << "," << ZERO << "\n";
    } else {
        code_ << "    CMPLTU " << T6 << "," << AC << "," << T3 << "\n";
        code_ << "    SUB " << T5 << "," << AC << "," << T3 << "\n";
        code_ << "    SUB " << T1 << "," << T1 << "," << T4 << "\n";
        code_ << "    SUB " << T1 << "," << T1 << "," << T6 << "\n";
        code_ << "    ADD " << AC << "," << T5 << "," << ZERO << "\n";
    }
}

// A `long long` shifted by a runtime count: a loop of 1-bit shifts, each carrying the bit
// across the word boundary by hand (SHL/SHR/SHRU only ever shift one 32-bit word). The value
// being shifted lives in T4:T5 throughout (never AC:T1), since the loop-exit test needs AC free
// to hold the predicate each iteration.
void Tms320C6713Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    genPushWide(AC, T1);
    genExpr(rhs);  // AC = shift count
    code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";
    genPopWide(T4, T5);  // T4:T5 = value (lo:hi)

    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    CMPEQ " << AC << "," << T3 << "," << ZERO << "\n";
    emitBranchZ(false, Ldone);
    loadImm32(T10, -1);
    code_ << "    ADD " << T3 << "," << T3 << "," << T10 << "\n";
    if (isLeft) {
        loadImm32(T10, 31);
        code_ << "    SHRU " << T6 << "," << T4 << "," << T10 << "\n";  // carry from lo's top bit
        loadImm32(T10, 1);
        code_ << "    SHL " << T4 << "," << T4 << "," << T10 << "\n";
        code_ << "    SHL " << T5 << "," << T5 << "," << T10 << "\n";
        code_ << "    OR " << T5 << "," << T5 << "," << T6 << "\n";
    } else {
        loadImm32(T10, 1);
        code_ << "    AND " << T6 << "," << T5 << "," << T10 << "\n";  // hi's bit0
        loadImm32(T10, 31);
        code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";  // move into lo's top-bit position
        loadImm32(T10, 1);
        code_ << "    " << (isUnsigned ? "SHRU" : "SHR") << " " << T5 << "," << T5 << "," << T10 << "\n";
        code_ << "    SHRU " << T4 << "," << T4 << "," << T10 << "\n";
        code_ << "    OR " << T4 << "," << T4 << "," << T6 << "\n";
    }
    emitBranch(Lloop);
    code_ << Ldone << ":\n";
    code_ << "    ADD " << AC << "," << T4 << "," << ZERO << "\n";
    code_ << "    ADD " << T1 << "," << T5 << "," << ZERO << "\n";
}

// Negates AC:T1 (two's complement) in place.
void Tms320C6713Codegen::genNegate64() {
    loadImm32(T10, -1);
    code_ << "    XOR " << AC << "," << AC << "," << T10 << "\n";
    code_ << "    XOR " << T1 << "," << T1 << "," << T10 << "\n";
    loadImm32(T10, 1);
    code_ << "    ADD " << AC << "," << AC << "," << T10 << "\n";
    code_ << "    CMPEQ " << T11 << "," << AC << "," << ZERO << "\n";  // AC wrapped to 0 -> carry
    code_ << "    ADD " << T1 << "," << T1 << "," << T11 << "\n";
}

// --- compare/logical ---------------------------------------------------------

void Tms320C6713Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    genPush(AC);
    genExpr(rhs);  // AC = rhs
    genPop(T1);     // T1 = lhs
    if (op == "<") {
        code_ << "    " << (isUnsigned ? "CMPLTU" : "CMPLT") << " " << AC << "," << T1 << "," << AC << "\n";
        return;
    }
    if (op == ">") {
        code_ << "    " << (isUnsigned ? "CMPGTU" : "CMPGT") << " " << AC << "," << T1 << "," << AC << "\n";
        return;
    }
    if (op == "<=") {
        code_ << "    " << (isUnsigned ? "CMPGTU" : "CMPGT") << " " << T10 << "," << T1 << "," << AC << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
        return;
    }
    if (op == ">=") {
        code_ << "    " << (isUnsigned ? "CMPLTU" : "CMPLT") << " " << T10 << "," << T1 << "," << AC << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
        return;
    }
    if (op == "==") {
        code_ << "    CMPEQ " << AC << "," << T1 << "," << AC << "\n";
        return;
    }
    if (op == "!=") {
        code_ << "    CMPEQ " << T10 << "," << T1 << "," << AC << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
        return;
    }
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Compares lo-then-hi for ==/!= (either word differing is enough), hi-then-lo (falling back to
// an unsigned lo compare only once the hi words match) for an ordered comparison - same shape
// as MipsCodegen.cpp's own genWideCompare(). lhs/rhs are moved out of AC/T1 immediately (into
// T6:T7/T3:T4) so AC stays free to hold each comparison's own predicate test.
void Tms320C6713Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs,
                                         bool isUnsigned) {
    genExpr(lhs);
    genPushWide(AC, T1);
    genExpr(rhs);  // AC:T1 = rhs
    code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";
    code_ << "    ADD " << T4 << "," << T1 << "," << ZERO << "\n";
    genPopWide(AC, T1);  // AC:T1 = lhs
    code_ << "    ADD " << T6 << "," << AC << "," << ZERO << "\n";
    code_ << "    ADD " << T7 << "," << T1 << "," << ZERO << "\n";

    if (op == "==" || op == "!=") {
        std::string Lfalse = newLabel(), Ldone = newLabel();
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    CMPEQ " << AC << "," << T6 << "," << T3 << "\n";
        emitBranchZ(true, Lfalse);
        code_ << "    CMPEQ " << AC << "," << T7 << "," << T4 << "\n";
        emitBranchZ(true, Lfalse);
        loadImm32(AC, atoi(onEqual));
        emitBranch(Ldone);
        code_ << Lfalse << ":\n";
        loadImm32(AC, atoi(onDiffer));
        code_ << Ldone << ":\n";
        return;
    }

    std::string LhiDiff = newLabel(), Ldone = newLabel();
    code_ << "    CMPEQ " << AC << "," << T7 << "," << T4 << "\n";
    emitBranchZ(true, LhiDiff);
    // hi equal: unsigned lo compare
    if (op == "<") {
        code_ << "    CMPLTU " << AC << "," << T6 << "," << T3 << "\n";
    } else if (op == ">") {
        code_ << "    CMPGTU " << AC << "," << T6 << "," << T3 << "\n";
    } else if (op == "<=") {
        code_ << "    CMPGTU " << T10 << "," << T6 << "," << T3 << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
    } else {  // ">="
        code_ << "    CMPLTU " << T10 << "," << T6 << "," << T3 << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
    }
    emitBranch(Ldone);
    code_ << LhiDiff << ":\n";
    const char *ltInsn = isUnsigned ? "CMPLTU" : "CMPLT";
    const char *gtInsn = isUnsigned ? "CMPGTU" : "CMPGT";
    if (op == "<") {
        code_ << "    " << ltInsn << " " << AC << "," << T7 << "," << T4 << "\n";
    } else if (op == ">") {
        code_ << "    " << gtInsn << " " << AC << "," << T7 << "," << T4 << "\n";
    } else if (op == "<=") {
        code_ << "    " << gtInsn << " " << T10 << "," << T7 << "," << T4 << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
    } else {  // ">="
        code_ << "    " << ltInsn << " " << T10 << "," << T7 << "," << T4 << "\n";
        loadImm32(T11, 1);
        code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";
    }
    code_ << Ldone << ":\n";
}

void Tms320C6713Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    emitBranchZ(true, Lfalse);
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
    emitBranchZ(true, Lfalse);
    emitBranch(Ltrue);
    code_ << Lfalse << ":\n";
    loadImm32(AC, 0);
    emitBranch(Ldone);
    code_ << Ltrue << ":\n";
    loadImm32(AC, 1);
    code_ << Ldone << ":\n";
}

void Tms320C6713Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    emitBranchZ(false, Ltrue);
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
    emitBranchZ(false, Ltrue);
    emitBranch(Lfalse);
    code_ << Ltrue << ":\n";
    loadImm32(AC, 1);
    emitBranch(Ldone);
    code_ << Lfalse << ":\n";
    loadImm32(AC, 0);
    code_ << Ldone << ":\n";
}

void Tms320C6713Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
    code_ << "    CMPEQ " << AC << "," << AC << "," << ZERO << "\n";
}

// --- division ---------------------------------------------------------------

void Tms320C6713Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    genPush(AC);
    genExpr(rhs);  // AC = divisor
    code_ << "    ADD " << T1 << "," << AC << "," << ZERO << "\n";
    genPop(AC);  // AC = dividend
    genDivModCore(wantRemainder, isUnsigned);
}

// Bit-serial restoring division (no hardware divide at all on this target - see this backend's
// header comment), mirroring AlphaCodegen.cpp's own genDivModCore() but for a fixed 32-bit
// width. Entry: AC=dividend, T1=divisor. The dividend/divisor/remainder/quotient/signs all live
// in T2..T9 throughout (never AC/T1), since the loop's own exit test and sign checks need AC
// free to hold each predicate in turn.
void Tms320C6713Codegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";  // T3 = dividend (n)
    code_ << "    ADD " << T4 << "," << T1 << "," << ZERO << "\n";   // T4 = divisor (d)

    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        loadImm32(T10, 31);
        code_ << "    SHRU " << T7 << "," << T3 << "," << T10 << "\n";  // T7 = dividend sign
        code_ << "    ADD " << AC << "," << T7 << "," << ZERO << "\n";
        emitBranchZ(true, LskipNegN);
        code_ << "    SUB " << T3 << "," << ZERO << "," << T3 << "\n";
        code_ << LskipNegN << ":\n";
        code_ << "    SHRU " << T8 << "," << T4 << "," << T10 << "\n";  // T8 = divisor sign
        code_ << "    ADD " << AC << "," << T8 << "," << ZERO << "\n";
        emitBranchZ(true, LskipNegD);
        code_ << "    SUB " << T4 << "," << ZERO << "," << T4 << "\n";
        code_ << LskipNegD << ":\n";
    }

    loadImm32(T5, 0);   // r
    loadImm32(T6, 0);    // q
    loadImm32(T9, 32);    // cnt

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LcheckCmp = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), Lcont = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    CMPEQ " << AC << "," << T9 << "," << ZERO << "\n";
    emitBranchZ(false, Ldone);
    loadImm32(T10, -1);
    code_ << "    ADD " << T9 << "," << T9 << "," << T10 << "\n";

    loadImm32(T10, 31);
    code_ << "    SHRU " << T11 << "," << T3 << "," << T10 << "\n";  // dividend's own top bit
    loadImm32(T10, 31);
    code_ << "    SHRU " << T1 << "," << T5 << "," << T10 << "\n";    // remainder's own top bit (overflow
                                                                        // flag) - T1 is free here: its own
                                                                        // entry value (the divisor) was
                                                                        // already copied into T4 above, and
                                                                        // T2 must NOT be used (genCompoundOp's
                                                                        // own caller, genCompoundThroughT2,
                                                                        // keeps the live target address there
                                                                        // across this whole call)
    loadImm32(T10, 1);
    code_ << "    SHL " << T3 << "," << T3 << "," << T10 << "\n";      // n <<= 1
    loadImm32(T10, 1);
    code_ << "    SHL " << T5 << "," << T5 << "," << T10 << "\n";       // r <<= 1
    code_ << "    OR " << T5 << "," << T5 << "," << T11 << "\n";         // r |= n's captured top bit

    code_ << "    ADD " << AC << "," << T1 << "," << ZERO << "\n";
    emitBranchZ(true, LcheckCmp);
    emitBranch(LsubYes);
    code_ << LcheckCmp << ":\n";
    code_ << "    CMPLTU " << AC << "," << T5 << "," << T4 << "\n";  // r < d ?
    emitBranchZ(false, LsubNo);
    code_ << LsubYes << ":\n";
    code_ << "    SUB " << T5 << "," << T5 << "," << T4 << "\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << "    OR " << T6 << "," << T6 << "," << T10 << "\n";
    emitBranch(Lcont);
    code_ << LsubNo << ":\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << Lcont << ":\n";
    emitBranch(Lloop);
    code_ << Ldone << ":\n";

    if (!isUnsigned) {
        std::string LnoNegQ = newLabel(), LnoNegR = newLabel();
        code_ << "    XOR " << AC << "," << T7 << "," << T8 << "\n";
        emitBranchZ(true, LnoNegQ);
        code_ << "    SUB " << T6 << "," << ZERO << "," << T6 << "\n";
        code_ << LnoNegQ << ":\n";
        code_ << "    ADD " << AC << "," << T7 << "," << ZERO << "\n";
        emitBranchZ(true, LnoNegR);
        code_ << "    SUB " << T5 << "," << ZERO << "," << T5 << "\n";
        code_ << LnoNegR << ":\n";
    }
    code_ << "    ADD " << AC << "," << (wantRemainder ? T5 : T6) << "," << ZERO << "\n";
}

// 64-bit counterpart of genDivModCore(), the same algorithm run on register pairs kept in a
// small stack-relative scratch block (same technique as MipsCodegen.cpp's own
// genWideDivMod()), since there aren't enough registers to hold n/d/r/q (8 32-bit words) plus
// loop temporaries at once. Field layout (byte offsets from the reserved block's own base):
// 0 n_lo, 4 n_hi, 8 d_lo, 12 d_hi, 16 r_lo, 20 r_hi, 24 q_lo, 28 q_hi, 32 cnt, 36 lhsSign,
// 40 rhsSign.
void Tms320C6713Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder,
                                        bool isUnsigned) {
    adjustReg(SP, -44);
    genExpr(lhs);
    storeStack(AC, 0);
    storeStack(T1, 4);
    genExpr(rhs);
    storeStack(AC, 8);
    storeStack(T1, 12);
    storeStack(ZERO, 16);
    storeStack(ZERO, 20);
    storeStack(ZERO, 24);
    storeStack(ZERO, 28);
    loadImm32(T3, 64);
    storeStack(T3, 32);

    std::string Lskip1 = newLabel(), Lskip2 = newLabel();
    if (!isUnsigned) {
        loadImm32(T10, 31);
        loadStack(AC, 4);
        code_ << "    SHRU " << T3 << "," << AC << "," << T10 << "\n";
        storeStack(T3, 36);
        loadStack(AC, 12);
        code_ << "    SHRU " << T4 << "," << AC << "," << T10 << "\n";
        storeStack(T4, 40);

        code_ << "    ADD " << AC << "," << T3 << "," << ZERO << "\n";
        emitBranchZ(true, Lskip1);
        loadStack(AC, 0);
        loadStack(T1, 4);
        genNegate64();
        storeStack(AC, 0);
        storeStack(T1, 4);
        code_ << Lskip1 << ":\n";

        code_ << "    ADD " << AC << "," << T4 << "," << ZERO << "\n";
        emitBranchZ(true, Lskip2);
        loadStack(AC, 8);
        loadStack(T1, 12);
        genNegate64();
        storeStack(AC, 8);
        storeStack(T1, 12);
        code_ << Lskip2 << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    loadStack(T3, 32);
    code_ << "    CMPEQ " << AC << "," << T3 << "," << ZERO << "\n";
    emitBranchZ(false, Ldone);
    loadImm32(T10, -1);
    code_ << "    ADD " << T3 << "," << T3 << "," << T10 << "\n";
    storeStack(T3, 32);

    // capture n's top bit, then n <<= 1 (64-bit)
    loadStack(T4, 4);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T5 << "," << T4 << "," << T10 << "\n";  // n's top bit
    loadStack(T6, 0);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T7 << "," << T6 << "," << T10 << "\n";  // n_lo's own top bit (carries into hi)
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T4 << "," << T4 << "," << T10 << "\n";
    code_ << "    OR " << T4 << "," << T4 << "," << T7 << "\n";
    storeStack(T6, 0);
    storeStack(T4, 4);

    // r = (r<<1)|T5, capturing r's own (pre-shift) top bit as the overflow flag T9
    loadStack(T8, 20);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T9 << "," << T8 << "," << T10 << "\n";  // overflow flag
    loadStack(T11, 16);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T2 << "," << T11 << "," << T10 << "\n";  // r_lo's own top bit (carries into hi)
    loadImm32(T10, 1);
    code_ << "    SHL " << T11 << "," << T11 << "," << T10 << "\n";
    code_ << "    OR " << T11 << "," << T11 << "," << T5 << "\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T8 << "," << T8 << "," << T10 << "\n";
    code_ << "    OR " << T8 << "," << T8 << "," << T2 << "\n";
    storeStack(T11, 16);
    storeStack(T8, 20);

    std::string LcheckCmp = newLabel(), LloCmp = newLabel(), LcmpDone = newLabel();
    std::string LsubYes = newLabel(), LsubNo = newLabel(), Lcont = newLabel();
    code_ << "    ADD " << AC << "," << T9 << "," << ZERO << "\n";
    emitBranchZ(true, LcheckCmp);
    emitBranch(LsubYes);
    code_ << LcheckCmp << ":\n";
    // unsigned 64-bit compare: r (now at 16/20) >= d (at 8/12)
    loadStack(T4, 20);
    loadStack(T6, 12);
    code_ << "    CMPEQ " << AC << "," << T4 << "," << T6 << "\n";
    emitBranchZ(false, LloCmp);
    code_ << "    CMPGTU " << AC << "," << T4 << "," << T6 << "\n";
    emitBranch(LcmpDone);
    code_ << LloCmp << ":\n";
    loadStack(T5, 16);  // r_lo
    loadStack(T7, 8);    // d_lo
    code_ << "    CMPLTU " << T10 << "," << T5 << "," << T7 << "\n";  // r_lo < d_lo ?
    loadImm32(T11, 1);
    code_ << "    XOR " << AC << "," << T10 << "," << T11 << "\n";  // AC = r_lo >= d_lo
    code_ << LcmpDone << ":\n";
    emitBranchZ(true, LsubNo);
    code_ << LsubYes << ":\n";
    loadStack(T4, 16);
    loadStack(T6, 8);
    code_ << "    CMPLTU " << T9 << "," << T4 << "," << T6 << "\n";
    code_ << "    SUB " << T5 << "," << T4 << "," << T6 << "\n";
    loadStack(T4, 20);
    loadStack(T6, 12);
    code_ << "    SUB " << T4 << "," << T4 << "," << T6 << "\n";
    code_ << "    SUB " << T4 << "," << T4 << "," << T9 << "\n";
    storeStack(T5, 16);
    storeStack(T4, 20);
    loadStack(T6, 24);
    loadStack(T7, 28);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T11 << "," << T6 << "," << T10 << "\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << "    OR " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << "    SHL " << T7 << "," << T7 << "," << T10 << "\n";
    code_ << "    OR " << T7 << "," << T7 << "," << T11 << "\n";
    storeStack(T6, 24);
    storeStack(T7, 28);
    emitBranch(Lcont);
    code_ << LsubNo << ":\n";
    loadStack(T6, 24);
    loadStack(T7, 28);
    loadImm32(T10, 31);
    code_ << "    SHRU " << T11 << "," << T6 << "," << T10 << "\n";
    loadImm32(T10, 1);
    code_ << "    SHL " << T6 << "," << T6 << "," << T10 << "\n";
    code_ << "    SHL " << T7 << "," << T7 << "," << T10 << "\n";
    code_ << "    OR " << T7 << "," << T7 << "," << T11 << "\n";
    storeStack(T6, 24);
    storeStack(T7, 28);
    code_ << Lcont << ":\n";
    emitBranch(Lloop);
    code_ << Ldone << ":\n";

    if (!isUnsigned) {
        std::string LnoNegQ = newLabel(), LnoNegR = newLabel();
        loadStack(T3, 36);
        loadStack(T4, 40);
        code_ << "    XOR " << AC << "," << T3 << "," << T4 << "\n";
        emitBranchZ(true, LnoNegQ);
        loadStack(AC, 24);
        loadStack(T1, 28);
        genNegate64();
        storeStack(AC, 24);
        storeStack(T1, 28);
        code_ << LnoNegQ << ":\n";
        loadStack(T3, 36);
        code_ << "    ADD " << AC << "," << T3 << "," << ZERO << "\n";
        emitBranchZ(true, LnoNegR);
        loadStack(AC, 16);
        loadStack(T1, 20);
        genNegate64();
        storeStack(AC, 16);
        storeStack(T1, 20);
        code_ << LnoNegR << ":\n";
    }

    if (wantRemainder) {
        loadStack(AC, 16);
        loadStack(T1, 20);
    } else {
        loadStack(AC, 24);
        loadStack(T1, 28);
    }
    adjustReg(SP, 44);
}

// --- binary dispatch --------------------------------------------------------

void Tms320C6713Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") {
        code_ << "    ADD " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    if (op == "-") {
        code_ << "    SUB " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    if (op == "*") {
        genMulLow32(AC, AC, T1);
        return;
    }
    if (op == "&") {
        code_ << "    AND " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    if (op == "|") {
        code_ << "    OR " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    if (op == "^") {
        code_ << "    XOR " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "SHL" : (isUnsigned ? "SHRU" : "SHR");
        code_ << "    " << insn << " " << AC << "," << AC << "," << T1 << "\n";
        return;
    }
    // "/" or "%"
    genDivModCore(op == "%", isUnsigned);
}

void Tms320C6713Codegen::genCompoundThroughT2(const ir::Expr &e) {
    genPush(T2);
    genExpr(*e.rhs);
    code_ << "    ADD " << T1 << "," << AC << "," << ZERO << "\n";
    genPop(T2);
    loadOperand(T2, e.type);
    if (e.yieldOld) genPush(AC);
    genCompoundOp(e.op, e.type);
    narrowB0To(e.type);
    if (e.yieldOld) {
        code_ << "    ADD " << T1 << "," << AC << "," << ZERO << "\n";
        genPop(AC);
        code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";
        code_ << "    ADD " << AC << "," << T1 << "," << ZERO << "\n";
        storeOperand(T2, e.type);
        code_ << "    ADD " << AC << "," << T3 << "," << ZERO << "\n";
    } else {
        storeOperand(T2, e.type);
    }
}

void Tms320C6713Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        genPush(AC);
        genExpr(*e.rhs);
        genPop(T1);  // T1 = lhs, AC = rhs
        code_ << "    " << (op == "+" ? "ADD" : "SUB") << " " << AC << "," << T1 << "," << AC << "\n";
        narrowB0To(operandType);
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
            return;
        }
        genMul(*e.lhs, *e.rhs);
        narrowB0To(operandType);
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
        const char *insn = op == "&" ? "AND" : op == "|" ? "OR" : "XOR";
        if (wide) {
            genExpr(*e.lhs);
            genPushWide(AC, T1);
            genExpr(*e.rhs);
            code_ << "    ADD " << T3 << "," << AC << "," << ZERO << "\n";
            code_ << "    ADD " << T4 << "," << T1 << "," << ZERO << "\n";
            genPopWide(AC, T1);
            code_ << "    " << insn << " " << AC << "," << AC << "," << T3 << "\n";
            code_ << "    " << insn << " " << T1 << "," << T1 << "," << T4 << "\n";
            return;
        }
        genExpr(*e.lhs);
        genPush(AC);
        genExpr(*e.rhs);
        genPop(T1);
        code_ << "    " << insn << " " << AC << "," << T1 << "," << AC << "\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        genPush(AC);
        genExpr(*e.rhs);  // AC = shift count
        code_ << "    ADD " << T1 << "," << AC << "," << ZERO << "\n";
        genPop(AC);  // AC = value
        const char *insn = op == "<<" ? "SHL" : (isUnsigned ? "SHRU" : "SHR");
        code_ << "    " << insn << " " << AC << "," << AC << "," << T1 << "\n";
        narrowB0To(operandType);
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
    if (wide) {
        genWideCompare(op, *e.lhs, *e.rhs, isUnsigned);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs);
}

std::optional<std::string> Tms320C6713Codegen::assembleInProcess(const std::string &asmText,
                                                                   const std::string &format) const {
    if (format == "raw") return assembleTms320C6713(asmText);
    return std::nullopt;
}

}  // namespace rcc
