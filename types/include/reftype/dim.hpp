#ifndef REFTYPE_DIM_HPP
#define REFTYPE_DIM_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/check.hpp>
#include <reftype/strip.hpp>
#include <reftype/types.hpp>

namespace reftype::dim {

using refmacro::ASTNode;
using refmacro::copy_str;
using refmacro::Expression;
using refmacro::str_eq;

// ===================================================================
// Type constructor: tdim(L, T, M)
// ===================================================================

// Dimensional type with SI exponents: length (L), time (T), mass (M).
// Stored as AST node "tdim" with 3 literal children.
template <std::size_t Cap = 128>
consteval Expression<Cap> tdim(int l, int t, int m) {
    Expression<Cap> result;
    ASTNode ln{};
    copy_str(ln.tag, "lit");
    ln.payload = l;
    ASTNode tn{};
    copy_str(tn.tag, "lit");
    tn.payload = t;
    ASTNode mn{};
    copy_str(mn.tag, "lit");
    mn.payload = m;
    int li = result.ast.add_node(ln);
    int ti = result.ast.add_node(tn);
    int mi = result.ast.add_node(mn);
    result.id = result.ast.add_tagged_node("tdim", {li, ti, mi});
    return result;
}

// ===================================================================
// Accessors
// ===================================================================

template <std::size_t Cap> consteval bool is_dim(const Expression<Cap>& e) {
    return str_eq(type_tag(e), "tdim");
}

// Check whether a type has dimensional information, either directly
// (tdim) or as a refinement of a dimensional type (tref(tdim, pred)).
template <std::size_t Cap>
consteval bool is_dim_or_refined_dim(const Expression<Cap>& e) {
    if (is_dim(e))
        return true;
    if (is_refined(e))
        return is_dim(get_refined_base(e));
    return false;
}

// Extract the i-th exponent (0=L, 1=T, 2=M) from a tdim node.
template <std::size_t Cap>
consteval int dim_exp(const Expression<Cap>& e, int idx) {
    return static_cast<int>(
        e.ast.nodes[e.ast.nodes[e.id].children[idx]].payload);
}

// Extract the raw tdim(...) from either tdim or tref(tdim, pred).
template <std::size_t Cap>
consteval Expression<Cap> get_dim_base(const Expression<Cap>& e) {
    if (is_dim(e))
        return e;
    if (is_refined(e) && is_dim(get_refined_base(e)))
        return get_refined_base(e);
    throw "get_dim_base: not a dimensional type";
}

// ===================================================================
// Promotion: non-dimensional numeric -> dim(0,0,0)
// ===================================================================

template <std::size_t Cap> consteval bool has_dim(const Expression<Cap>& type) {
    return is_dim_or_refined_dim(type);
}

template <std::size_t Cap>
consteval Expression<Cap> promote_to_dim(const Expression<Cap>& type) {
    if (is_dim_or_refined_dim(type))
        return get_dim_base(type);
    auto bk = get_base_kind(type);
    if (bk == BaseKind::Int || bk == BaseKind::Real)
        return tdim<Cap>(0, 0, 0);
    throw "promote_to_dim: non-numeric type";
}

// ===================================================================
// Dimensional type rules
// ===================================================================

namespace detail {

// Format a dimension triple for error messages.
template <std::size_t Cap>
consteval refmacro::PrintBuffer<64> dim_str(const Expression<Cap>& d) {
    refmacro::PrintBuffer<64> buf{};
    buf.append("dim(");
    auto append_int = [&](int v) {
        if (v < 0) {
            buf.append_char('-');
            v = -v;
        }
        if (v >= 10)
            buf.append_char('0' + (v / 10));
        buf.append_char('0' + (v % 10));
    };
    append_int(dim_exp(d, 0));
    buf.append_char(',');
    append_int(dim_exp(d, 1));
    buf.append_char(',');
    append_int(dim_exp(d, 2));
    buf.append_char(')');
    return buf;
}

// Check operand is numeric (Int, Real, or Dim).
template <std::size_t Cap>
consteval void validate_dim_numeric(const Expression<Cap>& type,
                                    const char* op_name) {
    if (is_dim_or_refined_dim(type))
        return;
    auto bk = get_base_kind(type);
    if (bk != BaseKind::Int && bk != BaseKind::Real)
        report_error("non-numeric operand in dimensional arithmetic",
                     "Int, Real, or Dim", kind_name(bk), op_name);
}

// Shared implementation for add/sub (require matching dimensions).
template <bool IsSub> consteval auto dim_additive_rule() {
    constexpr const char* op = IsSub ? "sub" : "add";
    return def_typerule(op, [](const auto& expr, const auto& env,
                               auto synth_rec) {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto left = synth_rec(Expr{expr.ast, node.children[0]}, env);
        auto right = synth_rec(Expr{expr.ast, node.children[1]}, env);

        if (has_dim(left.type) || has_dim(right.type)) {
            validate_dim_numeric<Cap>(left.type, IsSub ? "sub" : "add");
            validate_dim_numeric<Cap>(right.type, IsSub ? "sub" : "add");
            auto ld = promote_to_dim(left.type);
            auto rd = promote_to_dim(right.type);
            if (dim_exp(ld, 0) != dim_exp(rd, 0) ||
                dim_exp(ld, 1) != dim_exp(rd, 1) ||
                dim_exp(ld, 2) != dim_exp(rd, 2)) {
                report_error<512>("dimension mismatch in arithmetic",
                                  dim_str(ld).data, dim_str(rd).data,
                                  IsSub ? "sub" : "add");
            }
            return TypeResult<Cap>{ld, left.valid && right.valid};
        }
        return reftype::detail::check_binary_numeric(expr, env, synth_rec,
                                                     IsSub ? "sub" : "add");
    });
}

} // namespace detail

inline constexpr auto TRDimAdd = detail::dim_additive_rule<false>();
inline constexpr auto TRDimSub = detail::dim_additive_rule<true>();

inline constexpr auto TRDimMul =
    def_typerule("mul", [](const auto& expr, const auto& env, auto synth_rec) {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto left = synth_rec(Expr{expr.ast, node.children[0]}, env);
        auto right = synth_rec(Expr{expr.ast, node.children[1]}, env);

        if (has_dim(left.type) || has_dim(right.type)) {
            detail::validate_dim_numeric<Cap>(left.type, "mul");
            detail::validate_dim_numeric<Cap>(right.type, "mul");
            auto ld = promote_to_dim(left.type);
            auto rd = promote_to_dim(right.type);
            auto result = tdim<Cap>(dim_exp(ld, 0) + dim_exp(rd, 0),
                                    dim_exp(ld, 1) + dim_exp(rd, 1),
                                    dim_exp(ld, 2) + dim_exp(rd, 2));
            return TypeResult<Cap>{result, left.valid && right.valid};
        }
        return reftype::detail::check_binary_numeric(expr, env, synth_rec,
                                                     "mul");
    });

inline constexpr auto TRDimDiv =
    def_typerule("div", [](const auto& expr, const auto& env, auto synth_rec) {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto left = synth_rec(Expr{expr.ast, node.children[0]}, env);
        auto right = synth_rec(Expr{expr.ast, node.children[1]}, env);

        if (has_dim(left.type) || has_dim(right.type)) {
            detail::validate_dim_numeric<Cap>(left.type, "div");
            detail::validate_dim_numeric<Cap>(right.type, "div");
            auto ld = promote_to_dim(left.type);
            auto rd = promote_to_dim(right.type);
            auto result = tdim<Cap>(dim_exp(ld, 0) - dim_exp(rd, 0),
                                    dim_exp(ld, 1) - dim_exp(rd, 1),
                                    dim_exp(ld, 2) - dim_exp(rd, 2));
            return TypeResult<Cap>{result, left.valid && right.valid};
        }
        return reftype::detail::check_binary_numeric(expr, env, synth_rec,
                                                     "div");
    });

inline constexpr auto TRDimNeg =
    def_typerule("neg", [](const auto& expr, const auto& env, auto synth_rec) {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto child = synth_rec(Expr{expr.ast, node.children[0]}, env);

        if (has_dim(child.type)) {
            auto d = promote_to_dim(child.type);
            return TypeResult<Cap>{d, child.valid};
        }
        auto ck = get_base_kind(child.type);
        if (ck != BaseKind::Int && ck != BaseKind::Real)
            report_error("non-numeric operand in negation", "Int or Real",
                         kind_name(ck), "neg");
        auto result_type = (ck == BaseKind::Int) ? tint<Cap>() : treal<Cap>();
        return decltype(child){result_type, child.valid};
    });

// ===================================================================
// dim_type_check: custom synth with dimensional rules
// ===================================================================

template <std::size_t Cap>
consteval TypeResult<Cap> dim_type_check(const Expression<Cap>& expr,
                                         const TypeEnv<Cap>& env) {
    Expression<Cap> plain = expr;
    return reftype::synth<TRAnn, TRDimAdd, TRDimSub, TRDimMul, TRDimDiv,
                          TRDimNeg, TREq, TRLt, TRGt, TRLe, TRGe, TRLand, TRLor,
                          TRLnot, TRCond, TRApply, TRLambda, TRProgn>(plain,
                                                                      env);
}

template <std::size_t Cap, auto... Ms>
consteval TypeResult<Cap> dim_type_check(const Expression<Cap, Ms...>& expr,
                                         const TypeEnv<Cap>& env) {
    Expression<Cap> plain = expr;
    return dim_type_check(plain, env);
}

// ===================================================================
// dim_typed_compile: type check + strip + compile
// ===================================================================

template <auto expr, auto env>
    requires requires {
        env.count;
        env.lookup("");
    }
consteval auto dim_typed_compile() {
    constexpr auto result = dim_type_check(expr, env);
    static_assert(result.valid,
                  "dim_typed_compile: dimensional type check failed");
    constexpr auto stripped = strip_types(expr);
    return reftype::detail::compile_with_macros_from<stripped>(expr);
}

// ===================================================================
// Convenience unit constants (Cap=128)
// ===================================================================

inline constexpr auto t_meter = tdim(1, 0, 0);   // m
inline constexpr auto t_second = tdim(0, 1, 0);  // s
inline constexpr auto t_kg = tdim(0, 0, 1);      // kg
inline constexpr auto t_mps = tdim(1, -1, 0);    // m/s (velocity)
inline constexpr auto t_mps2 = tdim(1, -2, 0);   // m/s^2 (acceleration)
inline constexpr auto t_newton = tdim(1, -2, 1); // kg*m/s^2 (force)
inline constexpr auto t_joule = tdim(2, -2, 1);  // kg*m^2/s^2 (energy)
inline constexpr auto t_scalar = tdim(0, 0, 0);  // dimensionless

} // namespace reftype::dim

#endif // REFTYPE_DIM_HPP
