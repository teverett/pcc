#include "STM32Codegen.h"

#include <algorithm>
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

void STM32Codegen::declareTemp(const std::string &lbl, Type type) {
    ramTrailer_.push_back(lbl + ": dsb " + std::to_string(widthOfType(type)));
}

void STM32Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    ramTrailer_.push_back(lbl + ": dsb " + std::to_string(widthOfType(elemType) * values.size()));
}

// A literal element's own value via `movl`; a symbol element's
// address via the same `lea` genAddrOf() uses for &slot, with that
// same function's own thumb-bit adjustment (`+1`, for `blx` - see
// genAddrOf()'s own comment) when the element names a Function
// rather than a Slot. Either way the loaded value then gets copied
// into the RAM cell `lbl+offset` resolves to (truncating to
// `elemType`'s own width via `strb`/`strh`/`str` - a `long long`
// array's own literal element additionally needs its own high word
// written, since `str` here is always exactly 4 bytes) - see
// genGlobalInit()'s own comment on why RAM needs this explicit
// one-time copy at all.
void STM32Codegen::genArrayElemInit(const std::string &lbl, size_t offset, Type elemType, const ir::ArrayInitElem &e) {
    if (!e.isSymbol && e.value == 0) return;  // RAM already starts zeroed - nothing to copy
    int w = widthOfType(elemType);
    if (e.isSymbol) {
        code_ << "    lea " << (e.slot ? label(e.slot) : label(e.func)) << ",r0\n";
        if (!e.slot) {
            code_ << "    movl 1,r1\n";
            code_ << "    add r0,r0,r1\n";
        }
    } else if (w == 8) {
        auto v = static_cast<uint64_t>(e.value);
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v)) << ",r0\n";
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
    } else {
        code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.value)) << ",r0\n";
    }
    code_ << "    lea " << lbl << "+" << offset << ",r2\n";
    const char *store = w == 1 ? "strb" : w == 2 ? "strh" : "str";
    code_ << "    " << store << " r0,[r2]\n";
    if (w == 8) {
        code_ << "    movl 4,r3\n    add r2,r2,r3\n";
        code_ << "    str r1,[r2]\n";
    }
}

// Flash-resident code that writes `value` into the RAM cell `lbl`
// resolves to, truncated to `type`'s own width - see
// STM32Codegen.h's own header comment on why a slot's own real
// initial value can't just sit there in the image the way every
// other backend's flat-blob data does: RAM starts out with whatever
// qemu/real silicon happens to leave it as, so a nonzero initializer
// needs an explicit one-time copy, emitted as ordinary code into
// whichever stream the caller is building (this backend's own
// `_start`, for a global; right where a `LocalDecl` executes, for a
// local - see genStmt()'s own `LocalDecl` case).
void STM32Codegen::genGlobalInit(const std::string &ramLabel, Type type, long value) {
    if (value == 0) return;  // RAM already starts zeroed - nothing to copy
    int w = widthOfType(type);
    code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(value)) << ",r0\n";
    if (w == 8) code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(static_cast<uint64_t>(value) >> 32)) << ",r1\n";
    code_ << "    lea " << ramLabel << ",r2\n";
    const char *store = w == 1 ? "strb" : w == 2 ? "strh" : "str";
    code_ << "    " << store << " r0,[r2]\n";
    if (w == 8) {
        code_ << "    movl 4,r3\n    add r2,r2,r3\n";
        code_ << "    str r1,[r2]\n";
    }
}

// `byteOffsetFromFp` is a real byte displacement now (not a word
// count pre-scaled by 4 here) - see IR.h's own comment on
// Slot::frameOffset: IRBuilder.cpp computes it as an already-correct
// byte offset (accounting for each slot's own type's width/alignment,
// and - for a Param - the 2-native-int-width reservation past the
// saved r7/return address), so this function's own job shrinks to
// "add or subtract exactly that many bytes from r7", no multiplying
// by this backend's own word size any more.
void STM32Codegen::loadFrameWordAddr(const std::string &reg, int byteOffsetFromFp) {
    code_ << "    mov " << reg << ",r7\n";
    if (byteOffsetFromFp == 0) return;
    code_ << "    movl " << (byteOffsetFromFp > 0 ? byteOffsetFromFp : -byteOffsetFromFp) << ",r3\n";
    code_ << "    " << (byteOffsetFromFp > 0 ? "add" : "sub") << " " << reg << "," << reg << ",r3\n";
}

