#include "STM32Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "STM32Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Real STM32 flash: this is where the reset vector table, every
// instruction, and nothing else lives - confirmed read-only against
// qemu's own `stm32vldiscovery` board model (see STM32Codegen.h's own
// header comment). `-kernel` loads this backend's own `--format=raw`
// output straight here.
constexpr uint32_t kFlashBase = 0x08000000;

// Real STM32 SRAM on this board (8KB, `0x20000000`-`0x20001FFF` - see
// tests/run_tests.sh's own check_stm32() comment on how this was
// confirmed): every global/local/temp/array slot lives here instead
// (see STM32Codegen.h's own header comment on why), reserved via
// `ds`, never given image bytes.
constexpr uint32_t kRamBase = 0x20000000;

// Top of that same SRAM - where `sp` starts out, loaded by the real
// Cortex-M reset exception from the vector table's own first word
// (confirmed directly: booting a hand-assembled image with this
// exact value at vector 0 leaves `sp` reading it back unchanged - no
// redundant explicit reload needed here the way MC68328Codegen.cpp's
// own comment says its own simulator board needs).
constexpr uint32_t kInitialSP = 0x20002000;
}  // namespace

void STM32Codegen::declareTemp(const std::string &lbl) { ramTrailer_.push_back(lbl + ": ds 1"); }

void STM32Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    ramTrailer_.push_back(lbl + ": ds " + std::to_string(values.size()));
}

// A literal element's own value via `movl`; a symbol element's
// address via the same `lea` genAddrOf() uses for &slot, with that
// same function's own thumb-bit adjustment (`+1`, for `blx` - see
// genAddrOf()'s own comment) when the element names a Function
// rather than a Slot. Either way the loaded value then gets copied
// into the RAM word `lbl+offset` resolves to - see genGlobalInit()'s
// own comment on why RAM needs this explicit one-time copy at all.
void STM32Codegen::genArrayElemInit(const std::string &lbl, size_t offset, const ir::ArrayInitElem &e) {
    if (!e.isSymbol && e.value == 0) return;  // RAM already starts zeroed - nothing to copy
    if (e.isSymbol) {
        code_ << "    lea " << (e.slot ? label(e.slot) : label(e.func)) << ",r0\n";
        if (!e.slot) {
            code_ << "    movl 1,r1\n";
            code_ << "    add r0,r0,r1\n";
        }
    } else {
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.value)) << ",r0\n";
    }
    code_ << "    lea " << lbl << "+" << offset << ",r2\n";
    code_ << "    str r0,[r2]\n";
}

// Flash-resident code that writes `value` into the RAM word `lbl`
// resolves to - see STM32Codegen.h's own header comment on why a
// slot's own real initial value can't just sit there in the image
// the way every other backend's flat-blob data does: RAM starts out
// with whatever qemu/real silicon happens to leave it as, so a
// nonzero initializer needs an explicit one-time copy, emitted as
// ordinary code into whichever stream the caller is building (this
// backend's own `_start`, for a global; right where a `LocalDecl`
// executes, for a local - see genStmt()'s own `LocalDecl` case).
void STM32Codegen::genGlobalInit(const std::string &ramLabel, long value) {
    if (value == 0) return;  // RAM already starts zeroed - nothing to copy
    code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(value)) << ",r0\n";
    code_ << "    lea " << ramLabel << ",r2\n";
    code_ << "    str r0,[r2]\n";
}

void STM32Codegen::loadFrameWordAddr(const std::string &reg, int wordsFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (wordsFromFp == 0) return;
    code_ << "    movl " << (wordsFromFp > 0 ? wordsFromFp : -wordsFromFp) * 4 << ",r3\n";
    code_ << "    " << (wordsFromFp > 0 ? "add" : "sub") << " " << reg << "," << reg << ",r3\n";
}

void STM32Codegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int wordsFromFp = slot->kind == ir::SlotKind::Param ? (2 + slot->paramIndex) : -slot->frameOffset;
    loadFrameWordAddr(reg, wordsFromFp);
}

void STM32Codegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        code_ << "    lea " << label(slot) << "," << reg << "\n";
    } else {
        loadFrameAddr(reg, slot);
    }
}

