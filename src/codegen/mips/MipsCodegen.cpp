#include "MipsCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "MipsAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program loads; the test harness sets the simulated
// CPU's PC here directly.
constexpr uint32_t kBaseAddress = 0x80010000u;

// Initial $sp, comfortably above the loaded program and its data.
constexpr uint32_t kInitialSP = 0x80100000u;
}  // namespace

// A literal element's decimal text, or a symbol element's referenced
// Slot's/Function's label written out bare.
std::string MipsCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void MipsCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive; a `long long` element is two "dw"s
    // (low word, then high) instead.
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

void MipsCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    lui " << reg << "," << lbl << "\n";
    code_ << "    ori " << reg << "," << reg << "," << lbl << "\n";
}

void MipsCodegen::loadImm32(const std::string &reg, int32_t v) {
    if (v >= -32768 && v <= 32767) {
        code_ << "    addiu " << reg << ",zero," << v << "\n";
    } else {
        uint32_t uv = static_cast<uint32_t>(v);
        code_ << "    lui " << reg << "," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
        code_ << "    ori " << reg << "," << reg << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
    }
}

// A Param lives at fp+frameOffset (caller's pushed argument space); a
// Local's address is its lowest one (see ir::localLowOffset()), its later
// bytes above it. No bracketed memory operand exists
// in this dialect, so the address is computed with a single ADDIU.
void MipsCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
        return;
    }
    int offset = slot->kind == ir::SlotKind::Param ? slot->frameOffset : -ir::localLowOffset(*slot, typeWidths());
    code_ << "    addiu " << reg << ",fp," << offset << "\n";
}

std::string MipsCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In objectMode, `prog` may not define `main`; the entry sequence
    // below is skipped since MipsLinkerFactory.cpp synthesizes it once
    // every object is linked. An object's base is `org 0`; the linker
    // places it.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    lui sp," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    ori sp,sp," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    jal " << label(mainFn) << "\n    nop\n";
        // main's result sits in $t0 once execution reaches halt.
        code_ << "halt:\n    beq zero,zero,halt\n    nop\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // MIPS raises an address-error exception on a misaligned
    // halfword/word access, so each dh/dw global needs padding bytes
    // first to reach its own natural alignment.
    size_t globalByteOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file; skip reserving
        // storage so references stay as undefined symbols/relocations.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        int elemW = widthOfType(slot->type);
        size_t totalBytes = slot->arraySize > 0 ? static_cast<size_t>(slot->arraySize) * elemW
                                                  : static_cast<size_t>(elemW);
        size_t align = elemW >= 4 ? 4 : static_cast<size_t>(elemW);
        while (align > 1 && (globalByteOffset % align) != 0) {
            trailer_.push_back("db 0  ; alignment padding");
            globalByteOffset += 1;
        }
        globalByteOffset += totalBytes;
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

std::map<std::string, std::string> MipsCodegen::perFileAsm() const { return perFileAsm_; }

// Every function saves/restores $ra, leaf or not: MIPS has exactly
// one link register, so a non-leaf function's own call would
// otherwise overwrite it before `jr ra` returns. The prologue also
// pushes the caller's $fp and points $fp at that saved word, giving
// this call (including recursive ones) its own frame; locals are then
// reserved by lowering $sp another frameSize words.
void MipsCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    addiu sp,sp,-4\n    stind sp,ra\n";
    code_ << "    addiu sp,sp,-4\n    stind sp,fp\n";
    code_ << "    addu fp,sp,zero\n";
    if (fn.func->frameSize > 0) code_ << "    addiu sp,sp,-" << fn.func->frameSize << "\n";

    genStmt(*fn.body);

    genEpilogue();
    code_ << "\n";
}

// Deallocates locals ($sp back up to $fp), restores the caller's
// $fp/$ra from the two words $fp points at, then returns. $at briefly
// holds the saved $fp across restoring $ra.
void MipsCodegen::genEpilogue() {
    code_ << "    addu sp,fp,zero\n";
    code_ << "    ldind sp,at\n    addiu sp,sp,4\n";
    code_ << "    ldind sp,ra\n    addiu sp,sp,4\n";
    code_ << "    addu fp,at,zero\n";
    code_ << "    jr ra\n    nop\n";
}