void STM32Codegen::loadFrameAddr(const std::string &reg, const ir::Slot *slot) {
    int byteOffsetFromFp = slot->kind == ir::SlotKind::Param ? slot->frameOffset : -slot->frameOffset;
    loadFrameWordAddr(reg, byteOffsetFromFp);
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
    // Running byte offset within RAM (byte-granular reservation now -
    // see declareTemp()'s/declareArray()'s own `dsb` comment), used
    // only to pad ahead of a global whose own type is wider than 1
    // byte - same reasoning as ArmCodegen.cpp's own identical comment
    // (an unaligned `ldr`/`str`/`ldrh`/`strh` is UNPREDICTABLE/
    // implementation-defined on this real core). `kRamBase` is itself
    // word-aligned, so tracking this from 0 is exact.
    size_t ramOffset = 0;
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
        int align = std::min(widthOfType(slot->type), 4);
        size_t padding = (align - (ramOffset % align)) % align;
        if (padding > 0) {
            ramTrailer_.push_back("dsb " + std::to_string(padding));
            ramOffset += padding;
        }
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit, slot->type);
            ramOffset += static_cast<size_t>(widthOfType(slot->type)) * static_cast<size_t>(slot->arraySize);
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
                int w = widthOfType(slot->type);
                for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                    genArrayElemInit(lbl, i * static_cast<size_t>(w), slot->type, slot->arrayInit[i]);
                }
            }
        } else {
            declareTemp(lbl, slot->type);
            ramOffset += static_cast<size_t>(widthOfType(slot->type));
            if (!objectMode) genGlobalInit(lbl, slot->type, slot->hasInit ? slot->initValue : 0);
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
    // frameSize is now a real byte count (see IR.h's own comment on
    // Function::frameSize) - each dummy push/pop still only ever
    // reserves/releases a whole 4-byte register regardless (this
    // dialect's PUSH is always register-width, see this file's own
    // header comment on why there's no `sub sp,#imm` here yet), so
    // this rounds up to the nearest whole word's worth of pushes.
    frameSize_ = fn.func->frameSize;
    int frameWords = (frameSize_ + 3) / 4;
    for (int i = 0; i < frameWords; i++) code_ << "    push r0\n";
    genStmt(*fn.body);
    emitEpilogue();  // reached only if control falls off the end with no explicit `return`
    code_ << "\n";
}

void STM32Codegen::emitEpilogue() {
    int frameWords = (frameSize_ + 3) / 4;
    for (int i = 0; i < frameWords; i++) code_ << "    pop r1\n";
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
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
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
            if (isWide(s.cond->type)) code_ << "    orr r0,r1\n";
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
                storeOperand("r2", s.declSlot->type);
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
            if (isWide(e.type)) {
                auto v = static_cast<uint64_t>(e.intValue);
                code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v)) << ",r0\n";
                code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
                return;
            }
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(e.intValue)) << ",r0\n";
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
            genExpr(*e.lhs);              // r0 = pointer value (deref is untyped - always native-word)
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
                code_ << "    ldr r0,[r2]\n";
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
            // return address `bl` is about to set `lr` for - see this
            // file's own header comment. Each callee lays them into
            // ITS OWN frame itself; nothing here needs to know
            // anything about the callee's own parameter count or
            // names beyond `e.args.size()`. The caller cleans the
            // pushed words back off afterward with one dummy `pop r1`
            // per argument (no `add sp,#imm` instruction exists in
            // this dialect - see genFunction()'s own identical
            // reasoning for the Local-frame reserve/release).
            // A wide (`long long`) argument pushes its high word
            // first so its low word ends up closer to `r7`, matching
            // a wide Param's own frameOffset/frameOffset+4 layout.
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
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
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
    // r0 *= the element type's own width (1/2/4/8 bytes, via
    // elemShift()) - a real variable-count `lsl`, no loop needed.
    int shift = elemShift(slot->type);
    if (shift > 0) {
        code_ << "    movl " << shift << ",r3\n";
        code_ << "    lsl r0,r0,r3\n";
    }
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
    loadOperand("r2", e.slot->type);
}

