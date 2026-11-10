
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    SIZEOF_KW = 7, IF_KW = 8, ELSE_KW = 9, WHILE_KW = 10, FOR_KW = 11, SWITCH_KW = 12, 
    CASE_KW = 13, DEFAULT_KW = 14, RETURN_KW = 15, BREAK_KW = 16, CONTINUE_KW = 17, 
    GOTO_KW = 18, EXTERN_KW = 19, LPAREN = 20, RPAREN = 21, LBRACK = 22, 
    RBRACK = 23, LBRACE = 24, RBRACE = 25, AT = 26, SEMI = 27, COMMA = 28, 
    ASSIGN = 29, STAR = 30, AMP = 31, MINUS = 32, BANG = 33, TILDE = 34, 
    SLASH = 35, PERCENT = 36, PLUS = 37, SHL = 38, SHR = 39, LE = 40, GE = 41, 
    LT = 42, GT = 43, EQ = 44, NE = 45, CARET = 46, PIPE = 47, ANDAND = 48, 
    OROR = 49, PLUSEQ = 50, MINUSEQ = 51, STAREQ = 52, SLASHEQ = 53, PERCENTEQ = 54, 
    AMPEQ = 55, CARETEQ = 56, PIPEEQ = 57, SHLEQ = 58, SHREQ = 59, INC = 60, 
    DEC = 61, QUESTION = 62, COLON = 63, IDENT = 64, NUMBER = 65, CHARLIT = 66, 
    STRINGLIT = 67, LINE_COMMENT = 68, BLOCK_COMMENT = 69, WS = 70, ASM_BEGIN = 71, 
    ASM_TEXT = 72, ASM_END = 73
  };

  enum {
    ASM_MODE = 1
  };

  explicit CLexer(antlr4::CharStream *input);

  ~CLexer() override;


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

