#include "Arm64Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "Arm64Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// Rounds `n` up to the next multiple of 8 - see genFunction()'s own
// comment on why the per-call frame reservation needs this (on top
// of, not instead of, IRBuilder's own `frameSize`, which only ever
// guarantees a 4-byte-aligned result).
int alignUp8(int n) { return (n + 7) & ~7; }

// Where the whole program is loaded and starts executing. qemu's own
// `-kernel` raw-image convention for the `virt` machine's AArch64
// boot (confirmed directly: no ELF header, no Linux `Image` magic -
// just a flat binary) loads it at RAM base + 0x80000 and sets the
// CPU's PC there directly, no vector table or `loader`-device PC-
// forcing trick needed at all (unlike M68kCodegen.cpp's own
// kBaseAddress) - see tests/run_tests.sh's own qemu-system-aarch64
// invocation. RAM base on `virt` is a fixed, documented 0x40000000.
constexpr uint64_t kBaseAddress = 0x40080000ull;

// Where this backend's own entry sequence points `sp` before calling
// main() - comfortably above the loaded program and its data,
// growing down into otherwise-unused RAM (the test harness gives the
// guest 256M, so this is nowhere near the top).
constexpr uint64_t kInitialSP = 0x44000000ull;

// The "w" or "x" view of general register number `n`, per `wide` -
// every arithmetic/compare/shift mnemonic this backend emits already
// derives its own 32-vs-64-bit form from the register name alone
// (see Arm64Assembler.cpp's own `width` field), so threading this
// through genBinary()/genMul()/genDivMod()/genCompare()/
// genLogicalAnd()/Or()/Not() is all a `long long` operand needs - no
// separate multi-word emulation the way a narrower-register target
// needs (see I386Codegen.cpp's own genWideAdd() for the contrast).
std::string reg(int n, bool wide) { return (wide ? "x" : "w") + std::to_string(n); }
}  // namespace

void Arm64Codegen::loadImm32(const std::string &reg, uint32_t uv) {
    std::string w = "w" + reg.substr(1);  // the same physical register's own 32-bit ("w") view
    if (uv <= 0xFFFFu) {
        code_ << "    movz " << w << "," << static_cast<int32_t>(uv) << "\n";
    } else {
        code_ << "    movz " << w << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
        code_ << "    movk " << w << "," << static_cast<int32_t>((uv >> 16) & 0xFFFFu) << "\n";
    }
}

void Arm64Codegen::loadImm64(const std::string &dstReg, uint64_t uv) {
    code_ << "    movz " << dstReg << "," << static_cast<int32_t>(uv & 0xFFFFu) << "\n";
    uint16_t c1 = static_cast<uint16_t>((uv >> 16) & 0xFFFFu);
    uint16_t c2 = static_cast<uint16_t>((uv >> 32) & 0xFFFFu);
    uint16_t c3 = static_cast<uint16_t>((uv >> 48) & 0xFFFFu);
    bool any = (uv & 0xFFFFu) != 0;
    if (c1 != 0 || (any && (c2 != 0 || c3 != 0))) {
        code_ << "    movk " << dstReg << "," << static_cast<int32_t>(c1) << "\n";
    }
    if (c2 != 0) code_ << "    movk2 " << dstReg << "," << static_cast<int32_t>(c2) << "\n";
    if (c3 != 0) code_ << "    movk3 " << dstReg << "," << static_cast<int32_t>(c3) << "\n";
}

void Arm64Codegen::narrowW0To(Type to) {
    int w = widthOfType(to);
    // Re-truncate+re-extend by shifting the unwanted high bits out
    // and back in (real immediate `lsl`/`lsr`/`asr` - zero-fill for
    // unsigned, sign-fill for signed) - no dedicated sign/zero-extend
    // instruction needed.
    if (w == 1) {
        code_ << "    lsl w0,w0,24\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " w0,w0,24\n";
    } else if (w == 2) {
        code_ << "    lsl w0,w0,16\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " w0,w0,16\n";
    }
}

