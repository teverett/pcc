#include "I8086LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/x86_8086/I8086Assembler.h"

namespace rcc {

namespace {
// A bare flat binary, same model as every other backend here: no boot sector, no BIOS
// load convention, no OS - just whole-program code/data starting at kI8086BaseAddress,
// with ds/es/ss set to the flat segment it all lives in and sp parked at the top of that
// 64KB segment (0xFFFE), well above where code+data start. Producing a bootable image is
// out of scope for this project; a caller that wants one builds it from this plain binary
// outside rcc.
constexpr const char *kEntryAsm =
    "BITS 16\n"
    "ORG 0\n"
    "_entry:\n"
    "    cli\n"
    "    xor ax, ax\n"
    "    mov ds, ax\n"
    "    mov es, ax\n"
    "    mov ss, ax\n"
    "    mov sp, 0xFFFE\n"
    "    sti\n"
    "    call fn_main\n"
    "    hlt\n"
    "_hang:\n"
    "    jmp _hang\n";

// `--target=8086-dos`: a real MS-DOS `.com` program. DOS's own loader already made
// CS=DS=ES=SS the one segment this process gets (the PSP) and gave it a valid stack
// before ever reaching `_entry`, so unlike kEntryAsm above there's no segment/stack
// setup at all - just call main and exit via `INT 21h` function `4Ch`. This dialect's
// `mov` has no `ah` register to load just the function number into (I8086Assembler.cpp
// has none at all), so AX is reloaded wholesale with `0x4C00`: function 4Ch, exit code
// 0 - main's own return value doesn't survive as a real DOS exit code this way, only up
// until this mov.
constexpr const char *kDosEntryAsm =
    "BITS 16\n"
    "ORG 0\n"
    "_entry:\n"
    "    call fn_main\n"
    "    mov ax, 0x4C00\n"
    "    int 0x21\n";
}  // namespace

std::unique_ptr<TargetLinker> makeI8086Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kI8086BaseAddress;
    config.machineType = kI8086CoffMachine;
    config.entryObject = assembleI8086Object(kEntryAsm);
    config.relocPatchers[kI8086RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kI8086RelocRel16] = RelocKinds::patchRel16;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

std::unique_ptr<TargetLinker> makeI8086DosLinker() {
    GenericLinkerConfig config;
    config.baseAddress = kI8086DosBaseAddress;
    config.machineType = kI8086DosCoffMachine;
    config.entryObject = assembleI8086Object(kDosEntryAsm, kI8086DosCoffMachine);
    config.relocPatchers[kI8086RelocAbs16] = RelocKinds::patchAbs16;
    config.relocPatchers[kI8086RelocRel16] = RelocKinds::patchRel16;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
