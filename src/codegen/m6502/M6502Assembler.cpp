#include "M6502Assembler.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "RccAsmLexer.h"
#include "RccAsmParser.h"

namespace rcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("M6502Assembler: " + msg); }

// --- Parsing, via the one shared grammar/RccAsm.g4 -----------------------
// A Syllable is a signed sum term: a number, a symbol, or a parenthesized
// group. No register syntax exists in this dialect.

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, DbLo, DbHi, Times, Instr } kind = None;
    std::string equateName;                         // Equate
    std::vector<std::vector<Syllable>> exprList;     // Equate (1)/Org (1)/Db/DbLo/DbHi (N)/Times' inner db (1)
    std::string mnemonic;                             // Instr
    std::vector<std::vector<Syllable>> operands;      // Instr
    std::vector<Syllable> timesCount;                  // Times
    std::string innerMnemonic;                          // Times: the inner mnemonic (always "db")
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

// A pending fixup, turned into a real ObjRelocation once assembly is done.
// `kind` is one of `kM6502RelocAbs16`/`...Lo`/`...Hi`.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

std::vector<Syllable> buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect");
    return exprToSyllables(ctx->expr());
}

Line buildLine(RccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    // A compile-time symbol=value binding, independent of the location
    // counter - used for the fixed zero-page pseudo-registers.
    if (auto *assign = dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    // `'@' addr` placement (M6502Codegen.cpp's emitPlacement()): pads forward to
    // a byte offset from the current `org` via `times (addr-($-$$)) db 0`,
    // matching the 8086 backend's own placement idiom (see I8086Assembler.cpp).
    if (auto *times = dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) {
        line.kind = Line::Times;
        line.timesCount = exprToSyllables(times->count);
        line.innerMnemonic = toLower(times->mnemonic->getText());
        if (line.innerMnemonic != "db") fail("'times' only supports a 'db' body in this dialect");
        for (auto *op : times->operand()) line.exprList.push_back(buildOperand(op));
        return line;
    }
    auto *instr = dynamic_cast<RccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "db") {
        line.kind = Line::Db;
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
        return line;
    }
    // `dblo`/`dbhi`: the low/high byte of a 16-bit expression (e.g. a
    // label's address), for a dispatch-table element naming a
    // function/global directly - resolved the same way as
    // `ldai`/`ldai_hi`'s immediate operand (see emitImmLow/High below).
    if (mnemonic == "dblo" || mnemonic == "dbhi") {
        line.kind = mnemonic == "dblo" ? Line::DbLo : Line::DbHi;
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
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

// --- Two-pass assembly ----------------------------------------------------
//
// Every instruction's size is fixed by its mnemonic alone, so both passes
// run identical encoding logic. `labels_` isn't reset between passes: pass
// 1 already resolves every value correctly; pass 2 just re-derives the
// same bindings and this time emits bytes.
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
        equateNames_.clear();
        offset_ = 0;
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    // A label and an Equate both bind a name: a label's value is the
    // current location; an Equate's is its expression's value now.
    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                // An Equate is a fixed compile-time constant, not a real
                // address, so it must never get a `..` base-shift relocation
                // (see resolveSyllable()) - tracked here to tell them apart.
                equateNames_.insert(line.equateName);
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emitByteOperand(e);
                return;
            case Line::DbLo:
                for (const auto &e : line.exprList) emitImmLow(e);
                return;
            case Line::DbHi:
                for (const auto &e : line.exprList) emitImmHigh(e);
                return;
            case Line::Times: {
                uint32_t count = evalExpr(line.timesCount);
                // A negative "times (addr-($-$$))" count wraps to a huge uint32_t -
                // reject it rather than silently emitting megabytes of padding
                // (it means `addr` is behind the current position already).
                if (count > 0x10000) fail("'@' placement address is before the current position");
                uint32_t v = line.exprList.empty() ? 0 : evalExpr(line.exprList.at(0));
                for (uint32_t i = 0; i < count; i++) emit8(static_cast<uint8_t>(v));
                return;
            }
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // In object mode (`allowExternal_`), any local address reference -
    // not just an external symbol - needs a relocation.
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

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        // `$`/`$$`: the current location / the current `org` base - only
        // meaningful inside a `times (addr-($-$$))` placement-padding count.
        if (syl.text == "$") return orgBase_ + offset_;
        if (syl.text == "$$") return orgBase_;
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            if (allowExternal_ && wasLocalAddress && !equateNames_.count(syl.text)) *wasLocalAddress = true;
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

    // --- byte emission --------------------------------------------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    // `db`'s operand must fit in a byte; unlike the other emit* helpers,
    // an out-of-range value here is a hard error.
    void emitByteOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("'db' value doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // Absolute addressing's 2-byte little-endian operand (full 16-bit
    // value, no range check): covers jsr/jmp and every absolute operand.
    void emitAbsOperand(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // `ldai`/`adci`/`andi`/`eori`/`ldyi`/`ldxi` immediate operand, and `dblo`'s data
    // byte: the LOW byte of the resolved value - equivalent to a real
    // assembler's `#<label` low-byte operator, spelled as a plain mnemonic.
    void emitImmLow(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16Lo});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16Lo});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // `ldai_hi`/`adci_hi` immediate operand and `dbhi`'s data byte: the
    // HIGH byte of the resolved value (a real assembler's `#>label`).
    void emitImmHigh(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16Hi});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16Hi});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    // `ldaind`/`staind` operand: zero-page indirect-indexed addressing is
    // only legal for an address that fits in zero page.
    void emitZpOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("indirect-indexed operand must be a zero-page address");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // A branch's signed rel8 operand. safeBranch() guarantees every branch
    // targets the very next instruction, so out-of-range here is a bug.
    void emitBranchOperand(const std::vector<Syllable> &target) {
        uint32_t endAddr = orgBase_ + offset_ + 1;  // this field is 1 byte, right after the opcode already emitted
        uint32_t targetAddr = evalExpr(target);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) {
            fail("internal: branch target out of range - safeBranch() should have prevented this");
        }
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    // The full instruction vocabulary M6502Codegen.cpp emits, plus `ldxi`
    // (immediate-only LDX, added for src/rt/atari400/'s own CIO IOCB-number
    // selection - see that directory's own atari400.c comment for why only
    // the immediate form is needed, never a general X register). Not a
    // general 6502 encoder otherwise: no X/Y-indexed modes, no absolute
    // LDX/LDY/STX/STY/CMP/CPX/CPY, no TAX/TXA/TAY/TYA - exactly the
    // mnemonic/operand combinations used here.
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        auto one = [&]() -> const std::vector<Syllable> & {
            if (ops.size() != 1) fail("'" + mnemonic + "' needs exactly one operand");
            return ops[0];
        };
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"rts", 0x60}, {"clc", 0x18}, {"sec", 0x38}, {"pha", 0x48}, {"pla", 0x68}, {"cli", 0x58}, {"sei", 0x78},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> absolute = {
            {"lda", 0xAD}, {"sta", 0x8D}, {"adc", 0x6D}, {"sbc", 0xED}, {"and", 0x2D}, {"ora", 0x0D}, {"eor", 0x4D},
            {"asl", 0x0E}, {"lsr", 0x4E}, {"rol", 0x2E}, {"ror", 0x6E}, {"inc", 0xEE}, {"dec", 0xCE},
            {"jmp", 0x4C}, {"jsr", 0x20},
            // Real opcode $6C, JMP indirect: operand is the address of a
            // two-byte cell holding the jump target.
            {"jmpi", 0x6C},
        };
        if (auto it = absolute.find(mnemonic); it != absolute.end()) {
            emit8(it->second);
            emitAbsOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> immLow = {
            {"ldai", 0xA9}, {"ldyi", 0xA0}, {"ldxi", 0xA2}, {"adci", 0x69}, {"andi", 0x29}, {"eori", 0x49},
        };
        if (auto it = immLow.find(mnemonic); it != immLow.end()) {
            emit8(it->second);
            emitImmLow(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> immHigh = {
            {"ldai_hi", 0xA9},
            {"adci_hi", 0x69},
        };
        if (auto it = immHigh.find(mnemonic); it != immHigh.end()) {
            emit8(it->second);
            emitImmHigh(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> indY = {
            {"ldaind", 0xB1},
            {"staind", 0x91},
        };
        if (auto it = indY.find(mnemonic); it != indY.end()) {
            emit8(it->second);
            emitZpOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> branch = {
            {"beq", 0xF0}, {"bne", 0xD0}, {"bcs", 0xB0}, {"bcc", 0x90},
            {"bmi", 0x30}, {"bpl", 0x10}, {"bvc", 0x50},
        };
        if (auto it = branch.find(mnemonic); it != branch.end()) {
            emit8(it->second);
            emitBranchOperand(one());
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_set<std::string> equateNames_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kM6502CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleM6502(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleM6502Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
