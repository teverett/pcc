
// Generated from /Users/tom/projects/pcc/grammar/RccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "RccAsmParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by RccAsmParser.
 */
class  RccAsmVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by RccAsmParser.
   */
    virtual std::any visitProgram(RccAsmParser::ProgramContext *context) = 0;

    virtual std::any visitLine(RccAsmParser::LineContext *context) = 0;

    virtual std::any visitLabel(RccAsmParser::LabelContext *context) = 0;

    virtual std::any visitAssignStmt(RccAsmParser::AssignStmtContext *context) = 0;

    virtual std::any visitTimesStmt(RccAsmParser::TimesStmtContext *context) = 0;

    virtual std::any visitInstrStmt(RccAsmParser::InstrStmtContext *context) = 0;

    virtual std::any visitOperand(RccAsmParser::OperandContext *context) = 0;

    virtual std::any visitExpr(RccAsmParser::ExprContext *context) = 0;

    virtual std::any visitSyllable(RccAsmParser::SyllableContext *context) = 0;

    virtual std::any visitSign(RccAsmParser::SignContext *context) = 0;

    virtual std::any visitAtom(RccAsmParser::AtomContext *context) = 0;


};

