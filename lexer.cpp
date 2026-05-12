#include "lexer.hpp"

#include <string>

namespace Lexer {

std::string diagnostic_to_string(const Diagnostic& d) {
    return d.filename + ":" + std::to_string(d.pos.line) + ":"
         + std::to_string(d.pos.column) + ": error: " + d.message;
}

Lexer::Lexer(std::string_view source, std::string filename)
    : source_(source), filename_(std::move(filename)) {}

char Lexer::peek(std::size_t offset) const {
    if (index_ + offset >= source_.size()) return '\0';
    return source_[index_ + offset];
}

char Lexer::advance() {
    if (is_at_end()) return '\0';
    char c = source_[index_++];
    if (c == '\n') { ++line_; column_ = 1; }
    else           { ++column_; }
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[index_] != expected) return false;
    advance();
    return true;
}

bool Lexer::is_at_end() const { return index_ >= source_.size(); }

SourcePos Lexer::current_pos() const { return {index_, line_, column_}; }

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); continue; }
        if (c == '/' && peek(1) == '/') {
            advance(); advance();
            while (!is_at_end() && peek() != '\n') advance();
            continue;
        }
        break;
    }
}

Token Lexer::make_token(TokenKind kind, SourcePos start) const {
    Token t;
    t.kind        = kind;
    t.span.begin  = start;
    t.span.end    = current_pos();
    std::size_t len = t.span.end.index - t.span.begin.index;
    t.lexeme      = std::string(source_.substr(start.index, len));
    return t;
}