// Stores arrayInit into `slot`'s own frame slots - every call needs
// this re-run, since (unlike a Global's one static trailer) a Local's
// storage is this activation's own fresh frame region, not pre-set to
// anything; arrayInit is always fully populated up to arraySize, so
// this always leaves the whole array in a known state, same as a
// Global array's static trailer always did. An element's own address
// is slot->frameOffset+i*widthOfType(slot->type) bytes below r7, the
// same arithmetic loadFrameAddr() uses for the array's own first
// word, just advanced by i element widths.
void STM32Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        loadArrayInitElem("r0", elem);
        if (w == 8 && !elem.isSymbol) {
            auto v = static_cast<uint64_t>(elem.value);
            code_ << "    movl " << static_cast<int32_t>(static_cast<uint32_t>(v >> 32)) << ",r1\n";
        }
        loadFrameWordAddr("r2", -(slot->frameOffset + w * static_cast<int>(i)));
        storeOperand("r2", slot->type);
    }
}

void STM32Codegen::narrowR0To(Type to) {
    int w = widthOfType(to);
    // Re-truncate+re-extend via a real variable-count `lsl` then
    // `asr`/`lsr` (zero-fill for unsigned, sign-fill for signed) -
    // `movl` loads the shift amount itself (24 or 16) with no 8-bit
    // immediate limit to work around, unlike ../armm/ArmMCodegen.cpp's
    // own version of this same idiom.
    if (w == 1) {
        code_ << "    movl 24,r3\n";
        code_ << "    lsl r0,r0,r3\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,r3\n";
    } else if (w == 2) {
        code_ << "    movl 16,r3\n";
        code_ << "    lsl r0,r0,r3\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " r0,r0,r3\n";
    }
}

// `addrReg` holds the already-computed effective address (r2, by
// this backend's own convention); loads into r0 (or r0:r1, for a
// `long long`), sign- or zero-extending a narrower-than-native-int
// value per its own signedness. `ldrb`/`ldrh` always zero-extend
// (see STM32Assembler.cpp's own comment) - a signed source needs an
// additional re-sign-extend via narrowR0To().
void STM32Codegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    ldrb r0,[" << addrReg << "]\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 2) {
        code_ << "    ldrh r0,[" << addrReg << "]\n";
        if (!type.isUnsigned) narrowR0To(type);
    } else if (w == 4) {
        code_ << "    ldr r0,[" << addrReg << "]\n";
    } else {
        code_ << "    ldr r0,[" << addrReg << "]\n";
        code_ << "    movl 4,r3\n    add r6," << addrReg << ",r3\n";
        code_ << "    ldr r1,[r6]\n";
    }
}

