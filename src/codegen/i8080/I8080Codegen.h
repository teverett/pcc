#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Intel 8080 assembly (this project's own dialect, but
// using real, standard 8080 mnemonics throughout - see
// I8080Assembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see I8080Codegen.cpp), halting in an infinite
// loop - no OS, no CP/M, no BDOS calls - the same "no stack, no
// recursion" model every backend shares (see README's "Scope and
// limitations"), just for a target whose own real hardware stack
// (the 8080 genuinely has one, via SP/PUSH/POP/CALL/RET) gets used
// for exactly the roles it's good at: real subroutine calls, and
// saving one operand across evaluating the other in a binary
// expression - the same role the 6502 backend's real-but-byte-at-a-
// time PHA/PLA plays (see M6502Codegen.h), just now a single `PUSH
// H`/`POP D` moves a whole 16-bit pair at once.
//
// This language's own `int` is 16 bits here (matching every other
// target's own 16-bit choice except pdp7/68000 - see
// I8086Codegen.h/M6502Codegen.h's own comments on why 16 bits is the
// right width even on an 8-bit CPU). The 8080 has three real 16-bit-
// capable register pairs (BC, DE, HL) plus the 8-bit accumulator A,
// but none of them survive a recursive-descent expression evaluator
// cleanly on their own - an arbitrarily deep expression needs
// arbitrarily many "the other operand, saved somewhere" slots, and
// three pairs isn't enough once dereferencing is in the mix (M-
// addressing, the 8080's one real indirect-memory mode, *always*
// means "through HL", so HL can never safely hold a live operand at
// the same time a nested subexpression needs to dereference a
// pointer). So, like the 6502 backend, every value in flight lives
// in one of five fixed, two-byte memory cells - `ac` (genExpr()'s
// own result), `bc` ("the other operand" in a binary op), `cx`/`dx`
// (multiply/divide's own extra scratch), `ptr` (an indirection
// target) - ordinary labels, not the 6502's special zero-page-only
// ones (the 8080 has no addressing-mode restriction on where a 16-
// bit memory operand can live - `LHLD`/`SHLD` take any absolute
// address), so these are declared exactly like any other global (see
// declareTemp()). HL is this backend's own transit register: loaded
// from a memory cell immediately before an operation needs a real
// register, stored back immediately after - `LHLD`/`SHLD` move a
// whole pair in one instruction, so this is far cheaper than the
// 6502's own byte-by-byte `lda`/`sta` pairs. Getting a memory cell's
// value into DE or BC (needed by `DAD rp`, which only ever adds a
// *register* pair to HL, never a memory operand) goes through HL
// first and then a real `PUSH H` / `POP D` (or `POP B`) - two
// instructions, not the 6502's four-instruction byte copy.
//
// No hardware multiply or divide (same as pdp7/6502 - see their own
// backends' comments; the 68000 is the only target here with real
// `MULS`/`DIVS`), so `*` is a textbook shift-and-add loop and `/`,
// `%` a simple repeated-subtraction loop, both only verified against
// non-negative operands - the same scope limitation every other
// software-multiply/divide backend already documents.
//
// No variable-count shift instruction either (the 8080's only shift/
// rotate instructions - RLC/RRC/RAL/RAR - always rotate the
// accumulator exactly one bit, and only the accumulator, never a
// register pair) - `<<`/`>>` loop one bit at a time, chaining RAL/RAR
// across both bytes of the pair the same way the 6502 backend's own
// ASL/ROL or LSR/ROR pairing does (see M6502Codegen.h), just using
// `ANA A` first to force the carry-in to 0 (the 8080 has no explicit
// "clear carry" instruction the way the 6502 has CLC - any logical
// instruction always clears carry as a side effect, and ANA A is the
// standard idiom for doing that without otherwise disturbing A).
//
// Comparisons (`<`,`<=`,`>`,`>=`) can't use the 6502/8086/68000 trick
// of a hardware overflow flag plus the result's sign bit (the 8080's
// status word - S, Z, AC, P, CY - has no overflow flag at all, unlike
// the 6502's V or the Z80's reused P/V). Instead this backend uses
// the standard bias trick: flipping just the sign bit (top bit of the
// high byte) of both operands maps the signed 16-bit range onto the
// unsigned one while preserving order (`a < b` signed `<=>` `(a XOR
// 0x8000) < (b XOR 0x8000)` unsigned, since XOR-ing the sign bit is
// exactly addition of 0x8000 mod 2^16, which is what maps two's-
// complement ordering onto unsigned ordering) - so the comparison
// becomes a plain unsigned subtract-and-check-the-final-borrow, with
// no flag-combination logic needed at all. `==`/`!=` need none of
// this - a 16-bit subtract-and-OR-the-bytes zero check, same as every
// other backend's own equality compare.
//
// Every real 8080 jump/call is an unconditional-range 16-bit absolute
// address - unlike the 6502's ±127-byte conditional branches (see
// M6502Codegen.h's own `safeBranch()`), there's no branch-range
// workaround needed anywhere in this backend, the same simplicity the
// 68000 backend's own header comment notes for its full-range `Bcc`.
class I8080Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8080"; }
    std::string sourceExtension() const override { return ".s80"; }
    std::vector<std::string> assembledFormats() const override { return {"raw"}; }
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
    // memory cell - the only thing in this backend that ever needs a
    // real address in a register (HL, via M-addressing) rather than
    // just a value passing through it.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Assumes HL already holds a target address (genIndexAddr's
    // `ptr`, or DerefAssign's own pointer value) and writes `ac`'s
    // two bytes there via plain accumulator loads/M-stores - doesn't
    // touch HL itself, so the caller's address survives.
    void storeAcIndirect();

    // "lhld src\nshld dst\n" - copies one 16-bit memory cell to
    // another in a single pair of real instructions (no byte-by-byte
    // workaround needed - see this file's own header comment).
    void copy16(const std::string &dst, const std::string &src);
    // Loads a 16-bit memory cell into DE or BC (`pairReg` is "d" or
    // "b") via HL and a real stack round-trip: "lhld src\npush
    // h\npop d\n" - the only way to get a memory value into a pair
    // other than HL, since `LHLD` only ever targets HL.
    void loadPair(const std::string &pairReg, const std::string &src);
    // The reverse: "push d\npop h\nshld dst\n".
    void storePair(const std::string &dst, const std::string &pairReg);
    // Shifts the 16-bit memory cell `lbl` left or right by exactly
    // one bit, in place, via HL - the shared body of genShift()'s own
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
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
