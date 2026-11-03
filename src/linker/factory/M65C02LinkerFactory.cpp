#include "M65C02LinkerFactory.h"

#include <cstdint>
#include <stdexcept>

#include "linker/GenericLinker.h"
#include "codegen/m65c02/M65C02Assembler.h"

namespace pcc {

namespace {
// `sp`/`fp` (the software call-frame stack's own state - see
// ../../codegen/m65c02/M65C02Codegen.h's own calling-convention
// comment) need the same one-time seeding here that
// M65C02Codegen.cpp's own `generate()` gives them for a single-file
// `--format=raw` program: `sp` starts at the same
// M65C02Codegen.cpp-internal top-of-frame-stack address (duplicated
// as a plain literal here, since that constant isn't exported - kept
// in sync by hand, same as every other value this entry stub already
// shares with that file's own non-object-mode entry sequence), and
// `fp` only needs zeroing for tidiness (see that file's own comment
// on why nothing ever reads it back meaningfully).
constexpr const char *kEntryAsm =
    "org 0\n"
    "ptr = 0\n"
    "ac = 2\n"
    "bc = 4\n"
    "cx = 6\n"
    "dx = 8\n"
    "sp = 10\n"
    "fp = 12\n"
    "_start:\n"
    "    ldai 61440\n"
    "    sta sp\n"
    "    ldai_hi 61440\n"
    "    sta sp+1\n"
    "    stzp fp\n"
    "    stzp fp+1\n"
    "    jsr fn_main\n"
    "    lda ac\n"
    "halt:\n"
    "    jmp halt\n";

// One-byte additive patch on the low half of a 16-bit resolved
// value - see M65C02Assembler.h's own `kM65C02RelocAbsLo8` comment
// on why this is a local function here rather than a shared
// GenericLinker::RelocKinds entry.
void patchAbsLo8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>((image[siteOffset] + (value & 0xFF)) & 0xFF);
}
// The high half.
void patchAbsHi8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>((image[siteOffset] + ((value >> 8) & 0xFF)) & 0xFF);
}
}  // namespace

std::unique_ptr<TargetLinker> makeM65C02Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kM65C02BaseAddress;
    config.machineType = kM65C02CoffMachine;
    config.entryObject = assembleM65C02Object(kEntryAsm);
    config.relocPatchers[kM65C02RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kM65C02RelocAbsLo8] = patchAbsLo8;
    config.relocPatchers[kM65C02RelocAbsHi8] = patchAbsHi8;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
