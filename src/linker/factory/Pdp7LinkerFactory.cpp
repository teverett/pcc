#include "Pdp7LinkerFactory.h"

#include <sstream>
#include <stdexcept>

#include "linker/GenericLinker.h"
#include "codegen/pdp7/Pdp7Assembler.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp7LinkerFactory: " + msg); }

constexpr uint32_t kMask18 = 0777777;

// kAddressFieldLimit/kStackReserve/kStackSafeLimit are shared with
// Pdp7Assembler.cpp's own `storeWord()` (see Pdp7Assembler.h's own
// header comment on both) - this file's job is the real separate-
// compilation/`--link` path's own copy of the same two checks, where
// the equivalent of `storeWord()`'s single-pass `..` resolution
// happens later, via a relocation (see makePdp7Linker()'s own
// `__sp: 8191` and this namespace's `patchPdp7Or`/`patchPdp7Add`
// below).

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

// Shared by patchPdp7Or (a fresh OR-combine, where `resultingAddress`
// is just `value` itself - the whole address being combined in) and
// patchPdp7Add's own address case below (where `resultingAddress` is
// this word's own already-resolved low-13-bit address plus the base
// being added, isolated from whatever opcode bits occupy the bits
// above it - see patchPdp7Add's own comment for why that isolation
// matters). Either way, this is the one real invariant that matters:
// whatever ends up in the low 13 bits once this patch is done has to
// be both a real, in-range address and comfortably clear of where the
// runtime stack will reach.
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
// Used for two different-looking but equally address-shaped cases -
// Pdp7Assembler.cpp's own two emitters of a `'+'`-typed relocation
// (see its own `processLine()` comment on `needsBaseShift`, and
// `evalExpr()`'s `pending` syllables): a same-file address shifted by
// this object's own real base once linked (the one `value` is always
// that base itself), and an external symbol combined with `+` in the
// source (`jmp externalLabel+5`, say - `value` there is that
// symbol's own resolved address). Both leave this word's existing low
// 13 bits holding a *real address* that was only ever 0-relative
// (or, for a word with no opcode bits riding along in the high bits
// at all - a plain data declaration, not an instruction - the entire
// word) until this patch adds the missing piece; any opcode bits
// riding along in the bits above that field (every real pdp7 opcode
// occupies bit 13 or higher - see the `builtins()` table above) ride
// through untouched, exactly the same as every other reference this
// backend ever makes to that same label would. Isolating the low 13
// bits before checking is what lets this use the same
// checkAddressSafe() as a fresh OR-combine does without tripping over
// those opcode bits themselves being large numbers - confirmed
// directly: a same-file reference engineered to land within
// kStackReserve of 8191 silently corrupted the runtime stack (see
// kStackReserve's own comment) with no error at all before this
// isolation was added, even though an otherwise-identical *cross*-
// file reference to the same address was already refused by
// patchPdp7Or above.
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
        "__sp: 8191\n"
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