void MipsCodegen::genStmt(const ir::Stmt &s) {
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
            code_ << "    beq t0,zero," << (s.elseStmt ? Lelse : Ldone) << "\n    nop\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    j " << Ldone << "\n    nop\n";
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
            code_ << "    beq t0,zero," << Lend << "\n    nop\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    j " << Lstart << "\n    nop\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    j " << loopLabels_.back().second << "\n    nop\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    j " << loopLabels_.back().first << "\n    nop\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit, bypassing the fall-through epilogue; genEpilogue()
            // doesn't touch $t0, where the return value already sits.
            genEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadSlotAddr("t2", s.declSlot);
                storeOperand("t2", s.declSlot->type);
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
            code_ << "    j " << irLabel(s.labelId) << "\n    nop\n";
            return;
        }
    }
}

void MipsCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
                loadImm32("t1", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
                return;
            }
            loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("t2", e.slot);
            loadOperand("t2", e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // t2 = target address
                code_ << "    addiu sp,sp,-4\n    stind sp,t2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,t2\n    addiu sp,sp,4\n";
                storeOperand("t2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("t2", e.slot);
            storeOperand("t2", e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // t2 = target address
                genCompoundThroughT2(e);
                return;
            }
            // MIPS has no absolute-addressing memory operand, so even a
            // scalar target's address is computed first.
            loadSlotAddr("t2", e.slot);
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    addu t2,t0,zero\n";  // t2 = address
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            // The condition's type can still be wide even though the
            // ternary's result type isn't, hence the OR-then-test.
            if (isWide(e.lhs->type)) code_ << "    or t0,t0,t1\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq t0,zero," << Lelse << "\n    nop\n";
            genExpr(*e.rhs);
            code_ << "    j " << Ldone << "\n    nop\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness (the pointee type, or
            // native-int-width for untyped-int-as-address).
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
            genExpr(*e.rhs);  // t0, or t1:t0 if wide
            code_ << "    ldind sp,t2\n    addiu sp,sp,4\n";
            storeOperand("t2", e.type);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    subu t0,zero,t0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width/signedness.
                genExpr(*e.rhs);  // t0 = pointer value
                code_ << "    addu t2,t0,zero\n";
                loadOperand("t2", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    nor t0,t0,zero\n";
                if (isWide(e.rhs->type)) code_ << "    nor t1,t1,zero\n";
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
            // cdecl-style: push args right-to-left onto $sp (arg 0
            // lands at fp+8+4*i once the callee's prologue runs), caller
            // cleans up after the call returns. A wide argument pushes
            // high word first so its low word ends up at the lower
            // address.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    addiu sp,sp,-8\n    stind sp,t1\n    stind sp,t0,4\n";
                    totalBytes += 8;
                } else {
                    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
                    totalBytes += 4;
                }
            }
            code_ << "    jal " << label(e.func) << "\n    nop\n";
            if (totalBytes > 0) code_ << "    addiu sp,sp," << totalBytes << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // jalr ra,t2 jumps to the address in t2 and leaves the
            // return address in ra, like jal for a runtime-known target.
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    addu t2,t0,zero\n";
            code_ << "    jalr ra,t2\n    nop\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No exception/interrupt setup on this bare-metal target; no-op.
            (void)e;
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

// t2 ends up holding the computed address. arraySize > 0 means a
// compile-time-known array base (Global label or Local fp-relative);
// otherwise `slot` is a pointer variable, so the base is read from its
// stored value first.
//
// Both a Global and a Local array's element addresses increase with
// the index (base+4*i).
void MipsCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    sll t0,t0," << shift << "\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("t2", label(slot));
            code_ << "    addu t2,t2,t0\n";
        } else {
            loadSlotAddr("t2", slot);
            code_ << "    addu t2,t2,t0\n";
        }
    } else {
        loadSlotAddr("t1", slot);
        code_ << "    ldind t1,t1\n";  // t1 = the pointer variable's own stored value
        code_ << "    addu t2,t1,t0\n";
    }
}

void MipsCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("t2", e.slot->type);
}

// Stores a Local array's compile-time initializer values into its
// frame slots, re-run on every call since the storage is freshly
// reserved each time. Element i lives w*i bytes above element 0.
void MipsCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int offset = -ir::localLowOffset(*slot, typeWidths()) + w * static_cast<int>(i);
        if (w == 8 && !elem.isSymbol) {
            // A long long element is split across both words.
            auto v = static_cast<uint64_t>(elem.value);
            loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(v)));
            code_ << "    addiu t2,fp," << offset << "\n    stind t2,t0\n";
            loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(v >> 32)));
            code_ << "    addiu t2,fp," << (offset + 4) << "\n    stind t2,t0\n";
            continue;
        }
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
        } else {
            loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        code_ << "    addiu t2,fp," << offset << "\n";
        if (w == 1) {
            code_ << "    stindb t2,t0\n";
        } else if (w == 2) {
            code_ << "    stindh t2,t0\n";
        } else {
            code_ << "    stind t2,t0\n";
        }
    }
}

void MipsCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("t2", operand.slot);
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
        code_ << "    addu t0,t2,zero\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void MipsCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindbu" : "ldindb") << " " << addrReg << ",t0\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindhu" : "ldindh") << " " << addrReg << ",t0\n";
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",t0\n";
    } else {  // 8 - long long: t1:t0 (hi:lo), low word first
        code_ << "    ldind " << addrReg << ",t0\n";
        code_ << "    ldind " << addrReg << ",t1,4\n";
    }
}

void MipsCodegen::storeOperand(const std::string &addrReg, Type type) {
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

void MipsCodegen::narrowT0To(Type to) {
    int w = widthOfType(to);
    if (w == 1) {
        code_ << "    sll t0,t0,24\n";
        code_ << "    " << (to.isUnsigned ? "srl" : "sra") << " t0,t0,24\n";
    } else if (w == 2) {
        code_ << "    sll t0,t0,16\n";
        code_ << "    " << (to.isUnsigned ? "srl" : "sra") << " t0,t0,16\n";
    }
    // 4 bytes (Int/Long): t0 already holds the full value.
}

// Crosses the native-int/`long long` boundary (sign/zero-extending t0
// into t1, or dropping t1), or re-truncates t0 for a narrower target.
void MipsCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);  // t0, or t1:t0 if fromWide
    if (fromWide && !toWide) {
        narrowT0To(to);  // t1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    addu t1,zero,zero\n";
        } else {
            code_ << "    sra t1,t0,31\n";  // sign-extend t0 into t1
        }
    } else if (!fromWide && !toWide) {
        narrowT0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct.
}

// `long long` add/sub: stash lhs on the stack while rhs evaluates,
// then ripple carry/borrow across the word pair. MIPS has no carry
// flag, so the carry out of the low-word add is recovered via sltu.
void MipsCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);  // t1:t0 = lhs
    code_ << "    addiu sp,sp,-8\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t1:t0 = rhs
    code_ << "    addu t3,t0,zero\n    addu t4,t1,zero\n";  // t3:t4 = rhs (lo:hi)
    code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    addiu sp,sp,8\n";  // t1:t0 = lhs
    if (isAdd) {
        code_ << "    addu t5,t0,t3\n";          // t5 = lo sum
        code_ << "    sltu t6,t5,t0\n";           // t6 = carry out
        code_ << "    addu t1,t1,t4\n    addu t1,t1,t6\n";
        code_ << "    addu t0,t5,zero\n";
    } else {
        code_ << "    sltu t6,t0,t3\n";           // t6 = borrow
        code_ << "    subu t5,t0,t3\n";
        code_ << "    subu t1,t1,t4\n    subu t1,t1,t6\n";
        code_ << "    addu t0,t5,zero\n";
    }
}

