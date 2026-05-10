#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"
#include "lexer.hpp"

namespace Parser {

struct ParseError {
    Lexer::Diagnostic diag;
};

struct ParseResult {
    bool ok = false;
    std::unique_ptr<AST::Program> program;
    ParseError error;
};

ParseResult parse(const std::vector<Lexer::Token>& tokens, const std::string& filename);

}
