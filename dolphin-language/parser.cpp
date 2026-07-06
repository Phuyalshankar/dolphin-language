#include "parser.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <set>
#include <sstream>

// ─── ANSI helpers for error output ──────────────────────────────────────────
static std::string err_bold(const std::string& s) { return "\033[1m"    + s + "\033[0m"; }
static std::string err_red(const std::string& s)  { return "\033[1;31m" + s + "\033[0m"; }
static std::string err_cyan(const std::string& s) { return "\033[1;36m" + s + "\033[0m"; }
static std::string err_blue(const std::string& s) { return "\033[1;34m" + s + "\033[0m"; }

Parser::Parser(const std::vector<Token>& tokens, const std::string& filepath)
    : tokens(tokens), filepath(filepath) {}

bool Parser::isAtEnd() const { return peek().type == TOKEN_EOF; }
Token Parser::peek() const   { return tokens[current]; }
Token Parser::previous() const { return tokens[current - 1]; }

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

// ─── Rich error messages ─────────────────────────────────────────────────────
Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();

    Token t = peek();
    // Build a Rust-style diagnostic
    std::ostringstream oss;
    oss << "\n"
        << err_red("error") << "[E002]: " << err_bold(message) << "\n"
        << err_blue(" --> ") << filepath << ":" << t.line << ":" << t.column << "\n"
        << "  |\n"
        << t.line << " |  " << "... near '" << t.lexeme << "' ...\n"
        << "  |  " << err_cyan("^") << "\n"
        << "  |\n"
        << err_blue("  = ") << "help: check your syntax at this location\n";
    throw std::runtime_error(oss.str());
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        switch (peek().type) {
            case TOKEN_FN:
            case TOKEN_LOOP:
            case TOKEN_IF:
            case TOKEN_RETURN:
            case TOKEN_IMPORT:
            case TOKEN_CLASS:
                return;
            default:
                advance();
        }
    }
}

// ─── Arrow-lambda look-ahead ─────────────────────────────────────────────────
bool Parser::isArrowLambda() {
    size_t lookahead = current;
    // Peek beyond '('
    int parens = 1;
    while (lookahead < tokens.size()) {
        TokenType t = tokens[lookahead].type;
        if (t == TOKEN_LPAREN)  parens++;
        if (t == TOKEN_RPAREN) { parens--; if (parens == 0) break; }
        lookahead++;
    }
    lookahead++; // past ')'
    if (lookahead < tokens.size() && tokens[lookahead].type == TOKEN_ARROW) return true;
    // Check single-param arrow: identifier =>
    if (current < tokens.size() && tokens[current].type == TOKEN_IDENTIFIER &&
        current + 1 < tokens.size() && tokens[current + 1].type == TOKEN_ARROW) return true;
    return false;
}

// ─── Statement parsers ───────────────────────────────────────────────────────
std::unique_ptr<BlockStmt> Parser::parse() {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!isAtEnd()) {
        try {
            stmts.push_back(statement());
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            synchronize();
        }
    }
    return std::make_unique<BlockStmt>(std::move(stmts));
}

std::unique_ptr<Stmt> Parser::statement() {
    while (match(TOKEN_SEMICOLON)) {}

    if (check(TOKEN_IDENTIFIER) &&
        peek().lexeme == "pin" &&
        current + 2 < tokens.size() &&
        tokens[current + 1].type == TOKEN_IDENTIFIER &&
        tokens[current + 2].type == TOKEN_ASSIGN) {
        std::string type_name = advance().lexeme;
        std::string name = consume(TOKEN_IDENTIFIER, "Expect variable name after type.").lexeme;
        consume(TOKEN_ASSIGN, "Expect '=' after typed variable name.");
        auto initializer = expression();
        return std::make_unique<TypedDeclStmt>(type_name, name, std::move(initializer));
    }
    if (match(TOKEN_FN)) return funcDeclaration(false);
    if (match(TOKEN_ASYNC)) {
        if (match(TOKEN_FN)) return funcDeclaration(true);
        // async lambda handled in expression
        current--; // un-consume async
    }
    if (match(TOKEN_CLASS))    return classDeclaration();
    if (match(TOKEN_IF))       return ifStatement();
    if (match(TOKEN_LOOP))     return loopStatement();
    if (match(TOKEN_RETURN))   return returnStatement();
    if (match(TOKEN_IMPORT))   return importStatement();
    if (match(TOKEN_TRY))      return tryCatchStatement();
    if (match(TOKEN_THROW))    return throwStatement();
    if (match(TOKEN_BREAK))    return std::make_unique<BreakStmt>();
    if (match(TOKEN_CONTINUE)) return std::make_unique<ContinueStmt>();
    return expressionStatement();
}

