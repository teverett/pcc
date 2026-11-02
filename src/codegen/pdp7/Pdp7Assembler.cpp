#include "Pdp7Assembler.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
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

namespace pcc {

namespace {

// Same convention as ../../../../pdp-assembler/src/Assembler.h: bit
// 0x40000 (01000000 octal) flags a value as relative-to-`..`.
constexpr uint32_t RELATIVE = 01000000;
constexpr uint32_t MASK18 = 0777777;
constexpr uint32_t BASE = 0 | RELATIVE;

// Builtin opcode table, copied verbatim (every mnemonic and octal
// value) from ../../../../pdp-assembler/src/Assembler.cpp's
// constructor - already verified correct by that project; Pdp7Codegen
// only ever emits a subset of these, but keeping the full table costs
// nothing and future-proofs against new instructions it might start
// using.
const std::unordered_map<std::string, uint32_t> &builtins() {
    static const std::unordered_map<std::string, uint32_t> table = {
        {"save", 1}, {"getuid", 2}, {"open", 3}, {"read", 4}, {"write", 5},
        {"creat", 6}, {"seek", 7}, {"tell", 8}, {"close", 9}, {"link", 10},
        {"unlink", 11}, {"setuid", 12}, {"rename", 13}, {"exit", 14}, {"time", 15},
        {"intrp", 16}, {"chdir", 17}, {"chmod", 18}, {"chown", 19},
        {"sysloc", 21}, {"capt", 23}, {"rele", 24}, {"status", 25},
        {"smes", 27}, {"rmes", 28}, {"fork", 29},

        {"sys", 0020000}, {"i", 0020000},

        {"dac", 0040000}, {"jms", 0100000}, {"dzm", 0140000}, {"lac", 0200000},
        {"xor", 0240000}, {"add", 0300000}, {"tad", 0340000}, {"xct", 0400000},
        {"isz", 0440000}, {"and", 0500000}, {"sad", 0540000}, {"jmp", 0600000},

        {"eae", 0640000}, {"osc", 0640001}, {"omq", 0640002}, {"cmq", 0640004},
        {"div", 0640323}, {"norm", 0640444}, {"lls", 0640600}, {"clls", 0641600},
        {"als", 0640700}, {"lrs", 0640500}, {"ecla", 0641000}, {"lacs", 0641001},
        {"lacq", 0641002}, {"abs", 0644000}, {"divs", 0644323},

        {"clq", 0650000}, {"frdiv", 0650323}, {"lmq", 0652000}, {"mul", 0653122},
        {"idiv", 0653323}, {"idivs", 0657323}, {"frdivs", 0654323}, {"muls", 0657122},

        {"norms", 0660444}, {"gsm", 0664000}, {"lrss", 0660500}, {"llss", 0660600},
        {"alss", 0660700},

        {"opr", 0740000}, {"nop", 0740000}, {"cma", 0740001}, {"cml", 0740002},
        {"oas", 0740004}, {"ral", 0740010}, {"rar", 0740020}, {"hlt", 0740040},
        {"xx", 0740040}, {"sma", 0740100}, {"sza", 0740200}, {"snl", 0740400},

        {"skp", 0741000}, {"spa", 0741100}, {"sna", 0741200}, {"szl", 0741400},

        {"rtl", 0742010}, {"rtr", 0742020},

        {"cll", 0744000}, {"stl", 0744002}, {"rcl", 0744010}, {"rcr", 0744020},

        {"cla", 0750000}, {"clc", 0750001}, {"las", 0750004}, {"glk", 0750010},

        {"law", 0760000},
    };
    return table;
}

// --- Parsing, via the one shared grammar/PccAsm.g4 -----------------------
//
// ../../../../grammar/PccAsm.g4 is generic across backends (see its own
// header comment): a statement is "mnemonic, operand (',' operand)*"
// with no built-in notion of what the mnemonic or any operand *means*.
// pdp7 has no real "instruction vs operand" distinction at all - an
// opcode is just a symbol whose value happens to be a bit pattern, so
// this backend's own semantic layer (buildSegment below) flattens a
// parsed InstrStmtContext's mnemonic token plus every operand's own
// syllables back into one combined expression, exactly reproducing
// real pdp7as semantics (`jmp i F3` OR-combines three syllables: jmp,
// i, F3) - see Pdp7Assembler.h's header comment for the fuller story.

[[noreturn]] void fail(const std::string &msg) { throw std::runtime_error("Pdp7Assembler: " + msg); }

enum class SylKind { Number, Symbol };
struct Syllable {
    char sign = 0;  // '+', '-', or 0 meaning "no sign" (default: OR)
    SylKind kind;
    std::string text;
};

struct Segment {
    std::vector<std::string> labels;
    enum Kind { None, Assign, Expr } kind = None;
    std::string lhs;                  // valid when kind == Assign
    std::vector<Syllable> syllables;  // rhs of an assignment, or a bare expression
};

struct Line {
    std::string sourceText;  // the raw physical line, for the a7out listing
    std::vector<Segment> segments;
};

std::vector<Syllable> exprToSyllables(PccAsmParser::ExprContext *ctx) {
    std::vector<Syllable> result;
    for (auto *syl : ctx->syllable()) {
        char sign = 0;
        if (auto *s = syl->sign()) sign = s->PLUS() ? '+' : '-';
        auto *atom = syl->atom();
        if (atom->expr() != nullptr) fail("'(...)' grouping isn't part of pdp7's own dialect");
        if (atom->NUMBER()) {
            result.push_back({sign, SylKind::Number, atom->NUMBER()->getText()});
        } else {
            result.push_back({sign, SylKind::Symbol, atom->IDENT()->getText()});
        }
    }
    if (result.empty()) fail("empty expression");
    return result;
}

void appendOperandSyllables(PccAsmParser::OperandContext *ctx, std::vector<Syllable> &out) {
    if (ctx->LBRACKET()) fail("'[...]' addressing isn't part of pdp7's own dialect");
    auto more = exprToSyllables(ctx->expr());
    out.insert(out.end(), more.begin(), more.end());
}

Segment buildSegment(PccAsmParser::LineContext *ctx) {
    Segment seg;
    for (auto *lbl : ctx->label()) seg.labels.push_back(lbl->IDENT()->getText());

    auto *stmt = ctx->statement();
    if (stmt == nullptr) {
        seg.kind = Segment::None;
        return seg;
    }
    if (auto *assign = dynamic_cast<PccAsmParser::AssignStmtContext *>(stmt)) {
        seg.kind = Segment::Assign;
        seg.lhs = assign->lhs->getText();
        seg.syllables = exprToSyllables(assign->rhs);
        return seg;
    }
    if (dynamic_cast<PccAsmParser::TimesStmtContext *>(stmt)) {
        fail("'times' isn't part of pdp7's own dialect - that's purely an 8086/NASM idiom");
    }
    auto *instr = dynamic_cast<PccAsmParser::InstrStmtContext *>(stmt);
    if (!instr) fail("internal: unhandled statement alternative");

    seg.kind = Segment::Expr;
    SylKind mnemonicKind = (instr->mnemonic->getType() == PccAsmParser::NUMBER) ? SylKind::Number : SylKind::Symbol;
    seg.syllables.push_back({0, mnemonicKind, instr->mnemonic->getText()});
    for (auto *op : instr->operand()) appendOperandSyllables(op, seg.syllables);
    return seg;
}

// Throws immediately on the first syntax error, same "fail loudly"
// posture as the rest of this file (and the rest of pcc's own
// frontend) - the default ANTLR error listener only prints to stderr
// and keeps going, which isn't what we want for text this backend
// generated itself.
class ThrowingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t col, const std::string &msg,
                      std::exception_ptr) override {
        fail(std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }
};

// Dynamic_cast dispatch on the labeled-alternative Context subclasses,
// the same convention ../../BuildAst.cpp already uses for the main C
// grammar, rather than the generated Visitor interface.
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
    for (auto *lineCtx : prog->line()) {
        Line line;
        antlr4::Token *start = lineCtx->getStart();
        antlr4::Token *stop = lineCtx->NEWLINE()->getSymbol();
        line.sourceText = text.substr(start->getStartIndex(), stop->getStartIndex() - start->getStartIndex());

        Segment seg = buildSegment(lineCtx);
        if (seg.kind != Segment::None || !seg.labels.empty()) line.segments.push_back(std::move(seg));
        lines.push_back(std::move(line));
    }
    return lines;
}

