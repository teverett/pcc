
// Generated from /Users/tom/projects/pcc/grammar/CParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"




class  CParser : public antlr4::Parser {
public:
  enum {
    INT_KW = 1, CHAR_KW = 2, SHORT_KW = 3, LONG_KW = 4, SIGNED_KW = 5, UNSIGNED_KW = 6, 
    SIZEOF_KW = 7, IF_KW = 8, ELSE_KW = 9, WHILE_KW = 10, FOR_KW = 11, SWITCH_KW = 12, 
    CASE_KW = 13, DEFAULT_KW = 14, RETURN_KW = 15, BREAK_KW = 16, CONTINUE_KW = 17, 
    GOTO_KW = 18, EXTERN_KW = 19, LPAREN = 20, RPAREN = 21, LBRACK = 22, 
    RBRACK = 23, LBRACE = 24, RBRACE = 25, AT = 26, SEMI = 27, COMMA = 28, 
    ASSIGN = 29, STAR = 30, AMP = 31, MINUS = 32, BANG = 33, TILDE = 34, 
    SLASH = 35, PERCENT = 36, PLUS = 37, SHL = 38, SHR = 39, LE = 40, GE = 41, 
    LT = 42, GT = 43, EQ = 44, NE = 45, CARET = 46, PIPE = 47, ANDAND = 48, 
    OROR = 49, PLUSEQ = 50, MINUSEQ = 51, STAREQ = 52, SLASHEQ = 53, PERCENTEQ = 54, 
    AMPEQ = 55, CARETEQ = 56, PIPEEQ = 57, SHLEQ = 58, SHREQ = 59, INC = 60, 
    DEC = 61, QUESTION = 62, COLON = 63, IDENT = 64, NUMBER = 65, CHARLIT = 66, 
    STRINGLIT = 67, LINE_COMMENT = 68, BLOCK_COMMENT = 69, WS = 70, ASM_BEGIN = 71, 
    ASM_TEXT = 72, ASM_END = 73
  };

  enum {
    RuleProgram = 0, RuleTypeSpec = 1, RuleBaseTypeSpec = 2, RuleTopLevelDecl = 3, 
    RuleFunctionDecl = 4, RuleParamList = 5, RuleParam = 6, RuleGlobalDecl = 7, 
    RuleArrayInit = 8, RuleArrayInitElem = 9, RuleBlock = 10, RuleStatement = 11, 
    RuleSimpleStmt = 12, RuleSwitchCase = 13, RuleAssignOp = 14, RuleExpr = 15, 
    RuleCondExpr = 16, RuleArgList = 17
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
  class TypeSpecContext;
  class BaseTypeSpecContext;
  class TopLevelDeclContext;
  class FunctionDeclContext;
  class ParamListContext;
  class ParamContext;
  class GlobalDeclContext;
  class ArrayInitContext;
  class ArrayInitElemContext;
  class BlockContext;
  class StatementContext;
  class SimpleStmtContext;
  class SwitchCaseContext;
  class AssignOpContext;
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

