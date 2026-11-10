#include "VaxAssembler.h"

#include <algorithm>
#include <cctype>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("VaxAssembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from the syllables of a bare expression. See VaxAssembler.h for
// the spelling of each addressing mode. `expr` is the displacement (Disp), the value
// (Imm/Abs) or the target (Label).
struct Operand {
    enum Kind { Reg, RegDeferred, AutoDec, AutoInc, Disp, Imm, Abs, Label } kind = Label;
    int reg = -1;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dh, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Db/Dh/Dw (N)
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

// r0-r11 are general registers; the last four are AP, FP, SP and PC. Returns -1 if `name`
// isn't a register.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"r0", 0}, {"r1", 1}, {"r2", 2},  {"r3", 3},  {"r4", 4},   {"r5", 5},   {"r6", 6},   {"r7", 7},
        {"r8", 8}, {"r9", 9}, {"r10", 10}, {"r11", 11}, {"r12", 12}, {"r13", 13}, {"r14", 14}, {"r15", 15},
        {"ap", 12}, {"fp", 13}, {"sp", 14}, {"pc", 15},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

// A syllable that is exactly one unsigned register name.
int bareReg(const Syllable &s) {
    if (s.sign != 0 || s.kind != SylKind::Symbol) return -1;
    return regNumber(toLower(s.text));
}

// A parenthesized register, `(rN)`, ignoring the group's own sign.
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

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail("bracketed operands aren't part of this target's dialect (see VaxAssembler.h)");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;