// `addrReg` holds the already-computed 64-bit effective address;
// loads into w0 (or x0, for a `long long` - a real 64-bit register
// holds the whole value directly on this target, no multi-word
// emulation needed), sign- or zero-extending a narrower-than-native-
// int value per its own signedness.
void Arm64Codegen::loadOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb " : "ldindsb ") << addrReg << ",w0\n";
    } else if (w == 2) {
        code_ << "    " << (type.isUnsigned ? "ldindh " : "ldindsh ") << addrReg << ",w0\n";
    } else if (w == 4) {
        code_ << "    ldind " << addrReg << ",w0\n";
    } else {
        code_ << "    ldind " << addrReg << ",x0\n";
    }
}

void Arm64Codegen::storeOperand(const std::string &addrReg, Type type) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",w0\n";
    } else if (w == 2) {
        code_ << "    stindh " << addrReg << ",w0\n";
    } else if (w == 4) {
        code_ << "    stind " << addrReg << ",w0\n";
    } else {
        code_ << "    stind " << addrReg << ",x0\n";
    }
}

// An implicit or explicit type conversion (see IR.h's ExprKind::
// Convert). Every arithmetic/comparison operand narrower than `int`
// is already promoted by IRBuilder before any Binary/Unary ever sees
// it, so the only real work here is crossing the native-int/`long
// long` boundary, or re-truncating+re-extending w0 for a narrower
// target - see narrowW0To().
void Arm64Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    genExpr(*e.lhs);
    if (fromWide && !toWide) {
        narrowW0To(to);  // the extra 32 bits in x0 are simply ignored from here on
    } else if (!fromWide && toWide) {
        if (from.isUnsigned) {
            // w0's own value is already correct in x0's low 32 bits,
            // zero-extended (every W-form write zeros the upper 32
            // bits of its own X view - see this file's own header
            // comment) - nothing to do.
        } else {
            // Sign-extend w0 into x0: no dedicated sign-extend
            // instruction in this dialect (real AArch64's SXTW is
            // itself just an alias for exactly this) - shift w0's own
            // value (already zero-extended into x0's low 32 bits) up
            // into the x-register's own top half, then shift back
            // down arithmetically, sign-filling from what is now
            // bit63 - exactly w0's own original sign bit. Same idea
            // as narrowW0To()'s own, one step wider.
            code_ << "    lsl x0,x0,32\n    asr x0,x0,32\n";
        }
    } else if (!fromWide && !toWide) {
        narrowW0To(to);
    }
    // fromWide && toWide: the bit pattern is already correct, nothing
    // to do.
}

void Arm64Codegen::loadSlotAddr(const std::string &reg, const ir::Slot *slot) {
    if (slot->kind == ir::SlotKind::Global) {
        loadAddress(reg, label(slot));
        return;
    }
    code_ << "    mov " << reg << ",x29\n";
    // `frameOffset` is already a real byte offset, computed by
    // IRBuilder from this slot's own declared type's width (see IR.h's
    // own comment on Slot::frameOffset) - no backend-side multiply by
    // a fixed word size any more. IRBuilder's own reservation below a
    // Param's own frameOffset=0 point is a flat two-native-`int`-
    // widths (8 bytes here), but this backend's own prologue actually
    // reserves 32 (two 16-byte-aligned pushes, each only 4 bytes
    // "real" - see genFunction()'s own comment on why: real AArch64
    // wants `sp` 16-byte-aligned) - `kParamExtra` makes up that
    // difference so a Param's real address still lands on the
    // caller's own pushed argument, not inside the saved-x30/x29
    // region.
    constexpr int kParamExtra = 32 - 8;
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    add " << reg << "," << reg << "," << (slot->frameOffset + kParamExtra) << "\n";
    } else {
        code_ << "    sub " << reg << "," << reg << "," << slot->frameOffset << "\n";
    }
}

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global, see IR.h's
// ArrayInitElem) is the referenced Slot's or Function's own label,
// written out bare - `dw`'s own expression evaluator
// (Arm64Assembler.cpp's Line::Dw) already resolves a bare symbol the
// same way `movz`/`movk`'s own label operand already does for an
// address loaded into a register (see loadAddress/genAddrOf).
std::string Arm64Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void Arm64Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long`
    // array's own literal elements are emitted as two "dw"s (low
    // word, then high) per element instead; a symbol element always
    // means a plain native-int-width address (the dispatch-table
    // idiom - see AST.h's ArrayInitElem), so it still gets one "dw".
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
        line << arrayElemText(e);
        first = false;
    }
    trailer_.push_back(line.str());
}

void Arm64Codegen::loadAddress(const std::string &reg, const std::string &lbl) {
    code_ << "    movz " << reg << "," << lbl << "\n";
    code_ << "    movk " << reg << "," << lbl << "\n";
}

std::string Arm64Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Real separate compilation (`--format=coff`/`elf`/`macho`/`aout`
    // from a single file - see IRBuilder.h's `allowExternal`) means
    // `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits this entry sequence
    // itself - Arm64LinkerFactory.cpp is solely responsible for
    // synthesizing exactly one, once every object is merged and
    // `main`'s linked address is resolvable (see Linker.h). An object
    // file's own base is `org 0` - it's the linker's job to place it,
    // not this backend's (see Arm64Assembler.h's own header comment).
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movz x9," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    movk x9," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    add sp,x9,0\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in w0 the moment execution reaches `halt` -
        // see Arm64Codegen.h's own header comment on why that's the
        // authoritative place to read it from.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Running byte offset within the data segment - used only to
    // insert alignment padding ahead of a global whose own type is
    // wider than 1 byte, same reasoning (and the same fix) as
    // ArmCodegen.cpp's own identical comment: a misaligned
    // `ldindh`/`stindh`/`ldind`/`stind` is UNPREDICTABLE/
    // implementation-defined on this core, and would otherwise
    // clobber whatever *previous* global happens to sit there. Every
    // instruction above is exactly 4 bytes and the image's own load
    // address is itself 4-byte-aligned (see kBaseAddress), so the
    // data segment always starts 4-byte-aligned - the coarsest
    // alignment ever actually needed here, since even a `long long`
    // is only ever accessed as two separate word-sized ldind/stind.
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
std::map<std::string, std::string> Arm64Codegen::perFileAsm() const { return perFileAsm_; }

