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
#include "coff/CoffWriter.h"
#include "elf/ElfWriter.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("I8086Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
//
// Same grammar as ../pdp7/Pdp7Assembler.cpp (see that file's own
// header comment for the shared rationale), interpreted completely
// differently here: a mnemonic selects a real x86 instruction and
// each comma-separated operand is a distinct, structurally real
// argument to it (register, memory, or immediate) - never OR-combined
// the way pdp7's own semantics work.

// `Group` is a parenthesized sub-expression (NASM's own "510-($-$$)"
// idiom, used for the boot-sector/placement padding - see
// I8086Codegen.cpp's `emitPlacement()`): its own syllables (`group`)
// get evaluated recursively (same evalExpr) to one combined value,
// which the OUTER sign then adds/subtracts, exactly like any other
// syllable's value - this is what lets a group be negated as a
// whole ("N-(A-B)" = N-A+B, not N-A-B).
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;            // Number/Symbol
    std::vector<Syllable> group;  // Group
};

// A parsed operand. `Reg`/`SegReg` carry a register index (see
// regIndex()/segRegIndex() below); `Mem` is one of the exactly three
// addressing-mode shapes I8086Codegen.cpp ever emits - `[label]`
// (direct, `memHasBx=false`), `[bx]` (`memHasBx=true`,
// `memHasDisp=false`), or `[label+bx]` (`memHasBx=true`,
// `memHasDisp=true`) - with `expr` holding the displacement for the
// two forms that have one; `Imm` covers everything else (a literal
// number, or a bare symbol used as a jump/call target or as a
// pointer value, e.g. `mov ax, arr_0` for `&arr`).
struct Operand {
    enum Kind { Reg, SegReg, Mem, Imm } kind = Imm;
    int reg = -1;
    bool memHasBx = false;
    bool memHasDisp = false;
    std::vector<Syllable> expr;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Bits, Org, Dw, Times, Instr } kind = None;
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

// `[...]` and plain-expr operand forms, classified into exactly the
// shapes I8086Codegen.cpp ever emits - see Operand's own comment.
Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    Operand op;
    if (ctx->LBRACKET()) {
        op.kind = Operand::Mem;
        auto syls = exprToSyllables(ctx->expr());
        // "bx" can be unsigned (bare `[bx]`) or '+'-signed (the second
        // syllable of `[label+bx]` - the label comes first there, see
        // I8086Codegen.cpp's `computeElementAddr`), so check by name
        // alone, not by position or sign.
        auto isBx = [](const Syllable &s) { return s.kind == SylKind::Symbol && s.text == "bx"; };
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

    line.kind = Line::Instr;
    line.mnemonic = mnemonic;
    for (auto *op : instr->operand()) line.operands.push_back(buildOperand(op));
    return line;
}

// Dynamic_cast dispatch on the labeled-alternative Context subclasses,
// the same convention ../../ast/BuildAst.cpp and
// ../pdp7/Pdp7Assembler.cpp already use, rather than the generated
// Visitor interface.
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

// One patch a future linker still needs to apply, for a symbol this
// file's own labels don't define - see I8086Assembler.h's own comment
// on `allowExternal`. `Abs16` is a direct-address/immediate 2-byte
// field that just needs the symbol's final linked address; `Rel16` is
// a `jmp`/`call`'s 2-byte displacement field, which needs
// `target - (site+2)` - only computable once linked, so this file
// just emits a `0x0000` placeholder and leaves the real relative
// subtraction to the linker too.
// (Values mirrored as kI8086RelocAbs16/Rel16 in I8086Assembler.h,
// exported for ../../linker/I8086Linker.cpp - kept in sync by hand,
// same spirit as Pdp7Assembler.h/.cpp's kPdp7RelocOr/Add/Sub.)
enum class RelocKind : uint16_t { Abs16 = 1, Rel16 = 2 };
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    RelocKind kind;
};

