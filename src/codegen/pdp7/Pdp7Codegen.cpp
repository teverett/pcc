#include "Pdp7Codegen.h"

#include <stdexcept>
#include <utility>

#include "Pdp7Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// A PDP-7 word is 18 bits, and grammar/PccAsm.g4's own data-word
// syntax (a bare NUMBER - see its own comment) has no unary minus:
// unlike an *expression* (sign? atom, used in a real operand), a raw
// data word written out by declareArray()/literalLabel()/the globals
// loop below is just one token with no sign, so a negative compile-
// time constant - previously only ever produced by e.g. a literal
// global initializer, now also by ConstantFolding (see
// optimizers/constant_folding/ConstantFolding.h) - has to be written
// out as its own two's-complement 18-bit bit pattern instead of a
// signed decimal string. That's exactly the bit pattern real PDP-7
// hardware would already compute for it at runtime, so every *read*
// of the word (lac, comparisons, etc.) still sees the same value as
// before - only how it's spelled in the generated assembly text
// changes.
long pdp7Word(long v) { return v & 0777777; }

// See Pdp7Codegen.h's own "Leaf fast path" comment for what this is
// for. `exprMakesCall` walks every sub-expression a Call/CallPtr
// could be hiding in; `stmtRulesOutLeaf` adds the one other
// disqualifier (a Local/Param array - excluded from the leaf path
// purely to keep its own static storage simple, not for any
// deeper safety reason the way "no calls" is).
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

// A `long`/`long long` Local/Param needs `widthOf(type)` consecutive
// words of storage and a width-aware address (the frame-based,
// non-leaf path already has both - see emitLoadSlotAddress()'s own
// frameOffset-based arithmetic), while the leaf fast path's own
// static-cell scheme (leafLabel()) and `__sp`-relative param reads
// (emitLeafParamBinding()) are both hardcoded to exactly one word
// per slot - so a wide Local/Param disqualifies the leaf path
// entirely, the same way a Local/Param array already does above.
// A pointer (ptrDepth>0) is always exactly one word, no matter what
// it points to - checked first so a `long*`/`long long*` slot (whose
// `.base` names its POINTEE's type, not its own - see Type.h's own
// header comment on Type::ptrDepth) isn't mistaken for an actual wide
// value here.
bool slotIsWide(const ir::Slot *s) {
    return !isPointer(s->type) && (s->type.base == BaseType::Long || s->type.base == BaseType::LongLong);
}

bool stmtRulesOutLeaf(const ir::Stmt *s) {
    if (!s) return false;
    if (s->kind == ir::StmtKind::LocalDecl && s->declSlot &&
        (s->declSlot->arraySize > 0 || slotIsWide(s->declSlot))) {
        return true;
    }
    // NOTE: an `asm{...}` block was excluded here for a while, on
    // the theory that raw inline assembly's opaque text might embed
    // its own assumptions this analysis can't see into. That theory
    // was wrong: the real bug behind the symptom that prompted it
    // (sys/s4.c's `dsktrans` hanging under a real pdp7/disk scenario)
    // was `__sp`'s own starting value colliding with ordinary
    // program code/data (see generate()'s own comment on `__sp:
    // 8191`) - a bug that hit leaf and non-leaf functions alike, not
    // anything specific to `dsktrans`, leaf-ness, or `asm{...}`.
    // With that fixed, re-verified directly that a leaf-eligible
    // function's own `asm{...}` block has no dependency on frame
    // mechanics to break: every reference to a Param/Local inside
    // one is an ordinary bare-expression statement (e.g. `dsktrans`'s
    // own "wordCount;"), which already goes through this file's
    // normal emitLoadSlot()/emitStoreSlot() (leaf-aware either way) -
    // the raw instruction text itself never names `__fp`/`__sp`
    // directly. No exclusion needed.
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

bool Pdp7Codegen::computeIsLeaf(const ir::Function *fn, const ir::Stmt *body) {
    for (const ir::Slot *p : fn->params) {
        if (p->arraySize > 0 || slotIsWide(p)) return false;
    }
    return !stmtRulesOutLeaf(body);
}

std::string Pdp7Codegen::allocTemp() { return "T" + std::to_string(tempCounter_++); }

void Pdp7Codegen::declareTemp(const std::string &label) { trailer_.push_back(label + ": 0"); }

// A literal element is this backend's own two's-complement word text
// (see pdp7Word's comment); a symbol element is the referenced Slot's
// or Function's own label, written out bare - the shared assembler
// (Pdp7Assembler.cpp's buildSegment) treats a bare identifier with no
// operands as a one-syllable expression evaluating to that symbol's
// resolved address, the exact same mechanism "jmp i F3" ORs several
// syllables together with, just with only one syllable here.
std::string Pdp7Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(pdp7Word(e.value));
    return e.slot ? label(e.slot) : label(e.func);
}

void Pdp7Codegen::declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values) {
    // One value per line, label on the first - the shared grammar
    // (../../../../grammar/PccAsm.g4) uses ';' for comments, not as a
    // multi-statement-per-line separator the way as.s's own
    // convention for consecutive words (e.g.
    // src-assem/tests/fork_test.s's "childmsg: <ch>; <il>; <d 012")
    // does - see that grammar's own header comment.
    if (values.empty()) return;
    trailer_.push_back(label + ": " + arrayElemText(values[0]));
    for (size_t i = 1; i < values.size(); i++) {
        trailer_.push_back(arrayElemText(values[i]));
    }
}

std::string Pdp7Codegen::literalLabel(long value) {
    auto it = literalCache_.find(value);
    if (it != literalCache_.end()) return it->second;
    std::string lbl = "N" + std::to_string(litCounter_++);
    literalCache_[value] = lbl;
    trailer_.push_back(lbl + ": " + std::to_string(pdp7Word(value)));
    return lbl;
}

std::string Pdp7Codegen::newLabel() { return "J" + std::to_string(labelCounter_++); }

// `char`/`short`/`int` are all exactly one pdp7 word (see
// typeWidths()) - a `short`/`int` is the native word itself, nothing
// to truncate, but `char` still has to behave like a real 8-bit C
// `char` even though its own STORAGE here is a full, untruncated
// word (every slot, regardless of declared type, occupies exactly
// one word - there's no sub-word addressing on this target at all).
// The fix is to truncate the VALUE, not the storage: masking (and,
// for a signed char, re-sign-extending) happens only here, at every
// point this value is actually interpreted as a char (an explicit
// cast, or loading a char Slot's own full-word storage back out via
// emitLoadSlot()) - never at store time, since a stray untruncated
// upper bit left behind by, say, assigning 300 to a char is
// harmless noise no later read of that same slot can ever observe
// (every read already goes through this same masking step).
void Pdp7Codegen::narrowAC(Type to) {
    // A pointer (ptrDepth>0) is always exactly one native word, no
    // matter what it points to - `to.base` here names the POINTEE's
    // type, not the pointer's own (see Type.h's own header comment),
    // so narrowing/sign-extending AC as if it held an actual `char`
    // value would corrupt a real address (e.g. storing `char *cp`
    // would silently truncate/sign-extend cp's own 18-bit address
    // down to 8 bits).
    if (isPointer(to)) return;
    if (to.base != BaseType::Char) return;
    code_ << "   and " << literalLabel(0377) << "\n";
    if (to.isUnsigned) return;
    code_ << "   dac __stmp\n";
    code_ << "   and " << literalLabel(0200) << "\n";
    std::string Lneg = newLabel(), Lpos = newLabel(), Ldone = newLabel();
    code_ << "   sza\n   jmp " << Lneg << "\n   jmp " << Lpos << "\n";
    code_ << Lneg << ":\n   lac __stmp\n   tad " << literalLabel(-256) << "\n   jmp " << Ldone << "\n";
    code_ << Lpos << ":\n   lac __stmp\n";
    code_ << Ldone << ":\n";
}

