#include "token.hpp"

namespace Lexer{

    std::string_view token_Kind_name(TokenKind kind){
        switch(kind){
            case TokenKind::EndOfFile: return "EndOfFile";
            case TokenKind::Invalid: return "Invalid";
            case TokenKind::Identifier: return "Indentifier";
            case TokenKind::IntLiteral: return "IntLiteral";
            case TokenKind::FloatLiteral: return "FloatLiteral";
            case TokenKind::StringLiteral: return "StringLiteral";
            case TokenKind::FnKw: return "FnKw";
            case TokenKind::LetKw: return "LetKw";
            case TokenKind::MutKw: return "MutKw";
            case TokenKind::StructKw: return "StructKw";
            case TokenKind::TypeKw: return "TypeKw";
            case TokenKind::NamespaceKw: return "NamespaceKw";
            case TokenKind::IfKw: return "IfKw";
            case TokenKind::ElseKw: return "ElseKw";
            case TokenKind::WhileKw: return "WhileKw";
            case TokenKind::BreakKw: return "BreakKw";
            case TokenKind::ContinueKw: return "continueKw";
            case TokenKind::ReturnKw: return "ReturnKw";
            case TokenKind::TrueKw: return "TrueKw";
            case TokenKind::FalseKw: return "FalseKw";
            case TokenKind::VoidKw: return "VoidKw";
            case TokenKind::LParen: return "LParen";
            case TokenKind::RParen: return "RParen";
            case TokenKind::LBrace: return "LBrace";
            case TokenKind::RBrace: return "RBrace";
            case TokenKind::LBracket: return "LBracket";
            case TokenKind::RBracket: return "RBracket";
            case TokenKind::Comma: return "Comma";
            case TokenKind::Semicolon: return "Semicolon";   
            case TokenKind::Plus: return "Plus";
            case TokenKind::Minus: return "Minus";
            case TokenKind::Star: return "Star";
            case TokenKind::Slash: return "Slash";
            case TokenKind::Percent: return "Percent";
            case TokenKind::Bang: return "Bang";
            case TokenKind::Assign: return "Assign";
            case TokenKind::EqEq: return "EqEq";
            case TokenKind::NotEq: return "NotEq";
            case TokenKind::Less: return "Less";
            case TokenKind::LessEq: return "LessEq";
            case TokenKind::Greater: return "Greater";
            case TokenKind::GreaterEq: return "GreaterEq";
            case TokenKind::AndAnd: return "AndAnd";
            case TokenKind::OrOr: return "OrOr";
        }   
        return "Unknown";     
    }
}