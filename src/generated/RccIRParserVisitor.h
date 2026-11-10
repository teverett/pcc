
// Generated from /Users/tom/projects/pcc/grammar/RccIRParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "RccIRParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by RccIRParser.
 */
class  RccIRParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by RccIRParser.
   */
    virtual std::any visitProgram(RccIRParser::ProgramContext *context) = 0;

    virtual std::any visitTypeSpec(RccIRParser::TypeSpecContext *context) = 0;

    virtual std::any visitGlobalDecl(RccIRParser::GlobalDeclContext *context) = 0;

    virtual std::any visitInitList(RccIRParser::InitListContext *context) = 0;

    virtual std::any visitInitElem(RccIRParser::InitElemContext *context) = 0;

    virtual std::any visitFuncDecl(RccIRParser::FuncDeclContext *context) = 0;

    virtual std::any visitTypeSpecOrVoid(RccIRParser::TypeSpecOrVoidContext *context) = 0;

    virtual std::any visitParamList(RccIRParser::ParamListContext *context) = 0;

    virtual std::any visitParam(RccIRParser::ParamContext *context) = 0;

    virtual std::any visitBlock(RccIRParser::BlockContext *context) = 0;

    virtual std::any visitBlockStmt(RccIRParser::BlockStmtContext *context) = 0;

    virtual std::any visitIfStmt(RccIRParser::IfStmtContext *context) = 0;

    virtual std::any visitWhileStmt(RccIRParser::WhileStmtContext *context) = 0;

    virtual std::any visitReturnStmt(RccIRParser::ReturnStmtContext *context) = 0;

    virtual std::any visitBreakStmt(RccIRParser::BreakStmtContext *context) = 0;

    virtual std::any visitContinueStmt(RccIRParser::ContinueStmtContext *context) = 0;

    virtual std::any visitLocalDeclStmt(RccIRParser::LocalDeclStmtContext *context) = 0;

    virtual std::any visitAsmStmt(RccIRParser::AsmStmtContext *context) = 0;

    virtual std::any visitEmptyStmt(RccIRParser::EmptyStmtContext *context) = 0;

    virtual std::any visitLabelStmt(RccIRParser::LabelStmtContext *context) = 0;

    virtual std::any visitGotoStmt(RccIRParser::GotoStmtContext *context) = 0;

    virtual std::any visitExprStmt(RccIRParser::ExprStmtContext *context) = 0;

    virtual std::any visitRef(RccIRParser::RefContext *context) = 0;

    virtual std::any visitCompoundOp(RccIRParser::CompoundOpContext *context) = 0;

    virtual std::any visitIntLitExpr(RccIRParser::IntLitExprContext *context) = 0;

    virtual std::any visitCompoundIndexExpr(RccIRParser::CompoundIndexExprContext *context) = 0;

    virtual std::any visitCompoundAssignExpr(RccIRParser::CompoundAssignExprContext *context) = 0;

    virtual std::any visitCompoundDerefExpr(RccIRParser::CompoundDerefExprContext *context) = 0;

    virtual std::any visitIndexExpr(RccIRParser::IndexExprContext *context) = 0;

    virtual std::any visitAssignExpr(RccIRParser::AssignExprContext *context) = 0;

    virtual std::any visitStarExpr(RccIRParser::StarExprContext *context) = 0;

    virtual std::any visitUnaryExpr(RccIRParser::UnaryExprContext *context) = 0;

    virtual std::any visitBinaryExpr(RccIRParser::BinaryExprContext *context) = 0;

    virtual std::any visitTernaryExpr(RccIRParser::TernaryExprContext *context) = 0;

    virtual std::any visitConvertExpr(RccIRParser::ConvertExprContext *context) = 0;

    virtual std::any visitCallExpr(RccIRParser::CallExprContext *context) = 0;

    virtual std::any visitCallPtrExpr(RccIRParser::CallPtrExprContext *context) = 0;

    virtual std::any visitIntrinsicExpr(RccIRParser::IntrinsicExprContext *context) = 0;

    virtual std::any visitVarExpr(RccIRParser::VarExprContext *context) = 0;

    virtual std::any visitArgList(RccIRParser::ArgListContext *context) = 0;

    virtual std::any visitBinOp(RccIRParser::BinOpContext *context) = 0;


};

