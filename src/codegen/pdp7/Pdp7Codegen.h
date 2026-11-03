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
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void ensureDivModRoutine();
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genIndex(const ir::Expr &e);       // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

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
    // case in genExpr) so that param 0 ends up closest to `__fp`, at
    // `__fp+2` - paramIndex k's own address is always `__fp+2+k` (see
    // emitLoadSlotAddress()). Locals live below `__fp`, in the
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
    void emitEpilogueAndReturn(bool hasValue);

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
};

}  // namespace pcc
