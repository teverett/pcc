#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates WDC 65C02 assembly (this project's own dialect - see
// M65C02Assembler.h) for the IR built by IRBuilder. This is
// ../m6502/M6502Codegen.cpp's own NMOS 6502 backend plus exactly one
// CMOS-only improvement: zeroing one of the fixed zero-page pseudo-
// registers below (`ac`/`bc`/`cx`/`dx`) goes through `stzp` (STZ,
// zero-page) directly instead of that file's own "LDA #0 / STA"
// two-instruction idiom - a real NMOS 6502 has no "store zero"
// opcode at all - see zeroZp()'s own comment; every other
// instruction and design choice here is identical to that file's
// own, so see its header comment for the shared rationale (zero-page
// pseudo-register layout, no hardware multiply/divide, no variable-
// count shift, the N-xor-V signed-compare fixup, safeBranch()'s own
// short-branch-plus-JMP range workaround) - repeated below only
// where this file's own doc comments need the exact wording to
// match the code they're attached to.
//
// See ../../../README.md for the shared language and this backend's
// own "bare flat binary" platform choice: `generate()` returns a
// complete, self-contained program loaded and entered at a fixed
// address (`kBaseAddress`, see M65C02Codegen.cpp), halting in an
// infinite loop - no OS, no I/O conventions. The 6502 has a real
// hardware stack (page 1, via the 8-bit S register), used by
// JSR/RTS for return addresses and by PHA/PLA for saving one operand
// across evaluating the other in a binary expression - exactly the
// role pdp7 instead has to improvise with named memory cells for
// (see Pdp7Codegen.cpp). That hardware stack is untouched by, and
// independent of, every Local/Param's own storage: see the frame
// pointer discussion below.
//
// RECURSION / PER-CALL STORAGE: the 6502 has no instruction that
// addresses memory as "stack-pointer plus displacement" (unlike the
// 8086's `[bp+N]`), and its one real hardware stack is only 256
// bytes - far too small to hold every function's locals for the
// program's whole run, and not displacement-addressable anyway. So
// every Local/Param instead lives in a *software* stack frame,
// reached through two more fixed zero-page pseudo-registers - `sp`
// (the software stack's own current top, growing toward decreasing
// addresses as calls nest deeper) and `fp` (the *currently
// executing* call's own frame base) - via the same `(zp),Y`
// indirect-indexed addressing genIndexAddr() already needs for array
// indexing and pointer dereference (see slotAddrToPtr()). This is
// the standard technique real 6502-family C compilers (cc65 included)
// use for the same reason.
//
// Frame layout, chosen to need only ADDITION for the (never-arrayed)
// Param side and keep the two sides on opposite sides of `fp` so they
// can never overlap: `fp` itself points at this call's own saved-old-
// `fp` word. Param i (i=0,1,2,...) sits ABOVE `fp`, where the caller
// already placed it before `jsr` - see genExpr()'s own `Call` case -
// at `fp + 2*(i+1)`. Local frameOffset k (k=1,2,...) sits BELOW `fp`,
// reserved by this function's own prologue (genFunction()), at
// `fp - 2*k` - so a local array's element i sits at `base - 2*i`
// (base = element 0's own address), the mirror image of a Global
// array's `base + 2*i` (see genIndexAddr()'s own comment on getting
// that sign right).
//
// Calling convention (genExpr()'s `Call` case / genFunction()):
// the caller evaluates each argument and pushes it onto the software
// stack (`sp -= 2`, store at `[sp]`) in *reverse* declaration order,
// so argument 0 ends up closest to the callee's own frame (at the
// lowest address, right above where `fp` will land) - then `jsr`s,
// then restores `sp` by the pushed byte count (cdecl-style caller
// cleanup). The callee's own prologue pushes the *caller's* `fp`
// onto the software stack right where the caller's pushed arguments
// end (this call's own saved-old-`fp` word), sets `fp` to that
// address, then reserves `frameSize*2` more bytes below it for
// locals (`sp -= frameSize*2`); the epilogue (also run inline by an
// early `return` - see genStmt()'s `Return` case) does the reverse:
// restore `sp` to just above the saved word, then restore `fp` from
// it. An indirect call (`CallPtr`) never passes arguments (IRBuilder
// already rejects any - see that case's own comment) so it needs
// none of this caller-side bookkeeping; the callee's own prologue/
// epilogue behaves identically either way, so recursion through a
// function pointer is exactly as reentrant as a direct call.
//
// Globals are unaffected by any of this - they keep the one static,
// program-lifetime trailer cell/array they always had (see
// declareArray()/generate()'s own Global loop), addressed directly
// by label, never through `fp`/`sp`.
//
// The 6502 has only three 8-bit registers (A, X, Y) and this
// language's own "every variable is one word" word is 16 bits (same
// width as pdp7/8086's own int, chosen for the same reason: 16 bits
// covers this target's own 64KB address space, matching how real
// 6502 C compilers - cc65 included - also default `int` to 16 bits
// despite the CPU being 8-bit). Rather than juggle A/X/Y directly the
// way the 8086 backend juggles ax/bx, every 16-bit value in flight
// lives in one of seven fixed, two-byte pseudo-registers in zero page
// - `ptr` (an indirection target - see below), `ac` (genExpr()'s own
// result, the 8086 backend's "ax"), `bc` ("the other operand" in a
// binary op, the 8086 backend's "bx"), `cx` (a loop counter, for
// shifts/multiply/divide), `dx` (divide's remainder/general extra
// scratch), and `sp`/`fp` (the software call-frame stack discussed
// above) - bound once at the top of generate() via this dialect's
// own `name = value` equate (M65C02Assembler.cpp's `Line::Equate`),
// not through the usual sequential data-trailer layout the way an
// ordinary variable is, since `ptr` specifically has to live at a
// real zero-page address (0-255) for the 6502's one indirect-
// addressing mode to use it at all - a genuine hardware restriction,
// not a stylistic choice (see genIndexAddr()'s own comment). Putting
// `ac`/`bc`/`cx`/`dx`/`sp`/`fp` in zero page too costs nothing (zero
// page has 256 bytes, this uses 14) and keeps every one of them
// equally cheap to reference with a 2-byte absolute instruction
// either way - there turned out to be no need for the zero-page-vs-
// absolute opcode variants (`$A5` vs `$AD`) at all, since every
// 2-byte-pair access to one of these pseudo-registers itself always
// goes through a FIXED, compile-time-known symbol, never a runtime-
// computed effective address the way `ptr`'s own *target* is.
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
class M65C02Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "65c02"; }
    std::string sourceExtension() const override { return ".s65c"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other four/`objraw`,
    // every one of which means a real relocatable object file for
    // separate compilation (see Codegen.h's own header comment on
    // why this is asked per-backend: `--format=elf` means something
    // else entirely for `8086` - see I8086Codegen.h).
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
    //
    // `slot->arraySize > 0` means the array's own storage is being
    // indexed directly: a Global's is a compile-time-known label
    // (base + idx*2 - see arrayElemText()'s own callers), a Local's
    // is `fp`-relative (base - idx*2, base = `fp - 2*frameOffset` -
    // see M65C02Codegen.h's own frame-layout comment; a Param can
    // never reach this branch, since a C array parameter always
    // decays to a pointer). Otherwise `slot` is a plain pointer
    // variable: the base is that variable's own *value*, read via
    // loadSlotToAc()'s/slotAddrToPtr()'s own machinery if it's a
    // Local/Param, or directly from its label if Global, then idx*2
    // is always ADDED to that value regardless of where the pointer
    // variable itself lives - only a Local *array*'s own storage ever
    // needs the base-minus-index form.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Leaves a Local/Param `slot`'s own storage address in `ptr` -
    // `fp + 2*(paramIndex+1)` for a Param, `fp - 2*frameOffset` for a
    // Local (see M65C02Codegen.h's own frame-layout comment). Never
    // called with a Global slot - a Global's address is always a
    // compile-time label, with no need for this at all.
    void slotAddrToPtr(const ir::Slot *slot);
    // Reads/writes a scalar slot's own value into/from `ac` - a
    // direct label copy for a Global, or an indirect access through
    // slotAddrToPtr()'s own `ptr` for a Local/Param.
    void loadSlotToAc(const ir::Slot *slot);
    void storeAcToSlot(const ir::Slot *slot);

    // Stores arrayInit into a Local array slot's own frame words -
    // every call needs this re-run, since (unlike a Global's one
    // static trailer) a Local's storage is this activation's own
    // fresh frame region, not pre-set to anything; arrayInit is
    // always fully populated up to arraySize (zero-padded even with
    // no source-level initializer - see BuildAst.cpp's
    // padArrayInit), so this always leaves the whole array in a
    // known state, same as a Global array's static trailer always
    // did.
    void genLocalArrayInit(const ir::Slot *slot);
    // Loads one ArrayInitElem's own value into `ac` - a plain 16-bit
    // immediate for a literal, or a label's own address (low/high
    // byte, same as genAddrOf()'s Global case) for a symbol element.
    void loadArrayElemToAc(const ir::ArrayInitElem &e);

    // Tears down the current call's own frame: restores `sp` to just
    // above this frame's saved-old-`fp` word, then restores `fp`
    // itself from that word (see M65C02Codegen.h's own calling-
    // convention comment). Run once at the natural end of
    // genFunction(), and inline before every early `return` (see
    // genStmt()'s own `Return` case) - in both cases immediately
    // before `rts`. Uses `ptr` as scratch; never touches `ac`, since
    // a `Return`'s own value already sits there and must survive
    // into `rts`.
    void emitEpilogue();

    // 16-bit `dstReg = srcReg + disp` (disp may be negative - stored
    // as its own two's-complement 16-bit encoding and added via
    // `adci`/`adci_hi`, so this is really a 16-bit add either way,
    // never a real subtract instruction). `dstReg`/`srcReg` name one
    // of this file's own fixed zero-page pseudo-registers (e.g.
    // "ptr", "fp", "sp") - safe even when they're the same register,
    // since the low byte is written before the high byte is read.
    void addConst16(const std::string &dstReg, const std::string &srcReg, int disp);
    // 16-bit `dst = a + b` / `dst = a - b`, each a direct register-
    // to-register operation (no immediate) - same in-place safety as
    // addConst16().
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    // "ldyi 0; ldaind zpPtr; sta dstReg; ldyi 1; ldaind zpPtr; sta
    // dstReg+1" / the `staind` mirror - reads/writes one 16-bit value
    // through `zpPtr`'s own `(zp),Y` indirect addressing (see
    // genIndexAddr()'s own comment on why that's the one real
    // zero-page-bound addressing mode here) into/from `dstReg`/
    // `srcReg`, another of this file's own pseudo-registers.
    void loadIndirect(const std::string &zpPtr, const std::string &dstReg);
    void storeIndirect(const std::string &zpPtr, const std::string &srcReg);

    // Encodes "if the branch condition holds, jump to `targetIfTaken`,
    // otherwise jump to `targetIfNot`" using a short branch to a
    // label immediately after it (so it's always ±2 bytes away -
    // trivially in range no matter how far `targetIfTaken` really
    // is) followed by an unconditional (always full-range) `JMP` -
    // see M65C02Codegen.h's own header comment on branch range.
    // `branchMnemonic` is one of M65C02Assembler.cpp's own branch
    // mnemonics ("beq", "bne", "bcs", "bmi", "bpl", "bvc").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // "lda src / sta dst / lda src+1 / sta dst+1" - copies one 16-bit
    // pseudo-register/slot to another, byte by byte.
    void copy16(const std::string &dst, const std::string &src);
    // "stzp lbl; stzp lbl+1" - zeroes one of the fixed zero-page
    // pseudo-registers (`ac`/`bc`/`cx`/`dx`, never `ptr` - nothing
    // here ever needs to zero that one) using the CMOS-only STZ
    // instruction (see M65C02Assembler.cpp's own `stzp` comment),
    // replacing ../m6502/M6502Codegen.cpp's own "ldai 0 / sta lbl /
    // ldai 0 / sta lbl+1" two-instruction-per-byte idiom - the one
    // genuine NMOS-vs-CMOS difference this whole backend exploits.
    // `lbl` must name a zero-page symbol (one of the five pseudo-
    // registers bound in generate() - never a plain IR slot, which
    // lives in the ordinary, non-zero-page data trailer instead), or
    // `stzp`'s own assembler-level zero-page range check fails.
    void zeroZp(const std::string &lbl);
    // "inc lbl; if that wrapped 0xFF -> 0x00, inc lbl+1 too" - a
    // 16-bit increment. The 6502 has no 16-bit INC, and the only
    // place this backend ever needs one (genDivMod()'s quotient) is a
    // handful of fixed instructions away from the label it tests, so
    // (unlike safeBranch(), which guards against an *arbitrary-
    // distance* user-code target) this uses a plain, direct `bne` -
    // its own target is always right here, not user code.
    void inc16(const std::string &lbl);

    // Only ever called with a Global slot - a Local/Param is never
    // addressed by name at all (see slotAddrToPtr()), since it never
    // crosses a file boundary and every reference to it is already
    // inside the one function that owns it; the fallback case below
    // only exists so this still returns *something* harmless (never
    // actually emitted into generated code) if that invariant were
    // ever violated.
    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dblo e" / "dbhi e" data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
