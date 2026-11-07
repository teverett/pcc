#include "MCS51Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MCS51Assembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4; nothing here is specific to the 8051.

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    // `Xdata` reserves `exprList`'s byte count in XDATA for every
    // attached label, instead of emitting CODE bytes.
    enum Kind { None, Equate, Org, Instr, Xdata } kind = None;
    std::string equateName;                      // Equate
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Xdata (1, the byte count)
    std::string mnemonic;                         // Instr
    std::vector<std::vector<Syllable>> operands;  // Instr
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

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

std::vector<Syllable> buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect");
    return exprToSyllables(ctx->expr());
}

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    // A compile-time symbol=value binding, no bytes emitted - used for
    // the backend's fixed internal-RAM pseudo-registers.
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "xdata") {
        line.kind = Line::Xdata;
        if (instr->operand().size() != 1 || instr->operand(0)->LBRACKET()) {
            fail("'xdata' needs exactly one operand (a byte count)");
        }
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
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

// A pending fixup, turned into a real ObjRelocation once assembly is
// done. Only ever against `kMCS51RelocAbs16`, so no `kind` field.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
};

// --- Two-pass assembly ----------------------------------------------------
//
// Instruction size is fixed by mnemonic alone, so pass 1 and pass 2
// run identical encoding logic; `labels_` isn't reset between passes
// since pass 1 already resolves every value correctly.
class Assembler {
   public:
    // `allowExternal` is object mode: an undefined CODE-space label in
    // an LJMP/LCALL target becomes a relocation instead of a hard
    // error, and every resolved local label used there also gets a
    // relocation against the pseudo-symbol ".." (see emitAbsOperand()).
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
    // Total XDATA usage in bytes, the sum of every `xdata <count>`
    // directive; becomes `ObjectFile::dataSize`.
    uint32_t dataSize() const { return dataCursor_; }
    const std::unordered_map<std::string, uint32_t> &dataLabels() const { return dataLabels_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        offset_ = 0;
        orgBase_ = 0;
        dataCursor_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        // An `xdata` line's label(s) bind into `dataLabels_` instead
        // of the default CODE-space `labels_`.
        if (line.kind != Line::Xdata) {
            for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        }
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
            case Line::Xdata: {
                uint32_t count = evalExpr(line.exprList.at(0));
                for (const auto &lbl : line.labels) dataLabels_[lbl] = dataCursor_;
                dataCursor_ += count;
                return;
            }
        }
    }

    // `externalName`/`needsBaseShift` are only passed by
    // emitAbsOperand(); other operand kinds never resolve a
    // relocatable CODE-space address, so they leave both null.
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

    // Decimal or `0x`-prefixed hex; shared by both CODE- and
    // XDATA-space resolution.
    static uint32_t parseNumber(const std::string &t) {
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
        }
        return static_cast<uint32_t>(std::stoul(t));
    }

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            // Only emitAbsOperand() tracks `wasLocalAddress`, and only
            // for a real CODE-space (function/branch) label, never a
            // pseudo-register equate.
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

    // XDATA-space counterpart of evalExpr(), resolving against
    // `dataLabels_`. Never deferred as an external reference: an
    // `xdata` directive is only ever emitted for a variable this file
    // itself defines, so an undefined symbol here is always a bug,
    // never something the linker could resolve later.
    uint32_t evalDataExpr(const std::vector<Syllable> &syls) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            uint32_t v = resolveDataSyllable(syl);
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveDataSyllable(const Syllable &syl) {
        if (syl.kind == SylKind::Group) return evalDataExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
        auto it = dataLabels_.find(syl.text);
        if (it != dataLabels_.end()) return it->second;
        if (stage_ == 2) {
            fail("internal: '" + syl.text + "' has no XDATA address - an 'xdata' directive should have been "
                 "emitted for it before any code referencing it");
        }
        return 0;
    }

    // --- byte emission --------------------------------------------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    // A direct-RAM address operand (0-255): every pseudo-register
    // equate and real SFR name resolves to one of these.
    void emitDirectOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("direct-address operand doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // An immediate 8-bit operand; always a plain literal <256, never a label.
    void emitImm8Operand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("immediate operand doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // `mov dptr,#imm16`'s operand: the only instruction given an
    // XDATA-space symbol (optionally `+<constant>` for an array
    // element), emitted high byte first (real 8051 encoding order).
    // In object mode this always also gets a relocation against the
    // pseudo-symbol "..d", even for a same-file reference.
    void emitDataPtrOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalDataExpr(e);
        if (stage_ == 2 && allowExternal_) {
            relocations_.push_back({offset_, "..d"});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // `ljmp`/`lcall`'s absolute CODE-space operand, high byte first. A
    // locally-resolved label still needs this object's base added
    // once linked (the ".." pseudo-symbol); an undefined one is
    // deferred to the linker entirely.
    void emitAbsOperand(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // A short conditional branch's signed rel8 operand. safeBranch()/
    // safeDjnz() guarantee the target is always the next instruction,
    // so an out-of-range result here can only mean a codegen bug.
    void emitRelOperand(const std::vector<Syllable> &target) {
        uint32_t endAddr = orgBase_ + offset_ + 1;  // right after the opcode (+ any direct/imm byte already emitted)
        uint32_t targetAddr = evalExpr(target);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) {
            fail("internal: branch target out of range");
        }
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    // Not a general 8051 encoder: only the mnemonic/operand-shape
    // combinations MCS51Codegen.cpp emits.
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
            {"ret", 0x22},    {"reti", 0x32},  {"nop", 0x00},    {"clra", 0xE4},   {"cpla", 0xF4},
            {"clrc", 0xC3},   {"rla", 0x23},   {"rra", 0x03},    {"rlca", 0x33},   {"rrca", 0x13},
            {"mul", 0xA4},    {"div", 0x84},   {"incdptr", 0xA3}, {"movxr", 0xE0}, {"movxw", 0xF0},
            {"movc", 0x93},   {"jmpadptr", 0x73},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }
        // SETB/CLR on EA (the global interrupt-enable bit, IE.7 =
        // 0xAF), the one bit address this dialect needs; spelled out
        // as fixed-encoding mnemonics rather than exposing general
        // bit-address operand syntax.
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> fixedBit = {
            {"setea", {0xD2, 0xAF}}, {"clrea", {0xC2, 0xAF}},
        };
        if (auto it = fixedBit.find(mnemonic); it != fixedBit.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second.first);
            emit8(it->second.second);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> direct = {
            {"mova", 0xE5}, {"movd", 0xF5}, {"incd", 0x05}, {"decd", 0x15}, {"add", 0x25},
            {"addc", 0x35}, {"subb", 0x95}, {"anl", 0x55},  {"orl", 0x45},  {"xrl", 0x65},
            {"push", 0xC0}, {"pop", 0xD0},
        };
        if (auto it = direct.find(mnemonic); it != direct.end()) {
            emit8(it->second);
            emitDirectOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> imm8 = {
            {"movai", 0x74}, {"addi", 0x24}, {"addci", 0x34}, {"subbi", 0x94}, {"anli", 0x54}, {"orli", 0x44},
            {"xrli", 0x64},
        };
        if (auto it = imm8.find(mnemonic); it != imm8.end()) {
            emit8(it->second);
            emitImm8Operand(one());
            return;
        }
        // `MOV DPL,direct`/`MOV DPH,direct`: real direct-to-direct
        // `MOV` (0x85) encodes source first, destination second. DPL
        // is SFR 0x82, DPH is 0x83.
        if (mnemonic == "movdpl" || mnemonic == "movdph") {
            emit8(0x85);
            emitDirectOperand(one());
            emit8(mnemonic == "movdpl" ? 0x82 : 0x83);
            return;
        }
        // A general direct-to-direct `MOV dst,src` (0x85): moves one
        // pseudo-register to another without going through the
        // accumulator.
        if (mnemonic == "movdd") {
            const auto &o = two();
            emit8(0x85);
            emitDirectOperand(o[1]);  // src first (real hardware encoding order)
            emitDirectOperand(o[0]);  // dst second
            return;
        }
        if (mnemonic == "movdptr") {
            emit8(0x90);
            emitDataPtrOperand(one());
            return;
        }
        // Same `MOV DPTR,#data16` encoding as `movdptr` (0x90), but
        // with a CODE-space operand instead of an XDATA-space one -
        // used when a function's address is needed as a plain 16-bit
        // value rather than something to jump/call to.
        if (mnemonic == "movdptrc") {
            emit8(0x90);
            emitAbsOperand(one());
            return;
        }
        if (mnemonic == "movdi") {  // MOV direct,#imm
            const auto &o = two();
            emit8(0x75);
            emitDirectOperand(o[0]);
            emitImm8Operand(o[1]);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> abs16 = {
            {"ljmp", 0x02},
            {"lcall", 0x12},
        };
        if (auto it = abs16.find(mnemonic); it != abs16.end()) {
            emit8(it->second);
            emitAbsOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> rel = {
            {"jz", 0x60}, {"jnz", 0x70}, {"jc", 0x40}, {"jnc", 0x50},
        };
        if (auto it = rel.find(mnemonic); it != rel.end()) {
            emit8(it->second);
            emitRelOperand(one());
            return;
        }
        // `JB bit,rel`/`JNB bit,rel` on the two fixed bits tested:
        // ACC.7 (0xE7, sign bit) and PSW.OV (0xD2, overflow).
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> fixedBitBranch = {
            {"jbacc7", {0x20, 0xE7}}, {"jnbacc7", {0x30, 0xE7}}, {"jbov", {0x20, 0xD2}}, {"jnbov", {0x30, 0xD2}},
        };
        if (auto it = fixedBitBranch.find(mnemonic); it != fixedBitBranch.end()) {
            emit8(it->second.first);
            emit8(it->second.second);
            emitRelOperand(one());
            return;
        }
        if (mnemonic == "djnz") {
            const auto &o = two();
            emit8(0xD5);
            emitDirectOperand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        // CJNE isn't emitted by codegen itself (compares use a
        // SUBB-chain idiom instead), but stays part of the dialect
        // for `asm`-escape use.
        if (mnemonic == "cjnei") {  // CJNE A,#imm,rel
            const auto &o = two();
            emit8(0xB4);
            emitImm8Operand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        if (mnemonic == "cjne") {  // CJNE A,direct,rel
            const auto &o = two();
            emit8(0xB5);
            emitDirectOperand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<Relocation> relocations_;
    bool allowExternal_;
    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    // XDATA-space label table and cursor, independent of
    // `labels_`/`offset_`/`orgBase_`; `dataCursor_` only advances via
    // an `xdata <count>` directive, never instruction byte emission.
    std::unordered_map<std::string, uint32_t> dataLabels_;
    uint32_t dataCursor_ = 0;
    int stage_ = 1;
};

// Builds the format-neutral ObjectFile from a completed object-mode
// Assembler: every `dataLabels_` entry becomes an `isData=true`
// symbol instead of a CODE-space one, and `dataCursor_` becomes
// `ObjectFile::dataSize`.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kMCS51CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());
    obj.dataSize = a.dataSize();

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    for (const auto &kv : a.dataLabels()) {
        obj.symbols.push_back({kv.first, kv.second, /*isDefined=*/true, /*isData=*/true});
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, kMCS51RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleMCS51(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleMCS51Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
