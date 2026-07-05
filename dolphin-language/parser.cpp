#include "parser.hpp"

Parser::Parser(std::vector<Token> tokens) : toks(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    size_t p = pos + offset;
    if (p >= toks.size()) return toks.back();
    return toks[p];
}

const Token& Parser::advance() {
    const Token& t = toks[pos];
    if (pos + 1 < toks.size()) pos++;
    return t;
}

bool Parser::check(TokType t) const { return peek().type == t; }

bool Parser::match(TokType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokType t, const std::string& what) {
    if (!check(t)) {
        error("expected " + what + " but got '" + peek().text + "'");
    }
    return advance();
}

void Parser::error(const std::string& msg) const {
    throw ParseError(peek().line, msg);
}

Program Parser::parseProgram() {
    Program prog;
    while (!check(TokType::Eof)) {
        prog.push_back(parseStatement());
    }
    return prog;
}

Block Parser::parseBlock() {
    expect(TokType::LBrace, "'{'");
    Block block;
    while (!check(TokType::RBrace) && !check(TokType::Eof)) {
        block.push_back(parseStatement());
    }
    expect(TokType::RBrace, "'}'");
    return block;
}

StmtPtr Parser::parseStatement() {
    int line = peek().line;
    if (check(TokType::KwFn) && peek(1).type == TokType::Ident) return parseFnDecl();
    if (check(TokType::KwIf)) return parseIf();
    if (check(TokType::KwWhile)) return parseWhile();
    if (check(TokType::KwLoop)) return parseLoop();
    if (check(TokType::KwReturn)) return parseReturn();
    if (check(TokType::KwImport)) return parseImport();
    if (check(TokType::KwBreak)) { advance(); auto s = std::make_unique<BreakStmt>(); s->line = line; return s; }
    if (check(TokType::KwContinue)) { advance(); auto s = std::make_unique<ContinueStmt>(); s->line = line; return s; }
    if (check(TokType::KwPin)) {
        advance();
        Token name = expect(TokType::Ident, "identifier after 'pin'");
        expect(TokType::Assign, "'=' in pin declaration");
        ExprPtr value = parseExpression();
        auto s = std::make_unique<PinDeclStmt>(name.text, std::move(value));
        s->line = line;
        return s;
    }

    // identifier '=' expr  (simple declaration/assignment, tracked for `var` insertion)
    if (check(TokType::Ident) && peek(1).type == TokType::Assign) {
        std::string name = advance().text;
        advance(); // '='
        ExprPtr value = parseExpression();
        auto s = std::make_unique<VarAssignStmt>(name, "=", std::move(value));
        s->line = line;
        return s;
    }

    ExprPtr expr = parseExpression();
    auto s = std::make_unique<ExprStmt>(std::move(expr));
    s->line = line;
    return s;
}

StmtPtr Parser::parseIf() {
    int line = peek().line;
    advance(); // if
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = line;
    stmt->cond = parseExpression();
    stmt->thenBlock = parseBlock();
    if (match(TokType::KwElse)) {
        stmt->hasElse = true;
        if (check(TokType::KwIf)) {
            StmtPtr nested = parseIf();
            Block wrap;
            wrap.push_back(std::move(nested));
            stmt->elseBlock = std::move(wrap);
        } else {
            stmt->elseBlock = parseBlock();
        }
    }
    return stmt;
}

StmtPtr Parser::parseWhile() {
    int line = peek().line;
    advance(); // while
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = line;
    stmt->cond = parseExpression();
    stmt->body = parseBlock();
    return stmt;
}

StmtPtr Parser::parseLoop() {
    int line = peek().line;
    advance(); // loop
    if (check(TokType::LBrace)) {
        auto stmt = std::make_unique<LoopInfiniteStmt>();
        stmt->line = line;
        stmt->body = parseBlock();
        return stmt;
    }

    expect(TokType::LParen, "'(' after loop");

    // foreach form: IDENT in expr
    if (check(TokType::Ident) && peek(1).type == TokType::KwIn) {
        std::string var = advance().text;
        advance(); // in
        ExprPtr container = parseExpression();
        expect(TokType::RParen, "')' closing loop(...)");
        auto stmt = std::make_unique<LoopForEachStmt>();
        stmt->line = line;
        stmt->var = var;
        stmt->container = std::move(container);
        stmt->body = parseBlock();
        return stmt;
    }

    // range form: IDENT , expr , expr
    if (check(TokType::Ident) && peek(1).type == TokType::Comma) {
        size_t save = pos;
        std::string var = advance().text;
        advance(); // comma
        ExprPtr start = parseExpression();
        if (check(TokType::Comma)) {
            advance();
            ExprPtr end = parseExpression();
            expect(TokType::RParen, "')' closing loop(...)");
            auto stmt = std::make_unique<LoopRangeStmt>();
            stmt->line = line;
            stmt->var = var;
            stmt->start = std::move(start);
            stmt->end = std::move(end);
            stmt->body = parseBlock();
            return stmt;
        }
        // not actually a range loop; rewind and treat as conditional expression
        pos = save;
    }

    // conditional form: expr
    ExprPtr cond = parseExpression();
    expect(TokType::RParen, "')' closing loop(...)");
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = line;
    stmt->cond = std::move(cond);
    stmt->body = parseBlock();
    return stmt;
}

