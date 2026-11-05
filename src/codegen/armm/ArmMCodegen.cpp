#include "ArmMCodegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "ArmMAssembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - the test
// harness sets the simulated CPU's PC here directly, same convention
// (and the same reason) as M68kCodegen.cpp's/MipsCodegen.cpp's own
// kBaseAddress.
constexpr uint32_t kBaseAddress = 0x1000u;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data.
constexpr uint32_t kInitialSP = 0x100000u;
}  // namespace

void ArmMCodegen::declareTemp(const std::string &lbl) { trailer_.push_back(lbl + ": dw 0"); }

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dw`'s own expression evaluator
// (ArmMAssembler.cpp's Line::Dw) already resolves a bare symbol the
// same way `loadAddress`'s own label operand already does for an
// address loaded into a register.
std::string ArmMCodegen::arrayElemText(const ir::ArrayInitElem &e, Type elemType) {
    if (!e.isSymbol) {
        // A `long long` literal element needs its own two "dw"s (low
        // word, high word) - handled by the caller, which never asks
        // for this text in that case; every narrower width still
        // fits one directive's own operand.
        (void)elemType;
        return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    }
    // A symbol element always means a plain native-int-width address
    // (the dispatch-table idiom - see AST.h's ArrayInitElem).
    return e.slot ? label(e.slot) : label(e.func);
}

void ArmMCodegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
    std::ostringstream line;
    line << lbl << ": " << directive << " ";
    bool first = true;
    for (const auto &e : values) {
        if (w == 8 && !e.isSymbol) {
            auto v = static_cast<uint64_t>(e.value);
            if (!first) line << ", ";
            line << static_cast<uint32_t>(v) << ", " << static_cast<uint32_t>(v >> 32);
            first = false;
            continue;
        }
        if (!first) line << ", ";
        line << arrayElemText(e, elemType);
        first = false;
    }
    trailer_.push_back(line.str());
}

void ArmMCodegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movw " << reg << "," << lbl << "\n";
    code_ << "    movt " << reg << "," << lbl << "\n";
}

void ArmMCodegen::loadImm32(const std::string &reg, uint32_t value) {
    if (value <= 0xFF) {
        code_ << "    movs " << reg << "," << static_cast<int32_t>(value) << "\n";
        return;
    }
    code_ << "    movw " << reg << "," << static_cast<int32_t>(value & 0xFFFFu) << "\n";
    code_ << "    movt " << reg << "," << static_cast<int32_t>((value >> 16) & 0xFFFFu) << "\n";
}

void ArmMCodegen::loadFrameByteAddr(const std::string &reg, int bytesFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (bytesFromFp == 0) return;
    loadImm32("r3", static_cast<uint32_t>(bytesFromFp > 0 ? bytesFromFp : -bytesFromFp));
    code_ << "    " << (bytesFromFp > 0 ? "adds" : "subs") << " " << reg << "," << reg << ",r3\n";
}

// A Param's own frameOffset already accounts for the real two-native-
// int-width reservation and each param's own (at-least-int-width)
// size - see IR.h's own comment on Slot::frameOffset - so it's used
// directly here, in bytes, the same as a Local's.
void ArmMCodegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int bytesFromFp = slot->kind == ir::SlotKind::Param ? slot->frameOffset : -slot->frameOffset;
    loadFrameByteAddr(reg, bytesFromFp);
}

void ArmMCodegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
    } else {
        loadFrameAddr(reg, slot);
    }
}

void ArmMCodegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        loadImm32(reg, static_cast<uint32_t>(e.value));
        return;
    }
    loadAddress(reg, e.slot ? label(e.slot) : label(e.func));
}

std::string ArmMCodegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - ArmMLinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see ArmMAssembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // `movw`/`movt` can't target `sp` directly (Rd==13 is
        // UNPREDICTABLE for the T32 "MOV (immediate)" encoding they
        // use - see ArmMAssembler.cpp's own encodeMovWT()), so the
        // initial stack pointer is built in a scratch register first
        // and moved into `sp` with the plain register-to-register
        // "MOV (register)" form instead, which has no such
        // restriction (it's the real, standard way every real Thumb
        // reset handler sets up its own initial `sp`).
        loadImm32("r0", kInitialSP);
        code_ << "    mov sp,r0\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt` -
        // see ArmMCodegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Running byte offset within the data segment, used only to pad
    // ahead of a global whose own type is wider than 1 byte - see
    // ArmCodegen.cpp's own identical comment on why an unaligned
    // `ldindh`/`ldind`/`stindh`/`stind` would otherwise silently
    // round down and clobber a neighboring global. A `long long`
    // only ever needs word (4-byte) alignment, same reasoning.
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        int align = std::min(widthOfType(slot->type), 4);
        size_t padding = (align - (dataOffset % align)) % align;
        if (padding > 0) {
            std::string fill = "0";
            for (size_t i = 1; i < padding; i++) fill += ", 0";
            trailer_.push_back("db " + fill);
            dataOffset += padding;
        }
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
            dataOffset += static_cast<size_t>(widthOfType(slot->type)) * static_cast<size_t>(slot->arraySize);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            int w = widthOfType(slot->type);
            if (w == 8) {
                auto uv = static_cast<uint64_t>(v);
                trailer_.push_back(lbl + ": dw " + std::to_string(static_cast<uint32_t>(uv)) + ", " +
                                    std::to_string(static_cast<uint32_t>(uv >> 32)) + "  ; " + slot->sourceName);
            } else {
                const char *directive = w == 1 ? "db" : w == 2 ? "dh" : "dw";
                trailer_.push_back(lbl + ": " + std::string(directive) + " " +
                                    std::to_string(static_cast<int32_t>(static_cast<uint32_t>(v))) + "  ; " +
                                    slot->sourceName);
            }
            dataOffset += static_cast<size_t>(w);
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only - see M68kCodegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> ArmMCodegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `lr` around its own body -
// same reasoning (and the same risk: exactly one link register) as
// MipsCodegen.cpp's own comment, just one real instruction on either
// side here (`pushlr`/`poppc`) instead of MIPS's two - already
// reentrant on its own, since `pushlr`/`poppc` are ordinary real
// `sp`-relative push/pop. `push r7`/`mov r7,sp` on top of that gives
// this call its own frame pointer (see this file's own header
// comment), and one dummy `push r0` per word of `frameSize` reserves
// this call's own fresh Local storage - the pushed value is never
// read, only the resulting `sp` displacement matters, so there's no
// need for this dialect's assembler to grow a `sub sp,#imm`
// instruction of its own just for this. emitEpilogue() undoes exactly
// this, in reverse, at every `return` and once more here as the
// fall-off-the-end default.
void ArmMCodegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    code_ << "    push r7\n";
    code_ << "    mov r7,sp\n";
    // `fn.func->frameSize` is now a real byte count (see IR.h's own
    // comment on Function::frameSize) - round up to whole words
    // since each `push` here reserves exactly one.
    frameSize_ = (fn.func->frameSize + 3) / 4;
    for (int i = 0; i < frameSize_; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();
    code_ << "\n";
}

