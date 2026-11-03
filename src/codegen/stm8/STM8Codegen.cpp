#include "STM8Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "STM8Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing, and the
// hardware stack's own initial top - same "comfortably clear of
// everything else this backend could generate, inside a flat 64KB
// address space" convention as every other bare-metal 8-bit backend
// here (see I8080Codegen.cpp's own comment on both constants).
constexpr uint32_t kBaseAddress = 0x0100;
constexpr uint32_t kStackTop = 0xFF00;

// Where this backend's own software frame stack starts (see
// STM8Codegen.h's own `genPrologue`/`genSlotAddress` comments) -
// comfortably clear of both the code/data region above
// `kBaseAddress` and the real hardware stack at `kStackTop`, with
// plenty of room between the two for either to grow without
// colliding, in the same "bare flat 64KB address space" spirit as
// `kBaseAddress`/`kStackTop` themselves.
constexpr uint32_t kFrameStackTop = 0xE000;
}  // namespace

void STM8Codegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": db 0, 0"); }

// A literal element's own decimal text, or a symbol element's own
// label text (the referenced Slot's or Function's own address) - the
// shared assembler's own "dw" directive (STM8Assembler.cpp's emit16)
// resolves either into a real, full 16-bit value and writes it out
// big-endian, the same order a `ldwi`/`call` operand already gets.
std::string STM8Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void STM8Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    std::ostringstream line;
    line << lbl << ": dw ";
    for (size_t i = 0; i < values.size(); i++) {
        if (i > 0) line << ", ";
        line << arrayElemText(values[i]);
    }
    trailer_.push_back(line.str());
}

void STM8Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    ldw x, " << src << "\n    ldw " << dst << ", x\n";
}

void STM8Codegen::storeIndirect(const std::string &src) {
    // Assumes `x` already holds the target address - `ld`/`stxind`
    // address `src` directly, by name, never touching `x` itself
    // until the `incw` between the two bytes, so the caller's address
    // in `x` only ever advances forward, same convention as
    // I8080Codegen::storeAcIndirect.
    code_ << "    ld a, " << src << "\n    stxind\n";
    code_ << "    incw x\n";
    code_ << "    ld a, " << src << "+1\n    stxind\n";
}

void STM8Codegen::loadIndirect(const std::string &dst) {
    code_ << "    ldxind\n    ld " << dst << ", a\n";
    code_ << "    incw x\n";
    code_ << "    ldxind\n    ld " << dst << "+1, a\n";
}

// One in-place 1-bit shift of the 16-bit memory cell `lbl`, used by
// genShift()'s own loop and inlined directly by genMul()/
// genDivMod() - via the real word-wide `SLAW`/`SRLW` (see
// STM8Codegen.h's own header comment on why there's still no
// variable-count shift to use instead, even though these exist).
void STM8Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    code_ << "    ldw x, " << lbl << "\n";
    code_ << "    " << (isLeft ? "slaw" : "srlw") << " x\n";
    code_ << "    ldw " << lbl << ", x\n";
}

