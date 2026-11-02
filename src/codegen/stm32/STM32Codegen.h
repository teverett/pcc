#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates Thumb-2 assembly (this project's own narrow dialect, built
// from real ARMv7-M mnemonics - see STM32Assembler.h) for a real
// STM32 Cortex-M3 (the same CPU core qemu's own `stm32vldiscovery`
// board model uses - see tests/run_tests.sh's own check_stm32()). This
// backend is the "rich register machine" case, same spirit as
// M68kCodegen.h/MC68328Codegen.h: almost everything maps onto a real
// instruction directly instead of needing a software workaround.
//
// This language's own `int` is 32 bits here, this target's native
// register width (same reasoning as M68kCodegen.h's own `.L` choice).
// Every value in flight lives in one of three real registers: `r0`
// (genExpr()'s own result - every other backend's own "ac") , `r1`
// ("the other operand" in a binary op - m68k's `d1`), and `r2` (a
// computed address, when dereferencing one - m68k's `a0`); Thumb has
// no separate address-register class, `r2` is just an ordinary
// general register pressed into that role by convention. The real
// hardware stack (`sp`/`r13`) saves one operand across evaluating the
// other in a binary expression (`push`/`pop`, one register at a time -
// see STM32Assembler.h's own comment on why there's no multi-register
// `{reglist}` form here) and every function call/return uses a real
// `bl`/`push {lr}` + `pop {pc}` - genuinely real subroutine linkage,
// not synthesized the way pdp7/6502 have to.
//
// `*` uses the real hardware `muls` (a genuine 32x32->32 multiply,
// unlike m68k's word-sized `MULS.W`). `/`/`%` use the real hardware
// `sdiv` - Cortex-M3 (ARMv7-M) has a real signed integer divide
// instruction, unlike every 8-bit target here and even unlike the
// original 68000 - so unlike every other backend's own multiply/
// divide comment, there's no operand-width or software-loop caveat to
// write here at all. `<<`/`>>` use the real hardware variable-count
// register-form shift (`lsl`/`lsr`) - no loop needed, same as m68k.
// `>>` is logical/unsigned, the same convention every other backend's
// own `>>` already uses (no unsigned type to make the distinction
// with).
//
// Comparisons use real `cmp` + a real condition-code branch, same as
// m68k's `CMP.L`+`Bcc` - this CPU's own flags already encode signed
// less-than/greater-than correctly in hardware, no fixup needed.
// Every conditional/unconditional branch here always uses the 32-bit
// Thumb-2 "wide" encoding (`beq.w`/`b.w`/etc, a real ~1MB range, not
// the 16-bit Thumb-1 form's +/-256 bytes) - so, like m68k, there's no
// branch-range workaround needed anywhere in this backend.
//
// The one real wrinkle a flat-binary 8/16-bit target never has to
// think about: a real STM32's flash and RAM are two genuinely
// different, non-overlapping address ranges (confirmed directly
// against qemu's own `stm32vldiscovery` board model - flash is a real
// read-only ROM region there, not just "unwritten"), so unlike every
// other backend here (one flat blob that's both code and data),
// `generate()` below emits two separate regions: flash (`kFlashBase`,
// where the vector table, this backend's own in-process assembler's
// `org`, and every instruction live - see STM32Assembler.h's own
// `dlt` directive for the one real Cortex-M boot requirement, the
// Thumb bit on the reset vector) and RAM (`kRamBase`, where every
// global/local/temp/array slot actually lives, reserved rather than
// initialized in-place - see STM32Assembler.h's own `ds`/`ramorg`).
// Since RAM starts out unknown (not loaded from the flash image the
// way every other backend's own data lives right there in its one
// flat blob), any global with a real nonzero initial value needs an
// explicit copy, emitted as ordinary flash-resident code that runs
// once before `main` - see generate()'s own comment.
class STM32Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "stm32"; }
    std::string sourceExtension() const override { return ".s32"; }
    std::vector<std::string> assembledFormats() const override { return {"bin"}; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in r0

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

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Emits flash-resident startup code that copies `value` into the
    // RAM address `label(slot)` resolves to - see this file's own
    // header comment on why a global with a real initial value can't
    // just sit there the way every other backend's own flat-blob data
    // does.
    void genGlobalInit(const std::string &ramLabel, long value);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Reserves one RAM word for a scalar slot (see STM32Assembler.h's
    // own `ds`).
    void declareTemp(const std::string &lbl);
    // Reserves `values.size()` RAM words for an array slot, and emits
    // a startup copy (see genGlobalInit()) for every nonzero element.
    void declareArray(const std::string &lbl, const std::vector<long> &values);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;       // flash: vector table + _start + every function body
    std::ostringstream initCode_;   // flash: global-init copies, spliced into _start before `bl main`
    std::vector<std::string> ramTrailer_;  // RAM: "label: ds N" reservation lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