Diagnostic Lexer::make_error(SourcePos pos, std::string message) const {
    return {filename_, pos, std::move(message)};
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool Lexer::is_digit(char c) const { return c >= '0' && c <= '9'; }
bool Lexer::is_alnum(char c) const { return is_alpha(c) || is_digit(c); }

TokenKind Lexer::identifier_kind(std::string_view t) const {
    if (t == "fn")        return TokenKind::FnKw;
    if (t == "let")       return TokenKind::LetKw;
    if (t == "mut")       return TokenKind::MutKw;
    if (t == "struct")    return TokenKind::StructKw;
    if (t == "type")      return TokenKind::TypeKw;
    if (t == "namespace") return TokenKind::NamespaceKw;
    if (t == "impl")      return TokenKind::ImplKw;
    if (t == "if")        return TokenKind::IfKw;
    if (t == "else")      return TokenKind::ElseKw;
    if (t == "while")     return TokenKind::WhileKw;
    if (t == "break")     return TokenKind::BreakKw;
    if (t == "continue")  return TokenKind::ContinueKw;
    if (t == "return")    return TokenKind::ReturnKw;
    if (t == "as")        return TokenKind::AsKw;
    if (t == "true")      return TokenKind::TrueKw;
    if (t == "false")     return TokenKind::FalseKw;
    if (t == "void")      return TokenKind::VoidKw;
    if (t == "bool")      return TokenKind::BoolKw;
    if (t == "string")    return TokenKind::StringKw;
    if (t == "i8")        return TokenKind::I8Kw;
    if (t == "i16")       return TokenKind::I16Kw;
    if (t == "i32")       return TokenKind::I32Kw;
    if (t == "i64")       return TokenKind::I64Kw;
    if (t == "u8")        return TokenKind::U8Kw;
    if (t == "u16")       return TokenKind::U16Kw;
    if (t == "u32")       return TokenKind::U32Kw;
    if (t == "u64")       return TokenKind::U64Kw;
    if (t == "f32")       return TokenKind::F32Kw;
    if (t == "f64")       return TokenKind::F64Kw;
    return TokenKind::Identifier;
}

Token Lexer::lex_identifier_or_keyword() {
    SourcePos start = current_pos();
    while (!is_at_end() && is_alnum(peek())) advance();
    Token tok = make_token(TokenKind::Identifier, start);
    tok.kind  = identifier_kind(tok.lexeme);
    return tok;
}

Token Lexer::lex_number() {
    SourcePos start = current_pos();
    while (!is_at_end() && is_digit(peek())) advance();
    bool is_float = !is_at_end() && peek() == '.' && is_digit(peek(1));
    if (is_float) {
        advance();
        while (!is_at_end() && is_digit(peek())) advance();
        return make_token(TokenKind::FloatLiteral, start);
    }
    return make_token(TokenKind::IntLiteral, start);
}

Token Lexer::lex_string() {
    SourcePos start = current_pos();
    advance();
    while (!is_at_end()) {
        char c = peek();
        if (c == '"')  { advance(); return make_token(TokenKind::StringLiteral, start); }
        if (c == '\n') { has_error_ = true; error_ = make_error(current_pos(), "unterminated string literal"); return {}; }
        if (c == '\\') {
            advance();
            if (is_at_end()) { has_error_ = true; error_ = make_error(current_pos(), "unterminated string literal"); return {}; }
            char esc = peek();
            if (esc == '"' || esc == '\\' || esc == 'n' || esc == 't' || esc == 'r' || esc == '0') {
                advance(); continue;
            }
            has_error_ = true;
            error_ = make_error(current_pos(), std::string("invalid escape sequence '\\") + esc + "'");
            return {};
        }
        advance();
    }
    has_error_ = true;
    error_ = make_error(start, "unterminated string literal");
    return {};
}

Token Lexer::lex_operator_or_punct() {
    SourcePos start = current_pos();
    char c = advance();
    switch (c) {
        case '(': return make_token(TokenKind::LParen,    start);
        case ')': return make_token(TokenKind::RParen,    start);
        case '{': return make_token(TokenKind::LBrace,    start);
        case '}': return make_token(TokenKind::RBrace,    start);
        case '[': return make_token(TokenKind::LBracket,  start);
        case ']': return make_token(TokenKind::RBracket,  start);
        case ',': return make_token(TokenKind::Comma,     start);
        case ';': return make_token(TokenKind::Semicolon, start);
        case '+': return make_token(TokenKind::Plus,      start);
        case '-': return make_token(TokenKind::Minus,     start);
        case '*': return make_token(TokenKind::Star,      start);
        case '/': return make_token(TokenKind::Slash,     start);
        case '%': return make_token(TokenKind::Percent,   start);
        case ':': return match(':') ? make_token(TokenKind::ColonColon, start)
                                    : make_token(TokenKind::Colon,      start);
        case '!': return match('=') ? make_token(TokenKind::NotEq,      start)
                                    : make_token(TokenKind::Bang,       start);
        case '=': return match('=') ? make_token(TokenKind::EqEq,       start)
                                    : make_token(TokenKind::Assign,     start);
        case '<': return match('=') ? make_token(TokenKind::LessEq,     start)
                                    : make_token(TokenKind::Less,       start);
        case '>': return match('=') ? make_token(TokenKind::GreaterEq,  start)
                                    : make_token(TokenKind::Greater,    start);
        case '&':
            if (match('&')) return make_token(TokenKind::AndAnd, start);
            has_error_ = true; error_ = make_error(start, "unexpected character '&'"); return {};
        case '|':
            if (match('|')) return make_token(TokenKind::OrOr, start);
            has_error_ = true; error_ = make_error(start, "unexpected character '|'"); return {};
        default: {
            std::string msg = "unexpected character '";
            msg += c; msg += "'";
            has_error_ = true; error_ = make_error(start, msg); return {};
        }
    }
}

LexResult Lexer::tokenize() {
    LexResult result;
    std::vector<Token> tokens;

    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;

        char c = peek();
        Token tok;
        if      (is_alpha(c)) tok = lex_identifier_or_keyword();
        else if (is_digit(c)) tok = lex_number();
        else if (c == '"')    tok = lex_string();
        else                  tok = lex_operator_or_punct();

        if (has_error_) { result.error = error_; return result; }
        tokens.push_back(std::move(tok));
    }

    SourcePos eof = current_pos();
    tokens.push_back({TokenKind::EndOfFile, "", {eof, eof}});
    result.ok     = true;
    result.tokens = std::move(tokens);
    return result;
}

}
