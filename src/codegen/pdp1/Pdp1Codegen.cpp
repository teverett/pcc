#include "Pdp1Codegen.h"

#include <stdexcept>
#include <utility>

#include "Pdp1Assembler.h"

namespace rcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// This target's native word is one's-complement: a negative value's bit
// pattern is the bitwise complement of its magnitude, not magnitude-plus-
// one the way two's complement is. Every literal this backend ever embeds
// in generated assembly - not just a user-written `-5` but every compile-
// time constant (frame offsets, array scaling, shift counts) - goes
// through this, never a bare two's-complement truncation, so hardware
// `add`/`sub` (which operate on one's-complement bit patterns) see values
// consistent with their own arithmetic.
long onesComplementWord(long v) {
    if (v >= 0) return v & 0777777L;
    long mag = (-v) & 0777777L;
    return (~mag) & 0777777L;
}

bool exprMakesCall(const ir::Expr *e) {
    if (!e) return false;
    if (e->kind == ir::ExprKind::Call || e->kind == ir::ExprKind::CallPtr) return true;
    if (exprMakesCall(e->lhs.get())) return true;
    if (exprMakesCall(e->rhs.get())) return true;
    for (const auto &a : e->args) {
        if (exprMakesCall(a.get())) return true;
    }
    return false;
}

bool slotIsWide(const ir::Slot *s) {
    return !isPointer(s->type) && (s->type.base == BaseType::Long || s->type.base == BaseType::LongLong);
}

bool stmtRulesOutLeaf(const ir::Stmt *s) {
    if (!s) return false;
    if (s->kind == ir::StmtKind::LocalDecl && s->declSlot &&
        (s->declSlot->arraySize > 0 || slotIsWide(s->declSlot))) {
        return true;
    }
    if (exprMakesCall(s->cond.get())) return true;
    if (exprMakesCall(s->initOrReturnOrExpr.get())) return true;
    for (const auto &c : s->body) {
        if (stmtRulesOutLeaf(c.get())) return true;
    }
    if (stmtRulesOutLeaf(s->thenStmt.get())) return true;
    if (stmtRulesOutLeaf(s->elseStmt.get())) return true;
    if (stmtRulesOutLeaf(s->loopBody.get())) return true;
    if (stmtRulesOutLeaf(s->postStmt.get())) return true;
    return false;
}
}  // namespace

bool Pdp1Codegen::computeIsLeaf(const ir::Function *fn, const ir::Stmt *body) {
    for (const ir::Slot *p : fn->params) {
        if (p->arraySize > 0 || slotIsWide(p)) return false;
    }
    return !stmtRulesOutLeaf(body);
}

std::string Pdp1Codegen::allocTemp() { return "T" + std::to_string(tempCounter_++); }

void Pdp1Codegen::declareTemp(const std::string &label) { trailer_.push_back(label + ": 0"); }

std::string Pdp1Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(onesComplementWord(e.value));
    return e.slot ? label(e.slot) : label(e.func);
}

void Pdp1Codegen::declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    trailer_.push_back(label + ": " + arrayElemText(values[0]));
    for (size_t i = 1; i < values.size(); i++) {
        trailer_.push_back(arrayElemText(values[i]));
    }
}

std::string Pdp1Codegen::literalLabel(long value) {
    auto it = literalCache_.find(value);
    if (it != literalCache_.end()) return it->second;
    std::string lbl = "N" + std::to_string(litCounter_++);
    literalCache_[value] = lbl;
    trailer_.push_back(lbl + ": " + std::to_string(onesComplementWord(value)));
    return lbl;
}

std::string Pdp1Codegen::newLabel() { return "J" + std::to_string(labelCounter_++); }

// AC += v (v>=0: `add`) or AC -= -v (v<0: `sub`) - see the class comment:
// this backend never needs a negative-valued literal cell for compile-time
// offset arithmetic, since hardware `sub` does the negating for free.
void Pdp1Codegen::addConst(long v) {
    if (v >= 0) {
        code_ << "   add " << literalLabel(v) << "\n";
    } else {
        code_ << "   sub " << literalLabel(-v) << "\n";
        normalizeNegZero();
    }
}

void Pdp1Codegen::negateAC() {
    code_ << "   cma\n";
    normalizeNegZero();
}

// The all-ones bit pattern is this target's "negative zero" - mathematically
// 0, but a different bit pattern from all-zeros, which a raw `sad`/`sza`
// zero/equality test can't see through. Hardware `add` already normalizes
// this away on its own (verified against SIMH's decode logic - see
// Pdp1Codegen.h's class comment); `sub` and `cma` do not, so every one of
// this backend's own `sub`/`cma` emissions runs through here immediately
// after, keeping every live value (AC, a slot, a temp) in normal form -
// nothing downstream (sad/sza/sma/sza on a stored value) ever has to think
// about which zero it's looking at.
void Pdp1Codegen::normalizeNegZero() {
    std::string Lclean = newLabel(), Lskip = newLabel();
    code_ << "   sad __mone\n";
    code_ << "   jmp " << Lclean << "\n";
    code_ << "   jmp " << Lskip << "\n";
    code_ << Lclean << ":\n   lac __zero\n";
    code_ << Lskip << ":\n";
}

void Pdp1Codegen::genWideUnsupported(int line, const std::string &what) {
    err(line, "'long'/'long long' " + what +
                  " isn't supported on the pdp1 backend: a correct multi-word carry chain on this "
                  "one's-complement machine (which has no link/carry bit at all, unlike pdp7's "
                  "two's-complement-with-link architecture) would need its own from-scratch, independently "
                  "verified algorithm - see Pdp1Codegen.h's class comment");
}

