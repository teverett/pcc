#include "Pdp7Codegen.h"

#include <stdexcept>

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
// optimizers/ConstantFolding.h) - has to be written out as its own
// two's-complement 18-bit bit pattern instead of a signed decimal
// string. That's exactly the bit pattern real PDP-7 hardware would
// already compute for it at runtime, so every *read* of the word
// (lac, comparisons, etc.) still sees the same value as before -
// only how it's spelled in the generated assembly text changes.
long pdp7Word(long v) { return v & 0777777; }
}  // namespace

std::string Pdp7Codegen::allocTemp() { return "T" + std::to_string(tempCounter_++); }

void Pdp7Codegen::declareTemp(const std::string &label) { trailer_.push_back(label + ": 0"); }

void Pdp7Codegen::declareArray(const std::string &label, const std::vector<long> &values) {
    // One value per line, label on the first - the shared grammar
    // (../../../../grammar/PccAsm.g4) uses ';' for comments, not as a
    // multi-statement-per-line separator the way as.s's own
    // convention for consecutive words (e.g.
    // src-assem/tests/fork_test.s's "childmsg: <ch>; <il>; <d 012")
    // does - see that grammar's own header comment.
    if (values.empty()) return;
    trailer_.push_back(label + ": " + std::to_string(pdp7Word(values[0])));
    for (size_t i = 1; i < values.size(); i++) {
        trailer_.push_back(std::to_string(pdp7Word(values[i])));
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
            // - see that grammar's own header comment.
            trailer_.push_back(label(slot.get()) + ": " + std::to_string(pdp7Word(slot->arrayInit[0])) + "  ; " +
                                slot->sourceName);
            for (size_t i = 1; i < slot->arrayInit.size(); i++) {
                trailer_.push_back(std::to_string(pdp7Word(slot->arrayInit[i])));
            }
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            trailer_.push_back(label(slot.get()) + ": " + std::to_string(pdp7Word(v)) + "  ; " + slot->sourceName);
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
    size_t sharedStart = trailer_.size();
    trailer_.push_back("__one: 1");
    trailer_.push_back("__zero: 0");
    ensureDivModRoutine();
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

std::map<std::string, std::string> Pdp7Codegen::perFileAsm() const { return perFileAsm_; }

void Pdp7Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunctionLabel_ = label(fn.func);
    code_ << curFunctionLabel_ << ": 0  ; fn " << fn.func->sourceName << "\n";

    for (ir::Slot *p : fn.func->params) declareTemp(label(p));
    genStmt(*fn.body);

    code_ << "   jmp i " << curFunctionLabel_ << "\n";
}

void Pdp7Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lthen = newLabel();
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "   sza\n   jmp " << Lthen << "\n   jmp " << (s.elseStmt ? Lelse : Ldone) << "\n";
            code_ << Lthen << ":\n";
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
            std::string Lbody = newLabel();
            std::string Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "   sza\n   jmp " << Lbody << "\n   jmp " << Lend << "\n";
            code_ << Lbody << ":\n";
            genStmt(*s.loopBody);
            code_ << "   jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            code_ << "   jmp i " << curFunctionLabel_ << "\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            std::string lbl = label(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                declareArray(lbl, s.declSlot->arrayInit);
            } else {
                declareTemp(lbl);
                if (s.initOrReturnOrExpr) {
                    genExpr(*s.initOrReturnOrExpr);
                    code_ << "   dac " << lbl << "\n";
                }
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
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "   lac " << literalLabel(e.intValue) << "\n";
            return;
        }
        case ir::ExprKind::Var: {
            // IRBuilder already rejects a bare array Var outside of
            // `&` (which bypasses this path via buildAddressableOperand),
            // so e.slot is always a scalar here.
            code_ << "   lac " << label(e.slot) << "\n";
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                std::string addr = computeElementAddress(e.slot, *e.lhs);
                genExpr(*e.rhs);
                code_ << "   dac " << addr << " i\n";  // dac leaves AC unchanged
                return;
            }
            genExpr(*e.rhs);
            code_ << "   dac " << label(e.slot) << "\n";  // dac leaves AC unchanged
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
                std::string T = allocTemp();
                declareTemp(T);
                code_ << "   dac " << T << "\n";
                code_ << "   lac " << T << " i\n";
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
            std::string T = allocTemp();
            declareTemp(T);
            code_ << "   dac " << T << "\n";
            genExpr(*e.rhs);  // AC = value to store
            code_ << "   dac " << T << " i\n";
            return;
        }
        case ir::ExprKind::Call: {
            // Builds each argument cell's label directly from
            // (e.func->sourceName, argument index) - exactly label()'s
            // own "P_<ownerFunctionName>_<paramIndex>" convention for
            // a Param Slot - rather than indexing e.func->params[i]:
            // an external function (real separate compilation, see
            // IR.h's `isExternal`) has no real parameter list at all
            // (there's no other file's own signature left to see),
            // so params[i] would read past an empty vector. The
            // call site's own argument count is what's trusted here
            // either way (see IRBuilder.cpp's resolveFunc), and for
            // a LOCAL function this is exactly the same label
            // IRBuilder's own param-registration loop already
            // assigned, so it's correct for both cases uniformly.
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "   dac P_" << e.func->sourceName << "_" << i << "\n";
            }
            code_ << "   jms " << label(e.func) << "\n";
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
std::string Pdp7Codegen::computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);
    std::string P = allocTemp();
    declareTemp(P);
    code_ << "   dac " << P << "\n";
    if (slot->arraySize > 0) {
        code_ << "   law " << label(slot) << "\n";  // base = the array's own address
    } else {
        code_ << "   lac " << label(slot) << "\n";  // base = the pointer variable's value
    }
    code_ << "   tad " << P << "\n";
    code_ << "   dac " << P << "\n";
    return P;
}

