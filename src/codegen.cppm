module;
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <numeric>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

export module codegen;
import parser;
import lexer;

export namespace Codegen {

struct CodegenError {
    std::string message;
};

struct CodegenResult {
    bool ok = false;
    CodegenError error;
};

CodegenResult generate(const AST::Program& prog, std::ostream& out,
                       const std::string& filename);

}

namespace Codegen {

using namespace AST;


struct LiveInterval {
    std::string name;
    int start = 0;
    int end   = 0;
    std::string reg;  
};

class RegAllocator {
public:
    static constexpr std::size_t NUM_REGS = 4;
    static inline const std::array<std::string, NUM_REGS> REGS =
        {"r12", "r13", "r14", "r15"};

    // Outputs
    std::map<std::string, std::string> reg_map;    
    std::vector<std::string>           saved_regs; 

    void run(const Program& prog, const FnDecl& fd);

private:
    std::map<std::string, int>      first_def_;
    std::map<std::string, int>      last_use_;
    std::map<std::string, uint32_t> var_types_;
    int pos_ = 0;

    void scan_stmt(const Program& prog, StmtId sid);
    void scan_expr(const Program& prog, ExprId eid);

    void touch(const std::string& name) {
        if (!first_def_.count(name)) first_def_[name] = pos_;
        last_use_[name] = pos_;
    }

    static bool allocatable(const Program& prog, uint32_t t) {
        if (t == NO_ID) return false;
        const SemType& st = prog.semtypes.get(t);
        return st.kind == SemKind::I8  || st.kind == SemKind::I16 ||
               st.kind == SemKind::I32 || st.kind == SemKind::I64 ||
               st.kind == SemKind::U8  || st.kind == SemKind::U16 ||
               st.kind == SemKind::U32 || st.kind == SemKind::U64 ||
               st.kind == SemKind::Bool;
    }

    void linear_scan(const Program& prog);
};

void RegAllocator::scan_stmt(const Program& prog, StmtId sid) {
    const StmtNode& s = prog.stmt(sid);

    if (std::holds_alternative<VarDeclStmt>(s)) {
        const VarDeclStmt& vs = std::get<VarDeclStmt>(s);
        scan_expr(prog, vs.init);
        var_types_[vs.name] = vs.sem_type;
        if (!first_def_.count(vs.name)) first_def_[vs.name] = pos_;
        last_use_[vs.name] = pos_;
        ++pos_;

    } else if (std::holds_alternative<BlockStmt>(s)) {
        for (StmtId child : std::get<BlockStmt>(s).stmts)
            scan_stmt(prog, child);

    } else if (std::holds_alternative<ExprStmt>(s)) {
        scan_expr(prog, std::get<ExprStmt>(s).expr);
        ++pos_;

    } else if (std::holds_alternative<IfStmt>(s)) {
        const IfStmt& is_ = std::get<IfStmt>(s);
        scan_expr(prog, is_.cond);
        scan_stmt(prog, is_.then_block);
        if (is_.else_stmt) scan_stmt(prog, *is_.else_stmt);
        ++pos_;

    } else if (std::holds_alternative<WhileStmt>(s)) {
        const WhileStmt& ws = std::get<WhileStmt>(s);
        scan_expr(prog, ws.cond);
        scan_stmt(prog, ws.body);
        ++pos_;

    } else if (std::holds_alternative<ReturnStmt>(s)) {
        const ReturnStmt& rs = std::get<ReturnStmt>(s);
        if (rs.value) scan_expr(prog, *rs.value);
        ++pos_;

    } else {
        ++pos_;
    }
}

void RegAllocator::scan_expr(const Program& prog, ExprId eid) {
    const ExprNode& e = prog.expr(eid);

    if (std::holds_alternative<IdentExpr>(e)) {
        touch(std::get<IdentExpr>(e).name);

    } else if (std::holds_alternative<BinaryExpr>(e)) {
        const BinaryExpr& be = std::get<BinaryExpr>(e);
        scan_expr(prog, be.left);
        scan_expr(prog, be.right);

    } else if (std::holds_alternative<UnaryExpr>(e)) {
        scan_expr(prog, std::get<UnaryExpr>(e).operand);

    } else if (std::holds_alternative<CastExpr>(e)) {
        scan_expr(prog, std::get<CastExpr>(e).operand);

    } else if (std::holds_alternative<IndexExpr>(e)) {
        const IndexExpr& ie = std::get<IndexExpr>(e);
        scan_expr(prog, ie.array);
        scan_expr(prog, ie.index);

    } else if (std::holds_alternative<FieldExpr>(e)) {
        scan_expr(prog, std::get<FieldExpr>(e).object);

    } else if (std::holds_alternative<CallExpr>(e)) {
        const CallExpr& ce = std::get<CallExpr>(e);
        const ExprNode& callee_node = prog.expr(ce.callee);
        if (std::holds_alternative<FieldExpr>(callee_node))
            scan_expr(prog, std::get<FieldExpr>(callee_node).object);
        for (ExprId a : ce.args) scan_expr(prog, a);

    } else if (std::holds_alternative<AssignExpr>(e)) {
        const AssignExpr& ae = std::get<AssignExpr>(e);
        scan_expr(prog, ae.value);
        const ExprNode& tgt = prog.expr(ae.target);
        if (std::holds_alternative<IdentExpr>(tgt))
            touch(std::get<IdentExpr>(tgt).name);
        else
            scan_expr(prog, ae.target);

    } else if (std::holds_alternative<ArrayLitExpr>(e)) {
        for (ExprId el : std::get<ArrayLitExpr>(e).elements)
            scan_expr(prog, el);

    } else if (std::holds_alternative<StructLitExpr>(e)) {
        for (auto& [_fn, feid] : std::get<StructLitExpr>(e).fields)
            scan_expr(prog, feid);
    }
}

void RegAllocator::linear_scan(const Program& prog) {
    std::vector<LiveInterval> ivals;
    for (auto& [name, fp] : first_def_) {
        auto tit = var_types_.find(name);
        if (tit == var_types_.end() || !allocatable(prog, tit->second)) continue;
        int lp = last_use_.count(name) ? last_use_.at(name) : fp;
        ivals.push_back({name, fp, lp, ""});
    }
    if (ivals.empty()) return;

    const std::size_t N = ivals.size();
    std::vector<std::set<std::size_t>> adj(N);
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (ivals[i].start <= ivals[j].end && ivals[j].start <= ivals[i].end) {
                adj[i].insert(j);
                adj[j].insert(i);
            }
        }
    }


    std::vector<std::size_t> order(N);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return adj[a].size() > adj[b].size();
    });

    std::vector<int> color(N, -1); 
    for (std::size_t idx : order) {
        std::set<int> used_colors;
        for (std::size_t nb : adj[idx]) {
            if (color[nb] >= 0) used_colors.insert(color[nb]);
        }
        for (int c = 0; c < static_cast<int>(NUM_REGS); ++c) {
            if (!used_colors.count(c)) {
                color[idx] = c;
                break;
            }
        }
    }

    std::set<std::string> used;
    for (std::size_t i = 0; i < N; ++i) {
        if (color[i] >= 0) {
            reg_map[ivals[i].name] = REGS[static_cast<std::size_t>(color[i])];
            used.insert(REGS[static_cast<std::size_t>(color[i])]);
        }
    }
    saved_regs.assign(used.begin(), used.end());
}

void RegAllocator::run(const Program& prog, const FnDecl& fd) {
    pos_ = 0;
    first_def_.clear();
    last_use_.clear();
    var_types_.clear();
    reg_map.clear();
    saved_regs.clear();
    scan_stmt(prog, fd.body);
    linear_scan(prog);
}