std::string Pdp1Codegen::generate(const ir::Program &prog, bool objectMode) {
    (void)objectMode;  // this backend has no separate-compilation/object mode - always whole-program
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    std::string homeFile = mainFn ? mainFn->sourceFile
                            : !prog.functionBodies.empty() ? prog.functionBodies.front().func->sourceFile
                            : !prog.slots.empty() ? prog.slots.front()->sourceFile
                                                   : std::string();

    if (mainFn) {
        size_t rangeStart = static_cast<size_t>(code_.tellp());
        code_ << "   jsp " << label(mainFn) << "  ; call main\n";
        code_ << "   hlt\n";
        codeRanges.push_back({mainFn->sourceFile, rangeStart, static_cast<size_t>(code_.tellp())});
    }

    for (const auto &fb : prog.functionBodies) {
        size_t fCodeStart = static_cast<size_t>(code_.tellp());
        size_t fTrailerStart = trailer_.size();
        if (fb.func->hasAddress) {
            code_ << "__saveloc = .\n";
            code_ << ". = ..+" << fb.func->address << "\n";
        }
        genFunction(fb);
        if (fb.func->hasAddress) code_ << ". = __saveloc\n";
        codeRanges.push_back({fb.func->sourceFile, fCodeStart, static_cast<size_t>(code_.tellp())});
        trailerRanges.push_back({fb.func->sourceFile, fTrailerStart, trailer_.size()});
    }

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (slot->isExternal) continue;
        size_t gTrailerStart = trailer_.size();
        if (slot->hasAddress) {
            trailer_.push_back("__saveloc = .");
            trailer_.push_back(". = ..+" + std::to_string(slot->address));
        }
        if (slot->arraySize > 0) {
            int elemW = widthOfType(slot->type);
            bool first = true;
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                const ir::ArrayInitElem &elem = slot->arrayInit[i];
                for (int j = 0; j < elemW; j++) {
                    std::string text =
                        (!elem.isSymbol) ? std::to_string(onesComplementWord(elem.value >> (18 * j)))
                                          : (j == 0 ? arrayElemText(elem) : "0");
                    if (first) {
                        trailer_.push_back(label(slot.get()) + ": " + text + "  ; " + slot->sourceName);
                        first = false;
                    } else {
                        trailer_.push_back(text);
                    }
                }
            }
        } else {
            int w = widthOfType(slot->type);
            long v = slot->hasInit ? slot->initValue : 0;
            for (int j = 0; j < w; j++) {
                std::string text = std::to_string(onesComplementWord(v >> (18 * j)));
                trailer_.push_back((j == 0 ? label(slot.get()) + ": " : "") + text +
                                    (j == 0 ? "  ; " + slot->sourceName : ""));
            }
        }
        if (slot->hasAddress) trailer_.push_back(". = __saveloc");
        trailerRanges.push_back({slot->sourceFile, gTrailerStart, trailer_.size()});
    }

    size_t sharedStart = trailer_.size();
    ensureRuntimeRoutines();
    trailerRanges.push_back({homeFile, sharedStart, trailer_.size()});

    std::string codeStr = code_.str();
    perFileAsm_.clear();
    for (const auto &[file, s, e] : codeRanges) perFileAsm_[file] += codeStr.substr(s, e - s);
    for (const auto &[file, s, e] : trailerRanges) {
        for (size_t i = s; i < e; i++) perFileAsm_[file] += trailer_[i] + "\n";
    }

    std::ostringstream out;
    out << codeStr;
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

std::map<std::string, std::string> Pdp1Codegen::perFileAsm() const { return perFileAsm_; }

void Pdp1Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ":  ; fn " << fn.func->sourceName << "\n";
    // `jsp` just set AC to the return address and jumped here - it must be
    // tucked away before anything else touches AC (see the class comment).
    code_ << "   dac " << retAddrCell() << "\n";
    trailer_.push_back(retAddrCell() + ": 0");

    currentFunctionIsLeaf_ = computeIsLeaf(fn.func, fn.body.get());
    if (currentFunctionIsLeaf_) {
        emitLeafParamBinding(fn.func);
        genStmt(*fn.body);
        emitLeafReturn(/*hasValue=*/false);
    } else {
        emitPrologue(fn.func);
        genStmt(*fn.body);
        emitEpilogueAndReturn(/*hasValue=*/false);
    }
}

void Pdp1Codegen::pushAC() {
    code_ << "   dac __stmp\n";
    code_ << "   lac __sp\n";
    code_ << "   sub __one\n";
    normalizeNegZero();
    code_ << "   dac __sp\n";
    code_ << "   lac __stmp\n";
    code_ << "   dac __sp i\n";
}

void Pdp1Codegen::popAC() {
    code_ << "   lac __sp i\n";
    code_ << "   dac __stmp\n";
    code_ << "   lac __sp\n";
    code_ << "   add __one\n";
    code_ << "   dac __sp\n";
    code_ << "   lac __stmp\n";
}

// Entry's "dac retAddrCell()" (genFunction()) already captured the return
// address; from here this is the same shape as pdp7's own prologue.
void Pdp1Codegen::emitPrologue(const ir::Function *fn) {
    code_ << "   lac " << retAddrCell() << "\n";
    pushAC();
    code_ << "   lac __fp\n";
    pushAC();
    code_ << "   lac __sp\n";
    code_ << "   dac __fp\n";
    if (fn->frameSize > 0) {
        code_ << "   lac __fp\n";
        addConst(-static_cast<long>(fn->frameSize));
        code_ << "   dac __sp\n";
    }
}

void Pdp1Codegen::emitEpilogueAndReturn(bool hasValue) {
    if (hasValue) code_ << "   dac __retval\n";
    code_ << "   lac __fp\n";
    code_ << "   dac __sp\n";
    popAC();
    code_ << "   dac __fp\n";
    popAC();
    code_ << "   dac " << retAddrCell() << "\n";
    if (hasValue) code_ << "   lac __retval\n";
    code_ << "   jmp " << retAddrCell() << " i\n";
}

void Pdp1Codegen::emitLeafParamBinding(const ir::Function *fn) {
    for (ir::Slot *p : fn->params) {
        code_ << "   lac __sp\n";
        addConst(p->paramIndex);
        code_ << "   dac __addr\n";
        code_ << "   lac __addr i\n";
        trailer_.push_back(leafLabel(p) + ": 0");
        code_ << "   dac " << leafLabel(p) << "\n";
    }
}

void Pdp1Codegen::emitLeafReturn(bool hasValue) {
    (void)hasValue;
    code_ << "   jmp " << retAddrCell() << " i\n";
}