void Pdp7Codegen::genIndex(const ir::Expr &e) {
    std::string addr = computeElementAddress(e.slot, *e.lhs);
    code_ << "   lac " << addr << " i\n";
}

// &operand: only a plain variable/array (&slot -> its own address) or
// an array element (&slot[idx] -> that element's address) are valid -
// there's nothing else in this subset with an address worth taking.
void Pdp7Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        code_ << "   law " << label(operand.slot) << "\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        std::string addr = computeElementAddress(operand.slot, *operand.lhs);
        code_ << "   lac " << addr << "\n";  // addr already holds the address itself
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Pdp7Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        std::string T = allocTemp();
        declareTemp(T);
        code_ << "   dac " << T << "\n";
        genExpr(*e.rhs);
        if (op == "-") code_ << "   cma\n   tad __one\n";
        code_ << "   tad " << T << "\n";
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
    std::string T = allocTemp();
    declareTemp(T);
    code_ << "   dac " << T << "\n";
    genExpr(rhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sad " << T << "\n   jmp " << Ltrue << "\n   jmp " << Lfalse << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
    if (negate) code_ << "   xor __one\n";
}

void Pdp7Codegen::emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate) {
    genExpr(lhs);
    std::string T = allocTemp();
    declareTemp(T);
    code_ << "   dac " << T << "\n";
    genExpr(rhs);
    code_ << "   cma\n   tad __one\n   tad " << T << "\n";  // AC = lhs - rhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "   sma\n   jmp " << Lfalse << "\n   jmp " << Ltrue << "\n";
    code_ << Ltrue << ":\n   lac __one\n   jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n   lac __zero\n";
    code_ << Ldone << ":\n";
    if (negate) code_ << "   xor __one\n";
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
    std::string Tl = allocTemp();
    declareTemp(Tl);
    code_ << "   dac " << Tl << "\n";
    genExpr(rhs);
    std::string Tr = allocTemp();  // declared inline below, NOT via declareTemp: it doubles as mul's operand slot
    code_ << "   dac " << Tr << "\n";
    code_ << "   lac " << Tl << "\n   cll\n   mul\n" << Tr << ": 0\n   lacq\n";
}