void STM32Codegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    strb r0,[" << addrReg << "]\n";
    } else if (w == 2) {
        code_ << "    strh r0,[" << addrReg << "]\n";
    } else if (w == 4) {
        code_ << "    str r0,[" << addrReg << "]\n";
    } else {
        code_ << "    str r0,[" << addrReg << "]\n";
        code_ << "    movl 4,r3\n    add r6," << addrReg << ",r3\n";
        code_ << "    str r1,[r6]\n";
    }
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is crossing the native-int/`long
// long` boundary, or re-truncating+re-extending r0 for a narrower
// target - see narrowR0To().
void STM32Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowR0To(to);  // r1 is simply discarded
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            code_ << "    movl 0,r1\n";
        } else {
            code_ << "    mov r1,r0\n    movl 31,r3\n    asr r1,r1,r3\n";
        }
    } else if (!fromWide && !toWide) {
        narrowR0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

void STM32Codegen::genNegate64() {
    // Two's complement negate of r0:r1 in place.
    code_ << "    mvn r0,r0\n    mvn r1,r1\n";
    code_ << "    movl 1,r3\n    add r0,r0,r3\n";
    code_ << "    movl 0,r3\n    adc r1,r3\n";
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

// `long long` add/sub (lhs OP rhs) - evaluate lhs into r0:r1,
// stashing its two words on the stack while rhs evaluates into
// r4:r5, then ripple the carry/borrow across the word pair with
// `adc`/`sbc`.
void STM32Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stack (top->bottom): lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r4,r0\n    mov r5,r1\n";  // r4=rhs_lo, r5=rhs_hi
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = lhs restored
    if (isAdd) {
        code_ << "    add r0,r0,r4\n    adc r1,r5\n";
    } else {
        code_ << "    sub r0,r0,r4\n    sbc r1,r5\n";
    }
}

// 32x32->64 unsigned widening multiply: {r1:r0} = r6(a) * r4(b),
// both consumed - the caller must place `a` in r6 and `b` in r4
// before calling. This dialect's real variable-count `lsl`/`lsr`
// only ever shift within one 32-bit register, so a genuine 64-bit
// product still needs the same bit-serial shift-and-add loop as
// ../armm/ArmMCodegen.cpp's own `genMul64()`: `a` consumed 1 bit at
// a time (shifted right, carry tested via `bhs`/`blo` - the real
// carry-flag condition codes, no separate AND/compare needed); `b`'s
// own current doubled value (r4:r5, growing to 64 bits) added into
// the 64-bit accumulator (r0:r1) whenever that bit is 1. Clobbers
// r0-r6.
void STM32Codegen::genMul64() {
    code_ << "    movl 0,r5\n";  // r5 = b's own high word (b, r4, is only ever 32 bits wide)
    code_ << "    movl 0,r0\n    movl 0,r1\n";
    code_ << "    movl 0,r2\n";  // r2 = iteration count-up (0..32)
    std::string Lloop = newLabel(), LdoAdd = newLabel(), LskipAdd = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    movl 32,r3\n    cmp r2,r3\n    beq " << Ldone << "\n";
    code_ << "    movl 1,r3\n    add r2,r2,r3\n";
    // Consume `a`'s own low bit by shifting it out - the carry flag
    // this leaves behind (the real "last bit shifted out" convention
    // every shift instruction here follows) IS that bit, tested
    // directly via `bhs`/`blo` (literally the carry-set/carry-clear
    // condition codes) with no separate AND/compare needed - same
    // idiom as ../armm/ArmMCodegen.cpp's own `genMul64()`.
    code_ << "    lsr r6,r6,r3\n    bhs " << LdoAdd << "\n    b " << LskipAdd << "\n";
    code_ << LdoAdd << ":\n";
    code_ << "    add r0,r0,r4\n    adc r1,r5\n";
    code_ << LskipAdd << ":\n";
    // Double `b` (r4:r5) by 1 bit for the next iteration - `add
    // r4,r4,r4` is a self-add, doubling r4 while leaving its own old
    // top bit in the carry flag for `adc r5,r5` to fold in.
    code_ << "    add r4,r4,r4\n    adc r5,r5\n";
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// `long long` multiply, keeping the low 64 bits of the true 128-bit
// product: result = lo(lhs)*lo(rhs) (the full 64-bit product, via
// genMul64()) plus the two cross terms lhs_hi*rhs_lo/lhs_lo*rhs_hi
// folded into the high word only (truncating `mul` is exactly right
// for them - see ArmCodegen.cpp's own identical reasoning).
void STM32Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";  // stash lhs: top->bottom = lhs_lo, lhs_hi
    genExpr(rhs);
    code_ << "    mov r6,r0\n    mov r5,r1\n";  // r6=rhs_lo, r5=rhs_hi
    code_ << "    pop r4\n    pop r2\n";        // r4=lhs_lo, r2=lhs_hi
    // Cross terms, computed now (before genMul64() clobbers r4/r6):
    // r3 = lhs_hi*rhs_lo, then r2 = rhs_hi*lhs_lo, summed into r2 -
    // r4 (lhs_lo) and r6 (rhs_lo) are left untouched for genMul64().
    code_ << "    mov r3,r2\n    mul r6,r3\n";  // r3 = lhs_hi*rhs_lo
    code_ << "    mov r2,r5\n    mul r4,r2\n";  // r2 = rhs_hi*lhs_lo
    code_ << "    add r2,r2,r3\n";
    code_ << "    push r2\n";  // stash the cross-term sum across genMul64()'s own full clobber
    genMul64();                 // {r1:r0} = r6(rhs_lo) * r4(lhs_lo), the full 64-bit low*low product
    code_ << "    pop r2\n";
    code_ << "    add r1,r1,r2\n";
}

// A `long long` shifted by a runtime count - no instruction here
// shifts more than 32 bits at once, so this is a counted loop of
// 1-bit shifts (r6 = count). `<<` doubles r0:r1 via the self-add
// carry-chained-doubling trick; `>>` needs the opposite direction,
// which this dialect's real `lsr`/`asr` can do directly within one
// register but without a carry-chain analogue of `adc` for the
// *downward* direction, so the bit crossing the r1/r0 boundary is
// captured by hand into r4 (the high word's own old bit 0) before
// shifting, then OR'd into r0's own new top bit via a precomputed
// 0x80000000 mask (r3) - `isUnsigned` only matters here, picking
// `asr` (sign-preserving) or `lsr` (zero-filling) for r1's own
// shift. Same overall shape as ../armm/ArmMCodegen.cpp's own
// `genWideShift()`.
void STM32Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);  // r0 = shift count (already promoted to a plain int)
    code_ << "    mov r6,r0\n";
    code_ << "    pop r0\n    pop r1\n";  // r0:r1 restored = the value being shifted
    code_ << "    movl 1,r5\n";            // r5 = the fixed amount (1) every shift/decrement below uses
    std::string Lloop = newLabel(), Ldone = newLabel();
    if (!isLeft) {
        code_ << "    movl 1,r3\n    movl 31,r2\n    lsl r3,r3,r2\n";  // r3 = 0x80000000
    }
    code_ << Lloop << ":\n";
    code_ << "    cmpz r6\n    beq " << Ldone << "\n    sub r6,r6,r5\n";
    if (isLeft) {
        code_ << "    add r0,r0,r0\n    adc r1,r1\n";
    } else {
        std::string LskipOr = newLabel();
        code_ << "    mov r4,r1\n    and r4,r5\n";  // r4 = r1's own old bit 0 (the bit crossing into r0)
        code_ << "    " << (isUnsigned ? "lsr" : "asr") << " r1,r1,r5\n";
        code_ << "    lsr r0,r0,r5\n";
        code_ << "    cmpz r4\n    beq " << LskipOr << "\n";
        code_ << "    orr r0,r3\n";
        code_ << LskipOr << ":\n";
    }
    code_ << "    b " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Classic bit-serial shift/compare/subtract long division (no
// instruction here is wide enough for a genuine 64/64 divide) - same
// overall shape as ../armm/ArmMCodegen.cpp's own `genWideDivMod()`:
// r0:r1 start as the dividend and finish as the quotient (the
// standard restoring-division trick of building the quotient into
// the bits vacated by the dividend as it shifts left); r4:r5 is the
// remainder (starts 0); r2:r3 is the divisor. `isUnsigned` false
// divides *magnitudes* (each operand's own sign bit, read before
// negating, is stashed on the stack across the whole loop and
// restored on the result afterward per C's truncating-division
// rule).
void STM32Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r1\n    push r0\n";
    genExpr(rhs);
    code_ << "    mov r2,r0\n    mov r3,r1\n";  // r2:r3 = divisor
    code_ << "    pop r0\n    pop r1\n";        // r0:r1 = dividend

    if (!isUnsigned) {
        code_ << "    movl 31,r4\n";
        code_ << "    mov r5,r1\n    lsr r5,r5,r4\n";  // r5 = dividend's own sign (0/1)
        code_ << "    mov r6,r3\n    lsr r6,r6,r4\n";  // r6 = divisor's own sign (0/1)
        code_ << "    push r5\n    push r6\n";          // stack (top->bottom): dividendSign, divisorSign
        std::string LskipNegN = newLabel(), LskipNegD = newLabel();
        code_ << "    cmpz r5\n    beq " << LskipNegN << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    movl 1,r4\n    add r0,r0,r4\n    movl 0,r4\n    adc r1,r4\n";
        code_ << LskipNegN << ":\n";
        code_ << "    cmpz r6\n    beq " << LskipNegD << "\n";
        code_ << "    mvn r2,r2\n    mvn r3,r3\n    movl 1,r4\n    add r2,r2,r4\n    movl 0,r4\n    adc r3,r4\n";
        code_ << LskipNegD << ":\n";
    }

    code_ << "    movl 0,r4\n    movl 0,r5\n";  // remainder = 0
    code_ << "    movl 64,r6\n";                 // iteration count-down (64..0) - see this
                                                   // function's own header comment on why
                                                   // `cmpz` (no second register needed) picks
                                                   // this direction over armm's own count-up

    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string LhiDiff = newLabel(), LsubYes = newLabel(), LsubNo = newLabel();
    code_ << Lloop << ":\n    cmpz r6\n    beq " << Ldone << "\n";
    // Every register 0-6 already holds live loop state (dividend/
    // quotient, divisor, remainder, counter) with nothing spare for
    // a "1" constant - `push r3`/`pop r3` borrows the divisor's own
    // high word as a one-instruction scratch slot, safe since
    // nothing reads it again until well after this restore.
    code_ << "    push r3\n    movl 1,r3\n    sub r6,r6,r3\n    pop r3\n";

    // Shift {remainder:dividend} left by 1 as one combined 128-bit
    // value, via the carry-chained-doubling trick (self-add on r0,
    // `adc` on r1/r4/r5 in sequence) - the bit that overflows out of
    // the dividend half (r1's own old top bit) lands, via the carry
    // chain, directly into the remainder's own new bit 0.
    code_ << "    add r0,r0,r0\n    adc r1,r1\n    adc r4,r4\n    adc r5,r5\n";

    // if (remainder >= divisor) { remainder -= divisor; quotient bit = 1 }
    // (always an unsigned magnitude compare - both operands were
    // already negated to non-negative above for the signed case).
    code_ << "    cmp r5,r3\n    bne " << LhiDiff << "\n";
    code_ << "    cmp r4,r2\n    bhs " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LhiDiff << ":\n    bhi " << LsubYes << "\n    b " << LsubNo << "\n";
    code_ << LsubYes << ":\n";
    code_ << "    sub r4,r4,r2\n    sbc r5,r3\n";
    code_ << "    push r3\n    movl 1,r3\n    add r0,r0,r3\n    pop r3\n";  // set the quotient bit the shift above just vacated
    code_ << LsubNo << ":\n";
    code_ << "    b " << Lloop << "\n";

    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    pop r6\n";  // r6 = divisorSign
        code_ << "    pop r2\n";  // r2 = dividendSign
        std::string LskipNegQ = newLabel(), LskipNegR = newLabel();
        code_ << "    mov r3,r6\n    eor r3,r2\n";  // r3 = divisorSign ^ dividendSign (quotient-negate flag)
        code_ << "    cmpz r3\n    beq " << LskipNegQ << "\n";
        code_ << "    mvn r0,r0\n    mvn r1,r1\n    movl 1,r6\n    add r0,r0,r6\n    movl 0,r6\n    adc r1,r6\n";
        code_ << LskipNegQ << ":\n";
        code_ << "    cmpz r2\n    beq " << LskipNegR << "\n";
        code_ << "    mvn r4,r4\n    mvn r5,r5\n    movl 1,r6\n    add r4,r4,r6\n    movl 0,r6\n    adc r5,r6\n";
        code_ << LskipNegR << ":\n";
    }
    if (wantRemainder) {
        code_ << "    mov r0,r4\n    mov r1,r5\n";
    }
}

