#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates STM8 assembly (this project's own dialect - see STM8Assembler.h)
// for the IR built by IRBuilder. `generate()` returns a complete,
// self-contained flat-binary program loaded at a fixed address, halting in
// an infinite loop; no bootloader or vector table.
//
// The STM8 has an 8-bit accumulator `a` and two 16-bit index registers
// `x`/`y`, not enough to hold every live operand of an arbitrarily deep
// expression tree, so values in flight live in fixed two-byte memory cells
// (`ac`, `bc`/`cx`/`dx`, `ptr`), with `x`/`y` as transit registers. `fp`/`fsp`
// are software frame-pointer/stack-pointer cells, since `x`/`y` are already
// committed elsewhere. Each cell is declared high-byte-first: the STM8 is
// big-endian (`LDW` stores a word's high byte at the lower address).
//
// `x`/`y` support register-register `ADDW`/`SUBW`/`CPW`, so binary `+`/`-`/
// comparisons just save the other operand on the real hardware stack
// (`PUSHW`/`POPW`). There is no register-register `AND`/`OR`/`XOR` on `x`/`y`
// (STM8 bitwise ops only touch `a`), so `&`/`|`/`^` go byte-at-a-time through
// `a` against each cell's direct-addressed byte.
//
// No hardware 16x16 multiply/divide (`MUL`/`DIV` only combine `a` with `x`,
// i.e. 8x8->16 and 16/8->16) - `*`/`/`/`%` use textbook shift-and-add /
// repeated-subtraction loops, verified only against non-negative operands.
// `<<`/`>>` loop one bit at a time via `SLAW`/`SRLW`; there is no
// variable-count shift instruction.
//
// This dialect's conditional jumps (jreq/jrne/jrult/jruge) are deliberately
// full 16-bit absolute, not the real STM8 8-bit signed relative JRxx.
//
// Signed `<`/`<=`/`>`/`>=` flip the top bit of each operand's high byte
// (sign-bit bias trick), then do a plain unsigned CPW+jrult/jruge.
class STM8Codegen : public Backend {
   public:
    static std::string wideScratch(const char *role, int i) { return std::string("w") + role + std::to_string(i); }

    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "stm8"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s8m"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // "raw" is a whole-program flat binary; every other format is a
    // relocatable object file for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

    // Saves the caller's `fp`, points `fp` at the saved word, and reserves
    // `frameSize` words of local storage below it; genEpilogue() reverses
    // this and is duplicated at every return site (no shared epilogue label).
    void genPrologue(const ir::Function &fn);
    void genEpilogue(const ir::Function &fn);

    // Pushes `cell`'s two bytes onto the software frame stack (decrements
    // `fsp` by one word first, then stores at the new `fsp`).
    void pushFrameWord(const std::string &cell);

    // Leaves a Slot's effective storage address in `x`: a Global's fixed
    // label, a Param's `fp + (paramIndex+1)*2`, or a Local's
    // `fp - frameOffset` - locals grow downward from `fp`. A Local array's
    // is its element 0, its lowest address (see ir::localLowOffset()), with
    // element i above it, the same direction as a Global array's.
    void genSlotAddress(const ir::Slot *slot);

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e), leaves truthiness in `ac`; a wide value (words on
    // the hardware stack, LSW on top) is ORed together first and popped,
    // since a multi-word value is true iff any word is nonzero.
    void testTruthy(const ir::Expr &e);
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step; `bc`
    // must already hold `old` and `ac` must already hold `rhs`. Leaves the
    // raw (not yet narrowed) result in `ac`. `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign;
    // `ptr` must already hold the resolved address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows a raw genCompoundOp() result in `ac` to `to`'s width/signedness.
    void narrowAcTo(Type to);

    // Leaves slot[idxExpr]'s effective address in the `ptr` memory cell.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3), the number of
    // one-bit `slaw`s genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Assumes `x` already holds the effective storage address. A `char`
    // only touches one byte (ldxind/stxind), sign/zero-extending the rest
    // of `ac` via genSignFillByte(); every wider type goes through
    // loadAcIndirect()/storeAcIndirect().
    void loadOperand(Type type);
    void storeOperand(Type type);

    // `wideScratch(role,i)` names a word's HIGH byte (big-endian, like
    // every named cell); the LOW byte is `wideScratch(role,i)+"+1"`.
    // Swapping these would corrupt the first adc/sbc of a multi-byte op,
    // since the low byte has no incoming carry to fold in.
    // Pops `words` words off the hardware stack (LSW on top) into `role`'s
    // wideScratch() cells.
    void popWideInto(const char *role, int words);
    // Pushes `role`'s wideScratch() cells onto the hardware stack, LSW on top.
    void pushWideFrom(const char *role, int words);
    // Leaves word `wordIndex`'s (0 = least significant) address in `x`.
    // Storage is most-significant-word-first, so word `wordIndex` sits at
    // an increasing offset as `wordIndex` decreases.
    void genWideWordAddr(const ir::Slot *slot, int words, int wordIndex);
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    // Same layout as genWideWordAddr(), but the base is a runtime pointer
    // value in `addrCell` rather than a Slot's compile-time address.
    void pushWideFromAddress(const std::string &addrCell, int words);
    void popWideToAddress(const std::string &addrCell, int words);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideNegate(int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    void genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words);
    void genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    // long/long long */÷/% need a bit-serial multi-byte routine, not
    // implemented: no simulator exists here to verify one against, so
    // failing loudly is safer than shipping an unverifiable routine.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, int words);
    // Leaves `a` as 0xFF if the already-loaded signed byte in `a` has its
    // top bit set, else 0x00.
    void genSignFillByte();

    // Assumes `x` holds a target address; writes/reads `ac`'s two bytes
    // one at a time via ldxind/stxind with an incw x between them, and
    // does not restore `x` afterward.
    void storeAcIndirect() { storeIndirect("ac"); }
    void loadAcIndirect() { loadIndirect("ac"); }

    // Same as above, generalized to any named 16-bit cell.
    void storeIndirect(const std::string &src);
    void loadIndirect(const std::string &dst);

    void copy16(const std::string &dst, const std::string &src);
    // Shifts the 16-bit memory cell `lbl` left or right by exactly one bit.
    void shiftOneBit(const std::string &lbl, bool isLeft);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    const ir::Function *currentFunction_ = nullptr;
    // One entry per open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
