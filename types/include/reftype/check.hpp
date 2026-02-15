#ifndef REFTYPE_CHECK_HPP
#define REFTYPE_CHECK_HPP

#include <refmacro/expr.hpp>
#include <refmacro/pretty_print.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/pretty.hpp>
#include <reftype/subtype.hpp>
#include <reftype/type_env.hpp>
#include <reftype/typerule.hpp>
#include <reftype/types.hpp>

namespace reftype {

using refmacro::Expression;
using refmacro::str_eq;

// --- Structured error reporting ---

template <std::size_t N = 512>
[[noreturn]] consteval void
report_error(const char* category, const char* expected, const char* actual,
             const char* context) {
    refmacro::PrintBuffer<N> msg{};
    msg.append("type error: ");
    msg.append(category);
    msg.append("\n  expected: ");
    msg.append(expected);
    msg.append("\n  actual:   ");
    msg.append(actual);
    msg.append("\n  at:       ");
    msg.append(context);
    throw msg.data;
}

template <std::size_t N = 512>
[[noreturn]] consteval void report_error(const char* category,
                                         const char* context) {
    refmacro::PrintBuffer<N> msg{};
    msg.append("type error: ");
    msg.append(category);
    msg.append("\n  at: ");
    msg.append(context);
    throw msg.data;
}

// --- TypeResult ---

template <std::size_t Cap = 128> struct TypeResult {
    Expression<Cap> type{};
    bool valid{true};
};

// --- Base kind classification ---

enum class BaseKind { None, Bool, Int, Real };

consteval BaseKind tag_to_kind(const char* tag) {
    if (str_eq(tag, "tbool"))
        return BaseKind::Bool;
    if (str_eq(tag, "tint"))
        return BaseKind::Int;
    if (str_eq(tag, "treal"))
        return BaseKind::Real;
    return BaseKind::None;
}

template <std::size_t Cap>
consteval BaseKind get_base_kind(const Expression<Cap>& type) {
    if (is_base(type))
        return tag_to_kind(type_tag(type));
    if (is_refined(type))
        return tag_to_kind(type_tag(get_refined_base(type)));
    return BaseKind::None;
}

consteval const char* kind_name(BaseKind k) {
    switch (k) {
    case BaseKind::Bool:
        return "Bool";
    case BaseKind::Int:
        return "Int";
    case BaseKind::Real:
        return "Real";
    case BaseKind::None:
        return "<unknown>";
    }
    return "<unknown>";
}

// --- Shared helpers for rule groups ---

namespace detail {

template <std::size_t Cap>
consteval TypeResult<Cap>
check_binary_numeric(const Expression<Cap>& expr, const TypeEnv<Cap>& env,
                     auto synth_rec, const char* op_name) {
    const auto& node = expr.ast.nodes[expr.id];
    auto left = synth_rec(Expression<Cap>{expr.ast, node.children[0]}, env);
    auto right = synth_rec(Expression<Cap>{expr.ast, node.children[1]}, env);

    auto lk = get_base_kind(left.type);
    auto rk = get_base_kind(right.type);

    if (lk != BaseKind::Int && lk != BaseKind::Real)
        report_error("non-numeric operand in arithmetic", "Int or Real",
                     kind_name(lk), op_name);
    if (rk != BaseKind::Int && rk != BaseKind::Real)
        report_error("non-numeric operand in arithmetic", "Int or Real",
                     kind_name(rk), op_name);
    if (lk != rk)
        report_error("arithmetic type mismatch", kind_name(lk), kind_name(rk),
                     op_name);

    auto result_type = (lk == BaseKind::Int) ? tint<Cap>() : treal<Cap>();
    return {result_type, left.valid && right.valid};
}

template <std::size_t Cap>
consteval TypeResult<Cap>
check_binary_comparison(const Expression<Cap>& expr, const TypeEnv<Cap>& env,
                        auto synth_rec, const char* op_name) {
    const auto& node = expr.ast.nodes[expr.id];
    auto left = synth_rec(Expression<Cap>{expr.ast, node.children[0]}, env);
    auto right = synth_rec(Expression<Cap>{expr.ast, node.children[1]}, env);

    auto lk = get_base_kind(left.type);
    auto rk = get_base_kind(right.type);

    if (lk != BaseKind::Int && lk != BaseKind::Real)
        report_error("non-numeric operand in comparison", "Int or Real",
                     kind_name(lk), op_name);
    if (rk != BaseKind::Int && rk != BaseKind::Real)
        report_error("non-numeric operand in comparison", "Int or Real",
                     kind_name(rk), op_name);
    if (lk != rk)
        report_error("comparison type mismatch", kind_name(lk), kind_name(rk),
                     op_name);

    return {tbool<Cap>(), left.valid && right.valid};
}

template <std::size_t Cap>
consteval TypeResult<Cap>
check_binary_logical(const Expression<Cap>& expr, const TypeEnv<Cap>& env,
                     auto synth_rec, const char* op_name) {
    const auto& node = expr.ast.nodes[expr.id];
    auto left = synth_rec(Expression<Cap>{expr.ast, node.children[0]}, env);
    auto right = synth_rec(Expression<Cap>{expr.ast, node.children[1]}, env);

    if (get_base_kind(left.type) != BaseKind::Bool)
        report_error("non-boolean operand in logical operation", "Bool",
                     kind_name(get_base_kind(left.type)), op_name);
    if (get_base_kind(right.type) != BaseKind::Bool)
        report_error("non-boolean operand in logical operation", "Bool",
                     kind_name(get_base_kind(right.type)), op_name);

    return {tbool<Cap>(), left.valid && right.valid};
}

// --- Dispatch: consteval linear search through rules for matching tag ---

template <std::size_t Cap, auto First, auto... Rest>
consteval TypeResult<Cap> dispatch_typerule(const Expression<Cap>& expr,
                                            const TypeEnv<Cap>& env,
                                            auto synth_fn) {
    const auto& node = expr.ast.nodes[expr.id];
    if (str_eq(node.tag, First.tag))
        return First.fn(expr, env, synth_fn);
    if constexpr (sizeof...(Rest) > 0)
        return dispatch_typerule<Cap, Rest...>(expr, env, synth_fn);
    else
        report_error("unsupported node tag", node.tag);
}

} // namespace detail

// --- Built-in type rules (18 total) ---

// Annotation (special: inspects child tag for lambda handling)
inline constexpr auto TRAnn =
    def_typerule("ann", [](const auto& expr, const auto& env, auto synth_rec) {
        const auto& node = expr.ast.nodes[expr.id];
        using E = std::remove_cvref_t<decltype(expr)>;
        E child_expr{expr.ast, node.children[0]};
        E declared_type{expr.ast, node.children[1]};

        if (str_eq(expr.ast.nodes[node.children[0]].tag, "lambda") &&
            is_arrow(declared_type)) {
            const auto& lambda_node = expr.ast.nodes[node.children[0]];
            auto param_name = expr.ast.nodes[lambda_node.children[0]].name;
            E body{expr.ast, lambda_node.children[1]};
            auto input_type = get_arrow_input(declared_type);
            auto output_type = get_arrow_output(declared_type);
            auto extended_env = env.bind(param_name, input_type);
            auto body_result = synth_rec(body, extended_env);
            bool valid =
                body_result.valid && is_subtype(body_result.type, output_type);
            return decltype(body_result){declared_type, valid};
        }

        auto child_result = synth_rec(child_expr, env);
        bool valid =
            child_result.valid && is_subtype(child_result.type, declared_type);
        return decltype(child_result){declared_type, valid};
    });

// Binary arithmetic (4 rules, shared helper)
inline constexpr auto TRAdd =
    def_typerule("add", [](const auto& expr, const auto& env, auto s) {
        return detail::check_binary_numeric(expr, env, s, "add");
    });
inline constexpr auto TRSub =
    def_typerule("sub", [](const auto& expr, const auto& env, auto s) {
        return detail::check_binary_numeric(expr, env, s, "sub");
    });
inline constexpr auto TRMul =
    def_typerule("mul", [](const auto& expr, const auto& env, auto s) {
        return detail::check_binary_numeric(expr, env, s, "mul");
    });
inline constexpr auto TRDiv =
    def_typerule("div", [](const auto& expr, const auto& env, auto s) {
        return detail::check_binary_numeric(expr, env, s, "div");
    });

// Unary negation
inline constexpr auto TRNeg =
    def_typerule("neg", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        const auto& node = expr.ast.nodes[expr.id];
        auto child = synth_rec(E{expr.ast, node.children[0]}, env);
        auto ck = get_base_kind(child.type);
        if (ck != BaseKind::Int && ck != BaseKind::Real)
            report_error("non-numeric operand in negation", "Int or Real",
                         kind_name(ck), "neg");
        auto result_type = (ck == BaseKind::Int) ? tint<Cap>() : treal<Cap>();
        return decltype(child){result_type, child.valid};
    });

