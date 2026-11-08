#include "I8008Codegen.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

#include "I8008Assembler.h"
#include "fileformat/FileFormat.h"

namespace pcc {

namespace {
[[noreturn]] void err(int line, const std::string &msg) {
    throw std::runtime_error("line " + std::to_string(line) + ": " + msg);
}

// The 8008's address space tops out at 16KB (14-bit addresses).
constexpr uint32_t kMemTop = 0x4000;
// Fixed memory cells (`ac`/`bc`/.../`fpsp`) are pinned to known addresses via the assembler's `name = expr`
// equates, so the test harness's Python 8008 emulator can read results by fixed address without parsing symbols.
// Anchored below `kMemTop` (not above 0) since everything from here up stays a small fixed size regardless of
// program size, leaving the rest of the address space free for generated code and globals.
constexpr uint32_t kPseudoBase = kMemTop - 0x400;
// `wac`/`wbc`/`wtmp`/`wtmp2`: the wide (`long`/`long long`) value buffers, 8 bytes each, after the other fixed
// pseudo-registers (18 bytes total).
constexpr uint32_t kWideBase = kPseudoBase + 18;
// Capacity of the subexpression-save stack (`sp`): a fixed limit with no overflow check.
[[maybe_unused]] constexpr int kStackSize = 32;  // documents the reserved region's size; its content is never read back
constexpr uint32_t kStackBase = kWideBase + 32;
// Capacity of the frame-pointer chain (`fpsp`): bounded in practice by the 8008's 7-level hardware call-nesting
// limit, with no overflow check of its own.
constexpr int kFpStackCapacity = 16;
constexpr uint32_t kFpStackBase = kStackBase + kStackSize * 2;
// Where the per-call Local/Param frame bump allocator (`fnext`/`fp`) starts claiming words from.
constexpr uint32_t kFrameBase = kFpStackBase + kFpStackCapacity * 2;
}  // namespace

// Pushes `srcCell`'s value onto the stack pointed to by `spCell`: reads `srcCell`'s bytes into B/C first (since
// loading `spCell` into H,L would otherwise clobber `srcCell` mid-read if they're the same cell), then writes them
// at the top of stack and advances `spCell` by one word.
void I8008Codegen::pushCell(const std::string &srcCell, const std::string &spCell) {
    pointAt(srcCell);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt(srcCell + "+1");
    code_ << "    mov a, m\n    mov c, a\n";
    loadHLFromCell(spCell);
    code_ << "    mov a, b\n    mov m, a\n";
    incHL();
    code_ << "    mov a, c\n    mov m, a\n";
    incHL();
    storeHLToCell(spCell);
}

// Retreats `spCell` by one word, then reads the two bytes now at the top of that stack into `dstCell`. Doesn't use
// storeHLToCell() - writing the decremented address back to `spCell` needs pointAt(spCell), which would repoint H,L
// away before the popped word is read - so the decremented address is stashed in D/E and written back afterward.
void I8008Codegen::popCell(const std::string &dstCell, const std::string &spCell) {
    loadHLFromCell(spCell);  // HL = current top-of-stack pointer (one past the last pushed word)
    decHL();
    decHL();  // HL = the address of the word being popped
    code_ << "    mov a, l\n    mov d, a\n";  // stash the decremented address in D/E
    code_ << "    mov a, h\n    mov e, a\n";
    code_ << "    mov a, m\n    mov b, a\n";  // read the popped word while HL still points at it
    incHL();
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt(spCell);
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt(spCell + "+1");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt(dstCell);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt(dstCell + "+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

namespace {
std::string byteAt(const char *base, int i) { return i == 0 ? base : (std::string(base) + "+" + std::to_string(i)); }
}  // namespace

// Byte-granular version of pushCell()/popCell() against the same `sp` stack, for wide values. `sp` stays resident
// in H,L for the whole loop rather than being reloaded per byte; reading `base`'s byte `i` still needs pointAt(),
// which repoints H,L away from the stack-top address, so that address is stashed in D,E across the read.
void I8008Codegen::pushWide(const char *base, int nbytes) {
    loadHLFromCell("sp");
    for (int i = 0; i < nbytes; i++) {
        code_ << "    mov a, l\n    mov d, a\n    mov a, h\n    mov e, a\n";  // d,e = this byte's own stack slot
        loadByte(byteAt(base, i));                                           // a = source byte (clobbers H,L)
        code_ << "    mov l, d\n    mov h, e\n";                             // h,l restored to that same slot
        code_ << "    mov m, a\n";
        incHL();
    }
    storeHLToCell("sp");
}

// Mirror of pushWide(): `sp` is dropped by `nbytes` in one addConstToHL() (not `nbytes` decHL() calls) to land on
// this value's first byte, written back as the new top-of-stack, then walked forward one byte at a time.
void I8008Codegen::popWide(const char *base, int nbytes) {
    loadHLFromCell("sp");
    addConstToHL(-nbytes);
    storeHLToCell("sp");   // storeHLToCell() leaves H,L pointing at `sp+1`, not the stored address, so reload it
    loadHLFromCell("sp");
    for (int i = 0; i < nbytes; i++) {
        code_ << "    mov a, l\n    mov d, a\n    mov a, h\n    mov e, a\n";  // d,e = this byte's own stack slot
        code_ << "    mov a, m\n";                                           // a = the byte at that slot
        storeByte(byteAt(base, i));                                         // clobbers H,L
        code_ << "    mov l, d\n    mov h, e\n";                             // h,l restored to that same slot
        incHL();
    }
}

void I8008Codegen::copyWide(const char *dst, const char *src, int nbytes) {
    for (int i = 0; i < nbytes; i++) {
        loadByte(byteAt(src, i));
        storeByte(byteAt(dst, i));
    }
}

void I8008Codegen::negateWideBuffer(const char *buf, int nbytes) {
    for (int i = 0; i < nbytes; i++) {
        loadByte(byteAt(buf, i));
        code_ << "    xri 0xff\n";
        storeByte(byteAt(buf, i));
    }
    loadByte(byteAt(buf, 0));
    code_ << "    adi 1\n";
    storeByte(byteAt(buf, 0));
    for (int i = 1; i < nbytes; i++) {
        loadByte(byteAt(buf, i));
        code_ << "    aci 0\n";
        storeByte(byteAt(buf, i));
    }
}

void I8008Codegen::genWideLoad(const ir::Slot *slot, int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        pointAtSlot(slot, i);
        code_ << "    mov a, m\n";
        storeByte(byteAt("wac", i));
    }
}

void I8008Codegen::genWideLoadLiteral(long value, int words) {
    auto uv = static_cast<unsigned long>(value);
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        code_ << "    mvi a, " << ((uv >> (8 * i)) & 0xFF) << "\n";
        storeByte(byteAt("wac", i));
    }
}

void I8008Codegen::genWideStore(const ir::Slot *slot, int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        // pointAtSlot() clobbers A internally for a Local/Param, so the value must be loaded before it's called.
        loadByte(byteAt("wac", i));
        code_ << "    mov b, a\n";
        pointAtSlot(slot, i);
        code_ << "    mov a, b\n    mov m, a\n";
    }
}

// Each iteration re-derives H,L from the `ptr` cell rather than walking a live H,L across the whole loop, since
// storeByte()/loadByte() on the `wac` side repoint H,L to a fixed address in between; `ptr`'s stored value is
// advanced in memory instead (addConstToCell()), which survives that repointing.
void I8008Codegen::genWideLoadAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        loadHLFromCell("ptr");
        code_ << "    mov a, m\n";
        storeByte(byteAt("wac", i));
        if (i + 1 < n) addConstToCell("ptr", 1);
    }
}

void I8008Codegen::genWideStoreAtPtr(int words) {
    int n = 2 * words;
    for (int i = 0; i < n; i++) {
        loadByte(byteAt("wac", i));
        code_ << "    mov b, a\n";
        loadHLFromCell("ptr");
        code_ << "    mov a, b\n    mov m, a\n";
        if (i + 1 < n) addConstToCell("ptr", 1);
    }
}

void I8008Codegen::storeHLToCell(const std::string &cell) {
    code_ << "    mov a, l\n    mov d, a\n";
    code_ << "    mov a, h\n    mov e, a\n";
    pointAt(cell);
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt(cell + "+1");
    code_ << "    mov a, e\n    mov m, a\n";
}

