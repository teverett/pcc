#include "Pdp7Codegen.h"

#include <stdexcept>
#include <utility>

#include "Pdp7Assembler.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// A raw data word (grammar/PccAsm.g4's bare NUMBER syntax has no
// unary minus) must be written as its two's-complement 18-bit pattern
// rather than a signed decimal string.
long pdp7Word(long v) { return v & 0777777; }

// See Pdp7Codegen.h's "Leaf fast path" comment. `exprMakesCall` walks
// every sub-expression a Call/CallPtr could be hiding in;
// `stmtRulesOutLeaf` adds the other disqualifier (a Local/Param
// array).
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

// A wide Local/Param disqualifies the leaf path: the leaf scheme's
// static cells and `__sp`-relative param reads are hardcoded to one
// word per slot. A pointer is always one word regardless of pointee,
// checked first since `.base` names a pointer's pointee type, not its
// own.
bool slotIsWide(const ir::Slot *s) {
    return !isPointer(s->type) && (s->type.base == BaseType::Long || s->type.base == BaseType::LongLong);
}

bool stmtRulesOutLeaf(const ir::Stmt *s) {
    if (!s) return false;
    if (s->kind == ir::StmtKind::LocalDecl && s->declSlot &&
        (s->declSlot->arraySize > 0 || slotIsWide(s->declSlot))) {
        return true;
    }
    // An `asm{...}` block references Param/Local only via ordinary
    // bare-expression statements, which already go through
    // emitLoadSlot()/emitStoreSlot(); it never names `__fp`/`__sp`
    // directly, so it needs no special exclusion here.
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

// A literal element is the two's-complement word text (see
// pdp7Word()); a symbol element is the referenced Slot's or
// Function's label, written bare - the assembler treats a bare
// identifier with no operands as a one-syllable expression evaluating
// to that symbol's resolved address.
std::string Pdp7Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(pdp7Word(e.value));
    return e.slot ? label(e.slot) : label(e.func);
}