// Comparisons (5 rules, shared helper)
inline constexpr auto TREq =
    def_typerule("eq", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_comparison(e, env, s, "eq");
    });
inline constexpr auto TRLt =
    def_typerule("lt", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_comparison(e, env, s, "lt");
    });
inline constexpr auto TRGt =
    def_typerule("gt", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_comparison(e, env, s, "gt");
    });
inline constexpr auto TRLe =
    def_typerule("le", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_comparison(e, env, s, "le");
    });
inline constexpr auto TRGe =
    def_typerule("ge", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_comparison(e, env, s, "ge");
    });

// Logical operators (3 rules)
inline constexpr auto TRLand =
    def_typerule("land", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_logical(e, env, s, "land");
    });
inline constexpr auto TRLor =
    def_typerule("lor", [](const auto& e, const auto& env, auto s) {
        return detail::check_binary_logical(e, env, s, "lor");
    });
inline constexpr auto TRLnot =
    def_typerule("lnot", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        const auto& node = expr.ast.nodes[expr.id];
        auto child = synth_rec(E{expr.ast, node.children[0]}, env);
        if (get_base_kind(child.type) != BaseKind::Bool)
            report_error("non-boolean operand in logical not", "Bool",
                         kind_name(get_base_kind(child.type)), "lnot");
        constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
        return decltype(child){tbool<Cap>(), child.valid};
    });