struct Gen {
    std::ostream& out;
    const std::string& src_filename;
    const Program& prog;

    std::vector<std::string> str_literals;
    std::vector<double> flt_literals;

    std::map<std::string, int64_t> locals;
    std::set<std::string> array_params;
    std::size_t frame_bytes = 0;
    std::size_t aligned_frame_bytes = 0;

    int label_cnt = 0;

    std::string new_label(const std::string& prefix = ".L") {
        return prefix + std::to_string(label_cnt++);
    }

    std::vector<std::string> loop_end_labels;
    std::vector<std::string> loop_top_labels;

    std::map<std::string, DeclId> struct_defs;

    std::map<std::string, std::string>  regalloc_map;   
    std::map<std::string, std::size_t>  reg_save_slots; 
    std::vector<std::string>            fn_saved_regs; 
    uint32_t cur_fn_ret_type_ = NO_ID; 

    void emit(const std::string& s) {
        out << s << "\n";
    }

    void emit_ins(const std::string& ins) {
        out << "    " << ins << "\n";
    }

    void emit_label(const std::string& l) {
        out << l << ":\n";
    }

    void emit_comment(const std::string& c) {
        out << "    ; " << c << "\n";
    }

    bool is_float_type(uint32_t id);
    std::string var_addr(const std::string& name);

    void emit_preamble();
    std::string add_string(const std::string& s);
    std::string add_float(double v);
    void pre_collect_literals();
    void emit_data_section();

    uint32_t emit_expr(ExprId eid);
    uint32_t emit_binary(const BinaryExpr& e);
    uint32_t emit_unary(const UnaryExpr& e);
    uint32_t emit_cast(const CastExpr& e);

    uint32_t emit_index(const IndexExpr& e);
    uint32_t emit_field(const FieldExpr& e);
    uint32_t emit_assign(const AssignExpr& e);

    uint32_t emit_call(const CallExpr& e);
    uint32_t emit_vtable_call(const CallExpr& e); // A.2.13
    uint32_t emit_builtin_print(const CallExpr& e);
    uint32_t emit_builtin_input(const CallExpr& e);
    uint32_t emit_builtin_exit(const CallExpr& e);
    uint32_t emit_builtin_panic(const CallExpr& e);
    uint32_t emit_builtin_print_char(const CallExpr& e);


    uint32_t emit_operator_call(ExprId lhs, ExprId rhs,
                                const std::string& fn_name, uint32_t ret_t);

    void emit_epilogue();

    void emit_var_decl(const VarDeclStmt& vs);
    void emit_stmt(StmtId sid);

    void emit_fn(const FnDecl& fd);
    void emit_input_helper();
    void emit_top_decl(DeclId did);
    void emit_vtable_thunks(); // A.2.13
    void generate();
};


bool Gen::is_float_type(uint32_t id) {
    if (id == NO_ID) return false;
    const SemType& t = prog.semtypes.get(id);
    return t.kind == SemKind::F32 || t.kind == SemKind::F64;
}

std::string Gen::var_addr(const std::string& name) {
    auto rit = regalloc_map.find(name);
    if (rit != regalloc_map.end()) return rit->second;

    auto it = locals.find(name);
    if (it != locals.end()) {
        int64_t off = it->second;
        if (off < 0) return "[rbp - " + std::to_string(-off) + "]";
        return "[rbp + " + std::to_string(off) + "]";
    }
    return "[rbp - 0] ; BUG: unknown var " + name;
}

void Gen::emit_preamble() {
    emit("    extern printf");
    emit("    extern putchar");
    emit("    extern exit");
    emit("    global main");
    emit("");
    emit("section .note.GNU-stack noalloc noexec nowrite progbits");
    emit("");
    emit("section .rodata");
    emit("__fmt_i64   db \"%ld\", 10, 0");
    emit("__fmt_u64   db \"%lu\", 10, 0");
    emit("__fmt_f64   db \"%g\", 10, 0");
    emit("__fmt_str   db \"%s\", 10, 0");
    emit("__str_true  db \"true\", 10, 0");
    emit("__str_false db \"false\", 10, 0");
    emit("__rt_div0   db \"runtime error: division by zero\", 10, 0");
    emit("__rt_oob    db \"runtime error: index out of bounds\", 10, 0");
    emit("__rt_panic  db \"%s\", 10, 0");
    emit("");
}

std::string Gen::add_string(const std::string& s) {
    for (std::size_t i = 0; i < str_literals.size(); ++i) {
        if (str_literals[i] == s) return "__S" + std::to_string(i);
    }
    str_literals.push_back(s);
    return "__S" + std::to_string(str_literals.size() - 1);
}

std::string Gen::add_float(double v) {
    for (std::size_t i = 0; i < flt_literals.size(); ++i) {
        if (flt_literals[i] == v) return "__F" + std::to_string(i);
    }
    flt_literals.push_back(v);
    return "__F" + std::to_string(flt_literals.size() - 1);
}

void Gen::pre_collect_literals() {
    for (const ExprNode& e : prog.exprs) {
        if (std::holds_alternative<StringLitExpr>(e)) {
            add_string(std::get<StringLitExpr>(e).value);
        } else if (std::holds_alternative<FloatLitExpr>(e)) {
            add_float(std::get<FloatLitExpr>(e).value);
        } else if (std::holds_alternative<IntLitExpr>(e)) {
            uint32_t st = std::get<IntLitExpr>(e).sem_type;
            if (st != NO_ID && is_float_type(st)) {
                double v = static_cast<double>(std::get<IntLitExpr>(e).value);
                add_float(v);
            }
        }
    }
}

void Gen::emit_data_section() {
    for (std::size_t i = 0; i < str_literals.size(); ++i) {
        out << "__S" << i << " db ";
        const std::string& s = str_literals[i];
        bool first = true;
        for (unsigned char c : s) {
            if (!first) out << ", ";
            out << static_cast<int>(c);
            first = false;
        }
        if (!first) out << ", ";
        out << "0\n";
    }

    for (std::size_t i = 0; i < flt_literals.size(); ++i) {
        uint64_t bits = std::bit_cast<uint64_t>(flt_literals[i]);
        std::array<char, 17> hex_buf{};
        std::snprintf(hex_buf.data(), hex_buf.size(), "%llx",
                      static_cast<unsigned long long>(bits));
        out << "__F" << i << " dq 0x" << hex_buf.data() << "\n";
    }

    for (auto& [key, methods] : prog.iface_impls) {
        out << "__vtable_" << key.struct_name << "_" << key.iface_name << ":\n";
        auto nit = prog.interface_method_names.find(key.iface_name);
        for (std::size_t i = 0; i < methods.size(); ++i) {
            std::string mname = (nit != prog.interface_method_names.end() &&
                                 i < nit->second.size())
                                ? nit->second[i] : std::to_string(i);
            out << "    dq __thunk_" << key.struct_name << "_"
                << key.iface_name << "_" << mname << "\n";
        }
    }
}