// `long long` multiply, keeping the low 64 bits of the 128-bit
// product via the standard 32x32 cross-multiply decomposition; each
// cross term's own MFHI overflow only affects bit 64+ and is discarded.
void MipsCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);  // t1:t0 = lhs
    code_ << "    addiu sp,sp,-8\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t1:t0 = rhs
    code_ << "    addu t5,t0,zero\n    addu t6,t1,zero\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldind sp,t4,4\n    addiu sp,sp,8\n";  // t3:t4 = lhs (lo:hi)
    code_ << "    mult t3,t6\n    mflo t0\n";  // lhs_lo*rhs_hi
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    code_ << "    mult t4,t5\n    mflo t0\n";  // lhs_hi*rhs_lo
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    code_ << "    mult t3,t5\n    mflo t0\n    mfhi t1\n";  // lhs_lo*rhs_lo (final result_lo/carry)
    code_ << "    ldind sp,t6\n    addiu sp,sp,4\n";   // crossB_lo (lhs_hi*rhs_lo)
    code_ << "    ldind sp,t5\n    addiu sp,sp,4\n";   // crossA_lo (lhs_lo*rhs_hi)
    code_ << "    addu t1,t1,t6\n    addu t1,t1,t5\n";
}

void MipsCodegen::genNegate64() {
    code_ << "    nor t0,t0,zero\n    nor t1,t1,zero\n";
    code_ << "    addiu t0,t0,1\n";
    code_ << "    sltiu t3,t0,1\n";  // t3 = 1 if t0 wrapped to 0 (i.e. the +1 carried out)
    code_ << "    addu t1,t1,t3\n";
}

