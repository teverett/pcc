#include "IRBuilder.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace pcc {

namespace {

[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
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
            irFn->hasAddress = fn.hasAddress;
            irFn->address = fn.address;
            irFn->sourceFile = fn.sourceFile;
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
    // if any are active, then this file's globals), then a function,
    // then fall back to treating it as an external global (the same
    // default a plain, non-'&' reference to an unknown name under
    // separate compilation has always gotten). Shared between a
    // global's own initializer (no active scope) and a LocalDecl's
    // (scopes_ may have locals in it).
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
            } else if (ir::Function *ext = resolveExternalFunc(e.symbolName)) {
                ie.func = ext;
            } else if (ir::Slot *ext = resolveExternalVar(e.symbolName)) {
                ie.slot = ext;
            } else {
                err(line, "undefined variable or function '" + e.symbolName + "' in array initializer");
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

    // An external global (real separate compilation - see
    // resolveExternalVar) starts out shaped like a plain scalar
    // (arraySize == 0): there's no declaration in this file to learn
    // its real shape from - this C subset has no `extern int
    // arr[9];` syntax to close that gap. Indexing it anywhere in this
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

    // Resolves a Var/Index/function-name operand without the "bare
    // array" check below - used only for `&`, where a bare array
    // name is exactly the point (&arr means the array's own address),
    // and where a bare function name is also valid (&f means f's own
    // entry address, never a call - see IR.h's ExprKind::Func).
    //
    // A bare IDENT that isn't a variable in this file is tried as a
    // function name next (this file's own, or - under real separate
    // compilation - one already seen at a call site elsewhere in it)
    // before falling back to creating it as an external global, the
    // same default a plain, non-'&' reference to an unknown name
    // under separate compilation has always gotten; a name genuinely
    // declared as an external function but never called anywhere in
    // this file still resolves correctly once something does call it
    // first, same as resolveFunc's own lazy external-function
    // creation - this only has to pick a default for a name this file
    // has no information about at all.
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
            if (ir::Function *ext = resolveExternalFunc(e.name)) {
                out->kind = ir::ExprKind::Func;
                out->func = ext;
                return out;
            }
            if (ir::Slot *ext = resolveExternalVar(e.name)) {
                out->kind = ir::ExprKind::Var;
                out->slot = ext;
                out->type = ext->type;
                return out;
            }
            err(e.line, "undefined variable or function '" + e.name + "'");
        }
        if (e.kind == ExprKind::Index) {
            out->kind = ir::ExprKind::Index;
            out->slot = resolveVar(e.name, e.line);
            out->type = out->slot->type;
            markIndexedIfExternal(out->slot);
            out->lhs = buildExpr(*e.lhs);
            return out;
        }
        err(e.line, "'&' requires a plain variable, array element, or function name, e.g. &x, &arr[i], or &f");
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
            case ExprKind::Index:
                out->kind = ir::ExprKind::Index;
                out->slot = resolveVar(e.name, e.line);
                out->type = out->slot->type;
                markIndexedIfExternal(out->slot);
                out->lhs = buildExpr(*e.lhs);
                return out;
            case ExprKind::Assign: {
                out->kind = ir::ExprKind::Assign;
                out->slot = resolveVar(e.name, e.line);
                if (e.lhs) {
                    markIndexedIfExternal(out->slot);  // arr[i] = ... - e.lhs is the index
                    out->lhs = buildExpr(*e.lhs);
                }
                out->type = out->slot->type;
                out->rhs = wrapConvert(buildExpr(*e.rhs), out->type, e.line);
                return out;
            }
            case ExprKind::DerefAssign:
                // '*' has no pointee type to track (pointers are
                // plain, untyped ints - see Type.h's own header
                // comment) - the stored-through value stays at native
                // pointer-word width, same as today.
                out->kind = ir::ExprKind::DerefAssign;
                out->type = Type{BaseType::Int, false};
                out->lhs = buildExpr(*e.lhs);
                out->rhs = buildExpr(*e.rhs);
                return out;
            case ExprKind::Unary: {
                out->kind = ir::ExprKind::Unary;
                out->op = e.op;
                if (e.op == "&") {
                    // An address is native pointer-word width,
                    // unaffected by the operand's own type.
                    out->type = Type{BaseType::Int, false};
                    out->rhs = buildAddressableOperand(*e.rhs);
                    return out;
                }
                ir::ExprPtr operand = buildExpr(*e.rhs);
                if (e.op == "*" || e.op == "!") {
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
                out->kind = ir::ExprKind::Binary;
                out->op = e.op;
                ir::ExprPtr lhs = buildExpr(*e.lhs);
                ir::ExprPtr rhs = buildExpr(*e.rhs);
                if (e.op == "&&" || e.op == "||") {
                    // Truth-testing is width-independent - no
                    // conversion needed on either operand.
                    out->type = Type{BaseType::Int, false};
                    out->lhs = std::move(lhs);
                    out->rhs = std::move(rhs);
                    return out;
                }
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
                Type common = commonType(lhs->type, rhs->type);
                lhs = wrapConvert(std::move(lhs), common, e.line);
                rhs = wrapConvert(std::move(rhs), common, e.line);
                out->lhs = std::move(lhs);
                out->rhs = std::move(rhs);
                if (e.op == "==" || e.op == "!=" || e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") {
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
            case ExprKind::Cast:
                return wrapConvert(buildExpr(*e.rhs), e.type, e.line);
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
    bool allowExternal_ = false;
    int loopDepth_ = 0;  // >0 while building a While's own loopBody/postStmt - see Break/Continue
    TypeWidths widths_{};
    Type currentReturnType_;  // the function currently being built - see StmtKind::Return
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
