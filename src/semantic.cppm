module;
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

export module semantic;
import parser;
import lexer;


export namespace Semantic {

struct SemanticError {
    Lexer::Diagnostic diag;
};

struct AnalysisResult {
    bool ok = false;
    std::vector<SemanticError> errors;
};

AnalysisResult analyse(AST::Program& prog, const std::string& filename);

} // export namespace Semantic

namespace Semantic {

using namespace AST;
using namespace Lexer;

static std::string mangle_type(uint32_t id, const SemTypePool& pool) {
    if (id == NO_ID) return "unk";
    const SemType& t = pool.get(id);
    switch (t.kind) {
        case SemKind::I8: return "i8";
        case SemKind::I16: return "i16";
        case SemKind::I32: return "i32";
        case SemKind::I64: return "i64";
        case SemKind::U8: return "u8";
        case SemKind::U16: return "u16";
        case SemKind::U32: return "u32";
        case SemKind::U64: return "u64";
        case SemKind::F32: return "f32";
        case SemKind::F64: return "f64";
        case SemKind::Bool: return "bool";
        case SemKind::String: return "str";
        case SemKind::Void: return "void";
        case SemKind::Array: return "arr";
        case SemKind::Struct: return t.struct_name;
        case SemKind::Interface: return t.struct_name;
        default: return "unk";
    }
}

struct VarInfo {
    uint32_t type;
    bool is_mut;
    bool initialized;
    int64_t frame_offset;
};

struct FnInfo {
    std::string mangled_name;
    std::vector<std::pair<std::string, uint32_t>> params;
    uint32_t return_type;
    bool is_public = true;
};

struct StructInfo {
    std::vector<std::pair<std::string, uint32_t>> fields;
    std::map<std::string, bool> field_visibility;   
    std::map<std::string, bool> method_visibility; 
    std::size_t total_bytes = 0;

    std::size_t field_offset(const std::string& name) const {
        std::size_t off = 0;
        for (auto& [n, _t] : fields) {
            if (n == name) return off;
            off += 8;
        }
        return 0;
    }
};


static bool can_implicit_widen(SemKind from, SemKind to) {
    if (from == to) return true;
    switch (from) {
        case SemKind::I8:
            return to == SemKind::I16 || to == SemKind::I32 || to == SemKind::I64
                || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::I16:
            return to == SemKind::I32 || to == SemKind::I64
                || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::I32:
            return to == SemKind::I64 || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::I64:
            return to == SemKind::F32 || to == SemKind::F64;
        case SemKind::U8:
            return to == SemKind::U16 || to == SemKind::U32 || to == SemKind::U64
                || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::U16:
            return to == SemKind::U32 || to == SemKind::U64
                || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::U32:
            return to == SemKind::U64 || to == SemKind::F32 || to == SemKind::F64;
        case SemKind::U64:
            return to == SemKind::F32 || to == SemKind::F64;
        case SemKind::F32:
            return to == SemKind::F64;
        default:
            return false;
    }
}


struct InterfaceMethodInfo {
    std::string name;
    std::vector<std::pair<std::string, uint32_t>> params; 
    uint32_t return_type = NO_ID;
};

struct InterfaceInfo {
    std::string name;
    std::vector<InterfaceMethodInfo> methods; 
};

struct Scope {
    std::optional<std::size_t> parent_idx;
    std::map<std::string, VarInfo> vars;
    std::map<std::string, std::vector<FnInfo>> fns;
    std::map<std::string, uint32_t> type_aliases;
    std::map<std::string, StructInfo> structs;
    std::map<std::string, InterfaceInfo> interfaces; 
};


class Analyser {
public:
    Analyser(const std::string& fname, Program& prog)
        : filename_(fname), prog_(prog) {}

    void run();

    std::vector<SemanticError> errors;

private:
    std::string filename_;
    Program& prog_;
    std::vector<Scope> scopes_;

    Scope& cur_scope() {
        return scopes_.back();
    }

    uint32_t cur_ret_type_ = NO_ID;
    bool in_loop_ = false;
    int64_t frame_bytes_ = 0;
    bool inferring_return_ = false;
    uint32_t inferred_return_type_ = NO_ID;
    std::string current_impl_struct_;

    void push_scope();
    void pop_scope();
    std::optional<VarInfo> lookup_var(const std::string& name);
    std::optional<uint32_t> lookup_type_alias(const std::string& name);
    std::optional<StructInfo> lookup_struct(const std::string& name);
    std::optional<InterfaceInfo> lookup_interface(const std::string& name); 
    std::optional<FnInfo> resolve_overload(const std::string& name,
                                           const std::vector<ExprId>& arg_exprs,
                                           const std::vector<uint32_t>& arg_types);
    int64_t alloc_local(std::size_t bytes);
    uint32_t resolve_type(TypeId tid);
    void err(SourceSpan span, const std::string& msg);
    bool try_coerce_literal(ExprId eid, uint32_t target_type);

    void update_fn_return_type(const std::string& mangled_name, uint32_t new_ret_type);
    void register_builtins();
    void register_decl(DeclId did, const std::string& ns_prefix);
    void register_fn(FnDecl& fd, const std::string& ns_prefix);
    void register_struct(StructDecl& sd);
    void analyse_decl(DeclId did);
    void analyse_fn(FnDecl& fd);

