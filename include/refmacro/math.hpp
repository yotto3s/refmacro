#ifndef REFMACRO_MATH_HPP
#define REFMACRO_MATH_HPP

#include <refmacro/ast.hpp>
#include <refmacro/compile.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>
#include <refmacro/transforms.hpp>

namespace refmacro {

// --- Math macros (lowering to lambdas) ---

inline constexpr auto MAdd = defmacro<"add">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

inline constexpr auto MSub = defmacro<"sub">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) - rhs(a...); };
});

inline constexpr auto MMul = defmacro<"mul">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) * rhs(a...); };
});

inline constexpr auto MDiv = defmacro<"div">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) / rhs(a...); };
});

inline constexpr auto MNeg = defmacro<"neg">(
    [](auto x) { return [=](auto... a) constexpr { return -x(a...); }; });

// --- Operator sugar (auto-tracks macros via MacroCaller delegation) ---

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator+(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MAdd(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator-(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MSub(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator*(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MMul(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator/(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MDiv(lhs, rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator-(Expression<Cap, Ms...> x) {
    return MNeg(x);
}

// double on LHS
template <std::size_t Cap, auto... Ms>
consteval auto operator+(double lhs, Expression<Cap, Ms...> rhs) {
    return MAdd(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator-(double lhs, Expression<Cap, Ms...> rhs) {
    return MSub(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator*(double lhs, Expression<Cap, Ms...> rhs) {
    return MMul(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator/(double lhs, Expression<Cap, Ms...> rhs) {
    return MDiv(Expression<Cap>::lit(lhs), rhs);
}

// double on RHS
template <std::size_t Cap, auto... Ms>
consteval auto operator+(Expression<Cap, Ms...> lhs, double rhs) {
    return MAdd(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator-(Expression<Cap, Ms...> lhs, double rhs) {
    return MSub(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator*(Expression<Cap, Ms...> lhs, double rhs) {
    return MMul(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator/(Expression<Cap, Ms...> lhs, double rhs) {
    return MDiv(lhs, Expression<Cap>::lit(rhs));
}

// --- Convenience: compile with all math macros ---

template <auto e> consteval auto math_compile() {
    return compile<e, MAdd, MSub, MMul, MDiv, MNeg>();
}

// --- simplify: algebraic identities + constant folding ---

namespace detail {
consteval double eval_op(std::string_view tag, double lhs, double rhs) {
    if (tag == "add")
        return lhs + rhs;
    if (tag == "sub")
        return lhs - rhs;
    if (tag == "mul")
        return lhs * rhs;
    if (tag == "div") {
        if (rhs == 0.0)
            throw "division by zero in constant folding";
        return lhs / rhs;
    }
    return 0.0;
}
} // namespace detail

template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> simplify(Expression<Cap, Ms...> e) {
    Expression<Cap> plain = e; // strip macros
    auto is_lit = [](NodeView<Cap> v, double val) consteval {
        return v.tag() == "lit" && v.payload() == val;
    };
    auto result = rewrite(
        plain,
        [is_lit](NodeView<Cap> n) consteval -> std::optional<Expression<Cap>> {
            // x + 0 -> x, 0 + x -> x
            if (n.tag() == "add" && n.child_count() == 2) {
                if (is_lit(n.child(1), 0.0))
                    return to_expr(n, n.child(0));
                if (is_lit(n.child(0), 0.0))
                    return to_expr(n, n.child(1));
            }
            // x * 1 -> x, 1 * x -> x, x * 0 -> 0, 0 * x -> 0
            if (n.tag() == "mul" && n.child_count() == 2) {
                if (is_lit(n.child(1), 1.0))
                    return to_expr(n, n.child(0));
                if (is_lit(n.child(0), 1.0))
                    return to_expr(n, n.child(1));
                if (is_lit(n.child(0), 0.0))
                    return Expression<Cap>::lit(0.0);
                if (is_lit(n.child(1), 0.0))
                    return Expression<Cap>::lit(0.0);
            }
            // x - 0 -> x
            if (n.tag() == "sub" && n.child_count() == 2 &&
                is_lit(n.child(1), 0.0))
                return to_expr(n, n.child(0));
            // x / 1 -> x
            if (n.tag() == "div" && n.child_count() == 2 &&
                is_lit(n.child(1), 1.0))
                return to_expr(n, n.child(0));
            // --x -> x
            if (n.tag() == "neg" && n.child_count() == 1 &&
                n.child(0).tag() == "neg")
                return to_expr(n, n.child(0).child(0));
            // -lit -> lit
            if (n.tag() == "neg" && n.child_count() == 1 &&
                n.child(0).tag() == "lit")
                return Expression<Cap>::lit(-n.child(0).payload());
            // constant folding: lit op lit -> lit
            if (n.child_count() == 2 && n.child(0).tag() == "lit" &&
                n.child(1).tag() == "lit") {
                auto tag = n.tag();
                if (tag == "add" || tag == "sub" || tag == "mul" ||
                    tag == "div")
                    return Expression<Cap>::lit(detail::eval_op(
                        tag, n.child(0).payload(), n.child(1).payload()));
            }
            return std::nullopt;
        });
    return result; // implicit conversion back to Expression<Cap, Ms...>
}

// --- differentiate: symbolic differentiation via structural recursion ---

template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> differentiate(Expression<Cap, Ms...> e,
                                               const char* var) {
    Expression<Cap> plain = e; // strip macros
    auto result = transform(
        plain,
        [var](NodeView<Cap> n, auto recurse) consteval -> Expression<Cap> {
            if (n.tag() == "lit")
                return Expression<Cap>::lit(0.0);
            if (n.tag() == "var")
                return Expression<Cap>::lit(n.name() == var ? 1.0 : 0.0);
            if (n.tag() == "neg" && n.child_count() == 1)
                return -recurse(n.child(0));
            if (n.tag() == "add" && n.child_count() == 2)
                return recurse(n.child(0)) + recurse(n.child(1));
            if (n.tag() == "sub" && n.child_count() == 2)
                return recurse(n.child(0)) - recurse(n.child(1));
            if (n.tag() == "mul" && n.child_count() == 2) {
                auto f = to_expr(n, n.child(0));
                auto g = to_expr(n, n.child(1));
                return f * recurse(n.child(1)) + recurse(n.child(0)) * g;
            }
            if (n.tag() == "div" && n.child_count() == 2) {
                auto f = to_expr(n, n.child(0));
                auto g = to_expr(n, n.child(1));
                return (recurse(n.child(0)) * g - f * recurse(n.child(1))) /
                       (g * g);
            }
            return Expression<Cap>::lit(0.0);
        });
    return result; // implicit conversion back to Expression<Cap, Ms...>
}

} // namespace refmacro

#endif // REFMACRO_MATH_HPP
