#include "SaturnAssembler.h"

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"
#include "fileformat/coff/CoffFormat.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("SaturnAssembler: " + msg); }

// --- Parsing, via the shared grammar/PccAsm.g4 ---------------------------
enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

// A bare register name (`a`/`c`/`b`/`d0`/`d1`) is Reg; anything else (a number, a label, or
// `label+N`) is Value - resolved as a 32-bit word (an address, when it names a label).
struct Operand {
    enum Kind { Reg, Value, Bracket } kind = Value;
    int reg = -1;                  // Reg
    std::vector<Syllable> expr;    // Value
    // Bracket ([Rb+disp] or [abs]): baseReg == -1 means no base register (an absolute address).
    int baseReg = -1;
    std::vector<Syllable> disp;    // may be empty (no displacement syllables -> 0)
};

std::string toLower(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Saturn's own register letters (a/c/b) plus its address registers (d0/d1), used here as this
// backend's stack pointer (d0) and frame pointer (d1) - see SaturnCodegen.h.
int regNumber(const std::string &lower) {
    static const std::unordered_map<std::string, int> names = {
        {"a", 0}, {"c", 1}, {"b", 2}, {"d0", 3}, {"d1", 4},
    };
    auto it = names.find(lower);
    return it == names.end() ? -1 : it->second;
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

// A syllable that is exactly one unsigned register name.
int bareReg(const Syllable &s) {
    if (s.sign != 0 || s.kind != SylKind::Symbol) return -1;
    return regNumber(toLower(s.text));
}

Operand buildOperand(PccAsmParser::OperandContext *ctx) {
    Operand op;
    if (ctx->LBRACKET()) {
        op.kind = Operand::Bracket;
        std::vector<Syllable> syls = exprToSyllables(ctx->expr());
        // At most one syllable may name a register (base); every other syllable is the
        // displacement expression. No base register at all means an absolute address.
        std::vector<Syllable> disp;
        bool sawBase = false;
        for (const auto &s : syls) {
            int r = bareReg(s);
            if (r >= 0 && !sawBase) {
                op.baseReg = r;
                sawBase = true;
            } else {
                disp.push_back(s);
            }
        }
        op.disp = std::move(disp);
        return op;
    }
    std::vector<Syllable> syls = exprToSyllables(ctx->expr());
    if (syls.size() == 1) {
        if (int r = bareReg(syls[0]); r >= 0) {
            op.kind = Operand::Reg;
            op.reg = r;
            return op;
        }
    }
    op.kind = Operand::Value;
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

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Dw, Dh, Db, Instr } kind = None;
    std::string equateName;
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Dw (N)
    std::string mnemonic;                           // Instr
    std::vector<Operand> operands;                  // Instr
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
};

// --- Opcode table ---------------------------------------------------------
// See SaturnAssembler.h's own header comment: this encoding is this backend's own invention, not
// real Saturn machine code. Every instruction's own size is fixed by its mnemonic plus its
// operands' syntactic shape (register vs. value), never by any symbol's resolved value, so a
// two-pass assembler's pass 1 (sizing) and pass 2 (resolving/emitting) always agree.
enum Opcode : uint8_t {
    OpHalt = 0x00,
    OpClr = 0x01,
    OpMov = 0x02,
    OpLd = 0x03,
    OpSt = 0x04,
    OpAdd = 0x05,
    OpSub = 0x06,
    OpInc = 0x07,
    OpDec = 0x08,
    OpNeg = 0x09,
    OpAnd = 0x0A,
    OpOr = 0x0B,
    OpXor = 0x0C,
    OpNot = 0x0D,
    OpShl = 0x0E,
    OpShr = 0x0F,
    OpSar = 0x10,
    OpCmp = 0x11,
    OpGoto = 0x12,
    OpGosbvl = 0x13,
    OpRtn = 0x14,
    OpJe = 0x15,
    OpJne = 0x16,
    OpJl = 0x17,
    OpJle = 0x18,
    OpJg = 0x19,
    OpJge = 0x1A,
    OpJb = 0x1B,
    OpJbe = 0x1C,
    OpJa = 0x1D,
    OpJae = 0x1E,
    OpPush = 0x1F,
    OpPop = 0x20,
    // Indirect forms: jump/call to the address held in a register rather than a fixed operand.
    // Needed because `rtn`'s own "pop the return address, then jump to it" order can't be
    // reordered around extra pushes (e.g. a wide return value's chunks) any other way - see
    // SaturnCodegen.cpp's emitEpilogueAndReturn() - and because a call through a function
    // pointer (`ir::ExprKind::CallPtr`) has no fixed label to begin with.
    OpJmpr = 0x21,
    OpGosbvlr = 0x22,
    // Narrower memory access: `char`/`short` are real 1-/2-byte storage on this backend (its
    // registers are real byte addresses, matching SaturnCodegen.h's byte-based typeWidths()),
    // so a plain 4-byte `ld`/`st` would read/write past a narrower slot's own storage. `ldb`/
    // `ldh` zero-extend into the destination register; `stb`/`sth` write only the source's low
    // 1/2 bytes.
    OpLdb = 0x23,
    OpStb = 0x24,
    OpLdh = 0x25,
    OpSth = 0x26,
};

// `kNoBase` in a `ld`/`st`'s baseReg byte means the displacement field is an absolute address
// instead of being added to a base register's value.
constexpr uint8_t kNoBase = 0xFF;

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
                offset_ = 0;
                return;
            case Line::Dw:
                for (const auto &e : line.exprList) emitWord(e);
                return;
            case Line::Dh:
                for (const auto &e : line.exprList) emit16(static_cast<uint16_t>(evalExpr(e)));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emit8(static_cast<uint8_t>(evalExpr(e)));
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

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal, bool *wasLocalAddress) {
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

    // --- byte emission (little-endian) -----------------------------------
    void emit8(uint8_t v) {
        bytes_.push_back(v);
        offset_ += 1;
    }
    void emit16(uint16_t v) {
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    void emit32(uint32_t v) {
        emit16(static_cast<uint16_t>(v & 0xFFFF));
        emit16(static_cast<uint16_t>((v >> 16) & 0xFFFF));
    }
    void emitReloc(const std::vector<Syllable> &expr) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(expr, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit32(v);
    }
    void emitWord(const std::vector<Syllable> &expr) { emitReloc(expr); }

    static uint8_t regOf(const Operand &op, const std::string &mnemonic) {
        if (op.kind != Operand::Reg) fail("'" + mnemonic + "' needs a register operand");
        return static_cast<uint8_t>(op.reg);
    }
    static const Operand &operandAt(const std::vector<Operand> &ops, size_t i, const std::string &mnemonic) {
        if (ops.size() <= i) fail("'" + mnemonic + "': missing operand");
        return ops[i];
    }

    // SRC operand: tag byte (0 = register, 1 = value) + payload.
    void emitSrc(const Operand &op, const std::string &mnemonic) {
        if (op.kind == Operand::Reg) {
            emit8(0);
            emit8(static_cast<uint8_t>(op.reg));
            return;
        }
        if (op.kind != Operand::Value) fail("'" + mnemonic + "': operand can't be a memory reference here");
        emit8(1);
        emitReloc(op.expr);
    }

    void emitRegAndSrc(const std::vector<Operand> &ops, const std::string &mnemonic) {
        emit8(regOf(operandAt(ops, 0, mnemonic), mnemonic));
        emitSrc(operandAt(ops, 1, mnemonic), mnemonic);
    }

    void emitReg(const std::vector<Operand> &ops, const std::string &mnemonic) {
        emit8(regOf(operandAt(ops, 0, mnemonic), mnemonic));
    }

    void emitBranchTarget(const std::vector<Operand> &ops, const std::string &mnemonic) {
        const Operand &target = operandAt(ops, 0, mnemonic);
        if (target.kind != Operand::Value) fail("'" + mnemonic + "' needs a plain address operand");
        emitReloc(target.expr);
    }

    void encodeMemOperand(const Operand &mem, const std::string &mnemonic) {
        if (mem.kind != Operand::Bracket) fail("'" + mnemonic + "' needs a '[...]' memory operand");
        emit8(mem.baseReg < 0 ? kNoBase : static_cast<uint8_t>(mem.baseReg));
        if (mem.disp.empty()) {
            emit32(0);
        } else {
            emitReloc(mem.disp);
        }
    }

    void encodeInstr(const std::string &mnemonic, const std::vector<Operand> &ops) {
        if (mnemonic == "halt") {
            emit8(OpHalt);
            return;
        }
        if (mnemonic == "rtn") {
            emit8(OpRtn);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> regOnly = {
            {"clr", OpClr}, {"inc", OpInc},    {"dec", OpDec},       {"neg", OpNeg},
            {"not", OpNot}, {"pop", OpPop},    {"jmpr", OpJmpr},     {"gosbvlr", OpGosbvlr},
        };
        if (auto it = regOnly.find(mnemonic); it != regOnly.end()) {
            emit8(it->second);
            emitReg(ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> regAndSrc = {
            {"mov", OpMov}, {"add", OpAdd}, {"sub", OpSub}, {"and", OpAnd}, {"or", OpOr},
            {"xor", OpXor}, {"shl", OpShl}, {"shr", OpShr}, {"sar", OpSar}, {"cmp", OpCmp},
        };
        if (auto it = regAndSrc.find(mnemonic); it != regAndSrc.end()) {
            emit8(it->second);
            emitRegAndSrc(ops, mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> branches = {
            {"goto", OpGoto}, {"gosbvl", OpGosbvl}, {"je", OpJe}, {"jne", OpJne}, {"jl", OpJl},
            {"jle", OpJle},   {"jg", OpJg},         {"jge", OpJge}, {"jb", OpJb}, {"jbe", OpJbe},
            {"ja", OpJa},     {"jae", OpJae},
        };
        if (auto it = branches.find(mnemonic); it != branches.end()) {
            emit8(it->second);
            emitBranchTarget(ops, mnemonic);
            return;
        }
        if (mnemonic == "push") {
            emit8(OpPush);
            emitSrc(operandAt(ops, 0, mnemonic), mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> loads = {
            {"ld", OpLd}, {"ldb", OpLdb}, {"ldh", OpLdh},
        };
        if (auto it = loads.find(mnemonic); it != loads.end()) {
            emit8(it->second);
            emit8(regOf(operandAt(ops, 0, mnemonic), mnemonic));
            encodeMemOperand(operandAt(ops, 1, mnemonic), mnemonic);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> stores = {
            {"st", OpSt}, {"stb", OpStb}, {"sth", OpSth},
        };
        if (auto it = stores.find(mnemonic); it != stores.end()) {
            emit8(it->second);
            encodeMemOperand(operandAt(ops, 0, mnemonic), mnemonic);
            emitSrc(operandAt(ops, 1, mnemonic), mnemonic);
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
    obj.machineType = kSaturnCoffMachine;
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
        obj.relocations.push_back({r.offset, r.symbolName, kSaturnRelocAbs32});
    }
    return obj;
}

std::string writeCoff(const Assembler &a) { return CoffFormatWriter().write(buildObjectFile(a)); }

}  // namespace

std::string assembleSaturn(const std::string &asmText, bool allowExternal) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(allowExternal);
    assembler.run(lines);
    if (!allowExternal) {
        const auto &bytes = assembler.bytes();
        return std::string(bytes.begin(), bytes.end());
    }
    return writeCoff(assembler);
}

ObjectFile assembleSaturnObject(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
