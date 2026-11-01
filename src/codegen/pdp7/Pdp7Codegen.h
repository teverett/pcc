#pragma once

#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "codegen/Codegen.h"
#include "ir/IR.h"

namespace pcc {

// Generates PDP-7 assembly (as.s / as7 / pdp-assembler syntax) for
// the IR built by IRBuilder (see IR.h: every name is already
// resolved to a Slot/Function by the time this runs, so this class
// is pure instruction selection - no symbol table of its own). See
// ../../../README.md for the exact instruction-level conventions
// this relies on (verified against the real pdp7 simulator, not
// just inferred from the opcode table): dac leaves AC unchanged,
// sza/sma/sad skip in the directions their names say, dac/lac move
// whole 18-bit words, and the testmul.s-style "mul" / "lacq" pair
// gives a direct hardware multiply.
//
// Every variable, temporary and literal is a single statically
// allocated memory word - there is no stack and no recursion
// support (see README's "Scope and limitations").
//
// pdp7as truncates every label to 8 characters (Assembler.cpp's
// truncate8()), so every label here is a short synthetic
// "<letter><N>" name: "S<slot id>" for an ir::Slot (global, local or
// param alike - one flat id space, assigned once by IRBuilder),
// "F<function id>" for an ir::Function, and "T<N>"/"N<N>"/"J<N>"
// (this class's own counters) for codegen-internal scratch cells,
// literal constants and jump targets, none of which exist in the IR
// since they're pure implementation detail of this backend's
// expression evaluation strategy.
class Pdp7Codegen : public Backend {
   public:
    std::string generate(const ir::Program &prog, bool objectMode = false) override;
    std::string name() const override { return "pdp7"; }
    std::string sourceExtension() const override { return ".s"; }
    std::vector<std::string> assembledFormats() const override { return {"a7out", "list", "ptr", "rim", "coff"}; }
    std::string defaultAssemblerPath() const override { return "../pdp-assembler/build/bin/pdp7as"; }
    std::string buildAssembleCommand(const std::string &assemblerPath, const std::string &format,
                                      const std::string &srcPath, const std::string &outPath) const override;
    std::map<std::string, std::string> perFileAsm() const override;
    std::optional<std::string> assembleInProcess(const std::string &asmText,
                                                  const std::string &format) const override;

   private:
    void genFunction(const ir::FunctionBody &fn);
    void genStmt(const ir::Stmt &s);
    void genExpr(const ir::Expr &e);  // leaves the result in AC

    void genBinary(const ir::Expr &e);
    void genCompare(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void emitEq(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    void emitLt(const ir::Expr &lhs, const ir::Expr &rhs, bool negate);
    void genLogicalAnd(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalOr(const ir::Expr &lhs, const ir::Expr &rhs);
    void genLogicalNot(const ir::Expr &rhs);
    void genMul(const ir::Expr &lhs, const ir::Expr &rhs);
    void genDivMod(const ir::Expr &lhs, const ir::Expr &rhs, bool wantRemainder);
    void ensureDivModRoutine();
    void genBitwise(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genShift(const std::string &op, const ir::Expr &lhs, const ir::Expr &rhs);
    void genIndex(const ir::Expr &e);       // slot[idx] read
    void genAddrOf(const ir::Expr &operand);  // &slot or &slot[idx]

    // Global/Param slots and every Function get a *name*-based label
    // (never `id`) - real separate compilation means the file calling
    // a function and the file defining it compile with completely
    // independent `id` sequences, so only a label computable from
    // stable information (the function's own source name; a param's
    // owning-function name + index; a global's own source name,
    // already unique program-wide) can mean the same thing on both
    // sides - see IR.h's own comment on Slot::ownerFunctionName for
    // the fuller rationale. A Local never crosses a file boundary at
    // all, so it keeps the simple `id`-based label.
    static std::string label(const ir::Slot *slot) {
        if (slot->kind == ir::SlotKind::Global) return "G_" + slot->sourceName;
        if (slot->kind == ir::SlotKind::Param) {
            return "P_" + slot->ownerFunctionName + "_" + std::to_string(slot->paramIndex);
        }
        return "S" + std::to_string(slot->id);
    }
    static std::string label(const ir::Function *fn) { return "FN_" + fn->sourceName; }

    std::string allocTemp();
    void declareTemp(const std::string &label);
    void declareArray(const std::string &label, const std::vector<long> &values);
    std::string literalLabel(long value);
    std::string newLabel();

    // Emits code that computes &slot[idxExpr] into a fresh temp cell
    // and returns that temp's label; used by both an Index read and
    // an indexed Assign. See README's "Codegen conventions" for why
    // this needs `law` (to get slot's address into AC at all, when
    // slot is itself the array) or `lac` (to get slot's *value* -
    // when slot is a plain variable holding a pointer instead - see
    // slot->arraySize) plus one level of `i` indirection (to
    // dereference a runtime-computed address, since a PDP-7
    // instruction's own address field is fixed at assemble time).
    std::string computeElementAddress(const ir::Slot *slot, const ir::Expr &idxExpr);

    std::ostringstream code_;
    std::vector<std::string> trailer_;  // "label: value" data-word lines, emitted once at the end
    std::map<std::string, std::string> perFileAsm_;  // built by generate() - see Codegen.h's perFileAsm()

    std::unordered_map<long, std::string> literalCache_;
    int tempCounter_ = 0;
    int litCounter_ = 0;
    int labelCounter_ = 0;
    std::string curFunctionLabel_;
    bool needDivMod_ = false;
};

}  // namespace pcc
