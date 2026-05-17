module;
#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

export module parser;
import lexer;


export namespace AST {

using Lexer::SourceSpan;
using Lexer::SourcePos;

constexpr uint32_t NO_ID = UINT32_MAX;

enum class SemKind {
    I8, I16, I32, I64, U8, U16, U32, U64,
    F32, F64, Bool, String, Void, Array, Struct, Unknown
};

struct SemType {
    SemKind     kind        = SemKind::Unknown;
    uint32_t    elem_id     = NO_ID;
    std::size_t array_size  = 0;
    std::string struct_name;

    bool is_integer()    const;
    bool is_unsigned()   const;
    bool is_float()      const;
    bool is_numeric()    const;
    bool is_signed_int() const;
};

struct SemTypePool {
    std::vector<SemType> pool;
    uint32_t add(SemType t) { pool.push_back(std::move(t)); return (uint32_t)(pool.size()-1); }
    SemType&       get(uint32_t id)       { return pool[id]; }
    const SemType& get(uint32_t id) const { return pool[id]; }
    bool valid(uint32_t id) const { return id != NO_ID && id < pool.size(); }
};

std::string     sem_to_string(uint32_t id, const SemTypePool& pool);
bool            sem_equal(uint32_t a, uint32_t b, const SemTypePool& pool);
std::size_t     sem_storage_bytes(uint32_t id, const SemTypePool& pool);

inline uint32_t make_sem(SemTypePool& pool, SemKind k) {
    return pool.add(SemType{k, NO_ID, 0, {}});
}
inline uint32_t sem_array(SemTypePool& pool, uint32_t elem, std::size_t n) {
    return pool.add(SemType{SemKind::Array, elem, n, {}});
}
inline uint32_t sem_struct(SemTypePool& pool, std::string name) {
    return pool.add(SemType{SemKind::Struct, NO_ID, 0, std::move(name)});
}

using TypeId = uint32_t;
struct PrimTypeNode  { SemKind prim; SourceSpan span; };
struct ArrayTypeNode { TypeId elem = NO_ID; std::size_t size = 0; SourceSpan span; };
struct NamedTypeNode { std::string name; SourceSpan span; };
using TypeNode = std::variant<PrimTypeNode, ArrayTypeNode, NamedTypeNode>;

inline SourceSpan type_span(const TypeNode& t) {
    return std::visit([](auto& n){ return n.span; }, t);
}

using ExprId = uint32_t;

struct IntLitExpr    { int64_t value = 0;    SourceSpan span; uint32_t sem_type = NO_ID; };
struct FloatLitExpr  { double  value = 0.0;  SourceSpan span; uint32_t sem_type = NO_ID; };
struct StringLitExpr { std::string value;    SourceSpan span; uint32_t sem_type = NO_ID; };
struct BoolLitExpr   { bool value = false;   SourceSpan span; uint32_t sem_type = NO_ID; };
struct IdentExpr     { std::string name;     SourceSpan span; uint32_t sem_type = NO_ID; };

struct BinaryExpr {
    std::string op; ExprId left = NO_ID, right = NO_ID;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct UnaryExpr {
    std::string op; ExprId operand = NO_ID;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct CastExpr {
    ExprId operand = NO_ID; TypeId target = NO_ID;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct IndexExpr {
    ExprId array = NO_ID, index = NO_ID;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct FieldExpr {
    ExprId object = NO_ID; std::string field;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct ScopeExpr {
    std::string ns, name;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct CallExpr {
    ExprId callee = NO_ID; std::vector<ExprId> args;
    std::string resolved_fn; SourceSpan span; uint32_t sem_type = NO_ID;
};
struct ArrayLitExpr {
    std::vector<ExprId> elements;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct StructLitExpr {
    std::string name;
    std::vector<std::pair<std::string, ExprId>> fields;
    SourceSpan span; uint32_t sem_type = NO_ID;
};
struct AssignExpr {
    ExprId target = NO_ID, value = NO_ID;
    SourceSpan span; uint32_t sem_type = NO_ID;
};

using ExprNode = std::variant<
    IntLitExpr, FloatLitExpr, StringLitExpr, BoolLitExpr, IdentExpr,
    BinaryExpr, UnaryExpr, CastExpr, IndexExpr, FieldExpr, ScopeExpr,
    CallExpr, ArrayLitExpr, StructLitExpr, AssignExpr>;

inline SourceSpan expr_span(const ExprNode& e) {
    return std::visit([](auto& n){ return n.span; }, e);
}
inline uint32_t expr_sem_type(const ExprNode& e) {
    return std::visit([](auto& n){ return n.sem_type; }, e);
}
inline void set_expr_sem_type(ExprNode& e, uint32_t st) {
    std::visit([st](auto& n){ n.sem_type = st; }, e);
}

using StmtId = uint32_t;

struct BlockStmt    { std::vector<StmtId> stmts; SourceSpan span; };
struct VarDeclStmt  {
    bool is_mut = false; std::string name;
    std::optional<TypeId> type_ann; ExprId init = NO_ID;
    uint32_t sem_type = NO_ID; int64_t frame_offset = 0; SourceSpan span;
};
struct ExprStmt     { ExprId expr = NO_ID; SourceSpan span; };
struct IfStmt       { ExprId cond = NO_ID; StmtId then_block = NO_ID; std::optional<StmtId> else_stmt; SourceSpan span; };
struct WhileStmt    { ExprId cond = NO_ID; StmtId body = NO_ID; SourceSpan span; };
struct ReturnStmt   { std::optional<ExprId> value; SourceSpan span; };
struct BreakStmt    { SourceSpan span; };
struct ContinueStmt { SourceSpan span; };
struct NullStmt     { SourceSpan span; };

using StmtNode = std::variant<
    BlockStmt, VarDeclStmt, ExprStmt, IfStmt, WhileStmt,
    ReturnStmt, BreakStmt, ContinueStmt, NullStmt>;

inline SourceSpan stmt_span(const StmtNode& s) {
    return std::visit([](auto& n){ return n.span; }, s);
}

using DeclId = uint32_t;

struct Param { std::string name; TypeId type = NO_ID; SourceSpan span; };

struct StructField {
    std::string name; TypeId type = NO_ID; SourceSpan span;
    uint32_t sem_type = NO_ID; std::size_t field_offset = 0;
};

struct FnDecl {
    std::string name; std::vector<Param> params;
    std::optional<TypeId> return_type; StmtId body = NO_ID;
    std::string mangled_name; uint32_t sem_return = NO_ID;
    std::size_t frame_bytes = 0; SourceSpan span;
};
struct StructDecl    { std::string name; std::vector<StructField> fields; SourceSpan span; };
struct TypeAliasDecl { std::string name; TypeId type = NO_ID; SourceSpan span; };
struct NamespaceDecl { std::string name; std::vector<DeclId> decls; SourceSpan span; };
struct ImplDecl      { std::string struct_name; std::vector<DeclId> methods; SourceSpan span; };

using DeclNode = std::variant<FnDecl, StructDecl, TypeAliasDecl, NamespaceDecl, ImplDecl>;

inline SourceSpan decl_span(const DeclNode& d) {
    return std::visit([](auto& n){ return n.span; }, d);
}

struct Program {
    std::vector<ExprNode> exprs;
    std::vector<StmtNode> stmts;
    std::vector<DeclNode> decl_pool;
    std::vector<TypeNode> types;
    SemTypePool           semtypes;
    std::vector<DeclId>   top_decls;

    ExprId add_expr(ExprNode n) { exprs.push_back(std::move(n)); return (uint32_t)(exprs.size()-1); }
    StmtId add_stmt(StmtNode n) { stmts.push_back(std::move(n)); return (uint32_t)(stmts.size()-1); }
    DeclId add_decl(DeclNode n) { decl_pool.push_back(std::move(n)); return (uint32_t)(decl_pool.size()-1); }
    TypeId add_type(TypeNode n) { types.push_back(std::move(n)); return (uint32_t)(types.size()-1); }

    ExprNode&       expr(ExprId id)       { return exprs[id]; }
    const ExprNode& expr(ExprId id) const { return exprs[id]; }
    StmtNode&       stmt(StmtId id)       { return stmts[id]; }
    const StmtNode& stmt(StmtId id) const { return stmts[id]; }
    DeclNode&       decl(DeclId id)       { return decl_pool[id]; }
    const DeclNode& decl(DeclId id) const { return decl_pool[id]; }
    TypeNode&       type(TypeId id)       { return types[id]; }
    const TypeNode& type(TypeId id) const { return types[id]; }
};

} // export namespace AST

export namespace Parser {

struct ParseError { Lexer::Diagnostic diag; };

struct ParseResult {
    bool      ok = false;
    AST::Program program;
    ParseError error;
};

ParseResult parse(const std::vector<Lexer::Token>& tokens, const std::string& filename);

} // export namespace Parser


namespace AST {

bool SemType::is_integer()    const { return kind==SemKind::I8||kind==SemKind::I16||kind==SemKind::I32||kind==SemKind::I64||kind==SemKind::U8||kind==SemKind::U16||kind==SemKind::U32||kind==SemKind::U64; }
bool SemType::is_unsigned()   const { return kind==SemKind::U8||kind==SemKind::U16||kind==SemKind::U32||kind==SemKind::U64; }
bool SemType::is_float()      const { return kind==SemKind::F32||kind==SemKind::F64; }
bool SemType::is_numeric()    const { return is_integer()||is_float(); }
bool SemType::is_signed_int() const { return kind==SemKind::I8||kind==SemKind::I16||kind==SemKind::I32||kind==SemKind::I64; }

std::size_t sem_storage_bytes(uint32_t id, const SemTypePool& pool) {
    if (id == NO_ID) return 8;
    const SemType& t = pool.get(id);
    if (t.kind==SemKind::Array && pool.valid(t.elem_id))
        return t.array_size * sem_storage_bytes(t.elem_id, pool);
    if (t.kind==SemKind::Struct) return 0;
    return 8;
}

std::string sem_to_string(uint32_t id, const SemTypePool& pool) {
    if (id==NO_ID) return "?";
    const SemType& t = pool.get(id);
    switch (t.kind) {
        case SemKind::I8:  return "i8";  case SemKind::I16: return "i16";
        case SemKind::I32: return "i32"; case SemKind::I64: return "i64";
        case SemKind::U8:  return "u8";  case SemKind::U16: return "u16";
        case SemKind::U32: return "u32"; case SemKind::U64: return "u64";
        case SemKind::F32: return "f32"; case SemKind::F64: return "f64";
        case SemKind::Bool:   return "bool";
        case SemKind::String: return "string";
        case SemKind::Void:   return "void";
        case SemKind::Array:
            return "[" + sem_to_string(t.elem_id, pool) + "; " + std::to_string(t.array_size) + "]";
        case SemKind::Struct: return t.struct_name;
        default: return "?";
    }
}

bool sem_equal(uint32_t a, uint32_t b, const SemTypePool& pool) {
    if (a==b) return true;
    if (a==NO_ID||b==NO_ID) return false;
    const SemType& sa = pool.get(a), sb = pool.get(b);
    if (sa.kind!=sb.kind) return false;
    if (sa.kind==SemKind::Array) return sa.array_size==sb.array_size && sem_equal(sa.elem_id, sb.elem_id, pool);
    if (sa.kind==SemKind::Struct) return sa.struct_name==sb.struct_name;
    return true;
}

} // namespace AST

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
        default:                  return SemKind::Unknown;
    }
}

static bool is_type_kw(TokenKind k) {
    return k==TokenKind::I8Kw||k==TokenKind::I16Kw||k==TokenKind::I32Kw||k==TokenKind::I64Kw||
           k==TokenKind::U8Kw||k==TokenKind::U16Kw||k==TokenKind::U32Kw||k==TokenKind::U64Kw||
           k==TokenKind::F32Kw||k==TokenKind::F64Kw||k==TokenKind::BoolKw||
           k==TokenKind::StringKw||k==TokenKind::VoidKw;
}


class P {
public:
    P(const std::vector<Token>& toks, const std::string& fname, Program& prog)
        : tokens_(toks), filename_(fname), prog_(prog) {}

    void parse_program();

private:
    const Token& cur()  const { return tokens_[pos_]; }
    const Token& peek(std::size_t off=1) const {
        std::size_t i = pos_+off;
        return i<tokens_.size() ? tokens_[i] : tokens_.back();
    }
    bool at(TokenKind k) const { return cur().kind==k; }
    bool at_end()        const { return at(TokenKind::EndOfFile); }

    Token consume() { Token t=cur(); if (!at_end()) ++pos_; return t; }

    bool try_eat(TokenKind k) { if (!at(k)) return false; consume(); return true; }

    Token expect(TokenKind k, const std::string& what) {
        if (!at(k)) error("expected "+what+", got '"+cur().lexeme+"'");
        return consume();
    }

    [[noreturn]] void error(const std::string& msg) {
        Diagnostic d; d.filename=filename_; d.pos=cur().span.begin; d.message=msg;
        throw ParseError{d};
    }

    TypeId parse_type() {
        SourceSpan span; span.begin = cur().span.begin;
        if (is_type_kw(cur().kind)) {
            PrimTypeNode n; n.prim=prim_from_kw(cur().kind); n.span={span.begin,cur().span.end};
            consume(); return prog_.add_type(n);
        }
        if (at(TokenKind::Identifier)) {
            NamedTypeNode n; n.name=cur().lexeme; n.span=cur().span; consume(); return prog_.add_type(n);
        }
        if (at(TokenKind::LBracket)) {
            consume(); ArrayTypeNode n; n.elem=parse_type();
            expect(TokenKind::Semicolon,"';'");
            if (!at(TokenKind::IntLiteral)) error("expected array size");
            n.size=(std::size_t)std::stoul(cur().lexeme); n.span.end=cur().span.end; consume();
            expect(TokenKind::RBracket,"']'"); return prog_.add_type(n);
        }
        error("expected type");
    }

    struct InfixInfo { int prec; bool right_assoc; };
    InfixInfo infix(TokenKind k) const {
        switch (k) {
            case TokenKind::Assign:    return {1,true};
            case TokenKind::OrOr:      return {2,false};
            case TokenKind::AndAnd:    return {3,false};
            case TokenKind::EqEq: case TokenKind::NotEq:
            case TokenKind::Less: case TokenKind::LessEq:
            case TokenKind::Greater: case TokenKind::GreaterEq: return {4,false};
            case TokenKind::Plus: case TokenKind::Minus:        return {5,false};
            case TokenKind::Star: case TokenKind::Slash:
            case TokenKind::Percent:   return {6,false};
            case TokenKind::AsKw:      return {7,false};
            default:                   return {0,false};
        }
    }

    ExprId parse_expr(int min_prec=0) {
        ExprId left = parse_unary();
        while (true) {
            auto [prec, right_assoc] = infix(cur().kind);
            if (prec<=min_prec) break;
            if (cur().kind==TokenKind::Assign) {
                consume(); ExprId rhs=parse_expr(prec-1);
                AssignExpr node; node.span={expr_span(prog_.expr(left)).begin,expr_span(prog_.expr(rhs)).end};
                node.target=left; node.value=rhs; left=prog_.add_expr(std::move(node)); continue;
            }
            if (cur().kind==TokenKind::AsKw) {
                SourcePos lb=expr_span(prog_.expr(left)).begin; consume();
                TypeId tid=parse_type(); CastExpr node;
                node.span={lb,type_span(prog_.type(tid)).end}; node.operand=left; node.target=tid;
                left=prog_.add_expr(std::move(node)); continue;
            }
            SourcePos lb=expr_span(prog_.expr(left)).begin;
            std::string op=cur().lexeme; consume();
            ExprId rhs=parse_expr(right_assoc?prec-1:prec);
            BinaryExpr node; node.span={lb,expr_span(prog_.expr(rhs)).end};
            node.op=op; node.left=left; node.right=rhs; left=prog_.add_expr(std::move(node));
        }
        return left;
    }

    ExprId parse_unary() {
        if (at(TokenKind::Minus)||at(TokenKind::Bang)) {
            SourcePos s=cur().span.begin; std::string op=cur().lexeme; consume();
            ExprId operand=parse_unary();
            UnaryExpr n; n.span={s,expr_span(prog_.expr(operand)).end}; n.op=op; n.operand=operand;
            return prog_.add_expr(std::move(n));
        }
        return parse_postfix(parse_primary());
    }

    ExprId parse_postfix(ExprId left) {
        while (true) {
            if (at(TokenKind::LBracket)) {
                SourcePos s=expr_span(prog_.expr(left)).begin; consume();
                ExprId idx=parse_expr(); expect(TokenKind::RBracket,"']'");
                IndexExpr n; n.span={s,cur().span.begin}; n.array=left; n.index=idx;
                left=prog_.add_expr(std::move(n)); continue;
            }
            if (at(TokenKind::Dot)) {
                SourcePos s=expr_span(prog_.expr(left)).begin; consume();
                if (!at(TokenKind::Identifier)) error("expected field name after '.'");
                std::string fname=cur().lexeme; SourcePos end=cur().span.end; consume();
                if (at(TokenKind::LParen)) {
                    consume(); FieldExpr fe; fe.span={s,end}; fe.field=fname; fe.object=left;
                    ExprId fe_id=prog_.add_expr(std::move(fe));
                    CallExpr node; node.callee=fe_id; node.args=parse_arg_list();
                    expect(TokenKind::RParen,"')'"); node.span={s,cur().span.begin};
                    left=prog_.add_expr(std::move(node));
                } else {
                    FieldExpr n; n.span={s,end}; n.field=fname; n.object=left;
                    left=prog_.add_expr(std::move(n));
                }
                continue;
            }
            if (at(TokenKind::ColonColon)) {
                SourcePos s=expr_span(prog_.expr(left)).begin; consume();
                if (!at(TokenKind::Identifier)) error("expected name after '::'");
                std::string ns_name;
                if (std::holds_alternative<IdentExpr>(prog_.expr(left)))
                    ns_name=std::get<IdentExpr>(prog_.expr(left)).name;
                else error("'::' must follow a namespace name");
                std::string member=cur().lexeme; SourcePos end=cur().span.end; consume();
                if (at(TokenKind::LParen)) {
                    consume(); ScopeExpr se; se.span={s,end}; se.ns=ns_name; se.name=member;
                    ExprId se_id=prog_.add_expr(std::move(se));
                    CallExpr n; n.callee=se_id; n.args=parse_arg_list();
                    expect(TokenKind::RParen,"')'"); n.span={s,cur().span.begin};
                    left=prog_.add_expr(std::move(n));
                } else {
                    ScopeExpr n; n.span={s,end}; n.ns=ns_name; n.name=member;
                    left=prog_.add_expr(std::move(n));
                }
                continue;
            }
            if (at(TokenKind::LParen)) {
                SourcePos s=expr_span(prog_.expr(left)).begin; consume();
                CallExpr n; n.callee=left; n.args=parse_arg_list();
                expect(TokenKind::RParen,"')'"); n.span={s,cur().span.begin};
                left=prog_.add_expr(std::move(n)); continue;
            }
            break;
        }
        return left;
    }

    std::vector<ExprId> parse_arg_list() {
        std::vector<ExprId> args;
        if (at(TokenKind::RParen)) return args;
        args.push_back(parse_expr());
        while (try_eat(TokenKind::Comma)) args.push_back(parse_expr());
        return args;
    }

    ExprId parse_primary(bool allow_struct_lit=true) {
        if (at(TokenKind::IntLiteral)) {
            int64_t val=0;
            std::from_chars(cur().lexeme.data(), cur().lexeme.data()+cur().lexeme.size(), val);
            IntLitExpr n; n.span=cur().span; n.value=val; consume(); return prog_.add_expr(std::move(n));
        }
        if (at(TokenKind::FloatLiteral)) {
            FloatLitExpr n; n.span=cur().span; n.value=std::stod(cur().lexeme);
            consume(); return prog_.add_expr(std::move(n));
        }
        if (at(TokenKind::StringLiteral)) {
            const std::string& raw=cur().lexeme; std::string val; val.reserve(raw.size());
            for (std::size_t i=1; i+1<raw.size(); ++i) {
                if (raw[i]=='\\'&&i+2<raw.size()) {
                    ++i;
                    switch(raw[i]){case 'n':val+='\n';break;case 't':val+='\t';break;
                                   case 'r':val+='\r';break;case '0':val+='\0';break;default:val+=raw[i];}
                } else val+=raw[i];
            }
            StringLitExpr n; n.span=cur().span; n.value=std::move(val); consume(); return prog_.add_expr(std::move(n));
        }
        if (at(TokenKind::TrueKw)||at(TokenKind::FalseKw)) {
            BoolLitExpr n; n.span=cur().span; n.value=at(TokenKind::TrueKw);
            consume(); return prog_.add_expr(std::move(n));
        }
        if (at(TokenKind::LParen)) { consume(); ExprId e=parse_expr(); expect(TokenKind::RParen,"')'"); return e; }
        if (at(TokenKind::LBracket)) {
            SourcePos arr_start=cur().span.begin; consume(); ArrayLitExpr n;
            if (!at(TokenKind::RBracket)) {
                n.elements.push_back(parse_expr());
                while (try_eat(TokenKind::Comma)&&!at(TokenKind::RBracket)) n.elements.push_back(parse_expr());
            }
            n.span={arr_start,cur().span.end}; expect(TokenKind::RBracket,"']'");
            return prog_.add_expr(std::move(n));
        }
        if (at(TokenKind::Identifier)) {
            std::string name=cur().lexeme; SourceSpan id_span=cur().span; consume();
            if (allow_struct_lit&&at(TokenKind::LBrace)) {
                consume(); StructLitExpr n; n.name=name;
                while (!at(TokenKind::RBrace)&&!at_end()) {
                    if (!at(TokenKind::Identifier)) error("expected field name");
                    std::string fname=cur().lexeme; consume();
                    expect(TokenKind::Colon,"':'");
                    ExprId val=parse_expr(); n.fields.emplace_back(fname,val);
                    if (!try_eat(TokenKind::Comma)) break;
                }
                n.span={id_span.begin,cur().span.end}; expect(TokenKind::RBrace,"'}'");
                return prog_.add_expr(std::move(n));
            }
            IdentExpr n; n.span=id_span; n.name=name; return prog_.add_expr(std::move(n));
        }
        error("unexpected token '"+cur().lexeme+"' in expression");
    }

    ExprId parse_cond_expr() { return parse_cond_expr_at(0); }
    ExprId parse_cond_expr_at(int min_prec) {
        ExprId left=parse_cond_unary();
        while (true) {
            auto [prec, right_assoc]=infix(cur().kind);
            if (prec<=min_prec||cur().kind==TokenKind::Assign) break;
            if (cur().kind==TokenKind::AsKw) {
                SourcePos lb=expr_span(prog_.expr(left)).begin; consume(); TypeId tid=parse_type();
                CastExpr n; n.span={lb,type_span(prog_.type(tid)).end}; n.operand=left; n.target=tid;
                left=prog_.add_expr(std::move(n)); continue;
            }
            SourcePos lb=expr_span(prog_.expr(left)).begin; std::string op=cur().lexeme; consume();
            ExprId rhs=parse_cond_expr_at(right_assoc?prec-1:prec);
            BinaryExpr n; n.span={lb,expr_span(prog_.expr(rhs)).end}; n.op=op; n.left=left; n.right=rhs;
            left=prog_.add_expr(std::move(n));
        }
        return left;
    }
    ExprId parse_cond_unary() {
        if (at(TokenKind::Minus)||at(TokenKind::Bang)) {
            SourcePos s=cur().span.begin; std::string op=cur().lexeme; consume();
            ExprId operand=parse_cond_unary();
            UnaryExpr n; n.span={s,expr_span(prog_.expr(operand)).end}; n.op=op; n.operand=operand;
            return prog_.add_expr(std::move(n));
        }
        return parse_postfix(parse_primary(false));
    }

    StmtId parse_stmt() {
        SourcePos start=cur().span.begin;
        if (at(TokenKind::Semicolon)) { consume(); NullStmt n; n.span={start,cur().span.begin}; return prog_.add_stmt(std::move(n)); }
        if (at(TokenKind::LBrace))  return parse_block();
        if (at(TokenKind::LetKw))   return parse_var_decl();
        if (at(TokenKind::IfKw))    return parse_if();
        if (at(TokenKind::WhileKw)) return parse_while();
        if (at(TokenKind::ReturnKw)) {
            consume(); ReturnStmt n; n.span.begin=start;
            if (!at(TokenKind::Semicolon)) n.value=parse_expr();
            n.span.end=cur().span.begin; expect(TokenKind::Semicolon,"';'"); return prog_.add_stmt(std::move(n));
        }
        if (at(TokenKind::BreakKw))    { consume(); expect(TokenKind::Semicolon,"';'"); BreakStmt n; n.span={start,cur().span.begin}; return prog_.add_stmt(std::move(n)); }
        if (at(TokenKind::ContinueKw)) { consume(); expect(TokenKind::Semicolon,"';'"); ContinueStmt n; n.span={start,cur().span.begin}; return prog_.add_stmt(std::move(n)); }
        ExprId e=parse_expr(); ExprStmt es; es.span={start,cur().span.begin}; es.expr=e;
        expect(TokenKind::Semicolon,"';'"); return prog_.add_stmt(std::move(es));
    }

    StmtId parse_block() {
        SourcePos start=cur().span.begin; expect(TokenKind::LBrace,"'{'");
        BlockStmt blk;
        while (!at(TokenKind::RBrace)&&!at_end()) blk.stmts.push_back(parse_stmt());
        blk.span={start,cur().span.end}; expect(TokenKind::RBrace,"'}'"); return prog_.add_stmt(std::move(blk));
    }

    StmtId parse_var_decl() {
        SourcePos start=cur().span.begin; consume(); bool is_mut=try_eat(TokenKind::MutKw);
        if (!at(TokenKind::Identifier)) error("expected variable name");
        std::string name=cur().lexeme; consume();
        std::optional<TypeId> type_ann;
        if (try_eat(TokenKind::Colon)) type_ann=parse_type();
        expect(TokenKind::Assign,"'='"); ExprId init=parse_expr(); expect(TokenKind::Semicolon,"';'");
        VarDeclStmt n; n.span={start,cur().span.begin}; n.is_mut=is_mut; n.name=name; n.type_ann=type_ann; n.init=init;
        return prog_.add_stmt(std::move(n));
    }

    StmtId parse_if() {
        SourcePos start=cur().span.begin; consume();
        ExprId cond=parse_cond_expr(); StmtId then_block=parse_block();
        IfStmt n; n.span.begin=start; n.cond=cond; n.then_block=then_block;
        if (try_eat(TokenKind::ElseKw))
            n.else_stmt = at(TokenKind::IfKw) ? parse_if() : parse_block();
        n.span.end=cur().span.begin; return prog_.add_stmt(std::move(n));
    }

    StmtId parse_while() {
        SourcePos start=cur().span.begin; consume();
        ExprId cond=parse_cond_expr(); StmtId body=parse_block();
        WhileStmt n; n.span={start,cur().span.begin}; n.cond=cond; n.body=body;
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
        SourcePos start=cur().span.begin; expect(TokenKind::FnKw,"'fn'");
        if (!at(TokenKind::Identifier)) error("expected function name");
        std::string name=cur().lexeme; consume();
        expect(TokenKind::LParen,"'('");
        std::vector<Param> params;
        while (!at(TokenKind::RParen)&&!at_end()) {
            Param p; p.span.begin=cur().span.begin;
            if (!at(TokenKind::Identifier)) error("expected parameter name");
            p.name=cur().lexeme; consume(); expect(TokenKind::Colon,"':'");
            p.type=parse_type(); p.span.end=type_span(prog_.type(p.type)).end;
            params.push_back(std::move(p)); if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen,"')'");
        std::optional<TypeId> ret_type;
        if (try_eat(TokenKind::Arrow)) ret_type=parse_type();
        StmtId body=parse_block();
        FnDecl fn; fn.span={start,cur().span.begin}; fn.name=name; fn.params=std::move(params);
        fn.return_type=ret_type; fn.body=body;
        fn.mangled_name=ns_prefix.empty()?name:ns_prefix+"__"+name;
        return prog_.add_decl(std::move(fn));
    }

    DeclId parse_struct_decl() {
        SourcePos start=cur().span.begin; expect(TokenKind::StructKw,"'struct'");
        if (!at(TokenKind::Identifier)) error("expected struct name");
        std::string name=cur().lexeme; consume(); expect(TokenKind::LBrace,"'{'");
        std::vector<StructField> fields;
        while (!at(TokenKind::RBrace)&&!at_end()) {
            StructField f; f.span.begin=cur().span.begin;
            if (!at(TokenKind::Identifier)) error("expected field name");
            f.name=cur().lexeme; consume(); expect(TokenKind::Colon,"':'");
            f.type=parse_type(); f.span.end=type_span(prog_.type(f.type)).end;
            fields.push_back(std::move(f)); if (!try_eat(TokenKind::Comma)) break;
        }
        expect(TokenKind::RBrace,"'}'");
        StructDecl sd; sd.span={start,cur().span.begin}; sd.name=name; sd.fields=std::move(fields);
        return prog_.add_decl(std::move(sd));
    }

    DeclId parse_type_alias() {
        SourcePos start=cur().span.begin; expect(TokenKind::TypeKw,"'type'");
        if (!at(TokenKind::Identifier)) error("expected alias name");
        std::string name=cur().lexeme; consume(); expect(TokenKind::Assign,"'='");
        TypeId type_id=parse_type(); expect(TokenKind::Semicolon,"';'");
        TypeAliasDecl ta; ta.span={start,cur().span.begin}; ta.name=name; ta.type=type_id;
        return prog_.add_decl(std::move(ta));
    }

    DeclId parse_namespace() {
        SourcePos start=cur().span.begin; expect(TokenKind::NamespaceKw,"'namespace'");
        if (!at(TokenKind::Identifier)) error("expected namespace name");
        std::string name=cur().lexeme; consume(); expect(TokenKind::LBrace,"'{'");
        NamespaceDecl nd; nd.name=name;
        while (!at(TokenKind::RBrace)&&!at_end())
            nd.decls.push_back(at(TokenKind::FnKw)?parse_fn_decl(name):parse_decl());
        nd.span={start,cur().span.end}; expect(TokenKind::RBrace,"'}'");
        return prog_.add_decl(std::move(nd));
    }

    DeclId parse_impl() {
        SourcePos start=cur().span.begin; expect(TokenKind::ImplKw,"'impl'");
        if (!at(TokenKind::Identifier)) error("expected struct name after 'impl'");
        std::string sname=cur().lexeme; consume(); expect(TokenKind::LBrace,"'{'");
        ImplDecl impl; impl.struct_name=sname;
        while (!at(TokenKind::RBrace)&&!at_end()) {
            if (!at(TokenKind::FnKw)) error("expected 'fn' inside impl block");
            impl.methods.push_back(parse_fn_decl(sname));
        }
        impl.span={start,cur().span.end}; expect(TokenKind::RBrace,"'}'");
        return prog_.add_decl(std::move(impl));
    }

    const std::vector<Token>& tokens_;
    const std::string& filename_;
    Program& prog_;
    std::size_t pos_ = 0;
};

void P::parse_program() {
    while (!at_end()) prog_.top_decls.push_back(parse_decl());
}

ParseResult parse(const std::vector<Token>& tokens, const std::string& filename) {
    ParseResult r;
    P parser(tokens, filename, r.program);
    try { parser.parse_program(); r.ok=true; return r; }
    catch (const ParseError& e) { r.ok=false; r.error=e; return r; }
}

} // namespace Parser