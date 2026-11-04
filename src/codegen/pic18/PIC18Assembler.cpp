#include "PIC18Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("PIC18Assembler: " + msg); }

// --- Parsing - identical shape to ../pic16/PIC16Assembler.cpp's own
// Syllable/Line; see that file's own comment.

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
    std::vector<Syllable> rhs;
    std::string mnemonic;
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
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect - PIC18 file-register "
                               "access always names the register directly (optionally via FSR0/INDF0/POSTINC0), "
                               "never a bracketed symbol");
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

// One pending fixup `assemblePIC18Object()` (see PIC18Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - mirrors ../i8008/I8008Assembler.cpp's own `Relocation`
// (a `kind` field, not just one implied type, since this dialect also
// has more than one relocation kind).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

// Which of this object's own linked address spaces a `labels_` entry
// lives in, tracked alongside it so `buildObjectFile()` below can
// build the right kind of `ObjSymbol` for each one - see that
// function's own comment. `Equate` is a plain compile-time constant
// (`name = expr`, e.g. STATUS/FSR0L/.../this backend's own `ac`/`bc`/
// .../`SSTACK` pseudo-registers in object mode - see
// PIC18Codegen.cpp's own `generate()`), never linker-visible at all.
enum class LabelKind { Code, Data, Equate };

