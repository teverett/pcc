#include "M6502Codegen.h"

#include <cstdint>
#include <stdexcept>

#include "M6502Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Where the whole program is loaded and starts executing - a bare,
// no-OS flat binary (see M6502Codegen.h's own header comment), $0600
// being the conventional "put your test program here" address for a
// minimal/bare 6502 setup (below any BASIC/OS reservation, above
// zero page and the hardware stack page $0100-$01FF).
constexpr uint32_t kBaseAddress = 0x0600;

// Where every call's own Local/Param frame storage is bump-allocated
// from (see M6502Codegen.h's own header comment on `fp`/`sp`) - below
// the real hardware stack page ($0100-$01FF, left to JSR/RTS and this
// backend's own transient PHA/PLA use) and comfortably below where
// any program's own code/data ever starts (`kBaseAddress` above, and
// the same address real separate compilation always links at too -
// see M6502Assembler.h's own `kM6502BaseAddress`), so this region
// never collides with either.
constexpr uint32_t kFrameStackBase = 0x0200;
}  // namespace

// A literal element's own decimal text, or a symbol element's (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) own label - the referenced Slot's or Function's own
// label() text. declareArray() below never has a resolved numeric
// value for a symbol element at codegen time (only the assembler,
// once every label is bound, does), so unlike a plain literal it
// can't be split into its own low/high byte here - each half is
// instead written out as its own `dblo`/`dbhi` directive and left for
// the assembler to resolve, the same way genAddrOf()/genIndexAddr()
// already get a label's low/high byte via `ldai`/`ldai_hi` (see
// M6502Assembler.cpp's own `emitImmLow`/`emitImmHigh`).
std::string M6502Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

void M6502Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values) {
    if (values.empty()) return;
    for (size_t i = 0; i < values.size(); i++) {
        std::string text = arrayElemText(values[i]);
        if (i == 0) trailer_.push_back(lbl + ": dblo " + text);
        else trailer_.push_back("dblo " + text);
        trailer_.push_back("dbhi " + text);
    }
}

void M6502Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lda " << src << "\n    sta " << dst << "\n";
    code_ << "    lda " << src << "+1\n    sta " << dst << "+1\n";
}

void M6502Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

void M6502Codegen::inc16(const std::string &lbl) {
    std::string Lskip = newLabel();
    code_ << "    inc " << lbl << "\n";
    code_ << "    lda " << lbl << "\n";
    code_ << "    bne " << Lskip << "\n";
    code_ << "    inc " << lbl << "+1\n";
    code_ << Lskip << ":\n";
}

