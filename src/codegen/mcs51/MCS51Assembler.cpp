#include "MCS51Assembler.h"

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

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("MCS51Assembler: " + msg); }

// --- Parsing, via the one shared grammar/PccAsm.g4 - same shape as
// ../m6502/M6502Assembler.cpp's own Syllable/Line (see that file's
// own header comment); nothing here is specific to the 8051 at all.

enum class SylKind { Number, Symbol, Group };
struct Syllable {
    char sign = 0;
    SylKind kind;
    std::string text;
    std::vector<Syllable> group;
};

struct Line {
    std::vector<std::string> labels;
    // `Xdata` reserves `exprList`'s own byte count in the XDATA
    // address space for every label attached to this line, instead of
    // emitting any CODE bytes - see MCS51Assembler.h's own header
    // comment on the `xdata <count>` directive.
    enum Kind { None, Equate, Org, Instr, Xdata } kind = None;
    std::string equateName;                      // Equate
    std::vector<std::vector<Syllable>> exprList;  // Equate (1)/Org (1)/Xdata (1, the byte count)
    std::string mnemonic;                         // Instr
    std::vector<std::vector<Syllable>> operands;  // Instr
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
    // A pure compile-time symbol=value binding, no bytes emitted - used
    // for this backend's own fixed internal-RAM pseudo-registers
    // (`ac`/`bc`/`cx`/`dx`/`ptr` - see MCS51Codegen.cpp's own comment
    // on why those need a *fixed* address rather than the usual
    // sequentially-laid-out one).
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
    if (mnemonic == "xdata") {
        line.kind = Line::Xdata;
        if (instr->operand().size() != 1 || instr->operand(0)->LBRACKET()) {
            fail("'xdata' needs exactly one operand (a byte count)");
        }
        line.exprList.push_back(exprToSyllables(instr->operand(0)->expr()));
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

// One pending fixup assembleMCS51Object() (see MCS51Assembler.h's own
// header comment) turns into a real ObjRelocation once assembly is
// done - mirrors ../i8008/I8008Assembler.cpp's own `Relocation`. Only
// ever populated against `kMCS51RelocAbs16` here (see
// emitAbsOperand() below) - this dialect has no split hi/lo pair the
// way i8008/m6502 need, so there's no `kind` field at all.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
};

// --- Two-pass assembly ----------------------------------------------------
//
// Every instruction's size is fixed by its mnemonic alone (see
// MCS51Assembler.h) - pass 1 and pass 2 run the exact same encoding
// logic, same reasoning as every other backend's own in-process
// assembler here. `labels_` (which also holds every Equate binding -
// see processLine's own comment) deliberately isn't reset between
// passes: pass 1 already resolves every value correctly (since sizes
// never depend on values), pass 2 just re-derives the identical
// bindings and this time actually emits bytes using them.
class Assembler {
   public:
    // `allowExternal` is object mode (see MCS51Assembler.h's own
    // `assembleMCS51Object()`): an undefined CODE-space label in
    // `LJMP`/`LCALL`'s own target operand becomes a relocation
    // instead of a hard error, and every *resolved* local label used
    // there also gets an additional relocation against the reserved
    // pseudo-symbol ".." (see emitAbsOperand() below) - mirrors
    // ../i8008/I8008Assembler.cpp's own `Assembler(bool
    // allowExternal)`.
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
    // This object's own final XDATA-space usage, in bytes - every
    // `xdata <count>` directive's own `count` summed in declaration
    // order, starting from 0 (see `dataCursor_`'s own comment) -
    // becomes `ObjectFile::dataSize` (see buildObjectFile() below).
    uint32_t dataSize() const { return dataCursor_; }
    const std::unordered_map<std::string, uint32_t> &dataLabels() const { return dataLabels_; }

   private:
    void pass(const std::vector<Line> &lines) {
        bytes_.clear();
        relocations_.clear();
        offset_ = 0;
        orgBase_ = 0;
        dataCursor_ = 0;
        for (const auto &line : lines) processLine(line);
    }

    void processLine(const Line &line) {
        // An `xdata` line's own label(s) bind into `dataLabels_`
        // instead of the default CODE-space `labels_` binding every
        // other line's label(s) get below - these two tables are
        // entirely independent (see MCS51Assembler.h's own header
        // comment), so a label never ends up in both.
        if (line.kind != Line::Xdata) {
            for (const auto &lbl : line.labels) labels_[lbl] = orgBase_ + offset_;
        }
        switch (line.kind) {
            case Line::None:
                return;
            case Line::Equate:
                labels_[line.equateName] = evalExpr(line.exprList.at(0));
                return;
            case Line::Org:
                orgBase_ = evalExpr(line.exprList.at(0));
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
            case Line::Xdata: {
                uint32_t count = evalExpr(line.exprList.at(0));
                for (const auto &lbl : line.labels) dataLabels_[lbl] = dataCursor_;
                dataCursor_ += count;
                return;
            }
        }
    }

