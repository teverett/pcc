#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
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
// real 31-level hardware call stack - a fixed-depth, return-address-
// only stack with no addressable storage and no way to redirect or
// extend it, a genuine hardware constraint of this core, not a
// limitation of this backend's own code. `generate()` returns a
// complete, self-contained program starting at the real hardware
// reset vector (0x000000), halting in an infinite loop. Unlike most
// backends here, this one supports real recursion, direct or mutually
// indirect, up to whatever depth that hardware call stack (or this
// backend's own `FSTACK` frame region, see below) allows first - see
// this file's own header comment further down on `FP`/`NFP`/`FSTACK`
// for how.
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
// rotate exactly one bit). `FP`/`NFP` (one byte each) and `FSTACK` (a
// fixed-size region) are this backend's own addition over PIC16's
// shared fixed-register set - see this file's own header comment
// further down for what they're for.
//
// Scope restriction: like the PIC16 backend's own 68-byte-GPR limit,
// this backend only ever allocates data in the 96-byte Access Bank
// GPR window (0x00-0x5F) - real banked GPR beyond that (this chip has
// up to 768 bytes total) is never used, so no BSR bank-select code is
// ever needed (every instruction's own access bit, `a`, is always 0 -
// see PIC18Assembler.h's own header comment). Every Local/Param of
// every function, across the whole program, now shares the one
// `FSTACK` region (see below) instead of each getting its own
// permanently-reserved byte pair, so this window holds every
// *simultaneously live* call's own frame, not every function
// definition's own storage at once.
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
//
// Real per-call recursion (direct and indirect/mutual): every Local
// and every Param lives in a per-call frame on `FSTACK`, a dedicated
// software stack region in data memory, addressed indirectly through
// FSR0/INDF0/POSTINC0 (the same triple storeAcIndirect()/
// loadAcIndirect() already use for array/pointer access) rather than
// in one shared, statically-named memory cell - the one thing that
// makes a recursive call compute correctly: two live calls to the
// same function now have two genuinely distinct copies of its own
// Locals/Params, not one shared cell the inner call clobbers out from
// under the outer one.
//
// `FP` (one byte - every address this backend ever computes already
// fits in 8 bits, see this file's own "Scope restriction" paragraph
// below) holds the address of the *current* call's own frame - its
// Params (by `paramIndex`, 2 bytes each, matching every other 16-bit
// value here) immediately followed by its Locals (by `Slot::
// frameOffset`, IR.h's own 1-based sequential word index in
// declaration order - see slotFrameOffsetBytes()). Reading/writing a
// Local/Param slot (loadSlotToAc()/storeAcToSlot()/slotAddrToPtr())
// means computing that slot's own real address as `FP + <this
// function's own compile-time offset for it>` into `ptr` and going
// through the same indirect FSR0 access path as any other computed
// address here - a Global's own address is still a plain, fixed,
// compile-time symbol (globals are genuinely shared across every
// call, by C's own semantics, not reentrant data at all), so a
// Global slot access never goes through `FP`/`ptr` this way.
//
// One byte directly below `FP` (i.e. at `FP-1`) holds *this* call's
// own saved caller's `FP` - exactly mirroring the real hardware return-
// address stack's own nesting discipline, but for frame addresses
// instead of code addresses, since nothing on this core can redirect
// or extend that hardware stack to carry anything else (see this
// file's own header comment above on the 31-level hardware call stack
// being fixed-depth and non-addressable - the actual reason a second,
// software-maintained "frame chain" is needed here at all, the same
// reasoning `SP`/`SSTACK` already apply to saving a binary operator's
// other operand across a call). A function's own prologue
// (emitPrologue()) saves the caller's `FP` there and adopts `NFP` -
// the address the *caller* already computed and left for it,
// immediately before `call` - as its own new `FP`; its epilogue
// (emitEpilogue(), run before every `return`) reads that same byte
// back to restore the caller's `FP` before control actually returns
// to it. Nothing about this pairing depends on call depth: it's
// correct at whatever depth it happens to run at, including a direct
// or mutually-indirect recursive call into the very same function,
// because each nested call's own prologue/epilogue pair only ever
// touches the one byte at *its own* `FP-1`, never any other call's.
//
// A call site (the `Call` case in genExpr()) computes where the
// *callee's* new frame will go - `FP + <this function's own
// frameSizeBytes_> + 1` (one byte for the callee's own saved-FP slot,
// then its Params/Locals) - entirely from its own already-known
// constants and its own current `FP`, writes each argument directly
// into that not-yet-adopted frame (via `ptr`, the same computed-
// address idiom as any other indirect store here), then hands that
// same address to the callee in `NFP` right before `call`, recomputed
// fresh immediately beforehand rather than held across argument
// evaluation - an argument expression can itself contain a nested
// call, which runs its own complete prologue/epilogue pair and always
// restores `FP` before returning, but would otherwise be free to
// clobber a value sitting in `NFP` early. No separate "stack top"
// register is ever needed alongside `FP`/`NFP`: a call's own new frame
// address is always a compile-time-constant offset from the *caller's*
// own current `FP`, since the caller's own frameSizeBytes_ never
// changes across any of its own calls, recursive or not.
//
// `FSTACK` itself is a fixed-size region (see its own `declareRam`
// call site in generate()) - real recursion depth is therefore
// bounded by whichever runs out first: this region's own fixed byte
// budget, or the real 31-level hardware call stack. Within this
// backend's own 96-byte Access Bank scope restriction (see below)
// `FSTACK`'s own size is a real, direct trade-off against how many
// bytes are left over for globals - sized generously enough here to
// comfortably clear ordinary recursion depths (a double-digit number
// of simultaneous calls at a few bytes of frame each), not to
// guarantee reaching the full 31-level hardware limit for every
// possible program; a program combining large frames with deep
// recursion can still exhaust `FSTACK` itself first, reported the
// same loud way this backend already reports Access Bank exhaustion
// in general (see PIC18Assembler.cpp's own `Line::Res` comment, and
// PIC18LinkerFactory.cpp's own `patchDataAbs8` for the linked,
// multi-object case).
//
// `objectMode` (real separate compilation, `--format=coff`/`elf`/
// `macho`/`aout`/`objraw` - see Codegen.h's own header comment)
// assembles at this same CODE address 0, but omits the entry sequence
// itself (the `goto START`/`START:`/FSR0H clear/SP setup/`call
// fn_main`/halt loop) entirely - PIC18LinkerFactory.cpp's own
// synthesized entry object is solely responsible for that, once
// every object is merged and `fn_main`'s linked address is
// resolvable, same pattern every other GenericLinker-based backend
// follows (see M68kCodegen.cpp's own `generate()` comment). This
// backend's own `ac`/`bc`/`cx`/`dx`/`ptr`/`SP`/`SSTACK` pseudo-
// registers become plain compile-time equates in object mode, fixed
// at the same addresses (0x00-0x21) in every object and in
// PIC18LinkerFactory.cpp's own entry object - see
// PIC18Assembler.h's own `kPIC18DataBaseAddress` comment for why that
// needs no relocation at all, the same choice
// ../mcs51/MCS51Codegen.cpp's own identical pseudo-registers already
// make.
//
// A global *variable*'s own initial value still has to be written at
// runtime, not loaded as part of the image (real data memory is RAM,
// with no "initial contents" of its own - see this file's own header
// comment above), so in object mode that write-out code runs as the
// prologue of `fn_main` itself instead of `START:` (the one point
// every linked program is guaranteed to reach exactly once, before
// any other code, without PIC18LinkerFactory.cpp's own entry object -
// built once, before it has ever seen any user object at all -
// needing to know anything about this file's own globals). Only the
// file that defines `main` gets this treatment: a global *defined* in
// a different file from the one that defines `main` has no mechanism
// here to run its own initializer under real separate compilation at
// all, nor does any function's own local/parameter get zero-
// initialized the way `generate()`'s own whole-program `START:`
// sequence otherwise guarantees - a real, documented scope limitation
// of real separate compilation on this backend specifically, not
// present in whole-program (`--format=hex`) mode at all.
//
// Every global/local/param's own Access-Bank address is a real,
// relocatable assembler symbol (see PIC18Assembler.cpp's own
// `fileAddr`/`fileAddr12`, and PIC18Assembler.h's own header comment
// on the `"..d"` pseudo-symbol): two separately-compiled files' own
// distinct globals get distinct, non-colliding linked data-space
// addresses once merged by GenericLinker.cpp's own second, Harvard-
// data-space layout pass, the actual fix this whole migration exists
// for (see ../../fileformat/ObjectFile.h's own `ObjSymbol::isData`
// comment).
class PIC18Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic18"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".pic18"; }
    // `hex` is this backend's own whole-program Intel-HEX output; the
    // other five mean a real relocatable object file for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend, and PIC18Assembler.h's own header comment on
    // what this backend's own ObjectFile actually carries).
    std::vector<std::string> assembledFormats() const override {
        return {"hex", "coff", "elf", "macho", "aout", "objraw"};
    }
    bool isObjectFormat(const std::string &format) const override { return format != "hex"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    // `extraPrologue` (raw asm text, emitted right after the frame
    // prologue and before `fn`'s own body) is how generate() runs the
    // whole-program's globalInit code as part of `fn_main`'s own
    // prologue under `objectMode` - see generate()'s own call site.
    void genFunction(const ir::FunctionBody &fn, const std::string &extraPrologue = "");
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

    // See PIC16Codegen.h's own header comment on requireNarrow() -
    // identical reasoning applies verbatim here: `ac` (and every
    // other pseudo-register this backend has) is a fixed 2 bytes,
    // and this backend's own 16-bit helpers (add16()/sub16(),
    // shiftOneBit(), pushCell2()/popCell2(), ...) are all hardcoded to
    // that width, with no parameterized "N bytes" form - the same
    // wall MCS51Codegen.h documents hitting. A single check at the
    // top of genExpr() catches every place a wide (`long`/`long long`)
    // value's own bits would otherwise need to end up in `ac`, the
    // same single-check reasoning PIC16Codegen.h's own comment gives
    // (its one exception, `&expr`, applies here too - see
    // genAddrOf()'s own call sites, none of which ever call genExpr()
    // on the operand's own value).
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    void requireNarrow(const ir::Expr &e) const;
    // log2 of an element type's own width (1/2/4/8 bytes -> 0/1/2/3
    // left shifts) - see PIC16Codegen.h's own identical comment on
    // elemShift(): genIndexAddr()'s own scale factor for an index, so
    // a `char` array scales by 1 (no shift), not 2.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }
    void genConvert(const ir::Expr &e);

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

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step -
    // `bc` must already hold `old` and `ac` must already hold `rhs`
    // (both already at `type`'s own width/signedness, sign/zero-
    // extended into this backend's one native 16-bit `ac`/`bc` cell
    // exactly like any other value here) - the same register roles
    // genBinary()'s own per-operator code (inline, or genMul()/
    // genDivMod()/genShift()) already settles into right after
    // evaluating its own lhs/rhs, just supplied directly here instead
    // of via a nested genExpr() call. Leaves the raw (not yet re-
    // narrowed - see narrowAcTo()) new value in `ac`. `type` is never
    // wide here - IRBuilder.cpp's own rejectIfWide() guarantees it (on
    // top of this backend's own blanket requireNarrow() check).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - `ptr` must already hold the resolved
    // address on entry. See the .cpp's own comment.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows `ac` to `to`'s own width/signedness, after a raw
    // genCompoundOp() result - genConvert()'s own int-to-char
    // narrowing case, factored out, since both are "a native-word
    // value, now re-interpreted at `to`'s own narrower width"
    // operations.
    void narrowAcTo(Type to);

    // `slot`'s own byte offset from `FP` within the current
    // function's own frame (Local/Param only - a Global never goes
    // through `FP` at all, see slotAddrToPtr()) - a Param's own
    // `paramIndex` placed first (`paramCount_` of them, 2 bytes
    // each), then a Local's own `frameOffset` - already a real byte
    // offset, 1-based from 2 (see IR.h's own comment on
    // Slot::frameOffset) - placed right after.
    int slotFrameOffsetBytes(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return slot->paramIndex * 2;
        return paramCount_ * 2 + slot->frameOffset - 2;
    }

    // Adopts `NFP` (already computed and left by the caller - see
    // genExpr()'s own `Call` case) as this call's own `FP`, saving the
    // caller's own `FP` at the one byte directly below it first - see
    // this file's own header comment on `FP`/`NFP`/`FSTACK`.
    void emitPrologue();
    // The reverse, run immediately before every `return`: restores
    // the caller's own `FP` from that same saved byte.
    void emitEpilogue();

    // Sets `ptr` to `slot`'s own effective Access Bank address: its
    // fixed, compile-time label for a Global, or `FP` plus
    // slotFrameOffsetBytes() for a Local/Param - see this file's own
    // header comment.
    void slotAddrToPtr(const ir::Slot *slot);
    // `ac` = `slot`'s own current value, sign/zero-extended into `ac`'s
    // own high byte per `slot->type` when it's a `char` (see
    // signExtendAcHighByte()'s own comment) - a plain copy2()/
    // loadAcFromLabel() for a Global, or slotAddrToPtr()+
    // loadAcIndirect() for a Local/Param.
    void loadSlotToAc(const ir::Slot *slot);
    // The reverse: writes `ac`'s own low `widthOfType(slot->type)`
    // byte(s) into `slot`.
    void storeAcToSlot(const ir::Slot *slot);

    // Via FSR0L/INDF0/POSTINC0 - see this file's own header comment
    // on why this core's real auto-increment indirect mode needs one
    // fewer instruction than PIC16's own FSR/INDF-plus-INCF pair.
    // `width` is 1 or 2 (never wider - see requireNarrow()'s own
    // comment); `loadAcIndirect()`'s own `width == 1` case sign/zero-
    // extends per `isUnsigned`, the same as PIC16Codegen.h's own
    // identical pair.
    void storeAcIndirect(int width);
    void loadAcIndirect(int width, bool isUnsigned);
    void signExtendAcHighByte(bool isUnsigned);
    void loadAcFromLabel(const std::string &lbl, int width, bool isUnsigned);
    void storeAcToLabel(const std::string &lbl, int width);

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

    // Emits the globalInit startup code that writes one array
    // element's own `width`-byte value into its already-reserved RAM
    // bytes starting at `base` - see generate()'s own call site for
    // how that's named and sized.
    void emitInitElem(std::ostream &out, const std::string &base, int width, const ir::ArrayInitElem &e);

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

    // The function currently being generated's own frame shape - see
    // this file's own header comment on `FP` and
    // slotFrameOffsetBytes()'s own comment. Set once, at the top of
    // every genFunction() call, from `fn.func->params.size()` and
    // `fn.func->frameSize` directly; never read across one function's
    // own codegen into another's, since generation itself is single-
    // pass and sequential, one function fully finished before the
    // next starts.
    int paramCount_ = 0;
    int frameSizeBytes_ = 0;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
