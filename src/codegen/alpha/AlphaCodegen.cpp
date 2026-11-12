#include "AlphaCodegen.h"

#include <cstdint>
#include <stdexcept>

#include "AlphaAssembler.h"
#include "fileformat/FileFormat.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program loads; the test harness sets the simulated CPU's PC here directly.
constexpr uint32_t kBaseAddress = kAlphaBaseAddress;

// Initial `sp`, comfortably above the loaded program and its data; matches
// AlphaLinkerFactory.cpp's own kInitialSP so a linked program behaves like a single-file one.
constexpr uint32_t kInitialSP = 0x00200000u;

// Splits a 32-bit value into the (hi,lo) halves an `ldah`+`lda` pair needs - see
// AlphaAssembler.h's own comment on why `lo` must be sign-extended before `hi` is derived from
// what's left.
std::pair<int32_t, int32_t> splitHiLo(int32_t v) {
    int32_t lo = static_cast<int32_t>(static_cast<int16_t>(v & 0xFFFF));
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(v) - lo) >> 16);
    return {hi, lo};
}
}  // namespace

std::string AlphaCodegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void AlphaCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                 Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect (see AlphaAssembler.h) - a `long long` array's
    // literal elements are emitted as two "dw"s (low word, then high) per element; a symbol
    // element is always a pointer-width address, so it still gets one "dw".
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

void AlphaCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    ldah " << reg << ",zero," << lbl << "\n";
    code_ << "    lda " << reg << "," << reg << "," << lbl << "\n";
}

void AlphaCodegen::loadImm32(const std::string &reg, int32_t v) {
    if (v >= -32768 && v <= 32767) {
        code_ << "    lda " << reg << ",zero," << v << "\n";
        return;
    }
    auto [hi, lo] = splitHiLo(v);
    // splitHiLo()'s %hi/%lo split is exactly reversible by one ldah+lda pair except at its
    // own edge: when `v` is large and positive enough that its low 16 bits (interpreted as
    // signed, per `lda`'s own sign extension) come out negative, `hi` can land one past the
    // top of `ldah`'s 16-bit signed field (`32768`, not representable there at all) - split
    // that one extra `0x10000` across two `ldah`s instead, each safely in range.
    if (hi > 32767) {
        code_ << "    ldah " << reg << ",zero,16384\n";
        code_ << "    ldah " << reg << "," << reg << ",16384\n";
    } else {
        code_ << "    ldah " << reg << ",zero," << hi << "\n";
    }
    code_ << "    lda " << reg << "," << reg << "," << lo << "\n";
}

// Builds the low 32 bits (cleaned to a genuine zero-extended quantity) and the high 32 bits
// separately, then combines them - the one construction a single loadImm32() can't do, since
// its own `ldah`/`lda` pair only ever produces a *sign-extended* 32-bit result.
void AlphaCodegen::loadImm64(const std::string &reg, uint64_t v) {
    int64_t sv = static_cast<int64_t>(v);
    if (sv == static_cast<int64_t>(static_cast<int32_t>(sv))) {
        loadImm32(reg, static_cast<int32_t>(sv));
        return;
    }
    loadImm32(reg, static_cast<int32_t>(static_cast<uint32_t>(v & 0xFFFFFFFFu)));
    code_ << "    sll " << reg << "," << reg << ",32\n    srl " << reg << "," << reg << ",32\n";
    loadImm32("at", static_cast<int32_t>(static_cast<uint32_t>((v >> 32) & 0xFFFFFFFFu)));
    code_ << "    sll at,at,32\n";
    code_ << "    bis " << reg << "," << reg << ",at\n";
}

// A Param lives at `fp+frameOffset` (the caller's pushed argument space, see genFunction()'s
// own comment); a Local's lowest address is `fp-localLowOffset()`. Either way one `lda`
// suffices - Alpha's own address-arithmetic instruction takes a base register and a 16-bit
// displacement directly, no separate add step needed the way a target with no such
// instruction (e.g. MIPS's `addiu`) still manages fine, but Alpha's real `lda` is exactly
// this operation already.
void AlphaCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
        return;
    }
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    lda " << reg << ",fp," << slot->frameOffset << "\n";
        return;
    }
    code_ << "    lda " << reg << ",fp,-" << ir::localLowOffset(*slot, typeWidths()) << "\n";
}