// Placement (`@addr` in source - see IR.h's Slot::hasAddress): a
// placed function/global is still emitted in its normal position in
// this backend's output (the program's own declaration order, same
// as an unplaced one), but wrapped in a save/jump/restore of
// `pdp7as`'s location-counter symbol ("." - see ../../../../docs/ir.md's
// "Fixed absolute placement"): `__saveloc = .` stashes the counter
// as an ordinary assignment, `. = ..+addr` jumps it to the placed
// address, the placed item is emitted there, then `. = __saveloc`
// jumps back so whatever comes next in program order resumes exactly
// where it would have without the placement at all.
//
// `addr` is an offset from `..` (pdp7as's relocation-base symbol,
// matching the real kernel's own "orig" convention - e.g.
// `../../../../src-assem/sys/s1.s`'s `. = orig+020`), NOT a true
// absolute address from 0, for a reason that cost a real debugging
// session to find: `pdp7as`'s `rim`/`ptr` punch formats (Assembler.cpp's
// writeOutput) only emit memory from `..` upward - a word stored
// below it is silently absent from the actual binary (even though it
// shows correctly in an `a7out` *listing*, which dumps every address
// unconditionally) and so is never loaded onto real hardware at all;
// jumping to it (a placed function) or reading it (a placed global)
// then operates on whatever garbage/zero happens to already be
// there - confirmed directly: an earlier version of this comment
// placed things at a literal `. = addr` with addr < `..` and the
// resulting program hung the real `pdp7` simulator (a placed
// function's own code was never loaded, so calling it jumped into
// unloaded memory). `. = ..+addr` keeps every placed address >= `..`,
// which is always within the punched range, confirmed by checking
// the placed address's actual byte values with the simulator's own
// `e` (examine) command against a loaded tape, not just by reading
// `pdp7as`'s `a7out` listing. `__saveloc` is reused across every
// placement in a program (never nested - each wrap fully completes
// before the next begins), same naming convention as this backend's
// other synthetic symbols (`__one`, `__zero`, `__dm_*`).
std::string Pdp7Codegen::generate(const ir::Program &prog, bool objectMode) {
    // Every Slot/Function name is already resolved by IRBuilder - no
    // symbol table of our own, just emit code and declare storage.
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // IRBuilder guarantees a parameterless "main" exists; this is
    // just finding its Function* among the ones it already validated.

    // --dump-asm wants one file per *input*, not one combined dump -
    // see Codegen.h's perFileAsm(). Tracked here as (sourceFile,
    // start, end) ranges into code_ (by write position, via tellp -
    // code_ itself is never split or rewritten, so the single
    // combined asmText this function returns, and every real
    // --format other than "asm"/"<dump>", is completely unaffected)
    // and into trailer_ (by index range - every line a top-level
    // call below pushes is contiguous in trailer_, however deep the
    // declareTemp/literalLabel call that pushed it, since generation
    // is single-threaded and sequential). Built into perFileAsm_
    // only at the very end, from these ranges plus the still-intact
    // code_/trailer_.
    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    // Real separate compilation (`--format=coff` from a single file -
    // see IRBuilder.h's `allowExternal`) means `prog` may not define
    // `main` at all - an ordinary object file never does. `objectMode`
    // (see Codegen.h) means no object file ever emits this entry
    // sequence itself, even the one that happens to define `main` -
    // the linker is solely responsible for synthesizing exactly one,
    // once every object is merged and `main`'s linked address is
    // resolvable (see Linker.h). `homeFile` is purely for --dump-asm's
    // own per-file attribution of the handful of things with no
    // single owning Slot/Function (see below) - never a correctness
    // concern.
    std::string homeFile = mainFn ? mainFn->sourceFile
                            : !prog.functionBodies.empty() ? prog.functionBodies.front().func->sourceFile
                            : !prog.slots.empty() ? prog.slots.front()->sourceFile
                                                   : std::string();

    if (mainFn && !objectMode) {
        size_t rangeStart = static_cast<size_t>(code_.tellp());
        code_ << "   jms " << label(mainFn) << "  ; call main\n";
        code_ << "   hlt\n";
        codeRanges.push_back({mainFn->sourceFile, rangeStart, static_cast<size_t>(code_.tellp())});
    }

    for (const auto &fb : prog.functionBodies) {
        size_t fCodeStart = static_cast<size_t>(code_.tellp());
        size_t fTrailerStart = trailer_.size();
        // See the Placement comment above `generate()` for why this
        // save/jump/restore dance is needed, not just `. = addr`.
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
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name
        // (G_<name> - see label()) in code, letting the assembler's
        // own undefined-symbol handling defer it to a relocation.
        // Emitting one anyway would make every file that merely
        // *references* a global look like it also *defines* it.
        if (slot->isExternal) continue;
        size_t gTrailerStart = trailer_.size();
        if (slot->hasAddress) {
            trailer_.push_back("__saveloc = .");
            trailer_.push_back(". = ..+" + std::to_string(slot->address));
        }
        if (slot->arraySize > 0) {
            // One value per line, not a ";"-joined single line: the
            // shared grammar (../../../../grammar/PccAsm.g4) uses ';'
            // for comments, not as a multi-statement-per-line
            // separator the way the historical as.s/as7 dialect does
            // - see that grammar's own header comment. Each logical
            // element reserves `elemW` consecutive words (word 0/low
            // first - see computeElementAddress()'s own comment on
            // why this is the address every element read/write
            // already assumes), not always one, now that an element
            // can be `long`/`long long`; a symbol element (the
            // dispatch-table idiom) only ever fills word 0, zero-
            // padded above it, same as emitLocalArrayInit()'s own
            // comment on that case.
            int elemW = widthOfType(slot->type);
            bool first = true;
            for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                const ir::ArrayInitElem &elem = slot->arrayInit[i];
                for (int j = 0; j < elemW; j++) {
                    std::string text =
                        (!elem.isSymbol) ? std::to_string(pdp7Word(elem.value >> (18 * j)))
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
                std::string text = std::to_string(pdp7Word(v >> (18 * j)));
                trailer_.push_back((j == 0 ? label(slot.get()) + ": " : "") + text +
                                    (j == 0 ? "  ; " + slot->sourceName : ""));
            }
        }
        if (slot->hasAddress) trailer_.push_back(". = __saveloc");
        trailerRanges.push_back({slot->sourceFile, gTrailerStart, trailer_.size()});
    }

    // Shared backend-internal housekeeping (literal/temp cells
    // already pushed above are attributed to whichever function
    // used them; this is the bit with no single owning Slot/
    // Function) - attributed to main's own file, the same
    // conceptual "where the program starts" home the entry sequence
    // above gets.
    //
    // Only emitted here in WHOLE-PROGRAM mode (`!objectMode`), where
    // this is the one and only compiled file, so defining them here
    // is correct and sufficient. Under real separate compilation
    // (`objectMode`), every `.c` file is assembled as its own
    // completely independent object (see generate()'s own header
    // comment and main.cpp's "each file compiles completely
    // independently" note) - if every one of them *also* pushed its
    // own copy of these cells here, as this code used to do
    // unconditionally, each file's software stack/frame mechanism
    // would operate on its OWN private, file-local `__sp`/`__fp`/
    // `__retaddr`/etc. (this assembler resolves an in-file-defined
    // label like `__sp` to a local address entirely within that
    // file's own pass - see Pdp7Assembler.cpp's resolveSyllable/
    // `wasExternal` - so the linker's own cross-object symbol
    // merging, keyed on the `g_`/`p_`/`fn_`/`G_`/`P_`/`FN_` naming
    // convention - see GenericLinker.cpp's `isGlobalName` - never
    // even sees these as symbols needing unification, unlike a real
    // `FN_foo` call target), silently corrupting any call that
    // crosses an object-file boundary (confirmed directly: a
    // function in one file calling a function in another hangs,
    // jumping through a `__retaddr` that was never actually written
    // by the call that mattered, because the caller and callee
    // disagree on which `__retaddr` cell that even is). Under
    // `objectMode`, every file instead leaves these as genuine
    // external references (nothing here defines them, so
    // `allowExternal_` lets the reference through, same as any other
    // cross-file name), and `Pdp7LinkerFactory::makePdp7Linker()`'s
    // `config.entryObject` - already the one object every link
    // includes exactly once, regardless of how many user files there
    // are - supplies the one real, shared definition instead. See
    // that factory's own comment for why this is the right home for
    // it.
    if (!objectMode) {
        size_t sharedStart = trailer_.size();
        trailer_.push_back("__one: 1");
        trailer_.push_back("__zero: 0");
        // The software stack/frame mechanism every function now uses
        // (see Pdp7Codegen.h's own header comment and pushAC()/
        // emitPrologue()'s comments) - `__sp` starts at 8191 (017777
        // octal), the highest word this target's own 13-bit direct-
        // addressing field can ever reach (`kAddressFieldLimit` in
        // Pdp7LinkerFactory.cpp is 8192, one past it), and grows
        // downward from there, toward where this backend's own
        // code/data grows UP from (`kPdp7BaseAddress`, see
        // Pdp7Assembler.h) - the two regions share this same 8192-word
        // window and only collide if a program's combined code+data
        // and worst-case call depth together exceed it, same as any
        // other static allocation on this target already risks (see
        // `patchPdp7Or`'s own range check). This used to start at a
        // much lower, arbitrarily-picked 8100 instead - safe only for
        // a program whose own code+data stayed under roughly 4000
        // words, which real multi-file links comfortably exceed;
        // confirmed directly as a real bug, not just a theoretical
        // one, via a minimal repro (two pointer parameters, each
        // separately indexed, linked alongside an unrelated, large
        // enough file) that silently corrupted its own stack the
        // moment total program size passed 8100 words - every push
        // after that point overwrote real code/data instead of
        // reserved stack space. `__fp`'s own starting value is never
        // read before the very first call (to `main`) gives it a real
        // one, so 0 is just a placeholder. `__stmp`/`__addr`/
        // `__retval`/`__retaddr` are this mechanism's own shared
        // scratch cells - see their use sites' comments for why one
        // shared cell each is safe under recursion (every use is a
        // single short, call-free sequence).
        trailer_.push_back("__sp: 8191");
        trailer_.push_back("__fp: 0");
        trailer_.push_back("__stmp: 0");
        trailer_.push_back("__stmp2: 0");
        trailer_.push_back("__addr: 0");
        trailer_.push_back("__retval: 0");
        trailer_.push_back("__retaddr: 0");
        // A wide (`long`/`long long`) return value can't survive
        // emitEpilogueAndReturn()'s own "__sp = __fp" teardown the way
        // a narrow one survives in `__retval` (that reset discards
        // everything below `__fp`, including anything freshly pushed
        // on the real stack) - these four cells (enough for
        // `long long`'s own 4-word width - see typeWidths()) are the
        // wide equivalent, word i in `__retvalW<i>`. Shared globally
        // for the same reason every other "__"-prefixed cell here is:
        // each one's own live range is a single short, call-free
        // sequence (see emitEpilogueAndReturn()'s/emitLeafReturn()'s
        // own comments on exactly which).
        trailer_.push_back("__retvalW0: 0");
        trailer_.push_back("__retvalW1: 0");
        trailer_.push_back("__retvalW2: 0");
        trailer_.push_back("__retvalW3: 0");
        ensureDivModRoutine();
        trailerRanges.push_back({homeFile, sharedStart, trailer_.size()});
    }

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

std::map<std::string, std::string> Pdp7Codegen::perFileAsm() const { return perFileAsm_; }

void Pdp7Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ": 0  ; fn " << fn.func->sourceName << "\n";

    // See Pdp7Codegen.h's "Leaf fast path" comment - every other
    // method that treats a Local/Param differently reads this same
    // flag, set once here for whichever function is being generated.
    currentFunctionIsLeaf_ = computeIsLeaf(fn.func, fn.body.get());
    if (currentFunctionIsLeaf_) {
        emitLeafParamBinding(fn.func);
        genStmt(*fn.body);
        // Falls through here if the body doesn't end with an explicit
        // `return` on every path - same "default return" behavior as
        // this backend's old bare trailing `jmp i curFunctionLabel_`
        // had (dead code whenever every path already returned
        // explicitly).
        emitLeafReturn(/*hasValue=*/false);
    } else {
        emitPrologue(fn.func);
        genStmt(*fn.body);
        emitEpilogueAndReturn(/*hasValue=*/false, Type{});
    }
}

void Pdp7Codegen::pushAC() {
    code_ << "   dac __stmp\n";
    code_ << "   lac __sp\n";
    code_ << "   tad " << literalLabel(-1) << "\n";
    code_ << "   dac __sp\n";
    code_ << "   lac __stmp\n";
    code_ << "   dac __sp i\n";
}

