#include "Pdp7LinkerFactory.h"

#include <sstream>
#include <stdexcept>

#include "linker/GenericLinker.h"
#include "codegen/pdp7/Pdp7Assembler.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp7LinkerFactory: " + msg); }

constexpr uint32_t kMask18 = 0777777;

// PDP-7 instruction words OR a resolved address directly into the low
// 13 bits of the opcode word; bit 13 (0020000 octal, 8192 decimal -
// see Pdp7Assembler.cpp's own "sys"/"i" indirect-addressing flag, the
// same bit) is the first bit outside that field. An address at or
// above 8192 silently collides with that bit (and higher ones)
// instead of erroring, corrupting the instruction - this is real
// PDP-7 hardware's own 8K-word field size, not a pcc-specific limit.
// Only the OR-combine (an opcode's own address field) has this
// hazard; `patchPdp7Add`/`patchPdp7Sub` below patch a plain data word
// with no opcode bits to protect, so they never need this check and
// can use the full 18-bit range.
constexpr uint32_t kAddressFieldLimit = 010000 << 1;  // 020000 octal = 8192 decimal

uint32_t read18(const std::vector<uint8_t> &image, uint32_t siteOffset) {
    return (static_cast<uint32_t>(image[siteOffset]) << 16) | (static_cast<uint32_t>(image[siteOffset + 1]) << 8) |
           static_cast<uint32_t>(image[siteOffset + 2]);
}
void write18(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t word) {
    word &= kMask18;
    image[siteOffset] = static_cast<uint8_t>((word >> 16) & 0xFF);
    image[siteOffset + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
    image[siteOffset + 2] = static_cast<uint8_t>(word & 0xFF);
}

void patchPdp7Or(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    if (value >= kAddressFieldLimit) {
        std::ostringstream msg;
        msg << "address " << value << " (0" << std::oct << value
            << " octal) is past the pdp7's 8192-word (020000 octal) direct-addressing field limit - "
               "too many/too large object files given; a reference to any address at or beyond 8192 "
               "would silently corrupt the instruction it's stored in instead of erroring, so this is "
               "refused up front";
        fail(msg.str());
    }
    write18(image, siteOffset, read18(image, siteOffset) | value);
}
void patchPdp7Add(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    write18(image, siteOffset, read18(image, siteOffset) + value);
}
void patchPdp7Sub(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    write18(image, siteOffset, read18(image, siteOffset) - value);
}

}  // namespace

std::unique_ptr<TargetLinker> makePdp7Linker() {
    GenericLinkerConfig config;
    config.baseAddress = kPdp7BaseAddress * 3;
    config.machineType = kPdp7CoffMachine;
    config.entryObject = assemblePdp7Object("jms FN_main\nhlt\n");
    config.relocPatchers[kPdp7RelocOr] = patchPdp7Or;
    config.relocPatchers[kPdp7RelocAdd] = patchPdp7Add;
    config.relocPatchers[kPdp7RelocSub] = patchPdp7Sub;
    config.outputFormats = {"a7out", "list", "ptr", "rim", "coff", "aout"};
    config.addressUnitBytes = 3;
    config.outputUnitBits = 18;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
