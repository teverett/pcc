#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates MCS-51 assembly (this project's own dialect, but using
// real, standard 8051 mnemonics throughout - see
// MCS51Assembler.h) for the IR built by IRBuilder, targeting the
// whole Intel MCS-51 family this backend covers under one roof -
// 8051, 8052, 8031, 8032 (see src/main.cpp's own makeBackend()
// comment: these four part numbers are software-identical as far as
// any instruction this compiler could ever emit is concerned - 8052
// vs 8051 is just more mask ROM/RAM and a third timer, 8031/8032 are
// just the same two parts with the mask ROM removed in favor of an
// external EPROM, and this backend never touches ROM size, RAM size,
// or on-chip peripherals - same reasoning as the 8086/8088 alias
// already documented there). `generate()` returns a complete, self-
// contained program assembled to run from CODE address 0 (the real
// 8051 reset vector), halting in an infinite loop - no OS, no
// interrupts, the same "no stack, no recursion" bare model every
// other backend shares (see README's "Scope and limitations"),
// just for a target with a genuinely Harvard architecture: CODE
// (where this backend's own instructions live, fetched via PC) and
// external DATA (XDATA, addressed via MOVX and DPTR) are two
// distinct address spaces on real hardware, not two views of the
// same bytes the way every other backend here gets to assume.
//
// `objectMode` (real separate compilation, `--format=coff`/`elf`/
// `macho`/`aout`/`objraw` - see Codegen.h's own header comment)
// assembles at this same CODE address 0, but omits the entry
// sequence itself - MCS51LinkerFactory.cpp's own synthesized entry
// object is solely responsible for that, once every object is merged
// and `fn_main`'s linked address is resolvable, same pattern every
// other GenericLinker-based backend follows (see M68kCodegen.cpp's
// own `generate()` comment). A global *variable*'s own initial value
// still has to be written at runtime, not loaded as part of the image
// (real XDATA is RAM, with no "initial contents" of its own - see
// declareGlobalTemp()'s own comment), so in object mode that write-
// out code runs as the prologue of `fn_main` itself instead of
// `_start` (the one point every linked program is guaranteed to reach
// exactly once, before any other code, without MCS51LinkerFactory.cpp's
// own entry object - built once, before it has ever seen any user
// object at all - needing to know anything about this file's own
// globals). Only the file that defines `main` gets this treatment: a
// global *defined* in a different file from the one that defines
// `main` has no mechanism here to run its own initializer under real
// separate compilation at all.
//
// Every global/local/param's own XDATA address is a real, relocatable
// assembler symbol (see allocXdata()'s own comment, and
// MCS51Assembler.h's own header comment on the `xdata` directive and
// the `"..d"` pseudo-symbol): two separately-compiled files' own
// distinct globals get distinct, non-colliding linked XDATA addresses
// once merged (GenericLinker.h's own second, data-space layout pass -
// see its own `dataBaseAddress` comment), the data-space counterpart
// of how `fn_`-prefixed functions' own CODE addresses already work.
// What's still unsupported is cross-file *visibility*: a variable's
// XDATA label is only ever declared (an `xdata` directive emitted) by
// the one file that actually defines it - `generate()`'s own pre-pass
// below skips every `isExternal` global entirely - so a file that
// only *declares* a global `extern` without defining it never emits
// that label at all, and referencing it from that file still fails,
// now as MCS51Assembler.cpp's own `resolveDataSyllable()` loud
// "no XDATA address" error instead of a silent miscompile. The same
// per-defining-file-only visibility applies to a function's own
// parameters: calling a function defined in a different file only
// works if it takes no arguments, since an argument is written
// straight into the callee's own `p_<name>_<i>` XDATA slot (see
// genExpr()'s own `Call` handling), which is just as much a label
// only the defining file ever declares - calling an external function
// with one or more arguments is a compile error here (see that same
// `Call` handling's own comment), not a silently-wrong program.
//
// The 8051 has no general-purpose 16-bit register and no memory-to-
// memory move wider than one byte at a time (MOV direct,direct is
// the closest thing, but still only 8 bits) - every ALU operation
// only ever touches the accumulator `A` plus one other 8-bit operand
// (direct, immediate, or @Ri-indirect). So, like every other 8-bit-
// register-starved backend here (6502, 8080), this one keeps every
// value in flight in one of five fixed, two-byte pseudo-registers -
// `ac` (genExpr()'s own result), `bc` ("the other operand" in a
// binary op), `cx`/`dx` (multiply/divide's own extra scratch), `ptr`
// (an indirection target) - but unlike the 6502/8080 backends, these
// live in *internal* direct RAM (fixed addresses `kScratchBase`.. -
// comfortably below the 128-byte floor every one of the four target
// parts guarantees), not in the same address space as a plain
// variable, since internal RAM is far too small (128-256 bytes) to
// also hold ordinary program data (this project's own tests/*.c
// already uses single arrays bigger than that). Every *other*
// variable - every global, local, and array element - instead lives
// in external DATA memory at a real, relocatable address (see
// declareTemp()/declareArray(), and allocXdata()'s own comment),
// reached only via MOVX + DPTR, one byte at a time - the same
// "generous flat address space" every other backend's own variable
// storage already gets to assume, just paid for here with an explicit
// MOVX round trip (through `ac`/`bc`/etc, which is where the
// accumulator-based ALU ops above actually operate) every time a
// variable's value needs to move in or out of a pseudo-register,
// rather than 6502's plain `lda`/`sta`. These two address spaces
// never alias each other in this backend's own model - no symbol
// ever needs to be resolved as "whichever space", because codegen
// itself (not the assembler - see MCS51Assembler.h) is the one
// deciding which space any given value lives in, and a pseudo-
// register's own direct address is a plain assembler equate exactly
// like the 6502 backend's own zero-page cells. Two more direct-RAM
// equates, `dpl`/`dph`, name the real DPL/DPH SFRs (0x82/0x83) that
// back the `ptr`-unrelated DPTR register MOVX addresses through -
// unlike `ac`/`bc`/`cx`/`dx`/`ptr`, these aren't this backend's own
// invented scratch cells, just plain `direct`-addressable names for
// two already-real SFRs, used by loadXdataAddress() to read a just-
// loaded, now-relocated DPTR value back out into an ordinary pseudo-
// register pair (computing an XDATA address as a *value*, the way
// `&g` or an array's own base needs, rather than dereferencing it via
// MOVX the way every plain variable read/write does).
//
// No hardware 16-bit multiply or divide (the real `MUL AB`/`DIV AB`
// are 8x8->16 and 8/8 only, and this language's own `int` is 16
// bits), so `*` is the same textbook shift-and-add loop, and `/`,
// `%` the same repeated-subtraction loop, every other software-
// multiply/divide backend here already uses - only verified against
// non-negative operands (see README's "Scope and limitations").
// `MUL`/`DIV` are still part of this dialect (see
// MCS51Assembler.h) for completeness/`asm`-escape use; codegen itself
// just never needs an 8x8 result widely enough to bother.
//
// No variable-count shift instruction either (`RLC A`/`RRC A` always
// rotate the accumulator by exactly one bit) - `<<`/`>>` loop one bit
// at a time, chaining RLC/RRC across both bytes, the same pairing
// idiom 6502's ASL/ROL or LSR/ROR already uses - just driven by a
// real `DJNZ` loop-and-branch-if-nonzero instruction instead of a
// separate decrement+test (see safeDjnz()'s own comment).
//
// Comparisons (`<`,`<=`,`>`,`>=`) use the exact same "N xor V" idiom
// the 6502/8080 backends use, since the 8051's PSW genuinely has both
// a usable sign bit (ACC.7) and a real signed-overflow flag (OV, set
// by ADD/ADDC/SUBB) - `JB OV,...`/`EOR #$80`'s role here is played by
// jumping around a `CPL ACC.7`-equivalent fixup (see
// emitLessCompare()'s own comment). `==`/`!=` need none of that - a
// 16-bit SUBB-chain-then-OR-the-bytes zero check, same as everywhere
// else.
//
// Every jump/call this backend emits uses the full-range `LJMP`/
// `LCALL` (unconditional, full 64KB CODE range) rather than risk the
// 8051's real ±127-byte-range conditional branches (`JZ`/`JNZ`/`JC`/
// `JNC`/`DJNZ`) ever landing out of range on a large generated
// function - the exact same "short branch to an always-near label,
// `LJMP` from there" pattern 6502's own `safeBranch()` uses (see
// that file's own header comment), just with 8051 mnemonics.
class MCS51Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "8051"; }
    std::string sourceExtension() const override { return ".s51"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is this backend's own whole-program flat binary; the
    // other five all mean a real relocatable ObjectFile for separate
    // compilation (see Codegen.h's own header comment on why this is
    // asked per-backend, and generate()'s own `objectMode` comment
    // above) - same convention as every other GenericLinker-based
    // backend's own `isObjectFormat()` override.
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
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` pseudo-register

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Leaves slot[idxExpr]'s own effective XDATA address in the
    // `ptr` pseudo-register - the only thing in this backend that
    // ever needs a real runtime-computed address (every other access
    // goes through a compile-time-known constant - see
    // allocXdata()'s own comment).
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Assumes `ptr` already holds a target XDATA address
    // (genIndexAddr()'s own result, or DerefAssign's own pointer
    // value) and writes `ac`'s two bytes there via DPTR+MOVX -
    // doesn't touch `ptr` itself, so the caller's address survives.
    void storeAcIndirect();

    // Copies one pseudo-register to another (both already internal-
    // RAM direct addresses) via this dialect's own real direct-to-
    // direct `MOV` (`movdd dst,src` - see MCS51Assembler.h).
    void copy16(const std::string &dst, const std::string &src);

    // Reads/writes the two bytes stored at `xdataExpr`'s own XDATA
    // address into/out of a pseudo-register, via DPTR+MOVX -
    // `xdataExpr` is a `label()` result (optionally `+<constant>`,
    // for an array element - see declareGlobalArray()'s own comment),
    // emitted as `movdptr`'s own operand, which this assembler
    // resolves against its own XDATA label table and, in object
    // mode, relocates against the `"..d"` pseudo-symbol (see
    // MCS51Assembler.h's own header comment) - a relocatable symbol
    // reference, never a bare numeric literal. See allocXdata()'s own
    // comment on why ordinary variables live here rather than in
    // internal RAM alongside the pseudo-registers.
    void loadXdata(const std::string &dstReg, const std::string &xdataExpr);
    void storeXdata(const std::string &xdataExpr, const std::string &srcReg);

    // Loads `xdataExpr`'s own XDATA *address* (not the two bytes
    // stored there) into a pseudo-register pair: `movdptr xdataExpr`
    // resolves and (in object mode) relocates the address into DPTR
    // exactly like loadXdata()/storeXdata() above, then `dpl`/`dph`
    // (the real DPL/DPH SFRs, plain `direct`-addressable - see this
    // file's own header comment) are copied out into `dstReg`'s own
    // two bytes via ordinary `mova`/`movd`. Used by genAddrOf() (`&g`,
    // `&arr[i]`) and genIndexAddr()'s own array-base case, the two
    // places that need an XDATA address as an ordinary 16-bit value
    // rather than as something to MOVX through.
    void loadXdataAddress(const std::string &dstReg, const std::string &xdataExpr);

    // Loads `codeExpr`'s own CODE-space address (a function's entry
    // label, or a locally-generated return-address label - never the
    // bytes stored there, there being no "bytes stored there" to a
    // function label at all) into a pseudo-register pair: `movdptrc
    // codeExpr` resolves and (in object mode) relocates the address
    // into DPTR via the real `MOV DPTR,#data16` encoding, exactly like
    // loadXdataAddress() above, just against this assembler's CODE-
    // space symbol table instead of its XDATA-space one - then `dpl`/
    // `dph` are copied out the same way. Used by genAddrOf() (`&fn`)
    // and genExpr()'s own `CallPtr` handling (to get a synthesized
    // return-address label's value into a pseudo-register pair before
    // pushing it).
    void loadCodeAddress(const std::string &dstReg, const std::string &codeExpr);

    // "the short branch Bcc almost reaches, plus an always-in-range
    // LJMP from there" - see this file's own header comment.
    // `branchMnemonic` is one of MCS51Assembler.cpp's own short
    // conditional-branch mnemonics ("jz", "jnz", "jc", "jnc",
    // "jbacc7", "jnbacc7", "jbov", "jnbov").
    void safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                     const std::string &targetIfNot);

    // Same idea as safeBranch(), but for the real `DJNZ direct,rel`
    // decrement-and-branch-if-nonzero instruction used by every loop
    // here (shift/multiply/divide's own counters) - `DJNZ` is always
    // the "taken" (still nonzero) edge, an always-in-range `LJMP`
    // the "not taken" (just hit zero) edge.
    void safeDjnz(const std::string &counterReg, const std::string &targetIfNonzero,
                  const std::string &targetIfZero);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    // Declares `lbl` (a label() result above) as the name of `count`
    // consecutive bytes in the XDATA address space, by emitting an
    // `<lbl>: xdata <count>` directive into `code_` - MCS51Assembler.cpp's
    // own two-pass assembler is the one that actually assigns `lbl`
    // its XDATA offset (sequentially, in the order these directives
    // appear - see that file's own header comment on the `xdata`
    // directive) and tracks it in its own data-space label table,
    // entirely separate from its CODE-space one, since XDATA and CODE
    // are genuinely different address spaces on this target (see this
    // file's own header comment) - nothing analogous to 6502's single
    // shared "trailer" exists here at all. Every later reference to
    // `lbl` (loadXdata()/storeXdata()/loadXdataAddress()) just emits
    // `lbl` itself (optionally `+<constant>`) as a `movdptr` operand
    // rather than resolving any address itself - this function's own
    // call sites never see a numeric XDATA address at all.
    void allocXdata(const std::string &lbl, int count);

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    // Declares a plain 16-bit variable: allocates its XDATA storage
    // and (for a global, which has no enclosing statement to run its
    // own initializer inline the way LocalDecl does - see
    // genStmt()'s own Return/LocalDecl handling) emits the actual
    // MOVX writes that give it its initial value into `initCode_`,
    // emitted once before any other code runs and before `main` is
    // ever called - real hardware XDATA is RAM, with no "initial
    // contents" of its own the way CODE/ROM has. That's `_start`
    // itself outside object mode, and `fn_main`'s own prologue inside
    // it (see generate()'s own `objectMode` comment above for why).
    void declareTemp(const std::string &lbl);
    void declareGlobalTemp(const std::string &lbl, long initValue);
    void declareArray(const std::string &lbl, int arraySize);
    void declareGlobalArray(const std::string &lbl, int arraySize, const std::vector<ir::ArrayInitElem> &values);
    void genArrayElementInit(const std::string &elemExpr, const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::ostringstream initCode_;  // globals' own initial-value MOVX writes - see declareGlobalTemp()'s own comment
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
};

}  // namespace pcc