void Pdp7Codegen::popAC() {
    code_ << "   lac __sp i\n";
    code_ << "   dac __stmp\n";
    code_ << "   lac __sp\n";
    code_ << "   tad __one\n";
    code_ << "   dac __sp\n";
    code_ << "   lac __stmp\n";
}

void Pdp7Codegen::emitPrologue(const ir::Function *fn) {
    code_ << "   lac " << curFunctionLabel_ << "\n";  // AC = return address jms just wrote here
    pushAC();
    code_ << "   lac __fp\n";
    pushAC();
    code_ << "   lac __sp\n";
    code_ << "   dac __fp\n";  // new frame's own __fp - points at the just-pushed oldFP word
    if (fn->frameSize > 0) {
        code_ << "   tad " << literalLabel(-static_cast<long>(fn->frameSize)) << "\n";
        code_ << "   dac __sp\n";  // reserve this function's own Locals, in one bulk step
    }
}

void Pdp7Codegen::emitEpilogueAndReturn(bool hasValue, Type retType) {
    bool wide = hasValue && isWide(retType);
    int w = wide ? widthOfType(retType) : 0;
    if (hasValue) {
        if (wide) {
            // The caller (genStmt's Return case) already pushed `w`
            // words via genWideExpr (word 0/low on top) - stash them in
            // `__retvalW<i>` before the teardown below discards
            // anything still on the real stack below `__fp`.
            for (int i = 0; i < w; i++) {
                popAC();
                code_ << "   dac __retvalW" << i << "\n";
            }
        } else {
            code_ << "   dac __retval\n";
        }
    }
    code_ << "   lac __fp\n";
    code_ << "   dac __sp\n";  // discard this call's own Locals (and anything else below __fp)
    popAC();                    // AC = oldFP
    code_ << "   dac __fp\n";
    popAC();                    // AC = this call's own return address
    code_ << "   dac __retaddr\n";
    if (hasValue) {
        if (wide) {
            // Push the saved words back onto the now-restored stack
            // (word 0/low on top, same convention as any other wide
            // value) - see the `Call` case in genExpr for why this
            // lands exactly where that call's own post-call cleanup
            // expects a wide result to be.
            for (int i = w - 1; i >= 0; i--) {
                code_ << "   lac __retvalW" << i << "\n";
                pushAC();
            }
        } else {
            code_ << "   lac __retval\n";
        }
    }
    code_ << "   jmp __retaddr i\n";
}

void Pdp7Codegen::emitLeafParamBinding(const ir::Function *fn) {
    // Params sit on the stack at entry, closest-first (Param 0 was
    // pushed LAST by the caller - see the `Call` case in genExpr),
    // so Param k's own value is at `__sp+k` - but this reads it
    // through there WITHOUT advancing `__sp`, unlike popAC(): the
    // caller's own post-call cleanup (incrementing `__sp` past every
    // argument it pushed, right after the call - see that same
    // `Call` case) runs completely unconditionally, with no idea
    // whether the callee happened to be a leaf, and still expects to
    // find every argument sitting exactly where it left them. An
    // earlier version of this used popAC() per param instead, which
    // DOES advance `__sp` - double-consuming each argument together
    // with the caller's own cleanup, and corrupting `__sp` for every
    // leaf call made (confirmed directly: tests/mix.c's `square`/
    // `fact` calls landed on the wrong answer, 27 instead of 34).
    for (ir::Slot *p : fn->params) {
        code_ << "   lac __sp\n";
        code_ << "   tad " << literalLabel(p->paramIndex) << "\n";
        code_ << "   dac __addr\n";
        code_ << "   lac __addr i\n";
        trailer_.push_back(leafLabel(p) + ": 0");
        code_ << "   dac " << leafLabel(p) << "\n";
    }
}

void Pdp7Codegen::emitLeafReturn(bool hasValue) {
    (void)hasValue;  // AC already holds the return value, if any - nothing else to do
    code_ << "   jmp i " << curFunctionLabel_ << "\n";
}

void Pdp7Codegen::emitLoadSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "   law " << label(slot) << "\n";
        return;
    }
    if (currentFunctionIsLeaf_) {
        code_ << "   law " << leafLabel(slot) << "\n";
        return;
    }
    long offset = (slot->kind == ir::SlotKind::Param) ? static_cast<long>(slot->frameOffset)
                                                       : -static_cast<long>(slot->frameOffset);
    code_ << "   lac __fp\n";
    code_ << "   tad " << literalLabel(offset) << "\n";
}

void Pdp7Codegen::emitLoadSlot(const ir::Slot *slot) {
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

void Pdp7Codegen::emitStoreSlot(const ir::Slot *slot) {
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

void Pdp7Codegen::emitSlotBaseAddress(const ir::Slot *slot) {
    if (slot->arraySize > 0) {
        emitLoadSlotAddress(slot);
    } else {
        emitLoadSlot(slot);
    }
}

void Pdp7Codegen::emitLocalArrayInit(const ir::Slot *slot) {
    // Each logical element occupies `elemW` consecutive words, word 0
    // (low) at the LOWEST address of its own span - the same
    // convention computeElementAddress()'s own scaled indexing already
    // establishes for reading this same array back (see that
    // function's own comment), so element i's word j sits at
    // `__fp-(frameOffset+i*elemW-j)`: element i's word-0 address is
    // exactly what computeElementAddress() would compute for index i,
    // and each subsequent word within that element increases from
    // there, matching pushWideFromAddress()/popWideToAddress()'s own
    // "ascend from the given base" contract.
    int elemW = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        for (int j = 0; j < elemW; j++) {
            if (!elem.isSymbol) {
                long word = (elem.value >> (18 * j)) & 0777777L;
                code_ << "   lac " << literalLabel(word) << "\n";
            } else if (j > 0) {
                // A symbol element (the dispatch-table idiom) is
                // always a plain native-word address - only word 0 of
                // a wider-than-1-word element ever holds it; every
                // higher word is simply zero (same convention
                // I386Codegen.cpp's own declareArray() uses for the
                // same case).
                code_ << "   lac __zero\n";
            } else if (elem.slot) {
                emitLoadSlotAddress(elem.slot);
            } else {
                code_ << "   law " << label(elem.func) << "\n";
            }
            code_ << "   dac __stmp\n";
            code_ << "   lac __fp\n";
            long offset = -(static_cast<long>(slot->frameOffset) + static_cast<long>(i) * elemW - j);
            code_ << "   tad " << literalLabel(offset) << "\n";
            code_ << "   dac __addr\n";
            code_ << "   lac __stmp\n";
            code_ << "   dac __addr i\n";
        }
    }
}

void Pdp7Codegen::storeThroughStackedAddress() {
    // __stmp2, not __stmp: popAC() below already uses __stmp
    // internally, which would otherwise silently clobber the value
    // stashed here before it's read back.
    code_ << "   dac __stmp2\n";
    popAC();
    code_ << "   dac __addr\n";
    code_ << "   lac __stmp2\n";
    code_ << "   dac __addr i\n";
}

std::string Pdp7Codegen::popAddressIntoScratch() {
    popAC();
    code_ << "   dac __addr\n";
    return "__addr";
}

// --- `long`/`long long` (wide) support -------------------------------

void Pdp7Codegen::pushWideFromAddress(Type type) {
    code_ << "   dac __addr\n";
    int w = widthOfType(type);
    for (int i = w - 1; i >= 0; i--) {
        code_ << "   lac __addr\n";
        if (i > 0) code_ << "   tad " << literalLabel(i) << "\n";
        code_ << "   dac __stmp\n";
        code_ << "   lac __stmp i\n";
        pushAC();
    }
}

void Pdp7Codegen::popWideToAddress(Type type) {
    code_ << "   dac __addr\n";
    int w = widthOfType(type);
    for (int i = 0; i < w; i++) {
        popAC();
        code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() above already uses __stmp
        code_ << "   lac __addr\n";
        if (i > 0) code_ << "   tad " << literalLabel(i) << "\n";
        code_ << "   dac __stmp\n";
        code_ << "   lac __stmp2\n";
        code_ << "   dac __stmp i\n";
    }
}

void Pdp7Codegen::pushWideLiteral(Type type, long value) {
    int w = widthOfType(type);
    // Each word is an 18-bit slice of `value`'s own two's-complement
    // pattern - a plain signed arithmetic right shift (relied on
    // elsewhere in this codebase too) sign-extends correctly even
    // past the host `long`'s own 64 bits (the vacated high bits of
    // an arithmetic shift are indistinguishable from "the number's
    // own sign, repeated forever"), so this is correct for every
    // word even on `long long` (4 words = 72 bits, wider than the
    // host type holding `value`).
    for (int i = w - 1; i >= 0; i--) {
        long wordVal = (value >> (18 * i)) & 0777777L;
        code_ << "   lac " << literalLabel(wordVal) << "\n";
        pushAC();
    }
}

// dest = a + b + (the current 0/1 content of `carryCell`), updating
// `carryCell` to this word's own carry-out. Verified empirically
// against the real pdp7 simulator (`tad` complements the LINK on
// 18-bit overflow - not simply "sets" it, but every `cll` here
// starts from a known-zero link, so "complement" and "set" coincide;
// `snl`/`szl` skip on link-set/link-clear respectively): no single
// `tad` can capture "carry out of a+b+carryIn" in one step (there's
// no 3-operand add), so this computes it in two independent stages -
// `a+b` first (carry1), then conditionally `+1` on top of that
// (carry2, only possible when `a+b` itself wrapped to exactly 0 and
// carryIn was 1) - and ORs the two, the standard way to build a
// full add from two half-adds.
void Pdp7Codegen::emitWideAddWithCarry(const std::string &dest, const std::string &a, const std::string &b,
                                         const std::string &carryCell, const std::string &carry1Cell,
                                         const std::string &carry2Cell) {
    std::string Lc1set = newLabel(), Lc1clear = newLabel(), Lc1done = newLabel();
    code_ << "   cll\n   lac " << a << "\n   tad " << b << "\n   dac " << dest << "\n";
    code_ << "   snl\n   jmp " << Lc1clear << "\n   jmp " << Lc1set << "\n";
    code_ << Lc1set << ":\n   lac __one\n   dac " << carry1Cell << "\n   jmp " << Lc1done << "\n";
    code_ << Lc1clear << ":\n   lac __zero\n   dac " << carry1Cell << "\n";
    code_ << Lc1done << ":\n";

    code_ << "   cll\n   lac " << dest << "\n   tad " << carryCell << "\n   dac " << dest << "\n";
    std::string Lc2set = newLabel(), Lc2clear = newLabel(), Lc2done = newLabel();
    code_ << "   snl\n   jmp " << Lc2clear << "\n   jmp " << Lc2set << "\n";
    code_ << Lc2set << ":\n   lac __one\n   dac " << carry2Cell << "\n   jmp " << Lc2done << "\n";
    code_ << Lc2clear << ":\n   lac __zero\n   dac " << carry2Cell << "\n";
    code_ << Lc2done << ":\n";

    std::string Lorset = newLabel(), Lorclear = newLabel(), Lordone = newLabel();
    code_ << "   lac " << carry1Cell << "\n   tad " << carry2Cell << "\n";
    code_ << "   sza\n   jmp " << Lorset << "\n   jmp " << Lorclear << "\n";
    code_ << Lorset << ":\n   lac __one\n   dac " << carryCell << "\n   jmp " << Lordone << "\n";
    code_ << Lorclear << ":\n   lac __zero\n   dac " << carryCell << "\n";
    code_ << Lordone << ":\n";
}

void Pdp7Codegen::genWideAddSub(bool isAdd, int wordCount) {
    std::vector<std::string> rhsT(wordCount), lhsT(wordCount), resT(wordCount);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : resT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << rhsT[i] << "\n";
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << lhsT[i] << "\n";
    }

    std::string carry = allocTemp(), carry1 = allocTemp(), carry2 = allocTemp();
    declareTemp(carry);
    declareTemp(carry1);
    declareTemp(carry2);

    if (!isAdd) {
        // a - b = a + (~b + 1) - negate rhsT in place first, via the
        // same ripple-carry machinery below (starting carry=1, the
        // "+1" of 2's complement negation; `b` is `__zero` here
        // since there's nothing else to add).
        code_ << "   lac __one\n   dac " << carry << "\n";
        for (int i = 0; i < wordCount; i++) {
            code_ << "   lac " << rhsT[i] << "\n   cma\n   dac " << rhsT[i] << "\n";
            emitWideAddWithCarry(rhsT[i], rhsT[i], "__zero", carry, carry1, carry2);
        }
    }

    code_ << "   lac __zero\n   dac " << carry << "\n";
    for (int i = 0; i < wordCount; i++) {
        emitWideAddWithCarry(resT[i], lhsT[i], rhsT[i], carry, carry1, carry2);
    }
    for (int i = wordCount - 1; i >= 0; i--) {
        code_ << "   lac " << resT[i] << "\n";
        pushAC();
    }
}

