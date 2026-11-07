#include "IRBuilder.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace pcc {

namespace {

[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

std::string typeText(Type t) {
    std::string prefix = t.isUnsigned ? "unsigned " : "";
    std::string base;
    switch (t.base) {
        case BaseType::Char:
            base = "char";
            break;
        case BaseType::Short:
            base = "short";
            break;
        case BaseType::Int:
            base = "int";
            break;
        case BaseType::Long:
            base = "long";
            break;
        case BaseType::LongLong:
            base = "long long";
            break;
    }
    return prefix + base + std::string(t.ptrDepth, '*');
}

// Flags an explicit `(type)expr` cast that either narrows the value
// (the target's own real storage width, per this compile's active
// `widths`, is smaller than the source's) or keeps the same width
// but flips signedness (so the same bit pattern reads as a different
// value) - both silently reinterpret the value rather than convert
// it, the two classes GCC/Clang's own `-Wconversion`/`-Wsign-
// conversion` warn about. An implicit conversion (assignment, a
// `return`, a call argument) is NOT checked here - only an explicit
// cast, which is the one place a programmer is asserting "I know
// this is fine," worth a warning if it might not be.
void warnRiskyCast(Type from, Type to, int line, const TypeWidths &widths) {
    if (from == to) return;
    int fromWidth = widthOf(from, widths);
    int toWidth = widthOf(to, widths);
    if (toWidth < fromWidth) {
        std::cerr << "line " << line << ": warning: cast from '" << typeText(from) << "' to '" << typeText(to)
                   << "' narrows the value (" << fromWidth << " to " << toWidth << " byte(s)) and may lose data\n";
    } else if (toWidth == fromWidth && from.isUnsigned != to.isUnsigned) {
        std::cerr << "line " << line << ": warning: cast from '" << typeText(from) << "' to '" << typeText(to)
                   << "' changes signedness - a negative value will be reinterpreted\n";
    }
}

int alignUp(int value, int align) {
    if (align <= 1) return value;
    return ((value + align - 1) / align) * align;
}

// Assigns every Local Slot reached from `s` its own frameOffset, in
// real storage units (see IR.h's own comment on Slot::frameOffset),
// in declaration order, and returns the next unused offset - a
// LocalDecl can only ever appear as a Stmt (never nested inside an
// Expr, by this subset's own grammar), so walking Block/If/While/
// LocalDecl's own sub-statements is exhaustive; nothing else here can
// contain one. The caller starts `nextOffset` at widthOf(Int, widths)
// - one native int's worth of reserved space below the frame pointer,
// the same reservation every backend's own prologue already expects.
int assignFrameOffsets(ir::Stmt *s, int nextOffset, const TypeWidths &widths) {
    if (!s) return nextOffset;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &child : s->body) nextOffset = assignFrameOffsets(child.get(), nextOffset, widths);
            return nextOffset;
        case ir::StmtKind::If:
            nextOffset = assignFrameOffsets(s->thenStmt.get(), nextOffset, widths);
            return assignFrameOffsets(s->elseStmt.get(), nextOffset, widths);
        case ir::StmtKind::While:
            nextOffset = assignFrameOffsets(s->loopBody.get(), nextOffset, widths);
            return assignFrameOffsets(s->postStmt.get(), nextOffset, widths);
        case ir::StmtKind::LocalDecl: {
            int elemWidth = widthOf(s->declSlot->type, widths);
            nextOffset = alignUp(nextOffset, elemWidth);
            s->declSlot->frameOffset = nextOffset;
            int count = s->declSlot->arraySize > 0 ? s->declSlot->arraySize : 1;
            return nextOffset + elemWidth * count;
        }
        case ir::StmtKind::Return:
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::ExprStmt:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
            return nextOffset;
    }
    return nextOffset;
}

// Wraps `inner` in an ir::Convert to `target`, unless it's already
// exactly that type - see IR.h's own comment on ExprKind::Convert.
ir::ExprPtr wrapConvert(ir::ExprPtr inner, Type target, int line) {
    if (inner->type == target) return inner;
    auto conv = std::make_unique<ir::Expr>();
    conv->kind = ir::ExprKind::Convert;
    conv->line = line;
    conv->type = target;
    conv->lhs = std::move(inner);
    return conv;
}

// Integer promotion: a sub-Int-rank operand becomes signed Int before
// any unary/binary op - see Type.h's own header comment for why this
// (deliberately simplified) rule never needs a target's real widths.
ir::ExprPtr promote(ir::ExprPtr e, int line) {
    if (rank(e->type.base) < rank(BaseType::Int)) {
        return wrapConvert(std::move(e), Type{BaseType::Int, false}, line);
    }
    return e;
}

class Builder {
   public:
    Builder(bool allowExternal, const TypeWidths &widths) : allowExternal_(allowExternal), widths_(widths) {}

    ir::Program build(const Program &prog) {
        registerGlobalsAndFunctions(prog);
        for (const auto &fn : prog.functions) {
            if (!fn.body) continue;  // prototype only
            ir::Function *irFn = funcByName_[fn.name];

            pushScope();
            for (ir::Slot *p : irFn->params) declareLocal(p->sourceName, p, fn.line);
            currentReturnType_ = irFn->returnType;
            currentSourceFile_ = fn.sourceFile;
            ir::StmtPtr body = buildStmt(*fn.body);
            popScope();
            int reserved = widthOf(Type{BaseType::Int, false}, widths_);
            // Rounded up to the native int width (even when the last
            // local is itself narrower) so a backend's own `sub
            // sp,sp,frameSize`-style reservation always leaves the
            // stack pointer at the same alignment it started at -
            // several targets (e.g. ARM) fault, or silently
            // misbehave, on a misaligned word access otherwise.
            irFn->frameSize = alignUp(assignFrameOffsets(body.get(), reserved, widths_), reserved) - reserved;

            ir::FunctionBody fb;
            fb.func = irFn;
            fb.body = std::move(body);
            fb.line = fn.line;
            prog_.functionBodies.push_back(std::move(fb));
        }
        return std::move(prog_);
    }

