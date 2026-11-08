#include <set>
#include "I8086Assembler.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"
#include "fileformat/coff/CoffFormat.h"
#include "fileformat/elf/ElfFormat.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I8086Assembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 -----------------------

// `Group` is a parenthesized sub-expression (NASM's "510-($-$$)" idiom, used for
// boot-sector/placement padding). Its syllables evaluate recursively to one combined
// value, which the outer sign then adds/subtracts - letting a group be negated as a
// whole ("N-(A-B)" = N-A+B).
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;            // Number/Symbol
    std::vector<Syllable> group;  // Group
};

// A parsed operand. Mem covers the addressing-mode shapes codegen emits: `[label]`
// (memHasBx=false), `[bx]` (memHasBx, !memHasDisp), `[label+bx]` (memHasBx+memHasDisp),
// or `[bp+-disp]` (memHasBp+memHasDisp, a Local/Param's frame-relative address). Imm
// covers a literal number or a bare symbol used as a jump/call target or pointer value.
struct Operand {
    enum Kind { Reg, SegReg, Mem, Imm } kind = Imm;
    int reg = -1;
    bool memHasBx = false;
    bool memHasBp = false;
    bool memHasDisp = false;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Bits, Org, Dw, Db, Times, Instr } kind = None;
    std::vector<Syllable> timesCount;    // Times
    std::string innerMnemonic;            // Times: the inner mnemonic (always "db")
    std::vector<std::vector<Syllable>> exprList;  // Org (1 entry) / Dw (N) / Times' inner db (1)
    std::string mnemonic;                  // Instr
    std::vector<Operand> operands;          // Instr
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

int regIndex(const std::string &name) {
    static const std::unordered_map<std::string, int> regs = {
        {"ax", 0}, {"cx", 1}, {"dx", 2}, {"bx", 3}, {"sp", 4}, {"bp", 5}, {"si", 6}, {"di", 7},
    };
    auto it = regs.find(name);
    return it != regs.end() ? it->second : -1;
}

int segRegIndex(const std::string &name) {
    static const std::unordered_map<std::string, int> regs = {
        {"es", 0}, {"cs", 1}, {"ss", 2}, {"ds", 3},
    };
    auto it = regs.find(name);
    return it != regs.end() ? it->second : -1;
}

