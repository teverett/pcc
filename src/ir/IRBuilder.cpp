#include "IRBuilder.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#include "../StructLayout.h"

namespace rcc {

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
        case BaseType::Struct:
            base = "struct " + t.structTag;
            break;
    }
    return prefix + base + std::string(t.ptrDepth, '*');
}

// Warns on an explicit cast that narrows the value or flips signedness at
// the same width; implicit conversions are not checked.
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

// Assigns every Local Slot reached from `s` its frameOffset in declaration
// order and returns the next unused offset. `nextOffset` starts at
// widthOf(Int, widths): one native int's reserved space every backend's
// prologue expects below the frame pointer.
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
            // A struct local (scalar or array) aligns to its own
            // structAlign (the widest member's alignment, not the whole
            // struct's size) and is addressed ascending from its lowest
            // byte like an array - see ir::localOffsetDescendingWide()'s
            // own Struct exclusion - so it never takes the wide-scalar
            // branch below, even though its size is usually > an int's.
            if (s->declSlot->type.base == BaseType::Struct) {
                int align = s->declSlot->type.structAlign;
                int size = s->declSlot->type.structSize;
                nextOffset = alignUp(nextOffset, align);
                s->declSlot->frameOffset = nextOffset;
                int count = s->declSlot->arraySize > 0 ? s->declSlot->arraySize : 1;
                return nextOffset + size * count;
            }
            // A Local's lowest address is frameOffset + size - one int
            // (see ir::localLowOffset()), so a type wider than an int is
            // aligned by that address rather than by frameOffset itself.
            int elemWidth = widthOf(s->declSlot->type, widths);
            if (elemWidth > widths.intUnits) {
                nextOffset = alignUp(nextOffset - widths.intUnits, elemWidth) + widths.intUnits;
            } else {
                nextOffset = alignUp(nextOffset, elemWidth);
            }
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
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return nextOffset;
    }
    return nextOffset;
}

ir::ExprPtr wrapConvert(ir::ExprPtr inner, Type target, int line) {
    if (inner->type == target) return inner;
    auto conv = std::make_unique<ir::Expr>();
    conv->kind = ir::ExprKind::Convert;
    conv->line = line;
    conv->type = target;
    conv->lhs = std::move(inner);
    return conv;
}

