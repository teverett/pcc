#include "MC68328Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MC68328Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Group shape as every other backend's own assembler -
// see M6502Assembler.cpp's own comment.
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression - see
// MC68328Assembler.h's own comment (and ../m68k/M68kAssembler.h's own,
// which this shares unchanged) on why there's no real `#imm`/`(An)`
// syntax here. `Dreg`/`Areg` come from recognizing a bare `d0`-`d7`/
// `a0`-`a7` identifier; `Imm` from a bare (optionally signed) number;
// anything else (a label, or `label+N`) is `Mem` - an absolute
// address.
// `AnDisp` is the real 68000 address-register-indirect-with-
// displacement mode (d16(An)) - this dialect spells it "[a6+N]"/
// "[a6-N]" (bracketed, like the other backends' own memory operands -
// see I386Assembler.cpp's own `buildOperand`), used by
// MC68328Codegen.cpp to address a Local/Param's own frame-relative
// storage. `reg` is the address register (always a6 in practice - the
// frame pointer); `expr` is the signed 16-bit displacement.
struct Operand {
    enum Kind { Dreg, Areg, Imm, Mem, AnDisp } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem/AnDisp
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dl, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dl (N)
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
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

// `d0`-`d7` / `a0`-`a7` only (case-insensitive, matching how
// mnemonics are lowercased too) - returns -1 if `name` isn't one.
int regNumber(const std::string &name, char prefix) {
    if (name.size() != 2 || std::tolower(static_cast<unsigned char>(name[0])) != prefix) return -1;
    if (name[1] < '0' || name[1] > '7') return -1;
    return name[1] - '0';
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        // The only bracketed shape this dialect supports: "[aN]" or
        // "[aN+disp]"/"[aN-disp]" - frame-relative addressing (see
        // Operand::AnDisp above). Anything else bracketed is rejected,
        // same as before.
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        if (!syls.empty() && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
            if (int r = regNumber(toLower(syls[0].text), 'a'); r >= 0) {
                Operand op;
                op.kind = Operand::AnDisp;
                op.reg = r;
                if (syls.size() == 2) {
                    op.expr = {syls[1]};
                } else if (syls.size() > 2) {
                    fail("unrecognized memory operand shape");
                }
                return op;
            }
        }
        fail("unrecognized memory operand shape");
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        std::string lower = toLower(syls[0].text);
        if (int r = regNumber(lower, 'd'); r >= 0) {
            op.kind = Operand::Dreg;
            op.reg = r;
            return op;
        }
        if (int r = regNumber(lower, 'a'); r >= 0) {
            op.kind = Operand::Areg;
            op.reg = r;
            return op;
        }
    }
    bool allNumbers = true;
    for (const auto &s : syls) {
        if (s.kind != SylKind::Number) {
            allNumbers = false;
            break;
        }
    }
    op.kind = allNumbers ? Operand::Imm : Operand::Mem;
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
    if (mnemonic == "dl") {
        line.kind = Line::Dl;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dl' operand can't be bracketed");
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

// --- Two-pass assembly ----------------------------------------------------
// One pending fixup `assembleMC68328Object()` turns into a real
// ObjRelocation once assembly is done - mirrors
// ../x86_8086/I8086Assembler.cpp's own `Relocation`/`RelocKind`
// (just one kind here, `kMC68328RelocAbs32` - see
// MC68328Assembler.h's own header comment).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
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
            case Line::Dl:
                // Each word's own expression goes through the exact same
                // external-name/base-shift tracking as a `Mem` operand's
                // extension word (see emitEAExtension) - a bare symbol here
                // (an array initializer's address-of-function/global
                // element) resolves and relocates identically to one used
                // as an instruction's absolute address.
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, ".."});
                        if (!externalName.empty()) relocations_.push_back({offset_, externalName});
                    }
                    emit32(v);
                }
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../x86_8086/I8086Assembler.cpp's own `evalExpr` exactly - see
    // MC68328Assembler.h's own header comment on why *any* local
    // address reference, not just an external one, needs a
    // relocation once `allowExternal_` is set.
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

    // --- byte/word/long emission ------------------------------------

    void emit16(uint16_t w) {
        bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(w & 0xFF));
        offset_ += 2;
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
        emit16(static_cast<uint16_t>(v & 0xFFFF));
    }

    // Mode/register pair for `op`'s own effective-address field (see
    // MC68328Assembler.h's own header comment for why there's no
    // general indirect/indexed mode here - only what `op`'s own
    // classification already covers): Dn direct (000), An direct
    // (001), address register indirect with displacement (101 -
    // frame-relative Local/Param access, MC68328Codegen.cpp's
    // `addr()`), absolute long (111, 001 - extension word(s) hold the
    // address), or immediate (111, 100 - extension word(s) hold the
    // value).
    struct EA {
        uint8_t mode;
        uint8_t reg;
    };
    EA classifyEA(const Operand &op) const {
        switch (op.kind) {
            case Operand::Dreg:
                return {0b000, static_cast<uint8_t>(op.reg)};
            case Operand::Areg:
                return {0b001, static_cast<uint8_t>(op.reg)};
            case Operand::AnDisp:
                return {0b101, static_cast<uint8_t>(op.reg)};
            case Operand::Mem:
                return {0b111, 0b001};
            case Operand::Imm:
                return {0b111, 0b100};
        }
        fail("internal: unhandled operand kind");
    }
    // Emits `op`'s own extension word(s), if its EA mode needs any
    // (Mem/Imm both do - a 32-bit absolute address or a 32-bit
    // immediate value, since this dialect only has .L-sized forms for
    // every instruction that can take a Mem/Imm operand at all;
    // `AnDisp`'s own extension word is a plain compile-time-constant
    // signed 16-bit displacement, never a label, one word wide - the
    // real hardware's own `d16(An)` extension-word size).
    void emitEAExtension(const Operand &op) {
        if (op.kind == Operand::Mem) {
            // An absolute address - every reference needs the base-
            // shift (and, in object mode, possibly external) handling
            // MC68328Assembler.h's own header comment describes;
            // `Operand::Imm` (a plain numeric constant - see this
            // file's own `Operand` comment on why it's never anything
            // else) never does, so it's intentionally excluded here.
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t v = evalExpr(op.expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({offset_, ".."});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName});
            }
            emit32(v);
            return;
        }
        if (op.kind == Operand::Imm) emit32(evalExpr(op.expr));
        if (op.kind == Operand::AnDisp) {
            int32_t disp = op.expr.empty() ? 0 : static_cast<int32_t>(evalExpr(op.expr));
            if (stage_ == 2 && (disp < -32768 || disp > 32767)) fail("frame displacement out of 16-bit range");
            emit16(static_cast<uint16_t>(static_cast<int16_t>(disp)));
        }
    }

    // The `<ea>,Dn` / `Dn,<ea>` family (ADD/SUB/AND/OR/CMP/EOR/MULS/
    // DIVS/ADDA - see encodeInstr's own table): a fixed 3-bit register
    // field, a fixed 3-bit opmode, and one EA (the *other* operand -
    // whichever one of the two isn't the plain Dn/An register).
    void encodeRegOpEA(uint16_t base, uint8_t regField, uint8_t opmode, const Operand &ea) {
        EA e = classifyEA(ea);
        emit16(static_cast<uint16_t>(base | (regField << 9) | (opmode << 6) | (e.mode << 3) | e.reg));
        emitEAExtension(ea);
    }

    // MOVE.L <src>,<dst> / MOVEA.L <src>,An (the same instruction
    // family - a destination mode of 001 is what makes it "MOVEA"
    // rather than "MOVE", with no other encoding difference at all) -
    // both operands get their own full EA field, so both get their
    // own extension word if they need one (source's own extension, if
    // any, comes first - matching real object code order, since the
    // source operand is evaluated before the destination for a MOVE).
    void encodeMove(const Operand &src, const Operand &dst) {
        if (dst.kind == Operand::Imm) fail("'movel': an immediate can't be a destination");
        EA s = classifyEA(src);
        EA d = classifyEA(dst);
        emit16(static_cast<uint16_t>(0x2000 | (d.reg << 9) | (d.mode << 6) | (s.mode << 3) | s.reg));
        emitEAExtension(src);
        emitEAExtension(dst);
    }

    // LEA <label>,An / LEA [aN+disp],An - loads the source's own
    // *effective address* (never a Dreg/Imm - see MC68328Codegen.cpp's
    // own call sites, always a label or a frame-relative Local/Param
    // slot) into An.
    void encodeLea(const Operand &src, const Operand &dst) {
        if (dst.kind != Operand::Areg) fail("'leal': destination must be an address register");
        if (src.kind != Operand::Mem && src.kind != Operand::AnDisp) {
            fail("'leal': source must be a label or a frame-relative operand");
        }
        EA s = classifyEA(src);
        emit16(static_cast<uint16_t>(0x41C0 | (dst.reg << 9) | (s.mode << 3) | s.reg));
        emitEAExtension(src);
    }

    // SWAP/EXT.L/NEG.L/NOT.L - a single Dn operand (SWAP/EXT.L) or a
    // single EA operand (NEG.L/NOT.L, but MC68328Codegen.cpp only
    // ever passes a Dreg).
    void encodeUnary(uint16_t word, const Operand &op) {
        EA e = classifyEA(op);
        emit16(static_cast<uint16_t>(word | (e.mode << 3) | e.reg));
        emitEAExtension(op);
    }

    // Bcc/BRA - always the 16-bit-displacement form (the opcode
    // word's own low byte is 0x00, which per the real hardware's own
    // rule always means "the extension word that immediately follows
    // is the real, 16-bit, displacement").
    void encodeBranch(uint8_t condition, const Operand &target) {
        if (target.kind != Operand::Mem) fail("a branch target must be a label");
        // The displacement is relative to the address of the opcode
        // word plus 2 - i.e. right after the *opcode* word alone,
        // regardless of whether a 16-bit or 32-bit displacement
        // extension follows it (see ../m68k/M68kAssembler.cpp's own
        // comment: confirmed directly against qemu's own disassembler
        // - a `bra` targeting itself needs displacement -2, not -4).
        uint32_t endAddr = orgBase_ + offset_ + 2;
        uint32_t targetAddr = evalExpr(target.expr);
        emit16(static_cast<uint16_t>(0x6000 | (condition << 8)));
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -32768 || rel > 32767)) fail("branch target out of 16-bit range");
        emit16(static_cast<uint16_t>(static_cast<int16_t>(rel)));
    }

    // LSL.L/LSR.L <countDn>,<dataDn> - the register (variable-count)
    // shift form: `dir` 1=left/0=right, `I`=1 (bit 5) means "count
    // comes from the register field", not a literal 1-8 count.
    void encodeShift(uint8_t countReg, uint8_t dataReg, bool left) {
        uint16_t word = 0xE000 | (countReg << 9) | (left ? 1 << 8 : 0) | (0b10 << 6) | (1 << 5) | (0b01 << 3) |
                         dataReg;
        emit16(word);
    }

    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }
    static uint8_t requireDreg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Dreg) fail("'" + mnemonic + "' needs a data-register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static uint8_t requireAreg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Areg) fail("'" + mnemonic + "' needs an address-register operand");
        return static_cast<uint8_t>(op.reg);
    }

    // The full instruction vocabulary ../mc68328/MC68328Codegen.cpp
    // ever emits - not a general 68000 encoder: exactly these
    // mnemonic/operand-shape combinations (identical to
    // ../m68k/M68kAssembler.cpp's own vocabulary - see
    // MC68328Assembler.h's own header comment).
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "rts") {
            if (!ops.empty()) fail("'rts' takes no operand");
            emit16(0x4E75);
            return;
        }
        if (mnemonic == "movel") {
            encodeMove(requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic));
            return;
        }
        if (mnemonic == "leal") {
            encodeLea(requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic));
            return;
        }
        // Push/pop onto the real hardware stack (A7) - MOVE.L
        // Dn/An,-(A7) / MOVE.L (A7)+,Dn/An (either kind of register -
        // MC68328Codegen.cpp pushes/pops `a0` as well as `d0`/`d1`).
        // Spelled as their own mnemonics, not a generic pre-decrement/
        // post-increment operand - mode 100/011 with register 7 (A7),
        // fixed.
        if (mnemonic == "pushl") {
            const Operand &reg = requireOperand(ops, 0, mnemonic);
            if (reg.kind != Operand::Dreg && reg.kind != Operand::Areg) {
                fail("'pushl' needs a register operand");
            }
            EA e = classifyEA(reg);
            // destination -(A7) (mode 100, reg 7), source `reg`
            emit16(static_cast<uint16_t>(0x2000 | (7 << 9) | (0b100 << 6) | (e.mode << 3) | e.reg));
            return;
        }
        if (mnemonic == "popl") {
            const Operand &reg = requireOperand(ops, 0, mnemonic);
            if (reg.kind != Operand::Dreg && reg.kind != Operand::Areg) {
                fail("'popl' needs a register operand");
            }
            EA e = classifyEA(reg);
            // destination `reg`, source (A7)+ (mode 011, reg 7)
            emit16(static_cast<uint16_t>(0x2000 | (e.reg << 9) | (e.mode << 6) | (0b011 << 3) | 7));
            return;
        }
        // (An) indirect, spelled as its own mnemonics rather than a
        // generic bracketed operand.
        if (mnemonic == "ldind") {
            uint8_t an = requireAreg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t dn = requireDreg(requireOperand(ops, 1, mnemonic), mnemonic);
            // destination Dn (mode 000, reg dn), source (An) (mode 010, reg an)
            emit16(static_cast<uint16_t>(0x2000 | (dn << 9) | (0b000 << 6) | (0b010 << 3) | an));
            return;
        }
        if (mnemonic == "stind") {
            uint8_t dn = requireDreg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t an = requireAreg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x2000 | (an << 9) | (0b010 << 6) | (0b000 << 3) | dn));
            return;
        }

        static const std::unordered_map<std::string, uint16_t> regOpEa = {
            {"addl", 0xD000}, {"subl", 0x9000}, {"andl", 0xC000}, {"orl", 0x8000}, {"cmpl", 0xB000},
        };
        if (auto it = regOpEa.find(mnemonic); it != regOpEa.end()) {
            uint8_t dn = requireDreg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeRegOpEA(it->second, dn, 0b010, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "adda") {
            uint8_t an = requireAreg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeRegOpEA(0xD000, an, 0b111, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "suba") {
            uint8_t an = requireAreg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeRegOpEA(0x9000, an, 0b111, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "eorl") {
            // EOR Dn,<ea> - Dn is always the *source* here (the
            // register field), <ea> the destination.
            uint8_t dn = requireDreg(requireOperand(ops, 0, mnemonic), mnemonic);
            encodeRegOpEA(0xB000, dn, 0b110, requireOperand(ops, 1, mnemonic));
            return;
        }
        if (mnemonic == "mulsw" || mnemonic == "divsw") {
            const Operand &ea = requireOperand(ops, 0, mnemonic);
            if (ea.kind != Operand::Dreg) fail("'" + mnemonic + "': source must be a data register in this dialect");
            uint8_t dn = requireDreg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeRegOpEA(mnemonic == "mulsw" ? 0xC000 : 0x8000, dn, 0b111, ea);
            return;
        }
        if (mnemonic == "swapl") {
            emit16(static_cast<uint16_t>(0x4840 | requireDreg(requireOperand(ops, 0, mnemonic), mnemonic)));
            return;
        }
        if (mnemonic == "extl") {
            emit16(static_cast<uint16_t>(0x48C0 | requireDreg(requireOperand(ops, 0, mnemonic), mnemonic)));
            return;
        }
        if (mnemonic == "negl") {
            encodeUnary(0x4480, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "notl") {
            encodeUnary(0x4680, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "lsll" || mnemonic == "lsrl") {
            uint8_t countReg = requireDreg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t dataReg = requireDreg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeShift(countReg, dataReg, mnemonic == "lsll");
            return;
        }
        // LINK An,#disp / UNLK An - this target's own dedicated
        // frame-setup/teardown instructions (MC68328Codegen.cpp's
        // genFunction()/Return case use them with An=a6): LINK pushes
        // An onto the real hardware stack (A7), sets An=A7, then adds
        // the signed 16-bit displacement `disp` to A7 (negative to
        // reserve that many bytes of locals below the new frame
        // pointer); UNLK sets A7=An then pops the old An back off the
        // stack. Neither goes through classifyEA/emitEAExtension - An
        // and the (for LINK) displacement are both fixed-position
        // fields of the opcode word/extension word themselves, not a
        // general effective address.
        if (mnemonic == "linkl") {
            uint8_t an = requireAreg(requireOperand(ops, 0, mnemonic), mnemonic);
            const Operand &dispOp = requireOperand(ops, 1, mnemonic);
            if (dispOp.kind != Operand::Imm) fail("'linkl': displacement must be a plain immediate");
            int32_t disp = static_cast<int32_t>(evalExpr(dispOp.expr));
            if (stage_ == 2 && (disp < -32768 || disp > 32767)) fail("'linkl': displacement out of 16-bit range");
            emit16(static_cast<uint16_t>(0x4E50 | an));
            emit16(static_cast<uint16_t>(static_cast<int16_t>(disp)));
            return;
        }
        if (mnemonic == "unlkl") {
            uint8_t an = requireAreg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4E58 | an));
            return;
        }
        if (mnemonic == "jsr") {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            EA e = classifyEA(target);
            emit16(static_cast<uint16_t>(0x4E80 | (e.mode << 3) | e.reg));
            emitEAExtension(target);
            return;
        }
        // "JSR (An)" - a real near call through whatever address An
        // holds, mode 0b010 - same "(An) gets its own mnemonic rather
        // than a generic bracketed operand" convention ldind/stind
        // already use (classifyEA's Areg case is register DIRECT,
        // mode 0b001, which the real MC68EC000 ISA doesn't accept as a
        // JSR target at all - this needs the true indirect mode, not
        // what "jsr a0" would otherwise encode).
        if (mnemonic == "jsrind") {
            uint8_t an = requireAreg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x4E80 | (0b010 << 3) | an));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> branchCond = {
            {"bra", 0b0000}, {"beq", 0b0111}, {"bne", 0b0110}, {"blt", 0b1101},
            {"ble", 0b1111}, {"bgt", 0b1110}, {"bge", 0b1100},
        };
        if (auto it = branchCond.find(mnemonic); it != branchCond.end()) {
            encodeBranch(it->second, requireOperand(ops, 0, mnemonic));
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
// Assembler produced - mirrors ../z80/Z80Assembler.cpp's own
// `buildObjectFile`.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kMC68328CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, kMC68328RelocAbs32});
    }
    return obj;
}

}  // namespace

std::string assembleMC68328(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleMC68328Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
