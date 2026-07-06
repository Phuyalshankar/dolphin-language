#pragma once
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <sstream>

// ─── Scope & Codegen context ──────────────────────────────────────────────────

struct Scope {
    enum Kind { SCOPE_BLOCK, SCOPE_INITIALIZER, SCOPE_CLASS };
    Kind kind;
    std::set<std::string> vars;
    std::string class_name;   // set when SCOPE_CLASS
};

struct CodegenContext {
    std::vector<Scope> scope_stack;
    std::vector<std::map<std::string, std::string>> typed_scope_stack;
    std::stringstream global_stream;
    bool in_function = false;
    bool in_async_func = false;
    bool in_class = false;
    int indent_level = 1;
    std::string current_class;

    void indent(std::stringstream& ss) const {
        for (int i = 0; i < indent_level; ++i) ss << "    ";
    }

    bool isDeclared(const std::string& name) const {
        for (auto const& scope : scope_stack)
            if (scope.vars.count(name)) return true;
        return false;
    }

    void declare(const std::string& name) {
        if (!scope_stack.empty()) {
            scope_stack.back().vars.insert(name);
        }
    }

    void declareTyped(const std::string& name, const std::string& type_name) {
        declare(name);
        if (!typed_scope_stack.empty()) {
            typed_scope_stack.back()[name] = type_name;
        }
    }

    std::string getDeclaredType(const std::string& name) const {
        for (auto it = typed_scope_stack.rbegin(); it != typed_scope_stack.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        return "";
    }
};

// ─── Base nodes ───────────────────────────────────────────────────────────────

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual std::string compile(CodegenContext& ctx) const = 0;
};

class Expr : public ASTNode {};
class Stmt : public ASTNode {};

// ─── Expressions ──────────────────────────────────────────────────────────────

class LiteralExpr : public Expr {
public:
    enum Kind { LIT_INT, LIT_DOUBLE, LIT_STRING, LIT_BOOL, LIT_NULL, LIT_TEMPLATE };
    Kind        kind;
    std::string value;
    LiteralExpr(Kind k, const std::string& val) : kind(k), value(val) {}
    std::string compile(CodegenContext& ctx) const override;
};

