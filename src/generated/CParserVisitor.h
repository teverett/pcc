
// Generated from /Users/tom/projects/pcc/grammar/CParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "CParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by CParser.
 */
class  CParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by CParser.
   */
    virtual std::any visitProgram(CParser::ProgramContext *context) = 0;

    virtual std::any visitTypeSpec(CParser::TypeSpecContext *context) = 0;

    virtual std::any visitCharType(CParser::CharTypeContext *context) = 0;

    virtual std::any visitShortType(CParser::ShortTypeContext *context) = 0;

    virtual std::any visitIntType(CParser::IntTypeContext *context) = 0;

    virtual std::any visitLongLongType(CParser::LongLongTypeContext *context) = 0;

    virtual std::any visitLongType(CParser::LongTypeContext *context) = 0;

    virtual std::any visitSignedOrUnsignedType(CParser::SignedOrUnsignedTypeContext *context) = 0;

    virtual std::any visitTopLevelDecl(CParser::TopLevelDeclContext *context) = 0;

    virtual std::any visitExternFunctionDecl(CParser::ExternFunctionDeclContext *context) = 0;

    virtual std::any visitLocalFunctionDecl(CParser::LocalFunctionDeclContext *context) = 0;

    virtual std::any visitParamList(CParser::ParamListContext *context) = 0;

    virtual std::any visitParam(CParser::ParamContext *context) = 0;

    virtual std::any visitExternArrayDecl(CParser::ExternArrayDeclContext *context) = 0;

    virtual std::any visitExternScalarDecl(CParser::ExternScalarDeclContext *context) = 0;

    virtual std::any visitGlobalArrayDecl(CParser::GlobalArrayDeclContext *context) = 0;

    virtual std::any visitGlobalScalarDecl(CParser::GlobalScalarDeclContext *context) = 0;

    virtual std::any visitArrayInit(CParser::ArrayInitContext *context) = 0;

    virtual std::any visitNumArrayElem(CParser::NumArrayElemContext *context) = 0;

    virtual std::any visitSymArrayElem(CParser::SymArrayElemContext *context) = 0;

    virtual std::any visitBlock(CParser::BlockContext *context) = 0;

    virtual std::any visitBlockStmt(CParser::BlockStmtContext *context) = 0;

    virtual std::any visitIfStmt(CParser::IfStmtContext *context) = 0;

    virtual std::any visitWhileStmt(CParser::WhileStmtContext *context) = 0;

    virtual std::any visitForStmt(CParser::ForStmtContext *context) = 0;

    virtual std::any visitSwitchStmt(CParser::SwitchStmtContext *context) = 0;

    virtual std::any visitReturnStmt(CParser::ReturnStmtContext *context) = 0;

    virtual std::any visitBreakStmt(CParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(CParser::ContinueStmtContext *context) = 0;

    virtual std::any visitGotoStmt(CParser::GotoStmtContext *context) = 0;

    virtual std::any visitLabelStmt(CParser::LabelStmtContext *context) = 0;

    virtual std::any visitAsmStmt(CParser::AsmStmtContext *context) = 0;

    virtual std::any visitSimpleStmtWrapper(CParser::SimpleStmtWrapperContext *context) = 0;

    virtual std::any visitEmptyStmt(CParser::EmptyStmtContext *context) = 0;

    virtual std::any visitLocalArrayDecl(CParser::LocalArrayDeclContext *context) = 0;

    virtual std::any visitLocalDecl(CParser::LocalDeclContext *context) = 0;

    virtual std::any visitExprStmt(CParser::ExprStmtContext *context) = 0;

    virtual std::any visitCaseLabel(CParser::CaseLabelContext *context) = 0;

    virtual std::any visitDefaultLabel(CParser::DefaultLabelContext *context) = 0;

    virtual std::any visitAssignOp(CParser::AssignOpContext *context) = 0;

    virtual std::any visitAssignDerefExpr(CParser::AssignDerefExprContext *context) = 0;

    virtual std::any visitAssignIndexExpr(CParser::AssignIndexExprContext *context) = 0;

    virtual std::any visitAssignExpr(CParser::AssignExprContext *context) = 0;

    virtual std::any visitExprCond(CParser::ExprCondContext *context) = 0;

    virtual std::any visitSizeofExprExpr(CParser::SizeofExprExprContext *context) = 0;

    virtual std::any visitPreIncDecDerefExpr(CParser::PreIncDecDerefExprContext *context) = 0;

    virtual std::any visitCastExpr(CParser::CastExprContext *context) = 0;

    virtual std::any visitPostIncDecVarExpr(CParser::PostIncDecVarExprContext *context) = 0;

    virtual std::any visitStringLitExpr(CParser::StringLitExprContext *context) = 0;

    virtual std::any visitPreIncDecIndexExpr(CParser::PreIncDecIndexExprContext *context) = 0;

    virtual std::any visitParenExpr(CParser::ParenExprContext *context) = 0;

    virtual std::any visitBitXorExpr(CParser::BitXorExprContext *context) = 0;

    virtual std::any visitPostIncDecDerefExpr(CParser::PostIncDecDerefExprContext *context) = 0;

    virtual std::any visitUnaryExpr(CParser::UnaryExprContext *context) = 0;

    virtual std::any visitPostIncDecIndexExpr(CParser::PostIncDecIndexExprContext *context) = 0;

    virtual std::any visitTernaryExpr(CParser::TernaryExprContext *context) = 0;

    virtual std::any visitMulExpr(CParser::MulExprContext *context) = 0;

    virtual std::any visitCallExpr(CParser::CallExprContext *context) = 0;

    virtual std::any visitRelExpr(CParser::RelExprContext *context) = 0;

    virtual std::any visitBitAndExpr(CParser::BitAndExprContext *context) = 0;

    virtual std::any visitCharExpr(CParser::CharExprContext *context) = 0;

    virtual std::any visitSizeofTypeExpr(CParser::SizeofTypeExprContext *context) = 0;

    virtual std::any visitAddrOfExpr(CParser::AddrOfExprContext *context) = 0;

    virtual std::any visitOrExpr(CParser::OrExprContext *context) = 0;

    virtual std::any visitCallPtrExpr(CParser::CallPtrExprContext *context) = 0;

    virtual std::any visitNumExpr(CParser::NumExprContext *context) = 0;

    virtual std::any visitShiftExpr(CParser::ShiftExprContext *context) = 0;

    virtual std::any visitIndexExpr(CParser::IndexExprContext *context) = 0;

    virtual std::any visitPreIncDecVarExpr(CParser::PreIncDecVarExprContext *context) = 0;

    virtual std::any visitVarExpr(CParser::VarExprContext *context) = 0;

    virtual std::any visitEqExpr(CParser::EqExprContext *context) = 0;

    virtual std::any visitBitOrExpr(CParser::BitOrExprContext *context) = 0;

    virtual std::any visitAddExpr(CParser::AddExprContext *context) = 0;

    virtual std::any visitDerefExpr(CParser::DerefExprContext *context) = 0;

    virtual std::any visitAndExpr(CParser::AndExprContext *context) = 0;

    virtual std::any visitArgList(CParser::ArgListContext *context) = 0;


};