// --- Two-pass assembly ----------------------------------------------------

// One combine-into-mem_[wordOffset] fixup a future linker still needs
// to apply, for a symbol this file's own labels don't define - see
// Pdp7Assembler.h's own comment on `allowExternal`. `op` is the
// syllable's own combine operator ('+', '-', or '|' for the default
// OR) - the external symbol's eventual resolved value gets combined
// into whatever this word already holds (computed treating the
// external syllable as contributing 0) using that same operator.
struct Relocation {
    uint32_t wordOffset;
    std::string symbolName;
    char op;
};

class Assembler {
   public:
    explicit Assembler(bool allowExternal) : allowExternal_(allowExternal) {
        symbols_ = builtins();
        symbols_["."] = BASE;
        // An object file's own text starts at word 0, not the
        // whole-program convention's 4096 - it's the linker's job to
        // place it, not this assembler's (see Pdp7Assembler.h).
        symbols_[".."] = allowExternal_ ? 0 : kPdp7BaseAddress;
    }

    void run(const std::vector<Line> &lines) {
        stage_ = 1;
        for (const auto &line : lines) processLine(line);
        symbols_["."] = BASE;
        stage_ = 2;
        for (const auto &line : lines) processLine(line);
    }

    const std::vector<std::optional<uint32_t>> &mem() const { return mem_; }
    const std::vector<std::string> &mline() const { return mline_; }
    const std::unordered_map<std::string, uint32_t> &labels() const { return labels_; }
    const std::vector<Relocation> &relocations() const { return relocations_; }
    uint32_t base() const { return symbols_.at(".."); }