void I8008Codegen::decHL() {
    std::string Lskip = newLabel();
    code_ << "    mov a, l\n    and a\n";  // Z set iff l==0 (about to borrow into h)
    code_ << "    jfz " << Lskip << "\n";  // l != 0 - no borrow needed
    code_ << "    dcr h\n";
    code_ << Lskip << ":\n";
    code_ << "    dcr l\n";
}

// A literal element's decimal text, or a symbol element's label text (the referenced Slot's/Function's address);
// the assembler's "dw" directive resolves either into a full 16-bit value.
std::string I8008Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// A `char`-width element is one plain `db` byte; a wide (`long`/`long long`) one is split into plain bytes
// directly (little-endian), since the shared "dw" directive only splits a 16-bit value.
void I8008Codegen::declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values,
                                 Type elemType) {
    if (values.empty()) return;
    int w = widthOfType(elemType);
    if (w == 2) {
        std::ostringstream line;
        line << lbl << ": dw ";
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) line << ", ";
            line << arrayElemText(values[i]);
        }
        trailer_.push_back(line.str());
        return;
    }
    bool first = true;
    auto emit = [&](const std::string &text) {
        trailer_.push_back((first ? lbl + ": " : "") + text);
        first = false;
    };
    for (const auto &e : values) {
        if (w == 1) {
            emit("db " + std::to_string(static_cast<int>(static_cast<uint8_t>(e.value))));
        } else {
            auto uv = static_cast<unsigned long>(e.value);
            for (int b = 0; b < w; b++) emit("db " + std::to_string(static_cast<int>((uv >> (8 * b)) & 0xFF)));
        }
    }
}

void I8008Codegen::pointAt(const std::string &lbl) {
    code_ << "    mvihi h, " << lbl << "\n";
    code_ << "    mvi l, " << lbl << "\n";
}

void I8008Codegen::loadHLFromCell(const std::string &cell) {
    pointAt(cell);
    code_ << "    mov a, m\n    mov e, a\n";  // e = low byte of the stored value
    pointAt(cell + "+1");
    code_ << "    mov a, m\n    mov d, a\n";  // d = high byte of the stored value
    code_ << "    mov l, e\n    mov h, d\n";  // HL = (d,e) - the value itself, not cell's own address
}

void I8008Codegen::storeImmediate16(const std::string &dst, const std::string &exprText) {
    pointAt(dst);
    code_ << "    mvi a, " << exprText << "\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mvihi a, " << exprText << "\n    mov m, a\n";
}

void I8008Codegen::storeAcIndirect(const std::string &addrCell) {
    pointAt("ac");
    code_ << "    mov a, m\n    mov b, a\n";  // b = ac's low byte
    pointAt("ac+1");
    code_ << "    mov a, m\n    mov c, a\n";  // c = ac's high byte
    loadHLFromCell(addrCell);                  // HL = the real target address
    code_ << "    mov a, b\n    mov m, a\n";
    incHL();
    code_ << "    mov a, c\n    mov m, a\n";
}

// The 8008 has no 16-bit increment instruction, so this chains two 8-bit INRs through a carry test.
void I8008Codegen::incHL() {
    std::string Lskip = newLabel();
    code_ << "    inr l\n";
    code_ << "    mov a, l\n    and a\n";  // Z set iff l wrapped to 0
    code_ << "    jfz " << Lskip << "\n";  // didn't wrap - skip the high-byte carry
    code_ << "    inr h\n";
    code_ << Lskip << ":\n";
}