void Pdp7Codegen::genWideNegate(int wordCount) {
    std::vector<std::string> t(wordCount);
    for (auto &x : t) {
        x = allocTemp();
        declareTemp(x);
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   cma\n   dac " << t[i] << "\n";
    }
    std::string carry = allocTemp(), carry1 = allocTemp(), carry2 = allocTemp();
    declareTemp(carry);
    declareTemp(carry1);
    declareTemp(carry2);
    code_ << "   lac __one\n   dac " << carry << "\n";
    for (int i = 0; i < wordCount; i++) {
        emitWideAddWithCarry(t[i], t[i], "__zero", carry, carry1, carry2);
    }
    for (int i = wordCount - 1; i >= 0; i--) {
        code_ << "   lac " << t[i] << "\n";
        pushAC();
    }
}

// Each word is independent - no carry chain needed (unlike +/-).
// `|` has no hardware equivalent here either (see genBitwise()'s own
// comment) - same De Morgan rewrite, word-by-word.
void Pdp7Codegen::genWideBitwise(const std::string &op, int wordCount) {
    std::vector<std::string> rhsT(wordCount), lhsT(wordCount);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << rhsT[i] << "\n";
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << lhsT[i] << "\n";
    }
    for (int i = wordCount - 1; i >= 0; i--) {
        if (op == "|") {
            code_ << "   lac " << lhsT[i] << "\n   cma\n   dac __stmp2\n";
            code_ << "   lac " << rhsT[i] << "\n   cma\n   and __stmp2\n   cma\n";
        } else {
            code_ << "   lac " << lhsT[i] << "\n   " << (op == "&" ? "and" : "xor") << " " << rhsT[i] << "\n";
        }
        pushAC();
    }
}

