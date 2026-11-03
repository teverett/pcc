#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
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
// Every value in flight lives in one of four real registers: `r0`
// (genExpr()'s own result - every other backend's own "ac") , `r1`
// ("the other operand" in a binary op - m68k's `d1`), `r2` (a computed
// address, when dereferencing one - m68k's `a0`), and `r3` (a scratch
// register for building a frame-relative offset - see
// loadFrameWordAddr()); Thumb has no separate address-register class,
// `r2` is just an ordinary general register pressed into that role by
// convention. `r7` is this backend's own dedicated frame pointer (see
// genFunction() below) - never used for anything else. The real
// hardware stack (`sp`/`r13`) saves one operand across evaluating the
// other in a binary expression (`push`/`pop`, one register at a time -
// see STM32Assembler.h's own comment on why there's no multi-register
// `{reglist}` form here), passes each call's own arguments and
// reserves each call's own Local frame storage (see genFunction()/
// `ExprKind::Call`), and every function call/return uses a real
// `bl`/`push {lr}` + `pop {pc}` - genuinely real subroutine linkage,
// not synthesized the way pdp7/6502 have to.
//
// Every function's own stack frame (see genFunction()): `pushlr`
// saves the return address, `push r7` saves the caller's own frame
// pointer, `mov r7,sp` makes r7 point at that saved-r7 word, and then
// one dummy `push r0` per word of `Function::frameSize` reserves this
// call's own Local storage (the pushed value itself is never read -
// only the resulting `sp` displacement matters, since this dialect has
// no `sub sp,#imm` of its own - see STM32Assembler.h's own header
// comment). emitEpilogue() (run at every `return` and once more as the
// fall-off-the-end default) undoes exactly that in reverse: one dummy
// `pop r1` per reserved word, `pop r7`, `poplr`. A Param's own address
// is `r7 + 8 + 4*paramIndex` (past the saved r7 and the return
// address, into the caller's own pushed-argument words - see
// `ExprKind::Call`); a Local's is `r7 - 4*frameOffset` (down into this
// call's own just-reserved words, growing the opposite direction - a
// Local array's element i therefore sits at frameOffset+i, i.e.
// address *base-minus* `4*i`, not base-plus - see loadFrameAddr()'s
// own callers for where this matters, e.g. genIndexAddr()). See
// ../i386/I386Codegen.h's own `addr()` comment for the same layout on
// a different register machine, and ../armm/ArmMCodegen.h's own
// identical header comment for this exact same frame-pointer approach
// on a sibling bare Cortex-M dialect.
//
// Each call's own arguments are pushed onto the real stack by the
// caller, last-declared one first, so the first argument ends up
// closest to the return address `bl` is about to set `lr` for -
// matching the address every Param already addresses itself by
// (paramIndex 0 is `r7+8`, the lowest positive offset). Each callee
// lays them into ITS OWN frame itself; the caller cleans the pushed
// words back off afterward with one dummy `pop r1` per argument (same
// "no `add sp,#imm` in this dialect" reasoning as the Local-frame
// reserve/release above).
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
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary (a real
    // Cortex-M image, vector table included) - a completely different
    // concept from the other five, every one of which means a real
    // relocatable object file for separate compilation (see
    // Codegen.h's own header comment on why this is asked per-backend).
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Emits flash-resident startup code that copies `value` into the
    // RAM address `label(slot)` resolves to - see this file's own
    // header comment on why a global with a real initial value can't
    // just sit there the way every other backend's own flat-blob data
    // does.
    void genGlobalInit(const std::string &ramLabel, long value);

    // Emits the epilogue every `return` (and the fall-off-the-end
    // default at the bottom of genFunction()) needs: pops this call's
    // own reserved Local words (into the scratch `r1`, discarded -
    // the point is only the `sp` displacement), restores the
    // caller's own `r7`, then `poplr`. See genFunction()'s own
    // comment for the matching prologue.
    void emitEpilogue();

    // Loads `r7 + 4*wordsFromFp` into `reg` (`wordsFromFp` may be
    // negative, using `sub` in that case - see genFunction()'s own
    // comment on which direction each of a Local's/Param's own
    // storage grows from `r7`). Uses `r3` as its own scratch register,
    // never as `reg` itself.
    void loadFrameWordAddr(const std::string &reg, int wordsFromFp);
    // Loads a Local or Param Slot's own effective address into `reg` -
    // a Param's is `r7 + 8 + 4*paramIndex` (past the saved `r7` and
    // the return address, into the caller's own pushed-argument
    // words); a Local's is `r7 - 4*frameOffset`. Never called for a
    // Global Slot - see genAddrOf()/the `lea` call sites instead.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot);
    // Loads a Slot's own effective address into `reg`, dispatching on
    // `slot->kind` between a plain `lea` (Global) and loadFrameAddr()
    // (Local/Param).
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // Loads one ir::ArrayInitElem's own value (a literal via `movl`,
    // or a symbol's own address via `lea`, including the Function
    // case's own thumb-bit adjustment) into `reg` at runtime - used by
    // genLocalArrayInit() in place of genArrayElemInit()'s own direct
    // store, which only ever targets a Global's static RAM word.
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Reserves one RAM word for a Global scalar slot (see
    // STM32Assembler.h's own `ds`). A Local/Param never calls this -
    // its own storage lives in this call's own stack frame instead
    // (see genFunction()'s own comment).
    void declareTemp(const std::string &lbl);
    // Reserves `values.size()` RAM words for a Global array slot, and
    // emits a startup copy (see genGlobalInit()) for every nonzero
    // element. A Local array never calls this - see genLocalArrayInit().
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    // Flash-resident code that writes one array element into the RAM
    // word at `lbl+offset` - a literal's own value loaded via `movl`
    // (skipped when it's 0, since RAM already starts zeroed), or a
    // symbol element's address loaded the same way genAddrOf() loads
    // &slot/&fn, including that function case's own thumb-bit
    // adjustment. Only ever used for a Global array's own static
    // RAM-resident initializer - see genLocalArrayInit() for a Local
    // array's own per-call equivalent.
    void genArrayElemInit(const std::string &lbl, size_t offset, const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;       // flash: vector table + _start + every function body
    std::ostringstream initCode_;   // flash: global-init copies, spliced into _start before `bl main`
    std::vector<std::string> ramTrailer_;  // RAM: "label: ds N" reservation lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // The function genFunction() is currently emitting's own
    // Function::frameSize - set once per function, read by
    // emitEpilogue() at every `return` inside it.
    int frameSize_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
