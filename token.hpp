#pragma once

#include <cstddef>     // std::size_t: для индексов, размеров, позиций
#include <string>      // std::string: для текста лексем и сообщений
#include <string_view> // std::string_view: для удобного просмотра текста без копирования

namespace Lexer {

enum class TokenKind {
    // Специальные токены
    EndOfFile, // конец входного файла
    Invalid,   // некорректный токен, обычно как запасной/временный вариант

    // Идентификаторы и литералы
    Identifier,    // имя переменной, функции, типа и т.п.
    IntLiteral,    // целочисленный литерал: 123
    FloatLiteral,  // вещественный литерал: 3.14
    StringLiteral, // строковый литерал: "hello"

    // Ключевые слова
    FnKw,         // fn
    LetKw,        // let
    MutKw,        // mut
    StructKw,     // struct
    TypeKw,       // type
    NamespaceKw,  // namespace
    IfKw,         // if
    ElseKw,       // else
    WhileKw,      // while
    BreakKw,      // break
    ContinueKw,   // continue
    ReturnKw,     // return
    TrueKw,       // true
    FalseKw,      // false
    VoidKw,       // void

    // Скобки и разделители
    LParen,    // (
    RParen,    // )
    LBrace,    // {
    RBrace,    // }
    LBracket,  // [
    RBracket,  // ]

    Comma,        // ,
    Semicolon,    // ;

    // Операторы
    Plus,       // +
    Minus,      // -
    Star,       // *
    Slash,      // /
    Percent,    // %
    Bang,       // !
    Assign,     // =
    EqEq,       // ==
    NotEq,      // !=
    Less,       // <
    LessEq,     // <=
    Greater,    // >
    GreaterEq,  // >=
    AndAnd,     // &&
    OrOr        // ||
};

struct SourcePos {
    std::size_t index = 0;    // Позиция символа в общем тексте файла
    std::size_t line = 1;     // Номер строки в файле
    std::size_t column = 1;   // Номер столбца внутри строки
};

struct SourceSpan {
    SourcePos begin {};
    SourcePos end {};
};

struct Token {
    TokenKind kind = TokenKind::Invalid;     // Тип токена
    std::string lexeme {};
    SourceSpan span {};                      // Диапазон токена в исходном коде.
};

std::string_view token_kind_name(TokenKind kind);

}  