    if (syls.size() == 1) {
        if (int r = bareReg(syls[0]); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
        if (int r = groupReg(syls[0]); r >= 0 && syls[0].sign != '+') {
            op.kind = syls[0].sign == '-' ? Operand::AutoDec : Operand::RegDeferred;
            op.reg = r;
            return op;
        }
    }
    if (syls.size() == 2 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol && toLower(syls[0].text) == "ai") {
        if (int r = groupReg(syls[1]); r >= 0 && syls[1].sign == 0) {
            op.kind = Operand::AutoInc;
            op.reg = r;
            return op;
        }
    }
    if (syls.size() >= 2 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        std::string head = toLower(syls[0].text);
        if (head == "imm" || head == "abs") {
            op.kind = head == "imm" ? Operand::Imm : Operand::Abs;
            op.expr.assign(syls.begin() + 1, syls.end());
            return op;
        }
    }
    if (syls.size() >= 2) {
        const Syllable &last = syls.back();
        if (int r = groupReg(last); r >= 0 && last.sign == 0) {
            std::vector<Syllable> disp(syls.begin(), syls.end() - 1);
            if (isNumeric(disp)) {
                op.kind = Operand::Disp;
                op.reg = r;
                op.expr = std::move(disp);
                return op;
            }
        }
    }
    op.kind = Operand::Label;
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
    if (mnemonic == "dw" || mnemonic == "db" || mnemonic == "dh") {
        line.kind = mnemonic == "db" ? Line::Db : mnemonic == "dh" ? Line::Dh : Line::Dw;
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

// One pending fixup, turned into an ObjRelocation once assembly is done - `offset` into
// `bytes()`, `symbolName` the unresolved reference (or the pseudo-symbol ".." for a
// same-file address's base shift).
struct Relocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t kind;
};

// One operand slot of an instruction: its access type (r read, w write, m modify, a
// address, v bit-field base, b branch displacement) and data size (b/w/l/q bytes 1/2/4/8;
// for a branch, the displacement's size).
struct OpSpec {
    char access;
    char size;
};

struct InstrDef {
    uint8_t opcode;
    std::vector<OpSpec> ops;
};

const std::unordered_map<std::string, InstrDef> &instructionTable() {
    static const std::unordered_map<std::string, InstrDef> table = [] {
        struct Row {
            const char *name;
            uint8_t opcode;
            const char *specs;  // "rl wl": one two-letter access+size pair per operand
        };
        static const Row rows[] = {
            {"halt", 0x00, ""},        {"nop", 0x01, ""},         {"ret", 0x04, ""},
            {"brb", 0x11, "bb"},       {"bneq", 0x12, "bb"},      {"beql", 0x13, "bb"},
            {"bgtr", 0x14, "bb"},      {"bleq", 0x15, "bb"},      {"jsb", 0x16, "ab"},
            {"jmp", 0x17, "ab"},       {"bgeq", 0x18, "bb"},      {"blss", 0x19, "bb"},
            {"bgtru", 0x1A, "bb"},     {"blequ", 0x1B, "bb"},     {"bgequ", 0x1E, "bb"},
            {"blssu", 0x1F, "bb"},     {"brw", 0x31, "bw"},       {"cvtwl", 0x32, "rw wl"},
            {"movzwl", 0x3C, "rw wl"}, {"movaw", 0x3E, "aw wl"},  {"ashl", 0x78, "rb rl wl"},
            {"ashq", 0x79, "rb rq wq"}, {"emul", 0x7A, "rl rl rl wq"}, {"ediv", 0x7B, "rl rq wl wl"},
            {"clrq", 0x7C, "wq"},      {"movq", 0x7D, "rq wq"},   {"movaq", 0x7E, "aq wl"},
            {"movb", 0x90, "rb wb"},   {"cmpb", 0x91, "rb rb"},   {"clrb", 0x94, "wb"},
            {"cvtbl", 0x98, "rb wl"},  {"movzbl", 0x9A, "rb wl"}, {"movab", 0x9E, "ab wl"},
            {"movw", 0xB0, "rw ww"},   {"cmpw", 0xB1, "rw rw"},   {"clrw", 0xB4, "ww"},
            {"addl2", 0xC0, "rl ml"},  {"addl3", 0xC1, "rl rl wl"}, {"subl2", 0xC2, "rl ml"},
            {"subl3", 0xC3, "rl rl wl"}, {"mull2", 0xC4, "rl ml"}, {"mull3", 0xC5, "rl rl wl"},
            {"divl2", 0xC6, "rl ml"},  {"divl3", 0xC7, "rl rl wl"}, {"bisl2", 0xC8, "rl ml"},
            {"bisl3", 0xC9, "rl rl wl"}, {"bicl2", 0xCA, "rl ml"}, {"bicl3", 0xCB, "rl rl wl"},
            {"xorl2", 0xCC, "rl ml"},  {"xorl3", 0xCD, "rl rl wl"}, {"mnegl", 0xCE, "rl wl"},
            {"movl", 0xD0, "rl wl"},   {"cmpl", 0xD1, "rl rl"},   {"mcoml", 0xD2, "rl wl"},
            {"bitl", 0xD3, "rl rl"},   {"clrl", 0xD4, "wl"},      {"tstl", 0xD5, "rl"},
            {"incl", 0xD6, "ml"},      {"decl", 0xD7, "ml"},      {"adwc", 0xD8, "rl ml"},
            {"sbwc", 0xD9, "rl ml"},   {"pushl", 0xDD, "rl"},     {"moval", 0xDE, "al wl"},
            {"pushal", 0xDF, "al"},    {"extv", 0xEE, "rl rb vb wl"}, {"extzv", 0xEF, "rl rb vb wl"},
            {"cvtlb", 0xF6, "rl wb"},  {"cvtlw", 0xF7, "rl ww"},  {"calls", 0xFB, "rl ab"},
        };
        std::unordered_map<std::string, InstrDef> t;
        for (const Row &r : rows) {
            InstrDef def;
            def.opcode = r.opcode;
            std::string s = r.specs;
            for (size_t i = 0; i + 1 < s.size(); i += 3) def.ops.push_back({s[i], s[i + 1]});
            t[r.name] = std::move(def);
        }
        return t;
    }();
    return table;
}

// A `j<cc>` pseudo-instruction's inverse byte-displacement branch opcode.
const std::unordered_map<std::string, uint8_t> &invertedBranch() {
    static const std::unordered_map<std::string, uint8_t> table = {
        {"jeql", 0x12},   // bneq
        {"jneq", 0x13},   // beql
        {"jgtr", 0x15},   // bleq
        {"jleq", 0x14},   // bgtr
        {"jgeq", 0x19},   // blss
        {"jlss", 0x18},   // bgeq
        {"jgtru", 0x1B},  // blequ
        {"jlequ", 0x1A},  // bgtru
        {"jgequ", 0x1F},  // blssu
        {"jlssu", 0x1E},  // bgequ
    };
    return table;
}

constexpr uint8_t kOpBrw = 0x31;

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
    uint32_t orgBase() const { return orgBase_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
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
                for (const auto &e : line.exprList) emitAddress32(e);
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

    uint32_t evalExpr(const std::vector<Syllable> &syls, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            bool wasExternal = false, wasLocalAddress = false;
            uint32_t v = resolveSyllable(syl, &wasExternal, &wasLocalAddress);
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

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal, bool *wasLocalAddress) {
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

    // --- byte emission (VAX is little-endian) ---------------------------
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

    // A 32-bit address field: in object mode, a symbolic expression records a relocation
    // for its external name and/or the base shift of a same-file label.
    void emitAddress32(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kVaxRelocAbs32});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kVaxRelocAbs32});
        }
        emit32(v);
    }

    static int sizeBytes(char size) {
        switch (size) {
            case 'b': return 1;
            case 'w': return 2;
            case 'l': return 4;
            default: return 8;
        }
    }

    static void rejectPc(const Operand &op, const std::string &mnemonic) {
        if (op.reg == 15) fail("'" + mnemonic + "': the PC isn't usable as an operand register in this dialect");
    }

    // One operand specifier (mode<<4 | register) plus any extension bytes.
    void encodeOperand(const Operand &op, OpSpec spec, const std::string &mnemonic) {
        switch (op.kind) {
            case Operand::Reg:
                rejectPc(op, mnemonic);
                if (spec.access == 'a') fail("'" + mnemonic + "': a register has no address");
                emit8(static_cast<uint8_t>(0x50 | op.reg));
                return;
            case Operand::RegDeferred:
                rejectPc(op, mnemonic);
                emit8(static_cast<uint8_t>(0x60 | op.reg));
                return;
            case Operand::AutoDec:
                rejectPc(op, mnemonic);
                emit8(static_cast<uint8_t>(0x70 | op.reg));
                return;
            case Operand::AutoInc:
                rejectPc(op, mnemonic);
                emit8(static_cast<uint8_t>(0x80 | op.reg));
                return;
            case Operand::Disp: {
                rejectPc(op, mnemonic);
                int32_t d = static_cast<int32_t>(evalExpr(op.expr));
                if (d >= -128 && d <= 127) {
                    emit8(static_cast<uint8_t>(0xA0 | op.reg));
                    emit8(static_cast<uint8_t>(d));
                } else if (d >= -32768 && d <= 32767) {
                    emit8(static_cast<uint8_t>(0xC0 | op.reg));
                    emit16(static_cast<uint16_t>(d));
                } else {
                    emit8(static_cast<uint8_t>(0xE0 | op.reg));
                    emit32(static_cast<uint32_t>(d));
                }
                return;
            }
            case Operand::Imm:
                encodeImmediate(op, spec, mnemonic);
                return;
            case Operand::Abs:
                emit8(0x9F);
                emitAddress32(op.expr);
                return;
            case Operand::Label:
                fail("'" + mnemonic + "': a bare label isn't a valid operand here (use 'abs label' or 'imm label')");
        }
    }

    void encodeImmediate(const Operand &op, OpSpec spec, const std::string &mnemonic) {
        if (spec.access != 'r') fail("'" + mnemonic + "': an immediate can only be a source operand");
        if (!isNumeric(op.expr)) {
            if (spec.size != 'l') fail("'" + mnemonic + "': a symbolic immediate must be a longword operand");
            emit8(0x8F);
            emitAddress32(op.expr);
            return;
        }
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        if (v >= 0 && v <= 63) {
            emit8(static_cast<uint8_t>(v));  // short literal
            return;
        }
        emit8(0x8F);
        switch (spec.size) {
            case 'b':
                if (v < -128 || v > 255) fail("'" + mnemonic + "': immediate out of byte range");
                emit8(static_cast<uint8_t>(v));
                return;
            case 'w':
                if (v < -32768 || v > 65535) fail("'" + mnemonic + "': immediate out of word range");
                emit16(static_cast<uint16_t>(v));
                return;
            case 'l':
                emit32(static_cast<uint32_t>(v));
                return;
            default:  // quad: the 32-bit value sign-extended
                emit32(static_cast<uint32_t>(v));
                emit32(v < 0 ? 0xFFFFFFFFu : 0u);
                return;
        }
    }

    // Emits `opcode`, then the displacement from the byte after the displacement field to
    // `target`. A target not in this file can't be reached by a PC-relative field.
    void encodeBranch(uint8_t opcode, char size, const Operand &target, const std::string &mnemonic) {
        if (target.kind != Operand::Label) fail("'" + mnemonic + "': target must be a label");
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        if (stage_ == 2 && !externalName.empty()) {
            fail("'" + mnemonic + "': branch target '" + externalName + "' isn't defined in this file");
        }
        int len = sizeBytes(size);
        emit8(opcode);
        int64_t rel = static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_ + len);
        if (size == 'b') {
            if (stage_ == 2 && (rel < -128 || rel > 127)) fail("'" + mnemonic + "': branch target out of byte range");
            emit8(static_cast<uint8_t>(rel));
        } else {
            if (stage_ == 2 && (rel < -32768 || rel > 32767)) fail("'" + mnemonic + "': branch target out of word range");
            emit16(static_cast<uint16_t>(rel));
        }
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "jbr") {
            if (ops.size() != 1) fail("'jbr' takes one label operand");
            encodeBranch(kOpBrw, 'w', ops[0], mnemonic);
            return;
        }
        if (auto it = invertedBranch().find(mnemonic); it != invertedBranch().end()) {
            if (ops.size() != 1) fail("'" + mnemonic + "' takes one label operand");
            emit8(it->second);
            emit8(3);  // skips the 3-byte brw below
            encodeBranch(kOpBrw, 'w', ops[0], mnemonic);
            return;
        }
        auto it = instructionTable().find(mnemonic);
        if (it == instructionTable().end()) fail("unsupported instruction '" + mnemonic + "'");
        const InstrDef &def = it->second;
        if (ops.size() != def.ops.size()) {
            fail("'" + mnemonic + "' takes " + std::to_string(def.ops.size()) + " operand(s), got " +
                 std::to_string(ops.size()));
        }
        if (def.ops.size() == 1 && def.ops[0].access == 'b') {
            encodeBranch(def.opcode, def.ops[0].size, ops[0], mnemonic);
            return;
        }
        emit8(def.opcode);
        for (size_t i = 0; i < ops.size(); i++) encodeOperand(ops[i], def.ops[i], mnemonic);
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kVaxCoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
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

std::string assembleVax(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleVaxObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
