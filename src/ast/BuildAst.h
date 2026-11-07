#pragma once

#include "AST.h"
#include "CParser.h"

namespace pcc {

// Walks an ANTLR parse tree into the AST.h data model, dispatching on the
// labeled-alternative Context subclasses via dynamic_cast rather than the
// generated Visitor interface.
Program buildProgram(CParser::ProgramContext *ctx);

}  // namespace pcc
