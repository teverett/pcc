#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates PDP-1 assembly for the IR built by IRBuilder. The PDP-1 (DEC's
// 1959/63 machine) is a genuinely different animal from this project's other
// 18-bit DEC target (pdp7): it's one's-complement, has no link/carry bit and
// no hardware multiply/divide in its base instruction set, and its only
// subroutine-call instruction (`jsp`) leaves the return address in AC
// instead of auto-storing it the way pdp7's `jms` does. None of pdp7's
// carry-chain or EAE-based code ports mechanically, so this backend is its
// own design, scoped to what a one's-complement machine with no link bit can
// do safely and verifiably:
//
//  - `char`/`short`/`int` are fully supported (arithmetic, logic, shifts,
//    comparisons, control flow, functions/recursion, globals/locals/
//    arrays/pointers).
//  - `long`/`long long` are NOT supported: a correct multi-word carry chain
//    on a one's-complement machine with no link bit would need its own
//    from-scratch algorithm (the classic ones'-complement "end-around
//    carry across words" problem), unverified here - see genWideUnsupported().
//    This is the same kind of documented hardware-shaped limitation as
//    pdp7's own `genWideMulDivErr()` for multiply/divide, just drawn one
//    level broader.
//  - `*`/`/`/`%` are implemented in software (sign-magnitude shift-add /
//    shift-subtract loops - see genMul()/genDivMod()), since this target
//    has no hardware multiply/divide to call.
//  - Every arithmetic routine that can produce the representable "negative
//    zero" (the all-ones bit pattern, mathematically equal to +0 on a
//    one's-complement machine) normalizes it immediately - see
//    normalizeNegZero() - so no live value in this backend's own generated
//    code is ever left holding that pattern; AC_, slot/temp cells, and
//    compile-time literals (via onesComplementWord()) are all kept in
//    normal form.
//  - `unsigned` is approximated on top of the same one's-complement ALU:
//    correct for any value whose sign bit (bit 17) is clear under this
//    target's representation, i.e. up to 2^17-1, same practical ceiling a
//    one's-complement machine's own "unsigned" idiom has historically had.
//    A C program relying on the full 0..2^18-1 `unsigned int` range wrapping
//    exactly isn't guaranteed to on this target - documented here rather
//    than silently assumed.
//  - Binary `&`/`|`/`^` (see genBitwise()) are likewise only verified for
//    non-negative operands: they run directly on AC's raw one's-complement
//    bits, which only match a value's own two's-complement bit pattern
//    (what C's bitwise operators are specified against) when the sign bit
//    is clear. Unary `~` has no such gap - it's implemented as the pure
//    arithmetic identity `-x-1` (see genExpr()'s Unary case), which is
//    representation-independent and correct for any operand.
//

