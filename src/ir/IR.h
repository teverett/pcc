#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../Type.h"

namespace pcc::ir {

// The IR is AST.h's tree shape with every name resolved: a Var/Index/
// Assign's source identifier becomes a direct Slot* (no more map
// lookup by string), and a Call's callee becomes a direct Function*
// (already arg-count-checked). Scoping, symbol resolution, and the
// "is this name an array or a pointer variable" distinction all
// happen exactly once, in IRBuilder - not independently in every
// backend. See ../README.md's top section for why that matters: it's
// exactly the kind of logic that silently diverged between the pdp7
// and 8086 backends before this existed (see git history/PRs for
// the array-vs-pointer-base indexing bug that prompted this).
//
// What's deliberately NOT here: backend-internal implementation
// temps (e.g. a cell to hold a binary operator's left operand while
// the right is evaluated). Those are an artifact of each backend's
// own register/stack strategy - pdp7 needs a named memory cell,
// 8086 can just push/pop - not a source-level concept, so backends
// still allocate and name those themselves.

enum class SlotKind { Global, Local, Param };

struct Slot;      // defined below - ArrayInitElem only ever needs a pointer to it
struct Function;  // defined below - ArrayInitElem/Slot only ever need a pointer to it

// One element of an array initializer - either a plain literal value,
// or a symbol reference (a Slot's or a Function's own address, the
// dispatch-table idiom, e.g. "int t[] = {f, g};") - exactly one of
// `slot`/`func` is set when `isSymbol`, matching whether the source
// name resolved to a variable or a function (see IRBuilder.cpp's
// resolveArrayInit, which resolves it the same way it resolves a bare
// "&name" expression).
struct ArrayInitElem {
    bool isSymbol = false;
    long value = 0;
    Slot *slot = nullptr;
    Function *func = nullptr;
};

// One storage location for one value (or, for an array, arraySize of
// them) - a global, a local, or a parameter. Every Slot in a Program
// has a unique `id`; `sourceName` is the original C identifier, kept
// only so a backend can build a readable label if it wants one (see
// e.g. the 8086 backend) - resolution itself never uses it again.
struct Slot {
    int id = 0;
    std::string sourceName;
    SlotKind kind = SlotKind::Local;
    Type type;                          // scalar type, or array element type
    int arraySize = 0;                 // 0 = scalar
    bool hasInit = false;               // scalar: literal initializer given
    long initValue = 0;                  // scalar initializer value
    std::vector<ArrayInitElem> arrayInit;  // array initializer, global or local - see ArrayInitElem

    // '@' addr from source (Global only - the grammar has no syntax
    // for it on a local/param, so IRBuilder never sets this for
    // those kinds). Pins the slot's storage to an exact address
    // instead of wherever the backend would otherwise lay it out -
    // see each backend's own comment at its use site for what
    // "address" means there. Both backends interpret it the same
    // general way, as an offset from their own natural starting
    // point, not a true machine address from 0 (pdp7: an offset from
    // `pdp7as`'s relocation-base symbol `..`, matching the real
    // kernel's own "orig+N" convention - see Pdp7Codegen.cpp's own
    // comment for why going lower doesn't work; 8086: a byte offset
    // from this backend's own ORG).
    bool hasAddress = false;
    long address = 0;

    // Which .c file this came from (Global only, same restriction as
    // hasAddress above) - purely for --dump-ir/--dump-asm to split
    // their output one file per input instead of one combined dump;
    // nothing in resolution or codegen itself ever branches on this.
    std::string sourceFile;

    // True for a global referenced but not defined in the file being
    // compiled - only possible when IRBuilder's "allow external" mode
    // is on (real separate compilation, `--format=coff` - see
    // IRBuilder.h). Created lazily on first reference, by name only;
    // `arraySize`/`hasInit`/etc. are meaningless for one of these -
    // a backend emits a bare reference to `sourceName`, same as any
    // other slot, and its own assembler is what turns "name not found
    // locally" into a relocation instead of a compile error.
    bool isExternal = false;

    // Param only: which function this is a parameter of, and at
    // which position - set once, by IRBuilder, when a function's
    // parameter list is registered. A backend's own label for a
    // *global* or *param* slot has to be a deterministic function of
    // only this information (never of `id`, which is just this
    // compile's own allocation order) - under real separate
    // compilation, the file defining a function and a file merely
    // *calling* it compile completely independently, each assigning
    // `id`s with no relationship to the other's; only a name-based
    // label (e.g. "the 2nd parameter of `betwen`") can be computed
    // identically by both sides. Globals need the same treatment for
    // the same reason - just keyed on `sourceName` alone, already
    // guaranteed unique program-wide. Locals need no such care: a
    // function's own locals are never visible to, or referenced by,
    // any other file.
    std::string ownerFunctionName;
    int paramIndex = -1;

