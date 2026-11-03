#include "PIC16LinkerFactory.h"

#include <cstdint>
#include <vector>

#include "linker/GenericLinker.h"
#include "codegen/pic16/PIC16Assembler.h"

namespace pcc {

namespace {

uint16_t get16(const std::vector<uint8_t> &image, size_t idx) {
    return static_cast<uint16_t>(static_cast<uint8_t>(image[idx]) |
                                  (static_cast<uint16_t>(static_cast<uint8_t>(image[idx + 1])) << 8));
}
void put16(std::vector<uint8_t> &image, size_t idx, uint16_t v) {
    image[idx] = static_cast<uint8_t>(v & 0xFF);
    image[idx + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// GOTO/CALL's own 11-bit word-address field (bits [10:0] of the
// already-emitted 16-bit cell at `siteOffset`) - see
// PIC16Assembler.h's own `kPIC16RelocGotoCall` comment for why this
// has to be a masked add (preserving the opcode bits in [15:11])
// rather than a plain whole-cell `RelocKinds::patchAbs16`, and for
// the `/2` (every `value` GenericLinker ever resolves is a *byte*
// address - see ObjectFile.h's own convention - but this field's own
// units are program-memory words, and every instruction this backend
// ever assembles is exactly one 16-bit/2-byte cell, so a code-space
// byte address is always exactly twice its real word address).
// Whatever this field already held (a local, same-object target's
// own pre-link word index, or 0 for an external one - see
// PIC16Assembler.cpp's own `controlAddr()`) is added to, never
// replaced, mirroring every other GenericLinker-based target's own
// relocation shape.
void patchPIC16GotoCall(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t addr = static_cast<uint16_t>(((word & 0x07FF) + (value / 2)) & 0x07FF);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0x07FFu) | addr));
}

// Every data-space (file-register) reference this dialect's own
// encoder ever emits - the FD/FB/`movwf`/`clrf` family's own 7-bit
// "f" field (bits [6:0]) and `movlw`'s own 8-bit literal field (bits
// [7:0]) when it's loading a data label's own address - see
// PIC16Assembler.h's own `kPIC16RelocData` comment for why both are
// patched the same way (masking bits [6:0] only: this target's whole
// GPR bank fits in 7 bits, so bit 7 of the wider field is always 0
// both before and after this patch). No word/byte scaling needed
// here, unlike patchPIC16GotoCall above - file-register space is
// already byte-addressed 1:1.
void patchPIC16Data(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint16_t word = get16(image, siteOffset);
    uint16_t addr = static_cast<uint16_t>(((word & 0x7F) + value) & 0x7F);
    put16(image, siteOffset, static_cast<uint16_t>((word & ~0x7Fu) | addr));
}

// Mirrors PIC16Codegen.cpp's own `declareRam()`'s object-mode fixed-
// pseudo-register equates exactly (same names, same `kPIC16Reg*`
// values - see PIC16Assembler.h's own header comment on why every
// object, this entry object included, declares these identically and
// harmlessly), then points `SP` at the start of `SSTACK` (the same
// real initialization PIC16Codegen.cpp's own non-object-mode `START`
// prologue already does) and calls `fn_main` - a genuine external
// relocation (`fn_main` is never defined in this text), resolved
// exactly like any other cross-object call once a real user object
// defines it.
const std::string kEntryAsm = "org 0\n"
                               "ac = " + std::to_string(kPIC16RegAc) + "\n" +
                               "bc = " + std::to_string(kPIC16RegBc) + "\n" +
                               "cx = " + std::to_string(kPIC16RegCx) + "\n" +
                               "dx = " + std::to_string(kPIC16RegDx) + "\n" +
                               "ptr = " + std::to_string(kPIC16RegPtr) + "\n" +
                               "SP = " + std::to_string(kPIC16RegSP) + "\n" +
                               "FP = " + std::to_string(kPIC16RegFP) + "\n" +
                               "SSTACK = " + std::to_string(kPIC16RegSStack) + "\n" +
                               "_start:\n"
                               "    movlw SSTACK\n"
                               "    movwf SP\n"
                               "    call fn_main\n"
                               "HALT:\n"
                               "    goto HALT\n";

}  // namespace

std::unique_ptr<TargetLinker> makePIC16Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPIC16BaseAddress;
    config.machineType = kPIC16CoffMachine;
    config.dataBaseAddress = kPIC16DataBaseAddress;
    config.entryObject = assemblePIC16Object(kEntryAsm);
    config.relocPatchers[kPIC16RelocGotoCall] = patchPIC16GotoCall;
    config.relocPatchers[kPIC16RelocData] = patchPIC16Data;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