void ArmMCodegen::emitEpilogue() {
    for (int i = 0; i < frameSize_; i++) code_ << "    pop r1\n";
    code_ << "    pop r7\n";
    code_ << "    poppc\n";
}

void ArmMCodegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmp r0,0\n";
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    beq " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    b " << Ldone << "\n";
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
            genExpr(*s.cond);
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
            code_ << "    cmp r0,0\n    beq " << Lend << "\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    b " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    b " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    b " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit - genFunction()'s own fall-off-the-end
            // epilogue never runs for this path, so it needs this
            // same frame teardown inline.
            emitEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadFrameAddr("r2", s.declSlot);
                storeOperand("r2", s.declSlot->type);
            }
            // No "else" case for a bare, uninitialized scalar: its
            // own frame word already exists (genFunction()'s own
            // reserve-pushes already made room for it) - same as a
            // real, ordinary C automatic variable, its value is
            // simply whatever was already in that stack slot until
            // something assigns it.
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

void ArmMCodegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                loadImm32("r0", static_cast<uint32_t>(v));
                loadImm32("r1", static_cast<uint32_t>(v >> 32));
                return;
            }
            loadImm32("r0", static_cast<uint32_t>(e.intValue));
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("r2", e.slot);
            loadOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // r2 = target address
                code_ << "    push r2\n";
                genExpr(*e.rhs);
                code_ << "    pop r2\n";
                storeOperand("r2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            storeOperand("r2", e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // r0 = pointer value (deref is untyped - always native-word)
            code_ << "    push r0\n";
            genExpr(*e.rhs);
            code_ << "    pop r2\n";
            code_ << "    stind r2,r0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (isWide(e.rhs->type)) {
                    genNegate64();
                } else {
                    code_ << "    neg r0,r0\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value (deref is untyped - always native-word)
                code_ << "    mov r2,r0\n";
                code_ << "    ldind r2,r0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn r0,r0\n";
                if (isWide(e.rhs->type)) code_ << "    mvn r1,r1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed onto the real stack, last-declared
            // one first, so the first argument ends up closest to the
            // return address `bl` is about to set `lr` for - matching
            // the address every Param already addresses itself by
            // (see this file's own header comment: paramIndex 0 is
            // `r7+8`, the lowest positive offset). Each callee lays
            // them into ITS OWN frame itself; nothing here needs to
            // know anything about the callee's own parameter count or
            // names beyond `e.args.size()`. The caller cleans the
            // pushed words back off afterward with one dummy `pop r1`
            // per pushed word (no `add sp,#imm` instruction exists in
            // this dialect - see genFunction()'s own identical
            // reasoning for the Local-frame reserve/release). A wide
            // (`long long`) argument pushes its high word first so
            // its low word ends up closer to `r7`, matching a wide
            // Param's own frameOffset/frameOffset+4 layout.
            int pushedWords = 0;
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    push r1\n    push r0\n";
                    pushedWords += 2;
                } else {
                    code_ << "    push r0\n";
                    pushedWords += 1;
                }
            }
            code_ << "    bl " << label(e.func) << "\n";
            for (int i = 0; i < pushedWords; i++) code_ << "    pop r1\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // `blx r2` is this dialect's own real Thumb-1 "BLX
            // (register)" encoding (see ArmMAssembler.cpp's own
            // encodeBlx()) - a real near call through whatever address
            // r2 holds, setting `lr` to the return address exactly
            // like `bl` does. r2's own bit 0 must be 1 (the real
            // ARMv7-M interworking rule `BLX (register)` enforces - a
            // UsageFault otherwise), which it already is here: `e.lhs`
            // only ever evaluates to a value that itself traces back
            // to genAddrOf()'s own `ExprKind::Func` case (there's
            // nothing else in this subset that's callable at all),
            // which already set that bit - see its own comment.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    mov r2,r0\n";
            code_ << "    blx r2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal Cortex-M with no NVIC/exception setup done
            // at all here - treated as a no-op, same reasoning (and
            // the same choice) as M68kCodegen.cpp's/MipsCodegen.cpp's
            // own Intrinsic case.
            (void)e;
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

