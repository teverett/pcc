
// Generated from /Users/tom/projects/pcc/grammar/RccIRParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  RccIRParser : public antlr4::Parser {
public:
  enum {
    GLOBALS_HEADER = 1, FUNCTIONS_HEADER = 2, INT_KW = 3, CHAR_KW = 4, SHORT_KW = 5, 
    LONG_KW = 6, UNSIGNED_KW = 7, VOID_KW = 8, STRUCT_KW = 9, LOCAL_KW = 10, 
    IF_KW = 11, ELSE_KW = 12, WHILE_KW = 13, RETURN_KW = 14, BREAK_KW = 15, 
    CONTINUE_KW = 16, GOTO_KW = 17, POST_KW = 18, OLDVAL_KW = 19, LPAREN = 20, 
    RPAREN = 21, LBRACK = 22, RBRACK = 23, LBRACE = 24, RBRACE = 25, SEMI = 26, 
    COMMA = 27, ASSIGN = 28, HASH = 29, STAR = 30, AMP = 31, MINUS = 32, 
    BANG = 33, TILDE = 34, SLASH = 35, PERCENT = 36, PLUS = 37, SHL = 38, 
    SHR = 39, LE = 40, GE = 41, LT = 42, GT = 43, EQ = 44, NE = 45, CARET = 46, 
    PIPE = 47, ANDAND = 48, OROR = 49, PLUSEQ = 50, MINUSEQ = 51, STAREQ = 52, 
    SLASHEQ = 53, PERCENTEQ = 54, AMPEQ = 55, CARETEQ = 56, PIPEEQ = 57, 
    SHLEQ = 58, SHREQ = 59, QUESTION = 60, COLON = 61, IDENT = 62, NUMBER = 63, 
    WS = 64, ASM_BEGIN = 65, ASM_TEXT = 66, ASM_END = 67
  };

  enum {
    RuleProgram = 0, RuleTypeSpec = 1, RuleGlobalDecl = 2, RuleInitList = 3, 
    RuleInitElem = 4, RuleFuncDecl = 5, RuleTypeSpecOrVoid = 6, RuleParamList = 7, 
    RuleParam = 8, RuleBlock = 9, RuleStmt = 10, RuleRef = 11, RuleCompoundOp = 12, 
    RuleExpr = 13, RuleArgList = 14, RuleBinOp = 15
  };

  explicit RccIRParser(antlr4::TokenStream *input);

  RccIRParser(antlr4::TokenStream *input, const antlr4::atn::ParserATNSimulatorOptions &options);

  ~RccIRParser() override;

  std::string getGrammarFileName() const override;

  const antlr4::atn::ATN& getATN() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;


  class ProgramContext;
  class TypeSpecContext;
  class GlobalDeclContext;
  class InitListContext;
  class InitElemContext;
  class FuncDeclContext;
  class TypeSpecOrVoidContext;
  class ParamListContext;
  class ParamContext;
  class BlockContext;
  class StmtContext;
  class RefContext;
  class CompoundOpContext;
  class ExprContext;
  class ArgListContext;
  class BinOpContext; 

  class  ProgramContext : public antlr4::ParserRuleContext {
  public:
    ProgramContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *GLOBALS_HEADER();
    antlr4::tree::TerminalNode *FUNCTIONS_HEADER();
    antlr4::tree::TerminalNode *EOF();
    std::vector<GlobalDeclContext *> globalDecl();
    GlobalDeclContext* globalDecl(size_t i);
    std::vector<FuncDeclContext *> funcDecl();
    FuncDeclContext* funcDecl(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ProgramContext* program();

  class  TypeSpecContext : public antlr4::ParserRuleContext {
  public:
    TypeSpecContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *CHAR_KW();
    antlr4::tree::TerminalNode *SHORT_KW();
    antlr4::tree::TerminalNode *INT_KW();
    std::vector<antlr4::tree::TerminalNode *> LONG_KW();
    antlr4::tree::TerminalNode* LONG_KW(size_t i);
    antlr4::tree::TerminalNode *STRUCT_KW();
    antlr4::tree::TerminalNode *IDENT();
    std::vector<antlr4::tree::TerminalNode *> STAR();
    antlr4::tree::TerminalNode* STAR(size_t i);
    antlr4::tree::TerminalNode *UNSIGNED_KW();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeSpecContext* typeSpec();

  class  GlobalDeclContext : public antlr4::ParserRuleContext {
  public:
    GlobalDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    RefContext *ref();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *ASSIGN();
    InitListContext *initList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  GlobalDeclContext* globalDecl();

  class  InitListContext : public antlr4::ParserRuleContext {
  public:
    InitListContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<InitElemContext *> initElem();
    InitElemContext* initElem(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  InitListContext* initList();

  class  InitElemContext : public antlr4::ParserRuleContext {
  public:
    InitElemContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *NUMBER();
    RefContext *ref();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  InitElemContext* initElem();

  class  FuncDeclContext : public antlr4::ParserRuleContext {
  public:
    FuncDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecOrVoidContext *typeSpecOrVoid();
    RefContext *ref();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    BlockContext *block();
    ParamListContext *paramList();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  FuncDeclContext* funcDecl();

  class  TypeSpecOrVoidContext : public antlr4::ParserRuleContext {
  public:
    TypeSpecOrVoidContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *VOID_KW();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeSpecOrVoidContext* typeSpecOrVoid();

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
    TypeSpecContext *typeSpec();
    RefContext *ref();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ParamContext* param();

  class  BlockContext : public antlr4::ParserRuleContext {
  public:
    BlockContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<StmtContext *> stmt();
    StmtContext* stmt(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  BlockContext* block();

  class  StmtContext : public antlr4::ParserRuleContext {
  public:
    StmtContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    StmtContext() = default;
    void copyFrom(StmtContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  LocalDeclStmtContext : public StmtContext {
  public:
    LocalDeclStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *LOCAL_KW();
    TypeSpecContext *typeSpec();
    RefContext *ref();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *ASSIGN();
    InitListContext *initList();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AsmStmtContext : public StmtContext {
  public:
    AsmStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *ASM_BEGIN();
    antlr4::tree::TerminalNode *ASM_TEXT();
    antlr4::tree::TerminalNode *ASM_END();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ExprStmtContext : public StmtContext {
  public:
    ExprStmtContext(StmtContext *ctx);

    ExprContext *expr();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  WhileStmtContext : public StmtContext {
  public:
    WhileStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *WHILE_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<StmtContext *> stmt();
    StmtContext* stmt(size_t i);
    antlr4::tree::TerminalNode *POST_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IfStmtContext : public StmtContext {
  public:
    IfStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *IF_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    std::vector<StmtContext *> stmt();
    StmtContext* stmt(size_t i);
    antlr4::tree::TerminalNode *ELSE_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BlockStmtContext : public StmtContext {
  public:
    BlockStmtContext(StmtContext *ctx);

    BlockContext *block();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BreakStmtContext : public StmtContext {
  public:
    BreakStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *BREAK_KW();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  EmptyStmtContext : public StmtContext {
  public:
    EmptyStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GotoStmtContext : public StmtContext {
  public:
    GotoStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *GOTO_KW();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LabelStmtContext : public StmtContext {
  public:
    LabelStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *COLON();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ReturnStmtContext : public StmtContext {
  public:
    ReturnStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *RETURN_KW();
    antlr4::tree::TerminalNode *SEMI();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ContinueStmtContext : public StmtContext {
  public:
    ContinueStmtContext(StmtContext *ctx);

    antlr4::tree::TerminalNode *CONTINUE_KW();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  StmtContext* stmt();

  class  RefContext : public antlr4::ParserRuleContext {
  public:
    RefContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *HASH();
    antlr4::tree::TerminalNode *NUMBER();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  RefContext* ref();

  class  CompoundOpContext : public antlr4::ParserRuleContext {
  public:
    CompoundOpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *PLUSEQ();
    antlr4::tree::TerminalNode *MINUSEQ();
    antlr4::tree::TerminalNode *STAREQ();
    antlr4::tree::TerminalNode *SLASHEQ();
    antlr4::tree::TerminalNode *PERCENTEQ();
    antlr4::tree::TerminalNode *AMPEQ();
    antlr4::tree::TerminalNode *CARETEQ();
    antlr4::tree::TerminalNode *PIPEEQ();
    antlr4::tree::TerminalNode *SHLEQ();
    antlr4::tree::TerminalNode *SHREQ();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  CompoundOpContext* compoundOp();

  class  ExprContext : public antlr4::ParserRuleContext {
  public:
    ExprContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    ExprContext() = default;
    void copyFrom(ExprContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  CompoundDerefExprContext : public ExprContext {
  public:
    CompoundDerefExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    CompoundOpContext *compoundOp();
    antlr4::tree::TerminalNode *OLDVAL_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  StarExprContext : public ExprContext {
  public:
    StarExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *LPAREN();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *ASSIGN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CompoundAssignExprContext : public ExprContext {
  public:
    CompoundAssignExprContext(ExprContext *ctx);

    RefContext *ref();
    CompoundOpContext *compoundOp();
    ExprContext *expr();
    antlr4::tree::TerminalNode *OLDVAL_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IntLitExprContext : public ExprContext {
  public:
    IntLitExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *NUMBER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  BinaryExprContext : public ExprContext {
  public:
    BinaryExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    BinOpContext *binOp();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CallPtrExprContext : public ExprContext {
  public:
    CallPtrExprContext(ExprContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> LPAREN();
    antlr4::tree::TerminalNode* LPAREN(size_t i);
    ExprContext *expr();
    std::vector<antlr4::tree::TerminalNode *> RPAREN();
    antlr4::tree::TerminalNode* RPAREN(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IndexExprContext : public ExprContext {
  public:
    IndexExprContext(ExprContext *ctx);

    RefContext *ref();
    antlr4::tree::TerminalNode *LBRACK();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *ASSIGN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  VarExprContext : public ExprContext {
  public:
    VarExprContext(ExprContext *ctx);

    RefContext *ref();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IntrinsicExprContext : public ExprContext {
  public:
    IntrinsicExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  UnaryExprContext : public ExprContext {
  public:
    UnaryExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *MINUS();
    antlr4::tree::TerminalNode *BANG();
    antlr4::tree::TerminalNode *TILDE();
    antlr4::tree::TerminalNode *AMP();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TernaryExprContext : public ExprContext {
  public:
    TernaryExprContext(ExprContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    antlr4::tree::TerminalNode *QUESTION();
    antlr4::tree::TerminalNode *COLON();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ConvertExprContext : public ExprContext {
  public:
    ConvertExprContext(ExprContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> LPAREN();
    antlr4::tree::TerminalNode* LPAREN(size_t i);
    TypeSpecContext *typeSpec();
    std::vector<antlr4::tree::TerminalNode *> RPAREN();
    antlr4::tree::TerminalNode* RPAREN(size_t i);
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CallExprContext : public ExprContext {
  public:
    CallExprContext(ExprContext *ctx);

    RefContext *ref();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignExprContext : public ExprContext {
  public:
    AssignExprContext(ExprContext *ctx);

    RefContext *ref();
    antlr4::tree::TerminalNode *ASSIGN();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CompoundIndexExprContext : public ExprContext {
  public:
    CompoundIndexExprContext(ExprContext *ctx);

    RefContext *ref();
    antlr4::tree::TerminalNode *LBRACK();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    antlr4::tree::TerminalNode *RBRACK();
    CompoundOpContext *compoundOp();
    antlr4::tree::TerminalNode *OLDVAL_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ExprContext* expr();

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

  class  BinOpContext : public antlr4::ParserRuleContext {
  public:
    BinOpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *PLUS();
    antlr4::tree::TerminalNode *MINUS();
    antlr4::tree::TerminalNode *STAR();
    antlr4::tree::TerminalNode *SLASH();
    antlr4::tree::TerminalNode *PERCENT();
    antlr4::tree::TerminalNode *SHL();
    antlr4::tree::TerminalNode *SHR();
    antlr4::tree::TerminalNode *EQ();
    antlr4::tree::TerminalNode *NE();
    antlr4::tree::TerminalNode *LT();
    antlr4::tree::TerminalNode *LE();
    antlr4::tree::TerminalNode *GT();
    antlr4::tree::TerminalNode *GE();
    antlr4::tree::TerminalNode *AMP();
    antlr4::tree::TerminalNode *CARET();
    antlr4::tree::TerminalNode *PIPE();
    antlr4::tree::TerminalNode *ANDAND();
    antlr4::tree::TerminalNode *OROR();


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  BinOpContext* binOp();


  // By default the static state used to implement the parser is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:
};

