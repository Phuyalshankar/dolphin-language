#pragma once
#include <string>
#include <vector>
#include <memory>
#include <utility>

struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
using Block = std::vector<StmtPtr>;
using Program = Block;

enum class ExprKind { Number, String, Bool, Ident, Array, Object, Lambda, Unary, Binary, Assign, Call, Index, Member, Group };

struct Expr {
    ExprKind kind;
    int line = 0;
    explicit Expr(ExprKind k) : kind(k) {}
    virtual ~Expr() = default;
};

struct NumberExpr : Expr {
    std::string value;
    explicit NumberExpr(std::string v) : Expr(ExprKind::Number), value(std::move(v)) {}
};

struct StringExpr : Expr {
    std::string value;
    explicit StringExpr(std::string v) : Expr(ExprKind::String), value(std::move(v)) {}
};

struct BoolExpr : Expr {
    bool value;
    explicit BoolExpr(bool v) : Expr(ExprKind::Bool), value(v) {}
};

struct IdentExpr : Expr {
    std::string name;
    explicit IdentExpr(std::string n) : Expr(ExprKind::Ident), name(std::move(n)) {}
};

struct ArrayExpr : Expr {
    std::vector<ExprPtr> elements;
    ArrayExpr() : Expr(ExprKind::Array) {}
};

struct ObjectExpr : Expr {
    std::vector<std::pair<std::string, ExprPtr>> pairs;
    ObjectExpr() : Expr(ExprKind::Object) {}
};

struct LambdaExpr : Expr {
    std::vector<std::string> params;
    Block body;
    LambdaExpr() : Expr(ExprKind::Lambda) {}
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr operand;
    bool prefix;
    UnaryExpr(std::string o, ExprPtr e, bool p) : Expr(ExprKind::Unary), op(std::move(o)), operand(std::move(e)), prefix(p) {}
};

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left, right;
    BinaryExpr(std::string o, ExprPtr l, ExprPtr r) : Expr(ExprKind::Binary), op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
};

struct AssignExpr : Expr {
    ExprPtr target;
    std::string op;
    ExprPtr value;
    AssignExpr(ExprPtr t, std::string o, ExprPtr v) : Expr(ExprKind::Assign), target(std::move(t)), op(std::move(o)), value(std::move(v)) {}
};

struct CallExpr : Expr {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    explicit CallExpr(ExprPtr c) : Expr(ExprKind::Call), callee(std::move(c)) {}
};

struct IndexExpr : Expr {
    ExprPtr object;
    ExprPtr index;
    IndexExpr(ExprPtr o, ExprPtr i) : Expr(ExprKind::Index), object(std::move(o)), index(std::move(i)) {}
};

struct MemberExpr : Expr {
    ExprPtr object;
    std::string name;
    MemberExpr(ExprPtr o, std::string n) : Expr(ExprKind::Member), object(std::move(o)), name(std::move(n)) {}
};

struct GroupExpr : Expr {
    ExprPtr inner;
    explicit GroupExpr(ExprPtr e) : Expr(ExprKind::Group), inner(std::move(e)) {}
};

enum class StmtKind { Expr, VarAssign, PinDecl, If, While, LoopInfinite, LoopRange, LoopForEach, FnDecl, Return, Import, Break, Continue };

struct Stmt {
    StmtKind kind;
    int line = 0;
    explicit Stmt(StmtKind k) : kind(k) {}
    virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e) : Stmt(StmtKind::Expr), expr(std::move(e)) {}
};

struct VarAssignStmt : Stmt {
    std::string name;
    std::string op;
    ExprPtr value;
    VarAssignStmt(std::string n, std::string o, ExprPtr v) : Stmt(StmtKind::VarAssign), name(std::move(n)), op(std::move(o)), value(std::move(v)) {}
};

struct PinDeclStmt : Stmt {
    std::string name;
    ExprPtr value;
    PinDeclStmt(std::string n, ExprPtr v) : Stmt(StmtKind::PinDecl), name(std::move(n)), value(std::move(v)) {}
};

struct IfStmt : Stmt {
    ExprPtr cond;
    Block thenBlock;
    Block elseBlock;
    bool hasElse = false;
    IfStmt() : Stmt(StmtKind::If) {}
};

struct WhileStmt : Stmt {
    ExprPtr cond;
    Block body;
    WhileStmt() : Stmt(StmtKind::While) {}
};

struct LoopInfiniteStmt : Stmt {
    Block body;
    LoopInfiniteStmt() : Stmt(StmtKind::LoopInfinite) {}
};

struct LoopRangeStmt : Stmt {
    std::string var;
    ExprPtr start, end;
    Block body;
    LoopRangeStmt() : Stmt(StmtKind::LoopRange) {}
};

struct LoopForEachStmt : Stmt {
    std::string var;
    ExprPtr container;
    Block body;
    LoopForEachStmt() : Stmt(StmtKind::LoopForEach) {}
};

struct FnDeclStmt : Stmt {
    std::string name;
    std::vector<std::string> params;
    Block body;
    FnDeclStmt() : Stmt(StmtKind::FnDecl) {}
};

struct ReturnStmt : Stmt {
    ExprPtr value;
    ReturnStmt() : Stmt(StmtKind::Return) {}
};

struct ImportStmt : Stmt {
    std::string path;
    explicit ImportStmt(std::string p) : Stmt(StmtKind::Import), path(std::move(p)) {}
};

struct BreakStmt : Stmt { BreakStmt() : Stmt(StmtKind::Break) {} };
struct ContinueStmt : Stmt { ContinueStmt() : Stmt(StmtKind::Continue) {} };
