#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates ATmega328P assembly (real AVR mnemonics, see AVRAssembler.h) from IRBuilder's IR.
// `--target=avr` names the ISA family; codegen itself is scoped to the ATmega328P specifically:
// 32 KB flash (program memory, byte-addressed here for code purposes - see AVRAssembler.h's own
// comment on `call`/`jmp`'s word-addressed encoding), 2 KB SRAM (data memory) starting at 0x0100,
// 1 KB EEPROM untouched by this backend, and the 32 real 8-bit GPRs R0-R31.
//
// Harvard architecture: a Global's storage is always SRAM (via `ram`/`lds`/`sts`), never flash -
// AVR can't write flash at runtime without self-programming, which this backend doesn't support,
// so even a `const` array lives in writable SRAM like every other target here that has no cheap
// read-only placement. SRAM has no static initial contents, so a Global's initializer instead
// runs once at program start (`_start` in a whole program; `fn_main`'s own prologue in object
// mode, since the synthesized entry sequence can't see a user object's globals - see
// AVRLinkerFactory.cpp) - the same split MCS51Codegen.cpp uses for its XDATA globals. Known
// limitation shared with every other Harvard-data target here (mcs51/pic16/pic18, none of which
// are wired into tests/run_link_tests.sh either): under real separate compilation, a Global's
// initializer only ever runs out of the one object defining `main`, so an `extern` Global actually
// *defined* (with an initializer) in a different linked object is never initialized before main's
// object reads it - only the single-file (non-`--link`) case, and a Global whose only initializer
// is the implicit zero, are relied on across an object boundary (see tests/avr/*link* coverage,
// or lack thereof, and run_link_tests.sh's own `is_known_unsupported`).
//
// `int`/`short` are 16 bits (one register pair), `char` 8, `long` 32 and `long long` 64 - the
// same choice as every other narrow-register target here (mcs51/stm8/z80/pic16/...), since `int`
// matches the hardware's native 16-bit pointer/index width rather than VAX/ARM's 32-bit `int`.
//
// Register roles (R0-R31, none of mcs51/stm8's software pseudo-register/frame-stack style - every
// value genuinely lives in a real GPR or is addressed with real `ld`/`st`/`lds`/`sts`):
//   r0:r1     - MUL's fixed hardware destination; never holds a live value between statements.
//   r16:r17   - "ac0:ac1", genExpr()'s result for a type up to 2 bytes (char/short/int/pointer) -
//               r16 the low byte. Both sit in the r16-r31 range `ldi` requires.
//   r18:r19   - "bc0:bc1", the other operand of a narrow (<=2 byte) binary op.
//   r20,r21   - "t0,t1", byte-wide transfer scratch for combineBytes()'s per-byte loop and every
//               other width-generic byte-at-a-time helper (genDivMod(), genWideMul(), ...).
//   r22       - "t2", one more byte of transfer scratch where a loop needs three bytes live at
//               once (e.g. genNarrowMul()'s/genWideMul()'s partial-product accumulator).
//   r23       - "zero", a permanent 0x00 - cleared once by the entry sequence and never written
//               again, needed as `adc`'s second operand to propagate a carry with no other real
//               addend (plain AVR has no "add with carry, immediate").
//   r24,r25   - "tmp,tmp2", loop counters (shift count, divide bit counter) and addConst16()'s own
//               scratch.
//   r26:r27   - X, the address register genIndexAddr()/genAddrOf() leave a computed SRAM address
//               in, and the generic wide (4/8-byte) byte-copy helpers' source/destination pointer.
//   r28:r29   - Y, the frame pointer (see genPrologue()'s own comment) - also addressable as plain
//               registers when a frame offset needs the `movw`+`subi`/`sbci` absolute fallback.
//   r30:r31   - Z, a second address register: LDD/STD's other displacement-capable pointer, and
//               CallPtr's indirect-call target (`icall` reads its word address from Z).
//
// A type wider than 2 bytes (`long`/`long long`) is staged through four fixed, 8-byte SRAM
// scratch buffers (`__wa`/`__wb`/`__wc`/`__wd`) rather than kept across registers - AVR has only
// 32 of them, and this backend spends them on making the common char/short/int/pointer case fully
// register-resident instead of splitting a handful across every width. A wide value is always
// safely saved (pushed byte-by-byte to the hardware stack, see pushPlace()) before a nested call
// the same way a narrow one is, so this is no less reentrant than an ordinary register. Every
// binary/compound/compare operator supports `long`/`long long` as a result of this staging, just
// via a byte-at-a-time loop instead of a 1-2 instruction register sequence.
//
// The calling convention is a real hardware stack, Y-relative frame, matching normal AVR-GCC
// practice: the caller pushes arguments right-to-left (byte-by-byte, narrowest unit first per
// argument - see genExpr(Call)), `call`/`rcall` pushes the 2-byte return address, and the callee's
// prologue pushes the caller's Y, then sets Y to the new frame's lowest address and lowers SP by
// `frameSize` to reserve Locals there (see genPrologue()). Y + frameSize therefore addresses the
// saved caller-Y/return-address block, and Y + frameSize + (Param::frameOffset) the first pushed
// argument - matching ir::IR.h's own "two native ints reserved ahead of Param 0" convention
// exactly, since one native int here is 2 bytes (saved Y) and the other 2 (the return address).
// `ldd`/`std`'s displacement only reaches 0-63, so a frame offset past that falls back to
// computing an absolute address in Z (`movw`+add off Y) and addressing through that instead - see
// materializeAddr().
//
// Hardware `mul` (unsigned 8x8->16, forced into r1:r0) is used directly for a narrow multiply (the
// well-known 3-`mul` 16x16->16 sequence - see genNarrowMul()); a wide one is schoolbook
// multiplication via the same instruction, only as many partial products as affect the kept low
// `w` bytes. There's no hardware divide at any width - genDivMod()/genWideDivMod() are a bit-
// serial restoring-division loop (unsigned only; signed negates both operands' magnitudes first
// and fixes the quotient's/remainder's sign after, the same idea as every other target here with
// no hardware divide).
//
// `brxx` only reaches a 7-bit signed word displacement (-64..+63); AVRCodegen.cpp never emits one
// directly; every conditional branch instead goes through safeBranch() - the inverted short
// branch, over an always-in-range `jmp`, idiom MCS51Codegen.cpp's own safeBranch() uses (not
// VaxCodegen.h's `j<cc>` assembler-level pseudo-op - this dialect's assembler only ever emits real
// AVR instructions).
//
// A 16-bit AND/OR/XOR/ADD/SUB is two chained byte instructions (the second using `adc`/`sbc` for
// +/-, a plain `and`/`or`/`eor` for the bitwise ops, no carry involved) - see combineBytes(),
// shared with the wide (4/8-byte) case.
class AVRCodegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "avr"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".savr"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    // A place genExpr()'s result (or a binary op's other operand) can live: either `width`
    // consecutive registers starting at `baseReg` (a narrow, <=2 byte value - "ac"/"bc"), or
    // `width` consecutive SRAM bytes at a fixed label (a wide, 4/8 byte value staged through one
    // of the `__w*` buffers) - see this file's own header comment.
    struct Place {
        bool isSram = false;
        int baseReg = 0;
        std::string label;
    };
    static std::string regName(int n) { return "r" + std::to_string(n); }
    // genExpr()'s result location for a `width`-byte value.
    Place canonicalPlace(int width) const;
    // The "other operand" location a binary op's rhs/lhs lands in, paired with canonicalPlace().
    Place secondaryPlace(int width) const;
    // Loads/stores one byte of `p` (index `i`, 0 = least significant) into/from register `reg` -
    // a no-op `mov` is skipped. The one place every width-generic byte loop touches a Place.
    void loadByte(const Place &p, int i, const std::string &reg);
    void storeByte(const Place &p, int i, const std::string &reg);
    // Pushes/pops all `width` bytes of `p` onto/from the hardware stack, byte 0 pushed first (so
    // popping restores the same byte order) - how a binary op's lhs survives evaluating rhs, and
    // how a wide value survives a nested call (see this file's own header comment).
    void pushPlace(const Place &p, int width);
    void popPlace(const Place &p, int width);
    // Copies `width` bytes from `src` to `dst`, transferring through `t0` one byte at a time.
    void copyPlace(const Place &dst, const Place &src, int width);
    // Zeros/sign-or-zero-extends `p` from `fromWidth` out to `toWidth` bytes in place
    // (`isUnsigned` picks zero- vs sign-extension; never called with toWidth <= fromWidth).
    void extendPlace(const Place &p, int fromWidth, int toWidth, bool isUnsigned);

    void genFunction(const ir::FunctionBody &fn);
    // Pushes Y, sets Y to this call's frame's lowest address and lowers SP by `fn->frameSize` to
    // reserve Locals there (see this file's own header comment); sets curFrameSize_.
    void genPrologue(const ir::Function *fn);
    // Raises SP back by curFrameSize_ (via Y) and pops Y - the exact inverse of genPrologue(),
    // used by both genFunction()'s fall-through exit and Return's early one. Does not emit `ret`.
    void genEpilogue();

    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in canonicalPlace(widthOfType(e.type))

    void genBinary(const ir::Expr &e);
    // The shared add/sub/bitwise byte loop: `acc = acc OP operand`, `width` bytes, LSB (index 0)
    // first - `op` is one of "+","-","&","|","^". `+`/`-` thread the real carry/borrow across
    // bytes via `adc`/`sbc`; the bitwise ops don't need to. Used directly on
    // canonicalPlace/secondaryPlace by genBinary() and genCompoundOp() alike, just with `acc`/
    // `operand` the other way around (see genCompoundOp()'s own comment).
    void combineBytes(const std::string &op, int width, const Place &acc, const Place &operand);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    // ORs every byte of canonicalPlace(width) together into `t0`, then `and t0,t0` - leaves SREG's
    // Z flag set exactly when the value is all-zero-bits, for an If/While/&&/||/!/Ternary test.
    void testCanonicalNonzero(int width);
    // `width`-byte dividend in secondaryPlace, divisor in canonicalPlace (genBinary()'s evaluation
    // order); leaves the quotient or remainder in canonicalPlace. Always staged through the
    // `__wc`/`__wd` scratch buffers (even at a narrow width - division has no fast register path
    // on hardware with no divide instruction at all) as an unsigned bit-serial restoring-division
    // loop (8*width iterations); `isUnsigned` false wraps it with the usual negate-both-operands'-
    // magnitudes-then-fix-up-the-quotient's/remainder's-sign dance (flags kept in `tmp2`/Z's low
    // byte, both otherwise unused across this call).
    void genDivMod(int width, bool wantRemainder, bool isUnsigned);
    void genNarrowMul();  // 2-byte canonical/secondary -> canonical, via 3 hardware `mul`s.
    void genWideMul(int width);  // 4/8-byte schoolbook multiply via hardware `mul`, through `__wc`.
    // Shifts canonicalPlace(width) left/right by the count already copied into `tmp` (see
    // genBinary()'s own shift case - the count is always a plain, narrow int, never `width`
    // bytes), in place; `isUnsigned` picks the fill bit for a right shift only.
    void genShift(int width, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);         // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot, &slot[idx] or &function

    // Leaves slot[idxExpr]'s effective SRAM address in X.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);
    // Loads/stores `width` bytes between [X] and `place`, advancing X past them (post-increment) -
    // the generic bridge between "a computed address" and a Place, used by genIndex()/
    // DerefAssign/CompoundAssign's indexed form.
    void loadFromX(const Place &place, int width);
    void storeToX(const Place &place, int width);

    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - canonicalPlace(width) must
    // already hold `old` and secondaryPlace(width) `rhs`; computes `canonical = canonical OP
    // secondary` (old op rhs, via combineBytes() with the roles reversed from genBinary()'s own
    // call), leaving the raw (not yet re-narrowed) new value in canonicalPlace(width). A compound
    // "/"="%=" swaps canonical/secondary's contents first, since genDivMod() assumes the opposite
    // (lhs-in-secondary, rhs-in-canonical) role.
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign's indexed form and CompoundDerefAssign - X must already hold
    // the resolved address on entry, and is stashed on the hardware stack across `e.rhs`'s
    // evaluation (which can clobber it).
    void genCompoundThroughX(const ir::Expr &e);

    int widthOfType(Type t) const { return rcc::widthOf(t, typeWidths()); }

    // Address of `slot`, a Local or Param, as a Y-relative displacement (see this file's own
    // header comment) - may exceed `ldd`/`std`'s 0-63 range, in which case loadSlotInto()/
    // storeSlotFrom()/genIndexAddr() fall back to materializeAddr()'s absolute-address form.
    int frameDisplacement(const ir::Slot *slot) const;
    // Emits `addConst16`'s "load a 16-bit literal and add/subtract it" idiom into register pair
    // (`loReg`,`loReg+1`) - `value` may be negative (`subi`/`sbci` for the magnitude would need
    // `loReg` in r16-r31, which X/Y/Z aren't all guaranteed to be, so this always goes through
    // `tmp`/`tmp2` and `add`/`adc` instead, working on any register pair).
    void addConst16(int loReg, int32_t value);
    // Materializes an absolute SRAM address into Z: `g_name[+byteOffset]` for a Global,
    // `Y + frameDisplacement(slot) + byteOffset` for a Local/Param.
    void materializeAddr(const ir::Slot *slot, int byteOffset);
    // 2's complement negation of `place`'s `width` bytes, in place.
    void negatePlace(const Place &place, int width);

    // Loads/stores `slot`'s whole scalar value between its storage and `place`.
    void loadSlotInto(const ir::Slot *slot, const Place &place, int width);
    void storeSlotFrom(const ir::Slot *slot, const Place &place, int width);
    void loadSlotValue(const ir::Slot *slot) { loadSlotInto(slot, canonicalPlace(widthOfType(slot->type)), widthOfType(slot->type)); }
    void storeSlotValue(const ir::Slot *slot) { storeSlotFrom(slot, canonicalPlace(widthOfType(slot->type)), widthOfType(slot->type)); }

    // Loads a compile-time-constant value into `place` (zero-filling or sign/zero-extending a
    // narrower literal, per `type`).
    void loadImmediateInto(const Place &place, int width, uint64_t value);

    void genConvert(const ir::Expr &e);
    void genLocalArrayInit(const ir::Slot *slot);  // stores arrayInit into a Local's frame slots

    // Raises (positive) or lowers (negative) the real hardware SP by `delta` bytes, through Z -
    // genExpr(Call)'s own cleanup of the arguments it pushed (`call` itself pops nothing).
    void adjustSP(int delta);

    static std::string label(const ir::Slot *slot) { return "g_" + slot->sourceName; }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    void declareGlobal(const ir::Slot *slot);
    void declareGlobalArray(const ir::Slot *slot);
    // Appends init_'s instructions writing one array element (a literal or a symbol's address)
    // into `elemLabel` (`label`, or `label+<byte offset>` past the first element).
    void genArrayElementInit(const std::string &elemLabel, Type elemType, const ir::ArrayInitElem &e);

    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }
    // The inverted-short-branch-over-a-long-jump idiom every conditional branch in this backend
    // goes through (see this file's own header comment); `s`/`branchIfClear` pick which real
    // `brxx` mnemonic (AVRAssembler.cpp's own breq/brne/brge/brlt/brlo/brsh) tests the condition
    // this call means "branch if true" for.
    void safeBranchIfTrue(const std::string &branchMnemonic, const std::string &target);
    void safeBranchIfFalse(const std::string &branchMnemonic, const std::string &target);

    std::ostringstream code_;
    std::ostringstream initCode_;  // Globals' initial-value `sts` writes - see this file's own header comment.
    std::vector<std::string> ramDecls_;  // "lbl: ram <count>" lines, emitted once, at the end.
    std::map<std::string, std::string> perFileAsm_;

    int labelCounter_ = 0;
    int curFrameSize_ = 0;  // set by genPrologue(), read by Return/genEpilogue().
    std::vector<std::pair<std::string, std::string>> loopLabels_;  // {continue label, break label}
    std::map<int, std::string> irLabels_;
};

}  // namespace rcc
