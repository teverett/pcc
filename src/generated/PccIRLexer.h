
// Generated from /Users/tom/projects/pcc/grammar/PccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  PccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, LOCAL_KW = 8, IF_KW = 9, ELSE_KW = 10, 
    WHILE_KW = 11, RETURN_KW = 12, BREAK_KW = 13, CONTINUE_KW = 14, GOTO_KW = 15, 
    POST_KW = 16, OLDVAL_KW = 17, LPAREN = 18, RPAREN = 19, LBRACK = 20, 
    RBRACK = 21, LBRACE = 22, RBRACE = 23, SEMI = 24, COMMA = 25, ASSIGN = 26, 
    HASH = 27, STAR = 28, AMP = 29, MINUS = 30, BANG = 31, TILDE = 32, SLASH = 33, 
    PERCENT = 34, PLUS = 35, SHL = 36, SHR = 37, LE = 38, GE = 39, LT = 40, 
    GT = 41, EQ = 42, NE = 43, CARET = 44, PIPE = 45, ANDAND = 46, OROR = 47, 
    PLUSEQ = 48, MINUSEQ = 49, STAREQ = 50, SLASHEQ = 51, PERCENTEQ = 52, 
    AMPEQ = 53, CARETEQ = 54, PIPEEQ = 55, SHLEQ = 56, SHREQ = 57, QUESTION = 58, 
    COLON = 59, IDENT = 60, NUMBER = 61, WS = 62, ASM_BEGIN = 63, ASM_TEXT = 64, 
    ASM_END = 65
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