std::string M6502Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all - an ordinary object
    // file never does. `objectMode` (see Codegen.h) means no object
    // file ever emits this entry sequence itself, even the one that
    // happens to define `main` - M6502Linker.cpp is solely
    // responsible for synthesizing exactly one, once every object is
    // merged and `main`'s linked address is resolvable (see
    // Linker.h) - same pattern Z80Codegen.cpp's own `objectMode`
    // comment documents. An object file's own base is `org 0` - it's
    // the linker's job to place it, not this backend's.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    // The fixed zero-page pseudo-registers - see M6502Codegen.h's own
    // header comment for what each one is for and why `ptr`
    // specifically has to be bound this way (a pure compile-time
    // equate, not the usual sequential data-trailer layout) rather
    // than wherever declareTemp() would otherwise place it. Unlike
    // Z80Codegen.cpp's own shared memory cells (which really are
    // runtime storage, and so can't be redeclared per object - see
    // that file's own `generate()` comment), these are plain
    // compile-time constants: `ptr`/`ac`/`bc`/`cx`/`dx` bind to the
    // exact same zero-page addresses in every object regardless of
    // where it ends up linked, so every object - including this one,
    // even in object mode - declares them identically with no
    // conflict and no relocation ever needed for the equate itself.
    // `fp`/`sp` - see M6502Codegen.h's own header comment: the
    // software frame pointer every Local/Param access goes through,
    // and the bump allocator tracking the next free byte of
    // `kFrameStackBase`'s own region that every call's frame is cut
    // from. Equated identically to `ptr`/`ac`/`bc`/`cx`/`dx` above,
    // for the same reason (see that code's own comment).
    code_ << "ptr = 0\nac = 2\nbc = 4\ncx = 6\ndx = 8\nfp = 10\nsp = 12\naBase = 14\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        // One-time initialization of the frame-stack bump allocator,
        // before the very first call - every function's own prologue
        // (genFunction()) only ever reads/advances `sp` from here on,
        // never re-initializes it.
        code_ << "    ldai " << (kFrameStackBase & 0xFF) << "\n    sta sp\n";
        code_ << "    ldai " << ((kFrameStackBase >> 8) & 0xFF) << "\n    sta sp+1\n";
        code_ << "    jsr " << label(mainFn) << "\n";
        // The program's own final result sits in `ac`/`ac+1` (memory,
        // $0002/$0003) the moment execution reaches `halt` below - the
        // authoritative place to read it from (see M6502Codegen.h's own
        // comment on the 6502 having no register wide enough to hold a
        // full 16-bit result the way pdp7's AC/8086's AX do). This extra
        // `lda` is a minor nicety only: it leaves the low byte sitting in
        // the real A register too, for a quick look with nothing more
        // than a register dump.
        code_ << "    lda ac\n";
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - it must never get a local
        // storage word here, only ever be referenced by name (see
        // label()) in code, letting the assembler's own undefined-
        // symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        const std::string lbl = label(slot.get());
        if (slot->arraySize > 0) {
            declareArray(lbl, slot->arrayInit);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            uint16_t v16 = static_cast<uint16_t>(v);
            trailer_.push_back(lbl + ": db " + std::to_string(v16 & 0xFF) + "  ; " + slot->sourceName);
            trailer_.push_back("db " + std::to_string((v16 >> 8) & 0xFF));
        }
    }

    std::ostringstream out;
    out << code_.str();
    for (const auto &line : trailer_) out << line << "\n";
    return out.str();
}

// Single-file-only (see M6502Codegen.h's own header comment: no
// `--format=coff`, so `--dump-asm` is never asked to split output
// across more than one input file for this target) - the whole
// generated text is simply attributed to whichever file defines
// `main`, skipping the fine-grained code/trailer-range tracking
// ../pdp7/Pdp7Codegen.cpp/../x86_8086/I8086Codegen.cpp's own
// perFileAsm() needs for their real multi-file case.
std::map<std::string, std::string> M6502Codegen::perFileAsm() const { return perFileAsm_; }

// See M6502Codegen.h's own header comment for the whole frame
// convention this implements. `paramBytes` is exactly the size of
// the argument area the CALLER already reserved and wrote (see the
// `Call` case in genExpr()) right before this `jsr` - subtracting it
// back off the current `sp` is what makes `fp` land on argument 0's
// own cell (frameByteOffset()'s Param case: 2*paramIndex). There's no
// immediate-operand SBC on this target (M6502Assembler.cpp's own
// `immLow` table), so the subtraction is done as addition of its
// two's-complement instead - ordinary mod-65536 wraparound makes that
// exact, the same trick a real 6502 assembler's own negated-constant
// idiom relies on.
void M6502Codegen::genFunction(const ir::FunctionBody &fn) {
    curFunction_ = fn.func;
    code_ << label(fn.func) << ":\n";
    int paramBytes = 2 * static_cast<int>(fn.func->params.size());
    int localBytes = 2 * fn.func->frameSize;
    // Save the caller's own `fp` on the real hardware stack (JSR/RTS'
    // own reentrant mechanism, already untouched by any of this - see
    // M6502Codegen.h's header comment) - restored by genEpilogue().
    code_ << "    lda fp\n    pha\n    lda fp+1\n    pha\n";
    if (paramBytes > 0) {
        int negParamBytes = (0x10000 - paramBytes) & 0xFFFF;
        code_ << "    clc\n    lda sp\n    adci " << (negParamBytes & 0xFF) << "\n    sta fp\n";
        code_ << "    lda sp+1\n    adci " << ((negParamBytes >> 8) & 0xFF) << "\n    sta fp+1\n";
    } else {
        code_ << "    lda sp\n    sta fp\n    lda sp+1\n    sta fp+1\n";
    }
    if (localBytes > 0) {
        code_ << "    clc\n    lda sp\n    adci " << (localBytes & 0xFF) << "\n    sta sp\n";
        code_ << "    lda sp+1\n    adci " << ((localBytes >> 8) & 0xFF) << "\n    sta sp+1\n";
    }
    genStmt(*fn.body);
    genEpilogue();
    code_ << "\n";
}

