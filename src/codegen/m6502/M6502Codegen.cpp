#include "M6502Codegen.h"

#include <algorithm>
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

// A symbol element (a dispatch-table's own function/global address)
// is always native-int width (2 bytes - see IR.h's own ArrayInitElem
// comment); only a plain literal element can be `char`-width (1
// byte) or wide (`long`/`long long`, 4 or 8 bytes). A `char` element
// is a single plain `db` (no low/high split needed - there's nothing
// to split). A wide element has no "dblo"/"dbhi" counterpart at all
// (this dialect's own data directives only ever split a 16-bit
// value) - its own literal value is instead split into `w` plain
// bytes here directly, least significant first, matching every wide
// scalar's own little-endian storage layout (see generate()'s own
// Global-scalar case).
void M6502Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                 Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    bool first = true;
    auto emit = [&](const std::string &text) {
        trailer_.push_back((first ? lbl + ": " : "") + text);
        first = false;
    };
    for (const auto &e : values) {
        if (w == 1) {
            emit("db " + std::to_string(static_cast<int>(static_cast<uint8_t>(e.value))));
        } else if (w == 2) {
            std::string text = arrayElemText(e);
            emit("dblo " + text);
            trailer_.push_back("dbhi " + text);
        } else {
            auto uv = static_cast<unsigned long>(e.value);
            for (int b = 0; b < w; b++) {
                emit("db " + std::to_string(static_cast<int>((uv >> (8 * b)) & 0xFF)));
            }
        }
    }
}

void M6502Codegen::copy16(const std::string &dst, const std::string &src) {
    code_ << "    lda " << src << "\n    sta " << dst << "\n";
    code_ << "    lda " << src << "+1\n    sta " << dst << "+1\n";
}

namespace {
// `base+i`'s own operand text (just `base` for byte 0, matching
// every existing "ac"/"ac+1"-style reference elsewhere in this file).
std::string byteAt(const char *base, int i) { return i == 0 ? base : (std::string(base) + "+" + std::to_string(i)); }
}  // namespace

// Pushes `base`'s own `nbytes` bytes onto the real hardware stack,
// ascending (byte 0 first) - the last one pushed (byte `nbytes-1`,
// the most significant) ends up on top, the direct generalization of
// every existing 2-byte "lda ac\npha\nlda ac+1\npha" idiom already in
// this file (push byte 0 then byte 1 - top of stack is the high
// byte) to an arbitrary wide-value byte count.
void M6502Codegen::pushWide(const char *base, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(base, i) << "\n    pha\n";
}

// Pops `nbytes` bytes back off the real hardware stack into `base`,
// descending (byte `nbytes-1` - the most significant, sitting on
// top - comes off first) - the exact mirror of pushWide() above, and
// the generalization of every existing "pla\nsta bc+1\npla\nsta bc"
// idiom.
void M6502Codegen::popWide(const char *base, int nbytes) {
    for (int i = nbytes - 1; i >= 0; i--) code_ << "    pla\n    sta " << byteAt(base, i) << "\n";
}

void M6502Codegen::copyWide(const char *dst, const char *src, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(src, i) << "\n    sta " << byteAt(dst, i) << "\n";
}

void M6502Codegen::negateWideBuffer(const char *buf, int nbytes) {
    for (int i = 0; i < nbytes; i++) code_ << "    lda " << byteAt(buf, i) << "\n    eori 255\n    sta "
                                            << byteAt(buf, i) << "\n";
    code_ << "    clc\n    lda " << byteAt(buf, 0) << "\n    adci 1\n    sta " << byteAt(buf, 0) << "\n";
    for (int i = 1; i < nbytes; i++) {
        code_ << "    lda " << byteAt(buf, i) << "\n    adci 0\n    sta " << byteAt(buf, i) << "\n";
    }
}

void M6502Codegen::safeBranch(const std::string &branchMnemonic, const std::string &targetIfTaken,
                               const std::string &targetIfNot) {
    std::string Lnear = newLabel();
    code_ << "    " << branchMnemonic << " " << Lnear << "\n";
    code_ << "    jmp " << targetIfNot << "\n";
    code_ << Lnear << ":\n";
    code_ << "    jmp " << targetIfTaken << "\n";
}

// Word `byteOffset/2`'s own label text (a Global's own `label+N`, 0
// omitted the same way every other `+N`-style operand here is) -
// only ever used for a Global's own wide storage; a Local/Param's
// wide words are reached through `ptr`/Y instead (see
// genWideLoad()/genWideStore()).
std::string M6502Codegen::wideWordLabel(const ir::Slot *slot, int byteOffset) const {
    std::string lbl = label(slot);
    return byteOffset == 0 ? lbl : (lbl + "+" + std::to_string(byteOffset));
}

