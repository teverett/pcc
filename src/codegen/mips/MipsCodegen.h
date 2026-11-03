#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates MIPS assembly (this project's own dialect - see
// MipsAssembler.h) for the IR built by IRBuilder, targeting the
// MIPS IV instruction set the R10000 implements - and, by the exact
// same `--target=8088`-style software-identical alias main.cpp's own
// makeBackend() uses for the 8088 (see its own comment), also
// `--target=r4000`: this backend only ever emits the real MIPS I
// instruction subset below, nothing R10000-specific at all, so a
// real R4000 (MIPS III, a strict superset of MIPS I) runs the exact
// same generated code unchanged - there's no separate R4000Codegen
// here for the same reason there's no separate I8088Codegen. Every
// real instruction this backend ever emits (ADDU/SUBU/AND/OR/XOR/NOR/
// SLL/SLLV/SRLV/SLT/SLTU/SLTIU/MULT/DIV/MFLO/MFHI/ADDIU/ORI/LUI/LW/
// SW/BEQ/BNE/J/JAL/JR/NOP) has been part of the MIPS instruction set
// since MIPS I, so it runs unchanged on a real R10000 (or any other
// MIPS IV/III/II/I core, including qemu's own `-cpu R4000`, used by
// tests/run_tests.sh --target=r10000 in lieu of a real R10000 model,
// which qemu doesn't ship - see that file's own comment). Like
// M68kCodegen (see its own header comment, the closest template to
// this backend - a real register machine with a real hardware
// stack, hardware multiply/divide/shift, and real signed compare),
// `generate()` returns a complete, self-contained program loaded at
// a fixed address (`kBaseAddress`, see MipsCodegen.cpp) - no OS, no
// bootloader, the simulator sets the CPU's PC there directly -
// halting in a self-branch infinite loop once `main` returns. Every
// function call goes through the real hardware link register
// (`JAL`/`JR ra`) and a real per-call stack frame on the real
// hardware stack pointer (`$sp`), giving every call - direct or
// mutually recursive - its own independent Local/Param storage (see
// this file's own header comment below on the frame layout `$fp`
// establishes).
//
// This language's own `int` is 32 bits here (R10000's own GPRs are
// 64 bits, but every instruction this backend emits is a 32-bit
// MIPS I op - `ADDU`/`SUBU`/etc, not their `-D` 64-bit MIPS III
// counterparts - so only the low 32 bits of any register are ever
// meaningful, matching every other backend's own word-size choice:
// pdp7's 18 bits, 8086/6502's 16 bits, 68000/pdp11's 32 bits).
//
// Register assignment (this backend's own convention):
//   $t0 (8)  - "ac", genExpr()'s own result (the register every other
//              backend also calls this - 68000's D0, 8086's ax)
//   $t1 (9)  - "the other operand" in a binary op (68000's D1)
//   $t2 (10) - holds a computed address when dereferencing one
//              (array/pointer indexing, `*`/`&`, and every Local/Param
//              frame-relative access - see loadSlotAddr())
//   $at (1)  - scratch for a comparison's own intermediate SLT, and
//              for the caller's own saved frame pointer while a
//              function's epilogue restores it (see genEpilogue())
//   $sp (29) - the real hardware stack pointer: ADDIU-adjusted,
//              LW/SW through it, used both to save one operand
//              across evaluating the other in a binary expression
//              and for every function call/return/argument
//   $ra (31) - the real hardware link register JAL sets and JR reads
//   $fp (30) - this call's own frame pointer (the real MIPS o32 ABI's
//              own $s8/$fp register, same role): every function's
//              prologue points it at its own freshly pushed saved-fp
//              word (see genFunction()), giving every call its own
//              private Local/Param storage, real recursion included -
//              same role I386Codegen.h's own `ebp` plays, just with
//              MIPS's own base+ADDIU-computed-offset addressing (no
//              bracketed memory operand exists in this dialect - see
//              MipsAssembler.h's own header comment) standing in for
//              a real `[ebp+N]` operand.
//
// A function's own frame, relative to `$fp` (growing in the same
// direction every migrated backend's own stack frame does - downward
// as the frame gets bigger): `fp+0` is the caller's own saved `$fp`,
// `fp+4` the saved `$ra`; the caller's own pushed arguments sit above
// that, param `i` at `fp+8+4*i` (closest first, same cdecl-style
// right-to-left push/caller-cleanup convention as I386Codegen.h's own
// `addr()` - see ir::ExprKind::Call's own genExpr() case). Locals sit
// below `fp`, reserved by the prologue's own `ADDIU sp,sp,-N`; a Local
// at Slot::frameOffset K lives at `fp-4*K` - see loadSlotAddr() and
// genIndexAddr()'s own comment on why a Local array's element
// addresses count down, not up, as the element index increases.
//
// MIPS has no absolute-addressing memory operand at all (unlike the
// 68000's own "MOVE.L label,D0" - see M68kCodegen.h's own header
// comment) - every load/store goes through a base register plus a
// 16-bit displacement, so reading or writing any global/local/array
// slot first loads that slot's own fixed address into `$t2` via
// `LUI`+`ORI` (MipsAssembler.cpp resolves a label operand to its
// upper/lower 16 bits respectively for exactly these two mnemonics -
// see its own header comment), then `ldind`/`stind $t2,$t0` (this
// backend's own dedicated mnemonics for `LW`/`SW` with a fixed zero
// displacement - same "no bracketed memory operand" convention as
// M16C/STM8 - see M16cAssembler.h's own header comment on why).
//
// `*`/`/` use the real hardware `MULT`/`DIV` (`MFLO`/`MFHI` pull the
// 32-bit product/quotient-and-remainder back out of the HI/LO pair
// they leave it in) - no 16-bit-operand limitation the way the
// 68000's own `MULS.W`/`DIVS.W` has (see M68kCodegen.h's own header
// comment): both take two full 32-bit operands directly.
//
// `<<`/`>>` use the real hardware variable-count register-form shift
// (`SLLV`/`SRLV`) - `>>` is logical/unsigned (`SRLV`, not the
// arithmetic `SRAV`), the same convention every other backend's own
// `>>` already uses (no unsigned type to make the distinction with).
// Multiplying an index by 4 (this target's own word size) uses the
// fixed-count form (`SLL ...,2`) instead, needing no extra register.
//
// Comparisons use the real hardware `SLT`/`SLTU`/`SLTIU` directly -
// each one *is* the boolean this language needs (1 or 0), no branch
// at all needed for `<`/`>`/`<=`/`>=`/`==`/`!=` (simpler than both
// the 68000's own CMP+Bcc and the 6502's N-xor-V fixup - see their
// own comments) - see genCompare()'s own comment for exactly which
// real instruction(s) each operator becomes.
//
// Every control-transfer instruction here (`J`/`JAL`/`JR`/`BEQ`/
// `BNE`) has a real hardware branch-delay slot - the instruction
// right after it always executes before the transfer takes effect,
// a genuine MIPS pipeline quirk every other backend's own target
// simply doesn't have. This backend never tries to usefully fill
// one: every branch/jump helper here always emits a `NOP` right
// after it (see emitBranch()/emitJump()), trivially correct (if not
// performance-optimal) the same way a real compiler's `-O0` often
// doesn't bother filling delay slots either.
class MipsCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "r10000"; }
    std::string sourceExtension() const override { return ".smips"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other five, every one of
    // which means a real relocatable object file for separate
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
    void genExpr(const ir::Expr &e);  // leaves the result in $t0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots

    // Leaves slot[idxExpr]'s own effective address in `$t2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into register `reg` (LUI+ORI - see
    // this file's own header comment on why MIPS needs two
    // instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads the address of a plain (non-array) Global/Local/Param
    // slot's own storage into `reg` - a Global's fixed label address
    // (see loadAddress()), or a Local/Param's own `$fp`-relative
    // address, computed in one ADDIU (see this file's own header
    // comment on the frame layout `$fp` establishes).
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    // Emits a 32-bit literal load into `reg` - ADDIU when it fits in
    // 16 bits (sign-extended), LUI+ORI otherwise. Shared by IntLit and
    // genLocalArrayInit()'s own literal array elements.
    void loadImm32(const std::string &reg, int32_t v);

    // The shared function-exit sequence (deallocate this call's own
    // locals, restore the caller's `$fp`/`$ra`, `JR ra`) - used by
    // both genFunction()'s own fall-through epilogue and every
    // explicit `return` (StmtKind::Return is its own early exit,
    // bypassing the fall-through epilogue entirely, so it needs this
    // same sequence inline).
    void genEpilogue();

    // A Global's own name-based label (needed identically across any
    // file that references it, under real separate compilation - see
    // IR.h's own `ownerFunctionName`/`frameOffset` comments) or a
    // Function's entry label. A Local/Param never gets one - every
    // reference to either goes through loadSlotAddr()'s own
    // `$fp`-relative addressing instead (see this file's own header
    // comment).
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last) - {continue
    // label, break label} - pushed/popped by genStmt's own While
    // case, consulted by its Break/Continue cases.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