void M6502Codegen::genEpilogue() {
    // sp = fp: Params and Locals collapse back to this call's own
    // frame base in one step (the software-fp analogue of x86's "mov
    // sp, bp" - I8086Codegen.cpp's own genFunction()).
    code_ << "    lda fp\n    sta sp\n    lda fp+1\n    sta sp+1\n";
    // Restore the caller's own fp, saved by the prologue above.
    code_ << "    pla\n    sta fp+1\n    pla\n    sta fp\n";
    code_ << "    rts\n";
}

int M6502Codegen::frameByteOffset(const ir::Slot *slot) const {
    if (slot->kind == ir::SlotKind::Param) return 2 * slot->paramIndex;
    return 2 * (static_cast<int>(curFunction_->params.size()) + (slot->frameOffset - 1));
}

void M6502Codegen::setPtrToFrameSlot(const ir::Slot *slot, int extraByteOffset) {
    int off = frameByteOffset(slot) + extraByteOffset;
    code_ << "    clc\n    lda fp\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
    code_ << "    lda fp+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
}

void M6502Codegen::loadSlotToAc(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        copy16("ac", label(slot));
        return;
    }
    setPtrToFrameSlot(slot, 0);
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::storeAcToSlot(const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        copy16(label(slot), "ac");
        return;
    }
    setPtrToFrameSlot(slot, 0);
    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
    code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
}

// See I8086Codegen.cpp's own genLocalArrayInit() for the general
// idea (every call needs its own array re-initialized, there being
// no one-time static trailer for a Local's storage any more) -
// element i's own cell is this array's own frame cell plus i*2 bytes
// (see setPtrToFrameSlot()'s own `extraByteOffset`).
void M6502Codegen::genLocalArrayInit(const ir::Slot *slot) {
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        std::string text = arrayElemText(slot->arrayInit[i]);
        code_ << "    ldai " << text << "\n    sta ac\n";
        code_ << "    ldai_hi " << text << "\n    sta ac+1\n";
        setPtrToFrameSlot(slot, 2 * static_cast<int>(i));
        code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
        code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
    }
}

void M6502Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            code_ << "    lda ac\n    ora ac+1\n";
            std::string Lthen = newLabel();
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            safeBranch("beq", s.elseStmt ? Lelse : Ldone, Lthen);
            code_ << Lthen << ":\n";
            genStmt(*s.thenStmt);
            code_ << "    jmp " << Ldone << "\n";
            if (s.elseStmt) {
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lbody = newLabel(), Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            code_ << "    lda ac\n    ora ac+1\n";
            safeBranch("beq", Lend, Lbody);
            code_ << Lbody << ":\n";
            loopLabels_.push_back({Lcont, Lend});
            genStmt(*s.loopBody);
            loopLabels_.pop_back();
            if (s.postStmt) {
                code_ << Lcont << ":\n";
                genStmt(*s.postStmt);
            }
            code_ << "    jmp " << Lstart << "\n";
            code_ << Lend << ":\n";
            return;
        }
        case ir::StmtKind::Break: {
            code_ << "    jmp " << loopLabels_.back().second << "\n";
            return;
        }
        case ir::StmtKind::Continue: {
            code_ << "    jmp " << loopLabels_.back().first << "\n";
            return;
        }
        case ir::StmtKind::Return: {
            if (s.initOrReturnOrExpr) genExpr(*s.initOrReturnOrExpr);
            // Early exit - genFunction()'s own fall-through epilogue
            // never runs for this path, so it needs its own frame
            // teardown inline here too (see genEpilogue()'s own
            // comment for what each instruction does).
            genEpilogue();
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                storeAcToSlot(s.declSlot);
            }
            // No "else" case for a bare, uninitialized scalar: its
            // own frame cell already exists (genFunction()'s own `sp`
            // bump reserved it) - same as a real, ordinary C
            // automatic variable, its value is simply whatever was
            // already in that frame cell until something assigns it.
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

