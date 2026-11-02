#include "Arm64Assembler.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Arm64Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Group shape as every other backend's own assembler -
// see M68kAssembler.cpp's own comment.
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression - see
// Arm64Assembler.h's own comment on why there's no real `[xn]`
// bracketed A64 syntax here. `Reg` comes from recognizing a bare
// register name (carrying its own `width`: 0 for a `w`-named
// register, 1 for an `x`-named one, `wzr`, or `sp`); `Imm` from a bare
// (optionally signed) number; anything else (a label, or `label+N`)
// is `Mem`.
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    int width = 0;  // Reg only: 0 = 32-bit ("w"), 1 = 64-bit ("x"/"sp")
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
};

std::string toLower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<Syllable> exprToSyllables(PccAsmParser::ExprContext *ctx) {
    std::vector<Syllable> result;
    for (auto *syl : ctx->syllable()) {
        char sign = 0;
        if (auto *s = syl->sign()) sign = s->PLUS() ? '+' : '-';
        auto *atom = syl->atom();
        if (atom->expr() != nullptr) {
            Syllable g;
            g.sign = sign;
            g.kind = SylKind::Group;
            g.group = exprToSyllables(atom->expr());
            result.push_back(std::move(g));
        } else if (atom->NUMBER()) {
            result.push_back({sign, SylKind::Number, atom->NUMBER()->getText(), {}});
        } else {
            result.push_back({sign, SylKind::Symbol, atom->IDENT()->getText(), {}});
        }
    }
    if (result.empty()) fail("empty expression");
    return result;
}

// This dialect's own register names: every general register, named
// both ways real A64 assembly names it (`w0`..`w30`, `x0`..`x30`),
// plus `wzr`/`xzr` (the zero register, number 31) and `sp` (the real
// hardware stack pointer, also number 31 - A64 itself distinguishes
// the two purely by which instruction/field it appears in, never by a
// different bit pattern, so this dialect doesn't need to either - see
// Arm64Codegen.h's own header comment). Returns false if `lower`
// isn't one of them.
bool regNumber(const std::string &lower, int &num, int &width) {
    static const std::unordered_map<std::string, std::pair<int, int>> *const names = [] {
        auto *m = new std::unordered_map<std::string, std::pair<int, int>>();
        for (int i = 0; i <= 30; i++) {
            (*m)["w" + std::to_string(i)] = {i, 0};
            (*m)["x" + std::to_string(i)] = {i, 1};
        }
        (*m)["wzr"] = {31, 0};
        (*m)["xzr"] = {31, 1};
        (*m)["sp"] = {31, 1};
        return m;
    }();
    auto it = names->find(lower);
    if (it == names->end()) return false;
    num = it->second.first;
    width = it->second.second;
    return true;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - A64 memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see Arm64Codegen.h)");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        int num = -1, width = 0;
        if (regNumber(toLower(syls[0].text), num, width)) {
            op.kind = Operand::Reg;
            op.reg = num;
            op.width = width;
            return op;
        }
    }
    bool allNumbers = true;
    for (const auto &s : syls) {
        if (s.kind != SylKind::Number) {
            allNumbers = false;
            break;
        }
    }
    op.kind = allNumbers ? Operand::Imm : Operand::Mem;
    op.expr = std::move(syls);
    return op;
}

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) fail("'times' isn't part of this target's dialect");
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dw' operand can't be bracketed");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }

    line.kind = Line::Instr;
    line.mnemonic = mnemonic;
    for (auto *op : instr->operand()) line.operands.push_back(buildOperand(op));
    return line;
}

std::vector<Line> parseProgram(const std::string &text) {
    antlr4::ANTLRInputStream input(text);
    PccAsmLexer lexer(&input);
    ThrowingErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    PccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    PccAsmParser::ProgramContext *prog = parser.program();

    std::vector<Line> lines;
    for (auto *lineCtx : prog->line()) lines.push_back(buildLine(lineCtx));
    return lines;
}

