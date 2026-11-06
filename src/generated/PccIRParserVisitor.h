
// Generated from /Users/tom/projects/pcc/grammar/PccIRParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "PccIRParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by PccIRParser.
 */
class  PccIRParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by PccIRParser.
   */
    virtual std::any visitProgram(PccIRParser::ProgramContext *context) = 0;

    virtual std::any visitTypeSpec(PccIRParser::TypeSpecContext *context) = 0;

    virtual std::any visitGlobalDecl(PccIRParser::GlobalDeclContext *context) = 0;

    virtual std::any visitInitList(PccIRParser::InitListContext *context) = 0;

    virtual std::any visitInitElem(PccIRParser::InitElemContext *context) = 0;

    virtual std::any visitFuncDecl(PccIRParser::FuncDeclContext *context) = 0;

    virtual std::any visitParamList(PccIRParser::ParamListContext *context) = 0;

    virtual std::any visitParam(PccIRParser::ParamContext *context) = 0;

    virtual std::any visitBlock(PccIRParser::BlockContext *context) = 0;

    virtual std::any visitBlockStmt(PccIRParser::BlockStmtContext *context) = 0;

    virtual std::any visitIfStmt(PccIRParser::IfStmtContext *context) = 0;

    virtual std::any visitWhileStmt(PccIRParser::WhileStmtContext *context) = 0;

    virtual std::any visitReturnStmt(PccIRParser::ReturnStmtContext *context) = 0;

    virtual std::any visitBreakStmt(PccIRParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(PccIRParser::ContinueStmtContext *context) = 0;

    virtual std::any visitLocalDeclStmt(PccIRParser::LocalDeclStmtContext *context) = 0;

    virtual std::any visitAsmStmt(PccIRParser::AsmStmtContext *context) = 0;

    virtual std::any visitEmptyStmt(PccIRParser::EmptyStmtContext *context) = 0;

    virtual std::any visitExprStmt(PccIRParser::ExprStmtContext *context) = 0;

    virtual std::any visitRef(PccIRParser::RefContext *context) = 0;

    virtual std::any visitCompoundOp(PccIRParser::CompoundOpContext *context) = 0;

    virtual std::any visitIntLitExpr(PccIRParser::IntLitExprContext *context) = 0;

    virtual std::any visitCompoundIndexExpr(PccIRParser::CompoundIndexExprContext *context) = 0;

    virtual std::any visitCompoundAssignExpr(PccIRParser::CompoundAssignExprContext *context) = 0;

    virtual std::any visitCompoundDerefExpr(PccIRParser::CompoundDerefExprContext *context) = 0;

    virtual std::any visitIndexExpr(PccIRParser::IndexExprContext *context) = 0;

    virtual std::any visitAssignExpr(PccIRParser::AssignExprContext *context) = 0;

    virtual std::any visitStarExpr(PccIRParser::StarExprContext *context) = 0;

    virtual std::any visitUnaryExpr(PccIRParser::UnaryExprContext *context) = 0;

    virtual std::any visitBinaryExpr(PccIRParser::BinaryExprContext *context) = 0;

    virtual std::any visitTernaryExpr(PccIRParser::TernaryExprContext *context) = 0;

    virtual std::any visitConvertExpr(PccIRParser::ConvertExprContext *context) = 0;

    virtual std::any visitCallExpr(PccIRParser::CallExprContext *context) = 0;

    virtual std::any visitCallPtrExpr(PccIRParser::CallPtrExprContext *context) = 0;

    virtual std::any visitIntrinsicExpr(PccIRParser::IntrinsicExprContext *context) = 0;

    virtual std::any visitVarExpr(PccIRParser::VarExprContext *context) = 0;

    virtual std::any visitArgList(PccIRParser::ArgListContext *context) = 0;

    virtual std::any visitBinOp(PccIRParser::BinOpContext *context) = 0;


};