StmtPtr Parser::parseFnDecl() {
    int line = peek().line;
    advance(); // fn
    Token name = expect(TokType::Ident, "function name");
    expect(TokType::LParen, "'(' after function name");
    std::vector<std::string> params = parseParamList();
    expect(TokType::RParen, "')' after parameters");
    auto stmt = std::make_unique<FnDeclStmt>();
    stmt->line = line;
    stmt->name = name.text;
    stmt->params = params;
    stmt->body = parseBlock();
    return stmt;
}

StmtPtr Parser::parseReturn() {
    int line = peek().line;
    advance(); // return
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->line = line;
    if (!check(TokType::RBrace) && !check(TokType::Eof)) {
        stmt->value = parseExpression();
    }
    return stmt;
}

StmtPtr Parser::parseImport() {
    int line = peek().line;
    advance(); // import
    Token path = expect(TokType::String, "module path string");
    auto stmt = std::make_unique<ImportStmt>(path.text);
    stmt->line = line;
    return stmt;
}

std::vector<std::string> Parser::parseParamList() {
    std::vector<std::string> params;
    if (check(TokType::RParen)) return params;
    params.push_back(expect(TokType::Ident, "parameter name").text);
    while (match(TokType::Comma)) {
        params.push_back(expect(TokType::Ident, "parameter name").text);
    }
    return params;
}

std::vector<ExprPtr> Parser::parseArgList() {
    std::vector<ExprPtr> args;
    if (check(TokType::RParen)) return args;
    args.push_back(parseExpression());
    while (match(TokType::Comma)) {
        args.push_back(parseExpression());
    }
    return args;
}

ExprPtr Parser::parseExpression() { return parseAssignment(); }

static bool isAssignOp(TokType t) {
    switch (t) {
        case TokType::Assign: case TokType::PlusEq: case TokType::MinusEq:
        case TokType::StarEq: case TokType::SlashEq: case TokType::PercentEq:
        case TokType::AmpEq: case TokType::PipeEq: case TokType::CaretEq:
        case TokType::ShlEq: case TokType::ShrEq:
            return true;
        default: return false;
    }
}

ExprPtr Parser::parseAssignment() {
    ExprPtr left = parseLogicalOr();
    if (isAssignOp(peek().type) &&
        (left->kind == ExprKind::Ident || left->kind == ExprKind::Index || left->kind == ExprKind::Member)) {
        std::string op = advance().text;
        ExprPtr value = parseAssignment();
        return std::make_unique<AssignExpr>(std::move(left), op, std::move(value));
    }
    return left;
}

