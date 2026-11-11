#include "GenericLinker.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rcc {

namespace {
[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("GenericLinker: " + msg); }

void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}
void put16BE(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>((v >> 8) & 0xFF);
    image[idx + 1] = static_cast<uint8_t>(v & 0xFF);
}
uint16_t get16BE(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>((static_cast<uint16_t>(static_cast<uint8_t>(image[idx])) << 8) |
                                  static_cast<uint8_t>(image[idx + 1]));
}
void put32(std::vector<uint8_t> &image, size_t idx, uint32_t v) {
    put16(image, idx, static_cast<uint16_t>(v & 0xFFFF));
    put16(image, idx + 2, static_cast<uint16_t>((v >> 16) & 0xFFFF));
}
uint32_t get32(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint32_t>(get16(image, idx)) | (static_cast<uint32_t>(get16(image, idx + 2)) << 16);
}
void put32BE(std::vector<uint8_t> &image, size_t idx, uint32_t v) {
    put16BE(image, idx, static_cast<uint16_t>((v >> 16) & 0xFFFF));
    put16BE(image, idx + 2, static_cast<uint16_t>(v & 0xFFFF));
}
uint32_t get32BE(const std::vector<uint8_t> &image, size_t idx) {
    return (static_cast<uint32_t>(get16BE(image, idx)) << 16) | get16BE(image, idx + 2);
}
}  // namespace

namespace RelocKinds {
void patchAbs16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put16(image, siteOffset, static_cast<uint16_t>(get16(image, siteOffset) + value));
}
void patchRel16(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr + 2);
    put16(image, siteOffset, static_cast<uint16_t>(rel));
}
void patchAbs16BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put16BE(image, siteOffset, static_cast<uint16_t>(get16BE(image, siteOffset) + value));
}
void patchAbs32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32(image, siteOffset, get32(image, siteOffset) + value);
}
void patchAbs32BE(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    put32BE(image, siteOffset, get32BE(image, siteOffset) + value);
}
void patchRel32(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t siteFinalAddr) {
    int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinalAddr + 4);
    put32(image, siteOffset, static_cast<uint32_t>(rel));
}
}  // namespace RelocKinds

namespace {
bool isGlobalName(const std::string &name) {
    return name.rfind("g_", 0) == 0 || name.rfind("p_", 0) == 0 || name.rfind("fn_", 0) == 0 ||
           name.rfind("G_", 0) == 0 || name.rfind("P_", 0) == 0 || name.rfind("FN_", 0) == 0;
}

// One contiguous run of an object's own `.text`, delimited by that object's own cross-file-visible
// (`isGlobalName`) symbol values - never split in the middle of a function/global, since only
// those symbols (never a backend-internal local label) start a new section. `[start, end)` is in
// that object's own pre-link byte offsets.
struct Section {
    uint32_t start = 0;
    uint32_t end = 0;
};

// False for object 0 (the entry sequence - always included whole, never worth splitting) or for
// an object using a same-object-offset relocation (".."/"..d") this target gives no
// AddrFieldExtractFn for: without one, a ".." relocation's real target can't be told apart from
// any other symbol in the same object (see AddrFieldExtractFn's own comment), so the whole object
// has to stay one section to stay correct. "..d" never gets an extractor at all (it addresses the
// Harvard data space, which this pass doesn't carve into sections to begin with).
bool canSplit(const std::vector<ObjectFile> &objects, const GenericLinkerConfig &config, size_t i) {
    if (i == 0) return false;
    for (const auto &r : objects[i].relocations) {
        if (r.symbolName == "..d") return false;
        if (r.symbolName == ".." && config.addrFieldExtractors.find(r.type) == config.addrFieldExtractors.end()) {
            return false;
        }
    }
    return true;
}

// Every object's own `.text` carved into Sections at its own global-symbol boundaries (always at
// least one, covering the whole file, even with zero global symbols in it) - unless `canSplit`
// says not to, in which case the whole file is one Section, same as a non-gc link.
std::vector<std::vector<Section>> sectionsOf(const std::vector<ObjectFile> &objects,
                                              const GenericLinkerConfig &config) {
    std::vector<std::vector<Section>> result(objects.size());
    for (size_t i = 0; i < objects.size(); i++) {
        uint32_t textSize = static_cast<uint32_t>(objects[i].textBytes.size());
        std::vector<uint32_t> bounds;
        if (canSplit(objects, config, i)) {
            for (const auto &sym : objects[i].symbols) {
                if (sym.isDefined && !sym.isData && isGlobalName(sym.name)) bounds.push_back(sym.value);
            }
            std::sort(bounds.begin(), bounds.end());
            bounds.erase(std::unique(bounds.begin(), bounds.end()), bounds.end());
        }
        if (bounds.empty() || bounds.front() != 0) bounds.insert(bounds.begin(), 0);
        for (size_t b = 0; b < bounds.size(); b++) {
            uint32_t end = (b + 1 < bounds.size()) ? bounds[b + 1] : textSize;
            if (bounds[b] >= end) continue;  // a duplicate/trailing boundary at the very end
            result[i].push_back({bounds[b], end});
        }
    }
    return result;
}

// Index of the section (within `sections[obj]`) whose `[start,end)` contains `value`, or
// `sections[obj].size()` (not found - value is past the last section, e.g. textSize itself).
size_t sectionContaining(const std::vector<Section> &sections, uint32_t value) {
    for (size_t s = 0; s < sections.size(); s++) {
        if (value >= sections[s].start && value < sections[s].end) return s;
    }
    return sections.size();
}
}  // namespace

