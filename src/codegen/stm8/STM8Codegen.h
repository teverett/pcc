#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates STM8 assembly (this project's own narrow dialect, built
// from real STM8 mnemonics - see STM8Assembler.h) for the IR built by
// IRBuilder. Same "bare flat binary" platform choice as the 6502/8080
// backends (see I8080Codegen.h): `generate()` returns a complete,
// self-contained program loaded at a fixed address (`kBaseAddress`,
// see STM8Codegen.cpp), halting in an infinite loop - no bootloader,
// no vector table setup beyond what a real chip's reset would do
// automatically.
//
// This language's own `int` is 16 bits, same as every non-pdp7/68000
// target. The STM8 has a real 8-bit accumulator `a` and two real
// 16-bit index registers, `x` and `y` - unlike the 8080's HL/DE/BC
// (which can at least hold a live operand across a nested
// subexpression's own HL-only M-addressing), an arbitrarily deep
// expression tree still needs arbitrarily many "the other operand"
// slots that `x`/`y` alone can't provide, so every value in flight
// lives in one of five fixed, two-byte memory cells - `ac`
// (genExpr()'s own result), `bc`/`cx`/`dx` (binary-op/multiply/divide
// scratch), `ptr` (an indirection target) - exactly the 8080 backend's
// own convention, just with `x`/`y` as the transit registers instead
// of HL. Each cell is declared high-byte-first (see declareTemp()) -
// the STM8 is a big-endian machine (`LDW` stores a word's high byte
// at the lower address), unlike every little-endian target here.
//
// `x`/`y` genuinely support register-register `ADDW`/`SUBW`/`CPW`, so
// a binary `+`/`-`/comparison needs no byte-at-a-time workaround -
// just "the other operand, saved across evaluating this one", via the
// real hardware stack (`PUSH`/`POP`, STM8's own `PUSHW`/`POPW`),
// exactly the role the 8080 backend's own real `PUSH H`/`POP D` plays
// (see I8080Codegen.h's own comment on why a named memory cell can't
// be used for that instead - a nested binary op would clobber it).
// There's no register-register `AND`/`OR`/`XOR` on `x`/`y` though (the
// STM8's own bitwise instructions only ever touch `a`), so `&`/`|`/`^`
// fall back to the 6502 backend's own byte-at-a-time approach, just
// using this target's real direct-addressed `AND A,mem`/`OR A,mem`/
// `XOR A,mem` instead of a load-op-store dance through a transit
// register.
//
// No hardware 16x16 multiply/divide (the real `MUL`/`DIV` only ever
// combine `a` with `x`, i.e. 8x8->16 and 16/8->16, not wide enough for
// this language's own 16-bit `*`/`/`/`%`) - same textbook shift-and-
// add / repeated-subtraction loops as the 8080/6502/pdp7 backends,
// only verified against non-negative operands. `<<`/`>>` loop one bit
// at a time too, via the real `SLAW`/`SRLW` (word-wide shift-by-one on
// `x`/`y` - the STM8 does have these, unlike the 8080's accumulator-
// only rotate), since there's no variable-count shift instruction
// either.
//
// This dialect's own conditional jumps (`jreq`/`jrne`/`jrult`/`jruge`)
// are deliberately *not* real STM8 `JRxx` (an 8-bit signed relative
// branch, +/-127 bytes - the same range limit the 6502 backend's own
// `safeBranch()` works around): since this backend's own in-process
// assembler never has to match a real STM8 toolchain's byte output
// (there's no available from-scratch STM8 emulator to cross-check
// against, unlike the 6502/8080 backends' py65/z80-python), every
// conditional jump here is defined as this dialect's own full 16-bit-
// absolute form instead, the same no-workaround-needed simplicity the
// 68000/pdp11 backends get for free from their own real hardware.
//
// Comparisons (`<`,`<=`,`>`,`>=`) use the same sign-bit-flip bias trick
// as the 8080 backend (see I8080Codegen.h's own comment): flip the
// top bit of each operand's high byte, then an ordinary unsigned
// `CPW`+`jrult`/`jruge` does the rest. `==`/`!=` need no such care -
// `CPW x,y` after loading both operands is a plain full-width compare.
class STM8Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "stm8"; }
    std::string sourceExtension() const override { return ".s8m"; }
    std::vector<std::string> assembledFormats() const override { return {"bin"}; }
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

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in the `ptr`
    // memory cell - same role as I8080Codegen::genIndexAddr, via `x`/
    // `y` instead of HL/DE.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Assumes `x` already holds a target address (genIndexAddr's
    // `ptr`, or DerefAssign's own pointer value) and writes `ac`'s two
    // bytes there one at a time via `ldxind`/`stxind` (this dialect's
    // own stand-ins for real `LD A,(X)`/`LD (X),A` - see
    // STM8Assembler.h) with an `incw x` between them - doesn't restore
    // `x` afterward, same convention as I8080Codegen::storeAcIndirect.
    void storeAcIndirect();
    // The reverse: reads the two bytes at `x` into `ac`.
    void loadAcIndirect();

    // "ldw x, src\nldw dst, x\n" - copies one 16-bit memory cell to
    // another via the real `x` register.
    void copy16(const std::string &dst, const std::string &src);
    // Shifts the 16-bit memory cell `lbl` left or right by exactly one
    // bit, in place, via `x` - the shared body of genShift()'s own
    // loop and genMul()/genDivMod()'s inline doubling/halving.
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
    void declareArray(const std::string &lbl, const std::vector<long> &values);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db hi, lo, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
