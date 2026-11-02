#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PIC18 assembly (this project's own dialect, but using
// real, standard PIC18 mnemonics throughout - see PIC18Assembler.h)
// for the IR built by IRBuilder. Targets a concrete, real chip - the
// classic PIC18F2420 - closely enough to matter: its real Access
// Bank SFR aliasing (STATUS=0xFD8, FSR0L/FSR0H=0xFE9/0xFEA,
// INDF0/POSTINC0=0xFEF/0xFEE, INTCON=0xFF2 - all addressable via
// their own 8-bit Access Bank short form, 0x60-0xFF mapping onto
// 0xF60-0xFFF automatically whenever an instruction's own access bit
// is 0, which every instruction this backend emits always sets), its
// real 16K-word program memory (0x00000-0x03FFF, so unlike PIC16's
// own direct-11-bit GOTO/CALL, this core's genuine 20-bit absolute
// GOTO/CALL range never needs a paging workaround here), and its
// real 31-level hardware call stack. `generate()` returns a complete,
// self-contained program starting at the real hardware reset vector
// (0x000000), halting in an infinite loop - the same "no stack, no
// recursion" model every backend shares.
//
// See PIC16Codegen.h's own header comment for the aspects this
// backend shares outright with its mid-range sibling: the Harvard
// code/data split (and the real startup code that follows from it -
// data memory has no "loaded image" the way program memory does, so
// every global's own bytes are explicitly set at `START:`, not baked
// into the `--format=hex` image the way a unified-memory backend's
// own globals are); the fixed 2-byte pseudo-registers (`ac`/`bc`/
// `cx`/`dx`); the real `SSTACK`/`SP` software push/pop stack a binary
// op's own "save the other operand across evaluating this
// subexpression" needs (a compile-time-indexed fixed cell is unsound
// here - a nested call reuses the same cell at runtime and clobbers
// whatever the caller was still holding in it; see PIC16Codegen.h's
// own header comment for the full reasoning, which applies verbatim
// to this backend too); `ptr` being one byte, not two (this target's
// own scope restriction below keeps every address this backend ever
// computes under 8 bits too); no hardware multiply/divide (shift-
// and-add / repeated-subtraction loops, only verified against non-
// negative operands); no variable-count shift (RLCF/RRCF always
// rotate exactly one bit).
//
// Scope restriction: like the PIC16 backend's own 68-byte-GPR limit,
// this backend only ever allocates data in the 96-byte Access Bank
// GPR window (0x00-0x5F) - real banked GPR beyond that (this chip has
// up to 768 bytes total) is never used, so no BSR bank-select code is
// ever needed (every instruction's own access bit, `a`, is always 0 -
// see PIC18Assembler.h's own header comment).
//
// What's genuinely different from PIC16, and why (the actual point of
// having a separate backend rather than just aliasing PIC16's):
//
// - ADDWFC/SUBWFB (carry-chained add/subtract) are real PIC18-only
//   instructions - a genuine enhanced-core addition over PIC16's
//   plain ADDWF/SUBWF. 16-bit add/subtract here is the textbook
//   *shorter* idiom this makes possible (low byte via plain ADDWF/
//   SUBWF, high byte via ADDWFC/SUBWFB continuing the same carry/
//   borrow chain) - no PIC16-style manual BTFSC+ADDLW-1 fixup needed
//   at all (see PIC16Codegen.h's own header comment on why *that*
//   backend needs one).
// - STATUS has real N (negative) and OV (overflow) bits that PIC16's
//   own STATUS lacks entirely (see PIC16Codegen.h's own header
//   comment on using the sign-bit-bias trick instead, for that exact
//   reason) - so signed comparisons here use the standard N-xor-OV
//   rule directly (see emitLessCompare()'s own comment), the same
//   idiom the 6502/8086/68000 backends use via their own hardware
//   overflow flag, rather than any bias trick.
// - FSR0/INDF0/POSTINC0 (a real, hardware-auto-incrementing indirect-
//   addressing register triple) replace PIC16's own FSR/INDF-plus-
//   explicit-INCF pair - one real instruction fewer per 16-bit
//   indirect access (see storeAcIndirect()/loadAcIndirect()'s own
//   comment).
// - MOVFF (a real two-word register-to-register move, bypassing W
//   entirely) replaces the MOVF-then-MOVWF pair every other backend
//   here needs for a plain copy - same code size (two words either
//   way), but leaves W/STATUS completely undisturbed, which happens
//   to make it a strictly safer choice for copy2() than the 8080/
//   6502/PIC16 backends' own W-based copy ever needs to be here.
class PIC18Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic18"; }
    std::string sourceExtension() const override { return ".pic18"; }
    std::vector<std::string> assembledFormats() const override { return {"hex"}; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

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
    void genIndex(const ir::Expr &e);
    void genAddrOf(const ir::Expr &operand);

    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Via FSR0L/INDF0/POSTINC0 - see this file's own header comment
    // on why this core's real auto-increment indirect mode needs one
    // fewer instruction than PIC16's own FSR/INDF-plus-INCF pair.
    void storeAcIndirect();
    void loadAcIndirect();

    // "movff src,dst\nmovff src+1,dst+1\n" - see this file's own
    // header comment on MOVFF.
    void copy2(const std::string &dst, const std::string &src);
    void clear2(const std::string &lbl);
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // 16-bit `dst = a + b` / `dst = a - b` via ADDWFC/SUBWFB's own
    // real carry/borrow chain - see this file's own header comment.
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    void inc16(const std::string &lbl);

    void testAcZero();
    void gotoIfZero(const std::string &target);
    void gotoIfNonZero(const std::string &target);

    // Real runtime push/pop on the shared `SSTACK` region via `SP`/
    // FSR0/POSTINC0/INDF0 - see this file's own header comment.
    void pushCell2(const std::string &src);
    void popCell2(const std::string &dst);
    void pushPtrCell();
    void popPtrCell();

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    void declareRam(const std::string &lbl, int count, bool zeroAtStart = true);

    std::ostringstream code_;
    std::vector<std::string> ramDecls_;
    std::vector<std::pair<std::string, int>> zeroAtStart_;
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
};

}  // namespace pcc
