#include "SaturnCodegen.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

#include "SaturnAssembler.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}
}  // namespace

std::optional<std::string> SaturnCodegen::assembleInProcess(const std::string &asmText,
                                                              const std::string &format) const {
    if (format == "raw") return assembleSaturn(asmText, /*allowExternal=*/false);
    if (format == "coff") return assembleSaturn(asmText, /*allowExternal=*/true);
    return std::nullopt;
}

// `char`/`short` are genuinely narrower than this backend's 32-bit registers (`ldb`/`ldh`
// already zero-extend on load, so this is belt-and-suspenders there, but a value arriving via
// arithmetic rather than a fresh load - e.g. after `add`/`sub`/a runtime multiply/divide call -
// has no such guarantee), so the VALUE (never the storage - that's already the right width) is
// masked, and re-sign-extended if signed, at every point it's read or converted.
void SaturnCodegen::narrowA(Type to) {
    // `to.base` names a pointer's pointee type, not its own, so narrowing `a` as a `char`/
    // `short` here would corrupt a real address.
    if (isPointer(to)) return;
    int width = widthOfType(to);
    if (width >= 4) return;  // int/long/long long are already full register width (or wide)
    uint32_t mask = width == 1 ? 0xFFu : 0xFFFFu;
    uint32_t signBit = width == 1 ? 0x80u : 0x8000u;
    uint32_t signExtend = ~mask;
    code_ << "    and a, " << mask << "\n";
    if (to.isUnsigned) return;
    std::string Ldone = newLabel();
    code_ << "    cmp a, " << signBit << "\n";
    code_ << "    jl " << Ldone << "\n";
    code_ << "    or a, " << signExtend << "\n";
    code_ << Ldone << ":\n";
}

// --- generate() -----------------------------------------------------------

std::string SaturnCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    std::string homeFile = mainFn ? mainFn->sourceFile
                            : !prog.functionBodies.empty() ? prog.functionBodies.front().func->sourceFile
                            : !prog.slots.empty() ? prog.slots.front()->sourceFile
                                                   : std::string();

    code_ << "org " << (objectMode ? 0 : kSaturnBaseAddress) << "\n\n";

    if (mainFn && !objectMode) {
        size_t rangeStart = static_cast<size_t>(code_.tellp());
        code_ << "    mov d0, " << kSaturnStackTop << "\n";
        code_ << "    gosbvl " << label(mainFn) << "\n";
        code_ << "    halt\n";
        codeRanges.push_back({mainFn->sourceFile, rangeStart, static_cast<size_t>(code_.tellp())});
    }

    for (const auto &fb : prog.functionBodies) {
        if (fb.func->hasAddress) {
            err(fb.line, "'@' fixed placement isn't supported on this backend (see "
                         "docs/platforms.md's saturn entry)");
        }
        size_t fCodeStart = static_cast<size_t>(code_.tellp());
        size_t fTrailerStart = trailer_.size();
        genFunction(fb);
        codeRanges.push_back({fb.func->sourceFile, fCodeStart, static_cast<size_t>(code_.tellp())});
        trailerRanges.push_back({fb.func->sourceFile, fTrailerStart, trailer_.size()});
    }

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        if (slot->hasAddress) {
            err(0, "'@' fixed placement isn't supported on this backend (see docs/platforms.md's "
                   "saturn entry)");
        }
        size_t gTrailerStart = trailer_.size();
        // A `char`/`short` element (byteWidth < 4) is one `db`/`dh` value; an `int`/pointer/
        // `long`/`long long` element (always a multiple of 4 bytes) is `byteWidth / 4`
        // consecutive `dw` words, word 0/low at the lowest address.
        int byteWidth = widthOfType(slot->type);
        int units = byteWidth < 4 ? 1 : byteWidth / 4;
        std::string directive = byteWidth == 1 ? "db" : byteWidth == 2 ? "dh" : "dw";
        auto elemUnitText = [&](const ir::ArrayInitElem &e, int unitIdx) -> std::string {
            if (!e.isSymbol) {
                long v = units == 1 ? e.value : (e.value >> (32 * unitIdx)) & 0xFFFFFFFFL;
                return std::to_string(v);
            }
            if (unitIdx > 0) return "0";
            return e.slot ? label(e.slot) : label(e.func);
        };
        if (slot->arraySize > 0) {
            bool first = true;
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                for (int u = 0; u < units; u++) {
                    std::string text = elemUnitText(slot->arrayInit[i], u);
                    if (first) {
                        trailer_.push_back(label(slot.get()) + ": " + directive + " " + text + "  ; " +
                                            slot->sourceName);
                        first = false;
                    } else {
                        trailer_.push_back(directive + " " + text);
                    }
                }
            }
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            for (int u = 0; u < units; u++) {
                long unitValue = units == 1 ? v : (v >> (32 * u)) & 0xFFFFFFFFL;
                std::string text = std::to_string(unitValue);
                trailer_.push_back((u == 0 ? label(slot.get()) + ": " + directive + " " : directive + " ") + text +
                                    (u == 0 ? "  ; " + slot->sourceName : ""));
            }
        }
        trailerRanges.push_back({slot->sourceFile, gTrailerStart, trailer_.size()});
    }

    // Scratch cells the multiply/divide runtime routines (saturnRuntimeAsm()) use. Only emitted
    // in whole-program mode - under objectMode every file leaves these external, and
    // SaturnLinkerFactory.cpp's entry object supplies the one shared definition (same reasoning
    // as pdp7's own __sp/__fp/etc. - see Pdp7Codegen.cpp's generate()).
    if (!objectMode) {
        size_t sharedStart = static_cast<size_t>(code_.tellp());
        code_ << saturnRuntimeAsm();
        codeRanges.push_back({homeFile, sharedStart, static_cast<size_t>(code_.tellp())});
    }

    std::string codeStr = code_.str();
    perFileAsm_.clear();
    for (const auto &[file, s, e] : codeRanges) perFileAsm_[file] += codeStr.substr(s, e - s);
    for (const auto &[file, s, e] : trailerRanges) {
        for (size_t i = s; i < e; i++) perFileAsm_[file] += trailer_[i] + "\n";
    }

    std::ostringstream out;
    out << codeStr;
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> SaturnCodegen::perFileAsm() const { return perFileAsm_; }

void SaturnCodegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ":\n";
    emitPrologue(fn.func);
    genStmt(*fn.body);
    emitEpilogueAndReturn(/*hasValue=*/false, Type{});
}

// --- Frame mechanics -------------------------------------------------------

void SaturnCodegen::emitPrologue(const ir::Function *fn) {
    // `gosbvl` already pushed the return address onto the `d0` stack before jumping here.
    code_ << "    push d1\n";       // save caller's d1 (oldFP)
    code_ << "    mov d1, d0\n";    // this call's own frame pointer
    if (fn->frameSize > 0) {
        code_ << "    sub d0, " << fn->frameSize << "\n";  // reserve this call's Locals
    }
}

