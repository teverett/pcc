#include "I8086Linker.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "codegen/x86_8086/I8086Assembler.h"
#include "coff/CoffReader.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I8086Linker: " + msg); }

// Synthesized here (not emitted by any object any more - see
// I8086Codegen.h's own `objectMode` comment) as ordinary 8086 text,
// assembled exactly like any other input: `call fn_main` becomes a
// genuine Rel16 relocation against the external symbol "fn_main",
// resolved by this same linker exactly like a cross-object call. See
// I8086Codegen.cpp's own former inline version of this same sequence
// for why each instruction is here.
constexpr const char *kEntryAsm =
    "BITS 16\n"
    "ORG 0\n"
    "_entry:\n"
    "    cli\n"
    "    xor ax, ax\n"
    "    mov ds, ax\n"
    "    mov es, ax\n"
    "    mov ss, ax\n"
    "    mov sp, 0x7C00\n"
    "    sti\n"
    "    call fn_main\n"
    "    hlt\n"
    "_hang:\n"
    "    jmp _hang\n";

void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}

}  // namespace

LinkedImage I8086Linker::merge(const std::vector<CoffObject> &userObjects) const {
    if (userObjects.empty()) fail("no object files given");
    for (const auto &obj : userObjects) {
        if (obj.machineType != kI8086CoffMachine) {
            fail("object's machine type doesn't match --target=8086's own (0x" +
                 std::to_string(kI8086CoffMachine) + ")");
        }
    }

    // The synthesized entry sequence always goes first - every other
    // object's own `..`/external relocations are unaffected by where
    // in link order it sits, since they're resolved by name/by their
    // own assigned base either way.
    std::vector<CoffObject> objects;
    objects.push_back(readCoffObject(assembleI8086(kEntryAsm, I8086OutputFormat::Coff, /*allowExternal=*/true)));
    objects.insert(objects.end(), userObjects.begin(), userObjects.end());

    // --- layout: sequentially by byte, from the boot-sector load
    // address. ---
    std::vector<uint32_t> objectBase(objects.size());
    uint32_t cursor = kI8086BaseAddress;
    for (size_t i = 0; i < objects.size(); i++) {
        objectBase[i] = cursor;
        cursor += static_cast<uint32_t>(objects[i].textBytes.size());
    }
    std::vector<uint8_t> image(cursor - kI8086BaseAddress, 0);
    for (size_t i = 0; i < objects.size(); i++) {
        std::copy(objects[i].textBytes.begin(), objects[i].textBytes.end(),
                   image.begin() + static_cast<long>(objectBase[i] - kI8086BaseAddress));
    }

    // --- global symbol table: every object's own *defined* symbols,
    // at their real linked address. I8086Codegen.h's label() only
    // gives a name real cross-file meaning when it starts with
    // "g_"/"p_"/"fn_" (a Global/Param/Function Slot); every other
    // defined symbol an object exports - a Local Slot's numeric
    // name, a literal/temp label, a "__"-prefixed backend-internal
    // helper, or the entry sequence's own "_entry"/"_hang" - is
    // purely local to that one object's own two-pass assembly
    // (numbered fresh per compile, or never referenced cross-file at
    // all), so a same-named duplicate from a different object is
    // never actually ambiguous to anything and is tolerated silently
    // (first one wins, arbitrarily) - see Pdp7Linker.cpp's own
    // version of this same rule. Only a genuine "g_"/"p_"/"fn_"
    // duplicate - two files really defining the same global/function -
    // is a hard error. ---
    auto isGlobalName = [](const std::string &name) {
        return name.rfind("g_", 0) == 0 || name.rfind("p_", 0) == 0 || name.rfind("fn_", 0) == 0;
    };
    std::unordered_map<std::string, uint32_t> globalSymbols;
    std::unordered_map<std::string, size_t> definedBy;
    for (size_t i = 0; i < objects.size(); i++) {
        for (const auto &sym : objects[i].symbols) {
            if (!sym.isDefined) continue;
            uint32_t addr = objectBase[i] + sym.value;
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
            uint32_t idx = objectBase[i] - kI8086BaseAddress + r.offset;
            if (idx + 1 >= image.size()) fail("relocation offset out of range");

            uint32_t value;
            if (r.symbolName == "..") {
                value = objectBase[i];
            } else {
                auto it = globalSymbols.find(r.symbolName);
                if (it == globalSymbols.end()) fail("undefined symbol '" + r.symbolName + "'");
                value = it->second;
            }

            if (r.type == kI8086RelocAbs16) {
                put16(image, idx, static_cast<uint16_t>(get16(image, idx) + value));
            } else if (r.type == kI8086RelocRel16) {
                // Same `target - (site+2)` formula as
                // I8086Assembler.cpp's own emitRel16 - only computable
                // now that both the target's and this site's real
                // linked addresses are known.
                uint32_t siteFinal = objectBase[i] + r.offset;
                int32_t rel = static_cast<int32_t>(value) - static_cast<int32_t>(siteFinal + 2);
                put16(image, idx, static_cast<uint16_t>(rel));
            } else {
                fail("unknown relocation type " + std::to_string(r.type));
            }
        }
    }

    LinkedImage linked;
    linked.unitBits = 8;
    linked.base = kI8086BaseAddress;
    linked.machineType = kI8086CoffMachine;
    linked.words.assign(image.begin(), image.end());
    linked.symbols = std::move(globalSymbols);
    return linked;
}

}  // namespace pcc
