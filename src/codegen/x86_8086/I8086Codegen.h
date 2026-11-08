#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 16-bit real-mode 8086 assembly (NASM syntax). Every
// variable/temporary is one 16-bit word; register-indexed addressing
// (`[label+bx]`) handles arrays and pointers.
//
// `--format=raw`: generate() emits a self-contained 512-byte boot
// sector (zeroes segment registers, sets up a stack at 0x7C00, calls
// main, halts). A program too large for one boot sector needs
// `--format=coff` instead (an ordinary object file with no size
// limit); linking/packaging that into a bootable image is not this
// compiler's job.
class I8086Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8086"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".asm"; }
    std::vector<std::string> assembledFormats() const override { return {"raw", "coff", "elf"}; }
    std::string defaultAssemblerPath() const override { return "nasm"; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:

    void emitPlacement(long address);
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in AX

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue, bool isUnsigned);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);          // slot[idx] read
    void genAddrOf(const ir::Expr &operand);   // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);
    // old (ax) <op> rhs (bx) -> raw result in ax, not yet truncated. type is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for an indexed CompoundAssign/CompoundDerefAssign; bx holds the resolved address.
    void genCompoundThroughBx(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width, used to scale an array index.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads `mem` into AX, sign/zero-extending a narrower-than-int type; for a wide
    // type (long/long long) leaves the ADDRESS of its storage in AX instead.
    void loadOperand(const std::string &mem, Type type);
    // Stores AX into `mem`, truncated to `type`'s width. Never called for a wide type.
    void storeOperand(const std::string &mem, Type type);
    // Re-extends AX from its low byte per `to`'s signedness.
    void narrowAxTo(Type to);
    void genConvert(const ir::Expr &e);

    // Leaves the address of slot's N-word (N=widthOfType(type)/2) storage in AX,
    // words stored low-to-high as address increases.
    void genWideSlotAddr(const ir::Slot *slot);
    // Builds a fresh toN-word low-to-high block on the stack from the fromN-word
    // block at the address in BX, sign/zero-extending per isUnsignedExt when
    // widening. Leaves the new block's address in AX.
    void copyWideBlock(int fromN, int toN, bool isUnsignedExt);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // Emits the N-word bit-serial divide subroutine into trailer_, once per N
    // (inlining it at every call site would overflow the 512-byte raw-format budget).
    void emitWideDivModSubroutine(int n);
    std::set<int> wideDivModEmitted_;
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the N-word value whose address is in AX, in place.
    void genWideNegate(int n);
    // Negates N words at scratch offset wordOffset (bx = scratch base), unconditionally
    // or only if the flag word at flagOffset is non-zero.
    void genWideNegateAt(int wordOffset, int n);
    void genWideNegateIf(int wordOffset, int n, int flagOffset);
    // Reduces the N-word value whose address is in AX to a single word in AX
    // that's zero iff every word was.
    void genWideTruthify(Type t);

    // Effective-address operand text (e.g. "[g_arr3+bx]") for slot[idxExpr], with bx loaded.
    std::string computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Globals/Functions get a name-based label (stable across separate compilation);
    // a Local/Param is addressed relative to bp instead (see addr()) and needs no label.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local/Param's effective-address text, e.g. "[bp-4]"/"[bp+6]": the prologue's
    // `push bp`/`mov bp,sp` puts bp just above the saved old bp, with the return
    // address above that and pushed args above that (param 0 closest), so a Param's
    // address is bp+frameOffset; Locals live below bp (bp-frameOffset). A Local array is
    // addressed from its lowest address, element 0 (see ir::localLowOffset()), and
    // `byteOffset` counts upward from there, the same direction as a Global array's elements.
    std::string addr(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[bp+" + std::to_string(slot->frameOffset + byteOffset) + "]";
        }
        if (slot->arraySize > 0) {
            return "[bp-" + std::to_string(ir::localLowOffset(*slot, typeWidths()) - byteOffset) + "]";
        }
        return "[bp-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths()) + byteOffset) + "]";
    }

    // The memory-operand text for a plain read/write of `slot`'s own
    // value, `byteOffset` bytes into it - "[g_x]"/"[g_x+N]" for a
    // Global, or addr(slot, byteOffset) ("[bp±N]") for a Local/Param -
    // used anywhere the two only differ in which of those this is (an
    // ordinary Var read/Assign write; see genAddrOf/computeElementAddr
    // for the two places that still have to tell them apart, since
    // only a Global's own address folds into a compile-time immediate
    // the way `mov ax, g_x` does).
    std::string operandOf(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? ("[" + lbl + "]") : ("[" + lbl + "+" + std::to_string(byteOffset) + "]");
    }

    std::string allocTemp();
    void declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel();

    // Wide values are built in stack temporaries that nothing pops; wideScope() runs `gen`
    // with `sp` saved in a frame cell below the Locals (one per nesting level) and restores
    // it afterward, reclaiming them. Used wherever a wide value is consumed into a narrow
    // result or a statement ends, so a narrow value pushed earlier pops back correctly and
    // a loop doesn't grow the stack.
    void wideScope(const std::function<void()> &gen);
    // genExpr(), then for a wide value reduce it to a nonzero-if-true word in ax. At a
    // statement's top level nothing else is on the stack, so `sp` is just reset to the
    // frame's bottom (resetStackToFrame()) instead of saved and restored.
    void genTruthValue(const ir::Expr &e, bool atStatement = false);
    // Resets `sp` to the bottom of this function's frame, where it is between statements.
    void resetStackToFrame();
    // Stands in for the frame size in the function body until genFunction() knows it.
    static constexpr const char *kFrameBottomPlaceholder = "@FRAME@";
    bool involvesWide(const ir::Expr &e) const;
    int curFrameSize_ = 0;
    int wideScopeDepth_ = 0;
    int maxWideScopeDepth_ = 0;

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    // {continue label, break label} per currently-open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
