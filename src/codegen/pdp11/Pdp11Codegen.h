#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PDP-11 assembly (this project's own dialect - see
// Pdp11Assembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see Pdp11Codegen.cpp) - no RT-11/Unix, no boot
// loader, the test harness sets the simulated CPU's PC to
// `kBaseAddress` directly - halting via the real `HALT` instruction
// (with a `br` safety net right after it, in case a simulator ever
// just resumes at the next word instead of actually stopping - see
// generate()'s own comment), the same "no stack, no recursion" model
// every backend shares (see README's "Scope and limitations") - on a
// CPU with eight real 16-bit general registers and genuine register-
// indirect/autoincrement/autodecrement addressing, so (like the
// 68000 backend) almost everything maps onto one real instruction.
//
// This language's own `int` is 16 bits here (the PDP-11's native
// word size, matching every other 16-bit-word target's own choice -
// see I8086Codegen.h/M6502Codegen.h) - the PDP-11 is itself byte-
// addressed but every value this project ever stores is a full 16-
// bit word, so no byte-sized (`.B`-suffixed) instruction is ever
// emitted. Every value in flight lives in one of two general
// registers: `R0` (genExpr()'s own result - the "ac" every other
// backend also calls it) and `R1` ("the other operand" in a binary
// op - 8086's "bx"/68000's "d1"). `R2` holds a computed address when
// dereferencing one (array/pointer indexing, `*`/`&`) - the 68000
// backend's own "a0" role, played here by an ordinary general
// register plus the PDP-11's real register-deferred addressing mode
// (`(Rn)`, no special placement requirement, unlike the 6502's
// zero-page-bound `ptr` - see M6502Codegen.h's own comment). `SP`
// (`R6`) is the real hardware stack pointer - saving one operand
// across evaluating the other in a binary expression, and every
// function call/return, use real autodecrement/autoincrement moves
// and real `JSR`/`RTS`, no synthesis needed (unlike pdp7 - see
// Pdp7Codegen.cpp's own comment on why it has to improvise this with
// named memory cells instead).
//
// `*`/`/` use the real EIS hardware `MUL`/`DIV` (available on every
// PDP-11 model this project's own simulator target emulates) -
// `MUL src,R0` leaves a full signed 32-bit product in `R0:R1` (high
// word:low word - see genMul()'s own comment for why the *low* word,
// `R1`, is this project's own 16-bit truncated answer); `DIV src,R0`
// divides the 32-bit dividend in `R0:R1` by a 16-bit divisor, so the
// dividend is first sign-extended from 16 bits into that pair with
// `SXT` (see genDivMod()'s own comment) - the exact same "widen, then
// use the real hardware op" shape as the 8086 backend's own
// `CWD`+`IDIV` (see I8086Codegen.cpp's own `genDivMod()`), just with
// a dedicated sign-extend instruction instead of a dedicated
// widening-move one.
//
// `<<`/`>>` have no variable-count hardware shift this dialect uses
// (the EIS `ASH` instruction exists, but shifts arithmetically -
// sign-extending on a right shift - which would make `>>` behave
// differently from every other backend's own deliberately logical,
// unsigned convention; see genShift()'s own comment) - both loop one
// bit at a time instead, the same fix (and the same reason) as
// pdp7/6502's own `<<`/`>>`. A single-bit logical left shift is just
// `ASL` (bit 0 fills with 0 either way); a single-bit logical right
// shift is `CLC` (clear the carry flag) followed by `ROR` (rotates
// the cleared carry into bit 15, instead of `ASR`'s own sign-bit
// fill).
//
// Comparisons use the real hardware `CMP` + a real signed branch
// (`BLT`/`BLE`/`BGT`/`BGE`/`BEQ`/`BNE`) directly - the PDP-11's own
// condition codes already encode signed less-than/greater-than
// correctly in hardware (same reasoning as the 68000/8086 backends -
// see M68kCodegen.h's own comment), unlike the 6502, which needs an
// explicit N-xor-V fixup. `CMP src,dst` sets the flags from `src -
// dst` (the PDP-11's own, slightly unusual operand order - *not*
// `dst - src, so genCompare()'s own `CMP R1,R0` (R1=lhs, R0=rhs)
// computes `lhs - rhs` directly, with no operand-order inversion
// needed anywhere else in this file.
//
// Every conditional branch and `BR` here is always encoded as a
// short branch to an immediately-following label plus a full-range
// `JMP` (see Pdp11Codegen.cpp's own `safeBranch()`) - the PDP-11's
// own conditional/unconditional branch instructions can only reach
// ±128 words (±256 bytes) from the next instruction, unlike the
// 68000's always-16-bit-displacement `Bcc` (see M68kCodegen.h's own
// comment) or the 8086's `jcc rel8/rel16` (assembled automatically to
// whichever fits) - the same range problem (and the same safe-branch
// fix) as the 6502 backend's own `safeBranch()` (see M6502Codegen.h's
// own comment), just because this target's own branch displacement
// field happens to be just as narrow, not because this backend
// somehow lost the hardware's real signed condition codes the way
// the 6502 did.
class Pdp11Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp11"; }
    std::string sourceExtension() const override { return ".s11"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other five, every one of
    // which means a real relocatable object file for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend).
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
    void genExpr(const ir::Expr &e);  // leaves the result in R0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `R2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Encodes "if the branch condition holds, jump to `targetIfTaken`,
    // otherwise jump to `targetIfNot`" using a short branch to a
    // label immediately after it (so it's always well within the
    // PDP-11's own ±128-word branch range) followed by an
    // unconditional, full-range `jmp` - see Pdp11Codegen.h's own
    // header comment on branch range. `branchMnemonic` is one of
    // Pdp11Assembler.cpp's own conditional-branch mnemonics ("beq",
    // "bne", "blt", "ble", "bgt", "bge").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

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
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