// `r2` holds the computed address, no zero-page-style placement
// restriction the way the 6502 backend's own `ptr` has. `arraySize >
// 0` means the base is the array's own first word - a compile-time-
// known label for a Global, or (see loadFrameAddr()) a runtime
// r7-relative address for a Local/Param; otherwise `slot` is a plain
// pointer variable, so the base instead comes from *reading* that
// variable's own stored value first (again from a label or an r7-
// relative address, depending on `slot->kind`). A Local/Param array's
// own element i sits *below* element 0 (addresses decrease as the
// index increases - see this file's own header comment), the mirror
// image of a Global array's increasing-address layout, so this is
// base-minus-scaled-index there, not base-plus.
void ArmMCodegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) code_ << "    lsli r0,r0," << shift << "\n";  // r0 *= the element type's own width
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            loadAddress("r2", label(slot));
            code_ << "    adds r2,r2,r0\n";
        } else {
            loadFrameAddr("r2", slot);
            code_ << "    subs r2,r2,r0\n";
        }
        return;
    }
    loadSlotAddr("r1", slot);
    code_ << "    ldind r1,r1\n";  // r1 = the pointer variable's own stored value
    code_ << "    adds r2,r1,r0\n";
}

void ArmMCodegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("r2", e.slot->type);
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a
// Local's storage is this activation's own fresh frame region, not
// pre-set to anything; arrayInit is always fully populated up to
// arraySize, so this always leaves the whole array in a known state,
// same as a Global array's static trailer always did. An element's
// own address is slot->frameOffset+i*widthOfType(slot->type) bytes
// below r7, the same arithmetic loadFrameAddr() uses for the array's
// own first word, just advanced by i element widths.
void ArmMCodegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        loadArrayInitElem("r0", elem);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            loadImm32("r1", static_cast<uint32_t>(v >> 32));
        }
        loadFrameByteAddr("r2", -(slot->frameOffset + w * static_cast<int>(i)));
        storeOperand("r2", slot->type);
    }
}

void ArmMCodegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    // Re-truncate+re-extend by shifting the unwanted high bits out
    // and back in - zero-fill (`lsr`) for unsigned, sign-fill
    // (`asr`) for signed - this dialect's own version of
    // ArmCodegen.cpp's identical idiom (no dedicated sign/zero-
    // extend instruction here either).
    if (w == 1) {
        code_ << "    lsli r0,r0,24\n";
        code_ << "    movs r1,24\n    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r1\n";
    } else if (w == 2) {
        code_ << "    lsli r0,r0,16\n";
        code_ << "    movs r1,16\n    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r1\n";
    }
}

// `addrReg` holds the already-computed effective address (r2, by
// this backend's own convention - see loadSlotAddr()/genIndexAddr());
// loads into r0 (or r0:r1, for a `long long`), sign- or zero-
// extending a narrower-than-native-int value per its own
// signedness. `ldindb`/`ldindh` always zero-extend (see
// ArmMAssembler.cpp's own comment) - a signed source needs an
// additional re-sign-extend via narrowR0To().
void ArmMCodegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldindb " << addrReg << ",r0\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 2) {
        code_ << "    ldindh " << addrReg << ",r0\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",r0\n";
    } else {
        // `subs` has no immediate form in this dialect (see
        // ArmMAssembler.cpp's own comment) - the high word's own
        // address is computed into the scratch register r6 instead
        // of mutating `addrReg` itself and subtracting back.
        code_ << "    ldind " << addrReg << ",r0\n";
        code_ << "    adds r6," << addrReg << ",4\n";
        code_ << "    ldind r6,r1\n";
    }
}

void ArmMCodegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",r0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",r0\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << ",r0\n";
    } else {
        code_ << "    stind " << addrReg << ",r0\n";
        code_ << "    adds r6," << addrReg << ",4\n";
        code_ << "    stind r6,r1\n";
    }
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is crossing the native-int/`long
// long` boundary, or re-truncating+re-extending r0 for a narrower
// target - see narrowR0To().
void ArmMCodegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    movs r1,0\n";
        } else {
            code_ << "    movs r1,r0\n    movs r4,31\n    asr r1,r4\n";
        }
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

void ArmMCodegen::genNegate64() {
    // Two's complement negate of r0:r1 in place.
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    movs r4,1\n    adds r0,r0,r4\n    movs r4,0\n    adc r1,r4\n";
}

// `long long` add/sub (lhs OP rhs) - evaluate lhs into r0:r1,
// stashing its two words on the stack while rhs evaluates into
// r4:r5, then ripple the carry/borrow across the word pair with
// `adc`/`sbc`.
void ArmMCodegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4=rhs_lo, r5=rhs_hi
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 = lhs restored
    if (isAdd) {
        code_ << "    adds r0,r0,r4\n    adc r1,r5\n";
    } else {
        code_ << "    subs r0,r0,r4\n    sbc r1,r5\n";
    }
}

