#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates ARM assembly (this project's own dialect - see
// ArmAssembler.h) for the IR built by IRBuilder, targeting the
// ARMv7-A "A32" (32-bit ARM, not Thumb) instruction encoding common
// to every A-profile core (Cortex-A5/A7/A8/A9/A15/...) -
// `--target=armv7a`. Like M68kCodegen/MipsCodegen (the closest
// templates here - a real register machine with a real hardware
// stack and real signed compare), `generate()` returns a complete,
// self-contained program loaded at a fixed address (`kBaseAddress`,
// see ArmCodegen.cpp) - no OS, no bootloader, the test harness sets
// the simulated CPU's PC there directly (see tests/run_tests.sh's
// own qemu-system-arm invocation) - halting in a self-branch infinite
// loop, the same "no stack, no recursion" model every backend shares
// even though real function calls (`BL`/`BX`) and a real hardware
// stack (`SP`) are both used.
//
// This language's own `int` is 32 bits here, matching this target's
// native register width (every A32 GPR is 32 bits) - same reasoning
// as every other backend's own word-size choice.
//
// Register assignment (this backend's own convention, not a real
// AAPCS calling-convention choice - arguments travel on the real
// hardware stack, not in registers, same reasoning as every other
// migrated backend - see genFunction()/the Call case in genExpr()):
//   r0  - "ac", genExpr()'s own result (the register every other
//         backend also calls this - 68000's D0, MIPS's $t0)
//   r1  - "the other operand" in a binary op (68000's D1, MIPS's $t1)
//   r2  - holds a computed address when dereferencing one
//         (array/pointer indexing, `*`/`&`) - 68000's A0, MIPS's $t2
//   r3  - scratch, used only inside genDivMod() to hold a quotient/
//         divisor copy so SDIV/MLS never has a source and destination
//         register collide (see its own comment)
//   sp (r13) - the real hardware stack pointer: SUB/ADD-adjusted,
//              LDR/STR through it (via the dedicated `ldind`/`stind`
//              mnemonics below), used both to save one operand across
//              evaluating the other in a binary expression and for
//              every function call/return/argument
//   fp (r11) - the real per-call frame pointer: every function's own
//              prologue points this at its own frame (see
//              genFunction()), so a Local or Param's address is
//              always computed relative to *this* call's own `fp`,
//              never a fixed label - the real-recursion equivalent of
//              every other migrated backend's own `ebp`/`bp` (see
//              loadFrameAddr()'s own comment)
//   lr (r14) - the real hardware link register BL sets and BX reads
//
// ARM has no absolute-addressing memory operand at all (unlike the
// 68000's own "MOVE.L label,D0" - see M68kCodegen.h's own header
// comment) - every load/store goes through a base register plus a
// (here, always zero) offset, so reading or writing any global/local/
// array slot first loads that slot's own fixed address into `r2` via
// `MOVW`+`MOVT` (ArmAssembler.cpp resolves a label operand to its
// lower/upper 16 bits respectively for exactly these two mnemonics -
// same "no %hi/%lo syntax needed" convention as MipsAssembler.h's own
// `lui`/`ori`, see its own header comment), then `ldind`/`stind
// r2,r0` (this backend's own dedicated mnemonics for `LDR`/`STR` with
// a fixed zero offset - same "no bracketed memory operand" convention
// as MIPS/M16C/STM8 - see MipsAssembler.h's own header comment on
// why).
//
// `*`/`/` use the real hardware `SDIV` (ARMv7-A's optional integer-
// divide extension - present on every Cortex-A model qemu offers,
// e.g. `-cpu cortex-a15`, see tests/run_tests.sh's own comment) - a
// full 32-bit signed quotient directly, no HI/LO-register-pair
// unpacking the way MIPS's own `MFLO`/`MFHI` needs. `%` additionally
// uses `MLS` (multiply-subtract, ARMv6T2+) to compute the remainder
// as `dividend - quotient*divisor` in one instruction - see
// genDivMod()'s own comment on why its operands are arranged the way
// they are (avoiding a same-register Rd/Rm multiply, which several
// real ARM cores treat as UNPREDICTABLE).
//
// `<<`/`>>` use the real hardware variable-count shifted-register
// form of `MOV` (this dialect's own `lsl`/`lsr` mnemonics - see
// ArmAssembler.h's own comment) - `>>` is logical/unsigned (`lsr`,
// not the arithmetic `asr`), the same convention every other
// backend's own `>>` already uses (no unsigned type to make the
// distinction with). Multiplying an index by 4 (this target's own
// word size) instead uses the fixed-shift-amount form (`lsl
// rd,rn,#2`) - no extra register needed, unlike the register-count
// form.
//
// Comparisons are where this target is genuinely nicer than every
// earlier one here: `CMP` sets NZCV exactly like the 68000's own
// `CMP`, but instead of needing a real conditional *branch* to
// materialize the 0/1 boolean this language needs (the 68000's own
// `CMP`+`Bcc`+two-branch dance - see M68kCodegen.h's own comment),
// every ARM data-processing instruction (including `MOV`) carries its
// own 4-bit condition field, so "the boolean is 1 iff flags satisfy
// condition X" is just `MOV r0,#0` followed by one *conditionally
// executed* `MOV r0,#1` (`MOVEQ`/`MOVNE`/`MOVLT`/`MOVGE`/`MOVLE`/
// `MOVGT`) - no branch at all, for any of the six operators. See
// genCompare()'s own comment for exactly which condition each
// operator becomes.
class ArmCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "armv7a"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 4, 8}; }
    std::string sourceExtension() const override { return ".sarm"; }
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
    void genExpr(const ir::Expr &e);  // leaves the result in r0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's own width (1/2/4/8 bytes -> 0/1/2/3) -
    // the shift amount genIndexAddr()/genAddrOf()'s Index case scale
    // an index by, instead of the old fixed "always 4 bytes".
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` into r0
    // (or r0:r1, for a `long long` - see isWide()), sign- or zero-
    // extending a narrower-than-native-int source per its own
    // signedness. `addrReg` must not be r0/r1 (both get clobbered).
    void loadOperand(const std::string &addrReg, Type type);
    // Stores r0 (or r0:r1, for a `long long`) into the address in
    // `addrReg`, truncating to `type`'s own width.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends r0 from its own low widthOf(to) bytes, per `to`'s
    // signedness - every Convert between two types no wider than a
    // native int.
    void narrowR0To(Type to);

    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd);
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software 64-bit divide/modulo (no A32 instruction is wide
    // enough) - a classic bit-serial shift/compare/subtract long
    // division, 64 iterations, operating on a small stack-resident
    // scratch block addressed via `r4` (see the .cpp's own comment).
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    // Negates the 64-bit value already in r0:r1, in place.
    void genNegate64();

    // Leaves slot[idxExpr]'s own effective address in `r2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Stores a Local array's own compile-time arrayInit values into
    // its frame slots, one element at a time - the frame-relative
    // equivalent of declareArray() below, which only ever applies to
    // a Global (see genStmt()'s own LocalDecl case).
    void genLocalArrayInit(const ir::Slot *slot);

    // Loads `lbl`'s own address into register `reg` (MOVW+MOVT - see
    // this file's own header comment on why ARM needs two
    // instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads a Local or Param's own frame-relative effective address
    // into `reg` - every function gets a real per-call stack frame
    // (see genFunction()): its own prologue pushes the caller's `fp`
    // and `lr`, then points `fp` at that saved pair, so a Param's
    // address is `fp+8+4*paramIndex` (the caller's own pushed
    // arguments sit just above the two saved words - see the Call
    // case in genExpr()), and a Local's is `fp-4*frameOffset`
    // (reserved by the prologue's own `sub sp,sp,frameSize*4`).
    // `elemIndex` (a Local array's own element index, always a
    // compile-time constant here - genIndexAddr() handles a runtime
    // index itself) adds `elemIndex` further words to a Local's own
    // offset, since a Local array's addresses decrease as its own
    // index increases (frameOffset is its first word's own index -
    // see IR.h's own Slot::frameOffset comment) - the mirror image of
    // a Global array's increasing-address layout. See
    // ../i386/I386Codegen.h's own `addr()` comment for why this makes
    // recursion (direct or indirect) safe: every activation gets its
    // own `fp`/frame, unlike a Global's one real static cell.
    void loadFrameAddr(const std::string &reg, const ir::Slot *slot, int elemIndex = 0);

    // Adds (`isAdd`) or subtracts `offsetBytes` from `reg` in place -
    // used by loadFrameAddr() and genFunction()'s own frame setup.
    // `ADD`/`SUB`'s own immediate operand2 is an unrotated 8-bit
    // field (see ArmAssembler.cpp's own emitDPImm()), so an offset
    // that doesn't fit is instead loaded into a scratch register
    // first (MOVW+MOVT, same two-instruction load every absolute
    // address here already needs) and added/subtracted as a register
    // operand2 - `r1` is this scratch register, or `r3` when `reg`
    // itself is `r1` (loadFrameAddr()'s own Local-pointer-variable
    // case in genIndexAddr() computes a base address into `r1`
    // itself).
    void emitFrameOffset(const std::string &reg, int offsetBytes, bool isAdd);

    // Loads an addressable Local/Param/Global's own effective address
    // into `reg`, dispatching to loadAddress() (a Global's own fixed
    // label) or loadFrameAddr() (a Local/Param's own frame-relative
    // address) as appropriate.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    // Loads one array initializer element's own value (a plain
    // literal, or a referenced Slot's/Function's own address - see
    // IR.h's ArrayInitElem) into register `reg`, as real
    // instructions - genLocalArrayInit()'s own per-element frame
    // store. declareArray()'s own Global trailer text (arrayElemText())
    // is a separate, assembler-time-evaluated form of the same value,
    // since a Global's own storage is static "dw" data, not code.
    void loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e);

    static std::string label(const ir::Slot *slot) {
        // Local/Param addressing is always frame-relative now (see
        // loadFrameAddr()) - only a Global ever needs a name-based
        // label.
        return "g_" + slot->sourceName;
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
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