void SaturnCodegen::emitEpilogueAndReturn(bool hasValue, Type retType) {
    bool wide = hasValue && isWide(retType);
    int chunks = wide ? chunkCount(retType) : 0;
    std::vector<std::string> retCells;
    if (hasValue && wide) {
        // genWideExpr() already pushed `chunks` 32-bit chunks (chunk 0/low on top); the frame
        // teardown below discards everything below the *caller's* own stack level, so pop them
        // into named cells first and push them back once the real frame is gone.
        for (int i = 0; i < chunks; i++) {
            std::string cell = "__retvalW" + std::to_string(i);
            trailer_.push_back(cell + ": dw 0");
            code_ << "    pop a\n    st [" << cell << "], a\n";
            retCells.push_back(cell);
        }
    }
    code_ << "    mov d0, d1\n";  // discard this call's own Locals
    code_ << "    pop d1\n";      // restore caller's d1 (oldFP); `d0` now points at the return
                                  // address `gosbvl` pushed (top of stack).
    if (!wide) {
        // A narrow return value is already sitting in `a` from the caller's own genExpr()/
        // genStmt() Return case; `rtn` pops the return address and jumps to it directly.
        code_ << "    rtn\n";
        return;
    }
    // A wide return value's chunks must land ABOVE the return address once we're back in the
    // caller (see genWideExpr()'s own Call case), so the return address has to come off the
    // stack before they go on - `rtn` alone can't be reordered around that, hence `jmpr`.
    code_ << "    pop b\n";  // b = return address
    for (int i = chunks - 1; i >= 0; i--) {
        code_ << "    ld a, [" << retCells[i] << "]\n    push a\n";
    }
    code_ << "    jmpr b\n";
}

namespace {
// `widthOfType()` is always 1 (char), 2 (short) or 4 (int/pointer) at every call site below -
// long/long long always go through the wide (genWideExpr) path instead, which only ever deals
// in 4-byte chunks via plain `ld`/`st`.
const char *ldMnemonic(int byteWidth) { return byteWidth == 1 ? "ldb" : byteWidth == 2 ? "ldh" : "ld"; }
const char *stMnemonic(int byteWidth) { return byteWidth == 1 ? "stb" : byteWidth == 2 ? "sth" : "st"; }
}  // namespace

void SaturnCodegen::emitLoadSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    mov a, " << label(slot) << "\n";
        return;
    }
    long offset = (slot->kind == ir::SlotKind::Param) ? static_cast<long>(slot->frameOffset)
                                                        : -localBaseOffset(slot);
    code_ << "    mov a, d1\n";
    if (offset > 0) {
        code_ << "    add a, " << offset << "\n";
    } else if (offset < 0) {
        code_ << "    sub a, " << (-offset) << "\n";
    }
}

void SaturnCodegen::emitLoadSlot(const ir::Slot *slot) {
    const char *ld = ldMnemonic(widthOfType(slot->type));
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    " << ld << " a, [" << label(slot) << "]\n";
        narrowA(slot->type);
        return;
    }
    emitLoadSlotAddress(slot);
    code_ << "    " << ld << " a, [a]\n";
    narrowA(slot->type);
}

void SaturnCodegen::emitStoreSlot(const ir::Slot *slot) {
    const char *st = stMnemonic(widthOfType(slot->type));
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    " << st << " [" << label(slot) << "], a\n";
        return;
    }
    code_ << "    push a\n";
    emitLoadSlotAddress(slot);
    code_ << "    mov c, a\n";
    code_ << "    pop a\n";
    code_ << "    " << st << " [c], a\n";
}

void SaturnCodegen::emitSlotBaseAddress(const ir::Slot *slot) {
    if (slot->arraySize > 0) {
        emitLoadSlotAddress(slot);
    } else {
        emitLoadSlot(slot);
    }
}

long SaturnCodegen::localBaseOffset(const ir::Slot *slot) { return ir::localLowOffset(*slot, typeWidths()); }

void SaturnCodegen::emitLocalArrayInit(const ir::Slot *slot) {
    // A `char`/`short` element (byteWidth < 4) is a single narrower store; an `int`/pointer/
    // `long`/`long long` element (always a multiple of 4 bytes) is `byteWidth / 4` consecutive
    // 4-byte words, word 0/low at the lowest address - the same layout a Global array's
    // initializer uses (see generate()'s own per-element loop).
    int byteWidth = widthOfType(slot->type);
    int units = byteWidth < 4 ? 1 : byteWidth / 4;
    const char *st = stMnemonic(byteWidth < 4 ? byteWidth : 4);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        for (int w = 0; w < units; w++) {
            if (!elem.isSymbol) {
                long unitValue = units == 1 ? elem.value : (elem.value >> (32 * w)) & 0xFFFFFFFFL;
                code_ << "    mov a, " << unitValue << "\n";
            } else if (w > 0) {
                // A symbol element is a plain word-wide address; only word 0 holds it, every
                // higher word is zero.
                code_ << "    mov a, 0\n";
            } else if (elem.slot) {
                emitLoadSlotAddress(elem.slot);
            } else {
                code_ << "    mov a, " << label(elem.func) << "\n";
            }
            code_ << "    mov c, d1\n";
            long offset = -localBaseOffset(slot) + static_cast<long>(i) * byteWidth + w * 4;
            if (offset > 0) {
                code_ << "    add c, " << offset << "\n";
            } else if (offset < 0) {
                code_ << "    sub c, " << (-offset) << "\n";
            }
            code_ << "    " << st << " [c], a\n";
        }
    }
}

// --- Element addressing -----------------------------------------------------

// Every scalar element width this type system ever produces on its own (1/2/4 words -
// char/short/int collapse to 1, long to 2, long long to 4) is a power of two, so scaling an
// index by the element's real byte stride is usually just a `shl`. A struct element's width is
// whatever its members add up to, though, and isn't always a power of two (e.g. three `char`
// members), so this falls back to the software `__mul32` routine - see genMul() - for any width
// `log2ElemWidth()` can't represent exactly.
namespace {
// Returns -1 (not a valid shift) for a width that isn't exactly a power of two, instead of
// silently rounding it down via the shift-right loop losing its low bits.
int log2ElemWidth(int bytesPerElem) {
    if (bytesPerElem <= 0 || (bytesPerElem & (bytesPerElem - 1)) != 0) return -1;
    int shiftAmt = 0;
    for (int w = bytesPerElem; w > 1; w >>= 1) shiftAmt++;
    return shiftAmt;
}
}  // namespace

