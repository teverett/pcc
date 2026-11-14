#include "TMS9900Assembler.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("TMS9900Assembler: " + msg); }

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// See TMS9900Assembler.h's own header comment for why addressing modes are spelled this way.
// `RegIndirectInc` and `Indexed` both need the operand's own register number recorded
// separately from `expr` (`Indexed`'s `expr` is the following displacement word; `Symbolic`'s
// is the following absolute-address word; `Imm` is a Format 8 instruction's own bare immediate
// word - never an addressing-mode operand at all).
struct Operand {
    enum Kind { Reg, RegIndirect, RegIndirectInc, Symbolic, Indexed, Imm } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Symbolic/Indexed/Imm
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Db/Dw (N)
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
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

// Returns -1 if `name` isn't a bare "r0".."r15". `prefix` is "r" for a plain register
// reference, "ir" for the "[irN]" register-indirect-autoincrement spelling.
int regNumber(const std::string &name, const char *prefix) {
    size_t plen = std::strlen(prefix);
    if (name.size() <= plen || name.compare(0, plen, prefix) != 0) return -1;
    std::string digits = name.substr(plen);
    for (char c : digits) {
        if (c < '0' || c > '9') return -1;
    }
    int n = std::stoi(digits);
    if (n < 0 || n > 15) return -1;
    // Reject a would-be "r" match that's actually "ir..." consumed as "r..." starting one
    // character late - e.g. regNumber("ir3", "r") must not accidentally read "r3" out of the
    // tail of "ir3". Since callers only ever pass the whole identifier, and "ir3"[0] == 'i' !=
    // 'r', this can't happen in practice; kept as a comment, not a check, to avoid dead code.
    return n;
}

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
            std::string lower = toLower(syls[0].text);
            if (int r = regNumber(lower, "ir"); r >= 0) {
                Operand op;
                op.kind = Operand::RegIndirectInc;
                op.reg = r;
                return op;
            }
            if (int r = regNumber(lower, "r"); r >= 0) {
                Operand op;
                op.kind = Operand::RegIndirect;
                op.reg = r;
                return op;
            }
        }
        if (syls.size() == 2 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
            if (int r = regNumber(toLower(syls[0].text), "r"); r >= 0) {
                Operand op;
                op.kind = Operand::Indexed;
                op.reg = r;
                op.expr = {syls[1]};
                return op;
            }
        }
        // Anything else bracketed - a plain number/label, or a multi-syllable expression with
        // no leading register (e.g. "[label+4]", a byte offset into a wide value/array/struct)
        // - is Symbolic addressing; the whole expression is evaluated as one 16-bit address.
        {
            Operand op;
            op.kind = Operand::Symbolic;
            op.expr = std::move(syls);
            return op;
        }
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        if (int r = regNumber(toLower(syls[0].text), "r"); r >= 0) {
            Operand op;
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
    }
    Operand op;
    op.kind = Operand::Imm;
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
    if (mnemonic == "dw" || mnemonic == "db") {
        line.kind = mnemonic == "dw" ? Line::Dw : Line::Db;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'" + mnemonic + "' operand can't be bracketed");
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

// `symbolName` is the unresolved reference, or the reserved pseudo-symbol ".." for a same-file
// address's needed base-shift.
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
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emitWordWithReloc(e);
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // In object mode (`allowExternal_`), any local address reference also needs a relocation,
    // not just an external symbol - see `needsBaseShift`.
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

    // --- byte/word emission (big-endian - the TMS9900's own native byte order) --------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }
    void emit16(uint16_t w) {
        bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(w & 0xFF));
        offset_ += 2;
    }
    // The one choke point every absolute-address/immediate-word reference passes through -
    // records a relocation (in object mode) when the expression names an external symbol, or a
    // same-file label whose own address will shift once a linker chooses this object's base.
    void emitWordWithReloc(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit16(static_cast<uint16_t>(v));
    }

    // --- addressing-mode encoding --------------------------------------------------

    // Ts/Td mode field (2 bits) plus the S/D register field (4 bits) - see
    // TMS9900Assembler.h's own header comment for the textual spelling of each.
    struct EA {
        uint8_t mode;
        uint8_t reg;
    };
    EA classifyEA(const Operand &op, const std::string &mnemonic) const {
        switch (op.kind) {
            case Operand::Reg:
                return {0b00, static_cast<uint8_t>(op.reg)};
            case Operand::RegIndirect:
                return {0b01, static_cast<uint8_t>(op.reg)};
            case Operand::RegIndirectInc:
                return {0b11, static_cast<uint8_t>(op.reg)};
            case Operand::Symbolic:
                return {0b10, 0};
            case Operand::Indexed:
                return {0b10, static_cast<uint8_t>(op.reg)};
            case Operand::Imm:
                fail("'" + mnemonic + "': a bare number/label is only valid where this dialect expects a Format "
                                      "8 immediate (li/ai/andi/ori/ci/lwpi/limi), not a general addressing-mode "
                                      "operand - the TMS9900 has no immediate addressing mode for ordinary "
                                      "instructions");
        }
        fail("internal: unhandled operand kind");
    }
    // Symbolic/Indexed need a following word (the absolute address, or the indexed base to add
    // to the index register); every other mode is fully encoded in the opcode word itself.
    void emitEAExtension(const Operand &op) {
        if (op.kind == Operand::Symbolic || op.kind == Operand::Indexed) emitWordWithReloc(op.expr);
    }

    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }
    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a plain register operand (r0-r15)");
        return static_cast<uint8_t>(op.reg);
    }
    uint32_t requireImmValue(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Imm) fail("'" + mnemonic + "' needs a plain, unbracketed immediate operand");
        return evalExpr(op.expr);
    }

    // --- Format 1: two-operand, both a full Ts/Td addressing-mode operand -----------
    // SZC(B)/S(B)/C(B)/A(B)/MOV(B)/SOC(B) - "mnemonic source,destination", matching real
    // TMS9900 assembly operand order. The source's own extension word (if any) is emitted
    // before the destination's, matching the order the real CPU fetches them in.
    void encodeFormat1(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &src = requireOperand(ops, 0, mnemonic);
        const Operand &dst = requireOperand(ops, 1, mnemonic);
        EA s = classifyEA(src, mnemonic);
        EA d = classifyEA(dst, mnemonic);
        emit16(static_cast<uint16_t>(base | (d.mode << 10) | (d.reg << 6) | (s.mode << 4) | s.reg));
        emitEAExtension(src);
        emitEAExtension(dst);
    }

    // --- Format 2: jumps (unconditional/conditional) and the single-CRU-bit ops -----
    // Jxx/JMP: "mnemonic label" (bare, unbracketed - there's no general addressing mode here,
    // just an 8-bit signed PC-relative word displacement baked into the opcode word itself,
    // relative to the address right after this one-word instruction, matching the real CPU's
    // own PC, which has already advanced past this instruction by the time the jump is taken).
    void encodeJump(uint8_t opcodeHighByte, const Operand &target, const std::string &mnemonic) {
        if (target.kind != Operand::Imm) fail("'" + mnemonic + "' needs a plain, unbracketed label/address operand");
        uint32_t endAddr = orgBase_ + offset_ + 2;
        uint32_t targetAddr = evalExpr(target.expr);
        int32_t wordsDisp = (static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr)) / 2;
        if (stage_ == 2) {
            if ((static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr)) % 2 != 0) {
                fail("'" + mnemonic + "': target isn't word-aligned relative to this instruction");
            }
            if (wordsDisp < -128 || wordsDisp > 127) fail("'" + mnemonic + "': target out of 8-bit word-displacement range");
        }
        emit16(static_cast<uint16_t>((opcodeHighByte << 8) | (static_cast<uint8_t>(wordsDisp))));
    }
    // SBO/SBZ/TB: "mnemonic displacement" - a plain signed 8-bit CRU-bit offset (from the CRU
    // base register, R12 - not PC), written as a bare, unbracketed number, never a label.
    void encodeCruBit(uint8_t opcodeHighByte, const Operand &disp, const std::string &mnemonic) {
        uint32_t v = requireImmValue(disp, mnemonic);
        if (stage_ == 2 && (static_cast<int32_t>(v) < -128 || static_cast<int32_t>(v) > 127)) {
            fail("'" + mnemonic + "': CRU bit displacement out of 8-bit signed range");
        }
        emit16(static_cast<uint16_t>((opcodeHighByte << 8) | (static_cast<uint8_t>(v))));
    }

    // --- Format 3: COC/CZC/XOR (dest a plain register) and XOP ----------------------
    void encodeFormat3(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &src = requireOperand(ops, 0, mnemonic);
        const Operand &regOp = requireOperand(ops, 1, mnemonic);
        EA s = classifyEA(src, mnemonic);
        uint8_t reg = requireReg(regOp, mnemonic);
        emit16(static_cast<uint16_t>(base | (reg << 6) | (s.mode << 4) | s.reg));
        emitEAExtension(src);
    }
    // XOP's own second operand is conventionally written as a plain 0-15 number (the vector
    // number), not a register this dialect's codegen would otherwise read/write - accepted as
    // either a Reg (r0-r15) or a bare Imm 0-15, encoded into the same register-number field.
    void encodeXop(const std::vector<Operand> &ops) {
        const Operand &src = requireOperand(ops, 0, "xop");
        const Operand &vecOp = requireOperand(ops, 1, "xop");
        EA s = classifyEA(src, "xop");
        uint32_t vec = vecOp.kind == Operand::Reg ? static_cast<uint32_t>(vecOp.reg) : requireImmValue(vecOp, "xop");
        if (stage_ == 2 && vec > 15) fail("'xop': vector number must be 0-15");
        emit16(static_cast<uint16_t>(0x2C00 | ((vec & 0xF) << 6) | (s.mode << 4) | s.reg));
        emitEAExtension(src);
    }

    // --- Format 4: LDCR/STCR ---------------------------------------------------------
    void encodeFormat4(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &src = requireOperand(ops, 0, mnemonic);
        const Operand &countOp = requireOperand(ops, 1, mnemonic);
        EA s = classifyEA(src, mnemonic);
        uint32_t count = requireImmValue(countOp, mnemonic);
        if (stage_ == 2 && count > 15) fail("'" + mnemonic + "': count must be 0-15 (0 means 16 bits)");
        emit16(static_cast<uint16_t>(base | ((count & 0xF) << 6) | (s.mode << 4) | s.reg));
        emitEAExtension(src);
    }

    // --- Format 5: SRA/SRL/SLA/SRC - "mnemonic register,count" ----------------------
    void encodeFormat5(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t reg = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint32_t count = requireImmValue(requireOperand(ops, 1, mnemonic), mnemonic);
        if (stage_ == 2 && count > 15) fail("'" + mnemonic + "': count must be 0-15 (0 means take it from r0)");
        emit16(static_cast<uint16_t>(base | (reg << 4) | (count & 0xF)));
    }

    // --- Format 6: single-operand (any addressing mode), no byte variants ----------
    void encodeFormat6(uint16_t base, const Operand &op, const std::string &mnemonic) {
        EA e = classifyEA(op, mnemonic);
        emit16(static_cast<uint16_t>(base | (e.mode << 4) | e.reg));
        emitEAExtension(op);
    }

    // --- Format 8: immediate/register-only ------------------------------------------
    // LI/AI/ANDI/ORI/CI: "mnemonic register,immediate".
    void encodeFormat8Imm(uint16_t base, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t reg = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        const Operand &immOp = requireOperand(ops, 1, mnemonic);
        if (immOp.kind != Operand::Imm) fail("'" + mnemonic + "': second operand must be a plain immediate");
        emit16(static_cast<uint16_t>(base | reg));
        emitWordWithReloc(immOp.expr);
    }
    // STWP/STST: "mnemonic register" - store WP/ST into a workspace register, no following word.
    void encodeFormat8Reg(uint16_t base, const Operand &op, const std::string &mnemonic) {
        emit16(static_cast<uint16_t>(base | requireReg(op, mnemonic)));
    }
    // LWPI/LIMI: "mnemonic immediate" - load WP/the interrupt mask directly, no register field.
    void encodeFormat8Direct(uint16_t base, const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Imm) fail("'" + mnemonic + "' needs a plain immediate operand");
        emit16(base);
        emitWordWithReloc(op.expr);
    }

    // Covers the complete, real TMS9900 instruction set (69 instructions) - see
    // TMS9900Assembler.h's own header comment for the addressing-mode syntax every operand
    // below uses.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint16_t> format1 = {
            {"szc", 0x4000}, {"szcb", 0x5000}, {"s", 0x6000}, {"sb", 0x7000}, {"c", 0x8000}, {"cb", 0x9000},
            {"a", 0xA000}, {"ab", 0xB000}, {"mov", 0xC000}, {"movb", 0xD000}, {"soc", 0xE000}, {"socb", 0xF000},
        };
        if (auto it = format1.find(mnemonic); it != format1.end()) {
            encodeFormat1(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint8_t> jumps = {
            {"jmp", 0x10}, {"jlt", 0x11}, {"jle", 0x12}, {"jeq", 0x13}, {"jhe", 0x14}, {"jgt", 0x15},
            {"jne", 0x16}, {"jnc", 0x17}, {"joc", 0x18}, {"jno", 0x19}, {"jl", 0x1A}, {"jh", 0x1B}, {"jop", 0x1C},
        };
        if (auto it = jumps.find(mnemonic); it != jumps.end()) {
            encodeJump(it->second, requireOperand(ops, 0, mnemonic), mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> cruBit = {{"sbo", 0x1D}, {"sbz", 0x1E}, {"tb", 0x1F}};
        if (auto it = cruBit.find(mnemonic); it != cruBit.end()) {
            encodeCruBit(it->second, requireOperand(ops, 0, mnemonic), mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format3 = {
            {"coc", 0x2000}, {"czc", 0x2400}, {"xor", 0x2800},
        };
        if (auto it = format3.find(mnemonic); it != format3.end()) {
            encodeFormat3(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "xop") {
            encodeXop(ops);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format4 = {{"ldcr", 0x3000}, {"stcr", 0x3400}};
        if (auto it = format4.find(mnemonic); it != format4.end()) {
            encodeFormat4(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format9 = {{"mpy", 0x3800}, {"div", 0x3C00}};
        if (auto it = format9.find(mnemonic); it != format9.end()) {
            // Same field shape as format3 (register + Ts/S), just a different base.
            encodeFormat3(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format5 = {
            {"sra", 0x0800}, {"srl", 0x0900}, {"sla", 0x0A00}, {"src", 0x0B00},
        };
        if (auto it = format5.find(mnemonic); it != format5.end()) {
            encodeFormat5(it->second, ops, mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format6 = {
            {"blwp", 0x0400}, {"b", 0x0440}, {"x", 0x0480}, {"clr", 0x04C0}, {"neg", 0x0500}, {"inv", 0x0540},
            {"inc", 0x0580}, {"inct", 0x05C0}, {"dec", 0x0600}, {"dect", 0x0640}, {"bl", 0x0680}, {"swpb", 0x06C0},
            {"seto", 0x0700}, {"abs", 0x0740},
        };
        if (auto it = format6.find(mnemonic); it != format6.end()) {
            encodeFormat6(it->second, requireOperand(ops, 0, mnemonic), mnemonic);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format7 = {
            {"idle", 0x0340}, {"rset", 0x0360}, {"rtwp", 0x0380}, {"ckon", 0x03A0}, {"ckof", 0x03C0}, {"lrex", 0x03E0},
        };
        if (auto it = format7.find(mnemonic); it != format7.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit16(it->second);
            return;
        }

        static const std::unordered_map<std::string, uint16_t> format8Imm = {
            {"li", 0x0200}, {"ai", 0x0220}, {"andi", 0x0240}, {"ori", 0x0260}, {"ci", 0x0280},
        };
        if (auto it = format8Imm.find(mnemonic); it != format8Imm.end()) {
            encodeFormat8Imm(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "stwp" || mnemonic == "stst") {
            encodeFormat8Reg(mnemonic == "stwp" ? 0x02A0 : 0x02C0, requireOperand(ops, 0, mnemonic), mnemonic);
            return;
        }
        if (mnemonic == "lwpi" || mnemonic == "limi") {
            encodeFormat8Direct(mnemonic == "lwpi" ? 0x02E0 : 0x0300, requireOperand(ops, 0, mnemonic), mnemonic);
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

ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kTMS9900CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, kTMS9900RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleTMS9900(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleTMS9900Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