// 32x32->64 unsigned widening multiply: {r1:r0} = r6(a) * r4(b),
// both consumed - the caller must place `a` in r6 and `b` in r4
// before calling (never as a parameter string, to sidestep the
// aliasing hazard of this function's own internal register choice
// overwriting a not-yet-read operand - see genWideMul()'s own
// comment on why it stages operands this way). Bit-serial shift-
// and-add (32 iterations), since this dialect has no real `UMULL`:
// `a` is consumed 1 bit at a time (shifted right, via the register-
// form `lsr` - `r3` always holds the fixed shift amount 1); `b`'s
// own current doubled value (r4:r5, growing to 64 bits as the loop
// progresses, via the `lsl`+`adc` carry-chained doubling trick also
// used by genWideDivMod()/genWideShift()) is added into the 64-bit
// accumulator (r0:r1) whenever the consumed bit of `a` is 1.
// Clobbers r0-r6.
void ArmMCodegen::genMul64() {
    code_ << "    movs r5,0\n";  // r5 = b's own high word (b, r4, is only ever 32 bits wide)
    code_ << "    movs r0,0\n    movs r1,0\n";
    code_ << "    movs r2,0\n";  // r2 = iteration count-up (0..32)
    code_ << "    movs r3,1\n";  // r3 = the fixed amount (1) every shift/increment below uses
    std::string Lloop = newLabel(), LdoAdd = newLabel(), LskipAdd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    cmp r2,32\n    beq " << Ldone << "\n";
    code_ << "    adds r2,r2,r3\n";
    // Consume `a`'s own low bit by shifting it out - the carry flag
    // this leaves behind (`lsr`'s own last-bit-shifted-out
    // convention) IS that bit, tested directly via `bhs`/`blo`
    // (literally the carry-set/carry-clear condition codes - see
    // ArmMAssembler.cpp's own `condBranch` table) with no separate
    // AND/compare needed.
    code_ << "    lsr r6,r3\n    bhs " << LdoAdd << "\n    b " << LskipAdd << "\n";
    code_ << LdoAdd << ":\n";
    code_ << "    adds r0,r0,r4\n    adc r1,r5\n";
    code_ << LskipAdd << ":\n";
    // Double `b` (r4:r5) by 1 bit for the next iteration - `adds
    // r4,r4,r4` is a self-add, doubling r4 while leaving its own old
    // top bit in the carry flag for `adc r5,r5` to fold in (same
    // carry-chained-doubling trick genWideDivMod()/genWideShift()
    // also use).
    code_ << "    adds r4,r4,r4\n    adc r5,r5\n";
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// `long long` comparison - compare the high words first (per
// `isUnsigned`, since that's where the sign bit lives); only if
// those are equal does the low word's own comparison matter, and at
// that point it's always unsigned regardless of the overall type's
// signedness.
void ArmMCodegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4:r5 = rhs
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = lhs
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (op == "==" || op == "!=") {
        const char *onEqual = op == "==" ? "1" : "0";
        const char *onDiffer = op == "==" ? "0" : "1";
        code_ << "    cmp r0,r4\n    bne " << Lfalse << "\n";
        code_ << "    cmp r1,r5\n    bne " << Lfalse << "\n";
        code_ << "    movs r0," << onEqual << "\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    movs r0," << onDiffer << "\n";
        code_ << Ldone << ":\n";
        return;
    }
    std::string signedCC = op == "<" ? "blt" : op == "<=" ? "ble" : op == ">" ? "bgt" : "bge";
    std::string unsignedCC = op == "<" ? "blo" : op == "<=" ? "bls" : op == ">" ? "bhi" : "bhs";
    std::string hiCC = isUnsigned ? unsignedCC : signedCC;
    std::string LhiDiff = newLabel();
    code_ << "    cmp r1,r5\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r0,r4\n    " << unsignedCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << LhiDiff << ":\n    " << hiCC << " " << Ltrue << "\n    b " << Lfalse << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit
// product: result_lo:result_hi = lo(lhs)*lo(rhs) (the full 64-bit
// product, via genMul64()) plus the two cross terms
// lhs_hi*rhs_lo/lhs_lo*rhs_hi folded into the high word only (each
// one's own overflow past bit 64 would only ever affect bit 64 and
// beyond, which a 64-bit result doesn't keep anyway - truncating
// `muls` is exactly right for them, same reasoning as
// ArmCodegen.cpp's own `genWideMul()`).
void ArmMCodegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stash lhs: top->bottom = lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r6,r0\n    mov r5,r1\n";  // r6=rhs_lo, r5=rhs_hi
    code_ << "    pop r4\n    pop r2\n";        // r4=lhs_lo, r2=lhs_hi
    // Cross terms, computed now (before genMul64() clobbers r4/r6):
    // r3 = lhs_hi*rhs_lo, then r2 = rhs_hi*lhs_lo, summed into r2 -
    // r4 (lhs_lo) and r6 (rhs_lo) are left untouched for genMul64().
    code_ << "    mov r3,r2\n    muls r3,r6\n";  // r3 = lhs_hi*rhs_lo
    code_ << "    mov r2,r5\n    muls r2,r4\n";  // r2 = rhs_hi*lhs_lo
    code_ << "    adds r2,r2,r3\n";
    code_ << "    push r2\n";  // stash the cross-term sum across genMul64()'s own full clobber
    genMul64();                 // {r1:r0} = r6(rhs_lo) * r4(lhs_lo), the full 64-bit low*low product
    code_ << "    pop r2\n";
    code_ << "    adds r1,r1,r2\n";
}

