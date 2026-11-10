#include "MCS51Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "MCS51Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// The software call-frame stack every activation's locals/params are
// carved out of at runtime: one fixed-size XDATA buffer. 2048 bytes is
// comfortably past any realistic recursion depth; overflow past it is
// unchecked.
constexpr const char *kFrameStackLabel = "__frame_stack";
constexpr int kFrameStackBytes = 2048;
}  // namespace

void MCS51Codegen::allocXdata(const std::string &lbl, int count) {
    // `lbl` and the `xdata` statement must share one physical line
    // (the grammar only attaches a label to a statement on that line).
    code_ << lbl << ": xdata " << count << "\n";
}

void MCS51Codegen::declareGlobalTemp(const std::string &lbl, long initValue) {
    allocXdata(lbl, 2);
    uint16_t v16 = static_cast<uint16_t>(initValue);
    initCode_ << "    movdptr " << lbl << "\n";
    initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
    initCode_ << "    incdptr\n";
    initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
}

// Writes array element `e` into the XDATA cell at `elemExpr`, exactly
// `elemWidth` bytes. A symbol element (address-of function/global) has
// no compile-time byte value, so its address is loaded into `bc` via
// loadCodeAddress()/loadXdataAddress() first, then stored - always a
// full 2-byte address regardless of `elemWidth`.
void MCS51Codegen::genArrayElementInit(const std::string &elemExpr, int elemWidth, const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        if (e.func) {
            initCode_ << "    movdptrc " << label(e.func) << "\n";
        } else {
            initCode_ << "    movdptr " << label(e.slot) << "\n";
        }
        initCode_ << "    mova dpl\n    movd bc\n";
        initCode_ << "    mova dph\n    movd bc+1\n";
        initCode_ << "    movdptr " << elemExpr << "\n";
        initCode_ << "    mova bc\n    movxw\n";
        initCode_ << "    incdptr\n";
        initCode_ << "    mova bc+1\n    movxw\n";
        return;
    }
    uint16_t v16 = static_cast<uint16_t>(e.value);
    initCode_ << "    movdptr " << elemExpr << "\n";
    initCode_ << "    movai " << (v16 & 0xFF) << "\n    movxw\n";
    if (elemWidth == 1) return;
    initCode_ << "    incdptr\n";
    initCode_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movxw\n";
}

void MCS51Codegen::declareGlobalArray(const std::string &lbl, int arraySize, int elemWidth,
                                       const std::vector<ir::ArrayInitElem> &values) {
    allocXdata(lbl, arraySize * elemWidth);
    for (int i = 0; i < arraySize; i++) {
        // Element i's `movdptr` operand: `lbl` for the first element,
        // `lbl+<byte offset>` otherwise - the assembler folds the
        // offset into the symbol so the whole address stays one
        // relocatable expression.
        std::string elemExpr = (i == 0) ? lbl : lbl + "+" + std::to_string(i * elemWidth);
        static const ir::ArrayInitElem kZero{};
        const ir::ArrayInitElem &e = (static_cast<size_t>(i) < values.size()) ? values[i] : kZero;
        genArrayElementInit(elemExpr, elemWidth, e);
    }
}

void MCS51Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    movdd " << dst << ", " << src << "\n";
    code_ << "    movdd " << dst << "+1, " << src << "+1\n";
}

void MCS51Codegen::loadXdata(const std::string &dstReg, const std::string &xdataExpr) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    movxr\n    movd " << dstReg << "\n";
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::storeXdata(const std::string &xdataExpr, const std::string &srcReg) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    mova " << srcReg << "\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova " << srcReg << "+1\n    movxw\n";
}

void MCS51Codegen::loadXdataAddress(const std::string &dstReg, const std::string &xdataExpr) {
    code_ << "    movdptr " << xdataExpr << "\n";
    code_ << "    mova dpl\n    movd " << dstReg << "\n";
    code_ << "    mova dph\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::loadCodeAddress(const std::string &dstReg, const std::string &codeExpr) {
    code_ << "    movdptrc " << codeExpr << "\n";
    code_ << "    mova dpl\n    movd " << dstReg << "\n";
    code_ << "    mova dph\n    movd " << dstReg << "+1\n";
}