// Copies one 16-bit memory cell to another via four real memory accesses through B/C, since H,L can't address
// both src and dst at once.
void I8008Codegen::copy16(const std::string &dst, const std::string &src) {
    pointAt(src);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt(src + "+1");
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt(dst);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

int I8008Codegen::frameByteOffset(const ir::Slot *slot) const {
    if (slot->kind == ir::SlotKind::Param) {
        return slot->frameOffset - 2 * widthOfType(Type{BaseType::Int, false});
    }
    return currentParamBytes_ + slot->frameOffset - widthOfType(Type{BaseType::Int, false});
}

void I8008Codegen::loadByte(const std::string &cell) {
    pointAt(cell);
    code_ << "    mov a, m\n";
}

void I8008Codegen::storeByte(const std::string &cell) {
    pointAt(cell);
    code_ << "    mov m, a\n";
}

void I8008Codegen::emitSignExtendByte(const std::string &valueCell, const std::string &highDestCell,
                                       bool isUnsigned) {
    if (isUnsigned) {
        code_ << "    mvi a, 0\n";
        storeByte(highDestCell);
        return;
    }
    loadByte(valueCell);
    code_ << "    and a\n    ral\n";  // carry = valueCell's own top (sign) bit
    std::string Lneg = newLabel(), Ldone = newLabel();
    code_ << "    jtc " << Lneg << "\n";
    code_ << "    mvi a, 0\n";
    storeByte(highDestCell);
    code_ << "    jmp " << Ldone << "\n";
    code_ << Lneg << ":\n    mvi a, 0xff\n";
    storeByte(highDestCell);
    code_ << Ldone << ":\n";
}

// Adds `byteOffset` to H,L: low byte via ADI (the carry source), high byte via ACI to absorb it.
void I8008Codegen::addConstToHL(int byteOffset) {
    int lo = byteOffset & 0xFF;
    int hi = (byteOffset >> 8) & 0xFF;
    code_ << "    mov a, l\n    adi " << lo << "\n    mov l, a\n";
    code_ << "    mov a, h\n    aci " << hi << "\n    mov h, a\n";
}

// Same ADI/ACI carry chain as addConstToHL(), against a named cell instead of H,L (no ALU instruction can add an
// immediate directly to a memory destination here).
void I8008Codegen::addConstToCell(const std::string &cell, int amount) {
    int lo = amount & 0xFF;
    int hi = (amount >> 8) & 0xFF;
    pointAt(cell);
    code_ << "    mov a, m\n    adi " << lo << "\n    mov m, a\n";
    pointAt(cell + "+1");
    code_ << "    mov a, m\n    aci " << hi << "\n    mov m, a\n";
}

void I8008Codegen::pointAtSlot(const ir::Slot *slot, int extraByteOffset) {
    if (slot->kind == ir::SlotKind::Global) {
        pointAt(extraByteOffset == 0 ? label(slot) : (label(slot) + "+" + std::to_string(extraByteOffset)));
        return;
    }
    loadHLFromCell("fp");
    addConstToHL(frameByteOffset(slot) + extraByteOffset);
}

void I8008Codegen::pointAtSlotNextByte(const ir::Slot *slot, int justPointedOffset) {
    if (slot->kind == ir::SlotKind::Global) {
        pointAtSlot(slot, justPointedOffset + 1);
        return;
    }
    incHL();
}

void I8008Codegen::copy16FromSlot(const std::string &dst, const ir::Slot *slot) {
    pointAtSlot(slot, 0);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAtSlotNextByte(slot, 0);
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt(dst);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt(dst + "+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::copy16ToSlot(const ir::Slot *slot, const std::string &src) {
    pointAt(src);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt(src + "+1");
    code_ << "    mov a, m\n    mov c, a\n";
    pointAtSlot(slot, 0);
    code_ << "    mov a, b\n    mov m, a\n";
    pointAtSlotNextByte(slot, 0);
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::storeImmediate16ToSlot(const ir::Slot *slot, int elemOffset, const std::string &exprText) {
    pointAtSlot(slot, elemOffset);
    code_ << "    mvi a, " << exprText << "\n    mov m, a\n";
    pointAtSlotNextByte(slot, elemOffset);
    code_ << "    mvihi a, " << exprText << "\n    mov m, a\n";
}

// Same body as popCell(), with the final write-back aimed at pointAtSlot() instead of a plain cell's pointAt().
void I8008Codegen::popIntoSlot(const ir::Slot *slot) {
    int w = std::max(widthOfType(slot->type), 2);
    if (w <= 2) {
        loadHLFromCell("sp");
        decHL();
        decHL();
        code_ << "    mov a, l\n    mov d, a\n";
        code_ << "    mov a, h\n    mov e, a\n";
        code_ << "    mov a, m\n    mov b, a\n";
        incHL();
        code_ << "    mov a, m\n    mov c, a\n";
        pointAt("sp");
        code_ << "    mov a, d\n    mov m, a\n";
        pointAt("sp+1");
        code_ << "    mov a, e\n    mov m, a\n";
        pointAtSlot(slot, 0);
        code_ << "    mov a, b\n    mov m, a\n";
        pointAtSlotNextByte(slot, 0);
        code_ << "    mov a, c\n    mov m, a\n";
        return;
    }
    // A wide param: popped one byte at a time, highest byte first, matching the `sp` stack's LIFO order against
    // how the caller's Call case pushed it.
    for (int i = w - 1; i >= 0; i--) {
        loadHLFromCell("sp");
        decHL();
        code_ << "    mov a, l\n    mov d, a\n";
        code_ << "    mov a, h\n    mov e, a\n";
        code_ << "    mov a, m\n    mov b, a\n";
        pointAt("sp");
        code_ << "    mov a, d\n    mov m, a\n";
        pointAt("sp+1");
        code_ << "    mov a, e\n    mov m, a\n";
        pointAtSlot(slot, i);
        code_ << "    mov a, b\n    mov m, a\n";
    }
}

// Gives this call's frame back to the bump allocator and restores the caller's `fp`.
void I8008Codegen::emitEpilogue() {
    copy16("fnext", "fp");
    popCell("fp", "fpsp");
}

// One in-place 1-bit rotate of 16-bit cell `lbl` through the accumulator (RLC/RRC/RAL/RAR work on A only).
// `and a` clears carry without disturbing A (every logical instruction clears CY as a side effect).
void I8008Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    if (isLeft) {
        pointAt(lbl);
        code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    } else {
        // High byte first, so its vacated-bit carry-out feeds the low byte's rotate.
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    and a\n    rar\n    mov m, a\n";
        pointAt(lbl);
        code_ << "    mov a, m\n    rar\n    mov m, a\n";
    }
}

std::string I8008Codegen::generate(const ir::Program &prog, bool objectMode) {
    const ir::Function *mainFn = nullptr;
    for (const auto &fn : prog.functions) {
        if (fn->sourceName == "main") mainFn = fn.get();
    }
    // In object mode, `prog` may not define `main`; no object ever emits the entry sequence itself -
    // I8008LinkerFactory.cpp synthesizes it once every object is merged and `main`'s linked address is known.
    code_ << "org 0\n\n";
    // Fixed memory pseudo-registers, declared as compile-time equates - the same constant in every object, so no
    // `objectMode` special-casing is needed.
    code_ << "ac = " << kPseudoBase << "\n";
    code_ << "bc = " << (kPseudoBase + 2) << "\n";
    code_ << "cx = " << (kPseudoBase + 4) << "\n";
    code_ << "dx = " << (kPseudoBase + 6) << "\n";
    code_ << "ptr = " << (kPseudoBase + 8) << "\n";
    code_ << "sp = " << (kPseudoBase + 10) << "\n";
    // `fp` needs no initial value here - the first function entered (`main`) overwrites it from `fnext` before
    // reading it.
    code_ << "fp = " << (kPseudoBase + 12) << "\n";
    code_ << "fnext = " << (kPseudoBase + 14) << "\n";
    code_ << "fpsp = " << (kPseudoBase + 16) << "\n";
    code_ << "wac = " << kWideBase << "\n";
    code_ << "wbc = " << (kWideBase + 8) << "\n";
    code_ << "wtmp = " << (kWideBase + 16) << "\n";
    code_ << "wtmp2 = " << (kWideBase + 24) << "\n";
    if (!objectMode) {
        code_ << "\n_start:\n";
        storeImmediate16("sp", std::to_string(kStackBase));
        storeImmediate16("fpsp", std::to_string(kFpStackBase));
        storeImmediate16("fnext", std::to_string(kFrameBase));
        code_ << "    cal " << label(mainFn) << "\n";
        // The program's final result sits in `ac` once execution reaches `halt`.
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    // Shared subroutines are only ever reached via `cal`, never fallen into, so it's safe to place them after
    // every real function's code.
    for (const std::string &body : wideDivModSubroutineBodies_) code_ << body;
    for (const std::string &body : wideEqualitySubroutineBodies_) code_ << body;

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global is only referenced by name, letting the assembler's undefined-symbol handling
        // defer it to a relocation.
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

std::map<std::string, std::string> I8008Codegen::perFileAsm() const { return perFileAsm_; }

// Prologue: saves the caller's `fp` onto `fpsp`, takes `fnext` as this call's frame base, bumps `fnext` past
// `params.size() + frameSize`, then pops each argument off `sp` (last pushed, first popped - params walked back
// to front) into the fresh frame.
void I8008Codegen::genFunction(const ir::FunctionBody &fn) {
    currentParamCount_ = static_cast<int>(fn.func->params.size());
    currentParamBytes_ = 0;
    for (const ir::Slot *p : fn.func->params) currentParamBytes_ += std::max(widthOfType(p->type), 2);
    const int totalBytes = currentParamBytes_ + fn.func->frameSize;
    code_ << label(fn.func) << ":\n";
    pushCell("fp", "fpsp");
    copy16("fp", "fnext");
    if (totalBytes > 0) addConstToCell("fnext", totalBytes);
    for (int i = currentParamCount_ - 1; i >= 0; i--) popIntoSlot(fn.func->params[static_cast<size_t>(i)]);
    currentEpilogueLabel_ = newLabel();
    genStmt(*fn.body);
    code_ << currentEpilogueLabel_ << ":\n";
    emitEpilogue();
    code_ << "    ret\n\n";
}

void I8008Codegen::genStmt(const ir::Stmt &s) {
    switch (s.kind) {
        case ir::StmtKind::Block: {
            for (const auto &child : s.body) genStmt(*child);
            return;
        }
        case ir::StmtKind::If: {
            genExpr(*s.cond);
            testTruthy(*s.cond);
            std::string Lelse = s.elseStmt ? newLabel() : "";
            std::string Ldone = newLabel();
            code_ << "    jtz " << (s.elseStmt ? Lelse : Ldone) << "\n";
            genStmt(*s.thenStmt);
            if (s.elseStmt) {
                code_ << "    jmp " << Ldone << "\n";
                code_ << Lelse << ":\n";
                genStmt(*s.elseStmt);
            }
            code_ << Ldone << ":\n";
            return;
        }
        case ir::StmtKind::While: {
            std::string Lstart = newLabel(), Lend = newLabel();
            std::string Lcont = s.postStmt ? newLabel() : Lstart;
            code_ << Lstart << ":\n";
            genExpr(*s.cond);
            testTruthy(*s.cond);
            code_ << "    jtz " << Lend << "\n";
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
            code_ << "    jmp " << currentEpilogueLabel_ << "\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // A Local array's initializer is written at runtime every time this declaration is reached (unlike a
            // Global array's, baked into the binary once), since a Local's storage is this call's own frame.
            if (s.declSlot->arraySize > 0) {
                int w = widthOfType(s.declSlot->type);
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    const ir::ArrayInitElem &elem = s.declSlot->arrayInit[i];
                    if (w <= 2) {
                        storeImmediate16ToSlot(s.declSlot, w * static_cast<int>(i), arrayElemText(elem));
                    } else {
                        // A wide element's literal is never a symbol - split directly into `w` plain bytes.
                        auto uv = static_cast<unsigned long>(elem.value);
                        for (int b = 0; b < w; b++) {
                            pointAtSlot(s.declSlot, w * static_cast<int>(i) + b);
                            code_ << "    mvi a, " << ((uv >> (8 * b)) & 0xFF) << "\n    mov m, a\n";
                        }
                    }
                }
            } else if (s.initOrReturnOrExpr) {
                genExpr(*s.initOrReturnOrExpr);
                if (isWide(s.declSlot->type)) {
                    genWideStore(s.declSlot, wordCountOf(s.declSlot->type));
                } else if (widthOfType(s.declSlot->type) == 1) {
                    loadByte("ac");
                    code_ << "    mov b, a\n";
                    pointAtSlot(s.declSlot, 0);
                    code_ << "    mov a, b\n    mov m, a\n";
                } else {
                    copy16ToSlot(s.declSlot, "ac");
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
            code_ << s.asmText;
            if (!s.asmText.empty() && s.asmText.back() != '\n') code_ << "\n";
            return;
        }
    }
}

void I8008Codegen::genExpr(const ir::Expr &e) {
    switch (e.kind) {
        case ir::ExprKind::IntLit: {
            if (isWide(e.type)) {
                genWideLoadLiteral(e.intValue, wordCountOf(e.type));
                return;
            }
            storeImmediate16("ac", std::to_string(static_cast<uint16_t>(e.intValue)));
            return;
        }
        case ir::ExprKind::Var: {
            if (isWide(e.slot->type)) {
                genWideLoad(e.slot, wordCountOf(e.slot->type));
                return;
            }
            if (widthOfType(e.slot->type) == 1) {
                pointAtSlot(e.slot, 0);
                code_ << "    mov a, m\n";
                storeByte("ac");
                emitSignExtendByte("ac", "ac+1", e.slot->type.isUnsigned);
                return;
            }
            copy16FromSlot("ac", e.slot);
            return;
        }
        case ir::ExprKind::Index: {
            genIndex(e);
            return;
        }
        case ir::ExprKind::Assign: {
            if (e.lhs) {  // slot[idx] = rhs
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                copy16("ac", "ptr");
                pushAc();  // push ptr's own value, carried via ac
                genExpr(*e.rhs);
                popInto("ptr");  // ac is untouched by popInto() - it still holds rhs's value
                storeOperandAtPtr(e.slot->type);
                return;
            }
            genExpr(*e.rhs);
            if (isWide(e.slot->type)) {
                genWideStore(e.slot, wordCountOf(e.slot->type));
            } else if (widthOfType(e.slot->type) == 1) {
                // pointAtSlot() clobbers A internally for a Local/Param (it computes `fp + offset` through it).
                loadByte("ac");
                code_ << "    mov b, a\n";
                pointAtSlot(e.slot, 0);
                code_ << "    mov a, b\n    mov m, a\n";
            } else {
                copy16ToSlot(e.slot, "ac");
            }
            return;
        }
        case ir::ExprKind::DerefAssign: {
            // Stores at e.type's width (the pointee type, or plain native-word width for untyped-int-as-address);
            // `ptr` is set directly from the pointer value instead of via genIndexAddr().
            genExpr(*e.lhs);  // ac = pointer value
            copy16("ptr", "ac");
            pushAc();  // ac already equals the pointer value - push it directly
            genExpr(*e.rhs);
            popInto("ptr");  // ac/wac is untouched by popInto() - it still holds rhs's value
            storeOperandAtPtr(e.type);
            return;
        }
        case ir::ExprKind::CompoundAssign: {
            if (e.lhs) {  // slot[idx] op= rhs (including slot[idx]++/--)
                genIndexAddr(e.slot, *e.lhs);  // ptr = target address
                genCompoundThroughPtr(e);
                return;
            }
            // A plain scalar target's storage is reconstructible at zero cost after rhs evaluates (fp is restored
            // by any nested call's epilogue), so nothing needs holding across rhs's evaluation here.
            genExpr(*e.rhs);     // ac = rhs
            copy16("bc", "ac");  // bc = rhs
            if (widthOfType(e.slot->type) == 1) {
                pointAtSlot(e.slot, 0);
                code_ << "    mov a, m\n";
                storeByte("ac");
                emitSignExtendByte("ac", "ac+1", e.slot->type.isUnsigned);
            } else {
                copy16FromSlot("ac", e.slot);  // ac = old
            }
            if (e.yieldOld) pushAc();
            genCompoundOp(e.op, e.type);
            narrowAcTo(e.type);
            if (widthOfType(e.slot->type) == 1) {
                loadByte("ac");
                code_ << "    mov b, a\n";
                pointAtSlot(e.slot, 0);
                code_ << "    mov a, b\n    mov m, a\n";
            } else {
                copy16ToSlot(e.slot, "ac");
            }
            if (e.yieldOld) popInto("ac");
            return;
        }
        case ir::ExprKind::CompoundDerefAssign: {
            genExpr(*e.lhs);     // ac = pointer value
            copy16("ptr", "ac");  // ptr = address
            genCompoundThroughPtr(e);
            return;
        }
        case ir::ExprKind::Ternary: {
            genExpr(*e.lhs);
            testTruthy(*e.lhs);
            std::string Lelse = newLabel();
            std::string Ldone = newLabel();
            code_ << "    jtz " << Lelse << "\n";
            genExpr(*e.rhs);
            code_ << "    jmp " << Ldone << "\n";
            code_ << Lelse << ":\n";
            genExpr(*e.elseExpr);
            code_ << Ldone << ":\n";
            return;
        }
        case ir::ExprKind::Unary: {
            bool wide = isWide(e.rhs->type);
            if (e.op == "-") {
                genExpr(*e.rhs);
                if (wide) {
                    negateWideBuffer("wac", 2 * wordCountOf(e.rhs->type));
                } else {
                    pointAt("ac");
                    code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                    pointAt("ac+1");
                    code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                    pointAt("ac");
                    code_ << "    mov a, m\n    adi 1\n    mov m, a\n";
                    pointAt("ac+1");
                    code_ << "    mov a, m\n    aci 0\n    mov m, a\n";
                }
            } else if (e.op == "!") {
                genLogicalNot(*e.rhs);
            } else if (e.op == "*") {
                // Loads at e.type's width, `ptr` set directly from the pointer value instead of genIndexAddr().
                genExpr(*e.rhs);  // ac = pointer value
                copy16("ptr", "ac");
                loadOperandAtPtr(e.type);
            } else if (e.op == "~") {
                genExpr(*e.rhs);
                if (wide) {
                    int n = 2 * wordCountOf(e.rhs->type);
                    for (int i = 0; i < n; i++) {
                        loadByte(byteAt("wac", i));
                        code_ << "    xri 0xff\n";
                        storeByte(byteAt("wac", i));
                    }
                } else {
                    pointAt("ac");
                    code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                    pointAt("ac+1");
                    code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
                }
            } else {  // "&"
                genAddrOf(*e.rhs);
            }
            return;
        }
        case ir::ExprKind::Call: {
            // Each argument is pushed onto `sp` in argument order rather than through a cell named after the
            // callee's param, since a shared cell would be clobbered by direct recursion before `cal` even runs.
            // The callee's prologue pops them in reverse into its fresh frame. A wide argument pushes its real
            // width in bytes; a narrow one always pushes 2 bytes via pushAc() (already promoted to `int`).
            for (size_t i = 0; i < e.args.size(); i++) {
                genExpr(*e.args[i]);
                int w = std::max(widthOfType(e.args[i]->type), 2);
                if (w > 2) {
                    pushWide("wac", w);
                } else {
                    pushAc();
                }
            }
            code_ << "    cal " << label(e.func) << "\n";
            return;
        }
        case ir::ExprKind::CallPtr: {
            // The 8008 has no indirect call/jump (every CAL/JMP operand is a fixed address baked in at assemble
            // time, unlike the 8080/Z80's PCHL/JP (HL)), so a computed call is emulated with self-modifying code:
            // the placeholder `cal 0` below has its own operand bytes (Lcal+1/+2) overwritten with the callee's
            // address, computed into `ac`, immediately before the CPU falls through and executes it as an
            // ordinary CAL. Safe here since this backend has no interrupts or concurrency to race the patch.
            genExpr(*e.lhs);  // ac = the callee's own entry address
            std::string Lcal = newLabel();
            pointAt("ac");
            code_ << "    mov a, m\n";
            pointAt(Lcal + "+1");
            code_ << "    mov m, a\n";
            pointAt("ac+1");
            code_ << "    mov a, m\n";
            pointAt(Lcal + "+2");
            code_ << "    mov m, a\n";
            code_ << Lcal << ": cal 0\n";
            return;
        }
        case ir::ExprKind::Binary: {
            genBinary(e);
            return;
        }
        case ir::ExprKind::Intrinsic: {
            // The 8008 has no interrupt enable/disable instruction (interrupts work via an external signal
            // forcing an instruction onto the bus), so ion()/iof() are no-ops.
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

// Leaves slot[idxExpr]'s effective address in `ptr`. `idxExpr` is evaluated first and alone. The base is the
// array's first word for `slot->arraySize > 0` (a Global's fixed label, or `fp` plus a runtime offset for a
// Local); otherwise `slot` is a plain pointer variable and the base comes from reading its stored value. No
// hardware 16-bit add exists, so the doubled index and base are added byte-wise with carry chained low into
// high. `fp` is the start, not the middle, of each frame, so every Local/Param offset here is non-negative.
void I8008Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ptr = index * widthOf(slot->type), via repeated doubling (elemShift()).
    copy16("ptr", "ac");
    for (int s = 0; s < elemShift(slot->type); s++) {
        pointAt("ptr");
        code_ << "    mov a, m\n    mov b, a\n";  // b = ptr_lo
        pointAt("ptr+1");
        code_ << "    mov a, m\n    mov c, a\n";  // c = ptr_hi
        code_ << "    mov a, b\n    add b\n    mov d, a\n";  // d = ptr_lo + ptr_lo
        code_ << "    mov a, c\n    adc c\n    mov e, a\n";  // e = ptr_hi + ptr_hi + carry
        pointAt("ptr");
        code_ << "    mov a, d\n    mov m, a\n";
        pointAt("ptr+1");
        code_ << "    mov a, e\n    mov m, a\n";
    }

    if (slot->arraySize > 0 && slot->kind == ir::SlotKind::Global) {
        // A Global array's address is a compile-time constant, loaded directly into B,C.
        std::string lbl = label(slot);
        code_ << "    mvi b, " << lbl << "\n";     // b = low byte of the array's own address
        code_ << "    mvihi c, " << lbl << "\n";    // c = high byte
    } else if (slot->arraySize > 0) {
        // A Local array's address is computed into H,L via pointAtSlot(), then copied into B,C.
        pointAtSlot(slot, 0);
        code_ << "    mov a, l\n    mov b, a\n";
        code_ << "    mov a, h\n    mov c, a\n";
    } else {
        // `slot` is a plain pointer variable - the base comes from reading its stored value.
        pointAtSlot(slot, 0);
        code_ << "    mov a, m\n    mov b, a\n";
        pointAtSlotNextByte(slot, 0);
        code_ << "    mov a, m\n    mov c, a\n";
    }
    pointAt("ptr");
    code_ << "    mov a, m\n    add b\n    mov d, a\n";
    pointAt("ptr+1");
    code_ << "    mov a, m\n    adc c\n    mov e, a\n";
    pointAt("ptr");
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt("ptr+1");
    code_ << "    mov a, e\n    mov m, a\n";
}

void I8008Codegen::genIndex(const ir::Expr &e) {
    genIndexAddr(e.slot, *e.lhs);
    loadOperandAtPtr(e.slot->type);
}

void I8008Codegen::loadOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideLoadAtPtr(wordCountOf(type));
        return;
    }
    if (w == 1) {
        loadHLFromCell("ptr");
        code_ << "    mov a, m\n";
        storeByte("ac");
        emitSignExtendByte("ac", "ac+1", type.isUnsigned);
        return;
    }
    loadHLFromCell("ptr");  // HL = the computed address itself
    code_ << "    mov a, m\n    mov b, a\n";
    incHL();
    code_ << "    mov a, m\n    mov c, a\n";
    pointAt("ac");
    code_ << "    mov a, b\n    mov m, a\n";
    pointAt("ac+1");
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::storeOperandAtPtr(Type type) {
    int w = widthOfType(type);
    if (w > 2) {
        genWideStoreAtPtr(wordCountOf(type));
        return;
    }
    if (w == 1) {
        // loadHLFromCell() clobbers A internally, so the value to store must survive in B across that call.
        loadByte("ac");
        code_ << "    mov b, a\n";
        loadHLFromCell("ptr");
        code_ << "    mov a, b\n    mov m, a\n";
        return;
    }
    storeAcIndirect("ptr");
}

void I8008Codegen::narrowAcTo(Type to) {
    if (widthOfType(to) != 1) return;  // a native word: ac already holds the full value
    emitSignExtendByte("ac", "ac+1", to.isUnsigned);
}

// `op` applied to ac=old, bc=rhs, leaving the raw new value in ac; `old` plays genBinary()'s lhs role, `rhs` its
// rhs role, for non-commutative operators.
void I8008Codegen::genCompoundOp(const std::string &op, Type type) {
    bool isUnsigned = type.isUnsigned;
    if (op == "+" || op == "-") {
        const char *lo = op == "+" ? "add" : "sub";
        const char *hi = op == "+" ? "adc" : "sbb";
        pointAt("ac");
        code_ << "    mov a, m\n";  // a = old_lo
        pointAt("bc");
        code_ << "    " << lo << " m\n";  // a = old_lo (+/-) rhs_lo, CY set accordingly
        code_ << "    mov b, a\n";
        pointAt("ac+1");
        code_ << "    mov a, m\n";  // a = old_hi
        pointAt("bc+1");
        code_ << "    " << hi << " m\n";  // a = old_hi (+/-) rhs_hi (+/-) carry
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
        return;
    }
    if (op == "*") {
        copy16("dx", "ac");  // dx = multiplier = old (bc already = rhs = multiplicand)
        genMulCore();
        return;
    }
    if (op == "/" || op == "%") {
        copy16("dx", "ac");  // dx = dividend = old (bc already = rhs = divisor)
        genDivModCore(op == "%", isUnsigned);
        return;
    }
    if (op == "&" || op == "|" || op == "^") {
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        pointAt("ac");
        code_ << "    mov a, m\n";
        pointAt("bc");
        code_ << "    " << insn << " m\n";
        code_ << "    mov b, a\n";
        pointAt("ac+1");
        code_ << "    mov a, m\n";
        pointAt("bc+1");
        code_ << "    " << insn << " m\n";
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
        return;
    }
    // "<<" or ">>" - genShiftCore() expects bc=value/cx=count, the opposite of ac=old/bc=rhs here.
    copy16("cx", "bc");  // cx = shift count (rhs)
    copy16("bc", "ac");  // bc = value to shift (old)
    genShiftCore(op == "<<", isUnsigned);
}

// `ptr` must already hold the resolved address on entry. `e.rhs`'s evaluation can clobber ptr/ac/bc/cx/dx, so the
// address is stashed on the `sp` stack across it; genCompoundOp()'s "/"/"%" case also reaches into `ptr`, so it's
// re-stashed a second time around that call.
void I8008Codegen::genCompoundThroughPtr(const ir::Expr &e) {
    pushCell("ptr", "sp");  // stash address across rhs's own evaluation
    genExpr(*e.rhs);        // ac = rhs
    copy16("bc", "ac");     // bc = rhs
    popCell("ptr", "sp");   // ptr restored
    loadOperandAtPtr(e.type);  // ac = old
    if (e.yieldOld) pushAc();  // stash old - this expr's own result
    pushCell("ptr", "sp");     // re-stash address around genCompoundOp()
    genCompoundOp(e.op, e.type);  // ac = new (raw), using ac=old, bc=rhs
    narrowAcTo(e.type);
    popCell("ptr", "sp");   // ptr restored (ac, holding `new`, is untouched)
    storeOperandAtPtr(e.type);
    if (e.yieldOld) popInto("ac");  // ac = old (final result)
}

void I8008Codegen::genAddrOf(const ir::Expr &operand) {
    if (operand.kind == ir::ExprKind::Var) {
        if (operand.slot->kind == ir::SlotKind::Global) {
            storeImmediate16("ac", label(operand.slot));
        } else {
            // A Local/Param's address is only known at runtime (`fp` plus a word offset), never a compile-time
            // immediate the way a Global's is.
            pointAtSlot(operand.slot, 0);  // H,L = this call's own copy of operand.slot
            storeHLToCell("ac");
        }
        return;
    }
    if (operand.kind == ir::ExprKind::Index) {
        genIndexAddr(operand.slot, *operand.lhs);
        copy16("ac", "ptr");
        return;
    }
    if (operand.kind == ir::ExprKind::Func) {
        storeImmediate16("ac", label(operand.func));
        return;
    }
    err(operand.line, "internal: unhandled addressable-operand kind");
}

// An implicit or explicit type conversion.
void I8008Codegen::genConvert(const ir::Expr &e) {
    Type from = e.lhs->type;
    Type to = e.type;
    bool fromWide = isWide(from);
    bool toWide = isWide(to);
    auto narrowCharFixup = [&]() { emitSignExtendByte("ac", "ac+1", to.isUnsigned); };
    if (!fromWide && !toWide) {
        genExpr(*e.lhs);
        if (widthOfType(to) == 1) narrowCharFixup();
        return;
    }
    if (fromWide && !toWide) {
        genExpr(*e.lhs);
        loadByte(byteAt("wac", 0));
        storeByte("ac");
        loadByte(byteAt("wac", 1));
        storeByte("ac+1");
        if (widthOfType(to) == 1) narrowCharFixup();
        return;
    }
    if (!fromWide && toWide) {
        int toWords = wordCountOf(to);
        genExpr(*e.lhs);
        loadByte("ac");
        storeByte(byteAt("wac", 0));
        loadByte("ac+1");
        storeByte(byteAt("wac", 1));
        if (toWords > 1) {
            emitSignExtendByte("ac+1", "dx", from.isUnsigned);
            for (int i = 2; i < 2 * toWords; i++) {
                loadByte("dx");
                storeByte(byteAt("wac", i));
            }
        }
        return;
    }
    int fromN = 2 * wordCountOf(from);
    int toN = 2 * wordCountOf(to);
    genExpr(*e.lhs);
    if (toN > fromN) {
        emitSignExtendByte(byteAt("wac", fromN - 1), "dx", from.isUnsigned);
        for (int i = fromN; i < toN; i++) {
            loadByte("dx");
            storeByte(byteAt("wac", i));
        }
    }
}

void I8008Codegen::genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    const char *op0 = isAdd ? "add" : "sub";
    const char *opc = isAdd ? "adc" : "sbb";
    for (int i = 0; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    " << (i == 0 ? op0 : opc) << " m\n";
        storeByte(byteAt("wac", i));
    }
}

void I8008Codegen::genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
    for (int i = 0; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    " << insn << " m\n";
        storeByte(byteAt("wac", i));
    }
}

std::string I8008Codegen::getWideEqualitySubroutine(int words) {
    if (auto it = wideEqualitySubroutineLabel_.find(words); it != wideEqualitySubroutineLabel_.end()) {
        return it->second;
    }
    std::string label = newLabel();
    wideEqualitySubroutineLabel_.emplace(words, label);

    std::ostringstream callSiteCode;
    std::swap(callSiteCode, code_);
    code_ << label << ":\n";

    int n = 2 * words;
    pointAt(byteAt("wbc", 0));
    code_ << "    mov a, m\n";
    pointAt(byteAt("wac", 0));
    code_ << "    sub m\n";
    storeByte("dx");
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    sub m\n";
        pointAt("dx");
        code_ << "    or m\n";
        storeByte("dx");
    }
    code_ << "    ret\n\n";
    wideEqualitySubroutineBodies_.push_back(code_.str());
    std::swap(callSiteCode, code_);
    return label;
}

void I8008Codegen::emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
    code_ << "    cal " << getWideEqualitySubroutine(words) << "\n";
    std::string Ltrue = newLabel(), Ldone = newLabel();
    pointAt("dx");
    code_ << "    mov a, m\n    and a\n";
    code_ << "    j" << (wantEqual ? "tz" : "fz") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

// Scans byte-wise least-significant first via a chained subtract (one `sub`, then `sbb` per later byte, borrow
// propagating automatically). Flipping the sign bit of only the most significant byte before subtracting maps
// the signed range onto the unsigned one, the wide generalization of the 16-bit sign-bit-bias trick.
void I8008Codegen::emitWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, int words,
                                        bool invert) {
    int n = 2 * words;
    genExpr(a);
    pushWide("wac", n);
    genExpr(b);
    popWide("wbc", n);  // wbc = a, wac = b
    if (!isUnsigned) {
        pointAt(byteAt("wbc", n - 1));
        code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";
        pointAt(byteAt("wac", n - 1));
        code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";
    }
    pointAt(byteAt("wbc", 0));
    code_ << "    mov a, m\n";
    pointAt(byteAt("wac", 0));
    code_ << "    sub m\n";
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    sbb m\n";
    }
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (invert ? "fc" : "tc") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned,
                                   int words) {
    if (op == "==") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/true);
    if (op == "!=") return emitWideEqualityCompare(lhs, rhs, words, /*wantEqual=*/false);
    if (op == "<") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/false);
    if (op == ">") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/false);
    if (op == "<=") return emitWideLessCompare(rhs, lhs, isUnsigned, words, /*invert=*/true);
    if (op == ">=") return emitWideLessCompare(lhs, rhs, isUnsigned, words, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// A wide value shifted by a runtime count: loops one bit at a time across all `n` bytes, chained through the
// accumulator (RAL/RAR work on A only). A signed right shift re-derives the current sign bit into carry before
// the top byte's rotate, instead of a fixed 0-fill.
void I8008Codegen::genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);  // ac = shift count (already promoted to a plain int)
    copy16("cx", "ac");
    popWide("wac", n);
    pointAt("cx");
    code_ << "    mov a, m\n    mov b, a\n";  // b = shift count
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n    mov a, b\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    if (isLeft) {
        pointAt(byteAt("wac", 0));
        code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
        for (int i = 1; i < n; i++) {
            pointAt(byteAt("wac", i));
            code_ << "    mov a, m\n    ral\n    mov m, a\n";
        }
    } else if (isUnsigned) {
        pointAt(byteAt("wac", n - 1));
        code_ << "    mov a, m\n    and a\n    rar\n    mov m, a\n";
        for (int i = n - 2; i >= 0; i--) {
            pointAt(byteAt("wac", i));
            code_ << "    mov a, m\n    rar\n    mov m, a\n";
        }
    } else {
        pointAt(byteAt("wac", n - 1));
        code_ << "    mov a, m\n    mov c, a\n    and a\n    ral\n";  // carry = current sign bit; c = saved value
        code_ << "    mov a, c\n    rar\n    mov m, a\n";
        for (int i = n - 2; i >= 0; i--) {
            pointAt(byteAt("wac", i));
            code_ << "    mov a, m\n    rar\n    mov m, a\n";
        }
    }
    code_ << "    dcr b\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

