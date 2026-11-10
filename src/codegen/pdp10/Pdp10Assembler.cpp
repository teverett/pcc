#include "Pdp10Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp10Assembler: " + msg); }

constexpr uint64_t kMask18 = 0777777;
constexpr uint64_t kMask36 = 0777777777777ULL;

// --- Parsing, via the shared grammar/PccAsm.g4 ---------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One `E` operand. `Plain`: a bare expression - a literal number (never relocated) or a label
// (always an absolute address, relocated via kPdp10RelocAbs18). `Indexed`: the `[ACn+N]`/
// `[ACn-N]` bracket form - index register `reg`, displacement `disp` (always a compile-time
// literal, never a symbol).
struct EOperand {
    enum Kind { Plain, Indexed } kind = Plain;
    std::vector<Syllable> expr;  // Plain: the operand's own value/target.
    int reg = -1;                // Indexed: the index register (0-17).
    std::vector<Syllable> disp;  // Indexed: the displacement (empty means 0).
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
    std::string mnemonic;                          // Instr/Halt
    std::vector<Syllable> acExpr;                  // Instr: the `ac` operand (always a bare 0-17 literal)
    EOperand eOperand;                             // Instr: the `E` operand
    bool hasEOperand = false;                      // false only for the zero-operand `halt`
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

// `[ACn+N]`/`[ACn-N]`: first syllable must be a bare, unsigned base-register number; every
// remaining syllable a literal number (never a symbol, so never a relocation).
EOperand buildEOperand(PccAsmParser::OperandContext *ctx) {
    EOperand op;
    if (ctx->LBRACKET()) {
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        if (syls.empty() || syls[0].sign != 0 || syls[0].kind != SylKind::Number) {
            fail("'[...]' operand must start with a bare index-register number, e.g. '[16+4]'");
        }
        op.kind = EOperand::Indexed;
        op.reg = static_cast<int>(std::stoul(syls[0].text));
        op.disp.assign(syls.begin() + 1, syls.end());
        return op;
    }
    op.kind = EOperand::Plain;
    op.expr = exprToSyllables(ctx->expr());
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
    if (mnemonic == "halt") {
        if (!instr->operand().empty()) fail("'halt' takes no operand");
        line.hasEOperand = false;
        return line;
    }
    auto ops = instr->operand();
    if (ops.size() != 2) fail("'" + mnemonic + "': needs exactly two operands (ac,E)");
    if (ops[0]->LBRACKET()) fail("'" + mnemonic + "': the ac operand can't be bracketed");
    line.acExpr = exprToSyllables(ops[0]->expr());
    line.eOperand = buildEOperand(ops[1]);
    line.hasEOperand = true;
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

// One pending fixup, turned into a real ObjRelocation once assembly is done.
struct Relocation {
    uint32_t byteOffset;  // in *word* units until buildObjectFile() scales it by 5
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

    // All in *word* units (matching `org`'s own units, the same as every address Pdp10Codegen
    // ever emits) - `words()` is the whole program's/object's text, one entry per 36-bit word;
    // packing into the 5-byte-per-word image is done once, at the very end (see
    // assemblePdp10()/buildObjectFile() below), mirroring Pdp7Assembler.cpp's own `words_`/
    // `wordOffset` convention exactly (same non-byte-addressable shape).
    const std::vector<uint64_t> &words() const { return words_; }
    uint32_t orgBase() const { return orgBase_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }

   private:
    void pass(const std::vector<Line> &lines) {
        words_.clear();
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
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint64_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, "..", kPdp10RelocAbs18});
                        if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPdp10RelocAbs18});
                    }
                    emit36(v);
                }
                return;
            case Line::Instr:
                encodeInstr(line);
                return;
        }
    }

    // `needsBaseShift` is set only when the resolved value came from a label defined in this
    // same file (a locally-resolved address, which still shifts if the linker places this
    // object at a non-zero base); a pure numeric literal never sets it.
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

    // Appends one 36-bit word to the word-oriented text, advancing the word-unit location
    // counter by exactly 1 (not 5 - see `words()`'s own comment above).
    void emit36(uint64_t w) {
        words_.push_back(w & kMask36);
        offset_ += 1;
    }

    // Every instruction word's `ac` operand must already be a bare literal 0-17 (Pdp10Codegen
    // never emits a symbolic register name) - never a relocation target.
    uint32_t evalAc(const std::vector<Syllable> &syls, const std::string &mnemonic) {
        uint64_t v = evalExpr(syls);
        if (v > 017) fail("'" + mnemonic + "': ac operand out of range (0-17): " + std::to_string(v));
        return static_cast<uint32_t>(v);
    }

    // Builds the full 36-bit instruction word for `opcode ac,E`, tracking E's relocation the
    // same way Dw's operand does.
    uint64_t encodeWord(uint32_t opcode, uint32_t ac, const EOperand &e, const std::string &mnemonic) {
        uint64_t word = (static_cast<uint64_t>(opcode) << 27) | (static_cast<uint64_t>(ac) << 23);
        if (e.kind == EOperand::Indexed) {
            if (e.reg < 0 || e.reg > 017) fail("'" + mnemonic + "': index register out of range (0-17)");
            uint64_t disp = e.disp.empty() ? 0 : evalExpr(e.disp);
            word |= (static_cast<uint64_t>(e.reg) << 18) | (disp & kMask18);
            return word;
        }
        std::string externalName;
        bool needsBaseShift = false;
        uint64_t y = evalExpr(e.expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            // The low 18 address bits aren't byte-aligned within the word's 5-byte packing
            // (they straddle a fractional boundary inside byte 2 - see Pdp10LinkerFactory.h's
            // own header comment), so the relocation site is the word's own start offset;
            // Pdp10LinkerFactory.cpp's patcher re-reads/re-writes the whole word via
            // read36()/write36() rather than touching individual bytes.
            if (needsBaseShift) relocations_.push_back({offset_, "..", kPdp10RelocAbs18});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPdp10RelocAbs18});
        }
        word |= (y & kMask18);
        return word;
    }

    static const std::unordered_map<std::string, uint32_t> &opcodeTable() {
        static const std::unordered_map<std::string, uint32_t> table = {
            {"move", 0200},  {"movei", 0201}, {"movem", 0202}, {"movn", 0210},
            {"add", 0270},   {"addi", 0271},  {"sub", 0274},   {"subi", 0275},
            {"and", 0404},   {"andi", 0405},  {"ior", 0434},   {"iori", 0435},
            {"xor", 0430},   {"xori", 0431},  {"setcm", 0460},
            {"lsh", 0242},   {"ash", 0240},
            {"mul", 0224},   {"idiv", 0230},  {"div", 0234},
            {"jumpl", 0321}, {"jumple", 0323}, {"jumpe", 0322}, {"jumpn", 0326},
            {"jumpge", 0325}, {"jumpg", 0327}, {"jrst", 0254},
            {"push", 0261},  {"pushj", 0260}, {"pop", 0262},   {"popj", 0263},
        };
        return table;
    }

    void encodeInstr(const Line &line) {
        if (line.mnemonic == "halt") {
            // JRST 4,0 - see Pdp10Codegen.cpp's own header comment.
            emit36((static_cast<uint64_t>(0254) << 27) | (static_cast<uint64_t>(4) << 23));
            return;
        }
        auto it = opcodeTable().find(line.mnemonic);
        if (it == opcodeTable().end()) fail("unsupported instruction '" + line.mnemonic + "'");
        uint32_t ac = evalAc(line.acExpr, line.mnemonic);
        emit36(encodeWord(it->second, ac, line.eOperand, line.mnemonic));
    }

    std::vector<uint64_t> words_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Packs the word-oriented text into the 5-byte-per-word image Pdp10LinkerFactory.cpp's own
// read36()/write36() (and the real `pdp10`-facing `--format=raw` output) expect.
std::vector<uint8_t> packWords(const std::vector<uint64_t> &words) {
    std::vector<uint8_t> bytes;
    bytes.reserve(words.size() * 5);
    for (uint64_t w : words) {
        w &= kMask36;
        bytes.push_back(static_cast<uint8_t>((w >> 32) & 0xF));
        bytes.push_back(static_cast<uint8_t>((w >> 24) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((w >> 16) & 0xFF));
        bytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(w & 0xFF));
    }
    return bytes;
}

// Builds the format-neutral ObjectFile from a completed object-mode Assembler, packing the
// word-oriented text into bytes and scaling every word-unit address/offset to the matching
// byte offset (5 bytes per word) - mirrors Pdp7Assembler.cpp's own buildObjectFile() exactly
// (same non-byte-addressable shape, different per-word byte count).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kPdp10CoffMachine;
    obj.textBytes = packWords(a.words());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, (kv.second - a.orgBase()) * 5, /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (r.symbolName != ".." && !known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset * 5, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assemblePdp10(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    std::vector<uint8_t> bytes = packWords(assembler.words());
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assemblePdp10Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
