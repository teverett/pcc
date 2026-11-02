#include "Pdp11Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp11Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same Syllable/Group shape as every other backend's own assembler -
// see M68kAssembler.cpp's own comment.
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// One operand, classified from a bare expression - see
// Pdp11Assembler.h's own comment on why there's no real MACRO-11
// `(Rn)`/`#n` syntax here. `Reg` comes from recognizing a bare
// `r0`-`r7`/`sp`/`pc` identifier; `Imm` from a bare (optionally
// signed) number; anything else (a label, or `label+N`) is `Mem` -
// PC-relative addressing to the value stored there.
struct Operand {
    enum Kind { Reg, Imm, Mem } kind = Imm;
    int reg = -1;
    std::vector<Syllable> expr;  // Imm/Mem
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Instr } kind = None;
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

// `r0`-`r7` (case-insensitive, matching how mnemonics are lowercased
// too), plus the `sp`/`pc` aliases for `r6`/`r7` - returns -1 if
// `name` is neither.
int regNumber(const std::string &lower) {
    if (lower == "sp") return 6;
    if (lower == "pc") return 7;
    if (lower.size() == 2 && lower[0] == 'r' && lower[1] >= '0' && lower[1] <= '7') return lower[1] - '0';
    return -1;
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect");
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
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dw' operand can't be bracketed");
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

// One pending fixup `assemblePdp11Object()` (see Pdp11Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - `byteOffset` into `bytes()`, `symbolName` the reference
// that couldn't be resolved locally, `kind` either
// `kPdp11RelocAbs16` (`lea`'s own absolute extension word) or
// `kPdp11RelocRel16` (every other, PC-relative, bare-label operand).
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
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror ../m68k/M68kAssembler.cpp's
    // own `evalExpr` exactly - see Pdp11Assembler.h's own header
    // comment on why only `lea`'s own absolute reference ever needs
    // `needsBaseShift` (an ordinary PC-relative `Mem` operand never
    // does - see `emitEAExtension`'s own comment below).
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

    // --- general ("double operand"-style) EA field --------------------
    // Reg direct (000 000-111), or - via PC (R7) - immediate (mode
    // 010, "(PC)+": the extension word that follows is the literal
    // value) or relative (mode 110, "X(PC)": the extension word is a
    // *displacement* from the extension word's own address to the
    // real target - see Pdp11Assembler.h's own header comment). No
    // other addressing mode (register-deferred, autoincrement,
    // indexed-by-a-non-PC-register, ...) is ever produced by this
    // classifier - Pdp11Codegen.cpp's own dedicated mnemonics
    // (`ldind`/`stind`/`push`/`pop`/`lea`) cover every one of those.
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
        }
        fail("internal: unhandled operand kind");
    }
    // Emits `op`'s own extension word, for whichever EA mode needs
    // one (Imm/Mem both do - see classifyEA's own comment). Must be
    // called exactly when the program counter sits where that
    // extension word will actually land, since Mem's own value is a
    // *relative* displacement computed from that address.
    void emitEAExtension(const Operand &op) {
        if (op.kind == Operand::Imm) {
            emit16(static_cast<uint16_t>(evalExpr(op.expr)));
        } else if (op.kind == Operand::Mem) {
            // PC-relative (mode 067) - target and extAddr are both
            // already final, zero-based, position-independent
            // offsets *within this one object* once resolved locally,
            // so a locally-resolved reference needs no relocation at
            // all (see Pdp11Assembler.h's own `kPdp11RelocRel16`
            // comment): only an `externalName` (never a
            // `needsBaseShift`) is ever requested here.
            std::string externalName;
            uint32_t target = evalExpr(op.expr, &externalName, nullptr);
            uint32_t extAddr = orgBase_ + offset_;
            if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
                relocations_.push_back({offset_, externalName, kPdp11RelocRel16});
            }
            emit16(static_cast<uint16_t>(target - extAddr));
        }
    }

    // The `opcode SS DD` double-operand family (MOV/CMP/BIC/BIS/ADD/
    // SUB - see encodeInstr's own table): a 4-bit opcode, a 6-bit
    // source EA, a 6-bit destination EA, each with its own extension
    // word if it needs one (source's extension, if any, always comes
    // first - matching real PDP-11 object code order, since the
    // source operand is evaluated before the destination).
    void encodeTwoOp(uint16_t opcodeBase, const Operand &src, const Operand &dst, bool writesDst,
                      const std::string &mnemonic) {
        if (writesDst && dst.kind == Operand::Imm) fail("'" + mnemonic + "': an immediate can't be a destination");
        EA s = classifyEA(src);
        EA d = classifyEA(dst);
        emit16(static_cast<uint16_t>(opcodeBase | (s.mode << 9) | (s.reg << 6) | (d.mode << 3) | d.reg));
        emitEAExtension(src);
        emitEAExtension(dst);
    }

    // The single-operand family (CLC aside - see below - every one
    // of these takes one general EA): CLR/COM/NEG/TST/ASL/ASR/ROR/SXT
    // - Pdp11Codegen.cpp only ever passes a bare register operand,
    // but the encoding itself is general.
    void encodeOneOp(uint16_t opcodeBase, const Operand &op) {
        EA e = classifyEA(op);
        emit16(static_cast<uint16_t>(opcodeBase | (e.mode << 3) | e.reg));
        emitEAExtension(op);
    }

    // The EIS `opcode Reg,<ea>` family: a fixed 3-bit register field
    // (MUL/DIV's own accumulator register, or XOR's own source
    // register) plus one general EA.
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

    // `lea label,Rn` - loads the label's own absolute address (never
    // a relative displacement the way an ordinary Mem operand's own
    // extension word is - see Pdp11Assembler.h's own header comment)
    // into Rn, via the exact same PC-immediate encoding as a numeric
    // Imm operand, just with the extension word holding a label's
    // value instead of a literal one.
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

    // Branch-offset family (BR/BEQ/BNE/BLT/BLE/BGT/BGE): an 8-bit
    // signed word offset, relative to the address right after this
    // one-word instruction - see Pdp11Codegen.h's own header comment
    // on why Pdp11Codegen.cpp never emits one of these with a target
    // more than ±128 words away.
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

    // The full instruction vocabulary ../Pdp11Codegen.cpp ever emits -
    // see Pdp11Assembler.h's own header comment. Not a general PDP-11
    // encoder: exactly these mnemonic/operand-shape combinations.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        static const std::unordered_map<std::string, uint16_t> twoOpWrite = {
            {"mov", 0x1000}, {"add", 0x6000}, {"sub", 0xE000}, {"bic", 0x4000}, {"bis", 0x5000},
        };
        if (auto it = twoOpWrite.find(mnemonic); it != twoOpWrite.end()) {
            encodeTwoOp(it->second, requireOperand(ops, 0, mnemonic), requireOperand(ops, 1, mnemonic),
                        /*writesDst=*/true, mnemonic);
            return;
        }
        if (mnemonic == "cmp") {
            // CMP's own destination is read-only (it only ever
            // updates the condition codes - see Pdp11Codegen.h's own
            // header comment on its unusual `src - dst` flag
            // convention), so an Imm destination is perfectly legal
            // hardware-wise - unlike MOV/ADD/SUB/BIC/BIS, which
            // really do write theirs.
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
            encodeRegEa(mnemonic == "mul" ? 0x7000 : 0x7400, rn, requireOperand(ops, 0, mnemonic));
            return;
        }
        static const std::unordered_map<std::string, uint16_t> oneOp = {
            {"com", 0x0A40}, {"neg", 0x0B00}, {"tst", 0x0BC0}, {"ror", 0x0C00},
            {"asr", 0x0C80}, {"asl", 0x0CC0}, {"sxt", 0x0DC0},
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
        // Push/pop onto the real hardware stack (SP/R6) - MOV
        // Rn,-(SP) / MOV (SP)+,Rn - spelled as their own mnemonics,
        // not a generic autoincrement/autodecrement operand (see
        // Pdp11Assembler.h's own header comment).
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
        // (Rn) register-deferred, spelled as its own mnemonics rather
        // than a generic bracketed operand - see Pdp11Assembler.h's
        // own header comment.
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
        if (mnemonic == "jsr") {
            // JSR PC,label - the "link register" is always PC here
            // (a plain call, saving only the return address, exactly
            // the m68k backend's own `jsr`/`rts` role).
            EA e = classifyEA(requireOperand(ops, 0, mnemonic));
            emit16(static_cast<uint16_t>(0x0800 | (7 << 6) | (e.mode << 3) | e.reg));
            emitEAExtension(requireOperand(ops, 0, mnemonic));
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

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see Pdp11Assembler.h's own
// `assemblePdp11Object` comment, and ../m68k/M68kAssembler.cpp's own
// `buildObjectFile` for the identical algorithm this is ported from.
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

}  // namespace pcc
