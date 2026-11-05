#include "Preprocessor.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

// A small, from-scratch text-level preprocessor - not a full ISO C
// preprocessor, sized for what this project actually needs: target-
// conditional kernel code (`#ifdef __PDP7__`), and the handful of
// directives real C code commonly leans on. Runs line-by-line over
// the raw source text, entirely before the ANTLR lexer/parser ever
// see it - `#include` needs real file I/O (open a path, splice its
// contents in), which doesn't fit naturally into a grammar rule the
// way `asm { ... }` (an opaque, unparsed span) does.
//
// Supported: `#include "file"` / `#include <file>`, `#define NAME`
// / `#define NAME value` (object-like only - no function-like
// macros with parameters, no token-pasting/stringizing), `#undef`,
// `#ifdef`/`#ifndef`/`#if`/`#elif`/`#else`/`#endif` (`#if`/`#elif`
// evaluate a small integer-constant-expression language: `defined`,
// integer literals, `! - + * / % == != < <= > >= && ||`, parens -
// real C precedence, undefined identifiers evaluate to 0 same as
// real cpp). Also predefines `__LINE__`/`__FILE__`/`__DATE__`/
// `__TIME__` (see below for `__FILE__`'s own caveat) alongside
// whatever `preprocess()`'s caller seeds (`main.cpp`'s target
// macros).
//
// Deliberate limitations, not bugs:
// - Macro expansion rescans (so `#define BAR FOO + 5` then using
//   `BAR` expands `FOO` too, not just one flat substitution) but caps
//   at 32 rescans per line rather than real cpp's per-expansion
//   "blue paint" cycle tracking - a genuinely self-referential chain
//   (`#define X X`) stops after 32 rounds instead of looping forever,
//   which is enough for any non-pathological use without the extra
//   bookkeeping real recursion-proof expansion needs.
// - `__FILE__`/`__DATE__`/`__TIME__` substitute as a double-quoted
//   string, matching real C's own `__FILE__` shape - a real string
//   literal (see grammar/CLexer.g4's own STRINGLIT), so each is valid
//   anywhere a string literal is, same as writing one directly
//   (`int p; p = __FILE__;`). `__LINE__` substitutes as a plain
//   integer literal, valid wherever a `NUMBER` is.
// - Each `#include`d file's own `#if`/`#ifdef`/.../`#endif` nesting
//   must balance *within that file* - an `#ifdef` can't open in one
//   file and `#endif` in another. True of virtually all real-world
//   C anyway.
// - Line numbers after a `#include` become approximate for anything
//   downstream (ANTLR's own parse-error reporting counts newlines in
//   the final flattened text) - `__LINE__`/`__FILE__` themselves stay
//   exactly right regardless, since they're substituted using this
//   preprocessor's own per-physical-line tracking, not re-derived
//   from the flattened output later.

namespace pcc {

namespace {

std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

// Strips a trailing `// ...` or `/* ... */` comment from one already-
// single-line piece of text (a `#define`'s own captured value, below)
// - real cpp strips comments in its own Phase 3, strictly before
// Phase 4's directive processing, so a macro's value never includes
// one; this preprocessor runs both in one pass, so it has to do this
// itself at the one point a macro's value gets captured as its own
// freestanding string, or a same-line trailing comment becomes part
// of the replacement text - silently corrupting whatever line the
// macro is later used on (that line's own rest, past the pasted-in
// `//`, reads as "still inside a comment" once ANTLR's lexer sees
// it - confirmed directly: this previously sent `pcc` into a long,
// seemingly-infinite stall on `#define NAME value // note`). A
// string literal's own contents (e.g. `#define S "a//b"`) are
// skipped over rather than scanned for `//`/`/*` - this language's
// own STRINGLIT escape set (`\\` `\"` `\n` `\t` `\r` `\0` - see
// grammar/CLexer.g4's own STRINGLIT_ESC) is enough to track whether a
// given double-quote is the closing one or an escaped one inside the
// string, so a `//`/`/*` genuinely inside a string literal's own text
// is left alone.
std::string stripTrailingComment(const std::string &s) {
    bool inString = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (inString) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                i++;  // skip the escaped character - never ends the string early
            } else if (s[i] == '"') {
                inString = false;
            }
            continue;
        }
        if (s[i] == '"') {
            inString = true;
            continue;
        }
        if (s[i] == '/' && i + 1 < s.size() && (s[i + 1] == '/' || s[i + 1] == '*')) {
            return trim(s.substr(0, i));
        }
    }
    return s;
}

std::string dirOf(const std::string &path) {
    size_t slash = path.find_last_of('/');
    return (slash == std::string::npos) ? "." : path.substr(0, slash);
}

