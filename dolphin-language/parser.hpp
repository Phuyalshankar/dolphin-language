#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <memory>
#include <vector>

// Operator precedence levels (Pratt parser)
enum Precedence {
    PREC_NONE,
    PREC_ASSIGNMENT,  // = += -= *= /= %= <<= >>= &= |= ^=
    PREC_PIPE,        // |>
    PREC_OR_OR,       // ||
    PREC_AND_AND,     // &&
    PREC_OR,          // |
    PREC_XOR,         // ^
    PREC_AND,         // &
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! ~ - ++ --
    PREC_EXPONENT,    // **
    PREC_CALL,        // . () []
    PREC_PRIMARY
};

class Parser {
private:
    std::vector<Token> tokens;
    size_t             current = 0;
    std::string        filepath;

    bool    isAtEnd()  const;
    Token   peek()     const;
    Token   previous() const;
    Token   advance();
    bool    check(TokenType type) const;
    bool    match(TokenType type);
    Token   consume(TokenType type, const std::string& message);
    void    synchronize();
    bool    isArrowLambda();

    // Statement parsers
    std::unique_ptr<Stmt>       statement();
    std::unique_ptr<Stmt>       funcDeclaration(bool is_async = false);
    std::unique_ptr<Stmt>       classDeclaration();
    std::unique_ptr<Stmt>       ifStatement();
    std::unique_ptr<Stmt>       loopStatement();
    std::unique_ptr<Stmt>       returnStatement();
    std::unique_ptr<Stmt>       importStatement();
    std::unique_ptr<Stmt>       tryCatchStatement();
    std::unique_ptr<Stmt>       throwStatement();
    std::unique_ptr<Stmt>       expressionStatement();
    std::unique_ptr<BlockStmt>  block();

    // Expression parsers (Pratt)
    std::unique_ptr<Expr> expression(Precedence precedence = PREC_NONE);
    std::unique_ptr<Expr> prefixExpr();
    std::unique_ptr<Expr> infixExpr(std::unique_ptr<Expr> lhs, Precedence precedence);

    Precedence            getPrecedence(TokenType type) const;
    std::unique_ptr<Expr> arrayLiteral();
    std::unique_ptr<Expr> objectLiteral();
    std::unique_ptr<Expr> lambdaExpression(bool is_async = false);

public:
    Parser(const std::vector<Token>& tokens, const std::string& filepath = "");
    std::unique_ptr<BlockStmt> parse();

    template <typename T, typename... Args>
    std::unique_ptr<T> makeNode(Args&&... args) {
        auto node = std::make_unique<T>(std::forward<Args>(args)...);
        if (current > 0 && current - 1 < tokens.size()) {
            node->line = tokens[current - 1].line;
            node->column = tokens[current - 1].column;
        } else if (!tokens.empty()) {
            node->line = tokens.front().line;
            node->column = tokens.front().column;
        }
        return node;
    }
};