void STM32Codegen::loadArrayInitElem(const std::string &reg, const ir::ArrayInitElem &e) {
    if (!e.isSymbol) {
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.value)) << "," << reg << "\n";
        return;
    }
    code_ << "    lea " << (e.slot ? label(e.slot) : label(e.func)) << "," << reg << "\n";
    if (!e.slot) {
        code_ << "    movl 1,r1\n";
        code_ << "    add " << reg << "," << reg << ",r1\n";
    }
}

std::string STM32Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's own
    // `allowExternal`) means `prog` may not define `main` at all.
    // `objectMode` (see Codegen.h) means no object file ever emits
    // the vector table/reset-handler/`_start` sequence itself -
    // STM32LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `fn_main`'s linked
    // address is resolvable (see Linker.h). An object file's own
    // flash section is `org 0` - it's the linker's job to place it,
    // not this backend's (see STM32Assembler.h's own header comment).
    //
    // RAM, unlike flash, keeps using its real fixed hardware base
    // (`kRamBase`) unchanged even here: RAM is a genuinely separate
    // address space whose placement has nothing to do with where this
    // object's own code ends up in flash once linked, so (unlike a
    // flash address) a RAM address resolves to its final value right
    // here, at assembly time, never needing a linker-time base-shift
    // relocation at all (see STM32Assembler.cpp's own `isRamLabel_`
    // comment).
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // own `isExternal`/IRBuilder.cpp's `resolveExternalVar`) is
        // defined in some *other* file, which is the one that
        // reserves its real RAM storage - this file only ever
        // references it by name (see label()), letting
        // STM32Assembler.cpp's own undefined-symbol handling defer it
        // to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
            // The startup copy below writes each nonzero element into
            // RAM before `main` runs - see genGlobalInit()'s own
            // comment on why RAM needs this at all. It's emitted as
            // ordinary flash-resident code spliced into `_start`
            // (never into this file's own functions), so in object
            // mode - where this file never emits a `_start` of its
            // own (see this function's own comment above) - it's
            // skipped entirely: a nonzero RAM-resident global
            // initializer only gets its copy from the single-file
            // `--format=raw` entry sequence, not yet from a real
            // multi-object `--link`.
            if (!objectMode) {
                for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                    genArrayElemInit(lbl, i * 4, slot->arrayInit[i]);
                }
            }
        } else {
            declareTemp(lbl);
            if (!objectMode) genGlobalInit(lbl, slot->hasInit ? slot->initValue : 0);
        }
    }
    std::string initCode = code_.str();
    code_.str("");

    code_ << "org " << (objectMode ? 0 : kFlashBase) << "\n";
    if (!objectMode) {
        code_ << "dl " << kInitialSP << "\n";  // vector[0]: initial sp
        code_ << "dlt _start\n";               // vector[1]: reset handler, thumb bit forced on
        code_ << "_start:\n";
        code_ << initCode;
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in r0 the moment execution reaches `halt` -
        // see STM32Codegen.h's own header comment on why that's the
        // authoritative place to read it from (a real, directly
        // inspectable 32-bit register).
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    code_ << "\nramorg " << kRamBase << "\n";
    for (const auto &line : ramTrailer_) code_ << line << "\n";

    return code_.str();
}

// Single-file-only - see M6502Codegen.cpp's own version of this same
// comment (no `--format=coff` here either).
std::map<std::string, std::string> STM32Codegen::perFileAsm() const { return perFileAsm_; }

void STM32Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    pushlr\n";
    code_ << "    push r7\n";
    code_ << "    mov r7,sp\n";
    frameSize_ = fn.func->frameSize;
    for (int i = 0; i < frameSize_; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();  // reached only if control falls off the end with no explicit `return`
    code_ << "\n";
}

void STM32Codegen::emitEpilogue() {
    for (int i = 0; i < frameSize_; i++) code_ << "    pop r1\n";
    code_ << "    pop r7\n";
    code_ << "    poplr\n";
}

