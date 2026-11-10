#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates Alpha AXP assembly (see AlphaAssembler.h) for the IR built by IRBuilder.
// `generate()` returns a complete, self-contained program loaded at a fixed address
// (`kBaseAddress`), ending in a self-branch infinite loop after `main` returns - Alpha has no
// way to stop the processor without PALcode (`HALT` is itself a `CALL_PAL`, privileged and
// unavailable without a PALcode image this project has no reason to write), so this backend
// never emits one; a test harness reads the result register while the loop spins, the same
// convention MipsCodegen.cpp/PowerPCCodegen.cpp use.
//
// `int` is 32 bits; `long`/`long long` are both the real, 64-bit-native Alpha quadword - unlike
// every 32-bit backend in this project, a wide value needs no register pair at all, since one
// of Alpha's 64-bit integer registers already holds it whole. Every arithmetic/logical/shift
// instruction this backend emits is therefore always the 64-bit ("Q") form, for every operand
// width; narrowT0To() re-normalizes the result's sign/zero extension afterwards whenever the
// logical type is narrower than 64 bits (see its own comment) - this is the one piece of
// bookkeeping a single-register-width design still owes a narrower C type.
//
// Register convention (the real Alpha calling standard's own names/numbers - see
// AlphaAssembler.cpp's regNumber(), not an invented layout):
//   t0  - genExpr()'s result ("ac")
//   t1  - the other operand in a binary op
//   t2  - holds a computed address when dereferencing one
//   t3-t7, at - scratch for the software multi-bit sequences below
//   sp  - hardware stack pointer: saves one operand across evaluating the other in a binary
//         expression, and carries each call's pushed arguments and each call's own frame
//   fp  - this call's frame pointer, giving every call (including recursive ones) its own
//         Local/Param storage, set up by the prologue and torn down by the epilogue
//   ra  - the hardware link register `jsr` sets and `ret` reads
//   t12 (pv) - the real Alpha calling convention's "procedure value" register: every call
//         (direct or through a function pointer) loads the callee's entry address here first,
//         since `jsr`'s target is always a register, never an immediate
//
// Frame layout relative to `fp`: `fp+0` is the caller's saved `fp`, `fp+4` the saved `ra` (each
// saved/restored as a 32-bit value - see genFunction()'s own comment on why that's safe), a
// Param's frameOffset`-relative address is `fp+frameOffset` (caller-pushed, right-to-left,
// caller cleanup, each argument w=widthOfType(type) bytes wide); a Local's is
// `fp-localLowOffset(...)`, in space the prologue reserves with one `lda sp,sp,-frameSize`.
//
// Alpha has no absolute-addressing memory operand: a Global's address is always built via an
// `ldah`+`lda` pair (see loadAddress()); a Local/Param's is a single `lda` off `fp`.
//
// Alpha has no hardware integer divide at all (a genuine, well-known omission from the real
// ISA - every real Alpha toolchain divides via a software routine or a floating-point
// reciprocal trick) - `/`/`%` always run genDivMod()'s bit-serial restoring-division loop,
// parameterized by the operand's own bit width (32 for `int`, 64 for `long`/`long long`), never
// just for the wide case the way every other backend here only needs a software routine for.
// `*` uses the real hardware `mulq` directly (a full 64-bit product's low 64 bits, which are
// exactly the right answer for `int*int` too - see genMul()'s own comment). `<<`/`>>` use the
// real hardware `sll`/`srl`/`sra`, always the 64-bit form (`>>` is `srl` unless the operand's
// signed, in which case it's `sra`) with narrowT0To() cleaning up after a narrower-than-64-bit
// result, since Alpha's shift instructions have no narrower form at all.
//
// Alpha has no condition-code register: `cmpeq`/`cmplt`/`cmple`/`cmpult`/`cmpule` each directly
// produce the 0/1 boolean a comparison needs (no branch required, mirroring MIPS's SLT); `if`/
// `while` test that 0/1 value against zero with `beq`/`bne` directly.
//
// Every call goes through `jsr`/`ret` (there is no CALLS-style single instruction that builds a
// whole call frame the way VAX's does) - see genFunction()/the `Call`/`CallPtr` cases of
// genExpr().
class AlphaCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "alpha"; }
    TypeWidths typeWidths() const override { return {1, 2, 4, 8, 8}; }
    std::string sourceExtension() const override { return ".salpha"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; the other five are real relocatable object files
    // for separate compilation.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    // Shared function-exit sequence (deallocate locals, restore the caller's `fp`/`ra`,
    // `ret`); used by both the fall-through epilogue and every explicit early `return`.
    void genEpilogue();
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in t0

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    // Software restoring division: dividend/divisor magnitudes shifted one bit at a time into
    // a remainder register, comparing against the divisor each step (see genDivMod()'s own
    // comment for why the top bit needs special care). `bits` is 32 for `int`, 64 for
    // `long`/`long long`. Leaves the quotient or remainder in t0.
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned, int bits);
    // The same algorithm, assuming t0/t1 already hold the dividend/divisor (genCompoundOp's
    // "old"/"rhs" are already there) rather than evaluating a fresh pair of sub-expressions.
    void genDivModCore(bool wantRemainder, bool isUnsigned, int bits);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's own frame slots
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step: old in t0, rhs in t1,
    // leaves the raw (not yet re-narrowed) new value in t0.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign/CompoundDerefAssign; t2 must already hold the resolved
    // target address on entry.
    void genCompoundThroughT2(const ir::Expr &e);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }
    // log2 of an element type's width (1/2/4/8 bytes -> 0/1/2/3), the shift amount
    // genIndexAddr() scales an index by.
    int elemShift(Type t) const {
        switch (widthOfType(t)) {
            case 1: return 0;
            case 2: return 1;
            case 4: return 2;
            default: return 3;
        }
    }

    // Loads a value of `type` from the address in `addrReg` into t0, sign- or zero-extending
    // per its signedness and width (always a no-op past the raw transfer for an 8-byte type).
    void loadOperand(const std::string &addrReg, Type type);
    // Stores t0 into the address in `addrReg`, truncating to `type`'s width.
    void storeOperand(const std::string &addrReg, Type type);
    // Re-extends t0 from its low widthOf(to) bytes, per `to`'s signedness, via an
    // `sll`+`sra`/`srl` pair - needed even for a plain `int` result here (unlike every
    // 32-bit-register backend in this project), since Alpha's own arithmetic/shift
    // instructions are always 64-bit and leave whatever was already in the register's upper
    // half otherwise untouched.
    void narrowT0To(Type to);
    void genConvert(const ir::Expr &e);

    // Leaves slot[idxExpr]'s effective address in t2.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Loads `lbl`'s address into `reg` (`ldah`+`lda`).
    void loadAddress(const std::string &reg, const std::string &lbl);

    // Loads 32-bit-range immediate `v` into `reg`: a single `lda` when it fits `lda`'s own
    // 16-bit signed displacement, `ldah`+`lda` otherwise (always sign-extending into the full
    // 64-bit register - exactly what every int-producing real Alpha sequence does).
    void loadImm32(const std::string &reg, int32_t v);
    // Loads an arbitrary 64-bit immediate into `reg`, for a `long long` literal outside
    // loadImm32()'s sign-extended 32-bit range: the low and high halves are each built via
    // loadImm32() into `reg`/`at`, the high half shifted into position, then ORed together.
    void loadImm64(const std::string &reg, uint64_t v);

    // Loads a Local/Param's `fp`-relative address (one `lda`), or a Global's fixed label
    // address (loadAddress()), into `reg`.
    void loadSlotAddr(const std::string &reg, const ir::Slot *slot);

    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw v0, v1, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    // One entry per currently-open loop (innermost last): {continue label, break label}.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
