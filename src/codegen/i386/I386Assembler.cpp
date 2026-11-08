#include "I386Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I386Assembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 ---
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// Reg carries a register index and whether it's `al` (is8), the only 8-bit register this
// dialect uses. Mem is one of three shapes: [label], [ebx] (memHasEbx), or [label+ebx]
// (both, expr holding the label). Imm is a literal number or a bare symbol branch target.
struct Operand {
    enum Kind { Reg, Mem, Imm } kind = Imm;
    int reg = -1;
    bool is8 = false;   // al
    bool is16 = false;  // ax
    // "byte [...]"/"word [...]" size override for a narrower-than-int load/store; None
    // means the plain 32-bit form.
    enum MemSize { None, Byte, Word } memSize = None;
    bool memHasEbx = false;
    bool memHasEbp = false;
    bool memHasDisp = false;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Org, Db, Dw, Dd, Instr } kind = None;
    std::vector<std::vector<Syllable>> exprList;  // Org (1) / Db,Dw,Dd (N)
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

// `cl` shares ecx's index; `al` is handled separately in buildOperand() since it needs its
// own 8-bit immediate/instruction forms, not just a shared index.
int regIndex(const std::string &nameLower) {
    static const std::unordered_map<std::string, int> regs = {
        {"eax", 0}, {"ecx", 1}, {"edx", 2}, {"ebx", 3}, {"esp", 4}, {"ebp", 5}, {"esi", 6}, {"edi", 7}, {"cl", 1},
    };
    auto it = regs.find(nameLower);
    return it != regs.end() ? it->second : -1;
}