// `[...]` and plain-expr operand forms, classified into the shapes codegen emits.
Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    Operand op;
    if (ctx->LBRACKET()) {
        op.kind = Operand::Mem;
        auto syls = exprToSyllables(ctx->expr());
        // "bx" is matched by name, not position/sign, since it can appear unsigned
        // (bare `[bx]`) or signed (second syllable of `[label+bx]`).
        auto isBx = [](const Syllable &s) { return s.kind == SylKind::Symbol && s.text == "bx"; };
        auto isBp = [](const Syllable &s) { return s.kind == SylKind::Symbol && s.text == "bp"; };
        if (syls.size() == 1 && isBx(syls[0])) {
            op.memHasBx = true;
            op.memHasDisp = false;
        } else if (syls.size() == 2 && isBx(syls[0]) && !isBx(syls[1])) {
            op.memHasBx = true;
            op.memHasDisp = true;
            op.expr = {syls[1]};
        } else if (syls.size() == 2 && isBx(syls[1]) && !isBx(syls[0])) {
            op.memHasBx = true;
            op.memHasDisp = true;
            op.expr = {syls[0]};
        } else if (syls.size() == 2 && isBp(syls[0]) && !isBp(syls[1])) {
            // "[bp+N]"/"[bp-N]": bp always comes first, since codegen only emits it that way.
            op.memHasBp = true;
            op.memHasDisp = true;
            op.expr = {syls[1]};
        } else if (syls.size() == 1 && !isBx(syls[0])) {
            op.memHasBx = false;
            op.memHasDisp = true;
            op.expr = {syls[0]};
        } else {
            fail("unrecognized memory operand shape");
        }
        return op;
    }
    auto syls = exprToSyllables(ctx->expr());
    if (syls.size() == 1 && syls[0].sign == 0 && syls[0].kind == SylKind::Symbol) {
        int r = regIndex(syls[0].text);
        if (r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
        int sr = segRegIndex(syls[0].text);
        if (sr >= 0) {
            op.kind = Operand::SegReg;
            op.reg = sr;
            return op;
        }
    }
    op.kind = Operand::Imm;
    op.expr = syls;
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
    if (dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        fail("assignment statements aren't part of 8086's own dialect - placement/padding here always goes "
             "through 'times', see I8086Codegen.cpp");
    }
    if (auto *times = dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        line.kind = Line::Times;
        line.timesCount = exprToSyllables(times->count);
        line.innerMnemonic = toLower(times->mnemonic->getText());
        for (auto *op : times->operand()) {
            if (op->LBRACKET()) fail("'times' body operand can't be a memory operand here");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    std::string mnemonic = toLower(instr->mnemonic->getText());
    if (mnemonic == "bits") {
        line.kind = Line::Bits;
        return line;
    }
    if (mnemonic == "org") {
        line.kind = Line::Org;
        if (instr->operand().empty() || instr->operand(0)->LBRACKET()) fail("'org' needs a plain address operand");
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
        return line;
    }
    if (mnemonic == "dw") {
        line.kind = Line::Dw;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'dw' operand can't be a memory operand here");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }
    // A `char` array's own byte-packed storage (I8086Codegen.cpp's
    // declareArray()) - top-level, not just as 'times''s own inner
    // mnemonic (see Line::Times above).
    if (mnemonic == "db") {
        line.kind = Line::Db;
        for (auto *op : instr->operand()) {
            if (op->LBRACKET()) fail("'db' operand can't be a memory operand here");
            line.exprList.push_back(exprToSyllables(op->expr()));
        }
        return line;
    }

    line.kind = Line::Instr;
    line.mnemonic = mnemonic;
    for (auto *op : instr->operand()) line.operands.push_back(buildOperand(op));
    return line;
}

// Dynamic_cast dispatch on the labeled-alternative Context subclasses, rather than
// the generated Visitor interface.
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

// A patch the linker still needs to apply, for a symbol this file's labels don't
// define. Abs16 is a direct-address/immediate field needing the symbol's final linked
// address; Rel16 is a jmp/call displacement needing `target - (site+2)`, only
// computable once linked - this file emits a 0x0000 placeholder for it.
// (Values mirrored as kI8086RelocAbs16/Rel16 in I8086Assembler.h - kept in sync by hand.)
enum class RelocKind : uint16_t { Abs16 = 1, Rel16 = 2 };
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    RelocKind kind;
};

// --- Two-pass assembly ----------------------------------------------------
//
// Pass 1 and pass 2 run the exact same encoding logic; pass 1 just discards the bytes
// and counts them, so there's no separate "how big is this" implementation to drift.
class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {}

    // Every conditional jump starts out short; a pass that finds one out of
    // range marks it long, and passes repeat (sizes only ever grow, so this
    // settles) before the final pass emits the bytes.
    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        pass(lines);
        do {
            grew_ = false;
            checkRange_ = true;
            pass(lines);
        } while (grew_);
        stage_ = 2;
        pass(lines);
    }

    const std::vector<uint8_t> &bytes() const { return bytes_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }
    uint32_t orgBase() const { return orgBase_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        offset_ = 0;
        orgBase_ = 0;
        jccIndex_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
            case Line::Bits:
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emitExprAsWord(e);
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
                return;
            case Line::Times: {
                if (line.innerMnemonic != "db") fail("'times' only supports a 'db' body in this dialect");
                uint32_t count = evalExpr(line.timesCount);
                // A negative "times (510-($-$$))" count wraps to a huge uint32_t -
                // reject it rather than silently emitting gigabytes of padding.
                if (count > 0x10000) fail("program too large for a 512-byte boot sector (--format=raw) - use --format=coff instead");
                uint32_t v = line.exprList.empty() ? 0 : evalExpr(line.exprList.at(0));
                for (uint32_t i = 0; i < count; i++) emit8(static_cast<uint8_t>(v));
                return;
            }
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // --- expression evaluation ---------

    // `externalName`, if given, is set to the one external symbol a syllable named
    // (only possible when allowExternal_). `needsBaseShift`, if given, is set true
    // when a syllable resolved to a LOCAL address (a label, $, or $$ - "offset from
    // this object's ORG 0" in object mode); the caller then records the relocation(s),
    // since this function doesn't know the field's byte offset or Abs16/Rel16 kind.
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

    // `wasExternal`: syl named an undefined symbol, allowed through by allowExternal_.
    // `wasLocalAddress`: syl resolved to a position-dependent local value (a label, $,
    // or $$), which needs a relocation in object mode, unlike a plain literal number.
    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
                return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(t));
        }
        if (syl.text == "$") {
            if (allowExternal_ && wasLocalAddress) *wasLocalAddress = true;
            return orgBase_ + offset_;
        }
        if (syl.text == "$$") {
            if (allowExternal_ && wasLocalAddress) *wasLocalAddress = true;
            return orgBase_;
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
    void emit16(uint32_t v) {
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    // Records an Abs16 relocation against the pseudo-symbol ".." when the expression
    // touched a local address (the linker adds this object's real base to it), and/or
    // a real Abs16 relocation against an external symbol - both can apply at once.
    void emitExprAsWord(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", RelocKind::Abs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, RelocKind::Abs16});
        }
        emit16(v);
    }
    static uint8_t modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
        return static_cast<uint8_t>((mod << 6) | (reg << 3) | rm);
    }

    struct MemForm {
        uint8_t mod;
        uint8_t rm;
        bool hasDisp;
    };
    static MemForm memForm(const Operand &op) {
        if (op.memHasBx && !op.memHasDisp) return {0b00, 0b111, false};  // [bx]
        if (op.memHasBx && op.memHasDisp) return {0b10, 0b111, true};    // [bx+disp16]
        if (op.memHasBp) return {0b10, 0b110, true};                     // [bp+disp16]
        if (!op.memHasBx && op.memHasDisp) return {0b00, 0b110, true};   // [disp16] (direct)
        fail("unrecognized memory operand");
    }

    void emitModRegRM(uint8_t opcode, uint8_t mod, uint8_t reg, uint8_t rm) {
        emit8(opcode);
        emit8(modrm(mod, reg, rm));
    }
    void emitMemInstr(uint8_t opcode, int regField, const Operand &memOp) {
        MemForm mf = memForm(memOp);
        emit8(opcode);
        emit8(modrm(mf.mod, static_cast<uint8_t>(regField), mf.rm));
        if (mf.hasDisp) emitExprAsWord(memOp.expr);
    }

    // Near jmp/call (opcode + rel16) and short Jcc (opcode + rel8) are both
    // fixed-size regardless of distance; only pass 2's rel8 range is checked
    // (pass 1's labels aren't all resolved yet, so only the size matters there).
    void emitRel16(uint8_t opcode, const Operand &target) {
        uint32_t endAddr = orgBase_ + offset_ + 3;
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        emit8(opcode);
        if (stage_ == 2 && !externalName.empty()) {
            // Only the linker can compute `target - (site+2)` once merged -
            // leave a placeholder and let it do the subtraction.
            relocations_.push_back({offset_, externalName, RelocKind::Rel16});
            emit16(0);
        } else {
            int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
            emit16(static_cast<uint16_t>(rel));
        }
    }
    // A conditional jump: `opcode rel8`, or, once marked long (see run()), the
    // opposite condition (the opcode with its low bit flipped) skipping a near
    // `jmp` to the target.
    void emitRel8(uint8_t opcode, const Operand &target) {
        size_t index = jccIndex_++;
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        // A conditional jump's target is always internal control flow, never cross-file.
        if (stage_ == 2 && !externalName.empty()) {
            fail("conditional jump target '" + externalName + "' is external - short jumps can't cross file boundaries");
        }
        if (longJcc_.count(index)) {
            emit8(static_cast<uint8_t>(opcode ^ 1));
            emit8(3);
            uint32_t endAddr = orgBase_ + offset_ + 3;
            emit8(0xE9);
            emit16(static_cast<uint16_t>(static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr)));
            return;
        }
        uint32_t endAddr = orgBase_ + offset_ + 2;
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (rel < -128 || rel > 127) {
            if (stage_ == 2) fail("conditional jump target out of short (rel8) range");
            if (checkRange_) {
                longJcc_.insert(index);
                grew_ = true;
            }
        }
        emit8(opcode);
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    static void requireReg(const std::vector<Operand> &ops, size_t n) {
        if (ops.size() != n) fail("wrong operand count");
        if (ops[0].kind != Operand::Reg) fail("expected a register operand");
    }
    static bool isLiteralNumber(const Operand &op, const std::string &text) {
        return op.kind == Operand::Imm && op.expr.size() == 1 && op.expr[0].sign == 0 &&
               op.expr[0].kind == SylKind::Number && op.expr[0].text == text;
    }
    static bool isBareIdent(const Operand &op, const std::string &name) {
        return op.kind == Operand::Imm && op.expr.size() == 1 && op.expr[0].sign == 0 &&
               op.expr[0].kind == SylKind::Symbol && op.expr[0].text == name;
    }

    // The full instruction vocabulary codegen emits - not a general x86 encoder.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "cli") { emit8(0xFA); return; }
        if (mnemonic == "sti") { emit8(0xFB); return; }
        if (mnemonic == "hlt") { emit8(0xF4); return; }
        if (mnemonic == "ret") { emit8(0xC3); return; }
        if (mnemonic == "cwd") { emit8(0x99); return; }
        if (mnemonic == "cbw") { emit8(0x98); return; }  // sign-extends AL into AX
        // `int imm8` - a real BIOS/DOS software interrupt (e.g. disk I/O via int 0x13).
        if (mnemonic == "int") {
            if (ops.size() != 1 || ops[0].kind != Operand::Imm) fail("'int' needs one immediate operand");
            emit8(0xCD);
            emit8(static_cast<uint8_t>(evalExpr(ops[0].expr)));
            return;
        }

        if (mnemonic == "push") { requireReg(ops, 1); emit8(static_cast<uint8_t>(0x50 + ops[0].reg)); return; }
        if (mnemonic == "pop")  { requireReg(ops, 1); emit8(static_cast<uint8_t>(0x58 + ops[0].reg)); return; }
        if (mnemonic == "neg")  { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b011, static_cast<uint8_t>(ops[0].reg)); return; }
        if (mnemonic == "not")  { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b010, static_cast<uint8_t>(ops[0].reg)); return; }
        if (mnemonic == "imul") { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b101, static_cast<uint8_t>(ops[0].reg)); return; }
        if (mnemonic == "idiv") { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b111, static_cast<uint8_t>(ops[0].reg)); return; }
        if (mnemonic == "mul")  { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b100, static_cast<uint8_t>(ops[0].reg)); return; }  // unsigned counterparts
        if (mnemonic == "div")  { requireReg(ops, 1); emitModRegRM(0xF7, 0b11, 0b110, static_cast<uint8_t>(ops[0].reg)); return; }

        if (mnemonic == "jmp")  { if (ops.size()!=1) fail("'jmp' needs one operand"); emitRel16(0xE9, ops[0]); return; }
        // `call reg16` (indirect, via a function pointer) is FF /2 - the same group-FF
        // ModRM form as not/imul/idiv, reg field 0b010 ("CALL r/m16, near indirect").
        if (mnemonic == "call") {
            if (ops.size() != 1) fail("'call' needs one operand");
            if (ops[0].kind == Operand::Reg) {
                emitModRegRM(0xFF, 0b11, 0b010, static_cast<uint8_t>(ops[0].reg));
                return;
            }
            emitRel16(0xE8, ops[0]);
            return;
        }
        if (mnemonic == "je")   { emitRel8(0x74, ops.at(0)); return; }
        if (mnemonic == "jne")  { emitRel8(0x75, ops.at(0)); return; }
        if (mnemonic == "jl")   { emitRel8(0x7C, ops.at(0)); return; }
        if (mnemonic == "jle")  { emitRel8(0x7E, ops.at(0)); return; }
        if (mnemonic == "jg")   { emitRel8(0x7F, ops.at(0)); return; }
        if (mnemonic == "jge")  { emitRel8(0x7D, ops.at(0)); return; }
        if (mnemonic == "jb")   { emitRel8(0x72, ops.at(0)); return; }  // unsigned counterparts
        if (mnemonic == "jae")  { emitRel8(0x73, ops.at(0)); return; }
        if (mnemonic == "jbe")  { emitRel8(0x76, ops.at(0)); return; }
        if (mnemonic == "ja")   { emitRel8(0x77, ops.at(0)); return; }
        if (mnemonic == "jc")   { emitRel8(0x72, ops.at(0)); return; }  // carry flag directly, same encoding as jb/jae
        if (mnemonic == "jnc")  { emitRel8(0x73, ops.at(0)); return; }

        if (mnemonic == "mov") {
            if (ops.size() != 2) fail("'mov' needs two operands");
            const Operand &dst = ops[0];
            const Operand &src = ops[1];
            if (dst.kind == Operand::SegReg && src.kind == Operand::Reg) {
                emitModRegRM(0x8E, 0b11, static_cast<uint8_t>(dst.reg), static_cast<uint8_t>(src.reg));
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Imm) {
                emit8(static_cast<uint8_t>(0xB8 + dst.reg));
                emitExprAsWord(src.expr);
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Reg) {
                emitModRegRM(0x89, 0b11, static_cast<uint8_t>(src.reg), static_cast<uint8_t>(dst.reg));
                return;
            }
            if (dst.kind == Operand::Reg && src.kind == Operand::Mem) { emitMemInstr(0x8B, dst.reg, src); return; }
            if (dst.kind == Operand::Mem && src.kind == Operand::Reg) { emitMemInstr(0x89, src.reg, dst); return; }
            fail("unsupported 'mov' operand combination");
        }
        // "movb ax,[mem]" / "movb [mem],ax" - a byte-sized (`char`) load/store, opcode
        // 0x8A/0x88 (MOV r8,r/m8 / MOV r/m8,r8); ax's reg field (0) is AL's. Only `ax`
        // is valid here (AH is never used).
        if (mnemonic == "movb") {
            if (ops.size() != 2) fail("'movb' needs two operands");
            const Operand &dst = ops[0];
            const Operand &src = ops[1];
            if (dst.kind == Operand::Reg && dst.reg == 0 && src.kind == Operand::Mem) {
                emitMemInstr(0x8A, dst.reg, src);
                return;
            }
            if (dst.kind == Operand::Mem && src.kind == Operand::Reg && src.reg == 0) {
                emitMemInstr(0x88, src.reg, dst);
                return;
            }
            fail("'movb' only supports ax<->mem in this dialect");
        }

        // "lea reg,[bp-4]" loads the effective address itself (always a Local/Param's
        // frame-relative address here), not the word stored there - opcode 0x8D, same
        // reg<-mem ModRM shape as "mov"'s Reg<-Mem case above.
        if (mnemonic == "lea") {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Mem) {
                fail("'lea' needs a register destination and a memory source in this dialect");
            }
            emitMemInstr(0x8D, ops[0].reg, ops[1]);
            return;
        }

        if (mnemonic == "xor" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Reg) {
            emitModRegRM(0x31, 0b11, static_cast<uint8_t>(ops[1].reg), static_cast<uint8_t>(ops[0].reg));
            return;
        }
        // "cmp ax, 0"-style truthiness checks need CMP r/m16,imm16 (0x81 /7), not just
        // the reg,reg form below.
        if (mnemonic == "cmp" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Imm) {
            emitModRegRM(0x81, 0b11, 0b111, static_cast<uint8_t>(ops[0].reg));
            emitExprAsWord(ops[1].expr);
            return;
        }
        // "add" also shows up as Reg,Mem (pointer arithmetic) and Reg,Imm (an array's
        // bare address used as an immediate) - no other ALU mnemonic needs either form.
        if (mnemonic == "add" && ops.size() == 2 && ops[0].kind == Operand::Reg) {
            if (ops[1].kind == Operand::Mem) {
                emitMemInstr(0x03, ops[0].reg, ops[1]);
                return;
            }
            if (ops[1].kind == Operand::Imm) {
                emitModRegRM(0x81, 0b11, 0b000, static_cast<uint8_t>(ops[0].reg));
                emitExprAsWord(ops[1].expr);
                return;
            }
        }
        // "sub sp,N" reserves/releases frame storage - same group-0x81 reg,imm form as
        // "add" above, reg field 0b101.
        if (mnemonic == "sub" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Imm) {
            emitModRegRM(0x81, 0b11, 0b101, static_cast<uint8_t>(ops[0].reg));
            emitExprAsWord(ops[1].expr);
            return;
        }
        // "and ax, 0x00FF" zero-extends AX's low byte - needs the reg,imm form since
        // this dialect has no "ah" register to mask with a plain reg,reg AND.
        if (mnemonic == "and" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Imm) {
            emitModRegRM(0x81, 0b11, 0b100, static_cast<uint8_t>(ops[0].reg));
            emitExprAsWord(ops[1].expr);
            return;
        }
        // "adc/sbb ax, 0" propagates a carry/borrow into a word - the reg,imm form,
        // which the reg,reg form below can't cover.
        if ((mnemonic == "adc" || mnemonic == "sbb") && ops.size() == 2 && ops[0].kind == Operand::Reg &&
            ops[1].kind == Operand::Imm) {
            emitModRegRM(0x81, 0b11, mnemonic == "adc" ? 0b010 : 0b011, static_cast<uint8_t>(ops[0].reg));
            emitExprAsWord(ops[1].expr);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> aluOpcode = {
            {"add", 0x03}, {"sub", 0x2B}, {"and", 0x23}, {"or", 0x0B}, {"cmp", 0x3B},
            {"adc", 0x13}, {"sbb", 0x1B},  // add/sub-with-carry/borrow, for multi-word arithmetic
        };
        if (auto it = aluOpcode.find(mnemonic); it != aluOpcode.end()) {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Reg) {
                fail("'" + mnemonic + "' needs two register operands in this dialect");
            }
            emitModRegRM(it->second, 0b11, static_cast<uint8_t>(ops[0].reg), static_cast<uint8_t>(ops[1].reg));
            return;
        }

        if (mnemonic == "shl" || mnemonic == "shr" || mnemonic == "sar" || mnemonic == "rcl" || mnemonic == "rcr") {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg) fail("'" + mnemonic + "' needs a register destination");
            // sar (signed) vs shr (unsigned) right shift. rcl/rcr (rotate-through-carry)
            // chain a 1-bit shift across a multi-word value: shl/shr leaves the bit that
            // crossed a word boundary in the carry flag, which `mov` never disturbs, so
            // the next word's rcl/rcr picks it back up even with loads/stores between.
            uint8_t regField = (mnemonic == "shl") ? 0b100 : (mnemonic == "shr") ? 0b101
                                : (mnemonic == "sar") ? 0b111 : (mnemonic == "rcl") ? 0b010 : 0b011;
            if (isLiteralNumber(ops[1], "1")) {
                emitModRegRM(0xD1, 0b11, regField, static_cast<uint8_t>(ops[0].reg));
                return;
            }
            if (isBareIdent(ops[1], "cl")) {
                emitModRegRM(0xD3, 0b11, regField, static_cast<uint8_t>(ops[0].reg));
                return;
            }
            fail("'" + mnemonic + "' only supports a literal shift of 1 or a 'cl' count in this dialect");
        }

        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::vector<Relocation> relocations_;
    bool allowExternal_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool checkRange_ = false;  // labels are known from a previous pass
    bool grew_ = false;
    size_t jccIndex_ = 0;
    std::set<size_t> longJcc_;
};

