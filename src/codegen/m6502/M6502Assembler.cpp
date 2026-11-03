#include "M6502Assembler.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "PccAsmLexer.h"
#include "PccAsmParser.h"

namespace pcc {

namespace {

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("M6502Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
// Same shape as ../x86_8086/I8086Assembler.cpp's own Syllable/Group -
// a signed sum of numbers/symbols/parenthesized groups - see that
// file's own comment. No register syntax exists in this dialect at
// all, so there's nothing here like I8086Assembler.cpp's own
// Operand::Reg/Mem distinction - every operand is just one of these.

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    enum Kind { None, Equate, Org, Db, DbLo, DbHi, Instr } kind = None;
    std::string equateName;                         // Equate
    std::vector<std::vector<Syllable>> exprList;     // Equate (1)/Org (1)/Db/DbLo/DbHi (N)
    std::string mnemonic;                             // Instr
    std::vector<std::vector<Syllable>> operands;      // Instr
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

// One pending fixup `assembleM6502Object()` (see M6502Assembler.h's
// own header comment) turns into a real ObjRelocation once assembly
// is done - `kind` is one of `kM6502RelocAbs16`/`...Lo`/`...Hi`
// (see that file's own comment on why there are three).
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

std::vector<Syllable> buildOperand(PccAsmParser::OperandContext *ctx) {
    if (ctx->LBRACKET()) fail("bracketed memory operands aren't part of this target's dialect");
    return exprToSyllables(ctx->expr());
}

Line buildLine(PccAsmParser::LineContext *ctx) {
    Line line;
    for (auto *lbl : ctx->label()) line.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        line.kind = Line::None;
        return line;
    }
    // A pure compile-time symbol=value binding (no bytes emitted, no
    // relation to the current location counter) - used for this
    // backend's own fixed zero-page pseudo-registers (`ptr`/`ac`/
    // `bc`/`cx`/`dx` - see M6502Codegen.cpp's own comment on why
    // those need a *fixed*, not sequentially-laid-out, address).
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        line.kind = Line::Equate;
        line.equateName = assign->lhs->getText();
        line.exprList.push_back(exprToSyllables(assign->rhs));
        return line;
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of this target's dialect");
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
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
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
        return line;
    }
    // `dblo`/`dbhi` - a data byte that's the low/high byte of a 16-bit
    // expression (a label's own address included) rather than an
    // already-8-bit value, used for a dispatch-table array element
    // that names a function/global directly (M6502Codegen.cpp's own
    // `arrayElemText()`/`declareArray()`): there's no 16-bit load on
    // this target to resolve a symbol's address in one piece the way
    // `jsr`/`jmp`'s own absolute operand does, so each half is written
    // out as its own directive instead, resolved the same way
    // `ldai`/`ldai_hi`'s own immediate operand already is (see
    // `emitImmLow`/`emitImmHigh` below).
    if (mnemonic == "dblo" || mnemonic == "dbhi") {
        line.kind = mnemonic == "dblo" ? Line::DbLo : Line::DbHi;
        for (auto *op : instr->operand()) line.exprList.push_back(buildOperand(op));
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
//
// Every instruction's size is fixed by its mnemonic alone (see
// M6502Assembler.h) - pass 1 and pass 2 run the exact same encoding
// logic, same reasoning as ../x86_8086/I8086Assembler.cpp's own
// version of this comment. `labels_` (which also holds every Equate
// binding - see processLine's own comment) deliberately isn't reset
// between passes: pass 1 already resolves every value correctly
// (since sizes never depend on values), pass 2 just re-derives the
// identical bindings and this time actually emits bytes using them.
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
        equateNames_.clear();
        offset_ = 0;
        orgBase_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    // A label (`name:`) and an Equate (`name = expr`) both just bind
    // `name` in the same symbol table - a label's value is simply
    // "the current location", an Equate's is whatever its own
    // expression evaluates to right now, independent of location.
    void processLine(const Line &line) {
        for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                // An Equate (M6502Codegen.cpp's own fixed zero-page
                // `ptr`/`ac`/`bc`/`cx`/`dx`) is a plain compile-time
                // constant, the same in every object regardless of
                // where it's linked - unlike a real label (bound from
                // `orgBase_ + offset_` above), it must never be
                // treated as a "local address" needing a `..`
                // base-shift relocation (see resolveSyllable()'s own
                // check below) - tracked here so that check can tell
                // the two apart by name.
                equateNames_.insert(line.equateName);
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Db:
                for (const auto &e : line.exprList) emitByteOperand(e);
                return;
            case Line::DbLo:
                for (const auto &e : line.exprList) emitImmLow(e);
                return;
            case Line::DbHi:
                for (const auto &e : line.exprList) emitImmHigh(e);
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../z80/Z80Assembler.cpp's own `evalExpr` exactly - see
    // M6502Assembler.h's own header comment on why *any* local
    // address reference, not just an external one, needs a
    // relocation here once `allowExternal_` is set.
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
            if (allowExternal_ && wasLocalAddress && !equateNames_.count(syl.text)) *wasLocalAddress = true;
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

    // `db`'s own operand - a real, standalone byte value (not a
    // register-form's own encoding), so (unlike every other emit*
    // helper below) it's a hard error for it to not fit in a byte.
    void emitByteOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("'db' value doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // Absolute addressing's own 2-byte little-endian operand - the
    // full 16-bit resolved value, no range check (any symbol in this
    // program's own 64KB address space is valid here). See
    // M6502Assembler.h's own `kM6502RelocAbs16` comment: covers
    // `jsr`/`jmp`/every absolute `lda`/`sta`/etc. operand - the same
    // single-choke-point role Z80Assembler.cpp's own `emit16` plays.
    void emitAbsOperand(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }

    // `ldai`/`adci`/`andi`/`eori`/`ldyi`'s own immediate operand, and
    // `dblo`'s own data byte - the LOW byte of whatever `e` resolves
    // to, whether that's already a plain literal (M6502Codegen.cpp
    // always keeps an *instruction* operand's own literal <256 itself)
    // or a label's own address (M6502Codegen.cpp's genAddrOf()/
    // genIndexAddr()/arrayElemText() - taking just the low byte of a
    // resolved address this way is exactly what a real assembler's
    // `#<label` low-byte operator means, just spelled as a plain
    // mnemonic here instead of needing a grammar token for `<`/`>`
    // that doesn't exist). See M6502Assembler.h's own
    // `kM6502RelocAbs16Lo` comment: a plain Abs16 patch can't be
    // reused here since only one byte of the resolved value is ever
    // written to this field.
    void emitImmLow(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16Lo});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16Lo});
        }
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // `ldai_hi`/`adci_hi`'s own immediate operand, and `dbhi`'s own
    // data byte - the HIGH byte of the resolved value (a real
    // assembler's `#>label`) - see `kM6502RelocAbs16Hi`'s own comment,
    // the high-byte counterpart of `emitImmLow`'s own relocation
    // handling above.
    void emitImmHigh(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, "..", kM6502RelocAbs16Hi});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName, kM6502RelocAbs16Hi});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    // `ldaind`/`staind`'s own operand - the zero-page indirect-
    // indexed addressing mode is only legal for an address that
    // actually fits in zero page; M6502Codegen.cpp only ever passes
    // `ptr` here, but this still fails loudly rather than silently
    // truncate if that ever stopped being true.
    void emitZpOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("indirect-indexed operand must be a zero-page address");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }
    // A branch's own signed rel8 operand. M6502Codegen.cpp's own
    // safeBranch() (see that file's comment) guarantees every branch
    // it ever emits targets the very next instruction - always
    // trivially in range - so an out-of-range target here can only
    // mean a real bug in that guarantee, not a legitimately-far
    // user-code target that needs handling.
    void emitBranchOperand(const std::vector<Syllable> &target) {
        uint32_t endAddr = orgBase_ + offset_ + 1;  // this field is 1 byte, right after the opcode already emitted
        uint32_t targetAddr = evalExpr(target);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) {
            fail("internal: branch target out of range - M6502Codegen.cpp's safeBranch() should have prevented this");
        }
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    // The full instruction vocabulary ../M6502Codegen.cpp ever emits -
    // see M6502Assembler.h's header comment. Not a general 6502
    // encoder: exactly these mnemonic/operand-shape combinations,
    // nothing else - most notably, no X/Y-indexed addressing modes
    // and no absolute-mode LDX/LDY/STX/STY/CMP/CPX/CPY at all (see
    // M6502Codegen.cpp's own comment for why none of those are ever
    // actually needed).
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        auto one = [&]() -> const std::vector<Syllable> & {
            if (ops.size() != 1) fail("'" + mnemonic + "' needs exactly one operand");
            return ops[0];
        };
        static const std::unordered_map<std::string, uint8_t> implied = {
            {"rts", 0x60}, {"clc", 0x18}, {"sec", 0x38}, {"pha", 0x48}, {"pla", 0x68}, {"cli", 0x58}, {"sei", 0x78},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> absolute = {
            {"lda", 0xAD}, {"sta", 0x8D}, {"adc", 0x6D}, {"sbc", 0xED}, {"and", 0x2D}, {"ora", 0x0D}, {"eor", 0x4D},
            {"asl", 0x0E}, {"lsr", 0x4E}, {"rol", 0x2E}, {"ror", 0x6E}, {"inc", 0xEE}, {"dec", 0xCE},
            {"jmp", 0x4C}, {"jsr", 0x20},
            // Real opcode $6C, JMP indirect: the operand is the
            // address of a two-byte cell holding the actual jump
            // target, little-endian, encoded exactly like every other
            // absolute operand above - the only difference is what the
            // real CPU does with it at runtime, not how it's assembled.
            {"jmpi", 0x6C},
        };
        if (auto it = absolute.find(mnemonic); it != absolute.end()) {
            emit8(it->second);
            emitAbsOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> immLow = {
            {"ldai", 0xA9}, {"ldyi", 0xA0}, {"adci", 0x69}, {"andi", 0x29}, {"eori", 0x49},
        };
        if (auto it = immLow.find(mnemonic); it != immLow.end()) {
            emit8(it->second);
            emitImmLow(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> immHigh = {
            {"ldai_hi", 0xA9},
            {"adci_hi", 0x69},
        };
        if (auto it = immHigh.find(mnemonic); it != immHigh.end()) {
            emit8(it->second);
            emitImmHigh(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> indY = {
            {"ldaind", 0xB1},
            {"staind", 0x91},
        };
        if (auto it = indY.find(mnemonic); it != indY.end()) {
            emit8(it->second);
            emitZpOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> branch = {
            {"beq", 0xF0}, {"bne", 0xD0}, {"bcs", 0xB0}, {"bmi", 0x30}, {"bpl", 0x10}, {"bvc", 0x50},
        };
        if (auto it = branch.find(mnemonic); it != branch.end()) {
            emit8(it->second);
            emitBranchOperand(one());
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    std::unordered_set<std::string> equateNames_;
    std::vector<Relocation> relocations_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    int stage_ = 1;
    bool allowExternal_;
};

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see M6502Assembler.h's own
// `assembleM6502Object` comment, and ../z80/Z80Assembler.cpp's own
// `buildObjectFile` for the template this is ported from (same
// algorithm; the only difference is three relocation kinds instead
// of one - see that constant's own header comment).
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kM6502CoffMachine;
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

std::string assembleM6502(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleM6502Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
