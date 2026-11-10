#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates AArch64 assembly (this project's dialect - see
// Arm64Assembler.h) for the IR built by IRBuilder, targeting the
// ARMv8-A "A64" instruction set (`--target=armv8a`). `generate()` returns
// a complete, self-contained program with no OS and no bootloader,
// halting in a self-branch infinite loop. Every function has a real
// per-call stack frame (see genFunction()/loadSlotAddr()), so direct and
// indirect recursion both work: every activation gets its own
// `x29`-rooted frame, its own copy of every Local/Param, and its own
// saved `x30`/`x29` pair.
//
// This language's `int` is 32 bits (every data-processing instruction
// here is the 32-bit "W" variant), but addresses need the full 64 bits
// (A64 has no 32-bit addressing mode) - so the `ac`/other-operand/addr
// registers below are each one physical register addressed two
// different ways depending on role: `w0` for a data value, `x0` for an
// address held in the same register. This is safe because every 32-bit
// ("W-form") instruction zero-extends its result into the full 64-bit
// register as an architectural guarantee (ARM DDI 0487) - a value
// computed via a `w`-form op is always already safe to read back as its
// `x`-form view with no explicit extension, and genAddrOf()/genIndex()
// rely on exactly that.
//
// Register assignment (this backend's own convention; arguments go
// through the caller's pushed stack words, not registers):
//   w0/x0  ("ac")   - genExpr()'s result
//   w1/x1           - "the other operand" in a binary op
//   w2/x2           - holds a computed address when dereferencing one
//   sp              - the hardware stack pointer, used to save one
//                      operand across evaluating the other, to pass each
//                      call's arguments, and to reserve/release each
//                      call's frame
//   x29 (fp)        - this call's frame pointer, set up by
//                      genFunction()'s prologue and torn down by its epilogue
//   x30 (lr)        - the link register `bl` sets and `ret` reads
//
// Like MIPS, A64 has no absolute-addressing memory operand - every
// load/store goes through a base register with a zero displacement.
// Reading or writing a Global first loads its fixed address into `x2`
// via `movz`+`movk`; a Local/Param instead computes its `x29`-relative
// address into `x2` with `mov`+`add`/`sub` (see loadSlotAddr()). Either
// way the access is then `ldind`/`stind x2,w0`.
//
// `*`/`/` use the hardware `sdiv` (32-bit signed divide) and `msub`
// (remainder = dividend - quotient*divisor, in one instruction).
//
// `<<`/`>>` use the variable-count register-form shift (`lslv`/`lsrv`);
// `>>` is always logical/unsigned. Multiplying an index by a fixed
// element width instead uses `mul` against a loaded constant, since this
// backend has no shift-by-immediate mnemonic.
//
// Comparisons use `cmp` (`subs` with a discarded result) plus
// `cset<cc>` (spelled as one of six dedicated mnemonics -
// `csetlt`/`csetgt`/`csetle`/`csetge`/`cseteq`/`csetne` - since this
// grammar's `expr` has no way to spell a bare condition mnemonic that
// isn't itself a register name or a label) - each directly produces the
// boolean this language needs (1 or 0), no branch needed.
//
// `if`/`while` conditions use `cbz`/`cbnz` directly against `w0` - no
// flags and no `cmp` needed for control flow.
//
// A64 branches/calls have no delay slot.
class Arm64Codegen : public Backend {
   public:
    void setHostedAbi(bool enabled) { darwinHosted_ = enabled; }
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "armv8a"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8, 8, darwinHosted_ ? 8 : 0}; }
    std::string sourceExtension() const override { return ".s64"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are
    // relocatable object files for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in w0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - old
    // in w0, rhs in w1 (both at `type`'s width/signedness; never wide
    // here), leaves the raw (not yet re-truncated, see narrowW0To())
    // new value in w0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign and CompoundDerefAssign - x2 must
    // already hold the resolved target address on entry. `mayBeUnaligned`
    // - see loadOperand()'s comment; true only when x2 came from
    // dereferencing a plain pointer value.
    void genCompoundThroughX2(const ir::Expr &e, bool mayBeUnaligned = false);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3) - the
    // shift amount genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` (an "x"
    // register) into w0 (or x0, for a `long long`), sign- or
    // zero-extending a narrower-than-native-int source per its
    // signedness. `mayBeUnaligned` is true when `addrReg` is a plain
    // pointer value computed at runtime rather than this backend's own
    // Slot/array-element address arithmetic (which is always naturally
    // aligned): a real A64 LDR/STR transfer wider than a byte takes an
    // Alignment Fault at an unaligned address, so such a width is
    // reassembled from individual byte transfers instead (see
    // loadIntUnaligned()).
    void loadOperand(const std::string &addrReg, Type type, bool mayBeUnaligned = false);
    // Stores w0 (or x0, for a `long long`) into the address in
    // `addrReg`, truncating to `type`'s width. `mayBeUnaligned` - see
    // loadOperand()'s comment.
    void storeOperand(const std::string &addrReg, Type type, bool mayBeUnaligned = false);
    // Byte-by-byte reassembly of an `nBytes`-wide (2, 4, or 8) load or
    // store at a possibly-unaligned runtime address, using `x3` as the
    // running byte address and `w4`/`x4` as the byte just transferred.
    // `destReg`/`srcReg` is always w0/x0, and `addrReg` is left unchanged.
    void loadIntUnaligned(const std::string &addrReg, const std::string &destReg, int nBytes);
    void storeIntUnaligned(const std::string &addrReg, const std::string &srcReg, int nBytes);
    // Re-extends w0 from its low widthOf(to) bytes, per `to`'s
    // signedness - every Convert between two types no wider than int.
    void narrowW0To(Type to);
    // Loads a 64-bit immediate into `reg` (an "x" register) - movz (hw=0)
    // + up to three movk/movk2/movk3 (hw=1/2/3), skipping any all-zero
    // chunk beyond the first.
    void loadImm64(const std::string &reg, uint64_t uv);

    // Leaves slot[idxExpr]'s effective address in `x2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s address into 64-bit register `reg` (`movz`+`movk`).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads 32-bit immediate `uv` into `reg` (a `w`-named register) -
    // `movz` alone when it fits in 16 bits, `movz`+`movk` otherwise.
    void loadImm32(const std::string &reg, uint32_t uv);

    // Every function gets a per-call stack frame: `x29` is this call's
    // frame pointer, set up by the prologue to point at the
    // saved-old-`x29` word once the old `x29`/`x30` are both saved below
    // the caller's pushed arguments. A Param's storage is the caller's
    // pushed argument, above those two saved words (`x29+32+4*
    // paramIndex`); a Local's is reserved by the prologue's
    // `sub sp,sp,frameSize*4` below `x29`, at its lowest address (see
    // ir::localLowOffset()). This
    // makes recursion safe: every activation gets its own `x29` and
    // frame. Loads the computed address into 64-bit register `reg`.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    std::string label(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Global) return darwinHosted_ ? "_" + slot->sourceName
                                                                     : "g_" + slot->sourceName;
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    std::string label(const ir::Function *fn) const {
        return darwinHosted_ ? "_" + fn->sourceName : "fn_" + fn->sourceName;
    }

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
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    bool darwinHosted_ = false;
    int currentParamBase_ = 0;
    // The frameSize of the function genStmt() is currently generating
    // code for, needed by a `return` statement's inline epilogue.
    int currentFrameSize_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