// Classifies `[...]` and plain-expr operand text into the shapes in Operand's comment.
Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    Operand op;
    if (ctx->LBRACKET()) {
        op.kind = Operand::Mem;
        if (ctx->size) {
            std::string sizeLower = toLower(ctx->size->getText());
            if (sizeLower == "byte") {
                op.memSize = Operand::Byte;
            } else if (sizeLower == "word") {
                op.memSize = Operand::Word;
            } else {
                fail("unrecognized memory operand size '" + ctx->size->getText() + "'");
            }
        }
        auto syls = exprToSyllables(ctx->expr());
        auto isEbx = [](const Syllable &s) { return s.kind == SylKind::Symbol && toLower(s.text) == "ebx"; };
        auto isEbp = [](const Syllable &s) { return s.kind == SylKind::Symbol && toLower(s.text) == "ebp"; };
        if (syls.size() == 1 && isEbx(syls[0])) {
            op.memHasEbx = true;
            op.memHasDisp = false;
        } else if (syls.size() == 2 && isEbx(syls[0]) && !isEbx(syls[1])) {
            op.memHasEbx = true;
            op.memHasDisp = true;
            op.expr = {syls[1]};
        } else if (syls.size() == 2 && isEbx(syls[1]) && !isEbx(syls[0])) {
            op.memHasEbx = true;
            op.memHasDisp = true;
            op.expr = {syls[0]};
        } else if (syls.size() == 2 && isEbp(syls[0]) && !isEbp(syls[1])) {
            op.memHasEbp = true;
            op.memHasDisp = true;
            op.expr = {syls[1]};
        } else if (std::none_of(syls.begin(), syls.end(),
                                [&](const Syllable &sy) { return isEbx(sy) || isEbp(sy); })) {
            // No register at all: an absolute address, e.g. `[label]` or `[label+4]`.
            op.memHasEbx = false;
            op.memHasDisp = true;
            op.expr = syls;
        } else {
            fail("unrecognized memory operand shape");
        }
        return op;
    }
    auto syls = exprToSyllables(ctx->expr());
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        std::string lower = toLower(syls[0].text);
        if (lower == "al") {
            op.kind = Operand::Reg;
            op.reg = 0;
            op.is8 = true;
            return op;
        }
        if (lower == "ax") {
            op.kind = Operand::Reg;
            op.reg = 0;
            op.is16 = true;
            return op;
        }
        if (int r = regIndex(lower); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
    }
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

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    if (dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) fail("'='-style equates aren't part of this dialect");
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
    if (mnemonic == "db" || mnemonic == "dw" || mnemonic == "dd") {
        line.kind = mnemonic == "db" ? Line::Db : mnemonic == "dw" ? Line::Dw : Line::Dd;
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

// symbolName is the unresolved reference, or the reserved pseudo-symbol ".." for a
// same-file address that needs a base-shift once linked.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ---
// `org` is called exactly once, at the top: one flat image, no section switching.
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
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    uint32_t currentAddr() const { return orgBase_ + static_cast<uint32_t>(bytes_.size()); }

    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = currentAddr();
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Dd:
                // Routed through emitAbs32() so a symbol gets the same relocation treatment
                // as a label used as a 32-bit immediate.
                for (const auto &e : line.exprList) emitAbs32(e);
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // Any local address reference, not just an external one, needs a relocation here once
    // allowExternal_ is set.
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

    // --- byte/word/dword emission --------------------------------
    void emit8(uint8_t b) { bytes_.push_back(b); }
    void emit16(uint16_t w) {
        emit8(static_cast<uint8_t>(w & 0xFF));
        emit8(static_cast<uint8_t>((w >> 8) & 0xFF));
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>(v & 0xFFFF));
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
    }

    static uint8_t requireReg(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &requireOperand(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // Encodes one of the three memory-operand shapes as an i386 ModRM [+disp32] pair; `reg`
    // carries the instruction's other register field.
    void emitModRMMem(const Operand &mem, uint8_t reg) {
        if (mem.memHasEbx && !mem.memHasDisp) {
            emit8(static_cast<uint8_t>(0x00 | (reg << 3) | 0x03));  // mod=00, rm=011 (ebx)
            return;
        }
        if (mem.memHasEbx && mem.memHasDisp) {
            emit8(static_cast<uint8_t>(0x80 | (reg << 3) | 0x03));  // mod=10, rm=011 (ebx), disp32 follows
            emitAbs32(mem.expr);
            return;
        }
        if (mem.memHasEbp) {
            // mod=10, rm=101 (ebp); mod=01 (8-bit disp) is never used since addr()
            // (I386Codegen.h) never emits a zero displacement.
            emit8(static_cast<uint8_t>(0x80 | (reg << 3) | 0x05));
            emitAbs32(mem.expr);
            return;
        }
        // direct [label]: mod=00, rm=101, disp32-only addressing
        emit8(static_cast<uint8_t>(0x00 | (reg << 3) | 0x05));
        emitAbs32(mem.expr);
    }

    // Every absolute 32-bit address reference passes through here. Records a relocation
    // against the ".." pseudo-symbol when the reference needs this object's own base added,
    // plus one against the real symbol name if it's not yet defined in this object.
    void emitAbs32(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset(), "..", kI386RelocAbs32});
            if (!externalName.empty()) relocations_.push_back({offset(), externalName, kI386RelocAbs32});
        }
        emit32(v);
    }

    uint32_t offset() const { return static_cast<uint32_t>(bytes_.size()); }

    // Exactly the mnemonic/operand-shape combinations I386Codegen.cpp emits, not a general
    // i386 encoder.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "ret" || mnemonic == "hlt" || mnemonic == "cli" || mnemonic == "sti" || mnemonic == "cdq") {
            static const std::unordered_map<std::string, uint8_t> simple = {
                {"ret", 0xC3}, {"hlt", 0xF4}, {"cli", 0xFA}, {"sti", 0xFB}, {"cdq", 0x99},
            };
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(simple.at(mnemonic));
            return;
        }
        if (mnemonic == "push" || mnemonic == "pop") {
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(static_cast<uint8_t>((mnemonic == "push" ? 0x50 : 0x58) | r));
            return;
        }
        if (mnemonic == "not" || mnemonic == "neg" || mnemonic == "mul" || mnemonic == "imul" ||
            mnemonic == "div" || mnemonic == "idiv") {
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            static const std::unordered_map<std::string, uint8_t> ext = {
                {"not", 2}, {"neg", 3}, {"mul", 4}, {"imul", 5}, {"div", 6}, {"idiv", 7},
            };
            emit8(0xF7);
            emit8(static_cast<uint8_t>(0xC0 | (ext.at(mnemonic) << 3) | r));
            return;
        }
        if (mnemonic == "shl" || mnemonic == "shr" || mnemonic == "sar") {
            // Two shapes: variable-count register form (<reg>,cl) and immediate-count form
            // (<reg>,<N>).
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            const Operand &count = requireOperand(ops, 1, mnemonic);
            uint8_t ext = mnemonic == "shl" ? 4 : mnemonic == "shr" ? 5 : 7;
            if (count.kind == Operand::Reg && count.reg == 1) {
                emit8(0xD3);
                emit8(static_cast<uint8_t>(0xC0 | (ext << 3) | r));
                return;
            }
            if (count.kind == Operand::Imm) {
                emit8(0xC1);
                emit8(static_cast<uint8_t>(0xC0 | (ext << 3) | r));
                emit8(static_cast<uint8_t>(evalExpr(count.expr) & 0xFF));
                return;
            }
            fail("'" + mnemonic + "': count operand must be cl or a literal");
        }
        if (mnemonic == "mov") {
            const Operand &dst = requireOperand(ops, 0, mnemonic);
            const Operand &src = requireOperand(ops, 1, mnemonic);
            if (dst.kind == Operand::Reg && dst.is8 && src.kind == Operand::Imm) {
                emit8(static_cast<uint8_t>(0xB0 | dst.reg));
                emit8(static_cast<uint8_t>(evalExpr(src.expr) & 0xFF));
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Imm) {
                emit8(static_cast<uint8_t>(0xB8 | dst.reg));
                emitAbs32(src.expr);
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Reg) {
                emit8(0x89);
                emit8(static_cast<uint8_t>(0xC0 | (src.reg << 3) | dst.reg));
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Mem && src.memSize == Operand::None) {
                // [label] with no base register uses the moffs32 shortcut, valid only for
                // eax; every other shape goes through the general ModRM form.
                if (dst.reg == 0 && !src.memHasEbx && !src.memHasEbp) {
                    emit8(0xA1);
                    emitAbs32(src.expr);
                    return;
                }
                emit8(0x8B);
                emitModRMMem(src, dst.reg);
                return;
            }
            if (dst.kind == Operand::Mem && dst.memSize == Operand::None && src.kind == Operand::Reg) {
                if (src.reg == 0 && !dst.memHasEbx && !dst.memHasEbp) {
                    emit8(0xA3);
                    emitAbs32(dst.expr);
                    return;
                }
                emit8(0x89);
                emitModRMMem(dst, src.reg);
                return;
            }
            // r/m8,r8 form (88 /r): writes only the one byte.
            if (dst.kind == Operand::Mem && dst.memSize == Operand::Byte && src.kind == Operand::Reg && src.is8) {
                emit8(0x88);
                emitModRMMem(dst, static_cast<uint8_t>(src.reg));
                return;
            }
            // r/m16,r16 form: the 32-bit 89 /r form with a 0x66 operand-size override prefix.
            if (dst.kind == Operand::Mem && dst.memSize == Operand::Word && src.kind == Operand::Reg && src.is16) {
                emit8(0x66);
                emit8(0x89);
                emitModRMMem(dst, static_cast<uint8_t>(src.reg));
                return;
            }
            fail("'mov': unsupported operand combination in this dialect");
        }
        if (mnemonic == "movzx" || mnemonic == "movsx") {
            // 0F B6/B7 zero-extend, 0F BE/BF sign-extend; byte vs word selects B6/BE vs
            // B7/BF.
            const Operand &dst = requireOperand(ops, 0, mnemonic);
            const Operand &src = requireOperand(ops, 1, mnemonic);
            if (dst.kind != Operand::Reg || dst.is8 || dst.is16) {
                fail("'" + mnemonic + "': destination must be a full 32-bit register");
            }
            bool zx = mnemonic == "movzx";
            if (src.kind == Operand::Reg && (src.is8 || src.is16)) {
                emit8(0x0F);
                emit8(src.is8 ? (zx ? 0xB6 : 0xBE) : (zx ? 0xB7 : 0xBF));
                emit8(static_cast<uint8_t>(0xC0 | (dst.reg << 3) | src.reg));
                return;
            }
            if (src.kind != Operand::Mem || src.memSize == Operand::None) {
                fail("'" + mnemonic + "': source must be al/ax or a sized memory operand (byte/word [...])");
            }
            emit8(0x0F);
            emit8(src.memSize == Operand::Byte ? (zx ? 0xB6 : 0xBE) : (zx ? 0xB7 : 0xBF));
            emitModRMMem(src, static_cast<uint8_t>(dst.reg));
            return;
        }
        if (mnemonic == "lea") {
            // 8D /r: computes an effective address without touching memory.
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Mem) {
                fail("'lea' needs a register destination and a memory source in this dialect");
            }
            emit8(0x8D);
            emitModRMMem(ops[1], static_cast<uint8_t>(ops[0].reg));
            return;
        }
        if (mnemonic == "add" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 0, mnemonic).reg != 0 && requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // 81 /0 id (full imm32 add), for any register but eax (which has its own
            // shorter 05-opcode form below).
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0x81);
            emit8(static_cast<uint8_t>(0xC0 | (0 << 3) | dst));
            emitAbs32(requireOperand(ops, 1, mnemonic).expr);
            return;
        }
        if (mnemonic == "sub" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // 83 /5 ib (sign-extended imm8 sub); every frame this backend builds fits under
            // 128 dwords.
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint32_t imm = evalExpr(requireOperand(ops, 1, mnemonic).expr);
            if (imm > 0x7F && imm < 0xFFFFFF80) fail("'sub': immediate operand must fit in a signed byte");
            emit8(0x83);
            emit8(static_cast<uint8_t>(0xC0 | (5 << 3) | dst));
            emit8(static_cast<uint8_t>(imm & 0xFF));
            return;
        }
        if (mnemonic == "out") {
            // E6: imm8 port, al source.
            const Operand &port = requireOperand(ops, 0, mnemonic);
            const Operand &src = requireOperand(ops, 1, mnemonic);
            if (port.kind != Operand::Imm) fail("'out': port must be a literal");
            if (src.kind != Operand::Reg || !src.is8) fail("'out': source must be al");
            emit8(0xE6);
            emit8(static_cast<uint8_t>(evalExpr(port.expr) & 0xFF));
            return;
        }
        if (mnemonic == "add" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 0, mnemonic).reg == 0 && requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // 05: eax-specific short immediate add form.
            emit8(0x05);
            emitAbs32(requireOperand(ops, 1, mnemonic).expr);
            return;
        }
        if (mnemonic == "and" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0x81);
            emit8(static_cast<uint8_t>(0xC0 | (4 << 3) | dst));
            emitAbs32(requireOperand(ops, 1, mnemonic).expr);
            return;
        }
        if (mnemonic == "add" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 1, mnemonic).kind == Operand::Mem) {
            // 03 /r: register+memory add form.
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0x03);
            emitModRMMem(requireOperand(ops, 1, mnemonic), dst);
            return;
        }
        if (mnemonic == "cmp" && requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // 83 /7 ib (sign-extended imm8 cmp).
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint32_t imm = evalExpr(requireOperand(ops, 1, mnemonic).expr);
            if (imm > 0x7F && imm < 0xFFFFFF80) fail("'cmp': immediate operand must fit in a signed byte");
            emit8(0x83);
            emit8(static_cast<uint8_t>(0xC0 | (7 << 3) | dst));
            emit8(static_cast<uint8_t>(imm & 0xFF));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluRegReg = {
            {"add", 0x01}, {"sub", 0x29}, {"cmp", 0x39}, {"and", 0x21}, {"or", 0x09}, {"xor", 0x31},
            {"adc", 0x11}, {"sbb", 0x19},  // add/sub-with-carry, used for long-long add/sub
        };
        if (auto it = aluRegReg.find(mnemonic); it != aluRegReg.end()) {
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t src = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit8(it->second);
            emit8(static_cast<uint8_t>(0xC0 | (src << 3) | dst));
            return;
        }
        if (mnemonic == "call" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg) {
            // FF /2: near-indirect call through a register.
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0xFF);
            emit8(static_cast<uint8_t>(0xC0 | (2 << 3) | r));
            return;
        }
        if (mnemonic == "call" || mnemonic == "jmp") {
            // E8/E9 rel32. A same-object target's displacement is base-shift-invariant, so
            // a relocation is only needed when the target isn't defined in this object.
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Imm) fail("'" + mnemonic + "': target must be a label");
            std::string externalName;
            uint32_t targetAddr = evalExpr(target.expr, &externalName);
            emit8(mnemonic == "call" ? 0xE8 : 0xE9);
            if (stage_ == 2 && allowExternal_ && !externalName.empty()) {
                relocations_.push_back({offset(), externalName, kI386RelocRel32});
                emit32(0);
                return;
            }
            emit32(targetAddr - (currentAddr() + 4));
            return;
        }
        static const std::unordered_map<std::string, uint8_t> jcc = {
            {"je", 0x84}, {"jne", 0x85}, {"jl", 0x8C}, {"jle", 0x8E}, {"jg", 0x8F}, {"jge", 0x8D},
            {"jb", 0x82}, {"jae", 0x83}, {"jbe", 0x86}, {"ja", 0x87},  // unsigned counterparts
        };
        if (auto it = jcc.find(mnemonic); it != jcc.end()) {
            const Operand &target = requireOperand(ops, 0, mnemonic);
            if (target.kind != Operand::Imm) fail("'" + mnemonic + "': target must be a label");
            uint32_t targetAddr = evalExpr(target.expr);
            emit8(0x0F);
            emit8(it->second);
            emit32(targetAddr - (currentAddr() + 4));
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile from a completed object-mode Assembler.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kI386CoffMachine;
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, r.kind});
    }
    return obj;
}

}  // namespace

std::string assembleI386(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleI386Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
