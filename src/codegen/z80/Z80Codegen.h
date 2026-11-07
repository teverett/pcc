#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates this project's own Z80 assembly dialect (real, standard Z80
// mnemonics - see Z80Assembler.h) from IR, using the same memory-resident
// ac/bc/cx/dx/ptr pseudo-register design as ../i8080/I8080Codegen.h.
// Z80 is object-code compatible with the 8080 but adds instructions worth
// using directly, so this is its own backend rather than an 8080 alias:
//  - ADD HL,rr/SBC HL,rr: real 16-bit add and subtract-with-carry (the 8080
//    only has add/DAD), so `-` and all comparisons collapse to one SBC HL,DE.
//  - LD rr,(nn)/LD (nn),rr for BC/DE/SP: a single instruction, vs. the 8080's
//    stack round-trip through HL for any pair but HL itself.
//  - RL r/RR r: rotate-through-carry on any register, not just A (8080's
//    RAL/RAR).
// Still true on both chips: no hardware multiply/divide/variable-count
// shift, and no flag for signed less-than (same sign-bit-bias trick as the
// 8080 backend). Every jump/call is a full 16-bit absolute address.
// `int` is 16 bits.
class Z80Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "z80"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".sz80"; }
    std::vector<std::string> assembledFormats() const override {
        return {"raw", "coff", "elf", "macho", "aout", "objraw"};
    }
    // `raw` is a whole-program flat binary; every other format is a relocatable object.
    bool isObjectFormat(const std::string &format) const override { return format != "raw"; }
    std::string defaultAssemblerPath() const override { return ""; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in the `ac` memory cell

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned);
    void emitLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned);
    void emitEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, bool isUnsigned);
    void genShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned);
    void genIndex(const ir::Expr &e);
    void genAddrOf(const ir::Expr &operand);
    void genLocalArrayInit(const ir::Slot *slot);
    // CompoundAssign/CompoundDerefAssign's "new = old <op> rhs" step - `old` in
    // HL, `rhs` in DE (both already at `type`'s width/signedness), leaves the
    // raw new value in `ac`. `type` is never wide here (IRBuilder rejects it).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign/CompoundDerefAssign; `ptr` must already hold the resolved address.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-sign/zero-extends `ac` from its low byte when `to` is a `char`; a
    // compound op computed at 16-bit width can leave garbage in the high byte.
    void narrowAcTo(Type to);

    // isWide(t) is widthOfType(t) > 2 (long/long long); represented as
    // wordCountOf(t) (2 or 4) 16-bit words on the real stack, LSW on top.
    // elemShift(t) = log2(widthOfType(t)), used to scale an array index.
    int widthOfType(Type t) const;
    bool isWide(Type t) const;
    int wordCountOf(Type t) const;
    int elemShift(Type t) const;

    // Assumes HL holds the operand's address. A `char` is sign/zero-extended
    // into ac's 16 bits via the `rla`/`sbc a,a` idiom; anything else is a
    // plain 16-bit load.
    void loadOperand(Type type);
    // Assumes HL holds the target address and ac the value; writes only the low byte for a `char`.
    void storeOperand(Type type);

    // Implicit/explicit conversion. The real work is crossing the
    // native-word/wide boundary (extra sign/zero-extension words) and
    // re-narrowing a `char` result.
    void genConvert(const ir::Expr &e);

    // Scratch cell label "wa0".."wa3"/"wb0".."wb3" (i=0 is the
    // least-significant word). Z80 has no SP-relative addressing, so wide
    // helpers pop operands into these fixed cells instead of operating on
    // the stack in place.
    static std::string wideScratch(const char *role, int i) { return std::string("w") + role + std::to_string(i); }

    // Pops `words` words off the stack (least-significant first) into scratch cells.
    void popWideInto(const char *role, int words);
    // Pushes scratch cells back onto the stack, most-significant word first (so LSW ends up on top).
    void pushWideFrom(const char *role, int words);

    void genWideLoad(const ir::Slot *slot, int words);
    void genWideLoadLiteral(long value, int words);
    void genWideStore(const ir::Slot *slot, int words);
    // Same as genWideLoad()/genWideStore(), but the address is already in `ptr` (from genIndexAddr()) rather than a Slot.
    void genWideLoadAtPtr(int words);
    void genWideStoreAtPtr(const char *role, int words);
    void genWideAdd(const ir::Expr &lhs, const ir::Expr &rhs, bool isAdd, int words);
    void genWideNegate(int words);
    void genWideBitwise(const ir::Expr &lhs, const ir::Expr &rhs, const std::string &op, int words);
    void genWideEqualityCompare(const ir::Expr &lhs, const ir::Expr &rhs, bool wantEqual, int words);
    void genWideLessCompare(const ir::Expr &a, const ir::Expr &b, bool invert, bool isUnsigned, int words);
    void genWideCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs, bool isUnsigned, int words);
    void genWideShift(const ir::Expr &lhs, const ir::Expr &rhs, bool isLeft, bool isUnsigned, int words);
    // Deliberately unimplemented; see the .cpp definitions.
    void genWideMul(const ir::Expr &lhs, const ir::Expr &rhs, int words);
    void genWideDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder, int words);

    // Sets the real Z flag true/false from the value genExpr(e) just left behind.
    void testTruthy(const ir::Expr &e);

    // Leaves slot[idxExpr]'s effective address in `ptr`.
    void genIndexAddr(const ir::Slot *slot, const ir::Expr &idxExpr);

    // Leaves `slot`'s effective address in HL. HL has no direct load from IX,
    // so this goes through the real stack ("push ix"/"pop hl"). A Local lives
    // at ix-(frameOffset+extraBytes) (frame grows down from ix); a Param at
    // ix+frameOffset+extraBytes. `extraBytes` lets a caller address an array
    // element or a wide scalar's word i.
    void loadFrameAddr(const ir::Slot *slot, long extraBytes = 0);

    // Reads the 16-bit value at [HL] into (loReg, hiReg), low byte first; HL ends up pointing at the high byte.
    void readWordAt(const std::string &loReg, const std::string &hiReg);

    // Assumes HL holds the target address; writes ac's two bytes there without modifying HL.
    void storeAcIndirect();

    // `ld hl,(src)` then `ld (dst),hl`.
    void copy16(const std::string &dst, const std::string &src);
    // Loads a 16-bit cell directly into DE or BC via a single `LD rr,(nn)` (no 8080 equivalent).
    void loadPair(const std::string &pairReg, const std::string &src);
    // The reverse: a single `LD (nn),rr`.
    void storePair(const std::string &dst, const std::string &pairReg);
    // Shifts the 16-bit cell `lbl` one bit via HL; `OR A` clears the carry-in
    // (no explicit clear-carry instruction exists), then RL/RR chains
    // directly across L and H.
    void shiftOneBit(const std::string &lbl, bool isLeft);

    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "g_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "p_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return slot->sourceName + "_" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "fn_" + fn->sourceName; }

    std::string allocTemp() { return "t" + std::to_string(tempCounter_++); }
    void declareTemp(const std::string &lbl);
    void declareArray(const std::string &lbl, const std::vector<ir::ArrayInitElem> &values, Type elemType);
    std::string arrayElemText(const ir::ArrayInitElem &e);
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: db lo, hi, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // Current function's frame-reservation size in bytes; Return must unwind by the same amount.
    int frameReserveBytes_ = 0;
    // One {continue label, break label} per open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
};

}  // namespace pcc
