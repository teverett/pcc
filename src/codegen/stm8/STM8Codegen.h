#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
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
// lives in one of several fixed, two-byte memory cells - `ac`
// (genExpr()'s own result), `bc`/`cx`/`dx` (binary-op/multiply/divide
// scratch), `ptr` (an indirection target) - exactly the 8080 backend's
// own convention, just with `x`/`y` as the transit registers instead
// of HL. Two more, `fp`/`fsp`, play the role a real frame-pointer/
// stack-pointer register pair would on a target that has one to
// spare (see genPrologue()/genSlotAddress() below) - `x`/`y` are
// already fully committed to the role above, so there's no hardware
// register left over to hold either one live across a function body.
// Each cell is declared high-byte-first (see declareTemp()) -
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
    // `role`'s own `words`-word value, as individual byte labels -
    // see STM8Codegen.cpp's own header comment. Public (unlike every
    // other helper below) because it's also used by a free function
    // in that file's own anonymous namespace.
    static std::string wideScratch(const char *role, int i) { return std::string("w") + role + std::to_string(i); }

    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "stm8"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s8m"; }
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
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

    // Establishes this call's own frame (saves the caller's `fp`,
    // points `fp` at that saved word, reserves `frameSize` words of
    // local storage below it) and the matching restore - see
    // STM8Codegen.cpp's own header comment on `fp`/`fsp` for the
    // layout. Every `return` (explicit or falling off the end of the
    // function body) runs genEpilogue() immediately before its own
    // `ret` - there's no shared epilogue label to jump to, the same
    // "duplicated at each return site" shape as I386Codegen.cpp's own
    // genFunction()/Return case.
    void genPrologue(const ir::Function &fn);
    void genEpilogue(const ir::Function &fn);

    // Pushes `cell`'s own two bytes onto the frame stack (`fsp` moves
    // down by one word first, then the value is stored at the new
    // `fsp`) - the frame-stack equivalent of a real hardware `push`,
    // used both by genPrologue() (to save the caller's `fp`) and by
    // the Call case (to place each argument in the new callee's own
    // frame before the real `call`).
    void pushFrameWord(const std::string &cell);

    // Leaves a Slot's own effective storage address in `x`: a
    // Global's fixed label address, a Param's `fp + (paramIndex+1)*2`
    // (the callee's own incoming-argument area, just above the saved
    // `fp` - see pushFrameWord()'s own call sites), or a Local's
    // `fp - frameOffset*2` (below the saved `fp`, growing downward as
    // frameOffset increases - see STM8Codegen.cpp's own header
    // comment on why a Local array's element i is therefore at
    // *decreasing* addresses as i increases, the opposite of a
    // Global array). Every Local/Param access - read, write, &-of, or
    // as an Index/array base - goes through this one function; only
    // a Global still uses its own fixed compile-time label directly.
    void genSlotAddress(const ir::Slot *slot);

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // After genExpr(e) has just run, leaves `ac` holding 0 (false) or
    // nonzero (true) - for a wide value (left on the real hardware
    // stack as `words` words, LSW on top), ORs them all together into
    // `ac` first and discards that stack space, since a multi-word
    // value is true iff ANY of its words is nonzero.
    void testTruthy(const ir::Expr &e);
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    // Leaves slot[idxExpr]'s own effective address in the `ptr`
    // memory cell - same role as I8080Codegen::genIndexAddr, via `x`/
    // `y` instead of HL/DE.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // How many native 16-bit words `t` occupies - 1 for every type up
    // to `int`, 2 for `long`, 4 for `long long` (see typeWidths()).
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3) - the
    // number of real one-bit `slaw`s genIndexAddr() scales an index
    // by, instead of the old fixed "always one" (*2).
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Assumes `x` already holds the effective storage address (same
    // precondition as loadAcIndirect()/storeAcIndirect()) and loads/
    // stores `ac` with a value of `type`'s own width - a `char` only
    // ever touches one byte (`ldxind`/`stxind`), sign/zero-extending
    // the rest of `ac` on a load via genSignFillByte(); every wider
    // type is the existing two-byte `loadAcIndirect()`/
    // `storeAcIndirect()`.
    void loadOperand(Type type);
    void storeOperand(Type type);

    // `role`'s own `words`-word value, as individual byte labels,
    // least significant byte first - `wideScratch(role,0)` is that
    // byte, `wideScratch(role,0)+"+1"` the next, `wideScratch(role,1)`
    // the one after that, and so on. Each word is a real 2-byte
    // `db hi, lo` cell (see declareTemp()) - `wideScratch(role,i)`
    // itself names the HIGH byte (matching every other named cell's
    // own big-endian layout - see this file's own header comment), so
    // the LOW byte is always `wideScratch(role,i)+"+1"`; swapping
    // these would silently corrupt the first `adc`/`sbc` of every
    // multi-byte op, since it's the chip's own *low* byte, not the
    // high one, that has no incoming carry to fold in (see this
    // class's own public wideScratch() declaration above).
    // Pops `words` words off the real hardware stack (LSW on top -
    // see genWideAdd()'s own header comment) into `role`'s own
    // wideScratch() cells.
    void popWideInto(const char *role, int words);
    // Pushes `role`'s own wideScratch() cells onto the real hardware
    // stack, LSW ending up on top.
    void pushWideFrom(const char *role, int words);
    // Leaves word `wordIndex`'s (0 = least significant) own effective
    // address in `x` - a Global's storage is genuinely big-endian
    // (the most significant word first - see generate()'s own
    // comment), so word `wordIndex` sits at an *increasing* offset as
    // `wordIndex` *decreases*; a Local/Param's own multi-word scalar
    // storage is one contiguous block starting at genSlotAddress()'s
    // own address, laid out the same way.
    void genWideWordAddr(const ir::Slot *slot, int words, int wordIndex);
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    // The same "words"-word, most-significant-word-first layout
    // genWideWordAddr()/genWideLoad()/genWideStore() use for a Slot's
    // own storage, except the base address is a runtime pointer value
    // already sitting in `addrCell` (a named 16-bit memory cell, e.g.
    // `ptr`) instead of a Slot's compile-time-known address - used to
    // dereference a wide (`long`/`long long`) pointee, whose address
    // is only known at runtime. pushWideFromAddress() reads `words`
    // consecutive words starting there and pushes them onto the real
    // hardware stack, LSW on top (matching every other genWide*()
    // load); popWideToAddress() is the reverse, popping LSW first and
    // writing back the same way.
    void pushWideFromAddress(const std::string &addrCell, int words);
    void popWideToAddress(const std::string &addrCell, int words);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideNegate(int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    void genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words);
    void genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    // `long`/`long long` `*`/`/`/`%` need a genuine bit-serial
    // multi-byte software routine - deliberately not implemented
    // here: this backend has no simulator in this project's own test
    // harness to verify one against at all (the same situation this
    // project's own pdp11/i8080 backends are in - see
    // Pdp11Codegen.cpp's own genWideMul() comment) - failing loudly is
    // safer than shipping an unverifiable, possibly wrong one.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, int words);
    // Leaves `a` holding 0xFF if `a` (already loaded, a signed byte)
    // has its own top bit set, else 0x00 - see STM8Codegen.cpp's own
    // comment.
    void genSignFillByte();

    // Assumes `x` already holds a target address (genIndexAddr's
    // `ptr`, or DerefAssign's own pointer value) and writes `ac`'s two
    // bytes there one at a time via `ldxind`/`stxind` (this dialect's
    // own stand-ins for real `LD A,(X)`/`LD (X),A` - see
    // STM8Assembler.h) with an `incw x` between them - doesn't restore
    // `x` afterward, same convention as I8080Codegen::storeAcIndirect.
    void storeAcIndirect() { storeIndirect("ac"); }
    // The reverse: reads the two bytes at `x` into `ac`.
    void loadAcIndirect() { loadIndirect("ac"); }

    // The same two conventions, generalized to any named 16-bit cell
    // (not just `ac`) - used by genSlotAddress()'s own callers to
    // read/write a Local/Param through its just-computed frame
    // address, and by genEpilogue() to recover the saved caller's
    // `fp` without disturbing `ac`'s own live return value.
    void storeIndirect(const std::string &src);
    void loadIndirect(const std::string &dst);

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
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db hi, lo, ..." / "label: dw ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // The function genStmt()/genExpr() are currently generating code
    // for - only ever needed by a Return statement, to find its own
    // enclosing function's `params.size()` for genEpilogue(). Set by
    // genFunction() around its own genStmt(*fn.body) call; never
    // null while generating a function body.
    const ir::Function *currentFunction_ = nullptr;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
