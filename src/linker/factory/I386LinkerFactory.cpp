#include "I386LinkerFactory.h"

#include <cstdint>
#include <string>

#include "codegen/i386/I386Assembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {
constexpr uint32_t kMbMagic = 0x1BADB002;
constexpr uint32_t kMbFlags = 0x10000;
constexpr uint32_t kMbChecksum = static_cast<uint32_t>(-(static_cast<int64_t>(kMbMagic) + kMbFlags));

// The entry object is placed first, always exactly at kI386LoadAddr, so header_addr/load_addr and
// entry_addr are written as literal already-linked addresses: `dd` operands are never relocated,
// so a label reference here would not get the base-shift that `mov`/`add`/moffs32 do.
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
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
