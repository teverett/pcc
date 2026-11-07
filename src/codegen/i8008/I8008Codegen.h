#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Intel 8008 assembly for the IR built by IRBuilder. Key 8008 hardware constraints this backend works
// around:
//  - No stack pointer or PUSH/POP; CAL/RET use an internal 7-level return-address stack (a hard call-nesting limit).
//    This backend builds two software stacks in ordinary RAM instead: `sp` (kStackBase) for saving a subexpression's
//    value across evaluating another part of the same expression (pushAc()/popInto()), and a frame-pointer chain
//    (`fp`/`fnext`/`fpsp`, kFrameBase/kFpStackBase) giving each call its own fresh Local/Param storage.
//  - Every call gets a fresh frame (bump-allocated via `fnext`, with `fp` as its base), making recursion safe: a
//    Param's value travels on the `sp` stack rather than through any shared named cell, since a shared cell would be
//    clobbered by a recursive call still in flight.
//  - No 16-bit instructions at all, not even DAD: every +/- is two 8-bit ops chained through carry (ADD+ADC,
//    SUB+SBB; ADI/ACI for adding a constant to H,L or a cell).
//  - No direct-addressing load/store (no LDA/STA/LHLD/SHLD): memory is only reachable as "the byte at H,L" (register
//    code `m`). pointAt() loads H,L from a fixed label's split bytes; pointAtSlot() instead adds a word offset to
//    `fp` for a Local/Param, since those have no compile-time address.
//  - AND/XOR/OR against the accumulator exist as single instructions, same as later chips.
//  - RLC/RRC/RAL/RAR rotate the accumulator only (no Z80-style `RL r`), so every 1-bit shift detours through A
//    (shiftOneBit()).
//  - Register codes differ from the 8080/Z80: a=0,b=1,c=2,d=3,e=4,h=5,l=6,m=7 (8080/Z80 use b=0..m=6,a=7).
//  - No hardware multiply/divide (`*` is shift-and-add, `/`/`%` repeated subtraction, non-negative operands only)
//    and no signed-overflow flag (`<`/`<=`/`>`/`>=` use the sign-bit-bias trick, as in the 8080 backend).
//
// `int` is 16 bits, same as every other backend.
class I8008Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8008"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s08"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are relocatable object formats for separate compilation.
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
    // Bit-serial core loop shared by genMul()/genDivMod()/genShift() and by genCompoundOp(), operating on operands
    // already staged in the fixed memory cells each caller sets up.
    void genMulCore();                                  // bc = multiplicand, dx = multiplier -> ac = product
    void genDivModCore(bool wantRemainder, bool isUnsigned);  // bc = divisor, dx = dividend -> ac = quotient/remainder
    void genShiftCore(bool isLeft, bool isUnsigned);     // bc = value, cx = count -> ac = result
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);
    // Assumes `ptr` already holds the effective storage address; loads/stores a value of `type`'s width into/from
    // `ac`.
    void loadOperandAtPtr(Type type);
    void storeOperandAtPtr(Type type);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: `old` in `ac`, `rhs` in `bc`, leaves the raw
    // (not yet re-truncated) new value in `ac`. `type` is never wider than a native word here (IRBuilder.cpp's
    // rejectIfWide() guarantees it). Its "/"/"%" case uses `ptr` itself as scratch, so a caller holding an address in
    // `ptr` across this call must re-stash it first.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign; `ptr` must already hold the resolved
    // address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-truncates `ac` to `to`'s width, re-extending per signedness if narrower than a native word.
    void narrowAcTo(Type to);

    // Leaves slot[idxExpr]'s effective address in the `ptr` cell; `idxExpr` is evaluated first and alone.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Local or Param's byte offset within its function's per-call frame. Slot::frameOffset already accounts for
    // each slot's real width; a Param's reservation here is `2*nativeIntWidth` less than IRBuilder assumes (`fp`
    // lands directly on param 0 - the caller's `fp` is saved on the separate `fpsp` chain instead), and a Local's
    // region starts right after the real total byte size of every param. Only valid while generating the function
    // that owns `slot` (currentParamCount_/currentParamBytes_ are set once per genFunction() call). A Global never
    // reaches this function.
    int frameByteOffset(const ir::Slot *slot) const;

    // Points H,L at `slot`'s storage, `extraByteOffset` bytes past its first byte - the slot-aware equivalent of
    // pointAt(). A Global resolves via its fixed label; a Local/Param loads `fp` and adds frameByteOffset() plus
    // `extraByteOffset`, since `fp` differs on every call.
    void pointAtSlot(const ir::Slot *slot, int extraByteOffset = 0);
    // Advances H,L from `slot`'s byte `justPointedOffset` (left there by a preceding pointAtSlot() with nothing in
    // between touching H,L) to the next byte: a fresh pointAtSlot() for a Global, incHL() for a Local/Param.
    void pointAtSlotNextByte(const ir::Slot *slot, int justPointedOffset);

    // "pointAt(cell); mov a, m" / "pointAt(cell); mov m, a" - the one-byte read/write every wide helper is built
    // from. `storeByte` expects the value already in A.
    void loadByte(const std::string &cell);
    void storeByte(const std::string &cell);
    // Sign/zero-extends the byte at `valueCell` into the single byte at `highDestCell` (0x00 or 0xFF) via `and a`
    // (clears CY without disturbing A) then `ral` (rotates the sign bit into CY) - the 8008 has no direct bit test.
    void emitSignExtendByte(const std::string &valueCell, const std::string &highDestCell, bool isUnsigned);

    // --- `long`/`long long` ("wide") values: four 8-byte buffers (`wac`/`wbc`/`wtmp`/`wtmp2`), byte-addressed
    // little-endian, with the `sp`-indexed runtime stack (pushWide()/popWide(), byte-granular versions of
    // pushCell()/popCell()) as a transient save slot for one operand while the other is evaluated.
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

    // Adds the compile-time constant `byteOffset` to H,L via the ADI/ACI carry chain.
    void addConstToHL(int byteOffset);
    // The same add, against a named 16-bit cell instead of H,L; used to bump `fnext` in genFunction()'s prologue.
    void addConstToCell(const std::string &cell, int amount);

    // Points H,L at the compile-time-known address `lbl` ("mvihi h, lbl\nmvi l, lbl\n") - how this backend addresses
    // any named memory cell.
    void pointAt(const std::string &lbl);
    // Loads H,L from the 16-bit value stored at cell `cell` ("LHLD cell"-equivalent via pointAt() plus two MOVs
    // through D/E, since the 8008 has no direct-addressing load). Used when H,L must become a runtime-computed
    // address rather than a compile-time label.
    void loadHLFromCell(const std::string &cell);
    // Writes a compile-time-resolvable 16-bit expression (literal or label) into cell `dst`, low byte then high
    // byte via `mvihi`.
    void storeImmediate16(const std::string &dst, const std::string &exprText);
    // Writes `ac`'s two bytes to the address stored in `addrCell`: reads both bytes into B/C first (since pointing
    // H,L at `addrCell`'s value would otherwise clobber `ac` mid-read), then writes them through H,L.
    void storeAcIndirect(const std::string &addrCell);
    // 16-bit increment of the H,L register pair: "inr l; if that wrapped 0xFF->0x00, inr h too" (no hardware 16-bit
    // increment on this chip).
    void incHL();

    // Copies one 16-bit memory cell to another by reading both bytes through A into B/C, then writing them out -
    // four real memory accesses, since H,L can't hold both addresses at once.
    void copy16(const std::string &dst, const std::string &src);
    // Same copy, with `slot`'s storage (Global/Local/Param, via pointAtSlot()) on the other end.
    void copy16FromSlot(const std::string &dst, const ir::Slot *slot);
    void copy16ToSlot(const ir::Slot *slot, const std::string &src);
    // Writes a compile-time-resolvable 16-bit expression into `slot`'s storage, `elemOffset` words past its first -
    // used for a Local array's literal initializer, re-written at runtime each time the declaration is reached
    // (unlike a Global array, baked into the binary once).
    void storeImmediate16ToSlot(const ir::Slot *slot, int elemOffset, const std::string &exprText);
    // Pops the top of the `sp` stack into `slot`'s storage; used by a function's prologue to move each incoming
    // argument (pushed by the caller onto `sp`) into that call's fresh frame.
    void popIntoSlot(const ir::Slot *slot);

    // Runtime software stack for saving a subexpression's value across evaluating another part of the same
    // expression; `sp` is a cell holding the current top-of-stack address in the fixed region kStackBase/kStackSize.
    // pushCell/popCell push/pop `srcCell`'s/`dstCell`'s value against the stack named by `spCell`, and must be
    // paired like a real push/pop. pushAc()/popInto() are the common case (`ac` against `sp`); `fp`'s save/restore
    // across a call uses pushCell/popCell directly against the separate `fpsp` chain.
    void pushCell(const std::string &srcCell, const std::string &spCell);
    void popCell(const std::string &dstCell, const std::string &spCell);
    void pushAc() { pushCell("ac", "sp"); }
    void popInto(const std::string &dst) { popCell(dst, "sp"); }
    // Inverse of loadHLFromCell(): writes H,L's current value out to `cell`.
    void storeHLToCell(const std::string &cell);
    // 16-bit decrement of H,L: tests whether L is already 0 before decrementing (borrow into H), then decrements L.
    void decHL();

    // One in-place 1-bit rotate of 16-bit cell `lbl` through the accumulator (the 8008's rotates work on A only).
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // A function's epilogue: gives this call's frame back to the bump allocator (`fnext = fp`) and restores the
    // caller's `fp` (popped off `fpsp`); emitted before every real `ret`.
    void emitEpilogue();

    // A label naming a fixed, compile-time-known address: a Global's storage (`g_<name>`) or a Function's entry
    // point (`fn_<name>`). A Local/Param has no such label (each call gets fresh runtime-addressed storage), so for
    // those this is only meaningful as a symbol-table key.
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
    // The function currently being generated's param count; set once per genFunction() call.
    int currentParamCount_ = 0;
    // The current function's total param byte count (sum of max(widthOfType(p->type), 2) over every param); set
    // once per genFunction() call.
    int currentParamBytes_ = 0;
    // The current function's single shared epilogue label; every `return` jumps here instead of duplicating
    // emitEpilogue()'s frame-teardown code at each site, and control falls into it when nothing explicitly returns.
    std::string currentEpilogueLabel_;
    // One entry per currently-open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;

    // genWideDivMod()'s routine operates on fixed globally-named cells regardless of call site, so it's shared as a
    // real `cal`/`ret` subroutine per (isUnsigned, words) instead of being duplicated at each division/modulo site.
    // The label is created, and its body queued in wideDivModSubroutineBodies_, the first time a pair is requested.
    std::map<std::pair<bool, int>, std::string> wideDivModSubroutineLabel_;
    std::vector<std::string> wideDivModSubroutineBodies_;
    // Returns the shared subroutine's label for (isUnsigned, words), emitting its body once on first request. On
    // entry, `wbc` holds the dividend and `wac` the divisor; on return, `wtmp` holds the remainder and `wtmp2` the
    // quotient (the call site still copies the one it wants into `wac`).
    std::string getWideDivModSubroutine(bool isUnsigned, int words);

    // Same sharing idea as getWideDivModSubroutine(), applied to the byte-wise XOR/OR scan emitWideEqualityCompare()
    // needs. Keyed on `words` alone (equality ignores signedness). On entry, `wbc`/`wac` hold the two operands; on
    // return, the byte at `dx` is zero iff they were equal (the call site does the `==`/`!=` branch).
    std::map<int, std::string> wideEqualitySubroutineLabel_;
    std::vector<std::string> wideEqualitySubroutineBodies_;
    std::string getWideEqualitySubroutine(int words);
};

}  // namespace pcc