// Loads DPTR from `ptr` and writes `srcReg`'s low `width` bytes there
// one MOVX at a time, without disturbing `ptr`.
void MCS51Codegen::storeIndirect(const std::string &srcReg, int width) {
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    mova " << srcReg << "\n    movxw\n";
    if (width == 1) return;
    code_ << "    incdptr\n";
    code_ << "    mova " << srcReg << "+1\n    movxw\n";
}

void MCS51Codegen::loadIndirect(const std::string &dstReg, int width, bool isUnsigned) {
    code_ << "    movdpl ptr\n    movdph ptr+1\n";
    code_ << "    movxr\n    movd " << dstReg << "\n";
    if (width == 1) {
        if (isUnsigned) {
            code_ << "    movai 0\n    movd " << dstReg << "+1\n";
        } else {
            // The `anli`/Z-flag branch must come immediately after
            // `mova`, since any instruction between them could clobber PSW.Z.
            code_ << "    mova " << dstReg << "\n    anli 128\n";
            std::string Lneg = newLabel(), Lnonneg = newLabel(), Ldone = newLabel();
            safeBranch("jz", Lnonneg, Lneg);
            code_ << Lneg << ":\n    movai 255\n    movd " << dstReg << "+1\n    ljmp " << Ldone << "\n";
            code_ << Lnonneg << ":\n    movai 0\n    movd " << dstReg << "+1\n";
            code_ << Ldone << ":\n";
        }
        return;
    }
    code_ << "    incdptr\n";
    code_ << "    movxr\n    movd " << dstReg << "+1\n";
}

void MCS51Codegen::addImm16(const std::string &reg, int value) {
    uint16_t v16 = static_cast<uint16_t>(value);
    code_ << "    mova " << reg << "\n    addi " << (v16 & 0xFF) << "\n    movd " << reg << "\n";
    code_ << "    mova " << reg << "+1\n    addci " << ((v16 >> 8) & 0xFF) << "\n    movd " << reg << "+1\n";
}

void MCS51Codegen::subImm16(const std::string &reg, int value) {
    uint16_t v16 = static_cast<uint16_t>(value);
    code_ << "    mova " << reg << "\n    clrc\n    subbi " << (v16 & 0xFF) << "\n    movd " << reg << "\n";
    code_ << "    mova " << reg << "+1\n    subbi " << ((v16 >> 8) & 0xFF) << "\n    movd " << reg << "+1\n";
}

void MCS51Codegen::loadFrameAddress(const std::string &dstReg, const ir::Slot *slot) {
    copy16(dstReg, "fp");
    // A Param's position is a plain word index; a Local's
    // `frameOffset` is a byte offset (1-based from 2), placed right
    // after the params region.
    int byteOffset = (slot->kind == ir::SlotKind::Param) ? slot->paramIndex * 2
                                                           : (curParamCount_ * 2 + slot->frameOffset - 2);
    if (byteOffset != 0) addImm16(dstReg, byteOffset);
}

void MCS51Codegen::pushFrameWord(const std::string &srcReg) {
    code_ << "    movdpl ssp\n    movdph ssp+1\n";
    code_ << "    mova " << srcReg << "\n    movxw\n";
    code_ << "    incdptr\n";
    code_ << "    mova " << srcReg << "+1\n    movxw\n";
    addImm16("ssp", 2);
}

void MCS51Codegen::genPrologue(const ir::Function *fn) {
    code_ << "    push fp\n    push fp+1\n";
    copy16("fp", "ssp");
    int nparams = static_cast<int>(fn->params.size());
    if (nparams > 0) subImm16("fp", nparams * 2);
    if (fn->frameSize > 0) addImm16("ssp", fn->frameSize);
    curParamCount_ = nparams;
}

void MCS51Codegen::genEpilogue() {
    copy16("ssp", "fp");
    code_ << "    pop fp+1\n    pop fp\n";
}

