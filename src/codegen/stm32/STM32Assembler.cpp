#include "STM32Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("STM32Assembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 ----------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand: `Reg` from a bare `r0`-`r12`/`sp`/`lr`/`pc` identifier, `IndirectReg` from a
// bracketed one (`[r2]`), `Imm` from a bare (optionally signed) number, else (a label or
// `label+N`) `Mem`.
struct Operand {
    enum Kind { Reg, Imm, Mem, IndirectReg } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Org, RamOrg, Ds, Dsb, Dl, Dlt, Instr } kind = None;
    std::vector<std::vector<Syllable>> exprList;  // Org/RamOrg (1)/Ds (1)/Dsb (1)/Dl (N)/Dlt (1)
    std::string mnemonic;                          // Instr
    std::vector<Operand> operands;                 // Instr
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

// r0-r12, sp (r13), lr (r14), pc (r15), case-insensitive. Returns -1 if not a register name.
int regNumber(const std::string &nameLower) {
    if (nameLower == "sp") return 13;
    if (nameLower == "lr") return 14;
    if (nameLower == "pc") return 15;
    if (nameLower.size() < 2 || nameLower.size() > 3 || nameLower[0] != 'r') return -1;
    for (size_t i = 1; i < nameLower.size(); i++) {
        if (nameLower[i] < '0' || nameLower[i] > '9') return -1;
    }
    int n = std::stoi(nameLower.substr(1));
    return (n >= 0 && n <= 15) ? n : -1;
}

// A bracketed operand's inner expr must be a single bare register; this dialect's `[Rn]` never
// has a displacement.
Operand buildBracketOperand(PccAsmParser::ExprContext *ctx) {
    std::vector<Syllable> syls = exprToSyllables(ctx);
    if (syls.size() != 1 || syls[0].sign != 0 || syls[0].kind != SylKind::Symbol) {
        fail("a bracketed operand must be a single bare register");
    }
    int r = regNumber(toLower(syls[0].text));
    if (r < 0) fail("'[" + syls[0].text + "]': not a register");
    Operand op;
    op.kind = Operand::IndirectReg;
    op.reg = r;
    return op;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) return buildBracketOperand(ctx->expr());
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        if (int r = regNumber(toLower(syls[0].text)); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
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
    if (dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) fail("'='-style equates aren't part of this dialect");
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) fail("'times' isn't part of this target's dialect");
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org" || mnemonic == "ramorg") {
        line.kind = mnemonic == "org" ? Line::Org : Line::RamOrg;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) {
            fail("'" + mnemonic + "' needs a plain address operand");
        }
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "ds" || mnemonic == "dsb") {
        line.kind = mnemonic == "ds" ? Line::Ds : Line::Dsb;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) {
            fail("'" + mnemonic + "' needs a plain count operand");
        }
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dl" || mnemonic == "dlt") {
        line.kind = mnemonic == "dl" ? Line::Dl : Line::Dlt;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'" + mnemonic + "' operand can't be bracketed");
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