// Every function here saves/restores `x30` (the link register) and
// `x29` (the frame pointer) around its own body, whether or not it
// happens to call anything else - `bl` sets `x30` to its own return
// address unconditionally, so a non-leaf function's own call to
// another function overwrites it; see MipsCodegen.cpp's own identical
// comment on `$ra` for the full reasoning (A64's `x30` plays exactly
// the same role MIPS's `$ra` does). Each save/restore here only ever
// moves its own register's low 32 bits through `stind`/`ldind` (this
// backend's 32-bit-transfer-only load/store - see Arm64Codegen.h's
// own header comment) - safe only because every address in this
// backend's own universe (the program's own base/stack addresses,
// both fixed well under 4GB - see this file's own kBaseAddress/
// kInitialSP) fits in 32 bits, so the reload's automatic zero-
// extension (see Arm64Codegen.h's own header comment on why every
// 32-bit op here already guarantees this) reconstructs the exact
// original 64-bit value.
//
// The prologue pushes the caller's own `x30` then its own `x29`, each
// in its own 16-byte-aligned block (matching every other temp-save in
// this file), then points the new `x29` at the saved-old-`x29` word -
// so the caller's own pushed arguments (see the `Call` case below)
// sit 32 bytes above the new `x29`, and this call's own Locals are
// reserved immediately below it by `sub sp,sp,frameSize*4` (see
// loadSlotAddr()'s own comment for the full address arithmetic this
// sets up). The epilogue (here, and inline in the `Return` case
// below, since a `return` statement is its own early exit from the
// function, bypassing this fall-through epilogue entirely) reverses
// all of this in the opposite order.
void Arm64Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    code_ << "    sub sp,sp,16\n    stind sp,w30\n";
    code_ << "    sub sp,sp,16\n    stind sp,w29\n";
    // `add x29,sp,0`, not `mov x29,sp`: this dialect's own `mov` alias
    // is `orr rd,<zr>,rn` (see Arm64Assembler.cpp's own encodeInstr())
    // - a real ORR's register fields always mean the zero register
    // for operand number 31, never `sp`, exactly like real AArch64
    // (`sp` is only ever addressable through an instruction whose own
    // encoding gives register 31 that meaning, e.g. `add`/`sub`
    // immediate - same reason this file's own entry sequence already
    // uses `add sp,x9,0` instead of `mov sp,x9` below).
    code_ << "    add x29,sp,0\n";
    // IRBuilder's own `frameSize` is only ever rounded up to the
    // native `int`'s width (see IRBuilder.cpp's own assignFrameOffsets()
    // comment) - fine for every Local's own frameOffset (an `int`-
    // sized slot only ever needs 4-byte alignment), but `sp` itself
    // has to stay a real AArch64 "Normal, Device-mapped" address: this
    // program never sets up the MMU, so every access (including every
    // later 64-bit `sub sp,sp,16 / stind sp,xN` temp push an `x`
    // register's own value or address ever uses - see genIndexAddr()/
    // the `Call` case below) runs against Device memory, which
    // enforces real natural alignment unconditionally (confirmed
    // directly: a `str x2,[sp]` at an `sp` that's a multiple of 4 but
    // not of 8 takes a real Data Abort under qemu). `x29` itself is
    // always 16- (hence 8-) byte aligned (every caller's own `sp`
    // before `bl` already is, and the 32 bytes of saved `x30`/`x29`
    // above don't change that), so rounding this one `sub`/`add sp`
    // pair's own amount up to a multiple of 8 - a purely backend-local
    // decision, independent of the frameOffset values IRBuilder
    // already assigned every Local - keeps `sp` 8-aligned for the
    // entire body, with at most 4 extra, otherwise-unused bytes below
    // the frame.
    int spFrameSize = alignUp8(fn.func->frameSize);
    if (spFrameSize > 0) code_ << "    sub sp,sp," << spFrameSize << "\n";
    currentFrameSize_ = spFrameSize;
    genStmt(*fn.body);
    if (spFrameSize > 0) code_ << "    add sp,sp," << spFrameSize << "\n";
    code_ << "    ldind sp,w29\n    add sp,sp,16\n";
    code_ << "    ldind sp,w30\n    add sp,sp,16\n";
    code_ << "    ret\n\n";
}

