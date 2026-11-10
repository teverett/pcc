#pragma once

#include <functional>
#include <unordered_map>

#include "Linker.h"

namespace rcc {

// `siteFinalAddr` is only needed by relative patches (target - (site+N)); targets with
// non-additive combine semantics or non-contiguous fields supply their own patch functions instead.
using RelocPatchFn = std::function<void(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value,
                                         uint32_t siteFinalAddr)>;

// Reads back a relocation site's own pre-patch embedded address field (whatever a matching
// RelocPatchFn would otherwise combine `value` into) - only needed for `gcSections`, to find which
// of *this object's own* other symbols a "..": a reference baked as a raw same-object offset
// rather than a named symbol (see GenericLinker.h's own class comment) - actually targets, since
// that target's name isn't preserved past assembly. A relocation type with no entry here is
// assumed capable of targeting any other symbol in its own object with no way to tell which, so
// `gcSections` never splits an object that uses it - correctness over precision.
using AddrFieldExtractFn = std::function<uint32_t(const std::vector<uint8_t> &image, uint32_t siteOffset)>;

// Reusable patch kinds for plain 16/32-bit absolute and relative fields. A split hi/lo address
// load or a non-contiguous branch field needs its own dedicated kind instead.
namespace RelocKinds {
// *site += value.
void patchAbs16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// *site = value - (siteFinalAddr + 2).
void patchRel16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Big-endian counterpart of patchAbs16.
void patchAbs16BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// 32-bit counterpart of patchAbs16.
void patchAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// Big-endian counterpart of patchAbs32.
void patchAbs32BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
// *site = value - (siteFinalAddr + 4).
void patchRel32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr);
}  // namespace RelocKinds

// One target's fixed configuration for GenericLinker.
struct GenericLinkerConfig {
    // Base address objects are laid out from, in addressUnitBytes units (e.g. pdp7's word
    // address times 3).
    uint32_t baseAddress = 0;
    // Every input object's machineType must match this exactly, or merge() fails.
    uint16_t machineType = 0;
    // Pre-assembled entry sequence (zero what needs zeroing, call main, halt); GenericLinker
    // itself never assembles anything, only merges already-assembled bytes.
    ObjectFile entryObject;
    std::unordered_map<uint16_t, RelocPatchFn> relocPatchers;
    // Only consulted when `merge(objects, /*gcSections=*/true)` needs to resolve a "..".-typed
    // relocation's own real target (see AddrFieldExtractFn's own comment).
    std::unordered_map<uint16_t, AddrFieldExtractFn> addrFieldExtractors;
    std::vector<std::string> outputFormats;

    // How many packed .textBytes bytes make up one addressable/relocatable unit. `1` for a
    // byte-addressed target. A word-addressed target (e.g. pdp7: 3 packed bytes per 18-bit word)
    // still keeps every ObjSymbol::value/ObjRelocation::offset as a plain byte offset into
    // .textBytes; only the final patched *value* and any address exposed outside the working
    // image get converted from this packed-byte scale to the real addressable-unit scale.
    uint32_t addressUnitBytes = 1;

    // ObjectFile::unitBits to report. `8` for byte-addressed targets; only meaningful with
    // addressUnitBytes > 1 (pdp7: addressUnitBytes = 3, outputUnitBits = 18).
    uint16_t outputUnitBits = 8;

    // Start of this target's Harvard data address space in real hardware addressing; nonzero
    // only for a target that actually uses ObjectFile::dataSize/ObjSymbol::isData.
    uint32_t dataBaseAddress = 0;
};

// Single TargetLinker implementation shared by every target: validate -> lay out sequentially ->
// resolve relocations -> patch, configured per target via GenericLinkerConfig instead of a
// bespoke subclass per architecture.
//
// Reserved pseudo-symbol ".." is this object's own linked base address, added because each
// object assembles with `org 0`; never looked up in the global symbol table. "..d" is its
// Harvard-data-space counterpart, meaningless for a von-Neumann target.
//
// A defined symbol is checked for cross-object duplicates only if its name starts with
// `g_`/`p_`/`fn_` (pdp7: uppercase `G_`/`P_`/`FN_`); other same-named duplicates (local temps)
// are tolerated silently, first one wins.
class GenericLinker : public TargetLinker {
   public:
    explicit GenericLinker(GenericLinkerConfig config) : config_(std::move(config)) {}

    ObjectFile merge(const std::vector<ObjectFile> &objects, bool gcSections = false) const override;
    std::vector<std::string> outputFormats() const override { return config_.outputFormats; }

   private:
    GenericLinkerConfig config_;
};

}  // namespace rcc
