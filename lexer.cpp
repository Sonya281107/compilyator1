#include "lexer.hpp"

namespace Lexer{
    std::string diagnostic_to_string(const Diagnostic& diagnostic){
        return diagnostic.filename + ":" + std::to_string(diagnostic.pos.line) + ":" + std::to_string(diagnostic.pos.column) + ": error: " + diagnostic.message;
    }

    Lexer::Lexer(std::string_view source, std::string filename) : source_(source), filename_(std::move(filename)){
    }

    LexResult Lexer::tokenize(){
        LexResult result;
        result.ok = false;
        std::vector<Token> tokens;
        while(!is_at_end()){
            skip_whitespace_and_comments();
            if(is_at_end()) break;
            char c = peek();
            Token token;
            if(is_alpha(c)){
                token = lex_identifier_or_keyword();
            } else if(is_digit(c)){
                token = lex_number();
            } else if(c == '"'){
                token = lex_string();
            } else {
                token = lex_operator_or_punct();
            }

            if(has_error_){
                result.error = error_;
                return result;
            }

            tokens.push_back(token);
        }
        SourcePos eof_pos = current_pos();
        Token eof_token;
        eof_token.kind = TokenKind::EndOfFile;
        eof_token.lexeme = "";
        eof_token.span.begin = eof_pos;
        eof_token.span.end = eof_pos;
        tokens.push_back(eof_token);
        result.ok = true;
        result.tokens = std::move(tokens);
        return result;
    }

    char Lexer::advance(){
        if(is_at_end()) return '\0';
        char c = source_[index_];
        ++index_;

        if(c == '\n'){
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }

        return c;
    }

    bool Lexer::match(char expected){
        if(is_at_end()) return false;
        if(source_[index_] != expected) return false;
        advance();
        return true;
    }

    bool Lexer::is_at_end() const{
        return index_ >= source_.size();
    }

    SourcePos Lexer::current_pos() const{
        SourcePos pos;
        pos.index = index_;
        pos.line = line_;
        pos.column = column_;
        return pos;
    }

    void Lexer::skip_whitespace_and_comments(){
        while(!is_at_end()){
            char c = peek();
            if(c == ' ' || c == '\t' || c == '\r' || c == '\n'){
                advance();
                continue;
            }

            if(c == '/' && peek(1) == '/'){
                advance();
                advance();

                while(!is_at_end() && peek() != '\n') advance();
                continue;
            }
            break;
        }
    }

    Token Lexer::lex_identifier_or_keyword(){
        SourcePos start = current_pos();

        while (!is_at_end() && is_alnum(peek())) advance();
        Token token = make_token(TokenKind::Identifier, start);
        token.kind = identifier_kind(token.lexeme);
        return token;
    }

    Token Lexer::lex_number(){
        SourcePos start = current_pos();
        while(!is_at_end() && is_digit(peek())) advance();
        bool is_float = false;
        if(!is_at_end() && peek() == '.' && is_digit(peek(1))){
            is_float = true;
            advance();
            while(!is_at_end() && is_digit(peek())) advance();
        }
        if(is_float)return make_token(TokenKind::FloatLiteral, start);
        return make_token(TokenKind::IntLiteral, start);
    }

    Token Lexer::lex_string(){
        SourcePos start = current_pos();
        advance();
        while(!is_at_end()){
            char c = peek();
            if(c == '"'){
                advance();
                return make_token(TokenKind::StringLiteral, start);
            }

            if(c == '\n'){
                has_error_ = true;
                error_ = make_error(current_pos(), "unterminated string literal");
                return Token{};
            }

            if(c == '\\'){
                advance();
                if(is_at_end()){
                    has_error_ = true;
                    error_ = make_error (current_pos(), "unterminated string literal");
                    return Token{};
                }

                char escaped = peek();

                if(escaped == '"' || escaped == '\\' || escaped == 'n' || escaped == 't') {
                    advance();
                    continue;
                } 

                has_error_ = true; 
                error_ = make_error(current_pos(), "invalid escape sequence");
                return Token{};
            }
            advance();
        }
        
        has_error_ = true;
        error_ = make_error(start, "unterminated string literal");
        return Token{};
    }
    
