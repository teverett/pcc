#include "Z80Assembler.h"

#include <algorithm>
#include <cstdint>
#include <optional>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Z80Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Line shape as ../i8080/I8080Assembler.cpp's own - see
// that file's own comment - except an Operand here also remembers
// whether it was bracketed (`[expr]`), since that distinguishes two
// genuinely different real Z80 instructions (see Z80Assembler.h's
// own header comment).

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Operand {
    bool isMemory = false;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Org, Db, Dw, Instr } kind = None;
    std::vector<std::vector<Syllable>> exprList;  // Org (1) / Db (N) / Dw (N)
    std::string mnemonic;                          // Instr
    std::vector<Operand> operands;                 // Instr
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

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    Operand op;
    op.isMemory = ctx->LBRACKET() != nullptr;
    op.expr = exprToSyllables(ctx->expr());
    return op;
}

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        fail("'='-style equates aren't part of this target's dialect - every Z80 pseudo-register here is a plain "
             "memory label (see Z80Codegen.h), never a fixed compile-time equate");
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
    if (mnemonic == "db") {
        line.kind = Line::Db;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'db' takes plain byte values, not a bracketed memory operand");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }
    // `dw` - a full little-endian 16-bit data word, resolved exactly
    // like `ld hl, nn`'s own 16-bit immediate operand
    // (Z80Codegen.cpp's own `arrayElemText()`/`declareArray()` uses
    // this for a dispatch-table array element that names a function/
    // global directly: a symbol's own address is only known to the
    // assembler, once every label is bound, never to the codegen that
    // writes this directive out).
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dw' takes plain word values, not a bracketed memory operand");
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

// A single-syllable, unsigned, bare-identifier operand - the shape
// every register name always takes. Returns its lowercased text, or
// nullopt if the operand isn't that shape (a real symbol/number/
// signed-or-grouped expression instead), in which case the caller
// evaluates it as an ordinary expr.
std::optional<std::string> asBareIdent(const Operand &operand) {
    if (operand.expr.size() != 1) return std::nullopt;
    const Syllable &s = operand.expr[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// One pending fixup `assembleZ80Object()` (see Z80Assembler.h's own
// header comment) turns into a real ObjRelocation once assembly is
// done - `byteOffset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift - see
// Z80Assembler.h's own header comment).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
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
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emitByteOperand(e);
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emit16(Operand{/*isMemory=*/false, e});
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../x86_8086/I8086Assembler.cpp's own `evalExpr` exactly - see
    // Z80Assembler.h's own header comment on why *any* local address
    // reference, not just an external one, needs a relocation here
    // once `allowExternal_` is set.
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

    // --- byte emission --------------------------------------------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    void emitByteOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("'db' value doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    void emitImm8(const Operand &op) {
        uint32_t v = evalExpr(op.expr);
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // A 16-bit immediate/absolute operand, little-endian, no range
    // check (any symbol in this program's own 64KB address space is
    // valid here) - `ld hl, nn`'s own operand, and `dw`'s own data
    // word.
    void emit16(const Operand &op) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(op.expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // --- operand helpers --------------------------------------------

    const Operand &operandAt(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic,
                              size_t wantCount) {
        if (ops.size() != wantCount) {
            fail("'" + mnemonic + "' needs exactly " + std::to_string(wantCount) + " operand(s)");
        }
        return ops[i];
    }

    void requireBare(const Operand &op, const std::string &mnemonic) {
        if (op.isMemory) fail("'" + mnemonic + "': this operand isn't a memory reference - drop the brackets");
    }
    void requireMemory(const Operand &op, const std::string &mnemonic) {
        if (!op.isMemory) fail("'" + mnemonic + "': this operand must be a bracketed memory reference, e.g. [label]");
    }

    // The Z80's own r/(HL) register field, shared by LD r,r'/LD r,n/
    // INC r/DEC r/ADD A,r/CP r/etc, and by the CB-prefixed rotate
    // group: b=0,c=1,d=2,e=3,h=4,l=5,m=6 (meaning the real `(HL)`
    // operand - see Z80Assembler.h's own header comment on why that
    // one case stays a bare pseudo-register name instead of using
    // brackets),a=7.
    uint8_t reg8Code(const Operand &operand, const std::string &mnemonic) {
        requireBare(operand, mnemonic);
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register name (a/b/c/d/e/h/l/m), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {
            {"b", 0}, {"c", 1}, {"d", 2}, {"e", 3}, {"h", 4}, {"l", 5}, {"m", 6}, {"a", 7},
        };
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register (a/b/c/d/e/h/l/m)");
        return it->second;
    }

    // LD rr,nn/INC rr/DEC rr/ADD HL,rr/ADC HL,rr/SBC HL,rr's own
    // register-pair field: bc=0,de=1,hl=2,sp=3. Pairs are always
    // spelled with their real two-letter name (never abbreviated to
    // a single letter) - that's what keeps a pair operand
    // unambiguous against a same-letter single register (reg8Code's
    // own table) with no context-dependent guessing needed anywhere.
    uint8_t pairCodeBDHSP(const Operand &operand, const std::string &mnemonic) {
        requireBare(operand, mnemonic);
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register pair (bc/de/hl/sp), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"bc", 0}, {"de", 1}, {"hl", 2}, {"sp", 3}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register pair (bc/de/hl/sp)");
        return it->second;
    }

    // PUSH/POP's own register-pair field: bc=0,de=1,hl=2,af=3.
    uint8_t pairCodeBDHAF(const Operand &operand, const std::string &mnemonic) {
        requireBare(operand, mnemonic);
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs a register pair (bc/de/hl/af), not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"bc", 0}, {"de", 1}, {"hl", 2}, {"af", 3}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a register pair (bc/de/hl/af)");
        return it->second;
    }

    // `[bc]`/`[de]` - LDAX/STAX-equivalent: bc=0,de=1.
    uint8_t pairCodeBD(const Operand &operand, const std::string &mnemonic) {
        requireMemory(operand, mnemonic);
        auto name = asBareIdent(operand);
        if (!name) fail("'" + mnemonic + "' needs [bc] or [de], not an expression");
        static const std::unordered_map<std::string, uint8_t> table = {{"bc", 0}, {"de", 1}};
        auto it = table.find(*name);
        if (it == table.end()) fail("'" + mnemonic + "': '" + *name + "' isn't [bc] or [de]");
        return it->second;
    }

    // Requires `operand0` to literally be the bare identifier "a" -
    // the explicit accumulator Z80 syntax demands for the 8-bit
    // `ADC A,r`/`SBC A,r` forms (to disambiguate them from the 16-bit
    // `ADC HL,rr`/`SBC HL,rr` forms - see Z80Codegen.h's own header
    // comment).
    void requireA(const Operand &operand, const std::string &mnemonic) {
        requireBare(operand, mnemonic);
        auto name = asBareIdent(operand);
        if (!name || *name != "a") fail("'" + mnemonic + "' needs 'a' as its first operand");
    }

    // The full instruction vocabulary ../Z80Codegen.cpp ever emits,
    // encoded as real, standard Z80 opcodes (see Z80Assembler.h's own
    // header comment) - not every Z80 instruction exists here, only
    // the ones that backend needs.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"nop", 0x00}, {"rlca", 0x07}, {"rrca", 0x0F}, {"rla", 0x17}, {"rra", 0x1F}, {"cpl", 0x2F},
            {"scf", 0x37}, {"ccf", 0x3F},  {"ret", 0xC9},  {"ei", 0xFB},  {"di", 0xF3},  {"halt", 0x76},
        };
        if (mnemonic == "ex") {
            // Only ever `EX DE,HL` here - checked by requiring the
            // two bare operand names explicitly rather than giving
            // this its own mnemonic spelling, since that's the real
            // Z80 syntax.
            auto a = asBareIdent(operandAt(ops, 0, mnemonic, 2));
            auto b = asBareIdent(operandAt(ops, 1, mnemonic, 2));
            if (!a || !b || *a != "de" || *b != "hl") fail("'ex' is only supported as 'ex de, hl' (EX DE,HL)");
            emit8(0xEB);
            return;
        }
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }

        // LD rr,nn (immediate) vs LD rr,[nn] (direct memory load) vs
        // LD [nn],rr (direct memory store) vs LD r,r'/LD r,n (8-bit)
        // vs LD a,[b]/LD a,[d]/LD [b],a/LD [d],a (LDAX/STAX-
        // equivalent) - every real `LD` form this backend ever emits,
        // disambiguated by each operand's own bracket-ness and
        // identity, same as a real Z80 assembler's own grammar would.
        if (mnemonic == "ld") {
            const Operand &dst = operandAt(ops, 0, mnemonic, 2);
            const Operand &src = operandAt(ops, 1, mnemonic, 2);
            auto dstName = asBareIdent(dst);
            auto srcName = asBareIdent(src);

            // LD (HL direct) [bc],a / [de],a / a,[bc] / a,[de]
            if (dst.isMemory && dstName && (*dstName == "bc" || *dstName == "de")) {
                uint8_t rp = *dstName == "bc" ? 0 : 1;
                requireA(src, mnemonic);
                emit8(static_cast<uint8_t>(0x02 | (rp << 4)));  // STAX-equivalent
                return;
            }
            if (src.isMemory && srcName && (*srcName == "bc" || *srcName == "de")) {
                uint8_t rp = *srcName == "bc" ? 0 : 1;
                requireA(dst, mnemonic);
                emit8(static_cast<uint8_t>(0x0A | (rp << 4)));  // LDAX-equivalent
                return;
            }
            // LD hl,[nn] / LD [nn],hl / LD a,[nn] / LD [nn],a
            if (dstName && *dstName == "hl" && src.isMemory) {
                emit8(0x2A);
                emit16(src);
                return;
            }
            if (dst.isMemory && srcName && *srcName == "hl") {
                emit8(0x22);
                emit16(dst);
                return;
            }
            if (dstName && *dstName == "a" && src.isMemory) {
                emit8(0x3A);
                emit16(src);
                return;
            }
            if (dst.isMemory && srcName && *srcName == "a") {
                emit8(0x32);
                emit16(dst);
                return;
            }
            // LD bc,[nn]/LD de,[nn]/LD sp,[nn] and the reverse - the
            // Z80's own `ED`-prefixed extended direct loads (see
            // Z80Codegen.h's own header comment: the 8080 has no
            // equivalent for any pair but HL).
            if (dstName && src.isMemory) {
                static const std::unordered_map<std::string, uint8_t> edLoad = {
                    {"bc", 0x4B}, {"de", 0x5B}, {"sp", 0x7B}};
                if (auto it = edLoad.find(*dstName); it != edLoad.end()) {
                    emit8(0xED);
                    emit8(it->second);
                    emit16(src);
                    return;
                }
            }
            if (dst.isMemory && srcName) {
                static const std::unordered_map<std::string, uint8_t> edStore = {
                    {"bc", 0x43}, {"de", 0x53}, {"sp", 0x73}};
                if (auto it = edStore.find(*srcName); it != edStore.end()) {
                    emit8(0xED);
                    emit8(it->second);
                    emit16(dst);
                    return;
                }
            }
            // LD rr,nn (16-bit immediate) - only once every bracketed
            // form above has already been ruled out.
            if (dstName && !dst.isMemory && !src.isMemory) {
                static const std::unordered_map<std::string, uint8_t> table = {
                    {"bc", 0x01}, {"de", 0x11}, {"hl", 0x21}, {"sp", 0x31}};
                if (auto it = table.find(*dstName); it != table.end()) {
                    emit8(it->second);
                    emit16(src);
                    return;
                }
            }
            // LD r,r' / LD r,n (8-bit, including `m` for the real
            // `(HL)` operand - see reg8Code()'s own comment).
            uint8_t dstR = reg8Code(dst, mnemonic);
            if (!src.isMemory && asBareIdent(src)) {
                uint8_t srcR = reg8Code(src, mnemonic);
                if (dstR == 6 && srcR == 6) fail("'ld m,m' isn't valid - that opcode (0x76) is HALT, not a move");
                emit8(static_cast<uint8_t>(0x40 | (dstR << 3) | srcR));
                return;
            }
            emit8(static_cast<uint8_t>(0x06 | (dstR << 3)));
            emitImm8(src);
            return;
        }

        if (mnemonic == "inc" || mnemonic == "dec") {
            const Operand &op0 = operandAt(ops, 0, mnemonic, 1);
            auto name = asBareIdent(op0);
            static const std::unordered_map<std::string, uint8_t> pair = {
                {"bc", 0}, {"de", 1}, {"hl", 2}, {"sp", 3}};
            if (name && !op0.isMemory) {
                if (auto it = pair.find(*name); it != pair.end()) {
                    emit8(static_cast<uint8_t>((mnemonic == "inc" ? 0x03 : 0x0B) | (it->second << 4)));
                    return;
                }
            }
            uint8_t r = reg8Code(op0, mnemonic);
            emit8(static_cast<uint8_t>((mnemonic == "inc" ? 0x04 : 0x05) | (r << 3)));
            return;
        }
        if (mnemonic == "add" || mnemonic == "adc" || mnemonic == "sbc") {
            const Operand &op0 = operandAt(ops, 0, mnemonic, 2);
            const Operand &op1 = operandAt(ops, 1, mnemonic, 2);
            auto op0Name = asBareIdent(op0);
            if (op0Name && *op0Name == "hl") {  // ADD HL,rr / ADC HL,rr / SBC HL,rr
                uint8_t rp = pairCodeBDHSP(op1, mnemonic);
                uint8_t base = mnemonic == "add" ? 0x09 : (mnemonic == "adc" ? 0x4A : 0x42);
                if (mnemonic == "add") {
                    emit8(static_cast<uint8_t>(base | (rp << 4)));
                } else {
                    emit8(0xED);
                    emit8(static_cast<uint8_t>(base | (rp << 4)));
                }
                return;
            }
            requireA(op0, mnemonic);  // ADD A,r/n / ADC A,r/n / SBC A,r/n
            if (!op1.isMemory && asBareIdent(op1)) {
                uint8_t r = reg8Code(op1, mnemonic);
                uint8_t base = mnemonic == "add" ? 0x80 : (mnemonic == "adc" ? 0x88 : 0x98);
                emit8(static_cast<uint8_t>(base | r));
                return;
            }
            uint8_t base = mnemonic == "add" ? 0xC6 : (mnemonic == "adc" ? 0xCE : 0xDE);
            emit8(base);
            emitImm8(op1);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluReg = {
            {"sub", 0x90}, {"and", 0xA0}, {"xor", 0xA8}, {"or", 0xB0}, {"cp", 0xB8},
        };
        if (auto it = aluReg.find(mnemonic); it != aluReg.end()) {
            const Operand &op0 = operandAt(ops, 0, mnemonic, 1);
            if (!op0.isMemory && asBareIdent(op0)) {
                uint8_t r = reg8Code(op0, mnemonic);
                emit8(static_cast<uint8_t>(it->second | r));
                return;
            }
            static const std::unordered_map<std::string, uint8_t> aluImm = {
                {"sub", 0xD6}, {"and", 0xE6}, {"xor", 0xEE}, {"or", 0xF6}, {"cp", 0xFE},
            };
            emit8(aluImm.at(mnemonic));
            emitImm8(op0);
            return;
        }
        if (mnemonic == "rl" || mnemonic == "rr") {
            uint8_t r = reg8Code(operandAt(ops, 0, mnemonic, 1), mnemonic);
            emit8(0xCB);
            emit8(static_cast<uint8_t>((mnemonic == "rl" ? 0x10 : 0x18) | r));
            return;
        }
        if (mnemonic == "push" || mnemonic == "pop") {
            uint8_t rp = pairCodeBDHAF(operandAt(ops, 0, mnemonic, 1), mnemonic);
            uint8_t base = mnemonic == "push" ? 0xC5 : 0xC1;
            emit8(static_cast<uint8_t>(base | (rp << 4)));
            return;
        }
        if (mnemonic == "jp") {
            // "jp [hl]" - the real Z80 `JP (HL)` (opcode 0xE9, same
            // encoding as the 8080's `PCHL` - see I8080Assembler.h's
            // own comment): jumps to the address *in* HL, not the
            // value stored at that address, even though it's spelled
            // with this dialect's own "[expr]" bracket syntax (see
            // this file's own header comment) - the one place that
            // syntax names a jump target rather than a true memory
            // operand.
            if (ops.size() == 1 && ops[0].isMemory) {
                auto name = asBareIdent(ops[0]);
                if (!name || *name != "hl") fail("'jp [mem]' is only supported as 'jp [hl]' (JP (HL))");
                emit8(0xE9);
                return;
            }
            static const std::unordered_map<std::string, uint8_t> condJump = {
                {"nz", 0xC2}, {"z", 0xCA}, {"nc", 0xD2}, {"c", 0xDA},
                {"po", 0xE2}, {"pe", 0xEA}, {"p", 0xF2},  {"m", 0xFA},
            };
            if (ops.size() == 2) {
                auto cc = asBareIdent(operandAt(ops, 0, mnemonic, 2));
                if (!cc) fail("'jp cc,addr' needs a condition code");
                auto it = condJump.find(*cc);
                if (it == condJump.end()) fail("'" + *cc + "' isn't a jp condition code");
                emit8(it->second);
                emit16(operandAt(ops, 1, mnemonic, 2));
                return;
            }
            emit8(0xC3);
            emit16(operandAt(ops, 0, mnemonic, 1));
            return;
        }
        if (mnemonic == "call") {
            emit8(0xCD);
            emit16(operandAt(ops, 0, mnemonic, 1));
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
};

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see Z80Assembler.h's own `assembleZ80Object`
// comment, and ../x86_8086/I8086Assembler.cpp's own `writeCoff` for
// the original, COFF-only version this is ported from (identical
// algorithm, just returning the neutral struct directly instead of
// handing it to `writeCoffObject`).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kZ80CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry
    // too - add one, undefined, for any name that isn't already a
    // real local label (this also covers the reserved pseudo-symbol
    // ".." - a linker resolves that one specially, never by looking
    // it up here, but it still needs a table entry for the object-
    // format writer to point the relocation's own symbol index at).
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, kZ80RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleZ80(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleZ80Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
