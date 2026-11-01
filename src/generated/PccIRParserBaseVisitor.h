
// Generated from /Users/tom/projects/pcc/grammar/PccIRParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "PccIRParserVisitor.h"


/**
 * This class provides an empty implementation of PccIRParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  PccIRParserBaseVisitor : public PccIRParserVisitor {
public:

  virtual std::any visitProgram(PccIRParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGlobalDecl(PccIRParser::GlobalDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitList(PccIRParser::InitListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncDecl(PccIRParser::FuncDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(PccIRParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(PccIRParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(PccIRParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockStmt(PccIRParser::BlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt(PccIRParser::IfStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(PccIRParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(PccIRParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalDeclStmt(PccIRParser::LocalDeclStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAsmStmt(PccIRParser::AsmStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEmptyStmt(PccIRParser::EmptyStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(PccIRParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRef(PccIRParser::RefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntLitExpr(PccIRParser::IntLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(PccIRParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignExpr(PccIRParser::AssignExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStarExpr(PccIRParser::StarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryExpr(PccIRParser::UnaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinaryExpr(PccIRParser::BinaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallExpr(PccIRParser::CallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntrinsicExpr(PccIRParser::IntrinsicExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarExpr(PccIRParser::VarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(PccIRParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinOp(PccIRParser::BinOpContext *ctx) override {
    return visitChildren(ctx);
  }


};