// Conditional
inline constexpr auto TRCond =
    def_typerule("cond", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        const auto& node = expr.ast.nodes[expr.id];
        auto test = synth_rec(E{expr.ast, node.children[0]}, env);
        auto then_ = synth_rec(E{expr.ast, node.children[1]}, env);
        auto else_ = synth_rec(E{expr.ast, node.children[2]}, env);
        if (get_base_kind(test.type) != BaseKind::Bool)
            report_error("condition must be boolean", "Bool",
                         kind_name(get_base_kind(test.type)), "cond");
        auto result = join(then_.type, else_.type);
        return decltype(test){result, test.valid && then_.valid && else_.valid};
    });

// Application (special: let-binding pattern)
inline constexpr auto TRApply = def_typerule(
    "apply", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        const auto& node = expr.ast.nodes[expr.id];
        E fn{expr.ast, node.children[0]};
        E arg{expr.ast, node.children[1]};

        // Let-binding: apply(lambda(x, body), val)
        if (str_eq(expr.ast.nodes[node.children[0]].tag, "lambda")) {
            const auto& lambda_node = expr.ast.nodes[node.children[0]];
            auto param_name = expr.ast.nodes[lambda_node.children[0]].name;
            E body{expr.ast, lambda_node.children[1]};
            auto arg_result = synth_rec(arg, env);
            auto extended_env = env.bind(param_name, arg_result.type);
            auto body_result = synth_rec(body, extended_env);
            return decltype(arg_result){body_result.type,
                                        arg_result.valid && body_result.valid};
        }

        // General application
        auto fn_result = synth_rec(fn, env);
        if (!is_arrow(fn_result.type)) {
            auto pp = reftype::pretty_print(fn_result.type);
            report_error("applying non-function", "arrow type", pp.data,
                         "apply");
        }
        auto arg_result = synth_rec(arg, env);
        auto input_type = get_arrow_input(fn_result.type);
        bool valid = fn_result.valid && arg_result.valid &&
                     is_subtype(arg_result.type, input_type);
        return decltype(fn_result){get_arrow_output(fn_result.type), valid};
    });