class IdentifierExpr : public Expr {
public:
    std::string name;
    explicit IdentifierExpr(const std::string& n) : name(n) {}
    std::string compile(CodegenContext& ctx) const override;
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> lhs;
    std::string           op;
    std::unique_ptr<Expr> rhs;
    BinaryExpr(std::unique_ptr<Expr> l, const std::string& o, std::unique_ptr<Expr> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class UnaryExpr : public Expr {
public:
    std::string           op;
    std::unique_ptr<Expr> rhs;
    bool                  postfix = false;
    UnaryExpr(const std::string& o, std::unique_ptr<Expr> r, bool post = false)
        : op(o), rhs(std::move(r)), postfix(post) {}
    std::string compile(CodegenContext& ctx) const override;
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr>              callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Expr>> a)
        : callee(std::move(c)), args(std::move(a)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class IndexExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;
    std::unique_ptr<Expr> index;
    IndexExpr(std::unique_ptr<Expr> e, std::unique_ptr<Expr> i)
        : expr(std::move(e)), index(std::move(i)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ArrayLiteralExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;
    explicit ArrayLiteralExpr(std::vector<std::unique_ptr<Expr>> elms)
        : elements(std::move(elms)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ObjectLiteralExpr : public Expr {
public:
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> properties;
    explicit ObjectLiteralExpr(
        std::vector<std::pair<std::string, std::unique_ptr<Expr>>> props)
        : properties(std::move(props)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class LambdaExpr : public Expr {
public:
    std::vector<std::string> args;
    std::unique_ptr<Stmt>    body;
    bool                     is_async = false;
    LambdaExpr(std::vector<std::string> a, std::unique_ptr<Stmt> b, bool async_val = false)
        : args(std::move(a)), body(std::move(b)), is_async(async_val) {}
    std::string compile(CodegenContext& ctx) const override;
};

class AwaitExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;
    explicit AwaitExpr(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class TypeofExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;
    explicit TypeofExpr(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class TypedDeclStmt : public Stmt {
public:
    std::string type_name;
    std::string name;
    std::unique_ptr<Expr> initializer;

    TypedDeclStmt(const std::string& t, const std::string& n, std::unique_ptr<Expr> init)
        : type_name(t), name(n), initializer(std::move(init)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class NewExpr : public Expr {
public:
    std::string                        class_name;
    std::vector<std::unique_ptr<Expr>> args;
    NewExpr(const std::string& cn, std::vector<std::unique_ptr<Expr>> a)
        : class_name(cn), args(std::move(a)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class MatchExpr : public Expr {
public:
    std::unique_ptr<Expr>                                             val;
    std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> arms;
    std::unique_ptr<Expr>                                             default_arm;
    MatchExpr(std::unique_ptr<Expr> v,
              std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> a,
              std::unique_ptr<Expr> def)
        : val(std::move(v)), arms(std::move(a)), default_arm(std::move(def)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ListComprehensionExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;
    std::string           var_name;
    std::unique_ptr<Expr> container;
    std::unique_ptr<Expr> condition; // Optional (may be nullptr)
    ListComprehensionExpr(std::unique_ptr<Expr> e, const std::string& v,
                          std::unique_ptr<Expr> c, std::unique_ptr<Expr> cond)
        : expr(std::move(e)), var_name(v), container(std::move(c)), condition(std::move(cond)) {}
    std::string compile(CodegenContext& ctx) const override;
};

// ─── Statements ───────────────────────────────────────────────────────────────

class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    explicit BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts)
        : statements(std::move(stmts)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class VarDeclStmt : public Stmt {
public:
    std::string           name;
    std::unique_ptr<Expr> initializer;
    VarDeclStmt(const std::string& n, std::unique_ptr<Expr> init)
        : name(n), initializer(std::move(init)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class AssignStmt : public Stmt {
public:
    std::unique_ptr<Expr> lhs;
    std::string           op;
    std::unique_ptr<Expr> rhs;
    AssignStmt(std::unique_ptr<Expr> l, const std::string& o, std::unique_ptr<Expr> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class FuncDeclStmt : public Stmt {
public:
    std::string              name;
    std::vector<std::string> args;
    std::unique_ptr<Stmt>    body;
    bool                     is_async;
    FuncDeclStmt(const std::string& n, std::vector<std::string> a,
                 std::unique_ptr<Stmt> b, bool async_val = false)
        : name(n), args(std::move(a)), body(std::move(b)), is_async(async_val) {}
    std::string compile(CodegenContext& ctx) const override;
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_branch;
    std::unique_ptr<Stmt> else_branch; // may be nullptr
    IfStmt(std::unique_ptr<Expr> c, std::unique_ptr<Stmt> t, std::unique_ptr<Stmt> e)
        : condition(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
    WhileStmt(std::unique_ptr<Expr> c, std::unique_ptr<Stmt> b)
        : condition(std::move(c)), body(std::move(b)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ForRangeStmt : public Stmt {
public:
    std::string           var_name;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    std::unique_ptr<Stmt> body;
    ForRangeStmt(const std::string& v, std::unique_ptr<Expr> s,
                 std::unique_ptr<Expr> e, std::unique_ptr<Stmt> b)
        : var_name(v), start(std::move(s)), end(std::move(e)), body(std::move(b)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ForEachStmt : public Stmt {
public:
    std::string           var_name;
    std::unique_ptr<Expr> container;
    std::unique_ptr<Stmt> body;
    ForEachStmt(const std::string& v, std::unique_ptr<Expr> c, std::unique_ptr<Stmt> b)
        : var_name(v), container(std::move(c)), body(std::move(b)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class InfiniteLoopStmt : public Stmt {
public:
    std::unique_ptr<Stmt> body;
    explicit InfiniteLoopStmt(std::unique_ptr<Stmt> b) : body(std::move(b)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value; // may be nullptr
    explicit ReturnStmt(std::unique_ptr<Expr> v) : value(std::move(v)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class BreakStmt : public Stmt {
public:
    std::string compile(CodegenContext& ctx) const override;
};

class ContinueStmt : public Stmt {
public:
    std::string compile(CodegenContext& ctx) const override;
};

class ImportStmt : public Stmt {
public:
    std::string                        path;
    std::vector<std::unique_ptr<Stmt>> resolved_stmts;
    ImportStmt(const std::string& p, std::vector<std::unique_ptr<Stmt>> stmts)
        : path(p), resolved_stmts(std::move(stmts)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class TryCatchStmt : public Stmt {
public:
    std::unique_ptr<BlockStmt> try_block;
    std::string                catch_var;
    std::unique_ptr<BlockStmt> catch_block;   // may be nullptr
    std::unique_ptr<BlockStmt> finally_block; // may be nullptr
    TryCatchStmt(std::unique_ptr<BlockStmt> tb, const std::string& cv,
                 std::unique_ptr<BlockStmt> cb, std::unique_ptr<BlockStmt> fb)
        : try_block(std::move(tb)), catch_var(cv),
          catch_block(std::move(cb)), finally_block(std::move(fb)) {}
    std::string compile(CodegenContext& ctx) const override;
};

class ThrowStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    explicit ThrowStmt(std::unique_ptr<Expr> e) : expr(std::move(e)) {}
    std::string compile(CodegenContext& ctx) const override;
};

// ─── Class Declaration ────────────────────────────────────────────────────────

struct ClassMethod {
    std::string              name;
    std::vector<std::string> params;
    std::unique_ptr<Stmt>    body;
    bool                     is_async = false;
};

class ClassDeclStmt : public Stmt {
public:
    std::string              name;
    std::string              base_class; // empty if no extends
    std::vector<ClassMethod> methods;

    ClassDeclStmt(const std::string& n, const std::string& base,
                  std::vector<ClassMethod> m)
        : name(n), base_class(base), methods(std::move(m)) {}
    std::string compile(CodegenContext& ctx) const override;
};