// --- Two-pass assembly ----------------------------------------------------
//
// Every instruction's *size* is fixed by its syntactic form alone
// (see I8086Assembler.h) - pass 1 and pass 2 run the exact same
// encoding logic; pass 1 just discards the bytes and only cares about
// how many there were, so there's never a second, separately
// maintained "how big is this" implementation to let drift out of
// sync with the real encoder.
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
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }
    uint32_t orgBase() const { return orgBase_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        offset_ = 0;
        orgBase_ = 0;
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
            case Line::Times: {
                if (line.innerMnemonic != "db") fail("'times' only supports a 'db' body in this dialect");
                uint32_t count = evalExpr(line.timesCount);
                uint32_t v = line.exprList.empty() ? 0 : evalExpr(line.exprList.at(0));
                for (uint32_t i = 0; i < count; i++) emit8(static_cast<uint8_t>(v));
                return;
            }
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // --- expression evaluation (ADD-combine, not pdp7's OR) ---------

    // `externalName`, if given, is set to the one symbol any syllable
    // here named that isn't defined anywhere in this file (only
    // possible when `allowExternal_` - see resolveSyllable); `
    // needsBaseShift`, if given, is set true when any syllable
    // resolved to a LOCAL address (a label, `$`, or `$$` - all are
    // "offset from this object's own `ORG 0`" in object mode, see
    // resolveSyllable) - the caller (emitExprAsWord/emitRel16) is
    // then responsible for recording the actual relocation(s), since
    // this function doesn't know the field's own byte offset or
    // Abs16-vs-Rel16 kind. Nothing pcc itself ever generates combines
    // more than one external reference into the same field, so "the
    // one" externalName is enough; a field can need both a base shift
    // and an external relocation at once (e.g. `label_local + 4`
    // combined with something external never actually happens today,
    // but a bare local label and a bare external reference are each
    // their own, separately-recorded case).
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

    // `wasExternal`, if given, is set true when `syl` named a symbol
    // not defined anywhere in this file and `allowExternal_` let that
    // through rather than failing. `wasLocalAddress`, if given, is
    // set true when `syl` resolved to a position-dependent LOCAL
    // value (a label already defined in this file, or `$`/`$$`) -
    // see evalExpr's own comment for why that also needs a
    // relocation in object mode, unlike a plain literal number.
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
    // The "evalExpr, then emit16 the result" pattern every Abs16 call
    // site (dw/mem-displacement/mov-imm/cmp-imm/add-imm) shares.
    // Records an Abs16 relocation against the reserved pseudo-symbol
    // ".." when the expression touched a local address (see
    // evalExpr's own comment: the value stored here is only "offset
    // from this object's own ORG 0" until the linker adds this
    // specific object's real, final base - Linker.cpp resolves ".."
    // that way, never by searching another object's symbol table),
    // and/or a real Abs16 relocation against an external symbol when
    // it referenced one - both can apply to the same field at once.
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

    // Near jmp/call (opcode + rel16) and short Jcc (opcode + rel8) -
    // both fixed-size regardless of distance (see I8086Assembler.h);
    // only pass 2's actual displacement value can ever be wrong, and
    // only Jcc's rel8 range is checked (pass 2 only - pass 1's labels
    // aren't all resolved yet, so the "value" is meaningless there,
    // only the size matters).
    void emitRel16(uint8_t opcode, const Operand &target) {
        uint32_t endAddr = orgBase_ + offset_ + 3;
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        emit8(opcode);
        if (stage_ == 2 && !externalName.empty()) {
            // Only the linker, once every object is merged, can know
            // `target`'s real linked address to compute `target -
            // (site+2)` - leave a placeholder and let it do the
            // subtraction itself (see Relocation's own comment).
            relocations_.push_back({offset_, externalName, RelocKind::Rel16});
            emit16(0);
        } else {
            int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
            emit16(static_cast<uint16_t>(rel));
        }
    }
    void emitRel8(uint8_t opcode, const Operand &target) {
        uint32_t endAddr = orgBase_ + offset_ + 2;
        std::string externalName;
        uint32_t targetAddr = evalExpr(target.expr, &externalName);
        // A short jCC's target is always internal control flow within
        // one function, never a cross-file reference - fail loudly
        // rather than silently resolve-to-0 if that ever stops being
        // true (see I8086Assembler.h).
        if (stage_ == 2 && !externalName.empty()) {
            fail("conditional jump target '" + externalName + "' is external - short jumps can't cross file boundaries");
        }
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) fail("conditional jump target out of short (rel8) range");
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

    // The full instruction vocabulary ../I8086Codegen.cpp ever emits
    // - see I8086Assembler.h's header comment. Not a general x86
    // encoder: exactly these mnemonic/operand-shape combinations,
    // nothing else.
    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "cli") { emit8(0xFA); return; }
        if (mnemonic == "sti") { emit8(0xFB); return; }
        if (mnemonic == "hlt") { emit8(0xF4); return; }
        if (mnemonic == "ret") { emit8(0xC3); return; }
        if (mnemonic == "cwd") { emit8(0x99); return; }
        // `int imm8` - a real BIOS/DOS software interrupt (e.g. `int
        // 0x13`, the real disk-I/O call every boot-sector program
        // uses - see ../../tests/disk.c) - no portable C expression
        // for this any more than pdp7's own IOT instructions have
        // one, same "raw hardware instruction, write it in an `asm`
        // block" story.
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

        if (mnemonic == "jmp")  { if (ops.size()!=1) fail("'jmp' needs one operand"); emitRel16(0xE9, ops[0]); return; }
        if (mnemonic == "call") { if (ops.size()!=1) fail("'call' needs one operand"); emitRel16(0xE8, ops[0]); return; }
        if (mnemonic == "je")   { emitRel8(0x74, ops.at(0)); return; }
        if (mnemonic == "jne")  { emitRel8(0x75, ops.at(0)); return; }
        if (mnemonic == "jl")   { emitRel8(0x7C, ops.at(0)); return; }
        if (mnemonic == "jle")  { emitRel8(0x7E, ops.at(0)); return; }
        if (mnemonic == "jg")   { emitRel8(0x7F, ops.at(0)); return; }
        if (mnemonic == "jge")  { emitRel8(0x7D, ops.at(0)); return; }

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

        if (mnemonic == "xor" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Reg) {
            emitModRegRM(0x31, 0b11, static_cast<uint8_t>(ops[1].reg), static_cast<uint8_t>(ops[0].reg));
            return;
        }
        // "cmp ax, 0"-style truthiness checks (If/While conditions,
        // &&/||/! - see genStmt/genLogicalAnd/Or/Not) need CMP
        // r/m16,imm16 (0x81 /7), not just the reg,reg form below.
        if (mnemonic == "cmp" && ops.size() == 2 && ops[0].kind == Operand::Reg && ops[1].kind == Operand::Imm) {
            emitModRegRM(0x81, 0b11, 0b111, static_cast<uint8_t>(ops[0].reg));
            emitExprAsWord(ops[1].expr);
            return;
        }
        // "add" alone also shows up as Reg,Mem (`add ax, [p]` - pointer
        // arithmetic reading through a slot's value, genAddrOf) and
        // Reg,Imm (`add ax, arr_1` - an array's own bare address used
        // as an immediate, same genAddrOf) - no other ALU mnemonic
        // here needs either form (see I8086Codegen.cpp's genAddrOf).
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
        static const std::unordered_map<std::string, uint8_t> aluOpcode = {
            {"add", 0x03}, {"sub", 0x2B}, {"and", 0x23}, {"or", 0x0B}, {"cmp", 0x3B},
        };
        if (auto it = aluOpcode.find(mnemonic); it != aluOpcode.end()) {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Reg) {
                fail("'" + mnemonic + "' needs two register operands in this dialect");
            }
            emitModRegRM(it->second, 0b11, static_cast<uint8_t>(ops[0].reg), static_cast<uint8_t>(ops[1].reg));
            return;
        }

        if (mnemonic == "shl" || mnemonic == "shr") {
            if (ops.size() != 2 || ops[0].kind != Operand::Reg) fail("'" + mnemonic + "' needs a register destination");
            uint8_t regField = (mnemonic == "shl") ? 0b100 : 0b101;
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
};