std::string AlphaCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In objectMode, `prog` may not define `main`; the entry sequence below is skipped since
    // AlphaLinkerFactory.cpp synthesizes it once every object is merged and `main` is
    // resolvable. An object's base is `org 0`; the linker places it.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        auto [spHi, spLo] = splitHiLo(static_cast<int32_t>(kInitialSP));
        code_ << "_start:\n";
        code_ << "    ldah sp,zero," << spHi << "\n";
        code_ << "    lda sp,sp," << spLo << "\n";
        loadAddress("t12", label(mainFn));
        code_ << "    jsr ra,(t12)\n";
        // main's result sits in t0 once execution reaches halt; there is no way to stop the
        // processor from here (see AlphaCodegen.h's own header comment), so this spins.
        code_ << "halt:\n    br halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // A `ldindq`/`stindq` faults on a misaligned address, so a global wider than one byte
    // needs padding ahead of it to reach its own natural alignment (capped at 8, the widest
    // transfer this backend ever makes).
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        int elemW = widthOfType(slot->type);
        size_t totalBytes = slot->arraySize > 0 ? static_cast<size_t>(slot->arraySize) * elemW
                                                  : static_cast<size_t>(elemW);
        size_t align = static_cast<size_t>(elemW) < 8 ? static_cast<size_t>(elemW) : 8;
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
            // emission (one zero "db" per byte) rather than a new
            // directive, since this assembler dialect has no "reserve N
            // zero bytes" form of its own.
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

std::map<std::string, std::string> AlphaCodegen::perFileAsm() const { return perFileAsm_; }

// `fp`/`ra` are saved/restored as plain 32-bit values (`stindl`/`ldindl`, sign-extending back
// out on reload) rather than full 64-bit quadwords: every address in this backend's universe
// fits in 32 bits (kAlphaBaseAddress plus whatever this program's own code+data+stack need),
// so the low 32 bits already reconstruct the exact original value - and it keeps this
// reservation at exactly IR.h's own assumed "two native ints" (8 bytes) ahead of a Param's
// frameOffset=0 point, with no extra per-target adjustment the way a genuine 64-bit save would
// need (see Arm64Codegen.h's own `currentParamBase_`/`kParamExtra` for what that adjustment
// looks like when it can't be avoided this way).
void AlphaCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    lda sp,sp,-8\n";
    code_ << "    stindl sp,fp,0\n";
    code_ << "    stindl sp,ra,4\n";
    code_ << "    bis fp,sp,zero\n";
    if (fn.func->frameSize > 0) code_ << "    lda sp,sp,-" << fn.func->frameSize << "\n";

    genStmt(*fn.body);

    genEpilogue();
    code_ << "\n";
}

void AlphaCodegen::genEpilogue() {
    code_ << "    bis sp,fp,zero\n";
    code_ << "    ldindl sp,at,0\n";
    code_ << "    ldindl sp,ra,4\n";
    code_ << "    lda sp,sp,8\n";
    code_ << "    bis fp,at,zero\n";
    code_ << "    ret zero,(ra)\n";
}

void AlphaCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq t0," << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    br " << Ldone << "\n";
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
            code_ << "    beq t0," << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    br " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    br " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    br " << loopLabels_.back().first << "\n";
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
            code_ << "    br " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

void AlphaCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                loadImm64("t0", static_cast<uint64_t>(e.intValue));
            } else {
                // loadImm32() always sign-extends (the same `ldah`+`lda` sequence any signed
                // 32-bit value would use); an unsigned literal whose own bit 31 is set (e.g.
                // `2147483648u`) needs narrowT0To() to turn that back into a zero-extended
                // canonical representation.
                loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(e.intValue)));
                narrowT0To(e.type);
            }
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
                code_ << "    lda sp,sp,-8\n    stindq sp,t2,0\n";
                genExpr(*e.rhs);
                code_ << "    ldindq sp,t2,0\n    lda sp,sp,8\n";
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
                genIndexAddr(e.slot, *e.lhs);
                genCompoundThroughT2(e);
                return;
            }
            loadSlotAddr("t2", e.slot);
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    bis t2,t0,zero\n";
            genCompoundThroughT2(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq t0," << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    br " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // t0 = pointer value
            code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
            genExpr(*e.rhs);
            code_ << "    ldindq sp,t2,0\n    lda sp,sp,8\n";
            storeOperand("t2", e.type);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    subq t0,zero,t0\n";
                narrowT0To(e.rhs->type);
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // t0 = pointer value
                code_ << "    bis t2,t0,zero\n";
                loadOperand("t2", e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    ornot t0,zero,t0\n";
                narrowT0To(e.rhs->type);
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
            // Caller pushes right to left (arg 0 lands closest to the callee's own prologue
            // reservation, matching IR.h's ascending Param::frameOffset - see
            // genFunction()'s own comment), caller cleans up afterward. The callee's entry
            // address always goes through `t12`/`pv`, the real Alpha calling convention's own
            // "procedure value" register - `jsr`'s target is always a register, never an
            // immediate.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
                    totalBytes += 8;
                } else {
                    code_ << "    lda sp,sp,-4\n    stindl sp,t0,0\n";
                    totalBytes += 4;
                }
            }
            loadAddress("t12", label(e.func));
            code_ << "    jsr ra,(t12)\n";
            if (totalBytes > 0) code_ << "    lda sp,sp," << totalBytes << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);  // t0 = the callee's own entry address
            code_ << "    bis t12,t0,zero\n";
            code_ << "    jsr ra,(t12)\n";
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

// t2 ends up holding the computed address. arraySize > 0 means a compile-time-known array
// base (Global label or Local `fp`-relative); otherwise `slot` is a pointer variable, so the
// base is read from its own stored (pointer-width) value first.
void AlphaCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // t0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    sll t0,t0," << shift << "\n";
    if (slot->arraySize > 0) {
        loadSlotAddr("t2", slot);
        code_ << "    addq t2,t2,t0\n";
    } else {
        loadSlotAddr("t1", slot);
        code_ << "    ldindl t1,t1,0\n";  // t1 = the pointer variable's own stored value
        code_ << "    addq t2,t1,t0\n";
    }
}

void AlphaCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("t2", e.slot->type);
}

void AlphaCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        int offset = -ir::localLowOffset(*slot, typeWidths()) + w * static_cast<int>(i);
        if (elem.isSymbol) {
            loadAddress("t0", elem.slot ? label(elem.slot) : label(elem.func));
            code_ << "    lda t2,fp," << offset << "\n    stindl t2,t0,0\n";
            continue;
        }
        if (w == 8) {
            loadImm64("t0", static_cast<uint64_t>(elem.value));
        } else {
            loadImm32("t0", static_cast<int32_t>(static_cast<uint32_t>(elem.value)));
        }
        code_ << "    lda t2,fp," << offset << "\n";
        if (w == 1) {
            code_ << "    stindb t2,t0,0\n";
        } else if (w == 2) {
            code_ << "    stindh t2,t0,0\n";
        } else if (w == 4) {
            code_ << "    stindl t2,t0,0\n";
        } else {
            code_ << "    stindq t2,t0,0\n";
        }
    }
}

void AlphaCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("t2", operand.slot);
        code_ << "    bis t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    bis t0,t2,zero\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("t2", label(operand.func));
        code_ << "    bis t0,t2,zero\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void AlphaCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindbu " << addrReg << ",t0,0\n";
    } else if (w == 2) {
        code_ << "    ldindhu " << addrReg << ",t0,0\n";
    } else if (w == 4) {
        code_ << "    ldindl " << addrReg << ",t0,0\n";
    } else {
        code_ << "    ldindq " << addrReg << ",t0,0\n";
        return;
    }
    narrowT0To(type);
}

void AlphaCodegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",t0,0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",t0,0\n";
    } else if (w == 4) {
        code_ << "    stindl " << addrReg << ",t0,0\n";
    } else {
        code_ << "    stindq " << addrReg << ",t0,0\n";
    }
}

