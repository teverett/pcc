
// Generated from /Users/tom/projects/pcc/grammar/RccAsm.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "RccAsmVisitor.h"


/**
 * This class provides an empty implementation of RccAsmVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  RccAsmBaseVisitor : public RccAsmVisitor {
public:

  virtual std::any visitProgram(RccAsmParser::ProgramContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLine(RccAsmParser::LineContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLabel(RccAsmParser::LabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignStmt(RccAsmParser::AssignStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTimesStmt(RccAsmParser::TimesStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInstrStmt(RccAsmParser::InstrStmtContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOperand(RccAsmParser::OperandContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpr(RccAsmParser::ExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSyllable(RccAsmParser::SyllableContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSign(RccAsmParser::SignContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAtom(RccAsmParser::AtomContext *ctx) override {
    return visitChildren(ctx);
  }


};