std::string readFileRaw(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("pcc preprocessor: cannot open '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

bool isBuiltinDynamicMacro(const std::string &name) {
    return name == "__LINE__" || name == "__FILE__" || name == "__DATE__" || name == "__TIME__";
}

// One substitution pass: every macro/builtin-dynamic-macro
// occurrence gets replaced, left to right. `*changed` is set if
// anything was substituted, so the caller (expandMacros, below) can
// rescan the result - a macro's replacement text can itself
// reference other macros (`#define BAR FOO + 5`), same as real cpp.
std::string substituteOnePass(const std::string &line, const std::map<std::string, std::string> &macros,
                               const std::string &path, int lineNo, const std::string &dateStr,
                               const std::string &timeStr, bool *changed) {
    std::string out;
    size_t i = 0;
    while (i < line.size()) {
        if (isIdentStart(line[i])) {
            size_t start = i;
            while (i < line.size() && isIdentChar(line[i])) i++;
            std::string word = line.substr(start, i - start);
            if (word == "__LINE__") {
                out += std::to_string(lineNo);
                *changed = true;
            } else if (word == "__FILE__") {
                out += "\"" + path + "\"";
                *changed = true;
            } else if (word == "__DATE__") {
                out += "\"" + dateStr + "\"";
                *changed = true;
            } else if (word == "__TIME__") {
                out += "\"" + timeStr + "\"";
                *changed = true;
            } else {
                auto it = macros.find(word);
                if (it != macros.end()) {
                    out += it->second;
                    *changed = true;
                } else {
                    out += word;
                }
            }
        } else {
            out += line[i];
            i++;
        }
    }
    return out;
}

// Repeatedly applies substituteOnePass so a macro's own replacement
// text gets expanded too (`#define BAR FOO + 5` then using `BAR`
// expands to `FOO`'s own value, not the literal text "FOO + 5") -
// real recursive macro expansion, not just one flat substitution.
// Capped (not real cpp's own per-expansion "blue paint" tracking) -
// a self-referential chain longer than this is a real but deliberate
// limitation, not a bug: `#define X X` would otherwise loop forever.
std::string expandMacros(const std::string &line, const std::map<std::string, std::string> &macros,
                          const std::string &path, int lineNo, const std::string &dateStr,
                          const std::string &timeStr) {
    std::string text = line;
    for (int iter = 0; iter < 32; iter++) {
        bool changed = false;
        text = substituteOnePass(text, macros, path, lineNo, dateStr, timeStr, &changed);
        if (!changed) break;
    }
    return text;
}

// Resolves every `defined(NAME)` / `defined NAME` in a `#if`/`#elif`
// expression to a literal "1"/"0" - has to run *before* ordinary
// macro substitution, same as real cpp, so `defined(FOO)` tests
// whether FOO is a macro rather than substituting FOO's value first.
std::string resolveDefined(const std::string &expr, const std::map<std::string, std::string> &macros) {
    std::string out;
    size_t i = 0;
    while (i < expr.size()) {
        if (isIdentStart(expr[i])) {
            size_t start = i;
            while (i < expr.size() && isIdentChar(expr[i])) i++;
            std::string word = expr.substr(start, i - start);
            if (word == "defined") {
                size_t j = i;
                while (j < expr.size() && std::isspace(static_cast<unsigned char>(expr[j]))) j++;
                bool paren = j < expr.size() && expr[j] == '(';
                if (paren) {
                    j++;
                    while (j < expr.size() && std::isspace(static_cast<unsigned char>(expr[j]))) j++;
                }
                size_t nameStart = j;
                while (j < expr.size() && isIdentChar(expr[j])) j++;
                std::string name = expr.substr(nameStart, j - nameStart);
                if (paren) {
                    while (j < expr.size() && std::isspace(static_cast<unsigned char>(expr[j]))) j++;
                    if (j < expr.size() && expr[j] == ')') j++;
                }
                bool isDefined = macros.count(name) > 0 || isBuiltinDynamicMacro(name);
                out += isDefined ? "1" : "0";
                i = j;
                continue;
            }
            out += word;
        } else {
            out += expr[i];
            i++;
        }
    }
    return out;
}

// Tiny recursive-descent evaluator for the already-macro-substituted
// `#if`/`#elif` expression text - real C operator precedence
// (lowest to highest: || && == != < <= > >= + - * / % unary ! - +),
// parens, integer literals, and any leftover identifier (an
// undefined one, since every macro name was already substituted
// away) evaluating to 0, same as real cpp.
class ExprEval {
   public:
    explicit ExprEval(const std::string &s) : s_(s) {}

    long parse() {
        long v = parseOr();
        return v;
    }

   private:
    std::string s_;
    size_t i_ = 0;

    void skipWs() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) i_++;
    }

    bool consume(const char *tok) {
        skipWs();
        size_t n = std::strlen(tok);
        if (s_.compare(i_, n, tok) == 0) {
            i_ += n;
            return true;
        }
        return false;
    }

    long parseOr() {
        long v = parseAnd();
        while (consume("||")) v = (v || parseAnd()) ? 1 : 0;
        return v;
    }
    long parseAnd() {
        long v = parseEq();
        while (consume("&&")) v = (v && parseEq()) ? 1 : 0;
        return v;
    }
    long parseEq() {
        long v = parseRel();
        for (;;) {
            if (consume("==")) {
                v = (v == parseRel()) ? 1 : 0;
            } else if (consume("!=")) {
                v = (v != parseRel()) ? 1 : 0;
            } else {
                return v;
            }
        }
    }
    long parseRel() {
        long v = parseAdd();
        for (;;) {
            if (consume("<=")) {
                v = (v <= parseAdd()) ? 1 : 0;
            } else if (consume(">=")) {
                v = (v >= parseAdd()) ? 1 : 0;
            } else if (consume("<")) {
                v = (v < parseAdd()) ? 1 : 0;
            } else if (consume(">")) {
                v = (v > parseAdd()) ? 1 : 0;
            } else {
                return v;
            }
        }
    }
    long parseAdd() {
        long v = parseMul();
        for (;;) {
            if (consume("+")) {
                v += parseMul();
            } else if (consume("-")) {
                v -= parseMul();
            } else {
                return v;
            }
        }
    }
    long parseMul() {
        long v = parseUnary();
        for (;;) {
            if (consume("*")) {
                v *= parseUnary();
            } else if (consume("/")) {
                long d = parseUnary();
                v = d ? v / d : 0;
            } else if (consume("%")) {
                long d = parseUnary();
                v = d ? v % d : 0;
            } else {
                return v;
            }
        }
    }
    long parseUnary() {
        if (consume("!")) return parseUnary() == 0 ? 1 : 0;
        if (consume("-")) return -parseUnary();
        if (consume("+")) return parseUnary();
        return parsePrimary();
    }
    long parsePrimary() {
        skipWs();
        if (consume("(")) {
            long v = parseOr();
            consume(")");
            return v;
        }
        size_t start = i_;
        if (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) {
            while (i_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[i_]))) i_++;
            return std::stol(s_.substr(start, i_ - start));
        }
        if (i_ < s_.size() && isIdentStart(s_[i_])) {
            while (i_ < s_.size() && isIdentChar(s_[i_])) i_++;
            return 0;  // leftover identifier: not a macro (already substituted) -> 0, same as real cpp
        }
        return 0;  // empty/garbage operand
    }
};

