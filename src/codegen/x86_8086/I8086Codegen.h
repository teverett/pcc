#pragma once

#include <map>
#include <optional>
#include <sstream>
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
    std::string sourceExtension() const override { return ".asm"; }
    std::vector<std::string> assembledFormats() const override { return {"bin", "coff"}; }
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
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &jumpIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);          // slot[idx] read
    void genAddrOf(const ir::Expr &operand);   // &slot or &slot[idx]

    // Emits code that leaves the 8086 effective-address operand text
    // (e.g. "[g_arr3+bx]") for slot[idxExpr], with bx already loaded -
    // used by both an Index read and an indexed Assign.
    std::string computeElementAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Global/Param slots and every Function get a *name*-based label
    // (never `id`) - same rationale as ../pdp7/Pdp7Codegen.h's own
    // version of this comment: real separate compilation means the
    // file calling a function and the file defining it compile with
    // completely independent `id` sequences, so only a label
    // computable from stable information means the same thing on
    // both sides. A Local never crosses a file boundary, so it keeps
    // the old `sourceName_id` label (still useful for readability,
    // and `id` uniqueness is all that mattered there anyway).
    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp();
    void declareTemp(const std::string &label);
    void declareArray(const std::string &label, const std::vector<long> &values);
    std::string newLabel();

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;  // built by generate() - see Codegen.h's perFileAsm()

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
};

}  // namespace pcc
