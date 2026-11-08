#include "Preprocessor.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

// A small, from-scratch text-level preprocessor, run line-by-line over the
// raw source before ANTLR sees it.
//
// Supported: #include "file"/<file>, object-like and function-like #define
// (including variadic macros, stringizing, and token pasting), #undef,
// #ifdef/#ifndef/#if/#elif/#else/#endif, and the dynamic predefined macros.
//
// Deliberate limitations:
// - Recursive macro expansion is suppressed while that macro is being
//   expanded, which terminates self-reference without full cpp blue-paint
//   token tracking.
// - __FILE__/__DATE__/__TIME__ substitute as string literals, __LINE__ as
//   an integer literal.
// - #if/#ifdef/.../#endif nesting must balance within a single #include'd
//   file.
// - Line numbers after a #include are approximate downstream (ANTLR counts
//   newlines in the flattened text); __LINE__/__FILE__ themselves stay
//   correct since they're substituted from this preprocessor's own
//   per-physical-line tracking.

namespace pcc {

namespace {

std::string trim(const std::string &s) {
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

// Strips a trailing `//`/`/* */` comment from a directive argument so it
// doesn't get pasted into a macro's replacement text. Tracks string-literal
// quoting so `//`/`/*` inside a string (e.g. "a//b") is left alone.
std::string stripTrailingComment(const std::string &s) {
    bool inString = false;
    for (size_t i = 0; i < s.size(); i++) {
        if (inString) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                i++;  // skip escaped character
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

struct Macro {
    bool functionLike = false;
    bool variadic = false;
    std::vector<std::string> parameters;
    std::string replacement;
};

using MacroMap = std::map<std::string, Macro>;

bool isBuiltinDynamicMacro(const std::string &name) {
    return name == "__LINE__" || name == "__FILE__" || name == "__DATE__" || name == "__TIME__";
}

std::string stripComments(const std::string &line, bool &inBlockComment) {
    std::string out;
    bool inString = false;
    bool inChar = false;
    for (size_t i = 0; i < line.size(); i++) {
        if (inBlockComment) {
            if (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '/') {
                inBlockComment = false;
                out += "  ";
                i++;
            } else {
                out += ' ';
            }
        } else if (inString || inChar) {
            char quote = inString ? '"' : '\'';
            out += line[i];
            if (line[i] == '\\' && i + 1 < line.size()) {
                out += line[++i];
            } else if (line[i] == quote) {
                inString = false;
                inChar = false;
            }
        } else if (line[i] == '"' || line[i] == '\'') {
            inString = line[i] == '"';
            inChar = line[i] == '\'';
            out += line[i];
        } else if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') {
            out.append(line.size() - i, ' ');
            break;
        } else if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '*') {
            inBlockComment = true;
            out += "  ";
            i++;
        } else {
            out += line[i];
        }
    }
    return out;
}

bool parseInvocation(const std::string &text, size_t openParen, std::vector<std::string> &arguments, size_t &end) {
    if (openParen >= text.size() || text[openParen] != '(') return false;
    int depth = 1;
    bool inString = false;
    bool inChar = false;
    size_t argumentStart = openParen + 1;
    for (size_t i = openParen + 1; i < text.size(); i++) {
        char c = text[i];
        if (inString || inChar) {
            char quote = inString ? '"' : '\'';
            if (c == '\\' && i + 1 < text.size()) {
                i++;
            } else if (c == quote) {
                inString = false;
                inChar = false;
            }
        } else if (c == '"' || c == '\'') {
            inString = c == '"';
            inChar = c == '\'';
        } else if (c == '(') {
            depth++;
        } else if (c == ')') {
            depth--;
            if (depth == 0) {
                std::string last = trim(text.substr(argumentStart, i - argumentStart));
                if (!arguments.empty() || !last.empty()) arguments.push_back(last);
                end = i + 1;
                return true;
            }
        } else if (c == ',' && depth == 1) {
            arguments.push_back(trim(text.substr(argumentStart, i - argumentStart)));
            argumentStart = i + 1;
        }
    }
    return false;
}

std::string stringifyArgument(const std::string &argument) {
    std::string normalized;
    bool pendingSpace = false;
    bool inString = false;
    bool inChar = false;
    const std::string trimmed = trim(argument);
    for (size_t i = 0; i < trimmed.size(); i++) {
        char c = trimmed[i];
        if (inString || inChar) {
            char quote = inString ? '"' : '\'';
            normalized += c;
            if (c == '\\' && i + 1 < trimmed.size()) {
                normalized += trimmed[++i];
            } else if (c == quote) {
                inString = false;
                inChar = false;
            }
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(c))) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !normalized.empty()) normalized += ' ';
        pendingSpace = false;
        normalized += c;
        if (c == '"') inString = true;
        else if (c == '\'') inChar = true;
    }
    std::string escaped;
    for (char c : normalized) {
        if (c == '"' || c == '\\') escaped += '\\';
        escaped += c;
    }
    return "\"" + escaped + "\"";
}

size_t tokenEnd(const std::string &text, size_t start) {
    if (start >= text.size()) return start;
    if (isIdentStart(text[start]) || std::isdigit(static_cast<unsigned char>(text[start]))) {
        size_t i = start + 1;
        while (i < text.size() &&
               (isIdentChar(text[i]) || std::isdigit(static_cast<unsigned char>(text[i])) ||
                (text[start] == '.' && text[i] == '.'))) i++;
        return i;
    }
    if (text[start] == '"' || text[start] == '\'') {
        char quote = text[start];
        size_t i = start + 1;
        while (i < text.size()) {
            if (text[i] == '\\' && i + 1 < text.size()) i += 2;
            else if (text[i++] == quote) break;
        }
        return i;
    }
    return start + 1;
}

std::string substituteFunctionMacro(const Macro &macro, const std::vector<std::string> &arguments,
                                    const MacroMap &macros, const std::string &path, int lineNo,
                                    const std::string &dateStr, const std::string &timeStr,
                                    const std::vector<std::string> &disabled);

std::string expandMacros(const std::string &text, const MacroMap &macros, const std::string &path, int lineNo,
                         const std::string &dateStr, const std::string &timeStr,
                         const std::vector<std::string> &disabled = {});

std::string substituteFunctionMacro(const Macro &macro, const std::vector<std::string> &arguments,
                                    const MacroMap &macros, const std::string &path, int lineNo,
                                    const std::string &dateStr, const std::string &timeStr,
                                    const std::vector<std::string> &disabled) {
    size_t fixedCount = macro.parameters.size();
    if ((!macro.variadic && arguments.size() != fixedCount) ||
        (macro.variadic && arguments.size() < fixedCount)) {
        throw std::runtime_error("pcc preprocessor: wrong number of arguments to function-like macro");
    }

    std::map<std::string, std::string> rawArguments;
    std::map<std::string, std::string> expandedArguments;
    for (size_t i = 0; i < fixedCount; i++) {
        rawArguments[macro.parameters[i]] = arguments[i];
        expandedArguments[macro.parameters[i]] =
            expandMacros(arguments[i], macros, path, lineNo, dateStr, timeStr, disabled);
    }
    if (macro.variadic) {
        std::string rawVariadic;
        for (size_t i = fixedCount; i < arguments.size(); i++) {
            if (i != fixedCount) rawVariadic += ", ";
            rawVariadic += arguments[i];
        }
        rawArguments["__VA_ARGS__"] = rawVariadic;
        expandedArguments["__VA_ARGS__"] =
            expandMacros(rawVariadic, macros, path, lineNo, dateStr, timeStr, disabled);
    }

    const std::string &replacement = macro.replacement;
    std::string substituted;
    size_t i = 0;
    while (i < replacement.size()) {
        if (std::isspace(static_cast<unsigned char>(replacement[i]))) {
            substituted += replacement[i++];
            continue;
        }
        if (replacement.compare(i, 2, "##") == 0) {
            while (!substituted.empty() && std::isspace(static_cast<unsigned char>(substituted.back()))) {
                substituted.pop_back();
            }
            i += 2;
            while (i < replacement.size() && std::isspace(static_cast<unsigned char>(replacement[i]))) i++;
            size_t end = tokenEnd(replacement, i);
            std::string rhs = replacement.substr(i, end - i);
            if (!rhs.empty() && isIdentStart(rhs[0])) {
                auto raw = rawArguments.find(rhs);
                if (raw != rawArguments.end()) rhs = raw->second;
            }
            substituted += rhs;
            i = end;
            continue;
        }
        if (replacement[i] == '#') {
            size_t j = i + 1;
            while (j < replacement.size() && std::isspace(static_cast<unsigned char>(replacement[j]))) j++;
            size_t end = tokenEnd(replacement, j);
            std::string parameter = replacement.substr(j, end - j);
            auto raw = rawArguments.find(parameter);
            if (raw != rawArguments.end()) {
                substituted += stringifyArgument(raw->second);
                i = end;
                continue;
            }
        }
        size_t end = tokenEnd(replacement, i);
        std::string token = replacement.substr(i, end - i);
        if (isIdentStart(token[0])) {
            auto argument = expandedArguments.find(token);
            if (argument != expandedArguments.end()) {
                size_t next = end;
                while (next < replacement.size() && std::isspace(static_cast<unsigned char>(replacement[next]))) next++;
                substituted += replacement.compare(next, 2, "##") == 0 ? rawArguments[token] : argument->second;
                i = end;
                continue;
            }
        }
        substituted += token;
        i = end;
    }
    return expandMacros(substituted, macros, path, lineNo, dateStr, timeStr, disabled);
}

std::string expandMacros(const std::string &text, const MacroMap &macros, const std::string &path, int lineNo,
                         const std::string &dateStr, const std::string &timeStr,
                         const std::vector<std::string> &disabled) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '"' || text[i] == '\'') {
            size_t end = tokenEnd(text, i);
            out += text.substr(i, end - i);
            i = end;
            continue;
        }
        if (text.compare(i, 2, "//") == 0) {
            out += text.substr(i);
            break;
        }
        if (text.compare(i, 2, "/*") == 0) {
            size_t close = text.find("*/", i + 2);
            size_t end = close == std::string::npos ? text.size() : close + 2;
            out += text.substr(i, end - i);
            i = end;
            continue;
        }
        if (!isIdentStart(text[i])) {
            out += text[i++];
            continue;
        }
        size_t end = i + 1;
        while (end < text.size() && isIdentChar(text[end])) end++;
        std::string name = text.substr(i, end - i);
        if (std::find(disabled.begin(), disabled.end(), name) != disabled.end()) {
            out += name;
            i = end;
            continue;
        }
        std::string value;
        bool isMacro = true;
        if (name == "__LINE__") value = std::to_string(lineNo);
        else if (name == "__FILE__") value = "\"" + path + "\"";
        else if (name == "__DATE__") value = "\"" + dateStr + "\"";
        else if (name == "__TIME__") value = "\"" + timeStr + "\"";
        else {
            auto macro = macros.find(name);
            if (macro == macros.end()) {
                isMacro = false;
            } else if (!macro->second.functionLike) {
                value = macro->second.replacement;
            } else {
                size_t open = end;
                while (open < text.size() && std::isspace(static_cast<unsigned char>(text[open]))) open++;
                std::vector<std::string> arguments;
                size_t invocationEnd = open;
                if (!parseInvocation(text, open, arguments, invocationEnd)) {
                    isMacro = false;
                } else {
                    if (arguments.empty() && !macro->second.parameters.empty()) arguments.emplace_back();
                    std::vector<std::string> nestedDisabled = disabled;
                    nestedDisabled.push_back(name);
                    value = substituteFunctionMacro(macro->second, arguments, macros, path, lineNo, dateStr, timeStr,
                                                    nestedDisabled);
                    end = invocationEnd;
                }
            }
        }
        if (isMacro) {
            std::vector<std::string> nestedDisabled = disabled;
            nestedDisabled.push_back(name);
            out += expandMacros(value, macros, path, lineNo, dateStr, timeStr, nestedDisabled);
        } else {
            out += name;
        }
        i = end;
    }
    return out;
}