    void analyse_block(StmtId sid);
    void analyse_stmt(StmtId sid);
    void analyse_stmt_node(VarDeclStmt& vs,  StmtId sid);
    void analyse_stmt_node(ExprStmt& es,     StmtId sid);
    void analyse_stmt_node(BlockStmt& bs,    StmtId sid);
    void analyse_stmt_node(IfStmt& is,       StmtId sid);
    void analyse_stmt_node(WhileStmt& ws,    StmtId sid);
    void analyse_stmt_node(ReturnStmt& rs,   StmtId sid);
    void analyse_stmt_node(BreakStmt& bs,    StmtId sid);
    void analyse_stmt_node(ContinueStmt& cs, StmtId sid);
    void analyse_stmt_node(NullStmt& ns,     StmtId sid);

    uint32_t analyse_expr(ExprId eid);
    uint32_t analyse_expr_node(IntLitExpr& e);
    uint32_t analyse_expr_node(FloatLitExpr& e);
    uint32_t analyse_expr_node(StringLitExpr& e);
    uint32_t analyse_expr_node(BoolLitExpr& e);
    uint32_t analyse_expr_node(IdentExpr& e);
    uint32_t analyse_expr_node(BinaryExpr& e);
    uint32_t analyse_expr_node(UnaryExpr& e);
    uint32_t analyse_expr_node(CastExpr& e);
    uint32_t analyse_expr_node(IndexExpr& e);
    uint32_t analyse_expr_node(FieldExpr& e);
    uint32_t analyse_expr_node(ScopeExpr& e);
    uint32_t analyse_expr_node(CallExpr& e);
    uint32_t analyse_expr_node(ArrayLitExpr& e);
    uint32_t analyse_expr_node(StructLitExpr& e);
    uint32_t analyse_expr_node(AssignExpr& e);
};


void Analyser::run() {
    push_scope();
    register_builtins();
    for (DeclId did : prog_.top_decls) register_decl(did, "");
    for (DeclId did : prog_.top_decls) analyse_decl(did);
    pop_scope();
}

AnalysisResult analyse(Program& prog, const std::string& filename) {
    Analyser a(filename, prog);
    a.run();
    AnalysisResult r;
    r.errors = std::move(a.errors);
    r.ok = r.errors.empty();
    return r;
}


void Analyser::push_scope() {
    Scope s;
    if (!scopes_.empty()) s.parent_idx = scopes_.size() - 1;
    scopes_.push_back(std::move(s));
}

void Analyser::pop_scope() {
    scopes_.pop_back();
}

std::optional<VarInfo> Analyser::lookup_var(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].vars.find(name);
        if (it != scopes_[i].vars.end()) return it->second;
    }
    return std::nullopt;
}

std::optional<uint32_t> Analyser::lookup_type_alias(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].type_aliases.find(name);
        if (it != scopes_[i].type_aliases.end()) return it->second;
    }
    return std::nullopt;
}

std::optional<StructInfo> Analyser::lookup_struct(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].structs.find(name);
        if (it != scopes_[i].structs.end()) return it->second;
    }
    return std::nullopt;
}

std::optional<InterfaceInfo> Analyser::lookup_interface(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].interfaces.find(name);
        if (it != scopes_[i].interfaces.end()) return it->second;
    }
    return std::nullopt;
}

std::optional<FnInfo> Analyser::resolve_overload(
        const std::string& name,
        const std::vector<ExprId>& arg_exprs,
        const std::vector<uint32_t>& arg_types) {

    auto is_lit_coercible = [&](std::size_t j, uint32_t param_t) -> bool {
        if (j >= arg_exprs.size() || param_t == NO_ID) return false;
        const ExprNode& en = prog_.expr(arg_exprs[j]);
        const SemType& pt  = prog_.semtypes.get(param_t);
        if (std::holds_alternative<IntLitExpr>(en))
            return pt.is_integer() || pt.is_float();
        if (std::holds_alternative<FloatLitExpr>(en))
            return pt.is_float();
        return false;
    };

    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].fns.find(name);
        if (it == scopes_[i].fns.end()) continue;
        const std::vector<FnInfo>& cands = it->second;

        for (const FnInfo& fi : cands) {
            if (fi.params.size() != arg_types.size()) continue;
            bool ok = true;
            for (std::size_t j = 0; j < arg_types.size(); ++j) {
                if (!sem_equal(arg_types[j], fi.params[j].second, prog_.semtypes)) {
                    ok = false;
                    break;
                }
            }
            if (ok) return fi;
        }

        for (const FnInfo& fi : cands) {
            if (fi.params.size() != arg_types.size()) continue;
            bool ok = true;
            for (std::size_t j = 0; j < arg_types.size(); ++j) {
                if (sem_equal(arg_types[j], fi.params[j].second, prog_.semtypes)) continue;
                if (!is_lit_coercible(j, fi.params[j].second)) {
                    ok = false;
                    break;
                }
            }
            if (ok) return fi;
        }

        for (const FnInfo& fi : cands) {
            if (fi.params.size() != arg_types.size()) continue;
            bool ok = true;
            for (std::size_t j = 0; j < arg_types.size(); ++j) {
                if (sem_equal(arg_types[j], fi.params[j].second, prog_.semtypes)) continue;
                if (is_lit_coercible(j, fi.params[j].second)) continue;
                if (arg_types[j] != NO_ID && fi.params[j].second != NO_ID) {
                    SemKind fk = prog_.semtypes.get(arg_types[j]).kind;
                    SemKind tk = prog_.semtypes.get(fi.params[j].second).kind;
                    if (can_implicit_widen(fk, tk)) continue;
                }
                ok = false;
                break;
            }
            if (ok) return fi;
        }
    }
    return std::nullopt;
}

int64_t Analyser::alloc_local(std::size_t bytes) {
    std::size_t aligned = (bytes + 7) & ~7ULL;
    frame_bytes_ += static_cast<int64_t>(aligned);
    return -frame_bytes_;
}