// Textbook shift-and-add multiply, `n*8` bits serially - the wide generalization of genMul(). Needs no
// signedness, since a truncated multiply's bit pattern never depends on it.
void I8008Codegen::genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = multiplicand, wac = multiplier
    for (int i = 0; i < n; i++) {
        code_ << "    mvi a, 0\n";
        storeByte(byteAt("wtmp", i));
    }
    code_ << "    mvi d, " << (8 * n) << "\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n    mov a, d\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    pointAt(byteAt("wac", 0));
    code_ << "    mov a, m\n    ani 1\n";
    code_ << "    jtz " << Lskipadd << "\n";
    code_ << "    mvi e, 0\n";  // e = carry-in for the first (least significant) byte's own add
    for (int i = 0; i < n; i++) {
        pointAt(byteAt("wtmp", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wbc", i));
        code_ << "    " << (i == 0 ? "add" : "adc") << " m\n";
        storeByte(byteAt("wtmp", i));
    }
    code_ << Lskipadd << ":\n";
    pointAt(byteAt("wbc", 0));
    code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    }
    pointAt(byteAt("wac", n - 1));
    code_ << "    mov a, m\n    and a\n    rar\n    mov m, a\n";
    for (int i = n - 2; i >= 0; i--) {
        pointAt(byteAt("wac", i));
        code_ << "    mov a, m\n    rar\n    mov m, a\n";
    }
    code_ << "    dcr d\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copyWide("wac", "wtmp", n);
}

