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

// Generates ARM Thumb assembly (this project's own dialect - see
// ArmMAssembler.h) for the IR built by IRBuilder, targeting the
// ARMv7-M "mainline" M-profile architecture (Cortex-M3/M4/M7 - see
// main.cpp's own makeBackend() comment on the `cortex-m4`/`cortex-m7`
// software-identical aliases this same class answers to, same
// `--target=8088`/`--target=r4000` reasoning as those). Every
// instruction this backend ever emits is real Thumb/Thumb-2 (16-bit
// Thumb-1 ALU/branch/load-store/push-pop ops, plus the handful of
// 32-bit Thumb-2 forms - `MOVW`/`MOVT`, `BL`, the wide `B` this
// backend's own conditional-branch expansion uses - that genuinely
// need Thumb-2's extra width), so it runs unchanged on any real
// ARMv7-M core. Baseline M-profile (Cortex-M0/M0+/M23, ARMv6-M) isn't
// covered: that architecture has neither `MOVW`/`MOVT` (no way here
// to load a 32-bit constant/address without them - a literal pool
// would be the alternative, not implemented) nor hardware `SDIV`
// (moot here anyway - see genDivMod()'s own comment, this backend
// never uses `SDIV` even on cores that have it).
//
// Like M68kCodegen/MipsCodegen (the closest templates - a real
// register machine with a real hardware stack and hardware
// multiply), `generate()` returns a complete, self-contained program
// loaded at a fixed address (`kBaseAddress`, see ArmMCodegen.cpp) -
// no vector table, no reset exception, no boot ROM (the simulator
// sets the CPU's PC there directly, the same "no OS, PC-forced"
// convention 68000/MIPS already use - unlike MC68328Codegen's own
// real-reset-vector-table choice, not needed here since nothing here
// targets real hardware/qemu's own Cortex-M boot sequence), halting
// in a self-branch infinite loop.
//
// This language's own `int` is 32 bits here (this target's native
// register width). Register assignment (this backend's own
// convention, not a real ARM calling convention - see r7/sp/lr below
// for the one piece of this that *is* real ABI-shaped: a genuine
// per-call stack frame):
//   r0 - "ac", genExpr()'s own result (68000's D0, MIPS's $t0) - also
//        where MULS's own 2-operand form naturally wants its
//        destination, and where a real ARM calling convention would
//        put a return value too, so this doubles as a happy
//        coincidence rather than a real ABI decision
//   r1 - "the other operand" in a binary op (68000's D1, MIPS's $t1),
//        and the discard register for every dummy `pop` an
//        epilogue/call-cleanup issues just to move `sp` back (see
//        emitEpilogue()/ExprKind::Call below) - never read afterward
//   r2 - holds a computed address when dereferencing one
//        (array/pointer indexing, `*`/`&`, and every Local/Param
//        frame-relative address - see loadFrameAddr()) - 68000's A0,
//        MIPS's $t2
//   r3 - scratch for a comparison's own materialized boolean,
//        genDivMod()'s own software loop, and loadFrameAddr()'s own
//        offset-immediate scratch
//   r7 - the frame pointer: every function's own prologue points it
//        at this call's own saved-r7 word (see genFunction()) and
//        every Local/Param address is computed relative to it (see
//        loadFrameAddr()) - the one register that gives this backend
//        real recursion, direct or indirect: each nested activation
//        gets its own r7/frame on the real hardware stack, instead of
//        every call sharing one permanent memory cell per Local/Param
//        the way a non-reentrant design would
//   sp - the real hardware stack pointer: real `PUSH`/`POP` (one
//        register at a time - see ArmMAssembler.h's own header
//        comment on why there's no real `{reglist}` syntax here),
//        used to save one operand across evaluating the other in a
//        binary expression, to pass this call's own arguments and
//        reserve this call's own Local frame storage (see
//        genFunction()/ExprKind::Call), and (via the dedicated
//        `pushlr`/`poppc` mnemonics) for every function call/return
//   lr - the real hardware link register `BL` sets; every function
//        here saves it with `pushlr` on entry and returns via
//        `poppc` (`POP {pc}` - a real, single-instruction ARM
//        return idiom: the saved `lr` value loads straight into
//        `pc`), the same "every function saves/restores its own
//        link register" discipline MipsCodegen.cpp's own comment
//        explains (ARM has exactly one link register too - a nested
//        call would clobber it otherwise). `pushlr`/`poppc` are real,
//        ordinary sp-relative pushes/pops, so a nested call's own
//        `pushlr` lands on a fresh stack slot above whatever the
//        caller's own `pushlr` already saved - already reentrant on
//        its own, with no frame-pointer involvement needed.
//
// Every function's own stack frame (see genFunction()): `pushlr`
// saves the return address, `push r7` saves the caller's own frame
// pointer, `mov r7,sp` makes r7 point at that saved-r7 word, and then
// one dummy `push r0` per word of `Function::frameSize` reserves this
// call's own Local storage (the pushed value itself is never read -
// only the resulting `sp` displacement matters). The epilogue
// (emitEpilogue(), run at every `return` and once more as the
// fall-off-the-end default) undoes exactly that in reverse: one dummy
// `pop r1` per reserved word, `pop r7`, `poppc`. A Param's own
// address is `r7 + 8 + 4*paramIndex` (past the saved r7 and the
// return address, into the caller's own pushed-argument words - see
// ExprKind::Call); a Local's is `r7 - 4*frameOffset` (down into this
// call's own just-reserved words, growing the opposite direction - a
// Local array's element i therefore sits at frameOffset+i, i.e.
// address *base-minus* `4*i`, not base-plus - see loadFrameAddr()'s
// own callers for where this matters, e.g. genIndexAddr()). See
// I386Codegen.h's/I8086Codegen.h's own `addr()` comment for the same
// layout one generation lower/higher.
//
// ARM has no absolute-addressing memory operand (same situation as
// MIPS - see MipsCodegen.h's own header comment): every load/store
// goes through a base register plus an immediate displacement, so
// reading or writing any Global/array-literal slot first loads that
// slot's own fixed address into `r2` via `MOVW`+`MOVT` (this
// dialect's own address-loading pair - MipsCodegen.h's `LUI`+`ORI`
// equivalent; ArmMAssembler.cpp resolves a label operand to its
// lower/upper 16 bits respectively for these two mnemonics), then
// `ldind`/`stind r2,r0` (this backend's own dedicated mnemonics for
// `LDR`/`STR` with a fixed zero displacement - same convention as
// MIPS's own `ldind`/`stind`). This dialect's own assembler has no
// `[Rn,#imm]` displacement syntax at all (see ArmMAssembler.cpp's own
// `buildOperand()`), so a Local/Param's own frame-relative address
// (`r7` plus/minus a word count - see this file's own header comment)
// is instead computed directly into a register with `mov`/`adds`/
// `subs` (see loadFrameAddr()), then read/written the same
// `ldind`/`stind r2,r0` way as any other computed address.
//
// `*` uses the real hardware `MULS` (Thumb-1's own 2-operand form,
// universal across every M-profile core including baseline) - a
// full 32x32->32 product, matching this language's own `int` width
// exactly, no operand-size limitation at all (unlike the 68000's own
// `MULS.W` - see M68kCodegen.h's own comment).
//
// `/`/`%` are a from-scratch *software* routine (see genDivMod()'s
// own comment) - not real hardware `SDIV`/`UDIV`: those exist on
// ARMv7-M mainline but not on baseline M-profile, and this backend
// would rather have one `/`/`%` implementation that's correct on
// every M-profile core (same reasoning pdp7/6502 already use for
// their own software multiply/divide, just for a different missing-
// hardware reason - see their own comments) than a hardware
// fast path plus a software fallback. It's a plain repeated-
// subtraction loop on absolute values with the sign reapplied
// afterward - correct for any operands, but slow (proportional to
// the quotient's own magnitude) rather than the handful of cycles a
// real `SDIV` or even a binary restoring-division loop would take;
// acceptable the same way 68000's own 16-bit-divisor limitation is -
// every value in this project's own test programs is small.
//
// `<<`/`>>` use the real hardware register-form shift (Thumb-1's own
// 2-operand `LSLS`/`LSRS Rdn,Rm` - the shift *amount* register) -
// `>>` is logical/unsigned (`LSRS`, not the arithmetic `ASRS`), the
// same convention every other backend's own `>>` already uses.
// Multiplying an index by 4 (this target's own word size) instead
// uses the fixed-count immediate form (`lsli ...,2`), needing no
// extra register.
//
// Comparisons use real `CMP` + a materializing branch, the same
// shape as 68000's own `CMP.L`+`Bcc` (ARM's own condition codes
// already encode signed less-than/greater-than correctly in
// hardware, same as 68000's) - see genCompare()'s own comment.
//
// Every conditional branch this backend ever emits
// (`beq`/`bne`/`blt`/`ble`/`bgt`/`bge`) is, in the real machine code
// ArmMAssembler.cpp produces, actually *two* real Thumb instructions:
// a short inverted-condition `Bcc` (always a fixed, tiny ±2-byte
// displacement - just far enough to skip over the instruction right
// after it) followed by a real wide, unconditional `B` with the
// genuine ±16MB range - see ArmMAssembler.cpp's own
// `encodeConditionalBranch()` comment. This sidesteps ever needing
// the trickier (and, for a from-scratch assembler with no way to
// cross-check against a real toolchain, higher-risk-to-get-wrong)
// 32-bit wide conditional-branch encoding, at the cost of 2 bytes
// per conditional branch versus a real compiler's own best case -
// the same kind of pragmatic, documented tradeoff as the 6502
// backend's own out-of-range-branch workaround (see its own
// `safeBranch()` comment), just applied unconditionally here instead
// of only when actually needed.
class ArmMCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "cortex-m3"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".sarm"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary - a
    // completely different concept from the other five, every one of
    // which means a real relocatable object file for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend, and M68kCodegen.h's own identical override).
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
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software restoring-by-repeated-subtraction divide/remainder -
    // see this file's own header comment on why there's no hardware
    // fast path.
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // The loop itself, shared between genDivMod() and genCompoundOp() -
    // entry convention r1=dividend, r0=divisor, result in r0. See the
    // .cpp's own comment.
    void genDivModCore(bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    void genConvert(const ir::Expr &e);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step -
    // old already in r0, rhs already in r1 (both already at `type`'s
    // own width/signedness, never wide here - IRBuilder.cpp's own
    // rejectIfWide() guarantees it), leaves the raw (not yet re-
    // truncated - see narrowR0To()) new value in r0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign (both its scalar and indexed
    // forms) and CompoundDerefAssign - r2 must already hold the
    // resolved target address on entry (and nothing else live). See
    // the .cpp's own comment.
    void genCompoundThroughR2(const ir::Expr &e);

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    // Only `long long` is wider than this target's native 32-bit int
    // (`long`==`int` here - see typeWidths()) - the one type that
    // ever needs the r0:r1 register-pair convention below.
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's own width (1/2/4/8 bytes -> 0/1/2/3) -
    // the shift amount genIndexAddr() scales a runtime index by,
    // instead of the old fixed "always 4 bytes" `lsli r0,r0,2`.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` into r0
    // (or r0:r1, for a `long long`), sign- or zero-extending a
    // narrower-than-native-int source per its own signedness.
    // `addrReg` must not be r0/r1.
    void loadOperand(const std::string &addrReg, Type type);
    // Stores r0 (or r0:r1, for a `long long`) into the address in
    // `addrReg`, truncating to `type`'s own width.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends r0 from its own low widthOf(to) bytes, per `to`'s
    // signedness, via the shift-left-then-shift-right-back idiom
    // (`lsl`+`asr`/`lsr`) - this dialect has no dedicated sign/zero-
    // extend instruction, same trick ArmCodegen.cpp's own
    // `narrowR0To()` uses.
    void narrowR0To(Type to);

    // `long long` add/sub (lhs OP rhs), evaluated into r0:r1 (lhs)
    // and r4:r5 (rhs, stashed there while lhs's own two words sit on
    // the real stack) - ripples the carry/borrow across the word
    // pair with `adc`/`sbc` (now part of this dialect - see
    // ArmMAssembler.cpp), one native word wider than a plain 32-bit
    // add/sub.
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    // `long long` multiply, keeping the low 64 bits of the true
    // 128-bit product. This dialect has no widening 32x32->64
    // multiply instruction (unlike A32's own `UMULL` - see
    // ArmCodegen.cpp's own `genWideMul()`), so the low*low cross
    // term is built from scratch via `genMul64()`'s own bit-serial
    // shift-and-add loop; the other two cross terms only ever
    // contribute to the result's own high word, so `muls`'s
    // truncating 32x32->32 result is exactly right for them (same
    // reasoning as ArmCodegen.cpp's own comment).
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // 32x32->64 unsigned widening multiply: {r1:r0} = r6 * r4 (the
    // caller must place `a` in r6 and `b` in r4 first - see the
    // .cpp's own comment on why this isn't a by-name parameter).
    // Bit-serial shift-and-add (32 iterations); clobbers r0-r6.
    void genMul64();
    // Software 64-bit divide/modulo (no instruction here is wide
    // enough) - classic bit-serial shift/compare/subtract long
    // division, 64 iterations, same shape as ArmCodegen.cpp's own
    // `genWideDivMod()` just using single-register push/pop instead
    // of sp-relative loads and this dialect's own `blo`/`bhs`
    // unsigned-compare branches.
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    // A `long long` shifted by a runtime count - a counted loop of
    // 1-bit shifts carrying the bit that crosses the word boundary by
    // hand, same spirit as the divide routine above. `isUnsigned`
    // only matters for `>>` (`asr` vs `lsr` on the high word each
    // step); `<<` never cares about signedness.
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in r0:r1, in place.
    void genNegate64();

    // Emits the epilogue every `return` (and the fall-off-the-end
    // default at the bottom of genFunction()) needs: pops this call's
    // own reserved Local words (into the scratch `r1`, discarded -
    // the point is only the `sp` displacement), restores the
    // caller's own `r7`, then `poppc`. See this file's own header
    // comment on the matching prologue in genFunction().
    void emitEpilogue();

    // Loads `lbl`'s own address into register `reg` (MOVW+MOVT - see
    // this file's own header comment on why ARM needs two
    // instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);
    // Loads an arbitrary 32-bit compile-time constant into `reg` -
    // `MOVS` alone if it fits in 8 bits, MOVW+MOVT otherwise.
    void loadImm32(const std::string &reg, uint32_t value);

    // Loads `r7 + bytesFromFp` into `reg` (`bytesFromFp` may be
    // negative - `subs` is used in that case, matching this file's
    // own header comment on which direction each of a Local's/Param's
    // own storage grows from `r7`). Uses `r3` as its own scratch
    // register, never as `reg` itself.
    void loadFrameByteAddr(const std::string &reg, int bytesFromFp);
    // Loads a Local or Param Slot's own effective address into `reg`
    // - both directly from `slot->frameOffset`, already a real byte
    // offset computed by IRBuilder (see IR.h's own comment on
    // Slot::frameOffset): a Param's grows up from `r7` (past the
    // saved `r7`/return address), a Local's grows down. Never called
    // for a Global Slot - see loadAddress()/label() for that case
    // instead.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot);
    // Loads a Slot's own effective address into `reg`, dispatching on
    // `slot->kind` between loadAddress() (Global) and loadFrameAddr()
    // (Local/Param).
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // Loads one ir::ArrayInitElem's own value (a literal, or a
    // symbol's own address - see arrayElemText()'s identical
    // literal/symbol split) into `reg` at runtime - used by
    // genLocalArrayInit() in place of arrayElemText()'s own bare text,
    // which only ever means something inside a `dw` data line.
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e, Type elemType);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

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
