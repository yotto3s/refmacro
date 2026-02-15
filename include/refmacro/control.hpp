#ifndef REFMACRO_CONTROL_HPP
#define REFMACRO_CONTROL_HPP

// Control-flow macros and first-class bindings for the refmacro AST framework.
//
// Provides: conditionals (MCond), comparisons (MEq, MLt, MGt, MLe, MGe),
// logical operators (MLand, MLor, MLnot), sequencing (MProgn), and
// lambda/apply/let for compile-time lexical scoping.
//
// ctrl_compile<e>()  — compile with control-flow macros only
// full_compile<e>()  — compile with math + control-flow macros

#include <refmacro/ast.hpp>
#include <refmacro/compile.hpp>
#include <refmacro/expr.hpp>
#include <refmacro/macro.hpp>
#include <refmacro/math.hpp>

namespace refmacro {

// --- Control-flow macros (lowering to lambdas) ---

inline constexpr auto MCond =
    defmacro<"cond">([](auto test, auto then_, auto else_) {
        return [=](auto... a) constexpr {
            return test(a...) ? then_(a...) : else_(a...);
        };
    });

inline constexpr auto MLand = defmacro<"land">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) && rhs(a...); };
});

inline constexpr auto MLor = defmacro<"lor">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) || rhs(a...); };
});

inline constexpr auto MLnot = defmacro<"lnot">(
    [](auto x) { return [=](auto... a) constexpr { return !x(a...); }; });

inline constexpr auto MEq = defmacro<"eq">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) == rhs(a...); };
});

inline constexpr auto MLt = defmacro<"lt">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) < rhs(a...); };
});

inline constexpr auto MGt = defmacro<"gt">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) > rhs(a...); };
});

inline constexpr auto MLe = defmacro<"le">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) <= rhs(a...); };
});

inline constexpr auto MGe = defmacro<"ge">([](auto lhs, auto rhs) {
    return [=](auto... a) constexpr { return lhs(a...) >= rhs(a...); };
});

inline constexpr auto MProgn = defmacro<"progn">([](auto a, auto b) {
    return [=](auto... args) constexpr { return (void)a(args...), b(args...); };
});

// --- Lambda / Apply / Let (first-class AST nodes, handled by compile_node) ---

template <std::size_t Cap = 64, auto... Ms>
consteval Expression<Cap, Ms...> lambda(const char* param,
                                        Expression<Cap, Ms...> body) {
    Expression<Cap, Ms...> result;
    result.ast = body.ast;
    ASTNode param_node{};
    copy_str(param_node.tag, "var");
    copy_str(param_node.name, param);
    int param_id = result.ast.add_node(param_node);
    result.id = result.ast.add_tagged_node("lambda", {param_id, body.id});
    return result;
}

template <std::size_t Cap = 64, auto... Ms1, auto... Ms2>
consteval Expression<Cap, Ms1..., Ms2...> apply(Expression<Cap, Ms1...> fn,
                                                Expression<Cap, Ms2...> arg) {
    Expression<Cap, Ms1..., Ms2...> result;
    result.ast = fn.ast;
    int off = result.ast.merge(arg.ast);
    result.id = result.ast.add_tagged_node("apply", {fn.id, arg.id + off});
    return result;
}

template <std::size_t Cap = 64, auto... Ms1, auto... Ms2>
consteval auto let_(const char* name, Expression<Cap, Ms1...> val,
                    Expression<Cap, Ms2...> body) {
    return apply(lambda(name, body), val);
}

// --- Comparison operator sugar (delegates to MacroCaller for auto-tracking)
// ---

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator==(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MEq(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator<(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MLt(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator>(Expression<Cap, Ms1...> lhs,
                         Expression<Cap, Ms2...> rhs) {
    return MGt(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator<=(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MLe(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator>=(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MGe(lhs, rhs);
}

// double on LHS (comparison)
template <std::size_t Cap, auto... Ms>
consteval auto operator==(double lhs, Expression<Cap, Ms...> rhs) {
    return MEq(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator<(double lhs, Expression<Cap, Ms...> rhs) {
    return MLt(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator>(double lhs, Expression<Cap, Ms...> rhs) {
    return MGt(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator<=(double lhs, Expression<Cap, Ms...> rhs) {
    return MLe(Expression<Cap>::lit(lhs), rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator>=(double lhs, Expression<Cap, Ms...> rhs) {
    return MGe(Expression<Cap>::lit(lhs), rhs);
}

// double on RHS (comparison)
template <std::size_t Cap, auto... Ms>
consteval auto operator==(Expression<Cap, Ms...> lhs, double rhs) {
    return MEq(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator<(Expression<Cap, Ms...> lhs, double rhs) {
    return MLt(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator>(Expression<Cap, Ms...> lhs, double rhs) {
    return MGt(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator<=(Expression<Cap, Ms...> lhs, double rhs) {
    return MLe(lhs, Expression<Cap>::lit(rhs));
}
template <std::size_t Cap, auto... Ms>
consteval auto operator>=(Expression<Cap, Ms...> lhs, double rhs) {
    return MGe(lhs, Expression<Cap>::lit(rhs));
}

// --- Logical operator sugar (delegates to MacroCaller for auto-tracking) ---

template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator&&(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MLand(lhs, rhs);
}
template <std::size_t Cap, auto... Ms1, auto... Ms2>
consteval auto operator||(Expression<Cap, Ms1...> lhs,
                          Expression<Cap, Ms2...> rhs) {
    return MLor(lhs, rhs);
}
template <std::size_t Cap, auto... Ms>
consteval auto operator!(Expression<Cap, Ms...> x) {
    return MLnot(x);
}

// --- Convenience: compile with all control-flow macros ---

template <auto e> consteval auto ctrl_compile() {
    return compile<e, MCond, MLand, MLor, MLnot, MEq, MLt, MGt, MLe, MGe,
                   MProgn>();
}

// --- Full compile: math + control-flow macros ---

template <auto e> consteval auto full_compile() {
    return compile<e, MAdd, MSub, MMul, MDiv, MNeg, MCond, MLand, MLor, MLnot,
                   MEq, MLt, MGt, MLe, MGe, MProgn>();
}

} // namespace refmacro

#endif // REFMACRO_CONTROL_HPP
