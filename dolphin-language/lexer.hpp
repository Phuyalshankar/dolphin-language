#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include "token.hpp"

struct LexError : std::runtime_error {
    int line;
    LexError(int l, const std::string& msg)
        : std::runtime_error("Lex error at line " + std::to_string(l) + ": " + msg), line(l) {}
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string src;
    size_t pos = 0;
    int line = 1;

    bool atEnd() const;
    char peek(int offset = 0) const;
    char advance();
    bool match(char expected);
    void skipWhitespaceAndComments();
    Token makeNumber();
    Token makeString();
    Token makeIdentOrKeyword();
    Token makeOperator();
};