   private:
    void processLine(const Line &line) {
        bool lineConsumed = false;
        for (const auto &seg : line.segments) {
            for (const auto &lbl : seg.labels) setLabel(lbl, symbols_["."]);
            if (seg.kind == Segment::Assign) {
                symbols_[seg.lhs] = evalExpr(seg.syllables, nullptr);
            } else if (seg.kind == Segment::Expr) {
                std::vector<std::pair<char, std::string>> pending;
                uint32_t word = evalExpr(seg.syllables, &pending);
                if (stage_ == 2) {
                    // A RELATIVE-flagged word embeds an address that
                    // was resolved relative to this object's own `..`
                    // (0 in object mode, see the constructor) - once
                    // the linker places this object at its real,
                    // final base, that embedded address needs exactly
                    // the same "+base" shift storeWord() itself would
                    // apply here if `..` were already final. In
                    // object mode it isn't, so record that shift as a
                    // relocation against the reserved pseudo-symbol
                    // "..", which Linker.cpp resolves to this specific
                    // object's own assigned base - never looked up in
                    // any other object's symbol table - instead of
                    // silently losing it the way a plain external
                    // reference's `pending` entries already don't.
                    // This applies to every address-valued word, not
                    // only ones that also have a genuine external
                    // reference (`pending` may be empty here and this
                    // still fires) - pdp7 has no PC-relative
                    // addressing mode at all, unlike 8086's rel16/rel8,
                    // so there's no self-correcting case to exclude.
                    bool needsBaseShift = allowExternal_ && (word & RELATIVE);
                    uint32_t idx = storeWord(line.sourceText, lineConsumed, word);
                    if (needsBaseShift) relocations_.push_back({idx, "..", '+'});
                    for (const auto &[op, name] : pending) relocations_.push_back({idx, name, op});
                }
                symbols_["."] = symbols_["."] + 1;
            }
        }
    }

    // No local-vs-global label scoping (unlike pdp-assembler) -
    // every label Pdp7Codegen emits is already globally unique by
    // construction, so a flat map is enough; see Pdp7Assembler.h.
    void setLabel(const std::string &name, uint32_t loc) {
        symbols_[name] = loc;
        labels_[name] = loc;
    }

    // `pending`, if given, collects (op, symbolName) for any syllable
    // that turned out to be an external reference (allowExternal_
    // only - see resolveSyllable) instead of failing; its caller
    // (processLine) is what actually knows the final word offset to
    // record the relocation against, since this function only ever
    // sees one expression's own syllables, not where it lands.
    uint32_t evalExpr(const std::vector<Syllable> &syls, std::vector<std::pair<char, std::string>> *pending) {
        uint32_t word = 0, flags = 0;
        for (const auto &syl : syls) {
            char op = syl.sign ? syl.sign : '|';
            bool wasExternal = false;
            uint32_t value = resolveSyllable(syl, &wasExternal);
            if (wasExternal && pending) pending->push_back({op, syl.text});
            uint32_t vFlags = value & RELATIVE;
            value &= MASK18;
            if (op == '+') {
                word += value;
                flags |= vFlags;
            } else if (op == '-') {
                word -= value;
                if (flags & RELATIVE) {
                    if (vFlags & RELATIVE) flags &= ~RELATIVE;
                } else if (vFlags & RELATIVE) {
                    fail("absolute value minus relative");
                }
            } else {
                word |= value;
                flags |= vFlags;
            }
            word &= MASK18;
        }
        word |= flags;
        return word;
    }

