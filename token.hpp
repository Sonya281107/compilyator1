#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Lexer {

enum class TokenKind {
    EndOfFile,    // конец файла
    Invalid,      // недопустимый токен

    Identifier,    // идентификатор
    IntLiteral,    // целочисленный 
    FloatLiteral,  // с плавающей точкой
    StringLiteral, // строковый 

    FnKw,        // fn
    LetKw,       // let
    MutKw,       // mut
    StructKw,    // struct
    TypeKw,      // type
    NamespaceKw, // namespace
    ImplKw,      // impl

    IfKw,       // if
    ElseKw,     // else
    WhileKw,    // while
    BreakKw,    // break
    ContinueKw, // continue
    ReturnKw,   // return
    AsKw,       // as приведение типов

    TrueKw,  // true
    FalseKw, // false

    VoidKw,   // void
    BoolKw,   // bool
    StringKw, // string
    I8Kw, I16Kw, I32Kw, I64Kw,   // знаковые целые: i8, i16, i32, i64
    U8Kw, U16Kw, U32Kw, U64Kw,   // беззнаковые целые: u8, u16, u32, u64
    F32Kw, F64Kw,                 // числа с плавающей точкой: f32, f64

    LParen,    // (
    RParen,    // )
    LBrace,    // {
    RBrace,    // }
    LBracket,  // [
    RBracket,  // ]
    Comma,     // ,
    Semicolon, // ;
    Colon,     // :
    ColonColon,// ::

    Plus,      // +
    Minus,     // -
    Star,      // *
    Slash,     // /
    Percent,   // %
    Bang,      // !
    Assign,    // =
    EqEq,      // ==
    NotEq,     // !=
    Less,      // <
    LessEq,    // <=
    Greater,   // >
    GreaterEq, // >=
    AndAnd,    // &&
    OrOr,      // ||
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