// Bit-serial shift/compare/subtract long division, using $sp-relative
// scratch (ldind/stind already take a displacement). Signed division
// operates on magnitudes: each negative operand is negated in place
// first, then the sign is restored on the result per C's truncating rule.
void MipsCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-8\n    stind sp,t1\n    stind sp,t0,4\n";  // [sp]=lhs_hi,[sp+4]=lhs_lo
    genExpr(rhs);
    code_ << "    addiu sp,sp,-8\n    stind sp,t1\n    stind sp,t0,4\n";  // [sp]=rhs_hi,[sp+4]=rhs_lo
    code_ << "    addiu sp,sp,-36\n";
    // Scratch layout at sp+0: n_lo,n_hi,d_lo,d_hi,r_lo,r_hi,q_lo,q_hi,cnt (9 words, 36 bytes)
    code_ << "    ldind sp,t0,44\n    stind sp,t0,0\n";   // n_lo = lhs_lo
    code_ << "    ldind sp,t0,40\n    stind sp,t0,4\n";   // n_hi = lhs_hi
    code_ << "    ldind sp,t0,36\n    stind sp,t0,8\n";   // d_lo = rhs_lo
    code_ << "    ldind sp,t0,32\n    stind sp,t0,12\n";  // d_hi = rhs_hi
    code_ << "    stind sp,zero,16\n    stind sp,zero,20\n";  // r = 0
    code_ << "    stind sp,zero,24\n    stind sp,zero,28\n";  // q = 0
    code_ << "    addiu t0,zero,32\n    stind sp,t0,32\n";    // cnt = 32 (per-word, so 2*32=64 total)

    if (!isUnsigned) {
        code_ << "    ldind sp,t0,4\n    srl t0,t0,31\n    stind sp,t0,40\n";    // [sp+40] = lhsSign
        code_ << "    ldind sp,t0,12\n    srl t0,t0,31\n    stind sp,t0,44\n";   // [sp+44] = rhsSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    ldind sp,t0,40\n    beq t0,zero," << LskipNegN << "\n    nop\n";
        code_ << "    ldind sp,t0,0\n    ldind sp,t1,4\n";
        genNegate64();
        code_ << "    stind sp,t0,0\n    stind sp,t1,4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    ldind sp,t0,44\n    beq t0,zero," << LskipNegD << "\n    nop\n";
        code_ << "    ldind sp,t0,8\n    ldind sp,t1,12\n";
        genNegate64();
        code_ << "    stind sp,t0,8\n    stind sp,t1,12\n";
        code_ << LskipNegD << ":\n";
    }

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), LshiftQ = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldind sp,t0,32\n    beq t0,zero," << Ldone << "\n    nop\n";
    code_ << "    addiu t0,t0,-1\n    stind sp,t0,32\n";

    // n <<= 1 (0-filled), capturing its own old top bit into t3.
    code_ << "    ldind sp,t0,4\n    srl t0,t0,31\n    addu t3,t0,zero\n";
    code_ << "    ldind sp,t0,0\n    srl t0,t0,31\n    addu t4,t0,zero\n";
    code_ << "    ldind sp,t0,0\n    sll t0,t0,1\n    stind sp,t0,0\n";
    code_ << "    ldind sp,t0,4\n    sll t0,t0,1\n    or t0,t0,t4\n    stind sp,t0,4\n";

    // r = (r << 1) | (n's old top bit, in t3).
    code_ << "    ldind sp,t0,16\n    srl t0,t0,31\n    addu t5,t0,zero\n";
    code_ << "    ldind sp,t0,16\n    sll t0,t0,1\n    or t0,t0,t3\n    stind sp,t0,16\n";
    code_ << "    ldind sp,t0,20\n    sll t0,t0,1\n    or t0,t0,t5\n    stind sp,t0,20\n";

    // if (r >= d) { r -= d; quotient bit = 1 } else { quotient bit = 0 }
    code_ << "    ldind sp,t0,20\n    ldind sp,t1,12\n    bne t0,t1," << LhiDiff << "\n    nop\n";
    code_ << "    ldind sp,t0,16\n    ldind sp,t1,8\n    sltu t3,t0,t1\n";
    code_ << "    beq t3,zero," << LsubYes << "\n    nop\n";
    code_ << "    j " << LsubNo << "\n    nop\n";
    code_ << LhiDiff << ":\n";
    code_ << "    sltu t3,t0,t1\n    beq t3,zero," << LsubYes << "\n    nop\n";
    code_ << "    j " << LsubNo << "\n    nop\n";
    code_ << LsubYes << ":\n";
    code_ << "    ldind sp,t0,16\n    ldind sp,t1,8\n    sltu t6,t0,t1\n    subu t0,t0,t1\n    stind sp,t0,16\n";
    code_ << "    ldind sp,t0,20\n    ldind sp,t1,12\n    subu t0,t0,t1\n    subu t0,t0,t6\n    stind sp,t0,20\n";
    code_ << "    addiu t3,zero,1\n    j " << LshiftQ << "\n    nop\n";
    code_ << LsubNo << ":\n    addu t3,zero,zero\n";
    code_ << LshiftQ << ":\n";

    // q = (q << 1) | quotient bit (in t3).
    code_ << "    ldind sp,t0,24\n    srl t0,t0,31\n    addu t5,t0,zero\n";
    code_ << "    ldind sp,t0,24\n    sll t0,t0,1\n    or t0,t0,t3\n    stind sp,t0,24\n";
    code_ << "    ldind sp,t0,28\n    sll t0,t0,1\n    or t0,t0,t5\n    stind sp,t0,28\n";
    code_ << "    j " << Lloop << "\n    nop\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    ldind sp,t3,40\n    ldind sp,t0,44\n    xor t3,t3,t0\n";
        code_ << "    beq t3,zero," << LskipNegQ << "\n    nop\n";
        code_ << "    ldind sp,t0,24\n    ldind sp,t1,28\n";
        genNegate64();
        code_ << "    stind sp,t0,24\n    stind sp,t1,28\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    ldind sp,t3,40\n    beq t3,zero," << LskipNegR << "\n    nop\n";
        code_ << "    ldind sp,t0,16\n    ldind sp,t1,20\n";
        genNegate64();
        code_ << "    stind sp,t0,16\n    stind sp,t1,20\n";
        code_ << LskipNegR << ":\n";
    }
    code_ << "    ldind sp,t0,24\n    ldind sp,t1,28\n";  // quotient
    code_ << "    ldind sp,t4,16\n    ldind sp,t5,20\n";  // remainder (saved past the scratch release)
    code_ << "    addiu sp,sp,52\n";  // release the 9-word scratch + the 4 earlier stashed operand words
    if (wantRemainder) {
        code_ << "    addu t0,t4,zero\n    addu t1,t5,zero\n";
    }
}