std::string STM8Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all - an ordinary object
    // file never does. `objectMode` (see Codegen.h) means no object
    // file ever emits this entry sequence itself, even the one that
    // happens to define `main` - STM8LinkerFactory.cpp is solely
    // responsible for synthesizing exactly one, once every object is
    // merged and `main`'s linked address is resolvable (see
    // Linker.h). An object file's own base is `org 0` - it's the
    // linker's job to place it, not this backend's.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    ldwi sp, " << kStackTop << "\n";
        // The software frame stack's own initial top, and `main`'s
        // own caller-side `fp` link - nothing ever reads this `fp`
        // value again once the whole program halts, so its initial
        // content doesn't matter beyond being a well-defined word.
        code_ << "    ldwi x, " << kFrameStackTop << "\n    ldw fsp, x\n";
        code_ << "    ldwi x, 0\n    ldw fp, x\n";
        code_ << "    call " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative place to
        // read it from. This extra `ldw` leaves it in the real `x`
        // register too, for a quick look with nothing more than a
        // register dump.
        code_ << "    ldw x, ac\n";
        code_ << "halt:\n    jp halt\n\n";
    }

    // The fixed memory pseudo-registers - see STM8Codegen.h's own
    // header comment for what each one is for. `lc` is this
    // backend's own loop counter (genMul()/genShift() need one but
    // can't use `a` - see their own comments), a plain byte, not one
    // of the five 16-bit cells. Every genExpr() call anywhere in the
    // *whole linked program* shares these same cells (a callee's own
    // return value and a caller's own next operand both pass through
    // `ac`), so unlike every other symbol this backend emits, they
    // can't each be redeclared per-object: in whole-program mode
    // they're declared once, right here; in object mode, this file
    // never declares them at all - every reference is then
    // automatically deferred as an external relocation, resolved
    // against the one real definition STM8LinkerFactory.cpp's own
    // synthesized entry object provides.
    if (!objectMode) {
        declareTemp("ac");
        declareTemp("bc");
        declareTemp("cx");
        declareTemp("dx");
        declareTemp("ptr");
        // `fp`/`fsp` are this backend's own software frame pointer
        // and frame-stack pointer (see STM8Codegen.h's own
        // `genPrologue`/`genSlotAddress` comments) - persistent
        // state across a whole call tree, so they're named memory
        // cells like every other pseudo-register here, never the
        // real `x`/`y` (both already fully committed as ephemeral
        // expression-evaluation transit registers everywhere else in
        // this file). They're entirely independent of the real
        // hardware `sp` (still used only for `call`/`ret` and
        // genBinary()'s own balanced `push`/`pop` scratch).
        declareTemp("fp");
        declareTemp("fsp");
        trailer_.push_back("lc: db 0");
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name in
        // code, letting the assembler's own undefined-symbol
        // handling defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            // High byte first, low byte second - see declareArray()'s
            // own comment.
            trailer_.push_back(lbl + ": db " + std::to_string((v16 >> 8) & 0xFF) + "  ; " + slot->sourceName);
            trailer_.push_back("db " + std::to_string(v16 & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (see STM8Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed to
// whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> STM8Codegen::perFileAsm() const { return perFileAsm_; }

void STM8Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    currentFunction_ = fn.func;
    genPrologue(*fn.func);
    genStmt(*fn.body);
    genEpilogue(*fn.func);
    currentFunction_ = nullptr;
    code_ << "\n";
}

// --- Per-call frame management ------------------------------------------
//
// Layout, growing down from `fp` (the address of this call's own
// saved caller's `fp`, i.e. the first word pushed onto the frame
// stack for this call):
//
//   fp+2*numParams .. fp+2   incoming arguments (param 0 closest to
//                            `fp` - the caller pushes them in reverse
//                            order, last argument first, so param 0
//                            ends up pushed last - see pushFrameWord()'s
//                            own call sites in the Call case below)
//   fp                       this call's own saved caller's `fp`
//   fp-2 .. fp-2*frameSize   this function's own Local storage, in
//                            declaration order (frameOffset 1 first)
//
// A Local's growing-downward layout (and its array elements'
// matching base-minus-index addressing - see genSlotAddress()) is
// the same shape I386Codegen.cpp's own `addr()`/`computeElementAddr`
// use for `ebp`-relative locals; the one real difference is that
// there's no spare STM8 register to hold a live frame-pointer
// snapshot the way `ebp` does on i386 (`x`/`y` are both already
// fully committed as this file's own ephemeral expression-evaluation
// transit registers - see STM8Codegen.h's own header comment), so
// `fp`/`fsp` are ordinary named memory cells instead, entirely
// separate from the real hardware `sp` (which keeps doing exactly
// what it already did - `call`/`ret` and genBinary()'s own balanced
// `push`/`pop` scratch - untouched by any of this).
void STM8Codegen::pushFrameWord(const std::string &cell) {
    code_ << "    ldw x, fsp\n    ldwi y, 2\n    subw x, y\n    ldw fsp, x\n";
    storeIndirect(cell);  // x still holds the just-decremented fsp
}

void STM8Codegen::genPrologue(const ir::Function &fn) {
    pushFrameWord("fp");           // save the caller's own fp
    code_ << "    ldw x, fsp\n    ldw fp, x\n";  // fp = address of that saved word
    if (fn.frameSize > 0) {
        code_ << "    ldw x, fp\n    ldwi y, " << (fn.frameSize * 2) << "\n    subw x, y\n    ldw fsp, x\n";
    }
}

// Mirrors genPrologue(): recovers the caller's own `fp` (read from
// the word at the current `fp` address, *before* `fp` itself is
// overwritten) and resets `fsp` back to its value just before this
// call's own frame was pushed, i.e. past this call's own incoming
// arguments too (`numParams` words) - the callee cleans up its own
// argument area, so the caller's own post-`call` code needs no
// matching adjustment. Uses `bc` as scratch (dead at every statement
// boundary - see genBinary()'s own header comment), never `ac`,
// since `ac` already holds this call's own live return value by the
// time this runs (see the Return case below) - `ret` itself is
// always the last instruction, strictly after `fp`/`fsp` are both
// restored, same "duplicated at every return site" shape
// I386Codegen.cpp's own genFunction()/Return case uses.
void STM8Codegen::genEpilogue(const ir::Function &fn) {
    code_ << "    ldw x, fp\n";
    loadIndirect("bc");  // bc = the caller's own saved fp
    int numParams = static_cast<int>(fn.params.size());
    code_ << "    ldw x, fp\n    ldwi y, " << ((numParams + 1) * 2) << "\n    addw x, y\n    ldw fsp, x\n";
    code_ << "    ldw x, bc\n    ldw fp, x\n";
    code_ << "    ret\n";
}

// Leaves a Slot's own effective storage address in `x` - see
// STM8Codegen.h's own header comment.
void STM8Codegen::genSlotAddress(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    ldwi x, " << label(slot) << "\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    ldw x, fp\n    ldwi y, " << ((slot->paramIndex + 1) * 2) << "\n    addw x, y\n";
        return;
    }
    code_ << "    ldw x, fp\n    ldwi y, " << (slot->frameOffset * 2) << "\n    subw x, y\n";
}

void STM8Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    ldw x, ac\n    tnzw x\n";
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jreq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jp " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lend = newLabel();
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    ldw x, ac\n    tnzw x\n";
            code_ << "    jreq " << Lend << "\n";
            genStmt(*s.loopBody);
            code_ << "    jp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            genEpilogue(*currentFunction_);
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // A Local's words are already reserved by genPrologue()
            // (`frameSize` accounts for every one of them) - nothing
            // is separately declared here the way a Global is. An
            // array initializer still has to run its own per-element
            // stores every time this declaration executes (not once,
            // statically): a fresh call gets a fresh copy of the
            // frame, so a shared one-time initializer the way
            // declareArray() gives a Global would leak the *previous*
            // call's own last values into every new call instead.
            if (s.declSlot->arraySize > 0) {
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    const ir::ArrayInitElem &elem = s.declSlot->arrayInit[i];
                    if (elem.isSymbol && elem.slot && elem.slot->kind != ir::SlotKind::Global) {
                        // "&otherLocal"/"&someParam" - unlike a
                        // Global's or a Function's own address (both
                        // real compile-time constants arrayElemText()
                        // can hand straight to `ldwi`), a Local's or
                        // a Param's own address only exists at
                        // runtime, relative to this call's own `fp` -
                        // same genSlotAddress() every other Local/
                        // Param access goes through.
                        genSlotAddress(elem.slot);
                        code_ << "    ldw ac, x\n";
                    } else {
                        code_ << "    ldwi x, " << arrayElemText(elem) << "\n    ldw ac, x\n";
                    }
                    genSlotAddress(s.declSlot);  // x = address of element 0
                    if (i > 0) {
                        // Element i sits *below* element 0 - see
                        // genSlotAddress()'s own header comment on why
                        // a Local array's addresses decrease as its
                        // own index increases, the opposite of a
                        // Global array.
                        code_ << "    ldwi y, " << (i * 2) << "\n    subw x, y\n";
                    }
                    storeIndirect("ac");
                }
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                genSlotAddress(s.declSlot);
                storeIndirect("ac");
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
    }
}

