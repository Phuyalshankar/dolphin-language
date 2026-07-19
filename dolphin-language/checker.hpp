#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include "ast.hpp"

struct VariableInfo {
    std::string name;
    std::string type; // "var", "const", "pin", "int", "string", etc.
    bool is_const = false;
    int line = 0;
    int column = 0;
};

class SemanticError : public std::runtime_error {
public:
    std::string message;
    int line;
    int column;
    std::string filepath;
    std::string source_line;

    SemanticError(const std::string& msg, int l, int c, const std::string& fp, const std::string& src_line)
        : std::runtime_error(msg), message(msg), line(l), column(c), filepath(fp), source_line(src_line) {}
};

class StaticAnalyzer {
private:
    std::string filepath;
    std::vector<std::string> source_lines;
    std::vector<std::map<std::string, VariableInfo>> scope_stack;

    void pushScope();
    void popScope();
    void declareVariable(const std::string& name, const std::string& type, bool is_const, int line, int col);
    bool isDeclared(const std::string& name) const;
    const VariableInfo* getVariable(const std::string& name) const;
    
    std::string getSourceLine(int line_num) const;
    void raiseError(const std::string& msg, const ASTNode* node) const;
    void raiseError(const std::string& msg, int line, int col) const;

    // Node checks
    void checkNode(const ASTNode* node);
    void checkExpr(const Expr* expr);
    void checkStmt(const Stmt* stmt);

public:
    explicit StaticAnalyzer(const std::string& filepath);
    void analyze(const std::unique_ptr<BlockStmt>& ast);
};