   private:
    void registerGlobalsAndFunctions(const Program &prog) {
        for (const auto &g : prog.globals) {
            if (globalByName_.count(g.name)) err(g.line, "redeclaration of global '" + g.name + "'");
            ir::Slot *slot = allocSlot(ir::SlotKind::Global, g.name, g.arraySize);
            slot->type = g.type;
            // An 'extern' declaration registers the name's kind (data)
            // right here, in this same first pass, before any '&name'/
            // array-initializer element in this file gets resolved
            // below - see this function's own header comment on why
            // that ordering is exactly what makes '&name'/array-init
            // resolution sound again. It has no initializer of its
            // own (the grammar doesn't allow one) and isn't really
            // this file's own storage, same as the lazily-created kind
            // resolveExternalVar makes on a first unannounced use.
            if (g.isExternDecl) {
                slot->isExternal = true;
                slot->sourceFile = g.sourceFile;
                globalByName_[g.name] = slot;
                continue;
            }
            slot->hasInit = g.hasInit;
            slot->initValue = g.initValue;
            // arrayInit is resolved in a second pass below, once
            // every global/function name in this file is registered
            // - an element can forward-reference a function or
            // global declared later in the same file (see
            // resolveArrayInit).
            slot->hasAddress = g.hasAddress;
            slot->address = g.address;
            slot->sourceFile = g.sourceFile;
            globalByName_[g.name] = slot;
        }
        for (const auto &fn : prog.functions) {
            if (funcByName_.count(fn.name)) err(fn.line, "redefinition of function '" + fn.name + "'");
            auto irFn = std::make_unique<ir::Function>();
            irFn->id = nextFuncId_++;
            irFn->sourceName = fn.name;
            irFn->returnType = fn.returnType;
            irFn->sourceFile = fn.sourceFile;
            // Same reasoning as an 'extern' global above, for the
            // function side: registers this name as a FUNCTION, not
            // data, before any '&name'/array-init element referring
            // to it is resolved - no param slots to allocate (there's
            // no body to call it against here, and resolveFunc never
            // checks an external function's own argument count
            // anyway - see resolveFunc's own comment).
            if (fn.isExternDecl) {
                irFn->isExternal = true;
                funcByName_[fn.name] = irFn.get();
                prog_.functions.push_back(std::move(irFn));
                continue;
            }
            irFn->hasAddress = fn.hasAddress;
            irFn->address = fn.address;
            // Each param's own frameOffset, in real storage units,
            // past a fixed two-native-`int`-widths reservation for
            // whatever a backend's own call sequence keeps there
            // (saved frame pointer/return address) - see IR.h's own
            // comment on Slot::frameOffset. Unlike a Local (packed by
            // its own real width - see assignFrameOffsets), every
            // param reserves AT LEAST one native `int`'s width: the
            // caller can only ever push/pass a whole native word per
            // argument (there's no narrower "push a byte" on a target
            // whose calling convention pushes arguments at all), so a
            // narrower-than-int param still occupies a full slot,
            // same as C's own default-argument-promotion convention.
            // Only a WIDER type (`long`/`long long`, when wider than
            // the native int) takes more than one slot's worth.
            int intWidth = widthOf(Type{BaseType::Int, false}, widths_);
            int paramOffset = 2 * intWidth;
            for (size_t i = 0; i < fn.params.size(); i++) {
                ir::Slot *p = allocSlot(ir::SlotKind::Param, fn.params[i].name);
                p->type = fn.params[i].type;
                p->ownerFunctionName = fn.name;
                p->paramIndex = static_cast<int>(i);
                // No alignUp here, unlike a Local's own packing below:
                // a caller's own push sequence lays params back-to-
                // back with no gap at all (there's no hardware
                // alignment requirement on any of these stack-passed
                // calling conventions), so rounding up here would
                // claim space the caller never actually reserved.
                int w = std::max(widthOf(p->type, widths_), intWidth);
                p->frameOffset = paramOffset;
                paramOffset += w;
                irFn->params.push_back(p);
            }
            funcByName_[fn.name] = irFn.get();
            prog_.functions.push_back(std::move(irFn));
        }
        // An object file (allowExternal_) never needs its own `main`
        // - only a final, directly-runnable program does. If this
        // file happens to define one anyway, its own signature is
        // still checked, same as always.
        if (!allowExternal_ && !funcByName_.count("main")) throw std::runtime_error("no 'main' function defined");
        if (funcByName_.count("main") && !funcByName_["main"]->params.empty()) {
            throw std::runtime_error("'main' must take no parameters");
        }

        // Second pass: every global's own arrayInit, now that every
        // global/function name in this file is registered (so an
        // element can name one declared later in the same file).
        for (const auto &g : prog.globals) {
            globalByName_[g.name]->arrayInit = resolveArrayInit(g.arrayInit, g.line);
        }
    }

    // Resolves each AST-level ArrayInitElem to its IR form: a plain
    // literal passes through unchanged; a symbol element is resolved
    // exactly the same way buildAddressableOperand resolves a bare
    // "&name" expression - try a variable first (this scope's locals,
    // if any are active, then this file's globals, including any
    // 'extern' global declared in this same file), then a function
    // (same: including any 'extern' function declared here). A name
    // that's neither is a hard error, NOT a guess: resolveExternalFunc/
    // resolveExternalVar used to be tried here as a silent fallback,
    // but there is genuinely no way to tell a function address from a
    // data address apart for a name this file has no other
    // information about at all (no call site, no local definition) -
    // see this file's own header comment, and docs/using.md's
    // "extern declarations", for why the real fix is requiring the
    // file to say which one it means, not guessing either way. Shared
    // between a global's own initializer (no active scope) and a
    // LocalDecl's (scopes_ may have locals in it).
    std::vector<ir::ArrayInitElem> resolveArrayInit(const std::vector<ArrayInitElem> &elems, int line) {
        std::vector<ir::ArrayInitElem> out;
        out.reserve(elems.size());
        for (const auto &e : elems) {
            ir::ArrayInitElem ie;
            if (!e.isSymbol) {
                ie.value = e.value;
                out.push_back(ie);
                continue;
            }
            ie.isSymbol = true;
            if (ir::Slot *slot = resolveVarOrNull(e.symbolName)) {
                ie.slot = slot;
            } else if (auto fit = funcByName_.find(e.symbolName); fit != funcByName_.end()) {
                ie.func = fit->second;
            } else {
                err(line, "'" + e.symbolName +
                              "' in array initializer is neither defined nor declared in this file - say "
                              "whether it's a function or data defined elsewhere with 'extern <type> " +
                              e.symbolName + "(...);' or 'extern <type> " + e.symbolName + "[N];'");
            }
            out.push_back(ie);
        }
        return out;
    }