void STM8Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldwi x, " << v16 << "\n    ldw ac, x\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16("ac", label(e.slot));
            } else {
                genSlotAddress(e.slot);
                loadAcIndirect();
            }
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                code_ << "    ldw x, ptr\n    push x\n";
                genExpr(*e.rhs);
                code_ << "    pop x\n";
                storeAcIndirect();
                return;
            }
            genExpr(*e.rhs);
            if (e.slot->kind == ir::SlotKind::Global) {
                copy16(label(e.slot), "ac");
            } else {
                genSlotAddress(e.slot);
                storeAcIndirect();
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            code_ << "    ldw x, ac\n    push x\n";
            genExpr(*e.rhs);
            code_ << "    pop x\n";
            storeAcIndirect();
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ldw x, ac\n    negw x\n    ldw ac, x\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                code_ << "    ldw x, ac\n";
                loadAcIndirect();
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    ldw x, ac\n    cplw x\n    ldw ac, x\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Pushed onto the frame stack in reverse (last argument
            // first), so param 0 ends up pushed *last* - right next
            // to the callee's own saved `fp` - matching
            // genSlotAddress()'s own `fp + (paramIndex+1)*2` formula
            // (param 0 at `fp+2`, param 1 at `fp+4`, ...). Each
            // argument lands directly in the callee's own fresh
            // frame - there's no shared per-function cell for this to
            // ever alias between a call and a nested recursive call
            // of the same function.
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                pushFrameWord("ac");
            }
            code_ << "    call " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no callee to
            // push a frame for yet, since the callee's own identity
            // isn't known until this is evaluated at runtime.
            //
            // The real STM8 `CALL [mem]` Indirect addressing mode
            // doesn't treat its own operand's address as the callee
            // directly - it fetches the actual entry address from the
            // word stored there (see STM8Assembler.cpp's own
            // `callind` comment), so the callee's own address (just
            // computed into `ac`) is spilled to a fresh temp cell
            // first, then `callind` is pointed at that cell - a real
            // hardware addressing mode, used exactly the way it calls
            // through an ordinary function-pointer variable in memory.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string T = allocTemp();
            declareTemp(T);
            copy16(T, "ac");
            code_ << "    callind " << T << "\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real STM8 interrupt enable/disable - RIM/SIM, the same
            // real hardware feature as the 8080/6502/8086 backends'
            // own ei/di-style intrinsics.
            code_ << "    " << (e.op == "ion" ? "rim" : "sim") << "\n";
            return;
        }
    }
}

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell - same role, and the same "idxExpr evaluated first and alone"
// caution, as I8080Codegen::genIndexAddr (see that file's own header
// comment - nothing here is safe to hold in `x`/`y`/`ptr`/`bc`/etc.
// across a general recursive genExpr() call).
void STM8Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    code_ << "    ldw x, ac\n    slaw x\n";  // x = index*2 (words are 2 bytes here too)
    code_ << "    ldw ptr, x\n";
    genSlotAddress(slot);  // x = slot's own storage address (Global/Local/Param alike)
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    ldw y, x\n";  // y = base (the array's own address)
            code_ << "    ldw x, ptr\n    addw x, y\n    ldw ptr, x\n";  // ptr = base + index*2
        } else {
            // A Local/Param array's own words sit at *decreasing*
            // addresses as its own element index increases
            // (genSlotAddress()'s own storage address is the array's
            // first word, closest to `fp`; later words are further
            // below it - see genSlotAddress()'s own header comment) -
            // the mirror image of a Global array's increasing-address
            // layout above, so this is base-minus-scaled-index, not
            // base-plus.
            code_ << "    ldw y, ptr\n    subw x, y\n    ldw ptr, x\n";  // ptr = base - index*2
        }
        return;
    }
    // y = base (a pointer variable's own stored value, read from its
    // just-computed storage address - a Global's, a Local's, or a
    // Param's, all alike) - ordinary pointer arithmetic, always
    // base-plus-scaled-index regardless of where the pointer
    // variable itself happens to live.
    loadIndirect("bc");
    code_ << "    ldw y, bc\n";
    code_ << "    ldw x, ptr\n    addw x, y\n    ldw ptr, x\n";
}