void AlphaCodegen::narrowT0To(Type to) {
    int w = widthOfType(to);
    if (w == 8) return;
    int shiftAmt = 64 - 8 * w;
    code_ << "    sll t0,t0," << shiftAmt << "\n";
    code_ << "    " << (to.isUnsigned ? "srl" : "sra") << " t0,t0," << shiftAmt << "\n";
}

// Crosses the native-`int`/`long` boundary (sign/zero-extending t0's low 32 bits across the
// rest of the register, or just re-narrowing them), or re-narrows a same-side Convert. Unlike
// every 32-bit-register backend in this project, the "wide && wide" case here genuinely is a
// complete no-op (the bit pattern needs no reshaping at all, since there's only ever one
// register to begin with).
void AlphaCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowT0To(to);
    } else if (!fromWide && toWide) {
        code_ << "    sll t0,t0,32\n";
        code_ << "    " << (from.isUnsigned ? "srl" : "sra") << " t0,t0,32\n";
    } else if (!fromWide && !toWide) {
        narrowT0To(to);
    }
    // fromWide && toWide: nothing to do.
}

void AlphaCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
        genExpr(*e.rhs);
        code_ << "    ldindq sp,t1,0\n    lda sp,sp,8\n";  // t1 = lhs, t0 = rhs
        code_ << "    " << (op == "+" ? "addq t0,t1,t0\n" : "subq t0,t1,t0\n");
        narrowT0To(operandType);
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        narrowT0To(operandType);
        return;
    }
    if (op == "/" || op == "%") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned, wide ? 64 : 32);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = op == "&" ? "and" : op == "|" ? "bis" : "xor";
        genExpr(*e.lhs);
        code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
        genExpr(*e.rhs);
        code_ << "    ldindq sp,t1,0\n    lda sp,sp,8\n";
        code_ << "    " << insn << " t0,t1,t0\n";
        narrowT0To(operandType);
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
        genExpr(*e.rhs);  // t0 = shift count
        code_ << "    bis t1,t0,zero\n";
        code_ << "    ldindq sp,t0,0\n    lda sp,sp,8\n";  // t0 = value to shift
        const char *insn = op == "<<" ? "sll" : (isUnsigned ? "srl" : "sra");
        code_ << "    " << insn << " t0,t0,t1\n";
        narrowT0To(operandType);
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

// A real hardware 64-bit multiply (`mulq`) directly, for every width: the low 64 bits of a
// product only ever depend on the low 64 bits of each operand (two's-complement
// multiplication is modular), so running it on the already sign/zero-extended `int` operands
// gives the exact right low-32-bit `int*int` result too - narrowT0To() (called by the caller,
// genBinary()) just re-trims it.
void AlphaCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
    genExpr(rhs);
    code_ << "    ldindq sp,t1,0\n    lda sp,sp,8\n";  // t1 = lhs, t0 = rhs
    code_ << "    mulq t0,t1,t0\n";
}

void AlphaCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                             int bits) {
    genExpr(lhs);
    code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
    genExpr(rhs);
    code_ << "    bis t1,t0,zero\n";                    // t1 = divisor
    code_ << "    ldindq sp,t0,0\n    lda sp,sp,8\n";    // t0 = dividend
    genDivModCore(wantRemainder, isUnsigned, bits);
}