void M6502Codegen::genWideLoad(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << wideWordLabel(slot, i) << "\n    sta " << byteAt("wac", i) << "\n";
        }
        return;
    }
    setPtrToFrameSlot(slot, 0);
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    ldaind ptr\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldai " << ((uv >> (8 * i)) & 0xFF) << "\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideStore(const ir::Slot *slot, int words) {
    int n = 2 * words;
    if (slot->kind == ir::SlotKind::Global) {
        for (int i = 0; i < n; i++) {
            code_ << "    lda " << byteAt("wac", i) << "\n    sta " << wideWordLabel(slot, i) << "\n";
        }
        return;
    }
    setPtrToFrameSlot(slot, 0);
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    lda " << byteAt("wac", i) << "\n    staind ptr\n";
    }
}

void M6502Codegen::genWideLoadAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    ldaind ptr\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideStoreAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    ldyi " << i << "\n    lda " << byteAt("wac", i) << "\n    staind ptr\n";
    }
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
    // `wac`/`wbc`/`wtmp`/`wtmp2` - the wide (`long`/`long long`) value
    // buffers (see M6502Codegen.h's own header comment on this whole
    // mechanism), 8 bytes each (wide enough for a `long long`), bound
    // the same fixed-equate way as every other pseudo-register above.
    code_ << "ptr = 0\nac = 2\nbc = 4\ncx = 6\ndx = 8\nfp = 10\nsp = 12\naBase = 14\n";
    code_ << "wac = 16\nwbc = 24\nwtmp = 32\nwtmp2 = 40\n\n";
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
            declareArray(lbl, slot->arrayInit, slot->type);
        } else {
            long v = slot->hasInit ? slot->initValue : 0;
            auto uv = static_cast<unsigned long>(v);
            int w = widthOfType(slot->type);
            trailer_.push_back(lbl + ": db " + std::to_string(uv & 0xFF) + "  ; " + slot->sourceName);
            for (int i = 1; i < w; i++) trailer_.push_back("db " + std::to_string((uv >> (8 * i)) & 0xFF));
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
    // Every param reserves at least one native `int`'s width (2
    // bytes - IRBuilder's own convention, see IR.h's own
    // Slot::frameOffset comment), a wider type (`long`/`long long`)
    // reserves its own real width instead - the sum is exactly the
    // caller's own argument-area size (see the `Call` case in
    // genExpr()).
    int paramBytes = 0;
    for (const ir::Slot *p : fn.func->params) paramBytes += std::max(widthOfType(p->type), 2);
    int localBytes = fn.func->frameSize;
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
    if (slot->kind == ir::SlotKind::Param) {
        // IRBuilder's own Slot::frameOffset for a Param is counted
        // past a fixed two-native-`int`-widths reservation (see
        // IR.h's own comment) for whatever a backend's own call
        // sequence keeps there - this backend's own `fp` lands
        // exactly on the first argument instead (see genFunction()'s
        // own header comment), so that reservation is subtracted
        // back off here to get this backend's own, zero-based offset.
        return slot->frameOffset - 2 * widthOfType(Type{BaseType::Int, false});
    }
    // The locals region starts right after the params region (see
    // genFunction()'s own `paramBytes`), so the first local's own
    // frameOffset (one native int's width, by IRBuilder's own
    // convention) has to land right there, not one int's width
    // further on.
    int paramBytes = 0;
    for (const ir::Slot *p : curFunction_->params) paramBytes += std::max(widthOfType(p->type), 2);
    return paramBytes + slot->frameOffset - widthOfType(Type{BaseType::Int, false});
}

void M6502Codegen::setPtrToFrameSlot(const ir::Slot *slot, int extraByteOffset) {
    int off = frameByteOffset(slot) + extraByteOffset;
    code_ << "    clc\n    lda fp\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
    code_ << "    lda fp+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
}

void M6502Codegen::loadSlotToAc(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    if (w == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda " << label(slot) << "\n    sta ac\n";
        } else {
            setPtrToFrameSlot(slot, 0);
            code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        }
        if (slot->type.isUnsigned) {
            code_ << "    ldai 0\n    sta ac+1\n";
            return;
        }
        // Sign-extend ac's own top bit into ac+1 - there's no direct
        // "test bit 7" instruction in this dialect (no BIT/CMP - see
        // M6502Assembler.cpp's own encodeInstr), so the sign bit is
        // extracted into the carry flag via ASL on a throwaway copy
        // (cx - free here, not yet live for anything else) instead.
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
    if (slot->kind == ir::SlotKind::Global) {
        copy16("ac", label(slot));
        return;
    }
    setPtrToFrameSlot(slot, 0);
    code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
    code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
}

