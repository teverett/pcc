#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 68000 assembly for the real Motorola/Freescale MC68328
// "DragonBall" (the SoC at the heart of the original Palm Pilot and
// several other mid-1990s PDAs) - this is ../m68k/M68kCodegen.cpp's
// own 68000 backend plus exactly one real difference, explained
// below; every other instruction-selection choice here is identical
// to that file's own (word size, D0/D1/A0 pseudo-register roles, real
// hardware MULS.W/DIVS.W, real hardware LSL.L/LSR.L, real signed
// CMP.L+Bcc, no branch-range workaround needed) - see that file's own
// header comment for the shared rationale, repeated below only where
// this file's own doc comments need the exact wording to match the
// code they're attached to.
//
// The MC68328's own CPU core is a real MC68EC000 - bit-for-bit the
// same user/supervisor instruction set as a plain MC68000 (no MMU,
// no additional opcodes, nothing missing either), wrapped in one
// chip together with a pile of on-chip peripherals this backend never
// touches (a UART, an LCD controller, a real-time clock, a PLL, a
// chip-select unit) - there's nothing for a bare, no-OS compile of
// this project's own language to do with any of them, the same
// "real but inert/unused" stance every other backend takes toward
// hardware features this language has no way to ask for (see
// M68kCodegen.cpp's own comment on `_ion()`/`_iof()` here, below).
//
// The one real difference: unlike ../m68k/M68kCodegen.cpp's own
// `generate()` (which leans on a *simulator* convenience - the test
// harness sets the CPU's PC directly, see that file's own
// `kBaseAddress` comment - specifically so it never has to lay out a
// real reset vector table), this backend emits one, because real
// DragonBall silicon genuinely needs it: chip-select A (CSA0) is
// enabled and based at address 0x00000000 at reset specifically so
// the CPU's own hardware reset exception can fetch its initial SSP
// from vector 0 and initial PC from vector 1 there - the standard
// 68000-family reset mechanism, not a DragonBall-specific quirk, but
// one this backend (unlike its sibling) actually lays out rather than
// sidestepping. `generate()` below writes exactly those two vectors
// (`dl kInitialSP` / `dl <_start's address>`) at address 0, and the
// real reset exception's own PC half of the dance is what gets this
// backend's own code running at all (confirmed directly against
// qemu-system-m68k's own `-M none` board: with no PC-forcing trick at
// all, it correctly fetches PC from vector 1 and starts executing
// right there). `_start` still reloads A7 from the same value
// explicitly anyway, belt-and-suspenders - see generate()'s own
// comment on why: that same board, while it gets PC right, doesn't
// actually load A7 from vector 0 the way real silicon's reset
// exception would, so relying on the vector table alone for the
// stack pointer turned out not to be real-hardware-accurate enough
// to trust blindly even here, let alone portable to every simulator.
class MC68328Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "mc68328"; }
    std::string sourceExtension() const override { return ".s328"; }
    std::vector<std::string> assembledFormats() const override { return {"raw"}; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in D0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective address in `A0`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<long> &values);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dl v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
