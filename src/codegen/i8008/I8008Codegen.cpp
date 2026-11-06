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

// Where this backend's own fixed memory cells live - `ac`/`bc`/`cx`/
// `dx`/`ptr`/`sp`/`fp`/`fnext`/`fpsp`, plus the two real runtime
// stacks `sp` and `fpsp` point into (see I8008Codegen.h's own header
// comment) - pinned to known, fixed addresses (via the assembler's
// own `name = expr` equate support, unlike every other backend's own
// `ac` etc., which are ordinary sequentially-laid-out temps) purely
// so tests/run_tests.sh's own from-scratch Python 8008 emulator can
// read the final result straight out of memory at a known address,
// without needing to parse this assembler's own symbol table at all.
// Comfortably above where this backend's own generated code and
// globals could ever reach for an ordinary test program, and
// comfortably inside the 8008's real 14-bit (16KB) address space.
constexpr uint32_t kPseudoBase = 0x3000;
// `wac`/`wbc`/`wtmp`/`wtmp2` - the wide (`long`/`long long`) value
// buffers (see I8008Codegen.h's own header comment on this
// mechanism), 8 bytes each, right after the other fixed pseudo-
// registers (`ac`=+0 .. `fpsp`=+16..17, 18 bytes total).
constexpr uint32_t kWideBase = kPseudoBase + 18;
// How many 16-bit values this backend's own subexpression-save
// stack (`sp`) can hold at once across the whole program - generous
// for ordinary test programs (every binary op, `*`, `/`, comparison,
// indexed/deref assignment, and function-call argument pushes at
// most one value for the lifetime of evaluating one subexpression),
// but a real, fixed limit with no overflow check - see pushCell()'s
// own comment.
[[maybe_unused]] constexpr int kStackSize = 32;  // documents the reserved region's size; nothing needs to read it
                                                  // back since the stack's own content is never initialized
constexpr uint32_t kStackBase = kWideBase + 32;
// How many nested `fp` saves the frame-pointer chain (`fpsp`) can
// hold at once - every save corresponds to exactly one real `CAL`
// still in flight, so this is bounded in practice by the 8008's own
// 7-level hardware call-nesting limit (see I8008Codegen.h's own
// header comment); comfortably larger than that real limit, with no
// overflow check of its own either.
constexpr int kFpStackCapacity = 16;
constexpr uint32_t kFpStackBase = kStackBase + kStackSize * 2;
// Where the per-call Local/Param frame bump allocator (`fnext`/`fp` -
// see genFunction()'s own comment) starts claiming words from - the
// last of this backend's own reserved regions, with no upper bound
// or overflow check of its own either, for the same reason as
// `kStackSize` above.
constexpr uint32_t kFrameBase = kFpStackBase + kFpStackCapacity * 2;
}  // namespace

// Pushes `srcCell`'s current value onto the real runtime stack
// pointed to by `spCell`: read `srcCell`'s two bytes into B/C first
// (computing `spCell`'s own value needs HL, which would otherwise
// clobber `srcCell` itself mid-read if the two happened to be the
// same cell - same reasoning as storeAcIndirect()), then write them
// at the current top of stack and advance `spCell` by one word.
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

// The reverse: retreats `spCell` by one word first, then reads the
// two bytes now at the top of that stack into `dstCell` - never
// touches any other cell unless `dstCell` happens to name it.
// Doesn't call storeHLToCell() - writing the decremented address
// back to `spCell` needs pointAt(spCell), which would repoint the
// real HL away from the very address this function still needs to
// read the popped word from, so the decremented address is stashed
// in D/E first instead and `spCell` is written from there once the
// read is already done.
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

// A one-byte version of pushCell()/popCell(), against the same `sp`
// stack - used by every wide (`long`/`long long`) helper below (see
// I8008Codegen.h's own header comment on that whole mechanism), which
// needs to save/restore individual bytes, not always whole 16-bit
// words. `incHL()`/`decHL()` already operate in units of 1 on the
// real H,L pair (not 2 - they're not word-stack-specific at all), so
// this is exactly pushCell()'s/popCell()'s own body with the second
// byte's own read/write simply dropped.
void I8008Codegen::pushByte(const std::string &srcCell) {
    loadByte(srcCell);
    code_ << "    mov b, a\n";
    loadHLFromCell("sp");
    code_ << "    mov a, b\n    mov m, a\n";
    incHL();
    storeHLToCell("sp");
}

