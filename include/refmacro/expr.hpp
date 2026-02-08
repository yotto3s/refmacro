#ifndef REFMACRO_EXPR_HPP
#define REFMACRO_EXPR_HPP

#include <refmacro/ast.hpp>

namespace refmacro {

template <std::size_t Cap = 64> struct Expression {
    AST<Cap> ast{};
    int id{-1};

    static consteval Expression lit(double v) {
        Expression e;
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = v;
        e.id = e.ast.add_node(n);
        return e;
    }

    static consteval Expression var(const char* name) {
        Expression e;
        ASTNode n{};
        copy_str(n.tag, "var");
        copy_str(n.name, name);
        e.id = e.ast.add_node(n);
        return e;
    }
};

using Expr = Expression<>;

// --- Generic node construction ---

// Nullary (leaf)
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag) {
    Expression<Cap> result;
    result.id = result.ast.add_tagged_node(tag, {});
    return result;
}

// Unary
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag, Expression<Cap> c0) {
    Expression<Cap> result;
    result.ast = c0.ast;
    result.id = result.ast.add_tagged_node(tag, {c0.id});
    return result;
}

// Binary
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag, Expression<Cap> c0,
                                    Expression<Cap> c1) {
    Expression<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    result.id = result.ast.add_tagged_node(tag, {c0.id, c1.id + off1});
    return result;
}

// Ternary
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag, Expression<Cap> c0,
                                    Expression<Cap> c1, Expression<Cap> c2) {
    Expression<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    result.id =
        result.ast.add_tagged_node(tag, {c0.id, c1.id + off1, c2.id + off2});
    return result;
}

// Quaternary
template <std::size_t Cap = 64>
consteval Expression<Cap> make_node(const char* tag, Expression<Cap> c0,
                                    Expression<Cap> c1, Expression<Cap> c2,
                                    Expression<Cap> c3) {
    Expression<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    int off3 = result.ast.merge(c3.ast);
    result.id = result.ast.add_tagged_node(
        tag, {c0.id, c1.id + off1, c2.id + off2, c3.id + off3});
    return result;
}

// --- Comparison operator sugar (creates AST nodes) ---

template <std::size_t Cap>
consteval Expression<Cap> operator==(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("eq", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator<(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("lt", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator>(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("gt", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator<=(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("le", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator>=(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("ge", lhs, rhs);
}

// double on LHS (comparison)
template <std::size_t Cap>
consteval Expression<Cap> operator==(double lhs, Expression<Cap> rhs) {
    return Expression<Cap>::lit(lhs) == rhs;
}
template <std::size_t Cap>
consteval Expression<Cap> operator<(double lhs, Expression<Cap> rhs) {
    return Expression<Cap>::lit(lhs) < rhs;
}
template <std::size_t Cap>
consteval Expression<Cap> operator>(double lhs, Expression<Cap> rhs) {
    return Expression<Cap>::lit(lhs) > rhs;
}
template <std::size_t Cap>
consteval Expression<Cap> operator<=(double lhs, Expression<Cap> rhs) {
    return Expression<Cap>::lit(lhs) <= rhs;
}
template <std::size_t Cap>
consteval Expression<Cap> operator>=(double lhs, Expression<Cap> rhs) {
    return Expression<Cap>::lit(lhs) >= rhs;
}

// double on RHS (comparison)
template <std::size_t Cap>
consteval Expression<Cap> operator==(Expression<Cap> lhs, double rhs) {
    return lhs == Expression<Cap>::lit(rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator<(Expression<Cap> lhs, double rhs) {
    return lhs < Expression<Cap>::lit(rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator>(Expression<Cap> lhs, double rhs) {
    return lhs > Expression<Cap>::lit(rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator<=(Expression<Cap> lhs, double rhs) {
    return lhs <= Expression<Cap>::lit(rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator>=(Expression<Cap> lhs, double rhs) {
    return lhs >= Expression<Cap>::lit(rhs);
}

// --- Logical operator sugar ---

template <std::size_t Cap>
consteval Expression<Cap> operator&&(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("land", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator||(Expression<Cap> lhs, Expression<Cap> rhs) {
    return make_node("lor", lhs, rhs);
}
template <std::size_t Cap>
consteval Expression<Cap> operator!(Expression<Cap> x) {
    return make_node("lnot", x);
}

// Pipe operator for transform composition
template <std::size_t Cap, typename F>
consteval auto operator|(Expression<Cap> e, F transform_fn) {
    return transform_fn(e);
}

// --- expr(): bind variable names to lambda parameters ---
//
// Sugar so users can write:
//   expr([](auto x, auto y) { return x * x + y; }, "x", "y")
// instead of manually creating Expr::var("x"), Expr::var("y").
//
// Note: C++26 reflection (P2996) cannot yet extract parameter names from
// generic lambdas in GCC's current implementation, so names are passed
// explicitly.

// Unary: one parameter
template <typename F> consteval auto expr(F f, const char* name0) {
    return f(Expr::var(name0));
}

// Binary: two parameters
template <typename F>
consteval auto expr(F f, const char* name0, const char* name1) {
    return f(Expr::var(name0), Expr::var(name1));
}

// Ternary: three parameters
template <typename F>
consteval auto expr(F f, const char* name0, const char* name1,
                    const char* name2) {
    return f(Expr::var(name0), Expr::var(name1), Expr::var(name2));
}

// Quaternary: four parameters
template <typename F>
consteval auto expr(F f, const char* name0, const char* name1,
                    const char* name2, const char* name3) {
    return f(Expr::var(name0), Expr::var(name1), Expr::var(name2),
             Expr::var(name3));
}

} // namespace refmacro

#endif // REFMACRO_EXPR_HPP
