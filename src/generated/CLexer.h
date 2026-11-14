
// Generated from /Users/tom/projects/pcc/grammar/CLexer.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CLexer : public antlr4::Lexer {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    VOID_KW = 7, SIZEOF_KW = 8, IF_KW = 9, ELSE_KW = 10, WHILE_KW = 11, 
    FOR_KW = 12, SWITCH_KW = 13, CASE_KW = 14, DEFAULT_KW = 15, RETURN_KW = 16, 
    BREAK_KW = 17, CONTINUE_KW = 18, GOTO_KW = 19, EXTERN_KW = 20, STATIC_KW = 21, 
    CONST_KW = 22, STRUCT_KW = 23, ENUM_KW = 24, LPAREN = 25, RPAREN = 26, 
    LBRACK = 27, RBRACK = 28, LBRACE = 29, RBRACE = 30, AT = 31, SEMI = 32, 
    COMMA = 33, ASSIGN = 34, STAR = 35, AMP = 36, MINUS = 37, BANG = 38, 
    TILDE = 39, SLASH = 40, PERCENT = 41, PLUS = 42, SHL = 43, SHR = 44, 
    LE = 45, GE = 46, LT = 47, GT = 48, EQ = 49, NE = 50, CARET = 51, PIPE = 52, 
    ANDAND = 53, OROR = 54, PLUSEQ = 55, MINUSEQ = 56, STAREQ = 57, SLASHEQ = 58, 
    PERCENTEQ = 59, AMPEQ = 60, CARETEQ = 61, PIPEEQ = 62, SHLEQ = 63, SHREQ = 64, 
    INC = 65, DEC = 66, QUESTION = 67, COLON = 68, DOT = 69, ARROW = 70, 
    IDENT = 71, NUMBER = 72, CHARLIT = 73, STRINGLIT = 74, LINE_COMMENT = 75, 
    BLOCK_COMMENT = 76, WS = 77, ASM_BEGIN = 78, ASM_TEXT = 79, ASM_END = 80
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

