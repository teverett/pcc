#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates this project's TMS9900 dialect (see TMS9900Assembler.h) for the IR built by
// IRBuilder, as a flat binary loaded at a fixed address with no OS/firmware - the simulator (or
// a future TI-99/4A platform's own linker factory) sets the CPU's PC there directly.
//
// `int` is 16 bits - the TMS9900's own native word. Values live in six of the chip's 16
// "registers" (really just RAM words addressed through the Workspace Pointer, WP - see
// TMS9900Assembler.h's own header comment on addressing-mode syntax): `r0` (genExpr()'s
// result), `r1` (a binary op's other operand), `r2`/`r3` (loop counter/scratch), `r4` (a
// computed pointer for array/pointer dereference - any register can be used indirectly here,
// unlike the 6502's zero-page-only `(zp),Y`), `r10` (software frame pointer), `r11` (the real
// hardware return-address register `BL` always sets - saved/restored across this backend's own
// nested calls, matching the documented real convention), and `r15` (a software stack pointer,
// bump-allocated downward over ordinary RAM - the TMS9900 has no hardware PUSH/POP of its own,
// unlike the 68000's A7; `dect`/`inct`'s "decrement/increment by 2" are used directly for this).
//
// `*`/`/` use the real hardware MPY/DIV instructions (16x16->32 multiply, 32/16->16r16
// divide) rather than a software routine - genuinely faster than every 8-bit backend here.
// DIV is unsigned-only in hardware; signed division negates both operands first and fixes the
// quotient/remainder's sign afterward (see genDivMod()'s own comment).
//
// `<<`/`>>` use the real hardware SLA/SRL/SRA, whose count field is either a compile-time
// constant (1-15) or, when the field is 0, taken from r0's own low nibble at runtime - used
// here for a variable shift count (see genShift()).
//
// Comparisons use C (compare) and the real status-register condition codes (JEQ/JNE/JLT-family
// for signed, JL/JLE/JH/JHE for unsigned - the TMS9900 has genuinely separate signed and
// unsigned branch conditions, unlike the 6502/z80's own sign-bit-correction tricks).
//
// Deliberately not implemented, and left that way rather than guessed at under time pressure:
// `long`/`long long` (wide) arithmetic - every wide-value code path throws a clear "not
// supported" error instead of emitting wrong code. This is the one explicit scope cut on top
// of a complete TMS9900Assembler.cpp (every real instruction, not just what this file emits).
class TMS9900Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "tms9900"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s99"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in r0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    // Evaluates lhs then rhs, leaves 0/1 in r0 per whether `branchIfTrue` (a real TMS9900
    // jump mnemonic) would fire. The TMS9900's own C instruction sets condition codes for
    // "destination compared to source" (`c rhsReg,lhsReg` - rhs is the source, lhs the
    // destination - leaves flags testable directly as "lhs <cond> rhs"); the TMS9900 has no
    // direct signed->= or unsigned-<= jump, so `swapOperands` computes "rhs <cond> lhs"
    // instead for exactly those two cases - see genCompare()'s own comment for why that's
    // equivalent.
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue,
                      bool swapOperands = false);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // r0=rhs, r1=lhs preset (genBinary's own arrangement) - see this function's own comment.
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // r0=dividend, r1=divisor preset (genBinary/genCompoundOp's own arrangement).
    void genDivMod(bool isUnsigned, bool wantRemainder);
    // r0=value, r1=count preset (genBinary/genCompoundOp's own arrangement).
    void genShiftCore(bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);
    void genConvert(const ir::Expr &e);
    // new = old <op> rhs: old in r0, rhs in r1, leaves the un-narrowed result in r0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for indexed CompoundAssign/CompoundDerefAssign; r4 must hold the resolved
    // address on entry and is preserved across evaluating e.rhs via pushR4()/popR4().
    void genCompoundThroughPtr(const ir::Expr &e);

    // Leaves slot[idxExpr]'s own effective address in `r4`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    [[noreturn]] void failWide(int line) const;
    // log2 of the element width (1/2/4/8 -> 0/1/2/3), or -1 when not a power of two (struct
    // element) - the number of left shifts (via the real SLA) to scale an index into a byte
    // offset.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            case 8: return 3;
            default: return -1;
        }
    }

    // Loads a value of `type` from operand text `mem` into r0, sign/zero-extending a narrower-
    // than-int type per its signedness (MOVB only ever touches a word's own high byte - see
    // TMS9900Assembler.h's own header comment - so a `char` load/store always goes through r0's
    // high byte, then narrows/extends into the low byte position this backend otherwise keeps
    // every 16-bit value in).
    void loadOperand(const std::string &mem, Type type);
    void storeOperand(const std::string &mem, Type type);
    void narrowR0To(Type to);
    // Loads/stores r0 through the address already in r4.
    void loadIndirect(Type type);
    void storeIndirect(Type type);
    void pushR0() { code_ << "    dect r15\n    mov r0,[r15]\n"; }
    void popR0() { code_ << "    mov [ir15],r0\n"; }
    void pushR1() { code_ << "    dect r15\n    mov r1,[r15]\n"; }
    void popR1() { code_ << "    mov [ir15],r1\n"; }
    void pushR4() { code_ << "    dect r15\n    mov r4,[r15]\n"; }
    void popR4() { code_ << "    mov [ir15],r4\n"; }

    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's frame-relative operand text, e.g. "[r10+4]"/"[r10-8]" - TMS9900's own
    // Indexed addressing mode (base register + a following displacement word) reaches this
    // directly, with no software pointer-staging workaround the 6502 backend needs. Params
    // ascend from `r10` (past the 4 bytes this backend's own call sequence reserves for the
    // saved return address/old frame pointer - see genFunction()'s own prologue/epilogue);
    // Locals descend, same convention every other backend here uses.
    //
    // Every Param's own reserved slot is a full native word regardless of its C type (every
    // Call argument is pushed as one word - see genExpr()'s own Call comment), but a `char`
    // Param's real byte sits at the LOW-address half of that word (this backend pushes an
    // ordinary 0-255 word value, whose big-endian layout puts the meaningful byte second) -
    // `paramByteFixup()` is the same kind of "narrow value's real byte isn't at the slot's own
    // base address" adjustment M68kCodegen.h's own `paramBigEndianFixup()` makes, just by a
    // fixed 1 byte instead of a width-dependent one (every Param slot here is exactly 2 bytes).
    int paramByteFixup(const ir::Slot *slot) const {
        return (slot->kind == ir::SlotKind::Param && widthOfType(slot->type) == 1) ? 1 : 0;
    }
    std::string addr(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[r10+" + std::to_string(slot->frameOffset + paramByteFixup(slot) + byteOffset) + "]";
        }
        if (slot->arraySize > 0) {
            return "[r10-" + std::to_string(ir::localLowOffset(*slot, typeWidths()) - byteOffset) + "]";
        }
        return "[r10-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths()) + byteOffset) + "]";
    }
    std::string operandOf(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return "[" + (byteOffset == 0 ? lbl : (lbl + "+" + std::to_string(byteOffset))) + "]";
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
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
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    const ir::Function *curFunction_ = nullptr;
    std::vector<std::pair<std::string, std::string>> loopLabels_;  // {continue label, break label}
    std::map<int, std::string> irLabels_;
};

}  // namespace rcc
