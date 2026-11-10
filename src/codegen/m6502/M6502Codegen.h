#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 6502 assembly (this project's dialect - see M6502Assembler.h)
// for the IR built by IRBuilder. generate() returns a complete flat binary
// loaded and entered at a fixed address, halting in an infinite loop; the
// real hardware stack (JSR/RTS, and PHA/PLA for saving an operand across
// evaluating the other side of a binary expression) is used directly.
//
// Every 16-bit value in flight lives in one of five fixed, two-byte
// zero-page pseudo-registers bound once via a compile-time equate: `ptr`
// (an indirection target - must be in zero page for the 6502's one
// indirect-addressing mode, `(zp),Y`), `ac` (genExpr()'s result), `bc`
// (the other operand of a binary op), `cx` (a loop counter), `dx` (divide
// remainder / scratch).
//
// No hardware multiply/divide: `*` is shift-and-add, `/` and `%` are
// repeated subtraction (only verified against non-negative operands). No
// variable-count shift instruction either, so `<<`/`>>` loop one bit at a
// time.
//
// Signed multi-byte `<`/`<=`/`>`/`>=` use the standard 6502 compare-via-
// subtraction idiom: after the final SBC, N is the true signed-less-than
// result only once corrected for overflow via BVC/EOR #$80 (see
// genCompare()).
//
// 6502 conditional branches reach only ±127 bytes, too short for an
// ordinary-sized `if`/`while` body, so every branch here is a short branch
// to an immediately-following label plus an unconditional (full-range)
// JMP - see safeBranch().
//
// Each call gets a fresh per-call frame (so recursion works) via a
// software frame pointer `fp`, since the 6502 has no load-relative-to-
// pointer addressing mode and the hardware stack register S isn't
// indexable. Every Local/Param access sets `ptr = fp + offset`
// (setPtrToFrameSlot()) and indirects through it. `sp` is a bump
// allocator over a dedicated RAM region (`kFrameStackBase`) that frames
// are carved from.
//
// Frame offsets increase ascending from `fp` (Params at 2*paramIndex,
// then Locals) rather than x86's descending-from-bp, because the 6502 has
// no immediate SBC: every offset stays a plain addition.
//
// A call's arguments are evaluated and parked on the hardware stack first
// (so a nested call in a later argument can't clobber an earlier one's
// already-computed address), then written into the reserved frame area
// once all are known. The callee's prologue computes `fp` as `sp` (at
// entry) minus that reserved size; its epilogue collapses `sp` back to
// `fp` and restores the caller's `fp` via PHA/PLA, mirroring x86's
// push/pop bp.
class M6502Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "6502"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s65"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is the whole-program flat binary; the other five are relocatable
    // object formats for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > 2; }
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 -> 0/1/2/3): number of left shifts
    // to scale an index into a byte offset (see genIndexAddr()).
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    void genFunction(const ir::FunctionBody &fn);
    // Collapses sp back to fp and restores the caller's fp; used for both
    // the function's fall-through exit and every `return` statement.
    void genEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

    // Re-runs arrayInit into `slot`'s frame cells on every call, since a
    // Local's storage is fresh per activation, unlike a Global's static one.
    void genLocalArrayInit(const ir::Slot *slot);

    // ac = slot's value / slot = ac. Globals load/store by fixed label;
    // Locals/Params go through `ptr` (setPtrToFrameSlot()). A `char` slot
    // sign/zero-extends into ac+1 on load so `ac` is always a promoted int.
    // Never called for a `long`/`long long` slot (see genWideLoad/Store).
    void loadSlotToAc(const ir::Slot *slot);
    void storeAcToSlot(const ir::Slot *slot);

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // Leaves `ac` holding 0 (false) or nonzero (true); for a wide value
    // (`wac`), ORs all its bytes into `ac` first (true iff any byte is nonzero).
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    // Bit-serial core of genMul()/genDivMod()/genShift(), factored out so
    // genCompoundOp() can drive the same loop over operands it stages itself.
    void genMulCore();                                  // bc = multiplicand, dx = multiplier -> ac = product
    void genDivModCore(bool wantRemainder, bool isUnsigned);  // bc = divisor, dx = dividend -> ac = quotient/remainder
    void genShiftCore(bool isLeft, bool isUnsigned);     // ac = value, cx = count (low byte) -> ac = result
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot, &slot[idx], or &fn
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: ac=old,
    // bc=rhs in, raw (untruncated) new value in ac out. `type` is never wider
    // than a native word here (IRBuilder's rejectIfWide() guarantees it).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign;
    // `ptr` must already hold the resolved address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-truncates `ac` to `to`'s width, re-extending per its signedness if
    // narrower than a native word.
    void narrowAcTo(Type to);

    // --- `long`/`long long` ("wide") values -----------------------
    //
    // No zero-page pseudo-register is wide enough for a `long`/`long long`,
    // and there's no hardware stack-relative addressing mode to park an
    // arbitrary-width value on the real stack and still address its bytes.
    // Wide values instead live in three fixed 8-byte zero-page buffers
    // (wide enough for a `long long`), byte-addressed little-endian: `wac`
    // (wide result), `wbc` (other operand), `wtmp`/`wtmp2` (scratch). Only
    // `words*2` bytes of each are touched for `long` (words=2); `long long`
    // (words=4) uses the whole buffer.
    //
    // Reentrancy across evaluating lhs/rhs is handled like the narrow
    // helpers: genExpr(lhs) leaves its result in `wac`, pushed byte-by-byte
    // (pushWide()) before genExpr(rhs) overwrites `wac`; popWide() into
    // `wbc` afterward leaves wbc=lhs, wac=rhs.
    void pushWide(const char *base, int nbytes);
    void popWide(const char *base, int nbytes);
    void copyWide(const char *dst, const char *src, int nbytes);
    // Two's-complement negation in place: complement every byte, then add 1
    // with the carry chained through the rest.
    void negateWideBuffer(const char *buf, int nbytes);

    void genWideLoad(const ir::Slot *slot, int words);        // wac = slot's own `words`-word value
    void genWideLoadLiteral(long value, int words);           // wac = a compile-time-known wide literal
    void genWideStore(const ir::Slot *slot, int words);        // slot = wac
    // wac = the `words`-word value at `ptr`/`ptr+1`, or the reverse.
    void genWideLoadAtPtr(int words);
    void genWideStoreAtPtr(int words);
    // Assumes `ptr`/`ptr+1` already holds the effective address. One byte
    // (sign/zero-extended into/from ac+1) for `char`; `ac`'s two bytes up to
    // a native word; genWideLoad/StoreAtPtr (`wac`) for a wide pointee.
    void loadOperandAtPtr(Type type);
    void storeOperandAtPtr(Type type);
    // Word `i`'s label sits `2*i` bytes past `slot`'s ordinary address.
    std::string wideWordLabel(const ir::Slot *slot, int byteOffset) const;

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words, bool invert);
    void emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);

    // Leaves slot[idxExpr]'s effective address in `ptr`: the 6502's one
    // indirect-addressing mode, `(zp),Y`, requires the pointer to be a
    // zero-page address, so every other pointer value is copied into
    // `ptr` only at the moment it's dereferenced.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Local/Param's byte offset from `fp` in the current frame: a Param
    // sits at 2*paramIndex; a Local follows all Params, at
    // 2*(paramCount + frameOffset-1) (frameOffset is 1-based).
    int frameByteOffset(const ir::Slot *slot) const;

    // Sets `ptr` to fp + frameByteOffset(slot) + extraByteOffset - the only
    // way to reach a Local/Param, there being no hardware load-relative-to-
    // pointer addressing mode.
    void setPtrToFrameSlot(const ir::Slot *slot, int extraByteOffset);

    // "if condition, jump to targetIfTaken, else targetIfNot" via a short
    // branch to an immediately-following label (always in range) plus an
    // unconditional full-range JMP. `branchMnemonic` is one of
    // M6502Assembler.cpp's branch mnemonics ("beq", "bne", "bcs", "bcc",
    // "bmi", "bpl", "bvc").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    void copy16(const std::string &dst, const std::string &src);
    // 16-bit increment: the 6502 has no 16-bit INC, so this increments the
    // low byte and, if it wrapped to 0, the high byte too.
    void inc16(const std::string &lbl);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
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
    std::vector<std::string> trailer_;  // "label: dblo e" / "dbhi e" data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;

    const ir::Function *curFunction_ = nullptr;  // function being generated; used by frameByteOffset()
    // {continue label, break label} per open loop (innermost last).
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