void STM32Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    cmpz r0\n";
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
            code_ << "    cmpz r0\n    beq " << Lend << "\n";
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
                code_ << "    str r0,[r2]\n";
            }
            // No "else" case for a bare, uninitialized scalar: its own
            // frame word already exists (genFunction()'s own reserve-
            // pushes already made room for it) - same as a real,
            // ordinary C automatic variable, its value is simply
            // whatever was already in that stack slot until something
            // assigns it.
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

void STM32Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",r0\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("r2", e.slot);
            code_ << "    ldr r0,[r2]\n";
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
                code_ << "    str r0,[r2]\n";
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("r2", e.slot);
            code_ << "    str r0,[r2]\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);              // r0 = pointer value
            code_ << "    mov r2,r0\n";
            code_ << "    push r2\n";
            genExpr(*e.rhs);
            code_ << "    pop r2\n";
            code_ << "    str r0,[r2]\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    neg r0,r0\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // r0 = pointer value
                code_ << "    mov r2,r0\n";
                code_ << "    ldr r0,[r2]\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn r0,r0\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Arguments are pushed onto the real stack, last-declared
            // one first, so the first argument ends up closest to the
            // return address `bl` is about to set `lr` for - see this
            // file's own header comment. Each callee lays them into
            // ITS OWN frame itself; nothing here needs to know
            // anything about the callee's own parameter count or
            // names beyond `e.args.size()`. The caller cleans the
            // pushed words back off afterward with one dummy `pop r1`
            // per argument (no `add sp,#imm` instruction exists in
            // this dialect - see genFunction()'s own identical
            // reasoning for the Local-frame reserve/release).
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                code_ << "    push r0\n";
            }
            code_ << "    bl " << label(e.func) << "\n";
            for (size_t i = 0; i < e.args.size(); i++) code_ << "    pop r1\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            // "blx r0" (STM32Assembler.cpp's new register-indirect
            // branch-with-link encoding) is a real Thumb indirect call
            // through whatever address r0 holds.
            genExpr(*e.lhs);  // r0 = the callee's own entry address
            code_ << "    blx r0\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Same "real but nothing for a bare-metal no-OS compile
            // to meaningfully do" stance as M68kCodegen.cpp's own
            // `_ion()`/`_iof()` comment - a real Cortex-M has a real
            // PRIMASK/`cpsie`/`cpsid`, but nothing here ever sets up
            // an exception/interrupt table for it to matter against.
            (void)e;
            return;
        }
    }
}

// `r2` holds the computed address, the same role m68k's `A0` plays
// (no zero-page-style placement restriction the 6502 backend's own
// `ptr` has). `arraySize > 0` means the base is the array's own first
// word - a compile-time-known label for a Global, or (see
// loadFrameAddr()) a runtime r7-relative address for a Local/Param;
// otherwise `slot` is a plain pointer variable, so the base instead
// comes from *reading* that variable's own stored value first (again
// from a label or an r7-relative address, depending on `slot->kind`).
// A Local/Param array's own element i sits *below* element 0
// (addresses decrease as the index increases - see this file's own
// header comment), the mirror image of a Global array's increasing-
// address layout, so this is base-minus-scaled-index there, not
// base-plus.
void STM32Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // r0 = index
    // r0 *= 4 (words are 4 bytes on this target); done as two
    // doublings rather than a shift, needing no extra register at
    // all - same trick as M68kCodegen.cpp's own genIndexAddr().
    code_ << "    add r0,r0,r0\n    add r0,r0,r0\n";
    if (slot->arraySize > 0) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lea " << label(slot) << ",r2\n";
            code_ << "    add r2,r2,r0\n";
        } else {
            loadFrameAddr("r2", slot);
            code_ << "    sub r2,r2,r0\n";
        }
        return;
    }
    loadSlotAddr("r2", slot);
    code_ << "    ldr r2,[r2]\n";  // r2 = the pointer variable's own stored value
    code_ << "    add r2,r2,r0\n";
}

