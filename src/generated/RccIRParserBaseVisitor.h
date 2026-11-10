
// Generated from /Users/tom/projects/pcc/grammar/RccIRParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "RccIRParserVisitor.h"


/**
 * This class provides an empty implementation of RccIRParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  RccIRParserBaseVisitor : public RccIRParserVisitor {
public:

  virtual std::any visitProgram(RccIRParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpec(RccIRParser::TypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGlobalDecl(RccIRParser::GlobalDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitList(RccIRParser::InitListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInitElem(RccIRParser::InitElemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFuncDecl(RccIRParser::FuncDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpecOrVoid(RccIRParser::TypeSpecOrVoidContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(RccIRParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(RccIRParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(RccIRParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockStmt(RccIRParser::BlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt(RccIRParser::IfStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(RccIRParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(RccIRParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(RccIRParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(RccIRParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalDeclStmt(RccIRParser::LocalDeclStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAsmStmt(RccIRParser::AsmStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEmptyStmt(RccIRParser::EmptyStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLabelStmt(RccIRParser::LabelStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGotoStmt(RccIRParser::GotoStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(RccIRParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRef(RccIRParser::RefContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundOp(RccIRParser::CompoundOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntLitExpr(RccIRParser::IntLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundIndexExpr(RccIRParser::CompoundIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundAssignExpr(RccIRParser::CompoundAssignExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompoundDerefExpr(RccIRParser::CompoundDerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(RccIRParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignExpr(RccIRParser::AssignExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStarExpr(RccIRParser::StarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryExpr(RccIRParser::UnaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinaryExpr(RccIRParser::BinaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTernaryExpr(RccIRParser::TernaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConvertExpr(RccIRParser::ConvertExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallExpr(RccIRParser::CallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallPtrExpr(RccIRParser::CallPtrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntrinsicExpr(RccIRParser::IntrinsicExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarExpr(RccIRParser::VarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(RccIRParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinOp(RccIRParser::BinOpContext *ctx) override {
    return visitChildren(ctx);
  }


};