void SaturnCodegen::computeElementAddressA(const ir::Slot *slot, const ir::Expr &idxExpr) {
    int bytesPerElem = widthOfType(slot->type);
    int shiftAmt = log2ElemWidth(bytesPerElem);
    if (exprIsCallFree(idxExpr)) {
        genExpr(idxExpr);  // index in `a`
        code_ << "    mov c, a\n";
        if (shiftAmt < 0) {
            // Non-power-of-two width: scale via __mul32 first (it clobbers `a`), then compute
            // the base address afterward, once `a` is free again.
            code_ << "    st [__mul_a], c\n";
            code_ << "    mov a, " << bytesPerElem << "\n";
            code_ << "    st [__mul_b], a\n";
            code_ << "    gosbvl __mul32\n";
            code_ << "    mov c, a\n";
            emitSlotBaseAddress(slot);  // base in `a`
            code_ << "    add a, c\n";
            return;
        }
        emitSlotBaseAddress(slot);  // base in `a`
        if (shiftAmt != 0) code_ << "    shl c, " << shiftAmt << "\n";
        code_ << "    add a, c\n";
        return;
    }
    // idxExpr may itself call: compute the base first and protect it on the stack while the
    // index evaluates.
    emitSlotBaseAddress(slot);
    code_ << "    push a\n";
    genExpr(idxExpr);
    code_ << "    mov c, a\n";
    if (shiftAmt < 0) {
        code_ << "    st [__mul_a], c\n";
        code_ << "    mov a, " << bytesPerElem << "\n";
        code_ << "    st [__mul_b], a\n";
        code_ << "    gosbvl __mul32\n";
        code_ << "    mov c, a\n";
        code_ << "    pop a\n";
        code_ << "    add a, c\n";
        return;
    }
    if (shiftAmt != 0) code_ << "    shl c, " << shiftAmt << "\n";
    code_ << "    pop a\n";
    code_ << "    add a, c\n";
}

void SaturnCodegen::computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr) {
    computeElementAddressA(slot, idxExpr);
    code_ << "    push a\n";
}

void SaturnCodegen::genIndex(const ir::Expr &e) {
    computeElementAddressA(e.slot, *e.lhs);
    code_ << "    " << ldMnemonic(widthOfType(e.slot->type)) << " a, [a]\n";
    narrowA(e.type);
}

void SaturnCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Index) {
        computeElementAddressA(operand.slot, *operand.lhs);
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    mov a, " << label(operand.func) << "\n";
        return;
    }
    // A bare Var naming either a declared array (its own address) or a plain scalar (its
    // address).
    emitLoadSlotAddress(operand.slot);
}

bool SaturnCodegen::exprIsCallFree(const ir::Expr &e) const {
    if (e.kind == ir::ExprKind::Call || e.kind == ir::ExprKind::CallPtr) return false;
    for (const ir::Expr *child : {e.lhs.get(), e.rhs.get(), e.elseExpr.get()}) {
        if (child && !exprIsCallFree(*child)) return false;
    }
    for (const auto &arg : e.args) {
        if (!exprIsCallFree(*arg)) return false;
    }
    return true;
}

// --- Statements -------------------------------------------------------------

void SaturnCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            genCondBranch(*s.cond, s.elseStmt ? Lelse : Ldone);
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    goto " << Ldone << "\n";
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
            genCondBranch(*s.cond, Lend);
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    goto " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    goto " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    goto " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            bool hasValue = s.initOrReturnOrExpr != nullptr;
            if (hasValue) {
                if (isWide(s.initOrReturnOrExpr->type)) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    genExpr(*s.initOrReturnOrExpr);
                }
            }
            emitEpilogueAndReturn(hasValue, hasValue ? s.initOrReturnOrExpr->type : Type{});
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                emitLocalArrayInit(s.declSlot);
            } else if (isWide(s.declSlot->type)) {
                if (s.initOrReturnOrExpr) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    pushWideLiteral(s.declSlot->type, 0);
                }
                emitLoadSlotAddress(s.declSlot);
                popWideToAddress(s.declSlot->type);
            } else {
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                } else {
                    code_ << "    mov a, 0\n";
                }
                emitStoreSlot(s.declSlot);
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
            code_ << "    goto " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

// --- Conditions ---------------------------------------------------------

std::string SaturnCodegen::jccForFalse(const std::string &op, bool isUnsigned) {
    // After `cmp c, a` (c = lhs, a = rhs): jump away when `c op a` is FALSE.
    if (op == "==") return "jne";
    if (op == "!=") return "je";
    if (op == "<") return isUnsigned ? "jae" : "jge";
    if (op == "<=") return isUnsigned ? "ja" : "jg";
    if (op == ">") return isUnsigned ? "jbe" : "jle";
    return isUnsigned ? "jb" : "jl";  // ">="
}

std::string SaturnCodegen::jccForTrue(const std::string &op, bool isUnsigned) {
    if (op == "==") return "je";
    if (op == "!=") return "jne";
    if (op == "<") return isUnsigned ? "jb" : "jl";
    if (op == "<=") return isUnsigned ? "jbe" : "jle";
    if (op == ">") return isUnsigned ? "ja" : "jg";
    return isUnsigned ? "jae" : "jge";  // ">="
}

namespace {
bool isRelational(const std::string &op) {
    return op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
}
}  // namespace

void SaturnCodegen::genCondBranch(const ir::Expr &cond, const std::string &label) {
    if (cond.kind == ir::ExprKind::Binary && cond.op == "&&") {
        genCondBranch(*cond.lhs, label);
        genCondBranch(*cond.rhs, label);
        return;
    }
    if (cond.kind == ir::ExprKind::Binary && cond.op == "||") {
        std::string Lcontinue = newLabel();
        genCondBranchTrue(*cond.lhs, Lcontinue);
        genCondBranch(*cond.rhs, label);
        code_ << Lcontinue << ":\n";
        return;
    }
    if (cond.kind == ir::ExprKind::Unary && cond.op == "!") {
        genCondBranchTrue(*cond.rhs, label);
        return;
    }
    if (cond.kind == ir::ExprKind::Binary && isRelational(cond.op) && !isWide(cond.lhs->type) &&
        !isWide(cond.rhs->type)) {
        bool unsignedCmp = cond.lhs->type.isUnsigned || cond.rhs->type.isUnsigned || isPointer(cond.lhs->type) ||
                            isPointer(cond.rhs->type);
        genOperandsIntoCA(*cond.lhs, *cond.rhs);
        code_ << "    cmp c, a\n";
        code_ << "    " << jccForFalse(cond.op, unsignedCmp) << " " << label << "\n";
        return;
    }
    genExpr(cond);
    code_ << "    cmp a, 0\n";
    code_ << "    je " << label << "\n";
}

void SaturnCodegen::genCondBranchTrue(const ir::Expr &cond, const std::string &label) {
    if (cond.kind == ir::ExprKind::Binary && cond.op == "||") {
        genCondBranchTrue(*cond.lhs, label);
        genCondBranchTrue(*cond.rhs, label);
        return;
    }
    if (cond.kind == ir::ExprKind::Binary && cond.op == "&&") {
        std::string Lcontinue = newLabel();
        genCondBranch(*cond.lhs, Lcontinue);
        genCondBranchTrue(*cond.rhs, label);
        code_ << Lcontinue << ":\n";
        return;
    }
    if (cond.kind == ir::ExprKind::Unary && cond.op == "!") {
        genCondBranch(*cond.rhs, label);
        return;
    }
    if (cond.kind == ir::ExprKind::Binary && isRelational(cond.op) && !isWide(cond.lhs->type) &&
        !isWide(cond.rhs->type)) {
        bool unsignedCmp = cond.lhs->type.isUnsigned || cond.rhs->type.isUnsigned || isPointer(cond.lhs->type) ||
                            isPointer(cond.rhs->type);
        genOperandsIntoCA(*cond.lhs, *cond.rhs);
        code_ << "    cmp c, a\n";
        code_ << "    " << jccForTrue(cond.op, unsignedCmp) << " " << label << "\n";
        return;
    }
    genExpr(cond);
    code_ << "    cmp a, 0\n";
    code_ << "    jne " << label << "\n";
}

