#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates Intel 8080 assembly (standard mnemonics) for the IR built by IRBuilder.
// `generate()` emits a flat binary loaded at a fixed address (kBaseAddress), halting in an
// infinite loop; no OS/CP-M/BDOS calls.
//
// `int` is 16 bits. The 8080's three register pairs (BC/DE/HL) plus A can't hold every live
// value of a recursive-descent expression evaluator, and M-addressing always goes through HL,
// so every in-flight value instead lives in a fixed 16-bit memory cell (`ac`, `bc`, `cx`/`dx`,
// `ptr`). HL is the transit register: loaded from a cell immediately before an operation needs
// a real register, stored back right after.
//
// No hardware multiply/divide: `*` is shift-and-add, `/`/`%` repeated subtraction, both
// verified only for non-negative operands. No variable-count shift either: `<<`/`>>` rotate one
// bit at a time via RAL/RAR, using `ANA A` first to clear carry (the 8080 has no explicit
// clear-carry instruction).
//
// Every function gets a real stack frame with a software frame pointer `fp` (the 8080 has no
// base+displacement addressing mode): `lhld fp`/`lxi d,<offset>`/`dad d` computes a frame-
// relative address by hand. Param i is at `fp+4+2*i`; Local i is at `fp-2*(frameOffset+i)`. A
// Local array's elements sit at increasing addresses from its lowest one, element 0 (see
// ir::localLowOffset()), the same layout as a Global array's.
//
// Comparisons (`<`,`<=`,`>`,`>=`) use the sign-bit-flip bias trick (XOR 0x8000 on both operands'
// high bytes) since the 8080's flags have no overflow bit; `==`/`!=` are a plain subtract-and-
// OR-the-bytes zero check.
//
// Every 8080 jump/call is a full 16-bit absolute address, so there's no branch-range workaround
// needed anywhere in this backend.
class I8080Codegen : public Backend {
   public:
    // Label for one scratch word; `role` is "a"/"b"/"ret", `i` is the word index (0 = least
    // significant). Public so the free function in I8080Codegen.cpp's anonymous namespace can
    // call it too.
    static std::string wideScratch(const char *role, int i) { return std::string("w") + role + std::to_string(i); }

    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8080"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s80"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other formats are relocatable object files.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots
    // Computes CompoundAssign/CompoundDerefAssign's "new = old <op> rhs": `old` in HL, `rhs` in
    // DE, both at `type`'s width/signedness; leaves the unnarrowed result in `ac` (see
    // narrowAcTo()). `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // CompoundAssign's unindexed target. The target address is a pure function of `e.slot`, so
    // it's recomputed after evaluating `rhs` rather than held across it; only the loaded `old`
    // value needs the real stack to survive that evaluation.
    void genCompoundScalar(const ir::Expr &e);
    // Shared tail for indexed CompoundAssign and CompoundDerefAssign; `ptr` must already hold
    // the resolved address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows `ac` to `to`'s width/signedness when `to` is a `char`.
    void narrowAcTo(Type to);

    // Leaves slot[idxExpr]'s effective address in the `ptr` memory cell.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Leaves a Local/Param slot's effective address in HL. `extraBytes` offsets into an array
    // element or a wide word's word i. Never called for a Global (see label()).
    void genFrameAddr(const ir::Slot *slot, long extraBytes = 0);

    // `widthOfType`: byte width of `t`. `isWide`: true for `long`/`long long` (wider than this
    // backend's native 16-bit word), represented as `wordCountOf` (2 or 4) 16-bit words, least
    // significant word pushed last.
    int widthOfType(Type t) const;
    bool isWide(Type t) const;
    int wordCountOf(Type t) const;

    // HL must hold the operand's address. A `char` is sign- or zero-extended into `ac`'s 16
    // bits; anything else is a plain 16-bit load.
    void loadOperand(Type type);
    // HL must hold the operand's address and `ac` the value to store; only writes one byte for
    // a `char`.
    void storeOperand(Type type);

    // An implicit or explicit conversion (ir::ExprKind::Convert).
    void genConvert(const ir::Expr &e);

    void popWideInto(const char *role, int words);
    void pushWideFrom(const char *role, int words);

    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    // Like genWideLoad()/genWideStore(), but the element's address is already in `ptr`
    // (genIndexAddr()'s output) instead of coming from a Slot.
    void genWideLoadAtPtr(int words);
    void genWideStoreAtPtr(const char *role, int words);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideNegate(int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    void genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words);
    void genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    // Unimplemented: no way to verify a hand-written multi-word multiply/divide routine.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, int words);

    // Leaves a plain 0/1 truth value in `ac` for whatever `e`'s genExpr() just left behind.
    void testTruthy(const ir::Expr &e);

    // Tears down the current frame and returns: `lhld fp`/`sphl` discards the locals' reserved
    // region, `pop h`/`shld fp` restores the caller's fp, `ret` returns. Used both by
    // genFunction()'s fall-through epilogue and inline by every Return statement.
    void emitEpilogue();

    // HL must hold a target address; writes `ac`'s two bytes there without touching HL.
    void storeAcIndirect();

    // Copies one 16-bit memory cell to another ("lhld src\nshld dst\n").
    void copy16(const std::string &dst, const std::string &src);
    // Loads a 16-bit memory cell into DE or BC (`pairReg` is "d" or "b") via HL and a stack
    // round-trip, since `LHLD` only ever targets HL.
    void loadPair(const std::string &pairReg, const std::string &src);
    // The reverse of loadPair().
    void storePair(const std::string &dst, const std::string &pairReg);
    // Shifts the 16-bit memory cell `lbl` left or right by one bit in place, via HL.
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // Only for a Global; a Local/Param is addressed relative to `fp` instead (see
    // genFrameAddr()).
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's byte offset from `fp`: `fp+frameOffset` for a Param, `fp-frameOffset` for
    // a Local, each `extraBytes` further from `fp`. A Local array's is its element 0's, with
    // `extraBytes` counting upward from there instead. Never called for a Global.
    long frameByteOffset(const ir::Slot *slot, long extraBytes = 0) const {
        if (slot->kind == ir::SlotKind::Param) return static_cast<long>(slot->frameOffset) + extraBytes;
        if (slot->arraySize > 0) return -static_cast<long>(ir::localLowOffset(*slot, typeWidths())) + extraBytes;
        return -(static_cast<long>(ir::localOffsetDescendingWide(*slot, typeWidths())) + extraBytes);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    // Maps an ir::Stmt::labelId (switch's own Label/Goto - see IR.h) to this
    // backend's own label text, generated the first time either a Label
    // defining it or a Goto referencing it is seen.
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // {continue label, break label} per open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