// A `long long` shifted by a runtime count: a loop of 1-bit shifts,
// each carrying the bit across the word boundary by hand (SLLV/SRLV
// only shift one 32-bit word).
void MipsCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-8\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);  // t0 = shift count (already promoted to a plain int)
    code_ << "    addu t3,t0,zero\n";
    code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    addiu sp,sp,8\n";  // t1:t0 restored = the value being shifted
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    beq t3,zero," << Ldone << "\n    nop\n    addiu t3,t3,-1\n";
    if (isLeft) {
        code_ << "    srl t4,t0,31\n";       // t4 = t0's old top bit, carries into t1
        code_ << "    sll t0,t0,1\n";
        code_ << "    sll t1,t1,1\n    or t1,t1,t4\n";
    } else {
        code_ << "    sll t4,t1,31\n";       // t4 = (t1&1)<<31, carries into t0's top
        code_ << "    srl t0,t0,1\n    or t0,t0,t4\n";
        code_ << "    " << (isUnsigned ? "srl" : "sra") << " t1,t1,1\n";
    }
    code_ << "    j " << Lloop << "\n    nop\n";
    code_ << Ldone << ":\n";
}

// A `long long` comparison: compare high words first (signed/unsigned
// per isUnsigned), falling back to an unsigned low-word compare only
// when the high words are equal.
void MipsCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-8\n    stind sp,t0\n    stind sp,t1,4\n";
    genExpr(rhs);
    code_ << "    addu t5,t0,zero\n    addu t6,t1,zero\n";  // t5:t6 = rhs (lo:hi)
    code_ << "    ldind sp,t3\n    ldind sp,t4,4\n    addiu sp,sp,8\n";  // t3:t4 = lhs (lo:hi)
    if (op == "==" || op == "!=") {
        std::string Lfalse = newLabel(), Ldone = newLabel();
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    bne t3,t5," << Lfalse << "\n    nop\n";
        code_ << "    bne t4,t6," << Lfalse << "\n    nop\n";
        code_ << "    addiu t0,zero," << onEqual << "\n    j " << Ldone << "\n    nop\n";
        code_ << Lfalse << ":\n    addiu t0,zero," << onDiffer << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    // Once the high words are equal, the low 32 bits are an unsigned
    // magnitude regardless of the overall type's signedness.
    std::string LhiDiff = newLabel(), Ldone = newLabel();
    code_ << "    bne t4,t6," << LhiDiff << "\n    nop\n";
    if (op == "<") {
        code_ << "    sltu t0,t3,t5\n";
    } else if (op == ">") {
        code_ << "    sltu t0,t5,t3\n";
    } else if (op == "<=") {
        code_ << "    sltu t0,t5,t3\n    addiu t1,zero,1\n    xor t0,t1,t0\n";  // !(rhs<lhs)
    } else {  // ">="
        code_ << "    sltu t0,t3,t5\n    addiu t1,zero,1\n    xor t0,t1,t0\n";  // !(lhs<rhs)
    }
    code_ << "    j " << Ldone << "\n    nop\n";
    code_ << LhiDiff << ":\n";
    const char *hiInsn = isUnsigned ? "sltu" : "slt";
    if (op == "<") {
        code_ << "    " << hiInsn << " t0,t4,t6\n";
    } else if (op == ">") {
        code_ << "    " << hiInsn << " t0,t6,t4\n";
    } else if (op == "<=") {
        code_ << "    " << hiInsn << " t0,t6,t4\n    addiu t1,zero,1\n    xor t0,t1,t0\n";
    } else {  // ">="
        code_ << "    " << hiInsn << " t0,t4,t6\n    addiu t1,zero,1\n    xor t0,t1,t0\n";
    }
    code_ << Ldone << ":\n";
}