// --- Two-pass assembly ----------------------------------------------------
// Same code-space/data-space split as ../pic16/PIC16Assembler.cpp's
// own Assembler (see that file's own header comment) - the one real
// difference is that some instructions here (GOTO/CALL/MOVFF) are two
// words, not one, so `codeOffset_` doesn't always advance by exactly
// 1 the way every PIC16 instruction's own did.
//
// `allowExternal_` (constructor parameter) switches this same class
// between the whole-program dialect (`assemblePIC18()`, every label
// must resolve within this one compile) and the real separate-
// compilation dialect (`assemblePIC18Object()`, an unresolved symbol
// is deferred as a relocation instead of a hard error) - mirrors
// every other GenericLinker-based backend's own single-Assembler-
// class convention (see e.g. ../m68k/M68kAssembler.cpp's own
// `Assembler(bool allowExternal)`) rather than duplicating this whole
// class. `codeOffset_` is still counted in *words* either way (`k`'s
// own real hardware unit, and this class's own existing encoding
// logic already assumes that) - only `buildObjectFile()` below ever
// needs a *byte* offset (GenericLinker.cpp's own unit throughout),
// computed there by multiplying by two, never by changing what this
// class counts internally.
class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {}

    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint16_t> &words() const { return words_; }
    uint32_t dataSize() const { return ramOffset_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::unordered_map<std::string, LabelKind> &labelKinds() const { return labelKind_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }

   private:
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
                for (const auto &lbl : line.labels) {
                    labels_[lbl] = codeOffset_;
                    labelKind_[lbl] = LabelKind::Code;
                }
                return;
            case Line::Equate: {
                const std::string &name = line.labels.back();
                labels_[name] = evalExpr(line.rhs);
                labelKind_[name] = LabelKind::Equate;
                equateNames_.insert(name);
                return;
            }
            case Line::Org:
                for (const auto &lbl : line.labels) {
                    labels_[lbl] = codeOffset_;
                    labelKind_[lbl] = LabelKind::Code;
                }
                codeOffset_ = evalExpr(line.rhs);
                return;
            case Line::OrgRam:
                for (const auto &lbl : line.labels) {
                    labels_[lbl] = codeOffset_;
                    labelKind_[lbl] = LabelKind::Code;
                }
                ramOffset_ = evalExpr(line.rhs);
                return;
            case Line::Res: {
                for (const auto &lbl : line.labels) {
                    labels_[lbl] = ramOffset_;
                    labelKind_[lbl] = LabelKind::Data;
                }
                uint32_t count = evalExpr(line.rhs);
                ramOffset_ += count;
                // This per-object bound only ever reflects reality in
                // whole-program mode, where `ramOffset_` *is* the real
                // final Access Bank address: in object mode every
                // object's own data space gets re-based at link time
                // (see PIC18Assembler.h's own `kPIC18DataBaseAddress`
                // comment), so a single object alone staying under the
                // window proves nothing about the linked total -
                // PIC18LinkerFactory.cpp's own `patchDataAbs8` is the
                // real, authoritative check for that case.
                if (stage_ == 2 && !allowExternal_ && ramOffset_ > 0x60) {
                    fail("Access Bank GPR space exhausted (this target is restricted to the 96-byte 0x00-0x5F "
                         "window - see PIC18Codegen.h's own header comment) - program needs too many globals/"
                         "locals/temporaries for this target");
                }
                return;
            }
            case Line::Instr:
                for (const auto &lbl : line.labels) {
                    labels_[lbl] = codeOffset_;
                    labelKind_[lbl] = LabelKind::Code;
                }
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror ../m68k/M68kAssembler.cpp's
    // own `evalExpr` exactly - see PIC18Assembler.h's own header
    // comment on why *any* local address reference, not just an
    // external one, needs a relocation here once `allowExternal_` is
    // set. Which reserved pseudo-symbol (".." vs "..d") and which
    // relocation kind a caller ends up using for `needsBaseShift`/
    // `externalName` is entirely up to that caller (`controlAddr20`
    // for a CODE-space reference, `fileAddr`/`fileAddr12` for a DATA-
    // space one) - this function only ever reports *that* a real,
    // linker-visible label (as opposed to a plain number or a compile-
    // time `equateNames_` constant) was resolved, never which address
    // space it lives in, since the caller already knows that from its
    // own instruction-encoding context.
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

    uint8_t destBit(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        auto name = asBareIdent(operand);
        if (name && *name == "w") return 0;
        if (name && *name == "f") return 1;
        fail("'" + mnemonic + "': destination must be 'w' or 'f'");
    }

    // Every ordinary single-word instruction's `f` operand is always
    // Access Bank (the `a` bit is always 0 - see PIC18Codegen.h's own
    // header comment: nothing here ever touches banked GPR or uses a
    // non-zero BSR), so it only ever needs 8 bits - resolving a full
    // SFR address like STATUS (0xFD8) here correctly keeps only its
    // low byte (0xD8), exactly the real hardware's own Access Bank
    // aliasing of 0x60-0xFF onto 0xF60-0xFFF.
    //
    // In object mode, a real (non-equate) label resolved here is
    // always a DATA-space reference (see PIC18Assembler.h's own
    // header comment: this field is never used for anything else),
    // so a needed relocation always targets "..d"/`kPIC18RelocDataAbs8`
    // - this instruction's own current word (not yet emitted - `f`
    // always resolves before its own `emitWord` call) is exactly
    // where that single byte lives, at byte offset `codeOffset_ * 2`
    // (the word-to-byte conversion every CODE-space site needs too -
    // see `controlAddr20`'s own comment).
    uint8_t fileAddr(const std::vector<Syllable> &operand, const std::string &) {
        std::string externalName;
        bool needsShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsShift);
        emitDataReloc(needsShift, externalName);
        return static_cast<uint8_t>(v & 0xFF);
    }

    // MOVFF's own two operands are never Access-Bank-truncated - its
    // whole reason to exist is addressing any byte in the full 4096-
    // byte data space directly, bypassing BSR/Access Bank entirely -
    // so an SFR here needs its real full address (e.g. STATUS is
    // 0xFD8, not 0xD8).
    //
    // `wordDelta` (0 for `fs`, 1 for `fd` - see `encodeInstr`'s own
    // `movff` case) is needed because *both* operands are resolved
    // before *either* of MOVFF's own two words is emitted (unlike
    // `fileAddr` above, always resolved immediately before its own
    // one-word instruction) - without it, `fd`'s own relocation site
    // would be computed against `codeOffset_` before it's advanced
    // past `fs`'s own word at all, landing on the wrong byte.
    uint16_t fileAddr12(const std::vector<Syllable> &operand, const std::string &mnemonic, uint32_t wordDelta = 0) {
        std::string externalName;
        bool needsShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsShift);
        if (stage_ == 2 && v > 0xFFF) fail("'" + mnemonic + "': address " + std::to_string(v) +
                                            " doesn't fit this target's 12-bit data space");
        emitDataReloc(needsShift, externalName, wordDelta);
        return static_cast<uint16_t>(v & 0xFFF);
    }

    // Shared by `fileAddr`/`fileAddr12` above - pushes a
    // `kPIC18RelocDataAbs8` relocation (see PIC18Assembler.h's own
    // comment) against "..d" (a same-object DATA-space reference) or
    // `externalName` (a cross-object one), at this instruction's own
    // `wordDelta`'th word's own low byte - the one and only byte
    // either caller's own field ever occupies (see
    // `kPIC18RelocDataAbs8`'s own comment on why the field's own high
    // nibble, for MOVFF, never needs its own separate patch site).
    void emitDataReloc(bool needsShift, const std::string &externalName, uint32_t wordDelta = 0) {
        if (stage_ != 2 || !allowExternal_) return;
        if (!needsShift && externalName.empty()) return;
        uint32_t site = (codeOffset_ + wordDelta) * 2;
        if (needsShift) relocations_.push_back({site, "..d", kPIC18RelocDataAbs8});
        if (!externalName.empty()) relocations_.push_back({site, externalName, kPIC18RelocDataAbs8});
    }

    uint8_t bitNum(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        uint32_t v = evalExpr(operand);
        if (v > 7) fail("'" + mnemonic + "': bit number must be 0-7");
        return static_cast<uint8_t>(v);
    }

    uint8_t literal8(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        uint32_t v = evalExpr(operand);
        if (stage_ == 2 && v > 0xFF) fail("'" + mnemonic + "': literal 0x" + std::to_string(v) +
                                           " doesn't fit in a byte");
        return static_cast<uint8_t>(v & 0xFF);
    }

    // GOTO/CALL's own full 20-bit absolute word address - real
    // program memory on this target (16K words on the concrete
    // PIC18F2420 this backend targets - see PIC18Codegen.h's own
    // header comment) comfortably fits, so unlike PIC16's own direct-
    // 11-bit GOTO/CALL, no paging/range workaround is ever needed
    // here.
    //
    // In object mode, a real (non-equate) label resolved here is
    // always a CODE-space reference (every GOTO/CALL target is always
    // a bare function/control-flow label - see PIC18Assembler.h's own
    // header comment), so a needed relocation always targets
    // ".."/the three `kPIC18RelocGotoCall*` kinds, at this
    // instruction's own first word (not yet emitted - this is always
    // called before either of `encodeInstr`'s own two `emitWord`
    // calls for `goto`/`call`). Pushed high-nibble-first, then mid,
    // then low - see PIC18LinkerFactory.cpp's own `patchGotoCallHiNibble`
    // comment for why that order (not emission order) is what makes
    // the carry chain across all three patch sites correct.
    uint32_t controlAddr20(const std::vector<Syllable> &operand, const std::string &mnemonic) {
        std::string externalName;
        bool needsShift = false;
        uint32_t v = evalExpr(operand, &externalName, &needsShift);
        if (stage_ == 2 && v > 0xFFFFF) fail("'" + mnemonic + "': address " + std::to_string(v) +
                                              " is beyond this target's 20-bit GOTO/CALL range");
        if (stage_ == 2 && allowExternal_ && (needsShift || !externalName.empty())) {
            uint32_t site = codeOffset_ * 2;
            if (needsShift) {
                relocations_.push_back({site + 3, "..", kPIC18RelocGotoCallHiNibble});
                relocations_.push_back({site + 2, "..", kPIC18RelocGotoCallMid});
                relocations_.push_back({site, "..", kPIC18RelocGotoCallLo});
            }
            if (!externalName.empty()) {
                relocations_.push_back({site + 3, externalName, kPIC18RelocGotoCallHiNibble});
                relocations_.push_back({site + 2, externalName, kPIC18RelocGotoCallMid});
                relocations_.push_back({site, externalName, kPIC18RelocGotoCallLo});
            }
        }
        return v & 0xFFFFF;
    }

    void encodeFD(uint16_t base6, const std::vector<std::vector<Syllable>> &ops, const std::string &mnemonic) {
        uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 2), mnemonic);
        uint8_t d = destBit(operandAt(ops, 1, mnemonic, 2), mnemonic);
        emitWord(static_cast<uint16_t>(base6 | (d << 9) | f));
    }

    void encodeFB(uint16_t base4, const std::vector<std::vector<Syllable>> &ops, const std::string &mnemonic) {
        uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 2), mnemonic);
        uint8_t b = bitNum(operandAt(ops, 1, mnemonic, 2), mnemonic);
        emitWord(static_cast<uint16_t>(base4 | (b << 9) | f));
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        static const std::unordered_map<std::string, uint16_t> implied = {
            {"nop", 0x0000}, {"return", 0x0012},
            // Pushes a fresh (soon-to-be-overwritten) slot onto the
            // real hardware return stack - see PIC18Codegen.cpp's own
            // CallPtr case for why.
            {"push", 0x0005},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emitWord(it->second);
            return;
        }

        if (mnemonic == "movwf" || mnemonic == "clrf") {
            uint8_t f = fileAddr(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emitWord(static_cast<uint16_t>((mnemonic == "movwf" ? 0x6E00 : 0x6A00) | f));
            return;
        }

        static const std::unordered_map<std::string, uint16_t> fdOps = {
            {"addwf", 0x2400},  {"addwfc", 0x2000}, {"subwf", 0x5C00}, {"subwfb", 0x5800}, {"andwf", 0x1400},
            {"iorwf", 0x1000},  {"xorwf", 0x1800},  {"comf", 0x1C00},  {"movf", 0x5000},   {"decf", 0x0400},
            {"incf", 0x2800},   {"decfsz", 0x2C00}, {"incfsz", 0x3C00}, {"rlcf", 0x3400},  {"rrcf", 0x3000},
        };
        if (auto it = fdOps.find(mnemonic); it != fdOps.end()) {
            encodeFD(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> fbOps = {
            {"bsf", 0x8000}, {"bcf", 0x9000}, {"btfss", 0xA000}, {"btfsc", 0xB000},
        };
        if (auto it = fbOps.find(mnemonic); it != fbOps.end()) {
            encodeFB(it->second, ops, mnemonic);
            return;
        }

        if (mnemonic == "movlw" || mnemonic == "retlw" || mnemonic == "addlw" || mnemonic == "sublw" ||
            mnemonic == "andlw" || mnemonic == "iorlw" || mnemonic == "xorlw") {
            static const std::unordered_map<std::string, uint16_t> litBase = {
                {"sublw", 0x0800}, {"iorlw", 0x0900}, {"xorlw", 0x0A00}, {"andlw", 0x0B00},
                {"retlw", 0x0C00}, {"movlw", 0x0E00}, {"addlw", 0x0F00},
            };
            uint8_t k = literal8(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emitWord(static_cast<uint16_t>(litBase.at(mnemonic) | k));
            return;
        }

        if (mnemonic == "goto" || mnemonic == "call") {
            uint32_t k = controlAddr20(operandAt(ops, 0, mnemonic, 1), mnemonic);
            uint16_t first = mnemonic == "goto" ? 0xEF00 : 0xEC00;
            emitWord(static_cast<uint16_t>(first | (k & 0xFF)));
            emitWord(static_cast<uint16_t>(0xF000 | ((k >> 8) & 0xFFF)));
            return;
        }

        // MOVLW with the low 8 bits / next 8 bits of a CODE-space
        // label's own real *byte* address (`labels_` holds the same
        // word index `controlAddr20()` above uses for GOTO/CALL's own
        // field, doubled here to get a real byte address, matching
        // every other backend's own pointer representation) -
        // PIC18Codegen.cpp's own `genAddrOf()` uses this pair to get a
        // function's entry address into a plain 16-bit value: GOTO/
        // CALL always encode their own target directly in the
        // instruction word, so taking a function's address as *data*
        // has no single real instruction either - this splits it
        // across two literal loads instead, the same byte-at-a-time
        // idiom ../pic16/PIC16Assembler.cpp's own `movlwcl`/`movlwch`
        // use for the identical problem on that core. Object-mode
        // relocation of a split code-space address isn't implemented
        // (same reasoning as that file's own comment) - taking a
        // function's address under real separate compilation is a
        // loud compile error here instead of a silently wrong value.
        if (mnemonic == "movlwcl" || mnemonic == "movlwch") {
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t wordAddr = evalExpr(operandAt(ops, 0, mnemonic, 1), &externalName, &needsBaseShift);
            if (stage_ == 2 && (needsBaseShift || !externalName.empty())) {
                fail("'" + mnemonic + "': taking a function's address isn't supported under separate compilation "
                     "on this target");
            }
            uint32_t byteAddr = wordAddr * 2;
            uint8_t k = mnemonic == "movlwcl" ? static_cast<uint8_t>(byteAddr & 0xFF)
                                               : static_cast<uint8_t>((byteAddr >> 8) & 0xFF);
            emitWord(static_cast<uint16_t>(0x0E00 | k));
            return;
        }

        if (mnemonic == "movff") {
            uint16_t fs = fileAddr12(operandAt(ops, 0, mnemonic, 2), mnemonic, /*wordDelta=*/0);
            uint16_t fd = fileAddr12(operandAt(ops, 1, mnemonic, 2), mnemonic, /*wordDelta=*/1);
            emitWord(static_cast<uint16_t>(0xC000 | fs));
            emitWord(static_cast<uint16_t>(0xF000 | fd));
            return;
        }

        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint16_t> words_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_map<std::string, LabelKind> labelKind_;
    std::unordered_set<std::string> equateNames_;
    std::vector<Relocation> relocations_;
    uint32_t codeOffset_ = 0;
    uint32_t ramOffset_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Packs each 16-bit instruction word into two little-endian bytes -
// the real on-disk PIC18 program-memory shape (see PIC18Assembler.h's
// own header comment), and `assemblePIC18Object()`'s own
// `ObjectFile::textBytes` representation of it (unlike
// `assemblePIC18()`'s own Intel-HEX text container, built straight
// from `words()` by `toIntelHex()` below instead).
std::vector<uint8_t> packWordsToBytes(const std::vector<uint16_t> &words) {
    std::vector<uint8_t> out;
    out.reserve(words.size() * 2);
    for (uint16_t w : words) {
        out.push_back(static_cast<uint8_t>(w & 0xFF));
        out.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
    }
    return out;
}

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see PIC18Assembler.h's own
// `assemblePIC18Object` comment, and ../m68k/M68kAssembler.cpp's own
// `buildObjectFile` for the single-address-space template this is
// ported from; the one real difference is the CODE/DATA split itself
// (`LabelKind` - see that enum's own comment), which this function is
// the one place that actually resolves into two separate `ObjSymbol`
// shapes.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kPIC18CoffMachine;
    obj.textBytes = packWordsToBytes(a.words());
    obj.dataSize = a.dataSize();

    for (const auto &kv : a.labelKinds()) {
        const std::string &name = kv.first;
        if (kv.second == LabelKind::Equate) continue;  // a pure compile-time constant - never linker-visible
        uint32_t raw = a.labels().at(name);
        if (kv.second == LabelKind::Code) {
            obj.symbols.push_back({name, raw * 2, /*isDefined=*/true, /*isData=*/false});
        } else {
            obj.symbols.push_back({name, raw, /*isDefined=*/true, /*isData=*/true});
        }
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            // An as-yet-undefined forward/cross-object reference (or
            // one of the reserved pseudo-symbols, ".."/"..d", neither
            // of which is ever a real label - see GenericLinker.cpp's
            // own `merge()`, which always special-cases both before
            // ever consulting the real symbol table at all, so this
            // placeholder's own `isData` is never actually read for
            // either one) - `isData=false` matches every other
            // backend's own convention for an undefined placeholder.
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false, /*isData=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

// Standard Intel HEX - see ../pic16/PIC16Assembler.cpp's own
// toIntelHex() comment (identical convention: each program-memory
// word is two little-endian bytes at byte address `wordIndex*2`).
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

}  // namespace

std::string assemblePIC18(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    return toIntelHex(assembler.words());
}

ObjectFile assemblePIC18Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