    // Looks up `name`, creating (and caching) an external one on a
    // miss - only when allowExternal_ is set; otherwise `nullptr`,
    // for the caller to turn into the usual "undefined" error. A
    // repeat reference to the same unresolved name reuses the same
    // external Function/Slot rather than creating a new one each time.
    ir::Function *resolveExternalFunc(const std::string &name) {
        if (!allowExternal_) return nullptr;
        auto it = funcByName_.find(name);
        if (it != funcByName_.end()) return it->second;
        auto irFn = std::make_unique<ir::Function>();
        irFn->id = nextFuncId_++;
        irFn->sourceName = name;
        irFn->isExternal = true;
        ir::Function *raw = irFn.get();
        funcByName_[name] = raw;
        prog_.functions.push_back(std::move(irFn));
        return raw;
    }

    ir::Slot *resolveExternalVar(const std::string &name) {
        if (!allowExternal_) return nullptr;
        auto it = globalByName_.find(name);
        if (it != globalByName_.end()) return it->second;
        ir::Slot *slot = allocSlot(ir::SlotKind::Global, name);
        slot->isExternal = true;
        globalByName_[name] = slot;
        return slot;
    }

    ir::Slot *allocSlot(ir::SlotKind kind, const std::string &name, int arraySize = 0) {
        auto slot = std::make_unique<ir::Slot>();
        slot->id = nextSlotId_++;
        slot->sourceName = name;
        slot->kind = kind;
        slot->arraySize = arraySize;
        ir::Slot *raw = slot.get();
        prog_.slots.push_back(std::move(slot));
        return raw;
    }

    void pushScope() { scopes_.emplace_back(); }
    void popScope() { scopes_.pop_back(); }

    void declareLocal(const std::string &sourceName, ir::Slot *slot, int line) {
        if (scopes_.empty()) err(line, "internal: no active scope for local declaration");
        if (scopes_.back().count(sourceName)) err(line, "redeclaration of '" + sourceName + "' in the same block");
        scopes_.back()[sourceName] = slot;
    }

    // Local scopes and this-file globals only - no external-var
    // auto-creation, no error on a miss. Used by resolveVar below and
    // by buildAddressableOperand, which has to tell "not a variable
    // at all" (worth trying as a function name) apart from "not
    // declared anywhere".
    ir::Slot *resolveVarOrNull(const std::string &name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        auto git = globalByName_.find(name);
        if (git != globalByName_.end()) return git->second;
        return nullptr;
    }

    ir::Slot *resolveVar(const std::string &name, int line) {
        if (ir::Slot *slot = resolveVarOrNull(name)) return slot;
        if (ir::Slot *ext = resolveExternalVar(name)) return ext;
        err(line, "undefined variable '" + name + "'");
    }

    ir::Function *resolveFunc(const std::string &name, size_t argc, int line) {
        auto it = funcByName_.find(name);
        if (it == funcByName_.end()) {
            if (ir::Function *ext = resolveExternalFunc(name)) return ext;
            err(line, "call to undefined function '" + name + "'");
        }
        ir::Function *fn = it->second;
        // An external function has no real parameter list to check
        // against (there's no other file's own signature left to see
        // once compilation is really separate) - the call site's own
        // argument count is trusted as-is.
        if (!fn->isExternal && fn->params.size() != argc) {
            err(line, "'" + name + "' expects " + std::to_string(fn->params.size()) + " argument(s), got " +
                          std::to_string(argc));
        }
        return fn;
    }

    // An external global lazily auto-created by resolveVar/resolveExternalVar
    // (a plain, unambiguous, non-'&' reference - never one explicitly
    // declared 'extern' in this file, which already states its real
    // arraySize up front, see registerGlobalsAndFunctions) starts out
    // shaped like a plain scalar (arraySize == 0): there's no
    // declaration in this file to learn its real shape from. Indexing it anywhere in this
    // file is conclusive proof it's actually array-shaped (only an
    // array can be indexed) - mark it so retroactively, since the
    // first reference might come before or after any particular
    // indexed one. The exact size is never needed outside the one
    // file that actually *defines* it (only that file's own trailer-
    // emitted storage cares); every other file only needs to know
    // "is this array-shaped at all", to pick the right addressing
    // mode (see Pdp7Codegen.cpp/I8086Codegen.cpp's own
    // computeElementAddress) - so an arbitrary nonzero placeholder is
    // enough. A bare `&name` (no subscript) never needs this: an
    // array's own address and its Slot's own address are the same
    // instruction either way, array-shaped or not.
    void markIndexedIfExternal(ir::Slot *slot) {
        if (slot->isExternal && slot->arraySize == 0) slot->arraySize = 1;
    }

    // `slot[idx]` on a plain pointer VARIABLE (arraySize==0, a real
    // pointer type - see Type.h's own header comment on
    // Type::ptrDepth) means exactly `*(slot+idx)`, never a real
    // array's own Index codegen: a backend's Index/genIndexAddr-style
    // code scales/widths its access by `slot->type` directly (not by
    // this expression's own `.type`), which already correctly means
    // "this array's own element type" for a real array - but for a
    // pointer variable `slot->type` instead names the POINTER's own
    // type, so reusing that same Index codegen would scale/width by
    // the pointer's own size instead of its pointee's. Lowering to a
    // real `*(slot+idx)` shape here - exactly the already-correct,
    // zero-backend-changes-needed pointer-arithmetic/dereference
    // machinery buildPointerBinary()/the Unary "*"/DerefAssign cases
    // below already provide - sidesteps that entirely, rather than
    // teaching every backend's own Index codegen a second, pointer-
    // variable-shaped way to compute the same thing. Returns the
    // scaled `slot+idx` pointer value itself (type: same as `slot`'s
    // own) - callers wrap it in a deref themselves, same as `&p[i]`
    // (which IS exactly `p+i`, nothing more) doesn't need to.
    ir::ExprPtr buildPointerIndexAddr(ir::Slot *slot, const Expr &idxExpr, int line) {
        auto ptrVar = std::make_unique<ir::Expr>();
        ptrVar->kind = ir::ExprKind::Var;
        ptrVar->line = line;
        ptrVar->slot = slot;
        ptrVar->type = slot->type;
        return buildPointerBinary("+", std::move(ptrVar), buildExpr(idxExpr), line);
    }

