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

// Rounds `n` up to the next multiple of 8, on top of IRBuilder's
// `frameSize`, which only guarantees a 4-byte-aligned result.
int alignUp8(int n) { return (n + 7) & ~7; }
int alignUp16(int n) { return (n + 15) & ~15; }

// Where the whole program is loaded and starts executing: qemu's
// `-kernel` raw-image convention for the `virt` machine's AArch64 boot
// loads it at RAM base + 0x80000 and sets the CPU's PC there directly.
// RAM base on `virt` is a fixed, documented 0x40000000.
constexpr uint64_t kBaseAddress = 0x40080000ull;

// Where the entry sequence points `sp` before calling main() -
// comfortably above the loaded program and its data.
constexpr uint64_t kInitialSP = 0x44000000ull;

// The "w" or "x" view of general register number `n`, per `wide` -
// threading this through genBinary()/genMul()/genDivMod()/genCompare()/
// genLogicalAnd()/Or()/Not() is all a `long long` operand needs; a real
// 64-bit register holds the whole value, no multi-word emulation needed.
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
    // Re-truncate+re-extend by shifting the unwanted high bits out and
    // back in (immediate `lsl`/`lsr`/`asr` - zero-fill for unsigned,
    // sign-fill for signed).
    if (w == 1) {
        code_ << "    lsl w0,w0,24\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " w0,w0,24\n";
    } else if (w == 2) {
        code_ << "    lsl w0,w0,16\n";
        code_ << "    " << (to.isUnsigned ? "lsr" : "asr") << " w0,w0,16\n";
    }
}

// `byteReg` is `x4` for an 8-byte destination/source, `w4` otherwise -
// keeping the shift/OR/AND chain's widths consistent with
// `destReg`/`srcReg` matches each instruction's `sf` bit to the width
// the other operand needs.
void Arm64Codegen::loadIntUnaligned(const std::string &addrReg, const std::string &destReg, int nBytes) {
    std::string byteReg = nBytes > 4 ? "x4" : "w4";
    code_ << "    ldindb " << addrReg << "," << byteReg << "\n";
    code_ << "    mov " << destReg << "," << byteReg << "\n";
    for (int byteIdx = 1; byteIdx < nBytes; byteIdx++) {
        code_ << "    mov x3," << addrReg << "\n    add x3,x3," << byteIdx << "\n    ldindb x3," << byteReg << "\n";
        code_ << "    lsl " << byteReg << "," << byteReg << "," << (byteIdx * 8) << "\n";
        code_ << "    orr " << destReg << "," << destReg << "," << byteReg << "\n";
    }
}
void Arm64Codegen::storeIntUnaligned(const std::string &addrReg, const std::string &srcReg, int nBytes) {
    std::string byteReg = nBytes > 4 ? "x4" : "w4";
    code_ << "    stindb " << addrReg << "," << srcReg << "\n";
    for (int byteIdx = 1; byteIdx < nBytes; byteIdx++) {
        code_ << "    mov " << byteReg << "," << srcReg << "\n    lsr " << byteReg << "," << byteReg << ","
              << (byteIdx * 8) << "\n";
        code_ << "    mov x3," << addrReg << "\n    add x3,x3," << byteIdx << "\n    stindb x3," << byteReg << "\n";
    }
}

// `addrReg` holds the already-computed 64-bit effective address; loads
// into w0 (or x0, for a `long long`), sign- or zero-extending a
// narrower-than-native-int value per its signedness.
void Arm64Codegen::loadOperand(const std::string &addrReg, Type type, bool mayBeUnaligned) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    " << (type.isUnsigned ? "ldindb " : "ldindsb ") << addrReg << ",w0\n";
    } else if (w == 2) {
        if (mayBeUnaligned) {
            loadIntUnaligned(addrReg, "w0", 2);
            if (!type.isUnsigned) code_ << "    lsl w0,w0,16\n    asr w0,w0,16\n";
        } else {
            code_ << "    " << (type.isUnsigned ? "ldindh " : "ldindsh ") << addrReg << ",w0\n";
        }
    } else if (w == 4) {
        if (mayBeUnaligned) {
            loadIntUnaligned(addrReg, "w0", 4);
        } else {
            code_ << "    ldind " << addrReg << ",w0\n";
        }
    } else if (mayBeUnaligned) {
        loadIntUnaligned(addrReg, "x0", 8);
    } else {
        code_ << "    ldind " << addrReg << ",x0\n";
    }
}