// Division/modulo via a small shared software routine (repeated
// subtraction) rather than the EAE "idiv" instruction, whose signed
// semantics weren't verified for this compiler. Non-negative
// operands only - see README.
void Pdp7Codegen::ensureDivModRoutine() {
    if (!needDivMod_) return;
    trailer_.push_back("__dm_a: 0");
    trailer_.push_back("__dm_b: 0");
    trailer_.push_back("__dm_q: 0");
    trailer_.push_back("__divmod: 0");
    trailer_.push_back("   dzm __dm_q");
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
    trailer_.push_back("   lac __dm_q");
    trailer_.push_back("   jmp i __divmod");
}

void Pdp7Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    needDivMod_ = true;
    genExpr(lhs);
    code_ << "   dac __dm_a\n";
    genExpr(rhs);
    code_ << "   dac __dm_b\n";
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
        std::string T = allocTemp();
        declareTemp(T);
        code_ << "   dac " << T << "\n";
        genExpr(rhs);
        code_ << "   cma\n";
        code_ << "   and " << T << "\n";
        code_ << "   cma\n";
        return;
    }
    genExpr(lhs);
    std::string T = allocTemp();
    declareTemp(T);
    code_ << "   dac " << T << "\n";
    genExpr(rhs);
    code_ << "   " << (op == "&" ? "and" : "xor") << " " << T << "\n";
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
// extension - verified directly (1000 -> 500). Both are O(1) per
// bit regardless of the value being shifted.
//
// An earlier version of `>>` used the existing `__divmod` routine
// (repeated subtraction) instead of `rar` - reasonable-looking
// (reuse an already-verified primitive, same as `<<` reusing `+`),
// but wrong in practice: `__divmod`'s cost scales with the *value*
// being divided, not the bit count, so shifting a large value right
// by a large count that way could take tens of thousands of
// simulated instructions per shift. Found this by running straight
// into it: `sys/s7.c`'s `wakeup`, shifting an ~17-bit vector right
// by up to 16, ran long enough that it looked indistinguishable
// from a hang under this project's normal `go`/`e ac` test
// harness - not actually incorrect (a breakpoint at the real `hlt`
// confirmed it did eventually get there), just far too slow to be
// usable. `rar` doesn't have this problem since it always costs
// exactly one instruction per bit, independent of the value.
void Pdp7Codegen::genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string V = allocTemp();
    declareTemp(V);
    code_ << "   dac " << V << "\n";
    genExpr(rhs);
    std::string N = allocTemp();
    declareTemp(N);
    code_ << "   dac " << N << "\n";
    std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
    code_ << Lstart << ":\n";
    code_ << "   lac " << N << "\n";
    code_ << "   sza\n   jmp " << Lbody << "\n   jmp " << Lend << "\n";
    code_ << Lbody << ":\n";
    if (op == "<<") {
        code_ << "   lac " << V << "\n   tad " << V << "\n   dac " << V << "\n";
    } else {
        // cll (clear link) then rar (rotate AC right through the
        // link by exactly one) is a plain hardware single-bit
        // logical right shift: the old bit 0 goes into the link
        // (discarded, link cleared again next iteration) and a
        // cleared link's 0 comes in as the new bit 17 - verified
        // directly (1000 -> 500). O(1) per bit, unlike an earlier
        // version of this code that called __divmod (repeated
        // subtraction) once per bit: __divmod's cost scales with
        // the *value*, not the bit count, so shifting a large value
        // right by a large count that way could take tens of
        // thousands of simulated instructions per shift - found
        // this the hard way when `wakeup` (sys/s7.c), shifting an
        // ~17-bit vector right by up to 16, took long enough on the
        // real simulator to make the "go"/"e ac" combination this
        // project's whole test harness relies on misbehave (a
        // separate, still-unexplained SimH console-I/O quirk - see
        // git history/PR discussion for the debugging session this
        // came out of).
        code_ << "   lac " << V << "\n   cll\n   rar\n   dac " << V << "\n";
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
