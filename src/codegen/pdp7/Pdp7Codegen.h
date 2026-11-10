#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates PDP-7 assembly for the IR built by IRBuilder; names are
// already resolved to Slot/Function, so this is pure instruction
// selection with no symbol table of its own.
//
// Every function gets a per-call activation record on a software
// stack built from `__sp`/`__fp` (declared in generate()): this
// target has no hardware stack pointer and no indexed/displacement
// addressing mode, only one accumulator plus direct/indirect memory
// addressing (a "law" instruction and one "i" indirection bit), so
// the whole frame mechanism is built out of that one indirection bit.
// See emitPrologue()/emitEpilogueAndReturn()/pushAC()/popAC() for the
// layout, and emitLoadSlotAddress() for how Slot::frameOffset becomes
// a real address. Direct and mutual recursion both work: every
// activation's Param/Local storage lives in that call's own frame or
// stack slot, never in a cell shared across calls the way a Global's
// storage is.
//
// pdp7as truncates labels to 8 characters, so every label here is a
// short synthetic name: "G_<name>" for a Global Slot, "FN_<name>" for
// an ir::Function, and "T<N>"/"N<N>"/"J<N>" for codegen-internal
// scratch cells, literal constants and jump targets, plus a fixed set
// of "__"-prefixed global cells (`__sp`, `__fp`, `__stmp`, `__stmp2`,
// `__addr`, `__retval`, `__retaddr`) shared by every function (safe
// since each one's live range is a single short, call-free
// instruction sequence). A Local or Param never gets a label - see
// emitLoadSlotAddress().
class Pdp7Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp7"; }
    TypeWidths typeWidths() const override { return {1, 1, 1, 2, 4, 18}; }
    std::string sourceExtension() const override { return ".s"; }
    std::vector<std::string> assembledFormats() const override { return {"a7out", "list", "ptr", "rim", "coff"}; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in AC

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitEq(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    void emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    // Tests signed less-than of two already-materialized cells,
    // jumping to `Ltrue`/`Lfalse`. Differing signs are decided
    // directly (a plain subtract-then-test-sign would overflow when
    // operands are far enough apart); same-sign operands fall through
    // to subtract-and-test-sign, safe since same-sign 18-bit values
    // can't differ by more than the representable range.
    // `signA`/`signB` give an operand's sign when it's known at compile
    // time (0 non-negative, 1 negative; -1 unknown), which removes
    // that operand's sign test.
    void emitSignedLtCells(const std::string &A, const std::string &B, const std::string &Ltrue,
                            const std::string &Lfalse, int signA = -1, int signB = -1);
    // Evaluates both sides of a relational comparison into cells for
    // emitSignedLtCells() (sign bits flipped first for an unsigned
    // comparison, mapping unsigned order onto signed order), setting
    // `A`/`B` to the cells and `signA`/`signB` to any sign known at
    // compile time.
    void genCompareCells(const ir::Expr &a, const ir::Expr &b, bool unsignedCmp, std::string &A,
                         std::string &B, int &signA, int &signB);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);

    // Masks AC down to `to`'s value range and, for a signed type,
    // re-sign-extends from that range's top bit.
    void narrowAC(Type to);
    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }
    // `char`/`short`/`int` are each 1 pdp7 word; only `long` (2 words)
    // and `long long` (4 words) are "wide" on this backend.
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }

    // --- `long`/`long long` (wide) support --------------------------------
    //
    // A wide value is `widthOfType(type)` consecutive pdp7 words, word
    // 0 (low) first. genExpr()'s single-AC contract doesn't generalize
    // to multiple words, so a wide value is instead left PUSHED on the
    // software stack when produced, word 0 on TOP; every genExpr()/
    // genStmt() call site checks `isWide(e.type)` and branches to the
    // matching wide helper instead of the plain single-AC path.
    //
    // Multiply and divide/modulo are not implemented for a wide
    // operand - see genWideMulDivErr().

    // Pushes a wide value's `widthOfType(type)` words from consecutive
    // memory starting at `baseAddr` (in AC on entry); word 0 is read
    // and pushed last so it ends up on top.
    void pushWideFromAddress(Type type);
    // Reverse of pushWideFromAddress(): pops the words (word 0 first)
    // into consecutive memory starting at the address in AC on entry.
    void popWideToAddress(Type type);
    // Pushes `widthOfType(type)` words of the wide sign/zero-extension
    // of `value`; word 0 pushed last.
    void pushWideLiteral(Type type, long value);
    // Generalizes genExpr() for a wide-typed expression: leaves its
    // words pushed on the stack (word 0 on top) instead of in AC.
    // Handles every expression kind IRBuilder.cpp can give a wide type
    // (Var, IntLit, Assign, Convert, Unary -/~, wide binary ops);
    // anything else reaching here is a compile-time error.
    void genWideExpr(const ir::Expr &e);
    // Pops two wide operands (lhs then rhs were pushed, so rhs is on
    // top) and pushes a `wordCount`-word +/- result, carry/borrow
    // ripple-chained via the link bit (`cll`/`tad`/`snl`).
    void genWideAddSub(bool isAdd, int wordCount);
    // Pops one wide operand and pushes its 2's-complement negation.
    void genWideNegate(int wordCount);
    // Pops two wide operands and pushes their bitwise and/xor
    // (word-by-word) or or (De Morgan `~a & ~b`, as genBitwise() uses
    // for the narrow case).
    void genWideBitwise(const std::string &op, int wordCount);
    // Pops two wide operands and leaves a narrow 0/1 result in AC:
    // compares from the most-significant word down, short-circuiting
    // on the first differing word. Only the top word's comparison can
    // be signed (`isUnsigned` picks that); every lower word is always
    // compared unsigned, since once the higher words are equal the
    // remaining value is a non-negative magnitude.
    void genWideCompare(const std::string &op, int wordCount, bool isUnsigned);
    [[noreturn]] void genWideMulDivErr(int line, const std::string &opName);
    // dest = a + b + carryCell, updating carryCell to this word's
    // carry-out. `carry1Cell`/`carry2Cell` are private scratch for the
    // two-stage carry capture, never read by the caller.
    void emitWideAddWithCarry(const std::string &dest, const std::string &a, const std::string &b,
                               const std::string &carryCell, const std::string &carry1Cell,
                               const std::string &carry2Cell);

    // Branch fusion for an `If`/`While` condition: jumps to `Lfalse`
    // when `cond` is false, else falls through. For a plain comparison
    // or an `&&` of such, skips genCompare()'s materialize-then-test
    // step and branches directly; any other condition shape (`||`, a
    // bare variable, a call, bitwise, ...) falls back to the generic
    // path below. Avoids `spa`, using only `sma`/`sad` as elsewhere in
    // this file.
    void genCondBranch(const ir::Expr &cond, const std::string &Lfalse);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    // Emits the call to __divmod or, for unsigned operands, __udivmod;
    // both take __dm_a/__dm_b and leave the quotient in AC and the
    // remainder in __dm_a.
    void callDivMod(bool isUnsigned);
    void ensureRuntimeRoutines();
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    // AC = AC shifted by `count` (0-17) via the EAE shift instructions.
    void emitConstantShift(const std::string &op, long count, bool isUnsigned);
    // How genOperandPair() transforms the right operand before
    // storing it: as is, negated (for `-`), or complemented (for `|`).
    enum class RhsXform { None, Negate, Complement };
    // Leaves `lhs` in AC and returns the name of a memory cell holding
    // `rhs` (after `xform`), with the fewest instructions available: a
    // constant or plain variable is used in place; a plain-variable
    // `lhs` is read after `rhs`; a call-free `rhs` keeps `lhs` in a
    // static temp; only an `rhs` that calls puts `lhs` on the stack.
    // C leaves the two operands' evaluation order unspecified, so
    // evaluating `rhs` first is allowed. With `dest` set, `rhs` always
    // ends up in that cell.
    std::string genOperandPair(const ir::Expr &lhs, const ir::Expr &rhs, RhsXform xform,
                               const std::string &dest = "");
    // The memory cell that already holds `e`'s value, if any: a
    // constant's literal cell, or a global or leaf-function scalar that
    // needs no narrowing on load. Emits nothing but the literal.
    std::optional<std::string> simpleCell(const ir::Expr &e);
    // True when evaluating `e` can't call a function, so a static temp
    // held across it can't be overwritten by a nested activation.
    static bool isCallFree(const ir::Expr &e);
    // AC = &slot[idxExpr], without touching the stack unless idxExpr
    // itself calls.
    void computeElementAddressAC(const ir::Slot *slot, const ir::Expr &idxExpr);
    void genIndex(const ir::Expr &e);       // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    // --- Compound assignment ('+=' etc., prefix/postfix '++'/'--') and
    // ternary -------------------------------------------------------------
    //
    // `CompoundAssign`/`CompoundDerefAssign` can never be wide here
    // (IRBuilder.cpp's rejectIfWide() rejects a wide target first), so
    // these only ever produce a single AC-full result.
    //
    // Consumes e.rhs's value (already in AC) and stashes it in
    // whichever per-operator cell the matching narrow Binary codegen
    // (genBinary/genBitwise/genShift/genMul/genDivMod) uses for its
    // second operand (`__stmp2` for `+`/`-`/`&`/`^`/`|`, a fresh cell
    // for `*`'s inline operand, `__dm_b` for `/`/`%`, a fresh cell for
    // `<<`/`>>`'s shift count); `+`/`-`/`|` pre-apply their own
    // negation/complement here. Returns that cell's label, for
    // finishCompoundOp() to read back.
    std::string prepareCompoundRhs(const ir::Expr &e);
    // AC must hold the target's OLD value on entry; leaves AC holding
    // `old <e.op> rhs`, via the same instruction sequence the narrow
    // Binary codegen emits for that operator.
    void finishCompoundOp(const ir::Expr &e, const std::string &rhsCell);
    // Shared by CompoundAssign's indexed branch and
    // CompoundDerefAssign: the target address must already be pushed
    // on the stack by the caller, before e.rhs is evaluated, so it
    // survives a nested call in rhs. Resolves that address once: loads
    // the old value, combines it with rhs via prepareCompoundRhs()/
    // finishCompoundOp(), stores the new value back through the same
    // address, leaving AC holding old or new per e.yieldOld.
    void genCompoundThroughStackedAddress(const ir::Expr &e);
    // The same, for a call-free e.rhs: the target address is in AC on
    // entry and is kept in a static temp instead.
    void genCompoundThroughCell(const ir::Expr &e);

    // A Global/Function gets a name-based label (never `id`), since
    // separate compilation gives caller and definer independent `id`
    // sequences but the same source name. A Local/Param gets no label
    // - every reference is frame-pointer-relative (emitLoadSlotAddress()),
    // computed fresh per call, so overlapping activations never share
    // storage.
    static std::string label(const ir::Slot *slot) { return "G_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "FN_" + fn->sourceName; }

    std::string allocTemp();
    void declareTemp(const std::string &label);
    void declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string literalLabel(long value);
    std::string newLabel();
    // Maps an ir::Stmt::labelId (switch's own Label/Goto - see IR.h) to this
    // backend's own label text, generated the first time either a Label
    // defining it or a Goto referencing it is seen.
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    // --- Software stack / frame mechanics --------------------------------
    //
    // `__sp` (top-of-stack address) and `__fp` (current frame pointer)
    // are fixed global cells declared in generate(). The stack grows
    // downward from a fixed high-memory base above where code/data
    // grows from `kPdp7BaseAddress`; the two regions only collide if
    // combined code+data and worst-case call depth exceed the 13-bit
    // direct-addressing window.
    //
    // pushAC()/popAC() each emit one `jms` to the runtime's `__push`/
    // `__pop` (see pdp7RuntimeAsm()), which move AC through whatever
    // address `__sp` holds, decrementing/incrementing `__sp` around it.
    // `__stmp` is their own momentary scratch, so any caller needing a
    // second value live across one of these must use `__stmp2`, never
    // `__stmp`.
    void pushAC();  // pushes AC's current value, leaving AC unchanged
    void popAC();   // pops the top of stack into AC

    // Once emitPrologue() has run: `__fp` points at the word holding
    // the caller's saved `__fp` ("oldFP"); the return address sits one
    // word above that, at `__fp+1`; the caller's pushed arguments sit
    // above that, in reverse push order, so param 0 is closest to
    // `__fp` - a param's address is `__fp+frameOffset` (the width-aware
    // offset IRBuilder.cpp's assignFrameOffsets() computes). Locals
    // live below `__fp`, in the `frameSize` words emitPrologue()
    // reserves in bulk; a Local's address is `__fp-localBaseOffset()`,
    // and a Local array's elements run upward from there, the same
    // direction as a Global array's.
    //
    // Every Local is reserved up front in emitPrologue(), not when its
    // LocalDecl statement runs, since that statement may sit inside a
    // conditional that doesn't execute on a given call; IRBuilder's
    // assignFrameOffsets() walks the whole function body unconditionally
    // for the same reason.
    void emitPrologue(const ir::Function *fn);
    // Shared by every `return` (explicit or implicit fall-through):
    // `hasValue` is whether AC holds this call's return value - if so
    // it's stashed in `__retval` before teardown and reloaded into AC
    // as the last step before the final indirect jump. Teardown resets
    // `__sp` to `__fp` (discarding this call's locals), pops oldFP into
    // `__fp` and the return address into `__retaddr`, then
    // "jmp __retaddr i" returns to the caller.
    void emitEpilogueAndReturn(bool hasValue, Type retType);

    // --- Leaf fast path ---------------------------------------------------
    //
    // A function that makes no call (`Call`/`CallPtr`) anywhere in its
    // body, and declares no Local/Param array, can never be re-entered
    // while running, so it's safe to skip the per-call frame and give
    // each Param/Local one fixed static storage cell instead - this
    // skips most of the frame's code-size cost per function.
    //
    // `currentFunctionIsLeaf_` is set once per function in
    // genFunction(); every method that treats a Local/Param
    // differently just reads it.
    bool currentFunctionIsLeaf_ = false;

    // A Local/Param's static cell when `currentFunctionIsLeaf_` - keyed
    // on `id` (unique within this file's IR) rather than `sourceName`,
    // since nothing outside this function ever references one by name.
    static std::string leafLabel(const ir::Slot *slot) { return "LF" + std::to_string(slot->id); }

    // True iff `fn`/`body` qualify for the leaf fast path above.
    static bool computeIsLeaf(const ir::Function *fn, const ir::Stmt *body);

    // The leaf equivalent of emitPrologue(): Params were pushed by the
    // caller in reverse order, so Param 0 is on top; popping them off
    // in declaration order both reads each one and reclaims its stack
    // slot (a leaf function has no stacked Locals, so there's nothing
    // left for emitLeafReturn() to tear down).
    void emitLeafParamBinding(const ir::Function *fn);
    // The leaf equivalent of emitEpilogueAndReturn(): AC already holds
    // the return value, if any, and there's no frame to tear down -
    // just jump back through this function's entry cell.
    void emitLeafReturn(bool hasValue);

    // Leaves `slot`'s address in AC: a Global's fixed address (`law`)
    // or a Local/Param's frame-relative address computed from `__fp`
    // plus a constant offset - never a label.
    void emitLoadSlotAddress(const ir::Slot *slot);
    // Leaves `slot`'s current value in AC: a Global reads directly
    // (`lac`); a Local/Param goes through emitLoadSlotAddress() plus
    // one level of `i` indirection via the shared `__addr` scratch.
    void emitLoadSlot(const ir::Slot *slot);
    // Stores AC's current value into `slot`; leaves AC unchanged. A
    // Global stores directly; a Local/Param stashes the value in
    // `__stmp` first (computing the destination address needs AC),
    // then stores through it.
    void emitStoreSlot(const ir::Slot *slot);
    // AC = the base address to index `slot` from: `slot`'s own address
    // if it's a declared array, or `slot`'s value if it's a pointer
    // variable (see computeElementAddress()).
    void emitSlotBaseAddress(const ir::Slot *slot);
    // Stores a Local array's `arrayInit` values into its reserved
    // frame words when its LocalDecl statement runs (a Local's frame
    // slot is only reserved, not pre-filled, by the prologue).
    void emitLocalArrayInit(const ir::Slot *slot);
    // How far below `__fp` a Local's address (its lowest word) sits.
    long localBaseOffset(const ir::Slot *slot);
    // Precondition: the target address is on top of the stack and AC
    // holds the value to store. Pops that address and stores AC
    // through it (via `__addr`), leaving AC unchanged. Pushing the
    // address before evaluating the value is what makes this safe
    // even when that evaluation involves a nested call.
    void storeThroughStackedAddress();
    // Pops an address off the stack into the shared `__addr` scratch
    // cell and returns "__addr", the operand label (with a trailing
    // " i" for the indirection bit added by the caller).
    std::string popAddressIntoScratch();

    // Emits code computing &slot[idxExpr] and leaves it pushed on the
    // stack (not a fixed temp cell, so it survives a call during the
    // caller's own evaluation); used by both an Index read and an
    // indexed Assign. Uses `law` (slot's address, when slot is the
    // array itself) or `lac` (slot's value, when slot is a pointer
    // variable) plus one level of `i` indirection to dereference the
    // runtime-computed address.
    void computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr);

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: value" data-word lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;  // built by generate() - see Codegen.h's perFileAsm()

    std::unordered_map<long, std::string> literalCache_;
    int tempCounter_ = 0;
    int litCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    bool needDivMod_ = false;
    bool needUDivMod_ = false;
    bool needPushPop_ = false;

    // (continueLabel, breakLabel) for the innermost active loop.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

// The pdp7 runtime every program shares: the scratch and stack cells,
// and whichever of the stack push/pop and signed/unsigned divide
// routines are asked for. A whole program gets it in its trailer; under
// separate compilation the linker's entry object supplies one copy
// with everything.
std::string pdp7RuntimeAsm(bool pushPop, bool divMod, bool uDivMod);

}  // namespace rcc
