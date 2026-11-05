
// Generated from /Users/tom/projects/pcc/grammar/PccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  PccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, LOCAL_KW = 8, IF_KW = 9, ELSE_KW = 10, 
    WHILE_KW = 11, RETURN_KW = 12, BREAK_KW = 13, CONTINUE_KW = 14, POST_KW = 15, 
    LPAREN = 16, RPAREN = 17, LBRACK = 18, RBRACK = 19, LBRACE = 20, RBRACE = 21, 
    SEMI = 22, COMMA = 23, ASSIGN = 24, HASH = 25, STAR = 26, AMP = 27, 
    MINUS = 28, BANG = 29, TILDE = 30, SLASH = 31, PERCENT = 32, PLUS = 33, 
    SHL = 34, SHR = 35, LE = 36, GE = 37, LT = 38, GT = 39, EQ = 40, NE = 41, 
    CARET = 42, PIPE = 43, ANDAND = 44, OROR = 45, IDENT = 46, NUMBER = 47, 
    WS = 48, ASM_BEGIN = 49, ASM_TEXT = 50, ASM_END = 51
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

