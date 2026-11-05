
// Generated from /Users/tom/projects/pcc/grammar/PccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  PccAsmParser : public antlr4::Parser {
public:
  enum {
    T__0 = 1, IDENT = 2, NUMBER = 3, COLON = 4, EQUALS = 5, COMMA = 6, PLUS = 7, 
    MINUS = 8, LBRACKET = 9, RBRACKET = 10, LPAREN = 11, RPAREN = 12, COMMENT = 13, 
    NEWLINE = 14, WS = 15
  };

  enum {
    RuleProgram = 0, RuleLine = 1, RuleLabel = 2, RuleStatement = 3, RuleOperand = 4, 
    RuleExpr = 5, RuleSyllable = 6, RuleSign = 7, RuleAtom = 8
  };

  explicit PccAsmParser(antlr4::TokenStream *input);

  PccAsmParser(antlr4::TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options);

  ~PccAsmParser() override;

  std::string getGrammarFileName() const override;

  const antlr4::atn::ATN& getATN() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;


  class ProgramContext;
  class LineContext;
  class LabelContext;
  class StatementContext;
  class OperandContext;
  class ExprContext;
  class SyllableContext;
  class SignContext;
  class AtomContext; 

  class  ProgramContext : public antlr4::ParserRuleContext {
  public:
    ProgramContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EOF();
    std::vector<LineContext *> line();
    LineContext* line(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ProgramContext* program();

  class  LineContext : public antlr4::ParserRuleContext {
  public:
    LineContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *NEWLINE();
    std::vector<LabelContext *> label();
    LabelContext* label(size_t i);
    StatementContext *statement();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LineContext* line();

  class  LabelContext : public antlr4::ParserRuleContext {
  public:
    LabelContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *COLON();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  LabelContext* label();

  class  StatementContext : public antlr4::ParserRuleContext {
  public:
    StatementContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    StatementContext() = default;
    void copyFrom(StatementContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  TimesStmtContext : public StatementContext {
  public:
    TimesStmtContext(StatementContext *ctx);

    PccAsmParser::ExprContext *count = nullptr;
    antlr4::Token *mnemonic = nullptr;
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *NUMBER();
    std::vector<OperandContext *> operand();
    OperandContext* operand(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignStmtContext : public StatementContext {
  public:
    AssignStmtContext(StatementContext *ctx);

    antlr4::Token *lhs = nullptr;
    PccAsmParser::ExprContext *rhs = nullptr;
    antlr4::tree::TerminalNode *EQUALS();
    antlr4::tree::TerminalNode *IDENT();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  InstrStmtContext : public StatementContext {
  public:
    InstrStmtContext(StatementContext *ctx);

    antlr4::Token *mnemonic = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *NUMBER();
    std::vector<OperandContext *> operand();
    OperandContext* operand(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  StatementContext* statement();

  class  OperandContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *size = nullptr;
    OperandContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACKET();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RBRACKET();
    antlr4::tree::TerminalNode *IDENT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  OperandContext* operand();

  class  ExprContext : public antlr4::ParserRuleContext {
  public:
    ExprContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<SyllableContext *> syllable();
    SyllableContext* syllable(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ExprContext* expr();

  class  SyllableContext : public antlr4::ParserRuleContext {
  public:
    SyllableContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    AtomContext *atom();
    SignContext *sign();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  SyllableContext* syllable();

  class  SignContext : public antlr4::ParserRuleContext {
  public:
    SignContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *PLUS();
    antlr4::tree::TerminalNode *MINUS();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  SignContext* sign();

  class  AtomContext : public antlr4::ParserRuleContext {
  public:
    AtomContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  AtomContext* atom();


  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