uint32_t Gen::emit_expr(ExprId eid) {
    const ExprNode& e = prog.expr(eid);
    uint32_t t = expr_sem_type(e);

    if (std::holds_alternative<IntLitExpr>(e)) {
        if (is_float_type(t)) {
            double v = static_cast<double>(std::get<IntLitExpr>(e).value);
            std::string lbl = add_float(v);
            emit_ins("movsd xmm0, [rel " + lbl + "]");
        } else {
            emit_ins("mov rax, " + std::to_string(std::get<IntLitExpr>(e).value));
        }
        return t;
    }

    if (std::holds_alternative<FloatLitExpr>(e)) {
        std::string lbl = add_float(std::get<FloatLitExpr>(e).value);
        emit_ins("movsd xmm0, [rel " + lbl + "]");
        return t;
    }

    if (std::holds_alternative<StringLitExpr>(e)) {
        std::string lbl = add_string(std::get<StringLitExpr>(e).value);
        emit_ins("lea rax, [rel " + lbl + "]");
        return t;
    }

    if (std::holds_alternative<BoolLitExpr>(e)) {
        bool val = std::get<BoolLitExpr>(e).value;
        emit_ins(std::string("mov rax, ") + (val ? "1" : "0"));
        return t;
    }

    if (std::holds_alternative<IdentExpr>(e)) {
        const std::string& name = std::get<IdentExpr>(e).name;
        std::string addr = var_addr(name);

        if (is_float_type(t)) {
            emit_ins("movsd xmm0, " + addr);
        } else if (t != NO_ID && prog.semtypes.get(t).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(t).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                emit_ins("lea rcx, " + addr);
                if (!sd.fields.empty()) {
                    bool f0_flt = sd.fields[0].sem_type != NO_ID && is_float_type(sd.fields[0].sem_type);
                    if (f0_flt) {
                        emit_ins("movsd xmm0, [rcx]");
                    } else {
                        emit_ins("mov rax, [rcx]");
                    }
                }
                if (sd.fields.size() > 1) {
                    bool f1_flt = sd.fields[1].sem_type != NO_ID && is_float_type(sd.fields[1].sem_type);
                    if (f1_flt) {
                        emit_ins("movsd xmm1, [rcx + 8]");
                    } else {
                        emit_ins("mov rdx, [rcx + 8]");
                    }
                }
            } else {
                emit_ins("mov rax, " + addr);
            }
        } else {
            emit_ins("mov rax, " + addr);
        }
        return t;
    }

    if (std::holds_alternative<BinaryExpr>(e))   return emit_binary(std::get<BinaryExpr>(e));
    if (std::holds_alternative<UnaryExpr>(e))    return emit_unary(std::get<UnaryExpr>(e));
    if (std::holds_alternative<CastExpr>(e))     return emit_cast(std::get<CastExpr>(e));
    if (std::holds_alternative<IndexExpr>(e))    return emit_index(std::get<IndexExpr>(e));
    if (std::holds_alternative<FieldExpr>(e))    return emit_field(std::get<FieldExpr>(e));
    if (std::holds_alternative<CallExpr>(e))     return emit_call(std::get<CallExpr>(e));
    if (std::holds_alternative<AssignExpr>(e))   return emit_assign(std::get<AssignExpr>(e));

    if (std::holds_alternative<ArrayLitExpr>(e)) {
        emit_comment("array literal (stored via emit_var_decl)");
        return t;
    }

    if (std::holds_alternative<StructLitExpr>(e)) {
        const StructLitExpr& sl = std::get<StructLitExpr>(e);
        auto it = struct_defs.find(sl.name);
        if (it == struct_defs.end()) {
            emit_ins("xor rax, rax");
            return t;
        }

        const StructDecl& sd = std::get<StructDecl>(prog.decl(it->second));
        std::map<std::string, ExprId> fmap;
        for (auto& [fn, feid] : sl.fields) {
            fmap[fn] = feid;
        }

        if (sd.fields.size() == 1) {
            bool f0_flt = sd.fields[0].sem_type != NO_ID && is_float_type(sd.fields[0].sem_type);
            auto feit = fmap.find(sd.fields[0].name);
            if (feit != fmap.end()) {
                emit_expr(feit->second);
            } else {
                if (f0_flt) {
                    emit_ins("xorpd xmm0, xmm0");
                } else {
                    emit_ins("xor rax, rax");
                }
            }
        } else if (sd.fields.size() >= 2) {
            bool f1_flt = sd.fields[1].sem_type != NO_ID && is_float_type(sd.fields[1].sem_type);
            bool f0_flt = sd.fields[0].sem_type != NO_ID && is_float_type(sd.fields[0].sem_type);

            auto it1 = fmap.find(sd.fields[1].name);
            if (it1 != fmap.end()) {
                emit_expr(it1->second);
            } else {
                if (f1_flt) {
                    emit_ins("xorpd xmm0, xmm0");
                } else {
                    emit_ins("xor rax, rax");
                }
            }

            if (f1_flt) {
                emit_ins("sub rsp, 8");
                emit_ins("movsd [rsp], xmm0");
            } else {
                emit_ins("push rax");
            }

            auto it0 = fmap.find(sd.fields[0].name);
            if (it0 != fmap.end()) {
                emit_expr(it0->second);
            } else {
                if (f0_flt) {
                    emit_ins("xorpd xmm0, xmm0");
                } else {
                    emit_ins("xor rax, rax");
                }
            }

            if (f1_flt) {
                emit_ins("movsd xmm1, [rsp]");
                emit_ins("add rsp, 8");
            } else {
                emit_ins("pop rdx");
            }
        }
        return t;
    }

    emit_comment("unhandled expr kind");
    emit_ins("xor rax, rax");
    return t;
}


void Gen::emit_epilogue() {
    for (auto it = fn_saved_regs.rbegin(); it != fn_saved_regs.rend(); ++it) {
        std::size_t off = reg_save_slots.at(*it);
        emit_ins("mov " + *it + ", [rbp - " + std::to_string(off) + "]");
    }
    emit_ins("leave");
    emit_ins("ret");
}


uint32_t Gen::emit_operator_call(ExprId lhs, ExprId rhs,
                                  const std::string& fn_name, uint32_t ret_t) {
    static const std::array<std::string, 6> int_regs{
        "rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const std::array<std::string, 8> flt_regs{
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"};

    struct Slot { uint32_t sem_t; bool is_flt; };
    std::vector<Slot> slots;

    auto expand = [&](ExprId eid) {
        uint32_t at = expr_sem_type(prog.expr(eid));
        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(at).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                for (auto& f : sd.fields) {
                    bool ff = f.sem_type != NO_ID && is_float_type(f.sem_type);
                    slots.push_back({f.sem_type, ff});
                }
                return;
            }
        }
        slots.push_back({at, is_float_type(at)});
    };

    expand(lhs);
    expand(rhs);

    emit_ins("sub rsp, " + std::to_string((slots.size() + 1) * 8));
    std::size_t save_idx = 0;

    auto save_one = [&](ExprId eid) {
        uint32_t at = expr_sem_type(prog.expr(eid));
        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(at).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                if (std::holds_alternative<IdentExpr>(prog.expr(eid)))
                    emit_ins("lea rcx, " +
                             var_addr(std::get<IdentExpr>(prog.expr(eid)).name));
                for (auto& f : sd.fields) {
                    bool ff = f.sem_type != NO_ID && is_float_type(f.sem_type);
                    std::string src = "[rcx + " + std::to_string(f.field_offset) + "]";
                    std::size_t off = save_idx++ * 8;
                    if (ff) {
                        emit_ins("movsd xmm0, " + src);
                        emit_ins("movsd [rsp + " + std::to_string(off) + "], xmm0");
                    } else {
                        emit_ins("mov rax, " + src);
                        emit_ins("mov [rsp + " + std::to_string(off) + "], rax");
                    }
                }
                return;
            }
        }
        emit_expr(eid);
        std::size_t off = save_idx++ * 8;
        if (is_float_type(at))
            emit_ins("movsd [rsp + " + std::to_string(off) + "], xmm0");
        else
            emit_ins("mov [rsp + " + std::to_string(off) + "], rax");
    };

    save_one(lhs);
    save_one(rhs);

    save_idx = 0;
    int ii = 0, fi = 0;
    for (auto& slot : slots) {
        std::size_t off = save_idx++ * 8;
        if (slot.is_flt) {
            if (fi < 8)
                emit_ins("movsd " + std::string(flt_regs[fi++]) +
                         ", [rsp + " + std::to_string(off) + "]");
        } else {
            if (ii < 6)
                emit_ins("mov " + std::string(int_regs[ii++]) +
                         ", [rsp + " + std::to_string(off) + "]");
        }
    }

    emit_ins("and rsp, -16");
    emit_ins("mov eax, " + std::to_string(fi));
    emit_ins("call " + fn_name);
    emit_ins("mov rsp, rbp");
    emit_ins("sub rsp, " + std::to_string(aligned_frame_bytes));
    return ret_t;
}


