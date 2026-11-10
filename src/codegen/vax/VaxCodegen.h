#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates VAX-11/780 assembly for the IR built by IRBuilder. `generate()` returns a
// complete, self-contained program loaded at a fixed address (kBaseAddress) with no OS or
// boot ROM, ending in a `halt` instruction.
//
// `int`/`long` are 32 bits, `short` 16, `char` 8 and `long long` 64 - a software-free
// native quadword, kept in a register pair (`r1:r0`, high:low) and in memory as two
// little-endian longwords, low word at the lower address.
//
// The calling convention is the real VAX procedure-call convention. A caller pushes the
// arguments right to left (a quadword argument as one 8-byte push) and runs `calls #n`,
// where n counts the pushed longwords; the callee's 2-byte entry mask is zero, so
// nothing is saved, and `ret` unwinds the frame and pops the arguments. After `calls`:
//   fp  addresses the call frame; Locals live below it, at `fp-frameOffset`, in the
//       space the prologue reserves by lowering `sp`.
//   ap  addresses the argument count longword; argument i is at `4+4*i(ap)`, which is
//       a Param's `frameOffset-4` (IR.h reserves two native ints ahead of the first
//       argument).
// No register is live across a call - every intermediate value is spilled to the stack
// around a nested expression - so the zero entry mask is sufficient. The result comes
// back in `r0` (`r1:r0` for a `long long`).
//
// Register roles:
//   r0  - "ac", genExpr()'s result; also the VAX return-value register.
//   r1  - the other operand in a binary op, or a wide value's high word.
//   r2  - holds a computed address when dereferencing one (array/pointer indexing,
//         `*`, or a lhs quadword's low word popped for a wide operation).
//   r3-r11 - scratch for the wide-integer and unsigned-divide sequences, which each
//         juggle several 32-bit quantities at once.
//
// A Global is addressed with absolute mode (`abs g_name`), a Local/Param with a
// displacement off `fp`/`ap`, and a computed address through `(r2)`.
//
// VAX has a native quadword move/shift/compare-with-carry, so `long long` needs no
// helper calls: add/sub chain `addl2`/`adwc` and `subl2`/`sbwc`, shifts use `ashq`,
// multiplication is `emul` plus an unsigned fix-up, and division is a bit-serial loop.
// The instruction set has no AND (it is `mcoml` then `bicl`), no unsigned divide (see
// genUnsignedDivMod()), and no logical right shift (`extzv` of a field).
//
// Every conditional branch only has a byte displacement, so the assembler's `j<cc>` /
// `jbr` pseudo-instructions are used throughout; see VaxAssembler.h.
class VaxCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "vax"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".svax"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's whole-program flat binary; the other five mean a real
    // relocatable object file for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in r0 (or r1:r0 for a long long)

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // 32-bit unsigned divide: dividend in r1, divisor in r0; leaves the quotient or the
    // remainder in r0. Clobbers r3-r4.
    void genUnsignedDivMod(bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots, at runtime
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `old` in r0, `rhs`
    // in r1 (both at `type`'s width/signedness), leaves the raw new value in r0. `type`
    // is never wide here. Clobbers r3-r4 but never r2.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign - r2 must
    // already hold the resolved address on entry, and is stashed on the stack across
    // `e.rhs`'s evaluation since that can clobber it.
    void genCompoundThroughR2(const ir::Expr &e);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3), the shift amount
    // genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }
    // Loads a value of `type` from the memory operand `mem` into r0 (or r1:r0 for a
    // `long long`), sign-/zero-extending a narrower-than-int source per its signedness.
    void loadMem(const std::string &mem, Type type);
    // Stores r0 (or r1:r0) into the memory operand `mem`, truncating to `type`'s width.
    void storeMem(const std::string &mem, Type type);
    // Re-extends r0 from its low widthOf(to) bytes, per `to`'s signedness.
    void narrowR0To(Type to);
    void genConvert(const ir::Expr &e);
    void genWideAddSub(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // 64-bit divide/modulo via bit-serial shift/compare/subtract long division. Leaves
    // the result in r1:r0.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value held in the register pair `lo`/`hi`, in place.
    void genNegate64(const std::string &lo, const std::string &hi);

    // Leaves slot[idxExpr]'s effective address in r2.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Leaves `slot`'s effective address in register `reg`.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // The memory operand naming `slot`'s storage: `abs g_name` for a Global, `d(ap)` for
    // a Param and `d(fp)` for a Local. A Local array's operand is its element 0, its
    // lowest address (see ir::localLowOffset()).
    std::string slotMem(const ir::Slot *slot) const;
    // `base+disp` for a slotMem()-style operand, used to address a later element or a
    // wide value's high word.
    std::string slotMemAt(const ir::Slot *slot, int extra) const;

    // Loads/stores `slot`'s scalar value from/into r0 (or r1:r0).
    void loadSlotValue(const ir::Slot *slot);
    void storeSlotValue(const ir::Slot *slot);

    // Loads a plain 32-bit literal into register `reg`.
    void loadImmediate(const std::string &reg, int32_t v);

    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per open loop (innermost last) - {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
