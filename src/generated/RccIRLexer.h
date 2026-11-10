
// Generated from /Users/tom/projects/pcc/grammar/RccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  RccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, VOID_KW = 8, LOCAL_KW = 9, IF_KW = 10, 
    ELSE_KW = 11, WHILE_KW = 12, RETURN_KW = 13, BREAK_KW = 14, CONTINUE_KW = 15, 
    GOTO_KW = 16, POST_KW = 17, OLDVAL_KW = 18, LPAREN = 19, RPAREN = 20, 
    LBRACK = 21, RBRACK = 22, LBRACE = 23, RBRACE = 24, SEMI = 25, COMMA = 26, 
    ASSIGN = 27, HASH = 28, STAR = 29, AMP = 30, MINUS = 31, BANG = 32, 
    TILDE = 33, SLASH = 34, PERCENT = 35, PLUS = 36, SHL = 37, SHR = 38, 
    LE = 39, GE = 40, LT = 41, GT = 42, EQ = 43, NE = 44, CARET = 45, PIPE = 46, 
    ANDAND = 47, OROR = 48, PLUSEQ = 49, MINUSEQ = 50, STAREQ = 51, SLASHEQ = 52, 
    PERCENTEQ = 53, AMPEQ = 54, CARETEQ = 55, PIPEEQ = 56, SHLEQ = 57, SHREQ = 58, 
    QUESTION = 59, COLON = 60, IDENT = 61, NUMBER = 62, WS = 63, ASM_BEGIN = 64, 
    ASM_TEXT = 65, ASM_END = 66
  };

  enum {
    ASM_MODE = 1
  };

  explicit RccIRLexer(antlr4::CharStream *input);

  ~RccIRLexer() override;


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

