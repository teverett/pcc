#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Intel 8008 assembly (this project's own dialect, using
// real, standard 8008 mnemonics - see I8008Assembler.h) for the IR
// built by IRBuilder. The 8008 is the most primitive target here by
// a wide margin - Intel's first microprocessor, and it shows:
//
//  - No stack pointer, no PUSH/POP, nothing an ordinary program can
//    use as a stack at all. `CAL`/`RET` work (they use a real but
//    *internal*, invisible-to-software 7-level return-address
//    stack) - a hard, fixed hardware limit on how many functions
//    deep a call chain can ever be, recursive or not, with no way
//    around it on this chip - so real function calls still work, but
//    every other backend's own "save one operand across evaluating
//    the other" trick (the 8080/Z80 backends' own `PUSH`/`POP`, the
//    6502's `PHA`/`PLA`, pdp7's AC-save cell) needs a different
//    answer here, since there is no real stack to push onto at all.
//    This backend builds two of its own: an ordinary RAM region
//    (`kStackBase`/`kStackSize` in I8008Codegen.cpp) with its own
//    real runtime pointer cell (`sp`), for saving one subexpression's
//    value across evaluating another part of the same expression
//    (`pushAc()`/`popInto()` below); and a second region
//    (`kFrameBase`/`fnext`/`fp`, plus `kFpStackBase`/`fpsp` for the
//    chain of saved `fp`s) giving every function call its own fresh
//    Local/Param storage (see genFunction()'s own comment). Both are
//    addressed by an actual value computed while the program runs,
//    not by where the compiler happens to be in its own traversal -
//    the same reason a real hardware stack supports recursion and a
//    compile-time-indexed save slot wouldn't. Neither region has an
//    overflow check (see `kStackSize`'s own comment), but both stay
//    bounded in practice by the same 7-level hardware call-nesting
//    limit above, since every push onto either one happens within
//    the dynamic extent of some real `CAL`.
//  - Every function call gets its own fresh storage for its own
//    Locals and Params, making direct and indirect/mutual recursion
//    safe: `fp` holds the *current* call's own frame base address,
//    `fnext` the address one past the last word any call anywhere on
//    the chain has claimed so far. A function's own prologue
//    (genFunction()) saves the caller's `fp` (onto the `fpsp`-indexed
//    chain), takes `fnext` as this call's own frame base, and bumps
//    `fnext` past it by this function's own word count
//    (`params.size() + frameSize`); its epilogue gives that space
//    back (`fnext = fp`) and restores the caller's own `fp`. A Local
//    or Param's own address is always `fp` plus a compile-time-
//    constant word offset, computed at runtime (`pointAtSlot()`) -
//    never a fixed label the way a Global's is, since the whole
//    point is that the *value* of `fp` differs for every call. A
//    Param's own incoming value is never written through any shared,
//    named cell either (that would break the moment a function calls
//    itself with its own call still in flight, which is exactly what
//    recursion means) - it travels on the same `sp`-indexed stack as
//    any other saved subexpression value, pushed by the caller in
//    argument order and popped by the callee's own prologue in
//    reverse, straight into that call's own fresh frame.
//  - No 16-bit instructions of any kind - not even the 8080's `DAD`.
//    Every `+`/`-` here is two 8-bit accumulator operations chained
//    through the real carry flag (`ADD`+`ADC`, `SUB`+`SBB`) - the
//    8080 backend's own fallback for `-` specifically (see
//    I8080Codegen.h), now needed for `+` too; the same carry chain,
//    via `ADI`/`ACI`, is how a compile-time-constant word offset gets
//    added to a runtime frame address held in `H,L` or in a memory
//    cell (`addConstToHL()`/`addConstToCell()` below).
//  - No direct-addressing load/store at all (no `LDA`/`STA`/`LHLD`/
//    `SHLD` equivalent of any kind) - the *only* way to touch memory
//    is "the byte at the address currently in H,L" (register code
//    `m`, same role as every other backend's own M-addressing). A
//    Global is reached by pointing HL at its own fixed label first
//    (`pointAt()` below: two `MVI`s, loading H and L from that
//    label's own high/low bytes - the same assemble-time address-
//    splitting trick the 6502 backend's own `ldai`/`ldai_hi` uses); a
//    Local or Param is reached the same way in the end (through H,L,
//    via `m`), but HL gets there by adding a word offset to the
//    runtime value of `fp` instead (`pointAtSlot()`) - there is no
//    assemble-time label for one of these at all.
//  - Real hardware `AND`/`XOR`/`OR` against the accumulator exist
//    (unlike the shift instructions below, this is one place the
//    8008 isn't more primitive than its later descendants) - `&`/
//    `|`/`^` are a single real instruction per byte, same shape as
//    the 8080 backend's own.
//  - The only rotate instructions (`RLC`/`RRC`/`RAL`/`RAR`) work on
//    the accumulator *only* - unlike the Z80's `RL r`/`RR r` (see
//    Z80Codegen.h), there's no way to rotate a byte that isn't
//    already in A, so every 1-bit shift here detours every byte
//    through the accumulator (see shiftOneBit()) - cruder than even
//    the 8080's version, which at least reads the byte straight into
//    A via a plain `MOV`.
//  - Different register codes than every later chip in this family:
//    a=0,b=1,c=2,d=3,e=4,h=5,l=6,m=7 (the 8080/Z80 moved A to the
//    *end*, b=0..m=6,a=7 - see I8080Codegen.h's own comment) - purely
//    a historical accident, not a capability difference, but real
//    enough that this backend's own assembler needs its own register
//    table rather than reusing the 8080's.
//  - No hardware multiply/divide (`*` is shift-and-add, `/`/`%`
//    repeated subtraction, only verified against non-negative
//    operands) and no signed-comparison overflow flag (`<`/`<=`/`>`/
//    `>=` use the same sign-bit-bias trick as the 8080 backend - see
//    I8080Codegen.h's own comment) - same story as every other
//    software-multiply/divide, software-signed-compare backend here.
//
// `int` is 16 bits, same reasoning as every other backend (see
// I8080Codegen.h's own comment) - just a much more expensive 16 bits
// to work with on this particular chip.
class I8008Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8008"; }
    std::string sourceExtension() const override { return ".s08"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // different concept from the other five, every one of which means
    // a real relocatable object file for separate compilation (see
    // Codegen.h's own header comment on why this is asked per-
    // backend).
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
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in the `ptr`
    // memory cell - see ../i8080/I8080Codegen.cpp's own genIndexAddr()
    // comment on why `idxExpr` is always evaluated first, and alone.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Local or Param's own 0-based word index within its function's
    // own per-call frame (see genFunction()'s own comment): a Param
    // is simply its own `paramIndex`; a Local continues on right
    // after every one of the current function's own params, in
    // declaration order (`currentParamCount_ + frameOffset - 1`,
    // `frameOffset` already being 1-based - see IR.h's own comment on
    // it). Only ever called while generating code for the function
    // that actually owns `slot` - `currentParamCount_` is that
    // function's own param count, set once by genFunction() for the
    // whole of its own body. A Global has no word index at all (it
    // never reaches this function - see pointAtSlot()).
    int wordIndexOf(const ir::Slot *slot) const;

    // Points the real H,L registers at `slot`'s own storage, `wordOffset`
    // words (plus one more byte, if `highByte`) past its first word -
    // the Local/Param-or-Global-agnostic equivalent of pointAt(),
    // used everywhere a *slot*, rather than a plain named cell, needs
    // addressing. A Global still resolves to pointAt() on its own
    // fixed label (byte-for-byte the same code this backend always
    // emitted for one); a Local or Param instead loads `fp`'s own
    // runtime value into H,L (loadHLFromCell()) and adds the word
    // offset's byte equivalent on top (addConstToHL()) - there is no
    // compile-time label for one of these to begin with, since `fp`
    // itself differs on every call.
    void pointAtSlot(const ir::Slot *slot, int wordOffset = 0, bool highByte = false);

    // Adds the compile-time constant `byteOffset` to the real H,L
    // register pair in place, via the same ADI/ACI carry chain as
    // every other 16-bit add in this file (see this file's own header
    // comment) - the one piece pointAtSlot() needs that pointAt()
    // never did, since a Global's own address is already a complete,
    // single compile-time constant.
    void addConstToHL(int byteOffset);
    // The same add, but against a named 16-bit memory cell (`cell`)
    // instead of the real H,L pair - used to bump `fnext` past a
    // function's own freshly claimed frame in genFunction()'s own
    // prologue.
    void addConstToCell(const std::string &cell, int amount);

    // Points H,L at the compile-time-known address `lbl` -
    // "mvihi h, lbl\nmvi l, lbl\n" - the only way this backend can
    // ever address a *named* memory cell at all (see this file's own
    // header comment). Every read/write of a named cell goes through
    // this first.
    void pointAt(const std::string &lbl);
    // Loads the real H,L registers from the 16-bit *value currently
    // stored at* memory cell `cell` (an "LHLD cell"-equivalent, built
    // from pointAt() plus two MOVs through D/E, since the 8008 has
    // no direct-addressing load of any kind - see this file's own
    // header comment). Used whenever HL needs to become a *runtime-
    // computed* address (a pointer variable's own value, or `ptr`'s
    // own stored value) rather than a compile-time label's own
    // address (which pointAt() already gives directly).
    void loadHLFromCell(const std::string &cell);
    // Writes a compile-time-resolvable 16-bit expression (a literal
    // number, or a label naming its own address) into memory cell
    // `dst` - "mvi a, expr\nmov m, a\n" (low byte) then the same
    // with `mvihi` for the high byte. Used for IntLit and for `&var`
    // alike, since both are just "the compiler already knows this
    // 16-bit value at compile time" (see pointAt()'s own split-byte
    // trick, same idea the 6502 backend's own `ldai`/`ldai_hi` uses).
    void storeImmediate16(const std::string &dst, const std::string &exprText);
    // Writes `ac`'s two bytes to the address *stored in* memory cell
    // `addrCell` (a runtime-computed address, e.g. `ptr`) - reads
    // both of `ac`'s bytes into B/C first (since getting HL to point
    // at `addrCell`'s own *value* needs HL, which would otherwise
    // clobber `ac` itself mid-read - see loadHLFromCell()), then
    // points HL at the real target and writes them out, advancing HL
    // with incHL() in between.
    void storeAcIndirect(const std::string &addrCell);
    // "inr l; if that wrapped 0xFF->0x00, inr h too" - a 16-bit
    // increment of the HL *register pair* (not a memory cell) - the
    // only place this backend ever needs one, the same carry-chain
    // workaround the 6502 backend's own inc16() needs for the same
    // reason (no real 16-bit increment exists on either chip).
    void incHL();

    // "point at src, read both bytes through A into b/c, point at
    // dst, write them back" - copies one 16-bit memory cell to
    // another. Four real memory accesses (sixteen-ish instructions),
    // not the one or two instructions every other backend's own
    // copy16() manages - see this file's own header comment on why.
    void copy16(const std::string &dst, const std::string &src);
    // The same copy, but with `slot`'s own storage (Global, Local, or
    // Param - see pointAtSlot()) on the other end instead of a plain
    // named cell - every ordinary read (copy16FromSlot) or write
    // (copy16ToSlot) of a scalar Var/Assign/LocalDecl goes through
    // one of these two instead of copy16() directly.
    void copy16FromSlot(const std::string &dst, const ir::Slot *slot);
    void copy16ToSlot(const ir::Slot *slot, const std::string &src);
    // Writes a compile-time-resolvable 16-bit expression (see
    // storeImmediate16()'s own comment) into `slot`'s own storage,
    // `elemOffset` whole words past its first one - used only for a
    // Local array's own literal initializer (see genStmt()'s own
    // LocalDecl case), each element written at runtime every time
    // that declaration is reached, the same way a scalar Local's own
    // initializer expression is (unlike a Global array, which still
    // gets its initial values baked into the binary once, at a fixed
    // address, since a Global has exactly one storage for its whole
    // program lifetime).
    void storeImmediate16ToSlot(const ir::Slot *slot, int elemOffset, const std::string &exprText);
    // Pops the top of the `sp` stack (see pushCell()'s own comment)
    // straight into `slot`'s own storage - used only by a function's
    // own prologue, to move each of its own incoming argument values
    // (pushed by the caller, onto the very same `sp` stack - see
    // genExpr()'s own Call case) into that call's own fresh frame.
    void popIntoSlot(const ir::Slot *slot);

    // This backend's own real, runtime software stack for saving a
    // subexpression's value across evaluating another part of the
    // same expression (see this file's own header comment on why a
    // compile-time-indexed save slot doesn't work once a real
    // function call is involved) - `sp` is an ordinary memory cell
    // holding the current top-of-stack address, in the fixed RAM
    // region `kStackBase`/`kStackSize` (see I8008Codegen.cpp).
    // `pushCell(srcCell, spCell)` pushes `srcCell`'s current value
    // onto the stack pointed to by `spCell`; `popCell(dstCell,
    // spCell)` pops the top of that same stack into `dstCell` (which
    // may equal `srcCell`/`dstCell`'s own counterpart, or not - it
    // never touches any cell but the one named). Must be paired
    // exactly the way a real push/pop would be. `pushAc()`/
    // `popInto(dst)` are this file's own most common case (`srcCell`/
    // `dstCell` is always `ac`, `spCell` is always `sp`); `fp`'s own
    // save/restore across a call (see genFunction()'s own comment)
    // uses `pushCell`/`popCell` directly instead, against the
    // separate `fpsp`-indexed chain.
    void pushCell(const std::string &srcCell, const std::string &spCell);
    void popCell(const std::string &dstCell, const std::string &spCell);
    void pushAc() { pushCell("ac", "sp"); }
    void popInto(const std::string &dst) { popCell(dst, "sp"); }
    // Loads the real H,L registers from `cell`'s own two stored
    // bytes - see loadHLFromCell()'s own comment; this is its exact
    // inverse, writing the real H,L registers' *current* values out
    // to `cell` instead of reading them in.
    void storeHLToCell(const std::string &cell);
    // A 16-bit decrement of the real HL register pair, the mirror of
    // incHL() - tests whether L is already 0 *before* decrementing
    // (if so, decrementing is about to borrow into H), then
    // decrements L unconditionally.
    void decHL();

    // One in-place 1-bit rotate of the 16-bit memory cell `lbl`,
    // through the accumulator (see this file's own header comment on
    // why every byte has to detour through A here, unlike the Z80
    // backend's own `RL r`/`RR r`).
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // A function's own epilogue - gives this call's own frame back to
    // the bump allocator (`fnext = fp`) and restores the caller's own
    // `fp` (popped off `fpsp`) - emitted right before every real
    // `ret`, both the one genFunction() itself appends after the
    // function's whole body and the one every explicit Return
    // statement emits (see genStmt()'s own Return case).
    void emitEpilogue();

    // A label naming a fixed, compile-time-known address: a Global's
    // own storage (`g_<name>`), or a Function's own entry point
    // (`fn_<name>`). A Local or Param has no such label any more -
    // every call gets its own fresh storage at a runtime-computed
    // address (`fp` plus a word offset - see pointAtSlot()), so this
    // overload's own returned text is only ever meaningful as a
    // symbol-table key, never as a real memory address to point H,L
    // at directly; the one remaining place it's still read for a
    // Local/Param (an array initializer's own symbol element, see
    // arrayElemText()) only works today because no test actually
    // initializes a Local array with another Local's or Param's own
    // address - see I8008Codegen.cpp's own `generate()` comment on
    // why a Global array's own literal initializer never needs this
    // restriction (it's emitted once, as fixed data, not evaluated at
    // runtime).
    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." / "label: dw ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // The function currently being generated's own param count - see
    // wordIndexOf()'s own comment; set once per genFunction() call,
    // for the whole of that function's own body.
    int currentParamCount_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
