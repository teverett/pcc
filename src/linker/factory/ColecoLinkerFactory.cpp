#include "ColecoLinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/z80/Z80Assembler.h"

namespace rcc {

namespace {
// The ColecoVision BIOS validates a cartridge by reading its own fixed
// header at $8000 - not by just jumping straight there the way the plain
// Z80 entry stub (Z80LinkerFactory.cpp's kEntryAsm) does for TRS-80 - so
// this entry object's first bytes have to be that header, not code: a
// 0x55AA signature word, four pointer fields the BIOS leaves alone when
// they're 0 (no official Coleco technical reference is bundled with this
// project to confirm the exact field order/count byte-for-byte - this
// follows the commonly-documented community layout, same "best-effort,
// no real ROM to verify against" caveat as src/rt/trs80/'s own header
// comment), and finally `ptr_code`, which actually points the BIOS at
// `_start` below.
//
// 25600 (0x6400): one past the top of the ColecoVision's real 1KB RAM
// window ($6000-$63FF, mirrored through $7FFF) - the only writable
// memory this platform has, so SP has to land inside it, unlike TRS-80
// where $FF00 is itself ordinary RAM.
constexpr const char *kEntryAsm =
    "org 0\n"
    "sig: dw 0x55AA\n"
    "ptr_sprites: dw 0\n"
    "ptr_reserved1: dw 0\n"
    "ptr_reserved2: dw 0\n"
    "ptr_code: dw _start\n"
    "ptr_nmi: dw 0\n"
    "_start:\n"
    "    ld sp, 25600\n"
    "    call fn_main\n"
    "    ld hl, [ac]\n"
    "halt:\n"
    "    jp halt\n"
    "ac: db 0, 0\n"
    "_bc: db 0, 0\n"
    "cx: db 0, 0\n"
    "dx: db 0, 0\n"
    "ptr: db 0, 0\n";
}  // namespace

std::unique_ptr<TargetLinker> makeColecoLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kZ80BaseAddress;
    config.machineType = kZ80CoffMachine;
    config.entryObject = assembleZ80Object(kEntryAsm);
    config.relocPatchers[kZ80RelocAbs16] = RelocKinds::patchAbs16;
    config.outputFormats = {"raw", "coff", "elf", "aout", "colecorom"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
