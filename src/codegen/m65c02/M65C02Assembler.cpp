#include "M65C02Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("M65C02Assembler: " + msg); }

// --- Parsing, via the one shared grammar/RccAsm.g4 -----------------------
// A signed sum of numbers/symbols/parenthesized groups. No register syntax
// exists in this dialect, so every operand is just one of these.

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
    std::string equateName;                       // Equate
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Db/DbLo/DbHi (N)/Times' inner db (1)
    std::string mnemonic;                         // Instr
    std::vector<std::vector<Syllable>> operands;  // Instr
    std::vector<Syllable> timesCount;              // Times
    std::string innerMnemonic;                      // Times: the inner mnemonic (always "db")
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
    // counter (used for the fixed zero-page pseudo-registers).
    if (auto *assign = dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    // `'@' addr` placement (M65C02Codegen.cpp's emitPlacement()): pads
    // forward to a byte offset from the current `org` via
    // `times (addr-($-$$)) db 0`, matching the 8086 backend's own
    // placement idiom (see I8086Assembler.cpp).
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
    // dblo/dbhi: the low/high byte of a 16-bit expression (e.g. a label's
    // address) rather than an already-8-bit value, since there's no 16-bit
    // data directive that can resolve a symbol's address in one piece.
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
// Every instruction's size is fixed by its mnemonic alone, so pass 1 and
// pass 2 run identical encoding logic. `labels_` isn't reset between
// passes: pass 1 already resolves every value correctly (sizes never
// depend on values), so pass 2 just re-derives the same bindings and emits
// bytes using them.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t type;
};

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

    // A label and an Equate both bind a name in the same symbol table: a
    // label's value is the current location, an Equate's is its own
    // expression's value, independent of location. `equates_` tracks which
    // names came from an Equate, since those (the fixed zero-page
    // pseudo-registers) must never get the ".." base-shift relocation an
    // ordinary label reference needs (see resolveSyllable() below).
    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                equates_.insert(line.equateName);
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
            if (allowExternal_ && wasLocalAddress && !equates_.count(syl.text)) *wasLocalAddress = true;
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

    // db's operand is a standalone byte value, so (unlike the other emit*
    // helpers) it's a hard error for it not to fit in a byte.
    void emitByteOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("'db' value doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // Absolute addressing's 2-byte little-endian operand; no range check.
    void emitAbsOperand(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM65C02RelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM65C02RelocAbs16});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // The LOW byte of e's resolved value - equivalent to a real assembler's
    // #<label low-byte operator, just spelled as a plain mnemonic.
    void emitImmLow(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM65C02RelocAbsLo8});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM65C02RelocAbsLo8});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // The HIGH byte of e's resolved value (a real assembler's #>label).
    void emitImmHigh(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM65C02RelocAbsHi8});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM65C02RelocAbsHi8});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    // A single-byte zero-page address operand: ldaind/staind's own operand
    // (zero-page indirect-indexed addressing is only legal for an address
    // that fits in zero page), and rmb0-7/smb0-7/bbr0-7/bbs0-7's own bit-
    // addressed byte (zero-page-only on real 65C02 hardware too).
    void emitZpOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("operand must be a zero-page address");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // A branch's signed rel8 operand, relative to the address right after
    // this field (the real 65C02 convention, also correct for bbr0-7/
    // bbs0-7's own trailing branch operand - callers emit any earlier
    // operand byte(s) first, so `offset_` is already past them here).
    // safeBranch() in M65C02Codegen.cpp guarantees every codegen-emitted
    // branch targets the very next instruction; a hand-written `asm { }`
    // bbr/bbs/beq/etc. branch has no such guarantee and can legitimately
    // fail this range check.
    void emitBranchOperand(const std::vector<Syllable> &target) {
        uint32_t endAddr = orgBase_ + offset_ + 1;  // 1-byte field, right after the opcode
        uint32_t targetAddr = evalExpr(target);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) {
            fail("branch target out of range (more than 127 bytes away)");
        }
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    // Not a general 65C02 encoder: only the mnemonic/operand-shape
    // combinations M65C02Codegen.cpp emits, plus (rmb0-7/smb0-7/bbr0-7/
    // bbs0-7/wai/stp) real WDC 65C02 instructions no C-level construct maps
    // to, plus `ldxi` (immediate-only LDX, same reason and same opcode as
    // M6502Assembler.cpp's own - CIO IOCB-number selection in
    // src/rt/atari400/) - these are never emitted by M65C02Codegen.cpp
    // itself, only reachable by a user's own `asm { }` block (see
    // docs/language.md's "Inline asm" - exactly the escape hatch that
    // comment describes).
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        auto one = [&]() -> const std::vector<Syllable> & {
            if (ops.size() != 1) fail("'" + mnemonic + "' needs exactly one operand");
            return ops[0];
        };
        auto two = [&]() -> const std::vector<std::vector<Syllable>> & {
            if (ops.size() != 2) fail("'" + mnemonic + "' needs exactly two operands");
            return ops;
        };
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"rts", 0x60}, {"clc", 0x18}, {"sec", 0x38}, {"pha", 0x48}, {"pla", 0x68}, {"cli", 0x58}, {"sei", 0x78},
            {"wai", 0xCB}, {"stp", 0xDB},
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
            {"jmpi", 0x6C},  // JMP indirect ($6C): operand is the address of a two-byte jump-target cell
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
        // STZ: the one CMOS-only instruction this backend uses. Two
        // distinct mnemonics, not one disambiguated by operand value, so
        // every mnemonic keeps a single fixed size: stzp is zero-page
        // (opcode $64), stz absolute (opcode $9C).
        if (mnemonic == "stzp") {
            emit8(0x64);
            uint32_t v = evalExpr(one());
            if (stage_ == 2 && v > 0xFF) fail("'stzp' operand must be a zero-page address");
            emit8(static_cast<uint8_t>(v & 0xFF));
            return;
        }
        if (mnemonic == "stz") {
            emit8(0x9C);
            emitAbsOperand(one());
            return;
        }
        // rmb0-7/smb0-7: reset/set bit n of a zero-page byte directly, no
        // register involved - a single two-byte instruction, opcode plus
        // the zero-page address.
        static const std::unordered_map<std::string, uint8_t> bitZp = {
            {"rmb0", 0x07}, {"rmb1", 0x17}, {"rmb2", 0x27}, {"rmb3", 0x37},
            {"rmb4", 0x47}, {"rmb5", 0x57}, {"rmb6", 0x67}, {"rmb7", 0x77},
            {"smb0", 0x87}, {"smb1", 0x97}, {"smb2", 0xA7}, {"smb3", 0xB7},
            {"smb4", 0xC7}, {"smb5", 0xD7}, {"smb6", 0xE7}, {"smb7", 0xF7},
        };
        if (auto it = bitZp.find(mnemonic); it != bitZp.end()) {
            emit8(it->second);
            emitZpOperand(one());
            return;
        }
        // bbr0-7/bbs0-7: branch if bit n of a zero-page byte is reset/set -
        // a three-byte instruction (opcode, zero-page address, signed rel8
        // branch displacement); emitBranchOperand() computes that
        // displacement relative to the end of this instruction regardless
        // of how many bytes already preceded it in this call, so it's
        // correct here exactly as it is for an ordinary two-byte branch.
        static const std::unordered_map<std::string, uint8_t> bitBranch = {
            {"bbr0", 0x0F}, {"bbr1", 0x1F}, {"bbr2", 0x2F}, {"bbr3", 0x3F},
            {"bbr4", 0x4F}, {"bbr5", 0x5F}, {"bbr6", 0x6F}, {"bbr7", 0x7F},
            {"bbs0", 0x8F}, {"bbs1", 0x9F}, {"bbs2", 0xAF}, {"bbs3", 0xBF},
            {"bbs4", 0xCF}, {"bbs5", 0xDF}, {"bbs6", 0xEF}, {"bbs7", 0xFF},
        };
        if (auto it = bitBranch.find(mnemonic); it != bitBranch.end()) {
            const auto &operands = two();
            emit8(it->second);
            emitZpOperand(operands[0]);
            emitBranchOperand(operands[1]);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_set<std::string> equates_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kM65C02CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.type});
    }
    return obj;
}

}  // namespace

std::string assembleM65C02(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleM65C02Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
