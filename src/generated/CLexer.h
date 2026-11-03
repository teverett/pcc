
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, IF_KW = 2, ELSE_KW = 3, WHILE_KW = 4, FOR_KW = 5, RETURN_KW = 6, 
    BREAK_KW = 7, CONTINUE_KW = 8, LPAREN = 9, RPAREN = 10, LBRACK = 11, 
    RBRACK = 12, LBRACE = 13, RBRACE = 14, AT = 15, SEMI = 16, COMMA = 17, 
    ASSIGN = 18, STAR = 19, AMP = 20, MINUS = 21, BANG = 22, TILDE = 23, 
    SLASH = 24, PERCENT = 25, PLUS = 26, SHL = 27, SHR = 28, LE = 29, GE = 30, 
    LT = 31, GT = 32, EQ = 33, NE = 34, CARET = 35, PIPE = 36, ANDAND = 37, 
    OROR = 38, IDENT = 39, NUMBER = 40, CHARLIT = 41, LINE_COMMENT = 42, 
    BLOCK_COMMENT = 43, WS = 44, ASM_BEGIN = 45, ASM_TEXT = 46, ASM_END = 47
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

