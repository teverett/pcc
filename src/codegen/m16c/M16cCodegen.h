#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Renesas M16C assembly (this project's own dialect - see
// M16cAssembler.h) for the IR built by IRBuilder. `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`), halting in an infinite loop.
//
// Registers: `R0`-`R3` general, `A0`/`A1` dedicated address registers, `FB`
// frame-base, `SP` a real hardware stack (`PUSH.W`/`POP.W`/`JSR.W`/`RTS`).
// Every activation gets a real frame: prologue `PUSH.W FB` / `MOV.W SP,FB` /
// `SUB.W N,SP`; epilogue `MOV.W FB,SP` / `POP.W FB`. A Call's arguments are
// pushed last-declared first, so argument 0 ends up closest to the return
// address (matching `frameOperand()`'s `FB+4+2*paramIndex`); the caller pops
// them back off after the call.
//
// `int` is 16 bits. `R0` holds genExpr()'s result ("ac"); `R1` holds "the
// other operand" in a binary op. `A0` holds a computed address when
// dereferencing one; it's saved (via the stack) only across a nested
// subexpression that might itself compute another address into `A0`.
//
// `*`/`/` use hardware `MUL.W`/`DIVU.W`: `MUL.W src,R0` leaves a 32-bit
// product in `R2:R0` (high:low); `DIVU.W src,R0` divides `R2:R0` by a 16-bit
// divisor into quotient `R0`/remainder `R2`. Only unsigned divide is used
// (`R2` zeroed, never sign-extended).
//
// `<<`/`>>` use hardware `SHL.W count,dst` directly: the count is a signed
// operand (positive = left, negative = right), so `>>` is `SHL.W` with the
// count negated (`NEG.W`) - no bit-at-a-time loop needed.
//
// Comparisons use `CMP.W` + a signed conditional jump; `CMP.W src,dst` sets
// flags from `dst - src`, so `genCompare()`'s `CMP.W R0,R1` (R1=lhs, R0=rhs)
// computes `lhs - rhs` directly.
//
// Every jump/conditional jump always encodes a full absolute address (a
// deliberate simplification - real M16C conditional jumps are short and
// PC-relative), so there's no branch-range limit to work around.
class M16cCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "m16c"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s16c"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other formats/`objraw` are
    // relocatable object files for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in R0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // Leaves r0 holding 0/nonzero; a wide value's words are first ORed
    // together (it's true iff any word is nonzero).
    void testTruthy(const ir::Expr &e);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    // Shared divide/sign-fixup tail of genDivMod(), driven directly off
    // r0=dividend/r1=divisor by genCompoundOp() too.
    void genDivModCore(bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step; old in
    // r0, rhs in r1, matching genBinary()'s own operand roles. Leaves the
    // untruncated new value in r0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign;
    // `a0` must already hold the resolved address on entry.
    void genCompoundThroughA0(const ir::Expr &e);
    // Re-truncates r0 to `to`'s width, re-extending per its signedness.
    void narrowR0To(Type to);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    int wordCountOf(Type t) const { return widthOfType(t) / 2; }
    // log2 of the element width (1/2/4/8 bytes -> 0/1/2/3).
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from effective-address text `mem` into r0; a
    // `char` sign-extends via `movb`'s hardware behavior, masked back off if
    // unsigned. A wide type's words land on the stack instead (genWideLoad()).
    void loadOperand(const std::string &mem, Type type);
    // Stores r0 into `mem`, truncated to `type`'s width.
    void storeOperand(const std::string &mem, Type type);

    // `long`/`long long` values of word count N live on the stack as N
    // consecutive words, word 0 (least significant) closest to `sp`.
    //
    // This dialect has no hardware carry flag an `add`/`sub` could chain
    // through, so carry/borrow is synthesized from an unsigned comparison
    // (`jltu`) against each word's pre-op value (see genCarryBool()).
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int words);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    // Negates the `words`-word stack-top value in place (NOT every word,
    // then add 1 through the same carry chain as genWideAdd()).
    void genWideNegate(int words);
    // Pushes `slot`'s `words`-word value, word 0 ending up on top.
    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    // Pops `words` words off the stack into `slot`'s storage.
    void genWideStore(const ir::Slot *slot, int words);
    // Word `wordIndex` (0 = least significant) of `slot`'s multi-word
    // storage; each later word is 2 bytes further from `fb`/the label.
    std::string wideWordOperand(const ir::Slot *slot, int wordIndex) const;
    // Leaves r1 holding 0xFFFF if r0 (already loaded) is negative, else 0.
    void genSignFillWord();

    // Leaves slot[idxExpr]'s effective address in `A0`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // A Global or Function gets a name-based label (stable across separate
    // compilation); a Local/Param never crosses a file boundary and is
    // addressed relative to `FB` instead, with no label at all.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's frame-relative operand, e.g. "[fb-4]"/"[fb+6]". `FB`
    // points at this call's saved old `FB`; the return address and the
    // caller's pushed arguments (argument 0 closest) sit above it, so a
    // Param's address is `FB+frameOffset`. Locals live below `FB`
    // (`FB-frameOffset`), reserved by the prologue's `SUB.W N,SP`. A Local
    // array's is its element 0, its lowest address (see ir::localLowOffset()).
    std::string frameOperand(const ir::Slot *slot) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[fb+" + std::to_string(slot->frameOffset) + "]";
        }
        return "[fb-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths())) + "]";
    }

    // Operand text for a plain read/write of `slot`'s value.
    std::string operandOf(const ir::Slot *slot) const {
        return slot->kind == ir::SlotKind::Global ? label(slot) : frameOperand(slot);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // One entry per open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