// ─── Function declaration ────────────────────────────────────────────────────
std::unique_ptr<Stmt> Parser::funcDeclaration(bool is_async) {
    Token name = consume(TOKEN_IDENTIFIER, "Expected function name after 'fn'.");
    consume(TOKEN_LPAREN, "Expected '(' after function name.");

    std::vector<std::string> args;
    if (!check(TOKEN_RPAREN)) {
        do {
            args.push_back(consume(TOKEN_IDENTIFIER, "Expected parameter name.").lexeme);
            // Skip optional default value for now
            if (match(TOKEN_ASSIGN)) expression(); // consume default
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expected ')' after parameters.");
    consume(TOKEN_LBRACE,  "Expected '{' to open function body.");
    auto body = block();
    return std::make_unique<FuncDeclStmt>(name.lexeme, args, std::move(body), is_async);
}

// ─── Class declaration ───────────────────────────────────────────────────────
std::unique_ptr<Stmt> Parser::classDeclaration() {
    Token name = consume(TOKEN_IDENTIFIER, "Expected class name after 'class'.");

    std::string base_class;
    if (match(TOKEN_EXTENDS)) {
        Token base = consume(TOKEN_IDENTIFIER, "Expected base class name after 'extends'.");
        base_class = base.lexeme;
    }

    consume(TOKEN_LBRACE, "Expected '{' to open class body.");

    std::vector<ClassMethod> methods;
    while (!check(TOKEN_RBRACE) && !isAtEnd()) {
        while (match(TOKEN_SEMICOLON)) {} // skip semicolons

        bool is_async = false;
        if (match(TOKEN_ASYNC)) is_async = true;
        consume(TOKEN_FN, "Expected 'fn' to declare a class method.");

        Token method_name = consume(TOKEN_IDENTIFIER, "Expected method name.");
        consume(TOKEN_LPAREN, "Expected '(' after method name.");

        std::vector<std::string> params;
        if (!check(TOKEN_RPAREN)) {
            do {
                params.push_back(consume(TOKEN_IDENTIFIER, "Expected parameter name.").lexeme);
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RPAREN, "Expected ')' after method parameters.");
        consume(TOKEN_LBRACE,  "Expected '{' to open method body.");
        auto body = block();

        methods.push_back({ method_name.lexeme, params, std::move(body), is_async });
    }
    consume(TOKEN_RBRACE, "Expected '}' to close class body.");

    return std::make_unique<ClassDeclStmt>(name.lexeme, base_class, std::move(methods));
}

// ─── Block ───────────────────────────────────────────────────────────────────
std::unique_ptr<BlockStmt> Parser::block() {
    std::vector<std::unique_ptr<Stmt>> stmts;
    while (!check(TOKEN_RBRACE) && !isAtEnd()) {
        stmts.push_back(statement());
    }
    consume(TOKEN_RBRACE, "Expected '}' to close block.");
    return std::make_unique<BlockStmt>(std::move(stmts));
}

// ─── If statement ────────────────────────────────────────────────────────────
std::unique_ptr<Stmt> Parser::ifStatement() {
    auto condition = expression();
    consume(TOKEN_LBRACE, "Expected '{' after if condition.");
    auto then_branch = block();

    std::unique_ptr<Stmt> else_branch = nullptr;
    if (match(TOKEN_ELSE)) {
        if (match(TOKEN_IF)) {
            else_branch = ifStatement();
        } else {
            consume(TOKEN_LBRACE, "Expected '{' after else.");
            else_branch = block();
        }
    }
    return std::make_unique<IfStmt>(std::move(condition), std::move(then_branch), std::move(else_branch));
}

// ─── Loop statement ──────────────────────────────────────────────────────────
std::unique_ptr<Stmt> Parser::loopStatement() {
    // loop { ... }  — infinite
    if (check(TOKEN_LBRACE)) {
        consume(TOKEN_LBRACE, "");
        auto body = block();
        return std::make_unique<InfiniteLoopStmt>(std::move(body));
    }

    consume(TOKEN_LPAREN, "Expected '(' after 'loop'.");

    // loop (i, start, end) — range
    if (check(TOKEN_IDENTIFIER) && current + 1 < tokens.size() &&
        tokens[current + 1].type == TOKEN_COMMA) {
        std::string var_name = advance().lexeme;
        consume(TOKEN_COMMA, "Expected ',' in range loop.");
        auto start = expression();
        consume(TOKEN_COMMA, "Expected ',' between start and end in range loop.");
        auto end_expr = expression();
        consume(TOKEN_RPAREN, "Expected ')' after range end.");
        consume(TOKEN_LBRACE,  "Expected '{' to open loop body.");
        auto body = block();
        return std::make_unique<ForRangeStmt>(var_name, std::move(start), std::move(end_expr), std::move(body));
    }

    // loop (item in collection) — foreach
    if (check(TOKEN_IDENTIFIER) && current + 1 < tokens.size() &&
        tokens[current + 1].type == TOKEN_IN) {
        std::string var_name = advance().lexeme;
        consume(TOKEN_IN, "Expected 'in' after loop variable.");
        auto container = expression();
        consume(TOKEN_RPAREN, "Expected ')' after loop collection.");
        consume(TOKEN_LBRACE,  "Expected '{' to open loop body.");
        auto body = block();
        return std::make_unique<ForEachStmt>(var_name, std::move(container), std::move(body));
    }

    // loop (condition) — while
    auto cond = expression();
    consume(TOKEN_RPAREN, "Expected ')' after loop condition.");
    consume(TOKEN_LBRACE,  "Expected '{' to open loop body.");
    auto body = block();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

// ─── Return / Import / TryCatch / Throw ──────────────────────────────────────
std::unique_ptr<Stmt> Parser::returnStatement() {
    if (!check(TOKEN_EOF) && !check(TOKEN_RBRACE) && !check(TOKEN_SEMICOLON)) {
        return std::make_unique<ReturnStmt>(expression());
    }
    return std::make_unique<ReturnStmt>(nullptr);
}

std::unique_ptr<Stmt> Parser::importStatement() {
    Token path_tok = consume(TOKEN_STRING, "Expected string path for import.");
    std::string path = path_tok.lexeme.substr(1, path_tok.lexeme.length() - 2);
    if (path.rfind(".dolphin") == std::string::npos) path += ".dolphin";

    std::string parent_dir;
    size_t last_slash = filepath.find_last_of("\\/");
    if (last_slash != std::string::npos) parent_dir = filepath.substr(0, last_slash + 1);
    std::string full_path = parent_dir + path;

    static std::set<std::string> imported_files;
    std::vector<std::unique_ptr<Stmt>> resolved;

    if (!imported_files.count(full_path)) {
        imported_files.insert(full_path);
        std::ifstream file(full_path);
        if (!file.is_open())
            throw std::runtime_error("[Import Error] Cannot open: " + full_path);
        std::stringstream buf;
        buf << file.rdbuf();
        Lexer lex(buf.str());
        Parser p(lex.tokenize(), full_path);
        auto blk = p.parse();
        if (blk) resolved = std::move(blk->statements);
    }
    return std::make_unique<ImportStmt>(path, std::move(resolved));
}

std::unique_ptr<Stmt> Parser::tryCatchStatement() {
    consume(TOKEN_LBRACE, "Expected '{' after 'try'.");
    auto try_block = block();

    std::string catch_var;
    std::unique_ptr<BlockStmt> catch_block;
    if (match(TOKEN_CATCH)) {
        consume(TOKEN_LPAREN, "Expected '(' after 'catch'.");
        catch_var = consume(TOKEN_IDENTIFIER, "Expected catch variable name.").lexeme;
        consume(TOKEN_RPAREN, "Expected ')' after catch variable.");
        consume(TOKEN_LBRACE,  "Expected '{' before catch body.");
        catch_block = block();
    }

    std::unique_ptr<BlockStmt> finally_block;
    if (match(TOKEN_FINALLY)) {
        consume(TOKEN_LBRACE, "Expected '{' before finally body.");
        finally_block = block();
    }

    return std::make_unique<TryCatchStmt>(
        std::move(try_block), catch_var, std::move(catch_block), std::move(finally_block));
}

std::unique_ptr<Stmt> Parser::throwStatement() {
    return std::make_unique<ThrowStmt>(expression());
}

std::unique_ptr<Stmt> Parser::expressionStatement() {
    auto expr = expression();

    // Assignment statement
    if (match(TOKEN_ASSIGN) || match(TOKEN_PLUS_ASSIGN) || match(TOKEN_MINUS_ASSIGN) ||
        match(TOKEN_STAR_ASSIGN) || match(TOKEN_SLASH_ASSIGN) || match(TOKEN_MOD_ASSIGN) ||
        match(TOKEN_SHL_ASSIGN) || match(TOKEN_SHR_ASSIGN) || match(TOKEN_AND_ASSIGN) ||
        match(TOKEN_OR_ASSIGN)  || match(TOKEN_XOR_ASSIGN)) {
        Token op = previous();
        auto rhs = expression();
        return std::make_unique<AssignStmt>(std::move(expr), op.lexeme, std::move(rhs));
    }

    match(TOKEN_SEMICOLON); // optional semicolon
    return std::make_unique<ExprStmt>(std::move(expr));
}

// ─── Expression parsers (Pratt) ───────────────────────────────────────────────
std::unique_ptr<Expr> Parser::expression(Precedence precedence) {
    auto lhs = prefixExpr();
    while (!isAtEnd() && precedence < getPrecedence(peek().type)) {
        lhs = infixExpr(std::move(lhs), getPrecedence(peek().type));
    }
    return lhs;
}

Precedence Parser::getPrecedence(TokenType type) const {
    switch (type) {
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN: case TOKEN_MINUS_ASSIGN:
        case TOKEN_STAR_ASSIGN: case TOKEN_SLASH_ASSIGN: case TOKEN_MOD_ASSIGN:
        case TOKEN_SHL_ASSIGN:  case TOKEN_SHR_ASSIGN:
        case TOKEN_AND_ASSIGN:  case TOKEN_OR_ASSIGN:   case TOKEN_XOR_ASSIGN:
            return PREC_NONE;
        case TOKEN_PIPE:      return PREC_PIPE;
        case TOKEN_OR_OR:     return PREC_OR_OR;
        case TOKEN_AND_AND:   return PREC_AND_AND;
        case TOKEN_OR:        return PREC_OR;
        case TOKEN_XOR:       return PREC_XOR;
        case TOKEN_AND:       return PREC_AND;
        case TOKEN_EQ: case TOKEN_NE:                     return PREC_EQUALITY;
        case TOKEN_LT: case TOKEN_GT: case TOKEN_LE: case TOKEN_GE: return PREC_COMPARISON;
        case TOKEN_SHL: case TOKEN_SHR:                   return PREC_SHIFT;
        case TOKEN_PLUS: case TOKEN_MINUS:                return PREC_TERM;
        case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT: return PREC_FACTOR;
        case TOKEN_EXP:                                   return PREC_EXPONENT;
        case TOKEN_DOT: case TOKEN_LPAREN: case TOKEN_LBRACKET:
        case TOKEN_INC: case TOKEN_DEC:                   return PREC_CALL;
        default: return PREC_NONE;
    }
}

std::unique_ptr<Expr> Parser::prefixExpr() {
    Token t = advance();
    switch (t.type) {
        // ── Literal values ─────────────────────────────────────────────────
        case TOKEN_INT:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_INT, t.lexeme);
        case TOKEN_DOUBLE:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_DOUBLE, t.lexeme);
        case TOKEN_STRING:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_STRING, t.lexeme);
        case TOKEN_TEMPLATE:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_TEMPLATE, t.lexeme);
        case TOKEN_NULL:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_NULL, "null");
        case TOKEN_TRUE:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_BOOL, "true");
        case TOKEN_FALSE:
            return std::make_unique<LiteralExpr>(LiteralExpr::LIT_BOOL, "false");

        // ── self ──────────────────────────────────────────────────────────
        case TOKEN_SELF:
            return std::make_unique<IdentifierExpr>("self");

        // ── super ─────────────────────────────────────────────────────────
        case TOKEN_SUPER: {
            // super(args...) — compile as __super_init__(self, args...)
            return std::make_unique<IdentifierExpr>("__super__");
        }

        // ── new ───────────────────────────────────────────────────────────
        case TOKEN_NEW: {
            Token cls = consume(TOKEN_IDENTIFIER, "Expected class name after 'new'.");
            consume(TOKEN_LPAREN, "Expected '(' after class name in 'new'.");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TOKEN_RPAREN)) {
                do { args.push_back(expression()); } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expected ')' after arguments.");
            return std::make_unique<NewExpr>(cls.lexeme, std::move(args));
        }

        // ── typeof ────────────────────────────────────────────────────────
        case TOKEN_TYPEOF: {
            auto expr = expression(PREC_UNARY);
            return std::make_unique<TypeofExpr>(std::move(expr));
        }

        // ── Lambda / function ─────────────────────────────────────────────
        case TOKEN_FN:
            return lambdaExpression(false);

        // ── await ─────────────────────────────────────────────────────────
        case TOKEN_AWAIT: {
            auto expr = expression(PREC_UNARY);
            return std::make_unique<AwaitExpr>(std::move(expr));
        }

        // ── async lambda ──────────────────────────────────────────────────
        case TOKEN_ASYNC: {
            if (match(TOKEN_FN)) return lambdaExpression(true);
            // async (args) => ...
            std::vector<std::string> args;
            if (match(TOKEN_LPAREN)) {
                if (!check(TOKEN_RPAREN)) {
                    do {
                        args.push_back(consume(TOKEN_IDENTIFIER, "Expected parameter name.").lexeme);
                    } while (match(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expected ')' after async parameters.");
                consume(TOKEN_ARROW,  "Expected '=>' after async parameters.");
                std::unique_ptr<BlockStmt> body;
                if (match(TOKEN_LBRACE)) {
                    body = block();
                } else {
                    auto expr = expression();
                    std::vector<std::unique_ptr<Stmt>> stmts;
                    stmts.push_back(std::make_unique<ReturnStmt>(std::move(expr)));
                    body = std::make_unique<BlockStmt>(std::move(stmts));
                }
                return std::make_unique<LambdaExpr>(args, std::move(body), true);
            }
            return std::make_unique<IdentifierExpr>("async");
        }

        // ── Unary operators ───────────────────────────────────────────────
        case TOKEN_BANG:
        case TOKEN_MINUS:
        case TOKEN_NOT: {
            auto rhs = expression(PREC_UNARY);
            return std::make_unique<UnaryExpr>(t.lexeme, std::move(rhs));
        }
        case TOKEN_INC:
        case TOKEN_DEC: {
            auto rhs = expression(PREC_UNARY);
            return std::make_unique<UnaryExpr>(t.lexeme, std::move(rhs));
        }

        // ── Grouping ──────────────────────────────────────────────────────
        case TOKEN_LPAREN: {
            // Check for arrow lambda: (params) =>
            if (isArrowLambda() || check(TOKEN_RPAREN)) {
                // put back '(' and parse as lambda
                current--;
                return lambdaExpression(false);
            }
            auto expr = expression();
            consume(TOKEN_RPAREN, "Expected ')' after expression.");
            return expr;
        }

        // ── Array literal ─────────────────────────────────────────────────
        case TOKEN_LBRACKET:
            return arrayLiteral();

        // ── Object literal ────────────────────────────────────────────────
        case TOKEN_LBRACE:
            return objectLiteral();

        // ── Identifier / arrow lambda ─────────────────────────────────────
        case TOKEN_IDENTIFIER: {
            // Single-param arrow: name => expr
            if (check(TOKEN_ARROW)) {
                advance(); // consume '=>'
                std::vector<std::string> args = { t.lexeme };
                std::unique_ptr<BlockStmt> body;
                if (check(TOKEN_LBRACE)) {
                    advance();
                    body = block();
                } else {
                    auto expr = expression();
                    std::vector<std::unique_ptr<Stmt>> stmts;
                    stmts.push_back(std::make_unique<ReturnStmt>(std::move(expr)));
                    body = std::make_unique<BlockStmt>(std::move(stmts));
                }
                return std::make_unique<LambdaExpr>(args, std::move(body), false);
            }
            return std::make_unique<IdentifierExpr>(t.lexeme);
        }

        default:
            throw std::runtime_error(
                err_red("error") + "[E003]: Unexpected token '" + t.lexeme +
                "' at " + filepath + ":" + std::to_string(t.line) + ":" + std::to_string(t.column));
    }
}

std::unique_ptr<Expr> Parser::infixExpr(std::unique_ptr<Expr> lhs, Precedence prec) {
    Token t = advance();

    switch (t.type) {
        // ── Method call / member access ───────────────────────────────────
        case TOKEN_DOT: {
            if (isAtEnd()) {
                throw std::runtime_error("[Parser Error] Unexpected end after '.' on Line " + std::to_string(t.line));
            }
            Token member = peek();
            switch (member.type) {
                case TOKEN_IDENTIFIER:
                case TOKEN_FN:
                case TOKEN_LOOP:
                case TOKEN_IF:
                case TOKEN_ELSE:
                case TOKEN_RETURN:
                case TOKEN_IMPORT:
                case TOKEN_IN:
                case TOKEN_MATCH:
                case TOKEN_TRY:
                case TOKEN_CATCH:
                case TOKEN_FINALLY:
                case TOKEN_THROW:
                case TOKEN_ASYNC:
                case TOKEN_AWAIT:
                    advance();
                    break;
                default:
                    throw std::runtime_error("[Parser Error] File: " + filepath + ", Line: " + std::to_string(member.line) +
                                             ", Col: " + std::to_string(member.column) + " near '" + member.lexeme +
                                             "': Expect member name after '.'.");
            }
            // Method call?
            if (check(TOKEN_LPAREN)) {
                advance();
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TOKEN_RPAREN)) {
                    do { args.push_back(expression()); } while (match(TOKEN_COMMA));
                }
                consume(TOKEN_RPAREN, "Expected ')' after method arguments.");
                auto member_expr = std::make_unique<IdentifierExpr>(member.lexeme);
                auto dot_expr = std::make_unique<BinaryExpr>(
                    std::move(lhs), ".", std::move(member_expr));
                return std::make_unique<CallExpr>(std::move(dot_expr), std::move(args));
            }
            return std::make_unique<BinaryExpr>(
                std::move(lhs), ".", std::make_unique<IdentifierExpr>(member.lexeme));
        }

        // ── Function call ─────────────────────────────────────────────────
        case TOKEN_LPAREN: {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TOKEN_RPAREN)) {
                do {
                    if (check(TOKEN_MATCH)) {
                        advance();
                        consume(TOKEN_LPAREN, "Expected '(' after match.");
                        auto val = expression();
                        consume(TOKEN_RPAREN, "Expected ')' after match value.");
                        consume(TOKEN_LBRACE, "Expected '{' to open match body.");
                        std::vector<std::pair<std::unique_ptr<Expr>, std::unique_ptr<Expr>>> arms;
                        std::unique_ptr<Expr> default_arm;
                        while (!check(TOKEN_RBRACE) && !isAtEnd()) {
                            if (match(TOKEN_ELSE)) {
                                consume(TOKEN_ARROW, "Expected '=>' after 'else' in match.");
                                default_arm = expression();
                                match(TOKEN_COMMA);
                                continue;
                            }
                            auto arm_val = expression();
                            consume(TOKEN_ARROW, "Expected '=>' in match arm.");
                            auto arm_body = expression();
                            arms.push_back({ std::move(arm_val), std::move(arm_body) });
                            match(TOKEN_COMMA);
                        }
                        consume(TOKEN_RBRACE, "Expected '}' to close match.");
                        args.push_back(std::make_unique<MatchExpr>(
                            std::move(val), std::move(arms), std::move(default_arm)));
                    } else {
                        args.push_back(expression());
                    }
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RPAREN, "Expected ')' after arguments.");
            return std::make_unique<CallExpr>(std::move(lhs), std::move(args));
        }

        // ── Index / list comprehension ────────────────────────────────────
        case TOKEN_LBRACKET: {
            auto idx_expr = expression();
            if (check(TOKEN_LOOP)) {
                advance();
                consume(TOKEN_LPAREN, "Expected '(' in list comprehension.");
                std::string var_n = consume(TOKEN_IDENTIFIER, "Expected variable in comprehension.").lexeme;
                consume(TOKEN_IN, "Expected 'in' in comprehension.");
                auto container = expression();
                consume(TOKEN_RPAREN, "Expected ')' after comprehension range.");
                std::unique_ptr<Expr> cond;
                if (match(TOKEN_IF)) cond = expression();
                consume(TOKEN_RBRACKET, "Expected ']' to close comprehension.");
                return std::make_unique<ListComprehensionExpr>(
                    std::move(idx_expr), var_n, std::move(container), std::move(cond));
            }
            consume(TOKEN_RBRACKET, "Expected ']' after index.");
            return std::make_unique<IndexExpr>(std::move(lhs), std::move(idx_expr));
        }

        // ── Pipe operator ─────────────────────────────────────────────────
        case TOKEN_PIPE: {
            auto rhs = expression(PREC_PIPE);
            if (auto call = dynamic_cast<CallExpr*>(rhs.get())) {
                std::vector<std::unique_ptr<Expr>> new_args;
                new_args.push_back(std::move(lhs));
                for (auto& a : call->args) new_args.push_back(std::move(a));
                return std::make_unique<CallExpr>(std::move(call->callee), std::move(new_args));
            }
            std::vector<std::unique_ptr<Expr>> args;
            args.push_back(std::move(lhs));
            return std::make_unique<CallExpr>(std::move(rhs), std::move(args));
        }

        // ── Postfix ++ / -- ───────────────────────────────────────────────
        case TOKEN_INC:
            return std::make_unique<UnaryExpr>("++", std::move(lhs), true);
        case TOKEN_DEC:
            return std::make_unique<UnaryExpr>("--", std::move(lhs), true);

        // ── Binary operators ──────────────────────────────────────────────
        default: {
            auto rhs = expression(prec);
            return std::make_unique<BinaryExpr>(std::move(lhs), t.lexeme, std::move(rhs));
        }
    }
}

