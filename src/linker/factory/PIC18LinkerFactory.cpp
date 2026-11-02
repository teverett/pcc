#include "PIC18LinkerFactory.h"

#include <sstream>
#include <stdexcept>

#include "codegen/pic18/PIC18Assembler.h"
#include "linker/GenericLinker.h"

namespace pcc {

namespace {
// Mirrors PIC18Codegen.cpp's own whole-program `generate()` entry
// sequence (minus `goto START`'s own real reset-vector placement,
// which GenericLinker.cpp's own `config_.baseAddress` already
// supplies by laying this object out first - see GenericLinker.h's
// own header comment): clear FSR0H (this target's own pointer only
// ever needs its low byte set - see PIC18Codegen.h's own header
// comment), point SP at SSTACK, call fn_main, then halt in a self-
// branch. `SP`/`SSTACK` are the same fixed compile-time equates
// (0x09/0x0A) PIC18Codegen.cpp's own object-mode `generate()`
// declares identically in every user object - see
// PIC18Assembler.h's own `kPIC18DataBaseAddress` comment for why that
// needs no relocation at all.
constexpr const char *kEntryAsm =
    "FSR0H = 0xEA\n"
    "SP = 9\n"
    "SSTACK = 10\n"
    "org 0\n"
    "START:\n"
    "    clrf FSR0H\n"
    "    movlw SSTACK\n"
    "    movwf SP\n"
    "    call fn_main\n"
    "HALT:\n"
    "    goto HALT\n";

// GOTO/CALL's own 20-bit absolute *word* address `k` (see
// PIC18Assembler.h's own `kPIC18RelocGotoCallLo` comment): `value`
// (always a *byte* address - every GenericLinker-based target lays
// out `.text` byte-wise) is first divided by two - exact, since every
// PIC18 object's own `.textBytes` size, and therefore every object's
// own linked base address, is always an even number of bytes (every
// instruction here is a whole 16-bit word - see
// ../../codegen/pic18/PIC18Assembler.cpp's own `buildObjectFile`) -
// before it's even the right unit to add onto `k`'s own baked-in
// local word offset.
//
// Patched high-nibble-first, then mid, then low - the three
// relocations for one GOTO/CALL are always pushed in exactly that
// order (see PIC18Assembler.cpp's own `controlAddr20`), mirroring
// ../../codegen/i8008/I8008LinkerFactory.cpp's own Hi-before-Lo carry
// technique: each one reads whichever lower byte(s) are still in
// their *original*, not-yet-patched state to compute the carry its
// own addition produces, since by the time a lower-order patch
// actually runs here, the higher-order one has already consumed the
// carry it needed - patching low-to-high instead would have nothing
// left to read once a carry mattered.
void patchGotoCallHiNibble(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t addVal = value >> 1;
    uint32_t origLo = image[siteOffset - 3];
    uint32_t origMid = image[siteOffset - 1];
    uint32_t carryIntoMid = ((origLo & 0xFF) + (addVal & 0xFF) > 0xFF) ? 1u : 0u;
    uint32_t midSum = origMid + ((addVal >> 8) & 0xFF) + carryIntoMid;
    uint32_t carryIntoHi = (midSum > 0xFF) ? 1u : 0u;
    uint32_t hiNibble = ((image[siteOffset] & 0x0F) + ((addVal >> 16) & 0xF) + carryIntoHi) & 0xF;
    image[siteOffset] = static_cast<uint8_t>(0xF0 | hiNibble);
}
// The second word's own low byte (k bits 8-15) - reads word1's own
// low byte (still original - patched last, see `patchGotoCallLo`
// below) for the one carry this byte's own addition can produce.
void patchGotoCallMid(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t addVal = value >> 1;
    uint32_t origLo = image[siteOffset - 2];
    uint32_t carryIntoMid = ((origLo & 0xFF) + (addVal & 0xFF) > 0xFF) ? 1u : 0u;
    uint32_t midSum = image[siteOffset] + ((addVal >> 8) & 0xFF) + carryIntoMid;
    image[siteOffset] = static_cast<uint8_t>(midSum & 0xFF);
}
// The first word's own low byte (k bits 0-7) - a plain truncating
// add, always correct on its own (mod-256 addition commutes with
// truncation); the two higher-order patches above have already read
// this byte's own pre-patch value by the time this one actually runs.
void patchGotoCallLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t addVal = value >> 1;
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (addVal & 0xFF));
}

// Every data-space reference's own plain single-byte additive patch
// (see PIC18Assembler.h's own `kPIC18RelocDataAbs8` comment) - unlike
// the GOTO/CALL kinds above, no word-vs-byte shift and no carry chain
// at all, since this field is already byte-addressed and this
// backend's own scope restriction (see PIC18Codegen.h's own header
// comment) never lets a real linked address reach anywhere near this
// byte's own high nibble. The one thing worth checking for here
// instead: this chip's real Access Bank GPR window is only 96 bytes
// (0x00-0x5F) wide, shared by every linked object's own globals/
// locals/params combined (unlike a single, per-object compile, which
// can only ever check *its own* usage - see PIC18Assembler.cpp's own
// `Line::Res` comment on why that per-object check is skipped in
// object mode) - so this is the one, real, authoritative place that
// limit is ever actually enforced for a linked, multi-object program.
void patchDataAbs8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t sum = image[siteOffset] + (value & 0xFF);
    if (sum > 0x5F) {
        std::ostringstream msg;
        msg << "PIC18LinkerFactory: linked data-space address 0x" << std::hex << sum
            << " overflows this chip's real 96-byte Access Bank GPR window (0x00-0x5F) - too many globals/"
               "locals/params across every linked object for this target (see PIC18Codegen.h's own header "
               "comment)";
        throw std::runtime_error(msg.str());
    }
    image[siteOffset] = static_cast<uint8_t>(sum);
}
}  // namespace

std::unique_ptr<TargetLinker> makePIC18Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPIC18BaseAddress;
    config.machineType = kPIC18CoffMachine;
    config.entryObject = assemblePIC18Object(kEntryAsm);
    config.relocPatchers[kPIC18RelocGotoCallHiNibble] = patchGotoCallHiNibble;
    config.relocPatchers[kPIC18RelocGotoCallMid] = patchGotoCallMid;
    config.relocPatchers[kPIC18RelocGotoCallLo] = patchGotoCallLo;
    config.relocPatchers[kPIC18RelocDataAbs8] = patchDataAbs8;
    config.outputFormats = {"flat", "coff", "elf", "aout"};
    // Real Access Bank address 0x22 - see PIC18Assembler.h's own
    // header comment on why this backend's own data-space linking
    // begins right after the fixed pseudo-register equate region
    // (0x00-0x21), not at 0x00 itself.
    config.dataBaseAddress = kPIC18DataBaseAddress;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
