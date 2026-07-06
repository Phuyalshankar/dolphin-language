#include "codegen.hpp"
#include <stdexcept>

void Codegen::pushScope() { scopeStack.push_back({}); }
void Codegen::popScope() { scopeStack.pop_back(); }

bool Codegen::isDeclared(const std::string& name) const {
    for (const auto& scope : scopeStack) {
        if (scope.count(name)) return true;
    }
    return false;
}

void Codegen::declare(const std::string& name) {
    scopeStack.back().insert(name);
}

void Codegen::generate(const Program& program, std::stringstream& global_stream, std::stringstream& main_stream) {
    scopeStack.clear();
    globalOut = &global_stream;
    pushScope(); // global scope

    for (const auto& stmtPtr : program) {
        const Stmt& s = *stmtPtr;
        if (s.kind == StmtKind::FnDecl) {
            renderFnDecl(static_cast<const FnDeclStmt&>(s), global_stream);
            declare(static_cast<const FnDeclStmt&>(s).name);
        } else if (s.kind == StmtKind::Import) {
            // Imports are resolved and inlined before codegen runs.
            continue;
        } else {
            renderStmt(s, main_stream);
        }
    }
}

void Codegen::generateHardware(const Program& program, std::stringstream& global_stream, std::stringstream& setup_stream, std::stringstream& loop_stream) {
    scopeStack.clear();
    globalOut = &global_stream;
    pushScope(); // global scope

    std::vector<const Stmt*> topLevel;
    for (const auto& stmtPtr : program) {
        const Stmt& s = *stmtPtr;
        if (s.kind == StmtKind::FnDecl) {
            renderFnDecl(static_cast<const FnDeclStmt&>(s), global_stream);
            declare(static_cast<const FnDeclStmt&>(s).name);
        } else if (s.kind == StmtKind::Import) {
            continue;
        } else {
            topLevel.push_back(&s);
        }
    }

    const Block* loopBody = nullptr;
    if (!topLevel.empty() && topLevel.back()->kind == StmtKind::LoopInfinite) {
        loopBody = &static_cast<const LoopInfiniteStmt&>(*topLevel.back()).body;
        topLevel.pop_back();
    }

    for (const Stmt* s : topLevel) {
        renderStmt(*s, setup_stream);
    }

    if (loopBody) {
        pushScope();
        renderBlock(*loopBody, loop_stream);
        popScope();
    }
}

void Codegen::renderFnDecl(const FnDeclStmt& fn, std::ostream& out) {
    out << "var " << fn.name << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i > 0) out << ", ";
        out << "var " << fn.params[i];
    }
    out << ") {\n";
    pushScope();
    for (const auto& p : fn.params) declare(p);
    renderBlock(fn.body, out);
    out << "return var();\n";
    popScope();
    out << "}\n";
}

void Codegen::renderBlock(const Block& block, std::ostream& out) {
    for (const auto& stmtPtr : block) {
        renderStmt(*stmtPtr, out);
    }
}

void Codegen::renderStmt(const Stmt& s, std::ostream& out) {
    switch (s.kind) {
        case StmtKind::Expr: {
            const auto& e = static_cast<const ExprStmt&>(s);
            out << renderExpr(*e.expr) << ";\n";
            break;
        }
        case StmtKind::VarAssign: {
            const auto& a = static_cast<const VarAssignStmt&>(s);
            std::string rhs = renderExpr(*a.value);
            bool declared = isDeclared(a.name);
            if (!declared) {
                declare(a.name);
                if (scopeStack.size() == 1 && globalOut) {
                    // True top-level variable: must be a real C++ global so
                    // that separately-generated functions can see it too.
                    *globalOut << "var " << a.name << ";\n";
                    out << a.name << " = " << rhs << ";\n";
                } else {
                    out << "var " << a.name << " = " << rhs << ";\n";
                }
            } else {
                out << a.name << " = " << rhs << ";\n";
            }
            break;
        }
        case StmtKind::PinDecl: {
            const auto& p = static_cast<const PinDeclStmt&>(s);
            declare(p.name);
            std::string rhs = renderExpr(*p.value);
            if (scopeStack.size() == 1 && globalOut) {
                // True top-level pin: must be a real C++ global so that
                // separately-generated functions (e.g. hardware's loop())
                // can see it too.
                *globalOut << "pin " << p.name << ";\n";
                out << p.name << " = " << rhs << ";\n";
            } else {
                out << "pin " << p.name << " = " << rhs << ";\n";
            }
            break;
        }
        case StmtKind::If: {
            const auto& f = static_cast<const IfStmt&>(s);
            out << "if (" << renderExpr(*f.cond) << ") {\n";
            pushScope();
            renderBlock(f.thenBlock, out);
            popScope();
            out << "}\n";
            if (f.hasElse) {
                out << "else {\n";
                pushScope();
                renderBlock(f.elseBlock, out);
                popScope();
                out << "}\n";
            }
            break;
        }
        case StmtKind::While: {
            const auto& w = static_cast<const WhileStmt&>(s);
            out << "while (" << renderExpr(*w.cond) << ") {\n";
            pushScope();
            renderBlock(w.body, out);
            popScope();
            out << "}\n";
            break;
        }
        case StmtKind::LoopInfinite: {
            const auto& l = static_cast<const LoopInfiniteStmt&>(s);
            out << "while (true) {\n";
            pushScope();
            renderBlock(l.body, out);
            popScope();
            out << "}\n";
            break;
        }
        case StmtKind::LoopRange: {
            const auto& l = static_cast<const LoopRangeStmt&>(s);
            pushScope();
            declare(l.var);
            out << "for (var " << l.var << " = " << renderExpr(*l.start)
                << "; " << l.var << " < " << renderExpr(*l.end)
                << "; " << l.var << "++) {\n";
            renderBlock(l.body, out);
            popScope();
            out << "}\n";
            break;
        }
        case StmtKind::LoopForEach: {
            const auto& l = static_cast<const LoopForEachStmt&>(s);
            pushScope();
            declare(l.var);
            out << "for (auto& " << l.var << " : " << renderExpr(*l.container) << ") {\n";
            renderBlock(l.body, out);
            popScope();
            out << "}\n";
            break;
        }
        case StmtKind::FnDecl: {
            // Nested function declarations are not supported; treat as an
            // error rather than silently dropping them.
            throw std::runtime_error("nested function declarations are not supported (line " + std::to_string(s.line) + ")");
        }
        case StmtKind::Return: {
            const auto& r = static_cast<const ReturnStmt&>(s);
            if (r.value) {
                // Wrap in var(...) so every return path in a function/lambda
                // deduces the same type, even when the expression is a
                // built-in bool (e.g. from && / || / comparisons).
                out << "return var(" << renderExpr(*r.value) << ");\n";
            } else {
                out << "return var();\n";
            }
            break;
        }
        case StmtKind::Import:
            break; // resolved before codegen
        case StmtKind::Break:
            out << "break;\n";
            break;
        case StmtKind::Continue:
            out << "continue;\n";
            break;
    }
}

