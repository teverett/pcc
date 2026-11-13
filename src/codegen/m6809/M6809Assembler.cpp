#include "M6809Assembler.h"

#include <algorithm>
#include <cstdint>
#include <optional>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("M6809Assembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 -----------------------
// An Operand also remembers whether it was bracketed (`[expr]`), since
// that distinguishes extended addressing from an immediate value (see
// M6809Assembler.h).

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

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    Operand op;
    op.isMemory = ctx->LBRACKET() != nullptr;
    op.expr = exprToSyllables(ctx->expr());
    return op;
}

Line buildLine(RccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (dynamic_cast<RccAsmParser::AssignStmtContext *>(stmt)) {
        fail("'='-style equates aren't part of this target's dialect - every 6809 pseudo-register here is a plain "
             "memory label (see M6809Codegen.h), never a fixed compile-time equate");
    }
    if (dynamic_cast<RccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
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
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'db' takes plain byte values, not a bracketed memory operand");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }
    // `dw` emits a big-endian 16-bit data word, matching real 6809 byte order (unlike the
    // little-endian Z80/8080 dialects' own `dw`).
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

// A single-syllable, unsigned, bare-identifier operand - the shape a
// register name takes. Returns its lowercased text, or nullopt otherwise
// (caller then evaluates it as an ordinary expr).
std::optional<std::string> asBareIdent(const Operand &operand) {
    if (operand.expr.size() != 1) return std::nullopt;
    const Syllable &s = operand.expr[0];
    if (s.sign != 0 || s.kind != SylKind::Symbol) return std::nullopt;
    return toLower(s.text);
}

// A pending fixup, turned into a real ObjRelocation once assembly is done;
// `symbolName` is the unresolved reference, or ".." for a same-file
// address's base-shift.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
};

// --- Indexed addressing: 8-bit-offset form only (see M6809Assembler.h) --
// Register-select bits, already placed at postbyte bits 6:5, with the
// 8-bit-offset mode's own base bits (0x88) folded in.
uint8_t idxPostbyteBase(const std::string &reg, const std::string &mnemonic) {
    static const std::unordered_map<std::string, uint8_t> table = {
        {"x", 0x88}, {"y", 0xA8}, {"u", 0xC8}, {"s", 0xE8},
    };
    auto it = table.find(reg);
    if (it == table.end()) fail("'" + mnemonic + "': '" + reg + "' isn't an indexed-addressing register (x/y/u/s)");
    return it->second;
}

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

    // `externalName`/`needsBaseShift` report an unresolved reference and a
    // local-address base-shift respectively, once `allowExternal_` is set.
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

    // A signed 8-bit indexed-addressing offset - the only indexed form
    // this backend supports (see M6809Assembler.h). `evalExpr` computes
    // mod 2^32, so a negative offset (e.g. "-12,s") shows up as a value
    // near 0xFFFFFFFF; in range iff it's 0..127 or 0xFFFFFF80..0xFFFFFFFF.
    void emitIndexOffset8(const Operand &op) {
        uint32_t v = evalExpr(op.expr);
        bool inRange = v <= 127 || v >= 0xFFFFFF80u;
        if (stage_ == 2 && !inRange) {
            fail("indexed-addressing offset doesn't fit a signed byte (-128..127) - only the 8-bit-offset indexed "
                 "form is supported");
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // A big-endian 16-bit immediate/extended-address operand - real 6809
    // byte order, MSB first (unlike the Z80/8080 dialects' little-endian
    // emit16). No range check (used for both extended addresses and
    // 16-bit immediates alike, same as Z80Assembler.cpp's own emit16).
    void emit16(const Operand &op) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(op.expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // A big-endian 16-bit PC-relative displacement for a long branch,
    // relative to the address right after the displacement field itself
    // (real 6809 semantics) - computed directly rather than via a
    // relocation, since every branch target is a local label within the
    // same function/object.
    void emitRel16(const std::vector<Syllable> &targetExpr) {
        uint32_t target = evalExpr(targetExpr);
        int32_t rel = static_cast<int32_t>(target) - static_cast<int32_t>(offset_ + 2);
        uint32_t v = static_cast<uint32_t>(rel);
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
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

    // --- addressing-mode encoding, shared by load/store/alu/compare -----

    // Register-select bits for LEAX/LEAY/LEAS/LEAU and the indexed form of
    // every other instruction below: x/y/u/s named as the *second*
    // operand of a two-operand `<offset>,<reg>` form.
    void encodeIndexedOperands(const std::vector<Operand> &ops, const std::string &mnemonic, uint8_t baseOpcode,
                                bool prefixed) {
        requireBare(ops[1], mnemonic);
        auto regName = asBareIdent(ops[1]);
        if (!regName) fail("'" + mnemonic + "' needs a register name (x/y/u/s) after the offset");
        uint8_t postbyte = idxPostbyteBase(*regName, mnemonic);
        if (prefixed) emit8(0x10);
        emit8(baseOpcode);
        emit8(postbyte);
        emitIndexOffset8(ops[0]);
    }

    struct LoadStoreOp {
        bool prefixed;
        uint8_t immOp;  // 0 => no immediate form (store mnemonics)
        uint8_t extOp;
        uint8_t idxOp;
        bool imm16;
    };

    void encodeLoadStore(const std::string &mnemonic, const std::vector<Operand> &ops, const LoadStoreOp &op) {
        if (ops.size() == 2) {
            encodeIndexedOperands(ops, mnemonic, op.idxOp, op.prefixed);
            return;
        }
        const Operand &only = operandAt(ops, 0, mnemonic, 1);
        if (only.isMemory) {
            if (op.prefixed) emit8(0x10);
            emit8(op.extOp);
            emit16(only);
            return;
        }
        if (op.immOp == 0) {
            fail("'" + mnemonic + "' needs a memory destination ([label] or offset,reg) - it has no immediate form");
        }
        if (op.prefixed) emit8(0x10);
        emit8(op.immOp);
        if (op.imm16) {
            emit16(only);
        } else {
            emitImm8(only);
        }
    }

    struct AluOp {
        bool prefixed;
        uint8_t immOp;
        uint8_t extOp;
        uint8_t idxOp;
        bool imm16;
    };

    void encodeAlu(const std::string &mnemonic, const std::vector<Operand> &ops, const AluOp &op) {
        if (ops.size() == 2) {
            encodeIndexedOperands(ops, mnemonic, op.idxOp, op.prefixed);
            return;
        }
        const Operand &only = operandAt(ops, 0, mnemonic, 1);
        if (only.isMemory) {
            if (op.prefixed) emit8(0x10);
            emit8(op.extOp);
            emit16(only);
            return;
        }
        if (op.prefixed) emit8(0x10);
        emit8(op.immOp);
        if (op.imm16) {
            emit16(only);
        } else {
            emitImm8(only);
        }
    }

    // Encodes only the 6809 instructions M6809Codegen.cpp emits, not the full ISA.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"rts", 0x39},  {"mul", 0x3D},  {"sex", 0x1D},  {"clra", 0x4F}, {"clrb", 0x5F},
            {"coma", 0x43}, {"comb", 0x53}, {"nega", 0x40}, {"negb", 0x50}, {"inca", 0x4C},
            {"incb", 0x5C}, {"deca", 0x4A}, {"decb", 0x5A}, {"asla", 0x48}, {"aslb", 0x58},
            {"asra", 0x47}, {"asrb", 0x57}, {"lsra", 0x44}, {"lsrb", 0x54}, {"rola", 0x49},
            {"rolb", 0x59}, {"rora", 0x46}, {"rorb", 0x56},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }

        static const std::unordered_map<std::string, LoadStoreOp> loadStore = {
            {"lda", {false, 0x86, 0xB6, 0xA6, false}},
            {"sta", {false, 0x00, 0xB7, 0xA7, false}},
            {"ldb", {false, 0xC6, 0xF6, 0xE6, false}},
            {"stb", {false, 0x00, 0xF7, 0xE7, false}},
            {"ldd", {false, 0xCC, 0xFC, 0xEC, true}},
            {"std", {false, 0x00, 0xFD, 0xED, true}},
            {"ldx", {false, 0x8E, 0xBE, 0xAE, true}},
            {"stx", {false, 0x00, 0xBF, 0xAF, true}},
            {"ldy", {true, 0x8E, 0xBE, 0xAE, true}},
            {"sty", {true, 0x00, 0xBF, 0xAF, true}},
            {"lds", {true, 0xCE, 0xFE, 0xEE, true}},
            {"sts", {true, 0x00, 0xFF, 0xEF, true}},
            {"ldu", {false, 0xCE, 0xFE, 0xEE, true}},
            {"stu", {false, 0x00, 0xFF, 0xEF, true}},
        };
        if (auto it = loadStore.find(mnemonic); it != loadStore.end()) {
            encodeLoadStore(mnemonic, ops, it->second);
            return;
        }

        static const std::unordered_map<std::string, AluOp> alu = {
            {"adda", {false, 0x8B, 0xBB, 0xAB, false}},
            {"addb", {false, 0xCB, 0xFB, 0xEB, false}},
            {"addd", {false, 0xC3, 0xF3, 0xE3, true}},
            // ADCA/ADCB (add-with-carry, 8-bit only - no 16-bit ADCD exists on
            // real 6809 hardware) let a multi-word (long/long long) add chain
            // a carry up from one 16-bit word into the next: word 0 uses ADDD
            // (whose own carry-out already reflects the bit-15/16 boundary),
            // every later word's low byte then high byte uses ADCB/ADCA to
            // continue that same carry chain - see M6809Codegen.cpp's
            // genWideAdd().
            {"adca", {false, 0x89, 0xB9, 0xA9, false}},
            {"adcb", {false, 0xC9, 0xF9, 0xE9, false}},
            {"suba", {false, 0x80, 0xB0, 0xA0, false}},
            {"subb", {false, 0xC0, 0xF0, 0xE0, false}},
            {"subd", {false, 0x83, 0xB3, 0xA3, true}},
            // SBCA/SBCB: the borrow-chain analog of ADCA/ADCB above, for genWideAdd()'s subtract case.
            {"sbca", {false, 0x82, 0xB2, 0xA2, false}},
            {"sbcb", {false, 0xC2, 0xF2, 0xE2, false}},
            {"anda", {false, 0x84, 0xB4, 0xA4, false}},
            {"andb", {false, 0xC4, 0xF4, 0xE4, false}},
            {"ora", {false, 0x8A, 0xBA, 0xAA, false}},
            {"orb", {false, 0xCA, 0xFA, 0xEA, false}},
            {"eora", {false, 0x88, 0xB8, 0xA8, false}},
            {"eorb", {false, 0xC8, 0xF8, 0xE8, false}},
            {"cmpa", {false, 0x81, 0xB1, 0xA1, false}},
            {"cmpb", {false, 0xC1, 0xF1, 0xE1, false}},
            {"cmpd", {true, 0x83, 0xB3, 0xA3, true}},
            {"cmpx", {false, 0x8C, 0xBC, 0xAC, true}},
        };
        if (auto it = alu.find(mnemonic); it != alu.end()) {
            encodeAlu(mnemonic, ops, it->second);
            return;
        }

        static const std::unordered_map<std::string, uint8_t> lea = {
            {"leax", 0x30}, {"leay", 0x31}, {"leas", 0x32}, {"leau", 0x33},
        };
        if (auto it = lea.find(mnemonic); it != lea.end()) {
            if (ops.size() != 2) fail("'" + mnemonic + "' needs an '<offset>,<reg>' indexed operand");
            encodeIndexedOperands(ops, mnemonic, it->second, /*prefixed=*/false);
            return;
        }

        // JMP/JSR also support the indexed form (`<offset>,<reg>`) - unlike a
        // load/store's indexed form, which accesses memory AT the computed
        // address, JMP/JSR's indexed form transfers control directly TO it.
        // This is the only way to call/jump through a register-held address
        // (e.g. a C function pointer) on this dialect.
        if (mnemonic == "jmp") {
            if (ops.size() == 2) {
                encodeIndexedOperands(ops, mnemonic, 0x6E, /*prefixed=*/false);
                return;
            }
            const Operand &target = operandAt(ops, 0, mnemonic, 1);
            if (!target.isMemory) {
                fail("'jmp' needs a bracketed extended-addressing target or an indexed one, e.g. 'jmp [label]' / "
                     "'jmp 0,x'");
            }
            emit8(0x7E);
            emit16(target);
            return;
        }
        if (mnemonic == "jsr") {
            if (ops.size() == 2) {
                encodeIndexedOperands(ops, mnemonic, 0xAD, /*prefixed=*/false);
                return;
            }
            const Operand &target = operandAt(ops, 0, mnemonic, 1);
            if (!target.isMemory) {
                fail("'jsr' needs a bracketed extended-addressing target or an indexed one, e.g. 'jsr [label]' / "
                     "'jsr 0,x'");
            }
            emit8(0xBD);
            emit16(target);
            return;
        }
        // ANDCC/ORCC: clear/set condition-code bits directly, given an
        // immediate mask - the 6809's only way to mask/unmask IRQ (there's no
        // dedicated EI/DI like the Z80/8080), used by M6809Codegen.cpp's
        // Intrinsic case ("ion"/"iof").
        if (mnemonic == "andcc" || mnemonic == "orcc") {
            const Operand &mask = operandAt(ops, 0, mnemonic, 1);
            requireBare(mask, mnemonic);
            emit8(mnemonic == "andcc" ? 0x1C : 0x1A);
            emitImm8(mask);
            return;
        }

        static const std::unordered_map<std::string, std::pair<bool, uint8_t>> longBranch = {
            {"lbra", {false, 0x16}}, {"lbeq", {true, 0x27}}, {"lbne", {true, 0x26}}, {"lblt", {true, 0x2D}},
            {"lbge", {true, 0x2C}},  {"lble", {true, 0x2F}}, {"lbgt", {true, 0x2E}}, {"lblo", {true, 0x25}},
            {"lbhs", {true, 0x24}},  {"lbhi", {true, 0x22}}, {"lbls", {true, 0x23}},
        };
        if (auto it = longBranch.find(mnemonic); it != longBranch.end()) {
            const Operand &target = operandAt(ops, 0, mnemonic, 1);
            requireBare(target, mnemonic);
            if (it->second.first) emit8(0x10);
            emit8(it->second.second);
            emitRel16(target.expr);
            return;
        }

        static const std::unordered_map<std::string, uint8_t> stackOp = {
            {"pshs", 0x34}, {"puls", 0x35}, {"pshu", 0x36}, {"pulu", 0x37},
        };
        if (auto it = stackOp.find(mnemonic); it != stackOp.end()) {
            // PSHS/PULS can list "u" (the *other* stack pointer) in their
            // register list; PSHU/PULU can list "s" instead - both use bit
            // 0x40 for whichever one is the "other" pointer.
            bool otherPointerIsU = mnemonic == "pshs" || mnemonic == "puls";
            static const std::unordered_map<std::string, uint8_t> baseBits = {
                {"cc", 0x01}, {"a", 0x02}, {"b", 0x04}, {"dp", 0x08}, {"x", 0x10}, {"y", 0x20}, {"pc", 0x80},
            };
            uint8_t mask = 0;
            for (const auto &op : ops) {
                requireBare(op, mnemonic);
                auto name = asBareIdent(op);
                if (!name) fail("'" + mnemonic + "' needs a register-name list");
                if (*name == (otherPointerIsU ? "u" : "s")) {
                    mask |= 0x40;
                    continue;
                }
                auto bit = baseBits.find(*name);
                if (bit == baseBits.end()) fail("'" + mnemonic + "': '" + *name + "' isn't a valid register here");
                mask |= bit->second;
            }
            emit8(it->second);
            emit8(mask);
            return;
        }

        if (mnemonic == "tfr" || mnemonic == "exg") {
            const Operand &src = operandAt(ops, 0, mnemonic, 2);
            const Operand &dst = operandAt(ops, 1, mnemonic, 2);
            requireBare(src, mnemonic);
            requireBare(dst, mnemonic);
            auto srcName = asBareIdent(src);
            auto dstName = asBareIdent(dst);
            static const std::unordered_map<std::string, uint8_t> regCode = {
                {"d", 0x0}, {"x", 0x1}, {"y", 0x2}, {"u", 0x3}, {"s", 0x4}, {"pc", 0x5},
                {"a", 0x8}, {"b", 0x9}, {"cc", 0xA}, {"dp", 0xB},
            };
            if (!srcName || !dstName) fail("'" + mnemonic + "' needs two register names");
            auto srcIt = regCode.find(*srcName);
            auto dstIt = regCode.find(*dstName);
            if (srcIt == regCode.end() || dstIt == regCode.end()) fail("'" + mnemonic + "': unrecognized register");
            emit8(mnemonic == "tfr" ? 0x1F : 0x1E);
            emit8(static_cast<uint8_t>((srcIt->second << 4) | dstIt->second));
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

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kM6809CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry too, including the pseudo-symbol "..".
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, kM6809RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleM6809(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleM6809Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