void I8008Codegen::popByte(const std::string &dstCell) {
    loadHLFromCell("sp");
    decHL();
    code_ << "    mov a, l\n    mov d, a\n";
    code_ << "    mov a, h\n    mov e, a\n";
    code_ << "    mov a, m\n    mov b, a\n";
    pointAt("sp");
    code_ << "    mov a, d\n    mov m, a\n";
    pointAt("sp+1");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt(dstCell);
    code_ << "    mov a, b\n    mov m, a\n";
}

void I8008Codegen::pushWide(const char *base, int nbytes) {
    for (int i = 0; i < nbytes; i++) pushByte(byteAt(base, i));
}

void I8008Codegen::popWide(const char *base, int nbytes) {
    for (int i = nbytes - 1; i >= 0; i--) popByte(byteAt(base, i));
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
        // `pointAtSlot` clobbers A internally for a Local/Param - see
        // the narrow Assign/LocalDecl `char` cases' own comment on
        // this same hazard.
        loadByte(byteAt("wac", i));
        code_ << "    mov b, a\n";
        pointAtSlot(slot, i);
        code_ << "    mov a, b\n    mov m, a\n";
    }
}

// `ptr` already holds the target address (a runtime value, not a
// compile-time label) - reached through the real H,L pair via
// loadHLFromCell(), one byte at a time, advancing with incHL() (the
// only way this target can walk a *computed* address at all - see
// this file's own header comment).
// Every iteration re-derives H,L from the `ptr` cell instead of
// walking a live H,L across the whole loop (the way genWideLoadAtPtr/
// genWideStoreAtPtr's single-word callers do via loadHLFromCell()+
// incHL() alone) - storeByte()/loadByte() on the `wac` side each
// repoint H,L to a `wac+i` byte's own fixed address in between, so a
// live H,L left pointing at the pointee would already be clobbered
// by the time the next iteration's incHL() ran on it. `ptr`'s own
// stored value is advanced in memory instead (addConstToCell()),
// which survives that repointing just fine.
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

// A literal element's own decimal text, or a symbol element's own
// label text (the referenced Slot's or Function's own address) - the
// shared assembler's own "dw" directive (I8008Assembler.cpp's emit16)
// resolves either one into a real, full 16-bit value and writes it
// out low byte then high byte, the same split a `jmp`/`cal` operand
// already gets.
std::string I8008Codegen::arrayElemText(const ir::ArrayInitElem &e) {
    if (!e.isSymbol) return std::to_string(e.value);
    return e.slot ? label(e.slot) : label(e.func);
}

// See ../m6502/M6502Codegen.cpp's own version of this same function
// for the general idea - a `char`-width element is one plain `db`
// byte; a wide (`long`/`long long`) one is split into plain bytes
// directly (little-endian), since this dialect's shared "dw"
// directive only ever splits a 16-bit value.
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

// A real 16-bit increment of the HL *register pair* itself (not a
// memory cell - see copy16()/pushAc() for that) - the 8008 has no
// 16-bit increment instruction at all, so this chains two 8-bit
// `INR`s through a carry test, the same workaround the 6502 backend's
// own inc16() needs for the same reason (see M6502Codegen.cpp).
void I8008Codegen::incHL() {
    std::string Lskip = newLabel();
    code_ << "    inr l\n";
    code_ << "    mov a, l\n    and a\n";  // Z set iff l wrapped to 0
    code_ << "    jfz " << Lskip << "\n";  // didn't wrap - skip the high-byte carry
    code_ << "    inr h\n";
    code_ << Lskip << ":\n";
}

// Copies one 16-bit memory cell to another - four real memory
// accesses (through B/C, since there's no way to read src's two
// bytes and write dst's two bytes without repointing HL in between -
// see this file's own header comment), not the one or two
// instructions every other backend's own copy16() needs.
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