uint32_t Gen::emit_binary(const BinaryExpr& e) {
    if (!e.resolved_fn.empty())
        return emit_operator_call(e.left, e.right, e.resolved_fn, e.sem_type);

    uint32_t lt = expr_sem_type(prog.expr(e.left));
    uint32_t rt = expr_sem_type(prog.expr(e.right));
    bool lt_flt = is_float_type(lt);
    bool rt_flt = is_float_type(rt);
    bool is_flt = lt_flt || rt_flt;

    if (is_flt) {
        emit_expr(e.left);
        if (!lt_flt) emit_ins("cvtsi2sd xmm0, rax"); 
        emit_ins("sub rsp, 8");
        emit_ins("movsd [rsp], xmm0");
        emit_expr(e.right);
        if (!rt_flt) emit_ins("cvtsi2sd xmm0, rax"); 
        emit_ins("movsd xmm1, xmm0");
        emit_ins("movsd xmm0, [rsp]");
        emit_ins("add rsp, 8");
    } else {
        emit_expr(e.left);
        emit_ins("push rax");
        emit_expr(e.right);
        emit_ins("mov rbx, rax");
        emit_ins("pop rax");
    }

    uint32_t result_t = e.sem_type;
    const std::string& op = e.op;

    if (op == "&&" || op == "||") {
        emit_ins(op == "&&" ? "and rax, rbx" : "or  rax, rbx");
        emit_ins("and rax, 1");
        return result_t;
    }

    bool is_cmp = op == "==" || op == "!=" || op == "<" ||
                  op == ">"  || op == "<=" || op == ">=";
    if (is_cmp) {
        if (is_flt) {
            emit_ins("ucomisd xmm0, xmm1");
        } else {
            emit_ins("cmp rax, rbx");
        }

        bool lt_unsigned = lt != NO_ID && prog.semtypes.get(lt).is_unsigned();
        std::string setcc;
        if      (op == "==") setcc = "sete";
        else if (op == "!=") setcc = "setne";
        else if (op == "<")  setcc = is_flt ? "setb"  : (lt_unsigned ? "setb"  : "setl");
        else if (op == ">")  setcc = is_flt ? "seta"  : (lt_unsigned ? "seta"  : "setg");
        else if (op == "<=") setcc = is_flt ? "setbe" : (lt_unsigned ? "setbe" : "setle");
        else                 setcc = is_flt ? "setae" : (lt_unsigned ? "setae" : "setge");
        emit_ins(setcc + " al");
        emit_ins("movzx rax, al");
        return result_t;
    }

    if (is_flt) {
        if      (op == "+") emit_ins("addsd xmm0, xmm1");
        else if (op == "-") emit_ins("subsd xmm0, xmm1");
        else if (op == "*") emit_ins("mulsd xmm0, xmm1");
        else if (op == "/") emit_ins("divsd xmm0, xmm1");
    } else {
        if      (op == "+") emit_ins("add rax, rbx");
        else if (op == "-") emit_ins("sub rax, rbx");
        else if (op == "*") emit_ins("imul rax, rbx");
        else if (op == "/" || op == "%") {
            std::string ok = new_label(".Lok");
            emit_ins("test rbx, rbx");
            emit_ins("jnz " + ok);
            emit_ins("lea rdi, [rel __rt_div0]");
            emit_ins("xor eax, eax");
            emit_ins("call printf");
            emit_ins("mov edi, 1");
            emit_ins("call exit");
            emit_label(ok);
            bool is_signed = lt != NO_ID && prog.semtypes.get(lt).is_signed_int();
            if (is_signed) {
                emit_ins("cqo");
                emit_ins("idiv rbx");
            } else {
                emit_ins("xor edx, edx");
                emit_ins("div rbx");
            }
            if (op == "%") emit_ins("mov rax, rdx");
        }
    }
    return result_t;
}

uint32_t Gen::emit_unary(const UnaryExpr& e) {
    uint32_t t = e.sem_type;
    emit_expr(e.operand);

    if (e.op == "-") {
        if (is_float_type(t)) {
            emit_ins("xorpd xmm1, xmm1");
            emit_ins("subsd xmm1, xmm0");
            emit_ins("movsd xmm0, xmm1");
        } else {
            emit_ins("neg rax");
        }
    } else if (e.op == "!") {
        emit_ins("test rax, rax");
        emit_ins("sete al");
        emit_ins("movzx rax, al");
    }
    return t;
}

uint32_t Gen::emit_cast(const CastExpr& e) {
    uint32_t src_t = expr_sem_type(prog.expr(e.operand));
    uint32_t dst_t = e.sem_type;
    emit_expr(e.operand);

    if (src_t == NO_ID || dst_t == NO_ID) return dst_t;

    bool src_flt = is_float_type(src_t);
    bool dst_flt = is_float_type(dst_t);

    if (!src_flt && dst_flt) {
        emit_ins("cvtsi2sd xmm0, rax");
    } else if (src_flt && !dst_flt) {
        emit_ins("cvttsd2si rax, xmm0");
    } else if (!src_flt && !dst_flt) {
        switch (prog.semtypes.get(dst_t).kind) {
            case SemKind::I8:  emit_ins("movsx rax, al");   break;
            case SemKind::I16: emit_ins("movsx rax, ax");   break;
            case SemKind::I32: emit_ins("movsxd rax, eax"); break;
            case SemKind::U8:  emit_ins("movzx rax, al");   break;
            case SemKind::U16: emit_ins("movzx rax, ax");   break;
            case SemKind::U32: emit_ins("mov eax, eax");    break;
            default: break;
        }
    }
    return dst_t;
}


uint32_t Gen::emit_index(const IndexExpr& e) {
    uint32_t arr_t = expr_sem_type(prog.expr(e.array));
    uint32_t elem_t = e.sem_type;

    if (std::holds_alternative<IdentExpr>(prog.expr(e.array))) {
        const std::string& aname = std::get<IdentExpr>(prog.expr(e.array)).name;
        if (array_params.count(aname)) {
            emit_ins("mov rcx, " + var_addr(aname));
        } else {
            emit_ins("lea rcx, " + var_addr(aname));
        }
    } else {
        emit_expr(e.array);
        emit_ins("mov rcx, rax");
    }
    emit_ins("push rcx");

    emit_expr(e.index);
    emit_ins("mov rbx, rax");

    std::size_t arr_size = (arr_t != NO_ID) ? prog.semtypes.get(arr_t).array_size : 0;
    std::string ok = new_label(".Lok");
    std::string oob = ".Loob_" + std::to_string(label_cnt - 1);
    emit_ins("test rbx, rbx");
    emit_ins("jl " + oob);
    emit_ins("cmp rbx, " + std::to_string(arr_size));
    emit_ins("jl " + ok);
    emit_label(oob);
    emit_ins("lea rdi, [rel __rt_oob]");
    emit_ins("xor eax, eax");
    emit_ins("call printf");
    emit_ins("mov edi, 1");
    emit_ins("call exit");
    emit_label(ok);

    emit_ins("pop rcx");
    if (is_float_type(elem_t)) {
        emit_ins("movsd xmm0, [rcx + rbx * 8]");
    } else {
        emit_ins("mov rax, [rcx + rbx * 8]");
    }
    return elem_t;
}

