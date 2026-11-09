#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates ARM assembly (this project's dialect - see ArmAssembler.h)
// for the IR built by IRBuilder, targeting the ARMv7-A "A32" (32-bit ARM,
// not Thumb) encoding. `generate()` returns a complete, self-contained
// program loaded at a fixed address (`kBaseAddress`) with no OS or
// bootloader, halting in a self-branch infinite loop.
//
// This language's `int` is 32 bits, matching the native register width.
//
// Register assignment (this backend's own convention; arguments travel
// on the hardware stack, not in registers):
//   r0       - "ac", genExpr()'s result
//   r1       - the other operand in a binary op
//   r2       - holds a computed address when dereferencing one
//   r3       - scratch inside genDivMod() for a quotient/divisor copy, so
//              SDIV/MLS never has a source/destination register collide
//   sp (r13) - the hardware stack pointer, used both to save one operand
//              across evaluating the other and for every call/return/argument
//   fp (r11) - the per-call frame pointer every function's prologue
//              points at its own frame, so a Local/Param's address is
//              always relative to that call's own `fp`
//   lr (r14) - the link register BL sets and BX reads
//
// ARM has no absolute-addressing memory operand - every load/store goes
// through a base register plus a (here, always zero) offset. Reading or
// writing a global/local/array slot first loads its fixed address into
// `r2` via `MOVW`+`MOVT`, then `ldind`/`stind r2,r0` (this dialect's
// mnemonics for LDR/STR with a fixed zero offset).
//
// `*`/`/` use the hardware `SDIV` (ARMv7-A integer-divide extension) for
// a full 32-bit signed quotient directly. `%` additionally uses `MLS`
// (multiply-subtract) to compute `dividend - quotient*divisor` in one
// instruction; genDivMod()'s operand arrangement avoids a same-register
// Rd/Rm multiply, which some ARM cores treat as UNPREDICTABLE.
//
// `<<`/`>>` use the variable-count shifted-register form of `MOV`
// (`lsl`/`lsr`); `>>` is always logical (`lsr`), never arithmetic (`asr`),
// since there's no unsigned type to distinguish them. Multiplying an
// index by a fixed element width instead uses the fixed-shift-amount form.
//
// Comparisons: `CMP` sets NZCV, and every ARM data-processing instruction
// (including `MOV`) carries a 4-bit condition field, so materializing a
// 0/1 boolean needs no branch at all - just `MOV r0,#0` followed by one
// conditionally executed `MOV r0,#1` (`MOVEQ`/`MOVNE`/...). See
// genCompare() for which condition each operator becomes.
class ArmCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "armv7a"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".sarm"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are
    // relocatable object files for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in r0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - old
    // in r0, rhs in r1 (both at `type`'s width/signedness), leaves the
    // raw (not yet re-truncated, see narrowR0To()) new value in r0.
    // `type` is never wide here - IRBuilder.cpp's rejectIfWide() guarantees it.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign and CompoundDerefAssign - r2 must
    // already hold the resolved target address on entry. `mayBeUnaligned`
    // - see loadOperand()'s comment; true only when r2 came from
    // dereferencing a plain pointer value.
    void genCompoundThroughR2(const ir::Expr &e, bool mayBeUnaligned = false);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3) - the
    // shift amount genIndexAddr()/genAddrOf()'s Index case scale an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` into r0 (or
    // r0:r1, for a `long long`), sign- or zero-extending a
    // narrower-than-native-int source per its signedness. `addrReg` must
    // not be r0/r1 (both get clobbered). `mayBeUnaligned` is true when
    // `addrReg` is a plain pointer value computed at runtime rather than
    // this backend's own Slot/array-element address arithmetic (which is
    // always naturally aligned): a real A32 LDR/STR word or halfword
    // transfer at an unaligned address takes an Alignment Fault, so such
    // an access is reassembled from individual byte transfers instead
    // (see loadWordUnaligned()/loadHalfUnaligned()).
    void loadOperand(const std::string &addrReg, Type type, bool mayBeUnaligned = false);
    // Stores r0 (or r0:r1, for a `long long`) into the address in
    // `addrReg`, truncating to `type`'s width. `mayBeUnaligned` - see
    // loadOperand()'s comment.
    void storeOperand(const std::string &addrReg, Type type, bool mayBeUnaligned = false);
    // Byte-by-byte reassembly of a 32-bit/16-bit load or store at a
    // possibly-unaligned runtime address, using r3/r4 as scratch.
    // `destReg`/`srcReg` is always r0 or r1; `addrReg` is left unchanged.
    void loadWordUnaligned(const std::string &addrReg, const std::string &destReg);
    void storeWordUnaligned(const std::string &addrReg, const std::string &srcReg);
    void loadHalfUnaligned(const std::string &addrReg, const std::string &destReg, bool isUnsigned);
    void storeHalfUnaligned(const std::string &addrReg, const std::string &srcReg);
    // Re-extends r0 from its low widthOf(to) bytes, per `to`'s
    // signedness - every Convert between two types no wider than int.
    void narrowR0To(Type to);

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (no A32 instruction is wide enough):
    // bit-serial shift/compare/subtract long division, 64 iterations,
    // in registers addressed via `r4`.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in r0:r1, in place.
    void genNegate64();

    // Leaves slot[idxExpr]'s effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Stores a Local array's compile-time arrayInit values into its
    // frame slots, one element at a time.
    void genLocalArrayInit(const ir::Slot *slot);

    // Loads `lbl`'s address into register `reg` (MOVW+MOVT).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads a Local or Param's frame-relative effective address into
    // `reg`. Each function's prologue pushes the caller's `fp` and `lr`,
    // then points `fp` at that saved pair, so a Param's address is
    // `fp+8+4*paramIndex` and a Local's is its lowest address (see
    // ir::localLowOffset()). For a Local array, `elemIndex` (a compile-time
    // index) counts elements upward from there, the same direction as a
    // Global array's.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot, int elemIndex = 0);

    // Adds (`isAdd`) or subtracts `offsetBytes` from `reg` in place.
    // `ADD`/`SUB`'s immediate operand2 is an unrotated 8-bit field, so an
    // offset that doesn't fit is loaded into a scratch register first
    // (MOVW+MOVT) and used as a register operand2 - `r1`, or `r3` when
    // `reg` itself is `r1`.
    void emitFrameOffset(const std::string &reg, int offsetBytes, bool isAdd);

    // Loads an addressable Local/Param/Global's effective address into
    // `reg`, dispatching to loadAddress() or loadFrameAddr().
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    // Loads one array initializer element's value (a plain literal, or a
    // referenced Slot's/Function's address) into register `reg`, as real
    // instructions. declareArray()'s arrayElemText() is the
    // assembler-time-evaluated form of the same value for Global "dw" data.
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        return "g_" + slot->sourceName;
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
