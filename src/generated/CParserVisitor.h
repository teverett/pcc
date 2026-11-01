
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

    virtual std::any visitTopLevelDecl(CParser::TopLevelDeclContext *context) = 0;

    virtual std::any visitFunctionDecl(CParser::FunctionDeclContext *context) = 0;

    virtual std::any visitParamList(CParser::ParamListContext *context) = 0;

    virtual std::any visitParam(CParser::ParamContext *context) = 0;

    virtual std::any visitGlobalArrayDecl(CParser::GlobalArrayDeclContext *context) = 0;

    virtual std::any visitGlobalScalarDecl(CParser::GlobalScalarDeclContext *context) = 0;

    virtual std::any visitArrayInit(CParser::ArrayInitContext *context) = 0;

    virtual std::any visitBlock(CParser::BlockContext *context) = 0;

    virtual std::any visitBlockStmt(CParser::BlockStmtContext *context) = 0;

    virtual std::any visitIfStmt(CParser::IfStmtContext *context) = 0;

    virtual std::any visitWhileStmt(CParser::WhileStmtContext *context) = 0;

    virtual std::any visitForStmt(CParser::ForStmtContext *context) = 0;

    virtual std::any visitReturnStmt(CParser::ReturnStmtContext *context) = 0;

    virtual std::any visitAsmStmt(CParser::AsmStmtContext *context) = 0;

    virtual std::any visitSimpleStmtWrapper(CParser::SimpleStmtWrapperContext *context) = 0;

    virtual std::any visitEmptyStmt(CParser::EmptyStmtContext *context) = 0;

    virtual std::any visitLocalArrayDecl(CParser::LocalArrayDeclContext *context) = 0;

    virtual std::any visitLocalDecl(CParser::LocalDeclContext *context) = 0;

    virtual std::any visitExprStmt(CParser::ExprStmtContext *context) = 0;

    virtual std::any visitAssignDerefExpr(CParser::AssignDerefExprContext *context) = 0;

    virtual std::any visitAssignIndexExpr(CParser::AssignIndexExprContext *context) = 0;

    virtual std::any visitAssignExpr(CParser::AssignExprContext *context) = 0;

    virtual std::any visitExprCond(CParser::ExprCondContext *context) = 0;

    virtual std::any visitCharExpr(CParser::CharExprContext *context) = 0;

    virtual std::any visitAddrOfExpr(CParser::AddrOfExprContext *context) = 0;

    virtual std::any visitOrExpr(CParser::OrExprContext *context) = 0;

    virtual std::any visitNumExpr(CParser::NumExprContext *context) = 0;

    virtual std::any visitParenExpr(CParser::ParenExprContext *context) = 0;

    virtual std::any visitBitXorExpr(CParser::BitXorExprContext *context) = 0;

    virtual std::any visitShiftExpr(CParser::ShiftExprContext *context) = 0;

    virtual std::any visitIndexExpr(CParser::IndexExprContext *context) = 0;

    virtual std::any visitVarExpr(CParser::VarExprContext *context) = 0;

    virtual std::any visitEqExpr(CParser::EqExprContext *context) = 0;

    virtual std::any visitBitOrExpr(CParser::BitOrExprContext *context) = 0;

    virtual std::any visitUnaryExpr(CParser::UnaryExprContext *context) = 0;

    virtual std::any visitAddExpr(CParser::AddExprContext *context) = 0;

    virtual std::any visitDerefExpr(CParser::DerefExprContext *context) = 0;

    virtual std::any visitMulExpr(CParser::MulExprContext *context) = 0;

    virtual std::any visitCallExpr(CParser::CallExprContext *context) = 0;

    virtual std::any visitRelExpr(CParser::RelExprContext *context) = 0;

    virtual std::any visitBitAndExpr(CParser::BitAndExprContext *context) = 0;

    virtual std::any visitAndExpr(CParser::AndExprContext *context) = 0;

    virtual std::any visitArgList(CParser::ArgListContext *context) = 0;


};