ExprPtr Parser::parseLogicalOr() {
    ExprPtr left = parseLogicalAnd();
    while (check(TokType::OrOr)) {
        std::string op = advance().text;
        ExprPtr right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    ExprPtr left = parseBitwiseOr();
    while (check(TokType::AndAnd)) {
        std::string op = advance().text;
        ExprPtr right = parseBitwiseOr();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseOr() {
    ExprPtr left = parseBitwiseXor();
    while (check(TokType::Pipe)) {
        advance();
        ExprPtr right = parseBitwiseXor();
        left = std::make_unique<BinaryExpr>("|", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseXor() {
    ExprPtr left = parseBitwiseAnd();
    while (check(TokType::Caret)) {
        advance();
        ExprPtr right = parseBitwiseAnd();
        left = std::make_unique<BinaryExpr>("^", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseAnd() {
    ExprPtr left = parseEquality();
    while (check(TokType::Amp)) {
        advance();
        ExprPtr right = parseEquality();
        left = std::make_unique<BinaryExpr>("&", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseRelational();
    while (check(TokType::EqEq) || check(TokType::NotEq)) {
        std::string op = advance().text;
        ExprPtr right = parseRelational();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseRelational() {
    ExprPtr left = parseShift();
    while (check(TokType::Lt) || check(TokType::Gt) || check(TokType::LtEq) || check(TokType::GtEq)) {
        std::string op = advance().text;
        ExprPtr right = parseShift();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseShift() {
    ExprPtr left = parseAdditive();
    while (check(TokType::Shl) || check(TokType::Shr)) {
        std::string op = advance().text;
        ExprPtr right = parseAdditive();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseMultiplicative();
    while (check(TokType::Plus) || check(TokType::Minus)) {
        std::string op = advance().text;
        ExprPtr right = parseMultiplicative();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    ExprPtr left = parseExponent();
    while (check(TokType::Star) || check(TokType::Slash) || check(TokType::Percent)) {
        std::string op = advance().text;
        ExprPtr right = parseExponent();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseExponent() {
    ExprPtr left = parseUnary();
    if (check(TokType::StarStar)) {
        advance();
        ExprPtr right = parseExponent(); // right-associative
        return std::make_unique<BinaryExpr>("**", std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokType::Not) || check(TokType::Minus) || check(TokType::Tilde)) {
        std::string op = advance().text;
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(operand), true);
    }
    if (check(TokType::PlusPlus) || check(TokType::MinusMinus)) {
        std::string op = advance().text;
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryExpr>(op, std::move(operand), true);
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    while (true) {
        if (check(TokType::LParen)) {
            advance();
            auto call = std::make_unique<CallExpr>(std::move(expr));
            call->args = parseArgList();
            expect(TokType::RParen, "')' closing call arguments");
            expr = std::move(call);
        } else if (check(TokType::LBracket)) {
            advance();
            ExprPtr idx = parseExpression();
            expect(TokType::RBracket, "']' closing index expression");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(idx));
        } else if (check(TokType::Dot)) {
            advance();
            Token name = expect(TokType::Ident, "member name after '.'");
            if (name.text == "length" && !check(TokType::LParen)) {
                auto member = std::make_unique<MemberExpr>(std::move(expr), name.text);
                expr = std::make_unique<CallExpr>(std::move(member));
            } else {
                expr = std::make_unique<MemberExpr>(std::move(expr), name.text);
            }
        } else if (check(TokType::PlusPlus) || check(TokType::MinusMinus)) {
            std::string op = advance().text;
            expr = std::make_unique<UnaryExpr>(op, std::move(expr), false);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    int line = peek().line;
    if (check(TokType::Number)) return std::make_unique<NumberExpr>(advance().text);
    if (check(TokType::String)) return std::make_unique<StringExpr>(advance().text);
    if (check(TokType::KwTrue)) { advance(); return std::make_unique<BoolExpr>(true); }
    if (check(TokType::KwFalse)) { advance(); return std::make_unique<BoolExpr>(false); }
    if (check(TokType::KwFn) && peek(1).type == TokType::LParen) return parseLambda();
    if (check(TokType::Ident)) return std::make_unique<IdentExpr>(advance().text);
    // 'pin' can also appear as an expression (e.g. `pin(13, OUTPUT)` used as
    // a constructor call), not just as the `pin x = ...` declaration keyword.
    if (check(TokType::KwPin)) { advance(); return std::make_unique<IdentExpr>("pin"); }
    if (check(TokType::LBracket)) return parseArrayLiteral();
    if (check(TokType::LBrace)) return parseObjectLiteral();
    if (check(TokType::LParen)) {
        advance();
        ExprPtr inner = parseExpression();
        expect(TokType::RParen, "')' closing grouped expression");
        return std::make_unique<GroupExpr>(std::move(inner));
    }
    (void)line;
    error("unexpected token '" + peek().text + "'");
}

ExprPtr Parser::parseArrayLiteral() {
    advance(); // [
    auto arr = std::make_unique<ArrayExpr>();
    if (!check(TokType::RBracket)) {
        arr->elements.push_back(parseExpression());
        while (match(TokType::Comma)) {
            if (check(TokType::RBracket)) break; // trailing comma
            arr->elements.push_back(parseExpression());
        }
    }
    expect(TokType::RBracket, "']' closing array literal");
    return arr;
}

ExprPtr Parser::parseObjectLiteral() {
    advance(); // {
    auto obj = std::make_unique<ObjectExpr>();
    if (!check(TokType::RBrace)) {
        do {
            if (check(TokType::RBrace)) break; // trailing comma
            Token key = expect(TokType::String, "string key in object literal");
            expect(TokType::Colon, "':' after object key");
            ExprPtr value = parseExpression();
            obj->pairs.emplace_back(key.text, std::move(value));
        } while (match(TokType::Comma));
    }
    expect(TokType::RBrace, "'}' closing object literal");
    return obj;
}

ExprPtr Parser::parseLambda() {
    advance(); // fn
    expect(TokType::LParen, "'(' after fn");
    auto lambda = std::make_unique<LambdaExpr>();
    lambda->params = parseParamList();
    expect(TokType::RParen, "')' after lambda parameters");
    lambda->body = parseBlock();
    return lambda;
}
