#include "GenericLinker.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace pcc {

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

ObjectFile GenericLinker::merge(const std::vector<ObjectFile> &userObjects) const {
    if (userObjects.empty()) fail("no object files given");
    for (const auto &obj : userObjects) {
        // `machineType == 0` is tolerated unconditionally: a.out's
        // own real header has no machine-id field at all (see
        // ../fileformat/aout/AoutFormat.h's own header
        // comment), so AoutFileFormatReader::read() can only ever
        // report 0 - this is that format's own real, structural
        // limitation, not a weaker check for every other format
        // (which always carries this backend's own real non-zero
        // `machineType` - see e.g. ../codegen/z80/Z80Assembler.h's
        // own `kZ80CoffMachine`).
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

    // --- layout: sequentially by byte, from the base address. ---
    std::vector<uint32_t> objectBase(objects.size());
    uint32_t cursor = config_.baseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        objectBase[i] = cursor;
        cursor += static_cast<uint32_t>(objects[i].textBytes.size());
    }
    std::vector<uint8_t> image(cursor - config_.baseAddress, 0);

    // --- a second, independent sequential layout over the Harvard
    // data address space (see GenericLinkerConfig::dataBaseAddress's
    // own comment) - every `dataSize`/`dataBase` here is always 0 for
    // a von-Neumann target, exactly mirroring the code layout above
    // but with nothing to copy into any image (a data-space
    // reservation is never backed by real bytes - see
    // ObjectFile::dataSize's own comment). ---
    std::vector<uint32_t> dataBase(objects.size());
    uint32_t dataCursor = config_.dataBaseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        dataBase[i] = dataCursor;
        dataCursor += objects[i].dataSize;
    }
    for (size_t i = 0; i < objects.size(); i++) {
        std::copy(objects[i].textBytes.begin(), objects[i].textBytes.end(),
                   image.begin() + static_cast<long>(objectBase[i] - config_.baseAddress));
    }

    // --- global symbol table - see GenericLinker.h's own header
    // comment on the g_/p_/fn_ (pdp7's own uppercase G_/P_/FN_ - see
    // Pdp7Codegen.h's own label()) cross-file-visibility convention.
    // ---
    auto isGlobalName = [](const std::string &name) {
        return name.rfind("g_", 0) == 0 || name.rfind("p_", 0) == 0 || name.rfind("fn_", 0) == 0 ||
               name.rfind("G_", 0) == 0 || name.rfind("P_", 0) == 0 || name.rfind("FN_", 0) == 0;
    };
    std::unordered_map<std::string, uint32_t> globalSymbols;
    std::unordered_map<std::string, bool> globalSymbolIsData;
    std::unordered_map<std::string, size_t> definedBy;
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &sym : objects[i].symbols) {
            if (!sym.isDefined) continue;
            uint32_t addr = (sym.isData ? dataBase[i] : objectBase[i]) + sym.value;
            auto it = definedBy.find(sym.name);
            if (it != definedBy.end() && it->second != i && isGlobalName(sym.name)) {
                fail("duplicate symbol '" + sym.name + "' defined in more than one object");
            }
            globalSymbols[sym.name] = addr;
            globalSymbolIsData[sym.name] = sym.isData;
            definedBy[sym.name] = i;
        }
    }

    // --- resolve and patch every relocation. `value`/`siteFinalAddr`
    // stay in the same packed-byte scale as `image`/`objectBase`
    // right up until the moment they're handed to a patch function -
    // see GenericLinkerConfig::addressUnitBytes's own comment for why
    // only that final division (a no-op for every byte-addressed
    // target, `addressUnitBytes == 1`) needs to happen at all. ---
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &r : objects[i].relocations) {
            uint32_t idx = objectBase[i] - config_.baseAddress + r.offset;
            uint32_t minBytes = std::max<uint32_t>(2, config_.addressUnitBytes);
            if (idx + minBytes > image.size()) fail("relocation offset out of range");

            uint32_t value;
            if (r.symbolName == "..") {
                value = objectBase[i];
            } else if (r.symbolName == "..d") {
                // This object's own linked data-space base - the
                // data-space counterpart of `".."` above, for a
                // Harvard target's own same-object data address
                // reference (see ObjSymbol::isData's own comment on
                // why even a same-file data reference needs
                // rebasing, same reasoning as `".."` for code).
                value = dataBase[i];
            } else {
                auto it = globalSymbols.find(r.symbolName);
                if (it == globalSymbols.end()) fail("undefined symbol '" + r.symbolName + "'");
                value = it->second;
            }

            auto patchIt = config_.relocPatchers.find(r.type);
            if (patchIt == config_.relocPatchers.end()) fail("unknown relocation type " + std::to_string(r.type));
            uint32_t siteFinalAddr = objectBase[i] + r.offset;
            patchIt->second(image, idx, value / config_.addressUnitBytes, siteFinalAddr / config_.addressUnitBytes);
        }
    }

    // --- build the linked ObjectFile. `textBytes` is already the
    // packed-byte image exactly as every not-yet-linked object's own
    // `.textBytes` is (see ObjectFile.h's own `unitBits`/`base`
    // comment) - no decode/unpack step needed at all. ---
    ObjectFile linked;
    linked.unitBits = config_.outputUnitBits;
    linked.base = config_.baseAddress / config_.addressUnitBytes;
    linked.machineType = config_.machineType;
    linked.textBytes = std::move(image);
    linked.symbols.reserve(globalSymbols.size());
    for (auto &kv : globalSymbols) {
        bool isData = globalSymbolIsData[kv.first];
        // A code symbol's own linked value is left relative to
        // `.text`'s own start, not absolute (see ObjectFile.h's own
        // header comment) - subtract the base this function just
        // added, undoing it, the same way every not-yet-linked
        // object's own symbol table is already relative to its own
        // `org 0`. Only a code address is in the packed-byte scale
        // `addressUnitBytes` describes at all; a data address is
        // already the real, final, absolute value with nothing to
        // convert (Harvard data is always byte-addressed 1:1 - see
        // ObjSymbol::isData's own comment).
        uint32_t value = isData ? kv.second : (kv.second - config_.baseAddress) / config_.addressUnitBytes;
        linked.symbols.push_back({kv.first, value, /*isDefined=*/true, isData});
    }
    return linked;
}

}  // namespace pcc
