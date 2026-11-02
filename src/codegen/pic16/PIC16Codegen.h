#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PIC16 mid-range (14-bit core) assembly (this project's
// own dialect, but using real, standard PIC16 mnemonics throughout -
// see PIC16Assembler.h) for the IR built by IRBuilder. Targets a
// concrete, real chip - the classic PIC16F84A - closely enough to
// matter: its real SFR map (INDF=0, STATUS=3, FSR=4, INTCON=0x0B),
// its real 68-byte GPR file-register bank (0x0C-0x4F), its real
// 11-bit direct GOTO/CALL range (program memory 0x000-0x3FF, 1024
// words - well inside that range, so unlike some larger mid-range
// parts, PCLATH paging is never needed here), and its real 8-level
// hardware call stack. `generate()` returns a complete, self-
// contained program starting at the real hardware reset vector
// (0x0000), halting in an infinite loop - the same "no stack, no
// recursion" model every backend shares (see README's "Scope and
// limitations").
//
// Harvard architecture: program memory (14-bit words, holding code)
// and data memory (byte-addressed file registers, holding every
// variable) are two genuinely separate address spaces - unlike every
// other backend here, a label can never mean "an address to load a
// value from" and "a place code lives" at the same time. This
// backend's own in-process assembler (see PIC16Assembler.h) tracks
// them as two independent cursors, `org` for code and its own private
// `orgram` for data; codegen itself allocates data-space storage via
// its own private `res` directive (`declareRam()`, below) rather than
// the "label: db values" trailer every unified-memory backend here
// uses.
//
// A second real consequence of that Harvard split: data memory is
// genuinely unknown at power-on (no "loaded binary image" to give it
// a known value the way every other backend's `db`-declared globals
// get for free) - so, unlike any other backend here, this one emits
// *real startup code* (see generate()'s own `START:` prologue) that
// explicitly clears or loads every global's own bytes before `main`
// ever runs, the same real embedded-C "crt0 initializes .data/.bss"
// step every PIC C compiler does - not a stylistic choice, a genuine
// requirement of this target's own memory architecture. Locals,
// params and this backend's own pseudo-registers are left alone at
// startup (always written before read, whether by codegen or by
// ordinary C "uninitialized local" semantics), exactly as every other
// backend already assumes.
//
// This language's `int` is 16 bits (the usual project-wide choice -
// see I8080Codegen.h's own comment on why 16 bits is still right even
// on an 8-bit CPU). The PIC16 core has exactly one real register
// (`W`, the accumulator) and no instruction wide enough to move more
// than one byte at a time, so - like the 6502/8080 backends - every
// 16-bit value in flight here lives in a fixed, two-byte file-
// register pseudo-register: `ac` (genExpr()'s own result), `bc` ("the
// other operand" in a binary op - but see below on how it gets there),
// `cx`/`dx` (multiply/divide/shift scratch), `ptr` (one byte only -
// see below). `W` itself is this backend's own transit register,
// loaded from a pseudo-register immediately before an ALU op needs
// it, stored back immediately after.
//
// No real general-purpose stack at all (unlike the 6502/8080, the
// PIC16 core's one hardware stack is a dedicated, non-addressable
// 8-level *return-address* stack used automatically by CALL/RETURN -
// there's no PUSH/POP-equivalent for ordinary data). So "save the
// other operand across evaluating this subexpression" (genBinary()'s
// own `+`/`-`/etc., and every comparison) needs a *real* software
// stack in file-register space after all: a dedicated `SSTACK`
// region plus an `SP` cursor byte, pushed/popped via FSR/INDF
// (pushCell2()/popCell2(), pushPtrCell()/popPtrCell() below). A
// compile-time-indexed scheme (one fixed cell per static nesting
// depth, no runtime pointer) looks tempting and costs nothing at
// runtime - but it's unsound the moment a call can happen between a
// push and its matching pop: the callee's own codegen was compiled
// independently and starts back at depth 0, so it reuses the exact
// same fixed cell the *caller* is still holding a live value in
// across that very call (e.g. `total + f()` - the `+`'s own saved
// left operand sits in a fixed cell while `f()` runs, and `f()`'s own
// body statistically starts fresh at the same nesting depth, clobbering
// it). A real stack has no such aliasing problem: PUSH/POP nest
// correctly around a CALL exactly because the hardware return-address
// stack and this software one are two genuinely independent
// structures, so a callee push/popping its own values never disturbs
// anything the caller pushed earlier.
//
// `ptr` - the one thing in this backend that ever needs to be a real
// indirection target (via FSR/INDF, the PIC16 core's one indirect-
// addressing mode) - is a single byte, not two: this target's entire
// usable data space (the 68-byte GPR bank above) fits in 7 bits, so
// every address this backend ever computes already fits in one byte
// (see PIC16Assembler.h's own `fileAddr()` 7-bit check) - no low/high
// split needed the way a 6502/8080 pointer needs one. pushPtrCell()/
// popPtrCell() push/pop it on the same shared `SSTACK` (one byte
// instead of two) - needed wherever evaluating a subexpression (a
// nested dereference or a call, most often) might reuse `ptr` itself.
//
// No hardware multiply or divide (same as pdp7/6502/8080), so `*` is
// a textbook shift-and-add loop and `/`, `%` a simple repeated-
// subtraction loop, both only verified against non-negative operands
// - the same scope limitation every other software-multiply/divide
// backend here already documents. No variable-count shift either
// (RLF/RRF always rotate exactly one bit) - `<<`/`>>` loop one bit at
// a time.
//
// No ADDWFC/SUBWFB (carry-chained add/subtract) on this core - those
// are a PIC18-only enhancement (see PIC18Codegen.h's own comment on
// how much simpler this makes PIC18's own 16-bit arithmetic) - so
// 16-bit add/subtract here use the standard PIC16 textbook idiom:
// compute the low byte first, then explicitly fold STATUS<C> (the
// carry/no-borrow flag) into the high byte's own operand before that
// second ADDWF/SUBWF, via BTFSC/BTFSS + ADDLW 1 (see genBinary()'s
// own comment).
//
// Comparisons (`<`,`<=`,`>`,`>=`) can't use a hardware overflow flag
// the way 6502/8086/68000 do - PIC16's STATUS register has no V bit
// at all, the same real gap I8080Codegen.h's own header comment
// documents for the 8080. So this backend uses that exact same fix:
// the standard sign-bit-bias trick (XOR 0x80 into both operands' high
// bytes, mapping signed order onto unsigned order), then a plain
// unsigned subtract-and-check-the-final-borrow.
class PIC16Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic16"; }
    std::string sourceExtension() const override { return ".pic16"; }
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
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective (one-byte) file address in
    // `ptr`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Assumes `ptr` already holds a target file address; writes
    // `ac`'s two bytes there via FSR/INDF.
    void storeAcIndirect();
    // The reverse: reads the two bytes at `ptr` into `ac`.
    void loadAcIndirect();

    // "movf src,w / movwf dst / movf src+1,w / movwf dst+1" - copies
    // one 2-byte pseudo-register/slot to another.
    void copy2(const std::string &dst, const std::string &src);
    // Zeros a 2-byte pseudo-register/slot: "clrf lbl\nclrf lbl+1\n".
    void clear2(const std::string &lbl);
    // One in-place 1-bit rotate of the 2-byte cell `lbl`, used by
    // genShift()'s own loop and inlined by genMul()/genDivMod() - no
    // variable-count shift instruction exists on this core (see this
    // file's own header comment).
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // 16-bit `dst = a OP b` (OP is "add" or "sub", selecting ADDWF/
    // SUBWF) via the standard PIC16 carry-fixup idiom (see this
    // file's own header comment) - shared by genBinary()'s `+`/`-`
    // and every comparison's own subtract-and-check-borrow.
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    // 16-bit increment via INCFSZ's own "skip next if the result just
    // wrapped to zero" idiom - the standard PIC16 textbook way to
    // chain a carry out of an INCF with no ADDWFC available.
    void inc16(const std::string &lbl);

    // Sets STATUS<Z> to reflect whether the 2-byte cell `ac` is zero
    // (via "movf ac,w\niorwf ac+1,w" - ORing both bytes into W),
    // trashing W - the shared zero-test every `if`/`while` condition
    // and every logical operator here reduces to.
    void testAcZero();
    // Jumps to `target` if the most recent testAcZero() found `ac`
    // zero/non-zero respectively, otherwise falls through - built on
    // BTFSC/BTFSS's own "skip the next instruction" shape (see
    // PIC16Assembler.h), i.e. "skip this goto" when the condition
    // doesn't hold.
    void gotoIfZero(const std::string &target);
    void gotoIfNonZero(const std::string &target);

    // Real runtime push/pop on the shared `SSTACK` region, via
    // `SP`/FSR/INDF - see this file's own header comment on why a
    // real stack (not a compile-time-indexed fixed cell) is required
    // here. pushCell2()/popCell2() move a 2-byte pseudo-register
    // (`ac`'s own shape); pushPtrCell()/popPtrCell() move `ptr`'s one
    // byte.
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

    // Reserves `count` bytes of file-register space for `lbl` (this
    // backend's own `res` directive - see PIC16Assembler.h's own
    // header comment); `zeroAtStart` additionally queues it to be
    // explicitly cleared by generate()'s own startup code (see this
    // file's own header comment on why that's needed at all here).
    void declareRam(const std::string &lbl, int count, bool zeroAtStart = true);

    std::ostringstream code_;
    std::vector<std::string> ramDecls_;
    std::vector<std::pair<std::string, int>> zeroAtStart_;
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
};

}  // namespace pcc