// ─── Array / Object literals ─────────────────────────────────────────────────
std::unique_ptr<Expr> Parser::arrayLiteral() {
    // List comprehension: [expr loop (var in container) if cond]
    if (!check(TOKEN_RBRACKET)) {
        auto first = expression();
        if (check(TOKEN_LOOP)) {
            advance();
            consume(TOKEN_LPAREN, "Expected '(' in list comprehension.");
            std::string var_n = consume(TOKEN_IDENTIFIER, "Expected variable.").lexeme;
            consume(TOKEN_IN, "Expected 'in'.");
            auto container = expression();
            consume(TOKEN_RPAREN, "Expected ')'.");
            std::unique_ptr<Expr> cond;
            if (match(TOKEN_IF)) cond = expression();
            consume(TOKEN_RBRACKET, "Expected ']' to close comprehension.");
            return std::make_unique<ListComprehensionExpr>(
                std::move(first), var_n, std::move(container), std::move(cond));
        }
        // Normal array
        std::vector<std::unique_ptr<Expr>> elems;
        elems.push_back(std::move(first));
        while (match(TOKEN_COMMA) && !check(TOKEN_RBRACKET)) {
            elems.push_back(expression());
        }
        match(TOKEN_COMMA); // trailing comma
        consume(TOKEN_RBRACKET, "Expected ']' after array elements.");
        return std::make_unique<ArrayLiteralExpr>(std::move(elems));
    }
    consume(TOKEN_RBRACKET, "Expected ']'.");
    return std::make_unique<ArrayLiteralExpr>(std::vector<std::unique_ptr<Expr>>{});
}