void Pdp7Codegen::genWideCompare(const std::string &op, int wordCount, bool isUnsigned) {
    std::vector<std::string> rhsT(wordCount), lhsT(wordCount);
    for (auto &t : rhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (auto &t : lhsT) {
        t = allocTemp();
        declareTemp(t);
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << rhsT[i] << "\n";
    }
    for (int i = 0; i < wordCount; i++) {
        popAC();
        code_ << "   dac " << lhsT[i] << "\n";
    }
    if (op == "==" || op == "!=") {
        std::string Ldiffer = newLabel(), Ldone = newLabel();
        for (int i = wordCount - 1; i >= 0; i--) {
            std::string Lcontinue = newLabel();
            code_ << "   lac " << lhsT[i] << "\n   sad " << rhsT[i] << "\n   jmp " << Lcontinue << "\n   jmp "
                  << Ldiffer << "\n";
            code_ << Lcontinue << ":\n";
        }
        code_ << "   lac " << (op == "==" ? "__one" : "__zero") << "\n   jmp " << Ldone << "\n";
        code_ << Ldiffer << ":\n   lac " << (op == "==" ? "__zero" : "__one") << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    // `<`/`>=` (negate of `<`) - `>`/`<=` reduce to these by swapping
    // operands, same convention genCompare() already uses for the
    // narrow case.
    bool negate = (op == ">=" || op == "<=");
    bool swap = (op == ">" || op == "<=");
    if (swap) std::swap(lhsT, rhsT);
    // Compare from the most significant word down: the top word's
    // own difference decides the whole comparison UNLESS it's
    // exactly equal, in which case the next lower word decides, and
    // so on - every word below the top is always a plain unsigned
    // magnitude compare (the sign, if any, only ever lives in the
    // top word, so lower words always get the sign-bit-flip trick
    // below unconditionally, not just when `isUnsigned`), the top
    // word's own compare only flips when `isUnsigned`. Each word's
    // actual "is this word less" decision goes through
    // emitSignedLtCells() rather than a plain subtract-and-test-sign
    // (see its own comment) - the flip trick routinely produces
    // operands at opposite extremes, which a naive subtract
    // overflows.
    std::string Llt = newLabel(), Lge = newLabel(), Ldone = newLabel();
    for (int i = wordCount - 1; i >= 0; i--) {
        bool flip = (i == wordCount - 1) ? isUnsigned : true;
        std::string A = allocTemp(), B = allocTemp();
        declareTemp(A);
        declareTemp(B);
        code_ << "   lac " << lhsT[i] << "\n";
        if (flip) code_ << "   xor " << literalLabel(0400000L) << "\n";
        code_ << "   dac " << A << "\n";
        code_ << "   lac " << rhsT[i] << "\n";
        if (flip) code_ << "   xor " << literalLabel(0400000L) << "\n";
        code_ << "   dac " << B << "\n";
        if (i > 0) {
            std::string Lcontinue = newLabel();
            code_ << "   lac " << A << "\n   sad " << B << "\n   jmp " << Lcontinue << "\n";
            emitSignedLtCells(A, B, Llt, Lge);
            code_ << Lcontinue << ":\n";
        } else {
            emitSignedLtCells(A, B, Llt, Lge);
        }
    }
    code_ << Llt << ":\n   lac " << (negate ? "__zero" : "__one") << "\n   jmp " << Ldone << "\n";
    code_ << Lge << ":\n   lac " << (negate ? "__one" : "__zero") << "\n";
    code_ << Ldone << ":\n";
}

// Multiply and divide/modulo are NOT implemented for a wide (`long`/
// `long long`) operand on this backend: every other wide operation
// above threads its own single carry/borrow bit through a ripple
// chain this backend's real link bit and `cll`/`tad`/`snl` already
// support directly (verified empirically against the real simulator
// - see emitWideAddWithCarry()'s own comment); a genuine multi-word
// multiply or long division needs the SAME ripple-carry machinery
// nested inside ITS OWN bit-serial or word-serial loop (shift-and-
// add for multiply, compare-shift-subtract for divide - the same
// algorithms every other software-multiply/divide backend here
// already uses, e.g. ../pdp11/Pdp11Codegen.cpp's own documented
// choice to leave exactly these two unimplemented for the same
// reason), and this backend has no general-purpose register to hold
// even one of the several words such a routine would need live at
// once - every intermediate here already lives in a named memory
// cell, reached only through AC and one level of indirection. A
// genuine implementation would need its own loop-nested variant of
// every helper above (a per-bit-per-word carry chain, not just a
// per-word one), disproportionate to attempt without a real way to
// verify it works beyond "it assembles" - the same judgment call
// MCS51Codegen.cpp's own header comment documents for its own
// architecturally narrower reason.
void Pdp7Codegen::genWideMulDivErr(int line, const std::string &opName) {
    err(line, "'" + opName + "' on a 'long'/'long long' operand isn't supported on this backend (pdp7 has no "
                               "general-purpose register and no verified multi-word multiply/divide routine - "
                               "see Pdp7Codegen.cpp's own genWideMulDivErr() comment)");
}

// Only ever called when `e.type` itself is narrow (not wide) - see
// genExpr()'s own Convert case, which dispatches to genWideExpr()
// instead whenever `e.type` is wide. A wide source just contributes
// its own low word (every higher word is simply discarded, same as
// any other narrowing truncation); a narrow source goes through
// narrowAC() exactly like any other narrow value reaching its own
// declared type.
void Pdp7Codegen::genConvert(const ir::Expr &e) {
    if (isWide(e.lhs->type)) {
        genWideExpr(*e.lhs);
        popAC();  // AC = the low word - this is the value being kept
        std::string Tlow = allocTemp();
        declareTemp(Tlow);
        code_ << "   dac " << Tlow << "\n";
        int w = widthOfType(e.lhs->type);
        for (int i = 1; i < w; i++) popAC();  // discard every higher word
        code_ << "   lac " << Tlow << "\n";
        narrowAC(e.type);
        return;
    }
    genExpr(*e.lhs);
    narrowAC(e.type);
}

// Generalizes genExpr() for a wide-typed expression (`e.type` itself
// is `long`/`long long`) - leaves its `widthOfType(e.type)` words
// pushed on the real stack, word 0 (low) on top, instead of in AC -
// see this file's own header comment on why a single-accumulator
// machine needs this separate contract at all.
void Pdp7Codegen::genWideExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            pushWideLiteral(e.type, e.intValue);
            return;
        }
        case ir::ExprKind::Var: {
            emitLoadSlotAddress(e.slot);
            pushWideFromAddress(e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            // computeElementAddress() already scales by this array's
            // own element width and leaves that element's own word-0
            // address pushed - see its own comment.
            computeElementAddress(e.slot, *e.lhs);
            popAC();
            pushWideFromAddress(e.type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Same shape as a wide Assign's own non-indexed branch
            // below, except the store address comes from evaluating
            // e.lhs (the pointer, always narrow - see Unary "*"'s own
            // identical comment above) instead of emitLoadSlotAddress().
            int w = widthOfType(e.type);
            genExpr(*e.lhs);  // AC = pointer value
            pushAC();          // protect it while evaluating rhs (which may itself call)
            genWideExpr(*e.rhs);
            std::vector<std::string> temps(w);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (auto &t : temps) {
                popAC();
                code_ << "   dac " << t << "\n";
            }
            popAC();  // AC = the pointer's own address
            code_ << "   dac __addr\n";
            for (int i = 0; i < w; i++) {
                code_ << "   lac __addr\n";
                if (i > 0) code_ << "   tad " << literalLabel(i) << "\n";
                code_ << "   dac __stmp2\n";
                code_ << "   lac " << temps[i] << "\n";
                code_ << "   dac __stmp2 i\n";
            }
            for (int i = w - 1; i >= 0; i--) {
                code_ << "   lac " << temps[i] << "\n";
                pushAC();
            }
            return;
        }
        case ir::ExprKind::Assign: {
            int w = widthOfType(e.slot->type);
            std::vector<std::string> temps(w);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            if (e.lhs) {  // slot[idx] = rhs, a wide array element
                computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address (1 word)
                genWideExpr(*e.rhs);                       // pushes w words, above that address
                for (auto &t : temps) {
                    popAC();
                    code_ << "   dac " << t << "\n";
                }
                popAC();  // AC = the element's own address
                code_ << "   dac __addr\n";
            } else {
                genWideExpr(*e.rhs);
                for (auto &t : temps) {
                    popAC();
                    code_ << "   dac " << t << "\n";
                }
                emitLoadSlotAddress(e.slot);
                code_ << "   dac __addr\n";
            }
            for (int i = 0; i < w; i++) {
                code_ << "   lac __addr\n";
                if (i > 0) code_ << "   tad " << literalLabel(i) << "\n";
                code_ << "   dac __stmp2\n";
                code_ << "   lac " << temps[i] << "\n";
                code_ << "   dac __stmp2 i\n";
            }
            for (int i = w - 1; i >= 0; i--) {
                code_ << "   lac " << temps[i] << "\n";
                pushAC();
            }
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genWideExpr(*e.rhs);
                genWideNegate(widthOfType(e.type));
                return;
            }
            if (e.op == "*") {
                // A wide (`long`/`long long`) pointee - the pointer
                // itself is always a narrow, single-word address (see
                // Type.h's own header comment on a pointer's own
                // storage width) - pushWideFromAddress() reads
                // `widthOfType(e.type)` consecutive words starting
                // there, same convention pushWideFromAddress()'s own
                // header comment and a wide Var's own identical case
                // above already establish.
                genExpr(*e.rhs);  // AC = pointer value
                pushWideFromAddress(e.type);
                return;
            }
            if (e.op == "~") {
                int w = widthOfType(e.type);
                genWideExpr(*e.rhs);
                std::vector<std::string> t(w);
                for (auto &x : t) {
                    x = allocTemp();
                    declareTemp(x);
                }
                for (int i = 0; i < w; i++) {
                    popAC();
                    code_ << "   cma\n   dac " << t[i] << "\n";
                }
                for (int i = w - 1; i >= 0; i--) {
                    code_ << "   lac " << t[i] << "\n";
                    pushAC();
                }
                return;
            }
            err(e.line, "internal: unary '" + e.op + "' on a wide ('long'/'long long') value isn't supported "
                                                        "on this backend");
        }
        case ir::ExprKind::Convert: {
            Type from = e.lhs->type;
            int toW = widthOfType(e.type);
            if (!isWide(from)) {
                genExpr(*e.lhs);
                std::string Tlo = allocTemp();
                declareTemp(Tlo);
                code_ << "   dac " << Tlo << "\n";
                std::string Text = allocTemp();
                declareTemp(Text);
                if (from.isUnsigned) {
                    code_ << "   lac __zero\n   dac " << Text << "\n";
                } else {
                    std::string Lneg = newLabel(), Lpos = newLabel(), Ldone = newLabel();
                    code_ << "   lac " << Tlo << "\n";
                    code_ << "   sma\n   jmp " << Lpos << "\n   jmp " << Lneg << "\n";
                    code_ << Lneg << ":\n   lac " << literalLabel(0777777L) << "\n   dac " << Text << "\n   jmp "
                          << Ldone << "\n";
                    code_ << Lpos << ":\n   lac __zero\n   dac " << Text << "\n";
                    code_ << Ldone << ":\n";
                }
                for (int i = 0; i < toW - 1; i++) {
                    code_ << "   lac " << Text << "\n";
                    pushAC();
                }
                code_ << "   lac " << Tlo << "\n";
                pushAC();
                return;
            }
            int fromW = widthOfType(from);
            genWideExpr(*e.lhs);
            std::vector<std::string> temps(fromW);
            for (auto &t : temps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (auto &t : temps) {
                popAC();
                code_ << "   dac " << t << "\n";
            }
            if (toW <= fromW) {
                for (int i = toW - 1; i >= 0; i--) {
                    code_ << "   lac " << temps[i] << "\n";
                    pushAC();
                }
                return;
            }
            std::string Text = allocTemp();
            declareTemp(Text);
            if (from.isUnsigned) {
                code_ << "   lac __zero\n   dac " << Text << "\n";
            } else {
                std::string Lneg = newLabel(), Lpos = newLabel(), Ldone = newLabel();
                code_ << "   lac " << temps[fromW - 1] << "\n";
                code_ << "   sma\n   jmp " << Lpos << "\n   jmp " << Lneg << "\n";
                code_ << Lneg << ":\n   lac " << literalLabel(0777777L) << "\n   dac " << Text << "\n   jmp "
                      << Ldone << "\n";
                code_ << Lpos << ":\n   lac __zero\n   dac " << Text << "\n";
                code_ << Ldone << ":\n";
            }
            for (int i = 0; i < toW - fromW; i++) {
                code_ << "   lac " << Text << "\n";
                pushAC();
            }
            for (int i = fromW - 1; i >= 0; i--) {
                code_ << "   lac " << temps[i] << "\n";
                pushAC();
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Identical argument-passing convention to genExpr()'s own
            // Call case (see its comment for the full reasoning). A
            // wide callee's own emitEpilogueAndReturn() leaves its
            // `widthOfType` return words pushed on the stack - but
            // AT `__sp` AS IT STANDS RIGHT AFTER RETURN, which sits
            // `totalWords` (the ARGUMENT words) further down than
            // this call's own argument-pushing left it; it does NOT
            // land wherever `__sp += totalWords` alone would put it.
            // (An earlier version of this assumed it did - every test
            // that happened to return one of its own untouched wide
            // arguments verbatim still "passed" by accident, reading
            // stale argument memory the callee never touched, rather
            // than the real returned value; anything that actually
            // computed its return value, or took more than one wide
            // argument, read garbage.) So: pop the real return words
            // from where they actually are FIRST, then undo the
            // argument push, then push the saved words back - now
            // genuinely sitting where the rest of this file expects a
            // wide value it left on the stack to be.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    genWideExpr(*e.args[i]);
                    totalWords += widthOfType(e.args[i]->type);
                } else {
                    genExpr(*e.args[i]);
                    pushAC();
                    totalWords += 1;
                }
            }
            code_ << "   jms " << label(e.func) << "\n";
            int returnWords = widthOfType(e.type);
            std::vector<std::string> retTemps(returnWords);
            for (auto &t : retTemps) {
                t = allocTemp();
                declareTemp(t);
            }
            for (auto &t : retTemps) {
                popAC();
                code_ << "   dac " << t << "\n";
            }
            if (totalWords > 0) {
                code_ << "   lac __sp\n";
                code_ << "   tad " << literalLabel(static_cast<long>(totalWords)) << "\n";
                code_ << "   dac __sp\n";
            }
            for (int i = returnWords - 1; i >= 0; i--) {
                code_ << "   lac " << retTemps[i] << "\n";
                pushAC();
            }
            return;
        }
        case ir::ExprKind::Binary: {
            const std::string &op = e.op;
            if (op == "+" || op == "-") {
                genWideExpr(*e.lhs);
                genWideExpr(*e.rhs);
                genWideAddSub(op == "+", widthOfType(e.type));
                return;
            }
            if (op == "&" || op == "|" || op == "^") {
                genWideExpr(*e.lhs);
                genWideExpr(*e.rhs);
                genWideBitwise(op, widthOfType(e.type));
                return;
            }
            if (op == "*") genWideMulDivErr(e.line, "*");
            if (op == "/") genWideMulDivErr(e.line, "/");
            if (op == "%") genWideMulDivErr(e.line, "%");
            err(e.line, "internal: binary '" + op + "' on a wide ('long'/'long long') value isn't supported "
                                                       "on this backend");
        }
        default:
            err(e.line, "internal: this expression kind can't produce a wide ('long'/'long long') value on this "
                          "backend");
    }
}

