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
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
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
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
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
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

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
    // Shared tail of genMul()/genDivMod()/genShift() - the loop that
    // does the actual bit-serial work once their own operands are
    // already staged in the fixed memory cells each expects (see each
    // one's own call site), factored out so CompoundAssign/
    // CompoundDerefAssign's own genCompoundOp() below can drive the
    // identical loop from operands it has already staged itself,
    // rather than retyping it (every instruction here has to stay an
    // exact copy of one this assembler already assembles correctly
    // elsewhere - see I8008Assembler.h's own header comment).
    void genMulCore();                                  // bc = multiplicand, dx = multiplier -> ac = product
    void genDivModCore(bool wantRemainder, bool isUnsigned);  // bc = divisor, dx = dividend -> ac = quotient/remainder
    void genShiftCore(bool isLeft, bool isUnsigned);     // bc = value, cx = count -> ac = result
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);
    // Assumes `ptr` already holds the effective storage address -
    // loads/stores a value of `type`'s own width there into/from
    // `ac`, the same width dispatch genIndex()'s/the indexed Assign
    // case's own former inline logic used (now shared by both of
    // those and by CompoundAssign/CompoundDerefAssign's own
    // genCompoundThroughPtr() below).
    void loadOperandAtPtr(Type type);
    void storeOperandAtPtr(Type type);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step -
    // `old` already in `ac`, `rhs` already in `bc` (both already
    // promoted to this backend's native 16-bit word, per `type`'s own
    // signedness), leaves the raw (not yet re-truncated - see
    // narrowAcTo()) new value in `ac`. `type` is never wider than a
    // native word here - IRBuilder.cpp's own rejectIfWide() guarantees
    // it, so this never needs the `w*`/wide-value machinery above.
    // Its own "/"/"%" case (genDivModCore()) uses `ptr` itself as
    // scratch (this backend's divide routine has no spare cell that
    // doesn't - see genDivMod()'s own comment), so a caller holding an
    // address in `ptr` across this call (genCompoundThroughPtr() below)
    // has to re-stash it first, the same as across any other nested
    // codegen that's free to clobber it.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - `ptr` must already hold the resolved
    // address on entry (and nothing else live). See the .cpp's own
    // comment.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-truncates `ac` to `to`'s own width, re-extending per its own
    // signedness if narrower than a native word - just
    // emitSignExtendByte()'s own existing "ac"/"ac+1" idiom (see
    // genConvert()'s own narrowCharFixup), pulled out under this name
    // since genCompoundOp()'s own result needs the identical
    // re-truncation.
    void narrowAcTo(Type to);

    // Leaves slot[idxExpr]'s own effective address in the `ptr`
    // memory cell - see ../i8080/I8080Codegen.cpp's own genIndexAddr()
    // comment on why `idxExpr` is always evaluated first, and alone.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Local or Param's own byte offset within its function's own
    // per-call frame (see genFunction()'s own comment), generalized
    // from a fixed 2-bytes-per-slot word index to a real, possibly
    // narrower/wider byte count - the same fix (and the same reason)
    // ../m6502/M6502Codegen.cpp's own frameByteOffset() needed:
    // IRBuilder's own Slot::frameOffset already accounts for each
    // slot's own real width (see IR.h's own comment on it); a Param's
    // own reservation here is `2*nativeIntWidth` less than IRBuilder
    // assumes (this backend's own `fp` lands directly on param 0,
    // with no saved-frame-pointer word taking up space in-frame - see
    // genFunction()'s own comment: the caller's `fp` is saved on the
    // separate `fpsp` chain instead), and a Local's own region starts
    // right after the real total byte size of every param (not
    // `paramCount*2`, now that a param can be wider or narrower).
    // Only ever called while generating code for the function that
    // actually owns `slot` - `currentParamCount_`/`currentParamBytes_`
    // are that function's own counts, set once by genFunction() for
    // the whole of its own body. A Global has no frame offset at all
    // (it never reaches this function - see pointAtSlot()).
    int frameByteOffset(const ir::Slot *slot) const;

    // Points the real H,L registers at `slot`'s own storage,
    // `extraByteOffset` bytes past its first byte - the Local/Param-
    // or-Global-agnostic equivalent of pointAt(), used everywhere a
    // *slot*, rather than a plain named cell, needs addressing. A
    // Global still resolves to pointAt() on its own fixed label
    // (byte-for-byte the same code this backend always emitted for
    // one); a Local or Param instead loads `fp`'s own runtime value
    // into H,L (loadHLFromCell()) and adds frameByteOffset() plus
    // `extraByteOffset` on top (addConstToHL()) - there is no
    // compile-time label for one of these to begin with, since `fp`
    // itself differs on every call.
    void pointAtSlot(const ir::Slot *slot, int extraByteOffset = 0);
    // Advances H,L from `slot`'s byte `justPointedOffset` (wherever a
    // just-made `pointAtSlot(slot, justPointedOffset)` left it, with
    // nothing in between touching H,L) to the very next byte. A
    // Global's own next byte is still a single compile-time label, so
    // another plain pointAtSlot() stays cheapest there; a Local/Param
    // instead reuses the H,L that pointAtSlot() already computed via
    // incHL(), rather than reloading `fp` and re-adding the frame
    // offset from scratch for a destination exactly one byte past
    // where H,L already sits - the same "point once, incHL() for the
    // next byte" idiom storeAcIndirect() already uses against a plain
    // named cell.
    void pointAtSlotNextByte(const ir::Slot *slot, int justPointedOffset);

    // "pointAt(cell); mov a, m" / "pointAt(cell); mov m, a" - the one-
    // byte read/write every wide (`long`/`long long`) helper below is
    // built out of; `storeByte` expects its own value already in A
    // (pointAt() itself never touches A, so a `loadByte` immediately
    // followed by a `storeByte` of a different cell is always safe,
    // with nothing in between needing to preserve A).
    void loadByte(const std::string &cell);
    void storeByte(const std::string &cell);
    // Sign/zero-extends the byte at `valueCell` (per `isUnsigned`)
    // into the single byte at `highDestCell` - 0x00 or 0xFF - via
    // `and a` (an 8008 idiom that clears CY as a side effect, without
    // disturbing A - see decHL()'s own comment) followed by `ral`
    // (rotating A's own top bit into CY), since this target has no
    // direct way to test a bit otherwise. Used for a `char`'s own
    // promotion to `int`, and for a `long`/`long long` conversion's
    // own fill byte.
    void emitSignExtendByte(const std::string &valueCell, const std::string &highDestCell, bool isUnsigned);

    // --- `long`/`long long` ("wide") values - see
    // ../m6502/M6502Codegen.h's own header comment on this whole
    // mechanism: three more fixed 8-byte memory-cell buffers
    // (`wac`/`wbc`/`wtmp`/`wtmp2`), byte-addressed little-endian,
    // with this backend's own real `sp`-indexed runtime stack (see
    // pushWide()/popWide() below - a byte-granular version of the
    // existing word-granular pushCell()/popCell()) used only as a
    // transient save slot for one operand while the other is
    // evaluated.
    void pushWide(const char *base, int nbytes);
    void popWide(const char *base, int nbytes);
    void copyWide(const char *dst, const char *src, int nbytes);
    void negateWideBuffer(const char *buf, int nbytes);

    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    void genWideLoadAtPtr(int words);   // wac = the value at the address stored in `ptr`
    void genWideStoreAtPtr(int words);

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words, bool invert);
    void emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);

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
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
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
    // The current function's own total param byte count (sum of
    // max(widthOfType(p->type), 2) over every param) - see
    // frameByteOffset()'s own comment; set once per genFunction()
    // call alongside currentParamCount_.
    int currentParamBytes_ = 0;
    // The current function's own single shared epilogue label - every
    // `return` in its body (see genStmt()'s own Return case) jumps
    // here instead of each emitting its own full copy of
    // emitEpilogue()'s own real frame-teardown code (a handful of
    // instructions, but real ones, not something `--optimize` folds
    // away across separate call sites) plus `ret`; genFunction() lands
    // this same body right after the function's own last statement,
    // so control simply falls into it when nothing explicitly returns.
    // Set once per genFunction() call, for the whole of that
    // function's own body.
    std::string currentEpilogueLabel_;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;

    // Every real 8008 instruction genWideDivMod() emits operates on
    // the same fixed, globally-named cells (`wbc`/`wac`/`wtmp`/`wtmp2`/
    // `dx`) regardless of which source-level `/`/`%` expression asked
    // for it, so - unlike this backend's every other genExpr() helper,
    // each inlined fresh at its own call site - the whole routine is
    // already shareable as a single real subroutine, called via `cal`/
    // `ret` from every `/`/`%` on a given (signedness, word count)
    // pair instead of being duplicated byte-for-byte at each one. A
    // `long`/`long long` program with several divisions or moduli
    // (see tests/types_longlong_divmod.c) would otherwise multiply
    // this routine's own considerable size by its own number of call
    // sites - see getWideDivModSubroutine()'s own comment. Keyed by
    // (isUnsigned, words); the label is created, and its body queued
    // in wideDivModSubroutineBodies_, the first time a given pair is
    // ever requested.
    std::map<std::pair<bool, int>, std::string> wideDivModSubroutineLabel_;
    std::vector<std::string> wideDivModSubroutineBodies_;
    // Returns the shared subroutine's own label for (isUnsigned,
    // words), emitting its body (once, into
    // wideDivModSubroutineBodies_) the first time this pair is asked
    // for. On entry, `wbc` holds the dividend and `wac` the divisor
    // (both `words`-word wide); on return, `wtmp` holds the remainder
    // and `wtmp2` the quotient - the call site itself (genWideDivMod())
    // still does the final `copyWide` into `wac`, since that part
    // alone differs (`wtmp` vs `wtmp2`) by `wantRemainder`, not by
    // signedness or width.
    std::string getWideDivModSubroutine(bool isUnsigned, int words);

    // Same sharing idea as getWideDivModSubroutine() - and for the
    // same reason (tests/types_longlong_divmod.c's own several `!=`
    // checks on `long`/`long long` would otherwise each duplicate
    // this routine in full) - applied to the byte-wise XOR/OR scan
    // emitWideEqualityCompare() needs. Equality doesn't care about
    // signedness at all, so this is keyed on `words` alone. On entry,
    // `wbc` and `wac` hold the two operands (`words`-word wide); on
    // return, the single byte at `dx` is zero iff they were equal -
    // the call site itself still does the final `wantEqual`-dependent
    // branch, since that part alone differs between `==` and `!=`.
    std::map<int, std::string> wideEqualitySubroutineLabel_;
    std::vector<std::string> wideEqualitySubroutineBodies_;
    std::string getWideEqualitySubroutine(int words);
};

}  // namespace pcc
