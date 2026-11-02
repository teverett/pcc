#pragma once

#include <functional>
#include <unordered_map>

#include "Linker.h"

namespace pcc {

// Patches one relocation in place, once its symbol's real linked
// value is known - `image` is the whole linked byte image (already
// the right final size), `siteOffset` is where in `image` to patch,
// `value` is the resolved symbol's own linked address, and
// `siteFinalAddr` is this patch site's own linked address (needed
// only by a relative-style patch, e.g. "target - (site+2)" - see
// RelocKinds::patchRel16 below). Shared across every GenericLinker-
// based target that uses the same relocation-kind family, instead of
// each target's own TargetLinker reimplementing the same patch
// arithmetic - a target with its own non-additive combine semantics
// (pdp7's OR/`+`/`-`) or an address load/branch field that isn't a
// plain contiguous N-bit integer supplies its own local patch
// functions instead (see e.g. Pdp7LinkerFactory.cpp's own
// `patchPdp7Or`/`Add`/`Sub`).
using RelocPatchFn = std::function<void(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value,
                                         uint32_t siteFinalAddr)>;

// Ready-made patch functions, grown as each new GenericLinker-based
// target needed one: plain 16/32-bit additive (absolute-address)
// patches in both byte orders, and the one relative-address shape
// (8086/i386's own "near call" convention) seen so far. A target with
// a real split hi16/lo16 address load (ARM/ArmM/stm32's `movw`/
// `movt`, mips/powerpc's `lui`/`ori`) or a PC-relative branch field
// that isn't a plain contiguous N-bit integer (ARM A32/Thumb-2's
// `B`/`BL`) needs its own dedicated kind instead - see each of those
// targets' own XLinker.cpp for ones not reusable here.
namespace RelocKinds {
// Little-endian 16-bit *additive* patch: `*site += value`. Used for
// an absolute address reference (the field already holds whatever
// constant offset the assembler added at the syllable level - see
// e.g. I8086Assembler.cpp's own `emitExprAsWord` comment).
void patchAbs16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Little-endian 16-bit relative patch: `*site = value - (siteFinalAddr + 2)` - a near call/jump whose own
// operand is "how far from the *end* of this 2-byte field" (see I8086Assembler.cpp's own `emitRel16` comment).
void patchRel16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Big-endian counterpart of patchAbs16 - for a target whose own
// assembler writes 16-bit fields high-byte-first (e.g. stm8's own
// `LDW`/"every multi-byte value here is big-endian" - see
// STM8Codegen.h's own header comment).
void patchAbs16BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Little-endian 32-bit *additive* patch - the 32-bit-target
// equivalent of patchAbs16 above, for a target whose own absolute
// address references (e.g. i386's flat `[label]`, ARM/ArmM/stm32's
// own hi/lo halves notwithstanding - see their own dedicated kinds
// below) are already a full 32-bit field written little-endian.
void patchAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Big-endian counterpart of patchAbs32 - for a target whose own
// assembler writes a 32-bit field high-byte-first (m68k/mc68328's
// `JSR <abs32>`, mips/powerpc's own 32-bit instruction words).
void patchAbs32BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Little-endian 32-bit relative patch: `*site = value - (siteFinalAddr + 4)` -
// i386's own CALL/JMP rel32 (E8/E9), where the operand is "how far
// from the *end* of this 4-byte field" (see I386Assembler.cpp's own
// `encodeBranch25`-adjacent comment for the exact convention this
// mirrors).
void patchRel32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
}  // namespace RelocKinds

// One target's own fixed configuration for GenericLinker - see that
// class's own header comment for what varies and what doesn't across
// every byte-addressed target that can share this one implementation.
struct GenericLinkerConfig {
    // Where the whole program loads and starts running, in the same
    // packed-byte scale every object's own `.textBytes` already uses
    // (see `addressUnitBytes` below) - every object is laid out
    // sequentially by byte starting here, same convention as every
    // existing bespoke TargetLinker. For a word-addressed target
    // (`addressUnitBytes > 1`), this is the real word address
    // multiplied by `addressUnitBytes` - e.g. pdp7's own
    // `kPdp7BaseAddress` (a word address) times 3.
    uint32_t baseAddress = 0;
    // This target's own COFF-style machine-id tag (see
    // CoffWriter.h's own header comment) - every input object's
    // `machineType` must match this exactly, or merge() fails loudly.
    uint16_t machineType = 0;
    // One already-assembled entry-sequence object (this target's own
    // "zero whatever needs zeroing, call main, halt" - see e.g.
    // I8086LinkerFactory.cpp's own `kEntryAsm`), built by calling this
    // target's own assembler *once*, by whoever constructs this
    // config (see e.g. Z80LinkerFactory.cpp) - GenericLinker
    // itself never assembles anything or knows any target's own
    // assembly syntax; it only ever merges already-assembled bytes.
    ObjectFile entryObject;
    // One patch function per relocation `type` value this target's
    // own assembler ever emits - see RelocKinds above for the common,
    // reusable ones.
    std::unordered_map<uint16_t, RelocPatchFn> relocPatchers;
    // Which --link --format values this target's own merge() output
    // can be written as (a FormatWriter whose own `unitBits`
    // expectation matches `outputUnitBits` below).
    std::vector<std::string> outputFormats;