std::unique_ptr<Expr> Parser::objectLiteral() {
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> props;
    while (!check(TOKEN_RBRACE) && !isAtEnd()) {
        std::string key;
        if (check(TOKEN_STRING)) {
            Token k = advance();
            key = k.lexeme.substr(1, k.lexeme.length() - 2);
        } else {
            key = consume(TOKEN_IDENTIFIER, "Expected property name.").lexeme;
        }
        consume(TOKEN_COLON, "Expected ':' after property name.");
        props.push_back({ key, expression() });
        if (!match(TOKEN_COMMA)) break;
    }
    consume(TOKEN_RBRACE, "Expected '}' to close object literal.");
    return std::make_unique<ObjectLiteralExpr>(std::move(props));
}

// ─── Lambda expression ────────────────────────────────────────────────────────
std::unique_ptr<Expr> Parser::lambdaExpression(bool is_async) {
    std::vector<std::string> args;

    if (match(TOKEN_LPAREN)) {
        if (!check(TOKEN_RPAREN)) {
            do {
                args.push_back(consume(TOKEN_IDENTIFIER, "Expected parameter name.").lexeme);
            } while (match(TOKEN_COMMA));
        }
        consume(TOKEN_RPAREN, "Expected ')' after lambda parameters.");
    }
    // else: fn { body } — no params

    if (match(TOKEN_ARROW)) {
        // Arrow body: implicit return if not a block
        if (check(TOKEN_LBRACE)) {
            advance();
            auto body = block();
            return std::make_unique<LambdaExpr>(args, std::move(body), is_async);
        }
        auto expr = expression();
        std::vector<std::unique_ptr<Stmt>> stmts;
        stmts.push_back(std::make_unique<ReturnStmt>(std::move(expr)));
        return std::make_unique<LambdaExpr>(
            args, std::make_unique<BlockStmt>(std::move(stmts)), is_async);
    }

    consume(TOKEN_LBRACE, "Expected '{' to open lambda body.");
    auto body = block();
    return std::make_unique<LambdaExpr>(args, std::move(body), is_async);
}
