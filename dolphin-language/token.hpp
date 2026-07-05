#pragma once
#include <string>

enum class TokType {
    Number, String, Ident,
    KwFn, KwIf, KwElse, KwWhile, KwLoop, KwIn, KwReturn, KwImport,
    KwTrue, KwFalse, KwBreak, KwContinue, KwPin,
    Plus, Minus, Star, StarStar, Slash, Percent,
    Assign, PlusEq, MinusEq, StarEq, SlashEq, PercentEq,
    AmpEq, PipeEq, CaretEq, ShlEq, ShrEq,
    EqEq, NotEq, Lt, Gt, LtEq, GtEq,
    AndAnd, OrOr, Not,
    Amp, Pipe, Caret, Tilde, Shl, Shr,
    PlusPlus, MinusMinus,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Colon, Dot,
    Eof
};

struct Token {
    TokType type;
    std::string text;
    int line;
};

std::string tokTypeName(TokType t);
