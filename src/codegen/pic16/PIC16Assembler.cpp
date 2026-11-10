#include "PIC16Assembler.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("PIC16Assembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 -----------------------

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, OrgRam, Res, Instr } kind = None;
    std::vector<Syllable> rhs;  // Equate / Org / OrgRam / Res's own single expr
    std::string mnemonic;       // Instr
    std::vector<std::vector<Syllable>> operands;
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
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect - PIC16 file-register "
                               "access always names the register directly (optionally via FSR/INDF), never a "
                               "bracketed symbol");
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
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.labels.push_back(assign->IDENT()->getText());
        line.rhs = exprToSyllables(assign->expr());
        return line;
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "org" || mnemonic == "orgram") {
        line.kind = mnemonic == "org" ? Line::Org : Line::OrgRam;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'" + mnemonic + "' needs a plain address operand");
        line.rhs = exprToSyllables(instr->operand(0)->expr());
        return line;
    }
    if (mnemonic == "res") {
        line.kind = Line::Res;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'res' needs a plain count operand");
        line.rhs = exprToSyllables(instr->operand(0)->expr());
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

std::optional<std::string> asBareIdent(const std::vector<Syllable> &operand) {
    if (operand.size() != 1) return std::nullopt;
    const Syllable &s = operand[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// A pending fixup assemblePIC16Object() turns into a real ObjRelocation
// once assembly is done.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
// Code space (program memory, one word per instruction) and data space
// (file registers, byte-addressed) are separate address spaces on this
// Harvard target: `codeOffset_`/`ramOffset_` are never the same counter,
// and `res`/`orgram` never touch `words_`.
class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {}

    void run(const std::vector<Line> &lines) {
        // Classified once up front, independently of either pass: the
        // fixed-pseudo-register equates and res-declared locals/params are
        // a trailer appended after every referencing function body, so
        // classifying lazily would misclassify earlier references.
        classifyLabels(lines);
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint16_t> &words() const { return words_; }
    // Packs two little-endian bytes per cell, matching toIntelHex()'s
    // on-disk shape and ObjectFile::textBytes.
    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> out;
        out.reserve(words_.size() * 2);
        for (uint16_t w : words_) {
            out.push_back(static_cast<uint8_t>(w & 0xFF));
            out.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        }
        return out;
    }
    uint32_t dataSize() const { return ramOffset_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }
    const std::unordered_set<std::string> &equateNames() const { return equateNames_; }
    const std::unordered_set<std::string> &dataLabelNames() const { return dataLabelNames_; }

   private:
    // A compile-time equate (fixed pseudo-registers or SFR names) is the
    // same value in every object regardless of where it's linked, so it
    // never needs a ".."/"..d" base-shift relocation. A `res` label is a
    // real global/local/param's data-space address.
    void classifyLabels(const std::vector<Line> &lines) {
        for (const auto &line : lines) {
            if (line.kind == Line::Equate) {
                equateNames_.insert(line.labels.back());
            } else if (line.kind == Line::Res) {
                for (const auto &lbl : line.labels) dataLabelNames_.insert(lbl);
            }
        }
    }

    void pass(const std::vector<Line> &lines) {
        words_.clear();
        relocations_.clear();
        codeOffset_ = 0;
        ramOffset_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        switch (line.kind) {
            case Line::None:
                for (const auto &lbl : line.labels) labels_[lbl] = codeOffset_;
                return;
            case Line::Equate:
                labels_[line.labels.back()] = evalExpr(line.rhs);
                return;
            case Line::Org:
                for (const auto &lbl : line.labels) labels_[lbl] = codeOffset_;
                codeOffset_ = evalExpr(line.rhs);
                return;
            case Line::OrgRam:
                for (const auto &lbl : line.labels) labels_[lbl] = codeOffset_;
                ramOffset_ = evalExpr(line.rhs);
                return;
            case Line::Res: {
                for (const auto &lbl : line.labels) labels_[lbl] = ramOffset_;
                uint32_t count = evalExpr(line.rhs);
                ramOffset_ += count;
                // This bound only applies in whole-program mode, where
                // `orgram` starts at `kPIC16RamBase` so `ramOffset_` is
                // already a real file-register address; in object mode
                // GenericLinker's data-space layout pass checks this
                // instead.
                if (stage_ == 2 && !allowExternal_ && ramOffset_ > 0x50) {
                    fail("file-register space exhausted (PIC16F84A-class GPR bank is 0x0C-0x4F, 68 bytes) - program "
                         "needs too many globals/locals/temporaries for this target");
                }
                return;
            }
            case Line::Instr:
                for (const auto &lbl : line.labels) labels_[lbl] = codeOffset_;
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // Any local address reference, not just an external one, needs a
    // relocation here once `allowExternal_` is set; the caller
    // (controlAddr() for code, fileAddr()/literal8() for data) decides
    // which pseudo-symbol/relocation kind the result turns into.
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

    // Byte offset of the word about to be emitted (ObjectFile values are
    // always byte offsets, but `codeOffset_` counts words).
    uint32_t byteOffset() const { return codeOffset_ * 2; }

    void emitWord(uint16_t w) {
        words_.push_back(w);
        codeOffset_ += 1;
    }

    const std::vector<Syllable> &operandAt(const std::vector<std::vector<Syllable>> &ops, size_t i,
                                            const std::string &mnemonic, size_t wantCount) {
        if (ops.size() != wantCount) {
            fail("'" + mnemonic + "' needs exactly " + std::to_string(wantCount) + " operand(s)");
        }
        return ops[i];
    }

    // The "d" destination-select bit shared by every f,d ALU instruction:
    // 0 means result to W, 1 means result back to f.
    uint8_t destBit(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (name && *name == "w") return 0;
        if (name && *name == "f") return 1;
        fail("'" + mnemonic + "': destination must be 'w' or 'f'");
    }

    // A data-space (file-register) address - the FD/FB/movwf/clrf
    // family's "f" operand. In object mode a local reference gets a
    // relocation against "..d", an unresolved one against the external
    // name; the field holds only this object's pre-link offset, and the
    // bound check is skipped since GenericLinker's layout pass checks the
    // linked address instead.
    uint8_t fileAddr(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({byteOffset(), "..d", kPIC16RelocData});
            if (!externalName.empty()) relocations_.push_back({byteOffset(), externalName, kPIC16RelocData});
        } else if (stage_ == 2 && v > 0x7F) {
            fail("'" + mnemonic + "': file address 0x" + std::to_string(v) +
                 " doesn't fit this target's 7-bit direct-addressing field");
        }
        return static_cast<uint8_t>(v & 0x7F);
    }

    uint8_t bitNum(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        uint32_t v = evalExpr(operand);
        if (v > 7) fail("'" + mnemonic + "': bit number must be 0-7");
        return static_cast<uint8_t>(v);
    }

    // An 8-bit literal operand (MOVLW/RETLW/ADDLW/SUBLW/ANDLW/IORLW/
    // XORLW) - usually a plain constant, except when loading a data
    // label's address (`movlw lbl`), patched the same way as fileAddr()'s
    // "f" field.
    uint8_t literal8(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({byteOffset(), "..d", kPIC16RelocData});
            if (!externalName.empty()) relocations_.push_back({byteOffset(), externalName, kPIC16RelocData});
        } else if (stage_ == 2 && v > 0xFF) {
            fail("'" + mnemonic + "': literal 0x" + std::to_string(v) + " doesn't fit in a byte");
        }
        return static_cast<uint8_t>(v & 0xFF);
    }

    // GOTO/CALL's 11-bit code-space word-address field, the code-space
    // counterpart of fileAddr(), relocated against ".." via
    // kPIC16RelocGotoCall instead.
    uint16_t controlAddr(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({byteOffset(), "..", kPIC16RelocGotoCall});
            if (!externalName.empty()) relocations_.push_back({byteOffset(), externalName, kPIC16RelocGotoCall});
        } else if (stage_ == 2 && v > 0x7FF) {
            fail("'" + mnemonic + "': address " + std::to_string(v) +
                 " is beyond this target's direct 11-bit GOTO/CALL range (2048 words)");
        }
        return static_cast<uint16_t>(v & 0x7FF);
    }

    // f,d - ADDWF/ANDWF/COMF/DECF/DECFSZ/INCF/INCFSZ/IORWF/MOVF/RLF/
    // RRF/SUBWF/SWAPF/XORWF all share this exact "00 oooo d fffffff"
    // shape.
    void encodeFD(uint16_t base6, const std::vector<std::vector<Syllable>> &ops, const std::string &mnemonic) {
        uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 2), mnemonic);
        uint8_t d = destBit(operandAt(ops, 1, mnemonic, 2), mnemonic);
        emitWord(static_cast<uint16_t>(base6 | (d << 7) | f));
    }

    // f,b - BCF/BSF/BTFSC/BTFSS's shared "01 oobbb fffffff" shape.
    void encodeFB(uint16_t base6, const std::vector<std::vector<Syllable>> &ops, const std::string &mnemonic) {
        uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 2), mnemonic);
        uint8_t b = bitNum(operandAt(ops, 1, mnemonic, 2), mnemonic);
        emitWord(static_cast<uint16_t>(base6 | (b << 7) | f));
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        static const std::unordered_map<std::string, uint16_t> implied = {
            {"nop", 0x0000}, {"return", 0x0008}, {"retfie", 0x0009}, {"clrw", 0x0100},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emitWord(it->second);
            return;
        }

        if (mnemonic == "movwf" || mnemonic == "clrf") {
            uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emitWord(static_cast<uint16_t>((mnemonic == "movwf" ? 0x0080 : 0x0180) | f));
            return;
        }

        static const std::unordered_map<std::string, uint16_t> fdOps = {
            {"addwf", 0x0700}, {"andwf", 0x0500}, {"comf", 0x0900}, {"decf", 0x0300}, {"decfsz", 0x0B00},
            {"incf", 0x0A00},  {"incfsz", 0x0F00}, {"iorwf", 0x0400}, {"movf", 0x0800}, {"rlf", 0x0D00},
            {"rrf", 0x0C00},   {"subwf", 0x0200}, {"swapf", 0x0E00}, {"xorwf", 0x0600},
        };
        if (auto it = fdOps.find(mnemonic); it != fdOps.end()) {
            encodeFD(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> fbOps = {
            {"bcf", 0x1000}, {"bsf", 0x1400}, {"btfsc", 0x1800}, {"btfss", 0x1C00},
        };
        if (auto it = fbOps.find(mnemonic); it != fbOps.end()) {
            encodeFB(it->second, ops, mnemonic);
            return;
        }

        if (mnemonic == "movlw" || mnemonic == "retlw" || mnemonic == "addlw" || mnemonic == "sublw" ||
            mnemonic == "andlw" || mnemonic == "iorlw" || mnemonic == "xorlw") {
            static const std::unordered_map<std::string, uint16_t> litBase = {
                {"movlw", 0x3000}, {"retlw", 0x3400}, {"iorlw", 0x3800}, {"andlw", 0x3900},
                {"xorlw", 0x3A00}, {"sublw", 0x3C00}, {"addlw", 0x3E00},
            };
            uint8_t k = literal8(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emitWord(static_cast<uint16_t>(litBase.at(mnemonic) | k));
            return;
        }

        // MOVLW with the low/high byte of a code-space label's word
        // address, splitting a function's program-memory address across
        // two literal loads since no single instruction loads it as data.
        // An extern (cross-file) symbol gets a relocation instead of a
        // hard error, same as controlAddr()'s GOTO/CALL case - its field
        // is 0 pre-link (resolveSyllable() never resolves it locally), so
        // the relocation's additive patch has nothing to carry into.
        // Taking the address of a function defined in THIS SAME file
        // under separate compilation still isn't supported: that would
        // need the field's own non-zero pre-link byte plus this object's
        // base to add correctly across the independent lo/hi instruction
        // words, which isn't implemented, so it stays a loud compile
        // error instead of a silently wrong value.
        if (mnemonic == "movlwcl" || mnemonic == "movlwch") {
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t v = evalExpr(operandAt(ops, 0, mnemonic, 1), &externalName, &needsBaseShift);
            if (stage_ == 2 && needsBaseShift) {
                fail("'" + mnemonic + "': taking the address of a function defined in this same file isn't "
                     "supported under separate compilation on this target");
            }
            if (stage_ == 2 && !externalName.empty()) {
                relocations_.push_back({byteOffset(), externalName,
                                        mnemonic == "movlwcl" ? kPIC16RelocCodeLo : kPIC16RelocCodeHi});
            }
            uint8_t k = mnemonic == "movlwcl" ? static_cast<uint8_t>(v & 0xFF) : static_cast<uint8_t>((v >> 8) & 0xFF);
            emitWord(static_cast<uint16_t>(0x3000 | k));
            return;
        }

        if (mnemonic == "goto" || mnemonic == "call") {
            uint16_t k = controlAddr(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emitWord(static_cast<uint16_t>((mnemonic == "goto" ? 0x2800 : 0x2000) | k));
            return;
        }

        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint16_t> words_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    std::unordered_set<std::string> equateNames_;
    std::unordered_set<std::string> dataLabelNames_;
    uint32_t codeOffset_ = 0;
    uint32_t ramOffset_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Standard Intel HEX, the Microchip-programmer load format: each PIC16
// instruction word is two little-endian bytes at byte address
// `wordIndex*2`, packed 8 words (16 bytes) per data record.
std::string toIntelHex(const std::vector<uint16_t> &words) {
    std::ostringstream out;
    auto writeRecord = [&](uint16_t addr, uint8_t type, const std::vector<uint8_t> &data) {
        uint8_t sum = static_cast<uint8_t>(data.size()) + static_cast<uint8_t>(addr >> 8) +
                      static_cast<uint8_t>(addr & 0xFF) + type;
        for (uint8_t b : data) sum += b;
        uint8_t checksum = static_cast<uint8_t>((~sum) + 1);
        char buf[12];
        std::snprintf(buf, sizeof(buf), ":%02X%04X%02X", static_cast<unsigned>(data.size()), addr, type);
        out << buf;
        for (uint8_t b : data) {
            std::snprintf(buf, sizeof(buf), "%02X", b);
            out << buf;
        }
        std::snprintf(buf, sizeof(buf), "%02X", checksum);
        out << buf << "\n";
    };

    constexpr size_t kWordsPerRecord = 8;
    for (size_t i = 0; i < words.size(); i += kWordsPerRecord) {
        std::vector<uint8_t> data;
        for (size_t j = i; j < words.size() && j < i + kWordsPerRecord; j++) {
            data.push_back(static_cast<uint8_t>(words[j] & 0xFF));
            data.push_back(static_cast<uint8_t>((words[j] >> 8) & 0xFF));
        }
        writeRecord(static_cast<uint16_t>(i * 2), 0x00, data);
    }
    writeRecord(0, 0x01, {});
    return out.str();
}

// Builds the format-neutral ObjectFile a completed object-mode Assembler
// produced. A label in `a.equateNames()` is skipped (a plain compile-time
// constant, never looked up via GenericLinker's symbol table); one in
// `a.dataLabelNames()` is a data-space symbol with a byte-offset value;
// everything else is a code-space symbol needing word-to-byte scaling
// (`a.labels()` stores a word index, ObjSymbol::value is a byte offset).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kPIC16CoffMachine;
    obj.textBytes = a.bytes();
    obj.dataSize = a.dataSize();

    for (const auto &kv : a.labels()) {
        if (a.equateNames().count(kv.first)) continue;
        bool isData = a.dataLabelNames().count(kv.first) != 0;
        uint32_t value = isData ? kv.second : kv.second * 2;
        obj.symbols.push_back({kv.first, value, /*isDefined=*/true, isData});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            // An as-yet-unseen referenced name (including the reserved
            // pseudo-symbols ".."/"..d"), classified code or data by
            // which relocation kind references it.
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false, r.kind == kPIC16RelocData});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assemblePIC16(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    return toIntelHex(assembler.words());
}

ObjectFile assemblePIC16Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
