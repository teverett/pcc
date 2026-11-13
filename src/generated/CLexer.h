
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
    STRUCT_KW = 22, LPAREN = 23, RPAREN = 24, LBRACK = 25, RBRACK = 26, 
    LBRACE = 27, RBRACE = 28, AT = 29, SEMI = 30, COMMA = 31, ASSIGN = 32, 
    STAR = 33, AMP = 34, MINUS = 35, BANG = 36, TILDE = 37, SLASH = 38, 
    PERCENT = 39, PLUS = 40, SHL = 41, SHR = 42, LE = 43, GE = 44, LT = 45, 
    GT = 46, EQ = 47, NE = 48, CARET = 49, PIPE = 50, ANDAND = 51, OROR = 52, 
    PLUSEQ = 53, MINUSEQ = 54, STAREQ = 55, SLASHEQ = 56, PERCENTEQ = 57, 
    AMPEQ = 58, CARETEQ = 59, PIPEEQ = 60, SHLEQ = 61, SHREQ = 62, INC = 63, 
    DEC = 64, QUESTION = 65, COLON = 66, DOT = 67, ARROW = 68, IDENT = 69, 
    NUMBER = 70, CHARLIT = 71, STRINGLIT = 72, LINE_COMMENT = 73, BLOCK_COMMENT = 74, 
    WS = 75, ASM_BEGIN = 76, ASM_TEXT = 77, ASM_END = 78
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

