#include "IRPrinter.h"

#include <sstream>

namespace pcc {

namespace {

std::string slotRef(const ir::Slot *s) { return s->sourceName + "#" + std::to_string(s->id); }
std::string funcRef(const ir::Function *f) { return f->sourceName + "#" + std::to_string(f->id); }

// A plain literal, or a symbol element's `ref` - see IR.h's
// ArrayInitElem and grammar/PccIRParser.g4's initElem, which this
// shape matches exactly.
void printArrayElem(const ir::ArrayInitElem &e, std::ostream &out) {
    if (!e.isSymbol) {
        out << e.value;
    } else if (e.slot) {
        out << slotRef(e.slot);
    } else {
        out << funcRef(e.func);
    }
}

void printExpr(const ir::Expr &e, std::ostream &out) {
    switch (e.kind) {
        case ir::ExprKind::IntLit:
            out << e.intValue;
            return;
        case ir::ExprKind::Var:
            out << slotRef(e.slot);
            return;
        case ir::ExprKind::Index:
            out << slotRef(e.slot) << "[";
            printExpr(*e.lhs, out);
            out << "]";
            return;
        case ir::ExprKind::Func:
            // Same "name#id" shape as slotRef - a bare `ref` already
            // covers this in grammar/PccIRParser.g4, so this needs no
            // grammar change; only ever reached as "&"'s operand (see
            // IR.h's ExprKind::Func).
            out << funcRef(e.func);
            return;
        case ir::ExprKind::Assign:
            if (e.lhs) {
                out << slotRef(e.slot) << "[";
                printExpr(*e.lhs, out);
                out << "] = ";
            } else {
                out << slotRef(e.slot) << " = ";
            }
            printExpr(*e.rhs, out);
            return;
        case ir::ExprKind::DerefAssign:
            out << "*(";
            printExpr(*e.lhs, out);
            out << ") = ";
            printExpr(*e.rhs, out);
            return;
        case ir::ExprKind::Unary:
            out << e.op << "(";
            printExpr(*e.rhs, out);
            out << ")";
            return;
        case ir::ExprKind::Binary:
            out << "(";
            printExpr(*e.lhs, out);
            out << " " << e.op << " ";
            printExpr(*e.rhs, out);
            out << ")";
            return;
        case ir::ExprKind::Call:
            out << funcRef(e.func) << "(";
            for (size_t i = 0; i < e.args.size(); i++) {
                if (i > 0) out << ", ";
                printExpr(*e.args[i], out);
            }
            out << ")";
            return;
        case ir::ExprKind::CallPtr:
            // Always 0 args (see IR.h's ExprKind::CallPtr) - the
            // callee is an arbitrary expr, not a `ref`, so this can't
            // reuse callExpr's "ref(...)" shape; grammar/
            // PccIRParser.g4's callPtrExpr rule matches this exactly.
            out << "(";
            printExpr(*e.lhs, out);
            out << ")()";
            return;
        case ir::ExprKind::Intrinsic:
            out << "_" << e.op << "()";
            return;
    }
}

void printStmt(const ir::Stmt &s, std::ostream &out, int indent) {
    std::string pad(indent, ' ');
    switch (s.kind) {
        case ir::StmtKind::Block:
            out << pad << "{\n";
            for (const auto &child : s.body) printStmt(*child, out, indent + 2);
            out << pad << "}\n";
            return;
        case ir::StmtKind::If:
            out << pad << "if (";
            printExpr(*s.cond, out);
            out << ")\n";
            printStmt(*s.thenStmt, out, indent + 2);
            if (s.elseStmt) {
                out << pad << "else\n";
                printStmt(*s.elseStmt, out, indent + 2);
            }
            return;
        case ir::StmtKind::While:
            out << pad << "while (";
            printExpr(*s.cond, out);
            out << ")\n";
            printStmt(*s.loopBody, out, indent + 2);
            return;
        case ir::StmtKind::Return:
            out << pad << "return";
            if (s.initOrReturnOrExpr) {
                out << " ";
                printExpr(*s.initOrReturnOrExpr, out);
            }
            out << ";\n";
            return;
        case ir::StmtKind::LocalDecl:
            out << pad << "local " << slotRef(s.declSlot);
            if (s.declSlot->arraySize > 0) {
                out << "[" << s.declSlot->arraySize << "] = {";
                for (size_t i = 0; i < s.declSlot->arrayInit.size(); i++) {
                    if (i > 0) out << ", ";
                    printArrayElem(s.declSlot->arrayInit[i], out);
                }
                out << "}";
            } else if (s.initOrReturnOrExpr) {
                out << " = ";
                printExpr(*s.initOrReturnOrExpr, out);
            }
            out << ";\n";
            return;
        case ir::StmtKind::ExprStmt:
            out << pad;
            printExpr(*s.initOrReturnOrExpr, out);
            out << ";\n";
            return;
        case ir::StmtKind::Asm:
            out << pad << "asm { " << s.asmText << " }\n";
            return;
        case ir::StmtKind::Empty:
            out << pad << ";\n";
            return;
    }
}

}  // namespace

std::string printIR(const ir::Program &prog, const std::string &sourceFile) {
    std::ostringstream out;

    out << "; --- globals ---\n";
    for (const auto &slot : prog.slots) {
        if (slot->kind != ir::SlotKind::Global) continue;
        if (!sourceFile.empty() && slot->sourceFile != sourceFile) continue;
        out << "int " << slotRef(slot.get());
        if (slot->arraySize > 0) {
            out << "[" << slot->arraySize << "]";
            if (!slot->arrayInit.empty()) {
                out << " = {";
                for (size_t i = 0; i < slot->arrayInit.size(); i++) {
                    if (i > 0) out << ", ";
                    printArrayElem(slot->arrayInit[i], out);
                }
                out << "}";
            }
        } else if (slot->hasInit) {
            out << " = " << slot->initValue;
        }
        out << ";\n";
    }

    out << "\n; --- functions ---\n";
    for (const auto &fb : prog.functionBodies) {
        if (!sourceFile.empty() && fb.func->sourceFile != sourceFile) continue;
        out << "int " << funcRef(fb.func) << "(";
        for (size_t i = 0; i < fb.func->params.size(); i++) {
            if (i > 0) out << ", ";
            out << "int " << slotRef(fb.func->params[i]);
        }
        out << ")\n";
        printStmt(*fb.body, out, 0);
        out << "\n";
    }

    return out.str();
}

}  // namespace pcc