void Arm64Codegen::storeOperand(const std::string &addrReg, Type type, bool mayBeUnaligned) {
    int w = widthOfType(type);
    if (w == 1) {
        code_ << "    stindb " << addrReg << ",w0\n";
    } else if (w == 2) {
        if (mayBeUnaligned) {
            storeIntUnaligned(addrReg, "w0", 2);
        } else {
            code_ << "    stindh " << addrReg << ",w0\n";
        }
    } else if (w == 4) {
        if (mayBeUnaligned) {
            storeIntUnaligned(addrReg, "w0", 4);
        } else {
            code_ << "    stind " << addrReg << ",w0\n";
        }
    } else if (mayBeUnaligned) {
        storeIntUnaligned(addrReg, "x0", 8);
    } else {
        code_ << "    stind " << addrReg << ",x0\n";
    }
}

// An implicit or explicit type conversion. Arithmetic/comparison
// operands narrower than `int` are already promoted by IRBuilder, so the
// only real work here is crossing the native-int/`long long` boundary,
// or re-truncating+re-extending w0 for a narrower target - see narrowW0To().
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
            // w0's value is already correct in x0's low 32 bits,
            // zero-extended by the architectural W-form guarantee.
        } else {
            // Sign-extend w0 into x0: shift its value up into the
            // x-register's top half, then shift back down arithmetically,
            // sign-filling from what is now bit63 (w0's original sign bit).
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
    if (darwinHosted_ && slot->kind == ir::SlotKind::Param) {
        code_ << "    sub " << reg << "," << reg << "," << (currentParamBase_ + (slot->paramIndex + 1) * 8)
              << "\n";
        return;
    }
    // `frameOffset` is a byte offset from IRBuilder, which reserves a
    // flat two-native-`int`-widths (8 bytes) below a Param's
    // frameOffset=0 point. This prologue actually reserves 32 bytes
    // (two 16-byte-aligned pushes, since AArch64 wants `sp`
    // 16-byte-aligned) - `kParamExtra` makes up the difference so a
    // Param's address lands on the caller's pushed argument.
    constexpr int kParamExtra = 32 - 8;
    if (slot->kind == ir::SlotKind::Param) {
        code_ << "    add " << reg << "," << reg << "," << (slot->frameOffset + kParamExtra) << "\n";
    } else {
        code_ << "    sub " << reg << "," << reg << "," << ir::localLowOffset(*slot, typeWidths()) << "\n";
    }
}

// A literal element is its own decimal text; a symbol element (a
// dispatch-table address-of-function/global) is the referenced Slot's
// or Function's label, written out bare - Arm64Assembler.cpp's `dw`
// evaluator resolves it the same way a label operand resolves for
// MOVZ/MOVK.
std::string Arm64Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(static_cast<int32_t>(static_cast<uint32_t>(e.value)));
    return e.slot ? label(e.slot) : label(e.func);
}

void Arm64Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    // No 64-bit data directive in this dialect - a `long long` array's
    // literal elements are emitted as two "dw"s (low word, then high) per
    // element; a symbol element is always a native-int-width address, so
    // it still gets one "dw".
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
    if (darwinHosted_) {
        code_ << "    adrp " << reg << "," << lbl << "\n";
        code_ << "    addlo " << reg << "," << reg << "," << lbl << "\n";
        return;
    }
    code_ << "    movz " << reg << "," << lbl << "\n";
    code_ << "    movk " << reg << "," << lbl << "\n";
}

