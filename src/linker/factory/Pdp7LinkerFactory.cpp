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
    // Beyond the entry sequence itself, this object is also the one
    // place that defines the software stack/frame mechanism's shared
    // runtime cells (`__sp`/`__fp`/`__stmp`/`__stmp2`/`__addr`/
    // `__retval`/`__retaddr` - see Pdp7Codegen.h's own header comment
    // and pushAC()/emitPrologue()'s comments for what each one does)
    // and the shared division/modulo routine (`__divmod` and its own
    // `__dm_a`/`__dm_b`/`__dm_q`, exactly Pdp7Codegen::
    // ensureDivModRoutine()'s own body) - under real separate
    // compilation, Pdp7Codegen::generate() deliberately leaves every
    // one of these as a plain external reference in every object file
    // instead of defining its own copy (see that function's own
    // comment on why: each file assembling its own private copy,
    // never unified by the linker, silently broke any call crossing
    // an object-file boundary - confirmed directly). `config.
    // entryObject` is already the one object every link includes
    // exactly once regardless of how many user files there are, so
    // it's the natural single owner of these too. The divmod routine
    // is included unconditionally (a handful of extra words) rather
    // than only when some linked file needs it, since this object is
    // built once, here, with no visibility into what any later link's
    // own input files will actually use.
    config.entryObject = assemblePdp7Object(
        "jms FN_main\n"
        "hlt\n"
        "__one: 1\n"
        "__zero: 0\n"
        "__sp: 8100\n"
        "__fp: 0\n"
        "__stmp: 0\n"
        "__stmp2: 0\n"
        "__addr: 0\n"
        "__retval: 0\n"
        "__retaddr: 0\n"
        "__dm_a: 0\n"
        "__dm_b: 0\n"
        "__dm_q: 0\n"
        "__divmod: 0\n"
        "   dzm __dm_q\n"
        "J_dmloop:\n"
        "   lac __dm_b\n"
        "   cma\n"
        "   tad __one\n"
        "   tad __dm_a\n"
        "   sma\n"
        "   jmp J_dmbody\n"
        "   jmp J_dmdone\n"
        "J_dmbody:\n"
        "   dac __dm_a\n"
        "   isz __dm_q\n"
        "   jmp J_dmloop\n"
        "J_dmdone:\n"
        "   lac __dm_q\n"
        "   jmp i __divmod\n");
    config.relocPatchers[kPdp7RelocOr] = patchPdp7Or;
    config.relocPatchers[kPdp7RelocAdd] = patchPdp7Add;
    config.relocPatchers[kPdp7RelocSub] = patchPdp7Sub;
    config.outputFormats = {"a7out", "list", "ptr", "rim", "coff", "aout"};
    config.addressUnitBytes = 3;
    config.outputUnitBits = 18;
    return std::make_unique<GenericLinker>(std::move(config));
}

}  // namespace pcc