    // `externalName`/`needsBaseShift` are only ever passed by
    // emitAbsOperand() below (see that function's own comment) - every
    // other operand kind (direct-RAM, immediate, rel8) never resolves
    // a relocatable CODE-space address at all, so they call this with
    // both left at their default `nullptr` and get plain local-only
    // resolution, same as assembleMCS51()'s own non-object use.
    // Mirrors ../i8008/I8008Assembler.cpp's own `evalExpr`.
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

    // A plain numeric syllable's own text, decimal or `0x`-prefixed
    // hex - shared by `resolveSyllable()` (CODE-space) and
    // `resolveDataSyllable()` (XDATA-space) below, since a number
    // means exactly the same thing in either expression.
    static uint32_t parseNumber(const std::string &t) {
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            return static_cast<uint32_t>(std::stoul(t.substr(2), nullptr, 16));
        }
        return static_cast<uint32_t>(std::stoul(t));
    }

    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr, bool *wasLocalAddress = nullptr) {
        if (syl.kind == SylKind::Group) return evalExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
        auto it = labels_.find(syl.text);
        if (it != labels_.end()) {
            // Every caller that tracks `wasLocalAddress` at all
            // (emitAbsOperand() below) only ever resolves a real
            // CODE-space label (a function or branch target) this
            // way, never one of the fixed internal-RAM pseudo-register
            // equates - see MCS51Assembler.h's own header comment on
            // why that's structurally guaranteed rather than needing
            // its own exclusion list the way ../i8008/I8008Assembler.cpp's
            // own `equateNames_` does.
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

    // XDATA-space counterpart of `evalExpr()` above - resolves every
    // syllable against `dataLabels_` instead of `labels_`, for
    // `movdptr`'s own operand (see `emitDataPtrOperand()` below). A
    // data-space symbol is never deferred as an external reference
    // the way a CODE-space one can be in object mode (see
    // `resolveDataSyllable()`'s own comment) - MCS51Codegen.cpp's own
    // `generate()` pre-pass only ever emits an `xdata` directive for a
    // variable this file itself defines, so an undefined one here
    // always means a real, same-file bug (a reference to an
    // `isExternal` global, or an internal-error coding mistake), never
    // something a linker could resolve later.
    uint32_t evalDataExpr(const std::vector<Syllable> &syls) {
        uint32_t total = 0;
        for (const auto &syl : syls) {
            uint32_t v = resolveDataSyllable(syl);
            if (syl.sign == '-') {
                total -= v;
            } else {
                total += v;
            }
        }
        return total;
    }

    uint32_t resolveDataSyllable(const Syllable &syl) {
        if (syl.kind == SylKind::Group) return evalDataExpr(syl.group);
        if (syl.kind == SylKind::Number) return parseNumber(syl.text);
        auto it = dataLabels_.find(syl.text);
        if (it != dataLabels_.end()) return it->second;
        if (stage_ == 2) {
            fail("internal: '" + syl.text + "' has no XDATA address - MCS51Codegen.cpp's own generate() pre-pass "
                 "should have emitted an 'xdata' directive for it before any code referencing it (a data-space "
                 "symbol can never be forward-declared across files - see MCS51Codegen.h's own header comment)");
        }
        return 0;
    }

    // --- byte emission --------------------------------------------

    void emit8(uint8_t b) {
        bytes_.push_back(b);
        offset_ += 1;
    }

    // A direct-RAM address operand (0-255; every pseudo-register
    // equate and every real SFR this dialect ever names resolves to
    // one of these - DPL/DPH's own fixed addresses are baked directly
    // into movdpl/movdph below when they're the *destination*, but
    // also reachable as an ordinary direct operand via the `dpl`/
    // `dph` equates MCS51Codegen.cpp's own generate() declares, for
    // reading DPTR back out after loadXdataAddress()'s own `movdptr`).
    void emitDirectOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("direct-address operand doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // An immediate 8-bit operand - MCS51Codegen.cpp only ever passes
    // plain literals <256 here, never a label (unlike direct
    // operands, which are always a pseudo-register's own equate).
    void emitImm8Operand(const std::vector<Syllable> &e) {
        uint32_t v = evalExpr(e);
        if (stage_ == 2 && v > 0xFF) fail("immediate operand doesn't fit in a byte");
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // `mov dptr,#imm16`'s own operand - the only instruction this
    // dialect ever gives an XDATA-space symbol to (every global/
    // local/param's own `label()`, optionally `+<constant>` for an
    // array element - see MCS51Codegen.cpp's own declareGlobalArray()
    // comment), resolved against `dataLabels_` via `evalDataExpr()`,
    // high byte first (real 8051 encoding order). In object mode this
    // always also gets a relocation against the reserved pseudo-
    // symbol `"..d"` - even a same-file reference, exactly like
    // `emitAbsOperand()`'s own `".."` handling below, and for the same
    // reason (see MCS51Assembler.h's own `kMCS51RelocAbs16` comment).
    void emitDataPtrOperand(const std::vector<Syllable> &e) {
        uint32_t v = evalDataExpr(e);
        if (stage_ == 2 && allowExternal_) {
            relocations_.push_back({offset_, "..d"});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // `ljmp`/`lcall`'s own absolute CODE-space operand - always a
    // function or branch-target label (see MCS51Assembler.h's own
    // header comment on the CODE/XDATA symbol split) - high byte
    // first, same real encoding order as emitDataPtrOperand() above.
    // The one place this assembler's own CODE-space resolution ever
    // needs a relocation (see MCS51Assembler.h's own
    // `kMCS51RelocAbs16` comment): a locally-resolved label still
    // needs this object's own base added once linked (the ".."
    // pseudo-symbol), and an undefined one (another file's own
    // function, in object mode) is deferred to the linker entirely -
    // mirrors ../i8008/I8008Assembler.cpp's own `emit16`.
    void emitAbsOperand(const std::vector<Syllable> &e) {
        std::string externalName;
        bool needsBaseShift = false;
        uint32_t v = evalExpr(e, &externalName, &needsBaseShift);
        if (stage_ == 2 && allowExternal_) {
            if (needsBaseShift) relocations_.push_back({offset_, ".."});
            if (!externalName.empty()) relocations_.push_back({offset_, externalName});
        }
        emit8(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit8(static_cast<uint8_t>(v & 0xFF));
    }

    // A short conditional branch's own signed rel8 operand.
    // MCS51Codegen.cpp's own safeBranch()/safeDjnz() (see that file's
    // own comment) guarantee every one of these always targets the
    // very next instruction - always trivially in range - so an out-
    // of-range target here can only mean a real bug in that
    // guarantee, not a legitimately-far user-code target that needs
    // handling.
    void emitRelOperand(const std::vector<Syllable> &target) {
        uint32_t endAddr = orgBase_ + offset_ + 1;  // this field is 1 byte, right after the opcode (+ any direct/imm
                                                     // byte already emitted by the caller)
        uint32_t targetAddr = evalExpr(target);
        int32_t rel = static_cast<int32_t>(targetAddr) - static_cast<int32_t>(endAddr);
        if (stage_ == 2 && (rel < -128 || rel > 127)) {
            fail("internal: branch target out of range - MCS51Codegen.cpp's safeBranch()/safeDjnz() should have "
                 "prevented this");
        }
        emit8(static_cast<uint8_t>(static_cast<int8_t>(rel)));
    }

    // The full instruction vocabulary ../MCS51Codegen.cpp ever emits -
    // see MCS51Assembler.h's own header comment. Not a general 8051
    // encoder: exactly these mnemonic/operand-shape combinations,
    // nothing else.
    void encodeInstr(const std::string &mnemonic, const std::vector<std::vector<Syllable>> &ops) {
        auto one = [&]() -> const std::vector<Syllable> & {
            if (ops.size() != 1) fail("'" + mnemonic + "' needs exactly one operand");
            return ops[0];
        };
        auto two = [&]() -> const std::vector<std::vector<Syllable>> & {
            if (ops.size() != 2) fail("'" + mnemonic + "' needs exactly two operands");
            return ops;
        };

        static const std::unordered_map<std::string, uint8_t> implied = {
            {"ret", 0x22},    {"reti", 0x32},  {"nop", 0x00},    {"clra", 0xE4},   {"cpla", 0xF4},
            {"clrc", 0xC3},   {"rla", 0x23},   {"rra", 0x03},    {"rlca", 0x33},   {"rrca", 0x13},
            {"mul", 0xA4},    {"div", 0x84},   {"incdptr", 0xA3}, {"movxr", 0xE0}, {"movxw", 0xF0},
            {"movc", 0x93},   {"jmpadptr", 0x73},
        };
        if (auto it = implied.find(mnemonic); it != implied.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second);
            return;
        }
        // SETB/CLR on the two fixed bit addresses this dialect ever
        // needs (EA - the global interrupt-enable bit, IE.7 = 0xAF -
        // see MCS51Codegen.cpp's own Intrinsic handling): a real
        // `SETB bit`/`CLR bit` is otherwise a general-bit-address
        // instruction this dialect deliberately doesn't expose (no
        // I/O pin or on-chip-peripheral modeling at all - see
        // MCS51Codegen.h's own header comment), so these two are
        // spelled out as their own fixed-encoding mnemonics instead
        // of adding real bit-address operand syntax for a single use.
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> fixedBit = {
            {"setea", {0xD2, 0xAF}}, {"clrea", {0xC2, 0xAF}},
        };
        if (auto it = fixedBit.find(mnemonic); it != fixedBit.end()) {
            if (!ops.empty()) fail("'" + mnemonic + "' takes no operand");
            emit8(it->second.first);
            emit8(it->second.second);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> direct = {
            {"mova", 0xE5}, {"movd", 0xF5}, {"incd", 0x05}, {"decd", 0x15}, {"add", 0x25},
            {"addc", 0x35}, {"subb", 0x95}, {"anl", 0x55},  {"orl", 0x45},  {"xrl", 0x65},
            {"push", 0xC0}, {"pop", 0xD0},
        };
        if (auto it = direct.find(mnemonic); it != direct.end()) {
            emit8(it->second);
            emitDirectOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> imm8 = {
            {"movai", 0x74}, {"addi", 0x24}, {"addci", 0x34}, {"subbi", 0x94}, {"anli", 0x54}, {"orli", 0x44},
            {"xrli", 0x64},
        };
        if (auto it = imm8.find(mnemonic); it != imm8.end()) {
            emit8(it->second);
            emitImm8Operand(one());
            return;
        }
        // `MOV DPL,direct`/`MOV DPH,direct` - the real direct-to-
        // direct `MOV` (0x85) encodes its *source* direct address
        // first and its *destination* second, no matter which way
        // round this dialect's own mnemonic reads (see
        // MCS51Codegen.h's own header comment) - DPL is SFR 0x82,
        // DPH is 0x83.
        if (mnemonic == "movdpl" || mnemonic == "movdph") {
            emit8(0x85);
            emitDirectOperand(one());
            emit8(mnemonic == "movdpl" ? 0x82 : 0x83);
            return;
        }
        // A general direct-to-direct `MOV dst,src` (0x85) -
        // MCS51Codegen.cpp's own copy16(), moving one pseudo-register
        // to another without detouring through the accumulator at
        // all (a real single-instruction 8051 feature `movdpl`/
        // `movdph` above are themselves just fixed-destination
        // special cases of).
        if (mnemonic == "movdd") {
            const auto &o = two();
            emit8(0x85);
            emitDirectOperand(o[1]);  // src first (real hardware encoding order)
            emitDirectOperand(o[0]);  // dst second
            return;
        }
        if (mnemonic == "movdptr") {
            emit8(0x90);
            emitDataPtrOperand(one());
            return;
        }
        // The same real `MOV DPTR,#data16` encoding as `movdptr`
        // above (opcode 0x90), but with a CODE-space operand (a
        // function or branch-target label, resolved and relocated via
        // `emitAbsOperand()` exactly like `ljmp`/`lcall`'s own target)
        // instead of an XDATA-space one - used wherever a function's
        // own entry address, or any other CODE-space address, is
        // needed as a plain 16-bit value rather than as something to
        // jump/call to.
        if (mnemonic == "movdptrc") {
            emit8(0x90);
            emitAbsOperand(one());
            return;
        }
        if (mnemonic == "movdi") {  // MOV direct,#imm
            const auto &o = two();
            emit8(0x75);
            emitDirectOperand(o[0]);
            emitImm8Operand(o[1]);
            return;
        }
        static const std::unordered_map<std::string, uint8_t> abs16 = {
            {"ljmp", 0x02},
            {"lcall", 0x12},
        };
        if (auto it = abs16.find(mnemonic); it != abs16.end()) {
            emit8(it->second);
            emitAbsOperand(one());
            return;
        }
        static const std::unordered_map<std::string, uint8_t> rel = {
            {"jz", 0x60}, {"jnz", 0x70}, {"jc", 0x40}, {"jnc", 0x50},
        };
        if (auto it = rel.find(mnemonic); it != rel.end()) {
            emit8(it->second);
            emitRelOperand(one());
            return;
        }
        // `JB bit,rel`/`JNB bit,rel` on the two fixed bits this
        // dialect ever tests - ACC.7 (0xE7, the sign bit, for signed
        // compares) and PSW.OV (0xD2, for the same compares' overflow
        // fixup) - same "fixed bit, no general bit-address syntax"
        // reasoning as setea/clrea above.
        static const std::unordered_map<std::string, std::pair<uint8_t, uint8_t>> fixedBitBranch = {
            {"jbacc7", {0x20, 0xE7}}, {"jnbacc7", {0x30, 0xE7}}, {"jbov", {0x20, 0xD2}}, {"jnbov", {0x30, 0xD2}},
        };
        if (auto it = fixedBitBranch.find(mnemonic); it != fixedBitBranch.end()) {
            emit8(it->second.first);
            emit8(it->second.second);
            emitRelOperand(one());
            return;
        }
        if (mnemonic == "djnz") {
            const auto &o = two();
            emit8(0xD5);
            emitDirectOperand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        // CJNE isn't emitted by MCS51Codegen.cpp itself (every
        // compare here goes through the uniform SUBB-chain idiom -
        // see MCS51Codegen.h's own header comment), but is still part
        // of this dialect (reachable via the language's own `asm`
        // escape) since it's one of the instructions this target's
        // real instruction set is built around.
        if (mnemonic == "cjnei") {  // CJNE A,#imm,rel
            const auto &o = two();
            emit8(0xB4);
            emitImm8Operand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        if (mnemonic == "cjne") {  // CJNE A,direct,rel
            const auto &o = two();
            emit8(0xB5);
            emitDirectOperand(o[0]);
            emitRelOperand(o[1]);
            return;
        }
        fail("unsupported instruction '" + mnemonic + "'");
    }

    std::vector<Relocation> relocations_;
    bool allowExternal_;
    std::vector<uint8_t> bytes_;
    std::unordered_map<std::string, uint32_t> labels_;
    uint32_t offset_ = 0;
    uint32_t orgBase_ = 0;
    // XDATA-space label table and cursor - entirely independent of
    // `labels_`/`offset_`/`orgBase_` above (see MCS51Assembler.h's own
    // header comment): `dataCursor_` starts at 0 and only ever
    // advances via an `xdata <count>` directive line (see
    // `processLine()`'s own `Line::Xdata` case), never via any
    // instruction's byte emission. Like `labels_`, `dataLabels_` isn't
    // reset between passes - pass 1 already resolves every value
    // correctly, pass 2 just re-derives the identical bindings.
    std::unordered_map<std::string, uint32_t> dataLabels_;
    uint32_t dataCursor_ = 0;
    int stage_ = 1;
};

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see ../i8008/I8008Assembler.cpp's own
// `buildObjectFile` for the template this is ported from (identical
// algorithm for the CODE side, minus the per-relocation `kind` field
// this dialect doesn't need - see MCS51Assembler.h's own
// `kMCS51RelocAbs16` comment), plus this target's own Harvard-
// specific second half: every `dataLabels_` entry becomes an
// `isData=true` symbol (see ObjectFile.h's own comment) instead of a
// CODE-space one, and `dataCursor_`'s own final value becomes
// `ObjectFile::dataSize` - GenericLinker.cpp's own second, data-space
// layout pass (see GenericLinker.h's own `dataBaseAddress` comment)
// is what actually turns these into real linked addresses.
ObjectFile buildObjectFile(const Assembler &a) {
    ObjectFile obj;
    obj.machineType = kMCS51CoffMachine;
    const auto &bytesVec = a.bytes();
    obj.textBytes.assign(bytesVec.begin(), bytesVec.end());
    obj.dataSize = a.dataSize();

    for (const auto &kv : a.labels()) {
        obj.symbols.push_back({kv.first, kv.second - a.orgBase(), /*isDefined=*/true});
    }
    for (const auto &kv : a.dataLabels()) {
        obj.symbols.push_back({kv.first, kv.second, /*isDefined=*/true, /*isData=*/true});
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
        obj.relocations.push_back({r.byteOffset, r.symbolName, kMCS51RelocAbs16});
    }
    return obj;
}

}  // namespace

std::string assembleMCS51(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/false);
    assembler.run(lines);
    const auto &bytes = assembler.bytes();
    return std::string(bytes.begin(), bytes.end());
}

ObjectFile assembleMCS51Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