// A `long long` shifted by a runtime count - no instruction here
// shifts more than 32 bits at once, so this is a counted loop of
// 1-bit shifts (r6 = count). `<<` doubles r0:r1 via the self-add
// carry-chained-doubling trick (same as genMul64()'s own `b`
// doubling); `>>` needs the opposite direction, which this dialect's
// register-form `lsr`/`asr` can do directly but without a carry-
// chain analogue of `adc` for the *downward* direction, so the bit
// crossing the r1/r0 boundary is captured by hand into r4 (the
// high word's own old bit 0) before shifting, then OR'd into r0's
// own new top bit via a precomputed 0x80000000 mask (r3, set up
// once before the loop with `lsli`) - `isUnsigned` only matters
// here, picking `asr` (sign-preserving) or `lsr` (zero-filling) for
// r1's own shift.
void ArmMCodegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);  // r0 = shift count (already promoted to a plain int)
    code_ << "    mov r6,r0\n";
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 restored = the value being shifted
    code_ << "    movs r5,1\n";            // r5 = the fixed amount (1) every shift/decrement below uses
    std::string Lloop = newLabel(), Ldone = newLabel();
    if (!isLeft) code_ << "    movs r3,1\n    lsli r3,r3,31\n";  // r3 = 0x80000000, used only by the '>>' path
    code_ << Lloop << ":\n";
    code_ << "    cmp r6,0\n    beq " << Ldone << "\n    subs r6,r6,r5\n";
    if (isLeft) {
        code_ << "    adds r0,r0,r0\n    adc r1,r1\n";
    } else {
        std::string LskipOr = newLabel();
        code_ << "    mov r4,r1\n    and r4,r5\n";  // r4 = r1's own old bit 0 (the bit crossing into r0)
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " r1,r5\n";
        code_ << "    lsr r0,r5\n";
        code_ << "    cmp r4,0\n    beq " << LskipOr << "\n";
        code_ << "    orr r0,r3\n";
        code_ << LskipOr << ":\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Classic bit-serial shift/compare/subtract long division (no
// instruction here is wide enough for a genuine 64/64 divide) -
// same overall shape as ArmCodegen.cpp's own `genWideDivMod()`, just
// adapted to this dialect's much smaller register file (no spare
// register for a separate quotient accumulator) via the standard
// restoring-division trick of building the quotient into the bits
// vacated by the dividend as it shifts left: r0:r1 start as the
// dividend and finish as the quotient; r4:r5 is the remainder
// (starts 0); r2:r3 is the divisor. `isUnsigned` false divides
// *magnitudes* (each operand's own sign bit, read before negating,
// is stashed on the stack across the whole loop and restored on the
// result afterward per C's truncating-division rule).
void ArmMCodegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r2,r0\n    mov r3,r1\n";  // r2:r3 = divisor
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = dividend

    if (!isUnsigned) {
        code_ << "    movs r4,31\n";
        code_ << "    mov r5,r1\n    lsr r5,r4\n";  // r5 = dividend's own sign (0/1)
        code_ << "    mov r6,r3\n    lsr r6,r4\n";  // r6 = divisor's own sign (0/1)
        code_ << "    push r5\n    push r6\n";       // stack (top->bottom): divisorSign, dividendSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    cmp r5,0\n    beq " << LskipNegN << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    adds r0,r0,1\n    movs r4,0\n    adc r1,r4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    cmp r6,0\n    beq " << LskipNegD << "\n";
        code_ << "    mvn r2,r2\n    mvn r3,r3\n    adds r2,r2,1\n    movs r4,0\n    adc r3,r4\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    movs r4,0\n    movs r5,0\n";  // remainder = 0
    code_ << "    movs r6,0\n";                  // iteration count-up (0..64)

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel();
    code_ << Lloop << ":\n    cmp r6,64\n    beq " << Ldone << "\n    adds r6,r6,1\n";

    // Shift {remainder:dividend} left by 1 as one combined 128-bit
    // value, via the carry-chained-doubling trick (self-add on r0,
    // `adc` on r1/r4/r5 in sequence) - the bit that overflows out of
    // the dividend half (r1's own old top bit) lands, via the
    // carry chain, directly into the remainder's own new bit 0.
    code_ << "    adds r0,r0,r0\n    adc r1,r1\n    adc r4,r4\n    adc r5,r5\n";

    // if (remainder >= divisor) { remainder -= divisor; quotient bit = 1 }
    // (always an unsigned magnitude compare - both operands were
    // already negated to non-negative above for the signed case).
    code_ << "    cmp r5,r3\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r4,r2\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bhi " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    subs r4,r4,r2\n    sbc r5,r3\n";
    code_ << "    adds r0,r0,1\n";  // set the quotient bit the shift above just vacated at r0's own bit 0
    code_ << LsubNo << ":\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    pop r6\n";  // r6 = divisorSign
        code_ << "    pop r2\n";  // r2 = dividendSign
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov r3,r6\n    eor r3,r2\n    cmp r3,0\n    beq " << LskipNegQ << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    adds r0,r0,1\n    movs r3,0\n    adc r1,r3\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    cmp r2,0\n    beq " << LskipNegR << "\n";
        code_ << "    mvn r4,r4\n    mvn r5,r5\n    adds r4,r4,1\n    movs r3,0\n    adc r5,r3\n";
        code_ << LskipNegR << ":\n";
    }
    if (wantRemainder) {
        code_ << "    mov r0,r4\n    mov r1,r5\n";
    }
}

void ArmMCodegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("r2", operand.slot);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov r0,r2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        // The real ARMv7-M function-pointer value, not a bare code
        // address: `fn_<name>` itself is always even (every Thumb
        // instruction, hence every function entry point, is 2-byte
        // aligned), but `BX`/`BLX (register)` - the only way this
        // address is ever actually branched to (see this file's own
        // `ExprKind::CallPtr` case) - loads the target register's own
        // bit 0 straight into `EPSR.T` and raises a UsageFault if it
        // isn't 1; M-profile has no ARM (A32) state to switch to at
        // all, Thumb is the only state, so this bit must always be
        // set on a value headed for one of those instructions. Adding
        // 1 is exactly OR-ing in that bit, since the address itself is
        // always even.
        loadAddress("r2", label(operand.func));
        code_ << "    adds r2,r2,1\n";
        code_ << "    mov r0,r2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void ArmMCodegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h), except shift (only the LHS's own
    // type matters) and &&/|| (width-independent, handled below).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+");
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    adds r0,r1,r0\n";  // r0 = lhs + rhs
        } else {
            code_ << "    subs r0,r1,r0\n";  // r0 = lhs - rhs
        }
        return;
    }
    if (op == "*") {
        if (wide) {
            genWideMul(*e.lhs, *e.rhs);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "orr" : "eor";
        if (wide) {
            genExpr(*e.lhs);
            code_ << "    push r1\n    push r0\n";
            genExpr(*e.rhs);
            code_ << "    mov r4,r0\n    mov r5,r1\n";
            code_ << "    pop r0\n    pop r1\n";  // r0:r1 = lhs
            code_ << "    " << insn << " r0,r4\n    " << insn << " r1,r5\n";
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        code_ << "    " << insn << " r0,r1\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
            return;
        }
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);               // r0 = shift count
        code_ << "    mov r1,r0\n";    // r1 = shift count
        code_ << "    pop r0\n";       // r0 = value to shift
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r1\n";
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

// Real hardware multiply (`MULS`) - a full 32x32->32 product, no
// operand-size limitation the way the 68000's own `MULS.W` has.
void ArmMCodegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    muls r0,r1\n";
}

