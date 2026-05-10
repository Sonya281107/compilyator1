#include "token.hpp"

namespace Lexer {

std::string_view token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile:    return "EndOfFile";
        case TokenKind::Invalid:      return "Invalid";
        case TokenKind::Identifier:   return "Identifier";
        case TokenKind::IntLiteral:   return "IntLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::StringLiteral:return "StringLiteral";
        case TokenKind::FnKw:         return "fn";
        case TokenKind::LetKw:        return "let";
        case TokenKind::MutKw:        return "mut";
        case TokenKind::StructKw:     return "struct";
        case TokenKind::TypeKw:       return "type";
        case TokenKind::NamespaceKw:  return "namespace";
        case TokenKind::ImplKw:       return "impl";
        case TokenKind::IfKw:         return "if";
        case TokenKind::ElseKw:       return "else";
        case TokenKind::WhileKw:      return "while";
        case TokenKind::BreakKw:      return "break";
        case TokenKind::ContinueKw:   return "continue";
        case TokenKind::ReturnKw:     return "return";
        case TokenKind::AsKw:         return "as";
        case TokenKind::TrueKw:       return "true";
        case TokenKind::FalseKw:      return "false";
        case TokenKind::VoidKw:       return "void";
        case TokenKind::BoolKw:       return "bool";
        case TokenKind::StringKw:     return "string";
        case TokenKind::I8Kw:         return "i8";
        case TokenKind::I16Kw:        return "i16";
        case TokenKind::I32Kw:        return "i32";
        case TokenKind::I64Kw:        return "i64";
        case TokenKind::U8Kw:         return "u8";
        case TokenKind::U16Kw:        return "u16";
        case TokenKind::U32Kw:        return "u32";
        case TokenKind::U64Kw:        return "u64";
        case TokenKind::F32Kw:        return "f32";
        case TokenKind::F64Kw:        return "f64";
        case TokenKind::LParen:       return "(";
        case TokenKind::RParen:       return ")";
        case TokenKind::LBrace:       return "{";
        case TokenKind::RBrace:       return "}";
        case TokenKind::LBracket:     return "[";
        case TokenKind::RBracket:     return "]";
        case TokenKind::Comma:        return ",";
        case TokenKind::Semicolon:    return ";";
        case TokenKind::Colon:        return ":";
        case TokenKind::ColonColon:   return "::";
        case TokenKind::Dot:          return ".";
        case TokenKind::Arrow:        return "->";
        case TokenKind::Plus:         return "+";
        case TokenKind::Minus:        return "-";
        case TokenKind::Star:         return "*";
        case TokenKind::Slash:        return "/";
        case TokenKind::Percent:      return "%";
        case TokenKind::Bang:         return "!";
        case TokenKind::Assign:       return "=";
        case TokenKind::EqEq:         return "==";
        case TokenKind::NotEq:        return "!=";
        case TokenKind::Less:         return "<";
        case TokenKind::LessEq:       return "<=";
        case TokenKind::Greater:      return ">";
        case TokenKind::GreaterEq:    return ">=";
        case TokenKind::AndAnd:       return "&&";
        case TokenKind::OrOr:         return "||";
    }
    return "Unknown";
}

}