// Entry: t0 = old, t1 = rhs (both at `type`'s width/signedness).
// Neither MULT/MFLO nor DIV/MFHI/MFLO touch t2, so the address
// genCompoundThroughT2() holds there survives this call.
void MipsCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    addu t0,t0,t1\n"; return; }
    if (op == "-") { code_ << "    subu t0,t0,t1\n"; return; }
    if (op == "*") {
        code_ << "    " << (isUnsigned ? "multu" : "mult") << " t0,t1\n";
        code_ << "    mflo t0\n";
        return;
    }
    if (op == "&") { code_ << "    and t0,t0,t1\n"; return; }
    if (op == "|") { code_ << "    or t0,t0,t1\n"; return; }
    if (op == "^") { code_ << "    xor t0,t0,t1\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "sllv" : (isUnsigned ? "srlv" : "srav");
        code_ << "    " << insn << " t0,t0,t1\n";
        return;
    }
    // "/" or "%"
    code_ << "    " << (isUnsigned ? "divu" : "div") << " t0,t1\n";
    code_ << "    " << (op == "%" ? "mfhi" : "mflo") << " t0\n";
}

// t2 must already hold the resolved address on entry; it's stashed on
// the stack across evaluating e.rhs (which may clobber t0-t4) and never
// recomputed, so the target's address sub-expression runs exactly once.
void MipsCodegen::genCompoundThroughT2(const ir::Expr &e) {
    code_ << "    addiu sp,sp,-4\n    stind sp,t2\n";  // stash address
    genExpr(*e.rhs);                                     // t0 = rhs
    code_ << "    addu t1,t0,zero\n";                      // t1 = rhs
    code_ << "    ldind sp,t2\n    addiu sp,sp,4\n";         // t2 = address (restored)
    loadOperand("t2", e.type);                                 // t0 = old
    if (e.yieldOld) code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";  // stash old
    genCompoundOp(e.op, e.type);                                 // t0 = new (raw)
    narrowT0To(e.type);
    if (e.yieldOld) {
        code_ << "    addu t1,t0,zero\n";                          // t1 = new
        code_ << "    ldind sp,t0\n    addiu sp,sp,4\n";             // t0 = old
        code_ << "    addu t3,t0,zero\n";                              // t3 = old, preserved across the store below
        code_ << "    addu t0,t1,zero\n";                                // t0 = new - storeOperand always stores from t0
        storeOperand("t2", e.type);
        code_ << "    addu t0,t3,zero\n";                                  // t0 = old (final result)
    } else {
        storeOperand("t2", e.type);  // t0 already = new
    }
}

void MipsCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Both operands share one common type (IRBuilder's usual-
    // arithmetic-conversion pass), except shift (only LHS matters)
    // and &&/|| (width-independent, handled below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
        if (op == "+") {
            code_ << "    addu t0,t1,t0\n";
        } else {
            code_ << "    subu t0,t1,t0\n";
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
            code_ << "    addiu sp,sp,-8\n    stind sp,t0\n    stind sp,t1,4\n";
            genExpr(*e.rhs);
            code_ << "    addu t3,t0,zero\n    addu t4,t1,zero\n";
            code_ << "    ldind sp,t0\n    ldind sp,t1,4\n    addiu sp,sp,8\n";
            code_ << "    " << insn << " t0,t0,t3\n    " << insn << " t1,t1,t4\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";
        code_ << "    " << insn << " t0,t1,t0\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
        genExpr(*e.rhs);  // t0 = shift count
        code_ << "    addu t1,t0,zero\n";
        code_ << "    ldind sp,t0\n    addiu sp,sp,4\n";  // t0 = value to shift
        const char *insn = op == "<<" ? "sllv" : (isUnsigned ? "srlv" : "srav");
        code_ << "    " << insn << " t0,t0,t1\n";
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

// Real hardware multiply (MULT/MULTU+MFLO), a full 32x32->32 product.
void MipsCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
    code_ << "    " << (isUnsigned ? "multu" : "mult") << " t1,t0\n";
    code_ << "    mflo t0\n";
}

// Real hardware divide (DIV/DIVU); MFLO/MFHI pull the quotient and
// remainder straight back out.
void MipsCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs (dividend), t0 = rhs (divisor)
    code_ << "    " << (isUnsigned ? "divu" : "div") << " t1,t0\n";
    code_ << "    " << (wantRemainder ? "mfhi" : "mflo") << " t0\n";
}

