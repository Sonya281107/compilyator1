#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

import lexer;
import parser;
import semantic;
import codegen;

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "ryc: cannot open '" << path << "'\n"; std::exit(1); }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void dump_tokens(const std::vector<Lexer::Token>& tokens) {
    for (auto& t : tokens) {
        std::cout << "[" << Lexer::token_kind_name(t.kind) << "] "
                  << "'" << t.lexeme << "' "
                  << "@ " << t.span.begin.line << ":" << t.span.begin.column
                  << "\n";
    }
}

static void dump_ast(const AST::Program& prog, int indent = 0) {
    auto pad = [](int n){ return std::string(n * 2, ' '); };
    std::cout << pad(indent) << "Program (" << prog.top_decls.size() << " decls)\n";
    for (AST::DeclId did : prog.top_decls) {
        const AST::DeclNode& d = prog.decl(did);
        if (std::holds_alternative<AST::FnDecl>(d)) {
            auto& fd = std::get<AST::FnDecl>(d);
            std::cout << pad(indent + 1) << "FnDecl " << fd.name
                      << " [mangled=" << fd.mangled_name << "]\n";
        } else if (std::holds_alternative<AST::StructDecl>(d)) {
            std::cout << pad(indent + 1) << "StructDecl " << std::get<AST::StructDecl>(d).name << "\n";
        } else if (std::holds_alternative<AST::TypeAliasDecl>(d)) {
            std::cout << pad(indent + 1) << "TypeAlias " << std::get<AST::TypeAliasDecl>(d).name << "\n";
        } else if (std::holds_alternative<AST::NamespaceDecl>(d)) {
            std::cout << pad(indent + 1) << "Namespace " << std::get<AST::NamespaceDecl>(d).name << "\n";
        } else if (std::holds_alternative<AST::ImplDecl>(d)) {
            auto& id = std::get<AST::ImplDecl>(d);
            std::cout << pad(indent + 1) << "Impl " << id.struct_name
                      << " (" << id.methods.size() << " methods)\n";
        } else {
            std::cout << pad(indent + 1) << "<decl>\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Ryst compiler v0.1\n"
                  << "Usage: ryc <source.ryst> [-o <output>] [--dump-tokens] [--dump-ast]\n";
        return 1;
    }

    std::string source_path;
    std::string output_path;
    bool flag_dump_tokens = false;
    bool flag_dump_ast    = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-tokens") { flag_dump_tokens = true; }
        else if (arg == "--dump-ast") { flag_dump_ast = true; }
        else if (arg == "-o" && i + 1 < argc) { output_path = argv[++i]; }
        else if (arg[0] != '-') { source_path = arg; }
        else { std::cerr << "ryc: unknown flag '" << arg << "'\n"; return 1; }
    }

    if (source_path.empty()) {
        std::cerr << "ryc: no input file\n"; return 1;
    }

    if (output_path.empty()) {
        auto last_sep = source_path.find_last_of("/\\");
        auto start = (last_sep == std::string::npos) ? 0 : last_sep + 1;
        auto stem = source_path.substr(start);
        auto dot = stem.rfind('.');
        output_path = (dot != std::string::npos) ? stem.substr(0, dot) : stem;
    }
    std::string asm_path = output_path + ".asm";
    std::string obj_path = output_path + ".o";

    // Lex
    std::string source = read_file(source_path);
    Lexer::Lexer lexer(source, source_path);
    auto lex_result = lexer.tokenize();
    if (!lex_result.ok) {
        std::cerr << Lexer::diagnostic_to_string(lex_result.error) << "\n";
        return 1;
    }

    if (flag_dump_tokens) {
        dump_tokens(lex_result.tokens);
        return 0;
    }

    // Parse
    auto parse_result = Parser::parse(lex_result.tokens, source_path);
    if (!parse_result.ok) {
        std::cerr << Lexer::diagnostic_to_string(parse_result.error.diag) << "\n";
        return 1;
    }

    if (flag_dump_ast) {
        dump_ast(parse_result.program);
        return 0;
    }

    // Semantic analysis
    auto sem_result = Semantic::analyse(parse_result.program, source_path);
    if (!sem_result.ok) {
        for (auto& e : sem_result.errors)
            std::cerr << Lexer::diagnostic_to_string(e.diag) << "\n";
        return 1;
    }

    // Code generation
    {
        std::ofstream asm_file(asm_path);
        if (!asm_file) {
            std::cerr << "ryc: cannot write '" << asm_path << "'\n"; return 1;
        }
        auto cg_result = Codegen::generate(parse_result.program, asm_file, source_path);
        if (!cg_result.ok) {
            std::cerr << "ryc: codegen error: " << cg_result.error.message << "\n";
            return 1;
        }
    }

    // Assemble + link
    {
        std::string nasm_cmd = "nasm -f elf64 -o " + obj_path + " " + asm_path;
        if (std::system(nasm_cmd.c_str()) != 0) {
            std::cerr << "ryc: assembly failed\n"; return 1;
        }
        std::string link_cmd = "gcc -o " + output_path + " " + obj_path + " -lc -no-pie";
        if (std::system(link_cmd.c_str()) != 0) {
            std::cerr << "ryc: linking failed\n"; return 1;
        }
    }

    return 0;
}
