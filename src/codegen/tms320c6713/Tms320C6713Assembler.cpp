#include "Tms320C6713Assembler.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "RccAsmLexer.h"
#include "RccAsmParser.h"

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Tms320C6713Assembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 (same shape as AlphaAssembler.cpp's own) ------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// A bare register is Reg; a parenthesized bare register (`(Bn)`, this dialect's spelling of
// BR's target-address operand) is RegDeferred; a bare signed number is Imm; anything else (a
// label, or `label+N`) is Mem, resolved as an address.
struct Operand {
    enum Kind { Reg, RegDeferred, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dh, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;
    std::string mnemonic;
    std::vector<Operand> operands;
};

std::string toLower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<Syllable> exprToSyllables(RccAsmParser::ExprContext *ctx) {
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

// Matches B0..B15 only (see Tms320C6713Codegen.h's own header comment: A0..A15 are never used
// by this backend, so this dialect's assembler doesn't even recognize them).
int regNumber(const std::string &lower) {
    if (lower.size() < 2 || lower.size() > 3) return -1;
    if (lower[0] != 'b') return -1;
    for (size_t i = 1; i < lower.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(lower[i]))) return -1;
    }
    int n = std::stoi(lower.substr(1));
    return (n >= 0 && n <= 15) ? n : -1;
}

int bareReg(const Syllable &s) {
    if (s.sign != 0 || s.kind != SylKind::Symbol) return -1;
    return regNumber(toLower(s.text));
}

int groupReg(const Syllable &s) {
    if (s.kind != SylKind::Group || s.group.size() != 1) return -1;
    return bareReg(s.group[0]);
}

bool isNumeric(const std::vector<Syllable> &syls) {
    for (const auto &s : syls) {
        if (s.kind == SylKind::Symbol) return false;
        if (s.kind == SylKind::Group && !isNumeric(s.group)) return false;
    }
    return true;
}

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail("bracketed memory operands aren't part of this target's dialect - every load/store "
             "goes through the dedicated 'LDx'/'STx' mnemonics instead (see Tms320C6713Assembler.h)");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1) {
        if (int r = bareReg(syls[0]); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
        if (int r = groupReg(syls[0]); r >= 0 && syls[0].sign == 0) {
            op.kind = Operand::RegDeferred;
            op.reg = r;
            return op;
        }
    }
    op.kind = isNumeric(syls) ? Operand::Imm : Operand::Mem;
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

Line buildLine(RccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (auto *assign = dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    if (dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) fail("'times' isn't part of this target's dialect");
    auto *instr = dynamic_cast<RccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dw" || mnemonic == "db" || mnemonic == "dh") {
        line.kind = mnemonic == "dw" ? Line::Dw : mnemonic == "dh" ? Line::Dh : Line::Db;
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
    RccAsmLexer lexer(&input);
    ThrowingErrorListener errors;
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    RccAsmParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    RccAsmParser::ProgramContext *prog = parser.program();

    std::vector<Line> lines;
    for (auto *lineCtx : prog->line()) lines.push_back(buildLine(lineCtx));
    return lines;
}

// --- two-pass assembly (raw, whole-program only - no object/relocation support; see
// Tms320C6713Codegen.h's own header comment on why this target doesn't implement real separate
// compilation) ---------------------------------------------------------------------------------
class Assembler {
   public:
    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint8_t> &bytes() const { return bytes_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
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
                for (const auto &e : line.exprList) emit32(evalExpr(e));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Dh:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    uint32_t evalExpr(const std::vector<Syllable> &syls) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            uint32_t v = resolveSyllable(syl);
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveSyllable(const Syllable &syl) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) return it->second;
        if (stage_ == 2) fail("undefined symbol '" + syl.text + "'");
        return 0;
    }

    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }
    void emit16(uint16_t v) {
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>(v & 0xFFFF));
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static uint8_t requireRegDeferred(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::RegDeferred) fail("'" + mnemonic + "' needs a '(Bn)' operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // Common fields shared by every format: p=0, s=1, x (only meaningful for formats that have
    // a cross-path bit), z/creg (the predicate - see Tms320C6713Codegen.h's own header
    // comment). This backend only ever uses creg=0 or creg=1.
    static uint32_t common(uint32_t z, uint32_t creg) { return (1u << 1) | (z << 28) | (creg << 29); }

    void encodeAluL(uint8_t op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t ra = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rb = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32(0x18u | common(0, 0) | (static_cast<uint32_t>(op) << 5) | (static_cast<uint32_t>(ra) << 13) |
               (static_cast<uint32_t>(rb) << 18) | (static_cast<uint32_t>(rd) << 23));
    }

    // `SHL/SHR/SHRU dst,value,shiftReg`: `value` maps to the hardware's src2 field, `shiftReg`
    // to src1 - see Tms320C6713Codegen.h's own header comment on this encoding's src1/src2
    // field assignment.
    void encodeAluS(uint8_t op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t value = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t shiftReg = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32(0x20u | common(0, 0) | (static_cast<uint32_t>(op) << 6) | (static_cast<uint32_t>(shiftReg) << 13) |
               (static_cast<uint32_t>(value) << 18) | (static_cast<uint32_t>(rd) << 23));
    }

    void encodeMpy(uint8_t op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t ra = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rb = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emit32(0x0u | common(0, 0) | (static_cast<uint32_t>(op) << 7) | (static_cast<uint32_t>(ra) << 13) |
               (static_cast<uint32_t>(rb) << 18) | (static_cast<uint32_t>(rd) << 23));
    }

    // Tms320C6713Codegen.cpp always passes the already-selected 16-bit field value directly
    // (see its own loadImm32()) *except* when the operand is a label (loadAddress()), where
    // the label's full resolved address stands in for the whole 32-bit value and this
    // encoder itself must pick out the requested half - so only the Mem case shifts.
    void encodeMvkFamily(bool high, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &imm = requireOperand(ops, 1, mnemonic);
        uint32_t cst16;
        if (imm.kind == Operand::Mem) {
            uint32_t full = evalExpr(imm.expr);
            cst16 = high ? ((full >> 16) & 0xFFFFu) : (full & 0xFFFFu);
        } else if (imm.kind == Operand::Imm) {
            cst16 = evalExpr(imm.expr) & 0xFFFFu;
        } else {
            fail("'" + mnemonic + "': second operand must be a number or a label");
            return;
        }
        emit32(0x28u | common(0, 0) | ((high ? 1u : 0u) << 6) | (cst16 << 7) | (static_cast<uint32_t>(rd) << 23));
    }

    // `LDx base,dst` / `STx base,src`: always mode=1, offsetR=0 (see Tms320C6713Codegen.h's
    // own header comment on why this backend always materializes a full address first, never
    // a nonzero offsetR).
    void encodeMemAccess(uint8_t op, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t reg = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint32_t word = 0x4u | common(0, 0) | (1u << 7) /* y=1 */ | (static_cast<uint32_t>(op) << 4) |
                         (1u << 9) /* mode=1 */ | (static_cast<uint32_t>(base) << 18) |
                         (static_cast<uint32_t>(reg) << 23);
        emit32(word);
    }

    void encodeBranchReg(const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rb = requireRegDeferred(requireOperand(ops, 0, mnemonic), mnemonic);
        emit32(0x360u | common(0, 0) | (static_cast<uint32_t>(rb) << 18));
    }

    void encodeBranchConst(uint32_t z, uint32_t creg, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = requireOperand(ops, 0, mnemonic);
        if (target.kind != Operand::Mem && target.kind != Operand::Imm) {
            fail("'" + mnemonic + "': branch target must be a label");
        }
        uint32_t targetAddr = evalExpr(target.expr);
        int64_t rel = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_)) / 4;
        if (stage_ == 2 && (rel < -(1 << 20) || rel > (1 << 20) - 1)) {
            fail("'" + mnemonic + "': branch target out of 21-bit range");
        }
        emit32(0x10u | common(z, creg) | ((static_cast<uint32_t>(rel) & 0x1FFFFFu) << 7));
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "nop") {
            emit32(0);
            return;
        }
        if (mnemonic == "mvk") {
            encodeMvkFamily(false, ops, mnemonic);
            return;
        }
        if (mnemonic == "mvkh") {
            encodeMvkFamily(true, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluL = {
            {"add", 0x03}, {"sub", 0x07},  {"and", 0x7b},  {"or", 0x7f},    {"xor", 0x6f},
            {"cmpeq", 0x53}, {"cmpgt", 0x47}, {"cmpgtu", 0x4f}, {"cmplt", 0x57}, {"cmpltu", 0x5f},
        };
        if (auto it = aluL.find(mnemonic); it != aluL.end()) {
            encodeAluL(it->second, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluS = {
            {"shl", 0x33}, {"shr", 0x37}, {"shru", 0x27},
        };
        if (auto it = aluS.find(mnemonic); it != aluS.end()) {
            encodeAluS(it->second, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> mpy = {
            {"mpy", 0x19}, {"mpyh", 0x01}, {"mpyhl", 0x09}, {"mpylh", 0x11},
        };
        if (auto it = mpy.find(mnemonic); it != mpy.end()) {
            encodeMpy(it->second, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> mem = {
            {"ldb", 2}, {"ldh", 4}, {"ldw", 6}, {"stb", 3}, {"sth", 5}, {"stw", 7},
        };
        if (auto it = mem.find(mnemonic); it != mem.end()) {
            encodeMemAccess(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "br") {
            encodeBranchReg(ops, mnemonic);
            return;
        }
        if (mnemonic == "b") {
            encodeBranchConst(0, 0, ops, mnemonic);
            return;
        }
        if (mnemonic == "bz") {
            encodeBranchConst(1, 1, ops, mnemonic);
            return;
        }
        if (mnemonic == "bnz") {
            encodeBranchConst(0, 1, ops, mnemonic);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
};

}  // namespace

std::string assembleTms320C6713(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler;
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace rcc
