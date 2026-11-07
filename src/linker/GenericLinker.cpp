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

    std::vector<uint32_t> objectBase(objects.size());
    uint32_t cursor = config_.baseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        objectBase[i] = cursor;
        cursor += static_cast<uint32_t>(objects[i].textBytes.size());
    }
    std::vector<uint8_t> image(cursor - config_.baseAddress, 0);

    // Independent sequential layout over the Harvard data address space; always 0-sized for a
    // von-Neumann target, and never backed by real bytes (a data-space reservation only).
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

    // value/siteFinalAddr stay in packed-byte scale until the final division into the patch
    // function (a no-op when addressUnitBytes == 1).
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &r : objects[i].relocations) {
            uint32_t idx = objectBase[i] - config_.baseAddress + r.offset;
            uint32_t minBytes = std::max<uint32_t>(2, config_.addressUnitBytes);
            if (idx + minBytes > image.size()) fail("relocation offset out of range");

            uint32_t value;
            if (r.symbolName == "..") {
                value = objectBase[i];
            } else if (r.symbolName == "..d") {
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

}  // namespace pcc
