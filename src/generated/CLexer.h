
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    SIZEOF_KW = 7, IF_KW = 8, ELSE_KW = 9, WHILE_KW = 10, FOR_KW = 11, RETURN_KW = 12, 
    BREAK_KW = 13, CONTINUE_KW = 14, LPAREN = 15, RPAREN = 16, LBRACK = 17, 
    RBRACK = 18, LBRACE = 19, RBRACE = 20, AT = 21, SEMI = 22, COMMA = 23, 
    ASSIGN = 24, STAR = 25, AMP = 26, MINUS = 27, BANG = 28, TILDE = 29, 
    SLASH = 30, PERCENT = 31, PLUS = 32, SHL = 33, SHR = 34, LE = 35, GE = 36, 
    LT = 37, GT = 38, EQ = 39, NE = 40, CARET = 41, PIPE = 42, ANDAND = 43, 
    OROR = 44, IDENT = 45, NUMBER = 46, CHARLIT = 47, LINE_COMMENT = 48, 
    BLOCK_COMMENT = 49, WS = 50, ASM_BEGIN = 51, ASM_TEXT = 52, ASM_END = 53
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