void Pdp1Codegen::emitLoadSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "   law " << label(slot) << "\n";
        return;
    }
    if (currentFunctionIsLeaf_) {
        code_ << "   law " << leafLabel(slot) << "\n";
        return;
    }
    long offset = (slot->kind == ir::SlotKind::Param) ? static_cast<long>(slot->frameOffset)
                                                       : -localBaseOffset(slot);
    code_ << "   lac __fp\n";
    addConst(offset);
}

void Pdp1Codegen::emitLoadSlot(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "   lac " << label(slot) << "\n";
        narrowAC(slot->type);
        return;
    }
    if (currentFunctionIsLeaf_) {
        code_ << "   lac " << leafLabel(slot) << "\n";
        narrowAC(slot->type);
        return;
    }
    emitLoadSlotAddress(slot);
    code_ << "   dac __addr\n";
    code_ << "   lac __addr i\n";
    narrowAC(slot->type);
}

void Pdp1Codegen::emitStoreSlot(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "   dac " << label(slot) << "\n";
        return;
    }
    if (currentFunctionIsLeaf_) {
        code_ << "   dac " << leafLabel(slot) << "\n";
        return;
    }
    code_ << "   dac __stmp\n";
    emitLoadSlotAddress(slot);
    code_ << "   dac __addr\n";
    code_ << "   lac __stmp\n";
    code_ << "   dac __addr i\n";
}

void Pdp1Codegen::emitSlotBaseAddress(const ir::Slot *slot) {
    if (slot->arraySize > 0) {
        emitLoadSlotAddress(slot);
    } else {
        emitLoadSlot(slot);
    }
}

long Pdp1Codegen::localBaseOffset(const ir::Slot *slot) { return ir::localLowOffset(*slot, typeWidths()); }

void Pdp1Codegen::emitLocalArrayInit(const ir::Slot *slot) {
    int elemW = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        for (int j = 0; j < elemW; j++) {
            if (!elem.isSymbol) {
                // literalLabel() itself applies onesComplementWord() - do
                // not pre-mask `elem.value` with a plain two's-complement
                // `&0777777` here, which would bake in the wrong bit
                // pattern for a negative value before that conversion runs.
                code_ << "   lac " << literalLabel(elem.value >> (18 * j)) << "\n";
            } else if (j > 0) {
                code_ << "   lac __zero\n";
            } else if (elem.slot) {
                emitLoadSlotAddress(elem.slot);
            } else {
                code_ << "   law " << label(elem.func) << "\n";
            }
            code_ << "   dac __stmp\n";
            code_ << "   lac __fp\n";
            long offset = -localBaseOffset(slot) + static_cast<long>(i) * elemW + j;
            addConst(offset);
            code_ << "   dac __addr\n";
            code_ << "   lac __stmp\n";
            code_ << "   dac __addr i\n";
        }
    }
}

void Pdp1Codegen::storeThroughStackedAddress() {
    code_ << "   dac __stmp2\n";
    popAC();
    code_ << "   dac __addr\n";
    code_ << "   lac __stmp2\n";
    code_ << "   dac __addr i\n";
}

std::string Pdp1Codegen::popAddressIntoScratch() {
    popAC();
    code_ << "   dac __addr\n";
    return "__addr";
}

void Pdp1Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            genCondBranch(*s.cond, s.elseStmt ? Lelse : Ldone);
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "   jmp " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel();
            std::string Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genCondBranch(*s.cond, Lend);
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "   jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "   jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "   jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            bool hasValue = s.initOrReturnOrExpr != nullptr;
            if (hasValue) genExpr(*s.initOrReturnOrExpr);
            if (currentFunctionIsLeaf_) {
                emitLeafReturn(hasValue);
            } else {
                emitEpilogueAndReturn(hasValue);
            }
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                emitLocalArrayInit(s.declSlot);
            } else if (isWide(s.declSlot->type)) {
                genWideUnsupported(s.line, "local variable");
            } else {
                if (currentFunctionIsLeaf_) {
                    trailer_.push_back(leafLabel(s.declSlot) + ": 0");
                }
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                } else {
                    code_ << "   lac __zero\n";
                }
                emitStoreSlot(s.declSlot);
            }
            return;
        }
        case ir::StmtKind::ExprStmt: {
            genExpr(*s.initOrReturnOrExpr);
            return;
        }
        case ir::StmtKind::Empty:
            return;
        case ir::StmtKind::Asm: {
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
        case ir::StmtKind::Label: {
            code_ << irLabel(s.labelId) << ":\n";
            return;
        }
        case ir::StmtKind::Goto: {
            code_ << "   jmp " << irLabel(s.labelId) << "\n";
            return;
        }
    }
}

// `to.base` names a pointer's pointee type, not its own, so narrowing AC as
// a `char` here would corrupt a real address.
//
// AC holds a full-word one's-complement VALUE that may be negative - unlike
// a two's-complement word, this target's negative bit pattern's own low 8
// bits are NOT that value's mod-256 truncation (two's-complement masking is
// only exact because of two's complement's clean mod-2^n structure; one's
// complement doesn't have it), so a negative value's byte is computed from
// the VALUE (magnitude, then `256 - magnitude mod 256`) rather than by
// masking AC's raw bits directly. A non-negative value's bit pattern
// equals its value in both schemes, so masking it is still exact - see the
// `Ldone` branch below, shared by both paths once each has produced the
// right pre-mask byte.
void Pdp1Codegen::narrowAC(Type to) {
    if (isPointer(to)) return;
    if (to.base != BaseType::Char) return;
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "   sma\n   jmp " << Ldone << "\n   jmp " << Lneg << "\n";
    code_ << Lneg << ":\n";
    code_ << "   cma\n";                                // AC = |V|
    code_ << "   and " << literalLabel(0377) << "\n";  // AC = |V| mod 256
    code_ << "   dac __stmp\n";
    code_ << "   lac " << literalLabel(256) << "\n";
    code_ << "   sub __stmp\n";  // 256 - (|V| mod 256); 256 itself (when the
                                 // mod result was 0) masks to 0 below, still
                                 // correct
    normalizeNegZero();
    code_ << Ldone << ":\n";
    code_ << "   and " << literalLabel(0377) << "\n";
    if (to.isUnsigned) return;
    code_ << "   dac __stmp\n";
    code_ << "   and " << literalLabel(0200) << "\n";
    std::string Lhineg = newLabel(), Lhipos = newLabel(), Lhidone = newLabel();
    code_ << "   sza\n   jmp " << Lhineg << "\n   jmp " << Lhipos << "\n";
    code_ << Lhineg << ":\n   lac __stmp\n";
    addConst(-256);
    code_ << "   jmp " << Lhidone << "\n";
    code_ << Lhipos << ":\n   lac __stmp\n";
    code_ << Lhidone << ":\n";
}