void STM32Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldr r0,[r2]\n";
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a Local's
// storage is this activation's own fresh frame region, not pre-set to
// anything; arrayInit is always fully populated up to arraySize, so
// this always leaves the whole array in a known state, same as a
// Global array's static trailer always did. An element's own address
// is slot->frameOffset+i words below r7, the same arithmetic
// loadFrameAddr() uses for the array's own first word, just advanced
// by i.
void STM32Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        loadArrayInitElem("r0", slot->arrayInit[i]);
        loadFrameWordAddr("r2", -(slot->frameOffset + static_cast<int>(i)));
        code_ << "    str r0,[r2]\n";
    }
}

void STM32Codegen::genAddrOf(const ir::Expr &operand) {
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
        // Real Cortex-M (Thumb-only) interworking: a function's own
        // numeric address value must carry the Thumb-select bit
        // (LSB=1) for `blx` (CallPtr's own register-indirect call,
        // below) to branch into it correctly - same convention this
        // backend's own vector table already uses for `_start`'s own
        // reset-handler entry (see STM32Assembler.h's own `dlt`
        // comment). Every function label here is always even (Thumb
        // instructions are 2-byte aligned), so this is exactly
        // "address + 1".
        code_ << "    lea " << label(operand.func) << ",r0\n";
        code_ << "    movl 1,r1\n";
        code_ << "    add r0,r0,r1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void STM32Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "+") {
            code_ << "    add r0,r0,r1\n";  // r0 = rhs + lhs
        } else {
            code_ << "    sub r0,r1,r0\n";  // r0 = lhs - rhs
        }
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
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
        if (op == "&") {
            code_ << "    and r0,r1\n";
        } else if (op == "|") {
            code_ << "    orr r0,r1\n";
        } else {
            code_ << "    eor r0,r1\n";
        }
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    push r0\n";
        genExpr(*e.rhs);
        code_ << "    mov r1,r0\n";  // r1 = shift count
        code_ << "    pop r0\n";     // r0 = value to shift
        code_ << "    " << (op == "<<" ? "lsl" : "lsr") << " r0,r0,r1\n";
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

// Real hardware multiply (`muls`) - a genuine 32x32->32 product, no
// operand-width caveat the way m68k's word-sized `MULS.W` needs (see
// STM32Codegen.h's own header comment).
void STM32Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";    // r1 = lhs, r0 = rhs
    code_ << "    mul r1,r0\n";  // r0 = r0 * r1 = rhs * lhs
}

// Real hardware divide (`sdiv`) - this CPU, unlike every other target
// here (even the original 68000), has a genuine signed integer divide
// with no operand-width limitation at all, so unlike every other
// backend's own genDivMod() there's no software loop or quotient/
// remainder-packing trick needed: `%` is computed directly from a
// real division plus one real multiply-subtract (see STM32Codegen.h's
// own header comment).
void STM32Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";          // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    sdiv r2,r1,r0\n";  // r2 = lhs / rhs
    if (wantRemainder) {
        code_ << "    mul r0,r2\n";       // r2 = r2 * r0 = quotient * rhs
        code_ << "    sub r0,r1,r2\n";    // r0 = lhs - quotient*rhs = remainder
    } else {
        code_ << "    mov r0,r2\n";
    }
}

void STM32Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitCompare(lhs, rhs, "beq");
    if (op == "!=") return emitCompare(lhs, rhs, "bne");
    if (op == "<") return emitCompare(lhs, rhs, "blt");
    if (op == "<=") return emitCompare(lhs, rhs, "ble");
    if (op == ">") return emitCompare(lhs, rhs, "bgt");
    if (op == ">=") return emitCompare(lhs, rhs, "bge");
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Real signed `cmp` + condition-code branch - this CPU's own flags
// already encode signed less-than/greater-than correctly in hardware
// (same as m68k's `CMP.L`+`Bcc` - see STM32Codegen.h's own header
// comment), no fixup needed the way the 6502 backend's own
// comparisons do.
void STM32Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp r1,r0\n";  // flags = r1 - r0 = lhs - rhs
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    " << branchIfTrue << " " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    cmpz r0\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    beq " << Ltrue << "\n";
    code_ << "    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

std::string STM32Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the stm32 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> STM32Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleSTM32(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see STM32Assembler.h's own `assembleSTM32Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see fileformat/FileFormat.h)
    // to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleSTM32Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
