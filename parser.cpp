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
    P(const std::vector<Token>& toks, const std::string& fname, Program& prog)
        : tokens_(toks), filename_(fname), prog_(prog) {}

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

    TypeId parse_type() {
        SourceSpan span;
        span.begin = cur().span.begin;

        if (is_type_kw(cur().kind)) {
            PrimTypeNode n;
            n.prim = prim_from_kw(cur().kind);
            n.span = {span.begin, cur().span.end};
            consume();
            return prog_.add_type(n);
        }

        if (at(TokenKind::Identifier)) {
            NamedTypeNode n;
            n.name = cur().lexeme;
            n.span = cur().span;
            consume();
            return prog_.add_type(n);
        }

        if (at(TokenKind::LBracket)) {
            consume();
            ArrayTypeNode n;
            n.elem = parse_type();
            expect(TokenKind::Semicolon, "';'");
            if (!at(TokenKind::IntLiteral)) error("expected array size");
            n.size     = static_cast<std::size_t>(std::stoul(cur().lexeme));
            n.span.end = cur().span.end;
            consume();
            expect(TokenKind::RBracket, "']'");
            return prog_.add_type(n);
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

    ExprId parse_expr(int min_prec = 0) {
        ExprId left = parse_unary();

        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= min_prec) break;

            if (cur().kind == TokenKind::Assign) {
                consume();
                ExprId rhs = parse_expr(prec - 1);
                SourcePos left_begin = expr_span(prog_.expr(left)).begin;
                SourcePos rhs_end    = expr_span(prog_.expr(rhs)).end;
                AssignExpr node;
                node.span   = {left_begin, rhs_end};
                node.target = left;
                node.value  = rhs;
                left = prog_.add_expr(std::move(node));
                continue;
            }

            if (cur().kind == TokenKind::AsKw) {
                SourcePos left_begin = expr_span(prog_.expr(left)).begin;
                consume();
                TypeId target_id = parse_type();
                SourcePos target_end = type_span(prog_.type(target_id)).end;
                CastExpr node;
                node.span    = {left_begin, target_end};
                node.operand = left;
                node.target  = target_id;
                left = prog_.add_expr(std::move(node));
                continue;
            }

            SourcePos left_begin = expr_span(prog_.expr(left)).begin;
            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            ExprId rhs = parse_expr(next_min);
            SourcePos rhs_end = expr_span(prog_.expr(rhs)).end;
            BinaryExpr node;
            node.span  = {left_begin, rhs_end};
            node.op    = op;
            node.left  = left;
            node.right = rhs;
            left = prog_.add_expr(std::move(node));
        }
        return left;
    }

    ExprId parse_unary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            SourcePos start = cur().span.begin;
            std::string op  = cur().lexeme;
            consume();
            ExprId operand = parse_unary();
            SourcePos operand_end = expr_span(prog_.expr(operand)).end;
            UnaryExpr node;
            node.span    = {start, operand_end};
            node.op      = op;
            node.operand = operand;
            return prog_.add_expr(std::move(node));
        }
        return parse_postfix(parse_primary());
    }

    ExprId parse_postfix(ExprId left) {
        while (true) {
            if (at(TokenKind::LBracket)) {
                SourcePos start = expr_span(prog_.expr(left)).begin;
                consume();
                ExprId idx = parse_expr();
                expect(TokenKind::RBracket, "']'");
                IndexExpr node;
                node.span  = {start, cur().span.begin};
                node.array = left;
                node.index = idx;
                left = prog_.add_expr(std::move(node));
                continue;
            }

            if (at(TokenKind::ColonColon)) {
                SourcePos start = expr_span(prog_.expr(left)).begin;
                consume();
                if (!at(TokenKind::Identifier)) error("expected name after '::'");

                std::string ns_name;
                if (std::holds_alternative<IdentExpr>(prog_.expr(left)))
                    ns_name = std::get<IdentExpr>(prog_.expr(left)).name;
                else
                    error("'::' must follow a namespace name");

                std::string member = cur().lexeme;
                SourcePos end = cur().span.end;
                consume();

                if (at(TokenKind::LParen)) {
                    consume();
                    ScopeExpr se;
                    se.span = {start, end};
                    se.ns   = ns_name;
                    se.name = member;
                    ExprId se_id = prog_.add_expr(std::move(se));
                    CallExpr node;
                    node.callee = se_id;
                    node.args   = parse_arg_list();
                    expect(TokenKind::RParen, "')'");
                    node.span = {start, cur().span.begin};
                    left = prog_.add_expr(std::move(node));
                } else {
                    ScopeExpr node;
                    node.span = {start, end};
                    node.ns   = ns_name;
                    node.name = member;
                    left = prog_.add_expr(std::move(node));
                }
                continue;
            }

            if (at(TokenKind::LParen)) {
                SourcePos start = expr_span(prog_.expr(left)).begin;
                consume();
                CallExpr node;
                node.callee = left;
                node.args   = parse_arg_list();
                expect(TokenKind::RParen, "')'");
                node.span = {start, cur().span.begin};
                left = prog_.add_expr(std::move(node));
                continue;
            }

            break;
        }
        return left;
    }

    std::vector<ExprId> parse_arg_list() {
        std::vector<ExprId> args;
        if (at(TokenKind::RParen)) return args;
        args.push_back(parse_expr());
        while (try_eat(TokenKind::Comma))
            args.push_back(parse_expr());
        return args;
    }

    ExprId parse_primary(bool allow_struct_lit = true) {
        SourcePos start = cur().span.begin;

        if (at(TokenKind::IntLiteral)) {
            int64_t val = 0;
            std::from_chars(cur().lexeme.data(),
                            cur().lexeme.data() + cur().lexeme.size(), val);
            IntLitExpr n;
            n.span  = cur().span;
            n.value = val;
            consume();
            return prog_.add_expr(std::move(n));
        }

        if (at(TokenKind::FloatLiteral)) {
            double val = std::stod(cur().lexeme);
            FloatLitExpr n;
            n.span  = cur().span;
            n.value = val;
            consume();
            return prog_.add_expr(std::move(n));
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
            StringLitExpr n;
            n.span  = cur().span;
            n.value = std::move(val);
            consume();
            return prog_.add_expr(std::move(n));
        }

        if (at(TokenKind::TrueKw) || at(TokenKind::FalseKw)) {
            BoolLitExpr n;
            n.span  = cur().span;
            n.value = at(TokenKind::TrueKw);
            consume();
            return prog_.add_expr(std::move(n));
        }

        if (at(TokenKind::LParen)) {
            consume();
            ExprId e = parse_expr();
            expect(TokenKind::RParen, "')'");
            return e;
        }

        if (at(TokenKind::LBracket)) {
            SourcePos arr_start = cur().span.begin;
            consume();
            ArrayLitExpr n;
            if (!at(TokenKind::RBracket)) {
                n.elements.push_back(parse_expr());
                while (try_eat(TokenKind::Comma) && !at(TokenKind::RBracket))
                    n.elements.push_back(parse_expr());
            }
            n.span = {arr_start, cur().span.end};
            expect(TokenKind::RBracket, "']'");
            return prog_.add_expr(std::move(n));
        }

        if (at(TokenKind::Identifier)) {
            std::string name   = cur().lexeme;
            SourceSpan id_span = cur().span;
            consume();

            if (allow_struct_lit && at(TokenKind::LBrace)) {
                consume();
                StructLitExpr n;
                n.name = name;
                while (!at(TokenKind::RBrace) && !at_end()) {
                    if (!at(TokenKind::Identifier)) error("expected field name");
                    std::string fname = cur().lexeme;
                    consume();
                    expect(TokenKind::Colon, "':'");
                    ExprId val = parse_expr();
                    n.fields.emplace_back(fname, val);
                    if (!try_eat(TokenKind::Comma)) break;
                }
                n.span = {id_span.begin, cur().span.end};
                expect(TokenKind::RBrace, "'}'");
                return prog_.add_expr(std::move(n));
            }

            IdentExpr n;
            n.span = id_span;
            n.name = name;
            return prog_.add_expr(std::move(n));
        }

        error("unexpected token '" + cur().lexeme + "' in expression");
    }

    ExprId parse_cond_expr() {
        ExprId left = parse_cond_unary();
        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= 0) break;
            if (cur().kind == TokenKind::Assign) break;
            if (cur().kind == TokenKind::AsKw) {
                SourcePos left_begin = expr_span(prog_.expr(left)).begin;
                consume();
                TypeId tgt_id   = parse_type();
                SourcePos t_end = type_span(prog_.type(tgt_id)).end;
                CastExpr node;
                node.span    = {left_begin, t_end};
                node.operand = left;
                node.target  = tgt_id;
                left = prog_.add_expr(std::move(node));
                continue;
            }
            SourcePos left_begin = expr_span(prog_.expr(left)).begin;
            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            ExprId rhs = parse_cond_expr_at(next_min);
            SourcePos rhs_end = expr_span(prog_.expr(rhs)).end;
            BinaryExpr node;
            node.span  = {left_begin, rhs_end};
            node.op    = op;
            node.left  = left;
            node.right = rhs;
            left = prog_.add_expr(std::move(node));
        }
        return left;
    }

    ExprId parse_cond_expr_at(int min_prec) {
        ExprId left = parse_cond_unary();
        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec <= min_prec) break;
            if (cur().kind == TokenKind::Assign) break;
            if (cur().kind == TokenKind::AsKw) {
                SourcePos left_begin = expr_span(prog_.expr(left)).begin;
                consume();
                TypeId tgt_id   = parse_type();
                SourcePos t_end = type_span(prog_.type(tgt_id)).end;
                CastExpr node;
                node.span    = {left_begin, t_end};
                node.operand = left;
                node.target  = tgt_id;
                left = prog_.add_expr(std::move(node));
                continue;
            }
            SourcePos left_begin = expr_span(prog_.expr(left)).begin;
            std::string op = cur().lexeme;
            consume();
            int next_min = right_assoc ? prec - 1 : prec;
            ExprId rhs = parse_cond_expr_at(next_min);
            SourcePos rhs_end = expr_span(prog_.expr(rhs)).end;
            BinaryExpr node;
            node.span  = {left_begin, rhs_end};
            node.op    = op;
            node.left  = left;
            node.right = rhs;
            left = prog_.add_expr(std::move(node));
        }
        return left;
    }

    ExprId parse_cond_unary() {
        if (at(TokenKind::Minus) || at(TokenKind::Bang)) {
            SourcePos s = cur().span.begin;
            std::string op = cur().lexeme;
            consume();
            ExprId operand = parse_cond_unary();
            SourcePos op_end = expr_span(prog_.expr(operand)).end;
            UnaryExpr n;
            n.span    = {s, op_end};
            n.op      = op;
            n.operand = operand;
            return prog_.add_expr(std::move(n));
        }
        return parse_postfix(parse_primary(false));
    }

    StmtId parse_stmt() {
        SourcePos start = cur().span.begin;

        if (at(TokenKind::Semicolon)) {
            consume();
            NullStmt n;
            n.span = {start, cur().span.begin};
            return prog_.add_stmt(std::move(n));
        }

        if (at(TokenKind::LBrace))  return parse_block();
        if (at(TokenKind::LetKw))   return parse_var_decl();
        if (at(TokenKind::IfKw))    return parse_if();
        if (at(TokenKind::WhileKw)) return parse_while();

        if (at(TokenKind::ReturnKw)) {
            consume();
            ReturnStmt n;
            n.span.begin = start;
            if (!at(TokenKind::Semicolon)) n.value = parse_expr();
            n.span.end = cur().span.begin;
            expect(TokenKind::Semicolon, "';'");
            return prog_.add_stmt(std::move(n));
        }

        if (at(TokenKind::BreakKw)) {
            consume();
            expect(TokenKind::Semicolon, "';'");
            BreakStmt n;
            n.span = {start, cur().span.begin};
            return prog_.add_stmt(std::move(n));
        }

        if (at(TokenKind::ContinueKw)) {
            consume();
            expect(TokenKind::Semicolon, "';'");
            ContinueStmt n;
            n.span = {start, cur().span.begin};
            return prog_.add_stmt(std::move(n));
        }

        ExprId e = parse_expr();
        ExprStmt es;
        es.span = {start, cur().span.begin};
        es.expr = e;
        expect(TokenKind::Semicolon, "';'");
        return prog_.add_stmt(std::move(es));
    }

    StmtId parse_block() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::LBrace, "'{'");
        BlockStmt blk;
        while (!at(TokenKind::RBrace) && !at_end())
            blk.stmts.push_back(parse_stmt());
        blk.span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return prog_.add_stmt(std::move(blk));
    }

    StmtId parse_var_decl() {
        SourcePos start = cur().span.begin;
        consume();
        bool is_mut = try_eat(TokenKind::MutKw);
        if (!at(TokenKind::Identifier)) error("expected variable name");
        std::string name = cur().lexeme;
        consume();

        std::optional<TypeId> type_ann;
        if (try_eat(TokenKind::Colon)) type_ann = parse_type();

        expect(TokenKind::Assign, "'='");
        ExprId init = parse_expr();
        expect(TokenKind::Semicolon, "';'");

        VarDeclStmt n;
        n.span     = {start, cur().span.begin};
        n.is_mut   = is_mut;
        n.name     = name;
        n.type_ann = type_ann;
        n.init     = init;
        return prog_.add_stmt(std::move(n));
    }

    StmtId parse_if() {
        SourcePos start = cur().span.begin;
        consume();
        ExprId cond       = parse_cond_expr();
        StmtId then_block = parse_block();
        IfStmt n;
        n.span.begin = start;
        n.cond       = cond;
        n.then_block = then_block;

        if (try_eat(TokenKind::ElseKw)) {
            if (at(TokenKind::IfKw))
                n.else_stmt = parse_if();
            else
                n.else_stmt = parse_block();
        }
        n.span.end = cur().span.begin;
        return prog_.add_stmt(std::move(n));
    }

    StmtId parse_while() {
        SourcePos start = cur().span.begin;
        consume();
        ExprId cond  = parse_cond_expr();
        StmtId body  = parse_block();
        WhileStmt n;
        n.span = {start, cur().span.begin};
        n.cond = cond;
        n.body = body;
        return prog_.add_stmt(std::move(n));
    }

    DeclId parse_decl() {
        if (at(TokenKind::FnKw))        return parse_fn_decl("");
        if (at(TokenKind::StructKw))    return parse_struct_decl();
        if (at(TokenKind::TypeKw))      return parse_type_alias();
        if (at(TokenKind::NamespaceKw)) return parse_namespace();
        if (at(TokenKind::ImplKw))      return parse_impl();
        error("expected declaration (fn, struct, type, namespace, impl)");
    }

    DeclId parse_fn_decl(const std::string& ns_prefix) {
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
            p.span.end = type_span(prog_.type(p.type)).end;
            params.push_back(std::move(p));
            if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "')'");
        std::optional<TypeId> ret_type;
        if (try_eat(TokenKind::Colon))
            ret_type = parse_type();
        StmtId body = parse_block();

        FnDecl fn;
        fn.span         = {start, cur().span.begin};
        fn.name         = name;
        fn.params       = std::move(params);
        fn.return_type  = ret_type;
        fn.body         = body;
        fn.mangled_name = ns_prefix.empty() ? name : ns_prefix + "__" + name;
        return prog_.add_decl(std::move(fn));
    }

    DeclId parse_struct_decl() {
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
            f.span.end = type_span(prog_.type(f.type)).end;
            fields.push_back(std::move(f));
            if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBrace, "'}'");

        StructDecl sd;
        sd.span   = {start, cur().span.begin};
        sd.name   = name;
        sd.fields = std::move(fields);
        return prog_.add_decl(std::move(sd));
    }

    DeclId parse_type_alias() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::TypeKw, "'type'");
        if (!at(TokenKind::Identifier)) error("expected alias name");
        std::string name = cur().lexeme;
        consume();
        expect(TokenKind::Assign, "'='");
        TypeId type_id = parse_type();
        expect(TokenKind::Semicolon, "';'");
        TypeAliasDecl ta;
        ta.span = {start, cur().span.begin};
        ta.name = name;
        ta.type = type_id;
        return prog_.add_decl(std::move(ta));
    }

    DeclId parse_namespace() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::NamespaceKw, "'namespace'");
        if (!at(TokenKind::Identifier)) error("expected namespace name");
        std::string name = cur().lexeme;
        consume();
        expect(TokenKind::LBrace, "'{'");
        NamespaceDecl nd;
        nd.name = name;
        while (!at(TokenKind::RBrace) && !at_end()) {
            if (at(TokenKind::FnKw))
                nd.decls.push_back(parse_fn_decl(name));
            else
                nd.decls.push_back(parse_decl());
        }
        nd.span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return prog_.add_decl(std::move(nd));
    }

    DeclId parse_impl() {
        SourcePos start = cur().span.begin;
        expect(TokenKind::ImplKw, "'impl'");
        if (!at(TokenKind::Identifier)) error("expected struct name after 'impl'");
        std::string sname = cur().lexeme;
        consume();
        expect(TokenKind::LBrace, "'{'");
        ImplDecl impl;
        impl.struct_name = sname;
        while (!at(TokenKind::RBrace) && !at_end()) {
            if (!at(TokenKind::FnKw)) error("expected 'fn' inside impl block");
            impl.methods.push_back(parse_fn_decl(sname));
        }
        impl.span = {start, cur().span.end};
        expect(TokenKind::RBrace, "'}'");
        return prog_.add_decl(std::move(impl));
    }

public:
    void parse_program() {
        while (!at_end())
            prog_.top_decls.push_back(parse_decl());
    }

    const std::vector<Token>& tokens_;
    const std::string&        filename_;
    Program&                  prog_;
    std::size_t               pos_ = 0;
};

ParseResult parse(const std::vector<Token>& tokens, const std::string& filename) {
    ParseResult r;
    P parser(tokens, filename, r.program);
    try {
        parser.parse_program();
        r.ok = true;
        return r;
    } catch (const ParseError& e) {
        r.ok    = false;
        r.error = e;
        return r;
    }
}

}