// Integer promotion: a sub-Int-rank operand becomes signed Int.
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
        registerStructs(prog);
        registerGlobalsAndFunctions(prog);
        for (const auto &fn : prog.functions) {
            if (!fn.body) continue;  // prototype only
            ir::Function *irFn = funcByName_[fn.name];

            pushScope();
            for (ir::Slot *p : irFn->params) declareLocal(p->sourceName, p, fn.line);
            currentReturnType_ = irFn->returnType;
            currentFunctionIsVoid_ = irFn->isVoid;
            currentFunctionName_ = fn.name;
            currentSourceFile_ = fn.sourceFile;
            currentFunctionLabels_.clear();
            collectLabels(fn.body.get());
            ir::StmtPtr body = buildStmt(*fn.body);
            popScope();
            int reserved = widthOf(Type{BaseType::Int, false}, widths_);
            // Rounds up to native int width so the stack pointer stays
            // aligned after a backend's frame reservation (some targets
            // fault on misaligned word access otherwise).
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
    // Resolves every struct tag's member offsets/size/alignment once,
    // up front - every other type boundary (globals, locals, params,
    // casts, sizeof) just looks a tag up here via resolveStructType(),
    // never recomputes a layout itself. Struct definitions must be
    // textually self-contained (a member naming another struct tag
    // requires that tag already registered), matching this subset's
    // define-before-use rule elsewhere (see grammar/CParser.g4's own
    // structDecl comment).
    void registerStructs(const Program &prog) {
        for (const auto &sd : prog.structs) {
            if (structsByTag_.count(sd.tag)) err(sd.line, "redefinition of 'struct " + sd.tag + "'");
            std::vector<StructMember> members;
            members.reserve(sd.members.size());
            for (const auto &m : sd.members) {
                StructMember sm;
                sm.name = m.name;
                sm.type = resolveStructType(m.type, m.line);
                sm.arraySize = m.arraySize;
                members.push_back(std::move(sm));
            }
            StructLayout layout = computeStructLayout(sd.tag, members, widths_, sd.packed);
            if (sd.packed) checkPackedAlignmentSafety(layout, sd.line);
            structsByTag_[sd.tag] = std::move(layout);
        }
    }

    // '@packed' drops inter-member padding on every backend alike, but whether a backend's
    // hardware (or this codegen's own wide-load/store sequences) tolerates a misaligned
    // multi-byte access varies - several of rcc's targets (word-addressed machines, several
    // RISC load/store ISAs) fault or misbehave on one. Rather than special-case per backend,
    // this rejects any packed layout that would need one, on every backend alike: a loud
    // compile error instead of a silent miscompile on whichever backend actually can't do it.
    // A member with alignment 1 (a `char`, or an array of `char`) can never be misaligned, so
    // this only ever fires for a multi-byte scalar/array/nested-struct member.
    void checkPackedAlignmentSafety(const StructLayout &layout, int line) {
        for (const auto &m : layout.members) {
            int align = m.type.base == BaseType::Struct ? m.type.structAlign : widthOf(m.type, widths_);
            if (align > 1 && m.byteOffset % align != 0) {
                err(line, "'struct " + layout.tag + "' member '" + m.name + "' would sit at byte offset " +
                              std::to_string(m.byteOffset) + " under '@packed', which isn't a multiple of its " +
                              "own " + std::to_string(align) +
                              "-byte alignment - some backends can't access a misaligned multi-byte value; "
                              "reorder this struct's members so every multi-byte member lands on its own "
                              "alignment boundary");
            }
        }
    }

    // Fills in structSize/structAlign for a struct-typed Type (any
    // ptrDepth - base/structTag always describe the pointee, so a
    // pointer-to-struct's pointee is resolved the same way). A no-op for
    // every non-struct Type, so every call site below is safe to apply
    // unconditionally.
    Type resolveStructType(Type t, int line) {
        if (t.base != BaseType::Struct) return t;
        auto it = structsByTag_.find(t.structTag);
        if (it == structsByTag_.end()) {
            err(line, "'struct " + t.structTag +
                          "' is used before it's defined (or never defined) - this subset requires a struct's "
                          "full definition, textually earlier, before any use of its tag");
        }
        t.structSize = it->second.size;
        t.structAlign = it->second.align;
        return t;
    }

    // By-value struct parameters/returns need their own calling-convention
    // support, which this subset doesn't implement - only a pointer to a
    // struct is allowed in either position.
    void rejectStructByValue(Type t, int line, const char *what) {
        if (t.base == BaseType::Struct && t.ptrDepth == 0) {
            err(line, std::string(what) + " can't be a struct by value - use a pointer to it instead (e.g. "
                                            "'struct " +
                          t.structTag + " *')");
        }
    }

    void registerGlobalsAndFunctions(const Program &prog) {
        for (const auto &g : prog.globals) {
            if (globalByName_.count(g.name)) err(g.line, "redeclaration of global '" + g.name + "'");
            ir::Slot *slot = allocSlot(ir::SlotKind::Global, g.name, g.arraySize);
            slot->type = resolveStructType(g.type, g.line);
            // Registers an 'extern' global's kind (data) before any
            // '&name'/array-init element elsewhere in this file is resolved.
            if (g.isExternDecl) {
                slot->isExternal = true;
                slot->sourceFile = g.sourceFile;
                globalByName_[g.name] = slot;
                continue;
            }
            if (slot->type.base == BaseType::Struct && g.arraySize > 0) {
                err(g.line, "an array of 'struct " + slot->type.structTag +
                                 "' isn't supported yet as a global (only a single struct variable, or a "
                                 "local array of struct, is) - see docs/language.md's \"Structs\" section");
            }
            if (slot->type.base == BaseType::Struct && g.hasInit) {
                err(g.line, "a struct global can't have an initializer yet - set its fields after "
                             "declaration instead");
            }
            slot->hasInit = g.hasInit;
            slot->initValue = g.initValue;
            // arrayInit is resolved in a second pass below, so a forward
            // reference to a later name in this file still resolves.
            slot->hasAddress = g.hasAddress;
            slot->address = g.address;
            slot->isStatic = g.isStatic;
            if (g.isStatic) slot->sourceName = mangleStaticName(g.sourceFile, g.name);
            slot->sourceFile = g.sourceFile;
            globalByName_[g.name] = slot;
        }
        for (const auto &fn : prog.functions) {
            if (funcByName_.count(fn.name)) err(fn.line, "redefinition of function '" + fn.name + "'");
            auto irFn = std::make_unique<ir::Function>();
            irFn->id = nextFuncId_++;
            irFn->sourceName = fn.name;
            irFn->returnType = resolveStructType(fn.returnType, fn.line);
            irFn->isVoid = fn.isVoid;
            irFn->sourceFile = fn.sourceFile;
            if (!fn.isVoid) rejectStructByValue(irFn->returnType, fn.line, "a function's return type");
            // Registers an 'extern' function's name before any '&name'/
            // array-init element referring to it is resolved.
            if (fn.isExternDecl) {
                irFn->isExternal = true;
                irFn->hasPrototype = true;
                for (const auto &p : fn.params) {
                    Type pt = resolveStructType(p.type, fn.line);
                    rejectStructByValue(pt, fn.line, "a function parameter");
                    irFn->prototypeParams.push_back(pt);
                }
                funcByName_[fn.name] = irFn.get();
                prog_.functions.push_back(std::move(irFn));
                continue;
            }
            irFn->hasAddress = fn.hasAddress;
            irFn->address = fn.address;
            irFn->isStatic = fn.isStatic;
            if (fn.isStatic) irFn->sourceName = mangleStaticName(fn.sourceFile, fn.name);
            // Params start past a two-native-int reservation for the saved
            // frame pointer/return address. Each param occupies at least one
            // native int's width (a caller pushes whole words), wider only
            // for a type wider than int.
            int intWidth = widthOf(Type{BaseType::Int, false}, widths_);
            int paramOffset = 2 * intWidth;
            for (size_t i = 0; i < fn.params.size(); i++) {
                ir::Slot *p = allocSlot(ir::SlotKind::Param, fn.params[i].name);
                p->type = resolveStructType(fn.params[i].type, fn.line);
                rejectStructByValue(p->type, fn.line, "a function parameter");
                p->ownerFunctionName = fn.name;
                p->paramIndex = static_cast<int>(i);
                // No alignUp: the caller's push sequence lays params
                // back-to-back with no gap.
                int w = std::max(widthOf(p->type, widths_), intWidth);
                p->frameOffset = paramOffset;
                paramOffset += w;
                irFn->params.push_back(p);
            }
            funcByName_[fn.name] = irFn.get();
            prog_.functions.push_back(std::move(irFn));
        }
        // Only a directly-runnable program needs its own `main`; an object
        // file (allowExternal_) doesn't. A defined main is still checked.
        if (!allowExternal_ && !funcByName_.count("main")) throw std::runtime_error("no 'main' function defined");
        if (funcByName_.count("main") && !funcByName_["main"]->params.empty()) {
            throw std::runtime_error("'main' must take no parameters");
        }

        for (const auto &g : prog.globals) {
            globalByName_[g.name]->arrayInit = resolveArrayInit(g.arrayInit, g.line);
        }
    }

    // Resolves each ArrayInitElem to IR form: a literal passes through; a
    // symbol is tried as a variable then a function (same order as
    // buildAddressableOperand's "&name"). An unresolvable name is a hard
    // error rather than a guess, since there's no way to tell a function
    // address from a data address apart otherwise — the file must declare
    // it 'extern'.
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

    // Creates (and caches) an external Function on a miss, only when
    // allowExternal_ is set; otherwise nullptr.
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

    // A 'static' global/function/local needs internal linkage: under real
    // separate compilation (--format=coff/elf/macho), its name must not
    // collide with another translation unit's own same-named 'static'
    // symbol at link time (GenericLinker treats any two same-named defined
    // symbols as a duplicate-symbol error, with no notion of per-file
    // visibility). Folding the originating source file into the name, not
    // just the bare C name, keeps every 'static' symbol unique across
    // files without the linker needing to know about linkage at all. The
    // "__rcc_local_" prefix additionally gets this symbol hidden/unexported
    // for whichever backends' object writers already sniff that exact
    // prefix for compiler-internal labels (see e.g. CoffFormat.cpp's/
    // ElfFormat.cpp's own isRccLocalSymbol()) - a side benefit on those
    // backends, not something every backend's object writer honors.
    std::string mangleStaticName(const std::string &sourceFile, const std::string &name) {
        std::string sanitized;
        for (char c : sourceFile) sanitized += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
        return "__rcc_local_" + sanitized + "_" + name;
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

    // Local scopes and this-file globals only; no external-var
    // auto-creation. Lets callers distinguish "not a variable" (try a
    // function name) from "not declared anywhere".
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
        // An external function with no `extern` prototype has no parameter
        // list to check against; the call site's argument count is trusted.
        size_t expected = fn->isExternal ? fn->prototypeParams.size() : fn->params.size();
        if ((!fn->isExternal || fn->hasPrototype) && expected != argc) {
            err(line, "'" + name + "' expects " + std::to_string(expected) + " argument(s), got " +
                          std::to_string(argc));
        }
        return fn;
    }

    // A lazily auto-created external global starts shaped as a scalar
    // (arraySize == 0), since there's no declaration here to learn its real
    // shape from. Indexing it anywhere in this file proves it's
    // array-shaped; mark it retroactively (any nonzero placeholder works —
    // only "is this an array at all" matters for addressing, never the
    // real size).
    void markIndexedIfExternal(ir::Slot *slot) {
        if (slot->isExternal && slot->arraySize == 0) slot->arraySize = 1;
    }

    // `slot[idx]` on a plain pointer variable means `*(slot+idx)`, not a
    // real array's Index codegen: Index scales by slot->type, which for a
    // pointer variable is the pointer's own type, not its pointee. Lowering
    // to pointer arithmetic here reuses the existing +/deref machinery
    // instead. Returns the scaled `slot+idx` value itself; callers deref it.
    ir::ExprPtr buildPointerIndexAddr(ir::Slot *slot, const Expr &idxExpr, int line) {
        auto ptrVar = std::make_unique<ir::Expr>();
        ptrVar->kind = ir::ExprKind::Var;
        ptrVar->line = line;
        ptrVar->slot = slot;
        ptrVar->type = slot->type;
        return buildPointerBinary("+", std::move(ptrVar), buildExpr(idxExpr), line);
    }

    // Resolves a Var/Index/function-name operand for `&`, skipping the
    // bare-array check below: &arr is the array's address, &f is f's entry
    // address (never a call). A bare IDENT is tried as a variable, then a
    // function; see resolveArrayInit for why an unresolved name errors
    // rather than guessing.
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
            // Only reached for a real array; `&p[i]` on a pointer variable
            // is intercepted earlier, in the Unary "&" case below.
            out->kind = ir::ExprKind::Index;
            out->slot = resolveVar(e.name, e.line);
            out->type = out->slot->type;
            markIndexedIfExternal(out->slot);
            out->lhs = buildExpr(*e.lhs);
            return out;
        }
        err(e.line, "'&' requires a plain variable, array element, or function name, e.g. &x, &arr[i], or &f");
    }

    ir::ExprPtr makeIntLit(long value, Type type, int line) {
        auto lit = std::make_unique<ir::Expr>();
        lit->kind = ir::ExprKind::IntLit;
        lit->line = line;
        lit->type = type;
        lit->intValue = value;
        return lit;
    }

    // Resolves `baseAst`'s own address, as a char*-typed pointer
    // expression (so a later +offset step is byte-exact, unscaled), and
    // its value's own Type (needed to look its struct tag up) - used by
    // buildMemberAddr for '.' (never for '->', which already holds a
    // pointer value with no address-of step needed). Mirrors
    // buildAddressableOperand's own Var/Index/pointer-indexing handling
    // for '&', since a struct value reached through '.' is addressable in
    // exactly the same shapes: a plain variable, a real array element, a
    // pointer variable indexed as an array (*(p+i), not a real element),
    // a dereferenced pointer, or another member access to recurse into.
    std::pair<ir::ExprPtr, Type> addressableStructValue(const Expr &baseAst) {
        Type charPtr(BaseType::Char, false, 1);
        if (baseAst.kind == ExprKind::Index) {
            if (ir::Slot *slot = resolveVar(baseAst.name, baseAst.line);
                slot->arraySize == 0 && isPointer(slot->type)) {
                // p[i] on a pointer-to-struct variable is *(p+i): the
                // pointee is the struct value, not a real array element.
                ir::ExprPtr sum = buildPointerIndexAddr(slot, *baseAst.lhs, baseAst.line);
                Type pointee = pointeeOf(sum->type);
                return {wrapConvert(std::move(sum), charPtr, baseAst.line), pointee};
            }
        }
        if (baseAst.kind == ExprKind::Var || baseAst.kind == ExprKind::Index) {
            ir::ExprPtr operand = buildAddressableOperand(baseAst);
            Type t = operand->type;
            auto addr = std::make_unique<ir::Expr>();
            addr->kind = ir::ExprKind::Unary;
            addr->op = "&";
            addr->line = baseAst.line;
            addr->type = charPtr;
            addr->rhs = std::move(operand);
            return {std::move(addr), t};
        }
        if (baseAst.kind == ExprKind::Unary && baseAst.op == "*") {
            ir::ExprPtr inner = buildExpr(*baseAst.rhs);
            if (!isPointer(inner->type)) err(baseAst.line, "'*' here must dereference a pointer to struct");
            Type pointee = pointeeOf(inner->type);
            return {wrapConvert(std::move(inner), charPtr, baseAst.line), pointee};
        }
        if (baseAst.kind == ExprKind::MemberAccess || baseAst.kind == ExprKind::MemberAssign ||
            baseAst.kind == ExprKind::CompoundMemberAssign) {
            Type memberType;
            ir::ExprPtr addr = buildMemberAddr(baseAst, memberType);
            return {std::move(addr), memberType};
        }
        err(baseAst.line, "'.' requires an addressable struct value (a variable, array element, pointer "
                            "dereference, or another member)");
    }

    // Resolves a MemberAccess/MemberAssign/CompoundMemberAssign AST node
    // (all three share the same lhs/name/viaPointer/memberIndex shape -
    // see AST.h) to the byte address of its named member, as a
    // char*-typed pointer expression - never dereferenced; callers wrap
    // the result in Unary"*" for a read or DerefAssign/
    // CompoundDerefAssign for a write, both of which key their load/
    // store width off their own node's `type`, not this pointer's,
    // exactly the way plain pointer dereference already does on every
    // backend. `outMemberType` receives the member's own resolved Type.
    // `e.memberIndex`, when set, indexes into a member that's itself an
    // array (e.g. buf.data[2]) in the same step; `outMemberType` becomes
    // the array's element type in that case.
    ir::ExprPtr buildMemberAddr(const Expr &e, Type &outMemberType) {
        ir::ExprPtr basePtr;
        std::string structTag;
        Type charPtr(BaseType::Char, false, 1);
        if (e.viaPointer) {
            ir::ExprPtr base = buildExpr(*e.lhs);
            if (!isPointer(base->type) || base->type.ptrDepth != 1 || base->type.base != BaseType::Struct) {
                err(e.line, "'->' requires a pointer to a struct");
            }
            structTag = base->type.structTag;
            basePtr = wrapConvert(std::move(base), charPtr, e.line);
        } else {
            auto [addr, valueType] = addressableStructValue(*e.lhs);
            if (valueType.base != BaseType::Struct || valueType.ptrDepth != 0) {
                err(e.line, "'.' requires a struct value - use '->' on a pointer instead");
            }
            structTag = valueType.structTag;
            basePtr = std::move(addr);
        }

        auto tagIt = structsByTag_.find(structTag);
        if (tagIt == structsByTag_.end()) err(e.line, "internal: unresolved struct tag '" + structTag + "'");
        const StructMember *member = nullptr;
        for (auto &m : tagIt->second.members) {
            if (m.name == e.name) {
                member = &m;
                break;
            }
        }
        if (!member) err(e.line, "'struct " + structTag + "' has no member named '" + e.name + "'");

        Type nativeInt(BaseType::Int, false);
        ir::ExprPtr offset = makeIntLit(member->byteOffset, nativeInt, e.line);
        if (e.memberIndex) {
            if (member->arraySize <= 0) err(e.line, "'" + e.name + "' isn't an array member - remove the '[...]'");
            ir::ExprPtr idx = promote(buildExpr(*e.memberIndex), e.line);
            int elemWidth = widthOf(member->type, widths_);
            if (elemWidth != 1) {
                auto scaled = std::make_unique<ir::Expr>();
                scaled->kind = ir::ExprKind::Binary;
                scaled->line = e.line;
                scaled->op = "*";
                scaled->type = nativeInt;
                scaled->lhs = std::move(idx);
                scaled->rhs = makeIntLit(elemWidth, nativeInt, e.line);
                idx = std::move(scaled);
            }
            auto sum = std::make_unique<ir::Expr>();
            sum->kind = ir::ExprKind::Binary;
            sum->line = e.line;
            sum->op = "+";
            sum->type = nativeInt;
            sum->lhs = std::move(offset);
            sum->rhs = std::move(idx);
            offset = std::move(sum);
        } else if (member->arraySize > 0) {
            err(e.line, "'" + e.name + "' is an array member - use '" + e.name + "[i]'");
        }

        outMemberType = member->type;

        auto addrOut = std::make_unique<ir::Expr>();
        addrOut->kind = ir::ExprKind::Binary;
        addrOut->line = e.line;
        addrOut->op = "+";
        addrOut->type = charPtr;
        addrOut->lhs = std::move(basePtr);
        addrOut->rhs = std::move(offset);
        return addrOut;
    }

    // Pointer arithmetic/comparison, called once either operand is a real
    // pointer: ptr +/- int (scaled by pointee width), ptr - ptr of the same
    // type (scaled address difference), and comparisons (unsigned
    // native-word compare). Anything else on a pointer operand is rejected.
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
            // Stays at native int width, not a wider result type: tagging
            // this node wider than its actual operands would wrongly route
            // it through a backend's wide (long/long long) codegen.
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

    // "Wide" means wider than the target's native int. CompoundAssign/
    // CompoundDerefAssign/Ternary reject a wide target: they compute their
    // result in a normal native-width register, and a wide target would
    // need a multi-word instruction sequence per operator per backend that
    // isn't implemented.
    bool isWideType(Type t) const {
        return widthOf(t, widths_) > widthOf(Type{BaseType::Int, false}, widths_);
    }

    void rejectIfWide(Type t, int line, const char *what) {
        if (isWideType(t)) {
            err(line, std::string(what) +
                          " on a 'long'/'long long' target isn't supported yet - only char/short/int/"
                          "pointer (see docs/language.md's own note on this)");
        }
    }

    // Scales a promoted int-typed step by ptrType's pointee width; shared
    // by ptr+int and CompoundAssign's p+=n/p++/p--.
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

    // Builds the rhs for `target <op>= e.rhs`. For a pointer target with
    // +/-, scales the step by pointee width (native-int typed). Otherwise
    // converts straight to targetType without promoting first: this is
    // bit-identical to promoting-then-truncating for +,-,*,&,^,|,<<,>>;
    // only '/','%' at a sub-int target can differ, handled by each
    // backend's own CompoundAssign codegen.
    ir::ExprPtr buildCompoundRhs(Type targetType, const std::string &op, const Expr &rhsAst, int line) {
        if (isPointer(targetType) && (op == "+" || op == "-")) {
            ir::ExprPtr rhs = promote(buildExpr(rhsAst), line);
            return scaleStepForPointer(targetType, std::move(rhs), line);
        }
        return wrapConvert(buildExpr(rhsAst), targetType, line);
    }

    // A decimal literal's type is the first of its suffix's type and the
    // wider ones after it (int, long, long long, each unsigned when
    // suffixed 'u') that can represent its value on this target.
    Type literalType(long value, Type suffixType) {
        Type t = suffixType;
        while (t.base != BaseType::LongLong) {
            int bits = widthOf(t, widths_) * widths_.unitBits;
            int valueBits = t.isUnsigned ? bits : bits - 1;
            if (valueBits >= 63 || static_cast<unsigned long>(value) < (1UL << valueBits)) break;
            t.base = t.base == BaseType::Int ? BaseType::Long : BaseType::LongLong;
        }
        return t;
    }

    // `allowVoid`: true only for a bare expression statement ("f();"),
    // the one place a void function's call is used for its side effect
    // alone. Every other caller (an operand, an argument, a return value,
    // ...) goes through the default, which rejects it - a void call's
    // non-existent "result" can't be used as a value.
    ir::ExprPtr buildExpr(const Expr &e, bool allowVoid = false) {
        ir::ExprPtr out = buildExprImpl(e);
        if (!allowVoid && out->kind == ir::ExprKind::Call && out->func && out->func->isVoid) {
            err(e.line, "'" + out->func->sourceName + "' returns void - its call can't be used as a value");
        }
        return out;
    }

    ir::ExprPtr buildExprImpl(const Expr &e) {
        auto out = std::make_unique<ir::Expr>();
        out->line = e.line;
        switch (e.kind) {
            case ExprKind::IntLit:
                out->kind = ir::ExprKind::IntLit;
                out->intValue = e.intValue;
                out->type = literalType(e.intValue, e.type);
                return out;
            case ExprKind::Var: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (slot->arraySize > 0) {
                    err(e.line, "array '" + e.name + "' used without a subscript - use " + e.name +
                                    "[i], or &" + e.name + " for its address");
                }
                rejectStructByValue(slot->type, e.line, "a struct variable read as a value (not through '.'/'->')");
                out->kind = ir::ExprKind::Var;
                out->slot = slot;
                out->type = slot->type;
                return out;
            }
            case ExprKind::Index: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (slot->arraySize == 0 && isPointer(slot->type)) {
                    // `p[i]` is `*(p+i)` for a pointer variable.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::Unary;
                    out->op = "*";
                    out->type = pointeeOf(sum->type);
                    rejectStructByValue(out->type, e.line, "a pointer dereference read as a value");
                    out->rhs = std::move(sum);
                    return out;
                }
                rejectStructByValue(slot->type, e.line, "a struct array element read as a value (not through '.')");
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
                    // `p[i] = rhs` is `*(p+i) = rhs`.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::DerefAssign;
                    Type pointee = pointeeOf(sum->type);
                    rejectStructByValue(pointee, e.line, "a whole-struct assignment through a pointer");
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
                rejectStructByValue(out->type, e.line, "a whole-struct assignment ('a = b;')");
                out->rhs = wrapConvert(buildExpr(*e.rhs), out->type, e.line);
                return out;
            }
            case ExprKind::CompoundAssign: {
                ir::Slot *slot = resolveVar(e.name, e.line);
                if (e.lhs && slot->arraySize == 0 && isPointer(slot->type)) {
                    // `p[i] op= rhs` (incl. p[i]++/p[i]--) is `*(p+i) op= rhs`.
                    ir::ExprPtr sum = buildPointerIndexAddr(slot, *e.lhs, e.line);
                    out->kind = ir::ExprKind::CompoundDerefAssign;
                    Type pointee = pointeeOf(sum->type);
                    rejectStructByValue(pointee, e.line, "a compound assignment through a pointer to struct");
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
                rejectStructByValue(out->type, e.line, "a compound assignment on a struct");
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
                rejectStructByValue(pointee, e.line, "a compound assignment through a pointer to struct");
                out->type = pointee;
                rejectIfWide(out->type, e.line, "compound assignment/'++'/'--'");
                out->rhs = buildCompoundRhs(pointee, e.op, *e.rhs, e.line);
                return out;
            }
            case ExprKind::DerefAssign: {
                out->kind = ir::ExprKind::DerefAssign;
                out->lhs = buildExpr(*e.lhs);
                if (isPointer(out->lhs->type)) {
                    Type pointee = pointeeOf(out->lhs->type);
                    rejectStructByValue(pointee, e.line, "a whole-struct assignment through a pointer");
                    out->type = pointee;
                    out->rhs = wrapConvert(buildExpr(*e.rhs), pointee, e.line);
                } else {
                    // A plain int used as an untyped address writes a full
                    // native word.
                    out->type = Type{BaseType::Int, false};
                    out->rhs = buildExpr(*e.rhs);
                }
                return out;
            }
            case ExprKind::MemberAccess: {
                Type memberType;
                ir::ExprPtr addr = buildMemberAddr(e, memberType);
                if (memberType.base == BaseType::Struct && memberType.ptrDepth == 0) {
                    err(e.line, "can't read '" + e.name +
                                     "' by value - it's a struct; access its own fields instead");
                }
                out->kind = ir::ExprKind::Unary;
                out->op = "*";
                out->type = memberType;
                out->rhs = std::move(addr);
                return out;
            }
            case ExprKind::MemberAssign: {
                Type memberType;
                ir::ExprPtr addr = buildMemberAddr(e, memberType);
                if (memberType.base == BaseType::Struct && memberType.ptrDepth == 0) {
                    err(e.line, "can't assign to '" + e.name +
                                     "' as a whole - it's a struct; set its own fields instead");
                }
                out->kind = ir::ExprKind::DerefAssign;
                out->type = memberType;
                out->lhs = std::move(addr);
                out->rhs = wrapConvert(buildExpr(*e.rhs), memberType, e.line);
                return out;
            }
            case ExprKind::CompoundMemberAssign: {
                Type memberType;
                ir::ExprPtr addr = buildMemberAddr(e, memberType);
                if (memberType.base == BaseType::Struct && memberType.ptrDepth == 0) {
                    err(e.line, "can't compound-assign to '" + e.name +
                                     "' as a whole - it's a struct; operate on its own fields instead");
                }
                out->kind = ir::ExprKind::CompoundDerefAssign;
                out->type = memberType;
                out->op = e.op;
                out->yieldOld = e.yieldOld;
                out->lhs = std::move(addr);
                rejectIfWide(out->type, e.line, "compound assignment/'++'/'--'");
                out->rhs = buildCompoundRhs(memberType, e.op, *e.rhs, e.line);
                return out;
            }
            case ExprKind::Unary: {
                out->kind = ir::ExprKind::Unary;
                out->op = e.op;
                if (e.op == "&") {
                    // `&p[i]` on a pointer variable is `p+i` itself, not a
                    // further address-of (unlike &arr[i] for a real array).
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
                    // A plain int used as an untyped address reads a full
                    // native word.
                    out->type =
                        isPointer(operand->type) ? pointeeOf(operand->type) : Type{BaseType::Int, false};
                    rejectStructByValue(out->type, e.line, "a plain pointer dereference ('*p') read as a value");
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
                    // Truth-testing is width-independent; no conversion needed.
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
                    // rhs promotes for its own evaluation but never affects
                    // the result type.
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
                // A comparison between two already-constant operands (e.g.
                // two sizeofs) is folded here so its result's width never
                // depends on a wide (long/long long) comparison some
                // backends can't actually emit.
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
                    // Compares at `common` (codegen reads .isUnsigned to
                    // pick signed/unsigned), result is always int 0/1.
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
                    // The pointer branch decides the result type; the other
                    // branch converts only if it's also a pointer.
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
                    // An external function without an `extern` prototype has
                    // no parameter list to convert against; its argument type
                    // passes through.
                    if (!out->func->isExternal) {
                        arg = wrapConvert(std::move(arg), out->func->params[i]->type, e.line);
                    } else if (out->func->hasPrototype) {
                        arg = wrapConvert(std::move(arg), out->func->prototypeParams[i], e.line);
                    }
                    out->args.push_back(std::move(arg));
                }
                out->type = out->func->isExternal && !out->func->hasPrototype ? Type{BaseType::Int, false}
                                                                               : out->func->returnType;
                return out;
            }
            case ExprKind::CallPtr:
                // No callee is known at compile time, so there's no ABI
                // slot for an argument and no signature for a real return
                // type (stays plain int); only a 0-argument (expr)() works.
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
                Type target = resolveStructType(e.type, e.line);
                rejectStructByValue(target, e.line, "a cast target type");
                warnRiskyCast(inner->type, target, e.line, widths_);
                return wrapConvert(std::move(inner), target, e.line);
            }
            case ExprKind::Sizeof: {
                // sizeof(expr) builds expr only to learn its static type
                // (still raising any unrelated semantic error) and discards
                // it; only `.type` is kept.
                Type measured = e.sizeofIsType ? resolveStructType(e.type, e.line) : buildExpr(*e.rhs)->type;
                out->kind = ir::ExprKind::IntLit;
                out->type = Type{BaseType::Long, true};
                out->intValue = widthOf(measured, widths_);
                return out;
            }
            case ExprKind::StringLit: {
                // Lowers to a synthetic anonymous global char array (bytes
                // plus trailing NUL) and takes its address, same as
                // &arrayName for a declared array. Each occurrence gets its
                // own global; no string pooling.
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
                out->type = pointerTo(slot->type);
                out->rhs = std::move(arrVar);
                return out;
            }
        }
        err(e.line, "internal: unhandled expr kind");
    }

    // Pre-pass over a function body, run before buildStmt(), so every
    // `goto` (forward or backward) resolves against a complete label table
    // regardless of build order. Labels have function scope in real C (not
    // block scope), so this recurses into every nested statement, not just
    // the top-level block.
    void collectLabels(const Stmt *s) {
        if (!s) return;
        switch (s->kind) {
            case StmtKind::Block:
                for (const auto &child : s->body) collectLabels(child.get());
                return;
            case StmtKind::If:
                collectLabels(s->thenStmt.get());
                collectLabels(s->elseStmt.get());
                return;
            case StmtKind::While:
                collectLabels(s->loopBody.get());
                collectLabels(s->postStmt.get());
                return;
            case StmtKind::Switch:
                for (const auto &c : s->cases) {
                    for (const auto &child : c.body) collectLabels(child.get());
                }
                return;
            case StmtKind::Label:
                if (currentFunctionLabels_.count(s->declName)) {
                    err(s->line, "duplicate label '" + s->declName + "'");
                }
                currentFunctionLabels_[s->declName] = nextLabelId_++;
                collectLabels(s->thenStmt.get());
                return;
            case StmtKind::Return:
            case StmtKind::Break:
            case StmtKind::Continue:
            case StmtKind::Goto:
            case StmtKind::LocalDecl:
            case StmtKind::ExprStmt:
            case StmtKind::Empty:
            case StmtKind::Asm:
                return;
        }
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
                breakTargets_.push_back(-1);  // -1: let codegen's own loop-break-label stack resolve it
                out->loopBody = buildStmt(*s.loopBody);
                breakTargets_.pop_back();
                if (s.postStmt) out->postStmt = buildStmt(*s.postStmt);
                --loopDepth_;
                return out;
            }
            case StmtKind::Switch:
                return buildSwitch(s);
            case StmtKind::Return:
                out->kind = ir::StmtKind::Return;
                if (s.initOrReturnOrExpr) {
                    if (currentFunctionIsVoid_) {
                        err(s.line, "a function returning void cannot 'return' a value");
                    }
                    out->initOrReturnOrExpr = wrapConvert(buildExpr(*s.initOrReturnOrExpr), currentReturnType_, s.line);
                }
                return out;
            case StmtKind::Break:
                if (breakTargets_.empty()) err(s.line, "'break' statement not inside a loop or switch");
                if (breakTargets_.back() >= 0) {
                    // Inside a switch (and not inside a further-nested loop,
                    // which would have pushed its own -1 on top): jump
                    // straight past it, rather than falling into codegen's
                    // generic Break handling, which only knows how to target
                    // the nearest enclosing *loop*.
                    out->kind = ir::StmtKind::Goto;
                    out->labelId = breakTargets_.back();
                } else {
                    out->kind = ir::StmtKind::Break;
                }
                return out;
            case StmtKind::Continue:
                // Deliberately keyed on loopDepth_ alone, not breakTargets_:
                // `continue` always means the nearest enclosing *loop*, even
                // from inside a switch nested in one (real C semantics) -
                // switch contributes no continue target of its own.
                if (loopDepth_ == 0) err(s.line, "'continue' statement not inside a loop");
                out->kind = ir::StmtKind::Continue;
                return out;
            case StmtKind::LocalDecl: {
                if (s.isStatic) {
                    // A 'static' local has the same storage as a Global (one
                    // persistent location, initialized once at program start
                    // rather than on every call) - it's just not reachable by
                    // name outside this function. Reuse a Global Slot
                    // outright rather than adding a fourth SlotKind: every
                    // backend's Global-storage emission already handles it
                    // with no further change, as long as the name is unique
                    // and `declareLocal` below still keys it under the
                    // short, unqualified source name for ordinary scope
                    // resolution inside this function.
                    std::string mangled = mangleStaticName(
                        currentSourceFile_,
                        currentFunctionName_ + "_" + s.declName + "_" + std::to_string(nextStaticLocalId_++));
                    ir::Slot *slot = allocSlot(ir::SlotKind::Global, mangled, s.arraySize);
                    slot->type = resolveStructType(s.type, s.line);
                    slot->isStatic = true;
                    slot->sourceFile = currentSourceFile_;
                    if (slot->type.base == BaseType::Struct && s.arraySize > 0) {
                        err(s.line, "an array of 'struct " + slot->type.structTag +
                                         "' isn't supported yet as a 'static' local (only a single struct "
                                         "variable is) - see docs/language.md's \"Structs\" section");
                    }
                    if (s.arraySize > 0) {
                        slot->arrayInit = resolveArrayInit(s.arrayInit, s.line);
                    } else if (s.initOrReturnOrExpr) {
                        // Grammar restricts a 'static' local's scalar initializer to a plain
                        // (optionally negated) literal - see CParser.g4's staticLocalDecl - so
                        // this is always an already-built IntLit, never a runtime expr to
                        // evaluate on every call.
                        slot->hasInit = true;
                        slot->initValue = s.initOrReturnOrExpr->intValue;
                    }
                    declareLocal(s.declName, slot, s.line);
                    out->kind = ir::StmtKind::Empty;
                    out->declSlot = slot;
                    return out;
                }
                out->kind = ir::StmtKind::LocalDecl;
                ir::Slot *slot = allocSlot(ir::SlotKind::Local, s.declName, s.arraySize);
                slot->type = resolveStructType(s.type, s.line);
                if (slot->type.base == BaseType::Struct) {
                    // A struct local (scalar or array) starts uninitialized,
                    // same as any other local with no initializer - skip
                    // resolveArrayInit's generic literal-list handling
                    // entirely (it assumes one scalar value per array
                    // element, at the element's own width, which doesn't
                    // apply to a struct element). Fields are set via '.'/
                    // '->' assignment after declaration instead.
                    if (s.initOrReturnOrExpr) {
                        err(s.line, "a struct local can't have an initializer yet - set its fields after "
                                     "declaration instead");
                    }
                } else {
                    slot->arrayInit = resolveArrayInit(s.arrayInit, s.line);
                }
                declareLocal(s.declName, slot, s.line);
                out->declSlot = slot;
                if (s.arraySize == 0 && s.initOrReturnOrExpr) {
                    out->initOrReturnOrExpr = wrapConvert(buildExpr(*s.initOrReturnOrExpr), slot->type, s.line);
                }
                return out;
            }
            case StmtKind::ExprStmt:
                out->kind = ir::StmtKind::ExprStmt;
                out->initOrReturnOrExpr = buildExpr(*s.initOrReturnOrExpr, /*allowVoid=*/true);
                return out;
            case StmtKind::Empty:
                out->kind = ir::StmtKind::Empty;
                return out;
            case StmtKind::Asm:
                out->kind = ir::StmtKind::Asm;
                out->asmText = s.asmText;
                return out;
            case StmtKind::Label: {
                // Wraps the marker and its one attached statement in a
                // Block, but WITHOUT pushScope()/popScope() (unlike a real
                // source-level Block) - a label doesn't open a new scope in
                // real C, so a LocalDecl under `thenStmt` must register
                // into whichever scope is already active here, visible to
                // whatever follows this Label in the real enclosing block.
                out->kind = ir::StmtKind::Block;
                auto marker = std::make_unique<ir::Stmt>();
                marker->kind = ir::StmtKind::Label;
                marker->line = s.line;
                marker->labelId = currentFunctionLabels_.at(s.declName);
                out->body.push_back(std::move(marker));
                if (s.thenStmt) out->body.push_back(buildStmt(*s.thenStmt));
                return out;
            }
            case StmtKind::Goto: {
                auto it = currentFunctionLabels_.find(s.declName);
                if (it == currentFunctionLabels_.end()) {
                    err(s.line, "'goto' to undefined label '" + s.declName + "'");
                }
                out->kind = ir::StmtKind::Goto;
                out->labelId = it->second;
                return out;
            }
        }
        err(s.line, "internal: unhandled stmt kind");
    }

    // Desugars `switch (expr) { case C1: B1 case C2: B2 default: B3 }` into
    // (pseudo-code, using the new Label/Goto-only IR primitives - see
    // IR.h's own comment on why they exist only for this):
    //   { tmp = expr;
    //     if (tmp == C1) goto L1;
    //     if (tmp == C2) goto L2;
    //     goto Ldefault;        // or Lend, if there's no `default`
    //     L1: B1
    //     L2: B2
    //     Ldefault: B3
    //     Lend: }
    // `tmp` makes `expr` evaluate exactly once, same as real C, regardless
    // of how many cases compare against it. Real fallthrough is simply not
    // injecting a jump between one case's body and the next's label - the
    // comparisons dispatch INTO the sequence, never between cases within
    // it. `break` inside the switch (but not inside a further-nested loop)
    // becomes a Goto Lend - see buildStmt's own Break case and
    // breakTargets_'s comment.
    ir::StmtPtr buildSwitch(const Stmt &s) {
        auto out = std::make_unique<ir::Stmt>();
        out->kind = ir::StmtKind::Block;
        out->line = s.line;
        pushScope();

        ir::ExprPtr controlExpr = buildExpr(*s.cond);
        ir::Slot *tempSlot = allocSlot(ir::SlotKind::Local, "__switch" + std::to_string(nextLabelId_));
        tempSlot->type = controlExpr->type;

        auto tempDecl = std::make_unique<ir::Stmt>();
        tempDecl->kind = ir::StmtKind::LocalDecl;
        tempDecl->line = s.line;
        tempDecl->declSlot = tempSlot;
        tempDecl->initOrReturnOrExpr = std::move(controlExpr);
        out->body.push_back(std::move(tempDecl));

        std::vector<int> caseLabels(s.cases.size());
        int defaultIndex = -1;
        for (size_t i = 0; i < s.cases.size(); i++) {
            caseLabels[i] = nextLabelId_++;
            if (s.cases[i].isDefault) defaultIndex = static_cast<int>(i);
        }
        int endLabel = nextLabelId_++;

        for (size_t i = 0; i < s.cases.size(); i++) {
            if (s.cases[i].isDefault) continue;

            auto tempVar = std::make_unique<ir::Expr>();
            tempVar->kind = ir::ExprKind::Var;
            tempVar->type = tempSlot->type;
            tempVar->slot = tempSlot;

            auto caseLit = std::make_unique<ir::Expr>();
            caseLit->kind = ir::ExprKind::IntLit;
            caseLit->intValue = s.cases[i].value;
            caseLit->type = tempSlot->type;

            ir::ExprPtr lhs = promote(std::move(tempVar), s.line);
            ir::ExprPtr rhs = promote(std::move(caseLit), s.line);
            Type common = commonType(lhs->type, rhs->type);
            lhs = wrapConvert(std::move(lhs), common, s.line);
            rhs = wrapConvert(std::move(rhs), common, s.line);

            auto cmp = std::make_unique<ir::Expr>();
            cmp->kind = ir::ExprKind::Binary;
            cmp->op = "==";
            cmp->type = Type{BaseType::Int, false};
            cmp->lhs = std::move(lhs);
            cmp->rhs = std::move(rhs);

            auto gotoCase = std::make_unique<ir::Stmt>();
            gotoCase->kind = ir::StmtKind::Goto;
            gotoCase->line = s.line;
            gotoCase->labelId = caseLabels[i];

            auto ifStmt = std::make_unique<ir::Stmt>();
            ifStmt->kind = ir::StmtKind::If;
            ifStmt->line = s.line;
            ifStmt->cond = std::move(cmp);
            ifStmt->thenStmt = std::move(gotoCase);
            out->body.push_back(std::move(ifStmt));
        }

        auto gotoDefaultOrEnd = std::make_unique<ir::Stmt>();
        gotoDefaultOrEnd->kind = ir::StmtKind::Goto;
        gotoDefaultOrEnd->line = s.line;
        gotoDefaultOrEnd->labelId = defaultIndex >= 0 ? caseLabels[defaultIndex] : endLabel;
        out->body.push_back(std::move(gotoDefaultOrEnd));

        breakTargets_.push_back(endLabel);
        for (size_t i = 0; i < s.cases.size(); i++) {
            auto label = std::make_unique<ir::Stmt>();
            label->kind = ir::StmtKind::Label;
            label->line = s.line;
            label->labelId = caseLabels[i];
            out->body.push_back(std::move(label));
            for (const auto &stmt : s.cases[i].body) out->body.push_back(buildStmt(*stmt));
        }
        breakTargets_.pop_back();

        auto endLabelStmt = std::make_unique<ir::Stmt>();
        endLabelStmt->kind = ir::StmtKind::Label;
        endLabelStmt->line = s.line;
        endLabelStmt->labelId = endLabel;
        out->body.push_back(std::move(endLabelStmt));

        popScope();
        return out;
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
    // What a bare `break` currently resolves to, innermost last: -1 for a
    // loop (deferred to codegen's own per-backend loop-break-label stack,
    // unchanged from before `switch` existed), or a switch's own Lend
    // labelId - see buildStmt's Break case and buildSwitch.
    std::vector<int> breakTargets_;
    int nextLabelId_ = 0;  // ir::Stmt::labelId's own counter - see buildSwitch
    // name -> labelId for the function currently being built - see
    // collectLabels(). User `goto`/labels have function scope (not block
    // scope, unlike a C++ variable), so this is reset once per function,
    // not pushed/popped per block the way scopes_ is.
    std::unordered_map<std::string, int> currentFunctionLabels_;
    TypeWidths widths_{};
    Type currentReturnType_;  // the function currently being built - see StmtKind::Return
    bool currentFunctionIsVoid_ = false;  // the function currently being built returns no value
    std::string currentFunctionName_;  // ditto - used to mangle a 'static' local's own Global slot name
    int nextStaticLocalId_ = 0;  // ditto - disambiguates two same-named 'static' locals in one function
    std::string currentSourceFile_;  // ditto - stamped onto a StringLit's own synthetic global
    // tag -> resolved layout, for this compile only - see registerStructs()/resolveStructType().
    std::unordered_map<std::string, StructLayout> structsByTag_;
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

}  // namespace rcc
