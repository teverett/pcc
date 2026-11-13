
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
    STRUCT_KW = 22, ENUM_KW = 23, LPAREN = 24, RPAREN = 25, LBRACK = 26, 
    RBRACK = 27, LBRACE = 28, RBRACE = 29, AT = 30, SEMI = 31, COMMA = 32, 
    ASSIGN = 33, STAR = 34, AMP = 35, MINUS = 36, BANG = 37, TILDE = 38, 
    SLASH = 39, PERCENT = 40, PLUS = 41, SHL = 42, SHR = 43, LE = 44, GE = 45, 
    LT = 46, GT = 47, EQ = 48, NE = 49, CARET = 50, PIPE = 51, ANDAND = 52, 
    OROR = 53, PLUSEQ = 54, MINUSEQ = 55, STAREQ = 56, SLASHEQ = 57, PERCENTEQ = 58, 
    AMPEQ = 59, CARETEQ = 60, PIPEEQ = 61, SHLEQ = 62, SHREQ = 63, INC = 64, 
    DEC = 65, QUESTION = 66, COLON = 67, DOT = 68, ARROW = 69, IDENT = 70, 
    NUMBER = 71, CHARLIT = 72, STRINGLIT = 73, LINE_COMMENT = 74, BLOCK_COMMENT = 75, 
    WS = 76, ASM_BEGIN = 77, ASM_TEXT = 78, ASM_END = 79
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

