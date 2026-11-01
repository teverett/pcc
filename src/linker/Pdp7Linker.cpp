#include "Pdp7Linker.h"

#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "codegen/pdp7/Pdp7Assembler.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp7Linker: " + msg); }

constexpr uint32_t kMask18 = 0777777;
// The entry sequence this linker itself synthesizes at the very
// front of the linked image, in place of what Pdp7Codegen.cpp's own
// whole-program mode used to emit inline (see Codegen.h's
// `objectMode`) - "jms <main>" (opcode 0100000) then "hlt" (opcode
// 0740040), 2 words, same as that inline sequence's own shape.
constexpr uint32_t kEntryWords = 2;
constexpr uint32_t kOpJms = 0100000;
constexpr uint32_t kOpHlt = 0740040;
constexpr const char *kEntrySymbol = "FN_main";  // see Pdp7Codegen.h's label() for this naming convention

// PDP-7 instruction words OR/add a resolved address directly into the
// low 13 bits of the opcode word; bit 13 (0020000 octal, 8192
// decimal - see Pdp7Assembler.cpp's "sys"/"i" indirect-addressing
// flag, the same bit) is the first bit outside that field. An address
// at or above 8192 silently collides with that bit (and higher ones)
// instead of erroring, corrupting the instruction - this is real
// PDP-7 hardware's own 8K-word field size, not a pcc-specific limit.
constexpr uint32_t kAddressFieldLimit = 010000 << 1;  // 020000 octal = 8192 decimal

}  // namespace

LinkedImage Pdp7Linker::merge(const std::vector<CoffObject> &objects) const {
    if (objects.empty()) fail("no object files given");
    for (const auto &obj : objects) {
        if (obj.machineType != kPdp7CoffMachine) {
            std::ostringstream msg;
            msg << "object's machine type 0x" << std::hex << obj.machineType
                << " doesn't match --target=pdp7's own (0x" << kPdp7CoffMachine << ")";
            fail(msg.str());
        }
        if (obj.textBytes.size() % 3 != 0) fail("object's .text size isn't a multiple of 3 bytes - not word-aligned");
    }

    // --- layout: entry sequence first, then every object's own
    // .text, sequentially, in the order given on the command line. ---
    std::vector<uint32_t> objectBase(objects.size());
    std::vector<uint32_t> wordCount(objects.size());
    uint32_t cursor = kPdp7BaseAddress + kEntryWords;
    for (size_t i = 0; i < objects.size(); i++) {
        wordCount[i] = static_cast<uint32_t>(objects[i].textBytes.size() / 3);
        objectBase[i] = cursor;
        cursor += wordCount[i];
    }
    if (cursor > kAddressFieldLimit) {
        std::ostringstream msg;
        msg << "linked image ends at word " << cursor
            << ", past the pdp7's 8192-word (020000 octal) direct-addressing field limit - "
               "too many/too large object files given; a reference to any address at or beyond "
               "8192 would silently corrupt the instruction it's stored in instead of erroring, "
               "so this is refused up front";
        fail(msg.str());
    }

    std::vector<uint32_t> words(cursor - kPdp7BaseAddress, 0);
    for (size_t i = 0; i < objects.size(); i++) {
        const auto &text = objects[i].textBytes;
        for (uint32_t w = 0; w < wordCount[i]; w++) {
            uint32_t b0 = static_cast<uint8_t>(text[w * 3]);
            uint32_t b1 = static_cast<uint8_t>(text[w * 3 + 1]);
            uint32_t b2 = static_cast<uint8_t>(text[w * 3 + 2]);
            words[objectBase[i] - kPdp7BaseAddress + w] = (b0 << 16) | (b1 << 8) | b2;
        }
    }

    // --- global symbol table: every object's own *defined* symbols,
    // at their real linked address. Pdp7Codegen.h's label() only
    // gives a name real cross-file meaning when it starts with
    // "G_"/"P_"/"FN_" (a Global/Param/Function Slot); every other
    // defined symbol an object exports - a Local Slot's numeric
    // "S<id>", a literal/temp/jump label ("N<N>"/"T<N>"/"J<N>"), or a
    // "__"-prefixed backend-internal helper like __one/__zero/
    // __divmod - is purely local to that one object's own two-pass
    // assembly (numbered fresh per compile, or always resolved
    // locally), never the target of a cross-file relocation, so a
    // same-named duplicate from a different object is never actually
    // ambiguous to anything and is tolerated silently (first one
    // wins, arbitrarily). Only a genuine "G_"/"P_"/"FN_" duplicate -
    // two files really defining the same global/function - is a hard
    // error. ---
    auto isGlobalName = [](const std::string &name) {
        return name.rfind("G_", 0) == 0 || name.rfind("P_", 0) == 0 || name.rfind("FN_", 0) == 0;
    };
    std::unordered_map<std::string, uint32_t> globalSymbols;
    std::unordered_map<std::string, size_t> definedBy;
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &sym : objects[i].symbols) {
            if (!sym.isDefined) continue;
            uint32_t addr = objectBase[i] + sym.value / 3;
            auto it = definedBy.find(sym.name);
            if (it != definedBy.end() && it->second != i && isGlobalName(sym.name)) {
                fail("duplicate symbol '" + sym.name + "' defined in more than one object");
            }
            globalSymbols[sym.name] = addr;
            definedBy[sym.name] = i;
        }
    }

    // --- resolve and patch every relocation. ---
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &r : objects[i].relocations) {
            if (r.offset % 3 != 0) fail("relocation offset isn't word-aligned");
            uint32_t localWord = r.offset / 3;
            uint32_t idx = objectBase[i] - kPdp7BaseAddress + localWord;
            if (idx >= words.size()) fail("relocation offset out of range");

            uint32_t value;
            if (r.symbolName == "..") {
                value = objectBase[i];
            } else {
                auto it = globalSymbols.find(r.symbolName);
                if (it == globalSymbols.end()) fail("undefined symbol '" + r.symbolName + "'");
                value = it->second;
            }
            value &= kMask18;
            uint32_t cur = words[idx];
            if (r.type == kPdp7RelocAdd) {
                words[idx] = (cur + value) & kMask18;
            } else if (r.type == kPdp7RelocSub) {
                words[idx] = (cur - value) & kMask18;
            } else {
                words[idx] = (cur | value) & kMask18;
            }
        }
    }

    // --- synthesize the entry sequence now that `main`'s real linked
    // address is resolvable. ---
    auto mainIt = globalSymbols.find(kEntrySymbol);
    if (mainIt == globalSymbols.end()) fail("undefined symbol '" + std::string(kEntrySymbol) + "' - no linked object defines 'main'");
    words[0] = (kOpJms | (mainIt->second & kMask18)) & kMask18;
    words[1] = kOpHlt;

    LinkedImage image;
    image.unitBits = 18;
    image.base = kPdp7BaseAddress;
    image.machineType = kPdp7CoffMachine;
    image.words = std::move(words);
    image.symbols = std::move(globalSymbols);
    return image;
}

}  // namespace pcc
