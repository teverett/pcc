#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
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
// generate()'s own comment) - on a CPU with eight real 16-bit general
// registers and genuine register-indirect/autoincrement/
// autodecrement addressing, so (like the 68000 backend) almost
// everything maps onto one real instruction.
//
// This language's own `int` is 16 bits here (the PDP-11's native
// word size, matching every other 16-bit-word target's own choice -
// see I8086Codegen.h/M6502Codegen.h) - the PDP-11 is itself byte-
// addressed but every value this project ever stores is a full 16-
// bit word, so no byte-sized (`.B`-suffixed) instruction is ever
// emitted. Every value in flight lives in one of three general
// registers: `R0` (genExpr()'s own result - the "ac" every other
// backend also calls it), `R1` ("the other operand" in a binary op -
// 8086's "bx"/68000's "d1"), and `R2` (a computed address when
// dereferencing one - array/pointer indexing, `*`/`&` - the 68000
// backend's own "a0" role, played here by an ordinary general
// register plus the PDP-11's real register-deferred addressing mode,
// `(Rn)`, no special placement requirement, unlike the 6502's
// zero-page-bound `ptr` - see M6502Codegen.h's own comment). `R5` is
// this backend's own dedicated frame-pointer register (see addr()
// below) - free for that role because nothing else here ever uses
// it. `SP` (`R6`) is the real hardware stack pointer: every function
// call/return uses real `JSR`/`RTS` (pushing/popping the real return
// address, no synthesis needed - unlike pdp7, see Pdp7Codegen.cpp's
// own comment on why it has to improvise this with named memory
// cells instead), every function's own prologue/epilogue pushes and
// pops the caller's `R5` on it, and every call's own arguments are
// pushed onto it by the caller before `jsr` and popped off by the
// caller after it returns (see genFunction()/the `Call` case in
// genExpr()). Every call therefore gets its own, independent stack
// frame - direct and indirect/mutual recursion both work, since two
// overlapping activations of the same function never share a Local's
// or Param's own storage the way one static memory cell per
// declaration would force them to.
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
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
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
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e) has just run, leaves r0 holding 0 (false) or
    // nonzero (true) and `TST r0`-ready for a branch - for a wide
    // value (left as `words` words on the stack), ORs them all
    // together into r0 first and discards that stack space, since a
    // multi-word value is true iff ANY of its words is nonzero.
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step -
    // `old` already in r0, `rhs` already in r1 (both already at
    // `type`'s own width/signedness), leaves the raw (not yet
    // re-narrowed - see narrowR0To()) new value in r0. `type` is never
    // wide here - IRBuilder.cpp's own rejectIfWide() guarantees it.
    // Every per-operator instruction form below is the exact same one
    // genBinary()/genShift()/genMul()/genDivMod() already use at this
    // same native width, just with both operands already sitting in
    // registers instead of freshly evaluated - `<<`/`>>` in particular
    // reuses genShift()'s own r0=value/r1=count loop body unchanged,
    // since that's already this entry convention with no swap needed.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - r2 must already hold the resolved address
    // on entry (and nothing else live). See the .cpp's own comment.
    void genCompoundThroughR2(const ir::Expr &e);
    // Re-narrows r0 from its own low byte, per `to`'s signedness, when
    // `to` is a `char` - the same two instruction shapes genConvert()
    // already uses for a narrow-to-narrow conversion, needed here
    // because a compound op computed at full register width can leave
    // garbage above a `char` target's own 8 bits (e.g. overflow past
    // bit 7). A no-op for every wider-than-`char` type.
    void narrowR0To(Type to);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // How many native 16-bit words `t` occupies - 1 for every type up
    // to `int`, 2 for `long`, 4 for `long long` (see typeWidths()).
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3) - the shift
    // amount genIndexAddr() scales an index by, instead of the old
    // fixed "always 2 bytes". Note: reading/writing a `long`/`long
    // long` *array element* itself (as opposed to a scalar) still
    // only moves one native word - see genIndexAddr()/genIndex()'s
    // own comment on this known gap.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the effective-address text `mem`
    // into r0 (a wide type's own N words sit on the stack after this
    // - see genWideLoad()'s own comment), sign/zero-extending a
    // narrower-than-int `char` per its own signedness via `movb`'s
    // real hardware auto-sign-extend-to-register behavior (see
    // Pdp11Assembler.cpp's own comment), masking the high byte
    // afterward when unsigned.
    void loadOperand(const std::string &mem, Type type);
    // Stores r0 into `mem`, truncating to `type`'s own width (a
    // `char` store writes only the low byte via `movb`, never
    // touching whatever shares the other byte of that word).
    void storeOperand(const std::string &mem, Type type);

    // The register-indirect counterpart of loadOperand() above, for a
    // dereferenced pointer (`*p`) whose own effective address is a
    // runtime value already sitting in `addrReg`, not a compile-time-
    // known slot operand - this dialect's `ldind` mnemonic only ever
    // addresses a bare `(Rn)` (no displacement - see
    // Pdp11Assembler.cpp's own comment), so a wide (`long`/`long long`)
    // pointee instead goes through the general `[Rn+N]` indexed operand
    // form (see Pdp11Assembler.cpp's own `classifyEA`) to reach its own
    // word 1, 2, ... Leaves r0 (narrow) or pushes
    // `wordCountOf(type)` words (wide, least significant on top), same
    // convention as every other genExpr case here.
    void loadIndirect(const std::string &addrReg, Type type);
    // `*lhs = rhs`'s own store, given that `r2` already holds `lhs`'s
    // pointer value pushed onto the stack (see the DerefAssign case's
    // own comment on why the address has to ride the data stack,
    // rather than stay in a register, across evaluating `rhs`) and
    // `rhs`'s own value then evaluated on top of it - r0 if narrow, or
    // `wordCountOf(type)` words (least significant on top) if wide.
    // Pops everything this leaves on the stack, recovering the address
    // last (underneath a wide rhs's own pushed words) and writing
    // through it at `type`'s own width.
    void storeIndirect(Type type);

    // `long`/`long long` arithmetic - a value of word count N lives
    // on the stack as N consecutive words, word 0 (least
    // significant) closest to `sp`, word N-1 (most significant)
    // furthest - evaluated by genExpr() and left there (see
    // genWideLoad()/genWideStore()'s own comment); every wide helper
    // below takes `words` (N) explicitly, so the identical ripple-
    // carry/bit-serial shape serves both `long` (2) and `long long`
    // (4) with no duplicated code.
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    // Negates the `words`-word value already on top of the stack, in
    // place.
    void genWideNegate(int words);
    // Loads `slot`'s own `words`-word value onto the stack (word 0 at
    // the new top), or pushes `words` words of 0/sign-extension when
    // `fromNarrow` is set (widening a narrower already-in-r0 value).
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    // Pops `words` words off the stack into `slot`'s own storage.
    void genWideStore(const ir::Slot *slot, int words);
    // Word `wordIndex` (0 = least significant) of `slot`'s own
    // multi-word storage - word 0 sits at `slot`'s own ordinary
    // (scalar) address, each later word 2 bytes further from `r5`/the
    // label (see IR.h's own comment on how a type wider than one word
    // occupies consecutive storage units).
    std::string wideWordOperand(const ir::Slot *slot, int wordIndex) const;

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

    // Only a Global (or a Function) ever gets a name-based label - a
    // Local or Param is addressed relative to `R5` instead (see
    // addr() below): it never crosses a file boundary, so it never
    // needs a label real separate compilation could resolve by name.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's own frame-pointer-relative effective-address
    // operand text, e.g. "[r5-4]"/"[r5+6]" - a real PDP-11 index-mode
    // operand (see Pdp11Assembler.h's own header comment on the
    // `[Rn+N]` bracket syntax this backend's assembler reads it as).
    // Every function gets a real per-call stack frame (see
    // genFunction()): `push r5`/`mov sp,r5` makes `r5` point at this
    // call's own saved old `r5`, with the return address one word
    // above that (`jsr` already pushed it, before `push r5` ran), and
    // the caller's own pushed arguments above that, in declaration
    // order (param 0 closest to the return address) - so a Param's
    // address is `r5+frameOffset`. Locals live below `r5`, reserved
    // by the prologue's `sub N,sp`; `Slot::frameOffset` is already
    // IRBuilder's own real byte offset for both a Param and a Local
    // (see IR.h's own comment on Slot::frameOffset), so a Local's
    // address is `r5-frameOffset`. Every call gets its own `r5`/
    // frame, so direct or indirect recursion never shares storage
    // between overlapping activations - unlike a Global, which still
    // gets the one real static cell a C global/static is supposed to
    // have.
    static std::string addr(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Param) {
            return "[r5+" + std::to_string(slot->frameOffset) + "]";
        }
        return "[r5-" + std::to_string(slot->frameOffset) + "]";
    }

    // The operand text for a plain read/write of `slot`'s own value -
    // the bare label for a Global (PC-relative addressing to the
    // value stored there - see label()'s own comment), or addr(slot)
    // ("[r5±N]") for a Local/Param.
    static std::string operandOf(const ir::Slot *slot) {
        return slot->kind == ir::SlotKind::Global ? label(slot) : addr(slot);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
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