void MCS51Codegen::loadArrayElemValue(const ir::ArrayInitElem &e) {
    if (e.isSymbol) {
        if (e.func) {
            loadCodeAddress("ac", label(e.func));
        } else {
            loadXdataAddress("ac", label(e.slot));
        }
        return;
    }
    uint16_t v16 = static_cast<uint16_t>(e.value);
    code_ << "    movai " << (v16 & 0xFF) << "\n    movd ac\n";
    code_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movd ac+1\n";
}

void MCS51Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int elemWidth = widthOfType(slot->type);
    loadFrameAddress("bc", slot);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        copy16("ptr", "bc");
        if (i != 0) addImm16("ptr", static_cast<int>(i) * elemWidth);
        loadArrayElemValue(slot->arrayInit[i]);
        storeIndirect("ac", elemWidth);
    }
}

void MCS51Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    ljmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfTaken << "\n";
}

void MCS51Codegen::safeDjnz(const std::string &counterReg, const std::string &targetIfNonzero,
                             const std::string &targetIfZero) {
    std::string Lnear = newLabel();
    code_ << "    djnz " << counterReg << ", " << Lnear << "\n";
    code_ << "    ljmp " << targetIfZero << "\n";
    code_ << Lnear << ":\n";
    code_ << "    ljmp " << targetIfNonzero << "\n";
}

std::string MCS51Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // mainFn is only null in object mode, for a file that doesn't
    // define `main` itself.

    // Pre-pass: every global's XDATA address is assigned up front,
    // before any code is generated, so a Var read has it regardless
    // of textual order. Locals/Params need no pre-pass: their storage
    // comes from the shared `__frame_stack` buffer at a
    // runtime-computed offset, never a per-slot XDATA label.
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; skipping it
        // keeps this file's XDATA layout from wasting space on it.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareGlobalArray(lbl, slot->arraySize, widthOfType(slot->type), slot->arrayInit);
        } else {
            declareGlobalTemp(lbl, slot->hasInit ? slot->initValue : 0);
        }
    }
    allocXdata(kFrameStackLabel, kFrameStackBytes);

    code_ << "org " << kMCS51BaseAddress << "\n";
    // Pseudo-register addresses: 0x30 sits above the 4 register banks
    // (0x00-0x1F) and the bit-addressable area (0x20-0x2F), below the
    // 128-byte floor every 8031/8032/8051/8052 guarantees. `dpl`/`dph`
    // are the real DPL/DPH SFRs (0x82/0x83), not invented cells.
    code_ << "ac = 0x30\nbc = 0x32\ncx = 0x34\ndx = 0x36\nptr = 0x38\nfp = 0x3A\nssp = 0x3C\n";
    code_ << "dpl = 0x82\ndph = 0x83\n\n";

    // No object file emits the entry sequence itself in object mode;
    // MCS51LinkerFactory.cpp synthesizes one once every object is
    // merged and `main`'s linked address is resolvable.
    if (!objectMode) {
        code_ << "_start:\n";
        // `ssp` must be valid before `main`'s genPrologue() reads it.
        loadXdataAddress("ssp", kFrameStackLabel);
        code_ << initCode_.str();
        code_ << "    lcall " << label(mainFn) << "\n";
        // The program's result sits in `ac`; this `mova` also leaves
        // the low byte in the real accumulator for a quick register-dump look.
        code_ << "    mova ac\n";
        code_ << "halt:\n    ljmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) {
        if (objectMode && fb.func == mainFn) {
            // This file's globals' init code has nowhere else to run
            // in object mode: the synthesized `_start` is built before
            // seeing any user object, so `fn_main`'s prologue - the
            // one point this file's code is guaranteed to run first -
            // takes over that role, `ssp` init included.
            code_ << label(fb.func) << ":\n";
            loadXdataAddress("ssp", kFrameStackLabel);
            genPrologue(fb.func);
            code_ << initCode_.str();
            genStmt(*fb.body);
            genEpilogue();
            code_ << "    ret\n\n";
        } else {
            genFunction(fb);
        }
    }

    return code_.str();
}

std::map<std::string, std::string> MCS51Codegen::perFileAsm() const { return perFileAsm_; }

void MCS51Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    genPrologue(fn.func);
    genStmt(*fn.body);
    genEpilogue();
    code_ << "    ret\n\n";
}