void Pdp1Codegen::genExpr(const ir::Expr &e) {
    if (isWide(e.type)) genWideUnsupported(e.line, "expression");
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "   lac " << literalLabel(e.intValue) << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            emitLoadSlot(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {
                if (isCallFree(*e.rhs)) {
                    computeElementAddressAC(e.slot, *e.lhs);
                    std::string T = allocTemp();
                    declareTemp(T);
                    code_ << "   dac " << T << "\n";
                    genExpr(*e.rhs);
                    code_ << "   dac " << T << " i\n";
                    return;
                }
                computeElementAddress(e.slot, *e.lhs);
                genExpr(*e.rhs);
                storeThroughStackedAddress();
                return;
            }
            genExpr(*e.rhs);
            emitStoreSlot(e.slot);
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                negateAC();
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);
                code_ << "   dac __addr\n";
                code_ << "   lac __addr i\n";
            } else if (e.op == "~") {
                // C's `~x` is the VALUE -x-1, same on every target
                // regardless of the hardware's own complement scheme -
                // `cma` alone only gives -x here (one's-complement
                // negate), so the extra `sub __one` finishes it: -x-1.
                genExpr(*e.rhs);
                code_ << "   cma\n";
                normalizeNegZero();
                code_ << "   sub __one\n";
                normalizeNegZero();
            } else {
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);
            if (isCallFree(*e.rhs)) {
                std::string T = allocTemp();
                declareTemp(T);
                code_ << "   dac " << T << "\n";
                genExpr(*e.rhs);
                code_ << "   dac " << T << " i\n";
                return;
            }
            pushAC();
            genExpr(*e.rhs);
            storeThroughStackedAddress();
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {
                if (isCallFree(*e.rhs)) {
                    computeElementAddressAC(e.slot, *e.lhs);
                    genCompoundThroughCell(e);
                    return;
                }
                computeElementAddress(e.slot, *e.lhs);
                genCompoundThroughStackedAddress(e);
                return;
            }
            genExpr(*e.rhs);
            std::string rhsCell = prepareCompoundRhs(e);
            emitLoadSlot(e.slot);
            if (e.yieldOld) pushAC();
            finishCompoundOp(e, rhsCell);
            narrowAC(e.type);
            emitStoreSlot(e.slot);
            if (e.yieldOld) popAC();
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);
            if (isCallFree(*e.rhs)) {
                genCompoundThroughCell(e);
                return;
            }
            pushAC();
            genCompoundThroughStackedAddress(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            genCondBranch(*e.lhs, Lelse);
            genExpr(*e.rhs);
            code_ << "   jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Call: {
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                pushAC();
                totalWords += 1;
            }
            code_ << "   jsp " << label(e.func) << "\n";
            if (totalWords > 0) {
                code_ << "   dac __retval\n";
                code_ << "   lac __sp\n";
                addConst(static_cast<long>(totalWords));
                code_ << "   dac __sp\n";
                code_ << "   lac __retval\n";
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            genExpr(*e.lhs);
            code_ << "   dac __addr\n";
            code_ << "   jsp __addr i\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No interrupt-mask instruction exists in the subset of the
            // instruction set this backend uses (see Pdp1Codegen.h's class
            // comment - no I/O support in this pass) - both are plain
            // no-ops here, the same as on a target with no real
            // interrupt-mask instruction of its own (e.g. i8008).
            (void)e.op;
            return;
        }
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
    }
}

void Pdp1Codegen::genConvert(const ir::Expr &e) {
    if (isWide(e.lhs->type) || isWide(e.type)) genWideUnsupported(e.line, "conversion");
    genExpr(*e.lhs);
    narrowAC(e.type);
}

std::optional<std::string> Pdp1Codegen::simpleCell(const ir::Expr &e) {
    if (e.kind == ir::ExprKind::IntLit) return literalLabel(e.intValue);
    if (e.kind != ir::ExprKind::Var || isWide(e.type)) return std::nullopt;
    const ir::Slot *slot = e.slot;
    if (!isPointer(slot->type) && slot->type.base == BaseType::Char) return std::nullopt;
    if (slot->kind == ir::SlotKind::Global) return label(slot);
    if (currentFunctionIsLeaf_) return leafLabel(slot);
    return std::nullopt;
}

bool Pdp1Codegen::isCallFree(const ir::Expr &e) {
    if (e.kind == ir::ExprKind::Call || e.kind == ir::ExprKind::CallPtr) return false;
    for (const ir::Expr *child : {e.lhs.get(), e.rhs.get(), e.elseExpr.get()}) {
        if (child && !isCallFree(*child)) return false;
    }
    for (const auto &arg : e.args) {
        if (!isCallFree(*arg)) return false;
    }
    return true;
}