// `long long` comparison - compare the high words first (per
// `isUnsigned`, since that's where the sign bit lives); only if
// those are equal does the low word's own comparison matter, and at
// that point it's always unsigned regardless of the overall type's
// signedness.
void STM32Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
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
        code_ << "    movl " << onEqual << ",r0\n    b " << Ldone << "\n";
        code_ << Lfalse << ":\n    movl " << onDiffer << ",r0\n";
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
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genBinary(const ir::Expr &e) {
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
            code_ << "    add r0,r0,r1\n";  // r0 = rhs + lhs
        } else {
            code_ << "    sub r0,r1,r0\n";  // r0 = lhs - rhs
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
        genExpr(*e.rhs);
        code_ << "    mov r1,r0\n";  // r1 = shift count
        code_ << "    pop r0\n";     // r0 = value to shift
        const char *insn = op == "<<" ? "lsl" : (isUnsigned ? "lsr" : "asr");
        code_ << "    " << insn << " r0,r0,r1\n";
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
// own header comment). `isUnsigned` picks `udiv`/`sdiv`.
void STM32Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    push r0\n";
    genExpr(rhs);
    code_ << "    pop r1\n";          // r1 = lhs (dividend), r0 = rhs (divisor)
    code_ << "    " << (isUnsigned ? "udiv" : "sdiv") << " r2,r1,r0\n";  // r2 = lhs / rhs
    if (wantRemainder) {
        code_ << "    mul r0,r2\n";       // r2 = r2 * r0 = quotient * rhs
        code_ << "    sub r0,r1,r2\n";    // r0 = lhs - quotient*rhs = remainder
    } else {
        code_ << "    mov r0,r2\n";
    }
}