void MCS51Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    mova ac\n    orl ac+1\n";
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("jz", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    ljmp " << Ldone << "\n";
            if (s.elseStmt) {
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    mova ac\n    orl ac+1\n";
            safeBranch("jz", Lend, Lbody);
            code_ << Lbody << ":\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    ljmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    ljmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    ljmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit: genFunction()'s fall-through epilogue never
            // runs for this path, so the teardown is inlined here.
            genEpilogue();
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddress("ptr", s.declSlot);
                storeIndirect("ac", widthOfType(s.declSlot->type));
            }
            // No "else" case for a bare, uninitialized scalar: its
            // frame word already exists (reserved by genPrologue()'s
            // `ssp` bump), holding whatever garbage was there until
            // something assigns it.
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
            code_ << "    ljmp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void MCS51Codegen::requireNarrow(const ir::Expr &e) const {
    if (widthOfType(e.type) > 2) {
        err(e.line, "'long'/'long long' arithmetic isn't supported on this target");
    }
}

void MCS51Codegen::genExpr(const ir::Expr &e) {
    requireNarrow(e);
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    movai " << (v16 & 0xFF) << "\n    movd ac\n";
            code_ << "    movai " << ((v16 >> 8) & 0xFF) << "\n    movd ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            // A Global's value sits at a compile-time label; a
            // Local/Param's sits at an `fp`-relative frame address.
            if (e.slot->kind == ir::SlotKind::Global) {
                loadXdata("ac", label(e.slot));
            } else {
                loadFrameAddress("ptr", e.slot);
                loadIndirect("ac", widthOfType(e.slot->type), e.slot->type.isUnsigned);
            }
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target XDATA address
                // Save `ptr` across evaluating rhs (which might itself
                // reuse `ptr`) via real PUSH/POP direct.
                code_ << "    push ptr\n    push ptr+1\n";
                genExpr(*e.rhs);
                code_ << "    pop ptr+1\n    pop ptr\n";
                storeIndirect("ac", widthOfType(e.slot->type));
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                genExpr(*e.rhs);
                storeXdata(label(e.slot), "ac");
                return;
            }
            loadFrameAddress("ptr", e.slot);
            code_ << "    push ptr\n    push ptr+1\n";
            genExpr(*e.rhs);
            code_ << "    pop ptr+1\n    pop ptr\n";
            storeIndirect("ac", widthOfType(e.slot->type));
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    push ptr\n    push ptr+1\n";
            genExpr(*e.rhs);
            code_ << "    pop ptr+1\n    pop ptr\n";
            storeIndirect("ac", widthOfType(e.type));
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = target XDATA address
                genCompoundThroughPtr(e);
                return;
            }
            if (e.slot->kind == ir::SlotKind::Global) {
                genExpr(*e.rhs);    // ac = rhs
                copy16("dx", "ac");   // dx = rhs, held (nothing else evaluates a genExpr() before it's consumed)
                loadXdata("ac", label(e.slot));  // ac = old
                copy16("bc", "ac");                 // bc = old - genCompoundOp()'s own precondition
                if (e.yieldOld) code_ << "    push ac\n    push ac+1\n";
                copy16("ac", "dx");  // ac = rhs - genCompoundOp()'s own other precondition
                genCompoundOp(e.op, e.type);
                narrowAcTo(e.type);
                storeXdata(label(e.slot), "ac");
                if (e.yieldOld) code_ << "    pop ac+1\n    pop ac\n";
                return;
            }
            loadFrameAddress("ptr", e.slot);
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            code_ << "    mova ac\n    orl ac+1\n";
            std::string Lthen = newLabel();
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            safeBranch("jz", Lelse, Lthen);
            code_ << Lthen << ":\n";
            genExpr(*e.rhs);
            code_ << "    ljmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    movai 0\n    clrc\n    subb ac\n    movd ac\n";
                code_ << "    movai 0\n    subb ac+1\n    movd ac+1\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                loadIndirect("ac", widthOfType(e.type), e.type.isUnsigned);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mova ac\n    xrli 255\n    movd ac\n";
                code_ << "    mova ac+1\n    xrli 255\n    movd ac+1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Each argument is computed and pushed onto the shared
            // frame stack positionally; the callee's genPrologue()
            // picks these words back up as its `fp`-relative Params.
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                pushFrameWord("ac");
            }
            code_ << "    lcall " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // The real 8051 has no indirect CALL, only `JMP @A+DPTR`
            // (an indirect jump). A call through a computed address is
            // faked by manually pushing a return address (low byte
            // first, matching real LCALL's push order) before jumping,
            // so the callee's `ret` returns here as if LCALL had been used.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string ret = newLabel();
            loadCodeAddress("bc", ret);  // bc = this call's own return address
            code_ << "    push bc\n    push bc+1\n";
            code_ << "    movdpl ac\n    movdph ac+1\n";  // dptr = callee address
            code_ << "    clra\n";                        // a = 0, so @A+DPTR == DPTR exactly
            code_ << "    jmpadptr\n";
            code_ << ret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // `SETB EA` (IE.7) enables all interrupts, `CLR EA` disables them.
            code_ << "    " << (e.op == "ion" ? "setea" : "clrea") << "\n";
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// Leaves slot[idxExpr]'s effective XDATA address in `ptr`. If
// `slot->arraySize > 0`, the base is the array's own address
// (loadXdataAddress() for a Global, loadFrameAddress() for a Local);
// otherwise `slot` is a pointer variable, so the base is read from its
// stored value instead. Either way the base ends up in `bc`, and a
// 16-bit add folds the scaled index into it.
void MCS51Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    int elemWidth = widthOfType(slot->type);
    genExpr(idxExpr);  // ac = index
    if (elemWidth != 1) {
        code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";  // ac *= 2, since each element is 2 bytes
        code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    }
    bool isGlobal = slot->kind == ir::SlotKind::Global;
    if (slot->arraySize > 0) {
        if (isGlobal) {
            loadXdataAddress("bc", label(slot));
        } else {
            loadFrameAddress("bc", slot);
        }
    } else if (isGlobal) {
        loadXdata("bc", label(slot));
    } else {
        loadFrameAddress("ptr", slot);
        loadIndirect("bc", 2, /*isUnsigned=*/true);
    }
    code_ << "    clrc\n    mova bc\n    add ac\n    movd ptr\n";
    code_ << "    mova bc+1\n    addc ac+1\n    movd ptr+1\n";
}

