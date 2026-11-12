
// Generated from /Users/tom/projects/pcc/grammar/RccIRLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  RccIRLexer : public antlr4::Lexer {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, VOID_KW = 8, STRUCT_KW = 9, LOCAL_KW = 10, 
    IF_KW = 11, ELSE_KW = 12, WHILE_KW = 13, RETURN_KW = 14, BREAK_KW = 15, 
    CONTINUE_KW = 16, GOTO_KW = 17, POST_KW = 18, OLDVAL_KW = 19, LPAREN = 20, 
    RPAREN = 21, LBRACK = 22, RBRACK = 23, LBRACE = 24, RBRACE = 25, SEMI = 26, 
    COMMA = 27, ASSIGN = 28, HASH = 29, STAR = 30, AMP = 31, MINUS = 32, 
    BANG = 33, TILDE = 34, SLASH = 35, PERCENT = 36, PLUS = 37, SHL = 38, 
    SHR = 39, LE = 40, GE = 41, LT = 42, GT = 43, EQ = 44, NE = 45, CARET = 46, 
    PIPE = 47, ANDAND = 48, OROR = 49, PLUSEQ = 50, MINUSEQ = 51, STAREQ = 52, 
    SLASHEQ = 53, PERCENTEQ = 54, AMPEQ = 55, CARETEQ = 56, PIPEEQ = 57, 
    SHLEQ = 58, SHREQ = 59, QUESTION = 60, COLON = 61, IDENT = 62, NUMBER = 63, 
    WS = 64, ASM_BEGIN = 65, ASM_TEXT = 66, ASM_END = 67
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

