#include "Pdp1Assembler.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "RccAsmLexer.h"
#include "RccAsmParser.h"
#include "fileformat/Pdp7TapePacking.h"

namespace rcc {

namespace {

constexpr uint32_t MASK18 = 0777777;

// The pdp1 opcode table Pdp1Codegen.cpp's own instruction selection emits
// against - see Pdp1Codegen.h's class comment for the instruction set this
// was built from (verified against SIMH's `pdp1_cpu.c` decode logic, not
// against pdp7's). `i` is the indirect bit (bit 12, mask 010000), a bare
// operand symbol OR'd in via the same generic syllable-combine mechanism
// Pdp7Assembler.cpp uses - see that file's own header comment for how the
// shared grammar's "no real instruction/operand distinction" parsing works.
//
// Deliberately narrow: only the mnemonics Pdp1Codegen.cpp (and
// pdp1RuntimeAsm()) actually emit. In particular there's no `sal`/`sar`
// (Pdp1Codegen.cpp builds every shift out of `ral`/`rar` plus an explicit
// bit mask instead - see emitShiftOne()'s comment) and no `iot`/iot-group
// device codes (no I/O support in this backend pass).
const std::unordered_map<std::string, uint32_t> &builtins() {
    static const std::unordered_map<std::string, uint32_t> table = {
        {"i", 010000},

        {"and", 0020000}, {"ior", 0040000}, {"xor", 0060000},
        {"lac", 0200000}, {"dac", 0240000}, {"dzm", 0340000},
        {"add", 0400000}, {"sub", 0420000},
        {"sad", 0500000}, {"sas", 0520000},
        {"jmp", 0600000}, {"jsp", 0620000},
        {"law", 0700000},

        // Skip group (opcode 032): base 0640000, OR'd with one mask.
        {"sma", 0640400}, {"spa", 0640200}, {"sza", 0640100},

        // Operate group (opcode 037): base 0760000, OR'd with one mask.
        {"nop", 0760000}, {"cla", 0760200}, {"cma", 0761000}, {"hlt", 0760400},

        // Shift/rotate group (opcode 033): base 0660000, subopcode<<9, a
        // fixed count of 1 (see emitShiftOne()'s comment on why this
        // backend never uses the hardware count field for a larger shift).
        {"ral", 0661001}, {"rar", 0671001},
    };
    return table;
}

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp1Assembler: " + msg); }

enum class SylKind { Number, Symbol };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
};

struct Segment {
    std::vector<std::string> labels;
    enum Kind { None, Assign, Expr } kind = None;
    std::string lhs;
    std::vector<Syllable> syllables;
};

struct Line {
    std::string sourceText;
    std::vector<Segment> segments;
};

std::vector<Syllable> exprToSyllables(RccAsmParser::ExprContext *ctx) {
    std::vector<Syllable> result;
    for (auto *syl : ctx->syllable()) {
        char sign = 0;
        if (auto *s = syl->sign()) sign = s->PLUS() ? '+' : '-';
        auto *atom = syl->atom();
        if (atom->expr() != nullptr) fail("'(...)' grouping isn't part of pdp1's own dialect");
        if (atom->NUMBER()) {
            result.push_back({sign, SylKind::Number, atom->NUMBER()->getText()});
        } else {
            result.push_back({sign, SylKind::Symbol, atom->IDENT()->getText()});
        }
    }
    if (result.empty()) fail("empty expression");
    return result;
}

void appendOperandSyllables(RccAsmParser::OperandContext *ctx, std::vector<Syllable> &out) {
    if (ctx->LBRACKET()) fail("'[...]' addressing isn't part of pdp1's own dialect");
    auto more = exprToSyllables(ctx->expr());
    out.insert(out.end(), more.begin(), more.end());
}

Segment buildSegment(RccAsmParser::LineContext *ctx) {
    Segment seg;
    for (auto *lbl : ctx->label()) seg.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        seg.kind = Segment::None;
        return seg;
    }
    if (auto *assign = dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        seg.kind = Segment::Assign;
        seg.lhs = assign->lhs->getText();
        seg.syllables = exprToSyllables(assign->rhs);
        return seg;
    }
    if (dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of pdp1's own dialect - that's purely an 8086/NASM idiom");
    }
    auto *instr = dynamic_cast<RccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    seg.kind = Segment::Expr;
    SylKind mnemonicKind = (instr->mnemonic->getType() == RccAsmParser::NUMBER) ? SylKind::Number : SylKind::Symbol;
    seg.syllables.push_back({0, mnemonicKind, instr->mnemonic->getText()});
    for (auto *op : instr->operand()) appendOperandSyllables(op, seg.syllables);
    return seg;
}

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

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
    for (auto *lineCtx : prog->line()) {
        Line line;
        antlr4::Token *start = lineCtx->getStart();
        antlr4::Token *stop = lineCtx->NEWLINE()->getSymbol();
        line.sourceText = text.substr(start->getStartIndex(), stop->getStartIndex() - start->getStartIndex());

        Segment seg = buildSegment(lineCtx);
        if (seg.kind != Segment::None || !seg.labels.empty()) line.segments.push_back(std::move(seg));
        lines.push_back(std::move(line));
    }
    return lines;
}

