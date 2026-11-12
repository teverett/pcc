#include "optimizers/address_hoisting/AddressHoisting.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rcc {

namespace {

// A real array's own address, re-derived from scratch at every use today -
// the thing this pass hoists. `e->lhs` is the index for all three (never
// null for the Assign/CompoundAssign cases, which is how IRBuilder.cpp
// itself tells "slot = rhs" from "slot[lhs] = rhs").
bool isArrayBaseOccurrence(const ir::Expr *e) {
    if (!e || !e->slot || e->slot->arraySize <= 0) return false;
    if (e->kind == ir::ExprKind::Index) return true;
    if ((e->kind == ir::ExprKind::Assign || e->kind == ir::ExprKind::CompoundAssign) && e->lhs) return true;
    return false;
}

void collectExpr(ir::Expr *e, std::unordered_map<ir::Slot *, std::vector<ir::Expr *>> &occurrences) {
    if (!e) return;

    // `&arr[idx]` needs the element's own real address, computed by
    // codegen's dedicated addressable-operand path for `Unary "&"` over
    // an `Index` - not this pass's general "dereference a hoisted
    // pointer" rewrite, which that path doesn't recognize. Same
    // exclusion LocalCSE.cpp makes for the same reason; the index
    // sub-expression still recurses normally.
    if (e->kind == ir::ExprKind::Unary && e->op == "&") {
        if (e->rhs && e->rhs->kind == ir::ExprKind::Index) {
            collectExpr(e->rhs->lhs.get(), occurrences);
        }
        return;
    }

    if (isArrayBaseOccurrence(e)) occurrences[e->slot].push_back(e);
    collectExpr(e->lhs.get(), occurrences);
    collectExpr(e->rhs.get(), occurrences);
    collectExpr(e->elseExpr.get(), occurrences);
    for (auto &a : e->args) collectExpr(a.get(), occurrences);
}

void collectStmt(ir::Stmt *s, std::unordered_map<ir::Slot *, std::vector<ir::Expr *>> &occurrences) {
    if (!s) return;
    switch (s->kind) {
        case ir::StmtKind::Block:
            for (auto &c : s->body) collectStmt(c.get(), occurrences);
            return;
        case ir::StmtKind::If:
            collectExpr(s->cond.get(), occurrences);
            collectStmt(s->thenStmt.get(), occurrences);
            collectStmt(s->elseStmt.get(), occurrences);
            return;
        case ir::StmtKind::While:
            collectExpr(s->cond.get(), occurrences);
            collectStmt(s->loopBody.get(), occurrences);
            collectStmt(s->postStmt.get(), occurrences);
            return;
        case ir::StmtKind::Return:
        case ir::StmtKind::LocalDecl:
        case ir::StmtKind::ExprStmt:
            collectExpr(s->initOrReturnOrExpr.get(), occurrences);
            return;
        case ir::StmtKind::Break:
        case ir::StmtKind::Continue:
        case ir::StmtKind::Empty:
        case ir::StmtKind::Asm:
        case ir::StmtKind::Label:
        case ir::StmtKind::Goto:
            return;
    }
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

// Matches IRBuilder.cpp's own `promote()`: a sub-Int-rank operand becomes
// signed Int before scaling.
ir::ExprPtr promote(ir::ExprPtr e, int line) {
    if (rank(e->type.base) < rank(BaseType::Int)) {
        return wrapConvert(std::move(e), Type{BaseType::Int, false}, line);
    }
    return e;
}

ir::ExprPtr makeVarRead(ir::Slot *slot, int line) {
    auto v = std::make_unique<ir::Expr>();
    v->kind = ir::ExprKind::Var;
    v->line = line;
    v->slot = slot;
    v->type = slot->type;
    return v;
}

// Scales a promoted index by the array's element width, matching
// IRBuilder.cpp's own `scaleStepForPointer()` convention (a pointer step
// arrives pre-scaled by pointee width, so codegen never inspects it).
ir::ExprPtr scaleIndex(ir::ExprPtr idx, int elemWidth, int line) {
    idx = promote(std::move(idx), line);
    if (elemWidth == 1) return idx;
    auto factor = std::make_unique<ir::Expr>();
    factor->kind = ir::ExprKind::IntLit;
    factor->line = line;
    factor->type = idx->type;
    factor->intValue = elemWidth;
    auto scaled = std::make_unique<ir::Expr>();
    scaled->kind = ir::ExprKind::Binary;
    scaled->line = line;
    scaled->op = "*";
    scaled->type = idx->type;
    scaled->lhs = std::move(idx);
    scaled->rhs = std::move(factor);
    return scaled;
}

ir::ExprPtr makePointerSum(ir::Slot *ptrTemp, ir::ExprPtr scaledIdx, Type ptrType, int line) {
    auto sum = std::make_unique<ir::Expr>();
    sum->kind = ir::ExprKind::Binary;
    sum->line = line;
    sum->op = "+";
    sum->type = ptrType;
    sum->lhs = makeVarRead(ptrTemp, line);
    sum->rhs = std::move(scaledIdx);
    return sum;
}

ir::Slot *makeAddrTemp(ir::Program &prog, int &counter, Type ptrType) {
    auto slot = std::make_unique<ir::Slot>();
    slot->id = static_cast<int>(prog.slots.size());  // every Slot id must be unique
    slot->sourceName = "__addrhoist" + std::to_string(counter++);
    slot->kind = ir::SlotKind::Local;
    slot->type = ptrType;
    ir::Slot *raw = slot.get();
    prog.slots.push_back(std::move(slot));
    return raw;
}

// `&arraySlot[0]` - the array's own base address, the exact shape
// IRBuilder.cpp itself builds for a source-level `&arr[i]`.
ir::ExprPtr makeArrayBaseAddr(ir::Slot *arraySlot, Type ptrType, int line) {
    auto zero = std::make_unique<ir::Expr>();
    zero->kind = ir::ExprKind::IntLit;
    zero->line = line;
    zero->type = Type{BaseType::Int, false};
    zero->intValue = 0;

    auto index = std::make_unique<ir::Expr>();
    index->kind = ir::ExprKind::Index;
    index->line = line;
    index->slot = arraySlot;
    index->type = arraySlot->type;
    index->lhs = std::move(zero);

    auto addrOf = std::make_unique<ir::Expr>();
    addrOf->kind = ir::ExprKind::Unary;
    addrOf->line = line;
    addrOf->op = "&";
    addrOf->type = ptrType;
    addrOf->rhs = std::move(index);
    return addrOf;
}

ir::StmtPtr makeAddrDecl(ir::Slot *temp, ir::ExprPtr init, int line) {
    auto decl = std::make_unique<ir::Stmt>();
    decl->kind = ir::StmtKind::LocalDecl;
    decl->line = line;
    decl->declSlot = temp;
    decl->initOrReturnOrExpr = std::move(init);
    return decl;
}

// Rewrites one array-base occurrence in place to go through `ptrTemp`
// instead of re-deriving `arraySlot`'s own address: the same
// Unary"*"-over-scaled-pointer-arithmetic (or DerefAssign/
// CompoundDerefAssign) shape IRBuilder.cpp already builds for an
// ordinary pointer variable's `p[i]`/`p[i]=`/`p[i] op=` - a path every
// backend already implements and tests, unlike `Index`/`Assign`/
// `CompoundAssign` with a pointer-typed (not real-array) slot, which no
// backend expects or handles (see IRBuilder.cpp's own `arraySize == 0 &&
// isPointer(slot->type)` diversion - that shape never reaches a backend
// today, so this pass doesn't invent a new one for it to see).
void rewriteOccurrence(ir::Expr *e, ir::Slot *ptrTemp, Type ptrType, int elemWidth) {
    int line = e->line;
    ir::ExprPtr idx = std::move(e->lhs);
    ir::ExprPtr scaled = scaleIndex(std::move(idx), elemWidth, line);
    ir::ExprPtr sum = makePointerSum(ptrTemp, std::move(scaled), ptrType, line);
    e->slot = nullptr;
    switch (e->kind) {
        case ir::ExprKind::Index:
            e->kind = ir::ExprKind::Unary;
            e->op = "*";
            e->rhs = std::move(sum);
            // e->type is unchanged: dereferencing ptrType yields the same
            // element type Index already had.
            break;
        case ir::ExprKind::Assign:
            e->kind = ir::ExprKind::DerefAssign;
            e->lhs = std::move(sum);
            // e->rhs (the value, already converted to the element type)
            // and e->type are untouched - only the addressing changed.
            break;
        case ir::ExprKind::CompoundAssign:
            e->kind = ir::ExprKind::CompoundDerefAssign;
            e->lhs = std::move(sum);
            // e->op, e->yieldOld, e->rhs, e->type are untouched.
            break;
        default:
            break;  // isArrayBaseOccurrence() only ever matches the three above.
    }
}

}  // namespace

bool AddressHoisting::run(ir::Program &prog, const TypeWidths &widths, const std::string &sourceFile) const {
    bool changed = false;
    int tempCounter = 0;
    for (auto &fb : prog.functionBodies) {
        if (!fb.body || fb.body->kind != ir::StmtKind::Block) continue;

        std::unordered_map<ir::Slot *, std::vector<ir::Expr *>> occurrences;
        collectStmt(fb.body.get(), occurrences);
        if (occurrences.empty()) continue;

        // Deterministic order, independent of unordered_map's own
        // iteration order: by Slot id, i.e. declaration/allocation order.
        std::vector<ir::Slot *> arraySlots;
        arraySlots.reserve(occurrences.size());
        for (auto &kv : occurrences) arraySlots.push_back(kv.first);
        std::sort(arraySlots.begin(), arraySlots.end(),
                   [](const ir::Slot *a, const ir::Slot *b) { return a->id < b->id; });

        for (ir::Slot *arraySlot : arraySlots) {
            std::vector<ir::Expr *> &uses = occurrences[arraySlot];
            if (uses.size() < 2) continue;  // a single use gains nothing from hoisting.

            Type ptrType = pointerTo(arraySlot->type);
            int elemWidth = widthOf(arraySlot->type, widths);
            ir::Slot *temp = makeAddrTemp(prog, tempCounter, ptrType);
            int declLine = uses.front()->line;
            ir::ExprPtr init = makeArrayBaseAddr(arraySlot, ptrType, declLine);
            fb.body->body.insert(fb.body->body.begin(), makeAddrDecl(temp, std::move(init), declLine));

            std::cerr << sourceFile << ": address-hoisting: fn " << fb.func->sourceName << ": hoisted `"
                       << arraySlot->sourceName << "`'s base address into `" << temp->sourceName << "` ("
                       << uses.size() << " uses)\n";

            for (ir::Expr *use : uses) {
                rewriteOccurrence(use, temp, ptrType, elemWidth);
            }
            changed = true;
        }
    }
    return changed;
}

}  // namespace rcc
