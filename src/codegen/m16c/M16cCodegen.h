#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Renesas M16C assembly (this project's own dialect - see
// M16cAssembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see M16cCodegen.cpp), halting in an infinite loop -
// no boot ROM, no on-chip peripheral setup - on a CPU with four real
// 16-bit general registers (`R0`-`R3`) plus two dedicated address
// registers (`A0`/`A1`), a dedicated frame-base register (`FB`, used
// exactly the way real M16C C compilers use it - see `frameOperand()`
// below) and a genuine hardware stack (`SP`, real
// `PUSH.W`/`POP.W`/`JSR.W`/`RTS`), so (like the pdp11/68000 backends)
// almost everything maps onto one real instruction, not a named
// memory cell the way the 8080/6502 backends have to improvise (see
// I8080Codegen.h's own comment for why those targets need that
// instead).
//
// Every activation gets its own real stack frame: `PUSH.W FB` saves
// the caller's frame base, `MOV.W SP,FB` makes `FB` the fixed point
// every Local/Param in this call addresses itself relative to (see
// `frameOperand()`), and `SUB.W N,SP` reserves this function's own
// locals below it. The epilogue's `MOV.W FB,SP` tears the whole frame
// down in one instruction regardless of how many locals it had, then
// `POP.W FB` restores the caller's own frame - this is what lets two
// overlapping activations of the same function (direct or indirect
// recursion) never share storage: each `JSR.W` gets a fresh `FB`/
// frame of its own. A Call's own arguments are pushed onto this same
// real stack right before the `JSR.W` (last-declared argument first,
// so argument 0 ends up closest to the return address `JSR.W` is
// about to push - matching `frameOperand()`'s own `FB+4+2*paramIndex`
// addressing), landing inside the callee's own fresh frame rather
// than any shared cell; the caller cleans them back off afterward
// (`ADD.W N,SP`). A Global still gets the one real static cell real
// C statics/globals are supposed to have - only Local/Param storage
// moved onto the per-call frame.
//
// This language's own `int` is 16 bits here (the M16C's native word
// size) - every value in flight lives in one of two general registers:
// `R0` (genExpr()'s own result - the "ac" every other backend also
// calls it) and `R1` ("the other operand" in a binary op). `A0` holds
// a computed address when dereferencing one (array/pointer indexing,
// `*`/`&`) - a dedicated address register, not an ordinary general one
// the way the pdp11 backend's own `R2` is (see Pdp11Codegen.h), so it
// never collides with `R0`/`R1` at all and - unlike that backend's own
// `genIndexAddr()` - never needs saving across a nested subexpression
// that only ever touches `R0`/`R1`; it's only saved (via the real
// stack) across a nested subexpression that might *itself* compute
// another address into `A0` (see genStmt()'s own Assign case).
//
// `*`/`/` use real M16C hardware `MUL.W`/`DIVU.W` - `MUL.W src,R0`
// leaves a full 32-bit product in `R2:R0` (high word:low word, this
// project's own 16-bit truncated answer always being the low word,
// `R0`); `DIVU.W src,R0` divides the 32-bit dividend in `R2:R0` by a
// 16-bit divisor, quotient into `R0`, remainder into `R2`. Only the
// *unsigned* divide is used (`R2` is simply zeroed before dividing,
// never sign-extended) - the same "only verified against non-negative
// operands" scope limitation every other software-divide backend here
// already documents (see I8080Codegen.h's own comment), just because
// this target's signed `DIV.W` would need its own sign-extension
// sequence this backend doesn't bother synthesizing.
//
// `<<`/`>>` use the real variable-count `SHL.W count,dst` directly -
// unlike every loop-one-bit-at-a-time backend here (pdp7/6502/pdp11/
// 8080/8086 - see their own comments), M16C's own `SHL.W` takes a
// *signed* count operand in hardware: positive shifts left, negative
// shifts right, so `>>` is simply `SHL.W` with the count register
// negated first (`NEG.W`) - one real instruction either way, no loop.
// Still the usual logical (unsigned) convention for `>>` (no unsigned
// type exists to make the distinction with) - this backend's own
// `R2`-left-as-zero `DIVU.W` convention above aside, there's no sign-
// extension anywhere in a right shift for that same reason.
//
// Comparisons use real hardware `CMP.W` + a real signed conditional
// jump (`JLT`/`JLE`/`JGT`/`JGE`/`JEQ`/`JNE`) directly - the M16C's own
// condition codes already encode signed less-than/greater-than
// correctly in hardware (same reasoning as the pdp11/68000/8086
// backends - see M68kCodegen.h's own comment), unlike the 6502, which
// needs an explicit N-xor-V fixup. This dialect's own `CMP.W src,dst`
// sets the flags from `dst - src` (matching every other two-operand
// mnemonic's own `dst = dst OP src` convention - see
// M16cAssembler.h's own header comment), so `genCompare()`'s own
// `CMP.W R0,R1` (R1=lhs, R0=rhs) computes `lhs - rhs` directly, with
// no operand-order inversion needed anywhere else in this file.
//
// Every jump and conditional jump here is always encoded as a full,
// unconditional-range absolute address - a deliberate simplification
// of this dialect (real M16C conditional jumps are short, PC-relative,
// and need a separate long-range form once a target is too far away -
// see M16cAssembler.h's own header comment), the same simplicity
// tradeoff the 8080 backend's own header comment makes for the exact
// same reason: it means no branch-range workaround (the pdp11/6502
// backends' own `safeBranch()`) is needed anywhere in this file.
class M16cCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "m16c"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s16c"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other four/`objraw`,
    // every one of which means a real relocatable object file for
    // separate compilation (see Codegen.h's own header comment on
    // why this is asked per-backend rather than assumed from the
    // format name alone).
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
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e) has just run, leaves r0 holding 0 (false) or
    // nonzero (true) - for a wide value (left as `words` words on the
    // stack), ORs them all together into r0 first and discards that
    // stack space, since a multi-word value is true iff ANY of its
    // words is nonzero.
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // How many native 16-bit words `t` occupies - 1 for every type up
    // to `int`, 2 for `long`, 4 for `long long` (see typeWidths()).
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3) - the shift
    // amount genIndexAddr() scales an index by, instead of the old
    // fixed "always 2 bytes".
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the effective-address text `mem`
    // into r0 - a `char` uses `movb`'s own real hardware auto-sign-
    // extend-to-register behavior, masking the high byte back off
    // afterward when unsigned (see M16cAssembler.cpp's own comment);
    // every other width is a plain `mov.w`. A wide type's own N words
    // sit on the stack after this instead - see genWideLoad().
    void loadOperand(const std::string &mem, Type type);
    // Stores r0 into `mem`, truncating to `type`'s own width (a
    // `char` store writes only the low byte via `movb`, never
    // touching whatever shares the other byte of that word).
    void storeOperand(const std::string &mem, Type type);

    // `long`/`long long` arithmetic - a value of word count N lives
    // on the real stack as N consecutive words, word 0 (least
    // significant) closest to `sp`, word N-1 (most significant)
    // furthest - evaluated by genExpr() and left there; every wide
    // helper below takes `words` (N) explicitly, so the identical
    // shape serves both `long` (2) and `long long` (4) with no
    // duplicated code.
    //
    // This dialect's own two-operand instruction set has no hardware
    // carry flag an `add`/`sub` could chain through directly (unlike
    // the pdp11/i386/m68k backends' own `ADC`/`ADDX` - see
    // M16cCodegen.cpp's own `genWideAdd()`), so the carry/borrow out
    // of each word is instead synthesized from an unsigned comparison
    // (`jltu`) against the word's own pre-op value - see
    // M16cCodegen.cpp's own `genCarryBool()`.
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    // Negates the `words`-word value already on top of the stack, in
    // place (two's complement: NOT every word, then add 1 through the
    // same software carry chain as genWideAdd()).
    void genWideNegate(int words);
    // Pushes `slot`'s own `words`-word value onto the stack, word 0
    // (least significant) ending up on top.
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    // Pops `words` words off the stack into `slot`'s own storage.
    void genWideStore(const ir::Slot *slot, int words);
    // Word `wordIndex` (0 = least significant) of `slot`'s own
    // multi-word storage - word 0 sits at `slot`'s own ordinary
    // (scalar) address, each later word 2 bytes further from `fb`/the
    // label.
    std::string wideWordOperand(const ir::Slot *slot, int wordIndex) const;
    // Leaves `r1` holding `0xFFFF` if `r0` (already loaded) is
    // negative, else `0` - see M16cCodegen.cpp's own comment.
    void genSignFillWord();

    // Leaves slot[idxExpr]'s own effective address in `A0` - see this
    // file's own header comment on why that never needs saving across
    // an *ordinary* nested subexpression, only one that itself might
    // compute another address into `A0`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Global or a Function gets a *name*-based label (never `id`) -
    // real separate compilation means the file calling a function and
    // the file defining it compile with completely independent `id`
    // sequences, so only a label computable from stable information
    // means the same thing on both sides. A Local or Param never
    // crosses a file boundary and is addressed relative to `FB`
    // instead (see frameOperand() below) - it never gets a label at
    // all.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's own frame-relative operand text, e.g.
    // "[fb-4]"/"[fb+6]" (M16cAssembler.cpp's own `Disp` addressing
    // mode - see its header comment). `FB` points at this call's own
    // saved old `FB`, with the return address one word above that and
    // the caller's pushed arguments above that in declaration order
    // (argument 0 closest to it) - so a Param's address is
    // `FB+frameOffset`. Locals live below `FB`, reserved by the
    // prologue's `SUB.W N,SP`; `Slot::frameOffset` is already
    // IRBuilder's own real byte offset for both a Param and a Local
    // (see IR.h's own comment on Slot::frameOffset), so a Local's
    // address is `FB-frameOffset`.
    static std::string frameOperand(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Param) {
            return "[fb+" + std::to_string(slot->frameOffset) + "]";
        }
        return "[fb-" + std::to_string(slot->frameOffset) + "]";
    }

    // The operand text for a plain read/write of `slot`'s own value -
    // `label(slot)` for a Global, or `frameOperand(slot)` for a
    // Local/Param - used anywhere the two only differ in which of
    // those this is (an ordinary Var read/Assign write; see
    // genAddrOf()/genIndexAddr() for the places that still have to
    // tell them apart, since only a Global's own address folds into a
    // compile-time immediate the way `mov.w g_x,r0` does).
    static std::string operandOf(const ir::Slot *slot) {
        return slot->kind == ir::SlotKind::Global ? label(slot) : frameOperand(slot);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