// Adds `byteOffset` to the real H,L register pair in place: low byte
// through a plain ADI (also the only source of a possible carry
// out), high byte through ACI, so the carry an ADI leaves behind
// always lands correctly in the high byte's own sum, even when
// `byteOffset`'s own high byte is 0.
void I8008Codegen::addConstToHL(int byteOffset) {
    int lo = byteOffset & 0xFF;
    int hi = (byteOffset >> 8) & 0xFF;
    code_ << "    mov a, l\n    adi " << lo << "\n    mov l, a\n";
    code_ << "    mov a, h\n    aci " << hi << "\n    mov h, a\n";
}

// The same add, against a named 16-bit memory cell instead of the
// real H,L pair - same ADI/ACI carry chain, through the accumulator,
// since there's no way to add to a memory operand directly (every
// ALU instruction here takes its second operand from a register or
// `m`, never an immediate *and* a memory destination at once).
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

void I8008Codegen::copy16FromSlot(const std::string &dst, const ir::Slot *slot) {
    pointAtSlot(slot, 0);
    code_ << "    mov a, m\n    mov b, a\n";
    pointAtSlot(slot, 1);
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
    pointAtSlot(slot, 1);
    code_ << "    mov a, c\n    mov m, a\n";
}

void I8008Codegen::storeImmediate16ToSlot(const ir::Slot *slot, int elemOffset, const std::string &exprText) {
    pointAtSlot(slot, elemOffset);
    code_ << "    mvi a, " << exprText << "\n    mov m, a\n";
    pointAtSlot(slot, elemOffset + 1);
    code_ << "    mvihi a, " << exprText << "\n    mov m, a\n";
}

// Pops the top of the `sp` stack straight into `slot`'s own storage -
// the same body as popCell(), with the final write-back aimed at
// pointAtSlot() instead of a plain named cell's own pointAt().
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
        pointAtSlot(slot, 1);
        code_ << "    mov a, c\n    mov m, a\n";
        return;
    }
    // A wide (`long`/`long long`) param - popped one byte at a time,
    // highest byte first (the real `sp` stack's own LIFO order,
    // matching how the caller's own `Call` case pushed it - see that
    // case's own comment), straight into this slot's own frame bytes.
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

// Gives this call's own frame back to the bump allocator and
// restores the caller's own `fp` - see genFunction()'s own comment.
void I8008Codegen::emitEpilogue() {
    copy16("fnext", "fp");
    popCell("fp", "fpsp");
}

