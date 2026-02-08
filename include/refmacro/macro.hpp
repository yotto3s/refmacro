#ifndef REFMACRO_MACRO_HPP
#define REFMACRO_MACRO_HPP

#include <refmacro/ast.hpp>
#include <refmacro/expr.hpp>

namespace refmacro {

// Macro: an AST node type with deferred lowering.
//
// - Calling a Macro builds an AST node (no lowering happens).
// - The lowering function `fn` is applied later by compile().
// - `fn` receives compiled children (lambdas) and returns a lambda.
// - CompileFn must be a stateless lambda (structural -> NTTP-compatible).
template <typename CompileFn> struct Macro {
    char tag[16]{};
    CompileFn fn{};

    // Build AST nodes -- lowering is deferred
    consteval Expr<> operator()() const { return make_node<>(tag); }
    consteval Expr<> operator()(Expr<> c0) const { return make_node(tag, c0); }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1) const {
        return make_node(tag, c0, c1);
    }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1, Expr<> c2) const {
        return make_node(tag, c0, c1, c2);
    }
    consteval Expr<> operator()(Expr<> c0, Expr<> c1, Expr<> c2,
                                Expr<> c3) const {
        return make_node(tag, c0, c1, c2, c3);
    }
};

template <typename CompileFn>
consteval auto defmacro(const char* name, CompileFn) {
    Macro<CompileFn> m{};
    copy_str(m.tag, name);
    return m;
}

} // namespace refmacro

#endif // REFMACRO_MACRO_HPP