void STM8Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldw x, ptr\n";
    loadAcIndirect();
}

void STM8Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        genSlotAddress(operand.slot);
        code_ << "    ldw ac, x\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        code_ << "    ldwi x, " << label(operand.func) << "\n    ldw ac, x\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// Every binary op here saves its left operand across evaluating the
// right one on the *real* STM8 hardware stack (`push x`/`pop x`),
// never in a named memory cell - same reasoning as
// I8080Codegen::genBinary's own header comment (a nested binary op
// might reuse the same named cell).
void STM8Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+") {
        genExpr(*e.lhs);
        code_ << "    ldw x, ac\n    push x\n";
        genExpr(*e.rhs);                     // ac = rhs
        code_ << "    ldw y, ac\n    pop x\n";  // y = rhs, x = lhs (popped only now)
        code_ << "    addw x, y\n    ldw ac, x\n";
        return;
    }
    if (op == "-") {
        genExpr(*e.lhs);
        code_ << "    ldw x, ac\n    push x\n";
        genExpr(*e.rhs);
        code_ << "    ldw y, ac\n    pop x\n";  // y = rhs, x = lhs
        code_ << "    subw x, y\n    ldw ac, x\n";
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
    if (op == "&" || op == "|" || op == "^") {
        genBitwise(op, *e.lhs, *e.rhs);
        return;
    }
    if (op == "<<" || op == ">>") {
        genShift(*e.lhs, *e.rhs, op == "<<");
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

// No register-register bitwise op on `x`/`y` (the real STM8's own
// `AND`/`OR`/`XOR` only ever touch `a` - see STM8Codegen.h's own
// header comment), so this goes byte-at-a-time through `a` instead,
// directly against each cell's own two named byte addresses
// (`bc`/`bc+1`, `ac`/`ac+1`) via this target's real direct-addressed
// ALU instructions - no transit register needed for the arithmetic
// itself, but `lhs` still has to survive evaluating `rhs` on the real
// hardware stack, not in `bc` directly (same reasoning as
// genBinary()'s own header comment: a nested `&`/`|`/`^` reuses `bc`
// too, so storing straight into it here, before `rhs` is evaluated,
// would get clobbered exactly the way a named cell always can).
void STM8Codegen::genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    pop x\n    ldw bc, x\n";  // bc = lhs (popped only now, after rhs is fully evaluated)
    const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
    code_ << "    ld a, bc\n    " << insn << " a, ac\n    ld ac, a\n";
    code_ << "    ld a, bc+1\n    " << insn << " a, ac+1\n    ld ac+1, a\n";
}

// No variable-count shift instruction - loop `rhs` times instead, one
// bit at a time, via shiftOneBit(). The loop count lives in `lc`, a
// plain memory byte, not `a` - `a` is still needed as scratch to move
// `lc` in and out of memory each iteration (the STM8 has no `dec`/
// `tnz` on an arbitrary memory byte directly the way `incw`/`tnzw`
// exist for a whole word), but it's never live *across* a genExpr()
// call the way a result-carrying cell would need to be.
void STM8Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);                          // ac = value to shift
    code_ << "    ldw x, ac\n    push x\n";  // saved on the real stack across evaluating the count
    genExpr(rhs);                           // ac = shift count
    code_ << "    ld a, ac+1\n    ld lc, a\n";  // lc = shift count (always small; low byte is enough)
    code_ << "    pop x\n    ldw bc, x\n";       // bc = value (restored only now, after the count is safely in lc)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    shiftOneBit("bc", isLeft);
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