void M6502Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
            code_ << "    ldai " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotToAc(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                // Save `ptr` across evaluating rhs (which might
                // itself index through `ptr` again) on the real
                // hardware stack - the exact role
                // ../x86_8086/I8086Codegen.cpp's own "push bx"/"pop
                // bx" plays around the same spot.
                code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
                genExpr(*e.rhs);
                code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
                code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                return;
            }
            genExpr(*e.rhs);
            storeAcToSlot(e.slot);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
            genExpr(*e.rhs);
            code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
            code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
            return;
        }
        case ir::ExprKind::Unary: {
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    ldai 0\n    sec\n    sbc ac\n    sta ac\n";
                code_ << "    ldai 0\n    sbc ac+1\n    sta ac+1\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
                code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    lda ac\n    eori 255\n    sta ac\n";
                code_ << "    lda ac+1\n    eori 255\n    sta ac+1\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Every argument is evaluated first and parked on the
            // real hardware stack (low byte then high, same
            // convention copy16()'s own callers use elsewhere) - this
            // has to finish, with no further `sp`/`aBase` use, before
            // a nested call inside a LATER argument's own expression
            // gets a chance to invalidate an EARLIER argument's own
            // computed address (see M6502Codegen.h's header comment
            // on this whole call sequence).
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
            }
            if (!e.args.empty()) {
                // aBase = sp (this call's own argument area, which
                // becomes the callee's own `fp` once its prologue
                // subtracts this same size back off - see
                // genFunction()), then reserve it.
                code_ << "    lda sp\n    sta aBase\n    lda sp+1\n    sta aBase+1\n";
                int paramBytes = 2 * static_cast<int>(e.args.size());
                code_ << "    clc\n    lda sp\n    adci " << (paramBytes & 0xFF) << "\n    sta sp\n";
                code_ << "    lda sp+1\n    adci " << ((paramBytes >> 8) & 0xFF) << "\n    sta sp+1\n";
                // Unpark each value (LIFO - the last-pushed, highest-
                // indexed argument comes off first) into its own cell,
                // aBase+2*i.
                for (size_t i = e.args.size(); i-- > 0;) {
                    code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";
                    int off = 2 * static_cast<int>(i);
                    code_ << "    clc\n    lda aBase\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
                    code_ << "    lda aBase+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
                    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                    code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): there's no per-callee
            // p_<fn>_<i> cell to write one into until the callee is
            // known, which it never is at compile time for this kind.
            //
            // The 6502 has no indirect-call instruction - only `jmp
            // (addr)` ("jmpi" in this dialect, M6502Assembler.cpp's own
            // encoding of real opcode $6C, indirect JMP). A call
            // through a computed address is built by hand out of that
            // and a real JSR's own return-address convention: JSR
            // pushes (return address - 1), high byte then low byte,
            // and RTS pulls low then high and adds 1 back before
            // jumping. Pushing (Lret-1) the same way, then jumping
            // indirectly through `ptr` (the fixed zero-page cell
            // holding the callee's own entry address, set just below),
            // leaves the real hardware stack in exactly the state a
            // `jsr` to the same callee would have, so the callee's own
            // `rts` returns here, to the instruction right after
            // "jmpi ptr", the same as any other call.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            copy16("ptr", "ac");
            std::string Lret = newLabel();
            code_ << "    ldai_hi " << Lret << "-1\n    pha\n";
            code_ << "    ldai " << Lret << "-1\n    pha\n";
            code_ << "    jmpi ptr\n";
            code_ << Lret << ":\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // Real 6502 interrupt enable/disable - SEI sets the
            // interrupt-disable flag (iof), CLI clears it (ion); same
            // real hardware feature as the 8086 backend's sti/cli
            // (see I8086Codegen.cpp), just the opposite mnemonic-to-
            // meaning mapping.
            code_ << "    " << (e.op == "ion" ? "cli" : "sei") << "\n";
            return;
        }
    }
}

