#ifndef REFMACRO_EXPR_HPP
#define REFMACRO_EXPR_HPP

#include <refmacro/ast.hpp>

namespace refmacro {

template <std::size_t Cap = 64>
struct Expr {
    AST<Cap> ast{};
    int id{-1};

    static consteval Expr lit(double v) {
        Expr e;
        ASTNode n{};
        copy_str(n.tag, "lit");
        n.payload = v;
        e.id = e.ast.add_node(n);
        return e;
    }

    static consteval Expr var(const char* name) {
        Expr e;
        ASTNode n{};
        copy_str(n.tag, "var");
        copy_str(n.name, name);
        e.id = e.ast.add_node(n);
        return e;
    }
};

// --- Generic node construction ---

// Nullary (leaf)
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag) {
    Expr<Cap> result;
    result.id = result.ast.add_tagged_node(tag, {});
    return result;
}

// Unary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag, Expr<Cap> c0) {
    Expr<Cap> result;
    result.ast = c0.ast;
    result.id = result.ast.add_tagged_node(tag, {c0.id});
    return result;
}

// Binary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag, Expr<Cap> c0, Expr<Cap> c1) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    result.id = result.ast.add_tagged_node(tag, {c0.id, c1.id + off1});
    return result;
}

// Ternary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag,
                               Expr<Cap> c0, Expr<Cap> c1, Expr<Cap> c2) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    result.id = result.ast.add_tagged_node(tag,
        {c0.id, c1.id + off1, c2.id + off2});
    return result;
}

// Quaternary
template <std::size_t Cap = 64>
consteval Expr<Cap> make_node(const char* tag,
                               Expr<Cap> c0, Expr<Cap> c1,
                               Expr<Cap> c2, Expr<Cap> c3) {
    Expr<Cap> result;
    result.ast = c0.ast;
    int off1 = result.ast.merge(c1.ast);
    int off2 = result.ast.merge(c2.ast);
    int off3 = result.ast.merge(c3.ast);
    result.id = result.ast.add_tagged_node(tag,
        {c0.id, c1.id + off1, c2.id + off2, c3.id + off3});
    return result;
}

// Pipe operator for transform composition
template <std::size_t Cap, typename F>
consteval auto operator|(Expr<Cap> e, F transform_fn) {
    return transform_fn(e);
}

} // namespace refmacro

#endif // REFMACRO_EXPR_HPP