bool evalCondition(const std::string &rawExpr, const std::map<std::string, std::string> &macros,
                    const std::string &path, int lineNo, const std::string &dateStr, const std::string &timeStr) {
    std::string resolved = resolveDefined(rawExpr, macros);
    std::string substituted = expandMacros(resolved, macros, path, lineNo, dateStr, timeStr);
    return ExprEval(substituted).parse() != 0;
}

std::string currentDateStr() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%b %e %Y", std::localtime(&t));
    return buf;
}

std::string currentTimeStr() {
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    return buf;
}

// One #if/#ifdef/#ifndef nesting level. `outerActive` is the
// enclosing scope's own active-ness, fixed at push time (nesting
// guarantees it can't change while this frame is open); `everTaken`
// tracks whether any branch in this if/elif/.../else chain has
// matched yet, for #elif/#else; `branchActive` is whether *this*
// branch is currently emitting.
struct CondFrame {
    bool outerActive;
    bool everTaken;
    bool branchActive;
};

std::string preprocessFile(const std::string &path, std::map<std::string, std::string> &macros,
                            std::vector<std::string> &includeStack, const std::string &dateStr,
                            const std::string &timeStr) {
    if (std::find(includeStack.begin(), includeStack.end(), path) != includeStack.end()) {
        throw std::runtime_error("pcc preprocessor: circular #include involving '" + path + "'");
    }
    if (includeStack.size() > 50) {
        throw std::runtime_error("pcc preprocessor: #include nested too deeply (over 50 levels, likely a cycle)");
    }
    includeStack.push_back(path);

    std::istringstream in(readFileRaw(path));
    std::string line;
    std::ostringstream out;
    std::vector<CondFrame> condStack;
    int lineNo = 0;

    auto isActive = [&]() { return condStack.empty() || condStack.back().branchActive; };
    auto where = [&](int n) { return path + ":" + std::to_string(n); };

    while (std::getline(in, line)) {
        lineNo++;
        std::string trimmed = trim(line);

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::string rest = trim(trimmed.substr(1));
            size_t sp = rest.find_first_of(" \t");
            std::string kw = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            // Stripped here, once, for every directive - matching real
            // cpp's own Phase 3 (comments gone) strictly before Phase 4
            // (directive processing) - see stripTrailingComment's own
            // comment for why `#define`'s own argument specifically
            // can't skip this.
            std::string arg = (sp == std::string::npos) ? "" : stripTrailingComment(trim(rest.substr(sp + 1)));

            if (kw == "include") {
                if (isActive()) {
                    if (arg.size() < 2) throw std::runtime_error("pcc preprocessor: malformed #include at " + where(lineNo));
                    char open = arg.front(), close = arg.back();
                    bool quoted = open == '"' && close == '"';
                    bool angled = open == '<' && close == '>';
                    if (!quoted && !angled) throw std::runtime_error("pcc preprocessor: malformed #include at " + where(lineNo));
                    std::string incName = arg.substr(1, arg.size() - 2);
                    std::string incPath = dirOf(path) + "/" + incName;
                    {
                        std::ifstream probe(incPath);
                        if (!probe) incPath = incName;  // fall back to as-given (cwd-relative)
                    }
                    out << preprocessFile(incPath, macros, includeStack, dateStr, timeStr);
                }
                out << "\n";
                continue;
            }
            if (kw == "define") {
                if (isActive()) {
                    size_t sp2 = arg.find_first_of(" \t");
                    std::string name = (sp2 == std::string::npos) ? arg : arg.substr(0, sp2);
                    std::string value = (sp2 == std::string::npos) ? "" : trim(arg.substr(sp2 + 1));
                    if (name.empty()) throw std::runtime_error("pcc preprocessor: #define with no name at " + where(lineNo));
                    macros[name] = value;
                }
                out << "\n";
                continue;
            }
            if (kw == "undef") {
                if (isActive()) macros.erase(arg);
                out << "\n";
                continue;
            }
            if (kw == "ifdef" || kw == "ifndef") {
                bool outer = isActive();
                bool defined = outer && (macros.count(arg) > 0 || isBuiltinDynamicMacro(arg));
                bool cond = outer && (kw == "ifdef" ? defined : !defined);
                condStack.push_back(CondFrame{outer, cond, cond});
                out << "\n";
                continue;
            }
            if (kw == "if") {
                bool outer = isActive();
                bool cond = outer && evalCondition(arg, macros, path, lineNo, dateStr, timeStr);
                condStack.push_back(CondFrame{outer, cond, cond});
                out << "\n";
                continue;
            }
            if (kw == "elif") {
                if (condStack.empty()) throw std::runtime_error("pcc preprocessor: #elif without #if at " + where(lineNo));
                CondFrame &top = condStack.back();
                bool cond = top.outerActive && !top.everTaken && evalCondition(arg, macros, path, lineNo, dateStr, timeStr);
                top.branchActive = cond;
                if (cond) top.everTaken = true;
                out << "\n";
                continue;
            }
            if (kw == "else") {
                if (condStack.empty()) throw std::runtime_error("pcc preprocessor: #else without #if at " + where(lineNo));
                CondFrame &top = condStack.back();
                top.branchActive = top.outerActive && !top.everTaken;
                if (top.branchActive) top.everTaken = true;
                out << "\n";
                continue;
            }
            if (kw == "endif") {
                if (condStack.empty()) throw std::runtime_error("pcc preprocessor: #endif without #if at " + where(lineNo));
                condStack.pop_back();
                out << "\n";
                continue;
            }
            throw std::runtime_error("pcc preprocessor: unknown directive '#" + kw + "' at " + where(lineNo));
        }

        if (!isActive()) {
            out << "\n";
            continue;
        }
        out << expandMacros(line, macros, path, lineNo, dateStr, timeStr) << "\n";
    }

    if (!condStack.empty()) {
        throw std::runtime_error("pcc preprocessor: unterminated #if/#ifdef/#ifndef in '" + path + "'");
    }

    includeStack.pop_back();
    return out.str();
}

}  // namespace

std::string preprocess(const std::string &path, const std::map<std::string, std::string> &predefined) {
    std::map<std::string, std::string> macros = predefined;
    std::vector<std::string> includeStack;
    return preprocessFile(path, macros, includeStack, currentDateStr(), currentTimeStr());
}

}  // namespace pcc