// See M6502Codegen.h's own header comment on this being the one
// thing that ever needs a real zero-page address. `slot->arraySize >
// 0` means the base is a compile-time-known label (an *address*, so
// `ldai`/`ldai_hi` applied to that label's own name split it into
// its low/high byte at assemble time - see
// M6502Assembler.cpp's emitImmLow/High's own comment); otherwise
// `slot` is a plain pointer variable, so the base instead comes from
// *reading* that variable's own stored value (`adc <label>`/`adc
// <label>+1`, an ordinary absolute-memory operand, not an immediate).
void M6502Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ac *= 2 (words are 2 bytes on this byte-addressed target, same
    // reason as ../x86_8086/I8086Codegen.cpp's own `shl ax,1`).
    code_ << "    asl ac\n    rol ac+1\n";
    if (slot->kind == ir::SlotKind::Global) {
        std::string lbl = label(slot);
        code_ << "    clc\n";
        if (slot->arraySize > 0) {
            code_ << "    lda ac\n    adci " << lbl << "\n    sta ptr\n";
            code_ << "    lda ac+1\n    adci_hi " << lbl << "\n    sta ptr+1\n";
        } else {
            code_ << "    lda ac\n    adc " << lbl << "\n    sta ptr\n";
            code_ << "    lda ac+1\n    adc " << lbl << "+1\n    sta ptr+1\n";
        }
        return;
    }
    // Local/Param: this slot's own cell lives at fp+frameByteOffset()
    // in THIS call's own frame (every activation gets its own `fp`) -
    // set into `ptr` first (see setPtrToFrameSlot()), since `ac`
    // already holds the scaled index.
    setPtrToFrameSlot(slot, 0);
    if (slot->arraySize > 0) {
        // `ptr` IS the array's own first word - base-plus-scaled-
        // index (ascending layout - see M6502Codegen.h's own header
        // comment on why this never needs a base-MINUS-index form).
        // `ptr`'s own low byte is read before being overwritten by
        // the very same `sta`, same as any other accumulator-based
        // read-modify-write sequence.
        code_ << "    clc\n    lda ac\n    adc ptr\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc ptr+1\n    sta ptr+1\n";
    } else {
        // slot is a plain pointer VARIABLE stored at that cell - read
        // its own value first (through `ptr`, which still holds the
        // cell's own address at this point), then add the index to
        // that value instead of to the cell's address.
        code_ << "    ldyi 0\n    ldaind ptr\n    sta bc\n";
        code_ << "    ldyi 1\n    ldaind ptr\n    sta bc+1\n";
        code_ << "    clc\n    lda ac\n    adc bc\n    sta ptr\n";
        code_ << "    lda ac+1\n    adc bc+1\n    sta ptr+1\n";
    }
}

