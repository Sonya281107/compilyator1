#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "token.hpp"

namespace AST {

using Lexer::SourceSpan;

static constexpr uint32_t NO_ID = UINT32_MAX;

enum class SemKind {
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    Bool, String, Void,
    Array, Struct,
    Unknown
};

struct SemType {
    SemKind     kind       = SemKind::Unknown;
    uint32_t    elem_id    = NO_ID;  
    std::size_t array_size = 0;
    std::string struct_name;

    bool is_integer()    const;
    bool is_unsigned()   const;
    bool is_float()      const;
    bool is_numeric()    const;
    bool is_signed_int() const;
    std::size_t slot_size() const { return 8; }
};

struct SemTypePool {
    std::vector<SemType> pool;

    uint32_t add(SemType t) {
        pool.push_back(std::move(t));
        return static_cast<uint32_t>(pool.size() - 1);
    }
    SemType&       get(uint32_t id)       { return pool[id]; }
    const SemType& get(uint32_t id) const { return pool[id]; }
    bool valid(uint32_t id) const { return id != NO_ID && id < pool.size(); }
};

std::string sem_to_string(uint32_t id, const SemTypePool& pool);
bool        sem_equal(uint32_t a, uint32_t b, const SemTypePool& pool);
std::size_t sem_storage_bytes(uint32_t id, const SemTypePool& pool);

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
    return std::visit([](auto& n) { return n.span; }, t);
}

using ExprId = uint32_t;

struct IntLitExpr    { int64_t value = 0;    SourceSpan span; uint32_t sem_type = NO_ID; };
struct FloatLitExpr  { double  value = 0.0;  SourceSpan span; uint32_t sem_type = NO_ID; };
struct StringLitExpr { std::string value;    SourceSpan span; uint32_t sem_type = NO_ID; };
struct BoolLitExpr   { bool value = false;   SourceSpan span; uint32_t sem_type = NO_ID; };
struct IdentExpr     { std::string name;     SourceSpan span; uint32_t sem_type = NO_ID; };

