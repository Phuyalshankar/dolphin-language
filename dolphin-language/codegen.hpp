#pragma once
#include <string>
#include <sstream>
#include <set>
#include <vector>
#include "ast.hpp"

class Codegen {
public:
    // Emits the program's top-level statements into global_stream (function
    // declarations) and main_stream (everything else), matching the wrapper
    // that transpiler.cpp writes around them.
    void generate(const Program& program, std::stringstream& global_stream, std::stringstream& main_stream);

    // Hardware-target variant: function declarations go to global_stream as
    // usual, but top-level statements are split across setup_stream and
    // loop_stream. If the last top-level statement is an infinite `loop {}`,
    // its body becomes loop_stream (called repeatedly by the board's real
    // loop()) and everything before it becomes setup_stream (run once).
    // Otherwise everything goes to setup_stream and loop_stream stays empty.
    void generateHardware(const Program& program, std::stringstream& global_stream, std::stringstream& setup_stream, std::stringstream& loop_stream);

private:
    std::vector<std::set<std::string>> scopeStack;
    std::ostream* globalOut = nullptr;

    void pushScope();
    void popScope();
    bool isDeclared(const std::string& name) const;
    void declare(const std::string& name);

    void renderBlock(const Block& block, std::ostream& out);
    void renderStmt(const Stmt& s, std::ostream& out);
    void renderFnDecl(const FnDeclStmt& fn, std::ostream& out);
    std::string renderExpr(const Expr& e);
    std::string renderLambdaBody(const LambdaExpr& lambda);
};
