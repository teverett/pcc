#include "MipsAssembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MipsAssembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 ---------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// A bare register name is Reg, a bare signed number is Imm, anything
// else (a label or `label+N`) is Mem, resolved as an address.
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, Dh, Dw, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
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

// Numbers match the real MIPS/o32 ABI register numbers (fp is $s8).
// Returns -1 if not one of this dialect's named registers.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"zero", 0}, {"at", 1},  {"t0", 8},  {"t1", 9},  {"t2", 10}, {"t3", 11},
        {"t4", 12},  {"t5", 13}, {"t6", 14}, {"sp", 29}, {"fp", 30}, {"ra", 31},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) {
        fail(
            "bracketed memory operands aren't part of this target's dialect - MIPS memory access always goes "
            "through the dedicated 'ldind'/'stind' mnemonics instead (see MipsCodegen.h)");
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
    if (mnemonic == "dw" || mnemonic == "db" || mnemonic == "dh") {
        line.kind = mnemonic == "dw" ? Line::Dw : mnemonic == "dh" ? Line::Dh : Line::Db;
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

// A pending fixup, turned into an ObjRelocation once assembly is done.
struct Relocation {
    uint32_t offset;
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
                for (const auto &e : line.exprList) {
                    std::string externalName;
                    bool needsBaseShift = false;
                    uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
                    if (stage_ == 2 && allowExternal_) {
                        if (needsBaseShift) relocations_.push_back({offset_, "..", kMipsRelocAbs32});
                        if (!externalName.empty()) relocations_.push_back({offset_, externalName, kMipsRelocAbs32});
                    }
                    emit32(v);
                }
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Dh:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
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

    // --- word emission -------------------------------------------------
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }

    void emit16(uint16_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        offset_ += 2;
    }

    void emit32(uint32_t v) {
        bytes_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        bytes_.push_back(static_cast<uint8_t>(v & 0xFF));
        offset_ += 4;
    }

    static uint16_t imm16Of(int32_t v) {
        if (v < -32768 || v > 65535) fail("immediate out of 16-bit range");
        return static_cast<uint16_t>(v & 0xFFFF);
    }

    uint16_t evalImm(const Operand &op) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        int32_t v = static_cast<int32_t>(evalExpr(op.expr));
        return imm16Of(v);
    }

    // A plain number (Imm) is used as-is; a label (Mem) gets its full
    // resolved address split into upper (lui) or lower (ori) 16 bits.
    uint16_t evalHiLo(const Operand &op, bool hi, std::string *externalName = nullptr,
                       bool *needsBaseShift = nullptr) {
        if (op.kind == Operand::Reg) fail("expected an immediate or label operand, not a register");
        uint32_t full = evalExpr(op.expr, externalName, needsBaseShift);
        if (op.kind == Operand::Mem) return static_cast<uint16_t>(hi ? (full >> 16) & 0xFFFFu : full & 0xFFFFu);
        return static_cast<uint16_t>(full & 0xFFFFu);
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // --- MIPS instruction formats ---------------------------------------
    // R-type: opcode(6) rs(5) rt(5) rd(5) shamt(5) funct(6).
    void emitR(uint8_t opcode, uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, uint8_t funct) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rs) << 21) |
               (static_cast<uint32_t>(rt) << 16) | (static_cast<uint32_t>(rd) << 11) |
               (static_cast<uint32_t>(shamt) << 6) | funct);
    }
    // I-type: opcode(6) rs(5) rt(5) imm16.
    void emitI(uint8_t opcode, uint8_t rs, uint8_t rt, uint16_t imm) {
        emit32((static_cast<uint32_t>(opcode) << 26) | (static_cast<uint32_t>(rs) << 21) |
               (static_cast<uint32_t>(rt) << 16) | imm);
    }
    // J-type: opcode(6) instr_index(26); target address shifted right
    // 2 (word-aligned) and truncated to 26 bits.
    void emitJ(uint8_t opcode, uint32_t targetAddr) {
        emit32((static_cast<uint32_t>(opcode) << 26) | ((targetAddr >> 2) & 0x03FFFFFFu));
    }

    void encodeRRR(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        // dialect order: mnemonic rd,rs,rt
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitR(0, rs, rt, rd, 0, funct);
    }
    // SLLV/SRLV - dialect order: mnemonic rd,rt,rs (rs is the shift-count register).
    void encodeShiftV(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 2, mnemonic), mnemonic);
        emitR(0, rs, rt, rd, 0, funct);
    }
    // SLL - dialect order: mnemonic rd,rt,shamt (shamt a literal 0-31 constant).
    void encodeShiftImm(uint8_t funct, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint8_t shamt = static_cast<uint8_t>(evalImm(requireOperand(ops, 2, mnemonic)) & 0x1F);
        emitR(0, 0, rt, rd, shamt, funct);
    }
    // ADDIU/SLTIU - dialect order: mnemonic rt,rs,imm; imm is always a plain literal.
    void encodeImmArith(uint8_t opcode, const std::vector<Operand> &ops, const std::string &mnemonic) {
        uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
        uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
        uint16_t imm = evalImm(requireOperand(ops, 2, mnemonic));
        emitI(opcode, rs, rt, imm);
    }

    // LUI/ORI's immediate operand: a label operand gets a relocation
    // against the reserved pseudo-symbol ".." (object assembles at
    // `org 0`), plus one against its own name if undefined. lui must
    // immediately precede ori for the same address so the linker's Hi
    // patch can find the paired Lo site at a fixed +4 offset.
    void encodeHiLo(uint8_t opcode, uint8_t rs, uint8_t rt, const Operand &immOp, bool hi, uint16_t relocKind) {
        std::string externalName;
        bool needsBaseShift = false;
        uint16_t imm = evalHiLo(immOp, hi, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_ && immOp.kind == Operand::Mem) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", relocKind});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, relocKind});
        }
        emitI(opcode, rs, rt, imm);
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "nop") {
            if (!ops.empty()) fail("'nop' takes no operand");
            emit32(0);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> rrr = {
            {"addu", 0x21}, {"subu", 0x23}, {"and", 0x24}, {"or", 0x25}, {"xor", 0x26}, {"nor", 0x27},
            {"slt", 0x2A},  {"sltu", 0x2B},
        };
        if (auto it = rrr.find(mnemonic); it != rrr.end()) {
            encodeRRR(it->second, ops, mnemonic);
            return;
        }
        if (mnemonic == "sllv" || mnemonic == "srlv" || mnemonic == "srav") {
            uint8_t funct = mnemonic == "sllv" ? 0x04 : mnemonic == "srlv" ? 0x06 : 0x07;
            encodeShiftV(funct, ops, mnemonic);
            return;
        }
        if (mnemonic == "sll" || mnemonic == "sra" || mnemonic == "srl") {
            uint8_t funct = mnemonic == "sll" ? 0x00 : mnemonic == "srl" ? 0x02 : 0x03;
            encodeShiftImm(funct, ops, mnemonic);
            return;
        }
        if (mnemonic == "addiu") {
            encodeImmArith(0x09, ops, mnemonic);
            return;
        }
        if (mnemonic == "sltiu") {
            encodeImmArith(0x0B, ops, mnemonic);
            return;
        }
        if (mnemonic == "ori") {
            uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            encodeHiLo(0x0D, rs, rt, requireOperand(ops, 2, mnemonic), /*hi=*/false, kMipsRelocLo);
            return;
        }
        if (mnemonic == "lui") {
            uint8_t rt = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            encodeHiLo(0x0F, 0, rt, requireOperand(ops, 1, mnemonic), /*hi=*/true, kMipsRelocHi);
            return;
        }
        // Byte/halfword load (sign- or zero-extending) and store
        // (LB/LBU/LH/LHU/SB/SH); disp defaults to 0 when omitted.
        static const std::unordered_map<std::string, uint8_t> sized = {
            {"ldindb", 0x20}, {"ldindbu", 0x24}, {"ldindh", 0x21},
            {"ldindhu", 0x25}, {"stindb", 0x28}, {"stindh", 0x29},
        };
        if (auto it = sized.find(mnemonic); it != sized.end()) {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            int16_t disp = 0;
            if (ops.size() > 2) disp = static_cast<int16_t>(evalExpr(requireOperand(ops, 2, mnemonic).expr));
            emitI(it->second, base, rt, static_cast<uint16_t>(disp));
            return;
        }
        // LW/SW underneath; disp defaults to 0 when omitted.
        if (mnemonic == "ldind" || mnemonic == "stind") {
            uint8_t base = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            int16_t disp = 0;
            if (ops.size() > 2) disp = static_cast<int16_t>(evalExpr(requireOperand(ops, 2, mnemonic).expr));
            emitI(mnemonic == "ldind" ? 0x23 : 0x2B, base, rt, static_cast<uint16_t>(disp));
            return;
        }
        if (mnemonic == "mult" || mnemonic == "div" || mnemonic == "multu" || mnemonic == "divu") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            uint8_t funct = mnemonic == "mult" ? 0x18 : mnemonic == "multu" ? 0x19 : mnemonic == "div" ? 0x1A : 0x1B;
            emitR(0, rs, rt, 0, 0, funct);
            return;
        }
        if (mnemonic == "mflo" || mnemonic == "mfhi") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emitR(0, 0, 0, rd, 0, mnemonic == "mflo" ? 0x12 : 0x10);
            return;
        }
        if (mnemonic == "jr") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emitR(0, rs, 0, 0, 0, 0x08);
            return;
        }
        // JALR: jumps to the address in rs, leaves return address in rd;
        // used when the call target isn't a compile-time label.
        if (mnemonic == "jalr") {
            uint8_t rd = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rs = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emitR(0, rs, 0, rd, 0, 0x09);
            return;
        }
        if (mnemonic == "j" || mnemonic == "jal") {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            // J/JAL's target is a pseudo-absolute address field, so even
            // an intra-function jump needs the ".." base-shift relocation.
            std::string externalName;
            bool needsBaseShift = false;
            uint32_t targetAddr = evalExpr(target.expr, &externalName, &needsBaseShift);
            if (stage_ == 2 && allowExternal_) {
                if (needsBaseShift) relocations_.push_back({offset_, "..", kMipsRelocJump});
                if (!externalName.empty()) relocations_.push_back({offset_, externalName, kMipsRelocJump});
            }
            emitJ(mnemonic == "j" ? 0x02 : 0x03, targetAddr);
            return;
        }
        if (mnemonic == "beq" || mnemonic == "bne") {
            uint8_t rs = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t rt = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            const Operand &target = requireOperand(ops, 2, mnemonic);
            if (target.kind != Operand::Mem) fail("'" + mnemonic + "': target must be a label");
            uint32_t targetAddr = evalExpr(target.expr);
            // Displacement is relative to the delay-slot instruction's
            // address (branch address + 4), measured in words.
            uint32_t branchEnd = orgBase_ + offset_ + 4;
            int32_t rel = (static_cast<int32_t>(targetAddr) - static_cast<int32_t>(branchEnd)) / 4;
            if (stage_ == 2 && (rel < -32768 || rel > 32767)) fail("branch target out of 16-bit range");
            emitI(mnemonic == "beq" ? 0x04 : 0x05, rs, rt, static_cast<uint16_t>(static_cast<int16_t>(rel)));
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
    obj.machineType = kMipsCoffMachine;
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
        obj.relocations.push_back({r.offset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleMips(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleMipsObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