// COFF (see ../../coff/CoffWriter.h): the one `.text` section is just
// `bytes()` as-is (already byte-addressable, unlike pdp7's 18-bit
// words), and each label's symbol value is its offset within that
// section (its address minus `orgBase()`, the fixed boot-sector load
// address every label is already relative to). Machine type 0x0086 is
// a custom, non-registered COFF machine ID (real 16-bit 8086/DOS
// tooling used OMF, not COFF, so there's no real one to use) chosen
// only to be unique and memorable, distinct from pdp7's 0x1987.
constexpr uint16_t kI8086CoffMachine = 0x0086;

std::string writeCoff(const Assembler &a) {
    const auto &bytesVec = a.bytes();
    std::vector<uint8_t> text(bytesVec.begin(), bytesVec.end());

    std::vector<CoffSymbol> symbols;
    for (const auto &kv : a.labels()) {
        symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    std::sort(symbols.begin(), symbols.end(), [](const CoffSymbol &x, const CoffSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry too
    // (the writer resolves relocations to symbols by index) - add one,
    // undefined, for any name that isn't already a real local label.
    std::vector<CoffRelocation> relocations;
    std::unordered_map<std::string, bool> known;
    for (const auto &s : symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        relocations.push_back({r.byteOffset, r.symbolName, static_cast<uint16_t>(r.kind)});
    }

    return writeCoffObject(kI8086CoffMachine, text, symbols, relocations);
}

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
        case I8086OutputFormat::Elf: {
            const auto &bytes = assembler.bytes();
            return writeElfExecutable(kI8086BaseAddress, bytes);
        }
    }
    fail("unknown output format");
}

}  // namespace pcc