ObjectFile GenericLinker::merge(const std::vector<ObjectFile> &userObjects, bool gcSections) const {
    if (userObjects.empty()) fail("no object files given");
    for (const auto &obj : userObjects) {
        // machineType == 0 is tolerated: a.out objects have no machine-id field, so it reads as 0.
        if (obj.machineType != 0 && obj.machineType != config_.machineType) {
            std::ostringstream msg;
            msg << "object's machine type 0x" << std::hex << obj.machineType << " doesn't match this target's own 0x"
                << config_.machineType;
            fail(msg.str());
        }
    }

    std::vector<ObjectFile> objects;
    objects.push_back(config_.entryObject);
    objects.insert(objects.end(), userObjects.begin(), userObjects.end());

    // Object 0 (the entry sequence) is always included whole, same as a non-gc link: it has no
    // visibility into what the real objects need, so it can't be pruned by this same mechanism.
    // `included[i]` marks, per object, which of `sections[i]` survive; absent gcSections every
    // section of every object is kept, reproducing the old whole-object behavior exactly.
    std::vector<std::vector<Section>> sections = sectionsOf(objects, config_);
    std::vector<std::vector<bool>> included(objects.size());
    for (size_t i = 0; i < objects.size(); i++) included[i].assign(sections[i].size(), !gcSections || i == 0);

    // defSection[name] = (object index, section index) of whichever object/section defines that
    // symbol - every defined symbol, not just the ones gcSections ultimately keeps, since a
    // relocation needs to look its own target up by name regardless of whether that name is
    // itself a section boundary. Built even when !gcSections: cheap, and the patch loop below
    // uses it for named relocations either way.
    std::unordered_map<std::string, std::pair<size_t, size_t>> defSection;
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &sym : objects[i].symbols) {
            if (!sym.isDefined || sym.isData) continue;
            size_t s = sectionContaining(sections[i], sym.value);
            if (s < sections[i].size()) defSection[sym.name] = {i, s};  // last definition wins, as before
        }
    }

    // The (object, section) a relocation at `objects[obj].relocations[...]`, sited in `sec`,
    // itself resolves to - nullopt for "..d" (data-space, no section of its own) or a named
    // external symbol with no definition anywhere (reported as "undefined symbol" below/by the
    // caller, not here).
    auto targetOf = [&](size_t obj, const ObjRelocation &r) -> std::optional<std::pair<size_t, size_t>> {
        if (r.symbolName == "..d") return std::nullopt;
        if (r.symbolName == "..") {
            if (sections[obj].size() == 1) return std::make_pair(obj, size_t{0});
            uint32_t embedded = config_.addrFieldExtractors.at(r.type)(objects[obj].textBytes, r.offset);
            size_t tSec = sectionContaining(sections[obj], embedded);
            if (tSec >= sections[obj].size()) return std::nullopt;
            return std::make_pair(obj, tSec);
        }
        auto it = defSection.find(r.symbolName);
        if (it == defSection.end()) return std::nullopt;
        return it->second;
    };

    if (gcSections) {
        std::vector<std::pair<size_t, size_t>> worklist;
        for (size_t s = 0; s < sections[0].size(); s++) worklist.push_back({0, s});
        // A symbol marked isRoot (e.g. an `@addr`-placed function meant to be entered only from
        // outside rcc's own compiled output) is a root in its own right, whether or not anything
        // in the kept call graph ever references it.
        for (size_t i = 0; i < objects.size(); i++) {
            for (const auto &sym : objects[i].symbols) {
                if (!sym.isRoot || !sym.isDefined || sym.isData) continue;
                size_t s = sectionContaining(sections[i], sym.value);
                if (s >= sections[i].size() || included[i][s]) continue;
                included[i][s] = true;
                worklist.push_back({i, s});
            }
        }
        while (!worklist.empty()) {
            auto [obj, sec] = worklist.back();
            worklist.pop_back();
            for (const auto &r : objects[obj].relocations) {
                if (r.offset < sections[obj][sec].start || r.offset >= sections[obj][sec].end) continue;
                std::optional<std::pair<size_t, size_t>> target = targetOf(obj, r);
                if (!target) continue;
                auto [tObj, tSec] = *target;
                if (included[tObj][tSec]) continue;
                included[tObj][tSec] = true;
                worklist.push_back({tObj, tSec});
            }
        }
    }

    // Per-object, per-section new base (pre-link byte offset into the final image); an excluded
    // section contributes no bytes at all, same as the object never having been given.
    std::vector<std::vector<uint32_t>> sectionNewBase(objects.size());
    uint32_t cursor = config_.baseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        sectionNewBase[i].assign(sections[i].size(), 0);
        for (size_t s = 0; s < sections[i].size(); s++) {
            if (!included[i][s]) continue;
            sectionNewBase[i][s] = cursor;
            cursor += sections[i][s].end - sections[i][s].start;
        }
    }
    std::vector<uint8_t> image(cursor - config_.baseAddress, 0);

    // Independent sequential layout over the Harvard data address space; always 0-sized for a
    // von-Neumann target, and never backed by real bytes (a data-space reservation only). Not
    // gc'd: dataSize is a per-object size reservation with no symbol-level granularity to prune.
    std::vector<uint32_t> dataBase(objects.size());
    uint32_t dataCursor = config_.dataBaseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        dataBase[i] = dataCursor;
        dataCursor += objects[i].dataSize;
    }
    for (size_t i = 0; i < objects.size(); i++) {
        for (size_t s = 0; s < sections[i].size(); s++) {
            if (!included[i][s]) continue;
            std::copy(objects[i].textBytes.begin() + sections[i][s].start, objects[i].textBytes.begin() + sections[i][s].end,
                       image.begin() + static_cast<long>(sectionNewBase[i][s] - config_.baseAddress));
        }
    }

    // A symbol's resolved address needs its own section's new base plus how far past that
    // section's own old start it sits (0 when gcSections is off, since then every section's new
    // base equals the old whole-object layout's objectBase exactly).
    auto resolvedAddr = [&](size_t i, const ObjSymbol &sym) -> std::optional<uint32_t> {
        if (sym.isData) return dataBase[i] + sym.value;
        size_t s = sectionContaining(sections[i], sym.value);
        if (s >= sections[i].size() || !included[i][s]) return std::nullopt;
        return sectionNewBase[i][s] + (sym.value - sections[i][s].start);
    };

    std::unordered_map<std::string, uint32_t> globalSymbols;
    std::unordered_map<std::string, bool> globalSymbolIsData;
    std::unordered_map<std::string, size_t> definedBy;
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &sym : objects[i].symbols) {
            if (!sym.isDefined) continue;
            std::optional<uint32_t> addr = resolvedAddr(i, sym);
            if (!addr) continue;  // this symbol's own section was dropped
            auto it = definedBy.find(sym.name);
            if (it != definedBy.end() && it->second != i && isGlobalName(sym.name)) {
                fail("duplicate symbol '" + sym.name + "' defined in more than one object");
            }
            globalSymbols[sym.name] = *addr;
            globalSymbolIsData[sym.name] = sym.isData;
            definedBy[sym.name] = i;
        }
    }

    // value/siteFinalAddr stay in packed-byte scale until the final division into the patch
    // function (a no-op when addressUnitBytes == 1).
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &r : objects[i].relocations) {
            size_t sec = sectionContaining(sections[i], r.offset);
            if (sec >= sections[i].size() || !included[i][sec]) continue;  // its own section was dropped
            uint32_t idx = sectionNewBase[i][sec] - config_.baseAddress + (r.offset - sections[i][sec].start);
            uint32_t minBytes = std::max<uint32_t>(2, config_.addressUnitBytes);
            if (idx + minBytes > image.size()) fail("relocation offset out of range");

            uint32_t value;
            if (r.symbolName == "..") {
                // The local offset embedded at the relocation site is this object's own real
                // target's pre-link byte offset - not necessarily in `sec` itself (see
                // AddrFieldExtractFn's own comment) - so "this object's own base" has to land on
                // *that* section's own new base, shifted back by that section's own start, to
                // reproduce the exact address a whole-object (non-gc) link would've produced.
                std::optional<std::pair<size_t, size_t>> target = targetOf(i, r);
                if (!target) fail("'..' relocation's own target address falls outside every known section");
                auto [tObj, tSec] = *target;
                // `gcSections` can move a section to a *lower* address than its own pre-link
                // offset (when something earlier in the same object got dropped), making this a
                // genuine negative shift - stored here as its usual two's-complement uint32_t
                // bit pattern, correct for the final add a patch function does, but NOT safe to
                // divide with ordinary unsigned division below (see there for why).
                value = sectionNewBase[tObj][tSec] - sections[tObj][tSec].start;
            } else if (r.symbolName == "..d") {
                value = dataBase[i];
            } else {
                auto it = globalSymbols.find(r.symbolName);
                if (it == globalSymbols.end()) fail("undefined symbol '" + r.symbolName + "'");
                value = it->second;
            }

            auto patchIt = config_.relocPatchers.find(r.type);
            if (patchIt == config_.relocPatchers.end()) fail("unknown relocation type " + std::to_string(r.type));
            uint32_t siteFinalAddr = sectionNewBase[i][sec] + (r.offset - sections[i][sec].start);
            // `value` is always an exact multiple of `addressUnitBytes` (every section boundary
            // and base address is word-aligned), but it can carry a negative shift (see the ".."
            // case above) as an ordinary uint32_t two's-complement bit pattern. Plain unsigned
            // division would corrupt that - `(-177 as uint32_t) / 3` is nowhere near
            // `-59 as uint32_t` - so this reinterprets it as the signed quantity it really is,
            // divides with ordinary signed division (exact, since it always divides evenly), and
            // converts back; a non-negative `value` (the overwhelmingly common case: every named-
            // symbol and "..d" value, and most "..") divides identically either way.
            int32_t signedValue = static_cast<int32_t>(value) / static_cast<int32_t>(config_.addressUnitBytes);
            patchIt->second(image, idx, static_cast<uint32_t>(signedValue), siteFinalAddr / config_.addressUnitBytes);
        }
    }

    ObjectFile linked;
    linked.unitBits = config_.outputUnitBits;
    linked.base = config_.baseAddress / config_.addressUnitBytes;
    linked.machineType = config_.machineType;
    linked.textBytes = std::move(image);
    linked.symbols.reserve(globalSymbols.size());
    for (auto &kv : globalSymbols) {
        bool isData = globalSymbolIsData[kv.first];
        // Code symbols are left relative to .text's start (matching org 0); data addresses are
        // already final absolute values (Harvard data is always byte-addressed 1:1).
        uint32_t value = isData ? kv.second : (kv.second - config_.baseAddress) / config_.addressUnitBytes;
        linked.symbols.push_back({kv.first, value, /*isDefined=*/true, isData});
    }
    return linked;
}

}  // namespace rcc
