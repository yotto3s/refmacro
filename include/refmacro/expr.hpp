#ifndef REFMACRO_EXPR_HPP
#define REFMACRO_EXPR_HPP

#include <refmacro/ast.hpp>

namespace refmacro {

template <std::size_t Cap = 64, auto... Macros> struct Expression {
    AST<Cap> ast{};
    int id{-1};

    // Converting constructor: allows conversion between Expression types with
    // different Macros
    template <auto... OtherMs>
    consteval Expression(const Expression<Cap, OtherMs...>& other)
        : ast(other.ast), id(other.id) {}

    // Constructor from AST and id (needed for aggregate-init replacement)
    consteval Expression(const AST<Cap>& a, int i) : ast(a), id(i) {}

    // Default constructor (needed since we added the converting constructor)
    consteval Expression() = default;

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

// N-ary (1-8 children) - same macro type
template <std::size_t Cap = 64, auto... Ms,
          std::same_as<Expression<Cap, Ms...>>... Rest>
    requires(sizeof...(Rest) <= 7)
consteval Expression<Cap, Ms...>
make_node(const char* tag, Expression<Cap, Ms...> c0, Rest... rest) {
    Expression<Cap, Ms...> result;
    result.ast = c0.ast;
    int ids[1 + sizeof...(Rest)]{c0.id};
    [[maybe_unused]] std::size_t i = 1;
    ((ids[i++] = rest.id + result.ast.merge(rest.ast)), ...);
    result.id = result.ast.add_tagged_node(tag, ids, 1 + sizeof...(Rest));
    return result;
}

// N-ary - mixed macro types (strips macros)
template <std::size_t Cap = 64, typename... Exprs>
    requires(sizeof...(Exprs) > 0 && sizeof...(Exprs) <= 8)
consteval Expression<Cap> make_node(const char* tag, Exprs... children) {
    Expression<Cap> result;
    // Convert all to Expression<Cap> and merge ASTs
    int ids[sizeof...(Exprs)];
    std::size_t idx = 0;
    bool first = true;
    (([&] {
         Expression<Cap> child =
             children; // converting constructor strips macros
         if (first) {
             result.ast = child.ast;
             ids[idx++] = child.id;
             first = false;
         } else {
             int off = result.ast.merge(child.ast);
             ids[idx++] = child.id + off;
         }
     }()),
     ...);
    result.id = result.ast.add_tagged_node(tag, ids, sizeof...(Exprs));
    return result;
}

// Pipe operator for transform composition
template <std::size_t Cap, auto... Ms, typename F>
consteval auto operator|(Expression<Cap, Ms...> e, F transform_fn) {
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