    Token Lexer::lex_operator_or_punct(){
        SourcePos start = current_pos();
        char c = advance();

        switch (c){
            case '(': return make_token(TokenKind::LParen, start);
            case ')': return make_token(TokenKind::RParen, start);
            case '{': return make_token(TokenKind::LBrace, start);
            case '}': return make_token(TokenKind::RBrace, start);
            case '[': return make_token(TokenKind::LBracket, start);
            case ']': return make_token(TokenKind::RBracket, start);
            case ',': return make_token(TokenKind::Comma, start);
            case ';': return make_token(TokenKind::Semicolon, start);
            case '+': return make_token(TokenKind::Plus, start);
            case '-': return make_token(TokenKind::Minus, start);
            case '*': return make_token(TokenKind::Star, start);
            case '/': return make_token(TokenKind::Slash, start);
            case '%': return make_token(TokenKind::Percent, start);
            case '!':
                if(match('=')) return make_token(TokenKind::NotEq, start);
                return make_token(TokenKind::Bang, start);
            case '=':
                if(match('=')) return make_token(TokenKind::EqEq, start);
                return make_token(TokenKind::Assign, start);
            case '>':
                if(match('=')) return make_token(TokenKind::GreaterEq, start);
                return make_token(TokenKind::Greater, start);
            case '<':
                if(match('=')) return make_token(TokenKind::LessEq, start);
                return make_token(TokenKind::Less, start);
            case '&':
                if(match('&')) return make_token(TokenKind::AndAnd, start);
                has_error_ = true;
                error_ = make_error(start, "unexpected character '&'");
                return Token{};
            case '|':
                if(match('|')) return make_token(TokenKind::OrOr, start);
                has_error_ = true;
                error_ = make_error(start, "unexpected character '|'");
                return Token{};
            default:
                return make_token(TokenKind::Invalid, start);
        }
    }

    Token Lexer::make_token(TokenKind kind, SourcePos start) const{
        Token token;
        token.kind = kind;
        token.span.begin = start;
        token.span.end = current_pos();

        std::size_t length = token.span.end.index - token.span.begin.index;
        token.lexeme = std::string(source_.substr(start.index, length));
        return token;
    }

    Diagnostic Lexer::make_error(SourcePos pos, std::string message) const {
        Diagnostic diagnostic;
        diagnostic.filename = filename_;
        diagnostic.pos = pos;
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    bool Lexer::is_alpha(char c) const{
        return(c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool Lexer::is_digit(char c) const {
        return c >= '0' && c <= '9';
    }

    bool Lexer::is_alnum(char c) const {
        return is_alpha(c) || is_digit(c);
    }

    TokenKind Lexer::identifier_kind(std::string_view text) const {
        if(text == "fn") return TokenKind::FnKw;
        if(text == "let") return TokenKind::LetKw;
        if(text == "mut") return TokenKind::MutKw;
        if(text == "struct") return TokenKind::StructKw;
        if(text == "type") return TokenKind::TypeKw;
        if(text == "namespace") return TokenKind::NamespaceKw;
        if(text == "if") return TokenKind::IfKw;
        if(text == "else") return TokenKind::ElseKw;
        if(text == "while") return TokenKind::WhileKw;
        if(text == "break") return TokenKind::BreakKw;
        if(text == "continue") return TokenKind::ContinueKw;
        if(text == "return") return TokenKind::ReturnKw;
        if(text == "true") return TokenKind::TrueKw;
        if(text == "false") return TokenKind::FalseKw;
        if(text == "void") return TokenKind::VoidKw;
        return TokenKind::Identifier;
    }
}