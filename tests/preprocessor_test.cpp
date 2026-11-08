#include "preprocessor/Preprocessor.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string preprocessSource(const std::string &source) {
    auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("pcc-preprocessor-test-" + std::to_string(suffix) + ".c");
    {
        std::ofstream file(path);
        if (!file) throw std::runtime_error("could not create preprocessor test source");
        file << source;
    }
    try {
        std::string result = pcc::preprocess(path.string(), {});
        std::filesystem::remove(path);
        return result;
    } catch (...) {
        std::filesystem::remove(path);
        throw;
    }
}

void requireContains(const std::string &text, const std::string &expected) {
    if (text.find(expected) == std::string::npos) {
        throw std::runtime_error("preprocessor output did not contain: " + expected + "\nOutput:\n" + text);
    }
}

void testFunctionMacrosAndTokenOperators() {
    std::string result = preprocessSource(
        "#define VALUE 8\n"
        "#define APPLY(a, b) ((a) + (b))\n"
        "#define STRINGIFY(x) #x\n"
        "#define JOIN(a, b) a ## b\n"
        "#define EMPTY() 17\n"
        "int n = APPLY(VALUE, 2);\n"
        "char *s = STRINGIFY(VALUE   + 1);\n"
        "char *quoted = STRINGIFY(\"A  VALUE\");\n"
        "int JOIN(var, iable) = 4;\n"
        "int empty = EMPTY();\n"
        "const char *literal = \"VALUE\";\n"
        "char character = 'V';\n"
        "int /* VALUE */ result = VALUE; // VALUE\n");

    requireContains(result, "int n = ((8) + (2));");
    requireContains(result, "char *s = \"VALUE + 1\";");
    requireContains(result, "char *quoted = \"\\\"A  VALUE\\\"\";");
    requireContains(result, "int variable = 4;");
    requireContains(result, "int empty = 17;");
    requireContains(result, "const char *literal = \"VALUE\";");
    requireContains(result, "char character = 'V';");
    requireContains(result, "int             result = 8;");
    if (result.find("// VALUE") != std::string::npos) {
        throw std::runtime_error("line comment was not preserved as a comment");
    }
}

void testVariadicAndRecursiveExpansion() {
    std::string result = preprocessSource(
        "#define ADD(a, b) ((a) + (b))\n"
        "#define CALL(first, ...) first, __VA_ARGS__\n"
        "#define TWICE(x) ((x) + (x))\n"
        "int values[] = {CALL(1, 2, 3)};\n"
        "int nested = TWICE(ADD(2, 3));\n");

    requireContains(result, "int values[] = {1, 2, 3};");
    requireContains(result, "int nested = ((((2) + (3))) + (((2) + (3))));");
}

void testRecursiveMacroTerminates() {
    std::string result = preprocessSource(
        "#define SELF SELF\n"
        "int value = SELF;\n");
    requireContains(result, "int value = SELF;");
}

}  // namespace

int main() {
    try {
        testFunctionMacrosAndTokenOperators();
        testVariadicAndRecursiveExpansion();
        testRecursiveMacroTerminates();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
