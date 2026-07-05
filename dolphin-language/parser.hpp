#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include "token.hpp"
#include "ast.hpp"

struct ParseError : std::runtime_error {
    int line;
    ParseError(int l, const std::string& msg)
        : std::runtime_error("Parse error at line " + std::to_string(l) + ": " + msg), line(l) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parseProgram();

private:
    std::vector<Token> toks;
    size_t pos = 0;

    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool check(TokType t) const;
    bool match(TokType t);
    const Token& expect(TokType t, const std::string& what);
    [[noreturn]] void error(const std::string& msg) const;

    StmtPtr parseStatement();
    Block parseBlock();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseLoop();
    StmtPtr parseFnDecl();
    StmtPtr parseReturn();
    StmtPtr parseImport();

    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseRelational();
    ExprPtr parseShift();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseExponent();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    ExprPtr parseArrayLiteral();
    ExprPtr parseObjectLiteral();
    ExprPtr parseLambda();
    std::vector<std::string> parseParamList();
    std::vector<ExprPtr> parseArgList();
};