std::string Arm64Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // Separate compilation means `prog` may not define `main` at all.
    // `objectMode` means no entry sequence is emitted here -
    // Arm64LinkerFactory.cpp synthesizes one once every object is merged
    // and `main`'s linked address is resolvable. An object file's base is
    // `org 0`; placing it is the linker's job.
    code_ << "org " << (objectMode ? 0 : kBaseAddress) << "\n\n";
    if (!objectMode) {
        code_ << "_start:\n";
        code_ << "    movz x9," << static_cast<int32_t>(kInitialSP & 0xFFFFu) << "\n";
        code_ << "    movk x9," << static_cast<int32_t>((kInitialSP >> 16) & 0xFFFFu) << "\n";
        code_ << "    add sp,x9,0\n";
        code_ << "    bl " << label(mainFn) << "\n";
        // The result sits in w0 the moment execution reaches `halt`.
        code_ << "halt:\n    b halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Running byte offset within the data segment (8-byte aligned from
    // the start), used to insert alignment padding ahead of a global
    // wider than 1 byte: a misaligned `ldindh`/`stindh`/`ldind`/`stind`
    // is UNPREDICTABLE on this core and would clobber whatever previous
    // global sits there. A `long long` is accessed with one 8-byte
    // `ldind`/`stind`, so it needs 8-byte alignment.
    trailer_.push_back("    align 8");
    size_t dataOffset = 0;
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global isn't defined in this file - it must never
        // get a local storage word, only be referenced by name, letting
        // the assembler's undefined-symbol handling defer it to a relocation.
        if (slot->isExternal) continue;
        int align = std::min(widthOfType(slot->type), 8);
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

std::map<std::string, std::string> Arm64Codegen::perFileAsm() const { return perFileAsm_; }

// Every function saves/restores `x30` (link register) and `x29` (frame
// pointer) around its body, whether or not it calls anything else: `bl`
// sets `x30` unconditionally, so a non-leaf function's call overwrites
// it. Each save/restore only moves the register's low 32 bits through
// `stind`/`ldind`, safe only because every address in this backend's
// universe fits in 32 bits, so the reload's automatic zero-extension
// reconstructs the exact original 64-bit value.
//
// The prologue pushes the caller's `x30` then its `x29`, each in its own
// 16-byte-aligned block, then points the new `x29` at the saved-old-`x29`
// word - so the caller's pushed arguments sit 32 bytes above the new
// `x29`, and this call's Locals are reserved immediately below it by
// `sub sp,sp,frameSize*4` (see loadSlotAddr() for the full address
// arithmetic). The epilogue (here, and inline in the `Return` case,
// since an early return bypasses this fall-through epilogue) reverses
// all of this in the opposite order.
void Arm64Codegen::genFunction(const ir::FunctionBody &fn) {
    code_ << label(fn.func) << ":\n";
    if (darwinHosted_) {
        code_ << "    sub sp,sp,16\n    stind sp,x29\n";
        code_ << "    add x9,sp,8\n    stind x9,x30\n";
        code_ << "    add x29,sp,0\n";
        currentParamBase_ = alignUp8(fn.func->frameSize);
        currentFrameSize_ = alignUp16(currentParamBase_ + static_cast<int>(fn.func->params.size()) * 8);
        if (currentFrameSize_ > 0) code_ << "    sub sp,sp," << currentFrameSize_ << "\n";
        for (size_t i = 0; i < fn.func->params.size(); i++) {
            const ir::Slot *param = fn.func->params[i];
            std::string valueReg = (isWide(param->type) ? "x" : "w") + std::to_string(i < 8 ? i : 0);
            if (i >= 8) {
                code_ << "    add x9,x29," << (16 + (i - 8) * 8) << "\n";
                code_ << "    ldind x9," << valueReg << "\n";
            }
            int offset = currentParamBase_ + static_cast<int>(i + 1) * 8;
            code_ << "    sub x10,x29," << offset << "\n";
            code_ << "    stind x10," << valueReg << "\n";
        }
        genStmt(*fn.body);
        if (currentFrameSize_ > 0) code_ << "    add sp,sp," << currentFrameSize_ << "\n";
        code_ << "    ldind sp,x29\n    add x9,sp,8\n    ldind x9,x30\n    add sp,sp,16\n";
        code_ << "    ret\n\n";
        return;
    }
    code_ << "    sub sp,sp,16\n    stind sp,w30\n";
    code_ << "    sub sp,sp,16\n    stind sp,w29\n";
    // `add x29,sp,0`, not `mov x29,sp`: this dialect's `mov` alias is
    // `orr rd,<zr>,rn`, and a real ORR's register fields mean the zero
    // register for operand 31, never `sp` - `sp` is only addressable
    // through an instruction whose encoding gives register 31 that
    // meaning, e.g. `add`/`sub` immediate.
    code_ << "    add x29,sp,0\n";
    // IRBuilder's `frameSize` is only rounded up to the native `int`'s
    // width, fine for a Local's frameOffset, but `sp` itself must stay
    // 8-byte aligned at all times on this target (every access runs
    // against Device memory, which enforces natural alignment
    // unconditionally). `x29` is always 16-byte aligned already, so
    // rounding this `sub`/`add sp` amount up to a multiple of 8 keeps
    // `sp` 8-aligned for the entire body.
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
            // An early return bypasses genFunction()'s fall-through
            // epilogue, so it needs this same teardown inline, in the
            // opposite order from the prologue.
            if (currentFrameSize_ > 0) code_ << "    add sp,sp," << currentFrameSize_ << "\n";
            if (darwinHosted_) {
                code_ << "    ldind sp,x29\n    add x9,sp,8\n    ldind x9,x30\n    add sp,sp,16\n";
            } else {
                code_ << "    ldind sp,w29\n    add sp,sp,16\n";
                code_ << "    ldind sp,w30\n    add sp,sp,16\n";
            }
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
                storeOperand("x2", e.slot->type, /*mayBeUnaligned=*/e.slot->arraySize == 0);
                return;
            }
            genExpr(*e.rhs);
            loadSlotAddr("x2", e.slot);
            storeOperand("x2", e.slot->type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // x2 = target address
                genCompoundThroughX2(e, /*mayBeUnaligned=*/e.slot->arraySize == 0);
                return;
            }
            // A plain scalar target still needs its address computed
            // (A64 has no absolute-addressing memory operand), so it
            // goes through the same held-address tail as the indexed form.
            loadSlotAddr("x2", e.slot);
            genCompoundThroughX2(e);
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);  // w0 = pointer value
            code_ << "    mov x2,x0\n";  // x2 = address
            genCompoundThroughX2(e, /*mayBeUnaligned=*/true);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    cbz " << reg(0, isWide(e.lhs->type)) << "," << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    b " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width/signedness - the pointee type of a
            // typed pointer, or native-int-width for untyped-int-as-address.
            genExpr(*e.lhs);  // w0 = pointer value
            code_ << "    sub sp,sp,16\n    stind sp,w0\n";
            genExpr(*e.rhs);  // w0, or x0 if wide
            code_ << "    ldind sp,w2\n    add sp,sp,16\n";
            storeOperand("x2", e.type, /*mayBeUnaligned=*/true);
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
                // Loads at e.type's width/signedness - see DerefAssign above.
                genExpr(*e.rhs);  // w0 = pointer value
                code_ << "    mov x2,x0\n";
                loadOperand("x2", e.type, /*mayBeUnaligned=*/true);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                code_ << "    mvn " << reg(0, wide) << "," << reg(0, wide) << "\n";
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            if (darwinHosted_) {
                size_t registerArgs = std::min<size_t>(8, e.args.size());
                size_t stackArgs = e.args.size() > 8 ? (e.args.size() - 8) * 8 : 0;
                int callFrameSize = alignUp16(static_cast<int>(stackArgs + registerArgs * 8));
                if (callFrameSize > 0) code_ << "    sub sp,sp," << callFrameSize << "\n";
                for (size_t i = e.args.size(); i-- > 0;) {
                    genExpr(*e.args[i]);
                    size_t offset = i < 8 ? stackArgs + i * 8 : (i - 8) * 8;
                    code_ << "    add x2,sp," << offset << "\n";
                    code_ << "    stind x2," << (isWide(e.args[i]->type) ? "x0" : "w0") << "\n";
                }
                for (size_t i = 0; i < registerArgs; i++) {
                    code_ << "    add x9,sp," << (stackArgs + i * 8) << "\n";
                    code_ << "    ldind x9," << (isWide(e.args[i]->type) ? "x" : "w") << i << "\n";
                }
                code_ << "    bl " << label(e.func) << "\n";
                if (callFrameSize > 0) code_ << "    add sp,sp," << callFrameSize << "\n";
                return;
            }
            // Pushes each argument right-to-left so arg 0 ends up closest
            // to the top of the pushed-argument space. The caller cleans
            // the pushed words up once `bl` returns. Every argument
            // reserves at least one native word, and a wide (`long long`)
            // argument pushes a full 8 bytes via `x0`.
            //
            // The pushed bytes (4 per plain arg, 8 per wide one) can add
            // up to a total that isn't itself a multiple of 8, but `sp`
            // must stay a multiple of 8 at all times. So any padding
            // needed is reserved first, before any argument is pushed:
            // that leaves arg 0 still the last thing pushed, landing
            // exactly `x29 + 32` bytes up regardless of padding - unlike
            // padding added after the arguments, which would shift every
            // one away from the fixed offset IRBuilder assigned it.
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
            // argument on this kind, since the callee isn't known at
            // compile time. `blr x2` is a near call through whatever
            // address x2 holds, setting `x30` like `bl` does.
            genExpr(*e.lhs);  // w0 = the callee's entry address
            code_ << "    mov x2,x0\n";
            code_ << "    blr x2\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // No portable interrupt enable/disable on this bare-metal
            // core with no exception/interrupt setup - treated as a no-op.
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