// The one `.text` section is just `bytes()` as-is; each label's symbol value is its
// offset within that section (address minus orgBase()). Machine type 0x0086 is a
// custom, non-registered COFF machine ID (real 8086/DOS tooling used OMF, not COFF).
constexpr uint16_t kI8086CoffMachine = 0x0086;

// Shared by writeCoff() (serializes to COFF bytes) and assembleI8086Object() (returns
// the ObjectFile directly).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kI8086CoffMachine;
    obj.base = a.orgBase();
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry (the writer resolves
    // relocations by index) - add one, undefined, for any name not already a local label.
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        obj.relocations.push_back({r.byteOffset, r.symbolName, static_cast<uint16_t>(r.kind)});
    }
    return obj;
}

std::string writeCoff(const Assembler &a) { return CoffFormatWriter().write(buildObjectFile(a)); }

}  // namespace

std::string assembleI8086(const std::string &asmText, I8086OutputFormat format, bool allowExternal) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(allowExternal);
    assembler.run(lines);
    switch (format) {
        case I8086OutputFormat::Bin: {
            const auto &bytes = assembler.bytes();
            return std::string(bytes.begin(), bytes.end());
        }
        case I8086OutputFormat::Coff:
            return writeCoff(assembler);
        case I8086OutputFormat::Elf:
            return ElfFormatWriter().write(buildObjectFile(assembler));
    }
    fail("unknown output format");
}

ObjectFile assembleI8086Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