uint32_t Gen::emit_field(const FieldExpr& e) {
    uint32_t obj_t = expr_sem_type(prog.expr(e.object));
    uint32_t res_t = e.sem_type;

    if (std::holds_alternative<IdentExpr>(prog.expr(e.object))) {
        emit_ins("lea rcx, " + var_addr(std::get<IdentExpr>(prog.expr(e.object)).name));
    } else {
        emit_expr(e.object);
        emit_ins("mov rcx, rax");
    }

    std::size_t offset = 0;
    if (obj_t != NO_ID && prog.semtypes.get(obj_t).kind == SemKind::Struct) {
        const std::string& sname = prog.semtypes.get(obj_t).struct_name;
        auto it = struct_defs.find(sname);
        if (it != struct_defs.end()) {
            const StructDecl& sd = std::get<StructDecl>(prog.decl(it->second));
            for (auto& f : sd.fields) {
                if (f.name == e.field) break;
                offset += 8;
            }
        }
    }

    std::string addr = "[rcx + " + std::to_string(offset) + "]";
    if (is_float_type(res_t)) {
        emit_ins("movsd xmm0, " + addr);
    } else {
        emit_ins("mov rax, " + addr);
    }
    return res_t;
}

uint32_t Gen::emit_assign(const AssignExpr& e) {
    uint32_t vt = e.sem_type;
    uint32_t val_t = expr_sem_type(prog.expr(e.value));
    bool val_flt = is_float_type(val_t);
    bool is_flt  = is_float_type(vt) || val_flt; 
    emit_expr(e.value);
    if (is_float_type(vt) && !val_flt) emit_ins("cvtsi2sd xmm0, rax");

    const ExprNode& target = prog.expr(e.target);

    if (std::holds_alternative<IdentExpr>(target)) {
        std::string addr = var_addr(std::get<IdentExpr>(target).name);
        if (is_flt) {
            emit_ins("movsd " + addr + ", xmm0");
        } else {
            emit_ins("mov " + addr + ", rax");
        }

    } else if (std::holds_alternative<IndexExpr>(target)) {
        const IndexExpr& ie = std::get<IndexExpr>(target);
        if (is_flt) {
            emit_ins("sub rsp, 8");
            emit_ins("movsd [rsp], xmm0");
        } else {
            emit_ins("push rax");
        }

        if (std::holds_alternative<IdentExpr>(prog.expr(ie.array))) {
            emit_ins("lea rcx, " +
                     var_addr(std::get<IdentExpr>(prog.expr(ie.array)).name));
            emit_expr(ie.index);
            emit_ins("mov rbx, rax");
            if (is_flt) {
                emit_ins("movsd xmm0, [rsp]");
                emit_ins("add rsp, 8");
                emit_ins("movsd [rcx + rbx * 8], xmm0");
            } else {
                emit_ins("pop rax");
                emit_ins("mov [rcx + rbx * 8], rax");
            }
        } else {
            if (is_flt) emit_ins("add rsp, 8");
            else        emit_ins("pop rax");
        }

    } else if (std::holds_alternative<FieldExpr>(target)) {
        const FieldExpr& fe = std::get<FieldExpr>(target);

        if (is_flt) {
            emit_ins("sub rsp, 8");
            emit_ins("movsd [rsp], xmm0");
        } else {
            emit_ins("push rax");
        }

        if (std::holds_alternative<IdentExpr>(prog.expr(fe.object))) {
            emit_ins("lea rcx, " + var_addr(std::get<IdentExpr>(prog.expr(fe.object)).name));
        }

        std::size_t offset = 0;
        uint32_t obj_t = expr_sem_type(prog.expr(fe.object));
        if (obj_t != NO_ID && prog.semtypes.get(obj_t).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(obj_t).struct_name;
            auto it = struct_defs.find(sname);
            if (it != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(it->second));
                for (auto& f : sd.fields) {
                    if (f.name == fe.field) break;
                    offset += 8;
                }
            }
        }

        if (is_flt) {
            emit_ins("movsd xmm0, [rsp]");
            emit_ins("add rsp, 8");
            emit_ins("movsd [rcx + " + std::to_string(offset) + "], xmm0");
        } else {
            emit_ins("pop rax");
            emit_ins("mov [rcx + " + std::to_string(offset) + "], rax");
        }
    }
    return vt;
}


uint32_t Gen::emit_call(const CallExpr& e) {
    uint32_t ret_t = e.sem_type;

    if (!e.resolved_fn.empty()) {
        if (e.resolved_fn == "__builtin_print")      return emit_builtin_print(e);
        if (e.resolved_fn == "__builtin_input")      return emit_builtin_input(e);
        if (e.resolved_fn == "__builtin_exit")       return emit_builtin_exit(e);
        if (e.resolved_fn == "__builtin_panic")      return emit_builtin_panic(e);
        if (e.resolved_fn == "__builtin_print_char") return emit_builtin_print_char(e);
    }

    static const std::array<std::string, 6> int_regs{"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const std::array<std::string, 8> flt_regs{"xmm0", "xmm1", "xmm2", "xmm3",
                                                      "xmm4", "xmm5", "xmm6", "xmm7"};

    bool is_method = std::holds_alternative<FieldExpr>(prog.expr(e.callee));

    struct Slot { uint32_t sem_t; bool is_flt; };
    std::vector<Slot> slots;

    auto expand = [&](ExprId eid) {
        uint32_t at = expr_sem_type(prog.expr(eid));
        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Array) {
            slots.push_back({at, false});
            return;
        }
        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(at).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                for (auto& f : sd.fields) {
                    bool ff = f.sem_type != NO_ID &&
                              (prog.semtypes.get(f.sem_type).kind == SemKind::F32 ||
                               prog.semtypes.get(f.sem_type).kind == SemKind::F64);
                    slots.push_back({f.sem_type, ff});
                }
                return;
            }
        }
        slots.push_back({at, is_float_type(at)});
    };

    if (is_method) {
        const FieldExpr& fe = std::get<FieldExpr>(prog.expr(e.callee));
        expand(fe.object);
    }
    for (ExprId arg : e.args) expand(arg);

    emit_ins("sub rsp, " + std::to_string((slots.size() + 1) * 8));
    std::size_t save_idx = 0;

    auto emit_save = [&](ExprId eid) {
        const ExprNode& en = prog.expr(eid);
        uint32_t at = expr_sem_type(en);

        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Array) {
            if (std::holds_alternative<IdentExpr>(en)) {
                const std::string& aname = std::get<IdentExpr>(en).name;
                if (array_params.count(aname)) {
                    emit_ins("mov rax, " + var_addr(aname));
                } else {
                    emit_ins("lea rax, " + var_addr(aname));
                }
            }
            std::size_t off = save_idx++ * 8;
            emit_ins("mov [rsp + " + std::to_string(off) + "], rax");
            return;
        }

        if (at != NO_ID && prog.semtypes.get(at).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(at).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                if (std::holds_alternative<IdentExpr>(en)) {
                    emit_ins("lea rcx, " + var_addr(std::get<IdentExpr>(en).name));
                }
                for (auto& f : sd.fields) {
                    bool ff = f.sem_type != NO_ID &&
                              (prog.semtypes.get(f.sem_type).kind == SemKind::F32 ||
                               prog.semtypes.get(f.sem_type).kind == SemKind::F64);
                    std::string src = "[rcx + " + std::to_string(f.field_offset) + "]";
                    std::size_t off = save_idx++ * 8;
                    if (ff) {
                        emit_ins("movsd xmm0, " + src);
                        emit_ins("movsd [rsp + " + std::to_string(off) + "], xmm0");
                    } else {
                        emit_ins("mov rax, " + src);
                        emit_ins("mov [rsp + " + std::to_string(off) + "], rax");
                    }
                }
                return;
            }
        }

        emit_expr(eid);
        std::size_t off = save_idx++ * 8;
        if (is_float_type(at)) {
            emit_ins("movsd [rsp + " + std::to_string(off) + "], xmm0");
        } else {
            emit_ins("mov [rsp + " + std::to_string(off) + "], rax");
        }
    };

    if (is_method) {
        const FieldExpr& fe = std::get<FieldExpr>(prog.expr(e.callee));
        emit_save(fe.object);
    }
    for (ExprId arg : e.args) emit_save(arg);

    save_idx = 0;
    int int_reg_idx = 0;
    int flt_reg_idx = 0;
    for (auto& slot : slots) {
        std::size_t off = save_idx++ * 8;
        if (slot.is_flt) {
            if (flt_reg_idx < 8) {
                emit_ins("movsd " + flt_regs[flt_reg_idx++] +
                         ", [rsp + " + std::to_string(off) + "]");
            }
        } else {
            if (int_reg_idx < 6) {
                emit_ins("mov " + int_regs[int_reg_idx++] +
                         ", [rsp + " + std::to_string(off) + "]");
            }
        }
    }

    emit_ins("and rsp, -16");
    if (e.resolved_fn.rfind("__vtable_call__", 0) == 0) {
        return emit_vtable_call(e);
    }
    emit_ins("mov eax, " + std::to_string(flt_reg_idx));
    emit_ins("call " + e.resolved_fn);
    emit_ins("mov rsp, rbp");
    emit_ins("sub rsp, " + std::to_string(aligned_frame_bytes));

    return ret_t;
}

