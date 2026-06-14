module;
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

export module lexer;

export namespace Lexer {

enum class TokenKind {
    EndOfFile, Invalid,
    Identifier, IntLiteral, FloatLiteral, StringLiteral,
    FnKw, LetKw, MutKw, StructKw, TypeKw, NamespaceKw, ImplKw, InterfaceKw,
    IfKw, ElseKw, WhileKw, BreakKw, ContinueKw, ReturnKw, AsKw,
    TrueKw, FalseKw, PubKw, PrivKw, VoidKw, BoolKw, StringKw,
    I8Kw, I16Kw, I32Kw, I64Kw, U8Kw, U16Kw, U32Kw, U64Kw, F32Kw, F64Kw,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Colon, ColonColon, Dot, Arrow,
    Plus, Minus, Star, Slash, Percent, Bang,
    Assign, EqEq, NotEq, Less, LessEq, Greater, GreaterEq,
    AndAnd, OrOr,
};

struct SourcePos {
    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct SourceSpan {
    SourcePos begin {};
    SourcePos end {};
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string lexeme {};
    SourceSpan span {};
};

struct Diagnostic {
    std::string filename {};
    SourcePos pos {};
    std::string message {};
};

struct LexResult {
    bool ok = false;
    std::vector<Token> tokens {};
    Diagnostic error {};
};

std::string_view token_kind_name(TokenKind kind);
std::string diagnostic_to_string(const Diagnostic& d);

class Lexer {
public:
    Lexer(std::string_view source, std::string filename);
    LexResult tokenize();

private:
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    bool is_at_end() const;
    SourcePos current_pos() const;
    void skip_whitespace_and_comments();
    Token lex_identifier_or_keyword();
    Token lex_number();
    Token lex_string();
    Token lex_operator_or_punct();
    Token make_token(TokenKind kind, SourcePos start) const;
    Diagnostic make_error(SourcePos pos, std::string message) const;
    bool is_alpha(char c) const;
    bool is_digit(char c) const;
    bool is_alnum(char c) const;
    bool is_hex_digit(char c) const;
    void consume_num_suffix();          
    TokenKind identifier_kind(std::string_view text) const;

    std::string_view source_;
    std::string filename_;
    std::size_t index_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
    bool has_error_ = false;
    Diagnostic error_ {};
};

} // export namespace Lexer


namespace Lexer {

std::string_view token_kind_name(TokenKind kind) {
    switch (kind) {
        case TokenKind::EndOfFile: return "EndOfFile";
        case TokenKind::Invalid: return "Invalid";
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::IntLiteral: return "IntLiteral";
        case TokenKind::FloatLiteral: return "FloatLiteral";
        case TokenKind::StringLiteral: return "StringLiteral";
        case TokenKind::FnKw: return "fn";
        case TokenKind::LetKw: return "let";
        case TokenKind::MutKw: return "mut";
        case TokenKind::StructKw: return "struct";
        case TokenKind::TypeKw: return "type";
        case TokenKind::NamespaceKw: return "namespace";
        case TokenKind::ImplKw: return "impl";
        case TokenKind::InterfaceKw: return "interface";
        case TokenKind::IfKw: return "if";
        case TokenKind::ElseKw: return "else";
        case TokenKind::WhileKw: return "while";
        case TokenKind::BreakKw: return "break";
        case TokenKind::ContinueKw: return "continue";
        case TokenKind::ReturnKw: return "return";
        case TokenKind::AsKw: return "as";
        case TokenKind::TrueKw: return "true";
        case TokenKind::FalseKw: return "false";
        case TokenKind::PubKw: return "pub";
        case TokenKind::PrivKw: return "priv";
        case TokenKind::VoidKw: return "void";
        case TokenKind::BoolKw: return "bool";
        case TokenKind::StringKw: return "string";
        case TokenKind::I8Kw: return "i8";
        case TokenKind::I16Kw: return "i16";
        case TokenKind::I32Kw: return "i32";
        case TokenKind::I64Kw: return "i64";
        case TokenKind::U8Kw: return "u8";
        case TokenKind::U16Kw: return "u16";
        case TokenKind::U32Kw: return "u32";
        case TokenKind::U64Kw: return "u64";
        case TokenKind::F32Kw: return "f32";
        case TokenKind::F64Kw: return "f64";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::Comma: return ",";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Colon: return ":";
        case TokenKind::ColonColon: return "::";
        case TokenKind::Dot: return ".";
        case TokenKind::Arrow: return "->";
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::Bang: return "!";
        case TokenKind::Assign: return "=";
        case TokenKind::EqEq: return "==";
        case TokenKind::NotEq: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::LessEq: return "<=";
        case TokenKind::Greater: return ">";
        case TokenKind::GreaterEq: return ">=";
        case TokenKind::AndAnd: return "&&";
        case TokenKind::OrOr: return "||";
    }
    return "Unknown";
}

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
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[index_] != expected) return false;
    advance();
    return true;
}