// Resolves every defined(NAME)/defined NAME to "1"/"0", before macro
// substitution, so `defined(FOO)` tests FOO's macro-ness rather than
// substituting its value first.
std::string resolveDefined(const std::string &expr, const MacroMap &macros) {
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

// Recursive-descent evaluator for an already-macro-substituted #if/#elif
// expression, real C operator precedence; any leftover identifier (always
// undefined, since macros were already substituted) evaluates to 0.
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
            return 0;
        }
        return 0;
    }
};

bool evalCondition(const std::string &rawExpr, const MacroMap &macros,
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

// One #if/#ifdef/#ifndef nesting level. `everTaken` tracks whether any
// branch in this chain has matched yet, for #elif/#else.
struct CondFrame {
    bool outerActive;
    bool everTaken;
    bool branchActive;
};

Macro parseMacroDefinition(const std::string &arg, const std::string &where) {
    size_t nameEnd = 0;
    if (arg.empty() || !isIdentStart(arg[0])) {
        throw std::runtime_error("pcc preprocessor: #define with invalid name at " + where);
    }
    while (nameEnd < arg.size() && isIdentChar(arg[nameEnd])) nameEnd++;

    Macro macro;
    size_t replacementStart = nameEnd;
    if (nameEnd < arg.size() && arg[nameEnd] == '(') {
        macro.functionLike = true;
        size_t i = nameEnd + 1;
        while (i < arg.size()) {
            while (i < arg.size() && std::isspace(static_cast<unsigned char>(arg[i]))) i++;
            if (i < arg.size() && arg[i] == ')') {
                replacementStart = i + 1;
                break;
            }
            if (arg.compare(i, 3, "...") == 0) {
                macro.variadic = true;
                i += 3;
                while (i < arg.size() && std::isspace(static_cast<unsigned char>(arg[i]))) i++;
                if (i >= arg.size() || arg[i] != ')') {
                    throw std::runtime_error("pcc preprocessor: variadic parameter must be last at " + where);
                }
                replacementStart = i + 1;
                break;
            }
            if (i >= arg.size() || !isIdentStart(arg[i])) {
                throw std::runtime_error("pcc preprocessor: malformed parameter list in #define at " + where);
            }
            size_t parameterStart = i++;
            while (i < arg.size() && isIdentChar(arg[i])) i++;
            std::string parameter = arg.substr(parameterStart, i - parameterStart);
            if (parameter == "__VA_ARGS__" ||
                std::find(macro.parameters.begin(), macro.parameters.end(), parameter) != macro.parameters.end()) {
                throw std::runtime_error("pcc preprocessor: duplicate or reserved macro parameter at " + where);
            }
            macro.parameters.push_back(parameter);
            while (i < arg.size() && std::isspace(static_cast<unsigned char>(arg[i]))) i++;
            if (i < arg.size() && arg[i] == ',') {
                i++;
                continue;
            }
            if (i < arg.size() && arg[i] == ')') {
                replacementStart = i + 1;
                break;
            }
            throw std::runtime_error("pcc preprocessor: malformed parameter list in #define at " + where);
        }
        if (replacementStart == nameEnd) {
            throw std::runtime_error("pcc preprocessor: unterminated parameter list in #define at " + where);
        }
    }
    macro.replacement = trim(arg.substr(replacementStart));
    return macro;
}

std::string preprocessFile(const std::string &path, MacroMap &macros,
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
    bool inBlockComment = false;

    auto isActive = [&]() { return condStack.empty() || condStack.back().branchActive; };
    auto where = [&](int n) { return path + ":" + std::to_string(n); };

    while (std::getline(in, line)) {
        lineNo++;
        std::string uncommented = stripComments(line, inBlockComment);
        std::string trimmed = trim(uncommented);

        if (!trimmed.empty() && trimmed[0] == '#') {
            std::string rest = trim(trimmed.substr(1));
            size_t sp = rest.find_first_of(" \t");
            std::string kw = (sp == std::string::npos) ? rest : rest.substr(0, sp);
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
                    size_t nameEnd = 0;
                    while (nameEnd < arg.size() && isIdentChar(arg[nameEnd])) nameEnd++;
                    if (nameEnd == 0 || !isIdentStart(arg[0])) {
                        throw std::runtime_error("pcc preprocessor: #define with invalid name at " + where(lineNo));
                    }
                    std::string name = arg.substr(0, nameEnd);
                    macros[name] = parseMacroDefinition(arg, where(lineNo));
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
        out << expandMacros(uncommented, macros, path, lineNo, dateStr, timeStr) << "\n";
    }

    if (!condStack.empty()) {
        throw std::runtime_error("pcc preprocessor: unterminated #if/#ifdef/#ifndef in '" + path + "'");
    }

    includeStack.pop_back();
    return out.str();
}

}  // namespace

std::string preprocess(const std::string &path, const std::map<std::string, std::string> &predefined) {
    MacroMap macros;
    for (const auto &entry : predefined) macros[entry.first].replacement = entry.second;
    std::vector<std::string> includeStack;
    return preprocessFile(path, macros, includeStack, currentDateStr(), currentTimeStr());
}

}  // namespace pcc
