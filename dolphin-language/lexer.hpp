#pragma once
#include <string>
#include <vector>

// ─── Token Types ──────────────────────────────────────────────────────────────
enum TokenType {
    TOKEN_EOF,
    TOKEN_ERROR,

    // Literals & Identifiers
    TOKEN_IDENTIFIER,
    TOKEN_INT,
    TOKEN_DOUBLE,
    TOKEN_STRING,

    // Control-flow keywords
    TOKEN_FN,
    TOKEN_LOOP,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_IMPORT,
    TOKEN_IN,
    TOKEN_MATCH,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_FINALLY,
    TOKEN_THROW,
    TOKEN_ASYNC,
    TOKEN_AWAIT,
    TOKEN_BREAK,       // break
    TOKEN_CONTINUE,    // continue

    // OOP keywords
    TOKEN_CLASS,       // class
    TOKEN_EXTENDS,     // extends
    TOKEN_SUPER,       // super
    TOKEN_SELF,        // self
    TOKEN_NEW,         // new

    // Literal keywords
    TOKEN_NULL,        // null
    TOKEN_TRUE,        // true
    TOKEN_FALSE,       // false

    // Type keywords
    TOKEN_TYPEOF,      // typeof
    TOKEN_INSTANCEOF,  // instanceof

    // Operators
    TOKEN_PLUS,          // +
    TOKEN_MINUS,         // -
    TOKEN_STAR,          // *
    TOKEN_SLASH,         // /
    TOKEN_PERCENT,       // %
    TOKEN_EXP,           // **
    TOKEN_ASSIGN,        // =

    // Compound assignments
    TOKEN_PLUS_ASSIGN,   // +=
    TOKEN_MINUS_ASSIGN,  // -=
    TOKEN_STAR_ASSIGN,   // *=
    TOKEN_SLASH_ASSIGN,  // /=
    TOKEN_MOD_ASSIGN,    // %=

    // Bitwise operators
    TOKEN_SHL,           // <<
    TOKEN_SHR,           // >>
    TOKEN_AND,           // &
    TOKEN_OR,            // |
    TOKEN_XOR,           // ^
    TOKEN_NOT,           // ~

    // Bitwise compound assignments
    TOKEN_SHL_ASSIGN,    // <<=
    TOKEN_SHR_ASSIGN,    // >>=
    TOKEN_AND_ASSIGN,    // &=
    TOKEN_OR_ASSIGN,     // |=
    TOKEN_XOR_ASSIGN,    // ^=

    // Logical operators
    TOKEN_BANG,          // !
    TOKEN_AND_AND,       // &&
    TOKEN_OR_OR,         // ||
    TOKEN_ARROW,         // =>
    TOKEN_PIPE,          // |>

    // Comparison operators
    TOKEN_EQ,            // ==
    TOKEN_NE,            // !=
    TOKEN_LT,            // <
    TOKEN_GT,            // >
    TOKEN_LE,            // <=
    TOKEN_GE,            // >=

    // Delimiters
    TOKEN_LPAREN,        // (
    TOKEN_RPAREN,        // )
    TOKEN_LBRACE,        // {
    TOKEN_RBRACE,        // }
    TOKEN_LBRACKET,      // [
    TOKEN_RBRACKET,      // ]
    TOKEN_COMMA,         // ,
    TOKEN_COLON,         // :
    TOKEN_DOT,           // .
    TOKEN_SEMICOLON,     // ;  (optional statement terminator)
    TOKEN_QUESTION,      // ?  (for optional chaining / ternary)
    TOKEN_SPREAD,        // ...

    // Increment/Decrement
    TOKEN_INC,           // ++
    TOKEN_DEC,           // --

    // Template literals
    TOKEN_TEMPLATE,      // `...${...}...`
};

// ─── Source location ──────────────────────────────────────────────────────────
struct Token {
    TokenType   type;
    std::string lexeme;
    int         line;
    int         column;
};

// ─── Error type returned for nice error messages ──────────────────────────────
struct LexError {
    std::string message;
    int         line;
    int         column;
    std::string source_line;   // the full source line (for caret display)
};

// ─── Lexer ───────────────────────────────────────────────────────────────────
class Lexer {
private:
    std::string source;
    size_t      start   = 0;
    size_t      current = 0;
    int         line    = 1;
    int         column  = 1;

    bool        isAtEnd()       const;
    char        advance();
    char        peek()          const;
    char        peekNext()      const;
    bool        match(char expected);
    Token       makeToken(TokenType type);
    Token       errorToken(const std::string& message);
    void        skipWhitespaceAndComments();
    Token       stringToken();
    Token       htmlToken(const std::string& tag_name);
    Token       templateToken();
    Token       numberToken();
    Token       identifierToken();
    TokenType   identifierType();
    std::string getSourceLine(int target_line) const;

public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
};