bool Lexer::is_at_end() const {
    return index_ >= source_.size();
}

SourcePos Lexer::current_pos() const {
    return {index_, line_, column_};
}

bool Lexer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::is_alnum(char c) const {
    return is_alpha(c) || is_digit(c);
}

bool Lexer::is_hex_digit(char c) const {
    return is_digit(c)
        || (c >= 'a' && c <= 'f')
        || (c >= 'A' && c <= 'F');
}

void Lexer::consume_num_suffix() {
    static constexpr std::string_view SUFFIXES[] = {
        "i64", "i32", "i16", "i8",
        "u64", "u32", "u16", "u8",
        "f64", "f32"
    };
    for (std::string_view suf : SUFFIXES) {
        std::size_t len = suf.size();
        std::size_t remaining = source_.size() - index_;
        if (remaining < len) continue;
        if (source_.substr(index_, len) != suf) continue;
        if (remaining > len && is_alnum(source_[index_ + len])) continue;
        for (std::size_t k = 0; k < len; ++k) advance();
        return;
    }
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            advance();
            advance();
            while (!is_at_end() && peek() != '\n') advance();
            continue;
        }
        break;
    }
}

Token Lexer::make_token(TokenKind kind, SourcePos start) const {
    Token t;
    t.kind = kind;
    t.span.begin = start;
    t.span.end = current_pos();
    t.lexeme = std::string(source_.substr(start.index, t.span.end.index - start.index));
    return t;
}

Diagnostic Lexer::make_error(SourcePos pos, std::string message) const {
    return {filename_, pos, std::move(message)};
}

TokenKind Lexer::identifier_kind(std::string_view t) const {
    if (t == "fn") return TokenKind::FnKw;
    if (t == "let") return TokenKind::LetKw;
    if (t == "mut") return TokenKind::MutKw;
    if (t == "struct") return TokenKind::StructKw;
    if (t == "type") return TokenKind::TypeKw;
    if (t == "namespace") return TokenKind::NamespaceKw;
    if (t == "impl") return TokenKind::ImplKw;
    if (t == "interface") return TokenKind::InterfaceKw;
    if (t == "if") return TokenKind::IfKw;
    if (t == "else") return TokenKind::ElseKw;
    if (t == "while") return TokenKind::WhileKw;
    if (t == "break") return TokenKind::BreakKw;
    if (t == "continue") return TokenKind::ContinueKw;
    if (t == "return") return TokenKind::ReturnKw;
    if (t == "as") return TokenKind::AsKw;
    if (t == "true") return TokenKind::TrueKw;
    if (t == "false") return TokenKind::FalseKw;
    if (t == "pub") return TokenKind::PubKw;
    if (t == "priv") return TokenKind::PrivKw;
    if (t == "void") return TokenKind::VoidKw;
    if (t == "bool") return TokenKind::BoolKw;
    if (t == "string") return TokenKind::StringKw;
    if (t == "i8") return TokenKind::I8Kw;
    if (t == "i16") return TokenKind::I16Kw;
    if (t == "i32") return TokenKind::I32Kw;
    if (t == "i64") return TokenKind::I64Kw;
    if (t == "u8") return TokenKind::U8Kw;
    if (t == "u16") return TokenKind::U16Kw;
    if (t == "u32") return TokenKind::U32Kw;
    if (t == "u64") return TokenKind::U64Kw;
    if (t == "f32") return TokenKind::F32Kw;
    if (t == "f64") return TokenKind::F64Kw;
    return TokenKind::Identifier;
}

Token Lexer::lex_identifier_or_keyword() {
    SourcePos start = current_pos();
    while (!is_at_end() && is_alnum(peek())) advance();
    Token tok = make_token(TokenKind::Identifier, start);
    tok.kind = identifier_kind(tok.lexeme);
    return tok;
}

