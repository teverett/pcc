#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Z80 assembly (this project's own dialect, but using real,
// standard Z80 mnemonics throughout - see Z80Assembler.h) for the IR
// built by IRBuilder. See ../i8080/I8080Codegen.h's own header
// comment for the shared design this backend starts from (memory-
// resident `ac`/`bc`/`cx`/`dx`/`ptr` pseudo-registers, a real
// hardware stack for saving one operand across evaluating the
// other) - the Z80 is object-code *compatible* with the 8080 (every
// 8080 instruction means the same thing on a Z80), but this backend
// doesn't just reuse that one (the way `--target=8088` reuses the
// 8086 backend outright - see main.cpp's own comment): the Z80 adds
// enough genuinely new instructions that a backend ignoring them
// would be leaving real, easy wins on the table, so this is its own
// independent backend, not a software alias.
//
// What the Z80 adds that this backend actually uses:
//  - `ADD HL,rr`/`SBC HL,rr` - real 16-bit add and *subtract-with-
//    carry*. The 8080 only ever got the add half (`DAD`) - see
//    I8080Codegen.h's own comment on why `-` needs a 4-instruction
//    byte-at-a-time borrow chain there. Here, `-`, `<`/`<=`/`>`/`>=`,
//    and `==`/`!=` all collapse to one `SBC HL,DE` (after `OR A` to
//    clear the incoming carry) - the Z flag and CY flag it sets are
//    exactly "operands equal" and "unsigned borrow" respectively,
//    with no manual byte-by-byte reconstruction needed at all.
//  - `LD rr,(nn)`/`LD (nn),rr` for BC/DE/SP, not just HL - the 8080
//    can only ever move a 16-bit value directly between memory and
//    HL (`LHLD`/`SHLD`); getting a value into BC or DE needs a real
//    stack round-trip through HL first (see I8080Codegen.h's own
//    `loadPair()`/`storePair()`). Here, `loadPair()`/`storePair()`
//    are each a single real instruction.
//  - `RL r`/`RR r` - a rotate-through-carry that targets *any*
//    register directly, not just the accumulator the way the 8080's
//    `RAL`/`RAR` are stuck doing - shiftOneBit() here is two
//    instructions instead of six.
//
// Still true on both chips, so still handled the same way as the
// 8080 backend: no hardware multiply/divide (`*` is shift-and-add,
// `/`/`%` repeated subtraction, both only verified against non-
// negative operands); no variable-count shift (still a one-bit-at-a-
// time loop, just a cheaper one - see shiftOneBit() above); no
// hardware signed-comparison flag combination (the Z80's status
// byte adds H and reuses P/V as an overflow flag after arithmetic,
// but there's still no single flag or condition code for "signed
// less-than" - so `<`/`<=`/`>`/`>=` still use the same sign-bit-bias
// trick I8080Codegen.h documents, just via one `SBC HL,DE` instead of
// two byte-wise `SUB`/`SBB`); every jump/call is a full 16-bit
// absolute address, so (like the 8080) there's no branch-range
// workaround needed anywhere here.
//
// `int` is 16 bits, same word-size reasoning as every other 16-bit-
// addressed backend (see I8080Codegen.h's own comment).
class Z80Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "z80"; }
    std::string sourceExtension() const override { return ".sz80"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary (see
    // generate()'s own header comment) - a completely different
    // concept from the other four/`objraw`, every one of which means
    // a real relocatable object file for separate compilation (see
    // Codegen.h's own header comment on why this is asked per-
    // backend: `--format=elf` means something else entirely for
    // `8086` - see I8086Codegen.h).
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots

    // Leaves slot[idxExpr]'s own effective address in the `ptr`
    // memory cell - see I8080Codegen.h's own genIndexAddr() comment
    // on why `idxExpr` is always evaluated first, and alone.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Every function gets a real per-call stack frame, built around
    // `ix` as the frame pointer (see Z80Codegen.cpp's own
    // genFunction() comment for the exact layout): a Local's or
    // Param's own storage is never a fixed label the way a Global's
    // is, since every call needs its own independent copy. HL has no
    // instruction that loads it from IX directly, so this goes
    // through the real hardware stack instead ("push ix" / "pop hl")
    // - the one real, standard way to copy one 16-bit register into
    // another on this chip when no direct `ld` form between them
    // exists. Leaves `slot`'s own effective address in HL; a Local
    // lives at `ix-2*(frameOffset+extraWords)` (frame storage grows
    // down from `ix` - see genFunction()'s own comment), a Param at
    // `ix+4+2*paramIndex` (past the saved `ix` and the return
    // address, both pushed before this call's own frame is
    // established). `extraWords` lets a Local array's own element i
    // reuse this same formula (frameOffset+i) without a caller
    // needing to synthesize its own displacement arithmetic.
    void loadFrameAddr(const ir::Slot *slot, long extraWords = 0);

    // Reads the 16-bit value at [HL] into the (loReg, hiReg) pair,
    // low byte first - the same `(HL)`-via-"m" idiom genIndex()/
    // storeAcIndirect() already use elsewhere in this file. HL ends
    // up one past the word read (pointing at the high byte).
    void readWordAt(const std::string &loReg, const std::string &hiReg);

    // Assumes HL already holds a target address and writes `ac`'s
    // two bytes there via plain accumulator loads/`(HL)` stores -
    // doesn't touch HL itself, so the caller's address survives.
    void storeAcIndirect();

    // "ld hl,(src)\nld (dst),hl\n" - one pair of real instructions,
    // same as I8080Codegen.h's own copy16().
    void copy16(const std::string &dst, const std::string &src);
    // Loads a 16-bit memory cell directly into DE or BC (`pairReg` is
    // "de" or "bc") - a single `LD rr,(nn)` instruction (the Z80's
    // own extended-opcode direct load that doesn't exist on the
    // 8080 - see this file's own header comment), not a stack round-
    // trip through HL.
    void loadPair(const std::string &pairReg, const std::string &src);
    // The reverse: a single `LD (nn),rr`.
    void storePair(const std::string &dst, const std::string &pairReg);
    // Shifts the 16-bit memory cell `lbl` left or right by exactly
    // one bit, in place, via HL - `OR A` clears the carry-in (same
    // idiom, and the same reason, as I8080Codegen.h's own `ANA A` -
    // no explicit "clear carry" instruction exists on either chip),
    // then a two-instruction `RL`/`RR` chain across L and H directly
    // (no need to shuttle through the accumulator first the way the
    // 8080's own RAL/RAR-only-works-on-A forces - see this file's own
    // header comment).
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
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // The function currently being generated's own frame-reservation
    // size in bytes (`frameSize*2` - see genFunction()) - every
    // `Return` needs this too, to unwind the same amount the
    // prologue reserved before it restores `ix`/`ret`s.
    int frameReserveBytes_ = 0;
};

}  // namespace pcc