void MCS51Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadIndirect("ac", widthOfType(e.slot->type), e.slot->type.isUnsigned);
}

void MCS51Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            loadXdataAddress("ac", label(operand.slot));
        } else {
            loadFrameAddress("ac", operand.slot);
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadCodeAddress("ac", label(operand.func));
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MCS51Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    mova bc\n    add ac\n    movd ac\n";
            code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
        } else {
            code_ << "    mova bc\n    clrc\n    subb ac\n    movd ac\n";
            code_ << "    mova bc+1\n    subb ac+1\n    movd ac+1\n";
        }
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        return;
    }
    if (op == "/") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/false, e.lhs->type.isUnsigned);
        return;
    }
    if (op == "%") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/true, e.lhs->type.isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        genExpr(*e.lhs);
        code_ << "    push ac\n    push ac+1\n";
        genExpr(*e.rhs);
        code_ << "    pop bc+1\n    pop bc\n";
        const char *insn = (op == "&") ? "anl" : (op == "|") ? "orl" : "xrl";
        code_ << "    mova bc\n    " << insn << " ac\n    movd ac\n";
        code_ << "    mova bc+1\n    " << insn << " ac+1\n    movd ac+1\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(*e.lhs, *e.rhs, op == "<<", e.lhs->type.isUnsigned);
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
    genCompare(op, *e.lhs, *e.rhs, e.lhs->type.isUnsigned);
}