void M6502Codegen::storeAcToSlot(const ir::Slot *slot) {
    if (widthOfType(slot->type) == 1) {
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    lda ac\n    sta " << label(slot) << "\n";
        } else {
            setPtrToFrameSlot(slot, 0);
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
        }
        return;
    }
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
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        setPtrToFrameSlot(slot, w * static_cast<int>(i));
        if (w <= 2) {
            std::string text = arrayElemText(elem);
            code_ << "    ldai " << text << "\n    sta ac\n";
            code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
            if (w == 2) {
                code_ << "    ldai_hi " << text << "\n    sta ac+1\n";
                code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
            }
        } else {
            // A wide element's own literal is never a symbol (see
            // declareArray()'s own comment) - split directly into
            // `w` plain bytes, least significant first.
            auto uv = static_cast<unsigned long>(elem.value);
            for (int b = 0; b < w; b++) {
                code_ << "    ldai " << ((uv >> (8 * b)) & 0xFF) << "\n    sta ac\n";
                code_ << "    ldyi " << b << "\n    lda ac\n    staind ptr\n";
            }
        }
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
            testTruthy(*s.cond);
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
            testTruthy(*s.cond);
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
                if (isWide(s.declSlot->type)) {
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                } else {
                    storeAcToSlot(s.declSlot);
                }
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
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            uint16_t v16 = static_cast<uint16_t>(e.intValue);
            code_ << "    ldai " << (v16 & 0xFF) << "\n    sta ac\n";
            code_ << "    ldai " << ((v16 >> 8) & 0xFF) << "\n    sta ac+1\n";
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            loadSlotToAc(e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                int w = widthOfType(e.slot->type);
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                // Save `ptr` across evaluating rhs (which might
                // itself index through `ptr` again) on the real
                // hardware stack - the exact role
                // ../x86_8086/I8086Codegen.cpp's own "push bx"/"pop
                // bx" plays around the same spot.
                code_ << "    lda ptr\n    pha\n    lda ptr+1\n    pha\n";
                genExpr(*e.rhs);
                code_ << "    pla\n    sta ptr+1\n    pla\n    sta ptr\n";
                if (w > 2) {
                    genWideStoreAtPtr(wordCountOf(e.slot->type));
                } else {
                    code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                    if (w == 2) code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                }
                return;
            }
            genExpr(*e.rhs);
            if (isWide(e.slot->type)) {
                genWideStore(e.slot, wordCountOf(e.slot->type));
            } else {
                storeAcToSlot(e.slot);
            }
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
            bool wide = isWide(e.rhs->type);
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (wide) {
                    negateWideBuffer("wac", 2 * wordCountOf(e.rhs->type));
                } else {
                    code_ << "    ldai 0\n    sec\n    sbc ac\n    sta ac\n";
                    code_ << "    ldai 0\n    sbc ac+1\n    sta ac+1\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
                code_ << "    ldyi 1\n    ldaind ptr\n    sta ac+1\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (wide) {
                    int n = 2 * wordCountOf(e.rhs->type);
                    for (int i = 0; i < n; i++) {
                        std::string b = i == 0 ? "wac" : ("wac+" + std::to_string(i));
                        code_ << "    lda " << b << "\n    eori 255\n    sta " << b << "\n";
                    }
                } else {
                    code_ << "    lda ac\n    eori 255\n    sta ac\n";
                    code_ << "    lda ac+1\n    eori 255\n    sta ac+1\n";
                }
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
            // Every argument reserves at least one native int's width
            // (2 bytes); a wider one (`long`/`long long`, already
            // converted to its own param's real type by IRBuilder's
            // own call-argument conversion) reserves its own real
            // width instead - the exact mirror of genFunction()'s own
            // `paramBytes` on the callee side.
            std::vector<int> argWidths(e.args.size());
            int totalBytes = 0;
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                int w = std::max(widthOfType(e.args[i]->type), 2);
                argWidths[i] = w;
                totalBytes += w;
                if (w > 2) {
                    pushWide("wac", w);
                } else {
                    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
                }
            }
            if (!e.args.empty()) {
                // aBase = sp (this call's own argument area, which
                // becomes the callee's own `fp` once its prologue
                // subtracts this same size back off - see
                // genFunction()), then reserve it.
                code_ << "    lda sp\n    sta aBase\n    lda sp+1\n    sta aBase+1\n";
                code_ << "    clc\n    lda sp\n    adci " << (totalBytes & 0xFF) << "\n    sta sp\n";
                code_ << "    lda sp+1\n    adci " << ((totalBytes >> 8) & 0xFF) << "\n    sta sp+1\n";
                std::vector<int> offsets(e.args.size());
                int running = 0;
                for (size_t i = 0; i < e.args.size(); i++) {
                    offsets[i] = running;
                    running += argWidths[i];
                }
                // Unpark each value (LIFO - the last-pushed, highest-
                // indexed argument comes off first) into its own
                // cell, aBase+offsets[i].
                for (size_t i = e.args.size(); i-- > 0;) {
                    int w = argWidths[i];
                    if (w > 2) {
                        popWide("wac", w);
                    } else {
                        code_ << "    pla\n    sta ac+1\n    pla\n    sta ac\n";
                    }
                    int off = offsets[i];
                    code_ << "    clc\n    lda aBase\n    adci " << (off & 0xFF) << "\n    sta ptr\n";
                    code_ << "    lda aBase+1\n    adci " << ((off >> 8) & 0xFF) << "\n    sta ptr+1\n";
                    if (w > 2) {
                        genWideStoreAtPtr(w / 2);
                    } else {
                        code_ << "    ldyi 0\n    lda ac\n    staind ptr\n";
                        code_ << "    ldyi 1\n    lda ac+1\n    staind ptr\n";
                    }
                }
            }
            code_ << "    jsr " << label(e.func) << "\n";
            // A wide return value is simply wherever the callee's own
            // `return` expression last left it (`wac`, a fixed
            // buffer untouched by genEpilogue() - see
            // M6502Codegen.h's own header comment) - nothing further
            // to do here, unlike a register-based convention that
            // would need saving across the callee's own epilogue.
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
        case ir::ExprKind::Convert: {
            genConvert(e);
            return;
        }
        case ir::ExprKind::Func:
            err(e.line, "internal: Func reached genExpr - should only ever appear as '&'s operand");
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
    // ac *= widthOf(slot->type) (a power of two - see elemShift()),
    // same reason as ../x86_8086/I8086Codegen.cpp's own `shl ax,N`.
    int shift = elemShift(slot->type);
    for (int i = 0; i < shift; i++) code_ << "    asl ac\n    rol ac+1\n";
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
    int w = widthOfType(e.slot->type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(e.slot->type));
        return;
    }
    if (w == 1) {
        code_ << "    ldyi 0\n    ldaind ptr\n    sta ac\n";
        if (e.slot->type.isUnsigned) {
            code_ << "    ldai 0\n    sta ac+1\n";
            return;
        }
        code_ << "    lda ac\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta ac+1\n";
        safeBranch("bcs", Lneg, Ldone);
        code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
        code_ << Ldone << ":\n";
        return;
    }
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