void Arm64Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cbz " << reg(0, isWide(s.cond->type)) << "," << (s.elseStmt ? Lelse : Ldone) << "\n";
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
            code_ << "    cbz " << reg(0, isWide(s.cond->type)) << "," << Lend << "\n";
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
            // Reverses genFunction()'s own prologue, in the opposite
            // order - a `return` statement is its own early exit from
            // the function, bypassing genFunction()'s own fall-
            // through epilogue entirely, so it needs this same
            // teardown inline (see genFunction()'s own comment).
            if (currentFrameSize_ > 0) code_ << "    add sp,sp," << currentFrameSize_ << "\n";
            code_ << "    ldind sp,w29\n    add sp,sp,16\n";
            code_ << "    ldind sp,w30\n    add sp,sp,16\n";
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            if (s.declSlot->arraySize > 0) {
                genLocalArrayInit(s.declSlot);
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                loadSlotAddr("x2", s.declSlot);
                storeOperand("x2", s.declSlot->type);
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

void Arm64Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                loadImm64("x0", static_cast<uint64_t>(e.intValue));
            } else {
                loadImm32("x0", static_cast<uint32_t>(e.intValue));
            }
            return;
        }
        case ir::ExprKind::Var: {
            loadSlotAddr("x2", e.slot);
            loadOperand("x2", e.slot->type);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // x2 = target address
                code_ << "    sub sp,sp,16\n    stind sp,x2\n";
                genExpr(*e.rhs);
                code_ << "    ldind sp,x2\n    add sp,sp,16\n";
                storeOperand("x2", e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("x2", e.slot);
            storeOperand("x2", e.slot->type);
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Deref is untyped (plain ints with '*'/'&' applied, see
            // Type.h's own header comment) - always a full native-int
            // word, unaffected by the new type system.
            genExpr(*e.lhs);  // w0 = pointer value
            code_ << "    sub sp,sp,16\n    stind sp,w0\n";
            genExpr(*e.rhs);
            code_ << "    ldind sp,w2\n    add sp,sp,16\n";
            code_ << "    stind x2,w0\n";
            return;
        }
        case ir::ExprKind::Unary: {
            bool wide = isWide(e.rhs->type);
            if (e.op == "-") {
                genExpr(*e.rhs);
                code_ << "    sub " << reg(0, wide) << ",xzr," << reg(0, wide) << "\n";
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                genExpr(*e.rhs);  // w0 = pointer value (deref is untyped - always native word)
                code_ << "    mov x2,x0\n";
                code_ << "    ldind x2,w0\n";
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn " << reg(0, wide) << "," << reg(0, wide) << "\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Pushes each argument right-to-left so arg 0 ends up
            // closest to the top of this call's own pushed-argument
            // space - see loadSlotAddr()'s own comment on how the
            // callee then finds them. Caller cleans the pushed words
            // back up once `bl` returns. Every argument reserves at
            // least one native word (the same rule IRBuilder.cpp's
            // own param-offset layout already applies), and a wide
            // (`long long`) argument pushes a full 8 bytes via `x0`.
            //
            // The real pushed bytes (4 per plain arg, 8 per wide one)
            // can add up to a total that isn't itself a multiple of 8
            // (e.g. three plain `int`s: 12) - harmless for the args'
            // own tight packing (matching IRBuilder's param-offset
            // layout exactly needs no alignment at all), but `sp`
            // itself has to stay a real multiple of 8 at all times on
            // this target (see genFunction()'s own comment on why) -
            // including right where the callee computes its own `x29`
            // as `sp - 32`. So any padding needed is reserved first,
            // *before* any argument is pushed: that leaves it sitting
            // in otherwise-unused space just above the tightly packed
            // argument region (nearer the caller's own frame), with
            // arg 0 still the very last thing pushed and so still
            // landing exactly `x29 + 32` bytes up, regardless of how
            // much padding came before it - unlike padding added
            // *after* the arguments, which would shift every single
            // one of them away from the fixed offset IRBuilder already
            // assigned it.
            int totalBytes = 0;
            for (const auto &arg : e.args) totalBytes += isWide(arg->type) ? 8 : 4;
            int padding = (8 - totalBytes % 8) % 8;
            if (padding > 0) code_ << "    sub sp,sp," << padding << "\n";
            for (size_t i = e.args.size(); i-- > 0;) {
                genExpr(*e.args[i]);
                if (isWide(e.args[i]->type)) {
                    code_ << "    sub sp,sp,8\n    stind sp,x0\n";
                } else {
                    code_ << "    sub sp,sp,4\n    stind sp,w0\n";
                }
            }
            code_ << "    bl " << label(e.func) << "\n";
            if (totalBytes + padding > 0) code_ << "    add sp,sp," << (totalBytes + padding) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr): the callee isn't known
            // at compile time for this kind, so there's no fixed
            // parameter count to push arguments against.
            // `blr x2` is this dialect's own real AArch64 "BLR Xn"
            // encoding (see Arm64Assembler.cpp's own emitBranchReg())
            // - a real near call through whatever address x2 holds,
            // setting `x30` to the return address exactly like `bl`
            // does.
            genExpr(*e.lhs);  // w0 = the callee's own entry address
            code_ << "    mov x2,x0\n";
            code_ << "    blr x2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No real portable notion of interrupt enable/disable on
            // a bare-metal A64 core with no exception/interrupt setup
            // done at all here - treated as a no-op, same reasoning
            // (and the same choice) as M68kCodegen.cpp's own/
            // MipsCodegen.cpp's own Intrinsic case.
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

// `x2` holds the computed address, no zero-page-style placement
// restriction. `arraySize > 0` means the base is the array's own
// first-word address (a Global's compile-time-known label, or a
// Local's own `x29`-relative address - see loadSlotAddr()); otherwise
// `slot` is a plain pointer variable, so the base instead comes from
// *reading* that variable's own stored value first.
//
// A Global array's element addresses increase with the index (its
// storage is ordinary ascending-address trailer data); a Local
// array's instead *decrease* with the index (element i sits at
// frameOffset+i - see IR.h's own Slot::frameOffset comment - and
// loadSlotAddr() already points `x2` at frameOffset's own, lowest-
// indexed, highest address), so indexing a Local array subtracts the
// scaled index instead of adding it.
//
// Multiplying the index by 4 (this target's own word size) uses a
// real `mul` against a loaded constant rather than a shift, since
// this dialect has no dedicated shift-by-immediate mnemonic at all
// (unlike MIPS's own `sll`) - `lslv`/`lsrv` (variable, register-form
// shift) cover every *language*-level `<<`/`>>` this backend ever
// needs, and a fixed `mul` by 4 is no less "real hardware" for this
// one compile-time-known case.
void Arm64Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // w0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) {
        code_ << "    movz w1," << (1 << shift) << "\n    mul w0,w0,w1\n";  // w0 *= the element type's own width
    }
    if (slot->arraySize > 0) {
        loadSlotAddr("x2", slot);
        // `x0`'s own upper 32 bits are already zero (the `mul` above
        // is a 32-bit "W-form" op - see Arm64Codegen.h's own header
        // comment on why that's an architectural guarantee, not an
        // assumption), so `x0` is already a valid 64-bit operand here
        // with no explicit zero-extension needed.
        if (slot->kind == ir::SlotKind::Global) {
            code_ << "    add x2,x2,x0\n";
        } else {
            code_ << "    sub x2,x2,x0\n";
        }
    } else {
        loadSlotAddr("x1", slot);
        code_ << "    ldind x1,w1\n";  // w1 = the pointer variable's own stored value (x1 zero-extends too)
        code_ << "    add x2,x1,x0\n";
    }
}

void Arm64Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("x2", e.slot->type);
}