class Assembler {
   public:
    Assembler() {
        symbols_ = builtins();
        symbols_["."] = kPdp1BaseAddress;
        symbols_[".."] = kPdp1BaseAddress;
    }

    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        for (const auto &line : lines) processLine(line);
        symbols_["."] = kPdp1BaseAddress;
        stage_ = 2;
        for (const auto &line : lines) processLine(line);
    }

    const std::vector<std::optional<uint32_t>> &mem() const { return mem_; }
    const std::vector<std::string> &mline() const { return mline_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    uint32_t base() const { return kPdp1BaseAddress; }

   private:
    void processLine(const Line &line) {
        bool lineConsumed = false;
        for (const auto &seg : line.segments) {
            for (const auto &lbl : seg.labels) setLabel(lbl, symbols_["."]);
            if (seg.kind == Segment::Assign) {
                symbols_[seg.lhs] = evalExpr(seg.syllables);
            } else if (seg.kind == Segment::Expr) {
                uint32_t word = evalExpr(seg.syllables);
                if (stage_ == 2) storeWord(line.sourceText, lineConsumed, word);
                symbols_["."] = symbols_["."] + 1;
            }
        }
    }

    void setLabel(const std::string &name, uint32_t loc) {
        symbols_[name] = loc;
        labels_[name] = loc;
    }

    uint32_t evalExpr(const std::vector<Syllable> &syls) {
        uint32_t word = 0;
        for (const auto &syl : syls) {
            char op = syl.sign ? syl.sign : '|';
            uint32_t value = resolveSyllable(syl) & MASK18;
            if (op == '+') {
                word += value;
            } else if (op == '-') {
                word -= value;
            } else {
                word |= value;
            }
            word &= MASK18;
        }
        return word;
    }

    uint32_t resolveSyllable(const Syllable &syl) {
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            uint32_t v = 0;
            if (!t.empty() && t[0] == '0') {
                for (char c : t) v = v * 8 + static_cast<uint32_t>(c - '0');
            } else {
                v = static_cast<uint32_t>(std::stoul(t));
            }
            return v & MASK18;
        }
        auto it = symbols_.find(syl.text);
        if (it != symbols_.end()) return it->second;
        if (stage_ == 2) fail("undefined symbol '" + syl.text + "'");
        return 0;
    }

    // Checks the same invariant Pdp1Assembler.h's own constants describe:
    // an instruction word's address field must stay inside the 12-bit
    // direct-addressing field, and clear of the runtime call stack's own
    // reserved margin at the top of the address space.
    void checkAddressSafe(uint32_t addr) {
        if (addr >= kPdp1AddressFieldLimit) {
            std::ostringstream msg;
            msg << "address " << addr << " (0" << std::oct << addr
                << " octal) is past the pdp1's 4096-word (010000 octal) direct-addressing field limit - "
                   "program too large";
            fail(msg.str());
        }
        if (addr >= kPdp1StackSafeLimit) {
            std::ostringstream msg;
            msg << "address " << addr << " (0" << std::oct << addr << " octal) is within " << std::dec
                << kPdp1StackReserve
                << " words of the pdp1's 4096-word address space top, too close to where the runtime "
                   "call stack (starting at 4095, growing downward with no bounds check) will reach - "
                   "program too large";
            fail(msg.str());
        }
    }

    void storeWord(const std::string &sourceText, bool &lineConsumed, uint32_t word) {
        uint32_t location = symbols_["."];
        checkAddressSafe(location);
        size_t idx = static_cast<size_t>(location);
        if (mem_.size() <= idx) {
            mem_.resize(idx + 1);
            mline_.resize(idx + 1);
        }
        mem_[idx] = word;
        mline_[idx] = lineConsumed ? "" : sourceText;
        lineConsumed = true;
    }

    std::unordered_map<std::string, uint32_t> symbols_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<std::optional<uint32_t>> mem_;
    std::vector<std::string> mline_;
    int stage_ = 1;
};

