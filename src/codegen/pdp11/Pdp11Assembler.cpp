#include "Pdp11Assembler.h"

#include <algorithm>
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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp11Assembler: " + msg); }

// --- Parsing, via the shared grammar/RccAsm.g4 ---------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression. `Reg`: a bare `r0`-`r7`/`sp`/`pc`
// identifier. `Imm`: a bare (optionally signed) number. `Mem`: anything else (a label, or
// `label+N`) - PC-relative addressing to the value stored there. `Indexed`: the `[Rn+N]`/
// `[Rn-N]` bracket form - index mode `X(Rn)`, base register plus a compile-time-constant
// displacement, used for frame-pointer-relative Local/Param addressing.
struct Operand {
    enum Kind { Reg, Imm, Mem, Indexed } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem: the operand's own value/target.
                                  // Indexed: the displacement added to `reg` (empty means 0).
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Db, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
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

// `r0`-`r7` (case-insensitive), plus `sp`/`pc` aliases for `r6`/`r7`; -1 if neither.
int regNumber(const std::string &lower) {
    if (lower == "sp") return 6;
    if (lower == "pc") return 7;
    if (lower.size() == 2 && lower[0] == 'r' && lower[1] >= '0' && lower[1] <= '7') return lower[1] - '0';
    return -1;
}

Operand buildOperand(RccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        // `[Rn+N]`/`[Rn-N]`: first syllable must be a bare, unsigned base register; every
        // remaining syllable a literal number (never a symbol, so never a relocation).
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        if (syls.empty() || syls[0].sign != 0 || syls[0].kind != SylKind::Symbol) {
            fail("'[...]' operand must start with a bare base register, e.g. '[r5+4]'");
        }
        int baseReg = regNumber(toLower(syls[0].text));
        if (baseReg < 0) fail("'[...]' operand's base must be a register");
        for (size_t i = 1; i < syls.size(); i++) {
            if (syls[i].kind != SylKind::Number) fail("'[...]' operand's displacement must be a literal number");
        }
        Operand op;
        op.kind = Operand::Indexed;
        op.reg = baseReg;
        op.expr.assign(syls.begin() + 1, syls.end());
        return op;
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    Operand op;
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        if (int r = regNumber(toLower(syls[0].text)); r >= 0) {
            op.kind = Operand::Reg;
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

// One pending fixup, turned into a real ObjRelocation once assembly is done.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
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
                // Each word goes through the same external-name/base-shift tracking as
                // `lea`'s absolute extension word (see encodeLea()).
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, "..", kPdp11RelocAbs16});
                        if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPdp11RelocAbs16});
                    }
                    emit16(static_cast<uint16_t>(v));
                }
                return;
            case Line::Db:
                // `char` global storage; never a symbol element (no 1-byte relocation kind
                // exists on this target).
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `needsBaseShift` is set only for `lea`'s absolute reference; a PC-relative `Mem`
    // operand never needs it (see emitEAExtension()).
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

    void emit16(uint16_t w) {
        bytes_.push_back(static_cast<uint8_t>(w & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        offset_ += 2;
    }

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    // --- general ("double operand"-style) EA field --------------------
    // Reg direct (mode 000), or via PC (R7): immediate (mode 010, extension word is the
    // literal value) or relative (mode 110, "X(PC)", extension word is a displacement to the
    // target); via any other register: index mode (mode 110, "X(Rn)", extension word is a
    // plain additive displacement). No other addressing mode is ever produced here -
    // dedicated mnemonics (`ldind`/`stind`/`push`/`pop`/`lea`) cover the rest.
    struct EA {
        uint8_t mode;
        uint8_t reg;
    };
    static EA classifyEA(const Operand &op) {
        switch (op.kind) {
            case Operand::Reg:
                return {0b000, static_cast<uint8_t>(op.reg)};
            case Operand::Imm:
                return {0b010, 7};
            case Operand::Mem:
                return {0b110, 7};
            case Operand::Indexed:
                return {0b110, static_cast<uint8_t>(op.reg)};
        }
        fail("internal: unhandled operand kind");
    }
    // Emits `op`'s extension word (Imm/Mem both need one). Must be called exactly when the
    // PC sits where that word will land, since Mem's value is a displacement from that address.
    void emitEAExtension(const Operand &op) {
        if (op.kind == Operand::Imm) {
            emit16(static_cast<uint16_t>(evalExpr(op.expr)));
        } else if (op.kind == Operand::Mem) {
            // PC-relative (mode 067): a locally-resolved reference needs no relocation,
            // since target and extAddr are both already final, zero-based offsets.
            std::string externalName;
            uint32_t target = evalExpr(op.expr, &externalName, nullptr);
            uint32_t extAddr = orgBase_ + offset_;
            if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
                relocations_.push_back({offset_, externalName, kPdp11RelocRel16});
            }
            // Mode 067's displacement is relative to the PC after this extension word is
            // fetched, i.e. extAddr + 2.
            emit16(static_cast<uint16_t>(target - (extAddr + 2)));
        } else if (op.kind == Operand::Indexed) {
            // A plain additive displacement - never a symbol, so never a relocation.
            emit16(static_cast<uint16_t>(evalExpr(op.expr)));
        }
    }

    // The `opcode SS DD` double-operand family (MOV/CMP/BIC/BIS/ADD/SUB): 4-bit opcode, 6-bit
    // source EA, 6-bit destination EA, each with its own extension word if needed (source's
    // extension comes first, matching real PDP-11 object code order).
    void encodeTwoOp(uint16_t opcodeBase, const Operand &src, const Operand &dst, bool writesDst,
                      const std::string &mnemonic) {
        if (writesDst && dst.kind == Operand::Imm) fail("'" + mnemonic + "': an immediate can't be a destination");
        EA s = classifyEA(src);
        EA d = classifyEA(dst);
        emit16(static_cast<uint16_t>(opcodeBase | (s.mode << 9) | (s.reg << 6) | (d.mode << 3) | d.reg));
        emitEAExtension(src);
        emitEAExtension(dst);
    }

    // The single-operand family (CLR/COM/NEG/TST/ASL/ASR/ROR/SXT): one general EA.
    void encodeOneOp(uint16_t opcodeBase, const Operand &op) {
        EA e = classifyEA(op);
        emit16(static_cast<uint16_t>(opcodeBase | (e.mode << 3) | e.reg));
        emitEAExtension(op);
    }

    // The EIS `opcode Reg,<ea>` family: a fixed 3-bit register field (MUL/DIV's accumulator,
    // or XOR's source register) plus one general EA.
    void encodeRegEa(uint16_t opcodeBase, uint8_t regField, const Operand &ea) {
        EA e = classifyEA(ea);
        emit16(static_cast<uint16_t>(opcodeBase | (regField << 6) | (e.mode << 3) | e.reg));
        emitEAExtension(ea);
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // `lea label,Rn`: loads the label's absolute address into Rn, via the same PC-immediate
    // encoding as a numeric Imm operand, with the extension word holding the label's value.
    void encodeLea(const Operand &src, const Operand &dst) {
        if (src.kind != Operand::Mem) fail("'lea': source must be a label");
        uint8_t rn = requireReg(dst, "lea");
        emit16(static_cast<uint16_t>(0x1000 | (0b010 << 9) | (7 << 6) | rn));
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(src.expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kPdp11RelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kPdp11RelocAbs16});
        }
        emit16(static_cast<uint16_t>(v));
    }

    // Branch-offset family (BR/BEQ/BNE/BLT/BLE/BGT/BGE): an 8-bit signed word offset,
    // relative to the address right after this one-word instruction.
    void encodeBranch(uint16_t opcodeBase, const Operand &target, const std::string &mnemonic) {
        if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
        uint32_t targetAddr = evalExpr(target.expr);
        uint32_t nextAddr = orgBase_ + offset_ + 2;
        int32_t byteDelta = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(nextAddr);
        if (byteDelta % 2 != 0) fail("'" + mnemonic + "': branch target isn't word-aligned relative to here");
        int32_t wordDelta = byteDelta / 2;
        if (stage_ == 2 && (wordDelta < -128 || wordDelta > 127)) fail("'" + mnemonic + "': branch target out of range");
        emit16(static_cast<uint16_t>(opcodeBase | (static_cast<uint8_t>(wordDelta) & 0xFF)));
    }

    // Not a general PDP-11 encoder: exactly the mnemonic/operand-shape combinations
    // Pdp11Codegen.cpp emits.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint16_t> twoOpWrite = {
            {"mov", 0x1000}, {"add", 0x6000}, {"sub", 0xE000}, {"bic", 0x4000}, {"bis", 0x5000},
            // Byte form of MOV (bit 15, the "B" bit); a register destination auto-sign-extends
            // to 16 bits in hardware, giving a `char` load's signed semantics for free.
            {"movb", 0x9000},
        };
        if (auto it = twoOpWrite.find(mnemonic); it != twoOpWrite.end()) {
            encodeTwoOp(it->second, requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic),
                        /*writesDst=*/true, mnemonic);
            return;
        }
        if (mnemonic == "cmp") {
            // CMP only updates condition codes (flags = src - dst), so an Imm destination is
            // legal here unlike MOV/ADD/SUB/BIC/BIS, which really do write theirs.
            encodeTwoOp(0x2000, requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic),
                        /*writesDst=*/false, mnemonic);
            return;
        }
        if (mnemonic == "xor") {
            uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            encodeRegEa(0x7800, rn, requireOperand(ops, 1, mnemonic));
            return;
        }
        if (mnemonic == "mul" || mnemonic == "div") {
            uint8_t rn = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            // MUL is 070RSS (0x7000); DIV is 071RSS (0x7200), not ASH's 072RSS (0x7400).
            encodeRegEa(mnemonic == "mul" ? 0x7000 : 0x7200, rn, requireOperand(ops, 0, mnemonic));
            return;
        }
        static const std::unordered_map<std::string, uint16_t> oneOp = {
            {"com", 0x0A40}, {"neg", 0x0B00}, {"tst", 0x0BC0}, {"ror", 0x0C00},
            {"asr", 0x0C80}, {"asl", 0x0CC0}, {"sxt", 0x0DC0},
            // ADC/SBC: add/subtract the carry bit into the operand in place, used for
            // multi-word ripple-carry add/sub (genWideAdd()).
            {"adc", 0x0B40}, {"sbc", 0x0B80},
        };
        if (auto it = oneOp.find(mnemonic); it != oneOp.end()) {
            encodeOneOp(it->second, requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "clc") {
            if (!ops.empty()) fail("'clc' takes no operand");
            emit16(0x00A1);
            return;
        }
        if (mnemonic == "lea") {
            encodeLea(requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic));
            return;
        }
        // push/pop: MOV Rn,-(SP) / MOV (SP)+,Rn, spelled as their own mnemonics rather than a
        // generic autoincrement/autodecrement operand.
        if (mnemonic == "push") {
            uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            // src Rn direct (mode 000), dst -(SP) (mode 100, reg 6)
            emit16(static_cast<uint16_t>(0x1000 | (0b000 << 9) | (rn << 6) | (0b100 << 3) | 6));
            return;
        }
        if (mnemonic == "pop") {
            uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            // src (SP)+ (mode 010, reg 6), dst Rn direct (mode 000)
            emit16(static_cast<uint16_t>(0x1000 | (0b010 << 9) | (6 << 6) | (0b000 << 3) | rn));
            return;
        }
        // (Rn) register-deferred, spelled as its own mnemonics rather than a generic
        // bracketed operand.
        if (mnemonic == "ldind") {
            uint8_t addr = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t dst = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            // src (addr) deferred (mode 001), dst Rn direct (mode 000)
            emit16(static_cast<uint16_t>(0x1000 | (0b001 << 9) | (addr << 6) | (0b000 << 3) | dst));
            return;
        }
        if (mnemonic == "stind") {
            uint8_t src = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t addr = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            // src Rn direct (mode 000), dst (addr) deferred (mode 001)
            emit16(static_cast<uint16_t>(0x1000 | (0b000 << 9) | (src << 6) | (0b001 << 3) | addr));
            return;
        }
        // Byte-sized counterparts of ldind/stind (MOVB, opcode 0x9000, same field layout).
        if (mnemonic == "ldindb") {
            uint8_t addr = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t dst = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x9000 | (0b001 << 9) | (addr << 6) | (0b000 << 3) | dst));
            return;
        }
        if (mnemonic == "stindb") {
            uint8_t src = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t addr = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x9000 | (0b000 << 9) | (src << 6) | (0b001 << 3) | addr));
            return;
        }
        if (mnemonic == "jsr") {
            // JSR PC,label: a plain call, saving only the return address.
            EA e = classifyEA(requireOperand(ops, 0, mnemonic));
            emit16(static_cast<uint16_t>(0x0800 | (7 << 6) | (e.mode << 3) | e.reg));
            emitEAExtension(requireOperand(ops, 0, mnemonic));
            return;
        }
        // "JSR PC,@Rn": register-deferred call (mode 0b001) - the target is the address
        // stored in Rn, not Rn's own value (a bare `jsr Rn` would be register-direct, not a
        // legal call target), so this needs its own mnemonic.
        if (mnemonic == "jsrind") {
            uint8_t rn = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit16(static_cast<uint16_t>(0x0800 | (7 << 6) | (0b001 << 3) | rn));
            return;
        }
        if (mnemonic == "rts") {
            if (!ops.empty()) fail("'rts' takes no operand");
            emit16(0x0087);  // RTS PC
            return;
        }
        if (mnemonic == "jmp") {
            EA e = classifyEA(requireOperand(ops, 0, mnemonic));
            emit16(static_cast<uint16_t>(0x0040 | (e.mode << 3) | e.reg));
            emitEAExtension(requireOperand(ops, 0, mnemonic));
            return;
        }
        if (mnemonic == "halt") {
            if (!ops.empty()) fail("'halt' takes no operand");
            emit16(0x0000);
            return;
        }
        static const std::unordered_map<std::string, uint16_t> branchCond = {
            {"br", 0x0100}, {"bne", 0x0200}, {"beq", 0x0300}, {"bge", 0x0400},
            {"blt", 0x0500}, {"bgt", 0x0600}, {"ble", 0x0700},
            // Unsigned counterparts: branch on carry/zero flags rather than N/V.
            // BHI=101000, BLOS=101400, BHIS(=BCC)=103000, BLO(=BCS)=103400 octal.
            {"bhi", 0x8200}, {"blos", 0x8300}, {"bhis", 0x8600}, {"blo", 0x8700},
        };
        if (auto it = branchCond.find(mnemonic); it != branchCond.end()) {
            encodeBranch(it->second, requireOperand(ops, 0, mnemonic), mnemonic);
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
    obj.machineType = kPdp11CoffMachine;
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
        if (r.symbolName != ".." && !known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assemblePdp11(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assemblePdp11Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace rcc