// Stores `slot`'s own arrayInit values into its own per-call frame
// slots, at runtime, every time this LocalDecl executes - unlike a
// Global array (whose initial values are fixed trailer data, written
// once at load time), a Local array's storage is a fresh `x29`-
// relative frame slot on every call, so its own initial values have
// to be written freshly on every call too. See genIndexAddr()'s own
// comment on why a Local array's element i sits at frameOffset+i,
// not frameOffset-i.
void Arm64Codegen::genLocalArrayInit(const ir::Slot *slot) {
    int w = widthOfType(slot->type);
    for (size_t i = 0; i < slot->arrayInit.size(); i++) {
        const ir::ArrayInitElem &elem = slot->arrayInit[i];
        if (elem.isSymbol) {
            loadAddress("x0", elem.slot ? label(elem.slot) : label(elem.func));
        } else if (w == 8) {
            loadImm64("x0", static_cast<uint64_t>(elem.value));
        } else {
            loadImm32("x0", static_cast<uint32_t>(elem.value));
        }
        code_ << "    mov x2,x29\n    sub x2,x2," << (slot->frameOffset + static_cast<int>(i) * w) << "\n";
        storeOperand("x2", slot->type);
    }
}

void Arm64Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        loadSlotAddr("x2", operand.slot);
        code_ << "    mov x0,x2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        code_ << "    mov x0,x2\n";
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        loadAddress("x2", label(operand.func));
        code_ << "    mov x0,x2\n";
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

