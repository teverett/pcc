#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PIC18 assembly for the IR built by IRBuilder, modeled on the
// PIC18F2420: Access Bank SFR aliasing (STATUS=0xFD8, FSR0L/FSR0H=
// 0xFE9/0xFEA, INDF0/POSTINC0=0xFEF/0xFEE, INTCON=0xFF2), 16K-word program
// memory (0x00000-0x03FFF, so the 20-bit absolute GOTO/CALL range never
// needs paging), and a 31-level hardware call stack (fixed-depth,
// return-address only, not addressable or extendable). `generate()`
// returns a complete program starting at the reset vector (0x000000),
// halting in an infinite loop. Unlike most backends here, this one
// supports real recursion (direct or mutual), up to whatever depth the
// hardware call stack or `FSTACK` (below) allows first.
//
// Shares with PIC16Codegen.h: the Harvard code/data split and its real
// startup code; the fixed 2-byte pseudo-registers (ac/bc/cx/dx); the real
// SSTACK/SP software push/pop stack for saving a binary op's other
// operand across evaluating a subexpression (a compile-time-indexed fixed
// cell is unsound since a nested call clobbers it); `ptr` as one byte;
// no hardware multiply/divide (shift-and-add / repeated-subtraction,
// non-negative operands only); no variable-count shift. `FP`/`NFP` (one
// byte each) and `FSTACK` are this backend's addition.
//
// Scope restriction: like PIC16's 68-byte-GPR limit, this backend only
// allocates data in the 96-byte Access Bank GPR window (0x00-0x5F); real
// banked GPR beyond that is never used, so no BSR bank-select code is
// needed. Every Local/Param of every function shares the one `FSTACK`
// region instead of each getting a permanently-reserved byte pair, so
// this window holds every simultaneously live call's frame, not every
// function definition's storage at once.
//
// What's genuinely different from PIC16:
//
// - ADDWFC/SUBWFB (carry-chained add/subtract) are PIC18-only, so 16-bit
//   add/subtract use the shorter textbook idiom (low byte via ADDWF/
//   SUBWF, high byte via ADDWFC/SUBWFB continuing the chain) - no manual
//   BTFSC+ADDLW-1 fixup needed.
// - STATUS has real N and OV bits PIC16 lacks, so signed comparisons use
//   the standard N-xor-OV rule directly instead of a sign-bias trick.
// - FSR0/INDF0/POSTINC0 (hardware auto-incrementing indirect addressing)
//   replaces PIC16's FSR/INDF-plus-explicit-INCF pair, one instruction
//   fewer per 16-bit indirect access.
// - MOVFF (a two-word register-to-register move bypassing W) replaces the
//   MOVF-then-MOVWF pair, leaving W/STATUS undisturbed.
//
// Real per-call recursion: every Local and Param lives in a per-call
// frame on `FSTACK`, addressed indirectly through FSR0/INDF0/POSTINC0
// rather than in one shared, statically-named cell, so two live calls to
// the same function get two distinct copies of its Locals/Params.
//
// `FP` (one byte) holds the address of the current call's frame: its
// Params (by paramIndex, 2 bytes each) immediately followed by its
// Locals (by Slot::frameOffset - see slotFrameOffsetBytes()). Reading/
// writing a Local/Param computes its address as `FP + <compile-time
// offset>` into `ptr` and goes through the same indirect FSR0 path as any
// other computed address; a Global's address is a plain fixed compile-
// time symbol instead, since globals are shared across every call.
//
// One byte directly below `FP` (`FP-1`) holds this call's saved caller
// `FP`, a software frame chain needed because the hardware call stack
// can't carry anything else. emitPrologue() saves the caller's `FP` there
// and adopts `NFP` (computed by the caller immediately before `call`) as
// its own new `FP`; emitEpilogue() (before every `return`) reads that
// byte back to restore the caller's `FP`. This is correct at any call
// depth, including recursive, since each call's prologue/epilogue only
// ever touches its own `FP-1`.
//
// A call site computes where the callee's new frame will go
// (`FP + frameSizeBytes_ + 1`), writes each argument directly into that
// not-yet-adopted frame via `ptr`, then hands that address to the callee
// in `NFP` right before `call`, recomputed fresh immediately beforehand
// since an argument expression can itself contain a nested call that
// would otherwise clobber `NFP` early. No separate "stack top" register
// is needed: a call's new frame address is always a compile-time-
// constant offset from the caller's current `FP`.
//
// `FSTACK` is a fixed-size region, so recursion depth is bounded by
// whichever runs out first: this region's byte budget, or the hardware
// call stack. Sized generously for ordinary recursion depths, not to
// guarantee the full 31-level hardware limit; a program combining large
// frames with deep recursion can still exhaust `FSTACK` first, reported
// the same way this backend reports Access Bank exhaustion in general.
//
// `objectMode` (separate compilation) assembles at CODE address 0 but
// omits the entry sequence entirely; PIC18LinkerFactory.cpp's
// synthesized entry object handles that once every object is merged.
// The fixed pseudo-registers become compile-time equates at the same
// addresses in every object, needing no relocation.
//
// A global variable's initial value is still written at runtime (data
// memory is RAM with no initial contents), so in object mode that
// write-out code runs as the prologue of `fn_main` instead of `START:`.
// Only the file defining `main` gets this treatment; under real separate
// compilation no function's local/parameter gets zero-initialized the
// way whole-program mode's `START:` sequence otherwise guarantees.
//
// Every global/local/param's Access-Bank address is a relocatable
// assembler symbol, so separately-compiled files' distinct globals get
// distinct, non-colliding linked data-space addresses once merged by
// GenericLinker's Harvard-data-space layout pass.
class PIC18Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic18"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".pic18"; }
    // `hex` is the whole-program Intel-HEX output; the other formats mean
    // a relocatable object file for separate compilation.
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
    // prologue and before `fn`'s body) is how generate() runs the
    // whole-program's globalInit code as part of `fn_main`'s prologue
    // under `objectMode`.
    void genFunction(const ir::FunctionBody &fn, const std::string &extraPrologue = "");
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

    // `ac` and every other pseudo-register is a fixed 2 bytes, and the
    // 16-bit helpers (add16()/sub16(), shiftOneBit(), pushCell2()/
    // popCell2(), ...) are hardcoded to that width, so requireNarrow()
    // (called at the top of genExpr()) fails loudly on a wide
    // (long/long long) value. The one exception is `&expr`, whose
    // genAddrOf() call sites never call genExpr() on the operand's value.
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    void requireNarrow(const ir::Expr &e) const;
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3 left
    // shifts): genIndexAddr()'s scale factor for an index.
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

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `bc`
    // must already hold `old` and `ac` must already hold `rhs`. Leaves the
    // raw (not yet re-narrowed) new value in `ac`. `type` is never wide
    // here (IRBuilder.cpp's rejectIfWide()).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - `ptr` must already hold the resolved address.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows `ac` to `to`'s width/signedness after a raw
    // genCompoundOp() result.
    void narrowAcTo(Type to);

    // `slot`'s byte offset from `FP` within the current frame (Local/Param
    // only; a Global never goes through `FP`). A Param's paramIndex comes
    // first (2 bytes each), then a Local's frameOffset right after.
    int slotFrameOffsetBytes(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return slot->paramIndex * 2;
        return paramCount_ * 2 + slot->frameOffset - 2;
    }

    // Adopts `NFP` (computed and left by the caller) as this call's `FP`,
    // saving the caller's `FP` at the byte directly below it first.
    void emitPrologue();
    // Reverse, run before every `return`: restores the caller's `FP`.
    void emitEpilogue();

    // Sets `ptr` to `slot`'s effective Access Bank address: a fixed
    // compile-time label for a Global, or `FP` plus
    // slotFrameOffsetBytes() for a Local/Param.
    void slotAddrToPtr(const ir::Slot *slot);
    // `ac` = `slot`'s current value, sign/zero-extended into `ac`'s high
    // byte when `slot->type` is `char`.
    void loadSlotToAc(const ir::Slot *slot);
    // The reverse: writes `ac`'s low `widthOfType(slot->type)` byte(s)
    // into `slot`.
    void storeAcToSlot(const ir::Slot *slot);

    // Via FSR0L/INDF0/POSTINC0's hardware auto-increment (one instruction
    // fewer than PIC16's FSR/INDF-plus-INCF pair). `width` is 1 or 2;
    // loadAcIndirect()'s width == 1 case sign/zero-extends per
    // `isUnsigned`.
    void storeAcIndirect(int width);
    void loadAcIndirect(int width, bool isUnsigned);
    void signExtendAcHighByte(bool isUnsigned);
    void loadAcFromLabel(const std::string &lbl, int width, bool isUnsigned);
    void storeAcToLabel(const std::string &lbl, int width);

    // "movff src,dst\nmovff src+1,dst+1\n".
    void copy2(const std::string &dst, const std::string &src);
    void clear2(const std::string &lbl);
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // 16-bit `dst = a + b` / `dst = a - b` via ADDWFC/SUBWFB's carry/
    // borrow chain.
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    void inc16(const std::string &lbl);

    void testAcZero();
    void gotoIfZero(const std::string &target);
    void gotoIfNonZero(const std::string &target);

    // Runtime push/pop on the shared `SSTACK` region via `SP`/FSR0/
    // POSTINC0/INDF0.
    void pushCell2(const std::string &src);
    void popCell2(const std::string &dst);
    void pushPtrCell();
    void popPtrCell();

    // Emits startup code writing one array element's `width`-byte value
    // into its reserved RAM bytes starting at `base`.
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

    // The function currently being generated's frame shape. Set once, at
    // the top of every genFunction() call, from fn.func->params.size()
    // and fn.func->frameSize.
    int paramCount_ = 0;
    int frameSizeBytes_ = 0;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue
    // label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