// `x2` holds the computed address. `arraySize > 0` means the base is the
// array's own first-word address (loadSlotAddr() dispatches Global vs.
// Local); otherwise `slot` is a plain pointer variable, so the base comes
// from reading that variable's stored value first.
//
// Both a Global and a Local array's element addresses increase with the
// index; loadSlotAddr() points `x2` at element 0, the lowest address.
//
// Multiplying the index by a fixed element width uses `mul` against a
// loaded constant rather than a shift, since this dialect has no
// dedicated shift-by-immediate mnemonic.
void Arm64Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // w0 = index
    int shift = elemShift(slot->type);
    if (shift > 0) {
        code_ << "    movz w1," << (1 << shift) << "\n    mul w0,w0,w1\n";  // w0 *= the element type's own width
    }
    if (slot->arraySize > 0) {
        loadSlotAddr("x2", slot);
        // `x0`'s upper 32 bits are already zero (the `mul` above is a
        // 32-bit "W-form" op, an architectural guarantee), so `x0` is
        // already a valid 64-bit operand with no explicit extension.
        code_ << "    add x2,x2,x0\n";
    } else {
        loadSlotAddr("x1", slot);
        code_ << "    ldind x1,w1\n";  // w1 = the pointer variable's own stored value (x1 zero-extends too)
        code_ << "    add x2,x1,x0\n";
    }
}