// Loops `rhs` times via DJNZ, guarded by an upfront zero check: DJNZ
// decrements before testing, so shift-by-zero must be caught before
// entering the loop body, not after.
void MCS51Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    mova ac\n    movd cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pop bc+1\n    pop bc\n";
    copy16("ac", "bc");  // ac = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    mova cx\n";
    safeBranch("jz", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
        code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    } else if (isUnsigned) {
        // Carry is cleared first, so RRC's top-bit fill is always 0.
        code_ << "    clrc\n    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    } else {
        // The sign bit is carried into the carry flag first (via a
        // left rotate), so the RRC that follows fills the vacated top
        // bit with the operand's sign instead of 0.
        code_ << "    mova ac+1\n    rlca\n";
        code_ << "    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    }
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// `ac` is always a plain 16-bit word, so a same-width conversion is a
// pure reinterpretation - nothing to do. Narrowing to `char` masks off
// the high byte and, for a signed target, sign-extends bit 7 across
// it, so a `char` value always arrives already correctly extended
// (making a later widen-from-`char` a pure pass-through).
void MCS51Codegen::genConvert(const ir::Expr &e) {
    genExpr(*e.lhs);
    if (widthOfType(e.type) != 1) return;
    if (e.type.isUnsigned) {
        code_ << "    movai 0\n    movd ac+1\n";
    } else {
        // The branch on `anli`'s Z flag must come immediately after it.
        code_ << "    mova ac\n    anli 128\n";
        std::string Lneg = newLabel(), Lnonneg = newLabel(), Ldone = newLabel();
        safeBranch("jz", Lnonneg, Lneg);
        code_ << Lneg << ":\n    movai 255\n    movd ac+1\n    ljmp " << Ldone << "\n";
        code_ << Lnonneg << ":\n    movai 0\n    movd ac+1\n";
        code_ << Ldone << ":\n";
    }
}

// Shift-and-add multiply (real `MUL AB` is only 8x8->16, too narrow
// for 16-bit `int`): `bc` is the multiplicand (shifted left), `dx` the
// multiplier (shifted right, consumed one bit at a time), `ac` the
// product, `cx` a fixed 16-iteration counter. Non-negative operands only.
void MCS51Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop bc+1\n    pop bc\n";                  // bc = multiplicand
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = product = 0
    code_ << "    movai 16\n    movd cx\n";
    std::string Lloop = newLabel(), Ladd = newLabel(), Lskipadd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mova dx\n    anli 1\n";
    safeBranch("jnz", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    mova bc\n    add ac\n    movd ac\n";
    code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    clrc\n    mova bc\n    rlca\n    movd bc\n";
    code_ << "    mova bc+1\n    rlca\n    movd bc+1\n";
    code_ << "    clrc\n    mova dx+1\n    rrca\n    movd dx+1\n";
    code_ << "    mova dx\n    rrca\n    movd dx\n";
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// Repeated subtraction (real `DIV AB` is only 8/8->8r8, same width
// problem as `MUL AB`): `dx` is the running remainder (starts as the
// dividend), `bc` the divisor, `ac` the quotient count. A signed
// operand is divided by absolute value, with `ptr`'s low/high byte
// holding the dividend's/divisor's original sign so the quotient's
// sign (1 iff the signs differed) and remainder's sign (the
// dividend's) can be restored afterward.
void MCS51Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop dx+1\n    pop dx\n";                  // dx = dividend (running remainder)
    auto isNeg = [&](const char *cell, const char *dst) {
        code_ << "    mova " << cell << "+1\n    anli 128\n";
        std::string Lneg = newLabel(), Lnonneg = newLabel(), Ldone = newLabel();
        safeBranch("jz", Lnonneg, Lneg);
        code_ << Lneg << ":\n    movai 1\n    movd " << dst << "\n    ljmp " << Ldone << "\n";
        code_ << Lnonneg << ":\n    movai 0\n    movd " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negate16 = [&](const char *cell) {
        code_ << "    movai 0\n    clrc\n    subb " << cell << "\n    movd " << cell << "\n";
        code_ << "    movai 0\n    subb " << cell << "+1\n    movd " << cell << "+1\n";
    };
    auto negateIf = [&](const char *flagCell, const char *cell) {
        code_ << "    mova " << flagCell << "\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("jz", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16(cell);
        code_ << Lskip << ":\n";
    };
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "bc");
    }
    code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    clrc\n    mova dx\n    subb bc\n    movd cx\n";
    code_ << "    mova dx+1\n    subb bc+1\n";
    safeBranch("jc", Llt, Lcommit);
    code_ << Lcommit << ":\n";
    code_ << "    movd dx+1\n    movdd dx, cx\n";
    code_ << "    mova ac\n    addi 1\n    movd ac\n";
    code_ << "    mova ac+1\n    addci 0\n    movd ac+1\n";
    code_ << "    ljmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    ljmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed.
        code_ << "    mova ptr\n    xrl ptr+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("jz", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

// Precondition: `bc` holds `old`, `ac` holds `rhs`. Leaves the raw new
// value in `ac`.
void MCS51Codegen::genCompoundOp(const std::string &op, Type type) {
    if (op == "+") {
        code_ << "    mova bc\n    add ac\n    movd ac\n";
        code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
        return;
    }
    if (op == "-") {
        code_ << "    mova bc\n    clrc\n    subb ac\n    movd ac\n";
        code_ << "    mova bc+1\n    subb ac+1\n    movd ac+1\n";
        return;
    }
    if (op == "*") {
        copy16("dx", "ac");  // dx = multiplier (rhs) - bc already holds the multiplicand (old)
        code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = product = 0
        code_ << "    movai 16\n    movd cx\n";
        std::string Lloop = newLabel(), Ladd = newLabel(), Lskipadd = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    mova dx\n    anli 1\n";
        safeBranch("jnz", Ladd, Lskipadd);
        code_ << Ladd << ":\n";
        code_ << "    mova bc\n    add ac\n    movd ac\n";
        code_ << "    mova bc+1\n    addc ac+1\n    movd ac+1\n";
        code_ << Lskipadd << ":\n";
        code_ << "    clrc\n    mova bc\n    rlca\n    movd bc\n";
        code_ << "    mova bc+1\n    rlca\n    movd bc+1\n";
        code_ << "    clrc\n    mova dx+1\n    rrca\n    movd dx+1\n";
        code_ << "    mova dx\n    rrca\n    movd dx\n";
        safeDjnz("cx", Lloop, Ldone);
        code_ << Ldone << ":\n";
        return;
    }
    if (op == "/" || op == "%") {
        copy16("dx", "bc");  // dx = dividend (old) - copied before bc is overwritten below
        copy16("bc", "ac");   // bc = divisor (rhs)
        code_ << "    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";  // ac = quotient = 0
        std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
        code_ << Lloop << ":\n";
        code_ << "    clrc\n    mova dx\n    subb bc\n    movd cx\n";
        code_ << "    mova dx+1\n    subb bc+1\n";
        safeBranch("jc", Llt, Lcommit);
        code_ << Lcommit << ":\n";
        code_ << "    movd dx+1\n    movdd dx, cx\n";
        code_ << "    mova ac\n    addi 1\n    movd ac\n";
        code_ << "    mova ac+1\n    addci 0\n    movd ac+1\n";
        code_ << "    ljmp " << Lloop << "\n";
        code_ << Llt << ":\n";
        code_ << "    ljmp " << Ldone << "\n";
        code_ << Ldone << ":\n";
        if (op == "%") copy16("ac", "dx");
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "anl" : (op == "|") ? "orl" : "xrl";
        code_ << "    mova bc\n    " << insn << " ac\n    movd ac\n";
        code_ << "    mova bc+1\n    " << insn << " ac+1\n    movd ac+1\n";
        return;
    }
    // "<<" or ">>"
    code_ << "    mova ac\n    movd cx\n";  // cx = shift count (rhs's own low byte) - ac about to be overwritten
    copy16("ac", "bc");                       // ac = value to shift (old)
    bool isLeft = op == "<<";
    bool isUnsigned = type.isUnsigned;
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    mova cx\n";
    safeBranch("jz", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    clrc\n    mova ac\n    rlca\n    movd ac\n";
        code_ << "    mova ac+1\n    rlca\n    movd ac+1\n";
    } else if (isUnsigned) {
        code_ << "    clrc\n    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    } else {
        code_ << "    mova ac+1\n    rlca\n";
        code_ << "    mova ac+1\n    rrca\n    movd ac+1\n";
        code_ << "    mova ac\n    rrca\n    movd ac\n";
    }
    safeDjnz("cx", Lloop, Ldone);
    code_ << Ldone << ":\n";
}

// `ptr` holds the resolved address on entry; stashed on the real
// hardware stack while `rhs` is evaluated, since evaluating it might
// itself reuse `ptr`. `yieldOld`'s `old` value survives on that same
// stack across genCompoundOp()'s own register use.
void MCS51Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    code_ << "    push ptr\n    push ptr+1\n";  // stash address
    genExpr(*e.rhs);                              // ac = rhs
    copy16("dx", "ac");                             // dx = rhs, held safely until genCompoundOp() consumes it
    code_ << "    pop ptr+1\n    pop ptr\n";          // ptr = address, restored only now
    loadIndirect("ac", widthOfType(e.type), e.type.isUnsigned);  // ac = old
    copy16("bc", "ac");                                            // bc = old - genCompoundOp()'s own precondition
    if (e.yieldOld) code_ << "    push ac\n    push ac+1\n";  // old, surviving genCompoundOp()'s own register use
    copy16("ac", "dx");                                         // ac = rhs - genCompoundOp()'s own other precondition
    genCompoundOp(e.op, e.type);                                  // ac = new (raw)
    narrowAcTo(e.type);
    storeIndirect("ac", widthOfType(e.type));  // stores ac (= new) - ptr untouched since the pop above
    if (e.yieldOld) code_ << "    pop ac+1\n    pop ac\n";  // ac = old (final result)
}

