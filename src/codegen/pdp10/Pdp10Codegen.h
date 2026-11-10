#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates PDP-10 assembly (this project's own dialect - see Pdp10Assembler.h) for the IR
// built by IRBuilder. `generate()` returns a complete flat binary loaded at a fixed address
// (`kBaseAddress`) with no OS, halting via the real `JRST 4,0` ("HALT") instruction, with a
// `jrst 0,halt` safety net after it in case a simulator's HALT just traps to a console prompt
// and execution resumes at the next word.
//
// This language's `int`/`char`/`short` are each one 36-bit PDP-10 word (not byte-addressable -
// see typeWidths()'s `unitBits=36`, the same convention as the pdp7 backend). `long` is 2 words,
// `long long` is 4.
//
// Sixteen real accumulators (AC0-AC17 octal) exist, and - the PDP-10's defining addressing
// quirk - AC*n*'s register and the memory word at address *n* (0-17 octal) are the exact same
// storage, so every instruction's plain-address operand slot doubles as "read/write this other
// accumulator" with no separate register-direct addressing mode needed. Live values use:
// `AC1` (genExpr()'s result, like pdp11's R0), `AC2` (the other operand of a binary op, like
// R1), `AC3` (a computed address, dereferenced via index-mode `[3+0]`, like R2). `AC4`-`AC7`
// are extra scratch for a `long`/`long long` value's several words at once (storeIndirect(),
// the wide carry-chain helpers). `AC0` is never used (the simplest choice - it avoids any
// AC0-is-special edge case on real hardware). `AC16` ("F") is the dedicated frame-pointer
// register (see addr()). `AC17` ("P") is the real hardware stack-pointer accumulator: calls
// use the real stack-pointer-in-AC instruction family, `PUSHJ`/`POPJ` for the call/return
// itself and `PUSH`/`POP` for argument passing and the frame pointer's own save/restore,
// confirmed directly against the real `pdp10` (open-simh) simulator (see pushReg()/popReg()).
// Every call gets its own frame, so recursion works.
//
// Confirmed directly against the real simulator: `PUSH`/`PUSHJ` increment P's address field
// *before* storing, so the real hardware stack grows *upward* (toward higher addresses) -
// the opposite of pdp11's software SP. This flips every sign in the frame layout relative to
// Pdp11Codegen.cpp: a Param's address is `AC16 - frameOffset` (Params sit *below* the frame
// pointer) and a Local's is `AC16 + localOffsetDescendingWide(...)` (Locals sit *above* it) -
// see addr(). A call's own epilogue (`MOVE P,AC16` / `POP P,AC16` / `POPJ P,`) is the exact
// mirror of pdp11's `mov r5,sp` / `pop r5` / `rts`, and the caller's post-call cleanup is
// `SUBI P,totalWords` (pdp11 `ADD`s to its down-growing SP; this target `SUB`s instead).
//
// `*`: real `MUL AC,E` (signed, double-length product) - confirmed directly: the *low*
// (truncated, single-length) word lands in `AC+1`, not `AC` itself, so `MUL AC1,AC2` leaves
// the real single-length product in `AC2`, copied back to `AC1` afterward (`genMul()`) -
// mirroring Pdp11Codegen.cpp's own `mul r1,r0 \ mov r1,r0` shape exactly.
// `/`,`%`: real `IDIV AC,E` (signed, *single*-length dividend - confirmed directly against
// the simulator, unlike the EIS-style double-length `DIV` pdp11 uses): quotient in `AC`,
// remainder in `AC+1`. There is no native *unsigned* divide; `emitUnsignedDiv()` ports
// Pdp11Codegen.cpp's own big/small-divisor case split one-for-one, using the real
// double-length `DIV` (zero-extended dividend) for the "small divisor" case and a software
// unsigned compare-and-subtract for the "big divisor" case (quotient can only be 0 or 1 once
// the divisor's own sign bit is set).
// `&`/`|`/`^`: real `AND`/`IOR`/`XOR` - no De Morgan trick needed, unlike pdp11's `BIC`-only
// hardware.
// unary `~`: real `SETCM AC,AC` (`AC <- NOT C(AC)`, via the AC's own address as `E`).
// unary `-`: real `MOVN AC,AC` (`AC <- -C(AC)`).
// `<<`: real `LSH AC,N` (logical shift, positive `N` = left). `>>` unsigned: `LSH AC,-N`.
// `>>` signed: real `ASH AC,-N` (arithmetic, sign-extending; `ASH`'s left-shift behavior is
// otherwise identical to `LSH`'s, so only the signed right-shift case ever uses it). A
// *runtime* shift count is obtained by computing it into an AC and using that AC as the
// shift instruction's own index register with a zero displacement (`[[acn]+0]`, i.e. real
// index-mode addressing `0(ACn)`) - confirmed directly: this really does vary the shift
// amount at runtime, not a fixed literal.
//
// Comparisons compute `lhs - rhs` via `SUB` into an AC, then use the real `JUMPL`/`JUMPLE`/
// `JUMPE`/`JUMPN`/`JUMPGE`/`JUMPG AC,label` family (jump-on-AC-vs-zero - confirmed directly).
// Unsigned comparison has no native support either; both operands' sign bit (bit 35, value
// 0400000000000 octal) is flipped via `XOR` first, remapping unsigned order onto signed order
// (the same bias trick Pdp7Codegen.cpp's own `genWideCompare()` already uses for its own
// top-word unsigned case), then the ordinary signed path runs on the flipped values.
//
// Every branch/jump here (`JRST`, the `JUMPx` family) takes a full 18-bit absolute address in
// one instruction word - no pdp11-style short-branch range limit exists on this target, so
// there is no `safeBranch()`-style short-branch-plus-long-jump trick: every conditional here
// is one direct instruction.
//
// `long`/`long long` arithmetic: a value of word count N lives on the real hardware stack as N
// consecutive words (word 0/least-significant closest to the top, i.e. pushed last) - the same
// convention as Pdp11Codegen.cpp's own wide values, just on this target's upward-growing real
// stack instead of a software one. Multiply/divide on a wide operand aren't implemented
// (deliberately, mirroring Pdp11Codegen.cpp's own `genWideMul()`/`genWideDivMod()` - this
// target has no verified multi-word multiply/divide routine either); add/subtract/bitwise/
// compare/shift are, via a software ripple-carry chain (no hardware carry flag exists on this
// target to chain through, so each word's carry-out is detected in software via an unsigned
// compare of the raw sum against either addend).
class Pdp10Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp10"; }
    TypeWidths typeWidths() const override { return {1, 1, 1, 2, 4, 36}; }
    std::string sourceExtension() const override { return ".s10"; }
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
    void genExpr(const ir::Expr &e);  // leaves the result in AC1

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // `isUnsigned` flips both operands' sign bit (bit 35) before the signed `SUB`+jump -
    // see this class's own header comment.
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue, bool isUnsigned);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e), AC1 already holds 0/nonzero and is JUMPx-ready; for a wide value
    // (left as `words` words on the stack), ORs them all into AC1 first.
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Unsigned 36-bit divide: AC1 = dividend, AC2 = divisor on entry; AC1 = quotient,
    // AC2 = remainder on exit. Clobbers only AC1-AC7.
    void emitUnsignedDiv();
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);
    void genConvert(const ir::Expr &e);
    void genCompoundOp(const std::string &op, Type type);
    void genCompoundThroughAC3(const ir::Expr &e);
    // Masks AC1 down to `to`'s value range, re-sign-extending from that range's top bit for a
    // signed type - mirrors Pdp7Codegen.cpp's own narrowAC() exactly (same reason: every type
    // here still occupies a full native word of *storage*, but `char`'s C *value* range is
    // nonetheless fixed at 8 bits regardless of the machine's own word size). Masking only
    // ever happens when a `char` value is read, never when it's stored - a stray untruncated
    // upper bit in storage is invisible to any later read, which always re-masks.
    void narrowAC1To(Type to);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    int wordCountOf(Type t) const { return widthOfType(t); }
    // Every element (char/short/int alike) is one native word here, and a `long`/`long long`
    // array element still only moves one native word (see genIndex()) - so the scale factor
    // is always 1 regardless of declared element type.
    int elemWordsOf(Type) const { return 1; }

    void loadOperand(const std::string &mem, Type type);
    void storeOperand(const std::string &mem, Type type);
    void loadIndirect(const std::string &addrReg, Type type);
    void storeIndirect(Type type);

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    void genWideNegate(int words);
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    std::string wideWordOperand(const ir::Slot *slot, int wordIndex) const;
    // dest = a + b + carryCell (all memory cell operands); updates carryCell to this word's
    // carry-out, detected via an unsigned compare (no hardware carry flag exists here) -
    // see emitUnsignedLess().
    void emitWideAddWithCarry(const std::string &dest, const std::string &a, const std::string &b,
                               const std::string &carryCell);
    // Unsigned `C(a) < C(b)` (both memory/AC operands) - leaves AC1 = 0/1, AC2 clobbered.
    // Operands with the *same* sign bit compare safely via an ordinary signed `SUB` (same-sign
    // 36-bit values can't differ by more than the representable signed range); operands with
    // *differing* sign bits are decided directly from which one has the bit set (that one is
    // the unsigned-larger value) - a plain "flip the sign bit, then SUB" trick isn't safe on
    // its own here, since that subtraction itself can overflow when the two flipped operands
    // land at opposite extremes (confirmed by this exact failure during development - see
    // tests/longlong_add_carry_chain.c and friends). Mirrors Pdp7Codegen.cpp's own
    // emitSignedLtCells()'s same-sign/differing-sign split.
    void emitUnsignedLess(const std::string &a, const std::string &b);

    int localArrayBase(const ir::Slot *slot);
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's frame-pointer-relative address text - see this class's own header
    // comment for the sign convention (Params below AC16, Locals above it). `14`/`15` are
    // AC16/AC17's *decimal* register numbers (this dialect's assembler always parses a bare
    // number as decimal - see Pdp10Assembler.h's own header comment); every other place in
    // this file referring to AC16 ("F")/AC17 ("P") by their real, octal, DEC-documented names
    // likewise always emits `14`/`15`.
    std::string addr(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[14-" + std::to_string(slot->frameOffset) + "]";
        }
        return "[14+" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths())) + "]";
    }

    std::string operandOf(const ir::Slot *slot) const {
        return slot->kind == ir::SlotKind::Global ? label(slot) : addr(slot);
    }

    void pushReg(int reg) { code_ << "    push 15," << reg << "\n"; }
    void popReg(int reg) { code_ << "    pop 15," << reg << "\n"; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Allocates a fresh one-word static scratch cell (declared in the trailer) and returns
    // its label - used by the wide carry-chain/unsigned-compare helpers, which need a real
    // memory operand (not just an AC) to hold an intermediate value across several
    // instructions.
    std::string allocTempCell() {
        std::string lbl = allocTemp();
        trailer_.push_back(lbl + ": dw 0");
        return lbl;
    }
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
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;
};

}  // namespace rcc
