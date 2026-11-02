#include "I8008LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/i8008/I8008Assembler.h"

namespace pcc {

namespace {
// Replicates I8008Codegen.cpp's own whole-program entry sequence
// exactly (`kPseudoBase`=12288/0x3000, `kStackBase`=`kPseudoBase`+12
// - see that file's own anonymous-namespace comment), spelled as
// literal decimal constants since this text is handed to a from-
// scratch assembler that only ever sees its own generated input,
// never a real #include.
constexpr const char *kEntryAsm =
    "org 0\n"
    "ac = 12288\n"
    "bc = 12290\n"
    "cx = 12292\n"
    "dx = 12294\n"
    "ptr = 12296\n"
    "sp = 12298\n"
    "_start:\n"
    "    mvihi h, sp\n"
    "    mvi l, sp\n"
    "    mvi a, 12300\n"
    "    mov m, a\n"
    "    mvihi h, sp+1\n"
    "    mvi l, sp+1\n"
    "    mvihi a, 12300\n"
    "    mov m, a\n"
    "    cal fn_main\n"
    "halt:\n"
    "    jmp halt\n";

// The byte-at-a-time patch kinds this dialect needs beyond
// GenericLinker's own shared `RelocKinds` - see
// I8008LinkerFactory.h's own header comment.
//
// These two always patch a single mvihi/mvi pair emitted back-to-back
// by I8008Codegen.cpp (`emitImmHigh` then `emitImmLow` - see
// I8008Assembler.cpp's own comment on that pair), so the Hi site's
// own offset is always exactly 2 less than its Lo partner's, and
// GenericLinker::merge() always patches Hi before Lo (relocations
// apply in emission order, and emitImmHigh always runs first for
// this pair). Low-byte truncated addition is always correct on its
// own (mod-256 addition commutes with truncation), but the high byte
// also needs any carry OUT of that low-byte addition, which this
// patch order lets us compute by reading the Lo site's own
// still-unpatched byte - plain independent byte-wise addition here
// (the earlier, broken version of this function) silently drops that
// carry whenever a local address's low byte plus the object's own
// base address overflows 0xFF, corrupting the high byte.
void patchAbs16Lo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchAbs16Hi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t loByte = image[siteOffset + 2];
    uint32_t carry = (loByte + (value & 0xFF)) > 0xFF ? 1 : 0;
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF) + carry);
}
}  // namespace

std::unique_ptr<TargetLinker> makeI8008Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kI8008BaseAddress;
    config.machineType = kI8008CoffMachine;
    config.entryObject = assembleI8008Object(kEntryAsm);
    config.relocPatchers[kI8008RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kI8008RelocAbs16Lo] = patchAbs16Lo;
    config.relocPatchers[kI8008RelocAbs16Hi] = patchAbs16Hi;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