std::string I8008Codegen::getWideDivModSubroutine(bool isUnsigned, int words) {
    auto key = std::make_pair(isUnsigned, words);
    if (auto it = wideDivModSubroutineLabel_.find(key); it != wideDivModSubroutineLabel_.end()) {
        return it->second;
    }
    std::string label = newLabel();
    wideDivModSubroutineLabel_.emplace(key, label);

    // Builds this subroutine's body into a fresh stream so it can be queued separately rather than landing at the
    // call site; `code_` is simply swapped for the duration, so every helper still writes through it as usual.
    std::ostringstream callSiteCode;
    std::swap(callSiteCode, code_);
    code_ << label << ":\n";

    int n = 2 * words;
    auto isNegByte = [&](const std::string &byteCell, const std::string &dstCell) {
        pointAt(byteCell);
        code_ << "    mov a, m\n    and a\n    ral\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    mvi a, 0\n";
        storeByte(dstCell);
        code_ << "    jtc " << Lneg << "\n";
        code_ << "    jmp " << Ldone << "\n";
        code_ << Lneg << ":\n    mvi a, 1\n";
        storeByte(dstCell);
        code_ << Ldone << ":\n";
    };
    auto negateIfFlag = [&](const std::string &flagCell, const char *buf) {
        pointAt(flagCell);
        code_ << "    mov a, m\n    and a\n";
        std::string Lskip = newLabel();
        code_ << "    jtz " << Lskip << "\n";
        negateWideBuffer(buf, n);
        code_ << Lskip << ":\n";
    };
    // On entry, `wbc` holds the dividend and `wac` the divisor - placed there by the call site before `cal`-ing
    // this label.
    if (!isUnsigned) {
        isNegByte(byteAt("wbc", n - 1), "dx");
        isNegByte(byteAt("wac", n - 1), "dx+1");
        negateIfFlag("dx", "wbc");
        negateIfFlag("dx+1", "wac");
    }
    for (int i = 0; i < n; i++) {
        code_ << "    mvi a, 0\n";
        storeByte(byteAt("wtmp", i));
        storeByte(byteAt("wtmp2", i));
    }
    code_ << "    mvi d, " << (8 * n) << "\n";
    std::string Lloop = newLabel(), Ldone = newLabel();
    std::string Lrestore = newLabel(), Lsetbit = newLabel(), Lshiftq = newLabel();
    code_ << Lloop << ":\n    mov a, d\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    // Combined 1-bit left shift of the (wtmp:wbc) pair.
    pointAt(byteAt("wbc", 0));
    code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wbc", i));
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    }
    for (int i = 0; i < n; i++) {
        pointAt(byteAt("wtmp", i));
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    }
    // Trial subtraction: wtmp -= wac, always committed.
    pointAt(byteAt("wtmp", 0));
    code_ << "    mov a, m\n";
    pointAt(byteAt("wac", 0));
    code_ << "    sub m\n";
    storeByte(byteAt("wtmp", 0));
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wtmp", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    sbb m\n";
        storeByte(byteAt("wtmp", i));
    }
    code_ << "    jtc " << Lrestore << "\n";
    code_ << "    jmp " << Lsetbit << "\n";
    code_ << Lrestore << ":\n";
    pointAt(byteAt("wtmp", 0));
    code_ << "    mov a, m\n";
    pointAt(byteAt("wac", 0));
    code_ << "    add m\n";
    storeByte(byteAt("wtmp", 0));
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wtmp", i));
        code_ << "    mov a, m\n";
        pointAt(byteAt("wac", i));
        code_ << "    adc m\n";
        storeByte(byteAt("wtmp", i));
    }
    code_ << "    mvi a, 0\n";  // new quotient bit = 0; carry must end up clear for the roll below
    code_ << "    and a\n    jmp " << Lshiftq << "\n";
    code_ << Lsetbit << ":\n";
    code_ << "    mvi a, 0xff\n    and a\n    ral\n";  // carry = 1 (bit7 of 0xff), A discarded
    code_ << Lshiftq << ":\n";
    for (int i = 0; i < n; i++) {
        pointAt(byteAt("wtmp2", i));
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    }
    code_ << "    dcr d\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        pointAt("dx");
        code_ << "    mov a, m\n";
        pointAt("dx+1");
        code_ << "    xor m\n";
        std::string Lskip = newLabel();
        code_ << "    jtz " << Lskip << "\n";
        negateWideBuffer("wtmp2", n);
        code_ << Lskip << ":\n";
        negateIfFlag("dx", "wtmp");
    }
    code_ << "    ret\n\n";
    wideDivModSubroutineBodies_.push_back(code_.str());
    std::swap(callSiteCode, code_);
    return label;
}