void SaturnCodegen::genCompareValue(const std::string &op, bool isUnsigned) {
    // On entry: `cmp c, a` has already run (c = lhs, a = rhs). Leaves a 0/1 result in `a`.
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << jccForTrue(op, isUnsigned) << " " << Ltrue << "\n";
    code_ << "    mov a, 0\n";
    code_ << "    goto " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    code_ << "    mov a, 1\n";
    code_ << Ldone << ":\n";
}

std::string SaturnCodegen::allocTemp() { return "T" + std::to_string(tempCounter_++); }
void SaturnCodegen::declareTemp(const std::string &label) { trailer_.push_back(label + ": dw 0"); }

// --- Binary operators -------------------------------------------------------

// Always protects `lhs` via the stack across `rhs`'s own evaluation, rather than trying to keep
// it in a register: `rhs` need not itself call a function to clobber `c`/`b` - almost every
// operator this backend emits (div/mod/multiply via the runtime routines, a nested binary op's
// own `genOperandsIntoCA`) uses them as scratch regardless, so "rhs is call-free" is not the
// same as "rhs leaves c alone".
void SaturnCodegen::genOperandsIntoCA(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push a\n";
    genExpr(rhs);
    code_ << "    pop c\n";
}

void SaturnCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genOperandsIntoCA(*e.lhs, *e.rhs);
        code_ << (op == "+" ? "    add c, a\n" : "    sub c, a\n");
        code_ << "    mov a, c\n";
        narrowA(e.type);
        return;
    }
    if (op == "&" || op == "^" || op == "|") {
        genBitwise(op, *e.lhs, *e.rhs);
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(op, *e.lhs, *e.rhs);
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
    if (isRelational(op)) {
        bool unsignedCmp = e.lhs->type.isUnsigned || e.rhs->type.isUnsigned || isPointer(e.lhs->type) ||
                            isPointer(e.rhs->type);
        if (isWide(e.lhs->type) || isWide(e.rhs->type)) {
            genWideExpr(*e.lhs);
            genWideExpr(*e.rhs);
            genWideCompare(op, chunkCount(isWide(e.lhs->type) ? e.lhs->type : e.rhs->type), unsignedCmp);
            return;
        }
        genOperandsIntoCA(*e.lhs, *e.rhs);
        code_ << "    cmp c, a\n";
        genCompareValue(op, unsignedCmp);
        return;
    }
    if (op == "&&") {
        std::string Lfalse = newLabel(), Ldone = newLabel();
        genCondBranch(*e.lhs, Lfalse);
        genCondBranch(*e.rhs, Lfalse);
        code_ << "    mov a, 1\n    goto " << Ldone << "\n";
        code_ << Lfalse << ":\n    mov a, 0\n" << Ldone << ":\n";
        return;
    }
    if (op == "||") {
        std::string Ltrue = newLabel(), Ldone = newLabel();
        genCondBranchTrue(*e.lhs, Ltrue);
        genCondBranchTrue(*e.rhs, Ltrue);
        code_ << "    mov a, 0\n    goto " << Ldone << "\n";
        code_ << Ltrue << ":\n    mov a, 1\n" << Ldone << ":\n";
        return;
    }
    err(e.line, "internal: unhandled binary operator '" + op + "'");
}

void SaturnCodegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genOperandsIntoCA(lhs, rhs);
    code_ << "    " << (op == "&" ? "and" : op == "|" ? "or" : "xor") << " c, a\n";
    code_ << "    mov a, c\n";
}

void SaturnCodegen::genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genOperandsIntoCA(lhs, rhs);  // c = value, a = count
    if (op == "<<") {
        code_ << "    shl c, a\n";
    } else if (lhs.type.isUnsigned) {
        code_ << "    shr c, a\n";
    } else {
        code_ << "    sar c, a\n";
    }
    code_ << "    mov a, c\n";
    narrowA(lhs.type);
}

// Unsigned 32-bit multiply via the shared `__mul32` runtime routine (shift-add loop - real
// Saturn hardware has no multiply instruction). The low 32 bits of a truncated product are the
// same bit pattern whether the operands are signed or unsigned, so one routine serves both.
void SaturnCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genOperandsIntoCA(lhs, rhs);
    code_ << "    st [__mul_a], c\n";
    code_ << "    st [__mul_b], a\n";
    code_ << "    gosbvl __mul32\n";
}

// Signed division/modulo reduces to the unsigned routine by negating any negative operand first
// and fixing the result's sign back up afterward - the same idea as pdp7's own callDivMod(),
// adapted since this backend has real registers to track the sign flags in rather than needing a
// second runtime routine.
void SaturnCodegen::callDivMod(bool isUnsigned, bool, bool) {
    if (isUnsigned) {
        code_ << "    gosbvl __udivmod32\n";
        return;
    }
    // `c` = dividend, `a` = divisor on entry (see genDivMod()). Track each operand's sign, negate
    // if negative, divide unsigned, then restore the quotient's sign (XOR of the two original
    // signs) and the remainder's sign (always the dividend's own sign, per C's truncating
    // division).
    std::string Ldvdpos = newLabel(), Ldvddone = newLabel();
    std::string Ldvspos = newLabel(), Ldvsdone = newLabel();
    // `__udivmod32` reads its operands back from `__dm_a`/`__dm_b` (not from `c`/`a`
    // directly), so a negated register has to be written back to its own memory cell too -
    // leaving the stale original (possibly negative) value there would feed the unsigned
    // routine a negative dividend/divisor bit pattern instead of its corrected magnitude.
    code_ << "    st [__dm_dvdneg], 0\n";
    code_ << "    cmp c, 0\n    jge " << Ldvdpos << "\n";
    code_ << "    neg c\n    st [__dm_a], c\n    st [__dm_dvdneg], 1\n";
    code_ << Ldvdpos << ":\n";
    code_ << "    st [__dm_dvsneg], 0\n";
    code_ << "    cmp a, 0\n    jge " << Ldvspos << "\n";
    code_ << "    neg a\n    st [__dm_b], a\n    st [__dm_dvsneg], 1\n";
    code_ << Ldvspos << ":\n";
    code_ << "    gosbvl __udivmod32\n";  // quotient in a, remainder in __dm_a
    // Quotient's sign: dividend-sign XOR divisor-sign.
    std::string Lqpos = newLabel(), Lqdone = newLabel();
    code_ << "    mov c, a\n";  // c = unsigned quotient (a is about to be clobbered by loads)
    code_ << "    ld a, [__dm_dvdneg]\n    mov b, a\n";
    code_ << "    ld a, [__dm_dvsneg]\n    xor b, a\n";
    code_ << "    cmp b, 0\n    je " << Lqpos << "\n";
    code_ << "    neg c\n";
    code_ << Lqpos << ":\n";
    code_ << "    mov a, c\n";
    // Remainder's sign always matches the dividend's original sign.
    code_ << "    ld b, [__dm_dvdneg]\n";
    code_ << "    cmp b, 0\n    je " << Lqdone << "\n";
    code_ << "    ld c, [__dm_a]\n    neg c\n    st [__dm_a], c\n";
    code_ << Lqdone << ":\n";
}

void SaturnCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genOperandsIntoCA(lhs, rhs);  // c = dividend, a = divisor
    bool isUnsigned = lhs.type.isUnsigned || rhs.type.isUnsigned;
    code_ << "    st [__dm_a], c\n";
    code_ << "    st [__dm_b], a\n";
    callDivMod(isUnsigned, false, false);
    if (wantRemainder) code_ << "    ld a, [__dm_a]\n";
    narrowA(lhs.type);
}

// --- Main expression dispatch ------------------------------------------------

void SaturnCodegen::genExpr(const ir::Expr &e) {
    // genExpr()'s contract is "leaves the result in `a`" - a wide result can't fit there, so
    // this computes it via genWideExpr() and collapses to just the low chunk, discarding the
    // rest. A caller that needs the full wide value (LocalDecl's initializer, Return) calls
    // genWideExpr() directly instead.
    if (isWide(e.type)) {
        genWideExpr(e);
        int chunks = chunkCount(e.type);
        code_ << "    pop a\n";
        for (int i = 1; i < chunks; i++) code_ << "    pop c\n";
        return;
    }
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    mov a, " << e.intValue << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            emitLoadSlot(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address
                genExpr(*e.rhs);
                code_ << "    pop c\n    " << stMnemonic(widthOfType(e.slot->type)) << " [c], a\n";
                return;
            }
            genExpr(*e.rhs);
            emitStoreSlot(e.slot);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg a\n";
            } else if (e.op == "!") {
                genExpr(*e.rhs);
                std::string Ltrue = newLabel(), Ldone = newLabel();
                code_ << "    cmp a, 0\n    je " << Ltrue << "\n    mov a, 0\n    goto " << Ldone << "\n";
                code_ << Ltrue << ":\n    mov a, 1\n" << Ldone << ":\n";
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // a = pointer value
                code_ << "    " << ldMnemonic(widthOfType(e.type)) << " a, [a]\n";
                narrowA(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    not a\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // a = pointer value
            code_ << "    push a\n";
            genExpr(*e.rhs);
            code_ << "    pop c\n    " << stMnemonic(widthOfType(e.type)) << " [c], a\n";
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            genCompoundScalar(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // a = pointer value
            code_ << "    push a\n";
            genCompoundThroughAddress(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            std::string Lelse = newLabel(), Ldone = newLabel();
            genCondBranch(*e.lhs, Lelse);
            genExpr(*e.rhs);
            code_ << "    goto " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Call: {
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    genWideExpr(*e.args[i]);
                    totalBytes += widthOfType(e.args[i]->type);  // already real bytes, 4/chunk
                } else {
                    genExpr(*e.args[i]);
                    code_ << "    push a\n";
                    totalBytes += 4;
                }
            }
            code_ << "    gosbvl " << label(e.func) << "\n";
            if (totalBytes > 0) code_ << "    add d0, " << totalBytes << "\n";
            narrowA(e.type);
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);  // a = callee's entry address
            code_ << "    gosbvlr a\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real Saturn interrupt-enable/disable has no analogue in this freestanding,
            // simplified model; this is a harmless no-op, the same convention most other
            // backends (e.g. mips) use for this IR node.
            code_ << "    ; " << e.op << " (no-op on this backend)\n";
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

void SaturnCodegen::genConvert(const ir::Expr &e) {
    if (isWide(e.lhs->type)) {
        genWideExpr(*e.lhs);
        int chunks = chunkCount(e.lhs->type);
        code_ << "    pop a\n";  // the low chunk - this is the value being kept
        for (int i = 1; i < chunks; i++) code_ << "    pop c\n";
        narrowA(e.type);
        return;
    }
    genExpr(*e.lhs);
    narrowA(e.type);
}

// --- Compound assignment ('+=' etc., prefix/postfix '++'/'--') -------------------------------

void SaturnCodegen::combineInPlace(const ir::Expr &e, const std::string &dst, const std::string &rhsReg) {
    const std::string &op = e.op;
    if (op == "+") {
        code_ << "    add " << dst << ", " << rhsReg << "\n";
    } else if (op == "-") {
        code_ << "    sub " << dst << ", " << rhsReg << "\n";
    } else if (op == "&") {
        code_ << "    and " << dst << ", " << rhsReg << "\n";
    } else if (op == "|") {
        code_ << "    or " << dst << ", " << rhsReg << "\n";
    } else if (op == "^") {
        code_ << "    xor " << dst << ", " << rhsReg << "\n";
    } else if (op == "<<") {
        code_ << "    shl " << dst << ", " << rhsReg << "\n";
    } else if (op == ">>") {
        code_ << (e.type.isUnsigned ? "    shr " : "    sar ") << dst << ", " << rhsReg << "\n";
    } else if (op == "*") {
        code_ << "    st [__mul_a], " << dst << "\n";
        code_ << "    st [__mul_b], " << rhsReg << "\n";
        code_ << "    gosbvl __mul32\n";
        if (dst != "a") code_ << "    mov " << dst << ", a\n";
    } else if (op == "/" || op == "%") {
        bool isUnsigned = e.type.isUnsigned;
        code_ << "    st [__dm_a], " << dst << "\n";
        code_ << "    st [__dm_b], " << rhsReg << "\n";
        if (dst != "c") code_ << "    mov c, " << dst << "\n";
        if (rhsReg != "a") code_ << "    ld a, [__dm_b]\n";
        callDivMod(isUnsigned, false, false);
        if (op == "%") code_ << "    ld a, [__dm_a]\n";
        if (dst != "a") code_ << "    mov " << dst << ", a\n";
    } else {
        err(e.line, "internal: unhandled compound-assignment operator '" + op + "'");
    }
}

void SaturnCodegen::genCompoundScalar(const ir::Expr &e) {
    if (e.lhs) {  // slot[idx] (op)= rhs
        computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address
        genCompoundThroughAddress(e);
        return;
    }
    // Plain scalar target - its own address (a Global's label, or a Local/Param's `d1`-relative
    // offset) is cheap enough to recompute from scratch after rhs's own evaluation via
    // emitLoadSlot()/emitStoreSlot(), neither of which ever touches `c`, so `c` safely holds
    // rhs across the `emitLoadSlot()` call below with nothing needing the stack.
    genExpr(*e.rhs);
    code_ << "    mov c, a\n";  // c = rhs
    emitLoadSlot(e.slot);       // a = old value
    if (e.yieldOld) code_ << "    push a\n";
    code_ << "    mov b, a\n";  // b = old value (dst for combineInPlace)
    combineInPlace(e, "b", "c");
    code_ << "    mov a, b\n";
    narrowA(e.type);
    emitStoreSlot(e.slot);  // leaves `a` (the new value) unchanged
    if (e.yieldOld) code_ << "    pop a\n";
}

// The target address must already be pushed on the stack by the caller, before e.rhs is
// evaluated, so it survives whatever e.rhs itself does to `b`/`c` - not just a nested call, but
// also a nested div/mod/multiply. It's then stashed in a named cell, not a register: `combine
// InPlace()`'s own divide/multiply paths (via the `__udivmod32`/`__mul32` runtime routines)
// freely clobber `b`/`c` as scratch, same as a nested call would, so the address can't ride in
// either of them across that call.
void SaturnCodegen::genCompoundThroughAddress(const ir::Expr &e) {
    const char *ld = ldMnemonic(widthOfType(e.type));
    const char *st = stMnemonic(widthOfType(e.type));
    std::string addrCell = allocTemp();
    declareTemp(addrCell);
    genExpr(*e.rhs);               // a = rhs
    code_ << "    pop c\n";        // c = address
    code_ << "    st [" << addrCell << "], c\n";
    code_ << "    mov b, a\n";     // b = rhs
    code_ << "    " << ld << " a, [c]\n";  // a = old value
    if (e.yieldOld) code_ << "    push a\n";
    combineInPlace(e, "a", "b");   // result in `a`; may clobber `b`/`c` freely
    narrowA(e.type);
    code_ << "    ld c, [" << addrCell << "]\n";
    code_ << "    " << st << " [c], a\n";
    if (e.yieldOld) code_ << "    pop a\n";
}

// --- `long`/`long long` (wide) support --------------------------------------------------------

void SaturnCodegen::pushWideFromAddress(Type type) {
    code_ << "    mov c, a\n";  // c = base address
    int chunks = chunkCount(type);
    for (int i = chunks - 1; i >= 0; i--) {
        code_ << "    mov a, c\n";
        if (i > 0) code_ << "    add a, " << (i * 4) << "\n";
        code_ << "    ld a, [a]\n";
        code_ << "    push a\n";
    }
}

void SaturnCodegen::popWideToAddress(Type type) {
    code_ << "    mov c, a\n";  // c = base address
    int chunks = chunkCount(type);
    for (int i = 0; i < chunks; i++) {
        code_ << "    pop a\n";
        code_ << "    mov b, c\n";
        if (i > 0) code_ << "    add b, " << (i * 4) << "\n";
        code_ << "    st [b], a\n";
    }
}

void SaturnCodegen::pushWideLiteral(Type type, long value) {
    int chunks = chunkCount(type);
    // Each chunk is a 32-bit slice of `value`'s own two's-complement pattern - an arithmetic
    // right shift sign-extends correctly even past the host `long`'s own width.
    for (int i = chunks - 1; i >= 0; i--) {
        long chunkVal = (value >> (32 * i)) & 0xFFFFFFFFL;
        code_ << "    mov a, " << chunkVal << "\n";
        code_ << "    push a\n";
    }
}

// dest = dest + addend + carryCell (read then overwritten with this chunk's carry-out). Computed
// as two separate half-adds (`dest += addend`, then `dest += carryIn`), each checked for
// unsigned wraparound against its own pre-add value via `cmp`+`jb` (this backend has no
// hardware carry flag to read directly) and OR'd together - a single combined "result < addend"
// test is NOT equivalent: it misses the case where `dest` was already 0xFFFFFFFF and `carryIn`
// alone supplies the second carry (same two-step shape as pdp7's own emitWideAddWithCarry()).
void SaturnCodegen::emitAddWithCarry(const std::string &dest, const std::string &addend,
                                      const std::string &carryCell) {
    std::string prev = allocTemp(), carry1 = allocTemp(), carry2 = allocTemp();
    declareTemp(prev);
    declareTemp(carry1);
    declareTemp(carry2);

    auto halfAdd = [&](const std::string &rhsSrc, const std::string &carryOutCell) {
        code_ << "    st [" << prev << "], " << dest << "\n";
        code_ << "    add " << dest << ", " << rhsSrc << "\n";
        std::string Lset = newLabel(), Ldone = newLabel();
        code_ << "    ld b, [" << prev << "]\n";
        code_ << "    cmp " << dest << ", b\n";
        code_ << "    jb " << Lset << "\n";
        code_ << "    st [" << carryOutCell << "], 0\n    goto " << Ldone << "\n";
        code_ << Lset << ":\n    st [" << carryOutCell << "], 1\n";
        code_ << Ldone << ":\n";
    };
    halfAdd(addend, carry1);
    code_ << "    ld b, [" << carryCell << "]\n";
    halfAdd("b", carry2);
    code_ << "    ld a, [" << carry1 << "]\n";
    code_ << "    ld b, [" << carry2 << "]\n";
    code_ << "    or a, b\n";
    code_ << "    st [" << carryCell << "], a\n";
}

void SaturnCodegen::genWideNegate(int chunks) {
    std::vector<std::string> t(chunks);
    for (auto &x : t) {
        x = allocTemp();
        declareTemp(x);
    }
    // Two's-complement negate: complement every chunk, then add 1 with carry rippling upward.
    for (int i = 0; i < chunks; i++) {
        code_ << "    pop a\n    not a\n    st [" << t[i] << "], a\n";
    }
    std::string carryCell = allocTemp();
    declareTemp(carryCell);
    code_ << "    st [" << carryCell << "], 1\n";  // the "+1" of two's-complement negation
    for (int i = 0; i < chunks; i++) {
        code_ << "    ld c, [" << t[i] << "]\n";
        code_ << "    mov a, 0\n";
        emitAddWithCarry("c", "a", carryCell);
        code_ << "    st [" << t[i] << "], c\n";
    }
    for (int i = chunks - 1; i >= 0; i--) {
        code_ << "    ld a, [" << t[i] << "]\n    push a\n";
    }
}

void SaturnCodegen::genWideAddSub(bool isAdd, int chunks) {
    std::vector<std::string> rhsT(chunks), lhsT(chunks);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    // genWideExpr() pushes chunk 0 (low) last, so it's on top: the FIRST pop must land in
    // index 0, not chunks-1.
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << rhsT[i] << "], a\n";
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << lhsT[i] << "], a\n";

    std::string carryCell = allocTemp();
    declareTemp(carryCell);
    if (!isAdd) {
        // a - b = a + (~b + 1): negate rhsT in place first via the same ripple-carry machinery.
        code_ << "    st [" << carryCell << "], 1\n";
        for (int i = 0; i < chunks; i++) {
            code_ << "    ld c, [" << rhsT[i] << "]\n    not c\n    mov a, 0\n";
            emitAddWithCarry("c", "a", carryCell);
            code_ << "    st [" << rhsT[i] << "], c\n";
        }
    }
    code_ << "    st [" << carryCell << "], 0\n";
    for (int i = 0; i < chunks; i++) {
        code_ << "    ld c, [" << lhsT[i] << "]\n";
        code_ << "    ld a, [" << rhsT[i] << "]\n";
        emitAddWithCarry("c", "a", carryCell);
        code_ << "    st [" << lhsT[i] << "], c\n";
    }
    for (int i = chunks - 1; i >= 0; i--) code_ << "    ld a, [" << lhsT[i] << "]\n    push a\n";
}

// Each chunk is independent - no carry chain needed.
void SaturnCodegen::genWideBitwise(const std::string &op, int chunks) {
    std::vector<std::string> rhsT(chunks), lhsT(chunks);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << rhsT[i] << "], a\n";
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << lhsT[i] << "], a\n";
    for (int i = chunks - 1; i >= 0; i--) {
        code_ << "    ld c, [" << lhsT[i] << "]\n";
        code_ << "    ld a, [" << rhsT[i] << "]\n";
        code_ << "    " << (op == "&" ? "and" : op == "|" ? "or" : "xor") << " c, a\n";
        code_ << "    mov a, c\n    push a\n";
    }
}

void SaturnCodegen::genWideCompare(const std::string &op, int chunks, bool isUnsigned) {
    std::vector<std::string> rhsT(chunks), lhsT(chunks);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << rhsT[i] << "], a\n";
    for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << lhsT[i] << "], a\n";

    if (op == "==" || op == "!=") {
        std::string Ldiffer = newLabel(), Ldone = newLabel();
        for (int i = chunks - 1; i >= 0; i--) {
            code_ << "    ld c, [" << lhsT[i] << "]\n    ld a, [" << rhsT[i] << "]\n";
            code_ << "    cmp c, a\n    jne " << Ldiffer << "\n";
        }
        code_ << "    mov a, " << (op == "==" ? 1 : 0) << "\n    goto " << Ldone << "\n";
        code_ << Ldiffer << ":\n    mov a, " << (op == "==" ? 0 : 1) << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    // Compare from the most-significant chunk down, short-circuiting once a chunk differs; only
    // the top chunk's compare is signed (when `!isUnsigned`) - lower chunks are always unsigned
    // magnitude once the higher chunks are known equal.
    std::string Llt = newLabel(), Lge = newLabel(), Ldone = newLabel();
    for (int i = chunks - 1; i >= 0; i--) {
        bool chunkUnsigned = (i == chunks - 1) ? isUnsigned : true;
        code_ << "    ld c, [" << lhsT[i] << "]\n    ld a, [" << rhsT[i] << "]\n";
        code_ << "    cmp c, a\n";
        code_ << "    " << jccForTrue("<", chunkUnsigned) << " " << Llt << "\n";
        if (i > 0) {
            // Equal so far: fall through to the next (lower) chunk's comparison. Only a
            // genuine difference (greater, here) short-circuits early.
            code_ << "    " << jccForTrue(">", chunkUnsigned) << " " << Lge << "\n";
        } else {
            code_ << "    goto " << Lge << "\n";
        }
    }
    code_ << Llt << ":\n";
    code_ << "    mov a, " << ((op == "<" || op == "<=") ? 1 : 0) << "\n    goto " << Ldone << "\n";
    code_ << Lge << ":\n";
    code_ << "    mov a, " << ((op == "<" || op == "<=") ? 0 : 1) << "\n";
    code_ << Ldone << ":\n";
}

// Multiply and divide/modulo are not implemented for a wide operand: a genuine multi-chunk
// multiply/divide needs the ripple-carry machinery above nested inside its own shift-and-add/
// compare-shift-subtract loop, and this backend's software `__mul32`/`__udivmod32` routines are
// only verified for a single 32-bit chunk - the same scope decision pdp7 makes (see
// Pdp7Codegen.cpp's own genWideMulDivErr()).
void SaturnCodegen::genWideMulDivErr(int line, const std::string &opName) {
    err(line, "'" + opName + "' on a 'long'/'long long' operand isn't supported on this backend "
                              "(no verified multi-chunk multiply/divide routine - see "
                              "SaturnCodegen.cpp's own genWideMulDivErr() comment)");
}

void SaturnCodegen::genWideExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            pushWideLiteral(e.type, e.intValue);
            return;
        }
        case ir::ExprKind::Var: {
            emitLoadSlotAddress(e.slot);
            pushWideFromAddress(e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            computeElementAddressA(e.slot, *e.lhs);
            pushWideFromAddress(e.type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            int chunks = chunkCount(e.type);
            genExpr(*e.lhs);  // a = pointer value
            code_ << "    push a\n";
            genWideExpr(*e.rhs);
            std::vector<std::string> temps(chunks);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << temps[i] << "], a\n";
            code_ << "    pop a\n";  // a = the pointer's own address
            code_ << "    mov c, a\n";
            for (int i = 0; i < chunks; i++) {
                code_ << "    ld a, [" << temps[i] << "]\n";
                code_ << "    mov b, c\n";
                if (i > 0) code_ << "    add b, " << (i * 4) << "\n";
                code_ << "    st [b], a\n";
            }
            for (int i = chunks - 1; i >= 0; i--) code_ << "    ld a, [" << temps[i] << "]\n    push a\n";
            return;
        }
        case ir::ExprKind::Assign: {
            int chunks = chunkCount(e.slot->type);
            std::vector<std::string> temps(chunks);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            if (e.lhs) {  // slot[idx] = rhs, a wide array element
                computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address
                genWideExpr(*e.rhs);
                for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << temps[i] << "], a\n";
                code_ << "    pop a\n";  // a = the element's own address
                code_ << "    mov c, a\n";
            } else {
                genWideExpr(*e.rhs);
                for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << temps[i] << "], a\n";
                emitLoadSlotAddress(e.slot);
                code_ << "    mov c, a\n";
            }
            for (int i = 0; i < chunks; i++) {
                code_ << "    ld a, [" << temps[i] << "]\n";
                code_ << "    mov b, c\n";
                if (i > 0) code_ << "    add b, " << (i * 4) << "\n";
                code_ << "    st [b], a\n";
            }
            for (int i = chunks - 1; i >= 0; i--) code_ << "    ld a, [" << temps[i] << "]\n    push a\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genWideExpr(*e.rhs);
                genWideNegate(chunkCount(e.type));
                return;
            }
            if (e.op == "*") {
                genExpr(*e.rhs);  // a = pointer value
                pushWideFromAddress(e.type);
                return;
            }
            if (e.op == "~") {
                int chunks = chunkCount(e.type);
                genWideExpr(*e.rhs);
                std::vector<std::string> t(chunks);
                for (auto &x : t) {
                    x = allocTemp();
                    declareTemp(x);
                }
                for (int i = 0; i < chunks; i++) code_ << "    pop a\n    not a\n    st [" << t[i] << "], a\n";
                for (int i = chunks - 1; i >= 0; i--) code_ << "    ld a, [" << t[i] << "]\n    push a\n";
                return;
            }
            err(e.line, "internal: unary '" + e.op + "' on a wide ('long'/'long long') value isn't "
                                                       "supported on this backend");
        }
        case ir::ExprKind::Convert: {
            Type from = e.lhs->type;
            int toChunks = chunkCount(e.type);
            if (!isWide(from)) {
                genExpr(*e.lhs);
                std::string Tlo = allocTemp();
                declareTemp(Tlo);
                code_ << "    st [" << Tlo << "], a\n";
                std::string Text = allocTemp();
                declareTemp(Text);
                if (from.isUnsigned) {
                    code_ << "    st [" << Text << "], 0\n";
                } else {
                    std::string Lneg = newLabel(), Ldone = newLabel();
                    code_ << "    cmp a, 0\n    jl " << Lneg << "\n";
                    code_ << "    st [" << Text << "], 0\n    goto " << Ldone << "\n";
                    code_ << Lneg << ":\n    st [" << Text << "], 0xFFFFFFFF\n";
                    code_ << Ldone << ":\n";
                }
                for (int i = 0; i < toChunks - 1; i++) code_ << "    ld a, [" << Text << "]\n    push a\n";
                code_ << "    ld a, [" << Tlo << "]\n    push a\n";
                return;
            }
            int fromChunks = chunkCount(from);
            genWideExpr(*e.lhs);
            std::vector<std::string> temps(fromChunks);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (int i = 0; i < fromChunks; i++) code_ << "    pop a\n    st [" << temps[i] << "], a\n";
            if (toChunks <= fromChunks) {
                for (int i = toChunks - 1; i >= 0; i--) code_ << "    ld a, [" << temps[i] << "]\n    push a\n";
                return;
            }
            std::string Text = allocTemp();
            declareTemp(Text);
            if (from.isUnsigned) {
                code_ << "    st [" << Text << "], 0\n";
            } else {
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ld a, [" << temps[fromChunks - 1] << "]\n    cmp a, 0\n    jl " << Lneg << "\n";
                code_ << "    st [" << Text << "], 0\n    goto " << Ldone << "\n";
                code_ << Lneg << ":\n    st [" << Text << "], 0xFFFFFFFF\n";
                code_ << Ldone << ":\n";
            }
            for (int i = 0; i < toChunks - fromChunks; i++) code_ << "    ld a, [" << Text << "]\n    push a\n";
            for (int i = fromChunks - 1; i >= 0; i--) code_ << "    ld a, [" << temps[i] << "]\n    push a\n";
            return;
        }
        case ir::ExprKind::Call: {
            // Same argument-passing convention as genExpr()'s Call case - see
            // emitEpilogueAndReturn()'s own comment for why a wide result needs the args
            // popped first, then discarded, before the saved chunks go back on the stack.
            int totalBytes = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    genWideExpr(*e.args[i]);
                    totalBytes += widthOfType(e.args[i]->type);  // already real bytes, 4/chunk
                } else {
                    genExpr(*e.args[i]);
                    code_ << "    push a\n";
                    totalBytes += 4;
                }
            }
            code_ << "    gosbvl " << label(e.func) << "\n";
            int chunks = chunkCount(e.type);
            std::vector<std::string> retTemps(chunks);
            for (auto &t : retTemps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (int i = 0; i < chunks; i++) code_ << "    pop a\n    st [" << retTemps[i] << "], a\n";
            if (totalBytes > 0) code_ << "    add d0, " << totalBytes << "\n";
            for (int i = chunks - 1; i >= 0; i--) code_ << "    ld a, [" << retTemps[i] << "]\n    push a\n";
            return;
        }
        case ir::ExprKind::Binary: {
            const std::string &op = e.op;
            if (op == "+" || op == "-") {
                genWideExpr(*e.lhs);
                genWideExpr(*e.rhs);
                genWideAddSub(op == "+", chunkCount(e.type));
                return;
            }
            if (op == "&" || op == "|" || op == "^") {
                genWideExpr(*e.lhs);
                genWideExpr(*e.rhs);
                genWideBitwise(op, chunkCount(e.type));
                return;
            }
            if (op == "*") genWideMulDivErr(e.line, "*");
            if (op == "/") genWideMulDivErr(e.line, "/");
            if (op == "%") genWideMulDivErr(e.line, "%");
            err(e.line, "internal: binary '" + op + "' on a wide ('long'/'long long') value isn't "
                                                      "supported on this backend");
        }
        default:
            err(e.line, "internal: this expression kind can't produce a wide ('long'/'long long') "
                          "value on this backend");
    }
}