    // How many raw `.textBytes` bytes make up one addressable,
    // relocatable unit this target's own symbol values/relocation
    // *resolved values* are ultimately expressed in - `1` (the
    // default) for every plain byte-addressed target, for which every
    // division by this value anywhere in merge() below is a no-op and
    // every existing byte-addressed target's own behavior is
    // completely unchanged. A word-addressed target packs more than
    // one byte per addressable unit into `.textBytes` itself (pdp7's
    // own 18-bit words, 3 packed bytes each, big-endian - see
    // Pdp7Assembler.cpp's own `buildObjectFile()`): every
    // `ObjSymbol::value`/`ObjRelocation::offset` ObjectFile already
    // carries is still, unconditionally, a plain BYTE offset into
    // that packed `.textBytes` (see each one's own comment in
    // ObjectFile.h) - so object *layout* (sequential by `.textBytes`
    // size) and the byte `idx` a relocation patches at need no change
    // at all for this case. Only the *value* finally handed to a
    // relocation's own patch function (a real word address, for
    // pdp7's own OR/`+`/`-` combine to operate on, not a byte
    // address), and every address this linker exposes outside its own
    // packed-byte working image (`LinkedImage::base`,
    // `LinkedImage::symbols`), need converting from this packed-byte
    // scale down to the real addressable-unit scale - see merge()'s
    // own comments for exactly where that conversion happens.
    uint32_t addressUnitBytes = 1;

    // What `LinkedImage::unitBits` this target's own merge() output
    // should report (see LinkedImage.h) - `8` (the default) for every
    // byte-addressed target. Only meaningful together with
    // `addressUnitBytes > 1` (pdp7's own 18-bit words: `addressUnitBytes
    // = 3`, `outputUnitBits = 18` - the packed bytes carry 24 bits,
    // but only the low 18 of each packed word are ever meaningful).
    uint16_t outputUnitBits = 8;

    // Where this target's own second, Harvard data address space
    // starts in real hardware addressing (e.g. MCS-51's XDATA bus
    // starts at 0; a PIC's own Access Bank GPR region starts wherever
    // its first general-purpose byte sits) - only ever non-zero for a
    // target whose own ObjectFile::dataSize/ObjSymbol::isData (see
    // ObjectFile.h's own comments) are actually in use. Every other,
    // von-Neumann target leaves this at its default `0` and never
    // produces an object with a nonzero `dataSize` or an `isData`
    // symbol in the first place, so merge()'s own data-space layout
    // pass below is a harmless no-op (every dataSize is 0, so every
    // dataBase[i] collapses to this same `0`, and no symbol or
    // relocation ever looks at it).
    uint32_t dataBaseAddress = 0;
};

// A single TargetLinker implementation reusable by every target in
// this project, byte- or word-addressed alike (see Linker.h's own
// header comment - there is no longer a target that needs its own
// bespoke TargetLinker subclass): merges objects the same way every
// existing one already does - "validate -> lay out sequentially ->
// resolve every relocation -> patch -> done" - but configured by a
// small per-target GenericLinkerConfig (see above) instead of a whole
// new C++ class per target. What genuinely has to differ by
// architecture is just relocation-patch *arithmetic* (a handful of
// local patch functions per target - see RelocKinds above for the
// common, reusable byte-addressed ones; a target with its own
// non-additive combine semantics, like pdp7's OR/`+`/`-`, or a split
// hi/lo address load, supplies its own, local to its own
// XLinkerFactory.cpp) plus a handful of constants and a tiny
// assembled entry snippet - everything else below is shared.
//
// The reserved pseudo-symbol ".." always means "this specific
// object's own linked base address" (in the same packed-byte scale as
// `GenericLinkerConfig::baseAddress` - see that field's own comment) -
// never looked up in the global symbol table: every object's own
// two-pass assembly runs with `org 0` in object mode (see e.g.
// Z80Assembler.cpp's own `allowExternal_`), so even a same-file
// address reference needs its own object's real final base added once
// linked, not just an external symbol's own address. A second
// reserved pseudo-symbol, `"..d"`, is its exact Harvard-data-space
// counterpart - "this specific object's own linked *data*-space
// base" - for a target whose own ObjSymbol::isData/ObjectFile::
// dataSize are in use (see ObjectFile.h's own comments); never looked
// up in the global symbol table either, and meaningless (never
// emitted) for a von-Neumann target.
//
// Cross-file symbol visibility: a defined symbol is only checked for
// cross-object duplicates if its name starts with `g_`/`p_`/`fn_` (or,
// for pdp7, the equivalent uppercase `G_`/`P_`/`FN_` - see
// Pdp7Codegen.h's own `label()`) - every backend's own `label()`
// already uses exactly this convention for Global/Param/Function
// Slots (see e.g. Z80Codegen.h's own `label()`) - every other same-
// named duplicate (a local temp/literal label, numbered fresh per
// compile) is tolerated silently, first one wins.
class GenericLinker : public TargetLinker {
   public:
    explicit GenericLinker(GenericLinkerConfig config) : config_(std::move(config)) {}

    LinkedImage merge(const std::vector<ObjectFile> &objects) const override;
    std::vector<std::string> outputFormats() const override { return config_.outputFormats; }

   private:
    GenericLinkerConfig config_;
};

}  // namespace pcc