    // Resolves a Var/Index/function-name operand without the "bare
    // array" check below - used only for `&`, where a bare array
    // name is exactly the point (&arr means the array's own address),
    // and where a bare function name is also valid (&f means f's own
    // entry address, never a call - see IR.h's ExprKind::Func).
    //
    // A bare IDENT that isn't a variable in this file (its own
    // globals, or an 'extern' global declared here) is tried as a
    // function name next (this file's own, a call site elsewhere in
    // it that already registered it, or an 'extern' function declared
    // here). A name that's none of those is a hard error, NOT a
    // guess: see resolveArrayInit's own comment (this resolves a bare
    // "&name" the exact same way) for why falling back to
    // resolveExternalFunc/resolveExternalVar here used to silently
    // guess the wrong kind half the time, and why the real fix is
    // requiring an explicit 'extern' declaration instead.
    ir::ExprPtr buildAddressableOperand(const Expr &e) {
        auto out = std::make_unique<ir::Expr>();
        out->line = e.line;
        if (e.kind == ExprKind::Var) {
            if (ir::Slot *slot = resolveVarOrNull(e.name)) {
                out->kind = ir::ExprKind::Var;
                out->slot = slot;
                out->type = slot->type;
                return out;
            }
            auto fit = funcByName_.find(e.name);
            if (fit != funcByName_.end()) {
                out->kind = ir::ExprKind::Func;
                out->func = fit->second;
                return out;
            }
            err(e.line, "'" + e.name +
                            "' is neither defined nor declared in this file - say whether it's a function or "
                            "data defined elsewhere with 'extern <type> " +
                            e.name + "(...);' or 'extern <type> " + e.name + "[N];'");
        }
        if (e.kind == ExprKind::Index) {
            // Only ever reached for a REAL array (`&arr[i]`) - `&p[i]`
            // on a plain pointer variable is intercepted one level up,
            // in the Unary "&" case below, before this is ever called
            // (see its own comment on why: `&p[i]` is exactly `p+i`
            // itself, not a further address-of).
            out->kind = ir::ExprKind::Index;
            out->slot = resolveVar(e.name, e.line);
            out->type = out->slot->type;
            markIndexedIfExternal(out->slot);
            out->lhs = buildExpr(*e.lhs);
            return out;
        }
        err(e.line, "'&' requires a plain variable, array element, or function name, e.g. &x, &arr[i], or &f");
    }

    // Pointer arithmetic/comparison - called once at least one of
    // lhs/rhs is a real pointer (ptrDepth>0). Handles exactly the
    // operators real C defines on a pointer operand: ptr +/- int
    // (scaled by the pointee's own width - an unscaled add/sub of two
    // native words would walk `int` BYTES, not `int` ELEMENTS, now
    // that a pointer tracks a real pointee type instead of being a
    // plain untyped int - see Type.h's own header comment), ptr - ptr
    // of the same type (address difference, itself scaled down to an
    // element count), and every comparison operator (raw, unsigned,
    // native-word address compare - a pointer's own storage is always
    // exactly one native int's width, see Type.h's widthOf()).
    // Anything else involving a pointer operand is rejected, same as
    // real C.
    ir::ExprPtr buildPointerBinary(const std::string &op, ir::ExprPtr lhs, ir::ExprPtr rhs, int line) {
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
            Type word = Type{BaseType::Int, true};
            auto out = std::make_unique<ir::Expr>();
            out->kind = ir::ExprKind::Binary;
            out->line = line;
            out->op = op;
            out->type = Type{BaseType::Int, false};
            out->lhs = wrapConvert(std::move(lhs), word, line);
            out->rhs = wrapConvert(std::move(rhs), word, line);
            return out;
        }
        if (op != "+" && op != "-") {
            err(line, "'" + op + "' is not defined on a pointer operand");
        }

        bool lhsPtr = isPointer(lhs->type);
        bool rhsPtr = isPointer(rhs->type);

        if (lhsPtr && rhsPtr) {
            if (op != "-") err(line, "'+' between two pointers is invalid - did you mean '-'?");
            if (lhs->type != rhs->type) err(line, "'-' between pointers to different types");
            // Both the raw address difference and the division below
            // stay at a pointer's own real width (plain native `int` -
            // see Type.h's own header comment on a pointer's own
            // storage) rather than some wider result type: lhs/rhs are
            // themselves narrow pointer-typed operands, and tagging a
            // Binary node with a WIDER .type than its own children's
            // actual type would wrongly route a backend's wide
            // (`long`/`long long`) codegen onto them (that codegen
            // assumes every operand it's handed is already that same
            // wide type - see e.g. Pdp7Codegen.cpp's genWideExpr).
            int elemWidth = widthOf(pointeeOf(lhs->type), widths_);
            Type nativeInt = Type{BaseType::Int, false};
            auto diff = std::make_unique<ir::Expr>();
            diff->kind = ir::ExprKind::Binary;
            diff->line = line;
            diff->op = "-";
            diff->type = nativeInt;
            diff->lhs = std::move(lhs);
            diff->rhs = std::move(rhs);
            if (elemWidth == 1) return diff;  // the raw address difference is already the element count
            auto divisor = std::make_unique<ir::Expr>();
            divisor->kind = ir::ExprKind::IntLit;
            divisor->line = line;
            divisor->type = nativeInt;
            divisor->intValue = elemWidth;
            auto scaled = std::make_unique<ir::Expr>();
            scaled->kind = ir::ExprKind::Binary;
            scaled->line = line;
            scaled->op = "/";
            scaled->type = nativeInt;
            scaled->lhs = std::move(diff);
            scaled->rhs = std::move(divisor);
            return scaled;
        }

