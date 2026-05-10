#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "token.hpp"

namespace AST {

using Lexer::SourceSpan;

enum class SemKind {
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    Bool, String, Void,
    Array, Struct,
    Unknown
};

struct SemType {
    SemKind kind        = SemKind::Unknown;
    std::shared_ptr<SemType> elem;
    std::size_t              array_size = 0;
    std::string struct_name;

    bool is_integer()  const;
    bool is_unsigned() const;
    bool is_float()    const;
    bool is_numeric()  const;
    bool is_signed_int() const;
    std::size_t slot_size() const { return 8; }
    std::size_t storage_bytes() const;
    std::string to_string() const;
    bool operator==(const SemType& o) const;
    bool operator!=(const SemType& o) const { return !(*this == o); }
};

using SemTypePtr = std::shared_ptr<SemType>;

inline SemTypePtr make_sem(SemKind k) { return std::make_shared<SemType>(SemType{k}); }
inline SemTypePtr sem_array(SemTypePtr elem, std::size_t n) {
    auto t = std::make_shared<SemType>();
    t->kind = SemKind::Array; t->elem = elem; t->array_size = n;
    return t;
}
inline SemTypePtr sem_struct(std::string name) {
    auto t = std::make_shared<SemType>();
    t->kind = SemKind::Struct; t->struct_name = std::move(name);
    return t;
}

struct TypeNode {
    virtual ~TypeNode() = default;
    SourceSpan span;
};
using TypePtr = std::unique_ptr<TypeNode>;

struct PrimTypeNode  : TypeNode { SemKind prim; };
struct ArrayTypeNode : TypeNode { TypePtr elem; std::size_t size = 0; };
struct NamedTypeNode : TypeNode { std::string name; };

struct Expr {
    virtual ~Expr() = default;
    SourceSpan span;
    SemTypePtr sem_type;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IntLitExpr    : Expr { int64_t     value = 0;    };
struct FloatLitExpr  : Expr { double      value = 0.0;  };
struct StringLitExpr : Expr { std::string value;        };
struct BoolLitExpr   : Expr { bool        value = false; };
struct IdentExpr     : Expr { std::string name;         };

struct BinaryExpr : Expr {
    std::string op;
    ExprPtr left, right;
};

struct UnaryExpr : Expr {
    std::string op;
    ExprPtr operand;
};

struct CastExpr : Expr {
    ExprPtr operand;
    TypePtr target;
};

struct IndexExpr : Expr {
    ExprPtr array, index;
};

struct FieldExpr : Expr {
    ExprPtr     object;
    std::string field;
};

struct ScopeExpr : Expr {
    std::string ns, name;
};

struct CallExpr : Expr {
    ExprPtr              callee;
    std::vector<ExprPtr> args;
    std::string resolved_fn;
};

struct ArrayLitExpr : Expr {
    std::vector<ExprPtr> elements;
};

struct StructLitExpr : Expr {
    std::string name;
    std::vector<std::pair<std::string, ExprPtr>> fields;
};

struct AssignExpr : Expr {
    ExprPtr target;
    ExprPtr value;
};

struct Stmt {
    virtual ~Stmt() = default;
    SourceSpan span;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
};

struct VarDeclStmt : Stmt {
    bool                   is_mut = false;
    std::string            name;
    std::optional<TypePtr> type_ann;
    ExprPtr                init;
    SemTypePtr             sem_type;
    int64_t                frame_offset = 0;
};

struct ExprStmt : Stmt { ExprPtr expr; };

struct IfStmt : Stmt {
    ExprPtr                    cond;
    std::unique_ptr<BlockStmt> then_block;
    StmtPtr                    else_stmt;
};

struct WhileStmt : Stmt {
    ExprPtr                    cond;
    std::unique_ptr<BlockStmt> body;
};

struct ReturnStmt   : Stmt { ExprPtr value; };
struct BreakStmt    : Stmt {};
struct ContinueStmt : Stmt {};
struct NullStmt     : Stmt {};

struct Decl {
    virtual ~Decl() = default;
    SourceSpan span;
};
using DeclPtr = std::unique_ptr<Decl>;

struct Param {
    std::string name;
    TypePtr     type;
    SourceSpan  span;
};

struct FnDecl : Decl {
    std::string            name;
    std::vector<Param>     params;
    std::optional<TypePtr> return_type;
    std::unique_ptr<BlockStmt> body;
    std::string            mangled_name;
    SemTypePtr             sem_return;
    std::size_t            frame_bytes = 0;
};

struct StructField {
    std::string name;
    TypePtr     type;
    SourceSpan  span;
    SemTypePtr  sem_type;
    std::size_t field_offset = 0;
};

struct StructDecl : Decl {
    std::string              name;
    std::vector<StructField> fields;
};

struct TypeAliasDecl : Decl {
    std::string name;
    TypePtr     type;
};

struct NamespaceDecl : Decl {
    std::string          name;
    std::vector<DeclPtr> decls;
};

struct ImplDecl : Decl {
    std::string                          struct_name;
    std::vector<std::unique_ptr<FnDecl>> methods;
};

struct Program {
    std::vector<DeclPtr> decls;
};

}