void Arm64Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here already shares
    // one common type (IRBuilder's own usual-arithmetic-conversion
    // pass - see Type.h), except shift (only the LHS's own type
    // matters) and &&/|| (width-independent, handled below). A real
    // 64-bit register holds a `long long` value directly on this
    // target - `reg(0,wide)` etc. just pick "x" over "w", no separate
    // multi-word emulation needed (contrast I386Codegen.cpp's own
    // genWideAdd() and friends).
    Type operandType = e.lhs->type;
    bool wide = isWide(operandType);
    bool isUnsigned = operandType.isUnsigned;
    if (op == "+" || op == "-") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";  // r1 = lhs, r0 = rhs
        code_ << "    " << (op == "+" ? "add " : "sub ") << reg(0, wide) << "," << reg(1, wide) << ","
              << reg(0, wide) << "\n";
        return;
    }
    if (op == "*") {
        genMul(*e.lhs, *e.rhs);
        return;
    }
    if (op == "/" || op == "%") {
        genDivMod(*e.lhs, *e.rhs, /*wantRemainder=*/op == "%", isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "orr" : "eor";
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
        genExpr(*e.rhs);
        code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";
        code_ << "    " << insn << " " << reg(0, wide) << "," << reg(1, wide) << "," << reg(0, wide) << "\n";
        return;
    }
    if (op == "<<" || op == ">>") {
        genExpr(*e.lhs);
        code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
        genExpr(*e.rhs);  // w0 = shift count (always plain int)
        code_ << "    mov w1,w0\n";
        code_ << "    ldind sp," << reg(0, wide) << "\n    add sp,sp,16\n";  // r0 = value to shift
        const char *insn = op == "<<" ? "lslv" : (isUnsigned ? "lsrv" : "asrv");
        code_ << "    " << insn << " " << reg(0, wide) << "," << reg(0, wide) << ",w1\n";
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

// Real hardware multiply (`mul`, a full NxN->N product, N per
// `wide`) - no operand-size limitation the way 68000's own `MULS.W`
// has.
void Arm64Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool wide = isWide(lhs.type);
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
    genExpr(rhs);
    code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";  // r1 = lhs, r0 = rhs
    code_ << "    mul " << reg(0, wide) << "," << reg(1, wide) << "," << reg(0, wide) << "\n";
}

// Real hardware divide (`sdiv`/`udiv`) plus `msub` (`rd = ra -
// rn*rm`, one instruction) for the remainder - no packed HI/LO
// register pair to unpack the way MIPS's own `mflo`/`mfhi` need.
void Arm64Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    bool wide = isWide(lhs.type);
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
    genExpr(rhs);
    code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";  // r1 = lhs (dividend), r0 = rhs (divisor)
    const char *divInsn = isUnsigned ? "udiv" : "sdiv";
    if (wantRemainder) {
        code_ << "    " << divInsn << " " << reg(2, wide) << "," << reg(1, wide) << "," << reg(0, wide) << "\n";
        code_ << "    msub " << reg(0, wide) << "," << reg(2, wide) << "," << reg(0, wide) << "," << reg(1, wide)
              << "\n";
    } else {
        code_ << "    " << divInsn << " " << reg(0, wide) << "," << reg(1, wide) << "," << reg(0, wide) << "\n";
    }
}

