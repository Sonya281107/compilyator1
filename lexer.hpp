#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "token.hpp"

namespace Lexer {
    struct Diagnostic{
        std::string filename {};
        SourcePos pos {};
        std::string message {};
    };

    struct LexResult {
        bool ok = false;
        std::vector<Token> tokens {};
        Diagnostic error;
    };

    std::string diagnostic_to_string(const Diagnostic& diagnostic);

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
            TokenKind identifier_kind(std::string_view text) const;
            std::string_view source_;
            std::string filename_;
            std::size_t index_ = 0;
            std::size_t line_ = 1;
            std::size_t column_ = -1;
            bool has_error_ = false;
            Diagnostic error_ {};
    };
}