void Pdp7Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            // genCondBranch() fuses a plain comparison (or `&&` of
            // them) straight into this branch, skipping genExpr()'s
            // own "materialize a real 0/1 value, then re-test it"
            // round trip - see its own comment. Anything else (`||`,
            // a bare variable, a call, bitwise, ...) falls back to
            // exactly the old "evaluate, then `sna`" sequence inside
            // genCondBranch() itself, so this case doesn't need its
            // own fallback here any more.
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
            // `continue` jumps here if this loop has its own post-
            // statement (a desugared for-loop's - see IR.h's
            // Stmt::postStmt), so post still runs before the condition
            // is retested; otherwise it's just Lstart itself, since
            // there's nothing else to run first - see e.g.
            // I8086Codegen.cpp's own While case for the same pattern.
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            // See the `If` case above for what genCondBranch() fuses.
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
            if (hasValue) {
                if (isWide(s.initOrReturnOrExpr->type)) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    genExpr(*s.initOrReturnOrExpr);
                }
            }
            if (currentFunctionIsLeaf_) {
                // No stack-frame teardown on this path at all (see
                // genFunction()'s own leaf branch) - a wide value's
                // own `w` words, already pushed above, are left
                // exactly where genWideExpr put them; nothing here
                // needs to know they're there.
                emitLeafReturn(hasValue);
            } else {
                emitEpilogueAndReturn(hasValue, hasValue ? s.initOrReturnOrExpr->type : Type{});
            }
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                emitLocalArrayInit(s.declSlot);
            } else if (isWide(s.declSlot->type)) {
                // A wide Local always takes the non-leaf, frame-based
                // path (see stmtRulesOutLeaf()'s own slotIsWide()
                // check), so its own storage is always `widthOfType`
                // consecutive frame words, addressed via
                // emitLoadSlotAddress() exactly like any other Local -
                // genExpr()'s own blanket wide-collapsing-to-AC
                // shortcut would silently drop everything but this
                // value's own low word, so this needs genWideExpr()
                // and popWideToAddress() directly instead, same as a
                // wide Assign's own real store (see genWideExpr()'s
                // Assign case).
                if (s.initOrReturnOrExpr) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    pushWideLiteral(s.declSlot->type, 0);
                }
                emitLoadSlotAddress(s.declSlot);
                popWideToAddress(s.declSlot->type);
            } else {
                // A leaf function's own Local gets a static cell
                // the first (and only - genStmt visits each
                // LocalDecl node exactly once, however many times
                // the real control flow re-runs it) time its own
                // declaration is generated - see Pdp7Codegen.h's
                // "Leaf fast path" comment.
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
            // Verbatim passthrough - see grammar/C.g4's `statement`
            // comment on `asm { ... }`. Not reformatted/indented:
            // whatever's between the braces goes straight into the
            // generated assembly text exactly as written.
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
    }
}

void Pdp7Codegen::genExpr(const ir::Expr &e) {
    // genExpr()'s own contract is "leaves the result in AC" - a wide
    // (`long`/`long long`) result can't fit there, so genWideExpr()
    // (see its own header comment) is the real implementation for
    // every expression kind it supports; this computes the correct
    // wide value there (including any store a wide Assign performs)
    // and then collapses it down to just its own low word in AC,
    // discarding the rest - correct for any caller that only reads a
    // wide value narrowly (e.g. genConvert()'s own narrow-destination
    // case), and a safe fallback (truncating, never silently wrong)
    // for any caller that doesn't itself know to call genWideExpr()
    // directly for the full value (every entry point that needs the
    // full value - LocalDecl's initializer, Return's own value - does
    // call genWideExpr() itself instead; see genStmt()'s own Return/
    // LocalDecl cases).
    if (isWide(e.type)) {
        genWideExpr(e);
        int w = widthOfType(e.type);
        std::string Tlow = allocTemp();
        declareTemp(Tlow);
        popAC();
        code_ << "   dac " << Tlow << "\n";
        for (int i = 1; i < w; i++) popAC();
        code_ << "   lac " << Tlow << "\n";
        return;
    }
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "   lac " << literalLabel(e.intValue) << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            // IRBuilder already rejects a bare array Var outside of
            // `&` (which bypasses this path via buildAddressableOperand),
            // so e.slot is always a scalar here.
            emitLoadSlot(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address
                genExpr(*e.rhs);                          // may itself call - the address is safe on the stack
                storeThroughStackedAddress();              // dac leaves AC unchanged
                return;
            }
            genExpr(*e.rhs);
            emitStoreSlot(e.slot);  // leaves AC unchanged
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "   cma\n   tad __one\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // AC = pointer value
                code_ << "   dac __addr\n";
                code_ << "   lac __addr i\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "   cma\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // AC = pointer value
            pushAC();          // protect it - evaluating rhs may itself call
            genExpr(*e.rhs);  // AC = value to store
            storeThroughStackedAddress();
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed in REVERSE order (last argument
            // first) so that param 0 ends up CLOSEST to the callee's
            // own `__fp` (address `__fp+2` - see emitLoadSlotAddress()'s
            // own comment); the callee then reads every Param straight
            // out of this same stack, never a fixed per-function cell,
            // so a recursive (direct or mutual) call's own arguments
            // can never collide with an outer, still-live call's own.
            // Each argument expression may itself contain a call (even
            // back into this same function) - safe regardless, since
            // every earlier argument already pushed for THIS call is
            // already on the real stack by the time a later one
            // evaluates.
            int totalWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                if (isWide(e.args[i]->type)) {
                    genWideExpr(*e.args[i]);
                    totalWords += widthOfType(e.args[i]->type);
                } else {
                    genExpr(*e.args[i]);
                    pushAC();
                    totalWords += 1;
                }
            }
            code_ << "   jms " << label(e.func) << "\n";
            // The callee's own epilogue leaves AC holding its return
            // value and `__sp` exactly where it was right after this
            // call's own arguments were pushed (see
            // emitEpilogueAndReturn()'s comment) - the caller still
            // owns popping them back off (classic caller-cleans-up
            // convention), protecting the return value in `__retval`
            // across that adjustment since it needs AC too. `totalWords`
            // is the real word count pushed above (a wide argument
            // pushes `widthOfType` words, not one), matching exactly
            // what the callee's own Param::frameOffset layout (see
            // IRBuilder.cpp) already assumes.
            if (totalWords > 0) {
                code_ << "   dac __retval\n";
                code_ << "   lac __sp\n";
                code_ << "   tad " << literalLabel(static_cast<long>(totalWords)) << "\n";
                code_ << "   dac __sp\n";
                code_ << "   lac __retval\n";
            }
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // ABI slot to put one in until the callee is known, which
            // it never is at compile time for this kind - so there's
            // no argument-stack bookkeeping to do here at all, unlike
            // the ir::ExprKind::Call case above.
            genExpr(*e.lhs);  // AC = the callee's own entry address
            code_ << "   dac __addr\n";
            code_ << "   jms __addr i\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // ion/iof aren't in pdp7as's builtin opcode table - the
            // real kernel defines them itself, in
            // ../../../../src-assem/sys/sop.s ("iof = 0700002" /
            // "ion = 0700042"), which this backend's generated
            // output doesn't link against. Emitting the literal
            // instruction word sop.s assigns that name sidesteps
            // needing to link it in. Verified on the real simulator
            // to execute without error and leave AC unchanged (an
            // "operate"-class instruction, like cla/cll/etc.) - not
            // verified for the actual interrupt-masking effect,
            // which needs a configured interrupt source to observe
            // at all.
            code_ << "   " << (e.op == "ion" ? "0700042" : "0700002") << "  ; " << e.op << "\n";
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

// &slot[idxExpr], left in a fresh temp cell (returned by label).
// Needs `law` to get slot's own address into AC at all - verified
// against the real simulator (see README's "Codegen conventions"):
// `law slot` leaves AC's low 13 bits as slot's address, regardless
// of the opcode bits LAW also sets in AC's high bits, since those
// are ignored wherever the result is later used as an address (here,
// via one level of `i` indirection) rather than as a plain value.
//
// slot is only ever the array itself when it's a declared array
// (slot->arraySize > 0); slot[idx] on a plain variable means that
// variable holds a pointer, and the base address to index from is
// its VALUE (lac), not its own storage address (law) - indexing a
// pointer parameter (e.g. p[1] inside a function taking `int p`)
// needs this distinction.
void Pdp7Codegen::computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr) {
    // A Local array's own elements sit at DECREASING addresses as the
    // index increases (element i is at `__fp-(frameOffset+i)` - see
    // emitLoadSlotAddress()'s own frame-layout comment), the opposite
    // of a Global array's normal increasing-address layout - so the
    // index gets negated up front here, making the final `tad` below
    // compute `base - index` for a Local array but `base + index`
    // for everything else (a Global array, or any plain pointer
    // variable's own value - ordinary memory, always increasing).
    bool descending = slot->arraySize > 0 && slot->kind == ir::SlotKind::Local;
    genExpr(idxExpr);     // AC = index (may itself call)
    // Scale the index by the array's own element width (see
    // typeWidths()) - every element is `elemW` consecutive words, not
    // always one, now that an element can be `long`/`long long`.
    // Unrolled as `elemW`-1 self-adds rather than a real multiply:
    // cheap for the only widths this backend ever has (1, 2, or 4),
    // and this is a compile-time constant either way.
    int elemW = widthOfType(slot->type);
    if (elemW > 1) {
        std::string Tidx = allocTemp();
        declareTemp(Tidx);
        code_ << "   dac " << Tidx << "\n";
        for (int i = 1; i < elemW; i++) code_ << "   tad " << Tidx << "\n";
    }
    if (descending) code_ << "   cma\n   tad __one\n";  // AC = -(scaled index)
    pushAC();               // protect the (possibly negated) index across the base-address computation below
    emitSlotBaseAddress(slot);
    code_ << "   dac __addr\n";
    popAC();                 // AC = index or -index
    code_ << "   tad __addr\n";  // AC = base +/- index = the element's own address
    pushAC();                     // leave it on the stack for the caller
}

void Pdp7Codegen::genIndex(const ir::Expr &e) {
    computeElementAddress(e.slot, *e.lhs);
    std::string addr = popAddressIntoScratch();
    code_ << "   lac " << addr << " i\n";
}

// &operand: a plain variable/array (&slot -> its own address), an
// array element (&slot[idx] -> that element's address), or a bare
// function name (&fn -> fn's own entry label, the same address a
// direct `jms label(fn)` call site already targets - see label()'s
// own comment; calling through it later is `jms i` on whatever holds
// this address, not this function's concern) are valid - there's
// nothing else in this subset with an address worth taking.
void Pdp7Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        emitLoadSlotAddress(operand.slot);
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        computeElementAddress(operand.slot, *operand.lhs);
        popAC();  // AC = the address itself - exactly what &slot[idx] wants
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "   law " << label(operand.func) << "\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Pdp7Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        pushAC();  // protect lhs - rhs's own evaluation may call (even back into this function)
        genExpr(*e.rhs);
        if (op == "-") code_ << "   cma\n   tad __one\n";
        code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() below uses __stmp internally
        popAC();  // AC = lhs
        code_ << "   tad __stmp2\n";
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

