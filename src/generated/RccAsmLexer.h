
// Generated from /Users/tom/projects/pcc/grammar/RccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  RccAsmLexer : public antlr4::Lexer {
public:
  enum {
    T__0 = 1, IDENT = 2, NUMBER = 3, COLON = 4, EQUALS = 5, COMMA = 6, PLUS = 7, 
    MINUS = 8, LBRACKET = 9, RBRACKET = 10, LPAREN = 11, RPAREN = 12, COMMENT = 13, 
    NEWLINE = 14, WS = 15
  };

  explicit RccAsmLexer(antlr4::CharStream *input);

  ~RccAsmLexer() override;


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

