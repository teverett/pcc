#include "PIC18LinkerFactory.h"

#include <sstream>
#include <stdexcept>

#include "codegen/pic18/PIC18Assembler.h"
#include "linker/GenericLinker.h"

namespace rcc {

namespace {
// Must mirror PIC18Codegen.cpp's object-mode entry sequence: clear
// FSR0H, point SP at SSTACK, point NFP at FSTACK+1 (fn_main's prologue
// expects NFP to already hold its new frame's address), call fn_main,
// halt.
constexpr const char *kEntryAsm =
    "FSR0H = 0xEA\n"
    "SP = 9\n"
    "SSTACK = 10\n"
    "FP = 34\n"
    "NFP = 35\n"
    "FSTACK = 36\n"
    "org 0\n"
    "START:\n"
    "    clrf FSR0H\n"
    "    movlw SSTACK\n"
    "    movwf SP\n"
    "    movlw FSTACK+1\n"
    "    movwf NFP\n"
    "    call fn_main\n"
    "HALT:\n"
    "    goto HALT\n";

// GOTO/CALL's 20-bit word address k. value is a byte address, halved
// to get the word address (always exact: every instruction is one
// 16-bit word). Patched high-nibble, then mid, then low, each reading
// the lower byte(s) in their still-original, not-yet-patched state to
// compute the carry its own addition produces.
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
// k bits 8-15. Reads word1's still-original low byte for the carry.
void patchGotoCallMid(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t addVal = value >> 1;
    uint32_t origLo = image[siteOffset - 2];
    uint32_t carryIntoMid = ((origLo & 0xFF) + (addVal & 0xFF) > 0xFF) ? 1u : 0u;
    uint32_t midSum = image[siteOffset] + ((addVal >> 8) & 0xFF) + carryIntoMid;
    image[siteOffset] = static_cast<uint8_t>(midSum & 0xFF);
}
// k bits 0-7. Patched last, after the higher-order patches have
// already read this byte's pre-patch value for their carries.
void patchGotoCallLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t addVal = value >> 1;
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (addVal & 0xFF));
}

// Plain single-byte additive patch for data-space addresses. This is
// the only place that enforces the real chip's 96-byte Access Bank GPR
// window (0x00-0x5F) across every linked object's combined globals,
// locals and params.
void patchDataAbs8(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t sum = image[siteOffset] + (value & 0xFF);
    if (sum > 0x5F) {
        std::ostringstream msg;
        msg << "PIC18LinkerFactory: linked data-space address 0x" << std::hex << sum
            << " overflows this chip's real 96-byte Access Bank GPR window (0x00-0x5F) - too many globals/"
               "locals/params across every linked object for this target";
        throw std::runtime_error(msg.str());
    }
    image[siteOffset] = static_cast<uint8_t>(sum);
}

// Masked add into MOVLW's 8-bit literal field with the low/next-8-bits
// byte of an extern function's resolved byte address - already
// byte-addressed like patchDataAbs8, unlike GOTO/CALL's word-address
// field. The field is always 0 pre-link for this relocation kind (only
// an extern symbol uses it - see PIC18Assembler.cpp's movlwcl/movlwch
// case), so there's no carry to lose by patching the lo/hi byte
// independently.
void patchCodeLo(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + (value & 0xFF));
}
void patchCodeHi(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    image[siteOffset] = static_cast<uint8_t>(image[siteOffset] + ((value >> 8) & 0xFF));
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
    config.relocPatchers[kPIC18RelocCodeLo] = patchCodeLo;
    config.relocPatchers[kPIC18RelocCodeHi] = patchCodeHi;
    config.outputFormats = {"raw", "coff", "elf", "aout"};
    // Data-space linking begins at 0x22, right after the fixed
    // pseudo-register equate region (0x00-0x21).
    config.dataBaseAddress = kPIC18DataBaseAddress;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