// One pending fixup `assembleArm64Object()` (see Arm64Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - `offset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift), `kind` one of
// kArm64RelocRel26/MovZLo/MovKHi.
struct Relocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {}

    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint8_t> &bytes() const { return bytes_; }
    uint64_t orgBase() const { return orgBase_; }
    const std::unordered_map<std::string, uint64_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        offset_ = 0;
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emit32(static_cast<uint32_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror ../m68k/M68kAssembler.cpp's
    // own `evalExpr` exactly - see Arm64Assembler.h's own header
    // comment on why *any* local address reference, not just an
    // external one, needs a relocation here once `allowExternal_` is
    // set.
    uint64_t evalExpr(const std::vector<Syllable> &syls, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        uint64_t total = 0;
        for (const auto &syl : syls) {
            bool wasExternal = false, wasLocalAddress = false;
            uint64_t v = resolveSyllable(syl, &wasExternal, &wasLocalAddress);
            if (wasExternal && externalName) *externalName = syl.text;
            if (wasLocalAddress && needsBaseShift) *needsBaseShift = true;
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint64_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return std::stoull(t.substr(2), nullptr, 16);
            }
            return std::stoull(t);
        }
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            if (allowExternal_ && wasLocalAddress) *wasLocalAddress = true;
            return it->second;
        }
        if (stage_ == 2) {
            if (allowExternal_) {
                if (wasExternal) *wasExternal = true;
                return 0;
            }
            fail("undefined symbol '" + syl.text + "'");
        }
        return 0;
    }

    // --- word emission -------------------------------------------------
    // Little-endian, unlike MipsAssembler.cpp's own big-endian
    // `emit32` - real AArch64 instruction words (and this backend's
    // own `dw` data words) are always little-endian in memory, no
    // configurable endianness the way classic MIPS has.
    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        offset_ += 4;
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // 12-bit unsigned immediate, as every `add`/`sub` (immediate)
    // operand here needs - plain numbers only (this dialect never
    // needs a label as an add/sub immediate).
    uint32_t evalImm12(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate operand, not a register");
        uint64_t v = evalExpr(op.expr);
        if (v > 0xFFFu) fail("immediate out of 12-bit range");
        return static_cast<uint32_t>(v);
    }

    // `movz`/`movk`'s own 16-bit immediate: a plain number is used
    // exactly as Arm64Codegen.cpp already computed it (that half of a
    // literal it split itself - see IntLit's own codegen, same
    // "already the right half" convention as MipsAssembler.cpp's own
    // `evalHiLo`); a label (`Mem`) instead has its own full resolved
    // address split into its own low (`movz`) or high (`movk`) 16
    // bits automatically here.
    uint32_t evalHiLo(const Operand &op, bool hi, std::string *externalName = nullptr,
                      bool *needsBaseShift = nullptr) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        uint64_t full = evalExpr(op.expr, externalName, needsBaseShift);
        if (op.kind == Operand::Mem) return static_cast<uint32_t>(hi ? (full >> 16) & 0xFFFFu : full & 0xFFFFu);
        return static_cast<uint32_t>(full & 0xFFFFu);
    }

    // --- A64 instruction formats -----------------------------------------
    void emitAddSubImm(int op, int sf, uint8_t rd, uint8_t rn, uint32_t imm12) {
        emit32((static_cast<uint32_t>(sf) << 31) | (static_cast<uint32_t>(op) << 30) | (0u << 29) |
               (0b10001u << 24) | (0u << 22) | (imm12 << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
    void emitAddSubReg(int op, int s, int sf, uint8_t rd, uint8_t rn, uint8_t rm) {
        emit32((static_cast<uint32_t>(sf) << 31) | (static_cast<uint32_t>(op) << 30) |
               (static_cast<uint32_t>(s) << 29) | (0b01011u << 24) | (0u << 22) | (0u << 21) |
               (static_cast<uint32_t>(rm) << 16) | (0u << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
    void emitLogicalReg(uint32_t opc, int n, int sf, uint8_t rd, uint8_t rn, uint8_t rm) {
        emit32((static_cast<uint32_t>(sf) << 31) | (opc << 29) | (0b01010u << 24) | (0u << 22) |
               (static_cast<uint32_t>(n) << 21) | (static_cast<uint32_t>(rm) << 16) | (0u << 10) |
               (static_cast<uint32_t>(rn) << 5) | rd);
    }
    void emitDp2Src(uint32_t opcode, int sf, uint8_t rd, uint8_t rn, uint8_t rm) {
        emit32((static_cast<uint32_t>(sf) << 31) | (0u << 30) | (0u << 29) | (0b11010110u << 21) |
               (static_cast<uint32_t>(rm) << 16) | (opcode << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
    void emitDp3Src(int o0, int sf, uint8_t rd, uint8_t rn, uint8_t rm, uint8_t ra) {
        emit32((static_cast<uint32_t>(sf) << 31) | (0u << 29) | (0b11011u << 24) | (0u << 21) |
               (static_cast<uint32_t>(rm) << 16) | (static_cast<uint32_t>(o0) << 15) |
               (static_cast<uint32_t>(ra) << 10) | (static_cast<uint32_t>(rn) << 5) | rd);
    }
    void emitCsinc(int sf, uint8_t rd, uint8_t rn, uint8_t rm, uint32_t cond) {
        emit32((static_cast<uint32_t>(sf) << 31) | (0u << 30) | (0u << 29) | (0b11010100u << 21) |
               (static_cast<uint32_t>(rm) << 16) | (cond << 12) | (0b01u << 10) | (static_cast<uint32_t>(rn) << 5) |
               rd);
    }
    void emitMovWide(uint32_t opc, int sf, uint8_t rd, uint32_t imm16, uint32_t hw) {
        emit32((static_cast<uint32_t>(sf) << 31) | (opc << 29) | (0b100101u << 23) | (hw << 21) |
               ((imm16 & 0xFFFFu) << 5) | rd);
    }
    // LDR/STR (immediate, unsigned offset), 32-bit transfer, fixed
    // zero displacement - see Arm64Codegen.h's own header comment on
    // why every load/store here is always through a freshly computed
    // address.
    void emitLdStImm32(uint32_t opc, uint8_t rn, uint8_t rt) {
        emit32((0b10u << 30) | (0b111u << 27) | (0u << 26) | (0b01u << 24) | (opc << 22) | (0u << 10) |
               (static_cast<uint32_t>(rn) << 5) | rt);
    }
    // Unconditional branch (immediate) - real A64 has no delay slot
    // at all (unlike MipsAssembler.cpp's own `emitJ`, which has to
    // account for one), so the offset is simply (target - this
    // instruction's own address) / 4.
    void emitBranchImm(uint32_t opc, int64_t relWords) {
        emit32((opc << 26) | (static_cast<uint32_t>(relWords) & 0x03FFFFFFu));
    }
    void emitCbz(int op, int sf, uint8_t rt, int64_t relWords) {
        emit32((static_cast<uint32_t>(sf) << 31) | (0b011010u << 25) | (static_cast<uint32_t>(op) << 24) |
               ((static_cast<uint32_t>(relWords) & 0x7FFFFu) << 5) | rt);
    }

    void encodeAddSub(int op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        int sf = requireOperand(ops, 0, mnemonic).width;
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        const Operand &third = requireOperand(ops, 2, mnemonic);
        if (third.kind == Operand::Reg) {
            emitAddSubReg(op, /*S=*/0, sf, rd, rn, static_cast<uint8_t>(third.reg));
        } else {
            emitAddSubImm(op, sf, rd, rn, evalImm12(third));
        }
    }

    void encodeLogical(uint32_t opc, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        int sf = requireOperand(ops, 0, mnemonic).width;
        uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitLogicalReg(opc, /*N=*/0, sf, rd, rn, rm);
    }

    // The full instruction vocabulary ../Arm64Codegen.cpp ever emits -
    // see Arm64Assembler.h's own header comment. Not a general A64
    // encoder: exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emit32(0xD503201Fu);
            return;
        }
        if (mnemonic == "ret") {
            if (!ops.empty()) fail("'ret' takes no operand");
            emit32(0xD65F03C0u);  // fixed encoding - always returns via x30
            return;
        }
        if (mnemonic == "movz" || mnemonic == "movk") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            const Operand &immOp = requireOperand(ops, 1, mnemonic);
            bool hi = mnemonic == "movk";
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t imm16 = evalHiLo(immOp, hi, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
                uint16_t kind = hi ? kArm64RelocMovKHi : kArm64RelocMovZLo;
                if (needsBaseShift) relocations_.push_back({static_cast<uint32_t>(offset_), "..", kind});
                if (!externalName.empty()) relocations_.push_back({static_cast<uint32_t>(offset_), externalName, kind});
            }
            emitMovWide(mnemonic == "movz" ? 0b10u : 0b11u, sf, rd, imm16, hi ? 1u : 0u);
            return;
        }
        if (mnemonic == "mov") {  // alias: orr rd,<zr matching rd's width>,rn
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitLogicalReg(0b01, /*N=*/0, sf, rd, /*rn=*/31, rn);
            return;
        }
        if (mnemonic == "add") {
            encodeAddSub(0, ops, mnemonic);
            return;
        }
        if (mnemonic == "sub") {
            encodeAddSub(1, ops, mnemonic);
            return;
        }
        if (mnemonic == "neg") {  // alias: sub rd,<zr matching rd's width>,rn
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitAddSubReg(1, /*S=*/0, sf, rd, /*rn=*/31, rn);
            return;
        }
        if (mnemonic == "and") {
            encodeLogical(0b00, ops, mnemonic);
            return;
        }
        if (mnemonic == "orr") {
            encodeLogical(0b01, ops, mnemonic);
            return;
        }
        if (mnemonic == "eor") {
            encodeLogical(0b10, ops, mnemonic);
            return;
        }
        if (mnemonic == "mvn") {  // alias: orn rd,<zr matching rd's width>,rn
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitLogicalReg(0b01, /*N=*/1, sf, rd, /*rn=*/31, rn);
            return;
        }
        if (mnemonic == "mul") {  // alias: madd rd,rn,rm,<zr matching rd's width>
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            emitDp3Src(/*o0=*/0, sf, rd, rn, rm, /*ra=*/31);
            return;
        }
        if (mnemonic == "msub") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            uint8_t ra = requireReg(requireOperand(ops, 3, mnemonic), mnemonic);
            emitDp3Src(/*o0=*/1, sf, rd, rn, rm, ra);
            return;
        }
        if (mnemonic == "sdiv" || mnemonic == "lslv" || mnemonic == "lsrv") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rm = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
            uint32_t opcode = mnemonic == "sdiv" ? 0b000011u : (mnemonic == "lslv" ? 0b001000u : 0b001001u);
            emitDp2Src(opcode, sf, rd, rn, rm);
            return;
        }
        if (mnemonic == "cmp") {
            uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            uint8_t rm = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitAddSubReg(/*op=sub*/ 1, /*S=*/1, sf, /*rd=*/31, rn, rm);
            return;
        }
        static const std::unordered_map<std::string, uint32_t> csets = {
            {"cseteq", 0b0000}, {"csetne", 0b0001}, {"csetge", 0b1010},
            {"csetlt", 0b1011}, {"csetgt", 0b1100}, {"csetle", 0b1101},
        };
        if (auto it = csets.find(mnemonic); it != csets.end()) {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            emitCsinc(sf, rd, /*rn=*/31, /*rm=*/31, it->second ^ 1u);
            return;
        }
        // Dedicated zero-displacement load/store - see Arm64Codegen.h's
        // own header comment: `ldind baseReg,destReg` / `stind
        // baseReg,srcReg`, real LDR/STR underneath with imm12=0.
        if (mnemonic == "ldind") {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitLdStImm32(0b01, base, rt);
            return;
        }
        if (mnemonic == "stind") {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitLdStImm32(0b00, base, rt);
            return;
        }
        if (mnemonic == "b" || mnemonic == "bl") {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            uint32_t opc = mnemonic == "b" ? 0b000101u : 0b100101u;
            std::string externalName;
            uint64_t targetAddr = evalExpr(target.expr, &externalName);
            // A branch target defined in *this* object is already a
            // valid PC-relative offset regardless of where this object
            // ends up linked (both its own address and this
            // instruction's own are shifted by the same base) - no
            // relocation needed at all, unlike an absolute-address
            // load (movz/movk above). Only a genuinely cross-object
            // target (undefined here) needs deferring - see
            // ../pdp11/Pdp11Assembler.cpp's own identical reasoning for
            // its own PC-relative addressing mode.
            if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
                relocations_.push_back({static_cast<uint32_t>(offset_), externalName, kArm64RelocRel26});
                emitBranchImm(opc, 0);
                return;
            }
            int64_t rel = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_)) / 4;
            emitBranchImm(opc, rel);
            return;
        }
        if (mnemonic == "cbz" || mnemonic == "cbnz") {
            uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            int sf = requireOperand(ops, 0, mnemonic).width;
            const Operand &target = requireOperand(ops, 1, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            uint64_t targetAddr = evalExpr(target.expr);
            int64_t rel = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_)) / 4;
            if (stage_ == 2 && (rel < -(1 << 18) || rel >= (1 << 18))) fail("branch target out of 19-bit range");
            emitCbz(mnemonic == "cbnz" ? 1 : 0, sf, rt, rel);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint64_t> labels_;
    std::vector<Relocation> relocations_;
    uint64_t offset_ = 0;
    uint64_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see Arm64Assembler.h's own `assembleArm64Object`
// comment, and ../m68k/M68kAssembler.cpp's own `buildObjectFile` for
// the identical algorithm this is ported from.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kArm64CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, static_cast<uint32_t>(kv.second - a.orgBase()), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.offset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleArm64(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleArm64Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
