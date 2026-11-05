#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates real 32-bit protected-mode i386 assembly (this project's
// own dialect, built from real i386 mnemonics - see I386Assembler.h)
// for the IR built by IRBuilder. This is ../x86_8086/I8086Codegen.h's
// own backend grown up one generation: same register-centric model
// (`eax` is every other backend's "ac", `ebx` the "other operand" in
// a binary op *and* the one indirect-addressing register, exactly
// I8086Codegen.h's own `ax`/`bx` roles - see that file's own header
// comment), same real hardware `imul`/`idiv`, `cmp`+conditional-jump,
// and `[label]`/`[ebx]`/`[label+ebx]` addressing - just 32 bits wide
// throughout instead of 16, and a real flat 4GB address space instead
// of 8086 real-mode segmentation.
//
// This language's own `int` is 32 bits here (this target's native
// register width, same reasoning as M68kCodegen.h's own `.L` choice).
//
// Getting from power-on into 32-bit protected mode at all - real
// i386 hardware always starts a boot sector in 16-bit real mode - is
// deliberately *not* this backend's job: `generate()` below emits a
// real Multiboot header (see the Multiboot Specification's "OS image
// format", and that function's own comment) instead of any boot-
// sector/real-mode code of its own. A real bootloader (GRUB, or
// here, `qemu-system-i386 -kernel` acting as one - see
// tests/run_tests.sh's own check_i386()) reads that header and does
// the real-mode-to-protected-mode transition itself, entirely before
// this image's own first instruction ever runs - confirmed directly
// against a real `qemu-system-i386` boot (reading `EAX`/`CS` over the
// monitor) rather than assumed from the spec alone. So this backend's
// own entry sequence only ever has to do two real things once control
// actually reaches it: mask both real 8259 PICs (see generate()'s own
// comment on why - this language's `_ion()` needs it, not booting
// itself) and call `main`. Real separate compilation
// (`--format=coff`/`elf`/`macho`/`aout`/`objraw`, see `objectMode`
// below) builds one format-neutral ObjectFile per compile (see
// I386Assembler.h's own `assembleI386Object`) instead of this one
// self-contained Multiboot image - I386LinkerFactory.cpp is solely
// responsible for synthesizing the one real Multiboot-header-bearing
// entry object, once every user object is merged.
class I386Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "i386"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".s386"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat Multiboot image -
    // a completely different concept from the other five, every one
    // of which means a real relocatable object file for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend).
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
    // Software 64-bit divide/modulo (no 32-bit x86 hardware
    // instruction is wide enough) - a classic bit-serial shift/
    // compare/subtract long division, 64 iterations, operating on a
    // small stack-resident scratch block addressed via `ebx` (see the
    // .cpp's own comment on this method for the full layout and the
    // signed case's sign handling). Leaves the result in edx:eax.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in edx:eax, in place.
    void genNegate64();
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's own width (1/2/4/8 bytes -> 0/1/2/3) -
    // the shift amount computeElementAddr()/genAddrOf()'s Index case
    // scale an index by, instead of the old fixed "always 4 bytes".
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the effective-address text `mem`
    // (no trailing size prefix - e.g. "[g_x]"/"[ebp-8]"/"[ebx]") into
    // eax (or edx:eax, for a `long long` - see isWide()), sign- or
    // zero-extending a narrower-than-native-int source per its own
    // signedness. `offsetMem` (defaulting to a plain copy of `mem`'s
    // own shape with +4 added to its own fixed displacement) gives
    // the high dword's own operand text for that case.
    void loadOperand(const std::string &mem, const std::string &highMem, Type type);
    // Stores eax (or edx:eax, for a `long long`) into `mem`/`highMem`,
    // truncating to `type`'s own width (the low N bytes only - never
    // touches whatever else shares that memory, e.g. an adjacent
    // array element or local).
    void storeOperand(const std::string &mem, const std::string &highMem, Type type);
    // Re-extends eax from its own low widthOf(to) bytes, per `to`'s
    // signedness - every Convert between two types no wider than a
    // native int (see genConvert()).
    void narrowEaxTo(Type to);

    // Emits the i386 effective-address operand text (e.g.
    // "[g_arr3+ebx]") for slot[idxExpr], with ebx already loaded -
    // used by both an Index read and an indexed Assign. See
    // I8086Codegen.h's own `computeElementAddr` - identical role,
    // 32-bit ("dd"-sized elements, `ebx`) throughout.
    std::string computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Global slots and every Function get a name-based label; a Local
    // or Param is addressed relative to `ebp` instead (see addr()
    // below) - see I8086Codegen.h's own identical comment, same
    // reasoning (real separate compilation means only name-derived
    // labels mean the same thing across a file boundary).
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's own effective-address operand text, e.g.
    // "[ebp-8]"/"[ebp+12]", optionally offset by `byteOffset` more
    // bytes into the value (used for a `long long`'s own high dword,
    // 4 bytes further in - see loadOperand()/storeOperand()). Every
    // function gets a real per-call stack frame (see genFunction()):
    // `push ebp`/`mov ebp,esp` makes `ebp` point just above this
    // call's own saved old `ebp`, with the return address one dword
    // above that. `Slot::frameOffset` is already IRBuilder's own
    // real byte offset from that point - a Param's grows upward
    // (`ebp+frameOffset`, past IRBuilder's own two-native-int
    // reservation for the saved `ebp`/return address), a Local's
    // grows downward (`ebp-frameOffset`, reserved by the prologue's
    // `sub esp,N`) - see IR.h's own comment on Slot::frameOffset for
    // why this already accounts for each slot's own type's width and
    // alignment, with no backend-side multiplication needed any more.
    // See I8086Codegen.h's own identical `addr()` comment for why
    // this makes recursion (direct or indirect) safe: every
    // activation gets its own ebp/frame, unlike a Global's one real
    // static cell.
    static std::string addr(const ir::Slot *slot, int byteOffset = 0) {
        if (slot->kind == ir::SlotKind::Param) {
            return "[ebp+" + std::to_string(slot->frameOffset + byteOffset) + "]";
        }
        return "[ebp-" + std::to_string(slot->frameOffset + byteOffset) + "]";
    }

    // The memory-operand text for a plain read/write of `slot`'s own
    // value, `byteOffset` bytes into it - see I8086Codegen.h's own
    // identical `operandOf()`.
    static std::string operandOf(const ir::Slot *slot, int byteOffset = 0) {
        if (slot->kind != ir::SlotKind::Global) return addr(slot, byteOffset);
        std::string lbl = label(slot);
        return byteOffset == 0 ? ("[" + lbl + "]") : ("[" + lbl + "+" + std::to_string(byteOffset) + "]");
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dd v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
