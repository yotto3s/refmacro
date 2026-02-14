#ifndef REFTYPE_CHECK_HPP
#define REFTYPE_CHECK_HPP

#include <refmacro/expr.hpp>
#include <refmacro/pretty_print.hpp>
#include <refmacro/str_utils.hpp>
#include <reftype/pretty.hpp>
#include <reftype/subtype.hpp>
#include <reftype/type_env.hpp>
#include <reftype/types.hpp>

namespace reftype {

using refmacro::Expression;
using refmacro::str_eq;

// --- Structured error reporting ---

template <std::size_t N = 512>
consteval void report_error(const char* category, const char* expected,
                            const char* actual, const char* context) {
    refmacro::FixedString<N> msg{};
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
consteval void report_error(const char* category, const char* context) {
    refmacro::FixedString<N> msg{};
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

// --- Type synthesis ---

template <std::size_t Cap>
consteval TypeResult<Cap> synth(const Expression<Cap>& expr,
                                const TypeEnv<Cap>& env) {
    const auto& node = expr.ast.nodes[expr.id];

    // --- Literals: singleton refinement type ---
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

    // --- Variables ---
    if (str_eq(node.tag, "var"))
        return {env.lookup(node.name)};

    // --- Annotation ---
    if (str_eq(node.tag, "ann")) {
        Expression<Cap> child_expr{expr.ast, node.children[0]};
        Expression<Cap> declared_type{expr.ast, node.children[1]};

        // Annotated lambda: check body against arrow output type
        if (str_eq(expr.ast.nodes[node.children[0]].tag, "lambda") &&
            is_arrow(declared_type)) {
            const auto& lambda_node = expr.ast.nodes[node.children[0]];
            auto param_name = expr.ast.nodes[lambda_node.children[0]].name;
            Expression<Cap> body{expr.ast, lambda_node.children[1]};

            auto input_type = get_arrow_input(declared_type);
            auto output_type = get_arrow_output(declared_type);

            auto extended_env = env.bind(param_name, input_type);
            auto body_result = synth(body, extended_env);

            bool valid =
                body_result.valid && is_subtype(body_result.type, output_type);
            return {declared_type, valid};
        }

        auto child_result = synth(child_expr, env);
        bool valid =
            child_result.valid && is_subtype(child_result.type, declared_type);
        return {declared_type, valid};
    }

    // --- Binary arithmetic: add, sub, mul, div ---
    // Note: Bool is excluded from numeric operations despite Bool <: Int in the
    // subtype lattice. Bool is its own domain for logical operations (land,
    // lor, lnot). Implicit promotion from Bool to Int is not supported in
    // arithmetic or comparisons — use an explicit annotation if needed.
    if (str_eq(node.tag, "add") || str_eq(node.tag, "sub") ||
        str_eq(node.tag, "mul") || str_eq(node.tag, "div")) {
        auto left = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto right = synth(Expression<Cap>{expr.ast, node.children[1]}, env);

        auto lk = get_base_kind(left.type);
        auto rk = get_base_kind(right.type);

        if (lk != BaseKind::Int && lk != BaseKind::Real)
            report_error("non-numeric operand in arithmetic", "Int or Real",
                         kind_name(lk), node.tag);
        if (rk != BaseKind::Int && rk != BaseKind::Real)
            report_error("non-numeric operand in arithmetic", "Int or Real",
                         kind_name(rk), node.tag);
        if (lk != rk)
            report_error("arithmetic type mismatch", kind_name(lk),
                         kind_name(rk), node.tag);

        auto result_type = (lk == BaseKind::Int) ? tint<Cap>() : treal<Cap>();
        return {result_type, left.valid && right.valid};
    }

    // --- Unary negation ---
    if (str_eq(node.tag, "neg")) {
        auto child = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto ck = get_base_kind(child.type);
        if (ck != BaseKind::Int && ck != BaseKind::Real)
            report_error("non-numeric operand in negation", "Int or Real",
                         kind_name(ck), "neg");
        auto result_type = (ck == BaseKind::Int) ? tint<Cap>() : treal<Cap>();
        return {result_type, child.valid};
    }

    // --- Comparisons: eq, lt, gt, le, ge ---
    if (str_eq(node.tag, "eq") || str_eq(node.tag, "lt") ||
        str_eq(node.tag, "gt") || str_eq(node.tag, "le") ||
        str_eq(node.tag, "ge")) {
        auto left = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto right = synth(Expression<Cap>{expr.ast, node.children[1]}, env);

        auto lk = get_base_kind(left.type);
        auto rk = get_base_kind(right.type);

        if (lk != BaseKind::Int && lk != BaseKind::Real)
            report_error("non-numeric operand in comparison", "Int or Real",
                         kind_name(lk), node.tag);
        if (rk != BaseKind::Int && rk != BaseKind::Real)
            report_error("non-numeric operand in comparison", "Int or Real",
                         kind_name(rk), node.tag);
        if (lk != rk)
            report_error("comparison type mismatch", kind_name(lk),
                         kind_name(rk), node.tag);

        return {tbool<Cap>(), left.valid && right.valid};
    }

    // --- Logical: land, lor ---
    if (str_eq(node.tag, "land") || str_eq(node.tag, "lor")) {
        auto left = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto right = synth(Expression<Cap>{expr.ast, node.children[1]}, env);

        if (get_base_kind(left.type) != BaseKind::Bool)
            report_error("non-boolean operand in logical operation", "Bool",
                         kind_name(get_base_kind(left.type)), node.tag);
        if (get_base_kind(right.type) != BaseKind::Bool)
            report_error("non-boolean operand in logical operation", "Bool",
                         kind_name(get_base_kind(right.type)), node.tag);

        return {tbool<Cap>(), left.valid && right.valid};
    }

    // --- Logical not ---
    if (str_eq(node.tag, "lnot")) {
        auto child = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        if (get_base_kind(child.type) != BaseKind::Bool)
            report_error("non-boolean operand in logical not", "Bool",
                         kind_name(get_base_kind(child.type)), "lnot");
        return {tbool<Cap>(), child.valid};
    }

    // --- Conditional ---
    if (str_eq(node.tag, "cond")) {
        auto test = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto then_ = synth(Expression<Cap>{expr.ast, node.children[1]}, env);
        auto else_ = synth(Expression<Cap>{expr.ast, node.children[2]}, env);

        if (get_base_kind(test.type) != BaseKind::Bool)
            report_error("condition must be boolean", "Bool",
                         kind_name(get_base_kind(test.type)), "cond");

        auto result = join(then_.type, else_.type);
        return {result, test.valid && then_.valid && else_.valid};
    }

    // --- Application ---
    if (str_eq(node.tag, "apply")) {
        Expression<Cap> fn{expr.ast, node.children[0]};
        Expression<Cap> arg{expr.ast, node.children[1]};

        // Let-binding pattern: apply(lambda(x, body), val)
        if (str_eq(expr.ast.nodes[node.children[0]].tag, "lambda")) {
            const auto& lambda_node = expr.ast.nodes[node.children[0]];
            auto param_name = expr.ast.nodes[lambda_node.children[0]].name;
            Expression<Cap> body{expr.ast, lambda_node.children[1]};

            auto arg_result = synth(arg, env);
            auto extended_env = env.bind(param_name, arg_result.type);
            auto body_result = synth(body, extended_env);

            return {body_result.type, arg_result.valid && body_result.valid};
        }

        // General application: fn(arg)
        auto fn_result = synth(fn, env);
        if (!is_arrow(fn_result.type)) {
            auto pp = reftype::pretty_print(fn_result.type);
            report_error("applying non-function", "arrow type", pp.data,
                         "apply");
        }

        auto arg_result = synth(arg, env);
        auto input_type = get_arrow_input(fn_result.type);

        bool valid = fn_result.valid && arg_result.valid &&
                     is_subtype(arg_result.type, input_type);
        return {get_arrow_output(fn_result.type), valid};
    }

    // --- Standalone lambda (error without annotation) ---
    if (str_eq(node.tag, "lambda"))
        report_error("cannot infer lambda type without annotation", "lambda");

    // --- Sequence ---
    if (str_eq(node.tag, "progn")) {
        auto first = synth(Expression<Cap>{expr.ast, node.children[0]}, env);
        auto second = synth(Expression<Cap>{expr.ast, node.children[1]}, env);
        return {second.type, first.valid && second.valid};
    }

    report_error("unsupported node tag", node.tag);
}

// --- Top-level type checking ---

template <std::size_t Cap = 128>
consteval TypeResult<Cap> type_check(const Expression<Cap>& e) {
    return synth(e, TypeEnv<Cap>{});
}

template <std::size_t Cap = 128>
consteval TypeResult<Cap> type_check(const Expression<Cap>& e,
                                     const TypeEnv<Cap>& env) {
    return synth(e, env);
}

} // namespace reftype

#endif // REFTYPE_CHECK_HPP
