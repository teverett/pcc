#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace rcc {

// Generates this project's own MC6809 assembly dialect (real, standard 6809
// mnemonics - see M6809Assembler.h) from IR, using the same memory-resident
// ac/_bc/cx/dx/ptr pseudo-register design as ../z80/Z80Codegen.h and
// ../i8080/I8080Codegen.h. The 6809 is genuinely more capable than either
// chip in ways this backend uses directly rather than imitating their
// workarounds:
//  - Real indexed addressing (LEAX/LEAY/LEAS/LEAU and every load/store/ALU
//    mnemonic's indexed form) means a Local/Param's frame address is one
//    `leax <off>,u` - no push/pop-through-a-register dance like Z80's `ix`
//    (which has no direct move to/from HL), and no software frame pointer
//    like the 8080's.
//  - Real N/V (overflow) flags on SUBD/CMPD mean a signed comparison needs
//    no sign-bit-bias trick (same trick both Z80/8080 still rely on):
//    unsigned picks BLO/BLS/BHI/BHS, signed picks BLT/BLE/BGT/BGE, directly.
//  - Real hardware MUL (8x8->16 unsigned) is used as a fast path when both
//    operands of a non-wide `*` are statically `unsigned char` (see
//    genMul()'s own comment); every other `*` still falls back to a
//    shift-and-add loop, since a correctness-verified 16x16 multiply built
//    from three partial MULs is new, unverified logic, not a template port.
//  - LDD/STD/CMPD/ADDD/SUBD operate on D (A:B) as a real 16-bit register
//    pair directly against a memory operand, so most of what Z80 needs a
//    second register (DE) staged for collapses into one instruction against
//    a pseudo-register cell.
// Still true here: no hardware divide, no variable-count shift (genDivMod/
// genShift stay bit-serial loops), and `long`/`long long` `*`/`/`/`%` are
// unsupported (see genWideMul/genWideDivMod). Every jump/call this backend
// emits is extended addressing or a long branch with a full 16-bit
// reference - never 8-bit-relative. The 6809 is big-endian; `int` is 16
// bits. Every indexed access (frame-relative or otherwise) this backend
// emits uses the 8-bit-offset form (see M6809Assembler.h), so a function
// whose frame or whose Local/Param offsets exceed +/-127 bytes will fail to
// assemble - a real, currently undocumented-elsewhere limitation worth
// knowing before trusting a large stack frame on this target.
class M6809Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "6809"; }
    TypeWidths typeWidths() const override { return {1, 2, 2, 4, 8}; }
    std::string sourceExtension() const override { return ".s6809"; }
    std::vector<std::string> assembledFormats() const override { return {"raw", "coff", "elf", "aout"}; }
    // `raw` is a whole-program flat binary; every other format is a relocatable object.
    bool isObjectFormat(const std::string &format) const override { return format != "raw" && format != "asm"; }
    std::map<std::string, std::string> perFileAsm() const override { return perFileAsm_; }
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
    // `_bc`, `rhs` in `ac` (both already at `type`'s width/signedness), leaves
    // the raw new value in `ac`. `type` is never wide here (IRBuilder rejects it).
    void genCompoundOp(const std::string &op, Type type);
    // Shared tail for CompoundAssign/CompoundDerefAssign; `ptr` must already hold the resolved address.
    void genCompoundThroughPtr(const ir::Expr &e);
    // Re-sign/zero-extends `ac` from its low byte when `to` is a `char`; a
    // compound op computed at 16-bit width can leave garbage in the high byte.
    void narrowAcTo(Type to);

    // isWide(t) is widthOfType(t) > 2 (long/long long); represented as
    // wordCountOf(t) (2 or 4) 16-bit words on the real stack, LSW on top.
    int widthOfType(Type t) const;
    bool isWide(Type t) const;
    int wordCountOf(Type t) const;

    // Assumes X holds the operand's address. A `char` is sign/zero-extended
    // into ac's 16 bits via the real SEX instruction (unsigned just zeroes
    // A); anything else is a plain 16-bit load.
    void loadOperand(Type type);
    // Assumes X holds the target address and ac the value; writes only the low byte for a `char`.
    void storeOperand(Type type);

    // Implicit/explicit conversion. The real work is crossing the
    // native-word/wide boundary (extra sign/zero-extension words) and
    // re-narrowing a `char` result.
    void genConvert(const ir::Expr &e);

    // Scratch cell label "wa0".."wa3"/"wb0".."wb3" (i=0 is the
    // least-significant word). The 6809's own stack (S) is already the frame
    // stack a call/return sequence depends on, so wide helpers park operands
    // in these fixed cells rather than risk disturbing it mid-expression.
    static std::string wideScratch(const char *role, int i) { return std::string("w") + role + std::to_string(i); }

    // Pops `words` 16-bit words off the real stack (least-significant
    // first, via PSHS/PULS A,B) into scratch cells.
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

    // Leaves `slot`'s effective address in X, via one `leax <off>,u` - the
    // 6809's real indexed addressing reaches straight off the fixed frame
    // pointer `u`, unlike Z80's push/pop-through-ix dance or the 8080's
    // software frame pointer. A Local lives at u-(frameOffset+extraBytes)
    // (frame grows down from u); a Param at u+frameOffset+extraBytes. A
    // Local array's address is its element 0, its lowest, with `extraBytes`
    // counting upward from there. `extraBytes` lets a caller address an
    // array element or a wide scalar's word i.
    void loadFrameAddr(const ir::Slot *slot, long extraBytes = 0);

    // One bit of a logical shift (0 shifted in either direction) on the
    // 16-bit cell `lbl`, via D: ASLB/ROLA chains the carry from B up into A
    // for a left shift; LSRA/RORB chains it from A down into B for a right
    // shift. Used by genMul()'s/genDivMod()'s doubling/halving and by
    // genShift()'s unsigned right-shift case (its signed case shifts
    // arithmetically inline instead - see genShift()).
    void shiftOneBit(const std::string &lbl, bool isLeft);

    // Loads a 16-bit cell's value into X (the address-holding register for
    // every loadOperand()/storeOperand() call), e.g. after copying a pointer
    // value into `ptr`.
    void loadAddrX(const std::string &cell);

    // `ldd [src]` then `std [dst]`.
    void copy16(const std::string &dst, const std::string &src);

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
    // Maps an ir::Stmt::labelId (switch's own Label/Goto - see IR.h) to this
    // backend's own label text, generated the first time either a Label
    // defining it or a Goto referencing it is seen.
    std::string irLabel(int id) {
        auto it = irLabels_.find(id);
        if (it != irLabels_.end()) return it->second;
        std::string name = newLabel();
        irLabels_[id] = name;
        return name;
    }

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: dw/db value, ..." data lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;

    int tempCounter_ = 0;
    int labelCounter_ = 0;
    // Current function's frame-reservation size in bytes; Return must unwind by the same amount.
    int frameReserveBytes_ = 0;
    // One {continue label, break label} per open loop, innermost last.
    std::vector<std::pair<std::string, std::string>> loopLabels_;
    std::map<int, std::string> irLabels_;  // see irLabel()
};

}  // namespace rcc
