#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 32-bit protected-mode i386 assembly for the IR built by IRBuilder. `eax` is the
// accumulator; `ebx` doubles as the other binary-op operand and the one indirect-addressing
// register. `int` is 32 bits.
//
// This backend never transitions from real mode to protected mode itself: generate() emits
// a Multiboot header instead, and a real bootloader (GRUB, or qemu-system-i386 -kernel)
// does that transition before this image's first instruction runs. The entry sequence only
// masks both 8259 PICs and calls `main`. Real separate compilation (objectMode) builds a
// format-neutral ObjectFile instead of a self-contained Multiboot image; I386LinkerFactory
// synthesizes the one real entry object once every user object is merged.
// Linux hosted mode uses ELF/i386 symbols; Windows hosted mode uses COFF symbols and the i386
// Windows C calling convention.
class I386Codegen : public Backend {
   public:
    void setHostedAbi(bool enabled) { hostedAbi_ = enabled; }
     void setWindowsAbi(bool enabled) { windowsAbi_ = enabled; }
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "i386"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".s386"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is the whole-program flat Multiboot image; the other five formats are real
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
    void genExpr(const ir::Expr &e);  // leaves the result in eax

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue, bool isUnsigned);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo: bit-serial shift/compare/subtract long division, 64
    // iterations, using a stack-resident scratch block addressed via `ebx`. Leaves the
    // result in edx:eax.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void genNegate64();  // negates the 64-bit value in edx:eax, in place
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // "new = old <op> rhs": old in eax, rhs in ebx, leaves the untruncated new value in eax.
    // `type` is never wide here.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign; ebx must
    // already hold the resolved address on entry.
    void genCompoundThroughEbx(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3), the shift amount an index
    // is scaled by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from effective-address text `mem` (e.g. "[g_x]"/"[ebp-8]")
    // into eax (or edx:eax for a wide type), sign/zero-extending a narrower-than-int source.
    // `highMem` is the high dword's operand text for the wide case.
    void loadOperand(const std::string &mem, const std::string &highMem, Type type);
    // Stores eax (or edx:eax) into `mem`/`highMem`, truncated to `type`'s width.
    void storeOperand(const std::string &mem, const std::string &highMem, Type type);
    // Re-extends eax from its low widthOf(to) bytes, per `to`'s signedness.
    void narrowEaxTo(Type to);

    // Effective-address operand text (e.g. "[g_arr3+ebx]") for slot[idxExpr], with ebx
    // already loaded; used by both an Index read and an indexed Assign.
    std::string computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Global slots and Functions get a name-based label; a Local or Param is addressed
    // relative to `ebp` instead (see addr()).
    std::string label(const ir::Slot *slot) const {
        return (windowsAbi_ ? "_" : hostedAbi_ ? "" : "g_") + slot->sourceName;
    }
    std::string label(const ir::Function *fn) const {
        return (windowsAbi_ ? "_" : hostedAbi_ ? "" : "fn_") + fn->sourceName;
    }

    // A Local or Param's effective-address operand text, e.g. "[ebp-8]"/"[ebp+12]",
    // optionally offset `byteOffset` more bytes in (a wide value's high dword). `ebp` points
    // just above the saved old `ebp`, return address one dword above that; a Param grows
    // upward from there, a Local grows downward into the prologue's `sub esp,N` space, at
    // ir::localOffsetDescendingWide() (a wide value's high dword below its low one).
    std::string addr(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind == ir::SlotKind::Param) {
            return "[ebp+" + std::to_string(slot->frameOffset + byteOffset) + "]";
        }
        return "[ebp-" + std::to_string(ir::localOffsetDescendingWide(*slot, typeWidths()) + byteOffset) + "]";
    }

    // A Local array's elements ascend from its lowest address, element 0, the same
    // direction as a Global array's; this is that address's operand text, `byteOffset`
    // bytes up from it.
    std::string localArrayAddr(const ir::Slot *slot, int byteOffset = 0) const {
        return "[ebp-" + std::to_string(ir::localLowOffset(*slot, typeWidths()) - byteOffset) + "]";
    }

    // Memory-operand text for a plain read/write of `slot`'s value, `byteOffset` bytes in.
    std::string operandOf(const ir::Slot *slot, int byteOffset = 0) const {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? ("[" + lbl + "]") : ("[" + lbl + "+" + std::to_string(byteOffset) + "]");
    }

    std::string allocTemp() {
        return (hostedAbi_ ? "__pcc_tmp_" : "t") + std::to_string(tempCounter_++);
    }
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() {
        return (hostedAbi_ ? "__pcc_local_" : "L") + std::to_string(labelCounter_++);
    }
    // Maps an ir::Stmt::labelId (switch's own Label/Goto - see IR.h) to this
    // backend's own label text, generated the first time either a Label
    // defining it or a Goto referencing it is seen.
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dd v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    bool hostedAbi_ = false;
    bool windowsAbi_ = false;
    // {continue label, break label} per currently-open loop (innermost last).
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace pcc