// `cmp` (real flags-setting `subs`) plus a dedicated `cset<cc>` per
// comparison operator - each one directly produces the boolean this
// language needs (1 or 0), no branch at all needed - see
// Arm64Codegen.h's own header comment on why this dialect spells
// `cset` as fixed mnemonics instead of one taking a condition-code
// operand. `isUnsigned` picks the HS/LO/HI/LS family instead of
// LT/GE/LE/GT for the ordered operators.
void Arm64Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs) {
    bool wide = isWide(lhs.type);
    bool isUnsigned = lhs.type.isUnsigned;
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
    genExpr(rhs);
    code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";  // r1 = lhs, r0 = rhs
    code_ << "    cmp " << reg(1, wide) << "," << reg(0, wide) << "\n";  // flags = lhs - rhs
    if (op == "<") {
        code_ << "    " << (isUnsigned ? "csetlo" : "csetlt") << " w0\n";
    } else if (op == ">") {
        code_ << "    " << (isUnsigned ? "csethi" : "csetgt") << " w0\n";
    } else if (op == "<=") {
        code_ << "    " << (isUnsigned ? "csetls" : "csetle") << " w0\n";
    } else if (op == ">=") {
        code_ << "    " << (isUnsigned ? "cseths" : "csetge") << " w0\n";
    } else if (op == "==") {
        code_ << "    cseteq w0\n";
    } else if (op == "!=") {
        code_ << "    csetne w0\n";
    } else {
        err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
    }
}

void Arm64Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    cbz " << reg(0, isWide(lhs.type)) << "," << Lfalse << "\n";
    genExpr(rhs);
    code_ << "    cbz " << reg(0, isWide(rhs.type)) << "," << Lfalse << "\n";
    code_ << "    b " << Ltrue << "\n";
    code_ << Lfalse << ":\n    movz w0,0\n    b " << Ldone << "\n";
    code_ << Ltrue << ":\n    movz w0,1\n";
    code_ << Ldone << ":\n";
}

void Arm64Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);
    std::string Ltrue = newLabel(), Lfalse = newLabel(), Ldone = newLabel();
    code_ << "    cbnz " << reg(0, isWide(lhs.type)) << "," << Ltrue << "\n";
    genExpr(rhs);
    code_ << "    cbnz " << reg(0, isWide(rhs.type)) << "," << Ltrue << "\n";
    code_ << "    b " << Lfalse << "\n";
    code_ << Ltrue << ":\n    movz w0,1\n    b " << Ldone << "\n";
    code_ << Lfalse << ":\n    movz w0,0\n";
    code_ << Ldone << ":\n";
}

// `cmp w0,wzr` + `cseteq` is exactly "w0 = (w0 == 0)" - logical not,
// no branch needed, same idea as MIPS's own `sltiu t0,t0,1`.
void Arm64Codegen::genLogicalNot(const ir::Expr &rhs) {
    bool wide = isWide(rhs.type);
    genExpr(rhs);
    code_ << "    cmp " << reg(0, wide) << "," << (wide ? "xzr" : "wzr") << "\n    cseteq w0\n";
}

std::string Arm64Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the armv8a backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> Arm64Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleArm64(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see Arm64Assembler.h's own `assembleArm64Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see fileformat/FileFormat.h)
    // to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArm64Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