void Pdp7Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    // A comparison's own result type is always plain (narrow) Int
    // even when its OPERANDS are wide (see IRBuilder's own comment on
    // comparisons), so genExpr()'s top-of-function `isWide(e.type)`
    // guard never catches this - dispatch on the operands themselves
    // here instead, straight to genWideCompare(), rather than falling
    // into emitEq()/emitLt()'s plain genExpr() calls below, which
    // would silently truncate a wide operand to just its low word.
    if (isWide(lhs.type)) {
        genWideExpr(lhs);
        genWideExpr(rhs);
        genWideCompare(op, widthOfType(lhs.type), lhs.type.isUnsigned);
        return;
    }
    if (op == "==") return emitEq(lhs, rhs, /*negate=*/false);
    if (op == "!=") return emitEq(lhs, rhs, /*negate=*/true);
    if (op == "<") return emitLt(lhs, rhs, /*negate=*/false);
    if (op == ">=") return emitLt(lhs, rhs, /*negate=*/true);
    if (op == ">") return emitLt(rhs, lhs, /*negate=*/false);
    if (op == "<=") return emitLt(rhs, lhs, /*negate=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Skip/branch primitives used below (verified against the real pdp7
// simulator, see ../README.md):
//   sad T   - skip next instruction if AC != M[T]  ("skip if different")
//   sma     - skip next instruction if AC is negative
//   sza     - skip next instruction if AC == 0

void Pdp7Codegen::emitEq(const ir::Expr &lhs, const ir::Expr &rhs, bool negate) {
    genExpr(lhs);
    pushAC();  // protect lhs - rhs's own evaluation may call
    genExpr(rhs);
    code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() below uses __stmp internally
    popAC();  // AC = lhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sad __stmp2\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
    if (negate) code_ << "   xor __one\n";
}

void Pdp7Codegen::emitSignedLtCells(const std::string &A, const std::string &B, const std::string &Ltrue,
                                     const std::string &Lfalse) {
    std::string LaNeg = newLabel(), LaPos = newLabel();
    std::string LbNegWhenANeg = newLabel(), LbPosWhenANeg = newLabel();
    std::string LbNegWhenAPos = newLabel(), LbPosWhenAPos = newLabel();
    std::string Lsub = newLabel();
    code_ << "   lac " << A << "\n   sma\n   jmp " << LaPos << "\n   jmp " << LaNeg << "\n";
    code_ << LaNeg << ":\n   lac " << B << "\n   sma\n   jmp " << LbPosWhenANeg << "\n   jmp "
          << LbNegWhenANeg << "\n";
    code_ << LbPosWhenANeg << ":\n   jmp " << Ltrue << "\n";  // A<0<=B -> A<B
    code_ << LbNegWhenANeg << ":\n   jmp " << Lsub << "\n";   // both negative -> safe subtract
    code_ << LaPos << ":\n   lac " << B << "\n   sma\n   jmp " << LbPosWhenAPos << "\n   jmp "
          << LbNegWhenAPos << "\n";
    code_ << LbNegWhenAPos << ":\n   jmp " << Lfalse << "\n";  // A>=0, B<0 -> A>=B
    code_ << LbPosWhenAPos << ":\n   jmp " << Lsub << "\n";    // both non-negative -> safe subtract
    code_ << Lsub << ":\n";
    code_ << "   lac " << B << "\n   cma\n   tad __one\n   dac __stmp2\n";
    code_ << "   lac " << A << "\n   tad __stmp2\n";  // AC = A-B, safe: same sign, can't overflow
    code_ << "   sma\n   jmp " << Lfalse << "\n   jmp " << Ltrue << "\n";
}

void Pdp7Codegen::emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate) {
    // Unsigned path: XOR each side's own sign bit (0400000, bit 17)
    // before the signed-less-than test below - the same sign-bit-
    // flip trick genWideCompare()'s own top-word comparison already
    // uses and documents, maps unsigned order onto signed order.
    // Delegates the actual `<` test to emitSignedLtCells() rather
    // than a plain subtract-and-test-sign, since the flip above
    // routinely produces operands at opposite extremes (e.g. 0 and
    // the all-ones pattern), which a naive subtract overflows.
    bool unsignedCmp = lhs.type.isUnsigned;
    genExpr(lhs);
    if (unsignedCmp) code_ << "   xor " << literalLabel(0400000L) << "\n";
    std::string A = allocTemp();
    declareTemp(A);
    code_ << "   dac " << A << "\n";
    genExpr(rhs);
    if (unsignedCmp) code_ << "   xor " << literalLabel(0400000L) << "\n";
    std::string B = allocTemp();
    declareTemp(B);
    code_ << "   dac " << B << "\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    emitSignedLtCells(A, B, Ltrue, Lfalse);
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
    if (negate) code_ << "   xor __one\n";
}

// See Pdp7Codegen.h's own comment for what this is for and why it
// avoids `spa`. Each comparison computes AC = a - b exactly the way
// emitLt()/emitEq() already do (reusing their own verified
// instruction choices), then branches on that raw result directly
// instead of materializing a 0/1 value first:
//   `<`/`>`  (swap operands for `>`, so AC = (the smaller side) -
//            (the larger side) tests the same way `<` already does):
//            `sma` (skip if AC<0 - verified, see emitLt()) - 2 words:
//            true (AC<0) skips the jump, false executes it.
//   `>=`/`<=` (negate of `>`/`<` - swap operands for `<=`): `spa`
//            would be the natural complement of `sma` here, but
//            nothing in this file has used or verified it yet, so
//            this uses the same 3-word "jump either way, fall into
//            a label placed right before the true-branch code"
//            shape genCompare()'s own old materializing path used,
//            just without ever producing a 0/1 value - still 4-5
//            words cheaper than the full materialize-then-test
//            round trip.
//   `==`     `sad` (skip if AC != M[T], i.e. "different" - verified,
//            see emitEq()) naturally gives "skip if different", the
//            OPPOSITE sense `==` wants (fall through when SAME) -
//            same 3-word shape as `>=`/`<=` above.
//   `!=`     `sad`'s natural "skip if different" sense already
//            matches what `!=` wants directly - 2 words.
// `&&` recurses (both sides must be true; either side's own failure
// goes straight to the same `Lfalse`, so this is just two calls in a
// row - correctly short-circuiting, since the second genCondBranch
// call is only ever reached by falling through the first one's own
// success path). `||` is NOT fused (the short-circuit-success case
// needs a different intermediate label shape this doesn't build) -
// it falls through to the generic path below along with anything
// else (`!`, a bare variable, a call, bitwise, ...), unchanged from
// before: evaluate normally, test the resulting 0/1 value.
void Pdp7Codegen::genCondBranch(const ir::Expr &cond, const std::string &Lfalse) {
    if (cond.kind == ir::ExprKind::Binary) {
        const std::string &op = cond.op;
        if (op == "&&") {
            genCondBranch(*cond.lhs, Lfalse);
            genCondBranch(*cond.rhs, Lfalse);
            return;
        }
        // A wide-operand comparison can't use any of this fused
        // narrow machinery below (it never materializes a value
        // genExpr() could read without silently truncating it - see
        // genCompare()'s own comment) - fall through to the generic
        // "evaluate normally, test the resulting 0/1 value" path at
        // the bottom of this function instead, which goes through
        // genExpr()/genBinary()/genCompare() and so already dispatches
        // a wide comparison to genWideCompare() correctly.
        bool wideCmp = (op == "<" || op == ">" || op == ">=" || op == "<=" || op == "==" || op == "!=") &&
                       isWide(cond.lhs->type);
        if (!wideCmp && (op == "<" || op == ">" || op == ">=" || op == "<=")) {
            const ir::Expr *a = cond.lhs.get();
            const ir::Expr *b = cond.rhs.get();
            bool negate = (op == ">=" || op == "<=");
            if (op == ">" || op == "<=") std::swap(a, b);
            bool unsignedCmp = a->type.isUnsigned;
            genExpr(*a);
            if (unsignedCmp) code_ << "   xor " << literalLabel(0400000L) << "\n";
            std::string A = allocTemp();
            declareTemp(A);
            code_ << "   dac " << A << "\n";
            genExpr(*b);
            if (unsignedCmp) code_ << "   xor " << literalLabel(0400000L) << "\n";
            std::string B = allocTemp();
            declareTemp(B);
            code_ << "   dac " << B << "\n";
            // emitSignedLtCells() below tests A<B directly (see its
            // own comment for why not a plain subtract-and-sma) -
            // negate just picks which outcome continues to Lfalse.
            std::string Ltrue = newLabel();
            if (!negate) {
                emitSignedLtCells(A, B, Ltrue, Lfalse);
                code_ << Ltrue << ":\n";
            } else {
                emitSignedLtCells(A, B, Lfalse, Ltrue);
                code_ << Ltrue << ":\n";
            }
            return;
        }
        if (!wideCmp && (op == "==" || op == "!=")) {
            genExpr(*cond.lhs);
            pushAC();  // protect lhs - rhs's own evaluation may call
            genExpr(*cond.rhs);
            code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() below uses __stmp internally
            popAC();  // AC = lhs
            if (op == "!=") {
                code_ << "   sad __stmp2\n   jmp " << Lfalse << "\n";
            } else {
                std::string Ltrue = newLabel();
                code_ << "   sad __stmp2\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
                code_ << Ltrue << ":\n";
            }
            return;
        }
    }
    genExpr(cond);
    code_ << "   sna\n   jmp " << Lfalse << "\n";
}

void Pdp7Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void Pdp7Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
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

void Pdp7Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sza\n   jmp " << Lfalse << "\n   jmp " << Ltrue << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
}

// Multiply via the hardware EAE "mul" instruction, exactly the
// pattern used by src-assem/tests/testmul.s: "mul" reads its
// multiplicand as the literal data word immediately following it in
// memory, and leaves a double-length product in AC:MQ; "lacq" brings
// the low-order (18-bit, wraparound) half into AC. Only verified
// against non-negative operands - see README.
void Pdp7Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushAC();  // protect lhs - rhs's own evaluation may call
    genExpr(rhs);
    std::string Tr = allocTemp();  // declared inline below, NOT via declareTemp: it doubles as mul's operand slot
    code_ << "   dac " << Tr << "\n";
    popAC();  // AC = lhs
    code_ << "   cll\n   mul\n" << Tr << ": 0\n   lacq\n";
}

// Division/modulo via a small shared software routine (repeated
// subtraction on the operands' absolute values, with the sign of
// each result reapplied afterward) rather than the EAE "idiv"
// instruction, whose signed semantics weren't verified for this
// compiler. Matches C's own truncating (round-toward-zero) division:
// the quotient's sign is the XOR of the two operands' signs, and the
// remainder's sign always matches the dividend's (`a == q*b + r`
// holds either way).
void Pdp7Codegen::ensureDivModRoutine() {
    if (!needDivMod_) return;
    trailer_.push_back("__dm_a: 0");   // dividend -> |dividend| -> |remainder| -> signed remainder
    trailer_.push_back("__dm_b: 0");   // divisor -> |divisor|
    trailer_.push_back("__dm_q: 0");   // |quotient| -> signed quotient
    trailer_.push_back("__dm_sa: 0");  // 1 if the dividend was negative, else 0
    trailer_.push_back("__dm_sb: 0");  // 1 if the divisor was negative, else 0
    trailer_.push_back("__divmod: 0");
    trailer_.push_back("   dzm __dm_q");
    trailer_.push_back("   dzm __dm_sa");
    trailer_.push_back("   dzm __dm_sb");
    // __dm_a = |dividend|, __dm_sa = 1 if it was negative.
    trailer_.push_back("   lac __dm_a");
    trailer_.push_back("   sma");
    trailer_.push_back("   jmp J_dm_adone");
    trailer_.push_back("   jmp J_dm_aneg");
    trailer_.push_back("J_dm_aneg:");
    trailer_.push_back("   isz __dm_sa");
    trailer_.push_back("   lac __dm_a");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   dac __dm_a");
    trailer_.push_back("J_dm_adone:");
    // __dm_b = |divisor|, __dm_sb = 1 if it was negative.
    trailer_.push_back("   lac __dm_b");
    trailer_.push_back("   sma");
    trailer_.push_back("   jmp J_dm_bdone");
    trailer_.push_back("   jmp J_dm_bneg");
    trailer_.push_back("J_dm_bneg:");
    trailer_.push_back("   isz __dm_sb");
    trailer_.push_back("   lac __dm_b");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   dac __dm_b");
    trailer_.push_back("J_dm_bdone:");
    // Unsigned repeated-subtraction division of the two magnitudes:
    // __dm_a ends up holding |remainder|, __dm_q holding |quotient|.
    trailer_.push_back("J_dmloop:");
    trailer_.push_back("   lac __dm_b");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   tad __dm_a");  // AC = remaining_a - b
    trailer_.push_back("   sma");
    trailer_.push_back("   jmp J_dmbody");
    trailer_.push_back("   jmp J_dmdone");
    trailer_.push_back("J_dmbody:");
    trailer_.push_back("   dac __dm_a");  // remaining_a -= b
    trailer_.push_back("   isz __dm_q");  // quotient += 1
    trailer_.push_back("   jmp J_dmloop");
    trailer_.push_back("J_dmdone:");
    // Quotient is negative iff exactly one of the two signs was set.
    trailer_.push_back("   lac __dm_sa");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   tad __dm_sb");  // AC = __dm_sb - __dm_sa: 0 iff same sign
    trailer_.push_back("   sza");
    trailer_.push_back("   jmp J_dm_qflip");
    trailer_.push_back("   jmp J_dm_qdone");
    trailer_.push_back("J_dm_qflip:");
    trailer_.push_back("   lac __dm_q");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   dac __dm_q");
    trailer_.push_back("J_dm_qdone:");
    // Remainder's sign always matches the dividend's.
    trailer_.push_back("   lac __dm_sa");
    trailer_.push_back("   sza");
    trailer_.push_back("   jmp J_dm_rflip");
    trailer_.push_back("   jmp J_dm_rdone");
    trailer_.push_back("J_dm_rflip:");
    trailer_.push_back("   lac __dm_a");
    trailer_.push_back("   cma");
    trailer_.push_back("   tad __one");
    trailer_.push_back("   dac __dm_a");
    trailer_.push_back("J_dm_rdone:");
    trailer_.push_back("   lac __dm_q");
    trailer_.push_back("   jmp i __divmod");
}

void Pdp7Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    needDivMod_ = true;
    genExpr(lhs);
    pushAC();  // protect lhs - rhs's own evaluation may call, possibly into another division
    genExpr(rhs);
    code_ << "   dac __dm_b\n";
    popAC();  // AC = lhs
    code_ << "   dac __dm_a\n";
    code_ << "   jms __divmod\n";
    if (wantRemainder) code_ << "   lac __dm_a\n";
}

// "&" (and) / "^" (xor) are real pdp7 memory-reference instructions
// already in pdp7as's builtin opcode table (Assembler.cpp:
// AC = AC <op> M[addr]) - same shape genBinary's "+"/"tad" already
// uses. "|" has no hardware equivalent at all on this target (no
// `or` in that table), so it's synthesized via De Morgan's instead:
// a|b = ~(~a & ~b), using only `cma` (complement, already relied on
// for unary `-`/`~`) and `and` - verified against the real
// simulator same as every other primitive here.
void Pdp7Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "|") {
        genExpr(lhs);
        code_ << "   cma\n";
        pushAC();  // protect ~lhs - rhs's own evaluation may call
        genExpr(rhs);
        code_ << "   cma\n";
        code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() below uses __stmp internally
        popAC();  // AC = ~lhs
        code_ << "   and __stmp2\n";
        code_ << "   cma\n";
        return;
    }
    genExpr(lhs);
    pushAC();  // protect lhs - rhs's own evaluation may call
    genExpr(rhs);
    code_ << "   dac __stmp2\n";  // __stmp2, not __stmp - popAC() below uses __stmp internally
    popAC();  // AC = lhs
    code_ << "   " << (op == "&" ? "and" : "xor") << " __stmp2\n";
}

// `<<`/`>>` have no *variable-count* backing hardware instruction
// this backend relies on: the EAE `als`/`lrs` shift instructions
// bake their count into the instruction word at assemble time, not
// something that can hold a runtime value the way an ordinary
// operand can. Implemented instead as a counted loop of single-bit
// doublings (`<<`, i.e. `v + v` - reusing the already-verified `+`)
// or, for `>>`, a plain hardware single-bit rotate: `cll` (clear
// link) then `rar` (rotate AC right through the link by exactly
// one) - the old bit 0 falls into the link and is discarded (the
// next `cll` clears it again), and the cleared link's 0 comes in as
// the new bit 17, giving a *logical* right shift with no sign
// extension. Both are O(1) per bit regardless of the value being
// shifted (unlike repeated subtraction, whose own cost scales with
// the value, not the bit count).
void Pdp7Codegen::genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushAC();  // protect the value being shifted - rhs's own evaluation (the shift count) may call
    genExpr(rhs);
    std::string N = allocTemp();
    declareTemp(N);
    code_ << "   dac " << N << "\n";
    popAC();  // AC = the value being shifted
    std::string V = allocTemp();
    declareTemp(V);
    code_ << "   dac " << V << "\n";
    std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
    code_ << Lstart << ":\n";
    code_ << "   lac " << N << "\n";
    code_ << "   sza\n   jmp " << Lbody << "\n   jmp " << Lend << "\n";
    code_ << Lbody << ":\n";
    if (op == "<<") {
        code_ << "   lac " << V << "\n   tad " << V << "\n   dac " << V << "\n";
    } else if (lhs.type.isUnsigned) {
        code_ << "   lac " << V << "\n   cll\n   rar\n   dac " << V << "\n";
    } else {
        // Arithmetic right shift: seed the link with V's own current
        // sign bit (sma skips the next instruction when AC is
        // negative) before rar rotates it into the vacated top bit,
        // so the sign replicates instead of always filling with 0.
        std::string Lpos = newLabel(), Lneg = newLabel(), Lshift = newLabel();
        code_ << "   lac " << V << "\n   sma\n   jmp " << Lpos << "\n   jmp " << Lneg << "\n";
        code_ << Lpos << ":\n   cll\n   jmp " << Lshift << "\n";
        code_ << Lneg << ":\n   stl\n";
        code_ << Lshift << ":\n   lac " << V << "\n   rar\n   dac " << V << "\n";
    }
    code_ << "   lac __zero\n   cma\n   tad " << N << "\n   dac " << N << "\n";
    code_ << "   jmp " << Lstart << "\n";
    code_ << Lend << ":\n";
    code_ << "   lac " << V << "\n";
}

std::string Pdp7Codegen::buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                               const std::string &srcPath, const std::string &outPath) const {
    std::string cmd = "\"" + assemblerPath + "\" --format=" + format;
    if (!outPath.empty()) cmd += " --output \"" + outPath + "\"";
    cmd += " \"" + srcPath + "\"";
    return cmd;
}

// No external assembler for this target anymore - see
// ../../../docs/pdp-c-compiler.md's "Building"/"Usage" and
// Pdp7Assembler.h for why a brand-new, from-scratch, in-process
// assembler was tractable here (Pdp7Codegen's own emitted dialect is
// far narrower than the full historical as.s/as7 language). Always
// returns a value (never falls back to buildAssembleCommand() above,
// which main.cpp keeps around only for backends that still need the
// external-assembler path - see Codegen.h's assembleInProcess()) for
// every format assembledFormats() lists.
std::optional<std::string> Pdp7Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "a7out") return assemblePdp7(asmText, Pdp7OutputFormat::A7Out);
    if (format == "list") return assemblePdp7(asmText, Pdp7OutputFormat::List);
    if (format == "ptr") return assemblePdp7(asmText, Pdp7OutputFormat::Ptr);
    if (format == "rim") return assemblePdp7(asmText, Pdp7OutputFormat::Rim);
    // `--format=coff` always means real separate compilation (see
    // IRBuilder.h's `allowExternal`) - an external/undefined symbol
    // is always allowed here, whole-program single-file compile or
    // not, deferred as a relocation for a future `--link` step.
    if (format == "coff") return assemblePdp7(asmText, Pdp7OutputFormat::Coff, /*allowExternal=*/true);
    return std::nullopt;
}

}  // namespace pcc
