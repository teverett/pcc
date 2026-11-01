
// Generated from /Users/tom/projects/pcc/grammar/PccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "PccAsmVisitor.h"


/**
 * This class provides an empty implementation of PccAsmVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  PccAsmBaseVisitor : public PccAsmVisitor {
public:

  virtual std::any visitProgram(PccAsmParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLine(PccAsmParser::LineContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLabel(PccAsmParser::LabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignStmt(PccAsmParser::AssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTimesStmt(PccAsmParser::TimesStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInstrStmt(PccAsmParser::InstrStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOperand(PccAsmParser::OperandContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpr(PccAsmParser::ExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSyllable(PccAsmParser::SyllableContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSign(PccAsmParser::SignContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAtom(PccAsmParser::AtomContext *ctx) override {
    return visitChildren(ctx);
  }


};

