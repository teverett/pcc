
// Generated from /Users/tom/projects/pcc/grammar/PccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "PccAsmParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by PccAsmParser.
 */
class  PccAsmVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by PccAsmParser.
   */
    virtual std::any visitProgram(PccAsmParser::ProgramContext *context) = 0;

    virtual std::any visitLine(PccAsmParser::LineContext *context) = 0;

    virtual std::any visitLabel(PccAsmParser::LabelContext *context) = 0;

    virtual std::any visitAssignStmt(PccAsmParser::AssignStmtContext *context) = 0;

    virtual std::any visitTimesStmt(PccAsmParser::TimesStmtContext *context) = 0;

    virtual std::any visitInstrStmt(PccAsmParser::InstrStmtContext *context) = 0;

    virtual std::any visitOperand(PccAsmParser::OperandContext *context) = 0;

    virtual std::any visitExpr(PccAsmParser::ExprContext *context) = 0;

    virtual std::any visitSyllable(PccAsmParser::SyllableContext *context) = 0;

    virtual std::any visitSign(PccAsmParser::SignContext *context) = 0;

    virtual std::any visitAtom(PccAsmParser::AtomContext *context) = 0;


};

