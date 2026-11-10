#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Saturn-flavored assembly for the IR built by IRBuilder; names are already resolved to
// Slot/Function, so this is pure instruction selection with no symbol table of its own.
//
// This backend is a deliberately simplified, byte-addressed model of the HP Saturn (the 4-bit/
// nibble-oriented CPU in the HP48/HP49 calculator series), not a real nibble-accurate one - see
// SaturnAssembler.h's own header comment. It has five registers: `a`/`c`/`b` (general-purpose,
// real Saturn register letters) and `d0`/`d1` (real Saturn address registers), used here as this
// backend's stack pointer and frame pointer. Every function gets a per-call activation record on
// that stack (`d0`), built the same shape as pdp7's own software frame (see Pdp7Codegen.h): `d1`
// points at the word holding the caller's saved `d1` ("oldFP"), the return address (pushed by
// `gosbvl` itself) sits one word above that, and the caller's pushed arguments sit above that in
// reverse push order, so param 0 is closest to `d1`. Unlike pdp7, there's no leaf fast path and no
// manual sign-bit branch fusion - real `cmp`/`jcc` and real `push`/`pop` instructions make both
// unnecessary; see IR.h's `localLowOffset()`/`frameOffset` for how a Local/Param's address is
// computed either way.
//
// `genExpr()` leaves its result in register `a` (this backend's accumulator-by-convention, not a
// hardware distinction). Multiply and divide/modulo have no real Saturn hardware op, so both are
// software routines in `saturnRuntimeAsm()`, called via `gosbvl`.
class SaturnCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "saturn"; }
    // Real byte widths - this backend's registers/addresses are plain byte addresses (see
    // SaturnAssembler.h), so TypeWidths' unit must be a real byte: IRBuilder pre-scales every
    // pointer-arithmetic step and frame offset by `widthOf()` in this same unit (see IR.h), and
    // a mismatch between that unit and the real addressing granularity would silently corrupt
    // every pointer and array access. `char`/`short` get their own narrower `ldb`/`ldh`/`stb`/
    // `sth` access (see emitLoadSlot()/emitStoreSlot()); `int`/pointers use the plain 4-byte
    // `ld`/`st`; `long`/`long long` are handled in 4-byte chunks (see chunkCount()).
    TypeWidths typeWidths() const override { return {1, 2, 4, 8, 16, 8, 4}; }
    std::string sourceExtension() const override { return ".s"; }
    std::vector<std::string> assembledFormats() const override { return {"raw", "coff"}; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in register `a`

    void genBinary(const ir::Expr &e);
    void genCompareValue(const std::string &op, bool isUnsigned);  // after lhs in c, rhs in a: a = (c op a) ? 1 : 0
    // Branch fusion for an `If`/`While`/`&&`/`||` condition tree: `genCondBranch` jumps to
    // `label` when `cond` is false (falling through otherwise); `genCondBranchTrue` jumps to
    // `label` when `cond` is true. Both recurse through `&&`/`||`/`!` and fuse a plain relational
    // comparison directly into a `cmp`+`jcc`; anything else evaluates via genExpr() and tests the
    // result against 0.
    void genCondBranch(const ir::Expr &cond, const std::string &label);
    void genCondBranchTrue(const ir::Expr &cond, const std::string &label);
    // The two signed/unsigned condition-mnemonic tables genCondBranch()/genCondBranchTrue()/
    // genCompareValue() all share, keyed by the six relational/equality operator spellings.
    static std::string jccForFalse(const std::string &op, bool isUnsigned);
    static std::string jccForTrue(const std::string &op, bool isUnsigned);

    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void callDivMod(bool isUnsigned, bool lhsMaybeNegative, bool rhsMaybeNegative);
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    // Evaluates `lhs` into register `c` and `rhs` into register `a`, protecting `lhs` across
    // `rhs`'s own evaluation (which may itself call) via the stack when needed. The common
    // entry point for every binary operator's operand evaluation.
    void genOperandsIntoCA(const ir::Expr &lhs, const ir::Expr &rhs);

    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    // --- `long`/`long long` (wide) support --------------------------------
    //
    // A wide value is `chunkCount(type)` consecutive 32-bit chunks, chunk 0 (low) first, kept
    // PUSHED on the stack (register `a` can't hold more than one chunk); every genExpr()/
    // genStmt() call site checks `isWide(e.type)` and branches to the matching wide helper.
    // Multiply and divide/modulo are not implemented for a wide operand - see
    // genWideMulDivErr().
    int chunkCount(Type t) const { return widthOfType(t) / 4; }
    void pushWideFromAddress(Type type);  // address in `a` on entry
    void popWideToAddress(Type type);     // address in `a` on entry
    void pushWideLiteral(Type type, long value);
    void genWideExpr(const ir::Expr &e);
    // Pops two wide operands (lhs then rhs were pushed, so rhs is on top) and pushes a
    // `chunks`-chunk +/- result, carry/borrow propagated via an unsigned-wraparound `cmp`+`jb`
    // test on each 32-bit chunk (this backend has no hardware carry flag to chain through).
    void genWideAddSub(bool isAdd, int chunks);
    void genWideNegate(int chunks);
    // dest = dest + addend + carryCell (read then overwritten with this chunk's carry-out).
    // `dest`/`addend` are register names; uses `b` as scratch throughout and may leave `addend`
    // holding an unrelated value by the time this returns - every call site keeps `dest`/
    // `addend` in `c`/`a` and never reads `addend` again afterward.
    void emitAddWithCarry(const std::string &dest, const std::string &addend, const std::string &carryCell);
    void genWideBitwise(const std::string &op, int chunks);
    void genWideCompare(const std::string &op, int chunks, bool isUnsigned);
    [[noreturn]] void genWideMulDivErr(int line, const std::string &opName);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // Masks register `a` down to `to`'s value range and, for a signed `char`, re-sign-extends
    // from that range's top bit. `short`/`int`/pointers are already full register width, so this
    // is a no-op for them - the only narrower-than-register type this backend ever stores is
    // `char`.
    void narrowA(Type to);

    static std::string label(const ir::Slot *slot) { return "G_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "FN_" + fn->sourceName; }
    std::string newLabel() { return "J" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string n = newLabel();
        irLabels_[id] = n;
        return n;
    }

    // --- Software stack / frame mechanics --------------------------------
    void emitPrologue(const ir::Function *fn);
    void emitEpilogueAndReturn(bool hasValue, Type retType);

    // Leaves `slot`'s address in register `a`: a Global's fixed label address, or a Local/
    // Param's `d1`-relative address.
    void emitLoadSlotAddress(const ir::Slot *slot);
    void emitLoadSlot(const ir::Slot *slot);   // leaves slot's value in `a`
    void emitStoreSlot(const ir::Slot *slot);  // stores `a` into slot, leaving `a` unchanged
    // `a` = the base address to index `slot` from: its own address if it's a declared array, or
    // its value if it's a pointer variable.
    void emitSlotBaseAddress(const ir::Slot *slot);
    void emitLocalArrayInit(const ir::Slot *slot);
    long localBaseOffset(const ir::Slot *slot);
    // Computes &slot[idxExpr] and leaves it pushed on the stack (not a fixed register, so it
    // survives a call during idxExpr's own evaluation).
    void computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr);
    void computeElementAddressA(const ir::Slot *slot, const ir::Expr &idxExpr);  // same, left in `a`

    // --- Compound assignment ('+=' etc., prefix/postfix '++'/'--') and ternary ----------------
    void genCompoundScalar(const ir::Expr &e);
    // The indexed/deref target's address must already be pushed on the stack by the caller
    // before e.rhs is evaluated; combines and stores through it.
    void genCompoundThroughAddress(const ir::Expr &e);
    // `old` (register `c`) combined with rhs (register `a`) per e.op, result left in `c`.
    void combineInPlace(const ir::Expr &e, const std::string &dst, const std::string &rhsReg);

    bool exprIsCallFree(const ir::Expr &e) const;

    // A named scratch memory cell for the wide (`long`/`long long`) chunk-at-a-time algorithms
    // below, which need more live values at once than this backend's three general registers
    // hold.
    std::string allocTemp();
    void declareTemp(const std::string &label);

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw value" data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    std::vector<std::pair<std::string, std::string>> loopLabels_;  // (continueLabel, breakLabel)
    std::map<int, std::string> irLabels_;
};

// The Saturn runtime every whole-program compile shares (or every object leaves external, with
// the linker's entry object supplying the one definition - see SaturnLinkerFactory.cpp): the
// software multiply and unsigned-divide/modulo routines, since real Saturn hardware has neither.
std::string saturnRuntimeAsm();

}  // namespace pcc
