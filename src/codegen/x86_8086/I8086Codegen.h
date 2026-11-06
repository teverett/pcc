#pragma once

#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 16-bit real-mode 8086 assembly (NASM syntax) for the IR
// built by IRBuilder (see IR.h: every name is already resolved to a
// Slot/Function by the time this runs, so this class is pure
// instruction selection - no symbol table of its own). See
// ../../../README.md for the shared language.
//
// Unlike the pdp7 backend, this one can lean on the 8086's own
// instructions directly instead of building workarounds: `mov ax,
// <imm>` loads a literal with no data-word indirection, `imul`/`idiv`
// are real hardware multiply/divide (no software __divmod routine),
// `cmp`+conditional-jump replaces the sza/sma/sad dance, and register-
// indexed addressing (`[label+bx]`) replaces pdp7's law+indirection
// trick for arrays and pointers. Same language, same "every variable
// is one word, no stack, no recursion" model (see README's "Scope
// and limitations") - just a much more direct mapping onto the target.
//
// Every variable/temporary is one 16-bit word. Unlike pdp7as, NASM
// has no label-length limit, so labels here are derived from each
// Slot/Function's source name plus its (globally unique) id, rather
// than synthetic the way pdp7's are - purely a readability choice,
// not a correctness requirement.
//
// generate() returns a complete, self-contained NASM source: a tiny
// boot-sector wrapper (zeroes the segment registers, sets up a stack
// at `0x7C00` growing down, calls `main`, halts) followed by every
// function body and then a data section, padded and signed as a
// 512-byte boot sector so it can be booted directly (e.g.
// `qemu-system-i386 -fda out.bin`). `pcc` itself doesn't build
// anything bigger than one boot sector - a program that doesn't fit
// (confirmed directly: `../../../src-c/sys/s4.c`'s real 8086 disk
// I/O, once ported, is already well past 510 bytes on its own) needs
// `--format=coff` instead (an ordinary object file, no boot-sector
// size limit at all) and a separate tool to link/package it into a
// real bootable image - that packaging is deliberately not this
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

    void emitPlacement(long address);  // '@' addr - see .cpp's own comment above generate()
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step -
    // `old` already in ax, `rhs` already in bx (both already at
    // `type`'s own width/signedness), leaves the raw (not yet
    // re-truncated - see narrowAxTo()) new value in ax. `type` is
    // never wide here - IRBuilder.cpp's own rejectIfWide() guarantees
    // it.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and
    // CompoundDerefAssign - bx must already hold the resolved address
    // on entry (and nothing else live). See the .cpp's own comment.
    void genCompoundThroughBx(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's own width (1/2/4/8 bytes -> 0/1/2/3) -
    // the shift amount computeElementAddr()/genAddrOf()'s Index case
    // scale an index by, instead of the old fixed "always 2 bytes".
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the effective-address text `mem`
    // into AX, sign- or zero-extending a narrower-than-native-int
    // source (`char`) per its own signedness; for a wide type
    // (`long`/`long long`), leaves the ADDRESS of its own storage in
    // AX instead (see genWideSlotAddr()'s own comment) - every wide
    // operation here works from an address, never a value directly,
    // since a 16-bit register can't hold more than one native word.
    void loadOperand(const std::string &mem, Type type);
    // Stores AX into `mem`, truncating to `type`'s own width (the low
    // byte only, for a `char`) - never called for a wide type (an
    // Assign to one goes through copyWideBlock() instead, copying
    // from an address rather than moving a single register).
    void storeOperand(const std::string &mem, Type type);
    // Re-extends AX from its own low byte, per `to`'s signedness -
    // every Convert between two types no wider than a native int
    // (see genConvert()).
    void narrowAxTo(Type to);
    void genConvert(const ir::Expr &e);

    // Leaves the address of `slot`'s own N-word (N=widthOfType(type)
    // /2) storage in AX, words stored low-to-high as address
    // increases (word 0 = least significant) - see the .cpp's own
    // comment on this method for why a Local's own case has to
    // reverse direction.
    void genWideSlotAddr(const ir::Slot *slot);
    // Builds a fresh `toN`-word low-to-high block on the real stack
    // from the `fromN`-word low-to-high block at the address already
    // in BX, sign- or zero-extending any extra words per
    // `isUnsignedExt` when widening (ignored when narrowing/equal).
    // Leaves the new block's own address in AX. Used by both a wide
    // Assign/LocalDecl-init (fromN==toN, BX already copy-source) and
    // a wide Convert.
    void copyWideBlock(int fromN, int toN, bool isUnsignedExt);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // Emits the N-word bit-serial divide subroutine (and its own
    // fixed-global scratch block) into `trailer_`, once per N
    // actually used in this program - genWideDivMod() calls into it
    // instead of inlining the whole routine at every division site,
    // since inlining it even once already overflows this backend's
    // own 512-byte boot-sector budget (--format=raw) for a `long` or
    // `long long` divide. A no-op on every call after the first for a
    // given N.
    void emitWideDivModSubroutine(int n);
    std::set<int> wideDivModEmitted_;
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the N-word value whose address is in AX, in place.
    void genWideNegate(int n);
    // Negates the N words at scratch offset `wordOffset` (bx already
    // the scratch base), unconditionally (genWideNegateAt) or only if
    // the flag word at `flagOffset` is non-zero (genWideNegateIf) -
    // see genWideDivMod()'s own comment.
    void genWideNegateAt(int wordOffset, int n);
    void genWideNegateIf(int wordOffset, int n, int flagOffset);
    // Reduces the N-word value whose ADDRESS is already in AX down to
    // a single word in AX that's zero iff every word was - see
    // genLogicalAnd()'s own comment on why a plain "cmp ax,0" can't
    // be used directly on a wide operand.
    void genWideTruthify(Type t);

    // Emits code that leaves the 8086 effective-address operand text
    // (e.g. "[g_arr3+bx]") for slot[idxExpr], with bx already loaded -
    // used by both an Index read and an indexed Assign.
    std::string computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Global slots and every Function get a *name*-based label (never
    // `id`) - real separate compilation means the file calling a
    // function and the file defining it compile with completely
    // independent `id` sequences, so only a label computable from
    // stable information means the same thing on both sides. A Local
    // or Param is addressed relative to `bp` instead (see
    // addr(const ir::Slot*) below) - it never needs a label at all,
    // since it never crosses a file boundary and every reference to
    // it is already inside the one function that owns it.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's own effective-address operand text, e.g.
    // "[bp-4]"/"[bp+6]" - every function gets a real per-call stack
    // frame (see genFunction()): `push bp` / `mov bp,sp` makes `bp`
    // point just above this call's own saved old `bp`, with the
    // return address one word above that, and the caller's pushed
    // arguments above the return address in declaration order (param
    // 0 closest to it) - so a Param's address is `bp+frameOffset`.
    // Locals live below `bp`, reserved by the prologue's `sub sp,N`;
    // `Slot::frameOffset` is already IRBuilder's own real byte offset
    // for both a Param and a Local (see IR.h's own comment on
    // Slot::frameOffset), so a Local's address is `bp-frameOffset`.
    // Every activation gets its own bp/frame, so two overlapping
    // calls to the same function (recursion, direct or indirect)
    // never share storage - unlike a Global, which still gets the one
    // real static cell real C statics/globals are supposed to have.
    static std::string addr(const ir::Slot *slot, int byteOffset = 0) {
        if (slot->kind == ir::SlotKind::Param) {
            return "[bp+" + std::to_string(slot->frameOffset + byteOffset) + "]";
        }
        return "[bp-" + std::to_string(slot->frameOffset + byteOffset) + "]";
    }

    // The memory-operand text for a plain read/write of `slot`'s own
    // value, `byteOffset` bytes into it - "[g_x]"/"[g_x+N]" for a
    // Global, or addr(slot, byteOffset) ("[bp±N]") for a Local/Param -
    // used anywhere the two only differ in which of those this is (an
    // ordinary Var read/Assign write; see genAddrOf/computeElementAddr
    // for the two places that still have to tell them apart, since
    // only a Global's own address folds into a compile-time immediate
    // the way `mov ax, g_x` does).
    static std::string operandOf(const ir::Slot *slot, int byteOffset = 0) {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? ("[" + lbl + "]") : ("[" + lbl + "+" + std::to_string(byteOffset) + "]");
    }

    std::string allocTemp();
    void declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel();

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;  // built by generate() - see Codegen.h's perFileAsm()

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