  class  TypeSpecContext : public antlr4::ParserRuleContext {
  public:
    TypeSpecContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    BaseTypeSpecContext *baseTypeSpec();
    std::vector<antlr4::tree::TerminalNode *> STAR();
    antlr4::tree::TerminalNode* STAR(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  TypeSpecContext* typeSpec();

  class  BaseTypeSpecContext : public antlr4::ParserRuleContext {
  public:
    BaseTypeSpecContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    BaseTypeSpecContext() = default;
    void copyFrom(BaseTypeSpecContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  CharTypeContext : public BaseTypeSpecContext {
  public:
    CharTypeContext(BaseTypeSpecContext *ctx);

    antlr4::tree::TerminalNode *CHAR_KW();
    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SignedOrUnsignedTypeContext : public BaseTypeSpecContext {
  public:
    SignedOrUnsignedTypeContext(BaseTypeSpecContext *ctx);

    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  IntTypeContext : public BaseTypeSpecContext {
  public:
    IntTypeContext(BaseTypeSpecContext *ctx);

    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ShortTypeContext : public BaseTypeSpecContext {
  public:
    ShortTypeContext(BaseTypeSpecContext *ctx);

    antlr4::tree::TerminalNode *SHORT_KW();
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LongLongTypeContext : public BaseTypeSpecContext {
  public:
    LongLongTypeContext(BaseTypeSpecContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> LONG_KW();
    antlr4::tree::TerminalNode* LONG_KW(size_t i);
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LongTypeContext : public BaseTypeSpecContext {
  public:
    LongTypeContext(BaseTypeSpecContext *ctx);

    antlr4::tree::TerminalNode *LONG_KW();
    antlr4::tree::TerminalNode *INT_KW();
    antlr4::tree::TerminalNode *SIGNED_KW();
    antlr4::tree::TerminalNode *UNSIGNED_KW();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  BaseTypeSpecContext* baseTypeSpec();

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
    FunctionDeclContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    FunctionDeclContext() = default;
    void copyFrom(FunctionDeclContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  ExternFunctionDeclContext : public FunctionDeclContext {
  public:
    ExternFunctionDeclContext(FunctionDeclContext *ctx);

    antlr4::tree::TerminalNode *EXTERN_KW();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LPAREN();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *SEMI();
    ParamListContext *paramList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LocalFunctionDeclContext : public FunctionDeclContext {
  public:
    LocalFunctionDeclContext(FunctionDeclContext *ctx);

    antlr4::Token *addr = nullptr;
    TypeSpecContext *typeSpec();
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
    TypeSpecContext *typeSpec();
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

  class  ExternScalarDeclContext : public GlobalDeclContext {
  public:
    ExternScalarDeclContext(GlobalDeclContext *ctx);

    antlr4::tree::TerminalNode *EXTERN_KW();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GlobalArrayDeclContext : public GlobalDeclContext {
  public:
    GlobalArrayDeclContext(GlobalDeclContext *ctx);

    antlr4::Token *size = nullptr;
    antlr4::Token *addr = nullptr;
    TypeSpecContext *typeSpec();
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

  class  ExternArrayDeclContext : public GlobalDeclContext {
  public:
    ExternArrayDeclContext(GlobalDeclContext *ctx);

    antlr4::Token *size = nullptr;
    antlr4::tree::TerminalNode *EXTERN_KW();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *SEMI();
    antlr4::tree::TerminalNode *NUMBER();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GlobalScalarDeclContext : public GlobalDeclContext {
  public:
    GlobalScalarDeclContext(GlobalDeclContext *ctx);

    antlr4::Token *sign = nullptr;
    antlr4::Token *val = nullptr;
    antlr4::Token *addr = nullptr;
    TypeSpecContext *typeSpec();
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
    CParser::ArrayInitElemContext *arrayInitElemContext = nullptr;
    std::vector<ArrayInitElemContext *> elem;
    ArrayInitContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<ArrayInitElemContext *> arrayInitElem();
    ArrayInitElemContext* arrayInitElem(size_t i);
    std::vector<antlr4::tree::TerminalNode *> COMMA();
    antlr4::tree::TerminalNode* COMMA(size_t i);


    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
   
  };

  ArrayInitContext* arrayInit();

  class  ArrayInitElemContext : public antlr4::ParserRuleContext {
  public:
    ArrayInitElemContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    ArrayInitElemContext() = default;
    void copyFrom(ArrayInitElemContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  SymArrayElemContext : public ArrayInitElemContext {
  public:
    SymArrayElemContext(ArrayInitElemContext *ctx);

    antlr4::tree::TerminalNode *IDENT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NumArrayElemContext : public ArrayInitElemContext {
  public:
    NumArrayElemContext(ArrayInitElemContext *ctx);

    antlr4::Token *sign = nullptr;
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *MINUS();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  ArrayInitElemContext* arrayInitElem();

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

  class  SimpleStmtWrapperContext : public StatementContext {
  public:
    SimpleStmtWrapperContext(StatementContext *ctx);

    SimpleStmtContext *simpleStmt();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  GotoStmtContext : public StatementContext {
  public:
    GotoStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *GOTO_KW();
    antlr4::tree::TerminalNode *IDENT();
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

  class  SwitchStmtContext : public StatementContext {
  public:
    SwitchStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *SWITCH_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();
    antlr4::tree::TerminalNode *LBRACE();
    antlr4::tree::TerminalNode *RBRACE();
    std::vector<SwitchCaseContext *> switchCase();
    SwitchCaseContext* switchCase(size_t i);

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

  class  BreakStmtContext : public StatementContext {
  public:
    BreakStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *BREAK_KW();
    antlr4::tree::TerminalNode *SEMI();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LabelStmtContext : public StatementContext {
  public:
    LabelStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *COLON();
    StatementContext *statement();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  ContinueStmtContext : public StatementContext {
  public:
    ContinueStmtContext(StatementContext *ctx);

    antlr4::tree::TerminalNode *CONTINUE_KW();
    antlr4::tree::TerminalNode *SEMI();

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

    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *ASSIGN();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  LocalArrayDeclContext : public SimpleStmtContext {
  public:
    LocalArrayDeclContext(SimpleStmtContext *ctx);

    antlr4::Token *size = nullptr;
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    antlr4::tree::TerminalNode *NUMBER();
    antlr4::tree::TerminalNode *ASSIGN();
    ArrayInitContext *arrayInit();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  SimpleStmtContext* simpleStmt();

  class  SwitchCaseContext : public antlr4::ParserRuleContext {
  public:
    SwitchCaseContext(antlr4::ParserRuleContext *parent, size_t invokingState);
   
    SwitchCaseContext() = default;
    void copyFrom(SwitchCaseContext *context);
    using antlr4::ParserRuleContext::copyFrom;

    virtual size_t getRuleIndex() const override;

   
  };

  class  DefaultLabelContext : public SwitchCaseContext {
  public:
    DefaultLabelContext(SwitchCaseContext *ctx);

    antlr4::tree::TerminalNode *DEFAULT_KW();
    antlr4::tree::TerminalNode *COLON();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CaseLabelContext : public SwitchCaseContext {
  public:
    CaseLabelContext(SwitchCaseContext *ctx);

    antlr4::Token *sign = nullptr;
    antlr4::Token *val = nullptr;
    antlr4::tree::TerminalNode *CASE_KW();
    antlr4::tree::TerminalNode *COLON();
    antlr4::tree::TerminalNode *NUMBER();
    std::vector<StatementContext *> statement();
    StatementContext* statement(size_t i);
    antlr4::tree::TerminalNode *MINUS();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  SwitchCaseContext* switchCase();

  class  AssignOpContext : public antlr4::ParserRuleContext {
  public:
    AssignOpContext(antlr4::ParserRuleContext *parent, size_t invokingState);
    virtual size_t getRuleIndex() const override;
    antlr4::tree::TerminalNode *ASSIGN();
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

  AssignOpContext* assignOp();

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
    CParser::AssignOpContext *op = nullptr;
    CParser::ExprContext *val = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    std::vector<ExprContext *> expr();
    ExprContext* expr(size_t i);
    AssignOpContext *assignOp();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignDerefExprContext : public ExprContext {
  public:
    AssignDerefExprContext(ExprContext *ctx);

    CParser::CondExprContext *ptr = nullptr;
    CParser::AssignOpContext *op = nullptr;
    CParser::ExprContext *val = nullptr;
    antlr4::tree::TerminalNode *STAR();
    CondExprContext *condExpr();
    AssignOpContext *assignOp();
    ExprContext *expr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  AssignExprContext : public ExprContext {
  public:
    AssignExprContext(ExprContext *ctx);

    CParser::AssignOpContext *op = nullptr;
    CParser::ExprContext *val = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    AssignOpContext *assignOp();
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

  class  SizeofExprExprContext : public CondExprContext {
  public:
    SizeofExprExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *SIZEOF_KW();
    antlr4::tree::TerminalNode *LPAREN();
    ExprContext *expr();
    antlr4::tree::TerminalNode *RPAREN();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PreIncDecDerefExprContext : public CondExprContext {
  public:
    PreIncDecDerefExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    CParser::CondExprContext *ptr = nullptr;
    antlr4::tree::TerminalNode *STAR();
    CondExprContext *condExpr();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  CastExprContext : public CondExprContext {
  public:
    CastExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *LPAREN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *RPAREN();
    CondExprContext *condExpr();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PostIncDecVarExprContext : public CondExprContext {
  public:
    PostIncDecVarExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  StringLitExprContext : public CondExprContext {
  public:
    StringLitExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *STRINGLIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  PreIncDecIndexExprContext : public CondExprContext {
  public:
    PreIncDecIndexExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    CParser::ExprContext *idx = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    ExprContext *expr();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

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

  class  PostIncDecDerefExprContext : public CondExprContext {
  public:
    PostIncDecDerefExprContext(CondExprContext *ctx);

    CParser::CondExprContext *ptr = nullptr;
    antlr4::Token *op = nullptr;
    antlr4::tree::TerminalNode *STAR();
    CondExprContext *condExpr();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

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

  class  PostIncDecIndexExprContext : public CondExprContext {
  public:
    PostIncDecIndexExprContext(CondExprContext *ctx);

    CParser::ExprContext *idx = nullptr;
    antlr4::Token *op = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *LBRACK();
    antlr4::tree::TerminalNode *RBRACK();
    ExprContext *expr();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  TernaryExprContext : public CondExprContext {
  public:
    TernaryExprContext(CondExprContext *ctx);

    CParser::CondExprContext *cond = nullptr;
    CParser::ExprContext *then = nullptr;
    CParser::CondExprContext *els = nullptr;
    antlr4::tree::TerminalNode *QUESTION();
    antlr4::tree::TerminalNode *COLON();
    std::vector<CondExprContext *> condExpr();
    CondExprContext* condExpr(size_t i);
    ExprContext *expr();

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

  class  CharExprContext : public CondExprContext {
  public:
    CharExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *CHARLIT();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  SizeofTypeExprContext : public CondExprContext {
  public:
    SizeofTypeExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *SIZEOF_KW();
    antlr4::tree::TerminalNode *LPAREN();
    TypeSpecContext *typeSpec();
    antlr4::tree::TerminalNode *RPAREN();

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

  class  CallPtrExprContext : public CondExprContext {
  public:
    CallPtrExprContext(CondExprContext *ctx);

    std::vector<antlr4::tree::TerminalNode *> LPAREN();
    antlr4::tree::TerminalNode* LPAREN(size_t i);
    ExprContext *expr();
    std::vector<antlr4::tree::TerminalNode *> RPAREN();
    antlr4::tree::TerminalNode* RPAREN(size_t i);
    ArgListContext *argList();

    virtual std::any accept(antlr4::tree::ParseTreeVisitor *visitor) override;
  };

  class  NumExprContext : public CondExprContext {
  public:
    NumExprContext(CondExprContext *ctx);

    antlr4::tree::TerminalNode *NUMBER();

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

  class  PreIncDecVarExprContext : public CondExprContext {
  public:
    PreIncDecVarExprContext(CondExprContext *ctx);

    antlr4::Token *op = nullptr;
    antlr4::tree::TerminalNode *IDENT();
    antlr4::tree::TerminalNode *INC();
    antlr4::tree::TerminalNode *DEC();

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

