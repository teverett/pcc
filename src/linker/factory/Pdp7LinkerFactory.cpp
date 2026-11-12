#include "Pdp7LinkerFactory.h"

#include <sstream>
#include <stdexcept>

#include "linker/GenericLinker.h"
#include "codegen/pdp7/Pdp7Assembler.h"
#include "codegen/pdp7/Pdp7Codegen.h"

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp7LinkerFactory: " + msg); }

constexpr uint32_t kMask18 = 0777777;

// kAddressFieldLimit/kStackReserve/kStackSafeLimit mirror
// Pdp7Assembler.cpp's own storeWord() checks, reapplied here at
// link time since address resolution there is deferred to a relocation.

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

// Checks that a resolved address is both in-range and clear of where the
// runtime stack (__sp, starting at 8191 and growing downward with no
// bounds check of its own) will reach.
void checkAddressSafe(uint32_t resultingAddress) {
    if (resultingAddress >= kAddressFieldLimit) {
        std::ostringstream msg;
        msg << "address " << resultingAddress << " (0" << std::oct << resultingAddress
            << " octal) is past the pdp7's 8192-word (020000 octal) direct-addressing field limit - "
               "too many/too large object files given; a reference to any address at or beyond 8192 "
               "would silently corrupt the instruction it's stored in instead of erroring, so this is "
               "refused up front";
        fail(msg.str());
    }
    if (resultingAddress >= kStackSafeLimit) {
        std::ostringstream msg;
        msg << "address " << resultingAddress << " (0" << std::oct << resultingAddress << " octal) is within "
            << std::dec << kStackReserve
            << " words of the pdp7's own 8192-word address space top - too close to where the runtime "
               "call stack (__sp, starting at 8191 and growing downward with no reserved region or "
               "bounds check of its own) will reach during real execution; a reference placed here "
               "wouldn't error, it would just get silently overwritten by ordinary stack usage at some "
               "real call depth, so this is refused up front instead - too many/too large object files "
               "given, the same as the direct-addressing field limit above, just with a safety margin "
               "for the stack it doesn't know about";
        fail(msg.str());
    }
}

void patchPdp7Or(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    checkAddressSafe(value);
    write18(image, siteOffset, read18(image, siteOffset) | value);
}
// Handles a same-file address shifted by this object's base, or an
// external symbol combined with `+` in the source. The low 13 address
// bits are isolated from any opcode bits above them before the safety
// check, so opcode bits are never mistaken for part of the address.
void patchPdp7Add(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    uint32_t existing = read18(image, siteOffset);
    uint32_t addrPart = existing & (kAddressFieldLimit - 1);
    checkAddressSafe(addrPart + value);
    write18(image, siteOffset, existing + value);
}
void patchPdp7Sub(std::vector<uint8_t> &image, uint32_t siteOffset, uint32_t value, uint32_t) {
    write18(image, siteOffset, read18(image, siteOffset) - value);
}

}  // namespace

std::unique_ptr<TargetLinker> makePdp7Linker(uint32_t baseAddressWords) {
    GenericLinkerConfig config;
    config.baseAddress = baseAddressWords * 3;
    config.machineType = kPdp7CoffMachine;
    // Beyond the entry sequence, this is the single object that defines
    // the shared runtime (see pdp7RuntimeAsm()), since every user object
    // leaves it as external references rather than defining its own
    // copy. Every routine is included, since this object has no
    // visibility into what the input files need.
    config.entryObject = assemblePdp7Object(
        "jms FN_main\nhlt\n" +
        pdp7RuntimeAsm(/*pushPop=*/true, /*divMod=*/true, /*uDivMod=*/true, /*genericLt=*/true));
    config.relocPatchers[kPdp7RelocOr] = patchPdp7Or;
    config.relocPatchers[kPdp7RelocAdd] = patchPdp7Add;
    config.relocPatchers[kPdp7RelocSub] = patchPdp7Sub;
    // Only "+"-combined relocations ever use the pseudo-symbol ".." (Pdp7Assembler.cpp's own
    // `needsBaseShift` always pushes '+'), so this is the only extractor `--gc-sections` needs.
    // The low 13 bits already sitting at the site - the same field patchPdp7Add's own `addrPart`
    // reads before adding to it - are that same-object target's own pre-link address in word
    // units, the same scale `patchPdp7Add`'s own `value` argument arrives in; GenericLinker's own
    // section math is all in packed-byte units (`addressUnitBytes = 3` below), so this converts
    // before returning, the same conversion GenericLinker itself undoes for every RelocPatchFn.
    config.addrFieldExtractors[kPdp7RelocAdd] = [](const std::vector<uint8_t> &image, uint32_t siteOffset) {
        return (read18(image, siteOffset) & (kAddressFieldLimit - 1)) * 3;
    };
    config.outputFormats = {"a7out", "list", "ptr", "rim", "coff", "aout"};
    config.addressUnitBytes = 3;
    config.outputUnitBits = 18;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace rcc
