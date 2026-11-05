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

// `Reg` carries a register index (see regIndex() below) and whether
// it's `al` specifically (`is8`) - the one 8-bit register name this
// dialect ever uses (`mov al,<imm8>` / `out <port>,al`, both only for
// I386Codegen.cpp's own fixed PIC-masking sequence - see that file's
// own `generate()` comment). `Mem` is one of the exactly three
// addressing-mode shapes I386Codegen.cpp ever emits - `[label]`
// (`memHasEbx=false`), `[ebx]` (`memHasEbx=true`, `memHasDisp=false`),
// or `[label+ebx]` (both true, `expr` holding the label) - see
// I8086Assembler.cpp's own `Operand` comment, the same three shapes
// with `ebx` in place of `bx`; `Imm` covers a literal number or a
// bare symbol used as a jump/call target.
struct Operand {
    enum Kind { Reg, Mem, Imm } kind = Imm;
    int reg = -1;
    bool is8 = false;   // al
    bool is16 = false;  // ax
    // A memory operand's own size override ("byte [...]"/"word
    // [...]" - grammar/PccAsm.g4's own `operand::size`), for a
    // narrower-than-native-int Var/Slot's own load/store - see
    // I386Codegen.cpp's loadOperand()/storeOperand(). Absent (0)
    // means the plain, always-32-bit shapes every other mnemonic
    // here already used before those existed.
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

// `cl` maps to the same index as `ecx` - this dialect never uses any
// other 8-bit sub-register name besides `al` (handled separately in
// buildOperand() below, since unlike `cl` it needs its own 8-bit
// immediate/instruction forms, not just a shared index) - see
// I386Codegen.h's own header comment on this language's "every
// variable is a 32-bit int" model; `shl`/`shr`'s own real `<reg>,cl`
// shape (see encodeInstr() below) is the one real i386 instruction
// that still names an 8-bit register no matter how wide everything
// else here is, so there's no need for a real, separate 8-bit
// register index space just for that one.
int regIndex(const std::string &nameLower) {
    static const std::unordered_map<std::string, int> regs = {
        {"eax", 0}, {"ecx", 1}, {"edx", 2}, {"ebx", 3}, {"esp", 4}, {"ebp", 5}, {"esi", 6}, {"edi", 7}, {"cl", 1},
    };
    auto it = regs.find(nameLower);
    return it != regs.end() ? it->second : -1;
}

// `[...]` and plain-expr operand forms, classified into exactly the
// shapes I386Codegen.cpp ever emits - see Operand's own comment, and
// I8086Assembler.cpp's `buildOperand` for the 16-bit original this is
// ported from.
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
        } else if (syls.size() == 1 && !isEbx(syls[0])) {
            op.memHasEbx = false;
            op.memHasDisp = true;
            op.expr = {syls[0]};
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

// One pending fixup `assembleI386Object()` (see I386Assembler.h's own
// header comment) turns into a real ObjRelocation once assembly is
// done - `byteOffset` into `bytes()`, `symbolName` the reference that
// couldn't be resolved locally (or the reserved pseudo-symbol ".."
// for a same-file address's own needed base-shift), `kind` one of
// `kI386RelocAbs32`/`kI386RelocRel32`.
struct Relocation {
    uint32_t byteOffset;
    std::string symbolName;
    uint16_t kind;
};

// --- Two-pass assembly ----------------------------------------------------
// One flat blob, `org` called exactly once at the top (this backend's
// Multiboot header is loaded and runs at the same fixed address - see
// I386Codegen.cpp's own `kLoadAddr`) - same single `orgBase_` shape as
// M68kAssembler.cpp's own assembler, no section-switching needed the
// way STM32Assembler.cpp's flash/RAM split requires.
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
                // Routed through the same emitAbs32() choke point a bare
                // label used as a 32-bit immediate goes through (see its
                // own header comment) - a "dd" word holding a symbol (an
                // array initializer's dispatch-table element, see
                // I386Codegen.cpp's own arrayElemText) gets the exact same
                // relocation treatment, while a plain numeric word's value
                // comes out unchanged.
                for (const auto &e : line.exprList) emitAbs32(e);
                return;
            case Line::Instr:
                encodeInstr(line.mnemonic, line.operands);
                return;
        }
    }

    // `externalName`/`needsBaseShift` mirror
    // ../m68k/M68kAssembler.cpp's own `evalExpr` exactly - see
    // I386Assembler.h's own header comment on why *any* local address
    // reference, not just an external one, needs a relocation here
    // once `allowExternal_` is set.
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

    // One of this dialect's three memory-operand shapes (see
    // Operand's own comment) as a real i386 ModRM [+disp32] pair,
    // with `reg` (0-7) already carrying the instruction's own real
    // register field (which register the memory operand is being
    // combined with/into - mov's own direction is the caller's
    // concern, not this helper's). Mod/rm values confirmed directly
    // against a real toolchain - see I386Assembler.h's own header
    // comment.
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
            // [ebp+disp32]: mod=10, rm=101 (ebp) - mod=00/rm=101 is
            // reserved for the no-base direct-[label] form below, so a
            // disp32-with-base ebp reference always needs mod=10 even
            // when the displacement would otherwise fit in a byte (a
            // Local/Param's own addr() - I386Codegen.h - never emits a
            // zero displacement, so the mod=01/rm=101 "[ebp]" edge case
            // never arises here).
            emit8(static_cast<uint8_t>(0x80 | (reg << 3) | 0x05));
            emitAbs32(mem.expr);
            return;
        }
        // direct [label]: mod=00, rm=101, disp32-only addressing
        emit8(static_cast<uint8_t>(0x00 | (reg << 3) | 0x05));
        emitAbs32(mem.expr);
    }

    // The one choke point every absolute-32-bit-address reference in
    // this dialect passes through - `[label]`/`[label+ebx]` disp32
    // fields, the moffs32 `mov eax,[label]`/`mov [label],eax`
    // shortcut forms, `add eax,<label>`'s own short-immediate form,
    // and a bare label used as a 32-bit immediate (`mov eax,<label>` -
    // genAddrOf()'s own address-of-a-static-variable case, the same
    // role as m68k's `leal`). Records a `kI386RelocAbs32` relocation
    // against the reserved pseudo-symbol ".." whenever the reference
    // needs its own object's base added (every one of them, once
    // linked - see I386Assembler.h's own header comment), plus a
    // second one against the real symbol name if it's not yet defined
    // in this object at all.
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

    // The full instruction vocabulary ../I386Codegen.cpp ever emits -
    // see I386Assembler.h's own header comment. Not a general i386
    // encoder: exactly these mnemonic/operand-shape combinations,
    // each confirmed against a real toolchain's own encoding during
    // development.
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
                // mul/div (unsigned) alongside imul/idiv (signed) -
                // I386Codegen.cpp picks between them per operand
                // signedness (see genMul()/genDivMod()).
                {"not", 2}, {"neg", 3}, {"mul", 4}, {"imul", 5}, {"div", 6}, {"idiv", 7},
            };
            emit8(0xF7);
            emit8(static_cast<uint8_t>(0xC0 | (ext.at(mnemonic) << 3) | r));
            return;
        }
        if (mnemonic == "shl" || mnemonic == "shr" || mnemonic == "sar") {
            // Two shapes: the real variable-count register form
            // (`<reg>,cl` - I386Codegen.cpp's own `<<`/`>>` operator,
            // where the count is itself a runtime value) and the real
            // immediate-count form (`<reg>,<N>` - computeElementAddr's
            // own fixed "multiply the index by 4" shift, where the
            // count is always a compile-time constant - see that
            // file's own comment). Both confirmed against a real
            // toolchain's own encoding - see I386Assembler.h's own
            // header comment.
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            const Operand &count = requireOperand(ops, 1, mnemonic);
            // sar (arithmetic/signed right shift, ext=7) alongside
            // shr (logical/unsigned, ext=5) - I386Codegen.cpp picks
            // between them per the shifted value's own signedness.
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
                // `mov al,<imm8>` - I386Codegen.cpp's own fixed PIC-
                // masking sequence only (see that file's own
                // `generate()` comment) - confirmed against a real
                // toolchain's own encoding.
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
                // `mov eax,[label]` (no base register) uses the real
                // moffs32 shortcut form - confirmed only ever paired
                // with `eax` here (see I386Codegen.cpp - the "ac"
                // register is the only one ever loaded straight from
                // a plain label); every other shape (`[ebx]`,
                // `[label+ebx]`, or any other destination register)
                // goes through the general ModRM form instead.
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
            // `mov byte [mem],al` - a narrower-than-native-int Var/
            // Slot's own store (see I386Codegen.cpp's storeOperand())
            // - the real r/m8,r8 form (88 /r), writing only that one
            // byte of memory, unlike the always-32-bit forms above.
            if (dst.kind == Operand::Mem && dst.memSize == Operand::Byte && src.kind == Operand::Reg && src.is8) {
                emit8(0x88);
                emitModRMMem(dst, static_cast<uint8_t>(src.reg));
                return;
            }
            // `mov word [mem],ax` - same idea, 16 bits: the real
            // r/m16,r16 form, which is the always-32-bit 89 /r above
            // with the standard 0x66 operand-size override prefix.
            if (dst.kind == Operand::Mem && dst.memSize == Operand::Word && src.kind == Operand::Reg && src.is16) {
                emit8(0x66);
                emit8(0x89);
                emitModRMMem(dst, static_cast<uint8_t>(src.reg));
                return;
            }
            fail("'mov': unsupported operand combination in this dialect");
        }
        if (mnemonic == "movzx" || mnemonic == "movsx") {
            // `movzx/movsx eax, byte/word [mem]` (a narrower-than-
            // native-int Var/Slot's own load) or `movzx/movsx eax,al`
            // / `...,ax` (re-extending a value already sitting in a
            // register, e.g. narrowEaxTo()'s own re-truncation) - zero-
            // or sign-extended up to a full 32-bit register in one
            // real instruction (0F B6/B7 zero-extend, 0F BE/BF sign-
            // extend, byte vs word respectively) - see
            // I386Codegen.cpp's own loadOperand()/narrowEaxTo().
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
            // `lea reg,[ebp+disp32]` - I386Codegen.cpp's own way to
            // compute a Local/Param's own runtime effective address
            // (its own stack slot has no compile-time-foldable label
            // the way a Global's does) - real 8D /r, same real
            // instruction as I8086Codegen.cpp's own `lea`, just 32
            // bits wide throughout.
            if (ops.size() != 2 || ops[0].kind != Operand::Reg || ops[1].kind != Operand::Mem) {
                fail("'lea' needs a register destination and a memory source in this dialect");
            }
            emit8(0x8D);
            emitModRMMem(ops[1], static_cast<uint8_t>(ops[0].reg));
            return;
        }
        if (mnemonic == "add" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 0, mnemonic).reg != 0 && requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // `add <reg>,<N or label>` for any register but eax (which
            // already has its own shorter `05`-opcode form above) -
            // two real call sites: `add esp,<N>` (I386Codegen.cpp's
            // own caller-side stack cleanup after a `Call`, always a
            // small literal count) and `add ebx,<label>`
            // (computeElementAddr()'s own array-base-plus-scaled-index
            // computation when the array is a Global - a real
            // relocatable address, not a literal). The real `81 /0
            // id` full-imm32 form (through the same emitAbs32() choke
            // point every other absolute address reference uses) - a
            // literal count just costs three wasted immediate bytes.
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0x81);
            emit8(static_cast<uint8_t>(0xC0 | (0 << 3) | dst));
            emitAbs32(requireOperand(ops, 1, mnemonic).expr);
            return;
        }
        if (mnemonic == "sub" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // `sub esp,<N>` - I386Codegen.cpp's own per-call stack-
            // frame prologue (reserving `frameSize` dwords of local
            // storage) - the real `83 /5 ib` sign-extended-imm8 form,
            // same shape as `cmp`'s own immediate case just above
            // (every frame this backend ever builds is comfortably
            // under 128 dwords).
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint32_t imm = evalExpr(requireOperand(ops, 1, mnemonic).expr);
            if (imm > 0x7F && imm < 0xFFFFFF80) fail("'sub': immediate operand must fit in a signed byte");
            emit8(0x83);
            emit8(static_cast<uint8_t>(0xC0 | (5 << 3) | dst));
            emit8(static_cast<uint8_t>(imm & 0xFF));
            return;
        }
        if (mnemonic == "out") {
            // `out <port>,al` - I386Codegen.cpp's own fixed PIC-
            // masking sequence only. The real imm8-port form
            // (confirmed against a real toolchain's own encoding).
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
            // `add eax,<label>` - genAddrOf()'s own "base address plus
            // scaled index" computation for a plain (non-pointer)
            // array's own `&arr[idx]`, where the label itself is a
            // compile-time constant address, not a register - the
            // real eax-specific short immediate form (confirmed
            // against a real toolchain's own encoding - see
            // I386Assembler.h's own header comment).
            emit8(0x05);
            emitAbs32(requireOperand(ops, 1, mnemonic).expr);
            return;
        }
        if (mnemonic == "add" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg &&
            requireOperand(ops, 1, mnemonic).kind == Operand::Mem) {
            // `add eax,[label]` - genAddrOf()'s own version of the
            // same computation when the base is a plain pointer
            // *variable* rather than a declared array: the base has
            // to be read out of memory first, not folded into the
            // instruction as a compile-time constant the way the
            // `Imm` case above is - the real register+memory ADD form
            // (confirmed against a real toolchain's own encoding -
            // see I386Assembler.h's own header comment).
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0x03);
            emitModRMMem(requireOperand(ops, 1, mnemonic), dst);
            return;
        }
        if (mnemonic == "cmp" && requireOperand(ops, 1, mnemonic).kind == Operand::Imm) {
            // `cmp <reg>,0` - the only immediate `cmp` I386Codegen.cpp
            // ever emits (every `if`/`while` condition check, plus
            // every logical-operator short-circuit test), always
            // against a literal that fits in a sign-extended imm8 -
            // the real `83 /7 ib` form (confirmed against a real
            // toolchain's own encoding choice for `cmp eax,0` - see
            // I386Assembler.h's own header comment).
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
            // adc/sbb (add/sub-with-carry/borrow) - the real r/m32,r32
            // ALU forms, same shape as add/sub above - I386Codegen.cpp's
            // own ripple-carry chain for a `long long` add/sub (2
            // native 32-bit words).
            {"adc", 0x11}, {"sbb", 0x19},
        };
        if (auto it = aluRegReg.find(mnemonic); it != aluRegReg.end()) {
            uint8_t dst = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            uint8_t src = requireReg(requireOperand(ops, 1, mnemonic), mnemonic);
            emit8(it->second);
            emit8(static_cast<uint8_t>(0xC0 | (src << 3) | dst));
            return;
        }
        if (mnemonic == "call" && requireOperand(ops, 0, mnemonic).kind == Operand::Reg) {
            // `call eax` (I386Codegen.cpp's new ExprKind::CallPtr) - a
            // real, direct near-indirect call through a register: FF
            // /2, the same "group FF" ModRM-register form `not`/
            // `imul`/`idiv` above already use, just with reg field
            // 0b010 ("CALL r/m32, near indirect") - confirmed against
            // a real toolchain's own encoding. No REX prefix concern
            // here (this dialect is pure 32-bit protected mode, not
            // long mode), and the plain register-direct ModRM byte
            // (mod=11) needs no disp32 extension.
            uint8_t r = requireReg(requireOperand(ops, 0, mnemonic), mnemonic);
            emit8(0xFF);
            emit8(static_cast<uint8_t>(0xC0 | (2 << 3) | r));
            return;
        }
        if (mnemonic == "call" || mnemonic == "jmp") {
            // `call`/`jmp label` (E8/E9 rel32) - the one place this
            // dialect's control flow can reach another file's own
            // code (every conditional `jcc` below is always an
            // intra-function branch to a `newLabel()`-generated
            // target - see I386Codegen.cpp's own call sites - so it
            // never needs a relocation). A same-object target's own
            // relative displacement is base-shift-invariant (both the
            // site and the target move by the same linked base - see
            // ../m68k/M68kAssembler.cpp's own `encodeBranch` comment
            // for the identical reasoning), so it's only deferred as
            // a real `kI386RelocRel32` relocation when the target
            // isn't defined in this object at all.
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
            // Unsigned counterparts - I386Codegen.cpp's own unsigned
            // comparisons (real x86 jb/jae/jbe/ja, same two-byte 0F 8x
            // family as the signed ones above).
            {"jb", 0x82}, {"jae", 0x83}, {"jbe", 0x86}, {"ja", 0x87},
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

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - see I386Assembler.h's own `assembleI386Object`
// comment, and ../m68k/M68kAssembler.cpp's own `buildObjectFile` for
// the identical algorithm this is ported from.
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