    // `wasExternal`, if given, is set true when `syl` named a symbol
    // not defined anywhere in this file and `allowExternal_` let that
    // through rather than failing - the caller (evalExpr) is then
    // responsible for recording a relocation, since this function
    // doesn't know the combine-op or the eventual word offset.
    uint32_t resolveSyllable(const Syllable &syl, bool *wasExternal = nullptr) {
        if (syl.kind == SylKind::Number) {
            const std::string &t = syl.text;
            uint32_t v = 0;
            if (!t.empty() && t[0] == '0') {
                for (char c : t) v = v * 8 + static_cast<uint32_t>(c - '0');
            } else {
                v = static_cast<uint32_t>(std::stoul(t));
            }
            return v & MASK18;
        }
        auto it = symbols_.find(syl.text);
        if (it != symbols_.end()) return it->second;
        if (stage_ == 2) {
            if (allowExternal_) {
                if (wasExternal) *wasExternal = true;
                return 0;
            }
            fail("undefined symbol '" + syl.text + "'");
        }
        return 0;
    }

    uint32_t storeWord(const std::string &sourceText, bool &lineConsumed, uint32_t word) {
        long location = static_cast<long>(symbols_["."]);
        if (location & RELATIVE) {
            location &= MASK18;
            location += static_cast<long>(symbols_[".."]) & MASK18;
        }
        if (word & RELATIVE) {
            word &= MASK18;
            word += symbols_[".."] & MASK18;
        }
        if (location < 0) fail("location below base");

        size_t idx = static_cast<size_t>(location);
        if (mem_.size() <= idx) {
            mem_.resize(idx + 1);
            mline_.resize(idx + 1);
        }
        mem_[idx] = word;
        mline_[idx] = lineConsumed ? "" : sourceText;
        lineConsumed = true;
        return static_cast<uint32_t>(idx);
    }

    std::unordered_map<std::string, uint32_t> symbols_;
    std::unordered_map<std::string, uint32_t> labels_;  // for List's listing only - see setLabel
    std::vector<std::optional<uint32_t>> mem_;
    std::vector<std::string> mline_;
    std::vector<Relocation> relocations_;
    bool allowExternal_;
    int stage_ = 1;
};

// --- Output formats - same byte-packing as ----------------------------
// ../../../../pdp-assembler/src/Assembler.cpp::writeOutput, verified
// correct by that project; reimplemented here rather than reused.

std::string writeA7Out(const Assembler &a) {
    std::ostringstream out;
    char buf[64];
    const auto &mem = a.mem();
    const auto &mline = a.mline();
    for (size_t i = 0; i < mem.size(); ++i) {
        if (!mem[i]) continue;
        std::snprintf(buf, sizeof(buf), "%06o: %06o\t", static_cast<unsigned>(i), *mem[i]);
        out << buf << mline[i] << "\n";
    }
    return out.str();
}

std::string writeList(const Assembler &a) {
    std::ostringstream out;
    out << "\nLabels:\n";
    std::vector<std::string> names;
    for (const auto &kv : a.labels()) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    char buf[64];
    for (const auto &name : names) {
        uint32_t addr = a.labels().at(name);
        const char *flags = (addr & RELATIVE) ? "r" : "";
        if (addr & RELATIVE) {
            addr &= MASK18;
            addr += a.base();
        }
        std::snprintf(buf, sizeof(buf), "%-8.8s %#06o %s", name.c_str(), addr & MASK18, flags);
        out << buf << "\n";
    }
    return out.str();
}

// `Ptr`/`Rim`: 3 bytes per 18-bit word (6 bits each, high bit set),
// only ever covering memory from `..` (the relocation base) upward -
// an address placed below that is silently absent from the real
// binary even though it shows up fine in an `a7out` listing. This
// exact behavior must be preserved: it's load-bearing for the
// placement feature (`. = ..+N`), and a previous version of that
// feature that didn't respect it hung the real simulator outright
// (see docs/ir.md's "Fixed absolute placement").
std::string writePtrOrRim(const Assembler &a, bool rim) {
    std::ostringstream out;
    uint32_t base = a.base();
    const auto &mem = a.mem();
    for (size_t loc = base; loc < mem.size(); ++loc) {
        uint32_t w = mem[loc] ? *mem[loc] : 0;
        out.put(static_cast<char>(((w >> 12) & 077) | 0200));
        out.put(static_cast<char>(((w >> 6) & 077) | 0200));
        out.put(static_cast<char>((w & 077) | 0200));
    }
    if (rim) {
        uint32_t finalWord = (0600000 | base) & MASK18;
        out.put(static_cast<char>(((finalWord >> 12) & 077) | 0200));
        out.put(static_cast<char>(((finalWord >> 6) & 077) | 0200));
        out.put(static_cast<char>((finalWord & 077) | 0200 | 0100));
    }
    return out.str();
}

