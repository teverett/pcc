#include "IRValidator.h"

#include <stdexcept>
#include <string>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "IRPrinter.h"
#include "RccIRLexer.h"
#include "RccIRParser.h"

namespace rcc {

namespace {

// ANTLR's default listener only prints to stderr; this one records whether
// an error happened and the first message, to throw below.
class CountingErrorListener : public antlr4::BaseErrorListener {
   public:
    void syntaxError(antlr4::Recognizer *, antlr4::Token *, size_t line, size_t charPositionInLine,
                      const std::string &msg, std::exception_ptr) override {
        if (count == 0) {
            firstMessage = "line " + std::to_string(line) + ":" + std::to_string(charPositionInLine) + " " + msg;
        }
        count++;
    }
    int count = 0;
    std::string firstMessage;
};

}  // namespace

void validateIRSyntax(const ir::Program &prog) {
    std::string text = printIR(prog);

    CountingErrorListener errors;

    antlr4::ANTLRInputStream input(text);
    RccIRLexer lexer(&input);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    RccIRParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    parser.program();

    if (errors.count > 0) {
        throw std::runtime_error(
            "internal error: rcc's own generated IR does not parse against grammar/RccIRLexer.g4 + "
            "grammar/RccIRParser.g4 (" +
            errors.firstMessage + ") - this is a rcc bug (IRPrinter.cpp and the IR grammar have drifted apart), "
                                   "not a problem with your source file");
    }
}

}  // namespace rcc