// Alpha has no integer divide instruction at all (a genuine architectural omission - see
// AlphaCodegen.h's own header comment), so `/`/`%` always run this bit-serial restoring
// division: `bits` iterations of "shift the next dividend bit into the remainder, then
// subtract the divisor out of it if that doesn't go negative". The remainder is only ever
// compared once it's known to still fit in `bits` bits; the one bit that could carry it past
// that (t9, captured from the remainder's own top bit just before each shift) forces the
// subtraction unconditionally instead of running a (64-bit-register-only) comparison that
// would otherwise silently miss an actual `bits`-wide overflow.
void AlphaCodegen::genDivModCore(bool wantRemainder, bool isUnsigned, int bits) {
    std::string LskipNegN = newLabel(), LskipNegD = newLabel();
    if (!isUnsigned) {
        code_ << "    srl t6,t0," << (bits - 1) << "\n    and t6,t6,1\n";  // t6 = dividend's sign
        code_ << "    srl t7,t1," << (bits - 1) << "\n    and t7,t7,1\n";  // t7 = divisor's sign
        code_ << "    beq t6," << LskipNegN << "\n";
        code_ << "    subq t0,zero,t0\n";
        if (bits == 32) code_ << "    sll t0,t0,32\n    srl t0,t0,32\n";
        code_ << LskipNegN << ":\n";
        code_ << "    beq t7," << LskipNegD << "\n";
        code_ << "    subq t1,zero,t1\n";
        if (bits == 32) code_ << "    sll t1,t1,32\n    srl t1,t1,32\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    bis t3,zero,zero\n";  // t3 = remainder
    code_ << "    bis t4,zero,zero\n";  // t4 = quotient
    code_ << "    lda t5,zero," << bits << "\n";

    std::string Lloop = newLabel(), Ldone = newLabel(), LsubYes = newLabel(), LsubNo = newLabel(), Lcont = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    beq t5," << Ldone << "\n";
    code_ << "    subq t5,t5,1\n";
    code_ << "    srl t8,t0," << (bits - 1) << "\n    and t8,t8,1\n";  // t8 = dividend's own top bit
    code_ << "    sll t0,t0,1\n";
    code_ << "    srl t9,t3," << (bits - 1) << "\n    and t9,t9,1\n";  // t9 = remainder's own top bit (overflow)
    code_ << "    sll t3,t3,1\n    bis t3,t3,t8\n";                     // remainder = (remainder<<1)|dividendBit
    code_ << "    bne t9," << LsubYes << "\n";
    code_ << "    cmpult at,t3,t1\n";
    code_ << "    bne at," << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    subq t3,t3,t1\n";
    code_ << "    sll t4,t4,1\n    bis t4,t4,1\n";
    code_ << "    br " << Lcont << "\n";
    code_ << LsubNo << ":\n";
    code_ << "    sll t4,t4,1\n";
    code_ << Lcont << ":\n";
    code_ << "    br " << Lloop << "\n";
    code_ << Ldone << ":\n";

    if (!isUnsigned) {
        std::string LnoNegQ = newLabel(), LnoNegR = newLabel();
        code_ << "    xor at,t6,t7\n";
        code_ << "    beq at," << LnoNegQ << "\n";
        code_ << "    subq t4,zero,t4\n";
        code_ << LnoNegQ << ":\n";
        code_ << "    beq t6," << LnoNegR << "\n";  // remainder's sign follows the dividend's
        code_ << "    subq t3,zero,t3\n";
        code_ << LnoNegR << ":\n";
    }
    code_ << "    bis t0," << (wantRemainder ? "t3" : "t4") << ",zero\n";
    if (bits == 32) {
        code_ << "    sll t0,t0,32\n";
        code_ << "    " << (isUnsigned ? "srl" : "sra") << " t0,t0,32\n";
    }
}

// Entry: t0 = old, t1 = rhs (both at `type`'s width/signedness). `genCompoundThroughT2()`
// never touches t2 while this runs, so the address it holds survives.
void AlphaCodegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") {
        code_ << "    addq t0,t0,t1\n";
        return;
    }
    if (op == "-") {
        code_ << "    subq t0,t0,t1\n";
        return;
    }
    if (op == "*") {
        code_ << "    mulq t0,t0,t1\n";
        return;
    }
    if (op == "&") {
        code_ << "    and t0,t0,t1\n";
        return;
    }
    if (op == "|") {
        code_ << "    bis t0,t0,t1\n";
        return;
    }
    if (op == "^") {
        code_ << "    xor t0,t0,t1\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "sll" : (isUnsigned ? "srl" : "sra");
        code_ << "    " << insn << " t0,t0,t1\n";
        return;
    }
    // "/" or "%"
    genDivModCore(op == "%", isUnsigned, isWide(type) ? 64 : 32);
}