void Arm64Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperand("x2", e.slot->type, /*mayBeUnaligned=*/e.slot->arraySize == 0);
}

// Stores `slot`'s arrayInit values into its per-call frame slots, at
// runtime, every time this LocalDecl executes - unlike a Global array,
// a Local array's storage is a fresh `x29`-relative frame slot on every
// call, so its initial values are written freshly each time too.
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
        code_ << "    mov x2,x29\n    sub x2,x2," << (ir::localLowOffset(*slot, typeWidths()) - static_cast<int>(i) * w)
              << "\n";
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

// Entry: w0 = old, w1 = rhs (both at `type`'s width/signedness; never wide).
void Arm64Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+") { code_ << "    add w0,w0,w1\n"; return; }
    if (op == "-") { code_ << "    sub w0,w0,w1\n"; return; }
    if (op == "*") { code_ << "    mul w0,w0,w1\n"; return; }
    if (op == "&") { code_ << "    and w0,w0,w1\n"; return; }
    if (op == "|") { code_ << "    orr w0,w0,w1\n"; return; }
    if (op == "^") { code_ << "    eor w0,w0,w1\n"; return; }
    if (op == "<<" || op == ">>") {
        const char *insn = op == "<<" ? "lslv" : (isUnsigned ? "lsrv" : "asrv");
        code_ << "    " << insn << " w0,w0,w1\n";
        return;
    }
    // "/" or "%" - w3 is the quotient scratch the remainder form needs,
    // since old and rhs both have to stay live in w0/w1 for msub.
    const char *divInsn = isUnsigned ? "udiv" : "sdiv";
    if (op == "%") {
        code_ << "    " << divInsn << " w3,w0,w1\n";
        code_ << "    msub w0,w3,w1,w0\n";  // w0 = w0 - w3*w1 = old - quotient*rhs
    } else {
        code_ << "    " << divInsn << " w0,w0,w1\n";
    }
}

