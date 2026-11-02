#include "I8086LinkerFactory.h"

#include "linker/GenericLinker.h"
#include "codegen/x86_8086/I8086Assembler.h"

namespace pcc {

namespace {
// Zeroes every segment register, sets up a stack growing down from
// `0x7C00` (the boot-sector load address itself - same convention as
// a bare-metal boot sector that never grows past its own load point),
// then calls `main` and halts - the exact sequence I8086Codegen.cpp's
// own whole-program (`--format=raw`) mode used to emit inline, before
// real separate compilation existed.
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

}  // namespace pcc
