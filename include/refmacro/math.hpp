#ifndef REFMACRO_MATH_HPP
#define REFMACRO_MATH_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>
#include <refmacro/compile.hpp>

namespace refmacro {

// --- Math macros (lowering to lambdas) ---

inline constexpr auto MAdd = defmacro("add", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) + rhs(a...); };
});

inline constexpr auto MSub = defmacro("sub", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) - rhs(a...); };
});

inline constexpr auto MMul = defmacro("mul", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) * rhs(a...); };
});

inline constexpr auto MDiv = defmacro("div", [](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) / rhs(a...); };
});

inline constexpr auto MNeg = defmacro("neg", [](auto x) {
    return [=](auto... a) constexpr { return -x(a...); };
});

// --- Operator sugar (creates AST nodes, no lowering) ---

template <std::size_t Cap>
consteval Expr<Cap> operator+(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("add", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator-(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("sub", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator*(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("mul", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator/(Expr<Cap> lhs, Expr<Cap> rhs) {
    return make_node("div", lhs, rhs);
}
template <std::size_t Cap>
consteval Expr<Cap> operator-(Expr<Cap> x) {
    return make_node("neg", x);
}

// double on LHS
consteval Expr<> operator+(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) + rhs; }
consteval Expr<> operator-(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) - rhs; }
consteval Expr<> operator*(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) * rhs; }
consteval Expr<> operator/(double lhs, Expr<> rhs) { return Expr<>::lit(lhs) / rhs; }

// double on RHS
consteval Expr<> operator+(Expr<> lhs, double rhs) { return lhs + Expr<>::lit(rhs); }
consteval Expr<> operator-(Expr<> lhs, double rhs) { return lhs - Expr<>::lit(rhs); }
consteval Expr<> operator*(Expr<> lhs, double rhs) { return lhs * Expr<>::lit(rhs); }
consteval Expr<> operator/(Expr<> lhs, double rhs) { return lhs / Expr<>::lit(rhs); }

// --- Convenience: compile with all math macros ---

template <auto e>
consteval auto math_compile() {
    return compile<e, MAdd, MSub, MMul, MDiv, MNeg>();
}

} // namespace refmacro

#endif // REFMACRO_MATH_HPP