// t2 must already hold the resolved address on entry; it's stashed on the stack across
// evaluating e.rhs (which may clobber t0-t9) and never recomputed, so the target's address
// sub-expression runs exactly once.
void AlphaCodegen::genCompoundThroughT2(const ir::Expr &e) {
    code_ << "    lda sp,sp,-8\n    stindq sp,t2,0\n";  // stash address
    genExpr(*e.rhs);                                      // t0 = rhs
    code_ << "    bis t1,t0,zero\n";                         // t1 = rhs
    code_ << "    ldindq sp,t2,0\n    lda sp,sp,8\n";          // t2 = address (restored)
    loadOperand("t2", e.type);                                   // t0 = old
    if (e.yieldOld) code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";  // stash old
    genCompoundOp(e.op, e.type);                                   // t0 = new (raw)
    narrowT0To(e.type);
    if (e.yieldOld) {
        code_ << "    bis t1,t0,zero\n";                                // t1 = new
        code_ << "    ldindq sp,t0,0\n    lda sp,sp,8\n";                 // t0 = old
        code_ << "    bis t3,t0,zero\n";                                    // t3 = old, preserved across the store
        code_ << "    bis t0,t1,zero\n";                                      // t0 = new - storeOperand stores from t0
        storeOperand("t2", e.type);
        code_ << "    bis t0,t3,zero\n";                                        // t0 = old (final result)
    } else {
        storeOperand("t2", e.type);  // t0 already = new
    }
}

// Real Alpha has no "not equal"/"greater"/"greater-or-equal" compare, only cmpeq/cmplt/cmple
// (and their unsigned cmpult/cmpule counterparts), each directly a 0/1 boolean (mirroring
// MIPS's SLT): the missing ones are derived by swapping operands (`>` is `<` reversed) or
// XOR-with-1 (`!=`/`<=`/`>=` are each the complement of one of the three real compares) -
// `xor`'s own literal form takes the `1` directly, no register needed to hold it first.
void AlphaCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    bool isUnsigned = lhs.type.isUnsigned;
    const char *ltInsn = isUnsigned ? "cmpult" : "cmplt";
    genExpr(lhs);
    code_ << "    lda sp,sp,-8\n    stindq sp,t0,0\n";
    genExpr(rhs);
    code_ << "    ldindq sp,t1,0\n    lda sp,sp,8\n";  // t1 = lhs, t0 = rhs
    if (op == "<") {
        code_ << "    " << ltInsn << " t0,t1,t0\n";  // t0 = (lhs < rhs)
        return;
    }
    if (op == ">") {
        code_ << "    " << ltInsn << " t0,t0,t1\n";  // t0 = (rhs < lhs) = (lhs > rhs)
        return;
    }
    if (op == "<=") {
        code_ << "    " << ltInsn << " at,t0,t1\n";  // at = (rhs < lhs)
        code_ << "    xor t0,at,1\n";                    // t0 = !(rhs<lhs) = (lhs<=rhs)
        return;
    }
    if (op == ">=") {
        code_ << "    " << ltInsn << " at,t1,t0\n";  // at = (lhs < rhs)
        code_ << "    xor t0,at,1\n";                    // t0 = !(lhs<rhs) = (lhs>=rhs)
        return;
    }
    if (op == "==") {
        code_ << "    cmpeq t0,t1,t0\n";
        return;
    }
    if (op == "!=") {
        code_ << "    cmpeq at,t1,t0\n";
        code_ << "    xor t0,at,1\n";
        return;
    }
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void AlphaCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq t0," << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    beq t0," << Lfalse << "\n";
    code_ << "    br " << Ltrue << "\n";
    code_ << Lfalse << ":\n    lda t0,zero,0\n    br " << Ldone << "\n";
    code_ << Ltrue << ":\n    lda t0,zero,1\n";
    code_ << Ldone << ":\n";
}

void AlphaCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    bne t0," << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    bne t0," << Ltrue << "\n";
    code_ << "    br " << Lfalse << "\n";
    code_ << Ltrue << ":\n    lda t0,zero,1\n    br " << Ldone << "\n";
    code_ << Lfalse << ":\n    lda t0,zero,0\n";
    code_ << Ldone << ":\n";
}

// `cmpeq t0,t0,zero` computes `(t0 == 0) ? 1 : 0` directly - logical not - in one instruction;
// `zero` is an ordinary operand register here (real register 31), not special syntax.
void AlphaCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmpeq t0,t0,zero\n";
}

std::optional<std::string> AlphaCodegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleAlpha(asmText);
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleAlphaObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace rcc