// x2 must already hold the resolved address on entry. The address is
// stashed on the stack across evaluating e.rhs (which can reach
// arbitrary nested codegen using w0/w1/w2/w3) and never recomputed, so
// an indexed/pointer target's address sub-expression is evaluated exactly once.
void Arm64Codegen::genCompoundThroughX2(const ir::Expr &e, bool mayBeUnaligned) {
    code_ << "    sub sp,sp,16\n    stind sp,x2\n";  // stash address
    genExpr(*e.rhs);                                   // w0 = rhs
    code_ << "    mov w1,w0\n";                          // w1 = rhs
    code_ << "    ldind sp,x2\n    add sp,sp,16\n";       // x2 = address (restored)
    loadOperand("x2", e.type, mayBeUnaligned);              // w0 = old
    if (e.yieldOld) code_ << "    sub sp,sp,16\n    stind sp,w0\n";  // stash old
    genCompoundOp(e.op, e.type);                              // w0 = new (raw)
    narrowW0To(e.type);
    if (e.yieldOld) {
        code_ << "    mov w3,w0\n";                             // w3 = new
        code_ << "    ldind sp,w0\n    add sp,sp,16\n";           // w0 = old
        code_ << "    mov w1,w0\n";                                // w1 = old, preserved across the store below
        code_ << "    mov w0,w3\n";                                 // w0 = new - storeOperand always stores from w0
        storeOperand("x2", e.type, mayBeUnaligned);
        code_ << "    mov w0,w1\n";                                  // w0 = old (final result)
    } else {
        storeOperand("x2", e.type, mayBeUnaligned);  // w0 already = new
    }
}

void Arm64Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
    // Every arithmetic/bitwise/comparison operand here shares one common
    // type (IRBuilder's usual-arithmetic-conversion pass), except shift
    // (only the LHS's type matters) and &&/|| (width-independent). A real
    // 64-bit register holds a `long long` value directly - `reg(0,wide)`
    // etc. just pick "x" over "w".
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

// Real hardware multiply (`mul`, a full NxN->N product, N per `wide`).
void Arm64Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    bool wide = isWide(lhs.type);
    genExpr(lhs);
    code_ << "    sub sp,sp,16\n    stind sp," << reg(0, wide) << "\n";
    genExpr(rhs);
    code_ << "    ldind sp," << reg(1, wide) << "\n    add sp,sp,16\n";  // r1 = lhs, r0 = rhs
    code_ << "    mul " << reg(0, wide) << "," << reg(1, wide) << "," << reg(0, wide) << "\n";
}

// Real hardware divide (`sdiv`/`udiv`) plus `msub` (`rd = ra - rn*rm`,
// one instruction) for the remainder.
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

// `cmp` (`subs` with a discarded result) plus a dedicated `cset<cc>`
// per comparison operator - each directly produces the boolean this
// language needs (1 or 0), no branch needed. `isUnsigned` picks the
// HS/LO/HI/LS family instead of LT/GE/LE/GT for the ordered operators.
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

// `cmp w0,wzr` + `cseteq` is exactly "w0 = (w0 == 0)" - no branch needed.
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
    // `coff`/`elf`/`macho`/`aout`/`objraw`: assembleArm64Object() builds
    // one format-neutral ObjectFile, and the requested format just picks
    // that format's own FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleArm64Object(asmText, darwinHosted_ && format == "macho");
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