void STM32Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (isWide(lhs.type)) {
        genWideCompare(op, lhs, rhs, lhs.type.isUnsigned);
        return;
    }
    bool isUnsigned = lhs.type.isUnsigned;
    if (op == "==") return emitCompare(lhs, rhs, "beq", isUnsigned);
    if (op == "!=") return emitCompare(lhs, rhs, "bne", isUnsigned);
    if (op == "<") return emitCompare(lhs, rhs, isUnsigned ? "blo" : "blt", isUnsigned);
    if (op == "<=") return emitCompare(lhs, rhs, isUnsigned ? "bls" : "ble", isUnsigned);
    if (op == ">") return emitCompare(lhs, rhs, isUnsigned ? "bhi" : "bgt", isUnsigned);
    if (op == ">=") return emitCompare(lhs, rhs, isUnsigned ? "bhs" : "bge", isUnsigned);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// Real `cmp` + condition-code branch - this CPU's own flags already
// encode signed/unsigned ordering correctly in hardware (same as
// m68k's `CMP.L`+`Bcc` - see STM32Codegen.h's own header comment),
// no fixup needed the way the 6502 backend's own comparisons do.
// `isUnsigned` is unused directly here (already baked into which
// `branchIfTrue` mnemonic genCompare() picked) but documents why.
void STM32Codegen::emitCompare(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &branchIfTrue,
                                 bool isUnsigned) {
    (void)isUnsigned;
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

// A wide (`long long`) operand's truthiness needs both words - `orr
// r0,r1` folds them together before the usual "cmpz r0" (the
// combined value is discarded immediately after, so clobbering r0
// this way is safe).
void STM32Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmpz r0\n    beq " << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    if (isWide(lhs.type)) code_ << "    orr r0,r1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
    code_ << "    cmpz r0\n    bne " << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movl 1,r0\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movl 0,r0\n";
    code_ << Ldone << ":\n";
}

void STM32Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    if (isWide(rhs.type)) code_ << "    orr r0,r1\n";
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