uint32_t Gen::emit_vtable_call(const CallExpr& e) {
    const FieldExpr& fe = std::get<FieldExpr>(prog.expr(e.callee));
    uint32_t obj_t = expr_sem_type(prog.expr(fe.object));
    const std::string& iname = prog.semtypes.get(obj_t).struct_name;
    const std::string& method_name = fe.field;

    int vtable_idx = -1;
    auto nit = prog.interface_method_names.find(iname);
    if (nit != prog.interface_method_names.end()) {
        for (int i = 0; i < static_cast<int>(nit->second.size()); ++i) {
            if (nit->second[i] == method_name) { vtable_idx = i; break; }
        }
    }

    int64_t iface_off = 0;
    const ExprNode& obj_en = prog.expr(fe.object);
    if (std::holds_alternative<IdentExpr>(obj_en)) {
        const std::string& vname = std::get<IdentExpr>(obj_en).name;
        auto lit = locals.find(vname);
        if (lit != locals.end()) iface_off = lit->second;
    }

    int64_t vtable_ptr_off = iface_off + 8;
    if (vtable_ptr_off < 0)
        emit_ins("mov r10, [rbp - " + std::to_string(-vtable_ptr_off) + "]");
    else
        emit_ins("mov r10, [rbp + " + std::to_string(vtable_ptr_off) + "]");

    if (vtable_idx >= 0)
        emit_ins("call [r10 + " + std::to_string(vtable_idx * 8) + "]");

    emit_ins("mov rsp, rbp");
    emit_ins("sub rsp, " + std::to_string(aligned_frame_bytes));
    return e.sem_type;
}

uint32_t Gen::emit_builtin_print(const CallExpr& e) {
    if (e.args.empty()) return e.sem_type;
    uint32_t arg_t = expr_sem_type(prog.expr(e.args[0]));
    emit_expr(e.args[0]);

    if (arg_t != NO_ID && prog.semtypes.get(arg_t).kind == SemKind::Bool) {
        std::string lbl_false = new_label(".Lpf");
        std::string lbl_end = new_label(".Lpe");
        emit_ins("test rax, rax");
        emit_ins("jz " + lbl_false);
        emit_ins("lea rdi, [rel __str_true]");
        emit_ins("jmp " + lbl_end);
        emit_label(lbl_false);
        emit_ins("lea rdi, [rel __str_false]");
        emit_label(lbl_end);
        emit_ins("xor eax, eax");
        emit_ins("call printf");

    } else if (arg_t != NO_ID && is_float_type(arg_t)) {
        emit_ins("lea rdi, [rel __fmt_f64]");
        emit_ins("mov eax, 1");
        emit_ins("call printf");

    } else if (arg_t != NO_ID && prog.semtypes.get(arg_t).kind == SemKind::String) {
        emit_ins("mov rsi, rax");
        emit_ins("lea rdi, [rel __fmt_str]");
        emit_ins("xor eax, eax");
        emit_ins("call printf");

    } else if (arg_t != NO_ID && prog.semtypes.get(arg_t).is_unsigned()) {
        emit_ins("mov rsi, rax");
        emit_ins("lea rdi, [rel __fmt_u64]");
        emit_ins("xor eax, eax");
        emit_ins("call printf");

    } else {
        emit_ins("mov rsi, rax");
        emit_ins("lea rdi, [rel __fmt_i64]");
        emit_ins("xor eax, eax");
        emit_ins("call printf");
    }

    return e.sem_type;
}

uint32_t Gen::emit_builtin_input(const CallExpr& e) {
    emit_ins("and rsp, -16");
    emit_ins("call __ryst_input");
    return e.sem_type;
}

uint32_t Gen::emit_builtin_exit(const CallExpr& e) {
    emit_expr(e.args[0]);
    emit_ins("mov edi, eax");
    emit_ins("and rsp, -16");
    emit_ins("call exit");
    return e.sem_type;
}

uint32_t Gen::emit_builtin_panic(const CallExpr& e) {
    emit_expr(e.args[0]);
    emit_ins("mov rsi, rax");
    emit_ins("lea rdi, [rel __rt_panic]");
    emit_ins("xor eax, eax");
    emit_ins("and rsp, -16");
    emit_ins("call printf");
    emit_ins("mov edi, 1");
    emit_ins("call exit");
    return e.sem_type;
}

uint32_t Gen::emit_builtin_print_char(const CallExpr& e) {
    emit_expr(e.args[0]);
    emit_ins("mov edi, eax");
    emit_ins("and rsp, -16");
    emit_ins("call putchar");
    return e.sem_type;
}