void I8008Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                                  int words) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = dividend, wac = divisor
    std::string label = getWideDivModSubroutine(isUnsigned, words);
    code_ << "    cal " << label << "\n";
    copyWide("wac", wantRemainder ? "wtmp" : "wtmp2", n);
}

void I8008Codegen::testTruthy(const ir::Expr &e) {
    if (!isWide(e.type)) {
        pointAt("ac");
        code_ << "    mov a, m\n";
        pointAt("ac+1");
        code_ << "    or m\n";
        return;
    }
    int n = 2 * wordCountOf(e.type);
    loadByte(byteAt("wac", 0));
    for (int i = 1; i < n; i++) {
        pointAt(byteAt("wac", i));
        code_ << "    or m\n";
    }
    storeByte("ac");
    code_ << "    mvi a, 0\n";
    storeByte("ac+1");
    pointAt("ac");
    code_ << "    mov a, m\n    and a\n";
}

void I8008Codegen::genBinary(const ir::Expr &e) {
    const std::string &op = e.op;
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
        pushAc();
        genExpr(*e.rhs);
        popInto("bc");  // bc = lhs
        const char *lo = op == "+" ? "add" : "sub";
        const char *hi = op == "+" ? "adc" : "sbb";
        pointAt("bc");
        code_ << "    mov a, m\n";  // a = lhs_lo
        pointAt("ac");
        code_ << "    " << lo << " m\n";  // a = lhs_lo (+/-) rhs_lo, CY set accordingly
        code_ << "    mov b, a\n";
        pointAt("bc+1");
        code_ << "    mov a, m\n";  // a = lhs_hi
        pointAt("ac+1");
        code_ << "    " << hi << " m\n";  // a = lhs_hi (+/-) rhs_hi (+/-) carry
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
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
        pushAc();
        genExpr(*e.rhs);
        popInto("bc");  // bc = lhs
        const char *insn = (op == "&") ? "and" : (op == "|") ? "or" : "xor";
        pointAt("bc");
        code_ << "    mov a, m\n";
        pointAt("ac");
        code_ << "    " << insn << " m\n";
        code_ << "    mov b, a\n";
        pointAt("bc+1");
        code_ << "    mov a, m\n";
        pointAt("ac+1");
        code_ << "    " << insn << " m\n";
        code_ << "    mov c, a\n";
        pointAt("ac");
        code_ << "    mov a, b\n    mov m, a\n";
        pointAt("ac+1");
        code_ << "    mov a, c\n    mov m, a\n";
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

// No variable-count shift instruction - loop `rhs` times, one bit at a time, via shiftOneBit(). A signed right
// shift re-derives the current sign bit into carry before each rotate, instead of a fixed 0-fill.
void I8008Codegen::genShiftCore(bool isLeft, bool isUnsigned) {
    pointAt("cx");
    code_ << "    mov a, m\n    mov b, a\n";  // b = shift count (always small; low byte is enough)
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a, b\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    if (isLeft || isUnsigned) {
        shiftOneBit("bc", isLeft);
    } else {
        pointAt("bc+1");
        code_ << "    mov a, m\n    mov c, a\n    and a\n    ral\n";  // carry = current sign bit
        code_ << "    mov a, c\n    rar\n    mov m, a\n";
        pointAt("bc");
        code_ << "    mov a, m\n    rar\n    mov m, a\n";
    }
    code_ << "    dcr b\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    copy16("ac", "bc");
}

void I8008Codegen::genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned) {
    genExpr(lhs);  // ac = value to shift
    pushAc();
    genExpr(rhs);         // ac = shift count
    copy16("cx", "ac");  // popInto() below uses every one of A/B/C/D/E/H/L as scratch, so stash the count in a cell
    popInto("bc");         // bc = value (restored only now)
    genShiftCore(isLeft, isUnsigned);
}