// --- Shared runtime: software multiply/divide ------------------------------------------------
//
// Real Saturn hardware has neither a multiply nor a divide instruction, so both are software
// routines here, called via `gosbvl`/returned from via `rtn` exactly like any C function - see
// SaturnCodegen.h's own header comment for the calling convention they share with everything
// else this backend generates.
std::string saturnRuntimeAsm() {
    return R"ASM(
__mul_a: dw 0
__mul_b: dw 0
__dm_a: dw 0
__dm_b: dw 0
__dm_dvdneg: dw 0
__dm_dvsneg: dw 0

; a = __mul_a * __mul_b (unsigned 32-bit multiply, truncated to the low 32 bits - the same bit
; pattern as a signed multiply's low word, so one routine serves both). Shift-and-add, LSB
; first: `c` is the shifting multiplier (consumed as it's tested), `a` the shifting
; multiplicand, `b` the accumulating result; `__mul_t` stashes `c` across the one-bit test
; below, since this ISA's `and`/`shr` only take a register or immediate operand, never a
; memory one.
__mul_t: dw 0
__mul32:
    ld  a, [__mul_a]
    ld  c, [__mul_b]
    mov b, 0
__mul32_loop:
    cmp c, 0
    je __mul32_done
    st  [__mul_t], c
    and c, 1
    cmp c, 0
    je __mul32_skip
    add b, a
__mul32_skip:
    ld  c, [__mul_t]
    shr c, 1
    shl a, 1
    goto __mul32_loop
__mul32_done:
    mov a, b
    rtn

; a = __dm_a / __dm_b (unsigned), remainder left in __dm_a. Restoring binary long division: `a`
; is the shifting dividend, `c` the shifting remainder, `b` the shifting quotient, `d1` (saved/
; restored - a leaf routine, so repurposing it is safe) the iteration counter. The divisor is
; reloaded from memory each iteration, held transiently in `a` itself (stashed in `__udm_shifted`
; while that happens) since there's no spare register for a fourth live value.
__udm_shifted: dw 0
__udivmod32:
    push d1
    ld  a, [__dm_a]
    mov b, 0
    mov c, 0
    mov d1, 32
__udivmod32_loop:
    cmp d1, 0
    je __udivmod32_done
    dec d1
    cmp a, 0
    jl __udivmod32_bitset
    shl a, 1
    shl c, 1
    goto __udivmod32_shifted_lbl
__udivmod32_bitset:
    shl a, 1
    shl c, 1
    or  c, 1
__udivmod32_shifted_lbl:
    shl b, 1
    st  [__udm_shifted], a
    ld  a, [__dm_b]
    cmp c, a
    jb __udivmod32_noSub
    sub c, a
    or  b, 1
__udivmod32_noSub:
    ld  a, [__udm_shifted]
    goto __udivmod32_loop
__udivmod32_done:
    pop d1
    st  [__dm_a], c
    mov a, b
    rtn
)ASM";
}

}  // namespace rcc