struct BinaryExpr {
    std::string op;
    ExprId left = NO_ID, right = NO_ID;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct UnaryExpr {
    std::string op;
    ExprId operand = NO_ID;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct CastExpr {
    ExprId operand = NO_ID;
    TypeId target  = NO_ID;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct IndexExpr {
    ExprId array = NO_ID, index = NO_ID;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct FieldExpr {
    ExprId object = NO_ID;
    std::string field;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct ScopeExpr {
    std::string ns, name;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct CallExpr {
    ExprId callee = NO_ID;
    std::vector<ExprId> args;
    std::string resolved_fn;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct ArrayLitExpr {
    std::vector<ExprId> elements;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct StructLitExpr {
    std::string name;
    std::vector<std::pair<std::string, ExprId>> fields;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};
struct AssignExpr {
    ExprId target = NO_ID, value = NO_ID;
    SourceSpan span;
    uint32_t sem_type = NO_ID;
};

using ExprNode = std::variant<
    IntLitExpr, FloatLitExpr, StringLitExpr, BoolLitExpr, IdentExpr,
    BinaryExpr, UnaryExpr, CastExpr, IndexExpr, FieldExpr, ScopeExpr,
    CallExpr, ArrayLitExpr, StructLitExpr, AssignExpr
>;

inline SourceSpan expr_span(const ExprNode& e) {
    return std::visit([](auto& n) { return n.span; }, e);
}
inline uint32_t expr_sem_type(const ExprNode& e) {
    return std::visit([](auto& n) { return n.sem_type; }, e);
}
inline void set_expr_sem_type(ExprNode& e, uint32_t st) {
    std::visit([st](auto& n) { n.sem_type = st; }, e);
}

using StmtId = uint32_t;

struct BlockStmt {
    std::vector<StmtId> stmts;
    SourceSpan span;
};
struct VarDeclStmt {
    bool is_mut = false;
    std::string name;
    std::optional<TypeId> type_ann;
    ExprId init = NO_ID;
    uint32_t sem_type = NO_ID;
    int64_t frame_offset = 0;
    SourceSpan span;
};
struct ExprStmt     { ExprId expr = NO_ID;  SourceSpan span; };
struct IfStmt       { ExprId cond = NO_ID; StmtId then_block = NO_ID; std::optional<StmtId> else_stmt; SourceSpan span; };
struct WhileStmt    { ExprId cond = NO_ID; StmtId body = NO_ID; SourceSpan span; };
struct ReturnStmt   { std::optional<ExprId> value; SourceSpan span; };
struct BreakStmt    { SourceSpan span; };
struct ContinueStmt { SourceSpan span; };
struct NullStmt     { SourceSpan span; };

using StmtNode = std::variant<
    BlockStmt, VarDeclStmt, ExprStmt, IfStmt, WhileStmt,
    ReturnStmt, BreakStmt, ContinueStmt, NullStmt
>;

inline SourceSpan stmt_span(const StmtNode& s) {
    return std::visit([](auto& n) { return n.span; }, s);
}

using DeclId = uint32_t;

struct Param {
    std::string name;
    TypeId type = NO_ID;
    SourceSpan span;
};

struct StructField {
    std::string name;
    TypeId      type = NO_ID;
    SourceSpan  span;
    uint32_t    sem_type     = NO_ID;
    std::size_t field_offset = 0;
};

struct FnDecl {
    std::string name;
    std::vector<Param> params;
    std::optional<TypeId> return_type;
    StmtId body = NO_ID;
    std::string mangled_name;
    uint32_t    sem_return  = NO_ID;
    std::size_t frame_bytes = 0;
    SourceSpan  span;
};

struct StructDecl {
    std::string              name;
    std::vector<StructField> fields;
    SourceSpan               span;
};

struct TypeAliasDecl {
    std::string name;
    TypeId      type = NO_ID;
    SourceSpan  span;
};

struct NamespaceDecl {
    std::string          name;
    std::vector<DeclId>  decls;
    SourceSpan           span;
};

struct ImplDecl {
    std::string         struct_name;
    std::vector<DeclId> methods;  
    SourceSpan          span;
};

using DeclNode = std::variant<FnDecl, StructDecl, TypeAliasDecl, NamespaceDecl, ImplDecl>;

inline SourceSpan decl_span(const DeclNode& d) {
    return std::visit([](auto& n) { return n.span; }, d);
}

struct Program {
    std::vector<ExprNode> exprs;
    std::vector<StmtNode> stmts;
    std::vector<DeclNode> decl_pool;
    std::vector<TypeNode> types;
    SemTypePool           semtypes;

    std::vector<DeclId> top_decls;  
    ExprId add_expr(ExprNode n) { exprs.push_back(std::move(n)); return static_cast<uint32_t>(exprs.size() - 1); }
    StmtId add_stmt(StmtNode n) { stmts.push_back(std::move(n)); return static_cast<uint32_t>(stmts.size() - 1); }
    DeclId add_decl(DeclNode n) { decl_pool.push_back(std::move(n)); return static_cast<uint32_t>(decl_pool.size() - 1); }
    TypeId add_type(TypeNode n) { types.push_back(std::move(n)); return static_cast<uint32_t>(types.size() - 1); }

    ExprNode&       expr(ExprId id)       { return exprs[id]; }
    const ExprNode& expr(ExprId id) const { return exprs[id]; }
    StmtNode&       stmt(StmtId id)       { return stmts[id]; }
    const StmtNode& stmt(StmtId id) const { return stmts[id]; }
    DeclNode&       decl(DeclId id)       { return decl_pool[id]; }
    const DeclNode& decl(DeclId id) const { return decl_pool[id]; }
    TypeNode&       type(TypeId id)       { return types[id]; }
    const TypeNode& type(TypeId id) const { return types[id]; }
};

}  // namespace AST
