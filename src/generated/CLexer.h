
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, IF_KW = 2, ELSE_KW = 3, WHILE_KW = 4, FOR_KW = 5, RETURN_KW = 6, 
    LPAREN = 7, RPAREN = 8, LBRACK = 9, RBRACK = 10, LBRACE = 11, RBRACE = 12, 
    AT = 13, SEMI = 14, COMMA = 15, ASSIGN = 16, STAR = 17, AMP = 18, MINUS = 19, 
    BANG = 20, TILDE = 21, SLASH = 22, PERCENT = 23, PLUS = 24, SHL = 25, 
    SHR = 26, LE = 27, GE = 28, LT = 29, GT = 30, EQ = 31, NE = 32, CARET = 33, 
    PIPE = 34, ANDAND = 35, OROR = 36, IDENT = 37, NUMBER = 38, CHARLIT = 39, 
    LINE_COMMENT = 40, BLOCK_COMMENT = 41, WS = 42, ASM_BEGIN = 43, ASM_TEXT = 44, 
    ASM_END = 45
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