    // Local or Param: this slot's own offset within its function's
    // frame, in the target's real storage units (bytes on every
    // byte-addressable backend) - assigned once, by IRBuilder, using
    // the active target's own TypeWidths (see Type.h), so it already
    // accounts for this slot's own type's width and natural alignment;
    // a backend reads it directly and no longer multiplies by its own
    // word size itself. A Local's frameOffset is counted from the
    // bottom of the locals region (the first local starts one native
    // `int`'s width down, leaving that reserved space exactly as every
    // backend's own prologue already expects); a Param's is counted
    // from the top of the same frame, past a fixed two-native-`int`-
    // widths reservation for whatever a backend's own call sequence
    // keeps there (saved frame pointer/return address) - see
    // Function::frameSize for a Local's own total. A local array's
    // frameOffset is its first element's offset; the array occupies
    // frameOffset..frameOffset+widthOf(type)*arraySize-1. Only the
    // *sign* (which direction from the frame pointer) is still
    // backend-specific - everything else here is already resolved.
    int frameOffset = 0;
};

struct Function {
    int id = 0;
    std::string sourceName;
    Type returnType;
    std::vector<Slot *> params;  // owned by Program::slots

    // '@' addr from source - see Slot::hasAddress above; pins the
    // function's entry point (not its whole body) to that address.
    bool hasAddress = false;
    long address = 0;

    // Which .c file defined this function - see Slot::sourceFile.
    std::string sourceFile;

    // True for a function called but not defined in the file being
    // compiled - see Slot::isExternal above; the call site's own
    // argument count is trusted (there's no other file's own
    // signature left to check against once compilation is really
    // separate), not validated against anything.
    bool isExternal = false;

    // Total storage units (bytes, on every byte-addressable backend)
    // of local-variable storage this function's own frame needs - the
    // highest Local Slot::frameOffset declared in this function's
    // body, plus that local's own size (widthOf(type)*arraySize, or
    // widthOf(type) for a scalar), minus the one-native-`int`-width
    // reservation every Local::frameOffset is already counted from
    // (see its own comment); 0 if it declares no locals at all. Params
    // aren't counted here - every backend puts them in the CALLER's
    // pushed argument space, never inside this function's own locals
    // region.
    int frameSize = 0;
};

// --- Expressions --------------------------------------------------------

enum class ExprKind {
    IntLit,
    Var,          // slot (scalar read) or slot used bare as &-operand's target
    Index,        // slot[lhs]
    Assign,       // slot = rhs, or (if lhs set) slot[lhs] = rhs
    DerefAssign,  // *lhs = rhs
    // slot = slot <op> rhs, or (if lhs set) slot[lhs] = slot[lhs] <op>
    // rhs - `rhs` arrives already converted/scaled (a pointer target's
    // step is pre-scaled by its pointee width, the same multiply
    // `buildPointerBinary` already does for ptr+int) so codegen never
    // needs to know the target's own type beyond its storage width.
    // `yieldOld` (true only for a postfix '++'/'--') picks which of the
    // two values - the one read before the op, or the one stored after
    // it - is left in the backend's normal result location; see
    // IRBuilder.cpp's own header comment on why this is a single
    // value-producing node rather than a `slot = slot op rhs` desugar
    // (the target's own index/pointer sub-expression is computed once,
    // and the pre-op value has to still be reachable for `yieldOld`).
    CompoundAssign,
    CompoundDerefAssign,  // *lhs = *lhs <op> rhs - same as CompoundAssign, deref target
    Unary,        // op in {"-", "!", "~", "*", "&"}; "*"/"&" operate on rhs
    Binary,       // op in {"+","-","*","/","%","<<",">>","==","!=","<","<=",">",">=","&","^","|","&&","||"}
    Call,
    // A bare function name used as "&"'s operand (e.g. "&dispatch") -
    // the function's own entry address, never a call. Never appears
    // anywhere else a Var could (there's no way to read, assign to,
    // or index a function) - see IRBuilder's buildAddressableOperand,
    // the only place that ever produces one. `func` holds the
    // Function; `slot` is unused.
    Func,
    // A call through a computed address rather than a literal
    // function name (e.g. "(table[i])()") - `lhs` is the callee
    // expression (evaluates to the target address); `args` is its
    // argument list, same as Call, except IRBuilder's buildExpr
    // always leaves it empty: there's no per-callee ABI slot a
    // backend could put an argument in until the callee is known,
    // which for this kind it never is at compile time - see
    // IRBuilder.cpp's own comment at the one place that builds this.
    CallPtr,
    // A source-level primitive with no portable C expression and no
    // per-target calling convention to speak of - just one bare
    // instruction, chosen per backend, with `op` naming which one
    // (currently only "ion"/"iof": enable/disable interrupts - pdp7
    // emits the literal instruction words `../src-assem/sys/sop.s`
    // defines them as, 8086 emits `sti`/`cli`). Recognized from a
    // `_ion()`/`_iof()` call by IRBuilder, which also enforces "no
    // arguments" - by the time a backend sees this kind, that's
    // already validated, same as every other ExprKind here.
    Intrinsic,
    // An implicit or explicit type conversion - `lhs` is the operand
    // (already typed by this same type-checking pass), `type` is the
    // target type. Used for every conversion IRBuilder's buildExpr
    // inserts (integer promotion, the usual arithmetic conversions,
    // assignment/return/call-argument conversion) and for lowering a
    // source-level cast `(type)expr` - a cast needs no separate IR kind
    // of its own, since it's exactly this.
    Convert,
    Ternary,  // lhs ? rhs : elseExpr - both branches already converted to this node's own `type`
};

struct Expr {
    ExprKind kind;
    int line = 0;