// Textbook shift-and-add multiply (no hardware multiply): `bc` is
// the multiplicand (doubled each iteration), `dx` the multiplier
// (halved each iteration), `ac` the accumulating product. Only
// verified against non-negative operands.
void I8008Codegen::genMulCore() {
    storeImmediate16("ac", "0");
    code_ << "    mvi d, 16\n";  // loop counter, in the real register D
    std::string Lloop = newLabel(), Ldone = newLabel(), Lskipadd = newLabel();
    code_ << Lloop << ":\n";
    code_ << "    mov a, d\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    pointAt("dx");
    code_ << "    mov a, m\n    ani 1\n";
    code_ << "    jtz " << Lskipadd << "\n";
    pointAt("bc");
    code_ << "    mov a, m\n    mov e, a\n";  // e = bc_lo
    pointAt("ac");
    code_ << "    mov a, m\n    add e\n    mov m, a\n";
    pointAt("bc+1");
    code_ << "    mov a, m\n    mov e, a\n";  // e = bc_hi
    pointAt("ac+1");
    code_ << "    mov a, m\n    adc e\n    mov m, a\n";
    code_ << Lskipadd << ":\n";
    shiftOneBit("bc", /*isLeft=*/true);
    shiftOneBit("dx", /*isLeft=*/false);
    code_ << "    dcr d\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
}

void I8008Codegen::genMul(const ir::Expr &lhs, const ir::Expr &rhs) {
    genExpr(lhs);  // ac = multiplicand
    pushAc();
    genExpr(rhs);
    popInto("bc");       // bc = multiplicand (restored only now)
    copy16("dx", "ac");    // dx = multiplier
    genMulCore();
}

