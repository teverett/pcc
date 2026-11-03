#pragma once

#include <map>
#include <optional>
#include <sstream>
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
//    stack), so real function calls still work - but every other
//    backend's own "save one operand across evaluating the other"
//    trick (the 8080/Z80 backends' `PUSH`/`POP`, the 6502's PHA/PLA,
//    pdp7's AC-save cell) needs a different answer here, since there
//    is no real stack to push onto at all. This backend builds its
//    own: a small, ordinary RAM region (`kStackBase`/`kStackSize` in
//    I8008Codegen.cpp) plus a real runtime stack-pointer cell (`sp`),
//    with `pushAc()`/`popInto()` below doing the pointer arithmetic
//    by hand. A *compile-time* recursion-depth counter was tried
//    first and is wrong: it only protects against a nested genExpr()
//    call still being compiled as part of the same expression tree,
//    not against a real `CAL` to an already-compiled function body
//    that happens to reuse the exact same depth for its own,
//    completely unrelated save - `result + f(x)` breaks the moment
//    `f`'s own body does any saving of its own at the same compile-
//    time depth the caller is still relying on across the call. A
//    genuine runtime stack has no such problem, the same reason a
//    real hardware one wouldn't either - it's addressed by an actual
//    value computed while the program runs, not by where the
//    compiler happens to be in its own traversal. `kStackSize`
//    bounds how much of this gets pushed at once across the whole
//    program - generous for ordinary test programs, but a real,
//    documented limit with no overflow check (see pushAc()).
//  - No 16-bit instructions of any kind - not even the 8080's `DAD`.
//    Every `+`/`-` here is two 8-bit accumulator operations chained
//    through the real carry flag (`ADD`+`ADC`, `SUB`+`SBB`) - the
//    8080 backend's own fallback for `-` specifically (see
//    I8080Codegen.h), now needed for `+` too.
//  - No direct-addressing load/store at all (no `LDA`/`STA`/`LHLD`/
//    `SHLD` equivalent of any kind) - the *only* way to touch memory
//    is "the byte at the address currently in H,L" (register code
//    `m`, same role as every other backend's own M-addressing).
//    Reading or writing any named memory cell therefore always means
//    pointing HL at it first (`pointAt()` below: two `MVI`s, loading
//    H and L from that label's own high/low bytes - the same
//    assemble-time address-splitting trick the 6502 backend's own
//    `ldai`/`ldai_hi` uses, applied here to every single memory
//    access instead of just a handful of them).
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

    // This backend's own real, runtime software stack (see this
    // file's own header comment on why a compile-time depth counter
    // doesn't work once a real function call is involved) - `sp` is
    // an ordinary memory cell holding the current top-of-stack
    // address, in the fixed RAM region `kStackBase`/`kStackSize`
    // (see I8008Codegen.cpp). `pushAc()` pushes `ac`'s current
    // value; `popInto(dst)` pops the top of the stack into memory
    // cell `dst` (which may be `ac` itself, or any other cell - it
    // never touches `ac` unless told to). Must be paired exactly the
    // way a real push/pop would be.
    void pushAc();
    void popInto(const std::string &dst);
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
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." / "label: dw ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
