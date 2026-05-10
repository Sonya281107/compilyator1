#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Lexer {

enum class TokenKind {
    EndOfFile,
    Invalid,

    Identifier,
    IntLiteral,
    FloatLiteral,
    StringLiteral,

    FnKw,
    LetKw,
    MutKw,
    StructKw,
    TypeKw,
    NamespaceKw,
    ImplKw,

    IfKw,
    ElseKw,
    WhileKw,
    BreakKw,
    ContinueKw,
    ReturnKw,
    AsKw,

    TrueKw,
    FalseKw,

    VoidKw,
    BoolKw,
    StringKw,
    I8Kw, I16Kw, I32Kw, I64Kw,
    U8Kw, U16Kw, U32Kw, U64Kw,
    F32Kw, F64Kw,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Semicolon,
    Colon,
    ColonColon,
    Dot,
    Arrow,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Bang,
    Assign,
    EqEq,
    NotEq,
    Less,
    LessEq,
    Greater,
    GreaterEq,
    AndAnd,
    OrOr,
};

struct SourcePos {
    std::size_t index  = 0;
    std::size_t line   = 1;
    std::size_t column = 1;
};

struct SourceSpan {
    SourcePos begin {};
    SourcePos end   {};
};

struct Token {
    TokenKind   kind   = TokenKind::Invalid;
    std::string lexeme {};
    SourceSpan  span   {};
};

std::string_view token_kind_name(TokenKind kind);

}