void M6502Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            std::string lbl = label(operand.slot);
            code_ << "    ldai " << lbl << "\n    sta ac\n";
            code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        } else {
            setPtrToFrameSlot(operand.slot, 0);
            copy16("ac", "ptr");
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        std::string lbl = label(operand.func);
        code_ << "    ldai " << lbl << "\n    sta ac\n";
        code_ << "    ldai_hi " << lbl << "\n    sta ac+1\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void M6502Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
        if (op == "+") {
            code_ << "    clc\n    lda bc\n    adc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    adc ac+1\n    sta ac+1\n";
        } else {
            code_ << "    sec\n    lda bc\n    sbc ac\n    sta ac\n";
            code_ << "    lda bc+1\n    sbc ac+1\n    sta ac+1\n";
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
        code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
        genExpr(*e.rhs);
        code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
        const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
        code_ << "    lda bc\n    " << insn << " ac\n    sta ac\n";
        code_ << "    lda bc+1\n    " << insn << " ac+1\n    sta ac+1\n";
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

// No variable-count shift instruction (ASL/LSR always shift exactly
// one bit) - loop `rhs` times instead, the same fix
// ../pdp7/Pdp7Codegen.cpp's own `<<`/`>>` already needed for the
// same reason (see M6502Codegen.h's own header comment).
void M6502Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    lda ac\n    sta cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
    copy16("ac", "bc");  // ac = value to shift
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl ac\n    rol ac+1\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with in
        // the first place), as every other backend's own `>>`.
        code_ << "    lsr ac+1\n    ror ac\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (no hardware multiply - see
// M6502Codegen.h's own header comment): `bc` is the multiplicand
// (shifted left each iteration), `dx` the multiplier (shifted right,
// consumed one bit at a time into `cx`'s own loop), `ac` the
// accumulating product. Only verified against non-negative operands.
void M6502Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("dx", "ac");  // dx = multiplier
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = multiplicand
    code_ << "    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";  // ac = product = 0
    code_ << "    ldai 16\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Ladd = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    code_ << "    lda dx\n    andi 1\n";
    safeBranch("bne", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    clc\n    lda ac\n    adc bc\n    sta ac\n";
    code_ << "    lda ac+1\n    adc bc+1\n    sta ac+1\n";
    code_ << Lskipadd << ":\n";
    code_ << "    asl bc\n    rol bc+1\n";
    code_ << "    lsr dx+1\n    ror dx\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Repeated subtraction (no hardware divide - see M6502Codegen.h's own
// header comment) - the same algorithm, for the same reason,
// ../pdp7/Pdp7Codegen.cpp's own ensureDivModRoutine() already uses:
// `dx` is the running remainder (starts as the dividend, `bc` the
// fixed divisor), `ac` the quotient, counting how many times `bc`
// was subtracted out of `dx` before it would've gone negative. Only
// verified against non-negative operands.
void M6502Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pla\n    sta dx+1\n    pla\n    sta dx\n";  // dx = dividend (running remainder)
    code_ << "    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";  // ac = quotient = 0
    std::string Lloop = newLabel(), Lcommit = newLabel(), Llt = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentatively compute dx - bc; carry set afterward means no
    // borrow occurred, i.e. dx >= bc. The low byte's tentative result
    // is stashed on the stack (A is needed for the high byte's own
    // subtraction next) until it's known whether to actually commit.
    code_ << "    sec\n    lda dx\n    sbc bc\n    pha\n";
    code_ << "    lda dx+1\n    sbc bc+1\n";
    safeBranch("bcs", Lcommit, Llt);
    code_ << Lcommit << ":\n";
    code_ << "    sta dx+1\n    pla\n    sta dx\n";
    inc16("ac");
    code_ << "    jmp " << Lloop << "\n";
    code_ << Llt << ":\n";
    code_ << "    pla\n";  // discard the stashed tentative low byte - this iteration's subtraction is rejected
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ldone << ":\n";
    if (wantRemainder) copy16("ac", "dx");
}

void M6502Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to one "is a < b?" primitive
    // (emitLessCompare) by choosing which operand goes first and
    // whether to invert the final sense: a>b is b<a; a<=b is !(b<a);
    // a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b, checked the same
// "OR the two result bytes together" way genLogicalNot() checks `ac`
// for zero - `dx` holds the low byte's tentative result while the
// high byte's own subtraction needs A.
void M6502Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = lhs
    code_ << "    sec\n    lda bc\n    sbc ac\n    sta dx\n";
    code_ << "    lda bc+1\n    sbc ac+1\n    ora dx\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via subtraction, the standard 6502 multi-byte
// signed-compare idiom: after `a - b`'s final (high) byte SBC, the
// sign bit (N) only reflects the *true* signed result once corrected
// for overflow (V) - if V is set, the apparent sign is backwards, so
// `EOR #$80` (flipping just the sign bit, which also recomputes N/Z
// for the corrected value) fixes it up. This is exactly the "N xor V"
// rule x86's SF/OF flags encode as a single combined test (see
// ../x86_8086/I8086Codegen.cpp's own signed `cmp`+`jl`/`jle`/etc.) -
// the 6502 has both flags too, just no one instruction to test their
// XOR directly, hence the explicit fixup.
void M6502Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert) {
    genExpr(a);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(b);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = a
    code_ << "    sec\n    lda bc\n    sbc ac\n    lda bc+1\n    sbc ac+1\n";
    std::string Lfixskip = newLabel();
    code_ << "    bvc " << Lfixskip << "\n";
    code_ << "    eori 128\n";
    code_ << Lfixskip << ":\n";
    // N now correctly reflects "a < b" (BMI) / "a >= b" (BPL).
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("beq", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("bne", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    code_ << "    lda ac\n    ora ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    safeBranch("beq", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

std::string M6502Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                 const std::string &) const {
    throw std::runtime_error("the 6502 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> M6502Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleM6502(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see M6502Assembler.h's own `assembleM6502Object`), and
    // whichever of these names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it - this file never
    // touches a real COFF/ELF/Mach-O/a.out byte layout itself.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleM6502Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
