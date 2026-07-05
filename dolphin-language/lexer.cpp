#include "lexer.hpp"
#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, TokType> kKeywords = {
    {"fn", TokType::KwFn}, {"if", TokType::KwIf}, {"else", TokType::KwElse},
    {"while", TokType::KwWhile}, {"loop", TokType::KwLoop}, {"in", TokType::KwIn},
    {"return", TokType::KwReturn}, {"import", TokType::KwImport},
    {"true", TokType::KwTrue}, {"false", TokType::KwFalse},
    {"break", TokType::KwBreak}, {"continue", TokType::KwContinue},
    {"pin", TokType::KwPin},
};

Lexer::Lexer(std::string source) : src(std::move(source)) {}

bool Lexer::atEnd() const { return pos >= src.size(); }

char Lexer::peek(int offset) const {
    size_t p = pos + offset;
    if (p >= src.size()) return '\0';
    return src[p];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') line++;
    return c;
}

bool Lexer::match(char expected) {
    if (atEnd() || peek() != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {
            while (!atEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::makeNumber() {
    int startLine = line;
    std::string text;
    while (!atEnd() && std::isdigit((unsigned char)peek())) text += advance();
    if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
        text += advance();
        while (!atEnd() && std::isdigit((unsigned char)peek())) text += advance();
    }
    return { TokType::Number, text, startLine };
}

Token Lexer::makeString() {
    int startLine = line;
    advance(); // opening quote
    std::string text;
    while (!atEnd() && peek() != '"') {
        char c = advance();
        if (c == '\\' && !atEnd()) {
            text += c;
            text += advance();
        } else {
            text += c;
        }
    }
    if (atEnd()) {
        throw LexError(startLine, "unterminated string literal");
    }
    advance(); // closing quote
    return { TokType::String, text, startLine };
}

Token Lexer::makeIdentOrKeyword() {
    int startLine = line;
    std::string text;
    while (!atEnd() && (std::isalnum((unsigned char)peek()) || peek() == '_')) text += advance();
    auto it = kKeywords.find(text);
    if (it != kKeywords.end()) return { it->second, text, startLine };
    return { TokType::Ident, text, startLine };
}

Token Lexer::makeOperator() {
    int startLine = line;
    char c = advance();
    switch (c) {
        case '+':
            if (match('+')) return { TokType::PlusPlus, "++", startLine };
            if (match('=')) return { TokType::PlusEq, "+=", startLine };
            return { TokType::Plus, "+", startLine };
        case '-':
            if (match('-')) return { TokType::MinusMinus, "--", startLine };
            if (match('=')) return { TokType::MinusEq, "-=", startLine };
            return { TokType::Minus, "-", startLine };
        case '*':
            if (match('*')) return { TokType::StarStar, "**", startLine };
            if (match('=')) return { TokType::StarEq, "*=", startLine };
            return { TokType::Star, "*", startLine };
        case '/':
            if (match('=')) return { TokType::SlashEq, "/=", startLine };
            return { TokType::Slash, "/", startLine };
        case '%':
            if (match('=')) return { TokType::PercentEq, "%=", startLine };
            return { TokType::Percent, "%", startLine };
        case '=':
            if (match('=')) return { TokType::EqEq, "==", startLine };
            return { TokType::Assign, "=", startLine };
        case '!':
            if (match('=')) return { TokType::NotEq, "!=", startLine };
            return { TokType::Not, "!", startLine };
        case '<':
            if (match('<')) {
                if (match('=')) return { TokType::ShlEq, "<<=", startLine };
                return { TokType::Shl, "<<", startLine };
            }
            if (match('=')) return { TokType::LtEq, "<=", startLine };
            return { TokType::Lt, "<", startLine };
        case '>':
            if (match('>')) {
                if (match('=')) return { TokType::ShrEq, ">>=", startLine };
                return { TokType::Shr, ">>", startLine };
            }
            if (match('=')) return { TokType::GtEq, ">=", startLine };
            return { TokType::Gt, ">", startLine };
        case '&':
            if (match('&')) return { TokType::AndAnd, "&&", startLine };
            if (match('=')) return { TokType::AmpEq, "&=", startLine };
            return { TokType::Amp, "&", startLine };
        case '|':
            if (match('|')) return { TokType::OrOr, "||", startLine };
            if (match('=')) return { TokType::PipeEq, "|=", startLine };
            return { TokType::Pipe, "|", startLine };
        case '^':
            if (match('=')) return { TokType::CaretEq, "^=", startLine };
            return { TokType::Caret, "^", startLine };
        case '~': return { TokType::Tilde, "~", startLine };
        case '(': return { TokType::LParen, "(", startLine };
        case ')': return { TokType::RParen, ")", startLine };
        case '{': return { TokType::LBrace, "{", startLine };
        case '}': return { TokType::RBrace, "}", startLine };
        case '[': return { TokType::LBracket, "[", startLine };
        case ']': return { TokType::RBracket, "]", startLine };
        case ',': return { TokType::Comma, ",", startLine };
        case ':': return { TokType::Colon, ":", startLine };
        case '.': return { TokType::Dot, ".", startLine };
        default:
            throw LexError(startLine, std::string("unexpected character '") + c + "'");
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipWhitespaceAndComments();
        if (atEnd()) break;
        char c = peek();
        if (std::isdigit((unsigned char)c)) {
            tokens.push_back(makeNumber());
        } else if (c == '"') {
            tokens.push_back(makeString());
        } else if (std::isalpha((unsigned char)c) || c == '_') {
            tokens.push_back(makeIdentOrKeyword());
        } else {
            tokens.push_back(makeOperator());
        }
    }
    tokens.push_back({ TokType::Eof, "", line });
    return tokens;
}

std::string tokTypeName(TokType t) {
    switch (t) {
        case TokType::Number: return "number";
        case TokType::String: return "string";
        case TokType::Ident: return "identifier";
        case TokType::Eof: return "end of file";
        default: return "token";
    }
}