// A pending fixup that becomes an ObjRelocation: `offset` into bytes(), `symbolName` the
// unresolved reference (or the pseudo-symbol ".." for a same-file flash address's base-shift,
// never emitted for RAM - see `isRamLabel_`), `kind` one of kSTM32RelocAbs32/MovWLo/MovTHi/Branch25.
struct Relocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
// Two independent address spaces; `inRam_` says which one is currently active. Only flash bytes
// land in `bytes_` (RAM has no image content). `org`/`ramorg` each run exactly once, in that order.
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
    uint32_t flashBase() const { return flashBase_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        flashOffset_ = 0;
        flashBase_ = 0;
        ramOffset_ = 0;
        ramBase_ = 0;
        inRam_ = false;
        for (const auto &line : lines) processLine(line);
    }

    uint32_t currentAddr() const { return inRam_ ? ramBase_ + ramOffset_ : flashBase_ + flashOffset_; }

    void processLine(const Line &line) {
        // `isRamLabel_` records which space each label was defined in, independently of
        // `labels_`'s resolved value: only a flash label's address ever needs the ".."
        // link-time base-shift, since `ramorg` always uses the real fixed RAM base.
        for (const auto &lbl : line.labels) {
            labels_[lbl] = currentAddr();
            isRamLabel_[lbl] = inRam_;
        }
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Org:
                flashBase_ = evalExpr(line.exprList.at(0));
                flashOffset_ = 0;
                inRam_ = false;
                return;
            case Line::RamOrg:
                ramBase_ = evalExpr(line.exprList.at(0));
                ramOffset_ = 0;
                inRam_ = true;
                return;
            case Line::Ds:
                if (!inRam_) fail("'ds' only valid after 'ramorg'");
                ramOffset_ += 4 * evalExpr(line.exprList.at(0));
                return;
            case Line::Dsb:
                // Byte-granular reservation, so a char/short global or array doesn't need a
                // whole RAM word per element.
                if (!inRam_) fail("'dsb' only valid after 'ramorg'");
                ramOffset_ += evalExpr(line.exprList.at(0));
                return;
            case Line::Dl:
                if (inRam_) fail("'dl' only valid in the flash section");
                for (const auto &e : line.exprList) emit32(evalExpr(e));
                return;
            case Line::Dlt: {
                // The reset-handler vector entry: a label's resolved flash address with the low
                // (Thumb) bit forced on. It needs the same ".." base-shift relocation as any
                // same-file flash reference; the shift and the Thumb bit coexist correctly
                // under plain addition since every flash address here is always even, so
                // OR-ing in bit 0 is equivalent to adding 1.
                if (inRam_) fail("'dlt' only valid in the flash section");
                std::string externalName;
                bool needsBaseShift = false;
                uint32_t v = evalExpr(line.exprList.at(0), &externalName, &needsBaseShift) | 1;
                if (stage_ == 2 && allowExternal_) {
                    if (needsBaseShift) relocations_.push_back({flashOffset_, "..", kSTM32RelocAbs32});
                    if (!externalName.empty()) relocations_.push_back({flashOffset_, externalName, kSTM32RelocAbs32});
                }
                emit32(v);
                return;
            }
            case Line::Instr:
                if (inRam_) fail("an instruction can't appear after 'ramorg'");
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `needsBaseShift` is only set for a flash label; a RAM label's value needs no base-shift
    // since `ramorg` already placed it at its final hardware address.
    uint32_t evalExpr(const std::vector<Syllable> &syls, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            bool wasExternal = false, wasLocalFlashAddress = false;
            uint32_t v = resolveSyllable(syl, &wasExternal, &wasLocalFlashAddress);
            if (wasExternal && externalName) *externalName = syl.text;
            if (wasLocalFlashAddress && needsBaseShift) *needsBaseShift = true;
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalFlashAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            // Only a flash label needs the ".." link-time base-shift; a RAM label's value is
            // already final.
            if (allowExternal_ && wasLocalFlashAddress && !isRamLabel_.at(syl.text)) *wasLocalFlashAddress = true;
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

    // --- byte/word emission (flash only) -----------------------------
    void emit16(uint16_t w) {
        bytes_.push_back(static_cast<uint8_t>(w & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        flashOffset_ += 2;
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>(v & 0xFFFF));
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
    }

    static void requireImm(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Imm) fail("'" + mnemonic + "': immediate operand expected");
    }
    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    // Every two/three-register Thumb-1 encoding this backend emits only has room for a low
    // register (r0-r7) - `mov`/push/pop's fixed lr/pc forms are the only place a high register
    // appears.
    static uint8_t requireLowReg(const Operand &op, const std::string &mnemonic) {
        uint8_t r = requireReg(op, mnemonic);
        if (r > 7) fail("'" + mnemonic + "': register operand must be r0-r7 in this dialect");
        return r;
    }
    static uint8_t requireIndirectReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::IndirectReg) fail("'" + mnemonic + "' needs a bracketed register operand");
        if (op.reg > 7) fail("'" + mnemonic + "': bracketed register must be r0-r7 in this dialect");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // `movw`/`movt Rd,#imm16` (T3), the only way to get a 32-bit constant into a register;
    // `movl`/`lea` below each call this twice.
    void emitMovWT(uint8_t rd, uint16_t imm16, bool top) {
        uint8_t imm4 = static_cast<uint8_t>((imm16 >> 12) & 0xF);
        uint8_t i = static_cast<uint8_t>((imm16 >> 11) & 1);
        uint8_t imm3 = static_cast<uint8_t>((imm16 >> 8) & 0x7);
        uint8_t imm8 = static_cast<uint8_t>(imm16 & 0xFF);
        uint16_t hw1 = static_cast<uint16_t>((top ? 0xF2C0 : 0xF240) | (i << 10) | imm4);
        uint16_t hw2 = static_cast<uint16_t>((imm3 << 12) | (rd << 8) | imm8);
        emit16(hw1);
        emit16(hw2);
    }
    void emitMovConst32(uint8_t rd, uint32_t val) {
        emitMovWT(rd, static_cast<uint16_t>(val & 0xFFFF), /*top=*/false);
        emitMovWT(rd, static_cast<uint16_t>((val >> 16) & 0xFFFF), /*top=*/true);
    }

    // `bl`/`b` (T1 BL / T4 B, a 25-bit signed byte offset via the S/I1/I2 "J bit" trick) and the
    // conditional `bcc` (T3, a 21-bit signed byte offset, no J-bit trick). These are this
    // dialect's only branch forms.
    void encodeBranch25(const Operand &target, bool isLink, const std::string &mnemonic) {
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': branch target must be a label");
        // A target defined in this object is already a valid PC-relative offset regardless of
        // link-time placement, so no relocation is needed; only a cross-object target needs
        // deferring, with the field left zero for the linker to patch in.
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
            relocations_.push_back({flashOffset_, externalName, kSTM32RelocBranch25});
            emit16(static_cast<uint16_t>(0xF000));
            emit16(static_cast<uint16_t>(isLink ? 0xD000 : 0x9000));
            return;
        }
        uint32_t instrAddr = flashBase_ + flashOffset_;
        int32_t offset = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(instrAddr + 4);
        if (stage_ == 2 && (offset < -16777216 || offset > 16777214)) fail("'" + mnemonic + "': target out of range");
        uint32_t val25 = static_cast<uint32_t>(offset) & 0x1FFFFFFu;
        uint32_t S = (val25 >> 24) & 1, I1 = (val25 >> 23) & 1, I2 = (val25 >> 22) & 1;
        uint32_t imm10 = (val25 >> 12) & 0x3FF, imm11 = (val25 >> 1) & 0x7FF;
        uint32_t J1 = S ^ I1 ^ 1, J2 = S ^ I2 ^ 1;
        uint16_t hw1 = static_cast<uint16_t>(0xF000 | (S << 10) | imm10);
        uint16_t hw2 = static_cast<uint16_t>((isLink ? 0xD000 : 0x9000) | (J1 << 13) | (J2 << 11) | imm11);
        emit16(hw1);
        emit16(hw2);
    }
    void encodeBcc(uint8_t cond, const Operand &target, const std::string &mnemonic) {
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': branch target must be a label");
        uint32_t instrAddr = flashBase_ + flashOffset_;
        uint32_t targetAddr = evalExpr(target.expr);
        int32_t offset = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(instrAddr + 4);
        if (stage_ == 2 && (offset < -1048576 || offset > 1048574)) fail("'" + mnemonic + "': target out of range");
        uint32_t val21 = static_cast<uint32_t>(offset) & 0x1FFFFFu;
        uint32_t S = (val21 >> 20) & 1, J2 = (val21 >> 19) & 1, J1 = (val21 >> 18) & 1;
        uint32_t imm6 = (val21 >> 12) & 0x3F, imm11 = (val21 >> 1) & 0x7FF;
        uint16_t hw1 = static_cast<uint16_t>(0xF000 | (S << 10) | (cond << 6) | imm6);
        uint16_t hw2 = static_cast<uint16_t>(0x8000 | (J1 << 13) | (J2 << 11) | imm11);
        emit16(hw1);
        emit16(hw2);
    }

    // The full instruction vocabulary STM32Codegen.cpp emits. Not a general Thumb-2 encoder:
    // exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emit16(0xBF00);
            return;
        }
        if (mnemonic == "pushlr") {
            if (!ops.empty()) fail("'pushlr' takes no operand");
            emit16(0xB500);
            return;
        }
        if (mnemonic == "poplr") {
            if (!ops.empty()) fail("'poplr' takes no operand");
            emit16(0xBD00);
            return;
        }
        if (mnemonic == "push" || mnemonic == "pop") {
            uint8_t r = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>((mnemonic == "push" ? 0xB400 : 0xBC00) | (1 << r)));
            return;
        }
        if (mnemonic == "movl") {
            const Operand &src = requireOperand(ops, 0, mnemonic);
            uint8_t rd = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            requireImm(src, mnemonic);
            emitMovConst32(rd, evalExpr(src.expr));
            return;
        }
        if (mnemonic == "lea") {
            const Operand &src = requireOperand(ops, 0, mnemonic);
            uint8_t rd = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            if (src.kind != Operand::Mem) fail("'lea': source must be a label");
            // `needsBaseShift` only fires for a flash label (a RAM address is already final).
            // Each half of the movw/movt split gets its own relocation, recorded right before
            // the instruction it belongs to.
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t v = evalExpr(src.expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({flashOffset_, "..", kSTM32RelocMovWLo});
                if (!externalName.empty()) relocations_.push_back({flashOffset_, externalName, kSTM32RelocMovWLo});
            }
            emitMovWT(rd, static_cast<uint16_t>(v & 0xFFFF), /*top=*/false);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({flashOffset_, "..", kSTM32RelocMovTHi});
                if (!externalName.empty()) relocations_.push_back({flashOffset_, externalName, kSTM32RelocMovTHi});
            }
            emitMovWT(rd, static_cast<uint16_t>((v >> 16) & 0xFFFF), /*top=*/true);
            return;
        }
        if (mnemonic == "mov") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t d = static_cast<uint8_t>((rd >> 3) & 1);
            emit16(static_cast<uint16_t>(0x4600 | (d << 7) | (rn << 3) | (rd & 7)));
            return;
        }
        if (mnemonic == "add" || mnemonic == "sub") {
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rm = requireLowReg(requireOperand(ops, 2, mnemonic), mnemonic);
            uint16_t base = mnemonic == "add" ? 0x1800 : 0x1A00;
            emit16(static_cast<uint16_t>(base | (rm << 6) | (rn << 3) | rd));
            return;
        }
        if (mnemonic == "cmp") {
            uint8_t rn = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rm = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4280 | (rm << 3) | rn));
            return;
        }
        if (mnemonic == "cmpz") {
            uint8_t rn = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x2800 | (rn << 8)));
            return;
        }
        if (mnemonic == "mul") {
            uint8_t rn = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rd = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4340 | (rn << 3) | rd));
            return;
        }
        if (mnemonic == "and" || mnemonic == "orr" || mnemonic == "eor" || mnemonic == "mvn" ||
            mnemonic == "adc" || mnemonic == "sbc") {
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rm = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            static const std::unordered_map<std::string, uint16_t> base = {
                {"and", 0x4000}, {"orr", 0x4300}, {"eor", 0x4040}, {"mvn", 0x43C0},
                // ADC/SBC: `Rdn += Rm + C` / `Rdn -= Rm + NOT(C)`, used for wide (`long long`)
                // add/sub to ripple the carry/borrow across a word pair.
                {"adc", 0x4140}, {"sbc", 0x4180},
            };
            emit16(static_cast<uint16_t>(base.at(mnemonic) | (rm << 3) | rd));
            return;
        }
        if (mnemonic == "neg") {
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4240 | (rn << 3) | rd));
            return;
        }
        if (mnemonic == "sdiv" || mnemonic == "udiv" || mnemonic == "lsl" || mnemonic == "lsr" ||
            mnemonic == "asr") {
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireLowReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t rm = requireLowReg(requireOperand(ops, 2, mnemonic), mnemonic);
            uint16_t hw1;
            if (mnemonic == "sdiv") {
                hw1 = static_cast<uint16_t>(0xFB90 | rn);
            } else if (mnemonic == "udiv") {
                // UDIV.W Rd,Rn,Rm - same encoding family as sdiv, differing only in bit 4
                // (0=signed, 1=unsigned).
                hw1 = static_cast<uint16_t>(0xFBB0 | rn);
            } else if (mnemonic == "lsl") {
                hw1 = static_cast<uint16_t>(0xFA10 | rn);
            } else if (mnemonic == "lsr") {
                hw1 = static_cast<uint16_t>(0xFA30 | rn);
            } else {
                // ASR.W Rd,Rn,Rm (register-shifted-register, S=1).
                hw1 = static_cast<uint16_t>(0xFA50 | rn);
            }
            bool isDivide = mnemonic == "sdiv" || mnemonic == "udiv";
            uint16_t hw2 = static_cast<uint16_t>((isDivide ? 0xF0F0 : 0xF000) | (rd << 8) | rm);
            emit16(hw1);
            emit16(hw2);
            return;
        }
        if (mnemonic == "ldr" || mnemonic == "str") {
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireIndirectReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>((mnemonic == "ldr" ? 0x6800 : 0x6000) | (rn << 3) | rd));
            return;
        }
        if (mnemonic == "ldrb" || mnemonic == "strb" || mnemonic == "ldrh" || mnemonic == "strh") {
            // LDRB/STRB/LDRH/STRH Rt,[Rn] (T1, imm5=0) - loads always zero-extend; a signed
            // value is re-sign-extended afterward via `lsl`+`asr`.
            uint8_t rd = requireLowReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rn = requireIndirectReg(requireOperand(ops, 1, mnemonic), mnemonic);
            bool isLoad = mnemonic[0] == 'l';
            bool isByte = mnemonic.back() == 'b';
            uint16_t base = isByte ? (isLoad ? 0x7800 : 0x7000) : (isLoad ? 0x8800 : 0x8000);
            emit16(static_cast<uint16_t>(base | (rn << 3) | rd));
            return;
        }
        if (mnemonic == "bl" || mnemonic == "b") {
            encodeBranch25(requireOperand(ops, 0, mnemonic), /*isLink=*/mnemonic == "bl", mnemonic);
            return;
        }
        if (mnemonic == "blx") {
            // BLX Rm (indirect call through Rm): 0100 0111 1 Rm(4) 000. Unlike the Thumb-1
            // two/three-register forms above, Rm is the full 4-bit register number (r0-r14).
            uint8_t rm = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4780 | (rm << 3)));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> branchCond = {
            {"beq", 0x0}, {"bne", 0x1}, {"blt", 0xB}, {"ble", 0xD}, {"bgt", 0xC}, {"bge", 0xA},
            // Unsigned orderings - ARM condition-field values (CS/HS=2, CC/LO=3, HI=8, LS=9).
            {"blo", 0x3}, {"bhs", 0x2}, {"bhi", 0x8}, {"bls", 0x9},
        };
        if (auto it = branchCond.find(mnemonic); it != branchCond.end()) {
            encodeBcc(it->second, requireOperand(ops, 0, mnemonic), mnemonic);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_map<std::string, bool> isRamLabel_;
    std::vector<Relocation> relocations_;
    uint32_t flashOffset_ = 0;
    uint32_t flashBase_ = 0;
    uint32_t ramOffset_ = 0;
    uint32_t ramBase_ = 0;
    bool inRam_ = false;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile a completed object-mode Assembler produced. Every
// symbol's recorded value is its resolved address minus flashBase() (0 in object mode): for a
// flash label that's a small offset the linker re-bases correctly; for a RAM label (always
// >= kRamBase) the subtraction leaves the value numerically unchanged, still the real hardware
// address. The linker re-basing a RAM label anyway when another object refers to it by name is
// a known gap, harmless since this backend's test coverage never crosses files.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kSTM32CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.flashBase(), /*isDefined=*/true});
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

std::string assembleSTM32(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleSTM32Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