// Same truncate-then-sign/zero-extend step as genConvert()'s
// int-to-char case; the only width this ever narrows to.
void MCS51Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;
    if (to.isUnsigned) {
        code_ << "    movai 0\n    movd ac+1\n";
        return;
    }
    code_ << "    mova ac\n    anli 128\n";
    std::string Lneg = newLabel(), Lnonneg = newLabel(), Ldone = newLabel();
    safeBranch("jz", Lnonneg, Lneg);
    code_ << Lneg << ":\n    movai 255\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lnonneg << ":\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to "is a < b?" by choosing
    // operand order and whether to invert the sense.
    if (op == "<") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// a-b (16-bit, borrow chained low into high) is zero iff a==b; the
// final ORL leaves "low result OR high result" in A, zero iff both
// halves were zero (JZ/JNZ test A directly, with no separate zero flag).
void MCS51Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(rhs);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = lhs
    code_ << "    mova bc\n    clrc\n    subb ac\n    movd dx\n";
    code_ << "    mova bc+1\n    subb ac+1\n    orl dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "jz" : "jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

// "a < b" via subtraction. Unsigned: the carry after the chained SUBB
// directly says borrow (a<b) or no borrow (a>=b). Signed uses the "N
// xor V" idiom: ACC.7 after the final SUBB only reflects the true
// signed result once corrected for the real PSW.OV overflow flag, via
// an XRL A,#0x80 applied iff OV is set.
void MCS51Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert) {
    genExpr(a);
    code_ << "    push ac\n    push ac+1\n";
    genExpr(b);
    code_ << "    pop bc+1\n    pop bc\n";  // bc = a
    code_ << "    mova bc\n    clrc\n    subb ac\n";
    code_ << "    mova bc+1\n    subb ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        safeBranch(invert ? "jnc" : "jc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    jnbov " << Lfixskip << "\n";
        code_ << "    xrli 128\n";
        code_ << Lfixskip << ":\n";
        // ACC.7 now correctly reflects "a < b" (set) / "a >= b" (clear).
        safeBranch(invert ? "jnbacc7" : "jbacc7", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jz", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    safeBranch("jnz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

void MCS51Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    mova ac\n    orl ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("jz", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    movai 1\n    movd ac\n    movai 0\n    movd ac+1\n    ljmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    movai 0\n    movd ac\n    movai 0\n    movd ac+1\n";
    code_ << Ldone << ":\n";
}

std::string MCS51Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 8051 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> MCS51Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleMCS51(asmText);
    // Builds one format-neutral ObjectFile, then picks the requested
    // format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMCS51Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