// No RhsXform is needed on this target: `+`/`-` use native `add`/`sub`
// directly (no pre-negation), and `|` uses native `ior` directly (no
// De Morgan complement) - see genBinary()/genBitwise().
std::string Pdp1Codegen::genOperandPair(const ir::Expr &lhs, const ir::Expr &rhs, RhsXform xform,
                                         const std::string &dest) {
    (void)xform;
    if (dest.empty()) {
        if (rhs.kind == ir::ExprKind::IntLit) {
            genExpr(lhs);
            return literalLabel(rhs.intValue);
        }
        if (auto cell = simpleCell(rhs)) {
            genExpr(lhs);
            return *cell;
        }
    }
    std::string cell = dest.empty() ? "__stmp2" : dest;
    auto finishRhs = [&]() { code_ << "   dac " << cell << "\n"; };
    if (simpleCell(lhs)) {
        genExpr(rhs);
        finishRhs();
        genExpr(lhs);
        return cell;
    }
    genExpr(lhs);
    if (isCallFree(rhs)) {
        std::string T = allocTemp();
        declareTemp(T);
        code_ << "   dac " << T << "\n";
        genExpr(rhs);
        finishRhs();
        code_ << "   lac " << T << "\n";
        return cell;
    }
    pushAC();
    genExpr(rhs);
    finishRhs();
    popAC();
    return cell;
}

void Pdp1Codegen::computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr) {
    computeElementAddressAC(slot, idxExpr);
    pushAC();
}

void Pdp1Codegen::computeElementAddressAC(const ir::Slot *slot, const ir::Expr &idxExpr) {
    int elemW = widthOfType(slot->type);
    if (idxExpr.kind == ir::ExprKind::IntLit) {
        long offset = idxExpr.intValue * elemW;
        emitSlotBaseAddress(slot);
        if (offset != 0) addConst(offset);
        return;
    }
    genExpr(idxExpr);
    if (elemW > 1) {
        std::string Tidx = allocTemp();
        declareTemp(Tidx);
        code_ << "   dac " << Tidx << "\n";
        for (int i = 1; i < elemW; i++) code_ << "   add " << Tidx << "\n";
    }
    code_ << "   dac __stmp2\n";
    emitSlotBaseAddress(slot);
    code_ << "   add __stmp2\n";
}

void Pdp1Codegen::genIndex(const ir::Expr &e) {
    computeElementAddressAC(e.slot, *e.lhs);
    code_ << "   dac __addr\n";
    code_ << "   lac __addr i\n";
}

void Pdp1Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        emitLoadSlotAddress(operand.slot);
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        computeElementAddressAC(operand.slot, *operand.lhs);
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "   law " << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

std::string Pdp1Codegen::prepareCompoundRhs(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-" || op == "&" || op == "^" || op == "|") {
        code_ << "   dac __stmp2\n";
        return "__stmp2";
    }
    if (op == "*") {
        std::string Tr = allocTemp();
        declareTemp(Tr);
        code_ << "   dac " << Tr << "\n";
        return Tr;
    }
    if (op == "/" || op == "%") {
        code_ << "   dac __dm_b\n";
        return "__dm_b";
    }
    if (op == "<<" || op == ">>") {
        std::string N = allocTemp();
        declareTemp(N);
        code_ << "   dac " << N << "\n";
        return N;
    }
    err(e.line, "internal: unhandled compound-assignment operator '" + op + "'");
}

void Pdp1Codegen::finishCompoundOp(const ir::Expr &e, const std::string &rhsCell) {
    const std::string &op = e.op;
    if (op == "+") {
        code_ << "   add " << rhsCell << "\n";
        return;
    }
    if (op == "-") {
        code_ << "   sub " << rhsCell << "\n";
        normalizeNegZero();
        return;
    }
    if (op == "&" || op == "^") {
        code_ << "   " << (op == "&" ? "and" : "xor") << " " << rhsCell << "\n";
        return;
    }
    if (op == "|") {
        code_ << "   ior " << rhsCell << "\n";
        return;
    }
    if (op == "*") {
        needMul_ = true;
        code_ << "   dac __mul_a\n   lac " << rhsCell << "\n   dac __mul_b\n   jsp __mul\n";
        return;
    }
    if (op == "/" || op == "%") {
        code_ << "   dac __dm_a\n";
        callDivMod(e.type.isUnsigned);
        if (op == "%") code_ << "   lac __dm_a\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        std::string V = allocTemp();
        declareTemp(V);
        code_ << "   dac " << V << "\n";
        std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
        code_ << Lstart << ":\n";
        code_ << "   lac " << rhsCell << "\n";
        code_ << "   sza\n   jmp " << Lbody << "\n   jmp " << Lend << "\n";
        code_ << Lbody << ":\n";
        code_ << "   lac " << V << "\n";
        emitShiftOne(op, e.type.isUnsigned);
        code_ << "   dac " << V << "\n";
        code_ << "   lac " << rhsCell << "\n   sub __one\n";
        normalizeNegZero();
        code_ << "   dac " << rhsCell << "\n";
        code_ << "   jmp " << Lstart << "\n";
        code_ << Lend << ":\n";
        code_ << "   lac " << V << "\n";
        return;
    }
    err(e.line, "internal: unhandled compound-assignment operator '" + op + "'");
}

void Pdp1Codegen::genCompoundThroughCell(const ir::Expr &e) {
    std::string T = allocTemp();
    declareTemp(T);
    code_ << "   dac " << T << "\n";
    genExpr(*e.rhs);
    std::string rhsCell = prepareCompoundRhs(e);
    code_ << "   lac " << T << " i\n";
    if (e.yieldOld) pushAC();
    finishCompoundOp(e, rhsCell);
    narrowAC(e.type);
    code_ << "   dac " << T << " i\n";
    if (e.yieldOld) popAC();
}

void Pdp1Codegen::genCompoundThroughStackedAddress(const ir::Expr &e) {
    genExpr(*e.rhs);
    std::string rhsCell = prepareCompoundRhs(e);
    std::string addr = popAddressIntoScratch();
    code_ << "   lac " << addr << " i\n";
    if (e.yieldOld) pushAC();
    finishCompoundOp(e, rhsCell);
    narrowAC(e.type);
    code_ << "   dac " << addr << " i\n";
    if (e.yieldOld) popAC();
}

void Pdp1Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        std::string rhsCell = genOperandPair(*e.lhs, *e.rhs, RhsXform::None);
        code_ << "   add " << rhsCell << "\n";
        return;
    }
    if (op == "-") {
        std::string rhsCell = genOperandPair(*e.lhs, *e.rhs, RhsXform::None);
        code_ << "   sub " << rhsCell << "\n";
        normalizeNegZero();
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        return;
    }
    if (op == "/") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/false);
        return;
    }
    if (op == "%") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/true);
        return;
    }
    if (op == "&" || op == "^" || op == "|") {
        genBitwise(op, *e.lhs, *e.rhs);
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(op, *e.lhs, *e.rhs);
        return;
    }
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs);
}