void Gen::emit_var_decl(const VarDeclStmt& vs) {
    locals[vs.name] = vs.frame_offset;

    if (std::holds_alternative<ArrayLitExpr>(prog.expr(vs.init))) {
        const ArrayLitExpr& al = std::get<ArrayLitExpr>(prog.expr(vs.init));
        emit_ins("lea rcx, " + var_addr(vs.name));
        for (std::size_t i = 0; i < al.elements.size(); ++i) {
            uint32_t et = emit_expr(al.elements[i]);
            if (is_float_type(et)) {
                emit_ins("movsd [rcx + " + std::to_string(i * 8) + "], xmm0");
            } else {
                emit_ins("mov [rcx + " + std::to_string(i * 8) + "], rax");
            }
        }

    } else if (std::holds_alternative<StructLitExpr>(prog.expr(vs.init))) {
        const StructLitExpr& sl = std::get<StructLitExpr>(prog.expr(vs.init));
        emit_ins("lea rcx, " + var_addr(vs.name));
        auto it = struct_defs.find(sl.name);
        if (it != struct_defs.end()) {
            const StructDecl& sd = std::get<StructDecl>(prog.decl(it->second));
            for (auto& [fname, fexpr_id] : sl.fields) {
                std::size_t off = 0;
                for (auto& f : sd.fields) {
                    if (f.name == fname) break;
                    off += 8;
                }
                uint32_t et = emit_expr(fexpr_id);
                if (is_float_type(et)) {
                    emit_ins("movsd [rcx + " + std::to_string(off) + "], xmm0");
                } else {
                    emit_ins("mov [rcx + " + std::to_string(off) + "], rax");
                }
            }
        }

    } else if (vs.sem_type != NO_ID &&
               prog.semtypes.get(vs.sem_type).kind == SemKind::Interface) {
        const std::string& iname = prog.semtypes.get(vs.sem_type).struct_name;
        uint32_t init_t = expr_sem_type(prog.expr(vs.init));
        std::string sname;
        if (init_t != NO_ID && prog.semtypes.get(init_t).kind == SemKind::Struct)
            sname = prog.semtypes.get(init_t).struct_name;

        const ExprNode& init_en = prog.expr(vs.init);
        if (std::holds_alternative<IdentExpr>(init_en)) {
            emit_ins("lea rax, " + var_addr(std::get<IdentExpr>(init_en).name));
        }

        int64_t off = vs.frame_offset;
        auto mem_at = [](int64_t o) -> std::string {
            return o < 0 ? "[rbp - " + std::to_string(-o) + "]"
                         : "[rbp + " + std::to_string(o) + "]";
        };
        emit_ins("mov " + mem_at(off) + ", rax");         
        if (!sname.empty()) {
            emit_ins("lea rax, [rel __vtable_" + sname + "_" + iname + "]");
            emit_ins("mov " + mem_at(off + 8) + ", rax"); 
        }

    } else {
        uint32_t et = emit_expr(vs.init);
        std::string addr = var_addr(vs.name);
        if (is_float_type(vs.sem_type) && !is_float_type(et)) {
            emit_ins("cvtsi2sd xmm0, rax");
            et = vs.sem_type;
        }

        if (is_float_type(et)) {
            emit_ins("movsd " + addr + ", xmm0");

        } else if (et != NO_ID && prog.semtypes.get(et).kind == SemKind::Struct) {
            const std::string& sname = prog.semtypes.get(et).struct_name;
            auto sit = struct_defs.find(sname);
            if (sit != struct_defs.end()) {
                const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));
                emit_ins("lea rcx, " + addr);
                if (!sd.fields.empty()) {
                    bool f0_flt = sd.fields[0].sem_type != NO_ID && is_float_type(sd.fields[0].sem_type);
                    if (f0_flt) {
                        emit_ins("movsd [rcx], xmm0");
                    } else {
                        emit_ins("mov [rcx], rax");
                    }
                }
                if (sd.fields.size() > 1) {
                    bool f1_flt = sd.fields[1].sem_type != NO_ID && is_float_type(sd.fields[1].sem_type);
                    if (f1_flt) {
                        emit_ins("movsd [rcx + 8], xmm1");
                    } else {
                        emit_ins("mov [rcx + 8], rdx");
                    }
                }
            } else {
                emit_ins("mov " + addr + ", rax");
            }
        } else {
            emit_ins("mov " + addr + ", rax");
        }
    }
}

void Gen::emit_stmt(StmtId sid) {
    const StmtNode& s = prog.stmt(sid);

    if (std::holds_alternative<VarDeclStmt>(s)) {
        emit_var_decl(std::get<VarDeclStmt>(s));

    } else if (std::holds_alternative<ExprStmt>(s)) {
        emit_expr(std::get<ExprStmt>(s).expr);

    } else if (std::holds_alternative<BlockStmt>(s)) {
        for (StmtId child : std::get<BlockStmt>(s).stmts) {
            emit_stmt(child);
        }

    } else if (std::holds_alternative<IfStmt>(s)) {
        const IfStmt& is = std::get<IfStmt>(s);
        std::string lbl_else = new_label(".Lelse");
        std::string lbl_end = new_label(".Lend");
        emit_expr(is.cond);
        emit_ins("test rax, rax");
        emit_ins("jz " + lbl_else);
        emit_stmt(is.then_block);
        emit_ins("jmp " + lbl_end);
        emit_label(lbl_else);
        if (is.else_stmt) emit_stmt(*is.else_stmt);
        emit_label(lbl_end);

    } else if (std::holds_alternative<WhileStmt>(s)) {
        const WhileStmt& ws = std::get<WhileStmt>(s);
        std::string lbl_top = new_label(".Lwh");
        std::string lbl_end = new_label(".Lwe");
        loop_top_labels.push_back(lbl_top);
        loop_end_labels.push_back(lbl_end);
        emit_label(lbl_top);
        emit_expr(ws.cond);
        emit_ins("test rax, rax");
        emit_ins("jz " + lbl_end);
        emit_stmt(ws.body);
        emit_ins("jmp " + lbl_top);
        emit_label(lbl_end);
        loop_top_labels.pop_back();
        loop_end_labels.pop_back();

    } else if (std::holds_alternative<ReturnStmt>(s)) {
        const ReturnStmt& rs = std::get<ReturnStmt>(s);
        if (rs.value) {
            uint32_t val_t = emit_expr(*rs.value);
            if (cur_fn_ret_type_ != NO_ID && is_float_type(cur_fn_ret_type_) &&
                val_t != NO_ID && !is_float_type(val_t)) {
                emit_ins("cvtsi2sd xmm0, rax");
            }
        }
        emit_epilogue();

    } else if (std::holds_alternative<BreakStmt>(s)) {
        if (!loop_end_labels.empty()) {
            emit_ins("jmp " + loop_end_labels.back());
        }

    } else if (std::holds_alternative<ContinueStmt>(s)) {
        if (!loop_top_labels.empty()) {
            emit_ins("jmp " + loop_top_labels.back());
        }
    }
}