    long intValue = 0;  // IntLit
    Type type;             // this expression's own resolved type (every kind has one)
    bool yieldOld = false;  // CompoundAssign/CompoundDerefAssign: true only for postfix '++'/'--'
    Slot *slot = nullptr;  // Var / Index / Assign(CompoundAssign) target (never null for those kinds)
    Function *func = nullptr;  // Call / Func
    std::string op;            // Unary / Binary / Intrinsic ("ion" or "iof") / CompoundAssign(DerefAssign)

    std::unique_ptr<Expr> lhs;  // Binary lhs; Index/indexed-Assign(CompoundAssign) subscript;
                                  // DerefAssign/CompoundDerefAssign's pointer; CallPtr's callee
                                  // (evaluates to the target address); Convert's operand; Ternary's condition
    std::unique_ptr<Expr> rhs;  // Binary rhs; Assign/DerefAssign/CompoundAssign(DerefAssign) value;
                                  // Unary operand; Ternary's then-branch
    std::unique_ptr<Expr> elseExpr;  // Ternary's else-branch
    std::vector<std::unique_ptr<Expr>> args;  // Call arguments (CallPtr's is always empty - see ExprKind::CallPtr)
};

using ExprPtr = std::unique_ptr<Expr>;

// --- Statements -----------------------------------------------------------

enum class StmtKind { Block, If, While, Return, Break, Continue, LocalDecl, ExprStmt, Empty, Asm };

struct Stmt {
    StmtKind kind;
    int line = 0;

    std::vector<std::unique_ptr<Stmt>> body;  // Block
    ExprPtr cond;                               // If / While
    std::unique_ptr<Stmt> thenStmt, elseStmt;   // If
    std::unique_ptr<Stmt> loopBody;              // While
    // A for-loop's own post-statement, kept separate from loopBody -
    // see AST.h's Stmt::postStmt (same reasoning: `continue` needs to
    // run this before the condition is retested, without running the
    // rest of loopBody). Null for a real `while` (every backend's own
    // genStmt already needs to run this, if present, right where
    // `continue` lands - see each backend's own While case).
    std::unique_ptr<Stmt> postStmt;              // While (for-loop desugaring only)
    Slot *declSlot = nullptr;                     // LocalDecl
    ExprPtr initOrReturnOrExpr;                    // LocalDecl init / Return value / ExprStmt expr
    std::string asmText;                           // Asm: raw target text, emitted verbatim - see
                                                     // AST.h's Stmt::asmText; carried through unchanged,
                                                     // since there's nothing in it to resolve.
};

using StmtPtr = std::unique_ptr<Stmt>;

struct FunctionBody {
    Function *func = nullptr;  // owned by Program::functions
    StmtPtr body;
    int line = 0;
};

struct Program {
    std::vector<std::unique_ptr<Slot>> slots;         // owns every Global/Local/Param Slot
    std::vector<std::unique_ptr<Function>> functions;  // owns every Function (by id)
    std::vector<FunctionBody> functionBodies;           // one per function that has a body
};

}  // namespace pcc::ir