// Textbook shift-and-add multiply (no hardware multiply wide enough
// for a 16x16 - see STM8Codegen.h's own header comment): `bc` is the
// multiplicand (doubled each iteration), `dx` the multiplier (halved
// each iteration, consumed one bit at a time), `ac` the accumulating
// product, `lc` the loop counter (see genShift()'s own comment on why
// not `a`). Only verified against non-negative operands. `lhs` first
// survives evaluating `rhs` on the real hardware stack, not by an
// early, direct `copy16("bc", ...)` - `rhs` may itself be, or
// contain, a call (e.g. `n * fact(n - 1)`), and that callee's own
// body is just as free to reuse `bc` for its own unrelated purposes
// as any other function - same reasoning as genBitwise()'s own
// header comment.
void STM8Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pop x\n    ldw bc, x\n";  // bc = multiplicand (popped only now, after rhs is fully evaluated)
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = product = 0
    code_ << "    ldi a, 16\n    ld lc, a\n";    // loop counter
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ld a, lc\n    tnza\n";
    code_ << "    jreq " << Ldone << "\n";
    code_ << "    ld a, dx+1\n    andi a, 1\n";
    code_ << "    jreq " << Lskipadd << "\n";
    code_ << "    ldw x, ac\n    ldw y, bc\n    addw x, y\n    ldw ac, x\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    ld a, lc\n    deca\n    ld lc, a\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide wide enough for a 16/16 -