void Pdp7Codegen::declareArray(const std::string &label, const std::vector<ir::ArrayInitElem> &values) {
    // One value per line, label on the first - the shared grammar
    // uses ';' for comments, not as a multi-statement-per-line
    // separator.
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

// `char`/`short`/`int` are all one pdp7 word; a slot always occupies
// a full word regardless of declared type, so a `char`'s value (not
// its storage) is masked, and re-sign-extended if signed, at every
// point it's read or converted - never at store time, since a stray
// untruncated upper bit is invisible to any later read.
void Pdp7Codegen::narrowAC(Type to) {
    // `to.base` names a pointer's pointee type, not its own, so
    // narrowing AC as a `char` here would corrupt a real address.
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

// Placement (`@addr`): a placed function/global is emitted in its
// normal declaration-order position but wrapped in a save/jump/
// restore of the location-counter symbol ("."): `__saveloc = .`
// stashes it, `. = ..+addr` jumps to the placed address, the item is
// emitted there, then `. = __saveloc` resumes normal placement.
//
// `addr` is an offset from `..` (the relocation base), not a true
// absolute address: the `rim`/`ptr` punch formats only emit memory
// from `..` upward, so an address below it would be silently absent
// from the binary even though it shows in an `a7out` listing.
// `__saveloc` is reused across every placement (never nested).
std::string Pdp7Codegen::generate(const ir::Program &prog, bool objectMode) {
    // Every Slot/Function name is already resolved by IRBuilder - no
    // symbol table of our own, just emit code and declare storage.
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }

    // --dump-asm wants one file per input, not one combined dump.
    // Tracked as (sourceFile, start, end) ranges into code_ (by
    // tellp position) and trailer_ (by index range), built into
    // perFileAsm_ at the end.
    std::vector<std::tuple<std::string, size_t, size_t>> codeRanges;
    std::vector<std::tuple<std::string, size_t, size_t>> trailerRanges;

    // Real separate compilation (`--format=coff`) means `prog` may
    // not define `main` at all; `objectMode` means no object emits
    // the entry sequence itself - the linker synthesizes exactly one
    // once every object is merged. `homeFile` is just --dump-asm's
    // per-file attribution for the handful of things with no single
    // owning Slot/Function.
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
        // See the Placement comment above generate().
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
        // An external global isn't defined in this file - it must
        // never get a storage word here, only be referenced by name,
        // letting the assembler defer it to a relocation.
        if (slot->isExternal) continue;
        size_t gTrailerStart = trailer_.size();
        if (slot->hasAddress) {
            trailer_.push_back("__saveloc = .");
            trailer_.push_back(". = ..+" + std::to_string(slot->address));
        }
        if (slot->arraySize > 0) {
            // One value per line (';' is a comment in the shared
            // grammar, not a statement separator). Each element
            // reserves `elemW` consecutive words (word 0/low first);
            // a symbol element only fills word 0, zero-padded above.
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

    // Shared backend-internal cells with no single owning Slot/
    // Function, attributed to main's own file. Only emitted in
    // whole-program mode: under real separate compilation each `.c`
    // file assembles as its own independent object, and the
    // assembler resolves an in-file-defined label like `__sp` to a
    // local address, so defining these per-file would give each
    // object its own private `__sp`/`__fp`/`__retaddr`, corrupting
    // any call crossing an object-file boundary. Under `objectMode`
    // every file instead leaves these as external references, and
    // `Pdp7LinkerFactory::makePdp7Linker()`'s entry object supplies
    // the one shared definition.
    if (!objectMode) {
        size_t sharedStart = trailer_.size();
        trailer_.push_back("__one: 1");
        trailer_.push_back("__zero: 0");
        // `__sp` starts at 8191 (017777 octal), the highest word the
        // 13-bit direct-addressing field can reach, and grows
        // downward toward where code/data grows up from
        // `kPdp7BaseAddress` - the two regions only collide if
        // combined code+data and worst-case call depth exceed the
        // 8192-word window. `__fp`'s starting value is never read
        // before the first call gives it a real one, so 0 is a
        // placeholder. `__stmp`/`__addr`/`__retval`/`__retaddr` are
        // shared scratch cells, safe under recursion since each use
        // is a single short, call-free sequence.
        trailer_.push_back("__sp: 8191");
        trailer_.push_back("__fp: 0");
        trailer_.push_back("__stmp: 0");
        trailer_.push_back("__stmp2: 0");
        trailer_.push_back("__addr: 0");
        trailer_.push_back("__retval: 0");
        trailer_.push_back("__retaddr: 0");
        // A wide return value can't survive emitEpilogueAndReturn()'s
        // "__sp = __fp" teardown the way a narrow one survives in
        // `__retval`; these four cells (enough for `long long`'s
        // 4-word width) are the wide equivalent, word i in
        // `__retvalW<i>`.
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
        // Default return if the body doesn't return on every path;
        // dead code otherwise.
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
    // Params sit on the stack at entry, closest-first (Param 0 pushed
    // last by the caller), so Param k is at `__sp+k` - read without
    // advancing `__sp`, since the caller's own post-call cleanup
    // unconditionally reclaims every argument it pushed regardless of
    // whether the callee was a leaf.
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
    (void)hasValue;  // AC already holds the return value, if any
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
    // Each element occupies `elemW` consecutive words, word 0 at the
    // lowest address: element i's word j sits at
    // `__fp-(frameOffset+i*elemW-j)`.
    int elemW = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        for (int j = 0; j < elemW; j++) {
            if (!elem.isSymbol) {
                long word = (elem.value >> (18 * j)) & 0777777L;
                code_ << "   lac " << literalLabel(word) << "\n";
            } else if (j > 0) {
                // A symbol element is a plain native-word address;
                // only word 0 holds it, every higher word is zero.
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

// dest = a + b + carryCell, updating carryCell to this word's carry-
// out. `tad` complements the link on 18-bit overflow (each `cll`
// starts from a known-zero link, so complement and set coincide). No
// single `tad` captures "carry out of a+b+carryIn" in one step, so
// this computes it as two half-adds - `a+b` (carry1), then
// conditionally `+1` (carry2, only when `a+b` wrapped to exactly 0
// with carryIn 1) - ORed together.
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

// Each word is independent - no carry chain needed. `|` is synthesized
// via De Morgan, word-by-word (see genBitwise()).
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
    // operands.
    bool negate = (op == ">=" || op == "<=");
    bool swap = (op == ">" || op == "<=");
    if (swap) std::swap(lhsT, rhsT);
    // Compare from the most significant word down, short-circuiting
    // once a word differs; only the top word's compare flips sign
    // (via the sign-bit-flip trick) when `isUnsigned`, since the sign
    // only lives in the top word and lower words are always unsigned
    // magnitude. Each word's compare goes through emitSignedLtCells()
    // rather than a plain subtract-and-test-sign, since the flip
    // trick routinely produces operands at opposite extremes.
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

// Multiply and divide/modulo are not implemented for a wide operand:
// a genuine multi-word multiply/divide needs the ripple-carry
// machinery nested inside its own bit-serial or word-serial loop
// (shift-and-add / compare-shift-subtract), and this backend has no
// general-purpose register to hold the several words such a routine
// needs live at once.
void Pdp7Codegen::genWideMulDivErr(int line, const std::string &opName) {
    err(line, "'" + opName + "' on a 'long'/'long long' operand isn't supported on this backend (pdp7 has no "
                               "general-purpose register and no verified multi-word multiply/divide routine - "
                               "see Pdp7Codegen.cpp's own genWideMulDivErr() comment)");
}

// Only called when `e.type` is narrow (genExpr()'s Convert case
// dispatches to genWideExpr() when wide). A wide source contributes
// only its low word, discarding the rest; a narrow source just goes
// through narrowAC().
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

// Generalizes genExpr() for a wide-typed expression: leaves its
// `widthOfType(e.type)` words pushed on the stack, word 0 (low) on
// top, instead of in AC.
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
            // computeElementAddress() scales by the element width and
            // leaves that element's word-0 address pushed.
            computeElementAddress(e.slot, *e.lhs);
            popAC();
            pushWideFromAddress(e.type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Same shape as a wide Assign's non-indexed branch, except
            // the store address comes from evaluating e.lhs (the
            // pointer, always narrow) instead of emitLoadSlotAddress().
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
                // A wide pointee - the pointer itself is always a
                // narrow, single-word address.
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
            // Same argument-passing convention as genExpr()'s Call
            // case. A wide callee's emitEpilogueAndReturn() leaves its
            // return words pushed at `__sp` as it stands right after
            // return - `totalWords` (the argument words) further down
            // than where argument-pushing left it, not where
            // `__sp += totalWords` alone would put it. So: pop the
            // return words from where they actually are first, undo
            // the argument push, then push the saved words back to
            // where the rest of this file expects a wide stack value.
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
            // them) directly into the branch; anything else falls
            // back to evaluate-then-`sna` inside genCondBranch()
            // itself.
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
            // `continue` jumps here if this loop has a post-statement
            // (a desugared for-loop's), so it still runs before the
            // condition is retested; otherwise it's just Lstart.
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
            if (hasValue) {
                if (isWide(s.initOrReturnOrExpr->type)) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    genExpr(*s.initOrReturnOrExpr);
                }
            }
            if (currentFunctionIsLeaf_) {
                // No frame teardown on this path - a wide value's
                // words stay exactly where genWideExpr left them.
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
                // path, addressed via emitLoadSlotAddress(); genExpr()'s
                // wide-collapsing-to-AC shortcut would drop all but
                // the low word, so this uses genWideExpr()/
                // popWideToAddress() directly.
                if (s.initOrReturnOrExpr) {
                    genWideExpr(*s.initOrReturnOrExpr);
                } else {
                    pushWideLiteral(s.declSlot->type, 0);
                }
                emitLoadSlotAddress(s.declSlot);
                popWideToAddress(s.declSlot->type);
            } else {
                // A leaf function's Local gets a static cell the one
                // time its declaration is generated.
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
            // Verbatim passthrough - the text between the braces goes
            // straight into the generated assembly, unmodified.
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
    }
}

void Pdp7Codegen::genExpr(const ir::Expr &e) {
    // genExpr()'s contract is "leaves the result in AC" - a wide
    // result can't fit there, so this computes it via genWideExpr()
    // and collapses to just the low word in AC, discarding the rest.
    // Callers that need the full wide value (LocalDecl's initializer,
    // Return) call genWideExpr() directly instead.
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
            // IRBuilder rejects a bare array Var outside of `&`, so
            // e.slot is always a scalar here.
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
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] <op>= rhs (including slot[idx]++/--)
                computeElementAddress(e.slot, *e.lhs);  // pushes the element's own address
                genCompoundThroughStackedAddress(e);
                return;
            }
            // Plain scalar target - its own address (a Global's label,
            // or a Local/Param's `__fp`-relative offset) is cheap
            // enough to recompute from scratch after rhs's own
            // evaluation, the same way plain Assign's own non-indexed
            // branch above already does, so there's nothing here that
            // needs to survive across a nested call the way an
            // indexed/deref target's own sub-expression does.
            genExpr(*e.rhs);  // AC = rhs
            std::string rhsCell = prepareCompoundRhs(e);
            emitLoadSlot(e.slot);  // AC = old
            if (e.yieldOld) pushAC();
            finishCompoundOp(e, rhsCell);
            narrowAC(e.type);
            emitStoreSlot(e.slot);  // leaves AC (the new value) unchanged
            if (e.yieldOld) popAC();
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // AC = pointer value
            pushAC();          // protect it - evaluating rhs may itself call
            genCompoundThroughStackedAddress(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            // Mirrors StmtKind::If's own label/branch shape exactly,
            // using genExpr (not genStmt) for both branches, since a
            // ternary produces a value rather than running a
            // statement.
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
            // Arguments are pushed in reverse order so param 0 ends up
            // closest to the callee's `__fp`; the callee reads every
            // Param off this stack, never a fixed per-function cell,
            // so recursive calls never collide. Each argument may
            // itself contain a call safely, since every earlier
            // argument is already on the stack by the time a later
            // one evaluates.
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
            // The callee's epilogue leaves AC holding the return value
            // and `__sp` right where it was after the arguments were
            // pushed - caller-cleans-up convention, so `__retval`
            // protects the return value in AC while `__sp` is adjusted
            // back past `totalWords` (the real word count pushed,
            // since a wide argument pushes several words).
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
            // e.args is always empty - IRBuilder.cpp rejects any
            // argument on this kind, since there's no ABI slot for one
            // until the callee is known, which it never is here.
            genExpr(*e.lhs);  // AC = the callee's entry address
            code_ << "   dac __addr\n";
            code_ << "   jms __addr i\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // ion/iof aren't in pdp7as's builtin opcode table; emitting
            // the literal instruction word directly avoids needing to
            // link against the kernel's own definition of them.
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

// &slot[idxExpr], pushed on the stack. slot is the array itself when
// it's a declared array (slot->arraySize > 0); for a plain pointer
// variable the base address to index from is its value (lac), not
// its storage address (law) - indexing a pointer parameter needs this
// distinction.
void Pdp7Codegen::computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr) {
    // A Local array's elements sit at decreasing addresses as the
    // index increases (opposite a Global array's layout), so the
    // index is negated up front, making the final `tad` compute
    // `base - index` for a Local array but `base + index` otherwise.
    bool descending = slot->arraySize > 0 && slot->kind == ir::SlotKind::Local;
    genExpr(idxExpr);     // AC = index (may itself call)
    // Scale the index by the element width - unrolled as elemW-1
    // self-adds rather than a real multiply, cheap for widths 1/2/4.
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

// &operand: a plain variable/array, an array element, or a bare
// function name (whose entry label is the same address a direct
// `jms` call targets).
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

std::string Pdp7Codegen::prepareCompoundRhs(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        code_ << "   dac __stmp2\n";
        return "__stmp2";
    }
    if (op == "-") {
        code_ << "   cma\n   tad __one\n";
        code_ << "   dac __stmp2\n";
        return "__stmp2";
    }
    if (op == "&" || op == "^") {
        code_ << "   dac __stmp2\n";
        return "__stmp2";
    }
    if (op == "|") {
        code_ << "   cma\n";
        code_ << "   dac __stmp2\n";
        return "__stmp2";
    }
    if (op == "*") {
        // Not declared via declareTemp(): same inline-operand
        // convention as genMul() - the "<Tr>: 0" declaration happens
        // in finishCompoundOp(), right after the "mul" it's the
        // operand word for.
        std::string Tr = allocTemp();
        code_ << "   dac " << Tr << "\n";
        return Tr;
    }
    if (op == "/" || op == "%") {
        needDivMod_ = true;
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

void Pdp7Codegen::finishCompoundOp(const ir::Expr &e, const std::string &rhsCell) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        code_ << "   tad " << rhsCell << "\n";
        return;
    }
    if (op == "&" || op == "^") {
        code_ << "   " << (op == "&" ? "and" : "xor") << " " << rhsCell << "\n";
        return;
    }
    if (op == "|") {
        code_ << "   cma\n";
        code_ << "   and " << rhsCell << "\n";
        code_ << "   cma\n";
        return;
    }
    if (op == "*") {
        code_ << "   cll\n   mul\n" << rhsCell << ": 0\n   lacq\n";
        return;
    }
    if (op == "/" || op == "%") {
        code_ << "   dac __dm_a\n";
        code_ << "   jms __divmod\n";
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
        if (op == "<<") {
            code_ << "   lac " << V << "\n   tad " << V << "\n   dac " << V << "\n";
        } else if (e.type.isUnsigned) {
            code_ << "   lac " << V << "\n   cll\n   rar\n   dac " << V << "\n";
        } else {
            std::string Lpos = newLabel(), Lneg = newLabel(), Lshift = newLabel();
            code_ << "   lac " << V << "\n   sma\n   jmp " << Lpos << "\n   jmp " << Lneg << "\n";
            code_ << Lpos << ":\n   cll\n   jmp " << Lshift << "\n";
            code_ << Lneg << ":\n   stl\n";
            code_ << Lshift << ":\n   lac " << V << "\n   rar\n   dac " << V << "\n";
        }
        code_ << "   lac __zero\n   cma\n   tad " << rhsCell << "\n   dac " << rhsCell << "\n";
        code_ << "   jmp " << Lstart << "\n";
        code_ << Lend << ":\n";
        code_ << "   lac " << V << "\n";
        return;
    }
    err(e.line, "internal: unhandled compound-assignment operator '" + op + "'");
}

void Pdp7Codegen::genCompoundThroughStackedAddress(const ir::Expr &e) {
    genExpr(*e.rhs);  // may itself call - the address is safe on the stack
    std::string rhsCell = prepareCompoundRhs(e);
    std::string addr = popAddressIntoScratch();
    code_ << "   lac " << addr << " i\n";  // AC = old
    if (e.yieldOld) pushAC();
    finishCompoundOp(e, rhsCell);
    narrowAC(e.type);
    code_ << "   dac " << addr << " i\n";
    if (e.yieldOld) popAC();
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
    // A comparison's result type is always narrow Int even when its
    // operands are wide, so genExpr()'s isWide(e.type) guard doesn't
    // catch this - dispatch on the operands themselves here.
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

// Skip/branch primitives used below:
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
    // Unsigned path: XOR each side's sign bit (0400000, bit 17)
    // before the signed-less-than test, mapping unsigned order onto
    // signed order. Delegates to emitSignedLtCells() rather than a
    // plain subtract-and-test-sign, since the flip routinely produces
    // operands at opposite extremes, which a naive subtract overflows.
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

// See Pdp7Codegen.h's comment for what this is for. Each comparison
// computes AC = a - b (reusing emitLt()/emitEq()'s own instruction
// choices), then branches on that raw result directly instead of
// materializing a 0/1 value first: `<`/`>` use `sma` (skip if AC<0,
// swapping operands for `>`); `>=`/`<=`/`==` use a 3-word jump-either-
// way shape since `spa` is unused/unverified elsewhere in this file;
// `!=` uses `sad`'s natural "skip if different" sense directly.
// `&&` recurses, short-circuiting to the same `Lfalse`. `||` and
// anything else (`!`, a bare variable, a call, bitwise, ...) fall
// through to the generic path: evaluate normally, test the 0/1 value.
void Pdp7Codegen::genCondBranch(const ir::Expr &cond, const std::string &Lfalse) {
    if (cond.kind == ir::ExprKind::Binary) {
        const std::string &op = cond.op;
        if (op == "&&") {
            genCondBranch(*cond.lhs, Lfalse);
            genCondBranch(*cond.rhs, Lfalse);
            return;
        }
        // A wide-operand comparison can't use the fused narrow
        // machinery below, since it never materializes a value
        // genExpr() could read without truncating it - fall through
        // to the generic path, which dispatches to genWideCompare().
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
            // emitSignedLtCells() tests A<B directly; negate just
            // picks which outcome continues to Lfalse.
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

// Multiply via the hardware EAE "mul" instruction: it reads its
// multiplicand as the literal data word immediately following it in
// memory, leaving a double-length product in AC:MQ; "lacq" brings the
// low-order 18-bit half into AC. Only verified for non-negative
// operands.
void Pdp7Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    pushAC();  // protect lhs - rhs's own evaluation may call
    genExpr(rhs);
    std::string Tr = allocTemp();  // not declareTemp(): doubles as mul's operand slot
    code_ << "   dac " << Tr << "\n";
    popAC();  // AC = lhs
    code_ << "   cll\n   mul\n" << Tr << ": 0\n   lacq\n";
}

// Division/modulo via a shared software routine: repeated subtraction
// on the operands' absolute values, with each result's sign reapplied
// afterward, rather than the EAE "idiv" instruction. Matches C's
// truncating division: the quotient's sign is the XOR of the operand
// signs, and the remainder's sign always matches the dividend's.
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

// "&"/"^" are real pdp7 memory-reference instructions (AC = AC <op>
// M[addr]). "|" has no hardware equivalent, so it's synthesized via
// De Morgan: a|b = ~(~a & ~b).
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

// `<<`/`>>` have no variable-count hardware instruction (EAE
// `als`/`lrs` bake their count into the instruction word at assemble
// time). Implemented as a counted loop: `<<` is repeated `v+v`; `>>`
// is a single-bit rotate (`cll` then `rar`), a logical shift with no
// sign extension unless the link is seeded with the sign bit first.
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
        // Arithmetic right shift: seed the link with V's sign bit
        // before `rar` rotates it into the vacated top bit, so the
        // sign replicates instead of filling with 0.
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

// No external assembler for this target - always returns a value for
// every format assembledFormats() lists, never falling back to
// buildAssembleCommand() above.
std::optional<std::string> Pdp7Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "a7out") return assemblePdp7(asmText, Pdp7OutputFormat::A7Out);
    if (format == "list") return assemblePdp7(asmText, Pdp7OutputFormat::List);
    if (format == "ptr") return assemblePdp7(asmText, Pdp7OutputFormat::Ptr);
    if (format == "rim") return assemblePdp7(asmText, Pdp7OutputFormat::Rim);
    // `--format=coff` means real separate compilation: an undefined
    // symbol is deferred as a relocation for a future `--link` step.
    if (format == "coff") return assemblePdp7(asmText, Pdp7OutputFormat::Coff, /*allowExternal=*/true);
    return std::nullopt;
}

}  // namespace pcc
