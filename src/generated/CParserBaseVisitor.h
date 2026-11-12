
// Generated from /Users/tom/projects/pcc/grammar/CParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "CParserVisitor.h"


/**
 * This class provides an empty implementation of CParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  CParserBaseVisitor : public CParserVisitor {
public:

  virtual std::any visitProgram(CParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeSpec(CParser::TypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCharType(CParser::CharTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShortType(CParser::ShortTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIntType(CParser::IntTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLongLongType(CParser::LongLongTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLongType(CParser::LongTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSignedOrUnsignedType(CParser::SignedOrUnsignedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructType(CParser::StructTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTopLevelDecl(CParser::TopLevelDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDecl(CParser::StructDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructMember(CParser::StructMemberContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternFunctionDecl(CParser::ExternFunctionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalFunctionDecl(CParser::LocalFunctionDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnTypeSpec(CParser::ReturnTypeSpecContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParamList(CParser::ParamListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParam(CParser::ParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternArrayDecl(CParser::ExternArrayDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExternScalarDecl(CParser::ExternScalarDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGlobalArrayDecl(CParser::GlobalArrayDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGlobalScalarDecl(CParser::GlobalScalarDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayInit(CParser::ArrayInitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNumArrayElem(CParser::NumArrayElemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSymArrayElem(CParser::SymArrayElemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(CParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockStmt(CParser::BlockStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStmt(CParser::IfStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStmt(CParser::WhileStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForStmt(CParser::ForStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchStmt(CParser::SwitchStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStmt(CParser::ReturnStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStmt(CParser::BreakStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStmt(CParser::ContinueStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGotoStmt(CParser::GotoStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLabelStmt(CParser::LabelStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAsmStmt(CParser::AsmStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSimpleStmtWrapper(CParser::SimpleStmtWrapperContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEmptyStmt(CParser::EmptyStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalArrayDecl(CParser::LocalArrayDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLocalDecl(CParser::LocalDeclContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprStmt(CParser::ExprStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCaseLabel(CParser::CaseLabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDefaultLabel(CParser::DefaultLabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignOp(CParser::AssignOpContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignDerefExpr(CParser::AssignDerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignIndexExpr(CParser::AssignIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignExpr(CParser::AssignExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignMemberExpr(CParser::AssignMemberExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignMemberPtrExpr(CParser::AssignMemberPtrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExprCond(CParser::ExprCondContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSizeofExprExpr(CParser::SizeofExprExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreIncDecDerefExpr(CParser::PreIncDecDerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpr(CParser::CastExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostIncDecVarExpr(CParser::PostIncDecVarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStringLitExpr(CParser::StringLitExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreIncDecIndexExpr(CParser::PreIncDecIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParenExpr(CParser::ParenExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitXorExpr(CParser::BitXorExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostIncDecDerefExpr(CParser::PostIncDecDerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnaryExpr(CParser::UnaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostIncDecIndexExpr(CParser::PostIncDecIndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTernaryExpr(CParser::TernaryExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMulExpr(CParser::MulExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberAccessExpr(CParser::MemberAccessExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallExpr(CParser::CallExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRelExpr(CParser::RelExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitAndExpr(CParser::BitAndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCharExpr(CParser::CharExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSizeofTypeExpr(CParser::SizeofTypeExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddrOfExpr(CParser::AddrOfExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOrExpr(CParser::OrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberAccessPtrExpr(CParser::MemberAccessPtrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallPtrExpr(CParser::CallPtrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNumExpr(CParser::NumExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitShiftExpr(CParser::ShiftExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIndexExpr(CParser::IndexExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPreIncDecVarExpr(CParser::PreIncDecVarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVarExpr(CParser::VarExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEqExpr(CParser::EqExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBitOrExpr(CParser::BitOrExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddExpr(CParser::AddExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDerefExpr(CParser::DerefExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAndExpr(CParser::AndExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgList(CParser::ArgListContext *ctx) override {
    return visitChildren(ctx);
  }


};