        if (rhsPtr && !lhsPtr) {
            if (op == "-") err(line, "cannot subtract a pointer from an int");
            std::swap(lhs, rhs);  // normalize: lhs is always the pointer from here on
        }
        Type ptrType = lhs->type;
        rhs = promote(std::move(rhs), line);
        rhs = scaleStepForPointer(ptrType, std::move(rhs), line);
        auto out = std::make_unique<ir::Expr>();
        out->kind = ir::ExprKind::Binary;
        out->line = line;
        out->op = op;
        out->type = ptrType;
        out->lhs = std::move(lhs);
        out->rhs = std::move(rhs);
        return out;
    }

    // A wide (`long`/`long long`) type here means wider than this
    // target's own native int - see Type.h's widthOf(). Used to reject
    // CompoundAssign/CompoundDerefAssign/Ternary on a wide target (see
    // their own call sites below): these three compute their "new
    // value" directly in a backend's normal native-width register the
    // same way an ordinary narrow Var/Index read/store already does
    // (sign/zero-extend on load, truncate on store - no per-operator
    // codegen beyond what already exists for that), but a wide target
    // would need a second, genuinely new multi-word instruction
    // sequence per operator per backend that compound assignment/
    // `++`/`--`/ternary on a `long long` doesn't currently justify -
    // see docs/pdp-c-compiler.md's own note on this.
    bool isWideType(Type t) const {
        return widthOf(t, widths_) > widthOf(Type{BaseType::Int, false}, widths_);
    }

    void rejectIfWide(Type t, int line, const char *what) {
        if (isWideType(t)) {
            err(line, std::string(what) +
                          " on a 'long'/'long long' target isn't supported yet - only char/short/int/"
                          "pointer (see docs/pdp-c-compiler.md's own note on this)");
        }
    }

    // Scales an already-promoted int-typed step by `ptrType`'s own
    // pointee width - the one-liner multiply ptr+int above needs, and
    // the exact same one CompoundAssign/CompoundDerefAssign's own
    // `p += n`/`p++`/`p--` lowering (further below) needs too, so it's
    // factored out rather than duplicated.
    ir::ExprPtr scaleStepForPointer(Type ptrType, ir::ExprPtr step, int line) {
        int elemWidth = widthOf(pointeeOf(ptrType), widths_);
        if (elemWidth == 1) return step;
        auto factor = std::make_unique<ir::Expr>();
        factor->kind = ir::ExprKind::IntLit;
        factor->line = line;
        factor->type = step->type;
        factor->intValue = elemWidth;
        auto scaled = std::make_unique<ir::Expr>();
        scaled->kind = ir::ExprKind::Binary;
        scaled->line = line;
        scaled->op = "*";
        scaled->type = step->type;
        scaled->lhs = std::move(step);
        scaled->rhs = std::move(factor);
        return scaled;
    }

    // Shared by CompoundAssign/CompoundDerefAssign below: builds the
    // already-ready-to-combine `rhs` operand for `target <op>= e.rhs`
    // (or, for '++'/'--', e.rhs is already IntLit(1) - see BuildAst.cpp's
    // own makeIncDec). For a pointer target with '+'/'-', this is the
    // step scaled by the pointee's width (stays plain native-int typed,
    // exactly like ptr+int's own rhs above - the backend does a single
    // native-word add/sub, same as it already does for Binary ptr+int).
    // Otherwise it's converted straight to `targetType` (NOT promoted
    // first): computing +,-,*,&,^,|,<<,>> directly at a sub-int target's
    // own width is bit-for-bit identical to promoting first and
    // truncating the result back afterward (both are closed under
    // truncation - shifts' low bits never depend on bits above the
    // original width either direction, and +/-/* are ordinary modular
    // arithmetic); only '/','%' can differ, and only when `targetType`
    // is narrower than the backend's native int - see each backend's own
    // CompoundAssign codegen comment for how that one case widens/
    // narrows around the divide using the same extend/truncate it
    // already has for ExprKind::Convert, rather than needing a genuine
    // narrow-width divide instruction.
    ir::ExprPtr buildCompoundRhs(Type targetType, const std::string &op, const Expr &rhsAst, int line) {
        if (isPointer(targetType) && (op == "+" || op == "-")) {
            ir::ExprPtr rhs = promote(buildExpr(rhsAst), line);
            return scaleStepForPointer(targetType, std::move(rhs), line);
        }
        return wrapConvert(buildExpr(rhsAst), targetType, line);
    }

    ir::ExprPtr buildExpr(const Expr &e) {
        auto out = std::make_unique<ir::Expr>();
        out->line = e.line;
        switch (e.kind) {
            case ExprKind::IntLit:
                out->kind = ir::ExprKind::IntLit;
                out->intValue = e.intValue;
                out->type = e.type;
                return out;
            case ExprKind::Var: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (slot->arraySize > 0) {
                    err(e.line, "array '" + e.name + "' used without a subscript - use " + e.name +
                                    "[i], or &" + e.name + " for its address");
                }
                out->kind = ir::ExprKind::Var;
                out->slot = slot;
                out->type = slot->type;
                return out;
            }
            case ExprKind::Index: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (slot->arraySize == 0 && isPointer(slot->type)) {
                    // `p[i]` is exactly `*(p+i)` for a plain pointer
                    // VARIABLE - see buildPointerIndexAddr()'s own
                    // comment on why this goes through the dereference
                    // machinery instead of a real array's own Index.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::Unary;
                    out->op = "*";
                    out->type = pointeeOf(sum->type);
                    out->rhs = std::move(sum);
                    return out;
                }
                out->kind = ir::ExprKind::Index;
                out->slot = slot;
                out->type = slot->type;
                markIndexedIfExternal(out->slot);
                out->lhs = buildExpr(*e.lhs);
                return out;
            }
            case ExprKind::Assign: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (e.lhs && slot->arraySize == 0 && isPointer(slot->type)) {
                    // `p[i] = rhs` is exactly `*(p+i) = rhs` - see
                    // buildPointerIndexAddr()'s own comment.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::DerefAssign;
                    Type pointee = pointeeOf(sum->type);
                    out->type = pointee;
                    out->lhs = std::move(sum);
                    out->rhs = wrapConvert(buildExpr(*e.rhs), pointee, e.line);
                    return out;
                }
                out->kind = ir::ExprKind::Assign;
                out->slot = slot;
                if (e.lhs) {
                    markIndexedIfExternal(out->slot);  // arr[i] = ... - e.lhs is the index
                    out->lhs = buildExpr(*e.lhs);
                }
                out->type = out->slot->type;
                out->rhs = wrapConvert(buildExpr(*e.rhs), out->type, e.line);
                return out;
            }
            case ExprKind::CompoundAssign: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (e.lhs && slot->arraySize == 0 && isPointer(slot->type)) {
                    // `p[i] op= rhs` (including `p[i]++`/`p[i]--`,
                    // built with op="+"/"-") is exactly
                    // `*(p+i) op= rhs` - see buildPointerIndexAddr()'s
                    // own comment.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::CompoundDerefAssign;
                    Type pointee = pointeeOf(sum->type);
                    out->type = pointee;
                    out->op = e.op;
                    out->yieldOld = e.yieldOld;
                    out->lhs = std::move(sum);
                    rejectIfWide(out->type, e.line, "compound assignment/'++'/'--'");
                    out->rhs = buildCompoundRhs(pointee, e.op, *e.rhs, e.line);
                    return out;
                }
                out->kind = ir::ExprKind::CompoundAssign;
                out->slot = slot;
                out->op = e.op;
                out->yieldOld = e.yieldOld;
                if (e.lhs) {
                    markIndexedIfExternal(out->slot);  // arr[i] op= ... - e.lhs is the index
                    out->lhs = buildExpr(*e.lhs);
                }
                out->type = out->slot->type;
                rejectIfWide(out->type, e.line, "compound assignment/'++'/'--'");
                out->rhs = buildCompoundRhs(out->type, e.op, *e.rhs, e.line);
                return out;
            }
            case ExprKind::CompoundDerefAssign: {
                out->kind = ir::ExprKind::CompoundDerefAssign;
                out->op = e.op;
                out->yieldOld = e.yieldOld;
                out->lhs = buildExpr(*e.lhs);
                Type pointee =
                    isPointer(out->lhs->type) ? pointeeOf(out->lhs->type) : Type{BaseType::Int, false};
                out->type = pointee;
                rejectIfWide(out->type, e.line, "compound assignment/'++'/'--'");
                out->rhs = buildCompoundRhs(pointee, e.op, *e.rhs, e.line);
                return out;
            }
            case ExprKind::DerefAssign: {
                out->kind = ir::ExprKind::DerefAssign;
                out->lhs = buildExpr(*e.lhs);
                if (isPointer(out->lhs->type)) {
                    // A real pointer (ptrDepth>0) stores through at
                    // its own pointee type/width - see every backend's
                    // own genExpr DerefAssign case, which now reads
                    // this type instead of always writing a full
                    // native word.
                    Type pointee = pointeeOf(out->lhs->type);
                    out->type = pointee;
                    out->rhs = wrapConvert(buildExpr(*e.rhs), pointee, e.line);
                } else {
                    // A plain int used as an untyped address (the
                    // original, still-supported style - see Type.h's
                    // own header comment) keeps writing a full native
                    // word, exactly as before real pointer types
                    // existed.
                    out->type = Type{BaseType::Int, false};
                    out->rhs = buildExpr(*e.rhs);
                }
                return out;
            }
            case ExprKind::Unary: {
                out->kind = ir::ExprKind::Unary;
                out->op = e.op;
                if (e.op == "&") {
                    // `&p[i]` on a plain pointer VARIABLE is exactly
                    // `p+i` - already a complete pointer VALUE, not
                    // something to take a further address of (unlike
                    // `&arr[i]`, a real array's own element address) -
                    // see buildPointerIndexAddr()'s own comment.
                    if (e.rhs->kind == ExprKind::Index) {
                        if (ir::Slot *slot = resolveVar(e.rhs->name, e.rhs->line);
                            slot->arraySize == 0 && isPointer(slot->type)) {
                            return buildPointerIndexAddr(slot, *e.rhs->lhs, e.line);
                        }
                    }
                    out->rhs = buildAddressableOperand(*e.rhs);
                    out->type = pointerTo(out->rhs->type);
                    return out;
                }
                ir::ExprPtr operand = buildExpr(*e.rhs);
                if (e.op == "*") {
                    // A real pointer dereferences at its own pointee
                    // type/width; a plain int used as an untyped
                    // address keeps reading a full native word,
                    // exactly as before real pointer types existed -
                    // see DerefAssign's own identical comment above.
                    out->type =
                        isPointer(operand->type) ? pointeeOf(operand->type) : Type{BaseType::Int, false};
                    out->rhs = std::move(operand);
                    return out;
                }
                if (e.op == "!") {
                    out->type = Type{BaseType::Int, false};
                    out->rhs = std::move(operand);
                    return out;
                }
                // '-' or '~': ordinary integer promotion.
                operand = promote(std::move(operand), e.line);
                out->type = operand->type;
                out->rhs = std::move(operand);
                return out;
            }
            case ExprKind::Binary: {
                ir::ExprPtr lhs = buildExpr(*e.lhs);
                ir::ExprPtr rhs = buildExpr(*e.rhs);
                if (e.op == "&&" || e.op == "||") {
                    // Truth-testing is width-independent - no
                    // conversion needed on either operand, pointer or
                    // not.
                    out->kind = ir::ExprKind::Binary;
                    out->op = e.op;
                    out->type = Type{BaseType::Int, false};
                    out->lhs = std::move(lhs);
                    out->rhs = std::move(rhs);
                    return out;
                }
                if (isPointer(lhs->type) || isPointer(rhs->type)) {
                    return buildPointerBinary(e.op, std::move(lhs), std::move(rhs), e.line);
                }
                out->kind = ir::ExprKind::Binary;
                out->op = e.op;
                if (e.op == "<<" || e.op == ">>") {
                    // The RHS promotes on its own for its own
                    // evaluation but never affects the result type -
                    // see Type.h's own header comment.
                    lhs = promote(std::move(lhs), e.line);
                    rhs = promote(std::move(rhs), e.line);
                    out->type = lhs->type;
                    out->lhs = std::move(lhs);
                    out->rhs = std::move(rhs);
                    return out;
                }
                lhs = promote(std::move(lhs), e.line);
                rhs = promote(std::move(rhs), e.line);
                bool isComparison = e.op == "==" || e.op == "!=" || e.op == "<" || e.op == "<=" || e.op == ">" ||
                                     e.op == ">=";
                // A comparison between two values already known at
                // compile time (most commonly two `sizeof`s, or a
                // `sizeof` against a literal - see IR.h's own
                // ExprKind::Sizeof comment: it's already an IntLit by
                // this point) never needs any actual comparison
                // *instruction* at all, wide or narrow - the C
                // standard already settles it during translation. The
                // result is always a plain narrow `int` 0/1 regardless
                // of how wide `lhs`/`rhs`'s own static type is (`long`/
                // `long long` for `sizeof`), so folding it away here
                // keeps that width from ever reaching a backend that
                // can't actually do wide arithmetic at all (see
                // MCS51Codegen.cpp's own requireNarrow()) purely to
                // decide something this compile-time comparison
                // already knows regardless of target.
                if (isComparison && lhs->kind == ir::ExprKind::IntLit && rhs->kind == ir::ExprKind::IntLit) {
                    long a = lhs->intValue, b = rhs->intValue;
                    long result = 0;
                    if (e.op == "==") result = (a == b) ? 1 : 0;
                    else if (e.op == "!=") result = (a != b) ? 1 : 0;
                    else if (e.op == "<") result = (a < b) ? 1 : 0;
                    else if (e.op == "<=") result = (a <= b) ? 1 : 0;
                    else if (e.op == ">") result = (a > b) ? 1 : 0;
                    else result = (a >= b) ? 1 : 0;
                    out->kind = ir::ExprKind::IntLit;
                    out->intValue = result;
                    out->type = Type{BaseType::Int, false};
                    return out;
                }
                Type common = commonType(lhs->type, rhs->type);
                lhs = wrapConvert(std::move(lhs), common, e.line);
                rhs = wrapConvert(std::move(rhs), common, e.line);
                out->lhs = std::move(lhs);
                out->rhs = std::move(rhs);
                if (isComparison) {
                    // The comparison itself happens at `common`
                    // (both operands now share it, so codegen reads
                    // either side's .type.isUnsigned to pick a signed
                    // or unsigned compare) but the result is always a
                    // plain int 0/1, same as real C.
                    out->type = Type{BaseType::Int, false};
                } else {
                    out->type = common;
                }
                return out;
            }
            case ExprKind::Ternary: {
                out->kind = ir::ExprKind::Ternary;
                out->lhs = buildExpr(*e.lhs);  // condition - width-independent, no conversion needed
                ir::ExprPtr thenE = buildExpr(*e.rhs);
                ir::ExprPtr elseE = buildExpr(*e.elseExpr);
                bool thenPtr = isPointer(thenE->type);
                bool elsePtr = isPointer(elseE->type);
                if (thenPtr || elsePtr) {
                    // Permissive, no-cross-checking pointer typing -
                    // same style as this file's existing pointer/
                    // untyped-int interop elsewhere (see Type.h's own
                    // header comment): the branch that IS a pointer
                    // decides the result type; the other branch is only
                    // converted if it's also a pointer (matching depth),
                    // otherwise left as-is.
                    out->type = thenPtr ? thenE->type : elseE->type;
                    if (thenPtr && elsePtr) elseE = wrapConvert(std::move(elseE), out->type, e.line);
                    else if (!thenPtr && elsePtr) thenE = wrapConvert(std::move(thenE), out->type, e.line);
                } else {
                    thenE = promote(std::move(thenE), e.line);
                    elseE = promote(std::move(elseE), e.line);
                    out->type = commonType(thenE->type, elseE->type);
                    thenE = wrapConvert(std::move(thenE), out->type, e.line);
                    elseE = wrapConvert(std::move(elseE), out->type, e.line);
                }
                rejectIfWide(out->type, e.line, "the ternary operator");
                out->rhs = std::move(thenE);
                out->elseExpr = std::move(elseE);
                return out;
            }
            case ExprKind::Call: {
                if (e.name == "_ion" || e.name == "_iof") {
                    if (!e.args.empty()) err(e.line, e.name + "() takes no arguments");
                    out->kind = ir::ExprKind::Intrinsic;
                    out->op = e.name.substr(1);  // "ion" or "iof"
                    out->type = Type{BaseType::Int, false};
                    return out;
                }
                out->kind = ir::ExprKind::Call;
                out->func = resolveFunc(e.name, e.args.size(), e.line);
                for (size_t i = 0; i < e.args.size(); i++) {
                    ir::ExprPtr arg = buildExpr(*e.args[i]);
                    // An external function has no real parameter
                    // list in this file to convert against - see
                    // resolveFunc's own comment; its argument's
                    // built-in type passes through unchanged.
                    if (!out->func->isExternal) {
                        arg = wrapConvert(std::move(arg), out->func->params[i]->type, e.line);
                    }
                    out->args.push_back(std::move(arg));
                }
                out->type = out->func->isExternal ? Type{BaseType::Int, false} : out->func->returnType;
                return out;
            }
            case ExprKind::CallPtr:
                // No per-callee ABI slot to put an argument in until
                // the callee is known - which, by definition, this
                // kind never is at compile time (see IR.h's
                // ExprKind::CallPtr) - so, unlike Call above, this
                // subset only supports a bare (expr)() with no
                // arguments at all. There's likewise no callee
                // signature to learn a real return type from here;
                // the result stays plain int, same as today.
                if (!e.args.empty()) {
                    err(e.line, "a call through a computed function address ((expr)(...)) can't take "
                                 "arguments - there's no per-callee calling-convention slot to put one in "
                                 "until the callee is known, which it isn't at compile time; only a "
                                 "0-argument (expr)() is supported");
                }
                out->kind = ir::ExprKind::CallPtr;
                out->type = Type{BaseType::Int, false};
                out->lhs = buildExpr(*e.lhs);
                return out;
            case ExprKind::Cast: {
                ir::ExprPtr inner = buildExpr(*e.rhs);
                warnRiskyCast(inner->type, e.type, e.line, widths_);
                return wrapConvert(std::move(inner), e.type, e.line);
            }
            case ExprKind::Sizeof: {
                // sizeof(expr) never evaluates expr - it's built here
                // purely to learn its static type (still raising any
                // unrelated semantic error, e.g. an undefined
                // variable, exactly as real C requires even though it
                // never runs) and then discarded; only its `.type` is
                // kept. See Type.h's own header comment on why no
                // width table is needed anywhere else in this pass -
                // this is the one place it is.
                Type measured = e.sizeofIsType ? e.type : buildExpr(*e.rhs)->type;
                out->kind = ir::ExprKind::IntLit;
                out->type = Type{BaseType::Long, true};
                out->intValue = widthOf(measured, widths_);
                return out;
            }
            case ExprKind::StringLit: {
                // Lowers to exactly what `&arrayName` already means
                // for a real declared char array (see
                // buildAddressableOperand's Var case and every
                // backend's own genAddrOf) - a synthetic, anonymous
                // global char array holding the decoded bytes plus a
                // trailing NUL, and this expression's own value is
                // that array's address. No new IR node kind, no
                // codegen changes anywhere: every backend already
                // emits a literal char-array global and takes a bare
                // array's address correctly, from the char/short/
                // long work this same session added. Each occurrence
                // gets its own global (no string pooling), and the
                // pointer this produces, like every other pointer in
                // this language, is just a plain int - see Type.h's
                // own header comment on why there's no separate
                // pointer type to decay to here.
                ir::Slot *slot = allocSlot(ir::SlotKind::Global, "__strlit" + std::to_string(nextStringLitId_++),
                                            static_cast<int>(e.name.size()) + 1);
                slot->type = Type{BaseType::Char, false};
                slot->sourceFile = currentSourceFile_;
                slot->arrayInit.reserve(slot->arraySize);
                for (unsigned char ch : e.name) {
                    ir::ArrayInitElem elem;
                    elem.value = ch;
                    slot->arrayInit.push_back(elem);
                }
                slot->arrayInit.push_back(ir::ArrayInitElem{});  // trailing NUL

                auto arrVar = std::make_unique<ir::Expr>();
                arrVar->kind = ir::ExprKind::Var;
                arrVar->line = e.line;
                arrVar->slot = slot;
                arrVar->type = slot->type;

                out->kind = ir::ExprKind::Unary;
                out->op = "&";
                out->type = Type{BaseType::Int, false};
                out->rhs = std::move(arrVar);
                return out;
            }
        }
        err(e.line, "internal: unhandled expr kind");
    }

    ir::StmtPtr buildStmt(const Stmt &s) {
        auto out = std::make_unique<ir::Stmt>();
        out->line = s.line;
        switch (s.kind) {
            case StmtKind::Block: {
                out->kind = ir::StmtKind::Block;
                pushScope();
                for (const auto &child : s.body) out->body.push_back(buildStmt(*child));
                popScope();
                return out;
            }
            case StmtKind::If:
                out->kind = ir::StmtKind::If;
                out->cond = buildExpr(*s.cond);
                out->thenStmt = buildStmt(*s.thenStmt);
                if (s.elseStmt) out->elseStmt = buildStmt(*s.elseStmt);
                return out;
            case StmtKind::While: {
                out->kind = ir::StmtKind::While;
                out->cond = buildExpr(*s.cond);
                ++loopDepth_;
                out->loopBody = buildStmt(*s.loopBody);
                if (s.postStmt) out->postStmt = buildStmt(*s.postStmt);
                --loopDepth_;
                return out;
            }
            case StmtKind::Return:
                out->kind = ir::StmtKind::Return;
                if (s.initOrReturnOrExpr) {
                    out->initOrReturnOrExpr = wrapConvert(buildExpr(*s.initOrReturnOrExpr), currentReturnType_, s.line);
                }
                return out;
            case StmtKind::Break:
                if (loopDepth_ == 0) err(s.line, "'break' statement not inside a loop");
                out->kind = ir::StmtKind::Break;
                return out;
            case StmtKind::Continue:
                if (loopDepth_ == 0) err(s.line, "'continue' statement not inside a loop");
                out->kind = ir::StmtKind::Continue;
                return out;
            case StmtKind::LocalDecl: {
                out->kind = ir::StmtKind::LocalDecl;
                ir::Slot *slot = allocSlot(ir::SlotKind::Local, s.declName, s.arraySize);
                slot->type = s.type;
                slot->arrayInit = resolveArrayInit(s.arrayInit, s.line);
                declareLocal(s.declName, slot, s.line);
                out->declSlot = slot;
                if (s.arraySize == 0 && s.initOrReturnOrExpr) {
                    out->initOrReturnOrExpr = wrapConvert(buildExpr(*s.initOrReturnOrExpr), slot->type, s.line);
                }
                return out;
            }
            case StmtKind::ExprStmt:
                out->kind = ir::StmtKind::ExprStmt;
                out->initOrReturnOrExpr = buildExpr(*s.initOrReturnOrExpr);
                return out;
            case StmtKind::Empty:
                out->kind = ir::StmtKind::Empty;
                return out;
            case StmtKind::Asm:
                out->kind = ir::StmtKind::Asm;
                out->asmText = s.asmText;
                return out;
        }
        err(s.line, "internal: unhandled stmt kind");
    }

    ir::Program prog_;
    std::unordered_map<std::string, ir::Slot *> globalByName_;
    std::unordered_map<std::string, ir::Function *> funcByName_;
    std::vector<std::unordered_map<std::string, ir::Slot *>> scopes_;
    int nextSlotId_ = 0;
    int nextFuncId_ = 0;
    int nextStringLitId_ = 0;  // ExprKind::StringLit's own synthetic globals - see buildExpr's own case
    bool allowExternal_ = false;
    int loopDepth_ = 0;  // >0 while building a While's own loopBody/postStmt - see Break/Continue
    TypeWidths widths_{};
    Type currentReturnType_;  // the function currently being built - see StmtKind::Return
    std::string currentSourceFile_;  // ditto - stamped onto a StringLit's own synthetic global
};

}  // namespace

ir::Program buildIR(const Program &prog, const TypeWidths &widths, bool allowExternal) {
    Builder b(allowExternal, widths);
    return b.build(prog);
}

void recomputeFrameLayout(ir::Program &prog, const TypeWidths &widths) {
    int reserved = widthOf(Type{BaseType::Int, false}, widths);
    for (auto &fb : prog.functionBodies) {
        fb.func->frameSize = alignUp(assignFrameOffsets(fb.body.get(), reserved, widths), reserved) - reserved;
    }
}

}  // namespace pcc
