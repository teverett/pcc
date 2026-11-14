#include "M6502LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/m6502/M6502Assembler.h"

namespace rcc {

namespace {
// sp must be seeded to 0x0200 (kFrameStackBase in M6502Codegen.cpp) before
// the first call, matching that file's own _start initialization.
constexpr const char *kEntryAsm =
    "org 0\n"
    "ptr = 0\n"
    "ac = 2\n"
    "bc = 4\n"
    "cx = 6\n"
    "dx = 8\n"
    "fp = 10\n"
    "sp = 12\n"
    "aBase = 14\n"
    "_start:\n"
    "    ldai 0\n"
    "    sta sp\n"
    "    ldai 2\n"
    "    sta sp+1\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// `--target=atari2600`'s own entry sequence: identical shape to kEntryAsm above, just with
// every zero-page equate shifted up to M6502Codegen.h's `zeroPageBase()` ($0080, clear of the
// TIA's own write-only registers mirrored across $0000-$003F - VSYNC/WSYNC/COLUBK/etc., real
// addresses a plain 6502 object's own `ptr`/`ac`/etc. would otherwise alias and silently
// trigger as a side effect of an ordinary pointer dereference) and `sp` seeded to
// `frameStackBase()` ($00B0) instead of $0200, which isn't real RAM on this machine at all -
// the Atari 2600 has only 128 bytes of RAM total, at $0080-$00FF.
constexpr const char *kAtari2600EntryAsm =
    "org 0\n"
    "ptr = 128\n"
    "ac = 130\n"
    "bc = 132\n"
    "cx = 134\n"
    "dx = 136\n"
    "fp = 138\n"
    "sp = 140\n"
    "aBase = 142\n"
    "_start:\n"
    "    ldai 176\n"
    "    sta sp\n"
    "    ldai 0\n"
    "    sta sp+1\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// `--target=2a03`'s own entry sequence: identical in every respect to kEntryAsm above - same
// zero-page equates, same `sp` seed - since the NES's 2A03 has a real, uncontended 2KB RAM at
// $0000-$07FF and needs no relocation (see M6502Codegen.h's own header comment). Kept as its
// own copy anyway, matching kAtari2600EntryAsm's own convention, so a future divergence (e.g. a
// mapper that does need its own fixed RAM reservation) has somewhere to go without disturbing
// kEntryAsm.
constexpr const char *kNes2A03EntryAsm =
    "org 0\n"
    "ptr = 0\n"
    "ac = 2\n"
    "bc = 4\n"
    "cx = 6\n"
    "dx = 8\n"
    "fp = 10\n"
    "sp = 12\n"
    "aBase = 14\n"
    "_start:\n"
    "    ldai 0\n"
    "    sta sp\n"
    "    ldai 2\n"
    "    sta sp+1\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// Patches only the low/high byte of a 16-bit value.
void patchAbs16Lo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchAbs16Hi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF));
}
}  // namespace

std::unique_ptr<TargetLinker> makeM6502Linker(uint32_t baseAddress) {
    GenericLinkerConfig config;
    config.baseAddress = baseAddress;
    config.machineType = kM6502CoffMachine;
    config.entryObject = assembleM6502Object(kEntryAsm);
    config.relocPatchers[kM6502RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM6502RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kM6502RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"raw", "coff", "elf", "aout", "dos33", "prodos", "prg", "atari"};
    return std::make_unique<GenericLinker>(std::move(config));
}

std::unique_ptr<TargetLinker> makeAtari2600Linker(uint32_t baseAddress) {
    GenericLinkerConfig config;
    config.baseAddress = baseAddress;
    config.machineType = kM6502Atari2600CoffMachine;
    config.entryObject = assembleM6502Object(kAtari2600EntryAsm, kM6502Atari2600CoffMachine);
    config.relocPatchers[kM6502RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM6502RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kM6502RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"raw", "coff", "elf", "aout", "a26"};
    return std::make_unique<GenericLinker>(std::move(config));
}

std::unique_ptr<TargetLinker> makeNes2A03Linker(uint32_t baseAddress) {
    GenericLinkerConfig config;
    config.baseAddress = baseAddress;
    config.machineType = kM6502Nes2A03CoffMachine;
    config.entryObject = assembleM6502Object(kNes2A03EntryAsm, kM6502Nes2A03CoffMachine);
    config.relocPatchers[kM6502RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM6502RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kM6502RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"raw", "coff", "elf", "aout", "nes"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