// COFF (see ../../coff/CoffWriter.h): same "only memory from `..`
// upward" coverage as Ptr/Rim, packed 3 bytes per 18-bit word (the
// top 6 bits of the 24-bit field are always zero, since no word ever
// carries more than 18 significant bits) rather than Ptr/Rim's paper-
// tape-specific 6-bits-per-byte/high-bit-set scheme - this is a plain
// byte-oriented section, not a tape image. `kPdp7CoffMachine` (see
// Pdp7Assembler.h) is a custom, non-registered COFF machine ID
// (PDP-7 predates COFF, so there's no real one to use) chosen only
// to be unique and memorable.

// Relocation `type` values - see Pdp7Assembler.h's own
// kPdp7RelocOr/Add/Sub comment (exported from there, not redeclared
// here, so Pdp7Linker.cpp uses the exact same values).

// Builds the format-neutral ObjectFile a completed object-mode
// Assembler produced - shared by `writeCoff()` (serializes it
// straight to COFF bytes, for `assemblePdp7`'s own `Coff` case) and
// `assemblePdp7Object()` (returns it directly - see Pdp7Assembler.h's
// own comment on why). Every word is packed into 3 raw `.textBytes`
// bytes, big-endian (high 2 bits of the top byte always 0, since an
// 18-bit word never needs more than 3 bytes) - see
// GenericLinker.h's own `GenericLinkerConfig::addressUnitBytes`
// comment for how ../../linker/factory/Pdp7LinkerFactory.cpp's own
// `makePdp7Linker()` tells GenericLinker to treat every 3 of these
// packed bytes as one addressable/relocatable pdp7 word.
ObjectFile buildObjectFile(const Assembler &a) {
    uint32_t base = a.base();
    const auto &mem = a.mem();
    ObjectFile obj;
    obj.machineType = kPdp7CoffMachine;
    for (size_t loc = base; loc < mem.size(); ++loc) {
        uint32_t w = (mem[loc] ? *mem[loc] : 0) & MASK18;
        obj.textBytes.push_back(static_cast<uint8_t>((w >> 16) & 0xFF));
        obj.textBytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        obj.textBytes.push_back(static_cast<uint8_t>(w & 0xFF));
    }

    for (const auto &kv : a.labels()) {
        uint32_t addr = kv.second;
        if (addr & RELATIVE) addr = (addr & MASK18) + base;
        if (addr < base) continue;  // not part of the emitted .text section
        obj.symbols.push_back({kv.first, (addr - base) * 3, /*isDefined=*/true});
    }
    std::sort(obj.symbols.begin(), obj.symbols.end(),
              [](const ObjSymbol &x, const ObjSymbol &y) { return x.name < y.name; });

    // Every relocation's target symbol needs a symbol-table entry too
    // (the writer resolves relocations to symbols by index) - add one,
    // undefined, for any name that isn't already a real local label.
    std::unordered_map<std::string, bool> known;
    for (const auto &s : obj.symbols) known[s.name] = true;
    for (const auto &r : a.relocations()) {
        if (!known.count(r.symbolName)) {
            obj.symbols.push_back({r.symbolName, 0, /*isDefined=*/false});
            known[r.symbolName] = true;
        }
        uint16_t type = r.op == '+' ? kPdp7RelocAdd : (r.op == '-' ? kPdp7RelocSub : kPdp7RelocOr);
        obj.relocations.push_back({r.wordOffset * 3, r.symbolName, type});
    }
    return obj;
}

std::string writeCoff(const Assembler &a) {
    ObjectFile obj = buildObjectFile(a);
    return writeCoffObject(obj.machineType, obj.textBytes, obj.symbols, obj.relocations, obj.dataSize);
}

}  // namespace

std::string assemblePdp7(const std::string &asmText, Pdp7OutputFormat format, bool allowExternal) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(allowExternal);
    assembler.run(lines);

    switch (format) {
        case Pdp7OutputFormat::A7Out:
            return writeA7Out(assembler);
        case Pdp7OutputFormat::List:
            return writeList(assembler);
        case Pdp7OutputFormat::Ptr:
            return writePtrOrRim(assembler, /*rim=*/false);
        case Pdp7OutputFormat::Rim:
            return writePtrOrRim(assembler, /*rim=*/true);
        case Pdp7OutputFormat::Coff:
            return writeCoff(assembler);
    }
    fail("unknown output format");
}

ObjectFile assemblePdp7Object(const std::string &asmText) {
    std::vector<Line> lines = parseProgram(asmText);
    Assembler assembler(/*allowExternal=*/true);
    assembler.run(lines);
    return buildObjectFile(assembler);
}

}  // namespace pcc
