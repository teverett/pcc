
// Generated from /Users/tom/projects/pcc/grammar/CParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CParser : public antlr4::Parser {
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
    RuleProgram = 0, RuleTopLevelDecl = 1, RuleFunctionDecl = 2, RuleParamList = 3, 
    RuleParam = 4, RuleGlobalDecl = 5, RuleArrayInit = 6, RuleBlock = 7, 
    RuleStatement = 8, RuleSimpleStmt = 9, RuleExpr = 10, RuleCondExpr = 11, 
    RuleArgList = 12
  };

  explicit CParser(antlr4::TokenStream *input);

  CParser(antlr4::TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options);

  ~CParser() override;

  std::string getGrammarFileName() const override;

  const antlr4::atn::ATN& getATN() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;


  class ProgramContext;
  class TopLevelDeclContext;
  class FunctionDeclContext;
  class ParamListContext;
  class ParamContext;
  class GlobalDeclContext;
  class ArrayInitContext;
  class BlockContext;
  class StatementContext;
  class SimpleStmtContext;
  class ExprContext;
  class CondExprContext;
  class ArgListContext; 

  class  ProgramContext : public antlr4::ParserRuleContext {
  public:
    ProgramContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *EOF();
    std::vector<TopLevelDeclContext *> topLevelDecl();
    TopLevelDeclContext* topLevelDecl(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ProgramContext* program();

  class  TopLevelDeclContext : public antlr4::ParserRuleContext {
  public:
    TopLevelDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    FunctionDeclContext *functionDecl();
    GlobalDeclContext *globalDecl();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TopLevelDeclContext* topLevelDecl();

  class  FunctionDeclContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *addr = nullptr;
    FunctionDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    BlockContext *block();
    antlr4::tree::TerminalNode *SEMI();
    ParamListContext *paramList();
    antlr4::tree::TerminalNode *AT();
    antlr4::tree::TerminalNode *NUMBER();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FunctionDeclContext* functionDecl();

  class  ParamListContext : public antlr4::ParserRuleContext {
  public:
    ParamListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ParamContext *> param();
    ParamContext* param(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ParamListContext* paramList();

  class  ParamContext : public antlr4::ParserRuleContext {
  public:
    ParamContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ParamContext* param();

  class  GlobalDeclContext : public antlr4::ParserRuleContext {
  public:
    GlobalDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    GlobalDeclContext() = default;
    void copyFrom(GlobalDeclContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  GlobalArrayDeclContext : public GlobalDeclContext {
  public:
    GlobalArrayDeclContext(GlobalDeclContext *ctx);

    antlr4::Token *size = nullptr;
    antlr4::Token *addr = nullptr;
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *SEMI();
    std::vector<antlr4::tree::TerminalNode *> NUMBER();
    antlr4::tree::TerminalNode* NUMBER(size_t i);
    antlr4::tree::TerminalNode *ASSIGN();
    ArrayInitContext *arrayInit();
    antlr4::tree::TerminalNode *AT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GlobalScalarDeclContext : public GlobalDeclContext {
  public:
    GlobalScalarDeclContext(GlobalDeclContext *ctx);

    antlr4::Token *sign = nullptr;
    antlr4::Token *val = nullptr;
    antlr4::Token *addr = nullptr;
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *ASSIGN();
    antlr4::tree::TerminalNode *AT();
    std::vector<antlr4::tree::TerminalNode *> NUMBER();
    antlr4::tree::TerminalNode* NUMBER(size_t i);
    antlr4::tree::TerminalNode *MINUS();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  GlobalDeclContext* globalDecl();

  class  ArrayInitContext : public antlr4::ParserRuleContext {
  public:
    antlr4::Token *numberToken = nullptr;
    std::vector<antlr4::Token *> elem;
    ArrayInitContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<antlr4::tree::TerminalNode *> NUMBER();
    antlr4::tree::TerminalNode* NUMBER(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrayInitContext* arrayInit();

  class  BlockContext : public antlr4::ParserRuleContext {
  public:
    BlockContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  BlockContext* block();

  class  StatementContext : public antlr4::ParserRuleContext {
  public:
    StatementContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    StatementContext() = default;
    void copyFrom(StatementContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  ForStmtContext : public StatementContext {
  public:
    ForStmtContext(StatementContext *ctx);

    CParser::SimpleStmtContext *init = nullptr;
    CParser::ExprContext *cond = nullptr;
    CParser::SimpleStmtContext *post = nullptr;
    antlr4::tree::TerminalNode *FOR_KW();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<antlr4::tree::TerminalNode *> SEMI();
    antlr4::tree::TerminalNode* SEMI(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    StatementContext *statement();
    std::vector<SimpleStmtContext *> simpleStmt();
    SimpleStmtContext* simpleStmt(size_t i);
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AsmStmtContext : public StatementContext {
  public:
    AsmStmtContext(StatementContext *ctx);

    antlr4::Token *text = nullptr;
    antlr4::tree::TerminalNode *ASM_BEGIN();
    antlr4::tree::TerminalNode *ASM_END();
    antlr4::tree::TerminalNode *ASM_TEXT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  WhileStmtContext : public StatementContext {
  public:
    WhileStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *WHILE_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    StatementContext *statement();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IfStmtContext : public StatementContext {
  public:
    IfStmtContext(StatementContext *ctx);

    CParser::StatementContext *thenS = nullptr;
    CParser::StatementContext *elseS = nullptr;
    antlr4::tree::TerminalNode *IF_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);
    antlr4::tree::TerminalNode *ELSE_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BlockStmtContext : public StatementContext {
  public:
    BlockStmtContext(StatementContext *ctx);

    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SimpleStmtWrapperContext : public StatementContext {
  public:
    SimpleStmtWrapperContext(StatementContext *ctx);

    SimpleStmtContext *simpleStmt();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  EmptyStmtContext : public StatementContext {
  public:
    EmptyStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ReturnStmtContext : public StatementContext {
  public:
    ReturnStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *RETURN_KW();
    antlr4::tree::TerminalNode *SEMI();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  StatementContext* statement();

  class  SimpleStmtContext : public antlr4::ParserRuleContext {
  public:
    SimpleStmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    SimpleStmtContext() = default;
    void copyFrom(SimpleStmtContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  ExprStmtContext : public SimpleStmtContext {
  public:
    ExprStmtContext(SimpleStmtContext *ctx);

    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LocalDeclContext : public SimpleStmtContext {
  public:
    LocalDeclContext(SimpleStmtContext *ctx);

    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *ASSIGN();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LocalArrayDeclContext : public SimpleStmtContext {
  public:
    LocalArrayDeclContext(SimpleStmtContext *ctx);

    antlr4::Token *size = nullptr;
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *ASSIGN();
    ArrayInitContext *arrayInit();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  SimpleStmtContext* simpleStmt();

  class  ExprContext : public antlr4::ParserRuleContext {
  public:
    ExprContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    ExprContext() = default;
    void copyFrom(ExprContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  ExprCondContext : public ExprContext {
  public:
    ExprCondContext(ExprContext *ctx);

    CondExprContext *condExpr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignIndexExprContext : public ExprContext {
  public:
    AssignIndexExprContext(ExprContext *ctx);

    CParser::ExprContext *idx = nullptr;
    CParser::ExprContext *val = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *ASSIGN();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignDerefExprContext : public ExprContext {
  public:
    AssignDerefExprContext(ExprContext *ctx);

    CParser::CondExprContext *ptr = nullptr;
    CParser::ExprContext *val = nullptr;
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *ASSIGN();
    CondExprContext *condExpr();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignExprContext : public ExprContext {
  public:
    AssignExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *ASSIGN();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ExprContext* expr();

  class  CondExprContext : public antlr4::ParserRuleContext {
  public:
    CondExprContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    CondExprContext() = default;
    void copyFrom(CondExprContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  CharExprContext : public CondExprContext {
  public:
    CharExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *CHARLIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AddrOfExprContext : public CondExprContext {
  public:
    AddrOfExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *AMP();
    CondExprContext *condExpr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  OrExprContext : public CondExprContext {
  public:
    OrExprContext(CondExprContext *ctx);

    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *OROR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NumExprContext : public CondExprContext {
  public:
    NumExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *NUMBER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ParenExprContext : public CondExprContext {
  public:
    ParenExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitXorExprContext : public CondExprContext {
  public:
    BitXorExprContext(CondExprContext *ctx);

    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *CARET();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ShiftExprContext : public CondExprContext {
  public:
    ShiftExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *SHL();
    antlr4::tree::TerminalNode *SHR();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IndexExprContext : public CondExprContext {
  public:
    IndexExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RBRACK();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  VarExprContext : public CondExprContext {
  public:
    VarExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *IDENT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  EqExprContext : public CondExprContext {
  public:
    EqExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *EQ();
    antlr4::tree::TerminalNode *NE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitOrExprContext : public CondExprContext {
  public:
    BitOrExprContext(CondExprContext *ctx);

    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *PIPE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  UnaryExprContext : public CondExprContext {
  public:
    UnaryExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    CondExprContext *condExpr();
    antlr4::tree::TerminalNode *MINUS();
    antlr4::tree::TerminalNode *BANG();
    antlr4::tree::TerminalNode *TILDE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AddExprContext : public CondExprContext {
  public:
    AddExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *PLUS();
    antlr4::tree::TerminalNode *MINUS();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  DerefExprContext : public CondExprContext {
  public:
    DerefExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *STAR();
    CondExprContext *condExpr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  MulExprContext : public CondExprContext {
  public:
    MulExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *SLASH();
    antlr4::tree::TerminalNode *PERCENT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CallExprContext : public CondExprContext {
  public:
    CallExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  RelExprContext : public CondExprContext {
  public:
    RelExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *LT();
    antlr4::tree::TerminalNode *LE();
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *GE();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BitAndExprContext : public CondExprContext {
  public:
    BitAndExprContext(CondExprContext *ctx);

    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *AMP();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AndExprContext : public CondExprContext {
  public:
    AndExprContext(CondExprContext *ctx);

    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    antlr4::tree::TerminalNode *ANDAND();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  CondExprContext* condExpr();
  CondExprContext* condExpr(int precedence);
  class  ArgListContext : public antlr4::ParserRuleContext {
  public:
    ArgListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArgListContext* argList();


  bool sempred(antlr4::RuleContext *_localctx, size_t ruleIndex, size_t predicateIndex) override;

  bool condExprSempred(CondExprContext *_localctx, size_t predicateIndex);

  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

