#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PDP-7 assembly (as.s / as7 / pdp-assembler syntax) for
// the IR built by IRBuilder (see IR.h: every name is already
// resolved to a Slot/Function by the time this runs, so this class
// is pure instruction selection - no symbol table of its own). See
// ../../../README.md for the exact instruction-level conventions
// this relies on (verified against the real pdp7 simulator, not
// just inferred from the opcode table): dac leaves AC unchanged,
// sza/sma/sad skip in the directions their names say, dac/lac move
// whole 18-bit words, and the testmul.s-style "mul" / "lacq" pair
// gives a direct hardware multiply.
//
// Every function gets a real per-call activation record on a
// software stack (see the fixed `__sp`/`__fp` cells this class
// declares in generate()): there is no hardware stack pointer and no
// indexed/displacement addressing mode on this target at all, only
// one accumulator plus direct/indirect memory addressing (a "law"
// instruction and one "i" indirection bit), so the whole frame
// mechanism - pushing/popping through `__sp`, the frame pointer
// `__fp`, where a Local/Param physically lives, and even where a
// function's own return address lives across a nested call - is
// built entirely out of that one indirection bit. See emitPrologue()/
// emitEpilogueAndReturn()/pushAC()/popAC()'s own comments for the
// exact layout and arithmetic, and emitLoadSlotAddress()'s comment
// for how a Slot::frameOffset (see IR.h) becomes a real address.
// Direct and mutual/indirect recursion both work: every activation's
// Param/Local storage, and every value an expression needs to
// survive across a nested call (e.g. the `n` in `n * fact(n - 1)`),
// lives in that call's own frame or its own stack slot, never in a
// cell permanently shared by every call the way a Global's storage
// still deliberately is.
//
// pdp7as truncates every label to 8 characters (Assembler.cpp's
// truncate8()), so every label here is a short synthetic
// "<letter><N>" name: "G_<name>" for a Global Slot, "FN_<name>" for
// an ir::Function, and "T<N>"/"N<N>"/"J<N>" (this class's own
// counters) for codegen-internal scratch cells, literal constants
// and jump targets not in the IR at all (pure implementation detail
// of this backend's own expression-evaluation strategy) - plus a
// small fixed set of "__"-prefixed global cells (`__sp`, `__fp`,
// `__stmp`, `__stmp2`, `__addr`, `__retval`, `__retaddr`) this frame
// mechanism itself needs, shared by every function (safe to share:
// each one's live range is always a single short, call-free
// instruction sequence - see pushAC()/popAC()'s own comments). A
// Local or Param never gets a label at all any more - see
// emitLoadSlotAddress().
class Pdp7Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp7"; }
    TypeWidths typeWidths() const override { return {1, 1, 1, 2, 4}; }
    std::string sourceExtension() const override { return ".s"; }
    std::vector<std::string> assembledFormats() const override { return {"a7out", "list", "ptr", "rim", "coff"}; }
    std::string defaultAssemblerPath() const override { return "../pdp-assembler/build/bin/pdp7as"; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
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
    // Tests the signed less-than of two already-materialized cells,
    // jumping to `Ltrue`/`Lfalse` accordingly. Deliberately NOT a
    // plain "subtract then test sign" (`sma` alone) - that overflows
    // whenever A and B have opposite signs and are far enough apart
    // (e.g. the most-negative vs. most-positive 18-bit value), which
    // the unsigned-via-sign-bit-flip trick elsewhere in this file
    // routinely produces. Differing signs are decided directly, with
    // no subtraction at all; same-sign operands fall through to the
    // subtract-and-test-sign path, which is safe there since two
    // same-sign 18-bit values can never differ by more than the
    // representable range.
    void emitSignedLtCells(const std::string &A, const std::string &B, const std::string &Ltrue,
                            const std::string &Lfalse);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);

    // Masks AC down to `to`'s own real value range and (for a signed
    // type) re-sign-extends from that range's own top bit - see the
    // .cpp's own comment on why only `char` ever needs this.
    void narrowAC(Type to);
    int widthOfType(Type t) const { return pcc::widthOf(t, typeWidths()); }
    // `char`/`short`/`int` are all exactly 1 pdp7 word (see
    // typeWidths()) - only `long` (2 words) and `long long` (4
    // words) are ever "wide" on this backend.
    bool isWide(Type t) const { return widthOfType(t) > widthOfType(Type{BaseType::Int, false}); }

    // --- `long`/`long long` (wide) support --------------------------------
    //
    // A wide value is represented as `widthOfType(type)` consecutive
    // pdp7 words - word 0 (the low-order word) first, each
    // subsequent word one order higher - the exact same "element i"
    // addressing arithmetic a Global/Local array's own element i
    // already uses (see emitLoadSlotAddress()'s/declareArray()'s own
    // comments), since a wide scalar is really just a tiny fixed-size
    // array under the hood here. genExpr()'s own single-AC contract
    // doesn't generalize to more than one word at once on a machine
    // with exactly one accumulator and no general registers, so a
    // wide value is instead left PUSHED on the real software stack
    // (see pushAC()/popAC()) when produced, word 0 (low) on TOP (most
    // recently pushed) - every genExpr()/genStmt() call site checks
    // `isWide(e.type)` and branches to the matching wide helper
    // below instead of the plain single-AC path.
    //
    // Multiply and divide/modulo are NOT implemented for a wide
    // operand - see genWideMulDivErr()'s own comment for why.

    // Pushes a wide value's own `widthOfType(type)` words, reading
    // them from consecutive memory starting at `baseAddr` (already in
    // AC on entry - typically from emitLoadSlotAddress()) - word 0
    // (low) is read and pushed LAST, so it ends up on top, per this
    // file's own "word 0 on top" convention. `baseAddr` is left in
    // `__addr` throughout (never relies on AC surviving the pushes).
    void pushWideFromAddress(Type type);
    // The reverse: pops a wide value's own words off the stack
    // (word 0/low first, since it's on top) into consecutive memory
    // starting at the address already in AC on entry (left in
    // `__addr`).
    void popWideToAddress(Type type);
    // Pushes `widthOfType(type)` words representing the wide
    // sign/zero-extension of `value` (a plain, already-evaluated
    // narrow literal) - word 0 (low) pushed last.
    void pushWideLiteral(Type type, long value);
    // Generalizes genExpr() for a wide-typed expression: leaves its
    // `widthOfType(e.type)` words pushed on the stack (word 0/low on
    // top), instead of in AC. Every expression kind this language's
    // own type-checking pass (IRBuilder.cpp) can ever give a wide
    // type is handled directly here (Var, IntLit, Assign, Convert,
    // Unary -/~, and the wide arithmetic/bitwise binary operators);
    // anything else reaching here is a compile-time error.
    void genWideExpr(const ir::Expr &e);
    // Pops two wide operands (rhs word 0 on top, then lhs word 0 on
    // top beneath it - i.e. evaluated and pushed lhs-then-rhs, same
    // left-to-right order every narrow binary op here already uses)
    // and pushes a `wordCount`-word wide +/- result, carry/borrow
    // ripple-chained via the real link bit (`cll`/`tad`/`snl` - see
    // this file's own .cpp comment on why each carry-detecting `tad`
    // needs its own fresh `cll` immediately before it, not a shared
    // one across the whole chain).
    void genWideAddSub(bool isAdd, int wordCount);
    // Pops one wide operand and pushes its 2's-complement negation
    // (`~x + 1`, word-by-word, reusing genWideAddSub()'s own carry
    // chain against an all-zero "0" operand pushed first).
    void genWideNegate(int wordCount);
    // Pops two wide operands and pushes their bitwise and/xor (word-
    // by-word, each word independent - no carry chain needed) or or
    // (De Morgan, same `~a & ~b` identity the narrow case already
    // uses - see genBitwise()'s own comment).
    void genWideBitwise(const std::string &op, int wordCount);
    // Pops two wide operands and leaves a plain narrow 0/1 result in
    // AC - compares from the most-significant word down, short-
    // circuiting on the first word where the two operands differ
    // (same "compare from the top" idea as a real multi-digit
    // decimal comparison): the sign only matters in the top word
    // (`isUnsigned` picks whether that one word's own comparison
    // treats its sign bit as a real sign or just another value bit,
    // via the same sign-bit-flip trick emitLt()'s unsigned path
    // uses), every lower word's own comparison is always unsigned
    // (once the higher words are confirmed equal, the remaining
    // value is a plain non-negative magnitude).
    void genWideCompare(const std::string &op, int wordCount, bool isUnsigned);
    [[noreturn]] void genWideMulDivErr(int line, const std::string &opName);
    // dest = a + b + (the current 0/1 content of `carryCell`),
    // leaving `carryCell` updated to this word's own carry-out -
    // shared by genWideAddSub() (the ripple-carry chain itself, and
    // subtraction's own "negate rhs first" step) and genWideNegate().
    // `carry1Cell`/`carry2Cell` are this helper's own private scratch
    // (its two-stage carry capture - see the .cpp's own comment on
    // why one `tad` can't directly capture "carry from a+b+carryIn"
    // in a single step here) - never read by the caller.
    void emitWideAddWithCarry(const std::string &dest, const std::string &a, const std::string &b,
                               const std::string &carryCell, const std::string &carry1Cell,
                               const std::string &carry2Cell);

    // Branch fusion for an `If`/`While` condition: jumps to `Lfalse`
    // when `cond` is false, otherwise falls straight through into
    // whatever code follows (the true-branch's own, placed right
    // after the call site - same fall-through convention genStmt's
    // `If`/`While` cases already use for the non-fused case). For a
    // plain comparison (`==`/`!=`/`</>/<=`/`>=`) or an `&&` of such,
    // this skips genCompare()'s own "materialize a real 0/1 value,
    // then test it" dance entirely (the generic path any OTHER
    // condition shape - `||`, a bare variable, a call, bitwise,
    // ... - still goes through below) - that materialize-then-test
    // round trip costs ~7 words beyond the comparison itself, pure
    // overhead when the only thing the caller ever does with the
    // 0/1 value is immediately re-test it. Only uses instructions
    // this file's own other methods already rely on and have
    // verified against the real pdp7 simulator (`sma`/`sad` - see
    // emitLt()'s/emitEq()'s own comments); deliberately does NOT use
    // `spa` (`>=`/`<=`'s otherwise-natural complement to `sma`),
    // since nothing in this file uses or has verified it yet - see
    // this method's own .cpp comment for the safe fallback that
    // avoids it.
    void genCondBranch(const ir::Expr &cond, const std::string &Lfalse);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void ensureDivModRoutine();
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genIndex(const ir::Expr &e);       // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]
    void genConvert(const ir::Expr &e);

    // --- Compound assignment ('+=' etc., prefix/postfix '++'/'--') and
    // ternary -------------------------------------------------------------
    //
    // `CompoundAssign`/`CompoundDerefAssign` (see IR.h) can never be
    // wide here - IRBuilder.cpp's own rejectIfWide() rejects a wide
    // target before this backend ever sees one - so these only ever
    // need to produce a single AC-full result, the same narrow
    // contract every other case in genExpr()'s own switch already has.
    //
    // Consumes e.rhs's own value, already evaluated into AC by the
    // caller, and leaves it wherever this file's own existing narrow
    // Binary codegen for e.op (genBinary/genBitwise/genShift/genMul/
    // genDivMod) already stashes its own second operand before
    // combining it with the first - the exact same per-operator cell
    // each of those already uses (`__stmp2` for `+`/`-`/`&`/`^`/`|`, a
    // fresh cell doubling as the `mul` instruction's own inline
    // operand word for `*` - see genMul()'s own comment on that
    // convention, `__dm_b` for `/`/`%`, a fresh cell for `<<`/`>>`'s
    // own shift count) - `+`/`-`/`|` pre-apply their own negation/
    // complement here, exactly where genBinary()/genBitwise() already
    // do it, so finishCompoundOp() below never needs to know which
    // operator it's finishing beyond its own final combining step.
    // Returns that cell's own label, for finishCompoundOp() to read
    // back once the target's own old value is available in AC.
    std::string prepareCompoundRhs(const ir::Expr &e);
    // AC must already hold the target's own OLD value on entry; leaves
    // AC holding `old <e.op> rhs` afterward, using the exact same per-
    // operator instruction sequence this file's own narrow Binary
    // codegen already emits for that operator (see prepareCompoundRhs()'s
    // own comment for which cell each operator reads `rhsCell` as).
    void finishCompoundOp(const ir::Expr &e, const std::string &rhsCell);
    // Shared by CompoundAssign's own indexed branch (slot[idx] <op>=
    // rhs) and CompoundDerefAssign: the target's own address must
    // already be pushed on top of the real stack by the caller -
    // computeElementAddress()'s own pushed element address, or, for
    // CompoundDerefAssign, the pointer value itself pushed the same
    // way DerefAssign's own case already does - BEFORE this runs, so
    // it survives whatever e.rhs's own evaluation might do (including
    // a nested call); see storeThroughStackedAddress()'s own comment
    // on why pushing the address first, rather than computing it
    // fresh after, is what makes this safe against double-evaluating
    // an indexed/pointer target's own sub-expression. Resolves that
    // one address exactly once: loads the old value through it,
    // combines it with rhs via prepareCompoundRhs()/finishCompoundOp(),
    // and stores the new value back through the SAME address - never
    // recomputed - leaving AC holding whichever of the two (old or
    // new) e.yieldOld selects.
    void genCompoundThroughStackedAddress(const ir::Expr &e);

    // A Global gets a *name*-based label (never `id`) - real separate
    // compilation means the file calling a function and the file
    // defining it compile with completely independent `id`
    // sequences, so only a label computable from stable information
    // (a global's own source name, already unique program-wide) can
    // mean the same thing on both sides. A Function is the same idea,
    // keyed on its own source name. A Local or Param gets no label at
    // all any more - every reference to one is frame-pointer-relative
    // (see emitLoadSlotAddress()), computed fresh at every call, so
    // two overlapping activations of the same function (direct or
    // mutual recursion) never share storage the way they would with a
    // single fixed cell.
    static std::string label(const ir::Slot *slot) { return "G_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "FN_" + fn->sourceName; }

    std::string allocTemp();
    void declareTemp(const std::string &label);
    void declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string literalLabel(long value);
    std::string newLabel();

    // --- Software stack / frame mechanics --------------------------------
    //
    // `__sp` (current top-of-stack address) and `__fp` (current
    // function's own frame pointer) are the two fixed global cells
    // (declared in generate(), alongside `__one`/`__zero`) everything
    // below is built from. The stack grows downward (toward lower
    // addresses) from a fixed high-memory base picked well above
    // where this backend's own code+data naturally grows from
    // `kPdp7BaseAddress` (see generate()'s own `__sp` initializer) -
    // the two regions only collide if a program's combined code+data
    // and worst-case call depth together exceed the 13-bit direct-
    // addressing window this target's own "law"/memory-reference
    // instructions are limited to, exactly the same kind of real,
    // accepted hardware ceiling every other static allocation on this
    // target already has.
    //
    // pushAC()/popAC() are this backend's own emulated push/pop:
    // "dac __sp i"/"lac __sp i" (store/load through whatever address
    // `__sp` currently holds - the same one-level indirection
    // computeElementAddress's array indexing already relies on) do
    // the actual memory access, decrementing/incrementing `__sp`
    // itself around it. `__stmp` is purely this pair's own momentary
    // scratch (stashing the value being pushed/popped while `__sp`'s
    // own arithmetic runs through AC) - safe to share globally since
    // a push or pop is always one short, self-contained, call-free
    // instruction sequence; nothing else's state is ever live through
    // the middle of one. Because these two already own `__stmp` for
    // that, any caller that itself needs to hold a second value across
    // its own call to one of these (e.g. genBinary's own rhs, held
    // across popAC() to retrieve lhs) uses the separate `__stmp2`
    // cell instead, never `__stmp` - confusing the two silently
    // overwrites the caller's own value with pushAC()/popAC()'s
    // internal one.
    void pushAC();  // pushes AC's current value; AC is left unspecified
    void popAC();   // pops the top of stack into AC

    // A function's own activation record, once its prologue
    // (emitPrologue()) has run: `__fp` points at the word holding the
    // CALLER's own saved `__fp` ("oldFP"); the return address `jms`
    // wrote into this function's own entry cell (curFunctionLabel_)
    // sits one word above that, at `__fp+1` (pushed by the prologue
    // itself, before oldFP); the caller's own pushed arguments sit
    // above that, in REVERSE push order (see the ir::ExprKind::Call
    // case in genExpr, which pushes a wide argument's own
    // `widthOfType` words instead of one) so that param 0 ends up
    // closest to `__fp` - a param's own address is always
    // `__fp+frameOffset` (see emitLoadSlotAddress()), the same
    // width-aware offset IRBuilder.cpp's own assignFrameOffsets()
    // already computes (2*intWidth for param 0, advancing by each
    // param's own real word count after that - not a fixed "+2+k",
    // which only ever worked while every param was exactly one word).
    // Locals live below `__fp`, in the
    // `frameSize` words emitPrologue() reserves by decrementing `__sp`
    // once, in bulk, right after capturing `__fp` - Slot::frameOffset
    // (see IR.h) is already a 1-based sequential word index in
    // declaration order, so a Local's own address is `__fp-frameOffset`;
    // a Local array's element i therefore sits at
    // `__fp-(frameOffset+i)` - ONE LOWER (not higher) in address as i
    // increases, the opposite of a Global array's own normal
    // increasing-address layout (see emitLocalArrayInit()'s own
    // comment - this sign is exactly the kind of thing that's silently
    // gotten backwards on other targets migrating locals onto a real
    // frame).
    //
    // Reserving every local this way up front, in emitPrologue(),
    // rather than only when a given LocalDecl statement actually runs,
    // matters because a LocalDecl can sit inside an `if`/`while` body
    // that might not execute on a given call - IRBuilder's own
    // frameOffset assignment (assignFrameOffsets()) already walks the
    // whole function body unconditionally for exactly this reason, so
    // every Local this function could ever declare always has a
    // stable, reserved address from the moment the function is
    // entered, regardless of which branches actually run.
    void emitPrologue(const ir::Function *fn);
    // Shared by every `return` (explicit or an implicit fall-through
    // off the end of the function body - see genFunction()):
    // `hasValue` is whether AC currently holds this call's own return
    // value (computed by the caller, immediately before this runs) -
    // if so, it's stashed in `__retval` before anything else here
    // touches AC, and reloaded into AC as the very last step, right
    // before the final indirect jump, so it survives the teardown
    // below intact. Teardown: `__sp` is reset straight to `__fp`
    // (discarding this call's own locals - and anything else still
    // below `__fp` - in one step, since nothing there is still needed),
    // then oldFP and the return address are popped back off (in that
    // order - see this comment's own frame layout above) into `__fp`
    // and `__retaddr` respectively, and finally "jmp __retaddr i"
    // - an indirect jump through whatever address `__retaddr` now
    // holds - returns to the caller.
    void emitEpilogueAndReturn(bool hasValue, Type retType);

    // --- Leaf fast path ---------------------------------------------------
    //
    // A function that makes NO call (`Call`/`CallPtr`) anywhere in its
    // own body, and declares no Local/Param array, can never be
    // re-entered while it's running (the only way a call's own
    // activation could ever be live twice at once is via some call
    // chain that reaches it again, and a function with zero calls
    // never starts one) - true even under real separate compilation,
    // with zero visibility into any other file's own call graph,
    // since this needs nothing beyond "does this one function's own
    // body ever call anything at all". Safe, then, to skip the real
    // per-call frame entirely and give each of its Params/Locals one
    // fixed, static storage cell instead - exactly this backend's old,
    // pre-recursion convention, for exactly the functions it was
    // always correct for. This matters a lot in practice, not just in
    // principle: the real software-stack frame (emitPrologue()/
    // emitEpilogueAndReturn(), ~35 words together) and every frame-
    // relative Local/Param access (emitLoadSlot()/emitStoreSlot() via
    // `__fp`, 4-6 words vs. a static cell's 1) this skips are most of
    // this backend's own code-size cost per function - confirmed
    // directly: enabling this dropped a real multi-file kernel link
    // (sys/icreat_test.c + sys/s1/s2/s4/s5/s6/s8.c) from well past the
    // pdp7's 8192-word limit to comfortably under it.
    //
    // `currentFunctionIsLeaf_` is set once, in genFunction(), for
    // whichever function is currently being generated - every other
    // method below that behaves differently for a Local/Param
    // (emitLoadSlotAddress()/emitLoadSlot()/emitStoreSlot(), the
    // `Return` case, genFunction()'s own default-return fallthrough)
    // just reads it, rather than re-deriving it.
    bool currentFunctionIsLeaf_ = false;

    // A Local/Param's own static cell when `currentFunctionIsLeaf_` -
    // `id` (unique within this one file's own IR, which is all a
    // Local/Param ever needs: nothing outside this function, let
    // alone another file, ever references one by name) rather than
    // `sourceName` the way a Global's label is (see label()'s own
    // comment on why a Global needs name-stability across files and a
    // Local never does).
    static std::string leafLabel(const ir::Slot *slot) { return "LF" + std::to_string(slot->id); }

    // True iff `fn`/`body` qualify for the leaf fast path above - see
    // that section's own comment for exactly what disqualifies one.
    static bool computeIsLeaf(const ir::Function *fn, const ir::Stmt *body);

    // The leaf equivalent of emitPrologue(): every Param was pushed by
    // the caller in REVERSE order (see the `Call` case in genExpr),
    // so Param 0 is on top of the stack right now, Param 1 next, and
    // so on - popping them off in declaration order both reads each
    // one AND fully reclaims its stack slot in the same step (a leaf
    // function has no stacked Locals to reclaim later the way
    // emitEpilogueAndReturn()'s bulk "__sp = __fp" does, since every
    // Local here is a static cell, not a stack slot, so there's
    // nothing left for emitLeafReturn() to tear down).
    void emitLeafParamBinding(const ir::Function *fn);
    // The leaf equivalent of emitEpilogueAndReturn(): AC already holds
    // the return value, if any (same contract as
    // emitEpilogueAndReturn()'s own `hasValue`), and there's no frame
    // to tear down at all - just jump back through this function's
    // own entry cell, exactly like this backend's old, pre-recursion
    // convention's bare `jmp i curFunctionLabel_`.
    void emitLeafReturn(bool hasValue);

    // Leaves `slot`'s own address in AC: a Global's fixed address
    // (`law`, unchanged from before) or a Local/Param's own frame-
    // relative address, computed fresh from `__fp` plus a compile-time
    // constant offset (see emitPrologue()'s own comment for exactly
    // which) - never a label, since neither has one any more.
    void emitLoadSlotAddress(const ir::Slot *slot);
    // Leaves `slot`'s own current *value* in AC: a Global reads
    // directly (`lac`); a Local/Param goes through emitLoadSlotAddress()
    // plus one level of `i` indirection (through the shared `__addr`
    // scratch cell - safe for the same reason `__stmp` is, see
    // pushAC()'s own comment: computing an address and immediately
    // dereferencing it is always one short, call-free sequence).
    void emitLoadSlot(const ir::Slot *slot);
    // Stores AC's current value into `slot`; leaves AC unchanged
    // afterward (matching a plain `dac`'s own "leaves AC unchanged"
    // contract, which every existing call site already relies on) - a
    // Global stores directly; a Local/Param stashes the value in
    // `__stmp` first (computing the destination address needs AC),
    // then stores through it the same way emitLoadSlot() reads.
    void emitStoreSlot(const ir::Slot *slot);
    // AC = the base address to index `slot` from: `slot`'s own address
    // (it's a declared array) or `slot`'s own *value* (it's a plain
    // variable holding a pointer) - see computeElementAddress()'s own
    // comment on why indexing a pointer parameter needs this
    // distinction.
    void emitSlotBaseAddress(const ir::Slot *slot);
    // Stores a Local array's own `arrayInit` values into its reserved
    // frame words at the point its LocalDecl statement actually runs
    // (unlike a Global array, whose initializer is just baked into
    // static data once - a Local's frame slot is only reserved, not
    // pre-filled, by the prologue, so this has to actually run, every
    // call, at the declaration's own point in program order, same as
    // a scalar Local's own initializer expression already does). Element
    // i's own address is `__fp-(slot->frameOffset+i)` - see this
    // class's own frame-layout comment above emitPrologue() for why
    // that's MINUS i, not plus.
    void emitLocalArrayInit(const ir::Slot *slot);
    // Precondition: the target address is on top of the stack (pushed
    // earlier - see e.g. ir::ExprKind::DerefAssign's own genExpr()
    // case) and AC holds the value to store. Pops that address and
    // stores AC's current value through it (via `__addr`), leaving AC
    // unchanged - same contract as emitStoreSlot(). Pushing the
    // address *before* evaluating the value (rather than computing it
    // fresh right before the store) is what makes this safe even when
    // evaluating the value involves a nested call: a stack slot, pushed
    // before that call and popped after, is never the OTHER, textually
    // identical call site's own address temp the way a single static
    // cell would be under recursion.
    void storeThroughStackedAddress();
    // Pops an address off the stack into the shared `__addr` scratch
    // cell and returns "__addr" - the label to use as that
    // instruction's own operand, with a trailing " i" for the
    // indirection bit (see e.g. genIndex()).
    std::string popAddressIntoScratch();

    // Emits code that computes &slot[idxExpr] and leaves it pushed on
    // top of the stack (not in a fixed temp cell - see
    // storeThroughStackedAddress()'s own comment on why a stack slot,
    // not a static one, is what makes this safe across a call that
    // might happen while the caller still needs this address, e.g. an
    // indexed Assign's own rhs); used by both an Index read and an
    // indexed Assign. Needs `law` (slot's address, when slot is
    // itself the array - see emitSlotBaseAddress()) or `lac` (slot's
    // *value*, when slot is a plain pointer variable instead) plus
    // one level of `i` indirection to dereference the final, runtime-
    // computed address, since a PDP-7 instruction's own address field
    // is fixed at assemble time.
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

    // (continueLabel, breakLabel) for the innermost active loop - see
    // genStmt's While/Break/Continue cases, and e.g.
    // I8086Codegen.h's own loopLabels_ for the same pattern on
    // another backend.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