// An implicit or explicit type conversion (see IR.h's own ExprKind::
// Convert comment). Every arithmetic/comparison operand narrower
// than `int` is already promoted by IRBuilder before any Binary/
// Unary ever sees it, so the real work here is narrowing/widening a
// `char` (mask or sign-extend its own top byte) and crossing the
// native-word/`long`-or-`long long` boundary (padding `wac` with
// more sign/zero-extension bytes, or just leaving the low bytes
// already there alone when shrinking).
void M6502Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);  // ac
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldai 0\n    sta ac+1\n";
            } else {
                code_ << "    lda ac\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta ac+1\n";
                safeBranch("bcs", Lneg, Ldone);
                code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
                code_ << Ldone << ":\n";
            }
        }
        return;
    }
    if (fromWide && !toWide) {
        genExpr(*e.lhs);  // wac holds `wordCountOf(from)` words
        code_ << "    lda " << byteAt("wac", 0) << "\n    sta ac\n";
        code_ << "    lda " << byteAt("wac", 1) << "\n    sta ac+1\n";
        if (widthOfType(to) == 1) {
            if (to.isUnsigned) {
                code_ << "    ldai 0\n    sta ac+1\n";
            } else {
                code_ << "    lda ac\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta ac+1\n";
                safeBranch("bcs", Lneg, Ldone);
                code_ << Lneg << ":\n    ldai 255\n    sta ac+1\n";
                code_ << Ldone << ":\n";
            }
        }
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);  // ac, already correctly sign/zero-extended to 16 bits
        code_ << "    lda ac\n    sta " << byteAt("wac", 0) << "\n";
        code_ << "    lda ac+1\n    sta " << byteAt("wac", 1) << "\n";
        if (toWords > 1) {
            // Sign/zero-extend ac's own top byte into every
            // remaining byte, per the SOURCE's own signedness.
            if (from.isUnsigned) {
                code_ << "    ldai 0\n    sta dx\n";
            } else {
                code_ << "    lda ac+1\n    sta cx\n    asl cx\n";
                std::string Lneg = newLabel(), Ldone = newLabel();
                code_ << "    ldai 0\n    sta dx\n";
                safeBranch("bcc", Ldone, Lneg);
                code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
                code_ << Ldone << ":\n";
            }
            for (int i = 2; i < 2 * toWords; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
        }
        return;
    }
    // fromWide && toWide: pad/truncate word count, preserving sign.
    int fromN = 2 * wordCountOf(from);
    int toN = 2 * wordCountOf(to);
    genExpr(*e.lhs);  // wac holds `fromN` bytes
    if (toN > fromN) {
        if (from.isUnsigned) {
            code_ << "    ldai 0\n    sta dx\n";
        } else {
            code_ << "    lda " << byteAt("wac", fromN - 1) << "\n    sta cx\n    asl cx\n";
            std::string Lneg = newLabel(), Ldone = newLabel();
            code_ << "    ldai 0\n    sta dx\n";
            safeBranch("bcc", Ldone, Lneg);
            code_ << Lneg << ":\n    ldai 255\n    sta dx\n";
            code_ << Ldone << ":\n";
        }
        for (int i = fromN; i < toN; i++) code_ << "    lda dx\n    sta " << byteAt("wac", i) << "\n";
    }
    // toN < fromN: the low `toN` bytes already sitting in `wac` are
    // exactly this type's own truncated value - nothing further to do.
}