void Gen::emit_fn(const FnDecl& fd) {
    locals.clear();
    array_params.clear();
    regalloc_map.clear();
    reg_save_slots.clear();
    fn_saved_regs.clear();

    RegAllocator ra;
    ra.run(prog, fd);
    regalloc_map = ra.reg_map;
    fn_saved_regs = ra.saved_regs;

    cur_fn_ret_type_ = fd.sem_return; 
    frame_bytes = fd.frame_bytes;
    std::size_t reg_save_bytes = fn_saved_regs.size() * 8;
    std::size_t aligned = (frame_bytes + reg_save_bytes + 15) & ~15ULL;
    aligned_frame_bytes = aligned;

    for (std::size_t k = 0; k < fn_saved_regs.size(); ++k) {
        reg_save_slots[fn_saved_regs[k]] = frame_bytes + (k + 1) * 8;
    }

    emit_label(fd.mangled_name);
    emit_ins("push rbp");
    emit_ins("mov rbp, rsp");
    if (aligned > 0) {
        emit_ins("sub rsp, " + std::to_string(aligned));
    }

    for (std::size_t k = 0; k < fn_saved_regs.size(); ++k) {
        std::size_t off = reg_save_slots[fn_saved_regs[k]];
        emit_ins("mov [rbp - " + std::to_string(off) + "], " + fn_saved_regs[k]);
    }

    static const std::array<std::string, 6> int_regs{"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const std::array<std::string, 8> flt_regs{"xmm0", "xmm1", "xmm2", "xmm3",
                                                      "xmm4", "xmm5", "xmm6", "xmm7"};
    int int_idx = 0;
    int flt_idx = 0;
    std::size_t param_offset = 0;

    for (auto& p : fd.params) {
        std::string struct_name;
        if (p.type != NO_ID) {
            const TypeNode& tn = prog.type(p.type);
            if (std::holds_alternative<NamedTypeNode>(tn)) {
                struct_name = std::get<NamedTypeNode>(tn).name;
            }
        }

        if (!struct_name.empty() && struct_defs.count(struct_name)) {
            const StructDecl& sd = std::get<StructDecl>(prog.decl(struct_defs[struct_name]));
            std::size_t n = sd.fields.size();
            param_offset += n * 8;
            locals[p.name] = -static_cast<int64_t>(param_offset);

            for (std::size_t fi = 0; fi < n; ++fi) {
                const StructField& f = sd.fields[fi];
                bool ff = f.sem_type != NO_ID &&
                          (prog.semtypes.get(f.sem_type).kind == SemKind::F32 ||
                           prog.semtypes.get(f.sem_type).kind == SemKind::F64);
                std::string dest = "[rbp - " + std::to_string(param_offset - fi * 8) + "]";
                if (ff) {
                    if (flt_idx < 8) {
                        emit_ins("movsd " + dest + ", " + flt_regs[flt_idx++]);
                    }
                } else {
                    if (int_idx < 6) {
                        emit_ins("mov " + dest + ", " + int_regs[int_idx++]);
                    }
                }
            }
        } else {
            bool is_flt = false;
            bool is_array = false;

            if (p.type != NO_ID) {
                const TypeNode& tn = prog.type(p.type);
                if (std::holds_alternative<PrimTypeNode>(tn)) {
                    SemKind pk = std::get<PrimTypeNode>(tn).prim;
                    is_flt = (pk == SemKind::F32 || pk == SemKind::F64);
                } else if (std::holds_alternative<ArrayTypeNode>(tn)) {
                    is_array = true;
                }
            }

            param_offset += 8;
            int64_t off = -static_cast<int64_t>(param_offset);
            locals[p.name] = off;

            if (is_array) {
                array_params.insert(p.name);
                if (int_idx < 6) {
                    emit_ins("mov [rbp - " + std::to_string(-off) + "], " + int_regs[int_idx++]);
                }
            } else if (is_flt) {
                if (flt_idx < 8) {
                    emit_ins("movsd [rbp - " + std::to_string(-off) + "], " + flt_regs[flt_idx++]);
                }
            } else {
                if (int_idx < 6) {
                    emit_ins("mov [rbp - " + std::to_string(-off) + "], " + int_regs[int_idx++]);
                }
            }
        }
    }

    emit_stmt(fd.body);

    if (fd.sem_return == NO_ID ||
        prog.semtypes.get(fd.sem_return).kind == SemKind::Void) {
        emit_ins("xor eax, eax");
        emit_epilogue();  
    }
    emit("");
}

void Gen::emit_input_helper() {
    emit("section .bss");
    emit("__input_buf resb 1024");
    emit("");
    emit("section .text");
    emit_label("__ryst_input");
    emit_ins("push rbp");
    emit_ins("mov rbp, rsp");
    emit_ins("mov rax, 0");
    emit_ins("mov rdi, 0");
    emit_ins("lea rsi, [rel __input_buf]");
    emit_ins("mov rdx, 1023");
    emit_ins("syscall");
    emit_ins("test rax, rax");
    emit_ins("jle .input_done");
    emit_ins("lea rcx, [rel __input_buf]");
    emit_ins("dec rax");
    emit_ins("cmp byte [rcx + rax], 10");
    emit_ins("jne .input_done");
    emit_ins("mov byte [rcx + rax], 0");
    emit_label(".input_done");
    emit_ins("lea rax, [rel __input_buf]");
    emit_ins("pop rbp");
    emit_ins("ret");
    emit("");
}


void Gen::emit_vtable_thunks() {
    static const std::array<std::string, 6> int_regs{"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

    for (auto& [key, methods] : prog.iface_impls) {
        auto sit = struct_defs.find(key.struct_name);
        if (sit == struct_defs.end()) continue;
        const StructDecl& sd = std::get<StructDecl>(prog.decl(sit->second));

        auto nit = prog.interface_method_names.find(key.iface_name);

        for (std::size_t i = 0; i < methods.size(); ++i) {
            const std::string& mangled = methods[i];
            std::string mname = (nit != prog.interface_method_names.end() &&
                                 i < nit->second.size())
                                ? nit->second[i] : std::to_string(i);
            emit_label("__thunk_" + key.struct_name + "_" + key.iface_name + "_" + mname);

            if (!sd.fields.empty()) {
                emit_ins("mov r11, rdi");
                int int_idx = 0;
                for (std::size_t fi = 0; fi < sd.fields.size() && fi < 6; ++fi) {
                    std::string src = "[r11 + " + std::to_string(fi * 8) + "]";
                    if (sd.fields[fi].sem_type != NO_ID &&
                        is_float_type(sd.fields[fi].sem_type)) {
                        emit_ins("movsd xmm" + std::to_string(fi) + ", " + src);
                    } else {
                        emit_ins("mov " + int_regs[int_idx++] + ", " + src);
                    }
                }
            }

            emit_ins("jmp " + mangled);
            emit("");
        }
    }
}

void Gen::emit_top_decl(DeclId did) {
    const DeclNode& d = prog.decl(did);

    if (std::holds_alternative<FnDecl>(d)) {
        emit_fn(std::get<FnDecl>(d));

    } else if (std::holds_alternative<NamespaceDecl>(d)) {
        for (DeclId child : std::get<NamespaceDecl>(d).decls) {
            emit_top_decl(child);
        }

    } else if (std::holds_alternative<ImplDecl>(d)) {
        for (DeclId m : std::get<ImplDecl>(d).methods) {
            emit_fn(std::get<FnDecl>(prog.decl(m)));
        }
    } else if (std::holds_alternative<InterfaceDecl>(d)) {
        // A.2.13: interface declarations produce no code
    }
}

void Gen::generate() {
    for (DeclId did : prog.top_decls) {
        const DeclNode& d = prog.decl(did);
        if (std::holds_alternative<StructDecl>(d)) {
            struct_defs[std::get<StructDecl>(d).name] = did;
        }
        if (std::holds_alternative<NamespaceDecl>(d)) {
            for (DeclId child : std::get<NamespaceDecl>(d).decls) {
                const DeclNode& cd = prog.decl(child);
                if (std::holds_alternative<StructDecl>(cd)) {
                    struct_defs[std::get<StructDecl>(cd).name] = child;
                }
            }
        }
    }

    pre_collect_literals();
    emit_preamble();
    emit_data_section(); 
    emit("");
    emit("section .text");
    emit_input_helper();
    emit_vtable_thunks(); 
    for (DeclId did : prog.top_decls) {
        emit_top_decl(did);
    }
}


CodegenResult generate(const Program& prog, std::ostream& out,
                       const std::string& filename) {
    try {
        Gen g{out, filename, prog};
        g.generate();
        return {true};
    } catch (const std::exception& ex) {
        return {false, {ex.what()}};
    }
}

} // namespace Codegen
