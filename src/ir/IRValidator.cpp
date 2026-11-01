#include "IRValidator.h"

#include <stdexcept>
#include <string>

#include "ANTLRInputStream.h"
#include "BaseErrorListener.h"
#include "CommonTokenStream.h"
#include "IRPrinter.h"
#include "PccIRLexer.h"
#include "PccIRParser.h"

namespace pcc {

namespace {

// Same idea as main.cpp's own CountingErrorListener for the C
// frontend: ANTLR's default listener only prints to stderr and keeps
// going, so this one also remembers whether a syntax error happened
// at all (and what the first one said), to turn into a thrown
// std::runtime_error below.
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
    PccIRLexer lexer(&input);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errors);

    antlr4::CommonTokenStream tokens(&lexer);
    PccIRParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&errors);

    parser.program();

    if (errors.count > 0) {
        throw std::runtime_error(
            "internal error: pcc's own generated IR does not parse against grammar/PccIRLexer.g4 + "
            "grammar/PccIRParser.g4 (" +
            errors.firstMessage + ") - this is a pcc bug (IRPrinter.cpp and the IR grammar have drifted apart), "
                                   "not a problem with your source file");
    }
}

}  // namespace pcc
