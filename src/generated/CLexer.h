
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    VOID_KW = 7, SIZEOF_KW = 8, IF_KW = 9, ELSE_KW = 10, WHILE_KW = 11, 
    FOR_KW = 12, SWITCH_KW = 13, CASE_KW = 14, DEFAULT_KW = 15, RETURN_KW = 16, 
    BREAK_KW = 17, CONTINUE_KW = 18, GOTO_KW = 19, EXTERN_KW = 20, STRUCT_KW = 21, 
    LPAREN = 22, RPAREN = 23, LBRACK = 24, RBRACK = 25, LBRACE = 26, RBRACE = 27, 
    AT = 28, SEMI = 29, COMMA = 30, ASSIGN = 31, STAR = 32, AMP = 33, MINUS = 34, 
    BANG = 35, TILDE = 36, SLASH = 37, PERCENT = 38, PLUS = 39, SHL = 40, 
    SHR = 41, LE = 42, GE = 43, LT = 44, GT = 45, EQ = 46, NE = 47, CARET = 48, 
    PIPE = 49, ANDAND = 50, OROR = 51, PLUSEQ = 52, MINUSEQ = 53, STAREQ = 54, 
    SLASHEQ = 55, PERCENTEQ = 56, AMPEQ = 57, CARETEQ = 58, PIPEEQ = 59, 
    SHLEQ = 60, SHREQ = 61, INC = 62, DEC = 63, QUESTION = 64, COLON = 65, 
    DOT = 66, ARROW = 67, IDENT = 68, NUMBER = 69, CHARLIT = 70, STRINGLIT = 71, 
    LINE_COMMENT = 72, BLOCK_COMMENT = 73, WS = 74, ASM_BEGIN = 75, ASM_TEXT = 76, 
    ASM_END = 77
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