// Pushes `lhs`'s own `words`-word value onto the real hardware stack
// across evaluating `rhs` (see M6502Codegen.h's own header comment on
// this convention), leaving wbc=lhs, wac=rhs, then combines the two
// byte by byte with a single carry chain (CLC/SEC once, up through
// every byte - ADC/SBC each fold in the previous byte's own carry
// automatically), writing the result back into `wac`.
void M6502Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    code_ << "    " << (isAdd ? "clc" : "sec") << "\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    " << (isAdd ? "adc" : "sbc") << " " << byteAt("wac", i)
              << "\n    sta " << byteAt("wac", i) << "\n";
    }
}

void M6502Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    const char *insn = (op == "&") ? "and" : (op == "|") ? "ora" : "eor";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    " << insn << " " << byteAt("wac", i) << "\n    sta "
              << byteAt("wac", i) << "\n";
    }
}

// Scans bytes most-significant first, deciding "lhs < rhs" (or
// `==`/`!=`) from the first byte that differs - the byte-level
// generalization of every narrow backend's own multi-byte compare,
// just with "word" replaced by "byte" throughout (see
// Pdp11Codegen.cpp's own genWideCompare() for the word-level version
// of this exact same idea).
void M6502Codegen::emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    code_ << "    lda " << byteAt("wbc", 0) << "\n    eor " << byteAt("wac", 0) << "\n    sta dx\n";
    for (int i = 1; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    eor " << byteAt("wac", i) << "\n    ora dx\n    sta dx\n";
    }
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    lda dx\n";
    safeBranch(wantEqual ? "beq" : "bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// A single chained SBC from the least to the most significant byte
// (one `sec`, then one `sbc` per byte - the carry/borrow propagates
// automatically) computes the full `words`-word subtraction in one
// pass; the final (most significant) byte's own flags alone decide
// the whole comparison (same N-xor-V fixup as the narrow
// emitLessCompare(), just applied once, after every byte, instead of
// after 16 bits) - no separate "did an earlier byte already differ"
// scan is needed, the same reason a 32-bit x86 `cmp` correctly
// decides a signed 32-bit "<" in one shot instead of scanning bytes.
void M6502Codegen::emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words,
                                        bool invert) {
    int n = 2 * words;
    genExpr(a);
    pushWide("wac", n);
    genExpr(b);
    popWide("wbc", n);  // wbc = a, wac = b
    code_ << "    sec\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wbc", i) << "\n    sbc " << byteAt("wac", i) << "\n";
    }
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n    eori 128\n" << Lfixskip << ":\n";
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                   int words) {
    if (op == "==") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/true);
    if (op == "!=") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/false);
    if (op == "<") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/false);
    if (op == ">") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/false);
    if (op == "<=") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/true);
    if (op == ">=") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// A `long`/`long long` shifted by a runtime count - no hardware shift
