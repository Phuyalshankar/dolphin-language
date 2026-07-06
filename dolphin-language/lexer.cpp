#include "lexer.hpp"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

// ─── ANSI colour helpers for error output ────────────────────────────────────
static std::string ansi_red(const std::string& s)  { return "\033[1;31m" + s + "\033[0m"; }
static std::string ansi_bold(const std::string& s) { return "\033[1m"    + s + "\033[0m"; }
static std::string ansi_cyan(const std::string& s) { return "\033[1;36m" + s + "\033[0m"; }

Lexer::Lexer(const std::string& source) : source(source) {}

bool Lexer::isAtEnd() const { return current >= source.length(); }

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') { line++; column = 1; }
    else           { column++; }
    return c;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    advance();
    return true;
}

Token Lexer::makeToken(TokenType type) {
    return Token{ type, source.substr(start, current - start), line, column };
}

Token Lexer::errorToken(const std::string& message) {
    return Token{ TOKEN_ERROR, message, line, column };
}

std::string Lexer::getSourceLine(int target_line) const {
    int cur_line = 1;
    size_t i = 0;
    while (i < source.size() && cur_line < target_line) {
        if (source[i++] == '\n') cur_line++;
    }
    size_t end = i;
    while (end < source.size() && source[end] != '\n') end++;
    return source.substr(i, end - i);
}

// ─── Skip whitespace and comments ────────────────────────────────────────────
void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
            advance();
        } else if (c == '#') {
            // Line comment
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '/' && peekNext() == '/') {
            // C-style line comment
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '/' && peekNext() == '*') {
            // Block comment
            advance(); advance(); // consume /*
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); advance(); // consume */
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

// ─── String token (double-quoted) ────────────────────────────────────────────
Token Lexer::stringToken() {
    std::string value = "\"";
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            return errorToken("Unterminated string (no newlines inside double-quoted strings).");
        }
        if (peek() == '\\') {
            advance(); // consume backslash
            char esc = advance();
            switch (esc) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '"':  value += '"'; break;
                case '\\': value += '\\'; break;
                case '0':  value += '\0'; break;
                default:   value += '\\'; value += esc; break;
            }
        } else {
            value += advance();
        }
    }
    if (isAtEnd()) {
        return errorToken("Unterminated string literal.");
    }
    advance(); // closing "
    value += '"';
    return Token{ TOKEN_STRING, value, line, column };
}

// ─── Template literal token (backtick-quoted) ────────────────────────────────
Token Lexer::templateToken() {
    std::string value = "`";
    while (!isAtEnd() && peek() != '`') {
        if (peek() == '\\') {
            advance();
            value += '\\';
            value += advance();
        } else {
            value += advance();
        }
    }
    if (isAtEnd()) return errorToken("Unterminated template literal.");
    advance(); // closing `
    value += '`';
    return Token{ TOKEN_TEMPLATE, value, line, column };
}

// ─── Number token ─────────────────────────────────────────────────────────────
Token Lexer::numberToken() {
    bool is_hex    = false;
    bool is_binary = false;

    if (source[start] == '0' && !isAtEnd()) {
        if (peek() == 'x' || peek() == 'X') {
            is_hex = true;
            advance(); // consume 'x'
            while (!isAtEnd() && std::isxdigit((unsigned char)peek())) advance();
            return makeToken(TOKEN_INT);
        }
        if (peek() == 'b' || peek() == 'B') {
            is_binary = true;
            advance(); // consume 'b'
            while (!isAtEnd() && (peek() == '0' || peek() == '1')) advance();
            return makeToken(TOKEN_INT);
        }
    }

    while (!isAtEnd() && std::isdigit((unsigned char)peek())) advance();

    bool is_float = false;
    if (!isAtEnd() && peek() == '.' && std::isdigit((unsigned char)peekNext())) {
        is_float = true;
        advance(); // consume '.'
        while (!isAtEnd() && std::isdigit((unsigned char)peek())) advance();
    }

    // Exponent
    if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
        is_float = true;
        advance();
        if (!isAtEnd() && (peek() == '+' || peek() == '-')) advance();
        while (!isAtEnd() && std::isdigit((unsigned char)peek())) advance();
    }

    return makeToken(is_float ? TOKEN_DOUBLE : TOKEN_INT);
}

