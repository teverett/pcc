#include "IRPrinter.h"

#include <sstream>

namespace pcc {

namespace {

std::string slotRef(const ir::Slot *s) { return s->sourceName + "#" + std::to_string(s->id); }
std::string funcRef(const ir::Function *f) { return f->sourceName + "#" + std::to_string(f->id); }

// One fixed spelling per (base, signedness) pair, matching exactly what
// grammar/PccIRParser.g4's typeSpec accepts.
std::string typeSpecText(Type t) {
    std::string prefix = t.isUnsigned ? "unsigned " : "";
    std::string base;
    switch (t.base) {
        case BaseType::Char:
            base = "char";
            break;
        case BaseType::Short:
            base = "short";
            break;
        case BaseType::Int:
            base = "int";
            break;
        case BaseType::Long:
            base = "long";
            break;
        case BaseType::LongLong:
            base = "long long";
            break;
    }
    return prefix + base + std::string(t.ptrDepth, '*');
}

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
        case ir::ExprKind::CompoundAssign:
            if (e.yieldOld) out << "oldval ";
            if (e.lhs) {
                out << slotRef(e.slot) << "[";
                printExpr(*e.lhs, out);
                out << "] " << e.op << "= ";
            } else {
                out << slotRef(e.slot) << " " << e.op << "= ";
            }
            printExpr(*e.rhs, out);
            return;
        case ir::ExprKind::CompoundDerefAssign:
            if (e.yieldOld) out << "oldval ";
            out << "*(";
            printExpr(*e.lhs, out);
            out << ") " << e.op << "= ";
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
            // Always 0 args; the callee is an arbitrary expr, not a `ref`.
            out << "(";
            printExpr(*e.lhs, out);
            out << ")()";
            return;
        case ir::ExprKind::Intrinsic:
            out << "_" << e.op << "()";
            return;
        case ir::ExprKind::Convert:
            out << "(" << typeSpecText(e.type) << ")(";
            printExpr(*e.lhs, out);
            out << ")";
            return;
        case ir::ExprKind::Ternary:
            out << "(";
            printExpr(*e.lhs, out);
            out << " ? ";
            printExpr(*e.rhs, out);
            out << " : ";
            printExpr(*e.elseExpr, out);
            out << ")";
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
            // Printed as its own `post` clause (see IR.h's Stmt::postStmt)
            // so it stays visible rather than looking like a plain `while`.
            if (s.postStmt) {
                out << pad << "post\n";
                printStmt(*s.postStmt, out, indent + 2);
            }
            return;
        case ir::StmtKind::Return:
            out << pad << "return";
            if (s.initOrReturnOrExpr) {
                out << " ";
                printExpr(*s.initOrReturnOrExpr, out);
            }
            out << ";\n";
            return;
        case ir::StmtKind::Break:
            out << pad << "break;\n";
            return;
        case ir::StmtKind::Continue:
            out << pad << "continue;\n";
            return;
        case ir::StmtKind::LocalDecl:
            out << pad << "local " << typeSpecText(s.declSlot->type) << " " << slotRef(s.declSlot);
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
        case ir::StmtKind::Label:
            out << pad << "L" << s.labelId << ":\n";
            return;
        case ir::StmtKind::Goto:
            out << pad << "goto L" << s.labelId << ";\n";
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
        out << typeSpecText(slot->type) << " " << slotRef(slot.get());
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
        out << (fb.func->isVoid ? "void" : typeSpecText(fb.func->returnType)) << " " << funcRef(fb.func) << "(";
        for (size_t i = 0; i < fb.func->params.size(); i++) {
            if (i > 0) out << ", ";
            out << typeSpecText(fb.func->params[i]->type) << " " << slotRef(fb.func->params[i]);
        }
        out << ")\n";
        printStmt(*fb.body, out, 0);
        out << "\n";
    }

    return out.str();
}

}  // namespace pcc