// Restoring shift-subtract (no hardware divide), 16 steps: `dx`
// holds the dividend and fills with quotient bits from the bottom as
// the dividend shifts out of the top into the remainder `cx`; `bc` is
// the divisor. A carry out of the remainder's shift means it exceeds
// any 16-bit divisor, so the subtraction is forced. Signed operands
// are divided as magnitudes; the quotient is negated when the signs
// differ and the remainder takes the dividend's sign.
void I8008Codegen::genDivModCore(bool wantRemainder, bool isUnsigned) {
    auto negate16 = [&](const std::string &cell) {
        pointAt(cell);
        code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
        pointAt(cell + "+1");
        code_ << "    mov a, m\n    xri 0xff\n    mov m, a\n";
        pointAt(cell);
        code_ << "    mov a, m\n    adi 1\n    mov m, a\n";
        pointAt(cell + "+1");
        code_ << "    mov a, m\n    aci 0\n    mov m, a\n";
    };
    auto isNeg = [&](const std::string &cell, const std::string &dstCell) {
        pointAt(cell + "+1");
        code_ << "    mov a, m\n    and a\n    ral\n";
        std::string Lneg = newLabel(), Ldone = newLabel();
        code_ << "    mvi a, 0\n";
        storeByte(dstCell);
        code_ << "    jtc " << Lneg << "\n";
        code_ << "    jmp " << Ldone << "\n";
        code_ << Lneg << ":\n    mvi a, 1\n";
        storeByte(dstCell);
        code_ << Ldone << ":\n";
    };
    auto negateIf = [&](const std::string &flagCell, const std::string &cell) {
        pointAt(flagCell);
        code_ << "    mov a, m\n    and a\n";
        std::string Lskip = newLabel();
        code_ << "    jtz " << Lskip << "\n";
        negate16(cell);
        code_ << Lskip << ":\n";
    };
    // `ptr`'s own low/high byte double as this operation's two 0/1
    // sign flags (dividend/divisor) - `ptr` itself is otherwise never
    // touched by this routine.
    if (!isUnsigned) {
        isNeg("dx", "ptr");
        isNeg("bc", "ptr+1");
        negateIf("ptr", "dx");
        negateIf("ptr+1", "bc");
    }
    storeImmediate16("cx", "0");
    code_ << "    mvi d, 16\n";
    std::string Lloop = newLabel(), Ldone = newLabel(), Lforce = newLabel();
    std::string Lcommit = newLabel(), Lnext = newLabel();
    auto trialSubtract = [&]() {  // e = low, a = high byte of cx - bc; CY = borrow
        pointAt("cx");
        code_ << "    mov a, m\n";
        pointAt("bc");
        code_ << "    sub m\n    mov e, a\n";
        pointAt("cx+1");
        code_ << "    mov a, m\n";
        pointAt("bc+1");
        code_ << "    sbb m\n";
    };
    code_ << Lloop << ":\n    mov a, d\n    and a\n";
    code_ << "    jtz " << Ldone << "\n";
    pointAt("dx");
    code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
    for (const char *cell : {"dx+1", "cx", "cx+1"}) {
        pointAt(cell);
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    }
    code_ << "    jtc " << Lforce << "\n";
    trialSubtract();
    code_ << "    jtc " << Lnext << "\n";
    code_ << "    jmp " << Lcommit << "\n";
    code_ << Lforce << ":\n";
    trialSubtract();
    code_ << Lcommit << ":\n    mov c, a\n";
    pointAt("cx");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt("cx+1");
    code_ << "    mov a, c\n    mov m, a\n";
    pointAt("dx");
    code_ << "    mov a, m\n    ori 1\n    mov m, a\n";
    code_ << Lnext << ":\n    dcr d\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        pointAt("ptr");
        code_ << "    mov a, m\n";
        pointAt("ptr+1");
        code_ << "    xor m\n";
        std::string Lskip = newLabel();
        code_ << "    jtz " << Lskip << "\n";
        negate16("dx");
        code_ << Lskip << ":\n";
        negateIf("ptr", "cx");
    }
    copy16("ac", wantRemainder ? "cx" : "dx");
}

void I8008Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);  // ac = dividend
    pushAc();
    genExpr(rhs);
    popInto("dx");        // dx = dividend (restored only now)
    copy16("bc", "ac");     // bc = divisor
    genDivModCore(wantRemainder, isUnsigned);
}

void I8008Codegen::genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned) {
    if (op == "==") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/true);
    if (op == "!=") return emitEqualityCompare(lhs, rhs, /*wantEqual=*/false);
    if (op == "<") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/false);
    if (op == ">") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/false);
    if (op == "<=") return emitLessCompare(rhs, lhs, isUnsigned, /*invert=*/true);
    if (op == ">=") return emitLessCompare(lhs, rhs, isUnsigned, /*invert=*/true);
    err(lhs.line, "internal: unhandled comparison operator '" + op + "'");
}

// `==`/`!=` need no sign handling: a-b (16 bits, borrow chained low into high) is zero iff a==b, checked by
// ORing the two result bytes together.
void I8008Codegen::emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual) {
    genExpr(lhs);
    pushAc();
    genExpr(rhs);
    popInto("bc");  // bc = lhs
    pointAt("bc");
    code_ << "    mov a, m\n";
    pointAt("ac");
    code_ << "    sub m\n";
    code_ << "    mov b, a\n";  // b = tentative low diff
    pointAt("bc+1");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    sbb m\n";
    code_ << "    or b\n";  // Z set iff both diff bytes zero
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (wantEqual ? "tz" : "fz") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

// Signed 16-bit "a < b" via the sign-bit-bias trick (the 8008 has no hardware overflow flag): flipping the sign
// bit of each operand's high byte maps the signed range onto the unsigned one, leaving just the final borrow to
// check.
void I8008Codegen::emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool isUnsigned, bool invert) {
    genExpr(a);
    pushAc();
    genExpr(b);
    popInto("bc");  // bc = a
    if (!isUnsigned) {
        pointAt("bc+1");
        code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";  // a's high byte, sign flipped (in place)
        pointAt("ac+1");
        code_ << "    mov a, m\n    xri 0x80\n    mov m, a\n";  // b's high byte, sign flipped (in place)
    }
    pointAt("bc");
    code_ << "    mov a, m\n";
    pointAt("ac");
    code_ << "    sub m\n";  // a'_lo - b'_lo, CY = borrow
    pointAt("bc+1");
    code_ << "    mov a, m\n";
    pointAt("ac+1");
    code_ << "    sbb m\n";  // a'_hi - b'_hi - borrow; CY now = (a < b) unbiased
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    j" << (invert ? "fc" : "tc") << " " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jtz " << Lfalse << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jtz " << Lfalse << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Lfalse << ":\n";
    storeImmediate16("ac", "0");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs) {
    std::string Lfalse = newLabel(), Ltrue = newLabel(), Ldone = newLabel();
    genExpr(lhs);
    testTruthy(lhs);
    code_ << "    jfz " << Ltrue << "\n";
    genExpr(rhs);
    testTruthy(rhs);
    code_ << "    jfz " << Ltrue << "\n";
    code_ << Lfalse << ":\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

void I8008Codegen::genLogicalNot(const ir::Expr &rhs) {
    genExpr(rhs);
    testTruthy(rhs);
    std::string Ltrue = newLabel(), Ldone = newLabel();
    code_ << "    jtz " << Ltrue << "\n";
    storeImmediate16("ac", "0");
    code_ << "    jmp " << Ldone << "\n";
    code_ << Ltrue << ":\n";
    storeImmediate16("ac", "1");
    code_ << Ldone << ":\n";
}

std::string I8008Codegen::buildAssembleCommand(const std::string &, const std::string &format, const std::string &,
                                                const std::string &) const {
    throw std::runtime_error("the 8008 backend has no external assembler - '" + format +
                              "' isn't a format assembleInProcess() supports (only 'bin' is)");
}

std::optional<std::string> I8008Codegen::assembleInProcess(const std::string &asmText,
                                                             const std::string &format) const {
    if (format == "raw") return assembleI8008(asmText);
    // `coff`/`elf`/`macho`/`aout`/`objraw`: the assembler builds one format-neutral ObjectFile, and the
    // requested format name just picks that format's FileFormatWriter to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI8008Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
