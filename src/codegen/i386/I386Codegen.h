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
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots

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
    // "[ebp-8]"/"[ebp+12]" - every function gets a real per-call stack
    // frame (see genFunction()): `push ebp`/`mov ebp,esp` makes `ebp`
    // point just above this call's own saved old `ebp`, with the
    // return address one dword above that, and the caller's pushed
    // arguments above the return address in declaration order (param
    // 0 closest to it) - so a Param's address is `ebp+8+4*paramIndex`.
    // Locals live below `ebp`, reserved by the prologue's `sub esp,N`;
    // Slot::frameOffset is already a 1-based sequential dword index in
    // declaration order, so a Local's address is `ebp-4*frameOffset`.
    // See I8086Codegen.h's own identical `addr()` comment for why this
    // makes recursion (direct or indirect) safe: every activation gets
    // its own ebp/frame, unlike a Global's one real static cell.
    static std::string addr(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Param) {
            return "[ebp+" + std::to_string(8 + 4 * slot->paramIndex) + "]";
        }
        return "[ebp-" + std::to_string(4 * slot->frameOffset) + "]";
    }

    // The memory-operand text for a plain read/write of `slot`'s own
    // value - see I8086Codegen.h's own identical `operandOf()`.
    static std::string operandOf(const ir::Slot *slot) {
        return slot->kind == ir::SlotKind::Global ? ("[" + label(slot) + "]") : addr(slot);
    }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
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