// Standalone lambda (error without annotation)
inline constexpr auto TRLambda = def_typerule("lambda", [](const auto& expr,
                                                           const auto&, auto) {
    constexpr auto Cap = sizeof(expr.ast.nodes) / sizeof(expr.ast.nodes[0]);
    report_error("cannot infer lambda type without annotation", "lambda");
    return TypeResult<Cap>{}; // unreachable, needed for return type deduction
});

// Sequence
inline constexpr auto TRProgn = def_typerule(
    "progn", [](const auto& expr, const auto& env, auto synth_rec) {
        using E = std::remove_cvref_t<decltype(expr)>;
        const auto& node = expr.ast.nodes[expr.id];
        auto first = synth_rec(E{expr.ast, node.children[0]}, env);
        auto second = synth_rec(E{expr.ast, node.children[1]}, env);
        return decltype(first){second.type, first.valid && second.valid};
    });

// --- Type synthesis with rule dispatch ---

template <auto... Rules, std::size_t Cap>
consteval TypeResult<Cap> synth(const Expression<Cap>& expr,
                                const TypeEnv<Cap>& env) {
    const auto& node = expr.ast.nodes[expr.id];

    // Built-in: literals (always handled, like compile.hpp's "lit")
    if (str_eq(node.tag, "lit")) {
        double val = node.payload;
        constexpr double ll_max = static_cast<double>(
            (1LL << 52)); // 2^52: largest exact integer in double
        bool in_range = (val >= -ll_max && val <= ll_max);
        auto int_val = in_range ? static_cast<long long>(val) : 0LL;
        bool is_int = in_range && (val == static_cast<double>(int_val));
        auto base = is_int ? tint<Cap>() : treal<Cap>();
        auto pred = Expression<Cap>::var("#v") == Expression<Cap>::lit(val);
        return {tref(base, pred)};
    }

    // Built-in: variables (always handled, like compile.hpp's "var")
    if (str_eq(node.tag, "var"))
        return {env.lookup(node.name)};

    // Create recursive synth callable for rule functions.
    // Cap and Rules... are template parameters, not captures — stateless.
    auto synth_fn = [](const Expression<Cap>& e, const TypeEnv<Cap>& env2) {
        return synth<Rules...>(e, env2);
    };

    // Dispatch to matching rule
    if constexpr (sizeof...(Rules) > 0)
        return detail::dispatch_typerule<Cap, Rules...>(expr, env, synth_fn);
    else
        report_error("unsupported node tag", node.tag);
}

// --- Top-level type checking ---

template <auto... ExtraRules, std::size_t Cap = 128, auto... Ms>
consteval TypeResult<Cap> type_check(const Expression<Cap, Ms...>& e) {
    Expression<Cap> plain = e; // strip macros
    return synth<TRAnn, TRAdd, TRSub, TRMul, TRDiv, TRNeg, TREq, TRLt, TRGt,
                 TRLe, TRGe, TRLand, TRLor, TRLnot, TRCond, TRApply, TRLambda,
                 TRProgn, ExtraRules...>(plain, TypeEnv<Cap>{});
}

template <auto... ExtraRules, std::size_t Cap = 128, auto... Ms>
consteval TypeResult<Cap> type_check(const Expression<Cap, Ms...>& e,
                                     const TypeEnv<Cap>& env) {
    Expression<Cap> plain = e; // strip macros
    return synth<TRAnn, TRAdd, TRSub, TRMul, TRDiv, TRNeg, TREq, TRLt, TRGt,
                 TRLe, TRGe, TRLand, TRLor, TRLnot, TRCond, TRApply, TRLambda,
                 TRProgn, ExtraRules...>(plain, env);
}

} // namespace reftype

#endif // REFTYPE_CHECK_HPP
