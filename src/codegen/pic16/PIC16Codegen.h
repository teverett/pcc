#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PIC16 mid-range (14-bit core) assembly for the IR built by
// IRBuilder, modeled on the PIC16F84A: SFR map (INDF=0, STATUS=3, FSR=4,
// INTCON=0x0B), 68-byte GPR bank (0x0C-0x4F), 11-bit direct GOTO/CALL range
// (program memory 0x000-0x3FF, so PCLATH paging is never needed), and an
// 8-level hardware call stack. `generate()` returns a complete program
// starting at the reset vector (0x0000), halting in an infinite loop.
// Every Local/Param gets real per-call storage (see genFunction()'s
// emitPrologue()/emitEpilogue()), so recursion works up to the hardware
// call-stack depth.
//
// Harvard architecture: program memory (code) and file-register memory
// (data) are separate address spaces, so a label can never mean both "load
// from" and "code lives here". The in-process assembler (PIC16Assembler.h)
// tracks two independent cursors, `org` for code and `orgram` for data;
// codegen allocates data-space storage via its own `res` directive
// (declareRam(), below) rather than a "label: db values" trailer.
//
// Data memory has no defined power-on value, so this backend emits real
// startup code (generate()'s `START:` prologue) that clears or loads every
// global's bytes before `main` runs. Locals, params, and pseudo-registers
// are left alone at startup since they're always written before read.
//
// `int` is 16 bits. The core has one register (`W`) and no instruction
// moving more than one byte, so every 16-bit value lives in a fixed
// two-byte pseudo-register: `ac` (genExpr()'s result), `bc` (other
// operand), `cx`/`dx` (multiply/divide/shift scratch), `ptr` (one byte,
// see below). `W` is the transit register, loaded immediately before an
// ALU op needs it and stored back immediately after.
//
// The PIC16 hardware stack is a dedicated, non-addressable, fixed-depth
// return-address stack used only by CALL/RETURN - no data push/pop. So
// saving an operand across evaluating a subexpression needs a real
// software stack in file-register space: `SSTACK` plus an `SP` cursor,
// pushed/popped via FSR/INDF (pushCell2()/popCell2(),
// pushPtrCell()/popPtrCell()). A compile-time-indexed fixed-cell-per-depth
// scheme is unsound here because a call between a push and its matching
// pop reuses the same cell at the same nesting depth in the callee; a real
// stack avoids this since the hardware return-address stack and this
// software one are independent.
//
// The same `SSTACK`/`SP` stack also holds every Local/Param's per-call
// storage (genFunction()'s emitPrologue()/emitEpilogue(),
// frameSlotOffset()): a call frame is simply a longer-lived pushed region,
// torn down LIFO by the epilogue. `FP` holds the current call's frame base
// within `SSTACK`, saved/restored across each `call`. A frame holds params
// first (`FP+0`, `FP+2`, ... in paramIndex order), then the caller's saved
// `FP` (`FP+2*numParams`), then Locals in declaration order
// (frameSlotOffset()). `SSTACK` only ever grows upward (`SP` only
// increments), so a Local array's element `i` sits at increasing
// addresses, unlike a decreasing-address `bp`-relative frame.
//
// `ptr` is one byte, not two: the whole GPR bank fits in 7 bits (see
// PIC16Assembler.h's `fileAddr()` check), so no low/high split is needed.
// pushPtrCell()/popPtrCell() push/pop it on the same `SSTACK`.
//
// No hardware multiply/divide, so `*` is shift-and-add and `/`, `%` is
// repeated subtraction, both verified only against non-negative operands.
// No variable-count shift (RLF/RRF rotate one bit), so `<<`/`>>` loop one
// bit at a time.
//
// No ADDWFC/SUBWFB on this core (a PIC18-only enhancement), so 16-bit
// add/subtract compute the low byte first, then fold STATUS<C> into the
// high byte's operand before the second ADDWF/SUBWF via BTFSC/BTFSS +
// ADDLW 1 (see genBinary()).
//
// Comparisons (`<`,`<=`,`>`,`>=`) can't use a hardware overflow flag since
// PIC16's STATUS has no V bit, so this backend uses the sign-bit-bias
// trick (XOR 0x80 into both operands' high bytes) then an unsigned
// subtract-and-check-the-final-borrow.
//
// `objectMode` (separate compilation) assembles at CODE address 0 but
// omits the entry sequence; PIC16LinkerFactory.cpp's synthesized entry
// object handles that once every object is merged. A global variable's
// initial value is still written at runtime (file-register RAM has no
// initial contents), so in object mode that write-out code runs as the
// prologue of `fn_main` instead of `START`. Only the file defining `main`
// gets this treatment.
//
// Every Global's file-register address is a relocatable assembler symbol
// (see declareRam(), and PIC16Assembler.h's `kPIC16RelocData`/`"..d"`), so
// separately-compiled files' globals get distinct linked data-space
// addresses once merged. A Local/Param has no data-space symbol at all; it
// lives in a per-call frame inside `SSTACK` instead. The eight fixed
// scratch pseudo-registers/software-stack cells are shared by every
// function in every file, so they're declared as compile-time equates at
// a fixed address instead of relocatable data-space allocations.
class PIC16Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pic16"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".pic16"; }
    std::vector<std::string> assembledFormats() const override {
        return {"hex", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `hex` is the whole-program Intel-HEX image; the other formats are
    // relocatable ObjectFiles for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "hex" && format != "asm"; }
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

    // `ac` and every other pseudo-register is a fixed 2 bytes (native
    // `int` width); `char` fits narrower within that cell (genConvert()).
    // `long`/`long long` are wider than `ac` can hold, and the fixed-2-byte
    // pseudo-registers and SSTACK push/pop helpers have no parameterized
    // "N bytes" form, so requireNarrow() (called at the top of every
    // genExpr()) fails loudly on a wide operand rather than truncating
    // silently. The one exception is `&expr` (Unary "&"): its result is
    // always a plain address (native int width), and genAddrOf() computes
    // it without calling genExpr() on the operand's value, so
    // `&someLongVariable` still works even though reading it would not.
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
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame words

    // Leaves slot[idxExpr]'s effective (one-byte) file address in `ptr`,
    // scaled by the element type width (elemShift()). For a Global, the
    // base is a compile-time address or a pointer variable's stored value;
    // for a Local/Param, the base is FP-relative (frameSlotOffset()), read
    // via FSR/INDF first.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `bc`
    // must already hold `old` and `ac` must already hold `rhs`. Leaves the
    // raw (not yet re-narrowed) new value in `ac`.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - `ptr` must already hold the resolved address.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows `ac` to `to`'s width/signedness after a raw
    // genCompoundOp() result.
    void narrowAcTo(Type to);

    // Assumes `ptr` already holds a target file address; writes `ac`'s low
    // `width` byte(s) there via FSR/INDF (`width` 1 or 2). For width == 1
    // the stale high byte is left untouched.
    void storeAcIndirect(int width);
    // The reverse: reads `width` byte(s) at `ptr` into `ac`, sign- or
    // zero-extending a width == 1 source per `isUnsigned`.
    void loadAcIndirect(int width, bool isUnsigned);
    // Sets `ac`'s high byte to 0 (unsigned) or a sign-extension of `ac`'s
    // low byte's bit 7 (signed), matching `char`-to-`int` promotion.
    void signExtendAcHighByte(bool isUnsigned);
    // The Global counterpart of storeAcIndirect()/loadAcIndirect(): `lbl`
    // is a compile-time label.
    void loadAcFromLabel(const std::string &lbl, int width, bool isUnsigned);
    void storeAcToLabel(const std::string &lbl, int width);

    // Copies one 2-byte pseudo-register/slot to another.
    void copy2(const std::string &dst, const std::string &src);
    // Zeros a 2-byte pseudo-register/slot.
    void clear2(const std::string &lbl);
    // One in-place 1-bit rotate of the 2-byte cell `lbl` (no variable-count
    // shift instruction exists on this core).
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // 16-bit `dst = a OP b` (ADDWF/SUBWF) via the standard PIC16
    // carry-fixup idiom.
    void add16(const std::string &dst, const std::string &a, const std::string &b);
    void sub16(const std::string &dst, const std::string &a, const std::string &b);
    // 16-bit increment via INCFSZ's "skip next if the result wrapped to
    // zero" idiom, chaining a carry with no ADDWFC available.
    void inc16(const std::string &lbl);

    // Sets STATUS<Z> to reflect whether `ac` is zero, trashing W.
    void testAcZero();
    // Jumps to `target` if the most recent testAcZero() found `ac`
    // zero/non-zero respectively, otherwise falls through.
    void gotoIfZero(const std::string &target);
    void gotoIfNonZero(const std::string &target);

    // Runtime push/pop on the shared `SSTACK` region via `SP`/FSR/INDF.
    // pushCell2()/popCell2() move a 2-byte pseudo-register;
    // pushPtrCell()/popPtrCell() move `ptr`'s one byte.
    void pushCell2(const std::string &src);
    void popCell2(const std::string &dst);
    void pushPtrCell();
    void popPtrCell();

    // Emits startup code writing one array element's `width`-byte value
    // into its reserved RAM bytes starting at `base`.
    void emitInitElem(std::ostream &out, const std::string &base, int width, const ir::ArrayInitElem &e);

    // A Local/Param's word offset from `FP`. A Param's position needs no
    // knowledge of the current function (`paramIndex` alone fixes it); a
    // Local's does, via `curNumParams_` (set once per function by
    // genFunction()).
    int frameSlotOffset(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) return 2 * slot->paramIndex;
        return 2 * curNumParams_ - 1 + slot->frameOffset;
    }
    // Leaves FP + frameSlotOffset(slot) in `W`, without touching `ptr`/FSR.
    void loadFrameBaseW(const ir::Slot *slot);
    // ptr = FP + frameSlotOffset(slot): a Local/Param scalar's effective
    // file address, followed by storeAcIndirect()/loadAcIndirect().
    void loadFrameAddr(const ir::Slot *slot);
    // Per-call prologue/epilogue. emitPrologue() pushes the caller's `FP`
    // (right after the argument words the caller pushed before `call`),
    // sets `FP` to this call's frame base, then reserves `frameSize` more
    // words for Locals. emitEpilogue() undoes that in reverse (`SP = FP`,
    // a callee-cleans-up convention) and restores the caller's `FP`.
    void emitPrologue(int numParams, int frameSize);
    void emitEpilogue(int numParams);

    // A Global's name-based label. Never called with a Local/Param, which
    // has no static address and is reached only through
    // frameSlotOffset()/loadFrameAddr() instead.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    // Reserves `count` bytes of file-register space for `lbl` - a Global
    // or a fixed pseudo-register/software-stack name, never a Local/Param.
    // In whole-program mode this always emits the `res` directive. In
    // object mode, one of the eight fixed scratch names instead becomes a
    // compile-time equate at its fixed address (`kPIC16Reg*`); a real
    // Global still emits `res`, interpreted as a relocatable data-space
    // symbol.
    void declareRam(const std::string &lbl, int count);

    std::ostringstream code_;
    std::vector<std::string> ramDecls_;
    std::map<std::string, std::string> perFileAsm_;

    // Set once, at the top of generate().
    bool objectMode_ = false;

    // Number of Params the function currently being generated declares;
    // set once per function, read by frameSlotOffset().
    int curNumParams_ = 0;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue label,
    // break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