uint32_t Analyser::resolve_type(TypeId tid) {
    const TypeNode& tn = prog_.type(tid);

    if (std::holds_alternative<PrimTypeNode>(tn)) {
        return make_sem(prog_.semtypes, std::get<PrimTypeNode>(tn).prim);
    }

    if (std::holds_alternative<ArrayTypeNode>(tn)) {
        const auto& a = std::get<ArrayTypeNode>(tn);
        uint32_t elem = resolve_type(a.elem);
        return sem_array(prog_.semtypes, elem, a.size);
    }

    if (std::holds_alternative<NamedTypeNode>(tn)) {
        const auto& n = std::get<NamedTypeNode>(tn);
        if (auto alias = lookup_type_alias(n.name)) return alias.value();
        if (auto si = lookup_struct(n.name)) {
            (void)si;
            return sem_struct(prog_.semtypes, n.name);
        }
        if (auto ii = lookup_interface(n.name)) {
            (void)ii;
            return sem_iface(prog_.semtypes, n.name);
        }
        err(n.span, "unknown type '" + n.name + "'");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    return make_sem(prog_.semtypes, SemKind::Unknown);
}

void Analyser::err(SourceSpan span, const std::string& msg) {
    Diagnostic d;
    d.filename = filename_;
    d.pos = span.begin;
    d.message = msg;
    errors.push_back({d});
}

bool Analyser::try_coerce_literal(ExprId eid, uint32_t target_type) {
    if (target_type == NO_ID || !prog_.semtypes.valid(target_type)) return false;
    const SemType& tt = prog_.semtypes.get(target_type);
    ExprNode& e = prog_.expr(eid);

    if (std::holds_alternative<IntLitExpr>(e)) {
        if (std::get<IntLitExpr>(e).suffix_kind != SemKind::Unknown) return false;
        if (tt.is_integer() || tt.is_float()) {
            set_expr_sem_type(e, target_type);
            return true;
        }
    }

    if (std::holds_alternative<FloatLitExpr>(e)) {
        if (std::get<FloatLitExpr>(e).suffix_kind != SemKind::Unknown) return false;
        if (tt.is_float()) {
            set_expr_sem_type(e, target_type);
            return true;
        }
    }

    return false;
}


void Analyser::register_builtins() {
    for (SemKind k : {SemKind::I8,  SemKind::I16, SemKind::I32, SemKind::I64,
                      SemKind::U8,  SemKind::U16, SemKind::U32, SemKind::U64,
                      SemKind::F32, SemKind::F64,
                      SemKind::Bool, SemKind::String}) {
        FnInfo fi;
        fi.mangled_name = "__builtin_print";
        fi.params = {{"value", make_sem(prog_.semtypes, k)}};
        fi.return_type = make_sem(prog_.semtypes, SemKind::Void);
        cur_scope().fns["print"].push_back(fi);
    }

    {
        FnInfo fi;
        fi.mangled_name = "__builtin_input";
        fi.return_type = make_sem(prog_.semtypes, SemKind::String);
        cur_scope().fns["input"].push_back(fi);
    }

    {
        FnInfo fi;
        fi.mangled_name = "__builtin_exit";
        fi.params = {{"code", make_sem(prog_.semtypes, SemKind::I32)}};
        fi.return_type = make_sem(prog_.semtypes, SemKind::Void);
        cur_scope().fns["exit"].push_back(fi);
    }

    {
        FnInfo fi;
        fi.mangled_name = "__builtin_panic";
        fi.params = {{"msg", make_sem(prog_.semtypes, SemKind::String)}};
        fi.return_type = make_sem(prog_.semtypes, SemKind::Void);
        cur_scope().fns["panic"].push_back(fi);
    }

    {
        FnInfo fi;
        fi.mangled_name = "__builtin_print_char";
        fi.params = {{"c", make_sem(prog_.semtypes, SemKind::I64)}};
        fi.return_type = make_sem(prog_.semtypes, SemKind::Void);
        cur_scope().fns["print_char"].push_back(fi);
    }
}

void Analyser::register_decl(DeclId did, const std::string& ns_prefix) {
    DeclNode& d = prog_.decl(did);

    if (std::holds_alternative<FnDecl>(d)) {
        register_fn(std::get<FnDecl>(d), ns_prefix);

    } else if (std::holds_alternative<StructDecl>(d)) {
        register_struct(std::get<StructDecl>(d));

    } else if (std::holds_alternative<TypeAliasDecl>(d)) {
        auto& ta = std::get<TypeAliasDecl>(d);
        cur_scope().type_aliases[ta.name] = resolve_type(ta.type);

    } else if (std::holds_alternative<NamespaceDecl>(d)) {
        auto& nd = std::get<NamespaceDecl>(d);
        for (DeclId child : nd.decls) {
            register_decl(child, nd.name);
        }

    } else if (std::holds_alternative<ImplDecl>(d)) {
        auto& id = std::get<ImplDecl>(d);
        auto sit = cur_scope().structs.find(id.struct_name);
        for (DeclId m : id.methods) {
            FnDecl& method = std::get<FnDecl>(prog_.decl(m));
            method.impl_struct = id.struct_name;  // A.2.12
            register_fn(method, id.struct_name);
            if (sit != cur_scope().structs.end()) {
                sit->second.method_visibility[method.name] = method.is_public;
            }
        }
        if (!id.interface_name.empty()) {
            auto ii = lookup_interface(id.interface_name);
            if (!ii.has_value()) {
                err(id.span, "unknown interface '" + id.interface_name + "'");
            } else {
                std::vector<std::string> vtable;
                for (auto& mi : ii.value().methods) {
                    std::string found_mangled;
                    for (DeclId mid : id.methods) {
                        const FnDecl& method = std::get<FnDecl>(prog_.decl(mid));
                        if (method.name == mi.name) {
                            found_mangled = method.mangled_name;
                            break;
                        }
                    }
                    if (found_mangled.empty()) {
                        err(id.span, "impl of '" + id.interface_name + "' for '" +
                            id.struct_name + "' is missing method '" + mi.name + "'");
                        vtable.push_back("__missing");
                    } else {
                        vtable.push_back(found_mangled);
                    }
                }
                IfaceImplKey key{id.struct_name, id.interface_name};
                prog_.iface_impls[key] = std::move(vtable);
            }
        }
    } else if (std::holds_alternative<InterfaceDecl>(d)) {
        auto& id = std::get<InterfaceDecl>(d);
        InterfaceInfo info;
        info.name = id.name;
        std::vector<std::string> method_names;
        for (auto& sig : id.methods) {
            InterfaceMethodInfo mi;
            mi.name = sig.name;
            mi.return_type = sig.return_type.has_value()
                ? resolve_type(sig.return_type.value())
                : make_sem(prog_.semtypes, SemKind::Void);
            for (auto& p : sig.params) {
                mi.params.emplace_back(p.name, resolve_type(p.type));
            }
            info.methods.push_back(std::move(mi));
            method_names.push_back(sig.name);
        }
        cur_scope().interfaces[id.name] = std::move(info);
        prog_.interface_method_names[id.name] = std::move(method_names);
    }
}

void Analyser::register_fn(FnDecl& fd, const std::string& ns_prefix) {
    FnInfo fi;
    if (fd.return_type.has_value()) {
        fi.return_type = resolve_type(fd.return_type.value());
        fd.sem_return = fi.return_type;
    } else {
        fi.return_type = make_sem(prog_.semtypes, SemKind::Void);
        fd.sem_return = NO_ID;
    }

    for (auto& p : fd.params) {
        fi.params.emplace_back(p.name, resolve_type(p.type));
    }

    std::string base = ns_prefix.empty() ? fd.name : ns_prefix + "__" + fd.name;
    std::string mangled = base;

    if (!fi.params.empty()) {
        for (auto& [_n, t] : fi.params) {
            mangled += "__" + mangle_type(t, prog_.semtypes);
        }
    }

    fi.mangled_name = mangled;
    fi.is_public = fd.is_public;  // A.2.12
    fd.mangled_name = mangled;

    cur_scope().fns[base].push_back(fi);
}

void Analyser::register_struct(StructDecl& sd) {
    StructInfo si;
    std::size_t off = 0;
    for (auto& f : sd.fields) {
        uint32_t st = resolve_type(f.type);
        si.fields.emplace_back(f.name, st);
        si.field_visibility[f.name] = f.is_public;  // A.2.12
        f.sem_type = st;
        f.field_offset = off;
        off += 8;
    }
    si.total_bytes = off;
    cur_scope().structs[sd.name] = std::move(si);
}

void Analyser::analyse_decl(DeclId did) {
    DeclNode& d = prog_.decl(did);

    if (std::holds_alternative<FnDecl>(d)) {
        analyse_fn(std::get<FnDecl>(d));

    } else if (std::holds_alternative<NamespaceDecl>(d)) {
        for (DeclId child : std::get<NamespaceDecl>(d).decls) {
            analyse_decl(child);
        }

    } else if (std::holds_alternative<ImplDecl>(d)) {
        for (DeclId m : std::get<ImplDecl>(d).methods) {
            analyse_fn(std::get<FnDecl>(prog_.decl(m)));
        }
    }
}

void Analyser::update_fn_return_type(const std::string& mangled_name, uint32_t new_ret_type) {
    for (auto& scope : scopes_) {
        for (auto& [base, fns] : scope.fns) {
            for (auto& fi : fns) {
                if (fi.mangled_name == mangled_name) {
                    fi.return_type = new_ret_type;
                    return;
                }
            }
        }
    }
}

void Analyser::analyse_fn(FnDecl& fd) {
    int64_t saved_frame = frame_bytes_;
    frame_bytes_ = 0;
    uint32_t saved_ret = cur_ret_type_;
    bool saved_loop = in_loop_;
    bool saved_inferring = inferring_return_;
    uint32_t saved_inferred = inferred_return_type_;
    std::string saved_impl = current_impl_struct_;
    current_impl_struct_ = fd.impl_struct;
    bool infer_ret = !fd.return_type.has_value();
    if (infer_ret) {
        inferring_return_ = true;
        inferred_return_type_ = NO_ID;
        cur_ret_type_ = NO_ID;
    } else {
        inferring_return_ = false;
        cur_ret_type_ = fd.sem_return != NO_ID
                      ? fd.sem_return
                      : make_sem(prog_.semtypes, SemKind::Void);
    }
    in_loop_ = false;

    push_scope();
    for (auto& p : fd.params) {
        uint32_t pt = resolve_type(p.type);
        std::size_t sz = 8;
        if (pt != NO_ID && prog_.semtypes.get(pt).kind == SemKind::Struct) {
            auto si = lookup_struct(prog_.semtypes.get(pt).struct_name);
            if (si) sz = si->total_bytes;
        } else if (pt != NO_ID && prog_.semtypes.get(pt).kind == SemKind::Array) {
            sz = 8;
        } else {
            sz = sem_storage_bytes(pt, prog_.semtypes);
            if (!sz) sz = 8;
        }
        int64_t off = alloc_local(sz);
        cur_scope().vars[p.name] = {pt, false, true, off};
    }

    analyse_block(fd.body);
    fd.frame_bytes = static_cast<std::size_t>(frame_bytes_);
    pop_scope();
    if (infer_ret) {
        uint32_t inferred = (inferred_return_type_ != NO_ID)
            ? inferred_return_type_
            : make_sem(prog_.semtypes, SemKind::Void);
        fd.sem_return = inferred;
        update_fn_return_type(fd.mangled_name, inferred);
    }

    current_impl_struct_ = saved_impl;
    cur_ret_type_ = saved_ret;
    in_loop_ = saved_loop;
    inferring_return_ = saved_inferring;
    inferred_return_type_ = saved_inferred;
    frame_bytes_ = saved_frame;
}

void Analyser::analyse_block(StmtId sid) {
    push_scope();
    for (StmtId child : std::get<BlockStmt>(prog_.stmt(sid)).stmts) {
        analyse_stmt(child);
    }
    pop_scope();
}

void Analyser::analyse_stmt(StmtId sid) {
    StmtNode& s = prog_.stmt(sid);
    std::visit([&](auto& node) { analyse_stmt_node(node, sid); }, s);
}

void Analyser::analyse_stmt_node(VarDeclStmt& vs, StmtId) {
    uint32_t init_type = analyse_expr(vs.init);
    uint32_t decl_type = vs.type_ann
        ? resolve_type(vs.type_ann.value())
        : init_type;

    if (decl_type == NO_ID) {
        decl_type = make_sem(prog_.semtypes, SemKind::Unknown);
    }

    if (vs.type_ann.has_value() && try_coerce_literal(vs.init, decl_type)) {
        init_type = decl_type;
    }

    if (init_type != NO_ID &&
        prog_.semtypes.get(decl_type).kind != SemKind::Unknown &&
        !sem_equal(init_type, decl_type, prog_.semtypes)) {
        SemKind ik = prog_.semtypes.get(init_type).kind;
        SemKind dk = prog_.semtypes.get(decl_type).kind;
        if (dk == SemKind::Interface && ik == SemKind::Struct) {
            const std::string& iname = prog_.semtypes.get(decl_type).struct_name;
            const std::string& sname = prog_.semtypes.get(init_type).struct_name;
            auto ii = lookup_interface(iname);
            if (!ii.has_value()) {
                err(expr_span(prog_.expr(vs.init)), "unknown interface '" + iname + "'");
            } else {
                IfaceImplKey key{sname, iname};
                if (prog_.iface_impls.find(key) == prog_.iface_impls.end()) {
                    err(expr_span(prog_.expr(vs.init)),
                        "struct '" + sname + "' does not implement interface '" + iname + "'");
                }
            }
        } else if (!can_implicit_widen(ik, dk)) {
            err(expr_span(prog_.expr(vs.init)),
                "type mismatch in declaration: expected " +
                sem_to_string(decl_type, prog_.semtypes) + ", got " +
                sem_to_string(init_type, prog_.semtypes));
        }
    }

    vs.sem_type = decl_type;

    if (cur_scope().vars.count(vs.name)) {
        err(vs.span, "redeclaration of '" + vs.name + "' in the same scope");
    }

    std::size_t bytes = 8;
    const SemType& dt = prog_.semtypes.get(decl_type);
    if (dt.kind == SemKind::Array && prog_.semtypes.valid(dt.elem_id)) {
        bytes = dt.array_size * 8;
    } else if (dt.kind == SemKind::Struct) {
        auto si = lookup_struct(dt.struct_name);
        if (si.has_value()) bytes = si.value().total_bytes;
    } else if (dt.kind == SemKind::Interface) {
        bytes = 16; 
    }
    if (bytes == 0) bytes = 8;
    vs.frame_offset = alloc_local(bytes);
    cur_scope().vars[vs.name] = {decl_type, vs.is_mut, true, vs.frame_offset};
}

void Analyser::analyse_stmt_node(ExprStmt& es, StmtId) {
    analyse_expr(es.expr);
}

void Analyser::analyse_stmt_node(BlockStmt&, StmtId sid) {
    analyse_block(sid);
}

void Analyser::analyse_stmt_node(IfStmt& is, StmtId) {
    uint32_t ct = analyse_expr(is.cond);
    if (ct != NO_ID && prog_.semtypes.get(ct).kind != SemKind::Bool) {
        err(expr_span(prog_.expr(is.cond)),
            "if condition must be bool, got " + sem_to_string(ct, prog_.semtypes));
    }
    analyse_block(is.then_block);
    if (is.else_stmt.has_value()) {
        analyse_stmt(is.else_stmt.value());
    }
}

void Analyser::analyse_stmt_node(WhileStmt& ws, StmtId) {
    uint32_t ct = analyse_expr(ws.cond);
    if (ct != NO_ID && prog_.semtypes.get(ct).kind != SemKind::Bool) {
        err(expr_span(prog_.expr(ws.cond)),
            "while condition must be bool, got " + sem_to_string(ct, prog_.semtypes));
    }
    bool saved = in_loop_;
    in_loop_ = true;
    analyse_block(ws.body);
    in_loop_ = saved;
}

void Analyser::analyse_stmt_node(ReturnStmt& rs, StmtId sid) {
    if (rs.value.has_value()) {
        uint32_t vt = analyse_expr(rs.value.value());
        if (inferring_return_) {
            if (inferred_return_type_ == NO_ID) {
                inferred_return_type_ = vt;
            } else if (!sem_equal(vt, inferred_return_type_, prog_.semtypes)) {
                SemKind vk = prog_.semtypes.get(vt).kind;
                SemKind ik = prog_.semtypes.get(inferred_return_type_).kind;
                if (can_implicit_widen(vk, ik)) {
                } else if (can_implicit_widen(ik, vk)) {
                    inferred_return_type_ = vt; 
                } else {
                    err(expr_span(prog_.expr(rs.value.value())),
                        "conflicting inferred return types: " +
                        sem_to_string(inferred_return_type_, prog_.semtypes) + " vs " +
                        sem_to_string(vt, prog_.semtypes));
                }
            }
        } else if (vt != NO_ID && cur_ret_type_ != NO_ID &&
                   !sem_equal(vt, cur_ret_type_, prog_.semtypes)) {
            if (!try_coerce_literal(rs.value.value(), cur_ret_type_)) {
                SemKind vk = prog_.semtypes.get(vt).kind;
                SemKind rk = prog_.semtypes.get(cur_ret_type_).kind;
                if (!can_implicit_widen(vk, rk)) {
                    err(expr_span(prog_.expr(rs.value.value())),
                        "return type mismatch: expected " +
                        sem_to_string(cur_ret_type_, prog_.semtypes) + ", got " +
                        sem_to_string(vt, prog_.semtypes));
                }
            }
        }
    } else {
        if (!inferring_return_ && cur_ret_type_ != NO_ID &&
            prog_.semtypes.get(cur_ret_type_).kind != SemKind::Void) {
            err(stmt_span(prog_.stmt(sid)), "missing return value");
        }
    }
}

void Analyser::analyse_stmt_node(BreakStmt& bs, StmtId) {
    if (!in_loop_) {
        err(bs.span, "'break' outside loop");
    }
}

void Analyser::analyse_stmt_node(ContinueStmt& cs, StmtId) {
    if (!in_loop_) {
        err(cs.span, "'continue' outside loop");
    }
}

void Analyser::analyse_stmt_node(NullStmt&, StmtId) {
}


uint32_t Analyser::analyse_expr(ExprId eid) {
    ExprNode& e = prog_.expr(eid);
    uint32_t result = NO_ID;
    std::visit([&](auto& node) { result = analyse_expr_node(node); }, e);
    if (result == NO_ID) result = make_sem(prog_.semtypes, SemKind::Unknown);
    set_expr_sem_type(e, result);
    return result;
}

uint32_t Analyser::analyse_expr_node(IntLitExpr& e) {
    if (e.suffix_kind != SemKind::Unknown)
        return make_sem(prog_.semtypes, e.suffix_kind);
    return make_sem(prog_.semtypes, SemKind::I64);
}

uint32_t Analyser::analyse_expr_node(FloatLitExpr& e) {
    if (e.suffix_kind != SemKind::Unknown)
        return make_sem(prog_.semtypes, e.suffix_kind);
    return make_sem(prog_.semtypes, SemKind::F64);
}

uint32_t Analyser::analyse_expr_node(StringLitExpr&) {
    return make_sem(prog_.semtypes, SemKind::String);
}

uint32_t Analyser::analyse_expr_node(BoolLitExpr&) {
    return make_sem(prog_.semtypes, SemKind::Bool);
}

uint32_t Analyser::analyse_expr_node(IdentExpr& e) {
    auto vi = lookup_var(e.name);
    if (!vi.has_value()) {
        err(e.span, "undeclared identifier '" + e.name + "'");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }
    if (!vi.value().initialized) {
        err(e.span, "use of uninitialized variable '" + e.name + "'");
    }
    return vi.value().type;
}

uint32_t Analyser::analyse_expr_node(BinaryExpr& e) {
    uint32_t lt = analyse_expr(e.left);
    uint32_t rt = analyse_expr(e.right);

    if (lt == NO_ID || rt == NO_ID) {
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    if (prog_.semtypes.get(lt).kind == SemKind::Struct) {
        static const std::map<std::string, std::string> OP_DUNDER = {
            {"+",  "__add__"}, {"-",  "__sub__"}, {"*",  "__mul__"},
            {"/",  "__div__"}, {"%",  "__mod__"},
            {"==", "__eq__"},  {"!=", "__ne__"},
            {"<",  "__lt__"},  {">",  "__gt__"},
            {"<=", "__le__"},  {">=", "__ge__"}
        };
        auto dit = OP_DUNDER.find(e.op);
        if (dit != OP_DUNDER.end()) {
            const std::string method_name =
                prog_.semtypes.get(lt).struct_name + "__" + dit->second;
            auto fi = resolve_overload(method_name,
                                       {e.left, e.right}, {lt, rt});
            if (fi.has_value()) {
                e.resolved_fn = fi.value().mangled_name;
                return fi.value().return_type;
            }
        }
    }

    if (!sem_equal(lt, rt, prog_.semtypes)) {
        if (try_coerce_literal(e.right, lt)) rt = lt;
        else if (try_coerce_literal(e.left, rt)) lt = rt;
    }

    if (!sem_equal(lt, rt, prog_.semtypes) && lt != NO_ID && rt != NO_ID) {
        SemKind lk = prog_.semtypes.get(lt).kind;
        SemKind rk = prog_.semtypes.get(rt).kind;
        if (can_implicit_widen(lk, rk)) {
            lt = rt; 
        } else if (can_implicit_widen(rk, lk)) {
            rt = lt; 
        }
    }

    const std::string& op = e.op;
    bool is_cmp = op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
    bool is_logic = op == "&&" || op == "||";

    if (is_logic) {
        if (prog_.semtypes.get(lt).kind != SemKind::Bool) {
            err(expr_span(prog_.expr(e.left)),
                "logical operator requires bool, got " + sem_to_string(lt, prog_.semtypes));
        }
        if (prog_.semtypes.get(rt).kind != SemKind::Bool) {
            err(expr_span(prog_.expr(e.right)),
                "logical operator requires bool, got " + sem_to_string(rt, prog_.semtypes));
        }
        return make_sem(prog_.semtypes, SemKind::Bool);
    }

    if (is_cmp) {
        if (!sem_equal(lt, rt, prog_.semtypes)) {
            err(e.span, "comparison type mismatch: " +
                sem_to_string(lt, prog_.semtypes) + " vs " +
                sem_to_string(rt, prog_.semtypes));
        }
        return make_sem(prog_.semtypes, SemKind::Bool);
    }

    if (!sem_equal(lt, rt, prog_.semtypes)) {
        err(e.span, "arithmetic type mismatch: " +
            sem_to_string(lt, prog_.semtypes) + " vs " +
            sem_to_string(rt, prog_.semtypes));
    }
    if (!prog_.semtypes.get(lt).is_numeric()) {
        err(e.span, "arithmetic on non-numeric type " + sem_to_string(lt, prog_.semtypes));
    }
    return lt;
}

uint32_t Analyser::analyse_expr_node(UnaryExpr& e) {
    uint32_t t = analyse_expr(e.operand);
    if (t == NO_ID) return make_sem(prog_.semtypes, SemKind::Unknown);

    if (e.op == "-") {
        if (!prog_.semtypes.get(t).is_numeric()) {
            err(e.span, "unary minus on non-numeric type " + sem_to_string(t, prog_.semtypes));
        }
        return t;
    }

    if (e.op == "!") {
        if (prog_.semtypes.get(t).kind != SemKind::Bool) {
            err(e.span, "'!' applied to non-bool type " + sem_to_string(t, prog_.semtypes));
        }
        return make_sem(prog_.semtypes, SemKind::Bool);
    }

    return make_sem(prog_.semtypes, SemKind::Unknown);
}

uint32_t Analyser::analyse_expr_node(CastExpr& e) {
    analyse_expr(e.operand);
    return resolve_type(e.target);
}

uint32_t Analyser::analyse_expr_node(IndexExpr& e) {
    uint32_t arr_t = analyse_expr(e.array);
    uint32_t idx_t = analyse_expr(e.index);

    if (arr_t == NO_ID) return make_sem(prog_.semtypes, SemKind::Unknown);

    const SemType& at = prog_.semtypes.get(arr_t);
    if (at.kind != SemKind::Array) {
        err(expr_span(prog_.expr(e.array)),
            "indexing non-array type " + sem_to_string(arr_t, prog_.semtypes));
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    if (idx_t != NO_ID && !prog_.semtypes.get(idx_t).is_integer()) {
        err(expr_span(prog_.expr(e.index)),
            "array index must be integer, got " + sem_to_string(idx_t, prog_.semtypes));
    }

    return at.elem_id;
}

uint32_t Analyser::analyse_expr_node(FieldExpr& e) {
    uint32_t obj_t = analyse_expr(e.object);
    if (obj_t == NO_ID) return make_sem(prog_.semtypes, SemKind::Unknown);

    const SemType& ot = prog_.semtypes.get(obj_t);
    if (ot.kind != SemKind::Struct) {
        err(expr_span(prog_.expr(e.object)),
            "field access on non-struct type " + sem_to_string(obj_t, prog_.semtypes));
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    auto si = lookup_struct(ot.struct_name);
    if (!si.has_value()) {
        err(e.span, "unknown struct '" + ot.struct_name + "'");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    for (auto& [n, t] : si.value().fields) {
        if (n == e.field) {
            auto vis_it = si.value().field_visibility.find(n);
            bool is_pub = (vis_it == si.value().field_visibility.end()) || vis_it->second;
            if (!is_pub && current_impl_struct_ != ot.struct_name) {
                err(e.span, "field '" + e.field + "' of struct '" +
                    ot.struct_name + "' is private");
            }
            return t;
        }
    }

    err(e.span, "struct '" + ot.struct_name + "' has no field '" + e.field + "'");
    return make_sem(prog_.semtypes, SemKind::Unknown);
}

uint32_t Analyser::analyse_expr_node(ScopeExpr& e) {
    err(e.span, "bare scope expression not valid outside a call");
    return make_sem(prog_.semtypes, SemKind::Unknown);
}

uint32_t Analyser::analyse_expr_node(CallExpr& e) {
    std::vector<uint32_t> arg_types;
    for (ExprId a : e.args) {
        arg_types.push_back(analyse_expr(a));
    }

    std::string fn_name;
    ExprNode& callee_node = prog_.expr(e.callee);

    if (std::holds_alternative<IdentExpr>(callee_node)) {
        fn_name = std::get<IdentExpr>(callee_node).name;

    } else if (std::holds_alternative<ScopeExpr>(callee_node)) {
        auto& sc = std::get<ScopeExpr>(callee_node);
        fn_name = sc.ns + "__" + sc.name;

    } else if (std::holds_alternative<FieldExpr>(callee_node)) {
        auto& fe = std::get<FieldExpr>(callee_node);
        uint32_t obj_t = expr_sem_type(prog_.expr(fe.object));
        if (obj_t == NO_ID) obj_t = analyse_expr(fe.object);
        if (obj_t != NO_ID && prog_.semtypes.get(obj_t).kind == SemKind::Struct) {
            const std::string& sname = prog_.semtypes.get(obj_t).struct_name;
            fn_name = sname + "__" + fe.field;
            arg_types.insert(arg_types.begin(), obj_t);
            auto si = lookup_struct(sname);
            if (si.has_value()) {
                auto vis_it = si.value().method_visibility.find(fe.field);
                if (vis_it != si.value().method_visibility.end() &&
                    !vis_it->second && current_impl_struct_ != sname) {
                    err(fe.span, "method '" + fe.field + "' of struct '" +
                        sname + "' is private");
                }
            }
        } else if (obj_t != NO_ID && prog_.semtypes.get(obj_t).kind == SemKind::Interface) {
            const std::string& iname = prog_.semtypes.get(obj_t).struct_name;
            auto ii = lookup_interface(iname);
            if (!ii.has_value()) {
                err(fe.span, "unknown interface '" + iname + "'");
                return make_sem(prog_.semtypes, SemKind::Unknown);
            }
            for (auto& mi : ii.value().methods) {
                if (mi.name == fe.field) {
                    e.resolved_fn = "__vtable_call__" + iname + "__" + fe.field;
                    e.sem_type = mi.return_type;
                    return mi.return_type;
                }
            }
            err(fe.span, "interface '" + iname + "' has no method '" + fe.field + "'");
            return make_sem(prog_.semtypes, SemKind::Unknown);
        } else {
            err(fe.span, "method call on non-struct");
            return make_sem(prog_.semtypes, SemKind::Unknown);
        }

    } else {
        err(e.span, "invalid callee expression");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    auto fi = resolve_overload(fn_name, e.args, arg_types);
    if (!fi.has_value()) {
        err(e.span, "no matching overload of '" + fn_name + "' for given argument types");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    if (fi.value().mangled_name.find("__builtin_") == std::string::npos &&
        arg_types.size() != fi.value().params.size()) {
        err(e.span, "wrong number of arguments for '" + fn_name + "'");
    }

    for (std::size_t j = 0; j < e.args.size() && j < fi.value().params.size(); ++j) {
        try_coerce_literal(e.args[j], fi.value().params[j].second);
    }

    e.resolved_fn = fi.value().mangled_name;
    return fi.value().return_type;
}

uint32_t Analyser::analyse_expr_node(ArrayLitExpr& e) {
    if (e.elements.empty()) {
        err(e.span, "cannot infer element type of empty array literal");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    uint32_t elem_t = analyse_expr(e.elements[0]);
    for (std::size_t i = 1; i < e.elements.size(); ++i) {
        uint32_t t = analyse_expr(e.elements[i]);
        if (t != NO_ID && elem_t != NO_ID && !sem_equal(t, elem_t, prog_.semtypes)) {
            err(expr_span(prog_.expr(e.elements[i])), "array element type mismatch");
        }
    }

    return sem_array(prog_.semtypes, elem_t, e.elements.size());
}

uint32_t Analyser::analyse_expr_node(StructLitExpr& e) {
    auto si = lookup_struct(e.name);
    if (!si.has_value()) {
        err(e.span, "unknown struct '" + e.name + "'");
        return make_sem(prog_.semtypes, SemKind::Unknown);
    }

    for (auto& [fname, fval_id] : e.fields) {
        uint32_t vt = analyse_expr(fval_id);
        bool found = false;
        for (auto& [sn, st] : si.value().fields) {
            if (sn == fname) {
                if (vt != NO_ID && !sem_equal(vt, st, prog_.semtypes)) {
                    err(expr_span(prog_.expr(fval_id)),
                        "field '" + fname + "': expected " +
                        sem_to_string(st, prog_.semtypes) + ", got " +
                        sem_to_string(vt, prog_.semtypes));
                }
                found = true;
                break;
            }
        }
        if (!found) {
            err(expr_span(prog_.expr(fval_id)),
                "struct '" + e.name + "' has no field '" + fname + "'");
        }
    }

    return sem_struct(prog_.semtypes, e.name);
}

uint32_t Analyser::analyse_expr_node(AssignExpr& e) {
    uint32_t vt = analyse_expr(e.value);
    ExprNode& target_node = prog_.expr(e.target);

    if (std::holds_alternative<IdentExpr>(target_node)) {
        auto& id_expr = std::get<IdentExpr>(target_node);
        auto vi = lookup_var(id_expr.name);

        if (!vi.has_value()) {
            err(id_expr.span, "undeclared identifier '" + id_expr.name + "'");
        } else if (!vi.value().is_mut) {
            err(id_expr.span, "cannot assign to immutable variable '" + id_expr.name + "'");
        } else if (vt != NO_ID && !sem_equal(vt, vi.value().type, prog_.semtypes)) {
            if (!try_coerce_literal(e.value, vi.value().type)) {
                SemKind vk = prog_.semtypes.get(vt).kind;
                SemKind tk = prog_.semtypes.get(vi.value().type).kind;
                if (!can_implicit_widen(vk, tk)) {
                    err(expr_span(prog_.expr(e.value)),
                        "assignment type mismatch: expected " +
                        sem_to_string(vi.value().type, prog_.semtypes) + ", got " +
                        sem_to_string(vt, prog_.semtypes));
                }
            }
        }

        return analyse_expr(e.target);
    }

    if (std::holds_alternative<IndexExpr>(target_node) ||
        std::holds_alternative<FieldExpr>(target_node)) {
        uint32_t lt = analyse_expr(e.target);
        if (vt != NO_ID && lt != NO_ID && !sem_equal(vt, lt, prog_.semtypes)) {
            err(expr_span(prog_.expr(e.value)), "assignment type mismatch");
        }
        return lt;
    }

    err(expr_span(target_node), "invalid assignment target");
    return make_sem(prog_.semtypes, SemKind::Unknown);
}

} // namespace Semantic