// see STM8Codegen.h's own header comment): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative - via the real `CPW x,y` +
// `jrult`, no separate loop counter needed at all here. Only verified
// against non-negative operands. `lhs` survives evaluating `rhs` on
// the real hardware stack - see genMul()'s own identical comment on
// why an early `copy16("dx", ...)` isn't safe here either.
void STM8Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pop x\n    ldw dx, x\n";  // dx = dividend (popped only now, after rhs is fully evaluated)
    code_ << "    ldwi x, 0\n    ldw ac, x\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    ldw x, dx\n    ldw y, bc\n    cpw x, y\n";
    code_ << "    jrult " << Ldone << "\n";  // dx < bc - this round's subtraction can't happen, stop
    code_ << "    subw x, y\n    ldw dx, x\n";
    code_ << "    ldw x, ac\n    incw x\n    ldw ac, x\n";
    code_ << "    jp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void STM8Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: `CPW x,y` after loading
// both operands sets Z iff they're equal, full 16 bits at once.
// `lhs` survives evaluating `rhs` on the real hardware stack, not an
// early `copy16("bc", ...)` - see genMul()'s own identical comment
// on why (`rhs` may itself be, or contain, a call that reuses `bc`
// for its own unrelated purposes).
void STM8Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(rhs);  // ac = rhs
    code_ << "    ldw y, ac\n    pop x\n";
    code_ << "    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (wantEqual ? "jreq" : "jrne") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the standard bias trick (see
// I8080Codegen.h's own header comment on why: `CPW`'s own flags, like
// the 8080's status word, aren't trusted here for a signed compare):
// flipping just the top bit of each operand's high byte maps the
// signed range onto the unsigned one while preserving order, so a
// plain unsigned `CPW`+`jrult`/`jruge` is all that's left to do.
// `a` survives evaluating `b` on the real hardware stack, not an
// early `copy16("bc", ...)` - see genMul()'s own identical comment
// on why (`b` may itself be, or contain, a call that reuses `bc` for
// its own unrelated purposes).
void STM8Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    ldw x, ac\n    push x\n";
    genExpr(b);  // ac = b
    code_ << "    pop x\n    ldw bc, x\n";  // bc = a (popped only now, after b is fully evaluated)
    code_ << "    ld a, bc\n    xori a, 0x80\n    ld bc, a\n";  // bc's high byte, sign flipped
    code_ << "    ld a, ac\n    xori a, 0x80\n    ld ac, a\n";  // ac's high byte, sign flipped
    code_ << "    ldw x, bc\n    ldw y, ac\n    cpw x, y\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << (invert ? "jruge" : "jrult") << " " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jreq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jreq " << Lfalse << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jrne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    code_ << "    jrne " << Ltrue << "\n";
    code_ << Lfalse << ":\n    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

void STM8Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    ldw x, ac\n    tnzw x\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jreq " << Ltrue << "\n";
    code_ << "    ldwi x, 0\n    ldw ac, x\n    jp " << Ldone << "\n";
    code_ << Ltrue << ":\n    ldwi x, 1\n    ldw ac, x\n";
    code_ << Ldone << ":\n";
}

std::string STM8Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the stm8 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> STM8Codegen::assembleInProcess(const std::string &asmText,
                                                           const std::string &format) const {
    if (format == "raw") return assembleSTM8(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see STM8Assembler.h's own `assembleSTM8Object`), and whichever
    // of these names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it - this file never touches a real COFF/ELF/Mach-O/
    // a.out byte layout itself.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSTM8Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
