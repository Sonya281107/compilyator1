#include "parser.hpp"

#include <cassert>
#include <charconv>
#include <stdexcept>

namespace Parser {

using namespace Lexer;
using namespace AST;

static SemKind prim_from_kw(TokenKind k) {
    switch (k) {
        case TokenKind::I8Kw:     return SemKind::I8;
        case TokenKind::I16Kw:    return SemKind::I16;
        case TokenKind::I32Kw:    return SemKind::I32;
        case TokenKind::I64Kw:    return SemKind::I64;
        case TokenKind::U8Kw:     return SemKind::U8;
        case TokenKind::U16Kw:    return SemKind::U16;
        case TokenKind::U32Kw:    return SemKind::U32;
        case TokenKind::U64Kw:    return SemKind::U64;
        case TokenKind::F32Kw:    return SemKind::F32;
        case TokenKind::F64Kw:    return SemKind::F64;
        case TokenKind::BoolKw:   return SemKind::Bool;
        case TokenKind::StringKw: return SemKind::String;
        case TokenKind::VoidKw:   return SemKind::Void;
        default: return SemKind::Unknown;
    }
}

static bool is_type_kw(TokenKind k) {
    return k == TokenKind::I8Kw  || k == TokenKind::I16Kw ||
           k == TokenKind::I32Kw || k == TokenKind::I64Kw ||
           k == TokenKind::U8Kw  || k == TokenKind::U16Kw ||
           k == TokenKind::U32Kw || k == TokenKind::U64Kw ||
           k == TokenKind::F32Kw || k == TokenKind::F64Kw ||
           k == TokenKind::BoolKw || k == TokenKind::StringKw ||
           k == TokenKind::VoidKw;
}

class P {
public:
    P(const std::vector<Token>& toks, const std::string& fname)
        : tokens_(toks), filename_(fname) {}

private:
    const Token& cur() const { return tokens_[pos_]; }
    const Token& peek(std::size_t off = 1) const {
        std::size_t i = pos_ + off;
        if (i >= tokens_.size()) return tokens_.back();
        return tokens_[i];
    }
    bool at(TokenKind k) const { return cur().kind == k; }
    bool at_end()         const { return at(TokenKind::EndOfFile); }

    Token consume() {
        Token t = cur();
        if (!at_end()) ++pos_;
        return t;
    }

    bool try_eat(TokenKind k) {
        if (!at(k)) return false;
        consume();
        return true;
    }

    Token expect(TokenKind k, const std::string& what) {
        if (!at(k)) error("expected " + what + ", got '" + cur().lexeme + "'");
        return consume();
    }

    [[noreturn]] void error(const std::string& msg) {
        Diagnostic d;
        d.filename = filename_;
        d.pos      = cur().span.begin;
        d.message  = msg;
        throw ParseError{d};
    }

    TypePtr parse_type() {
        SourceSpan span;
        span.begin = cur().span.begin;

        if (is_type_kw(cur().kind)) {
            auto n  = std::make_unique<PrimTypeNode>();
            n->prim = prim_from_kw(cur().kind);
            n->span = {span.begin, cur().span.end};
            consume();
            return n;
        }

        if (at(TokenKind::Identifier)) {
            auto n  = std::make_unique<NamedTypeNode>();
            n->name = cur().lexeme;
            n->span = cur().span;
            consume();
            return n;
        }

        if (at(TokenKind::LBracket)) {
            consume();
            auto n    = std::make_unique<ArrayTypeNode>();
            n->elem   = parse_type();
            expect(TokenKind::Semicolon, "';'");
            if (!at(TokenKind::IntLiteral)) error("expected array size");
            n->size   = static_cast<std::size_t>(std::stoul(cur().lexeme));
            n->span.end = cur().span.end;
            consume();
            expect(TokenKind::RBracket, "']'");
            return n;
        }

        error("expected type");
    }

