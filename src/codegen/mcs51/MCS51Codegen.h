#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates MCS-51 assembly (standard 8051 mnemonics, see
// MCS51Assembler.h) from IRBuilder's IR, covering the 8051/8052/8031/
// 8032 family as one target (software-identical for codegen purposes).
//
// Real Harvard architecture: CODE (fetched via PC) and external DATA
// (XDATA, via MOVX/DPTR) are distinct address spaces. generate() emits
// a self-contained program starting at CODE address 0 (the real reset
// vector), looping forever at exit; objectMode omits that entry
// sequence (left to MCS51LinkerFactory's synthesized entry object) and
// instead runs global initializers in fn_main's prologue, since XDATA
// is RAM with no static initial contents. A global only gets storage/
// an initializer in the file that defines it; cross-file extern
// globals are unsupported (parameters work fine, passed positionally
// through the callee's frame regardless of which file defines it).
//
// No general-purpose 16-bit register, so every in-flight value lives
// in one of seven fixed two-byte pseudo-registers in internal RAM:
// `ac` (result), `bc` (other operand), `cx`/`dx` (mul/div scratch),
// `ptr` (indirection target), `fp` (current frame base), `ssp` (frame-
// stack top). Ordinary variables (globals/locals/arrays) live in
// XDATA instead, reached only via MOVX+DPTR. `dpl`/`dph` name the real
// DPL/DPH SFRs (0x82/0x83), used to read a just-loaded DPTR address
// back into a pseudo-register pair.
//
// Each call gets a software frame in a shared `__frame_stack` buffer:
// genPrologue() saves the caller's `fp` on the real hardware stack,
// sets `fp` = `ssp`, and bumps `ssp` past locals; genEpilogue() resets
// `ssp` to `fp` (reclaiming locals and arguments together, callee-
// side) and restores `fp`. See loadFrameAddress() for how a Local/
// Param becomes an `fp`-relative address.
//
// No hardware 16-bit multiply/divide (`MUL AB`/`DIV AB` are only
// 8x8->16 / 8/8) and no variable-count shift (`RLC`/`RRC` always
// rotate one bit), so both are software loops (unsigned operands
// only; see README's "Scope and limitations"). Signed comparisons use
// the "N xor V" idiom against the real ACC.7 sign bit and OV flag.
//
// Every jump/call uses full-range `LJMP`/`LCALL` rather than the
// 8051's short, ±127-byte conditional branches, which could land out
// of range on a large generated function; see safeBranch().
class MCS51Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8051"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s51"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other formats produce
    // a relocatable ObjectFile for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    // Used by genConvert() to tell a char-width conversion (needs
    // masking/sign-extension) apart from other, already-same-width ones.
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }

    void genFunction(const ir::FunctionBody &fn);

    // Establishes `fn`'s frame (see header comment); also sets
    // `curParamCount_` for loadFrameAddress().
    void genPrologue(const ir::Function *fn);
    // Tears the current frame down; used by both genFunction()'s
    // fall-through exit and Return's early one.
    void genEpilogue();

    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register
    // `ac` is a fixed 2-byte cell; aborts at compile time rather than
    // silently truncating a `long`/`long long` value.
    void requireNarrow(const ir::Expr &e) const;

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genConvert(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s effective XDATA address in `ptr` - the
    // only runtime-computed address this backend needs.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step:
    // `bc` must already hold `old` and `ac` must already hold `rhs`.
    // Leaves the raw (not yet re-narrowed - see narrowAcTo()) result
    // in `ac`. `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed/Local/Param-scalar form
    // and CompoundDerefAssign - `ptr` must already hold the resolved
    // address on entry.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-narrows `ac` to `to`'s width/signedness after a raw
    // genCompoundOp() result.
    void narrowAcTo(Type to);

    // `ptr` must already hold the target XDATA address (left
    // untouched); writes `ac`'s low `width` bytes there via DPTR+MOVX.
    // `width` must match the destination's real width, or this
    // overwrites whatever is packed right after it.
    void storeIndirect(const std::string &srcReg, int width);

    // Read-side counterpart of storeIndirect(): reads `width` bytes
    // from `[ptr]` into `dstReg`, sign/zero-extending its high byte
    // when `width==1` (per `isUnsigned`). Not used when `ptr` holds a
    // pointer variable's own stored value, which stays a plain,
    // explicit `width=2` call.
    void loadIndirect(const std::string &dstReg, int width, bool isUnsigned);

    // Copies one pseudo-register to another via `movdd dst,src`.
    void copy16(const std::string &dst, const std::string &src);

    // Adds/subtracts a compile-time-constant 16-bit value to/from a
    // pseudo-register pair via chained 8-bit ADD/ADDC or SUBB.
    void addImm16(const std::string &reg, int value);
    void subImm16(const std::string &reg, int value);

    // Address of a Local/Param in the current frame: `fp +
    // 2*paramIndex` for a Param (params occupy the frame's first
    // words, in declaration order), `fp + 2*(curParamCount_ +
    // frameOffset - 1)` for a Local (locals follow params). Unlike
    // x86/i386's `bp`-relative locals, element i of a Local array is
    // base-plus-i here, never base-minus-i. Never called for a
    // Global, whose address is a plain compile-time label.
    void loadFrameAddress(const std::string &dstReg, const ir::Slot *slot);

    // Pushes `srcReg`'s two bytes onto the frame stack at `[ssp]` and
    // bumps `ssp` past them.
    void pushFrameWord(const std::string &srcReg);

    // Re-populates a Local array's frame words from `slot->arrayInit`
    // - needed every call, since (unlike a Global's static trailer) a
    // Local's storage isn't pre-set. Element i's address is the
    // array's frame base (loadFrameAddress()) plus i words.
    void genLocalArrayInit(const ir::Slot *slot);
    // Loads one array-initializer element into `ac`: a literal's
    // bytes directly, or a referenced Slot's/Function's address via
    // loadXdataAddress()/loadCodeAddress(). Always loads a full 16-bit
    // value; the caller decides how many bytes to actually store.
    void loadArrayElemValue(const ir::ArrayInitElem &e);

    // Reads/writes the two bytes at `xdataExpr`'s XDATA address via
    // DPTR+MOVX. `xdataExpr` (a label, optionally `+<constant>` for an
    // array element) is resolved against the assembler's XDATA label
    // table and, in object mode, relocated against "..d".
    void loadXdata(const std::string &dstReg, const std::string &xdataExpr);
    void storeXdata(const std::string &xdataExpr, const std::string &srcReg);

    // Loads `xdataExpr`'s XDATA address itself (not its contents) into
    // `dstReg`: `movdptr` resolves/relocates it into DPTR, then
    // `dpl`/`dph` are copied out. Used by genAddrOf() (`&g`, `&arr[i]`)
    // and genIndexAddr()'s array-base case.
    void loadXdataAddress(const std::string &dstReg, const std::string &xdataExpr);

    // Loads `codeExpr`'s CODE-space address (a function entry label or
    // a return-address label) into `dstReg` via `movdptrc`, then
    // `dpl`/`dph` copied out, same as loadXdataAddress() but against
    // the CODE-space symbol table. Used by genAddrOf() (`&fn`) and
    // genExpr()'s `CallPtr` handling.
    void loadCodeAddress(const std::string &dstReg, const std::string &codeExpr);

    // "the short branch Bcc almost reaches, plus an always-in-range
    // LJMP from there" - see this file's own header comment.
    // `branchMnemonic` is one of MCS51Assembler.cpp's own short
    // conditional-branch mnemonics ("jz", "jnz", "jc", "jnc",
    // "jbacc7", "jnbacc7", "jbov", "jnbov").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // Same idea as safeBranch(), but for the real `DJNZ direct,rel`
    // decrement-and-branch-if-nonzero instruction used by every loop
    // here (shift/multiply/divide's own counters) - `DJNZ` is always
    // the "taken" (still nonzero) edge, an always-in-range `LJMP`
    // the "not taken" (just hit zero) edge.
    void safeDjnz(const std::string &counterReg, const std::string &targetIfNonzero,
                  const std::string &targetIfZero);

    // Globals only; a Local/Param never gets an XDATA label (see
    // loadFrameAddress()).
    static std::string label(const ir::Slot *slot) {
        assert(slot->kind == ir::SlotKind::Global);
        return "g_" + slot->sourceName;
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // Emits an `<lbl>: xdata <count>` directive reserving `count`
    // XDATA bytes; the assembler assigns `lbl` its actual offset.
    void allocXdata(const std::string &lbl, int count);

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Allocates XDATA storage for a global and emits the MOVX writes
    // that give it its initial value into `initCode_` - XDATA is RAM,
    // with no static initial contents the way CODE/ROM has.
    void declareGlobalTemp(const std::string &lbl, long initValue);
    void declareGlobalArray(const std::string &lbl, int arraySize, int elemWidth,
                             const std::vector<ir::ArrayInitElem> &values);
    void genArrayElementInit(const std::string &elemExpr, int elemWidth, const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::ostringstream initCode_;  // globals' initial-value MOVX writes
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // Param count of the function currently being generated, set by
    // genPrologue(); loadFrameAddress() uses it to place Locals after
    // Params. No nested function definitions, so one field suffices.
    int curParamCount_ = 0;
    // One entry per currently-open loop (innermost last): {continue
    // label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