// here is wider than one bit on one byte, so this loops one bit at a
// time (the wide generalization of genShift()'s own narrow version),
// chaining the carry across all `n` bytes via ASL/ROL (left) or
// LSR/ROR (right, processed highest byte first so the carry flows
// downward) each iteration. A signed right shift re-derives the
// current sign bit into the carry before the top byte's own ROR,
// instead of LSR's fixed 0-fill - the same trick genShift() uses,
// just needing its own throwaway copy (`dx`, free here: `cx` already
// holds the loop's own shift count) since every byte of `wac` is live.
void M6502Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);  // ac = shift count (already promoted to a plain int)
    code_ << "    lda ac\n    sta cx\n";
    popWide("wac", n);
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl " << byteAt("wac", 0) << "\n";
        for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wac", i) << "\n";
    } else if (isUnsigned) {
        code_ << "    lsr " << byteAt("wac", n - 1) << "\n";
        for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    } else {
        code_ << "    lda " << byteAt("wac", n - 1) << "\n    sta dx\n    asl dx\n";
        code_ << "    ror " << byteAt("wac", n - 1) << "\n";
        for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    }
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply (the wide generalization of
// genMul() - see M6502Codegen.h's own header comment on why there's
// no hardware multiply at all), `n*8` bits serially: `wbc` is the
// multiplicand (shifted left each iteration, truncated mod 2^(8n) -
// exactly matching this language's own truncating multiply
// semantics), `wac` the multiplier (shifted right, consumed one bit
// at a time), `wtmp` the accumulating product. Needs no signedness
// at all - a truncated product's own bit pattern is identical
// whether the operands are interpreted as signed or unsigned (the
// same reason plain 16-bit `*` never branches on it either).
void M6502Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = multiplicand, wac = multiplier
    for (int i = 0; i < n; i++) code_ << "    ldai 0\n    sta " << byteAt("wtmp", i) << "\n";
    code_ << "    ldai " << (8 * n) << "\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Ladd = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    code_ << "    lda " << byteAt("wac", 0) << "\n    andi 1\n";
    safeBranch("bne", Ladd, Lskipadd);
    code_ << Ladd << ":\n";
    code_ << "    clc\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    adc " << byteAt("wbc", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    code_ << Lskipadd << ":\n";
    code_ << "    asl " << byteAt("wbc", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wbc", i) << "\n";
    code_ << "    lsr " << byteAt("wac", n - 1) << "\n";
    for (int i = n - 2; i >= 0; i--) code_ << "    ror " << byteAt("wac", i) << "\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copyWide("wac", "wtmp", n);
}

// Binary long division, `n*8` bits serially: `wbc` (consumed from its
// own most significant bit) and `wtmp` (the running remainder,
// starting at 0) together act as one combined `2n`-byte shift
// register - each iteration shifts the whole pair left by one bit
// (wbc's own outgoing top bit becomes wtmp's own new bottom bit, via
// one continuous carry chain across both), then tries `wtmp -= wac`
// (the fixed divisor): if that leaves no borrow, the bit just
// shifted into the quotient (`wtmp2`) is 1 and the subtraction
// stands; otherwise it's 0 and `wac` is added straight back (always
// exactly restoring `wtmp` to what it was before the trial
// subtraction, borrow or not - see M6502Codegen.cpp's own header
// comment, or Pdp11Codegen.cpp's own non-restoring-division comment
// for the general technique). Repeated SUBTRACTION (the plain
// 16-bit genDivMod()'s own algorithm) would need up to 2^(8n)
// iterations - entirely infeasible once n reaches 4/8 bytes - so
// this needs the real bit-serial algorithm instead, not just a wider
// copy of that loop.
//
// Signed division/modulo wraps this same unsigned core: negate
// either operand's own magnitude first if it started negative
// (recording each original sign as a 0/1 flag in `dx`/`dx+1` - free
// here, this routine's own loop count lives in `cx`), then fix the
// quotient's sign (negative iff the two original signs differed) and
// the remainder's (always the dividend's own original sign)
// afterward - the same wrapper every other backend's own signed
// div/mod uses around an unsigned core.
void M6502Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                                  int words) {
    int n = 2 * words;
    auto isNegByte = [&](const char *lblByte, const char *dst) {
        code_ << "    lda " << lblByte << "\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta " << dst << "\n";
        safeBranch("bcc", Ldone, Lneg);
        code_ << Lneg << ":\n    ldai 1\n    sta " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negateIfFlag = [&](const char *flag, const char *buf) {
        std::string Lskip = newLabel(), Ldo = newLabel();
        code_ << "    lda " << flag << "\n";
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negateWideBuffer(buf, n);
        code_ << Lskip << ":\n";
    };
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = dividend, wac = divisor
    if (!isUnsigned) {
        isNegByte(byteAt("wbc", n - 1).c_str(), "dx");
        isNegByte(byteAt("wac", n - 1).c_str(), "dx+1");
        negateIfFlag("dx", "wbc");
        negateIfFlag("dx+1", "wac");
    }
    for (int i = 0; i < n; i++) code_ << "    ldai 0\n    sta " << byteAt("wtmp", i) << "\n    sta "
                                       << byteAt("wtmp2", i) << "\n";
    code_ << "    ldai " << (8 * n) << "\n    sta cx\n";
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    std::string Lrestore = newLabel(), Lsetbit = newLabel(), Lshiftq = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    // Combined 1-bit left shift of the (wtmp:wbc) pair - wbc's own
    // outgoing top bit (the carry after its own chain) becomes
    // wtmp's own new bottom bit via the very next `rol`.
    code_ << "    asl " << byteAt("wbc", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    rol " << byteAt("wbc", i) << "\n";
    for (int i = 0; i < n; i++) code_ << "    rol " << byteAt("wtmp", i) << "\n";
    // Trial subtraction: wtmp -= wac, always committed.
    code_ << "    sec\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    sbc " << byteAt("wac", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    safeBranch("bcs", Lsetbit, Lrestore);
    code_ << Lrestore << ":\n";
    code_ << "    clc\n";
    for (int i = 0; i < n; i++) {
        code_ << "    lda " << byteAt("wtmp", i) << "\n    adc " << byteAt("wac", i) << "\n    sta "
              << byteAt("wtmp", i) << "\n";
    }
    code_ << "    clc\n    jmp " << Lshiftq << "\n";
    code_ << Lsetbit << ":\n    sec\n";
    code_ << Lshiftq << ":\n";
    for (int i = 0; i < n; i++) code_ << "    rol " << byteAt("wtmp2", i) << "\n";
    code_ << "    dec cx\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        code_ << "    lda dx\n    eor dx+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negateWideBuffer("wtmp2", n);
        code_ << Lskip << ":\n";
        negateIfFlag("dx", "wtmp");
    }
    copyWide("wac", wantRemainder ? "wtmp" : "wtmp2", n);
}

void M6502Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already
    // shares one common type (IRBuilder's own usual-arithmetic-
    // conversion pass - see Type.h), except shift (only the LHS's own
    // type matters) and &&/|| (width-independent) - see
    // Pdp11Codegen.cpp's own version of this same comment.
    if (op == "&&") {
        genLogicalAnd(*e.lhs, *e.rhs);
        return;
    }
    if (op == "||") {
        genLogicalOr(*e.lhs, *e.rhs);
        return;
    }
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    int words = wide ? wordCountOf(operandType) : 1;
    if (op == "+" || op == "-") {
        if (wide) {
            genWideAdd(*e.lhs, *e.rhs, op == "+", words);
            return;
        }
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
        if (wide) {
            genWideMul(*e.lhs, *e.rhs, words);
        } else {
            genMul(*e.lhs, *e.rhs);
        }
        return;
    }
    if (op == "/" || op == "%") {
        if (wide) {
            genWideDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned, words);
        } else {
            genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        }
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        if (wide) {
            genWideBitwise(*e.lhs, *e.rhs, op, words);
            return;
        }
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
        if (wide) {
            genWideShift(*e.lhs, *e.rhs, op == "<<", isUnsigned, words);
            return;
        }
        genShift(*e.lhs, *e.rhs, op == "<<", isUnsigned);
        return;
    }
    if (wide) {
        genWideCompare(op, *e.lhs, *e.rhs, isUnsigned, words);
        return;
    }
    genCompare(op, *e.lhs, *e.rhs, isUnsigned);
}

// No variable-count shift instruction (ASL/LSR always shift exactly
// one bit) - loop `rhs` times instead, the same fix
// ../pdp7/Pdp7Codegen.cpp's own `<<`/`>>` already needed for the
// same reason (see M6502Codegen.h's own header comment).
void M6502Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    code_ << "    lda ac\n    sta cx\n";  // cx = shift count (always small; low byte is enough)
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";
    copy16("ac", "bc");  // ac = value to shift; bc is dead from here on (free as scratch)
    std::string Lloop = newLabel(), Lbody = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    lda cx\n";
    safeBranch("beq", Ldone, Lbody);
    code_ << Lbody << ":\n";
    if (isLeft) {
        code_ << "    asl ac\n    rol ac+1\n";
    } else if (isUnsigned) {
        // Logical (unsigned) shift: fills with 0 either way.
        code_ << "    lsr ac+1\n    ror ac\n";
    } else {
        // Arithmetic (signed) shift: re-derive the current sign bit
        // (ac+1's own bit 7) into the carry each iteration (via a
        // throwaway copy in `bc+1`, since ASL needs a memory operand
        // and would otherwise destroy ac+1 itself) and rotate that
        // bit back in at the top, instead of LSR's own fixed 0-fill.
        code_ << "    lda ac+1\n    sta bc+1\n    asl bc+1\n";
        code_ << "    ror ac+1\n    ror ac\n";
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
void M6502Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    auto negate16 = [&](const char *lbl) {
        code_ << "    ldai 0\n    sec\n    sbc " << lbl << "\n    sta " << lbl << "\n";
        code_ << "    ldai 0\n    sbc " << lbl << "+1\n    sta " << lbl << "+1\n";
    };
    // `isNeg(lbl)` leaves 0/1 in `dst` per `lbl`'s own top (sign) bit
    // - the same "copy to a throwaway byte, ASL it for the carry"
    // trick loadSlotToAc() uses for a `char`'s own sign extension.
    auto isNeg = [&](const char *lbl, const char *dst) {
        code_ << "    lda " << lbl << "+1\n    sta cx\n    asl cx\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    ldai 0\n    sta " << dst << "\n";
        safeBranch("bcc", Ldone, Lneg);
        code_ << Lneg << ":\n    ldai 1\n    sta " << dst << "\n";
        code_ << Ldone << ":\n";
    };
    auto negateIf = [&](const char *flag, const char *lbl) {
        std::string Lskip = newLabel(), Ldo = newLabel();
        code_ << "    lda " << flag << "\n";
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16(lbl);
        code_ << Lskip << ":\n";
    };
    genExpr(lhs);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(rhs);
    copy16("bc", "ac");  // bc = divisor
    code_ << "    pla\n    sta dx+1\n    pla\n    sta dx\n";  // dx = dividend (running remainder)
    // `cx`'s own low/high byte double as this operation's two 0/1
    // sign flags (dividend/divisor) - `cx` itself is otherwise never
    // touched by this routine (unlike genShift()/genMul(), which use
    // it as their own loop counter).
    if (!isUnsigned) {
        isNeg("dx", "cx");
        code_ << "    lda cx\n    sta aBase\n";  // stash the dividend's own sign flag (aBase: free here)
        isNeg("bc", "cx+1");
        negateIf("cx", "dx");
        negateIf("cx+1", "bc");
        code_ << "    lda aBase\n    sta cx\n";  // restore the dividend's own sign flag over cx's own low byte
    }
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
    if (!isUnsigned) {
        // Quotient is negative iff the two original signs differed;
        // the remainder always takes the dividend's own original
        // sign (same rule as every other backend's signed div/mod).
        code_ << "    lda cx\n    eor cx+1\n";
        std::string Lskip = newLabel(), Ldo = newLabel();
        safeBranch("beq", Lskip, Ldo);
        code_ << Ldo << ":\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("cx", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void M6502Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    // Every ordering comparison reduces to one "is a < b?" primitive
    // (emitLessCompare) by choosing which operand goes first and
    // whether to invert the final sense: a>b is b<a; a<=b is !(b<a);
    // a>=b is !(a<b).
    if (op == "<") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/true);
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
void M6502Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert) {
    genExpr(a);
    code_ << "    lda ac\n    pha\n    lda ac+1\n    pha\n";
    genExpr(b);
    code_ << "    pla\n    sta bc+1\n    pla\n    sta bc\n";  // bc = a
    code_ << "    sec\n    lda bc\n    sbc ac\n    lda bc+1\n    sbc ac+1\n";
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    if (isUnsigned) {
        // No sign/overflow fixup needed - the carry flag after a
        // full chained subtraction directly says "no borrow" (a>=b)
        // or "borrow" (a<b), which is exactly unsigned "<" already.
        safeBranch(invert ? "bcs" : "bcc", Ltrue, Lfalse);
    } else {
        std::string Lfixskip = newLabel();
        code_ << "    bvc " << Lfixskip << "\n";
        code_ << "    eori 128\n";
        code_ << Lfixskip << ":\n";
        // N now correctly reflects "a < b" (BMI) / "a >= b" (BPL).
        safeBranch(invert ? "bpl" : "bmi", Ltrue, Lfalse);
    }
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

// After genExpr(e) has just run, leaves `ac` holding 0 (false) or
// nonzero (true), with the Z flag already set correctly from the
// final instruction (safe for an immediately-following `beq`/`bne`,
// same as every call site already assumed before this existed) - for
// a wide value (left in `wac` - see M6502Codegen.h's own header
// comment), ORs every one of its bytes into `ac` first, since a
// multi-byte value is true iff ANY byte is nonzero.
void M6502Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        code_ << "    lda ac\n    ora ac+1\n";
        return;
    }
    int n = 2 * wordCountOf(e.type);
    code_ << "    lda " << byteAt("wac", 0) << "\n";
    for (int i = 1; i < n; i++) code_ << "    ora " << byteAt("wac", i) << "\n";
    code_ << "    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << "    lda ac\n";
}

void M6502Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("beq", Lfalse, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("beq", Lfalse, Ltrue);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lcont = newLabel(), Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    safeBranch("bne", Ltrue, Lcont);
    code_ << Lcont << ":\n";
    genExpr(rhs);
    testTruthy(rhs);
    safeBranch("bne", Ltrue, Lfalse);
    code_ << Ltrue << ":\n    ldai 1\n    sta ac\n    ldai 0\n    sta ac+1\n    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n    ldai 0\n    sta ac\n    ldai 0\n    sta ac+1\n";
    code_ << Ldone << ":\n";
}

void M6502Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
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
