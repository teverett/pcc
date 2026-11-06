
// Generated from /Users/tom/projects/pcc/grammar/PccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  PccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, LOCAL_KW = 8, IF_KW = 9, ELSE_KW = 10, 
    WHILE_KW = 11, RETURN_KW = 12, BREAK_KW = 13, CONTINUE_KW = 14, POST_KW = 15, 
    OLDVAL_KW = 16, LPAREN = 17, RPAREN = 18, LBRACK = 19, RBRACK = 20, 
    LBRACE = 21, RBRACE = 22, SEMI = 23, COMMA = 24, ASSIGN = 25, HASH = 26, 
    STAR = 27, AMP = 28, MINUS = 29, BANG = 30, TILDE = 31, SLASH = 32, 
    PERCENT = 33, PLUS = 34, SHL = 35, SHR = 36, LE = 37, GE = 38, LT = 39, 
    GT = 40, EQ = 41, NE = 42, CARET = 43, PIPE = 44, ANDAND = 45, OROR = 46, 
    PLUSEQ = 47, MINUSEQ = 48, STAREQ = 49, SLASHEQ = 50, PERCENTEQ = 51, 
    AMPEQ = 52, CARETEQ = 53, PIPEEQ = 54, SHLEQ = 55, SHREQ = 56, QUESTION = 57, 
    COLON = 58, IDENT = 59, NUMBER = 60, WS = 61, ASM_BEGIN = 62, ASM_TEXT = 63, 
    ASM_END = 64
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