void Pdp1Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEq(lhs, rhs, /*negate=*/false);
    if (op == "!=") return emitEq(lhs, rhs, /*negate=*/true);
    if (op == "<") return emitLt(lhs, rhs, /*negate=*/false);
    if (op == ">=") return emitLt(lhs, rhs, /*negate=*/true);
    if (op == ">") return emitLt(rhs, lhs, /*negate=*/false);
    if (op == "<=") return emitLt(rhs, lhs, /*negate=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

void Pdp1Codegen::emitEq(const ir::Expr &lhs, const ir::Expr &rhs, bool negate) {
    std::string rhsCell = genOperandPair(lhs, rhs, RhsXform::None);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sad " << rhsCell << "\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
    if (negate) code_ << "   xor __one\n";
}

// a<b via `sub`+sign-test when same-sign (safe: same-sign magnitudes can't
// overflow this target's representable range); decided directly when signs
// differ, avoiding the overflow a cross-sign subtract could hit. See
// Pdp1Codegen.h's class comment for this target's documented `unsigned`
// range ceiling (2^17-1) that this whole comparison family relies on.
void Pdp1Codegen::emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate) {
    bool unsignedCmp = lhs.type.isUnsigned;
    auto flip = [&]() {
        if (unsignedCmp) code_ << "   xor __signbit\n";
    };
    std::string A, B;
    auto toTemp = [&]() {
        std::string T = allocTemp();
        declareTemp(T);
        code_ << "   dac " << T << "\n";
        return T;
    };
    if (isCallFree(rhs)) {
        genExpr(lhs);
        flip();
        A = toTemp();
        genExpr(rhs);
        flip();
        B = toTemp();
    } else {
        genExpr(lhs);
        flip();
        if (isCallFree(rhs)) {
            A = toTemp();
            genExpr(rhs);
            flip();
            B = toTemp();
        } else {
            pushAC();
            genExpr(rhs);
            flip();
            B = toTemp();
            popAC();
            A = toTemp();
        }
    }
    std::string LaNeg = newLabel(), LaPos = newLabel();
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel(), Lsub = newLabel();
    // `sma` skips the immediately following instruction when AC<0: with
    // "sma\n jmp X\n jmp Y", X runs when AC>=0 and Y runs when AC<0 (the
    // skip jumps over X, landing on Y) - every branch below is ordered to
    // that rule, not to the "jmp true-case then jmp false-case" shape
    // used elsewhere in this file for an `sza`/`sad` test.
    code_ << "   lac " << A << "\n   sma\n   jmp " << LaPos << "\n   jmp " << LaNeg << "\n";
    // A<0: B>=0 decides A<B directly (true); B<0 (same sign) needs Lsub.
    code_ << LaNeg << ":\n   lac " << B << "\n   sma\n   jmp " << Ltrue << "\n   jmp " << Lsub << "\n";
    // A>=0: B>=0 (same sign) needs Lsub; B<0 decides A<B directly (false).
    code_ << LaPos << ":\n   lac " << B << "\n   sma\n   jmp " << Lsub << "\n   jmp " << Lfalse << "\n";
    code_ << Lsub << ":\n   lac " << A << "\n   sub " << B << "\n";
    normalizeNegZero();
    code_ << "   sma\n   jmp " << Lfalse << "\n   jmp " << Ltrue << "\n";
    code_ << Ltrue << ":\n   lac " << (negate ? "__zero" : "__one") << "\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac " << (negate ? "__one" : "__zero") << "\n";
    code_ << Ldone << ":\n";
}

void Pdp1Codegen::genCondBranch(const ir::Expr &cond, const std::string &Lfalse) {
    if (cond.kind == ir::ExprKind::Binary && cond.op == "&&") {
        genCondBranch(*cond.lhs, Lfalse);
        genCondBranch(*cond.rhs, Lfalse);
        return;
    }
    genExpr(cond);
    std::string Ltrue = newLabel();
    code_ << "   sza\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n";
}

void Pdp1Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lcheckb = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sza\n   jmp " << Lcheckb << "\n   jmp " << Lfalse << "\n";
    code_ << Lcheckb << ":\n";
    genExpr(rhs);
    code_ << "   sza\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
}

void Pdp1Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lcheckb = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sza\n   jmp " << Ltrue << "\n   jmp " << Lcheckb << "\n";
    code_ << Lcheckb << ":\n";
    genExpr(rhs);
    code_ << "   sza\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
}

void Pdp1Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sza\n   jmp " << Lfalse << "\n   jmp " << Ltrue << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
}

// Software sign-magnitude multiply (see pdp1RuntimeAsm()'s `__mul`): this
// target has no hardware multiply, so `*` always calls the shared runtime
// routine rather than inlining.
void Pdp1Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    needMul_ = true;
    // genOperandPair() leaves `lhs`'s value in AC and stores `rhs`'s value
    // into __mul_b; AC only needs to be stashed into __mul_a to complete
    // the pair.
    genOperandPair(lhs, rhs, RhsXform::None, "__mul_b");
    code_ << "   dac __mul_a\n";
    code_ << "   jsp __mul\n";
}

void Pdp1Codegen::ensureRuntimeRoutines() {
    std::istringstream lines(pdp1RuntimeAsm(needDivMod_, needUDivMod_, needMul_));
    for (std::string line; std::getline(lines, line);) trailer_.push_back(line);
}

void Pdp1Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genOperandPair(lhs, rhs, RhsXform::None, "__dm_b");
    code_ << "   dac __dm_a\n";
    callDivMod(lhs.type.isUnsigned);
    if (wantRemainder) code_ << "   lac __dm_a\n";
}

void Pdp1Codegen::callDivMod(bool isUnsigned) {
    if (isUnsigned) {
        needUDivMod_ = true;
        code_ << "   jsp __udivmod\n";
    } else {
        needDivMod_ = true;
        code_ << "   jsp __divmod\n";
    }
}

// "&"/"^" are real hardware instructions; "|" is also real hardware on this
// target (`ior`) - no De Morgan synthesis needed, unlike pdp7.
//
// Only verified for non-negative operands (same scoping as pdp7's own
// `mul`): these operate directly on AC's raw one's-complement bit
// pattern, which only equals a non-negative value's own two's-complement
// bit pattern (what C's `&`/`|`/`^` are specified against) when the sign
// bit is clear - unlike unary `~` (see genExpr()'s Unary case), there's
// no equivalent pure-arithmetic identity for a BINARY bitwise op that
// sidesteps this, so a negative operand here can give a value C wouldn't.
void Pdp1Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string rhsCell = genOperandPair(lhs, rhs, RhsXform::None);
    const char *instr = op == "&" ? "and" : (op == "^" ? "xor" : "ior");
    code_ << "   " << instr << " " << rhsCell << "\n";
}

// A single bit of shift/rotate, purely as bit manipulation (see
// Pdp1Codegen.h's method comment) - no dependence on this target's
// arithmetic (`add`/`sub`) semantics at all, so none of the one's-
// complement -0 concerns that apply elsewhere in this file apply here.
void Pdp1Codegen::emitShiftOne(const std::string &op, bool isUnsigned) {
    if (op == "<<") {
        code_ << "   ral\n   and __notbit0\n";
        return;
    }
    if (isUnsigned) {
        code_ << "   rar\n   and __notsignbit\n";
        return;
    }
    std::string Lneg = newLabel(), Lpos = newLabel(), Ldone = newLabel();
    code_ << "   sma\n   jmp " << Lpos << "\n   jmp " << Lneg << "\n";
    code_ << Lneg << ":\n   rar\n   ior __signbit\n   jmp " << Ldone << "\n";
    code_ << Lpos << ":\n   rar\n   and __notsignbit\n";
    code_ << Ldone << ":\n";
}

void Pdp1Codegen::emitConstantShift(const std::string &op, long count, bool isUnsigned) {
    for (long i = 0; i < count; i++) emitShiftOne(op, isUnsigned);
}

// No variable-count hardware shift is used on this target (see
// emitShiftOne()'s comment) - constant and variable counts both just
// repeat the one-bit primitive, the constant case merely unrolled instead
// of looped.
void Pdp1Codegen::genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (rhs.kind == ir::ExprKind::IntLit && rhs.intValue >= 0 && rhs.intValue < 18) {
        genExpr(lhs);
        emitConstantShift(op, rhs.intValue, lhs.type.isUnsigned);
        return;
    }
    std::string N = allocTemp();
    declareTemp(N);
    genOperandPair(lhs, rhs, RhsXform::None, N);
    std::string V = allocTemp();
    declareTemp(V);
    code_ << "   dac " << V << "\n";
    std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
    code_ << Lstart << ":\n";
    code_ << "   lac " << N << "\n";
    code_ << "   sza\n   jmp " << Lbody << "\n   jmp " << Lend << "\n";
    code_ << Lbody << ":\n";
    code_ << "   lac " << V << "\n";
    emitShiftOne(op, lhs.type.isUnsigned);
    code_ << "   dac " << V << "\n";
    code_ << "   lac " << N << "\n   sub __one\n";
    normalizeNegZero();
    code_ << "   dac " << N << "\n";
    code_ << "   jmp " << Lstart << "\n";
    code_ << Lend << ":\n";
    code_ << "   lac " << V << "\n";
}

std::optional<std::string> Pdp1Codegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "list") return assemblePdp1(asmText, Pdp1OutputFormat::List);
    if (format == "rim") return assemblePdp1(asmText, Pdp1OutputFormat::Rim);
    return std::nullopt;
}

std::string pdp1RuntimeAsm(bool divMod, bool uDivMod, bool mul) {
    std::ostringstream o;
    // `__sp` starts at the top of the 4096-word (12-bit address field)
    // address space, `kPdp1StackReserve` words clear of
    // `kPdp1StackSafeLimit` - see Pdp1Assembler.h. `__signbit`/
    // `__notsignbit`/`__notbit0`/`__mone` are the fixed bit masks
    // emitShiftOne()/normalizeNegZero() use.
    o << "__one: 1\n__zero: 0\n__mone: 0777777\n"
         "__signbit: 0400000\n__notsignbit: 0377777\n__notbit0: 0777776\n"
         "__sp: 4095\n__fp: 0\n__stmp: 0\n__stmp2: 0\n__addr: 0\n__retval: 0\n";
    // `__eighteen` is shared by `__mul`'s and `__udivmod`'s own 18-iteration
    // loops - declared here, once, rather than only alongside whichever of
    // the two happens to be needed, since either one alone must still see
    // it defined.
    if (divMod || uDivMod || mul) o << "__eighteen: 18\n";
    if (divMod || uDivMod) o << "__dm_a: 0\n__dm_b: 0\n";
    if (mul) o << "__mul_a: 0\n__mul_b: 0\n__mul_s: 0\n__mul_p: 0\n__mul_n: 0\n";
    if (mul) {
        // Sign-magnitude multiply: strip each operand's sign, shift-add the
        // magnitudes (`__mul_a` doubling left each step, `__mul_b` shifting
        // right to expose its next bit), then reapply the combined sign.
        // Every step works on non-negative magnitudes only, so none of
        // this needs the `sub`-after-`add` normalization dance above - see
        // Pdp1Codegen.h's class comment on this target's documented
        // `unsigned`/magnitude range ceiling (2^17-1), which this relies on.
        o << "__mul:\n"
             "   dac __mul_ra\n"
             "   lac __mul_a\n   xor __mul_b\n   and __signbit\n   dac __mul_s\n"
             "   lac __mul_a\n   sma\n   jmp M1P\n   jmp M1N\n"
             "M1N:\n   lac __mul_a\n   cma\n   dac __mul_a\n"
             "M1P:\n"
             "   lac __mul_b\n   sma\n   jmp M2P\n   jmp M2N\n"
             "M2N:\n   lac __mul_b\n   cma\n   dac __mul_b\n"
             "M2P:\n"
             "   lac __zero\n   dac __mul_p\n"
             "   lac __eighteen\n   dac __mul_n\n"
             "MLOOP:\n"
             "   lac __mul_b\n   and __one\n"
             "   sza\n   jmp MADD\n   jmp MSHIFT\n"
             "MADD:\n"
             "   lac __mul_p\n   add __mul_a\n   dac __mul_p\n"
             "MSHIFT:\n"
             "   lac __mul_a\n   ral\n   and __notbit0\n   dac __mul_a\n"
             "   lac __mul_b\n   rar\n   and __notsignbit\n   dac __mul_b\n"
             "   lac __mul_n\n   sub __one\n   dac __mul_n\n"
             "   sza\n   jmp MLOOP\n   jmp MDONE\n"
             "MDONE:\n"
             "   lac __mul_s\n"
             "   sza\n   jmp MSNEG\n   jmp MSPOS\n"
             // `cma` on an exact-zero product (e.g. 0 * -5) lands on the
             // all-ones "negative zero" pattern - same fixup as __divmod's
             // own (see that routine's comment).
             "MSNEG:\n   lac __mul_p\n   cma\n   dac __mul_p\n"
             "   sad __mone\n   jmp MSZFIX\n   jmp MSZSKIP\n"
             "MSZFIX:\n   lac __zero\n   dac __mul_p\n"
             "MSZSKIP:\n"
             "MSPOS:\n"
             "   lac __mul_p\n   jmp __mul_ra i\n"
             "__mul_ra: 0\n";
    }
    if (divMod || uDivMod) {
        o << "__dm_r: 0\n__dm_q: 0\n__dm_tmp: 0\n__dm_bit: 0\n__dm_qbit: 0\n__dm_n: 0\n";
    }
    if (divMod || uDivMod) {
        // Sign-magnitude restoring binary division of two magnitudes - see
        // the comment above `__mul` for the same sign-magnitude strategy
        // and its documented range ceiling. Returns the quotient in AC and
        // leaves the remainder in `__dm_a` (both magnitudes here; sign is
        // reapplied by `__divmod` below, never by this routine itself).
        o << "__udivmod:\n"
             "   dac __udivmod_ra\n"
             "   lac __zero\n   dac __dm_r\n   lac __zero\n   dac __dm_q\n"
             "   lac __eighteen\n   dac __dm_n\n"
             "UDMLOOP:\n"
             "   lac __dm_a\n   ral\n   dac __dm_tmp\n   and __one\n   dac __dm_bit\n"
             "   lac __dm_tmp\n   and __notbit0\n   dac __dm_a\n"
             "   lac __dm_r\n   ral\n   and __notbit0\n   ior __dm_bit\n   dac __dm_r\n"
             "   lac __dm_r\n   sub __dm_b\n"
             "   sma\n   jmp UDMSUB\n   jmp UDMNOSUB\n"
             "UDMSUB:\n"
             "   dac __dm_r\n   lac __one\n   dac __dm_qbit\n   jmp UDMMERGE\n"
             "UDMNOSUB:\n"
             "   lac __zero\n   dac __dm_qbit\n"
             "UDMMERGE:\n"
             "   lac __dm_q\n   ral\n   and __notbit0\n   ior __dm_qbit\n   dac __dm_q\n"
             "   lac __dm_n\n   sub __one\n   dac __dm_n\n"
             "   sza\n   jmp UDMLOOP\n   jmp UDMDONE\n"
             "UDMDONE:\n"
             "   lac __dm_r\n   dac __dm_a\n"
             "   lac __dm_q\n   jmp __udivmod_ra i\n"
             "__udivmod_ra: 0\n";
    }
    if (divMod) {
        o << "__dm_s: 0\n__dm_qs: 0\n"
             "__divmod:\n"
             "   dac __divmod_ra\n"
             "   lac __dm_a\n   sma\n   jmp DMAP\n   jmp DMAN\n"
             "DMAN:\n   lac __one\n   dac __dm_s\n   lac __dm_a\n   cma\n   dac __dm_a\n   jmp DMAD\n"
             "DMAP:\n   lac __zero\n   dac __dm_s\n"
             "DMAD:\n"
             "   lac __dm_b\n   sma\n   jmp DMBP\n   jmp DMBN\n"
             "DMBN:\n   lac __one\n   xor __dm_s\n   dac __dm_qs\n   lac __dm_b\n   cma\n   dac __dm_b\n   jmp DMDIV\n"
             "DMBP:\n   lac __zero\n   xor __dm_s\n   dac __dm_qs\n"
             "DMDIV:\n"
             "   jsp __udivmod\n   dac __dm_q\n"
             // Each `cma` below can land on the all-ones "negative zero"
             // pattern (an exact quotient/remainder negated - see
             // Pdp1Codegen.h's class comment on normalizeNegZero()), so
             // each is followed by the same inline fixup that method
             // emits: `sad __mone` skips the following `lac __zero`
             // unless AC is exactly that pattern.
             "   lac __dm_qs\n   sza\n   jmp DMQNEG\n   jmp DMQPOS\n"
             "DMQNEG:\n   lac __dm_q\n   cma\n   dac __dm_q\n"
             "   sad __mone\n   jmp DMQZFIX\n   jmp DMQZSKIP\n"
             "DMQZFIX:\n   lac __zero\n   dac __dm_q\n"
             "DMQZSKIP:\n"
             "DMQPOS:\n"
             "   lac __dm_s\n   sza\n   jmp DMRNEG\n   jmp DMRPOS\n"
             "DMRNEG:\n   lac __dm_a\n   cma\n   dac __dm_a\n"
             "   sad __mone\n   jmp DMRZFIX\n   jmp DMRZSKIP\n"
             "DMRZFIX:\n   lac __zero\n   dac __dm_a\n"
             "DMRZSKIP:\n"
             "DMRPOS:\n"
             "   lac __dm_q\n   jmp __divmod_ra i\n"
             "__divmod_ra: 0\n";
    }
    return o.str();
}

}  // namespace rcc