// Software restoring-by-repeated-subtraction divide/remainder - see
// ArmMCodegen.h's own header comment on why there's no hardware fast
// path. The unsigned path is a plain repeated-subtraction loop on
// the operands as-is; the signed path works on absolute values
// first, then reapplies the sign C's own truncating-toward-zero
// division needs: the quotient is negative iff the operands' signs
// differ, and the remainder always takes the dividend's own sign.
void ArmMCodegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs (dividend), r0 = rhs (divisor)

    std::string Lloop = newLabel();
    std::string LloopDone = newLabel();

    if (isUnsigned) {
        // r1 = remainder (starts as the dividend), r2 = quotient
        // (starts 0), r0 = divisor. Repeated subtraction: while
        // remainder>=divisor (unsigned), remainder-=divisor,
        // quotient+=1.
        code_ << "    movs r2,0\n";
        code_ << Lloop << ":\n";
        code_ << "    cmp r1,r0\n    blo " << LloopDone << "\n";
        code_ << "    subs r1,r1,r0\n";
        code_ << "    adds r2,r2,1\n";
        code_ << "    b " << Lloop << "\n";
        code_ << LloopDone << ":\n";
        code_ << "    mov r0," << (wantRemainder ? "r1" : "r2") << "\n";
        return;
    }

    std::string LlhsAbsDone = newLabel();
    std::string LrhsAbsDone = newLabel();
    std::string LnegateDone = newLabel();

    // r3 = 1 if the result (whichever one wantRemainder picks) needs
    // negating, else 0 - computed from the *original* signs before
    // either operand is overwritten by its own absolute value.
    if (wantRemainder) {
        code_ << "    movs r3,0\n";
        code_ << "    cmp r1,0\n    bge " << LlhsAbsDone << "\n";
        code_ << "    movs r3,1\n";
        code_ << LlhsAbsDone << ":\n";
    } else {
        // r3 = (lhs<0) XOR (rhs<0), built from two independent sign
        // tests (no real XOR-of-booleans instruction needed: the
        // second test just flips r3 via a second small branch).
        code_ << "    movs r3,0\n";
        code_ << "    cmp r1,0\n    bge " << LlhsAbsDone << "\n";
        code_ << "    movs r3,1\n";
        code_ << LlhsAbsDone << ":\n";
        code_ << "    cmp r0,0\n    bge " << LrhsAbsDone << "\n";
        code_ << "    movs r2,1\n";
        code_ << "    eor r3,r2\n";
        code_ << LrhsAbsDone << ":\n";
    }

    // Take absolute values of the dividend/divisor themselves - sign
    // already captured above.
    std::string LabsLhsDone = newLabel();
    code_ << "    cmp r1,0\n    bge " << LabsLhsDone << "\n";
    code_ << "    neg r1,r1\n";
    code_ << LabsLhsDone << ":\n";

    std::string LabsRhsDone = newLabel();
    code_ << "    cmp r0,0\n    bge " << LabsRhsDone << "\n";
    code_ << "    neg r0,r0\n";
    code_ << LabsRhsDone << ":\n";

    // r1 = |remainder| (starts as |lhs|), r2 = quotient (starts 0),
    // r0 = |divisor|. Repeated subtraction: while remainder>=divisor,
    // remainder-=divisor, quotient+=1.
    code_ << "    movs r2,0\n";
    code_ << Lloop << ":\n";
    code_ << "    cmp r1,r0\n    blt " << LloopDone << "\n";
    code_ << "    subs r1,r1,r0\n";
    code_ << "    adds r2,r2,1\n";
    code_ << "    b " << Lloop << "\n";
    code_ << LloopDone << ":\n";

    code_ << "    mov r0," << (wantRemainder ? "r1" : "r2") << "\n";
    code_ << "    cmp r3,0\n    beq " << LnegateDone << "\n";
    code_ << "    neg r0,r0\n";
    code_ << LnegateDone << ":\n";
}

// `CMP` sets the real condition codes for `Rn - Rm` directly in
// hardware (same as 68000's own `CMP.L` - see M68kCodegen.h's own
// header comment) - `isUnsigned` picks the unsigned branch mnemonic
// (`blo`/`bls`/`bhi`/`bhs` - see ArmMAssembler.cpp's own
// `condBranch` table) instead of the signed one for the ordered
// operators, no sign/overflow fixup needed either way.
void ArmMCodegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";      // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";   // flags = r1 - r0 = lhs - rhs
    std::string branchIfTrue;
    if (op == "==") branchIfTrue = "beq";
    else if (op == "!=") branchIfTrue = "bne";
    else if (op == "<") branchIfTrue = isUnsigned ? "blo" : "blt";
    else if (op == "<=") branchIfTrue = isUnsigned ? "bls" : "ble";
    else if (op == ">") branchIfTrue = isUnsigned ? "bhi" : "bgt";
    else if (op == ">=") branchIfTrue = isUnsigned ? "bhs" : "bge";
    else err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

// A wide (`long long`) operand's truthiness needs both words - `orr
// r0,r1` folds them together before the usual "cmp r0,0" (the
// combined value is discarded immediately after, so clobbering r0
// this way is safe).
void ArmMCodegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmp r0,0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmp r0,0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movs r0,0\n";
    code_ << Ldone << ":\n";
}

void ArmMCodegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmp r0,0\n    beq " << Ltrue << "\n";
    code_ << "    movs r0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movs r0,1\n";
    code_ << Ldone << ":\n";
}

std::string ArmMCodegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                               const std::string &) const {
    throw std::runtime_error("the cortex-m3 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> ArmMCodegen::assembleInProcess(const std::string &asmText,
                                                            const std::string &format) const {
    if (format == "raw") return assembleArmM(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see ArmMAssembler.h's own `assembleArmMObject`), and whichever
    // of these five names was asked for just picks that format's own
    // FileFormatWriter (see fileformat/FileFormat.h) to
    // serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArmMObject(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