std::string Codegen::renderLambdaBody(const LambdaExpr& lambda) {
    std::ostringstream out;
    out << "[=](";
    for (size_t i = 0; i < lambda.params.size(); ++i) {
        if (i > 0) out << ", ";
        out << "var " << lambda.params[i];
    }
    out << ") mutable {\n";
    pushScope();
    for (const auto& p : lambda.params) declare(p);
    renderBlock(lambda.body, out);
    out << "return var();\n";
    popScope();
    out << "}";
    return out.str();
}

std::string Codegen::renderExpr(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Number:
            return static_cast<const NumberExpr&>(e).value;
        case ExprKind::String:
            return "\"" + static_cast<const StringExpr&>(e).value + "\"";
        case ExprKind::Bool:
            return static_cast<const BoolExpr&>(e).value ? "true" : "false";
        case ExprKind::Ident: {
            const std::string& name = static_cast<const IdentExpr&>(e).name;
            if (name == "INPUT") return "PIN_INPUT";
            if (name == "OUTPUT") return "PIN_OUTPUT";
            return name;
        }
        case ExprKind::Array: {
            const auto& arr = static_cast<const ArrayExpr&>(e);
            std::string result = "var_array{";
            for (size_t i = 0; i < arr.elements.size(); ++i) {
                if (i > 0) result += ", ";
                result += renderExpr(*arr.elements[i]);
            }
            result += "}";
            return result;
        }
        case ExprKind::Object: {
            const auto& obj = static_cast<const ObjectExpr&>(e);
            std::string result = "var_object{";
            for (size_t i = 0; i < obj.pairs.size(); ++i) {
                if (i > 0) result += ", ";
                result += "{\"" + obj.pairs[i].first + "\", " + renderExpr(*obj.pairs[i].second) + "}";
            }
            result += "}";
            return result;
        }
        case ExprKind::Lambda:
            return renderLambdaBody(static_cast<const LambdaExpr&>(e));
        case ExprKind::Unary: {
            const auto& u = static_cast<const UnaryExpr&>(e);
            std::string operand = renderExpr(*u.operand);
            if (u.prefix) return u.op + "(" + operand + ")";
            return "(" + operand + ")" + u.op;
        }
        case ExprKind::Binary: {
            const auto& b = static_cast<const BinaryExpr&>(e);
            if (b.op == "**") {
                return "Math.pow(" + renderExpr(*b.left) + ", " + renderExpr(*b.right) + ")";
            }
            return "(" + renderExpr(*b.left) + " " + b.op + " " + renderExpr(*b.right) + ")";
        }
        case ExprKind::Assign: {
            const auto& a = static_cast<const AssignExpr&>(e);
            return "(" + renderExpr(*a.target) + " " + a.op + " " + renderExpr(*a.value) + ")";
        }
        case ExprKind::Call: {
            const auto& c = static_cast<const CallExpr&>(e);
            std::string result = renderExpr(*c.callee) + "(";
            for (size_t i = 0; i < c.args.size(); ++i) {
                if (i > 0) result += ", ";
                result += renderExpr(*c.args[i]);
            }
            result += ")";
            return result;
        }
        case ExprKind::Index: {
            const auto& idx = static_cast<const IndexExpr&>(e);
            return renderExpr(*idx.object) + "[" + renderExpr(*idx.index) + "]";
        }
        case ExprKind::Member: {
            const auto& m = static_cast<const MemberExpr&>(e);
            return renderExpr(*m.object) + "." + m.name;
        }
        case ExprKind::Group: {
            const auto& g = static_cast<const GroupExpr&>(e);
            return "(" + renderExpr(*g.inner) + ")";
        }
    }
    return "";
}
