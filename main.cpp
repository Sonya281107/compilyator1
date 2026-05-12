#include <iostream>
#include "lexer.hpp"
#include "parser.hpp"

static void print_tokens(const std::vector<Lexer::Token>& tokens) {
    for (const Lexer::Token& tok : tokens) {
        std::cout << "[" << tok.span.begin.line << ":" << tok.span.begin.column << "] "
                  << Lexer::token_kind_name(tok.kind)
                  << " => \"" << tok.lexeme << "\"\n";
    }
}

static void count_decls(const AST::Program& prog) {
    std::cout << "Parsed " << prog.top_decls.size() << " top-level declaration(s).\n";
    for (AST::DeclId did : prog.top_decls) {
        const AST::DeclNode& d = prog.decl(did);
        if (std::holds_alternative<AST::FnDecl>(d)) {
            auto& fn = std::get<AST::FnDecl>(d);
            std::cout << "  fn " << fn.name << " (" << fn.params.size() << " param(s))\n";
        } else if (std::holds_alternative<AST::StructDecl>(d)) {
            auto& st = std::get<AST::StructDecl>(d);
            std::cout << "  struct " << st.name << " (" << st.fields.size() << " field(s))\n";
        } else if (std::holds_alternative<AST::NamespaceDecl>(d)) {
            auto& ns = std::get<AST::NamespaceDecl>(d);
            std::cout << "  namespace " << ns.name << "\n";
        } else if (std::holds_alternative<AST::TypeAliasDecl>(d)) {
            auto& ta = std::get<AST::TypeAliasDecl>(d);
            std::cout << "  type " << ta.name << "\n";
        }
    }
}

int main() {
    std::string source = R"(
struct Point {
    x: f64,
    y: f64
}

fn add(a: i32, b: i32): i32 {
    return a + b;
}

fn main(): i32 {
    let mut p: Point = Point { x: 1.0, y: 2.5 };
    let result: i32 = add(3, 4);
    if result >= 5 {
        return 1;
    }
    return 0;
}
)";

    Lexer::Lexer lexer(source, "<test>");
    Lexer::LexResult lex = lexer.tokenize();

    if (!lex.ok) {
        std::cerr << "Lex error: " << Lexer::diagnostic_to_string(lex.error) << "\n";
        return 1;
    }

    std::cout << "=== TOKENS ===\n";
    print_tokens(lex.tokens);

    std::cout << "\n=== PARSE ===\n";
    Parser::ParseResult parse = Parser::parse(lex.tokens, "<test>");

    if (!parse.ok) {
        std::cerr << "Parse error: " << Lexer::diagnostic_to_string(parse.error.diag) << "\n";
        return 1;
    }

    count_decls(parse.program);
    return 0;
}
