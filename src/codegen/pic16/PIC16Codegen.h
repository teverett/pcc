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
// (0x0000), halting in an infinite loop. Every Local/Param gets real
// per-call storage (see genFunction()'s own emitPrologue()/
// emitEpilogue() and this file's own header comment further down),
// so direct and mutual/indirect recursion both compute correctly, up
// to whatever depth the real 8-level hardware call stack allows
// before it runs out (see that same comment).
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
// PIC16 core's one hardware stack is a dedicated, non-addressable,
// fixed-depth (8 levels on the real PIC16F84A) *return-address* stack
// used automatically by CALL/RETURN - there's no PUSH/POP-equivalent
// for ordinary data, and no way to extend or redirect it). So "save
// the other operand across evaluating this subexpression"
// (genBinary()'s own `+`/`-`/etc., and every comparison) needs a
// *real* software stack in file-register space after all: a
// dedicated `SSTACK` region plus an `SP` cursor byte, pushed/popped
// via FSR/INDF (pushCell2()/popCell2(), pushPtrCell()/popPtrCell()
// below). A compile-time-indexed scheme (one fixed cell per static
// nesting depth, no runtime pointer) looks tempting and costs nothing
// at runtime - but it's unsound the moment a call can happen between
// a push and its matching pop: the callee's own codegen was compiled
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
// That same `SSTACK`/`SP` software stack is also where every Local
// and Param's own real per-call storage lives (genFunction()'s own
// emitPrologue()/emitEpilogue(), frameSlotOffset()) - the two uses
// nest correctly for exactly the reason above: a whole call frame is
// just a longer-lived "thing pushed on `SSTACK`" than one binary op's
// saved operand, torn down (in LIFO order, like everything else here)
// by the callee's own epilogue before it returns. `FP`, one more
// fixed single-byte pseudo-register alongside `ac`/`bc`/etc., holds
// the *current* call's own frame base within `SSTACK`; saving/
// restoring it across a `call` (in `SSTACK` too, right after the
// pushed argument words - see emitPrologue()'s own comment) is what
// gives every activation of a function its own frame, direct or
// mutual/indirect recursion included, up to whatever depth the real
// hardware call stack allows before *that* (never this software one)
// runs out - a real, inherent hardware limit (8 levels on the real
// PIC16F84A), not a bug, and not something any software scheme here
// can lift: once 9 CALLs are outstanding at once, the 9th silently
// overwrites the oldest still-pending return address, with no
// overflow flag raised at all on this chip. A frame's own params
// live first (`FP+0`, `FP+2`, ... one per Param, in paramIndex
// order - the caller's own pushed argument words, never copied again),
// then one byte holding the caller's own saved `FP` (`FP+2*numParams`),
// then every Local in declaration order starting right after that
// (`frameOffset` 1's own word at `FP+2*numParams+1`, each later
// `frameOffset` 2 bytes higher - see frameSlotOffset()'s own comment).
// Unlike a stack that grows toward *decreasing* addresses (the 8086/
// i386 backends' own `bp`-relative frames), `SSTACK` only ever grows
// upward (`SP` only ever increments - see pushCell2() et al.), so a
// Local array's element `i` sits at *increasing* addresses as `i`
// grows, the same direction a Global array's own trailer already
// uses - never the base-minus-`i` arithmetic a decreasing-address
// frame needs (see I8086Codegen.h's own `addr()` comment for that
// opposite case, and the real miscompile it's there to avoid).
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
//
// `objectMode` (real separate compilation, `--format=coff`/`elf`/
// `aout`/`objraw` - see Codegen.h's own header comment) assembles at
// this same CODE address 0, but omits the entry sequence itself -
// PIC16LinkerFactory.cpp's own synthesized entry object is solely
// responsible for that, once every object is merged and `fn_main`'s
// linked address is resolvable, the same pattern every other
// GenericLinker-based backend follows (see M68kCodegen.cpp's own
// `generate()` comment). A global *variable*'s own initial value
// still has to be written at runtime, not loaded as part of the
// image (real file-register RAM has no "initial contents" of its own
// - see this file's own header comment above), so in object mode
// that write-out code runs as the prologue of `fn_main` itself
// instead of `START` (the one point every linked program is
// guaranteed to reach exactly once, before any other code, without
// PIC16LinkerFactory.cpp's own entry object - built once, before it
// has ever seen any user object at all - needing to know anything
// about this file's own globals). Only the file that defines `main`
// gets this treatment: a global *defined* in a different file from
// the one that defines `main` has no mechanism here to run its own
// initializer under real separate compilation at all.
//
// Every real Global's own file-register address is a real,
// relocatable assembler symbol (see declareRam()'s own comment, and
// PIC16Assembler.h's own header comment on the `kPIC16RelocData`
// relocation kind and the `"..d"` pseudo-symbol): two separately-
// compiled files' own distinct globals get distinct, non-colliding
// linked data-space addresses once merged (GenericLinker.h's own
// second, data-space layout pass - see its own `dataBaseAddress`
// comment), the data-space counterpart of how `fn_`-prefixed
// functions' own CODE addresses already work. A Local or Param is
// different again - it has no data-space symbol at all, relocatable
// or otherwise, real separate compilation or not, since it never
// crosses a file boundary and lives in a fresh per-call frame inside
// `SSTACK` instead (see this file's own header comment above and
// frameSlotOffset()'s own comment). The eight fixed scratch pseudo-
// registers/software-stack cells (`ac`/`bc`/`cx`/`dx`/`ptr`/`SP`/
// `FP`/`SSTACK`) are different from a Global too: every function in
// every file shares them, so they're declared as plain compile-time
// equates at a fixed address instead (see declareRam()'s own
// comment) - the same value in every object regardless of where it's
// linked, never part of this relocatable data-space allocation at
// all.
class PIC16Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic16"; }
    std::string sourceExtension() const override { return ".pic16"; }
    std::vector<std::string> assembledFormats() const override {
        return {"hex", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `hex` is this backend's own whole-program Intel-HEX image; the
    // other five all mean a real relocatable ObjectFile for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend, and generate()'s own `objectMode` comment
    // below) - same convention as every other GenericLinker-based
    // backend's own `isObjectFormat()` override.
    bool isObjectFormat(const std::string &format) const override { return format != "hex"; }
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame words

    // Leaves slot[idxExpr]'s own effective (one-byte) file address in
    // `ptr`. For a Global, the base is a compile-time address (the
    // array's own label) or, for a plain pointer variable, that
    // variable's own stored value; for a Local/Param, the base is
    // instead FP-relative (see frameSlotOffset()'s own comment) - an
    // array's own base address, or a pointer variable's own value,
    // read out of its frame slot via FSR/INDF first.
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

    // Emits the globalInit startup code that writes one array
    // element's own value into its already-reserved two RAM bytes
    // (`loLbl`/`hiLbl`) - see generate()'s own call site for how
    // those are named.
    void emitInitElem(std::ostream &out, const std::string &loLbl, const std::string &hiLbl,
                       const ir::ArrayInitElem &e);

    // A Local/Param's own word offset from `FP` - see this file's own
    // header comment for the frame layout this lays out. A Param's
    // position needs no knowledge of the current function at all
    // (`paramIndex` alone fixes it - params always come first, in
    // `paramIndex` order); a Local's does, since its own words start
    // right after every Param plus the one byte holding the caller's
    // saved `FP` - `curNumParams_` (set once, by genFunction(), for
    // the whole function currently being generated) supplies that.
    int frameSlotOffset(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return 2 * slot->paramIndex;
        return 2 * curNumParams_ + 2 * slot->frameOffset - 1;
    }
    // Leaves FP + frameSlotOffset(slot) in `W`, without touching
    // `ptr`/FSR - the shared arithmetic behind both loadFrameAddr()
    // (a scalar's own address) and genIndexAddr()'s own Local/Param
    // cases (an array's own base, or a pointer variable's own address
    // before its value is read out of it).
    void loadFrameBaseW(const ir::Slot *slot);
    // ptr = FP + frameSlotOffset(slot) - a Local/Param scalar's own
    // effective file address, the frame-relative counterpart of a
    // Global's plain `label(slot)` name; followed by storeAcIndirect()/
    // loadAcIndirect() wherever a Global would instead just name
    // itself directly as a movf/movwf operand.
    void loadFrameAddr(const ir::Slot *slot);
    // Real per-call prologue/epilogue - see this file's own header
    // comment for the frame layout and why `SSTACK`/`SP` (already
    // needed for genBinary()'s own operand-saving) is exactly the
    // right place for this too. emitPrologue() pushes the caller's
    // own `FP` (right after the `numParams` argument words the caller
    // already pushed via pushCell2() before `call` - see genExpr()'s
    // own Call case), sets `FP` to this call's own frame base, then
    // reserves `frameSize` more words for this function's own Locals.
    // emitEpilogue() (every Return, and genFunction()'s own
    // fallthrough) undoes exactly that, in reverse: deallocate the
    // whole frame (`SP = FP`, freeing the Locals, the saved-`FP` byte,
    // and the argument words all at once - this is a callee-cleans-up
    // convention, so no caller-side cleanup after `call` is needed
    // either) and restore the caller's own `FP`.
    void emitPrologue(int numParams, int frameSize);
    void emitEpilogue(int numParams);

    // A Global's own name-based label (never `id` - see IR.h's own
    // `Slot::ownerFunctionName` comment for why only a name, not an
    // allocation-order id, means the same thing on both sides of real
    // separate compilation). Never called with a Local/Param: neither
    // has a name-based (or any other static) address any more - see
    // this file's own header comment - so every reference to one
    // instead goes through frameSlotOffset()/loadFrameAddr() above.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    // Reserves `count` bytes of file-register space for `lbl` - a
    // Global or one of the fixed pseudo-registers/software-stack
    // names, never a Local/Param (see this file's own header comment:
    // those live in a per-call frame inside `SSTACK` instead, never
    // in a `res`-declared cell of their own). In whole-program mode
    // (`!objectMode_`) this always emits this backend's own `res`
    // directive (see PIC16Assembler.h's own header comment), the same
    // single, unified RAM counter every caller shares. In object
    // mode, one of the eight fixed, shared scratch pseudo-registers/
    // software-stack names (`ac`/`bc`/`cx`/`dx`/`ptr`/`SP`/`FP`/
    // `SSTACK`) instead becomes a plain compile-time equate at its own
    // fixed address (`kPIC16Reg*` - see PIC16Assembler.h's own header
    // comment on why these can never be part of any one object's own
    // relocatable per-object data allocation); every other name (a
    // real Global) still emits `res`, now interpreted by
    // PIC16Assembler.cpp's own object-mode assembler as a relocatable
    // data-space symbol (see its own header comment on `"..d"`).
    void declareRam(const std::string &lbl, int count);

    std::ostringstream code_;
    std::vector<std::string> ramDecls_;
    std::map<std::string, std::string> perFileAsm_;

    // Set once, at the top of generate() - see declareRam()'s own
    // comment on why the fixed pseudo-registers need a completely
    // different declaration (a plain compile-time equate, never a
    // relocatable data-space `res` label) once real separate
    // compilation is in play.
    bool objectMode_ = false;

    // The number of Params the function currently being generated by
    // genFunction() declares - set once per function, read by
    // frameSlotOffset() for every Local reference inside that
    // function's own body (see frameSlotOffset()'s own comment for
    // why a Local, unlike a Param, needs this to find its own words).
    int curNumParams_ = 0;

    int labelCounter_ = 0;
};

}  // namespace pcc