// ─── Identifier / keyword token ──────────────────────────────────────────────
TokenType Lexer::identifierType() {
    std::string word = source.substr(start, current - start);
    if (word == "fn")         return TOKEN_FN;
    if (word == "loop")       return TOKEN_LOOP;
    if (word == "if")         return TOKEN_IF;
    if (word == "else")       return TOKEN_ELSE;
    if (word == "return")     return TOKEN_RETURN;
    if (word == "import")     return TOKEN_IMPORT;
    if (word == "in")         return TOKEN_IN;
    if (word == "match")      return TOKEN_MATCH;
    if (word == "try")        return TOKEN_TRY;
    if (word == "catch")      return TOKEN_CATCH;
    if (word == "finally")    return TOKEN_FINALLY;
    if (word == "throw")      return TOKEN_THROW;
    if (word == "async")      return TOKEN_ASYNC;
    if (word == "await")      return TOKEN_AWAIT;
    if (word == "break")      return TOKEN_BREAK;
    if (word == "continue")   return TOKEN_CONTINUE;
    if (word == "class")      return TOKEN_CLASS;
    if (word == "extends")    return TOKEN_EXTENDS;
    if (word == "super")      return TOKEN_SUPER;
    if (word == "self")       return TOKEN_SELF;
    if (word == "new")        return TOKEN_NEW;
    if (word == "null")       return TOKEN_NULL;
    if (word == "true")       return TOKEN_TRUE;
    if (word == "false")      return TOKEN_FALSE;
    if (word == "typeof")     return TOKEN_TYPEOF;
    if (word == "instanceof") return TOKEN_INSTANCEOF;
    if (word == "OUTPUT")     return TOKEN_IDENTIFIER; // kept as identifier
    if (word == "INPUT")      return TOKEN_IDENTIFIER;
    if (word == "HIGH")       return TOKEN_IDENTIFIER;
    if (word == "LOW")        return TOKEN_IDENTIFIER;
    return TOKEN_IDENTIFIER;
}

Token Lexer::identifierToken() {
    while (!isAtEnd() && (std::isalnum((unsigned char)peek()) || peek() == '_')) {
        advance();
    }
    return makeToken(identifierType());
}

Token Lexer::htmlToken(const std::string& tag_name) {
    std::string content = "<" + tag_name;
    int nest_level = 1;
    bool in_style = (tag_name == "style");
    bool in_script = (tag_name == "script");
    bool has_expressions = false;

    while (!isAtEnd()) {
        if (peek() == '/' && peekNext() == '>') {
            content += "/>";
            advance(); advance();
            nest_level--;
            if (nest_level == 0) {
                break;
            }
            continue;
        }

        if (peek() == '<') {
            if (peekNext() == '/') {
                size_t temp = current + 2;
                std::string close_tag;
                while (temp < source.length() && std::isalnum((unsigned char)source[temp])) {
                    close_tag += source[temp];
                    temp++;
                }
                std::string close_tag_lower = close_tag;
                for (char &c : close_tag_lower) c = std::tolower(c);
                std::string tag_name_lower = tag_name;
                for (char &c : tag_name_lower) c = std::tolower(c);

                if (temp < source.length() && source[temp] == '>' && close_tag_lower == tag_name_lower) {
                    content += "</" + close_tag + ">";
                    current = temp + 1;
                    nest_level--;
                    if (nest_level == 0) {
                        break;
                    }
                    continue;
                }
                if (close_tag_lower == "style") in_style = false;
                if (close_tag_lower == "script") in_script = false;
            } else {
                size_t temp = current + 1;
                std::string open_tag;
                while (temp < source.length() && std::isalnum((unsigned char)source[temp])) {
                    open_tag += source[temp];
                    temp++;
                }
                std::string open_tag_lower = open_tag;
                for (char &c : open_tag_lower) c = std::tolower(c);
                std::string tag_name_lower = tag_name;
                for (char &c : tag_name_lower) c = std::tolower(c);

                if (open_tag_lower == tag_name_lower) {
                    nest_level++;
                }
                if (open_tag_lower == "style") in_style = true;
                if (open_tag_lower == "script") in_script = true;
            }
        }

        char c = advance();
        if (c == '{' && !in_style && !in_script) {
            content += "${";
            has_expressions = true;
        } else {
            content += c;
        }
    }

    std::string value;
    if (has_expressions) {
        value = "`";
        for (char c : content) {
            if (c == '`') value += "\\`";
            else value += c;
        }
        value += "`";
        return Token{ TOKEN_TEMPLATE, value, line, column };
    } else {
        value = "\"";
        for (char c : content) {
            if (c == '"') value += "\\\"";
            else if (c == '\n') value += "\\n";
            else if (c == '\r') value += "\\r";
            else value += c;
        }
        value += "\"";
        return Token{ TOKEN_STRING, value, line, column };
    }
}