Token Lexer::lex_number() {
    SourcePos start = current_pos();
    bool is_float = false;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        // Hexadecimal
        advance(); advance();  // consume '0x'
        if (is_at_end() || !is_hex_digit(peek())) {
            has_error_ = true;
            error_ = make_error(start, "expected hex digits after '0x'");
            return {};
        }
        while (!is_at_end() && is_hex_digit(peek())) advance();

    } else if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        // Binary
        advance(); advance();  // consume '0b'
        if (is_at_end() || (peek() != '0' && peek() != '1')) {
            has_error_ = true;
            error_ = make_error(start, "expected binary digits after '0b'");
            return {};
        }
        while (!is_at_end() && (peek() == '0' || peek() == '1')) advance();

    } else {
        // Decimal integer or float
        while (!is_at_end() && is_digit(peek())) advance();
        if (!is_at_end() && peek() == '.' && is_digit(peek(1))) {
            is_float = true;
            advance();
            while (!is_at_end() && is_digit(peek())) advance();
        }
    }

    std::size_t before_suffix = index_;
    consume_num_suffix();
    if (!is_float && index_ > before_suffix) {
        char first_suf = source_[before_suffix];
        if (first_suf == 'f') is_float = true;
    }

    return make_token(is_float ? TokenKind::FloatLiteral : TokenKind::IntLiteral, start);
}

Token Lexer::lex_string() {
    SourcePos start = current_pos();
    advance();
    while (!is_at_end()) {
        char c = peek();
        if (c == '"') {
            advance();
            return make_token(TokenKind::StringLiteral, start);
        }
        if (c == '\n') {
            has_error_ = true;
            error_ = make_error(current_pos(), "unterminated string literal");
            return {};
        }
        if (c == '\\') {
            advance();
            if (is_at_end()) {
                has_error_ = true;
                error_ = make_error(current_pos(), "unterminated string literal");
                return {};
            }
            char esc = peek();
            if (esc == '"' || esc == '\\' || esc == 'n' || esc == 't' || esc == 'r' || esc == '0') {
                advance();
                continue;
            }
            has_error_ = true;
            error_ = make_error(current_pos(), std::string("invalid escape '\\") + esc + "'");
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
        case '(': return make_token(TokenKind::LParen, start);
        case ')': return make_token(TokenKind::RParen, start);
        case '{': return make_token(TokenKind::LBrace, start);
        case '}': return make_token(TokenKind::RBrace, start);
        case '[': return make_token(TokenKind::LBracket, start);
        case ']': return make_token(TokenKind::RBracket, start);
        case ',': return make_token(TokenKind::Comma, start);
        case ';': return make_token(TokenKind::Semicolon, start);
        case '.': return make_token(TokenKind::Dot, start);
        case '+': return make_token(TokenKind::Plus, start);
        case '*': return make_token(TokenKind::Star, start);
        case '/': return make_token(TokenKind::Slash, start);
        case '%': return make_token(TokenKind::Percent, start);
        case '-':
            return match('>') ? make_token(TokenKind::Arrow, start)
                              : make_token(TokenKind::Minus, start);
        case ':':
            return match(':') ? make_token(TokenKind::ColonColon, start)
                              : make_token(TokenKind::Colon, start);
        case '!':
            return match('=') ? make_token(TokenKind::NotEq, start)
                              : make_token(TokenKind::Bang, start);
        case '=':
            return match('=') ? make_token(TokenKind::EqEq, start)
                              : make_token(TokenKind::Assign, start);
        case '<':
            return match('=') ? make_token(TokenKind::LessEq, start)
                              : make_token(TokenKind::Less, start);
        case '>':
            return match('=') ? make_token(TokenKind::GreaterEq, start)
                              : make_token(TokenKind::Greater, start);
        case '&':
            if (match('&')) return make_token(TokenKind::AndAnd, start);
            has_error_ = true;
            error_ = make_error(start, "unexpected '&'");
            return {};
        case '|':
            if (match('|')) return make_token(TokenKind::OrOr, start);
            has_error_ = true;
            error_ = make_error(start, "unexpected '|'");
            return {};
        default: {
            std::string msg = "unexpected character '";
            msg += c;
            msg += "'";
            has_error_ = true;
            error_ = make_error(start, msg);
            return {};
        }
    }
}

LexResult Lexer::tokenize() {
    LexResult result;
    while (!is_at_end()) {
        skip_whitespace_and_comments();
        if (is_at_end()) break;
        char c = peek();
        Token tok;
        if (is_alpha(c)) tok = lex_identifier_or_keyword();
        else if (is_digit(c)) tok = lex_number();
        else if (c == '"') tok = lex_string();
        else tok = lex_operator_or_punct();
        if (has_error_) {
            result.error = error_;
            return result;
        }
        result.tokens.push_back(std::move(tok));
    }
    SourcePos eof = current_pos();
    result.tokens.push_back({TokenKind::EndOfFile, "", {eof, eof}});
    result.ok = true;
    return result;
}

} // namespace Lexer
