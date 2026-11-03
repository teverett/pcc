#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 6502 assembly (this project's own dialect - see
// M6502Assembler.h) for the IR built by IRBuilder. See
// ../../../README.md for the shared language and this backend's own
// "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded and entered at a fixed
// address (`kBaseAddress`, see M6502Codegen.cpp), halting in an
// infinite loop - no OS, no I/O conventions, the same "no stack, no
// recursion" model every backend shares (see README's "Scope and
// limitations"), just for a target whose own real hardware stack
// (the 6502 genuinely has one, unlike pdp7) is used anyway for
// JSR/RTS and for saving one operand across evaluating the other in
// a binary expression (PHA/PLA) - exactly the role pdp7 instead has
// to improvise with named memory cells for (see Pdp7Codegen.cpp).
//
// The 6502 has only three 8-bit registers (A, X, Y) and this
// language's own "every variable is one word" word is 16 bits (same
// width as pdp7/8086's own int, chosen for the same reason: 16 bits
// covers this target's own 64KB address space, matching how real
// 6502 C compilers - cc65 included - also default `int` to 16 bits
// despite the CPU being 8-bit). Rather than juggle A/X/Y directly the
// way the 8086 backend juggles ax/bx, every 16-bit value in flight
// lives in one of five fixed, two-byte pseudo-registers in zero page
// - `ptr` (an indirection target - see below), `ac` (genExpr()'s own
// result, the 8086 backend's "ax"), `bc` ("the other operand" in a
// binary op, the 8086 backend's "bx"), `cx` (a loop counter, for
// shifts/multiply/divide), `dx` (divide's remainder/general extra
// scratch) - bound once at the top of generate() via this dialect's
// own `name = value` equate (M6502Assembler.cpp's `Line::Equate`),
// not through the usual sequential data-trailer layout the way an
// ordinary variable is, since `ptr` specifically has to live at a
// real zero-page address (0-255) for the 6502's one indirect-
// addressing mode to use it at all - a genuine hardware restriction,
// not a stylistic choice (see genIndexAddr()'s own comment). Putting
// `ac`/`bc`/`cx`/`dx` in zero page too costs nothing (zero page has
// 256 bytes, this uses 10) and keeps every one of them equally cheap
// to reference with a 2-byte absolute instruction either way - there
// turned out to be no need for the zero-page-vs-absolute opcode
// variants (`$A5` vs `$AD`) at all, since every 2-byte-pair access
// here always goes through a FIXED, compile-time-known symbol, never
// a runtime-computed effective address the way `ptr` itself is.
//
// No hardware multiply or divide (the EAE pdp7 at least has `mul` -
// see Pdp7Codegen.cpp - and the 8086 has `imul`/`idiv` outright; the
// 6502 has neither), so `*` is a textbook shift-and-add loop and `/`,
// `%` a simple repeated-subtraction loop - the same "good enough,
// not fast" choice Pdp7Codegen.cpp's own software divmod already
// made for pdp7, and like that routine, only verified against non-
// negative operands (see README's "Scope and limitations").
//
// No variable-count shift instruction either (ASL/LSR always shift
// exactly one bit) - `<<`/`>>` loop one bit at a time, the same
// reason (and the same fix) as Pdp7Codegen.cpp's own `<<`/`>>`.
//
// Comparisons (`<`,`<=`,`>`,`>=`) use the standard 6502 idiom for a
// signed multi-byte compare-via-subtraction: after the final byte's
// SBC, the sign bit (N) is only the *true* signed-less-than result
// once corrected for overflow (V) - `BVC`/`EOR #$80` fixes that up
// (see genCompare()'s own comment) - the same N-xor-V rule x86's own
// SF/OF flags encode directly, just without a combined flag the 6502
// can test in one instruction.
//
// The 6502's conditional branches (`BEQ`/`BNE`/...) can only reach
// ±127 bytes - unlike pdp7/8086, which (see those backends' own
// comments) never need to worry about jump range in practice, a
// 6502-generated `if`/`while` body of ordinary size easily exceeds
// that. Every conditional branch here is therefore always encoded as
// a short branch to an immediately-following label plus an
// unconditional (always full-range) `JMP` - see safeBranch()'s own
// comment - rather than ever risk a real out-of-range short branch.
//
// Every Local/Param gets a fresh, per-call storage cell rather than
// one permanent cell shared by every call (so direct and indirect
// recursion both work, each activation's own copy untouched by a
// nested call) - but the 6502 has no "load relative to a pointer"
// addressing mode at all (not even the real hardware stack register
// `S` supports it - S is used purely as JSR/RTS already uses it, for
// the return address, left alone here), so there's no way to reuse
// x86's bp-relative-displacement trick directly. Instead this follows
// the standard real-world 6502 C-compiler technique (cc65 included):
// a SOFTWARE frame pointer, `fp`, one of the fixed two-byte zero-page
// pseudo-registers (see the comment above on `ptr`/`ac`/`bc`/`cx`/
// `dx`), reached through the one real indirect-addressing mode this
// CPU has, `(zp),Y` - every Local/Param access computes `ptr = fp +
// <its own fixed byte offset>` first (setPtrToFrameSlot()) and then
// loads/stores through `ptr` with `Y` selecting the low/high byte.
// `sp` (also zero page) is a software bump allocator tracking the
// next free byte of a dedicated RAM region (`kFrameStackBase`,
// M6502Codegen.cpp - chosen below the hardware stack page and below
// where any program's own code/data ever starts, so it never
// collides with either) that every call's own frame is carved out of.
//
// Byte offsets within a frame are chosen ASCENDING as declaration
// index increases - every Param at 2*paramIndex, followed immediately
// by every Local at 2*(paramCount + frameOffset-1) - not x86's
// descending-from-bp convention. Nothing about this CPU favors one
// direction over the other (there's no hardware displacement mode
// either way has to fit into), so ascending was chosen simply because
// it keeps every offset a small non-negative constant added to `fp`
// with a single `clc`/`adc`/`adc` pair - a descending layout would
// need a 16-bit *subtraction* for every single access instead (the
// 6502 has no immediate-operand SBC at all - see M6502Assembler.cpp's
// own `immLow` table - so a subtraction would have to go through a
// two's-complement addition anyway), and would still need one for a
// Local array's own element-i address (base PLUS i*2 here, since
// index and declaration order increase together - never base MINUS
// i*2, the sign-flip bug class a descending layout invites).
//
// A call's own argument words are written into the exact frame cells
// the callee's own Params will read, before the callee ever runs -
// every argument is first evaluated and parked on the real hardware
// stack (so a nested call inside a LATER argument's own expression
// can never invalidate an EARLIER argument's already-computed value
// or address), then `sp` is bumped once to reserve the whole argument
// area, and finally each parked value is written into its own cell
// relative to that reserved area's own base (see the `Call` case in
// genExpr()). The callee's own prologue (genFunction()) then computes
// its own `fp` as `sp` (at entry) minus that same reserved size,
// landing exactly on the area the caller just wrote - then bumps `sp`
// further, past its own Locals, before running its body. Its epilogue
// collapses `sp` straight back to `fp`, deallocating Params and
// Locals together in one step (mirroring x86's own "mov sp, bp" - see
// I8086Codegen.cpp), and restores the caller's own `fp` from where
// the prologue saved it - on the real hardware stack, via plain
// PHA/PLA, exactly mirroring x86's own "push bp"/"pop bp": nothing
// about saving/restoring one more two-byte value across a call needs
// its own software stack discipline when the real one (already
// reentrant via JSR/RTS - untouched by any of this) is sitting right
// there for exactly that purpose.
class M6502Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "6502"; }
    std::string sourceExtension() const override { return ".s65"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other five, every one of
    // which means a real relocatable object file for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend: `--format=elf` means something else entirely
    // for `8086` - see I8086Codegen.h).
    bool isObjectFormat(const std::string &format) const override { return format != "raw"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    // Tears down the current call's own frame (locals and the
    // params the caller reserved for it collapse back to this call's
    // own `fp` in one step - the software-frame-pointer analogue of
    // x86's "mov sp, bp" - see I8086Codegen.cpp's own genFunction()
    // comment), restores the caller's own `fp` (saved by the
    // prologue - see genFunction()), and returns. Used both for the
    // function's own fall-through exit and for every `return`
    // statement (see genStmt()'s own StmtKind::Return case) - there
    // is no single shared fall-through point to exit through instead,
    // the same reason I8086Codegen.cpp's own Return case repeats its
    // epilogue inline.
    void genEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

    // Stores arrayInit into `slot`'s own frame cells - every call
    // needs this re-run, since (unlike a Global's one static
    // trailer) a Local's storage is this activation's own fresh
    // frame region, not pre-set to anything.
    void genLocalArrayInit(const ir::Slot *slot);

    // ac = slot's value / slot = ac - for a Global, a plain absolute
    // load/store (copy16() against its own fixed label); for a Local
    // or Param, through `ptr` via setPtrToFrameSlot() - see that
    // method's own comment.
    void loadSlotToAc(const ir::Slot *slot);
    void storeAcToSlot(const ir::Slot *slot);

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
    void genAddrOf(const ir::Expr &operand);  // &slot, &slot[idx], or &fn

    // Leaves slot[idxExpr]'s own effective address in `ptr` - the
    // only thing in this whole backend that has to live in zero
    // page: the 6502's one true indirect-addressing mode, `(zp),Y`,
    // is only legal when the pointer itself is a zero-page address
    // (not a size/encoding choice the way zero-page-vs-absolute
    // normally is on this CPU - an *absolute*-indirect-indexed mode
    // simply doesn't exist in the instruction set at all). Every
    // other "pointer" in this backend's own model is just a 16-bit
    // value sitting in an ordinary (non-zero-page) pseudo-register
    // until the moment it's actually dereferenced, at which point it
    // gets copied into `ptr` first.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Local/Param's own byte offset from `fp` within the CURRENT
    // function's own frame (see genFunction()'s own header comment
    // for the overall frame layout): a Param sits at 2*paramIndex (the
    // caller already reserved and wrote this region right below `fp`
    // before the call); a Local sits right after every Param, at
    // 2*(paramCount + frameOffset-1) (frameOffset is 1-based - see
    // IR.h's own comment). Only ever called while generating the one
    // function that owns `slot` (curFunction_ is that function) - a
    // Local/Param is never referenced from any other function's body.
    int frameByteOffset(const ir::Slot *slot) const;

    // Sets `ptr` to slot's own frame cell address (fp + its own
    // frameByteOffset(), plus `extraElemOffset` more bytes - a Local
    // array's own element i passes 2*i here) - the only way to reach
    // a Local/Param at all, there being no hardware "load relative to
    // a software pointer" addressing mode on this target (see
    // M6502Codegen.h's own header comment) - every access instead
    // goes through the one real indirect mode, (zp),Y, via `ptr`.
    void setPtrToFrameSlot(const ir::Slot *slot, int extraByteOffset);

    // Encodes "if the branch condition holds, jump to `targetIfTaken`,
    // otherwise jump to `targetIfNot`" using a short branch to a
    // label immediately after it (so it's always ±2 bytes away -
    // trivially in range no matter how far `targetIfTaken` really
    // is) followed by an unconditional (always full-range) `JMP` -
    // see M6502Codegen.h's own header comment on branch range.
    // `branchMnemonic` is one of M6502Assembler.cpp's own branch
    // mnemonics ("beq", "bne", "bcs", "bmi", "bpl", "bvc").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // "lda src / sta dst / lda src+1 / sta dst+1" - copies one 16-bit
    // pseudo-register/slot to another, byte by byte.
    void copy16(const std::string &dst, const std::string &src);
    // "inc lbl; if that wrapped 0xFF -> 0x00, inc lbl+1 too" - a
    // 16-bit increment. The 6502 has no 16-bit INC, and the only
    // place this backend ever needs one (genDivMod()'s quotient) is a
    // handful of fixed instructions away from the label it tests, so
    // (unlike safeBranch(), which guards against an *arbitrary-
    // distance* user-code target) this uses a plain, direct `bne` -
    // its own target is always right here, not user code.
    void inc16(const std::string &lbl);

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
    std::vector<std::string> trailer_;  // "label: dblo e" / "dbhi e" data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;

    // The function currently being generated - only ever used to
    // read its own params().size() (see frameByteOffset()), since a
    // Local/Param's frame layout depends on which function owns it.
    const ir::Function *curFunction_ = nullptr;
};

}  // namespace pcc
