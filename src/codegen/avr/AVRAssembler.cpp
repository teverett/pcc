#include "AVRAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("AVRAssembler: " + msg); }

// --- Parsing, via the one shared grammar/RccAsm.g4 -----------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from the syllables of a bare expression - see AVRAssembler.h for the
// spelling of each kind. `expr` is the displacement (Disp), the immediate value (Imm) or the
// target/address (Label).
struct Operand {
    enum Kind { Reg, RegDeferred, AutoDec, AutoInc, Disp, Imm, Label } kind = Label;
    int reg = -1;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Ram, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Ram (1, the byte count)
    std::string mnemonic;                         // Instr
    std::vector<Operand> operands;                // Instr
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

// r0-r31: every AVR GPR, addressed only by number in this dialect - Y (r28:r29) is
// AVRCodegen.cpp's own frame pointer, X (r26:r27) and Z (r30:r31) its address-scratch pairs, all
// spelled out as plain registers rather than named aliases. Returns -1 if `name` isn't one.
int regNumber(const std::string &lower) {
    if (lower.size() < 2 || lower[0] != 'r') return -1;
    for (size_t i = 1; i < lower.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(lower[i]))) return -1;
    }
    int n = std::stoi(lower.substr(1));
    return (n >= 0 && n <= 31) ? n : -1;
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

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed operands aren't part of this target's dialect (see AVRAssembler.h)");
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;

    if (syls.size() == 1) {
        if (int r = bareReg(syls[0]); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
        if (int r = groupReg(syls[0]); r >= 0 && syls[0].sign != '+') {
            if (r != 26 && r != 28 && r != 30) {
                fail("only X (r26), Y (r28) or Z (r30) can be used for indirect addressing");
            }
            op.kind = syls[0].sign == '-' ? Operand::AutoDec : Operand::RegDeferred;
            op.reg = r;
            return op;
        }
    }
    if (syls.size() == 2 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol && toLower(syls[0].text) == "ai") {
        if (int r = groupReg(syls[1]); r >= 0 && syls[1].sign == 0) {
            if (r != 26 && r != 28 && r != 30) {
                fail("only X (r26), Y (r28) or Z (r30) can be used for indirect addressing");
            }
            op.kind = Operand::AutoInc;
            op.reg = r;
            return op;
        }
    }
    if (syls.size() >= 2 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol && toLower(syls[0].text) == "imm") {
        op.kind = Operand::Imm;
        op.expr.assign(syls.begin() + 1, syls.end());
        return op;
    }
    if (syls.size() >= 2) {
        const Syllable &last = syls.back();
        if (int r = groupReg(last); r >= 0 && last.sign == 0) {
            if (r != 28 && r != 30) fail("only Y (r28) or Z (r30) support a displacement");
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
    if (mnemonic == "ram") {
        line.kind = Line::Ram;
        if (instr->operand().size() != 1 || instr->operand(0)->LBRACKET()) {
            fail("'ram' needs exactly one operand (a byte count)");
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

// One pending fixup, turned into an ObjRelocation once assembly is done - `offset` into
// `bytes()`, `symbolName` the unresolved reference (or ".."/"..d" for a same-file/same-object
// base shift - see AVRAssembler.h).
struct Relocation {
    uint32_t offset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
class Assembler {
   public:
    // `allowExternal` is object mode (see AVRAssembler.h); `dataCursor_`'s starting point differs
    // only so a non-object assembly's `lds`/`sts` operands come out as real, final SRAM addresses
    // with no linker step to add the base in - object mode leaves that to the linker, same as
    // every relocatable `..`-style reference here.
    explicit Assembler(bool allowExternal)
        : allowExternal_(allowExternal), dataCursorStart_(allowExternal ? 0 : kAVRSramBaseAddress) {}

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
    uint32_t dataSize() const { return dataCursor_ - dataCursorStart_; }
    const std::unordered_map<std::string, uint32_t> &dataLabels() const { return dataLabels_; }
    uint32_t dataBase() const { return dataCursorStart_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        offset_ = 0;
        orgBase_ = 0;
        dataCursor_ = dataCursorStart_;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        if (line.kind != Line::Ram) {
            for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        }
        switch (line.kind) {
            case Line::None: return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Ram: {
                uint32_t count = evalExpr(line.exprList.at(0));
                for (const auto &lbl : line.labels) dataLabels_[lbl] = dataCursor_;
                dataCursor_ += count;
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

    static uint32_t parseNumber(const std::string &t) {
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
        }
        return static_cast<uint32_t>(std::stoul(t));
    }

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal, bool *wasLocalAddress) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
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

    // Data-space (SRAM) counterpart of evalExpr()/resolveSyllable(), resolving against
    // `dataLabels_` - used only by `lds`/`sts`'s absolute operand. A label this file itself
    // `ram`-declared resolves locally and still needs "..d" added at link time (this object's own
    // data-space base - see AVRAssembler.h); a label this file never declared is an `extern`
    // global truly defined in another object, deferred (in object mode) as a plain by-name
    // relocation the same way evalExpr()'s code-space symbols already are - the linker resolves
    // it to that other object's own already-absolute data address (GenericLinker::merge()'s own
    // `resolvedAddr()` for an `isData` symbol).
    uint32_t evalDataExpr(const std::vector<Syllable> &syls, std::string *externalName = nullptr,
                          bool *needsBaseShift = nullptr) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            bool wasExternal = false, wasLocal = false;
            uint32_t v = resolveDataSyllable(syl, &wasExternal, &wasLocal);
            if (wasExternal && externalName) *externalName = syl.text;
            if (wasLocal && needsBaseShift) *needsBaseShift = true;
            total = (syl.sign == '-') ? total - v : total + v;
        }
        return total;
    }

    uint32_t resolveDataSyllable(const Syllable &syl, bool *wasExternal, bool *wasLocal) {
        if (syl.kind == SylKind::Group) return evalDataExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
        auto it = dataLabels_.find(syl.text);
        if (it != dataLabels_.end()) {
            if (allowExternal_ && wasLocal) *wasLocal = true;
            return it->second;
        }
        if (stage_ == 2) {
            if (allowExternal_) {
                if (wasExternal) *wasExternal = true;
                return 0;
            }
            fail("internal: '" + syl.text +
                 "' has no SRAM address - a 'ram' directive should have been emitted for it before any code "
                 "referencing it");
        }
        return 0;
    }

    // --- byte emission (AVR is little-endian) ---------------------------
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }
    void emit16(uint16_t v) {
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // A plain instruction word with no relocatable field.
    void emitWord(uint16_t w) { emit16(w); }

    // `lds`/`sts`'s second word: always SRAM-space, relocated against "..d" in object mode (see
    // evalDataExpr()'s own comment).
    void emitDataAddress(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalDataExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..d", kAVRRelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kAVRRelocAbs16});
        }
        emit16(static_cast<uint16_t>(v));
    }

    // `call`/`jmp`'s second word: a flash *word* address (byte address / 2). `kAVRRelocCallWord`
    // halves the final linked value itself, so the field this emits pre-link is the local
    // word-offset-from-this-file's-org-0, i.e. exactly `v/2` - ordinary addition at link time
    // then reproduces `(v + base)/2` since every symbol here is word-aligned.
    void emitCallTarget(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kAVRRelocCallWord});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kAVRRelocCallWord});
        }
        emit16(static_cast<uint16_t>((v / 2) & 0xFFFF));
    }

    // `ldiw`/`ldiwc`: two real `ldi` words loading a relocatable 16-bit address into a register
    // pair (`lo`, `lo+1`) - the only way this dialect has to materialize a label's address as a
    // runtime value, since `ldi` itself only takes a plain byte. `ldiw` resolves against the
    // SRAM/data-space table (a `&global`/`&array[i]` value); `ldiwc` resolves against the plain
    // flash/code-space table at its plain *byte* address, un-halved - AVRCodegen.cpp's own
    // CallPtr runtime-halves it into a word address with `lsr`/`ror` just before `icall`.
    void encodeLdiw(const std::vector<Operand> &ops, bool codeSpace, const std::string &mnemonic) {
        if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Label) {
            fail("'" + mnemonic + "' takes a register and a label");
        }
        int lo = ops[0].reg;
        if (lo < 16 || lo > 30) fail("'" + mnemonic + "': register must be r16-r30");
        uint32_t v;
        uint32_t ldiwOffset = offset_;  // the first `ldi` word's own offset - see kAVRRelocLdiw16
        if (codeSpace) {
            std::string externalName;
            bool needsBaseShift = false;
            v = evalExpr(ops[1].expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({ldiwOffset, "..", kAVRRelocLdiw16});
                if (!externalName.empty()) relocations_.push_back({ldiwOffset, externalName, kAVRRelocLdiw16});
            }
        } else {
            std::string externalName;
            bool needsBaseShift = false;
            v = evalDataExpr(ops[1].expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({ldiwOffset, "..d", kAVRRelocLdiw16});
                if (!externalName.empty()) relocations_.push_back({ldiwOffset, externalName, kAVRRelocLdiw16});
            }
        }
        emitWord(static_cast<uint16_t>(
            0xE000 | (((v >> 4) & 0xF) << 8) | (((lo - 16) & 0xF) << 4) | (v & 0xF)));
        emitWord(static_cast<uint16_t>(
            0xE000 | ((((v >> 8) >> 4) & 0xF) << 8) | (((lo + 1 - 16) & 0xF) << 4) | ((v >> 8) & 0xF)));
    }

    static int regField(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "': expected a register operand");
        return op.reg;
    }

    // Accepts an explicit `imm V` operand or a bare numeric literal - every mnemonic that takes
    // one of these (`ldi`/`cpi`/`subi`/`sbci`/`adiw`/`sbiw`/`in`/`out`) never takes a register in
    // this same operand position, so there's no ambiguity to the `imm` prefix resolve the way
    // VaxAssembler.cpp's dialect needs it.
    uint32_t immField(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Imm && op.kind != Operand::Label) {
            fail("'" + mnemonic + "': expected an immediate operand");
        }
        return evalExpr(op.expr);
    }

    // Every register-register ALU op (`add`/`adc`/.../`mul`): 0000/0001/0010/1001-family base
    // plus `0010 1 1rd dddd rrrr`-shaped register fields.
    static uint16_t rrWord(uint16_t base, int d, int r) {
        return static_cast<uint16_t>(base | (((r >> 4) & 1) << 9) | (((d >> 4) & 1) << 8) | ((d & 0xF) << 4) |
                                       (r & 0xF));
    }
    // A single-register op (`com`/`neg`/.../`push`/`pop`): `1001 00xd dddd xxxx`-shaped.
    static uint16_t dWord(uint16_t base, int d) {
        return static_cast<uint16_t>(base | (((d >> 4) & 1) << 8) | ((d & 0xF) << 4));
    }

    void encodeRR(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 2) fail("'" + mnemonic + "' takes two register operands");
        emitWord(rrWord(base, regField(ops[0], mnemonic), regField(ops[1], mnemonic)));
    }
    void encodeD(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 1) fail("'" + mnemonic + "' takes one register operand");
        emitWord(dWord(base, regField(ops[0], mnemonic)));
    }

    // `ldi`/`cpi` - d must be r16-r31 (the real hardware restriction) and K is a plain byte.
    void encodeKD(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 2) fail("'" + mnemonic + "' takes a register and an 'imm' operand");
        int d = regField(ops[0], mnemonic);
        if (d < 16 || d > 31) fail("'" + mnemonic + "': destination register must be r16-r31");
        uint32_t k = immField(ops[1], mnemonic);
        if (stage_ == 2 && k > 0xFF) fail("'" + mnemonic + "': immediate doesn't fit in a byte");
        emitWord(static_cast<uint16_t>(base | (((k >> 4) & 0xF) << 8) | (((d - 16) & 0xF) << 4) | (k & 0xF)));
    }

    // `adiw`/`sbiw` - d must be r24/r26/r28/r30 and K is 0-63.
    void encodeAdiw(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 2) fail("'" + mnemonic + "' takes a register and an 'imm' operand");
        int d = regField(ops[0], mnemonic);
        if (d != 24 && d != 26 && d != 28 && d != 30) fail("'" + mnemonic + "': register must be r24/r26/r28/r30");
        uint32_t k = immField(ops[1], mnemonic);
        if (stage_ == 2 && k > 63) fail("'" + mnemonic + "': immediate must be 0-63");
        int dd = (d - 24) >> 1;
        emitWord(static_cast<uint16_t>(base | (((k >> 4) & 0x3) << 6) | ((dd & 0x3) << 4) | (k & 0xF)));
    }

    // Splits a 6-bit LDD/STD displacement into its non-contiguous opcode positions:
    // `10q0 qq0d dddd 1qqq` - q5 alone at bit13, q4:q3 at bits11:10, q2:q0 at bits2:0.
    static uint16_t qBits(uint32_t q) {
        return static_cast<uint16_t>((((q >> 5) & 1) << 13) | (((q >> 3) & 3) << 10) | (q & 0x7));
    }

    // `ld`: `Rd, X`/`X+`/`-X`/`Y`/`Y+`/`-Y`/`Z`/`Z+`/`-Z`/`Y+q`/`Z+q`.
    void encodeLd(const std::vector<Operand> &ops) {
        if (ops.size() != 2 || ops[0].kind != Operand::Reg) fail("'ld' takes a register and an addressing operand");
        int d = ops[0].reg;
        const Operand &a = ops[1];
        uint16_t base;
        switch (a.kind) {
            case Operand::RegDeferred: base = a.reg == 26 ? 0x900C : a.reg == 28 ? 0x8008 : 0x8000; break;
            case Operand::AutoInc: base = a.reg == 26 ? 0x900D : a.reg == 28 ? 0x9009 : 0x9001; break;
            case Operand::AutoDec: base = a.reg == 26 ? 0x900E : a.reg == 28 ? 0x900A : 0x9002; break;
            case Operand::Disp: {
                uint32_t q = evalExpr(a.expr);
                if (stage_ == 2 && q > 63) fail("'ld': displacement must be 0-63");
                base = static_cast<uint16_t>((a.reg == 28 ? 0x8008 : 0x8000) | qBits(q));
                break;
            }
            default: fail("'ld': second operand must be an X/Y/Z addressing mode");
        }
        emitWord(dWord(base, d));
    }

    // `st`: the addressing operand first, the source register second (real hardware order).
    void encodeSt(const std::vector<Operand> &ops) {
        if (ops.size() != 2 || ops[1].kind != Operand::Reg) fail("'st' takes an addressing operand and a register");
        int r = ops[1].reg;
        const Operand &a = ops[0];
        uint16_t base;
        switch (a.kind) {
            case Operand::RegDeferred: base = a.reg == 26 ? 0x920C : a.reg == 28 ? 0x8208 : 0x8200; break;
            case Operand::AutoInc: base = a.reg == 26 ? 0x920D : a.reg == 28 ? 0x9209 : 0x9201; break;
            case Operand::AutoDec: base = a.reg == 26 ? 0x920E : a.reg == 28 ? 0x920A : 0x9202; break;
            case Operand::Disp: {
                uint32_t q = evalExpr(a.expr);
                if (stage_ == 2 && q > 63) fail("'st': displacement must be 0-63");
                base = static_cast<uint16_t>((a.reg == 28 ? 0x8208 : 0x8200) | qBits(q));
                break;
            }
            default: fail("'st': first operand must be an X/Y/Z addressing mode");
        }
        emitWord(dWord(base, r));
    }

    void encodeIn(const std::vector<Operand> &ops) {
        if (ops.size() != 2) fail("'in' takes a register and an 'imm' operand");
        int d = regField(ops[0], "in");
        uint32_t a = immField(ops[1], "in");
        if (stage_ == 2 && a > 63) fail("'in': I/O address must be 0-63");
        emitWord(static_cast<uint16_t>(0xB000 | (((a >> 4) & 0x3) << 9) | (((d >> 4) & 1) << 8) | ((d & 0xF) << 4) |
                                         (a & 0xF)));
    }
    void encodeOut(const std::vector<Operand> &ops) {
        if (ops.size() != 2) fail("'out' takes an 'imm' operand and a register");
        uint32_t a = immField(ops[0], "out");
        int r = regField(ops[1], "out");
        if (stage_ == 2 && a > 63) fail("'out': I/O address must be 0-63");
        emitWord(static_cast<uint16_t>(0xB800 | (((a >> 4) & 0x3) << 9) | (((r >> 4) & 1) << 8) | ((r & 0xF) << 4) |
                                         (a & 0xF)));
    }

    void encodeMovw(const std::vector<Operand> &ops) {
        if (ops.size() != 2) fail("'movw' takes two register operands");
        int d = regField(ops[0], "movw"), r = regField(ops[1], "movw");
        if (d % 2 != 0 || r % 2 != 0) fail("'movw': both registers must be even-numbered");
        emitWord(static_cast<uint16_t>(0x0100 | ((d >> 1) << 4) | (r >> 1)));
    }

    // `rjmp`/`rcall`: a 12-bit signed word displacement, PC-relative to the word *after* this one
    // (`PC <- PC + k + 1`) - always same-file (a real target can't reach past a 12-bit field
    // reliably anyway; AVRCodegen.cpp only ever uses these within one function).
    void encodeRelWord12(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 1 || ops[0].kind != Operand::Label) fail("'" + mnemonic + "' takes one label operand");
        std::string externalName;
        uint32_t targetAddr = evalExpr(ops[0].expr, &externalName);
        if (stage_ == 2 && !externalName.empty()) {
            fail("'" + mnemonic + "': target '" + externalName + "' isn't defined in this file");
        }
        int64_t k = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_ + 2)) / 2;
        if (stage_ == 2 && (k < -2048 || k > 2047)) fail("'" + mnemonic + "': target out of 12-bit word range");
        emitWord(static_cast<uint16_t>(base | (static_cast<uint32_t>(k) & 0xFFF)));
    }

    // The six short conditional branches AVRCodegen.cpp's `safeBranch()` composes into an
    // always-in-range idiom: `1111 cc kkkkkkk sss` (c=00/clear base 0x0400 unset selects "branch
    // if set", 0x0400 set selects "branch if clear"; s is the tested SREG bit).
    void encodeBranch(bool branchIfClear, int s, const std::vector<Operand> &ops, const std::string &mnemonic) {
        if (ops.size() != 1 || ops[0].kind != Operand::Label) fail("'" + mnemonic + "' takes one label operand");
        std::string externalName;
        uint32_t targetAddr = evalExpr(ops[0].expr, &externalName);
        if (stage_ == 2 && !externalName.empty()) {
            fail("'" + mnemonic + "': target '" + externalName + "' isn't defined in this file");
        }
        int64_t k = (static_cast<int64_t>(targetAddr) - static_cast<int64_t>(orgBase_ + offset_ + 2)) / 2;
        if (stage_ == 2 && (k < -64 || k > 63)) fail("'" + mnemonic + "': target out of 7-bit word range");
        uint16_t word =
            static_cast<uint16_t>(0xF000 | (branchIfClear ? 0x0400 : 0) | ((static_cast<uint32_t>(k) & 0x7F) << 3) |
                                    (s & 0x7));
        emitWord(word);
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint16_t> rr = {
            {"add", 0x0C00}, {"adc", 0x1C00}, {"sub", 0x1800}, {"sbc", 0x0800}, {"and", 0x2000},
            {"or", 0x2800},  {"eor", 0x2400}, {"mov", 0x2C00}, {"cp", 0x1400},  {"cpc", 0x0400},
            {"mul", 0x9C00},
        };
        if (auto it = rr.find(mnemonic); it != rr.end()) {
            encodeRR(it->second, ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint16_t> d1 = {
            {"com", 0x9400 | 0x0}, {"neg", 0x9400 | 0x1}, {"inc", 0x9400 | 0x3}, {"asr", 0x9400 | 0x5},
            {"lsr", 0x9400 | 0x6}, {"ror", 0x9400 | 0x7}, {"dec", 0x9400 | 0xA}, {"push", 0x9200 | 0xF},
            {"pop", 0x9000 | 0xF},
        };
        if (auto it = d1.find(mnemonic); it != d1.end()) {
            encodeD(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "ldi") { encodeKD(0xE000, ops, mnemonic); return; }
        if (mnemonic == "cpi") { encodeKD(0x3000, ops, mnemonic); return; }
        if (mnemonic == "subi") { encodeKD(0x5000, ops, mnemonic); return; }
        if (mnemonic == "sbci") { encodeKD(0x4000, ops, mnemonic); return; }
        if (mnemonic == "adiw") { encodeAdiw(0x9600, ops, mnemonic); return; }
        if (mnemonic == "sbiw") { encodeAdiw(0x9700, ops, mnemonic); return; }
        if (mnemonic == "in") { encodeIn(ops); return; }
        if (mnemonic == "out") { encodeOut(ops); return; }
        if (mnemonic == "movw") { encodeMovw(ops); return; }
        if (mnemonic == "ld" || mnemonic == "ldd") { encodeLd(ops); return; }
        if (mnemonic == "st" || mnemonic == "std") { encodeSt(ops); return; }
        if (mnemonic == "lds") {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Label) {
                fail("'lds' takes a register and a data-space label");
            }
            emitWord(dWord(0x9000, ops[0].reg));
            emitDataAddress(ops[1].expr);
            return;
        }
        if (mnemonic == "sts") {
            if (ops.size() != 2 || ops[0].kind != Operand::Label || ops[1].kind != Operand::Reg) {
                fail("'sts' takes a data-space label and a register");
            }
            emitWord(dWord(0x9200, ops[1].reg));
            emitDataAddress(ops[0].expr);
            return;
        }
        if (mnemonic == "call") {
            if (ops.size() != 1 || ops[0].kind != Operand::Label) fail("'call' takes one label operand");
            emitWord(0x940E);
            emitCallTarget(ops[0].expr);
            return;
        }
        if (mnemonic == "jmp") {
            if (ops.size() != 1 || ops[0].kind != Operand::Label) fail("'jmp' takes one label operand");
            emitWord(0x940C);
            emitCallTarget(ops[0].expr);
            return;
        }
        if (mnemonic == "rjmp") { encodeRelWord12(0xC000, ops, mnemonic); return; }
        if (mnemonic == "rcall") { encodeRelWord12(0xD000, ops, mnemonic); return; }
        if (mnemonic == "ret") {
            if (!ops.empty()) fail("'ret' takes no operand");
            emitWord(0x9508);
            return;
        }
        if (mnemonic == "icall") {
            if (!ops.empty()) fail("'icall' takes no operand");
            emitWord(0x9509);
            return;
        }
        if (mnemonic == "ijmp") {
            if (!ops.empty()) fail("'ijmp' takes no operand");
            emitWord(0x9409);
            return;
        }
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emitWord(0x0000);
            return;
        }
        if (mnemonic == "ldiw") { encodeLdiw(ops, /*codeSpace=*/false, mnemonic); return; }
        if (mnemonic == "ldiwc") { encodeLdiw(ops, /*codeSpace=*/true, mnemonic); return; }
        static const std::unordered_map<std::string, std::pair<bool, int>> branches = {
            {"breq", {false, 1}}, {"brne", {true, 1}}, {"brge", {true, 4}},
            {"brlt", {false, 4}}, {"brlo", {false, 0}}, {"brsh", {true, 0}},
        };
        if (auto it = branches.find(mnemonic); it != branches.end()) {
            encodeBranch(it->second.first, it->second.second, ops, mnemonic);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;

    std::unordered_map<std::string, uint32_t> dataLabels_;
    uint32_t dataCursorStart_;
    uint32_t dataCursor_ = 0;
};

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kAVRCoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());
    obj.dataSize = a.dataSize();

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    for (const auto &kv : a.dataLabels()) {
        obj.symbols.push_back({kv.first, kv.second - a.dataBase(), /*isDefined=*/true, /*isData=*/true});
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

std::string assembleAVR(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleAVRObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