// One in-place 1-bit rotate of the 16-bit memory cell `lbl`, through
// the accumulator (the 8008's only rotates - RLC/RRC/RAL/RAR - work
// on A alone, unlike the Z80 backend's own register-targeted `RL r`/
// `RR r` - see I8008Codegen.h's own header comment). `and a` is this
// target's own idiom for "clear carry without disturbing A" (every
// logical instruction clears CY as a side effect - confirmed against
// NDA/XRA/ORA's own documented "C=0" flag behavior).
void I8008Codegen::shiftOneBit(const std::string &lbl, bool isLeft) {
    if (isLeft) {
        pointAt(lbl);
        code_ << "    mov a, m\n    and a\n    ral\n    mov m, a\n";
        pointAt(lbl + "+1");
        code_ << "    mov a, m\n    ral\n    mov m, a\n";
    } else {
        // Logical (unsigned) shift - same convention, and the same
        // reason (no unsigned type to make the distinction with), as
        // every other backend's own `>>`. High byte first, so its
        // own vacated-bit carry-out feeds the low byte's rotate.
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
    // Real separate compilation (`--format=coff`/`elf`/`macho`/
    // `aout` from a single file - see IRBuilder.h's `allowExternal`)
    // means `prog` may not define `main` at all. `objectMode` (see
    // Codegen.h) means no object file ever emits the entry sequence
    // itself, even the one that happens to define `main` -
    // I8008LinkerFactory.cpp is solely responsible for synthesizing
    // exactly one, once every object is merged and `main`'s linked
    // address is resolvable - same pattern Z80Codegen.cpp/
    // M6502Codegen.cpp's own `objectMode` comment documents.
    code_ << "org 0\n\n";
    // The fixed memory pseudo-registers and the runtime software
    // stack's own pointer cell - see this file's own anonymous-
    // namespace comment on `kPseudoBase`/`kStackBase`. These are
    // compile-time `name = expr` equates (a plain constant, the same
    // in every object regardless of where it's linked - see
    // I8008Assembler.h's own header comment), so - unlike a real
    // backend's own per-object *storage* cells (z80's own `ac` etc. -
    // see Z80Codegen.cpp's own `generate()` comment) - every object
    // declares them identically and harmlessly; no `objectMode`
    // special-casing needed here at all.
    code_ << "ac = " << kPseudoBase << "\n";
    code_ << "bc = " << (kPseudoBase + 2) << "\n";
    code_ << "cx = " << (kPseudoBase + 4) << "\n";
    code_ << "dx = " << (kPseudoBase + 6) << "\n";
    code_ << "ptr = " << (kPseudoBase + 8) << "\n";
    code_ << "sp = " << (kPseudoBase + 10) << "\n";
    // `fp` holds the *current* call's own frame base address; `fnext`
    // the bump allocator's own next-free-word pointer; `fpsp` the
    // top-of-stack pointer for the chain of saved `fp`s a nested call
    // restores from on return (see genFunction()'s own comment and
    // I8008Codegen.h's own header comment). `fp` itself needs no
    // initial value here - the first function ever entered (`main`)
    // overwrites it from `fnext` before reading it at all.
    code_ << "fp = " << (kPseudoBase + 12) << "\n";
    code_ << "fnext = " << (kPseudoBase + 14) << "\n";
    code_ << "fpsp = " << (kPseudoBase + 16) << "\n";
    code_ << "wac = " << kWideBase << "\n";
    code_ << "wbc = " << (kWideBase + 8) << "\n";
    code_ << "wtmp = " << (kWideBase + 16) << "\n";
    code_ << "wtmp2 = " << (kWideBase + 24) << "\n";
    if (!objectMode) {
        code_ << "\n_start:\n";
        // The subexpression-save stack grows up from `kStackBase`;
        // `sp` always holds the address *just past* the last pushed
        // word (see pushCell()/popCell()). The frame-pointer chain
        // grows up from `kFpStackBase` the same way, via `fpsp`; the
        // frame bump allocator starts out with its own next-free word
        // at `kFrameBase`.
        storeImmediate16("sp", std::to_string(kStackBase));
        storeImmediate16("fpsp", std::to_string(kFpStackBase));
        storeImmediate16("fnext", std::to_string(kFrameBase));
        code_ << "    cal " << label(mainFn) << "\n";
        // The program's own final result sits in `ac` the moment
        // execution reaches `halt` below - the authoritative (and, on
        // this backend, only practical - there's no register wide enough
        // to dump the way every other backend's own halt sequence
        // leaves one) place to read it from.
        code_ << "halt:\n    jmp halt\n\n";
    }

    for (const auto &fb : prog.functionBodies) genFunction(fb);

    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        // An external global (real separate compilation - see IR.h's
        // `isExternal`/IRBuilder.cpp's resolveExternalVar) isn't
        // defined in this file at all - only ever referenced by name,
        // letting the assembler's own undefined-symbol handling defer
        // it to a relocation.
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

// Single-file-only (see I8008Codegen.h's own header comment: no
// `--format=coff`) - the whole generated text is simply attributed
// to whichever file defines `main`, same as every other single-file
// backend's own perFileAsm().
std::map<std::string, std::string> I8008Codegen::perFileAsm() const { return perFileAsm_; }

// A function's own prologue builds this call's own fresh Local/Param
// frame: saves the caller's `fp` onto the `fpsp` chain, takes the
// bump allocator's current `fnext` as this call's own frame base,
// claims this function's own whole word count
// (`params.size() + frameSize`) by bumping `fnext` past it, then
// pops each of its own incoming argument values - pushed by the
// caller, in argument order, onto the `sp` stack (see genExpr()'s
// own Call case) - straight into that fresh frame, in reverse
// (`sp` is a stack: the last argument pushed is the first one
// popped, so the loop below walks `params` back to front). Every
// Local/Param access for the rest of this function's own body (see
// pointAtSlot()) is relative to `fp`, which this prologue is the only
// place that ever changes - `currentParamCount_` records this
// function's own param count for wordIndexOf() to use throughout.
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
    genStmt(*fn.body);
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
            emitEpilogue();
            code_ << "    ret\n";
            return;
        }
        case ir::StmtKind::LocalDecl: {
            // A Local's own storage is this call's own frame (see
            // genFunction()'s own comment) - never a fixed, compile-
            // time label the way a Global's is - so, unlike a Global
            // array's own literal initializer (baked into the binary
            // once, in generate()'s own loop below), a Local array's
            // own initializer is written out at runtime, every time
            // this declaration is reached (so a declaration inside a
            // loop body re-initializes its own array each iteration,
            // the same call's own frame being reused for every one of
            // them).
            if (s.declSlot->arraySize > 0) {
                int w = widthOfType(s.declSlot->type);
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    const ir::ArrayInitElem &elem = s.declSlot->arrayInit[i];
                    if (w <= 2) {
                        storeImmediate16ToSlot(s.declSlot, w * static_cast<int>(i), arrayElemText(elem));
                    } else {
                        // A wide element's own literal is never a
                        // symbol (see declareArray()'s own comment) -
                        // split directly into `w` plain bytes.
                        auto uv = static_cast<unsigned long>(elem.value);
                        for (int b = 0; b < w; b++) {
                            // `pointAtSlot` has to run BEFORE the
                            // value is loaded into A, not after - see
                            // genWideStore()'s own comment on this
                            // same hazard.
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
                // `pointAtSlot` clobbers A internally for a Local/Param
                // (it computes `fp + offset` through it) - see the
                // indexed-assign case's own comment just above.
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
            // Stores through at e.type's own width - the pointee type
            // of a real typed pointer, or plain native-word width for
            // the still-supported untyped-int-as-address style - see
            // IRBuilder.cpp's own DerefAssign comment. Mirrors the
            // indexed Assign's own identical width dispatch above,
            // with `ptr` set directly from the pointer value instead
            // of genIndexAddr().
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
            // Plain scalar target - its own storage (a Global's fixed
            // label, or fp-relative for a Local/Param, via
            // pointAtSlot()) is reconstructible at zero cost after rhs
            // evaluates: unlike ptr/ac/bc/cx/dx, `fp` itself is
            // restored by any nested call's own epilogue before that
            // call ever returns (see emitEpilogue()), so there's
            // nothing that needs holding across rhs's own evaluation
            // here at all.
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
                // Loads at e.type's own width - see DerefAssign's
                // identical comment above. Mirrors genIndex()'s own
                // identical width dispatch, with `ptr` set directly
                // from the pointer value instead of genIndexAddr().
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
            // Each argument is pushed onto the real `sp` stack, in
            // argument order, rather than written through any cell
            // named after the callee's own params - a shared per-
            // function cell would break the moment this same call
            // site's own later argument's own evaluation makes
            // another call to the same function (direct recursion)
            // before this call's `cal` below ever runs, overwriting
            // the earlier argument's own cell before it's even been
            // used. The callee's own prologue (genFunction()) pops
            // every one of these, in reverse, straight into that
            // call's own fresh frame. A wide argument pushes its own
            // real width in bytes (ascending, byte 0 first); a narrow
            // one always pushes exactly 2 bytes via pushAc() (see
            // IRBuilder's own call-argument conversion, promoting
            // anything narrower than `int` up to it first).
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
            // e.args is always empty here - IRBuilder.cpp rejects any
            // argument on this kind before it ever reaches a backend
            // (see IR.h's ExprKind::CallPtr). The 8008 has no
            // indirect call or jump of any kind - every real `CAL`/
            // `JMP` operand is a fixed 14-bit address baked into the
            // instruction's own bytes at assemble time, and there's
            // no "jump to the address in a register pair" instruction
            // the way the 8080/Z80 have `PCHL`/`JP (HL)` (see
            // I8008Codegen.h's own header comment) - so a computed
            // call is instead emulated with self-modifying code: the
            // placeholder `cal 0` below is a real CAL instruction
            // whose own operand bytes (`Lcal+1`/`Lcal+2`, immediately
            // following its opcode byte at `Lcal`) get overwritten
            // with the callee's own address - already computed into
            // `ac` - right before the CPU falls through and executes
            // it. The instruction the CPU then fetches is a perfectly
            // ordinary CAL to that address; the real internal
            // hardware call stack handles the return exactly as it
            // would for `cal <label>`. Safe because this backend has
            // no interrupts and no concurrency (see I8008Codegen.h's
            // own header comment) - nothing can observe or race the
            // placeholder bytes between the patch and the CAL that
            // immediately follows it, even across recursive calls
            // through this same call site.
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
            err(e.line,
                "the 8008 has no interrupt enable/disable instruction at all (interrupts work by an external "
                "signal forcing an instruction onto the bus, not a software EI/DI the way every later chip in "
                "this family has one) - _ion()/_iof() aren't supported on this target, see I8008Codegen.h's own "
                "header comment");
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

// Leaves slot[idxExpr]'s own effective address in the `ptr` memory
// cell. `idxExpr` is always evaluated first, and alone - see
// ../i8080/I8080Codegen.cpp's own genIndexAddr() comment on why.
// `slot->arraySize > 0` means the base is the array's own first
// word - a compile-time-known address for a Global, loaded via
// `mvi`/`mvihi` directly (same split-byte trick as everywhere else in
// this backend), or a runtime one (`fp` plus a word offset) for a
// Local; otherwise `slot` is a plain pointer variable and the base
// instead comes from *reading* that variable's own stored value
// (again either a Global's fixed address or a Local/Param's own
// runtime one - see pointAtSlot()). Either way there's no hardware
// 16-bit add here (no `DAD` - see I8008Codegen.h's own header
// comment), so the doubled index and the base are added byte-wise
// through the accumulator with the carry chained from low into high.
// A Local array's own element `i` therefore sits at this same base
// *plus* `i` words, the array's own word index growing the same
// direction as its own element index - unlike a frame scheme that
// addresses a Local by a *negative* offset from its own frame
// pointer (where element `i` would sit at the array's base *minus*
// `i` words instead - see I386Codegen.h's own `addr()` comment),
// this backend's own `fp` is the start, not the middle, of each
// call's own frame (see genFunction()'s own comment), so every
// Local/Param address here - scalar or array element alike - only
// ever adds a non-negative word offset to it, never subtracts one.
void I8008Codegen::genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr) {
    genExpr(idxExpr);  // ac = index
    // ptr = index * widthOf(slot->type) (repeated doubling, via self-
    // add - see elemShift()), same reason every other backend's own
    // indexing scales the index first.
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
        // A Global array's own address is a compile-time constant -
        // loaded directly into B,C, same immediate split-byte trick
        // as everywhere else in this backend (no need to round-trip
        // it through H,L and `m` first).
        std::string lbl = label(slot);
        code_ << "    mvi b, " << lbl << "\n";     // b = low byte of the array's own address
        code_ << "    mvihi c, " << lbl << "\n";    // c = high byte
    } else if (slot->arraySize > 0) {
        // A Local array's own address is this call's own frame plus
        // a runtime offset (see pointAtSlot()) - computed into H,L
        // first, then copied into B,C, since B,C (not H,L) is where
        // the rest of this function needs the base.
        pointAtSlot(slot, 0);
        code_ << "    mov a, l\n    mov b, a\n";
        code_ << "    mov a, h\n    mov c, a\n";
    } else {
        // `slot` is a plain pointer variable - the base comes from
        // *reading* its own stored value (Global, Local, or Param -
        // pointAtSlot() handles all three).
        pointAtSlot(slot, 0);
        code_ << "    mov a, m\n    mov b, a\n";
        pointAtSlot(slot, 1);
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
        // `loadHLFromCell` clobbers A internally (it reads `ptr`'s own
        // two stored bytes through it) - the value to store has to
        // survive in B across that call, the same reason copy16()/
        // storeAcIndirect() never read a value straight into A right
        // before repointing H,L.
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

// `op` applied to ac=old, bc=rhs, leaving the raw new value in ac -
// every instruction here is the exact same mnemonic/operand shape
// genBinary()'s own per-operator case already emits (see that
// method), with only ac/bc's own roles swapped where the operator
// isn't commutative (old plays genBinary()'s own "first/lhs operand"
// role here, rhs its "second/rhs operand" role - see this method's
// own header comment in I8008Codegen.h).
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
    // "<<" or ">>" - genShiftCore() operates on bc=value/cx=count, the
    // opposite of this method's own ac=old/bc=rhs(count) convention,
    // so the two are swapped into place first.
    copy16("cx", "bc");  // cx = shift count (rhs)
    copy16("bc", "ac");  // bc = value to shift (old)
    genShiftCore(op == "<<", isUnsigned);
}

// `ptr` must already hold the resolved address on entry (see
// genIndexAddr()/the Assign indexed case, or copy16("ptr","ac") off a
// dereferenced pointer value, for the two ways a caller gets there).
// `e.rhs`'s own evaluation can recursively reach arbitrarily complex
// codegen (calls, nested expressions) that's free to clobber every
// one of ptr/ac/bc/cx/dx - exactly the reason Assign's own indexed
// case and DerefAssign already stash `ptr` on the real `sp` stack
// across it rather than trusting it to survive in place;
// genCompoundOp()'s own "/"/"%" case reaches back into `ptr` too (see
// its own header comment), so the address is re-stashed a second time
// around that call as well.
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
            // A Local/Param's own address is only known at runtime
            // (this call's own `fp` plus a word offset - see
            // pointAtSlot()), never a compile-time immediate the way
            // a Global's is.
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

// Every binary op here saves its left operand across evaluating the
// right one on the real runtime stack (pushAc()/popInto() - see
// I8008Codegen.h's own header comment on why a compile-time-indexed
// save cell doesn't work once a real function call is involved).
// An implicit or explicit type conversion - see
// ../m6502/M6502Codegen.cpp's own version of this same function for
// the full rationale (identical here, just reached through this
// backend's own `pointAt`/`loadByte`/`storeByte` idiom in place of a
// direct "lda"/"sta").
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

void I8008Codegen::emitWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, int words, bool wantEqual) {
    int n = 2 * words;
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = lhs, wac = rhs
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

// Scans byte-wise most-significant first via a single chained
// subtract (one `sub`, then `sbb` per later byte - the borrow
// propagates automatically), the direct byte-level generalization of
// ../i8080/I8080Codegen.h's own 16-bit sign-bit-bias trick: flipping
// the top bit of only the MOST significant byte (not every byte) and
// subtracting maps the signed range onto the unsigned one the same
// way, since every lower byte is already comparing a pure magnitude
// once the top byte's own sign has been accounted for.
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

// A `long`/`long long` shifted by a runtime count - loops one bit at
// a time across all `n` bytes (ASL/ROL-equivalent chained through
// the accumulator, since RAL/RAR only ever work on A itself - see
// I8008Codegen.h's own header comment on shiftOneBit()). A signed
// right shift re-derives the current sign bit into the carry before
// the top byte's own rotate, instead of a fixed 0-fill - the same
// trick genShift() uses for the narrow case.
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

// Textbook shift-and-add multiply, `n*8` bits serially - the wide
// generalization of genMul() (see that function's own comment).
// Needs no signedness at all (see M6502Codegen.cpp's own comment on
// why a truncated multiply's own bit pattern never depends on it).
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

void I8008Codegen::genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned,
                                  int words) {
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
    genExpr(lhs);
    pushWide("wac", n);
    genExpr(rhs);
    popWide("wbc", n);  // wbc = dividend, wac = divisor
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

// No variable-count shift instruction (see I8008Codegen.h's own
// header comment) - loop `rhs` times instead, one bit at a time, via
// shiftOneBit(). A signed right shift re-derives the current sign
// bit into the carry before each 1-bit rotate, instead of
// shiftOneBit()'s own fixed 0-fill.
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
    copy16("cx", "ac");  // stash the count in a memory cell, not a register - popInto() below
                          // uses every one of A/B/C/D/E/H/L as its own scratch internally
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

// Repeated subtraction (no hardware divide): `dx` is the running
// remainder (starts as the dividend), `bc` the fixed divisor, `ac`
// the quotient, counting how many times `bc` was subtracted out of
// `dx` before it would've gone negative. Only verified against
// non-negative operands.
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
    storeImmediate16("ac", "0");
    std::string Lloop = newLabel(), Ldone = newLabel();
    code_ << Lloop << ":\n";
    // Tentative dx - bc; carry (borrow) set afterward means dx < bc,
    // so this round's subtraction can't happen - stop.
    pointAt("dx");
    code_ << "    mov a, m\n";
    pointAt("bc");
    code_ << "    sub m\n";
    code_ << "    mov e, a\n";  // e = tentative low result
    pointAt("dx+1");
    code_ << "    mov a, m\n";
    pointAt("bc+1");
    code_ << "    sbb m\n";  // a = tentative high result; CY = final borrow
    code_ << "    jtc " << Ldone << "\n";
    // Commit: dx -= bc (the tentative result just computed).
    code_ << "    mov d, a\n";
    pointAt("dx");
    code_ << "    mov a, e\n    mov m, a\n";
    pointAt("dx+1");
    code_ << "    mov a, d\n    mov m, a\n";
    // Quotient += 1 - a 16-bit increment of the `ac` memory cell via
    // a carry-chained ADI/ACI (not incHL() - that's only for the
    // real HL register pair, see this file's own header comment).
    pointAt("ac");
    code_ << "    mov a, m\n    adi 1\n    mov m, a\n";
    pointAt("ac+1");
    code_ << "    mov a, m\n    aci 0\n    mov m, a\n";
    code_ << "    jmp " << Lloop << "\n";
    code_ << Ldone << ":\n";
    if (!isUnsigned) {
        pointAt("ptr");
        code_ << "    mov a, m\n";
        pointAt("ptr+1");
        code_ << "    xor m\n";
        std::string Lskip = newLabel();
        code_ << "    jtz " << Lskip << "\n";
        negate16("ac");
        code_ << Lskip << ":\n";
        negateIf("ptr", "dx");
    }
    if (wantRemainder) copy16("ac", "dx");
}

void I8008Codegen::genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned) {
    genExpr(lhs);  // ac = dividend
    pushAc();
    genExpr(rhs);
    popInto("dx");        // dx = dividend (running remainder, restored only now)
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

// `==`/`!=` need no sign handling at all: a-b (full 16 bits, borrow
// chained from low into high byte) is zero iff a==b, checked the
// same "OR the two result bytes together" way genLogicalNot() checks
// a value for zero.
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

// Signed 16-bit "a < b" via the standard bias trick (see
// ../i8080/I8080Codegen.h's own header comment: the 8008, like the
// 8080, has no hardware overflow flag) - flipping the sign bit of
// each operand's high byte, in place, maps the signed range onto the
// unsigned one while preserving order, and the final borrow out of a
// plain byte-wise subtract is all that's left to check.
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
    // `coff`/`elf`/`macho`/`aout`/`objraw`: real separate compilation
    // (see generate()'s own `objectMode` comment) - this backend's
    // own assembler builds one format-neutral ObjectFile exactly once
    // (see I8008Assembler.h's own `assembleI8008Object`), and
    // whichever of these five names was asked for just picks that
    // format's own FileFormatWriter (see
    // fileformat/FileFormat.h) to serialize it.
    if (auto kind = fileFormatKindFromName(format)) {
        ObjectFile obj = assembleI8008Object(asmText);
        return makeFileFormatWriter(*kind)->write(obj);
    }
    return std::nullopt;
}

}  // namespace pcc
