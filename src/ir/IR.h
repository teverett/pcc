#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../Type.h"

namespace pcc::ir {

// The IR is AST.h's tree shape with every name resolved: a Var/Index/Assign's
// identifier becomes a direct Slot*, and a Call's callee a direct Function*
// (already arg-count-checked). All scoping/symbol resolution happens once
// here, in IRBuilder, not independently per backend. Backend-internal
// temporaries (e.g. operand-holding cells) are not represented here; each
// backend allocates its own.

enum class SlotKind { Global, Local, Param };

struct Slot;
struct Function;

// Array initializer element: a literal value, or a symbol's address (the
// dispatch-table idiom); when isSymbol, exactly one of slot/func is set,
// matching whether the name resolved to a variable or a function.
struct ArrayInitElem {
    bool isSymbol = false;
    long value = 0;
    Slot *slot = nullptr;
    Function *func = nullptr;
};

// One storage location for a scalar, or arraySize of them. `sourceName` is
// kept only for a backend to build a readable label; resolution never uses it.
struct Slot {
    int id = 0;
    std::string sourceName;
    SlotKind kind = SlotKind::Local;
    Type type;                          // scalar type, or array element type
    int arraySize = 0;                 // 0 = scalar
    bool hasInit = false;               // scalar: literal initializer given
    long initValue = 0;                  // scalar initializer value
    std::vector<ArrayInitElem> arrayInit;  // array initializer, global or local - see ArrayInitElem

    // '@' addr from source; Global only. Pins storage to an exact address,
    // interpreted by each backend as an offset from its own natural origin
    // (not a true address 0).
    bool hasAddress = false;
    long address = 0;

    // Originating .c file; Global only. Used by --dump-ir/--dump-asm to
    // split output per input file.
    std::string sourceFile;

    // True for a global referenced but not defined in this compile (separate
    // compilation, --format=coff). Created lazily by name on first reference;
    // other fields are meaningless, and the backend's assembler turns the
    // unresolved reference into a relocation.
    bool isExternal = false;

    // Param only. A backend's label for a global/param slot must be a
    // deterministic function of name + position, not of `id` (which is just
    // this compile's own allocation order) — separate compilation units
    // assign `id`s independently, so only a name-based label matches across
    // files.
    std::string ownerFunctionName;
    int paramIndex = -1;

    // Local/Param: byte offset within the function's frame, already scaled
    // by this slot's type width/alignment via the target's TypeWidths. Local
    // offsets count down from the locals region (below one reserved native
    // `int`); Param offsets count down from the top, past a two-native-`int`
    // reservation for the backend's own call sequence. Only the sign (which
    // direction from the frame pointer) is still backend-specific.
    int frameOffset = 0;
};

// How far below the frame pointer a Local's lowest address sits, in
// frameOffset's units. assignFrameOffsets() gives each Local the range
// [frameOffset, frameOffset + size) below one reserved native int, so its
// storage is the `size` units ending where a native int at frameOffset
// would end: an int-sized scalar sits exactly at fp - frameOffset, and an
// array's elements (or a wide value's units) ascend from this address the
// same way a Global's do.
inline int localLowOffset(const Slot &slot, const TypeWidths &widths) {
    int count = slot.arraySize > 0 ? slot.arraySize : 1;
    return slot.frameOffset + count * widthOf(slot.type, widths) - widths.intUnits;
}

// For a backend that lays a scalar wider than an int out downward from
// fp - frameOffset (its later words at frameOffset plus one int each),
// which also stays within the slot: the frame offset of the address it
// uses for `slot`, frameOffset for such a wide scalar and the lowest
// address (localLowOffset()) for every other Local.
inline int localOffsetDescendingWide(const Slot &slot, const TypeWidths &widths) {
    if (slot.arraySize == 0 && widthOf(slot.type, widths) > widths.intUnits) return slot.frameOffset;
    return localLowOffset(slot, widths);
}

struct Function {
    int id = 0;
    std::string sourceName;
    Type returnType;
    std::vector<Slot *> params;  // owned by Program::slots

    bool hasAddress = false;  // '@' addr: pins the entry point, not the whole body
    long address = 0;

    std::string sourceFile;  // see Slot::sourceFile

    // True for a function called but not defined here (see Slot::isExternal);
    // the call site's own argument count is trusted, not checked against
    // another file's signature.
    bool isExternal = false;

    // Local-variable frame size in bytes: highest Local frameOffset plus that
    // local's size, minus the reserved-int offset frameOffset is counted
    // from; 0 if no locals. Excludes params, which live in the caller's
    // pushed argument space.
    int frameSize = 0;

    // An external function declared with `extern` and a parameter list:
    // that list's types, which a call's arguments convert to as they
    // would for a function defined here.
    bool hasPrototype = false;
    std::vector<Type> prototypeParams;
};

// --- Expressions --------------------------------------------------------

enum class ExprKind {
    IntLit,
    Var,          // scalar read, or slot used bare as &-operand's target
    Index,        // slot[lhs]
    Assign,       // slot = rhs, or slot[lhs] = rhs if lhs set
    DerefAssign,  // *lhs = rhs
    // slot (op=) rhs, or slot[lhs] (op=) rhs. `rhs` arrives already
    // converted/scaled (pointer step pre-scaled by pointee width), so
    // codegen never inspects the target's type beyond its storage width.
    // `yieldOld` (postfix '++'/'--' only) selects pre- vs post-op value as
    // the result. Kept as one node rather than desugared to `slot = slot op
    // rhs` because the index/pointer sub-expression must be computed once
    // and the pre-op value must stay reachable for yieldOld.
    CompoundAssign,
    CompoundDerefAssign,  // *lhs = *lhs (op=) rhs
    Unary,        // op in {"-", "!", "~", "*", "&"}; "*"/"&" operate on rhs
    Binary,       // op in {"+","-","*","/","%","<<",">>","==","!=","<","<=",">",">=","&","^","|","&&","||"}
    Call,
    // A bare function name as "&"'s operand (its entry address, never a
    // call); `func` holds the Function, `slot` unused.
    Func,
    // A call through a computed address (`lhs`) rather than a literal
    // function name; `args` is always empty since there's no known callee
    // ABI to place arguments against at this point.
    CallPtr,
    // A single bare target instruction with no C-level calling convention,
    // named by `op` (currently only "ion"/"iof": enable/disable interrupts).
    Intrinsic,
    // Implicit or explicit type conversion: `lhs` is the operand, `type` the
    // target type. Covers promotions, usual arithmetic conversions, and a
    // source-level cast.
    Convert,
    Ternary,  // lhs ? rhs : elseExpr; both branches already converted to `type`
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
    // For-loop post-statement, kept separate from loopBody; see AST.h's
    // Stmt::postStmt. Null for a real `while`.
    std::unique_ptr<Stmt> postStmt;              // While (for-loop desugaring only)
    Slot *declSlot = nullptr;                     // LocalDecl
    ExprPtr initOrReturnOrExpr;                    // LocalDecl init / Return value / ExprStmt expr
    std::string asmText;                           // Asm: raw target text, carried through unchanged
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
