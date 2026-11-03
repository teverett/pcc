
// Generated from /Users/tom/projects/pcc/grammar/PccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  PccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, LOCAL_KW = 4, 
    IF_KW = 5, ELSE_KW = 6, WHILE_KW = 7, RETURN_KW = 8, BREAK_KW = 9, CONTINUE_KW = 10, 
    POST_KW = 11, LPAREN = 12, RPAREN = 13, LBRACK = 14, RBRACK = 15, LBRACE = 16, 
    RBRACE = 17, SEMI = 18, COMMA = 19, ASSIGN = 20, HASH = 21, STAR = 22, 
    AMP = 23, MINUS = 24, BANG = 25, TILDE = 26, SLASH = 27, PERCENT = 28, 
    PLUS = 29, SHL = 30, SHR = 31, LE = 32, GE = 33, LT = 34, GT = 35, EQ = 36, 
    NE = 37, CARET = 38, PIPE = 39, ANDAND = 40, OROR = 41, IDENT = 42, 
    NUMBER = 43, WS = 44, ASM_BEGIN = 45, ASM_TEXT = 46, ASM_END = 47
  };

  enum {
    ASM_MODE = 1
  };

  explicit PccIRLexer(antlr4::CharStream *input);

  ~PccIRLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