// SLT/SLTU directly produce the 0/1 boolean needed, no branch required.
// isUnsigned picks SLT vs SLTU for the ordered forms; ==/!= need no
// such choice.
void MipsCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    const char *sltInsn = isUnsigned ? "sltu" : "slt";
    genExpr(lhs);
    code_ << "    addiu sp,sp,-4\n    stind sp,t0\n";
    genExpr(rhs);
    code_ << "    ldind sp,t1\n    addiu sp,sp,4\n";  // t1 = lhs, t0 = rhs
    if (op == "<") {
        code_ << "    " << sltInsn << " t0,t1,t0\n";  // t0 = (lhs < rhs)
        return;
    }
    if (op == ">") {
        code_ << "    " << sltInsn << " t0,t0,t1\n";  // t0 = (rhs < lhs) = (lhs > rhs)
        return;
    }
    if (op == "<=") {
        code_ << "    " << sltInsn << " at,t0,t1\n";           // at = (rhs < lhs)
        code_ << "    addiu t0,zero,1\n    xor t0,t0,at\n";  // t0 = !(rhs<lhs) = (lhs<=rhs)
        return;
    }
    if (op == ">=") {
        code_ << "    " << sltInsn << " at,t1,t0\n";   // at = (lhs < rhs)
        code_ << "    addiu t0,zero,1\n    xor t0,t0,at\n";  // t0 = !(lhs<rhs) = (lhs>=rhs)
        return;
    }
    if (op == "==") {
        code_ << "    subu at,t1,t0\n";    // at = lhs - rhs
        code_ << "    sltiu t0,at,1\n";    // t0 = (unsigned(at) < 1) = (at == 0)
        return;
    }
    if (op == "!=") {
        code_ << "    subu at,t1,t0\n";    // at = lhs - rhs
        code_ << "    sltu t0,zero,at\n";  // t0 = (0 < unsigned(at)) = (at != 0)
        return;
    }
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// A wide operand's truthiness needs both words; `or t0,t0,t1` folds
// them together before the usual zero test.
void MipsCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq t0,zero," << Lfalse << "\n    nop\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    beq t0,zero," << Lfalse << "\n    nop\n";
    code_ << "    j " << Ltrue << "\n    nop\n";
    code_ << Lfalse << ":\n    addiu t0,zero,0\n    j " << Ldone << "\n    nop\n";
    code_ << Ltrue << ":\n    addiu t0,zero,1\n";
    code_ << Ldone << ":\n";
}

void MipsCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    or t0,t0,t1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    bne t0,zero," << Ltrue << "\n    nop\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    bne t0,zero," << Ltrue << "\n    nop\n";
    code_ << "    j " << Lfalse << "\n    nop\n";
    code_ << Ltrue << ":\n    addiu t0,zero,1\n    j " << Ldone << "\n    nop\n";
    code_ << Lfalse << ":\n    addiu t0,zero,0\n";
    code_ << Ldone << ":\n";
}

// SLTIU t0,t0,1 computes (unsigned(t0) < 1), i.e. logical not, in one
// instruction.
void MipsCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    or t0,t0,t1\n";
    code_ << "    sltiu t0,t0,1\n";
}

std::optional<std::string> MipsCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleMips(asmText);
    // coff/elf/macho/aout/objraw: build one format-neutral ObjectFile,
    // then pick that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleMipsObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
