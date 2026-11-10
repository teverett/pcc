#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates this project's WDC 65C02 assembly dialect (see
// M65C02Assembler.h) for the IR built by IRBuilder. Identical to
// M6502Codegen.cpp's NMOS 6502 backend, except zeroing ac/bc/cx/dx uses the
// CMOS-only stzp instead of "LDA #0 / STA" (see zeroZp()).
//
// generate() returns a flat binary loaded and entered at kBaseAddress,
// halting in an infinite loop; no OS, no I/O. JSR/RTS and PHA/PLA use the
// real hardware stack (page 1); it is untouched by Local/Param storage.
//
// RECURSION / FRAMES: the 6502 has no stack-relative addressing and only
// 256 bytes of hardware stack, so each call gets a software stack frame
// reached through two more fixed zero-page pseudo-registers: sp (current
// top) and fp (current frame base), accessed via (zp),Y indirect-indexed
// addressing (see genIndexAddr()/slotAddrToPtr()).
//
// Frame layout: fp points at this call's saved-old-fp word. Param i sits
// above fp at fp + 2*(i+1) (placed there by the caller before jsr). A Local
// sits below fp at fp - ir::localLowOffset(), reserved by the callee's
// prologue; a local array's element i is at base + 2*i, the same direction
// as a global array's (see genIndexAddr()).
//
// Calling convention: the caller pushes arguments onto the software stack
// in reverse order (so argument 0 ends up nearest fp), jsr's, then pops
// them (cdecl-style caller cleanup). The callee's prologue pushes the
// caller's fp, sets fp to that slot, and reserves frameSize*2 more bytes
// for locals; the epilogue (emitEpilogue(), also run inline before an
// early return) reverses this. An indirect call never passes arguments, so
// it skips only the caller-side bookkeeping; the callee side is identical
// either way.
//
// Globals keep one static, program-lifetime trailer cell/array, addressed
// directly by label, never through fp/sp.
//
// Every 16-bit value lives in one of seven fixed zero-page pseudo-registers,
// bound once in generate() via equates: ptr (indirection target), ac
// (result, like x86 ax), bc (other operand, like bx), cx (loop counter), dx
// (remainder/scratch), and sp/fp (above). ptr must be zero-page for the
// 6502's one indirect-addressing mode; the others are zero-page only
// because it's free and uniformly cheap.
//
// No hardware multiply/divide: * is shift-and-add, / and % are repeated
// subtraction (verified only against non-negative operands).
// No variable-count shift instruction: << and >> loop one bit at a time.
//
// Signed comparisons use the standard 6502 compare-via-subtraction idiom:
// after the final SBC, N is the true signed-less-than result only once
// corrected for overflow via BVC/EOR #$80 (see genCompare()).
//
// Conditional branches reach only ±127 bytes, easily exceeded by generated
// if/while bodies, so every branch here is a short branch to an
// immediately-following label plus an unconditional full-range JMP (see
// safeBranch()).
class M65C02Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "65c02"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s65c"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's whole-program flat binary; the rest are
    // relocatable object formats for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > 2; }
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // result left in the `ac` pseudo-register

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    // Shared bit-serial loop tail of genMul()/genDivMod()/genShift(), reused
    // by genCompoundOp() once it has staged operands in the same registers.
    void genMulCore();                                  // bc = multiplicand, dx = multiplier -> ac = product
    void genDivModCore(bool wantRemainder, bool isUnsigned);  // bc = divisor, dx = dividend -> ac = quotient/remainder
    void genShiftCore(bool isLeft, bool isUnsigned);     // ac = value, cx = count (low byte) -> ac = result
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot, &slot[idx], or &fn
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: old
    // already in ac, rhs already in bc; leaves the raw (not yet
    // re-truncated) new value in ac. Its "/"/"%" case uses ptr as scratch,
    // so a caller holding an address in ptr must re-stash it first.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign;
    // `ptr` must already hold the resolved address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-truncates `ac` to `to`'s width, re-extending per its signedness if
    // narrower than a native word.
    void narrowAcTo(Type to);

    // long/long long ("wide") values: three more fixed 8-byte zero-page
    // buffers, wac/wbc/wtmp/wtmp2, little-endian; the hardware stack is
    // used only as transient storage for one operand while the other is
    // evaluated.
    void pushWide(const char *base, int nbytes);
    void popWide(const char *base, int nbytes);
    void copyWide(const char *dst, const char *src, int nbytes);
    void negateWideBuffer(const char *buf, int nbytes);

    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    void genWideLoadAtPtr(int words);
    void genWideStoreAtPtr(int words);
    // Assumes ptr/ptr+1 holds the effective address; dispatches by type
    // width: one sign/zero-extended byte for char, ac's two bytes for a
    // native word, or genWideLoadAtPtr()/genWideStoreAtPtr() for long/long long.
    void loadOperandAtPtr(Type type);
    void storeOperandAtPtr(Type type);

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words, bool invert);
    void emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);

    // Leaves slot[idxExpr]'s effective address in `ptr` - the one real
    // zero-page-bound addressing mode here is (zp),Y, so any dereferenced
    // pointer is copied into ptr first.
    //
    // slot->arraySize > 0 means the array's storage is indexed directly: a
    // Global's is a compile-time label (base + idx*2), a Local's is
    // fp-relative (base - idx*2; a Param array always decays to a pointer,
    // so can't reach this branch). Otherwise slot is a plain pointer
    // variable: its stored value is the base, and idx*2 is always ADDED to
    // it regardless of where the pointer variable itself lives.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Leaves a Local/Param slot's storage address in ptr: fp +
    // 2*(paramIndex+1) for a Param, fp - ir::localLowOffset() for a Local. Never
    // called with a Global slot (always a compile-time label).
    void slotAddrToPtr(const ir::Slot *slot);
    // Reads/writes a scalar slot's value into/from ac: a direct label copy
    // for a Global, indirect through slotAddrToPtr()'s ptr for a Local/Param.
    void loadSlotToAc(const ir::Slot *slot);
    void storeAcToSlot(const ir::Slot *slot);

    // Stores arrayInit into a Local array slot's frame words; re-run every
    // call since a Local's frame storage isn't pre-set the way a Global's
    // static trailer is.
    void genLocalArrayInit(const ir::Slot *slot);
    // Loads one ArrayInitElem's value into ac: a plain 16-bit immediate for
    // a literal, or a label's address (low/high byte) for a symbol element.
    void loadArrayElemToAc(const ir::ArrayInitElem &e);

    // Restores sp to just above this frame's saved-old-fp word, then
    // restores fp from it. Run at the end of genFunction() and inline
    // before every early return. Uses ptr as scratch; never touches ac,
    // since a Return's value already sits there and must survive into rts.
    void emitEpilogue();

    // 16-bit dstReg = srcReg + disp (disp may be negative, added via
    // adci/adci_hi); safe even when dstReg == srcReg, since the low byte is
    // written before the high byte is read.
    void addConst16(const std::string &dstReg, const std::string &srcReg, int disp);
    // 16-bit dst = a + b / dst = a - b; same in-place safety as addConst16().
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    // Reads/writes a 16-bit value through zpPtr's (zp),Y indirect addressing
    // into/from dstReg/srcReg.
    void loadIndirect(const std::string &zpPtr, const std::string &dstReg);
    void storeIndirect(const std::string &zpPtr, const std::string &srcReg);

    // "If the branch holds, jump to targetIfTaken, else targetIfNot" via a
    // short branch to an immediately-following label (always in range)
    // plus an unconditional full-range JMP. branchMnemonic is one of
    // M65C02Assembler.cpp's branch mnemonics (beq/bne/bcs/bmi/bpl/bvc).
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // Copies one 16-bit pseudo-register/slot to another, byte by byte.
    void copy16(const std::string &dst, const std::string &src);
    // Zeroes a zero-page pseudo-register (ac/bc/cx/dx, never ptr) using the
    // CMOS-only STZ. `lbl` must be a zero-page symbol or stzp's assembler-
    // level range check fails.
    void zeroZp(const std::string &lbl);
    // 16-bit increment: increments lbl, then lbl+1 if lbl wrapped to 0. Uses
    // a plain bne, not safeBranch(), since its target is always a few
    // instructions away, never arbitrary user code.
    void inc16(const std::string &lbl);

    // Only ever called with a Global slot; a Local/Param is addressed via
    // slotAddrToPtr() instead.
    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

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
    std::vector<std::string> trailer_;  // data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // {continue label, break label} per open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