    struct InfixInfo { int prec; bool right_assoc; };
    InfixInfo infix(TokenKind k) const {
        switch (k) {
            case TokenKind::Assign:    return {1, true};
            case TokenKind::OrOr:      return {2, false};
            case TokenKind::AndAnd:    return {3, false};
            case TokenKind::EqEq:
            case TokenKind::NotEq:
            case TokenKind::Less:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq: return {4, false};
            case TokenKind::Plus:
            case TokenKind::Minus:     return {5, false};
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:   return {6, false};
            case TokenKind::AsKw:      return {7, false};
            default:                   return {0, false};
        }
    }

    ExprPtr parse_expr(int min_prec = 0) {
        ExprPtr left = parse_unary();

        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= min_prec) break;

            SourceSpan span = cur().span;

            if (cur().kind == TokenKind::Assign) {
                consume();
                auto rhs = parse_expr(prec - 1);
                SourceSpan full{left->span.begin, rhs->span.end};
                auto node      = std::make_unique<AssignExpr>();
                node->span     = full;
                node->target   = std::move(left);
                node->value    = std::move(rhs);
                left = std::move(node);
                continue;
            }

            if (cur().kind == TokenKind::AsKw) {
                consume();
                TypePtr target = parse_type();
                auto node      = std::make_unique<CastExpr>();
                node->span     = {left->span.begin, target->span.end};
                node->operand  = std::move(left);
                node->target   = std::move(target);
                left = std::move(node);
                continue;
            }

            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            auto rhs = parse_expr(next_min);
            SourceSpan full{left->span.begin, rhs->span.end};
            auto node    = std::make_unique<BinaryExpr>();
            node->span   = full;
            node->op     = op;
            node->left   = std::move(left);
            node->right  = std::move(rhs);
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_unary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            SourcePos start = cur().span.begin;
            std::string op = cur().lexeme;
            consume();
            auto operand = parse_unary();
            auto node    = std::make_unique<UnaryExpr>();
            node->span   = {start, operand->span.end};
            node->op     = op;
            node->operand= std::move(operand);
            return node;
        }
        return parse_postfix(parse_primary());
    }

    ExprPtr parse_postfix(ExprPtr left) {
        while (true) {
            if (at(TokenKind::LBracket)) {
                SourcePos start = left->span.begin;
                consume();
                auto idx = parse_expr();
                expect(TokenKind::RBracket, "']'");
                auto node   = std::make_unique<IndexExpr>();
                node->span  = {start, cur().span.begin};
                node->array = std::move(left);
                node->index = std::move(idx);
                left = std::move(node);
                continue;
            }

            if (at(TokenKind::Dot)) {
                SourcePos start = left->span.begin;
                consume();
                if (!at(TokenKind::Identifier)) error("expected field name after '.'");
                std::string fname = cur().lexeme;
                SourcePos end = cur().span.end;
                consume();

                if (at(TokenKind::LParen)) {
                    consume();
                    auto node       = std::make_unique<CallExpr>();
                    auto fe         = std::make_unique<FieldExpr>();
                    fe->span        = {start, end};
                    fe->field       = fname;
                    fe->object      = std::move(left);
                    node->callee    = std::move(fe);
                    node->args      = parse_arg_list();
                    expect(TokenKind::RParen, "')'");
                    node->span      = {start, cur().span.begin};
                    left = std::move(node);
                } else {
                    auto node   = std::make_unique<FieldExpr>();
                    node->span  = {start, end};
                    node->field = fname;
                    node->object= std::move(left);
                    left = std::move(node);
                }
                continue;
            }

            if (at(TokenKind::ColonColon)) {
                SourcePos start = left->span.begin;
                consume();
                if (!at(TokenKind::Identifier)) error("expected name after '::'");
                std::string ns_name;
                if (auto* id = dynamic_cast<IdentExpr*>(left.get()))
                    ns_name = id->name;
                else
                    error("'::' must follow a namespace name");

                std::string member = cur().lexeme;
                SourcePos end = cur().span.end;
                consume();

                if (at(TokenKind::LParen)) {
                    consume();
                    auto se      = std::make_unique<ScopeExpr>();
                    se->span     = {start, end};
                    se->ns       = ns_name;
                    se->name     = member;
                    auto node    = std::make_unique<CallExpr>();
                    node->callee = std::move(se);
                    node->args   = parse_arg_list();
                    expect(TokenKind::RParen, "')'");
                    node->span   = {start, cur().span.begin};
                    left = std::move(node);
                } else {
                    auto node  = std::make_unique<ScopeExpr>();
                    node->span = {start, end};
                    node->ns   = ns_name;
                    node->name = member;
                    left = std::move(node);
                }
                continue;
            }

            if (at(TokenKind::LParen)) {
                SourcePos start = left->span.begin;
                consume();
                auto node    = std::make_unique<CallExpr>();
                node->callee = std::move(left);
                node->args   = parse_arg_list();
                expect(TokenKind::RParen, "')'");
                node->span   = {start, cur().span.begin};
                left = std::move(node);
                continue;
            }

            break;
        }
        return left;
    }

    std::vector<ExprPtr> parse_arg_list() {
        std::vector<ExprPtr> args;
        if (at(TokenKind::RParen)) return args;
        args.push_back(parse_expr());
        while (try_eat(TokenKind::Comma))
            args.push_back(parse_expr());
        return args;
    }

    ExprPtr parse_primary(bool allow_struct_lit = true) {
        SourcePos start = cur().span.begin;

        if (at(TokenKind::IntLiteral)) {
            int64_t val = 0;
            std::from_chars(cur().lexeme.data(),
                            cur().lexeme.data() + cur().lexeme.size(), val);
            auto n  = std::make_unique<IntLitExpr>();
            n->span = cur().span;
            n->value= val;
            consume();
            return n;
        }

        if (at(TokenKind::FloatLiteral)) {
            double val = std::stod(cur().lexeme);
            auto n  = std::make_unique<FloatLitExpr>();
            n->span = cur().span;
            n->value= val;
            consume();
            return n;
        }

        if (at(TokenKind::StringLiteral)) {
            const std::string& raw = cur().lexeme;
            std::string val;
            val.reserve(raw.size());
            for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
                if (raw[i] == '\\' && i + 2 < raw.size()) {
                    ++i;
                    switch (raw[i]) {
                        case 'n':  val += '\n'; break;
                        case 't':  val += '\t'; break;
                        case 'r':  val += '\r'; break;
                        case '0':  val += '\0'; break;
                        default:   val += raw[i]; break;
                    }
                } else { val += raw[i]; }
            }
            auto n  = std::make_unique<StringLitExpr>();
            n->span = cur().span;
            n->value= std::move(val);
            consume();
            return n;
        }

        if (at(TokenKind::TrueKw) || at(TokenKind::FalseKw)) {
            auto n  = std::make_unique<BoolLitExpr>();
            n->span = cur().span;
            n->value= at(TokenKind::TrueKw);
            consume();
            return n;
        }

        if (at(TokenKind::LParen)) {
            consume();
            auto e = parse_expr();
            expect(TokenKind::RParen, "')'");
            return e;
        }

        if (at(TokenKind::LBracket)) {
            SourcePos arr_start = cur().span.begin;
            consume();
            auto n = std::make_unique<ArrayLitExpr>();
            if (!at(TokenKind::RBracket)) {
                n->elements.push_back(parse_expr());
                while (try_eat(TokenKind::Comma) && !at(TokenKind::RBracket))
                    n->elements.push_back(parse_expr());
            }
            n->span = {arr_start, cur().span.end};
            expect(TokenKind::RBracket, "']'");
            return n;
        }

        if (at(TokenKind::Identifier)) {
            std::string name = cur().lexeme;
            SourceSpan id_span = cur().span;
            consume();

            if (allow_struct_lit && at(TokenKind::LBrace)) {
                consume();
                auto n  = std::make_unique<StructLitExpr>();
                n->name = name;
                while (!at(TokenKind::RBrace) && !at_end()) {
                    if (!at(TokenKind::Identifier)) error("expected field name");
                    std::string fname = cur().lexeme;
                    consume();
                    expect(TokenKind::Colon, "':'");
                    auto val = parse_expr();
                    n->fields.emplace_back(fname, std::move(val));
                    if (!try_eat(TokenKind::Comma)) break;
                }
                n->span = {id_span.begin, cur().span.end};
                expect(TokenKind::RBrace, "'}'");
                return n;
            }

            auto n  = std::make_unique<IdentExpr>();
            n->span = id_span;
            n->name = name;
            return n;
        }

        error("unexpected token '" + cur().lexeme + "' in expression");
    }

    ExprPtr parse_cond_expr() {
        ExprPtr left = parse_cond_unary();
        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= 0) break;
            if (cur().kind == TokenKind::Assign) break;
            if (cur().kind == TokenKind::AsKw) {
                consume();
                TypePtr tgt = parse_type();
                auto node     = std::make_unique<CastExpr>();
                node->span    = {left->span.begin, tgt->span.end};
                node->operand = std::move(left);
                node->target  = std::move(tgt);
                left = std::move(node);
                continue;
            }
            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            auto rhs = parse_cond_expr_at(next_min);
            auto node  = std::make_unique<BinaryExpr>();
            node->span = {left->span.begin, rhs->span.end};
            node->op   = op;
            node->left = std::move(left);
            node->right= std::move(rhs);
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_cond_expr_at(int min_prec) {
        ExprPtr left = parse_cond_unary();
        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= min_prec) break;
            if (cur().kind == TokenKind::Assign) break;
            if (cur().kind == TokenKind::AsKw) {
                consume();
                TypePtr tgt = parse_type();
                auto node     = std::make_unique<CastExpr>();
                node->span    = {left->span.begin, tgt->span.end};
                node->operand = std::move(left);
                node->target  = std::move(tgt);
                left = std::move(node);
                continue;
            }
            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            auto rhs  = parse_cond_expr_at(next_min);
            auto node = std::make_unique<BinaryExpr>();
            node->span= {left->span.begin, rhs->span.end};
            node->op  = op;
            node->left= std::move(left);
            node->right=std::move(rhs);
            left = std::move(node);
        }
        return left;
    }

    ExprPtr parse_cond_unary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            SourcePos s = cur().span.begin;
            std::string op = cur().lexeme;
            consume();
            auto operand = parse_cond_unary();
            auto n       = std::make_unique<UnaryExpr>();
            n->span      = {s, operand->span.end};
            n->op        = op;
            n->operand   = std::move(operand);
            return n;
        }
        return parse_postfix(parse_primary(false));
    }

    StmtPtr parse_stmt() {
        SourcePos start = cur().span.begin;

        if (at(TokenKind::Semicolon)) {
            consume();
            auto n  = std::make_unique<NullStmt>();
            n->span = {start, cur().span.begin};
            return n;
        }

        if (at(TokenKind::LBrace))  return parse_block();
        if (at(TokenKind::LetKw))   return parse_var_decl();
        if (at(TokenKind::IfKw))    return parse_if();
        if (at(TokenKind::WhileKw)) return parse_while();

        if (at(TokenKind::ReturnKw)) {
            consume();
            auto n = std::make_unique<ReturnStmt>();
            n->span.begin = start;
            if (!at(TokenKind::Semicolon)) n->value = parse_expr();
            n->span.end = cur().span.begin;
            expect(TokenKind::Semicolon, "';'");
            return n;
        }

        if (at(TokenKind::BreakKw)) {
            consume();
            expect(TokenKind::Semicolon, "';'");
            auto n  = std::make_unique<BreakStmt>();
            n->span = {start, cur().span.begin};
            return n;
        }

        if (at(TokenKind::ContinueKw)) {
            consume();
            expect(TokenKind::Semicolon, "';'");
            auto n  = std::make_unique<ContinueStmt>();
            n->span = {start, cur().span.begin};
            return n;
        }

        auto e  = parse_expr();
        auto es = std::make_unique<ExprStmt>();
        es->span= {start, cur().span.begin};
        es->expr= std::move(e);
        expect(TokenKind::Semicolon, "';'");
        return es;
    }

    std::unique_ptr<BlockStmt> parse_block() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::LBrace, "'{'");
        auto blk = std::make_unique<BlockStmt>();
        while (!at(TokenKind::RBrace) && !at_end())
            blk->stmts.push_back(parse_stmt());
        blk->span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return blk;
    }

    StmtPtr parse_var_decl() {
        SourcePos start = cur().span.begin;
        consume();
        bool is_mut = try_eat(TokenKind::MutKw);
        if (!at(TokenKind::Identifier)) error("expected variable name");
        std::string name = cur().lexeme;
        consume();

        std::optional<TypePtr> type_ann;
        if (try_eat(TokenKind::Colon)) type_ann = parse_type();

        expect(TokenKind::Assign, "'='");
        auto init = parse_expr();
        expect(TokenKind::Semicolon, "';'");

        auto n      = std::make_unique<VarDeclStmt>();
        n->span     = {start, cur().span.begin};
        n->is_mut   = is_mut;
        n->name     = name;
        n->type_ann = std::move(type_ann);
        n->init     = std::move(init);
        return n;
    }

    StmtPtr parse_if() {
        SourcePos start = cur().span.begin;
        consume();
        auto cond       = parse_cond_expr();
        auto then_block = parse_block();
        auto n          = std::make_unique<IfStmt>();
        n->span.begin   = start;
        n->cond         = std::move(cond);
        n->then_block   = std::move(then_block);

        if (try_eat(TokenKind::ElseKw)) {
            if (at(TokenKind::IfKw)) {
                n->else_stmt = parse_if();
            } else {
                n->else_stmt = parse_block();
            }
        }
        n->span.end = cur().span.begin;
        return n;
    }

    StmtPtr parse_while() {
        SourcePos start = cur().span.begin;
        consume();
        auto cond = parse_cond_expr();
        auto body = parse_block();
        auto n    = std::make_unique<WhileStmt>();
        n->span   = {start, cur().span.begin};
        n->cond   = std::move(cond);
        n->body   = std::move(body);
        return n;
    }

    DeclPtr parse_decl() {
        if (at(TokenKind::FnKw))        return parse_fn_decl("");
        if (at(TokenKind::StructKw))    return parse_struct_decl();
        if (at(TokenKind::TypeKw))      return parse_type_alias();
        if (at(TokenKind::NamespaceKw)) return parse_namespace();
        if (at(TokenKind::ImplKw))      return parse_impl();
        error("expected declaration (fn, struct, type, namespace, impl)");
    }

    std::unique_ptr<FnDecl> parse_fn_decl(const std::string& ns_prefix) {
        SourcePos start = cur().span.begin;
        expect(TokenKind::FnKw, "'fn'");
        if (!at(TokenKind::Identifier)) error("expected function name");
        std::string name = cur().lexeme;
        consume();

        expect(TokenKind::LParen, "'('");
        std::vector<Param> params;
        while (!at(TokenKind::RParen) && !at_end()) {
            Param p;
            p.span.begin = cur().span.begin;
            if (!at(TokenKind::Identifier)) error("expected parameter name");
            p.name = cur().lexeme;
            consume();
            expect(TokenKind::Colon, "':'");
            p.type     = parse_type();
            p.span.end = p.type->span.end;
            params.push_back(std::move(p));
            if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "')'");

        std::optional<TypePtr> ret_type;
        if (try_eat(TokenKind::Arrow)) ret_type = parse_type();

        auto body = parse_block();

        auto fn            = std::make_unique<FnDecl>();
        fn->span           = {start, cur().span.begin};
        fn->name           = name;
        fn->params         = std::move(params);
        fn->return_type    = std::move(ret_type);
        fn->body           = std::move(body);
        fn->mangled_name   = ns_prefix.empty() ? name : ns_prefix + "__" + name;
        return fn;
    }

    DeclPtr parse_struct_decl() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::StructKw, "'struct'");
        if (!at(TokenKind::Identifier)) error("expected struct name");
        std::string name = cur().lexeme;
        consume();

        expect(TokenKind::LBrace, "'{'");
        std::vector<StructField> fields;
        while (!at(TokenKind::RBrace) && !at_end()) {
            StructField f;
            f.span.begin = cur().span.begin;
            if (!at(TokenKind::Identifier)) error("expected field name");
            f.name = cur().lexeme;
            consume();
            expect(TokenKind::Colon, "':'");
            f.type     = parse_type();
            f.span.end = f.type->span.end;
            fields.push_back(std::move(f));
            if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBrace, "'}'");

        auto sd    = std::make_unique<StructDecl>();
        sd->span   = {start, cur().span.begin};
        sd->name   = name;
        sd->fields = std::move(fields);
        return sd;
    }

    DeclPtr parse_type_alias() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::TypeKw, "'type'");
        if (!at(TokenKind::Identifier)) error("expected alias name");
        std::string name = cur().lexeme;
        consume();
        expect(TokenKind::Assign, "'='");
        auto type = parse_type();
        expect(TokenKind::Semicolon, "';'");
        auto ta    = std::make_unique<TypeAliasDecl>();
        ta->span   = {start, cur().span.begin};
        ta->name   = name;
        ta->type   = std::move(type);
        return ta;
    }

    DeclPtr parse_namespace() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::NamespaceKw, "'namespace'");
        if (!at(TokenKind::Identifier)) error("expected namespace name");
        std::string name = cur().lexeme;
        consume();
        expect(TokenKind::LBrace, "'{'");
        auto nd  = std::make_unique<NamespaceDecl>();
        nd->name = name;
        while (!at(TokenKind::RBrace) && !at_end()) {
            if (at(TokenKind::FnKw)) {
                nd->decls.push_back(parse_fn_decl(name));
            } else {
                nd->decls.push_back(parse_decl());
            }
        }
        nd->span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return nd;
    }

    DeclPtr parse_impl() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::ImplKw, "'impl'");
        if (!at(TokenKind::Identifier)) error("expected struct name after 'impl'");
        std::string sname = cur().lexeme;
        consume();
        expect(TokenKind::LBrace, "'{'");
        auto impl = std::make_unique<ImplDecl>();
        impl->struct_name = sname;
        while (!at(TokenKind::RBrace) && !at_end()) {
            if (!at(TokenKind::FnKw)) error("expected 'fn' inside impl block");
            impl->methods.push_back(parse_fn_decl(sname));
        }
        impl->span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return impl;
    }

public:
    std::unique_ptr<Program> parse_program() {
        auto prog = std::make_unique<Program>();
        while (!at_end()) prog->decls.push_back(parse_decl());
        return prog;
    }

    const std::vector<Token>& tokens_;
    const std::string&        filename_;
    std::size_t               pos_ = 0;
};

ParseResult parse(const std::vector<Token>& tokens, const std::string& filename) {
    P parser(tokens, filename);
    try {
        auto prog  = parser.parse_program();
        ParseResult r;
        r.ok       = true;
        r.program  = std::move(prog);
        return r;
    } catch (const ParseError& e) {
        ParseResult r;
        r.ok    = false;
        r.error = e;
        return r;
    }
}

}