// ─── Main tokenize loop ───────────────────────────────────────────────────────
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;

        start = current;
        char c = advance();

        // Identifiers & keywords
        if (std::isalpha((unsigned char)c) || c == '_') {
            tokens.push_back(identifierToken());
            continue;
        }

        // Numbers
        if (std::isdigit((unsigned char)c)) {
            tokens.push_back(numberToken());
            continue;
        }

        switch (c) {
            // Single-char
            case '(':  tokens.push_back(makeToken(TOKEN_LPAREN));   break;
            case ')':  tokens.push_back(makeToken(TOKEN_RPAREN));   break;
            case '{':  tokens.push_back(makeToken(TOKEN_LBRACE));   break;
            case '}':  tokens.push_back(makeToken(TOKEN_RBRACE));   break;
            case ']':  tokens.push_back(makeToken(TOKEN_RBRACKET)); break;
            case ',':  tokens.push_back(makeToken(TOKEN_COMMA));    break;
            case ':':  tokens.push_back(makeToken(TOKEN_COLON));    break;
            case ';':  tokens.push_back(makeToken(TOKEN_SEMICOLON)); break;
            case '?':  tokens.push_back(makeToken(TOKEN_QUESTION)); break;
            case '^':
                if (match('=')) tokens.push_back(makeToken(TOKEN_XOR_ASSIGN));
                else            tokens.push_back(makeToken(TOKEN_XOR));
                break;
            case '~':  tokens.push_back(makeToken(TOKEN_NOT));      break;

            // '[' or '[' '['
            case '[':  tokens.push_back(makeToken(TOKEN_LBRACKET)); break;

            // '.' or '...'
            case '.':
                if (peek() == '.' && peekNext() == '.') {
                    advance(); advance();
                    tokens.push_back(makeToken(TOKEN_SPREAD));
                } else {
                    tokens.push_back(makeToken(TOKEN_DOT));
                }
                break;

            // '+' '++' '+='
            case '+':
                if (match('+'))      tokens.push_back(makeToken(TOKEN_INC));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_PLUS_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_PLUS));
                break;

            // '-' '--' '-='
            case '-':
                if (match('-'))      tokens.push_back(makeToken(TOKEN_DEC));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_MINUS_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_MINUS));
                break;

            // '*' '**' '*='
            case '*':
                if (match('*'))      tokens.push_back(makeToken(TOKEN_EXP));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_STAR_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_STAR));
                break;

            // '/' '/='
            case '/':
                if (match('='))      tokens.push_back(makeToken(TOKEN_SLASH_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_SLASH));
                break;

            // '%' '%='
            case '%':
                if (match('='))      tokens.push_back(makeToken(TOKEN_MOD_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_PERCENT));
                break;

            // '&' '&&' '&='
            case '&':
                if (match('&'))      tokens.push_back(makeToken(TOKEN_AND_AND));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_AND_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_AND));
                break;

            // '|' '||' '|>' '|='
            case '|':
                if (match('|'))      tokens.push_back(makeToken(TOKEN_OR_OR));
                else if (match('>')) tokens.push_back(makeToken(TOKEN_PIPE));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_OR_ASSIGN));
                else                 tokens.push_back(makeToken(TOKEN_OR));
                break;

            // '=' '=>' '=='
            case '=':
                if (match('>'))      tokens.push_back(makeToken(TOKEN_ARROW));
                else if (match('=')) tokens.push_back(makeToken(TOKEN_EQ));
                else                 tokens.push_back(makeToken(TOKEN_ASSIGN));
                break;

            // '!' '!='
            case '!':
                if (match('='))      tokens.push_back(makeToken(TOKEN_NE));
                else                 tokens.push_back(makeToken(TOKEN_BANG));
                break;

            // '<' '<<' '<<=' '<='
            case '<': {
                // Check if it is a JSX/HTML literal
                size_t temp = current;
                std::string tag_name;
                while (temp < source.length() && isalnum(source[temp])) {
                    tag_name += source[temp];
                    temp++;
                }

                static const std::unordered_set<std::string> html_tags = {
                    "html", "head", "body", "h1", "h2", "h3", "h4", "h5", "h6",
                    "div", "p", "span", "a", "button", "style", "meta", "title",
                    "link", "script", "img", "input", "ul", "ol", "li", "br", "hr",
                    "table", "tr", "td", "th", "form", "label", "iframe", "section",
                    "header", "footer", "nav", "main", "aside"
                };

                if (!tag_name.empty() && html_tags.count(tag_name)) {
                    current = temp; // skip tag name in lexer stream
                    tokens.push_back(htmlToken(tag_name));
                    break;
                }

                if (match('<')) {
                    if (match('=')) tokens.push_back(makeToken(TOKEN_SHL_ASSIGN));
                    else            tokens.push_back(makeToken(TOKEN_SHL));
                } else if (match('=')) {
                    tokens.push_back(makeToken(TOKEN_LE));
                } else {
                    tokens.push_back(makeToken(TOKEN_LT));
                }
                break;
            }

            // '>' '>>' '>>=' '>='
            case '>':
                if (match('>')) {
                    if (match('=')) tokens.push_back(makeToken(TOKEN_SHR_ASSIGN));
                    else            tokens.push_back(makeToken(TOKEN_SHR));
                } else if (match('=')) {
                    tokens.push_back(makeToken(TOKEN_GE));
                } else {
                    tokens.push_back(makeToken(TOKEN_GT));
                }
                break;

            // String literals
            case '"': tokens.push_back(stringToken()); break;
            case '`': tokens.push_back(templateToken()); break;

            default: {
                // Emit a rich error with the source line and caret
                std::string src_line = getSourceLine(line);
                std::string caret(std::max(0, column - 2), ' ');
                caret += '^';
                std::string err =
                    ansi_red("error") + "[E001]: Unexpected character '" +
                    std::string(1, c) + "'\n" +
                    " --> <source>:" + std::to_string(line) + ":" + std::to_string(column) + "\n" +
                    "  |\n" +
                    std::to_string(line) + " | " + src_line + "\n" +
                    "  | " + ansi_cyan(caret) + "\n";
                throw std::runtime_error(err);
            }
        }
    }

    tokens.push_back(Token{ TOKEN_EOF, "", line, column });
    return tokens;
}
