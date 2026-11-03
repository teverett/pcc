#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PowerPC assembly (this project's own dialect - see
// PowerPCAssembler.h) for the IR built by IRBuilder, targeting the
// classic 32-bit PowerPC instruction set (the one every G3/G4/G5 and
// embedded PowerPC core shares) - like MipsCodegen (the closest
// template to this backend - another real register machine with a
// real hardware stack and a single hardware link register, see its
// own header comment), `generate()` returns a complete, self-
// contained program loaded at a fixed address (`kBaseAddress`, see
// PowerPCCodegen.cpp) - no OS, no bootloader, the simulator sets the
// CPU's PC there directly - halting in a self-branch infinite loop,
// the same "no stack, no recursion" model every backend shares even
// though real function calls (`bl`/`blr`) and a real hardware stack
// (`r1`) are both used.
//
// This language's own `int` is 32 bits here, matching this target's
// native GPR width (classic PowerPC - not the 64-bit extension),
// same word-size reasoning as every other backend.
//
// Register assignment (this backend's own convention, not a real
// calling-convention choice, since there's no cross-function
// register-passed-argument convention at all - every parameter is a
// statically allocated slot, same as every other backend - see
// genFunction()):
//   r3 ("t0") - "ac", genExpr()'s own result (the register every
//               other backend also calls this - 68000's D0, MIPS's
//               $t0) - also the real PowerPC ABI's own return-value
//               register, though that convention is otherwise unused
//               here.
//   r4 ("t1") - "the other operand" in a binary op (68000's D1)
//   r5 ("t2") - holds a computed address when dereferencing one
//               (array/pointer indexing, `*`/`&`) - 68000's A0
//   r1 ("sp") - the real hardware stack pointer: addi-adjusted,
//               lwz/stw through it, used both to save one operand
//               across evaluating the other in a binary expression
//               and for every function call/return
//   r0        - scratch, used only to shuttle the real hardware link
//               register through `mflr`/`mtlr` around a call (PowerPC
//               has no direct memory-access form for a special
//               register the way MIPS's `$ra` is itself a plain GPR -
//               see genFunction()'s own comment)
//   r31 ("fp") - this backend's own frame pointer: set once per call,
//               in genFunction()'s own prologue, to the address of
//               that call's saved-old-`fp` word, and held fixed for
//               the rest of that activation's own body even while
//               `sp` itself moves transiently (every binary-operator
//               temp push/pop above uses `sp`, not `fp`) - every
//               Local/Param access addresses relative to `fp` (see
//               slotDisp()) instead of a single shared memory cell,
//               which is what makes direct and mutual recursion safe:
//               each nested call gets its own `fp`/frame, chained
//               through the stack exactly like `sp` is left running
//               for genFunction()'s own `old fp`/saved-LR pair to
//               restore at return.
//
// PowerPC has no absolute-addressing memory operand at all (same
// restriction as MIPS - see MipsCodegen.h's own header comment) -
// every load/store goes through a base register plus a displacement,
// so reading or writing any global/local/array slot first loads that
// slot's own fixed address into `t2` via `lis`+`ori`
// (PowerPCAssembler.cpp resolves a label operand to its upper/lower
// 16 bits respectively for exactly these two mnemonics - see its own
// header comment), then `ldind`/`stind t2,t0` (this backend's own
// dedicated mnemonics for `lwz`/`stw` with a fixed zero displacement -
// same "no bracketed memory operand" convention as MIPS/M16C/STM8).
//
// `*`/`/` use the real hardware `mullw`/`divw` - both take two full
// 32-bit operands directly and leave a full 32-bit result, no
// unpacking needed (unlike the 68000's own 16-bit-operand `MULS.W`/
// `DIVS.W` - see M68kCodegen.h's own header comment). `%` has no
// direct hardware remainder instruction on classic PowerPC, so it's
// synthesized the standard way: `divw` for the quotient, `mullw` to
// multiply back out, `subf` to recover the remainder from the
// original dividend (see genDivMod()'s own comment).
//
// `<<`/`>>` use the real hardware variable-count register-form shift
// (`slw`/`srw`) - `>>` is logical/unsigned (`srw`, not the arithmetic
// `sraw`), the same convention every other backend's own `>>`
// already uses (no unsigned type to make the distinction with).
//
// Comparisons use the real hardware `cmpw` plus a conditional branch
// (`beq`/`bne`/`blt`/`bgt`/`ble`/`bge`, each a `bc` encoding against
// the condition register PowerPC's `cmpw` sets - see
// PowerPCAssembler.cpp's own encodeInstr()) to materialize the 0/1
// boolean this language needs - the same "compare, then branch to
// decide the boolean" shape as the 68000's own CMP+Bcc (see
// M68kCodegen.h's own header comment), since PowerPC (unlike MIPS's
// direct `slt`) has no single compare-and-set instruction. See
// genCompare()'s own comment for exactly which branch each operator
// uses.
//
// Unlike MIPS, PowerPC branches/calls have no branch-delay slot at
// all - every control-transfer instruction here (`b`/`bl`/`blr`/the
// `bc` forms) takes effect immediately, so there's no MIPS-style
// trailing `nop` anywhere in this backend's own output.
class PowerPCCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "powerpc"; }
    std::string sourceExtension() const override { return ".sppc"; }
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
    // Tears down the current call's own frame: restores `fp`/the
    // link register from the words genFunction()'s own prologue
    // saved them to, and leaves `sp` back where it was on entry (the
    // bottom of the caller's own pushed arguments) - shared by both
    // genFunction()'s own fall-through return and every `return`
    // statement's early exit (see genStmt()'s own Return case). Never
    // emits `blr` itself - the caller emits that right after.
    void emitEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in t0

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

    // Leaves slot[idxExpr]'s own effective address in `t2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into register `reg` (lis+ori - see
    // this file's own header comment on why PowerPC needs two
    // instructions here where 68000 needs none).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads a compile-time-known 32-bit literal into register `reg` -
    // `li` when it fits 16 bits signed, `lis`+`ori` otherwise (the
    // same split IntLit's own genExpr() case already uses).
    void loadImmediate(const std::string &reg, int32_t v);

    // Loads/stores `slot`'s own scalar value from/into `reg` - a
    // Global goes through `loadAddress`+`ldind`/`stind` (its address
    // is a compile-time label, not a frame offset); a Local or Param
    // goes straight through `ldindd`/`stindd fp,reg,slotDisp(slot)`,
    // no address computation needed at all since `slotDisp()` is a
    // compile-time constant. Never called with `reg` equal to "t2" -
    // the Global path's own scratch register for the address load.
    void loadSlotValue(const std::string &reg, const ir::Slot *slot);
    void storeSlotValue(const ir::Slot *slot, const std::string &reg);

    // Only ever called for a Global (the only kind with a name-based
    // label - see slotDisp() for a Local/Param's own frame-relative
    // address instead): a real separate-compilation label, shared
    // identically by every file that references this same global by
    // name.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // A Local or Param's own signed word displacement from `fp`. Every
    // call's prologue (see genFunction()) leaves `fp` pointing at that
    // call's own saved-old-`fp` word, with the saved return address
    // one word above it and the caller's own pushed arguments (in
    // declaration order, param 0 closest) above that - so a Param's
    // displacement is `+8+4*paramIndex`. Locals live below `fp`,
    // reserved by the prologue's own `addi sp,sp,-4*frameSize`;
    // Slot::frameOffset is already a 1-based sequential word index in
    // declaration order, so a Local's displacement is
    // `-4*frameOffset`. A Local array's element i therefore sits at
    // `-4*(frameOffset+i)` - storage address *decreases* as the
    // element index increases, the mirror image of a Global array's
    // increasing-address layout (see genIndexAddr()'s own comment).
    static int slotDisp(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Param) return 8 + 4 * slot->paramIndex;
        return -4 * slot->frameOffset;
    }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
};

}  // namespace pcc