// Every function gets a per-call activation record on a software stack
// built from `__sp`/`__fp`, exactly like pdp7's - this target has no
// hardware stack pointer either. The one structural difference: `jsp`
// doesn't auto-store a return address anywhere, so the very first thing a
// function does (framed or leaf) is capture AC (which `jsp` just set to the
// return address) into its own return-address cell, before anything else
// can clobber AC. See emitPrologue()/emitEpilogueAndReturn() and
// emitLeafParamBinding()/emitLeafReturn().
//
// Labels: "G_<name>" for a Global Slot, "FN_<name>" for an ir::Function, and
// "T<N>"/"N<N>"/"J<N>" for codegen-internal scratch cells, literal constants
// and jump targets, plus a fixed set of "__"-prefixed global cells (`__sp`,
// `__fp`, `__stmp`, `__stmp2`, `__addr`, `__retval`, `__retaddr`) shared by
// every function, same convention as pdp7.
class Pdp1Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp1"; }
    TypeWidths typeWidths() const override { return {1, 1, 1, 2, 4, 18}; }
    std::string sourceExtension() const override { return ".s"; }
    std::vector<std::string> assembledFormats() const override { return {"list", "rim"}; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in AC

    [[noreturn]] void genWideUnsupported(int line, const std::string &what);
    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return !isPointer(t) && (t.base == BaseType::Long || t.base == BaseType::LongLong); }

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitEq(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    void emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    void genCondBranch(const ir::Expr &cond, const std::string &Lfalse);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);

    // Masks AC down to `to`'s value range and, for a signed type,
    // re-sign-extends from that range's top bit. `char`/`short`/`int` are
    // each one pdp1 word, so narrowing only ever matters for a `char`.
    void narrowAC(Type to);

    // Emits AC += v (v >= 0) or AC -= (-v) (v < 0) using this target's
    // native `add`/`sub` and a positive-only literal cell either way - see
    // the class comment on why this backend never needs a negative-valued
    // literal cell for compile-time offset arithmetic.
    void addConst(long v);
    // AC = ~AC (one's-complement negate), then normalizeNegZero().
    void negateAC();
    // If AC holds the all-ones "negative zero" bit pattern, clears it to
    // all-zeros - see the class comment. Safe (a no-op) on any other value.
    void normalizeNegZero();

    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void callDivMod(bool isUnsigned);
    void ensureRuntimeRoutines();
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    // AC = AC shifted by `count` (>=0), one bit at a time via emitShiftOne() -
    // see that method's comment for why this backend never uses the
    // hardware shift group's own multi-bit count field.
    void emitConstantShift(const std::string &op, long count, bool isUnsigned);
    // One bit of `<<`/unsigned `>>`/signed `>>`, on the value already in AC.
    // `<<`: rotate left (`ral`) then mask bit 0 back to 0 - the old sign bit
    // rotates into bit 0 and is immediately discarded, giving a plain
    // bit-pattern shift with no dependence on one's-complement add/subtract
    // semantics. Unsigned `>>`: rotate right (`rar`) then mask bit 17 back
    // to 0, same idea. Signed `>>`: `rar` moves the wrong bit (the old bit 0)
    // into bit 17, so the sign actually in AC on entry is saved first (via
    // `sma`) and then force-written back into bit 17 with `ior`/`and` after
    // the rotate - see the .cpp for the exact sequence.
    void emitShiftOne(const std::string &op, bool isUnsigned);

    enum class RhsXform { None, Negate, Complement };
    std::string genOperandPair(const ir::Expr &lhs, const ir::Expr &rhs, RhsXform xform,
                                const std::string &dest = "");
    std::optional<std::string> simpleCell(const ir::Expr &e);
    static bool isCallFree(const ir::Expr &e);
    void computeElementAddressAC(const ir::Slot *slot, const ir::Expr &idxExpr);
    void genIndex(const ir::Expr &e);
    void genAddrOf(const ir::Expr &operand);
    void genConvert(const ir::Expr &e);

    std::string prepareCompoundRhs(const ir::Expr &e);
    void finishCompoundOp(const ir::Expr &e, const std::string &rhsCell);
    void genCompoundThroughStackedAddress(const ir::Expr &e);
    void genCompoundThroughCell(const ir::Expr &e);

    static std::string label(const ir::Slot *slot) { return "G_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "FN_" + fn->sourceName; }

    std::string allocTemp();
    void declareTemp(const std::string &label);
    void declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string literalLabel(long value);
    std::string newLabel();
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    // --- Software stack / frame mechanics --------------------------------
    //
    // Same `__sp`/`__fp` convention as pdp7, inlined rather than shared
    // subroutines (see the .cpp's pushAC()/popAC()): this target's `jsp`
    // has no auto-store step of its own to reuse the way pdp7's `jms`-based
    // `__push`/`__pop` subroutines do, and push/pop are small enough that
    // inlining them avoids needing a second, nested return-address
    // convention just to call them.
    void pushAC();
    void popAC();

    void emitPrologue(const ir::Function *fn);
    void emitEpilogueAndReturn(bool hasValue);

    bool currentFunctionIsLeaf_ = false;
    static std::string leafLabel(const ir::Slot *slot) { return "LF" + std::to_string(slot->id); }
    static bool computeIsLeaf(const ir::Function *fn, const ir::Stmt *body);
    void emitLeafParamBinding(const ir::Function *fn);
    void emitLeafReturn(bool hasValue);
    // This function's own fixed return-address cell: captured once at
    // entry (see the class comment), read back by emitLeafReturn() or
    // emitEpilogueAndReturn(). Named off curFunctionLabel_ so it's globally
    // unique without its own counter.
    std::string retAddrCell() const { return curFunctionLabel_ + "_ra"; }

    void emitLoadSlotAddress(const ir::Slot *slot);
    void emitLoadSlot(const ir::Slot *slot);
    void emitStoreSlot(const ir::Slot *slot);
    void emitSlotBaseAddress(const ir::Slot *slot);
    void emitLocalArrayInit(const ir::Slot *slot);
    long localBaseOffset(const ir::Slot *slot);
    void storeThroughStackedAddress();
    std::string popAddressIntoScratch();
    void computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr);

    std::ostringstream code_;
    std::vector<std::string> trailer_;
    std::map<std::string, std::string> perFileAsm_;

    std::unordered_map<long, std::string> literalCache_;
    int tempCounter_ = 0;
    int litCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    bool needDivMod_ = false;
    bool needUDivMod_ = false;
    bool needMul_ = false;

    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;
};

// The pdp1 runtime every whole-program compile shares: the scratch/stack
// cells and bit-mask constants, plus whichever of the signed/unsigned
// divide routines are asked for.
std::string pdp1RuntimeAsm(bool divMod, bool uDivMod, bool mul);

}  // namespace rcc
