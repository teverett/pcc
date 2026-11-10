
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    VOID_KW = 7, SIZEOF_KW = 8, IF_KW = 9, ELSE_KW = 10, WHILE_KW = 11, 
    FOR_KW = 12, SWITCH_KW = 13, CASE_KW = 14, DEFAULT_KW = 15, RETURN_KW = 16, 
    BREAK_KW = 17, CONTINUE_KW = 18, GOTO_KW = 19, EXTERN_KW = 20, LPAREN = 21, 
    RPAREN = 22, LBRACK = 23, RBRACK = 24, LBRACE = 25, RBRACE = 26, AT = 27, 
    SEMI = 28, COMMA = 29, ASSIGN = 30, STAR = 31, AMP = 32, MINUS = 33, 
    BANG = 34, TILDE = 35, SLASH = 36, PERCENT = 37, PLUS = 38, SHL = 39, 
    SHR = 40, LE = 41, GE = 42, LT = 43, GT = 44, EQ = 45, NE = 46, CARET = 47, 
    PIPE = 48, ANDAND = 49, OROR = 50, PLUSEQ = 51, MINUSEQ = 52, STAREQ = 53, 
    SLASHEQ = 54, PERCENTEQ = 55, AMPEQ = 56, CARETEQ = 57, PIPEEQ = 58, 
    SHLEQ = 59, SHREQ = 60, INC = 61, DEC = 62, QUESTION = 63, COLON = 64, 
    IDENT = 65, NUMBER = 66, CHARLIT = 67, STRINGLIT = 68, LINE_COMMENT = 69, 
    BLOCK_COMMENT = 70, WS = 71, ASM_BEGIN = 72, ASM_TEXT = 73, ASM_END = 74
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

