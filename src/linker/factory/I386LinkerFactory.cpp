#include "I386LinkerFactory.h"

#include <cstdint>
#include <string>

#include "codegen/i386/I386Assembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {
// Real Multiboot header fields - see I386Codegen.cpp's own
// `generate()` comment for why each has the value it does.
constexpr uint32_t kMbMagic = 0x1BADB002;
constexpr uint32_t kMbFlags = 0x10000;
constexpr uint32_t kMbChecksum = static_cast<uint32_t>(-(static_cast<int64_t>(kMbMagic) + kMbFlags));

// Verbatim copy of I386Codegen.cpp's own whole-program entry
// sequence (Multiboot header, both 8259 PICs masked, `call fn_main`,
// halt loop) - see that file's own `generate()` comment. `org 0` -
// this object is placed first by GenericLinker::merge() (see that
// class's own header comment), always exactly at `kI386LoadAddr`, so
// the header's own `header_addr`/`load_addr` fields (which must be
// real, already-linked addresses, not offsets) are written as
// `kI386LoadAddr` directly rather than through any label - the
// header must be the very first thing in the linked image. Likewise
// `entry_addr` (where execution starts) is `kI386LoadAddr` plus this
// 32-byte header's own fixed size, rather than `dd _start` - the
// shared `dd`/`dw`/`db` directive path (see I386Assembler.cpp's own
// `processLine`) never records a relocation for its own operands, so
// a label reference there wouldn't get this object's own base-shift
// added once linked (unlike every `mov`/`add`/moffs32 label
// reference, which does - see I386Assembler.cpp's own `emitAbs32`).
std::string entryAsm() {
    uint32_t entryAddr = kI386LoadAddr + 32;
    return "org 0\n"
           "dd " +
           std::to_string(kMbMagic) + ", " + std::to_string(kMbFlags) + ", " + std::to_string(kMbChecksum) + "\n" +
           "dd " + std::to_string(kI386LoadAddr) + ", " + std::to_string(kI386LoadAddr) + ", 0, 0\n" + "dd " +
           std::to_string(entryAddr) + "\n\n" +
           "_start:\n"
           "    mov al,0xFF\n    out 0x21,al\n"
           "    mov al,0xFF\n    out 0xA1,al\n"
           "    call fn_main\n"
           "    hlt\n"
           "halt:\n    jmp halt\n";
}
}  // namespace

std::unique_ptr<TargetLinker> makeI386Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kI386LoadAddr;
    config.machineType = kI386CoffMachine;
    config.entryObject = assembleI386Object(entryAsm());
    config.relocPatchers[kI386RelocAbs32] = RelocKinds::patchAbs32;
    config.relocPatchers[kI386RelocRel32] = RelocKinds::patchRel32;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
