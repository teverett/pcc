#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates 32-bit SPARC V8 assembly (this project's own dialect -
// see SparcAssembler.h) for the IR built by IRBuilder - like
// PowerPCCodegen (the closest template to this backend - another
// real-register, real-hardware-stack 32-bit RISC load/store target
// with no immediate wide enough to hold a full 32-bit address in one
// instruction, see its own header comment), `generate()` returns a
// complete, self-contained program loaded at a fixed address
// (`kBaseAddress`, see SparcCodegen.cpp) - no OS, no boot ROM, the
// simulator sets the CPU's PC there directly - halting in a
// self-branch infinite loop, the same "no stack, no recursion" model
// every backend shares even though real function calls (`call`/
// `retl`) and a real hardware stack (conventionally `%o6`/`sp`) are
// both used.
//
// This language's own `int` is 32 bits here, matching this target's
// native integer-register width (SPARC V8, not the 64-bit SPARC V9
// extension), same word-size reasoning as every other backend.
//
// Real SPARC has 32-entry *windowed* `%i0-%i7`/`%l0-%l7`/`%o0-%o7`
// register banks tied to a real `save`/`restore` instruction pair
// that rotates the whole window on every non-leaf call, giving each
// activation of a function its own private `%i`/`%l` bank for free.
// That mechanism is deliberately *not* used here even though this
// backend does support real recursion (direct and mutual - see
// genFunction()'s own comment): real SPARC hardware has only a small,
// implementation-defined number of windows (commonly 8), and once
// every window is in use a further `save` traps (window overflow) -
// on real hardware that trap is serviced by the OS, spilling the
// oldest window to memory so the count effectively becomes unbounded.
// This target has no OS and sets up no trap table at all (see
// SparcCodegen.cpp's own entry-sequence comment), so relying on
// `save`/`restore` here would silently cap recursion depth at
// whatever this simulator's own window count happens to be instead of
// the "as deep as the stack allows" every other migrated backend
// already provides - not an acceptable trade merely to get the
// hardware's own free argument-passing. So `%i`/`%l`/`%o`/`%g` stay
// just 24 plain, always-the-same-physical-register names for the
// entire run (real `%g0-%g7` are never windowed regardless), and
// recursion safety instead comes from a conventional software stack
// frame - every call gets its own frame on the real hardware stack
// (`sp`), addressed through an explicit frame-pointer register (`fp`)
// this backend pushes/pops itself, the same general shape as
// I386Codegen.h's own `ebp`-based frame (see genFunction()'s own
// comment for the exact layout), just built from SPARC's own
// mnemonics instead of `push`/`pop`:
//   t0 (%o0) - "ac", genExpr()'s own result (the register every other
//              backend also calls this - 68000's D0, PowerPC's r3) -
//              also the real SPARC ABI's own return-value register,
//              though that convention is otherwise unused here.
//   t1 (%o1) - "the other operand" in a binary op (68000's D1)
//   t2 (%o2) - holds a computed address when dereferencing one
//              (array/pointer indexing, `*`/`&`, or a Local/Param's
//              own frame-relative address - see loadSlotAddr()) -
//              68000's A0
//   sp (%o6) - the real SPARC stack-pointer register number: used to
//              save one operand across evaluating the other in a
//              binary expression, to push this call's own argument
//              words right before a `call`, and (in every function's
//              own prologue/epilogue) to push/pop `lr` and `fp` and to
//              reserve/release this call's own Local storage.
//   lr (%o7) - the real register SPARC's own `call` instruction
//              itself writes the return address into - directly
//              readable/writable like any other GPR (unlike PowerPC's
//              own link register, which needs `mflr`/`mtlr` to reach
//              at all - see PowerPCCodegen.h's own header comment),
//              so no shuttle register is needed here. Spilled to this
//              call's own frame in the prologue and reloaded in the
//              epilogue (see genFunction()'s own comment) - without
//              that, a non-leaf function's own call to another
//              function would overwrite `lr` outright, so its own
//              eventual `retl` would return into whatever its *own*
//              last call last returned to, not back to its own
//              caller.
//   fp       - this backend's own dedicated frame-pointer register
//              (an otherwise-unused encoding - see SparcAssembler.cpp's
//              own `regNumber()` - not real SPARC's windowed `%fp`/
//              `%i6`, since no window ever rotates here): points at
//              this call's own saved-`fp` word for as long as this
//              call's own body runs, giving every Local/Param a
//              stable per-call address (see loadSlotAddr()) regardless
//              of how deep the recursion is.
//   g0       - the real hardwired-zero register, used only as the
//              `or g0,src,dst` "move register" idiom (real SPARC's
//              own synthetic `mov`) and as `sdiv`/`call`'s own
//              discarded-result register
//
// SPARC has no absolute-addressing memory operand at all (same
// restriction as PowerPC/MIPS - see PowerPCCodegen.h's own header
// comment) - every load/store goes through a base register plus a
// (here always zero) displacement, so reading or writing any slot
// first computes that slot's own address into `t2` (or `t1` - see
// genIndexAddr()), then `ldind`/`stind t2,t0` (this backend's own
// dedicated mnemonics for `ld`/`st` with a fixed zero displacement -
// same "no bracketed memory operand" convention as PowerPC/MIPS). A
// Global's address is fixed for the whole run, loaded via `sethi`+`or`
// (SparcAssembler.cpp resolves a label operand to its own upper-22/
// lower-10-bit halves respectively for exactly these two mnemonics -
// see its own header comment); a Local or Param's address instead
// varies with every call, computed from `fp` with a plain `add`/`sub`
// immediate (see loadSlotAddr()) - Globals alone keep the fixed,
// name-derived label (see label()) a Local/Param used to also get
// back when every Slot was one shared static cell.
//
// `*` uses the real hardware `smul` (a full 32x32->32 product, no
// operand-size limitation the way the 68000's own `MULS.W` has). `/`
// uses the real hardware `sdiv`, which (like every SPARC V8 32-bit
// divide) computes a 64-bit `Y:rs1` dividend under the hood - the Y
// register has to be explicitly seeded with the dividend's own sign
// bits first (`sra rs1,31,t2` then `wry t2,g0`, real SPARC's own
// standard idiom for a 32-bit signed divide - see genDivMod()'s own
// comment) or the result is simply wrong whenever the dividend is
// negative. `%` has no direct hardware remainder instruction (same
// situation as PowerPC's own classic 32-bit `divw` - see
// PowerPCCodegen.h's own header comment), so it's synthesized the
// same standard way: `sdiv` for the quotient, `smul` to multiply back
// out, `sub` to recover the remainder from the original dividend.
//
// `<<`/`>>` use the real hardware variable-count register-form shift
// (`sll`/`srl`) - `>>` is logical/unsigned (`srl`, not the arithmetic
// `sra`), the same convention every other backend's own `>>` already
// uses (no unsigned type to make the distinction with).
//
// SPARC has no single compare-and-set instruction (same situation as
// PowerPC's own `cmpw` - see PowerPCCodegen.h's own header comment) -
// a comparison here always needs `cmp` (this dialect's own name for
// real SPARC's `subcc rs1,rs2,%g0` - SPARC has no standalone compare
// opcode at all, only a subtract that discards its own result and
// keeps the flags) plus a real conditional branch to decide which
// boolean value to materialize. See genCompare()'s own comment for
// exactly which branch each operator uses.
//
// Every real SPARC branch and `call` has a one-instruction delay
// slot - this backend always fills it with a real `nop`, synthesized
// automatically by SparcAssembler.cpp itself (see that file's own
// header comment) rather than ever emitted explicitly here -
// simplest-and-always-correct, same "this project already prioritizes
// correctness/simplicity over performance everywhere else" reasoning
// used throughout.
class SparcCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "sparc"; }
    std::string sourceExtension() const override { return ".ssparc"; }
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
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots, at runtime

    // Leaves slot[idxExpr]'s own effective address in `t2`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s own address into register `reg` (sethi+or - see
    // this file's own header comment on why SPARC needs two
    // instructions here where 68000 needs none). Only ever called with
    // a Global's or a Function's own name-derived label - a Local/
    // Param's address is never fixed, so it goes through
    // loadSlotAddr() below instead.
    void loadAddress(const std::string &reg, const std::string &lbl);
    // `or g0,src,dst` - real SPARC's own synthetic "move register"
    // idiom (SPARC has no dedicated move opcode at all).
    void movReg(const std::string &dst, const std::string &src);

    // Leaves `slot`'s own effective address in register `reg` - a
    // Global goes through loadAddress()/label() as always; a Local or
    // Param instead goes through `fp` (see this file's own header
    // comment and genFunction()'s own comment for the exact layout):
    // `fp+8+4*paramIndex` for a Param (the caller's own pushed
    // argument, already sitting above the return address and the
    // saved `fp` this call's own prologue just pushed), `fp-4*
    // frameOffset` for a Local (frameOffset is 1-based - see IR.h's
    // own comment - so the first Local sits at `fp-4`, growing further
    // down from there). Emits nothing but a plain `or g0,fp,reg` when
    // the computed offset is exactly 0.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);
    // The byte offset from `fp` loadSlotAddr() above computes for a
    // Local or Param (never called for a Global).
    static int slotFrameByteOffset(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Param) return 8 + 4 * slot->paramIndex;
        return -4 * slot->frameOffset;
    }

    // Loads a plain 32-bit literal into register `reg` - shared by
    // genExpr()'s own IntLit case and genLocalArrayInit()'s own
    // per-element store, both of which need the exact same small-
    // immediate-vs-sethi+or choice (see genExpr()'s own IntLit case
    // for why: no single SPARC instruction ever carries a full 32-bit
    // immediate).
    void loadImmediate(const std::string &reg, int32_t v);
    // Restores `lr`/`fp`/`sp` to the calling function's own values,
    // right before a `retl` - see genFunction()'s own comment for the
    // exact sequence this undoes. Shared by genFunction()'s own
    // fall-through epilogue and genStmt()'s own early-exit Return
    // case; never touches `t0`, which by the time either caller emits
    // this already holds the function's own return value (or is
    // simply unread, for a `void`-shaped return).
    void emitEpilogue();

    // Only ever called for a Global slot - a Local/Param's address is
    // never fixed for the whole run, so it's computed fresh every call
    // through loadSlotAddr() above instead of named by a label at all.
    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
};

}  // namespace pcc