std::string writeList(const Assembler &a) {
    std::ostringstream out;
    char buf[64];
    const auto &mem = a.mem();
    const auto &mline = a.mline();
    for (size_t i = 0; i < mem.size(); ++i) {
        if (!mem[i]) continue;
        std::snprintf(buf, sizeof(buf), "%06o: %06o\t", static_cast<unsigned>(i), *mem[i]);
        out << buf << mline[i] << "\n";
    }
    out << "\nLabels:\n";
    std::vector<std::string> names;
    for (const auto &kv : a.labels()) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    for (const auto &name : names) {
        std::snprintf(buf, sizeof(buf), "%-8.8s %#06o", name.c_str(), a.labels().at(name));
        out << buf << "\n";
    }
    return out.str();
}

// A genuine PDP-1 RIM-format tape is NOT a contiguous memory dump with a
// specially-marked trailer byte the way pdp7's is (verified against SIMH's
// `rim_load` decode logic in pdp1_sys.c, not assumed from pdp7's own
// convention - the two targets' hardware bootstrap loaders work
// differently despite sharing a word size and byte-packing scheme): the
// loader reads 18-bit words in a plain loop with no out-of-band marker
// bit at all, and tells words apart purely by their opcode bits - a
// `dac`/`dio` word gives the address the *next* word (arbitrary data,
// never opcode-checked) is stored at, and a `jmp` word ends the load and
// starts execution at its own address field. So this writes one
// `(dac addr, data)` pair per defined memory word, terminated by one
// `jmp start`. Only the 3-byte/6-bits-per-byte/high-bit-set packing itself
// is shared with pdp7 (via packPdp7Tape(), called here with
// `rimSignature=false` so it adds no pdp7-specific marker bit).
constexpr uint32_t kDacOpcode = 0240000;
constexpr uint32_t kJmpOpcode = 0600000;

std::string writeRim(const Assembler &a) {
    std::vector<uint32_t> words;
    const auto &mem = a.mem();
    for (size_t loc = 0; loc < mem.size(); ++loc) {
        if (!mem[loc]) continue;
        words.push_back(kDacOpcode | static_cast<uint32_t>(loc));
        words.push_back(*mem[loc]);
    }
    words.push_back(kJmpOpcode | a.base());
    return packPdp7Tape(words, /*base=*/0, /*rimSignature=*/false);
}

}  // namespace

std::string assemblePdp1(const std::string &asmText, Pdp1OutputFormat format) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler;
    assembler.run(lines);

    switch (format) {
        case Pdp1OutputFormat::List:
            return writeList(assembler);
        case Pdp1OutputFormat::Rim:
            return writeRim(assembler);
    }
    fail("unknown output format");
}

}  // namespace rcc
